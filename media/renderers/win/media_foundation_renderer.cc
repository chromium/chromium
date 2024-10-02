// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/win/media_foundation_renderer.h"

#include <Audioclient.h>
#include <mferror.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/wrapped_window_proc.h"
#include "media/base/cdm_context.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/timestamp_constants.h"
#include "media/base/win/dxgi_device_manager.h"
#include "media/base/win/hresults.h"
#include "media/base/win/mf_helpers.h"
#include "media/base/win/mf_initializer.h"

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

namespace {

ATOM g_video_window_class = 0;

// The |g_video_window_class| atom obtained is used as the |lpClassName|
// parameter in CreateWindowEx().
// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexa
//
// To enable OPM
// (https://docs.microsoft.com/en-us/windows/win32/medfound/output-protection-manager)
// protection for video playback, We call CreateWindowEx() to get a window
// and pass it to MFMediaEngine as an attribute.
bool InitializeVideoWindowClass() {
  if (g_video_window_class)
    return true;

  WNDCLASSEX intermediate_class;
  base::win::InitializeWindowClass(
      L"VirtualMediaFoundationCdmVideoWindow",
      &base::win::WrappedWindowProc<::DefWindowProc>, CS_OWNDC, 0, 0, nullptr,
      reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)), nullptr, nullptr,
      nullptr, &intermediate_class);
  g_video_window_class = RegisterClassEx(&intermediate_class);
  if (!g_video_window_class) {
    HRESULT register_class_error = HRESULT_FROM_WIN32(GetLastError());
    DLOG(ERROR) << "RegisterClass failed: " << PrintHr(register_class_error);
    return false;
  }

  return true;
}

const std::string GetErrorReasonString(
    const MediaFoundationRenderer::ErrorReason& reason) {
#define STRINGIFY(value)                            \
  case MediaFoundationRenderer::ErrorReason::value: \
    return #value
  switch (reason) {
    STRINGIFY(kUnknown);
    STRINGIFY(kCdmProxyReceivedInInvalidState);
    STRINGIFY(kFailedToSetSourceOnMediaEngine);
    STRINGIFY(kFailedToSetCurrentTime);
    STRINGIFY(kFailedToPlay);
    STRINGIFY(kOnPlaybackError);
    STRINGIFY(kOnDCompSurfaceHandleSetError);
    STRINGIFY(kOnConnectionError);
    STRINGIFY(kFailedToSetDCompMode);
    STRINGIFY(kFailedToGetDCompSurface);
    STRINGIFY(kFailedToDuplicateHandle);
    STRINGIFY(kFailedToCreateMediaEngine);
    STRINGIFY(kFailedToCreateDCompTextureWrapper);
    STRINGIFY(kFailedToInitDCompTextureWrapper);
    STRINGIFY(kFailedToSetPlaybackRate);
    STRINGIFY(kFailedToGetMediaEngineEx);
  }
#undef STRINGIFY
}

// INVALID_HANDLE_VALUE is the official invalid handle value. Historically, 0 is
// not used as a handle value too.
bool IsInvalidHandle(const HANDLE& handle) {
  return handle == INVALID_HANDLE_VALUE || handle == nullptr;
}

}  // namespace

// static
void MediaFoundationRenderer::ReportErrorReason(ErrorReason reason) {
  base::UmaHistogramEnumeration("Media.MediaFoundationRenderer.ErrorReason",
                                reason);
}

MediaFoundationRenderer::MediaFoundationRenderer(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<MediaLog> media_log,
    LUID gpu_process_adapter_luid,
    bool force_dcomp_mode_for_testing)
    : task_runner_(std::move(task_runner)),
      media_log_(std::move(media_log)),
      gpu_process_adapter_luid_(gpu_process_adapter_luid),
      force_dcomp_mode_for_testing_(force_dcomp_mode_for_testing) {
  DVLOG_FUNC(1);
}

MediaFoundationRenderer::~MediaFoundationRenderer() {
  DVLOG_FUNC(1);

  // Perform shutdown/cleanup in the order (shutdown/detach/destroy) we wanted
  // without depending on the order of destructors being invoked. We also need
  // to invoke MFShutdown() after shutdown/cleanup of MF related objects.

  StopSendingStatistics();

  // 'mf_media_engine_notify_' should be shutdown first as errors are possible
  // if source is being created while shutdown is called (causing
  // ERROR_FILE_NOT_FOUND from Media Foundations). These errors should be
  // ignored by 'mf_media_engine_notify_' instead of being propagated up.
  if (mf_media_engine_notify_) {
    mf_media_engine_notify_->Shutdown();
  }
  if (mf_media_engine_extension_) {
    mf_media_engine_extension_->Shutdown();
  }
  if (mf_media_engine_) {
    mf_media_engine_->Shutdown();
  }

  if (mf_source_) {
    mf_source_->DetachResource();
  }

  if (dxgi_device_manager_) {
    dxgi_device_manager_.Reset();
    MFUnlockDXGIDeviceManager();
  }
  if (virtual_video_window_) {
    DestroyWindow(virtual_video_window_);
  }
}

void MediaFoundationRenderer::Initialize(MediaResource* media_resource,
                                         RendererClient* client,
                                         PipelineStatusCallback init_cb) {
  DVLOG_FUNC(1);

  renderer_client_ = client;

  // Check the rendering strategy & whether we're operating on clear or
  // protected content to determine the starting 'rendering_mode_'.
  // If the Direct Composition strategy is specified or if we're operating on
  // protected content then start in Direct Composition mode, else start in
  // Frame Server mode. This behavior must match the logic in
  // MediaFoundationRendererClient::Initialize.
  auto rendering_strategy = kMediaFoundationClearRenderingStrategyParam.Get();
  rendering_mode_ =
      rendering_strategy ==
              MediaFoundationClearRenderingStrategy::kDirectComposition
          ? MediaFoundationRenderingMode::DirectComposition
          : MediaFoundationRenderingMode::FrameServer;
  for (DemuxerStream* stream : media_resource->GetAllStreams()) {
    if (stream->type() == DemuxerStream::Type::VIDEO &&
        stream->video_decoder_config().is_encrypted()) {
      // This is protected content which only supports Direct Composition mode,
      // update 'rendering_mode_' accordingly.
      rendering_mode_ = MediaFoundationRenderingMode::DirectComposition;
    }
  }

  // debug, force mode to dcomp
  if (force_dcomp_mode_for_testing_) {
    rendering_mode_ = MediaFoundationRenderingMode::DirectComposition;
  }

  MEDIA_LOG(INFO, media_log_)
      << "Starting MediaFoundationRenderingMode: " << rendering_mode_;

  HRESULT hr = CreateMediaEngine(media_resource);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create media engine: " << PrintHr(hr);
    base::UmaHistogramSparse(
        "Media.MediaFoundationRenderer.CreateMediaEngineError", hr);
    OnError(PIPELINE_ERROR_INITIALIZATION_FAILED,
            ErrorReason::kFailedToCreateMediaEngine, hr, std::move(init_cb));
    return;
  }

  SetVolume(volume_);
  std::move(init_cb).Run(PIPELINE_OK);
}

HRESULT MediaFoundationRenderer::CreateMediaEngine(
    MediaResource* media_resource) {
  DVLOG_FUNC(1);

  if (!InitializeMediaFoundation())
    return kErrorInitializeMediaFoundation;

  // Set `cdm_proxy_` early on so errors can be reported via the CDM for better
  // error aggregation. See `CdmDocumentServiceImpl::OnCdmEvent()`.
  if (cdm_context_) {
    cdm_proxy_ = cdm_context_->GetMediaFoundationCdmProxy();
    if (!cdm_proxy_) {
      DLOG(ERROR) << __func__ << ": CDM does not support MF CDM interface";
      return kErrorInvalidCdmProxy;
    }
  }

  // Only call the following when there is a video stream.
  for (media::DemuxerStream* stream : media_resource->GetAllStreams()) {
    if (stream->type() == media::DemuxerStream::VIDEO) {
      RETURN_IF_FAILED(InitializeDXGIDeviceManager());
      RETURN_IF_FAILED(InitializeVirtualVideoWindow());
      break;
    }
  }

  // The OnXxx() callbacks are invoked by MF threadpool thread, we would like
  // to bind the callbacks to |task_runner_| MessageLoop.
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto weak_this = weak_factory_.GetWeakPtr();
  RETURN_IF_FAILED(MakeAndInitialize<MediaEngineNotifyImpl>(
      &mf_media_engine_notify_,
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &MediaFoundationRenderer::OnPlaybackError, weak_this)),
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &MediaFoundationRenderer::OnPlaybackEnded, weak_this)),
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &MediaFoundationRenderer::OnFormatChange, weak_this)),
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &MediaFoundationRenderer::OnLoadedData, weak_this)),
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &MediaFoundationRenderer::OnCanPlayThrough, weak_this)),
      base::BindPostTaskToCurrentDefault(
          base::BindRepeating(&MediaFoundationRenderer::OnPlaying, weak_this)),
      base::BindPostTaskToCurrentDefault(
          base::BindRepeating(&MediaFoundationRenderer::OnWaiting, weak_this)),
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &MediaFoundationRenderer::OnFrameStepCompleted, weak_this)),
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &MediaFoundationRenderer::OnTimeUpdate, weak_this))));

  ComPtr<IMFAttributes> creation_attributes;
  RETURN_IF_FAILED(MFCreateAttributes(&creation_attributes, 6));
  RETURN_IF_FAILED(creation_attributes->SetUnknown(
      MF_MEDIA_ENGINE_CALLBACK, mf_media_engine_notify_.Get()));
  RETURN_IF_FAILED(
      creation_attributes->SetUINT32(MF_MEDIA_ENGINE_CONTENT_PROTECTION_FLAGS,
                                     MF_MEDIA_ENGINE_ENABLE_PROTECTED_CONTENT));
  RETURN_IF_FAILED(creation_attributes->SetUINT32(
      MF_MEDIA_ENGINE_AUDIO_CATEGORY, AudioCategory_Media));
  if (virtual_video_window_) {
    RETURN_IF_FAILED(creation_attributes->SetUINT64(
        MF_MEDIA_ENGINE_OPM_HWND,
        reinterpret_cast<uint64_t>(virtual_video_window_)));
  }

  if (dxgi_device_manager_) {
    RETURN_IF_FAILED(creation_attributes->SetUnknown(
        MF_MEDIA_ENGINE_DXGI_MANAGER, dxgi_device_manager_.Get()));

    // TODO(crbug.com/40808656): We'll investigate scenarios to see if we can
    // use the on-screen video window size and not the native video size.
    if (rendering_mode_ == MediaFoundationRenderingMode::FrameServer) {
      gfx::Size max_video_size;
      bool has_video = false;
      for (media::DemuxerStream* stream : media_resource->GetAllStreams()) {
        if (stream->type() == media::DemuxerStream::VIDEO) {
          has_video = true;
          gfx::Size video_size = stream->video_decoder_config().natural_size();
          if (video_size.height() > max_video_size.height()) {
            max_video_size.set_height(video_size.height());
          }

          if (video_size.width() > max_video_size.width()) {
            max_video_size.set_width(video_size.width());
          }
        }
      }

      if (has_video) {
        RETURN_IF_FAILED(InitializeTexturePool(max_video_size));
      }
    }
  }

  RETURN_IF_FAILED(
      MakeAndInitialize<MediaEngineExtension>(&mf_media_engine_extension_));
  RETURN_IF_FAILED(creation_attributes->SetUnknown(
      MF_MEDIA_ENGINE_EXTENSION, mf_media_engine_extension_.Get()));

  ComPtr<IMFMediaEngineClassFactory> class_factory;
  RETURN_IF_FAILED(CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&class_factory)));

  DWORD creation_flags = 0;
  // Enable low-latency mode if latency hint is low.
  if (latency_hint_.has_value() && (*latency_hint_ <= base::Milliseconds(50))) {
    creation_flags |= MF_MEDIA_ENGINE_REAL_TIME_MODE;
  }
  RETURN_IF_FAILED(class_factory->CreateInstance(
      creation_flags, creation_attributes.Get(), &mf_media_engine_));

  // The Media Foundation Media Engine has an initial playback rate of 1.0, but
  // chromium uses an initial playback rate of 0.0. The Media Engine's topology
  // may not be completely loaded at this point - so we use
  // SetDefaultPlaybackRate as using SetPlaybackRate may be overwritten while
  // the topology is loading.
  RETURN_IF_FAILED(mf_media_engine_->SetDefaultPlaybackRate(0.0));

  auto media_resource_type_ = media_resource->GetType();
  if (media_resource_type_ != MediaResource::Type::kStream) {
    DLOG(ERROR) << "MediaResource is not of STREAM";
    return E_INVALIDARG;
  }

  RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationSourceWrapper>(
      &mf_source_, media_resource, media_log_.get(), task_runner_));

  if (force_dcomp_mode_for_testing_)
    std::ignore = SetDCompModeInternal();

  if (!mf_source_->HasEncryptedStream()) {
    // Supports clear stream for testing.
    return SetSourceOnMediaEngine();
  }

  // Has encrypted stream.
  RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationProtectionManager>(
      &content_protection_manager_, task_runner_,
      base::BindRepeating(&MediaFoundationRenderer::OnProtectionManagerWaiting,
                          weak_factory_.GetWeakPtr())));
  ComPtr<IMFMediaEngineProtectedContent> protected_media_engine;
  RETURN_IF_FAILED(mf_media_engine_.As(&protected_media_engine));
  RETURN_IF_FAILED(protected_media_engine->SetContentProtectionManager(
      content_protection_manager_.Get()));

  waiting_for_mf_cdm_ = true;
  if (!cdm_context_) {
    DCHECK(!cdm_proxy_);
    return S_OK;
  }

  DCHECK(cdm_proxy_);
  OnCdmProxyReceived();
  return S_OK;
}

HRESULT MediaFoundationRenderer::SetSourceOnMediaEngine() {
  DVLOG_FUNC(1);

  if (!mf_source_) {
    LOG(ERROR) << "mf_source_ is null.";
    return HRESULT_FROM_WIN32(ERROR_INVALID_STATE);
  }

  ComPtr<IUnknown> source_unknown;
  RETURN_IF_FAILED(mf_source_.As(&source_unknown));
  RETURN_IF_FAILED(
      mf_media_engine_extension_->SetMediaSource(source_unknown.Get()));

  DVLOG(2) << "Set MFRendererSrc scheme as the source for MFMediaEngine.";
  base::win::ScopedBstr mf_renderer_source_scheme(
      base::ASCIIToWide("MFRendererSrc"));
  // We need to set our source scheme first in order for the MFMediaEngine to
  // load of our custom MFMediaSource.
  RETURN_IF_FAILED(
      mf_media_engine_->SetSource(mf_renderer_source_scheme.Get()));

  return S_OK;
}

HRESULT MediaFoundationRenderer::InitializeDXGIDeviceManager() {
  UINT device_reset_token;
  RETURN_IF_FAILED(
      MFLockDXGIDeviceManager(&device_reset_token, &dxgi_device_manager_));
  // `dxgi_device_manager_` returned is a singleton object, thus all
  // MediaFoundationRenderer instances will all receive the
  // `dxgi_device_manager_` pointing to the same object. Therefore we only need
  // to and can only call `ResetDevice()` once, If it's called more than once,
  // all open device handles become invalid, even when it is the same device as
  // before. This will cause an existing instance attempting to use the invalid
  // handle to error out.
  // https://learn.microsoft.com/en-us/windows/win32/api/mfobjects/nf-mfobjects-imfdxgidevicemanager-resetdevice
  DXGIDeviceScopedHandle dxgi_device_handle(dxgi_device_manager_.Get());
  if (dxgi_device_handle.GetDevice()) {
    return S_OK;
  }

  ComPtr<ID3D11Device> d3d11_device;
  UINT creation_flags =
      (D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT |
       D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS);
  static const D3D_FEATURE_LEVEL feature_levels[] = {
      D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,
      D3D_FEATURE_LEVEL_9_1};

  Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
  RETURN_IF_FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));

  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_to_use;
  // TODO(crbug.com/40899242): Need to handle the case when Adapter LUID is
  // specific per instance of the video playback. This will now allow all
  // instances to use the default DXGI device manager.
  if (gpu_process_adapter_luid_.LowPart || gpu_process_adapter_luid_.HighPart) {
    Microsoft::WRL::ComPtr<IDXGIAdapter> temp_adapter;
    for (UINT i = 0; SUCCEEDED(factory->EnumAdapters(i, &temp_adapter)); i++) {
      DXGI_ADAPTER_DESC desc;
      RETURN_IF_FAILED(temp_adapter->GetDesc(&desc));
      if (desc.AdapterLuid.LowPart == gpu_process_adapter_luid_.LowPart &&
          desc.AdapterLuid.HighPart == gpu_process_adapter_luid_.HighPart) {
        adapter_to_use = std::move(temp_adapter);
        break;
      }
    }
  }

  HRESULT hr = D3D11CreateDevice(
      adapter_to_use.Get(),
      adapter_to_use ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, 0,
      creation_flags, feature_levels, std::size(feature_levels),
      D3D11_SDK_VERSION, &d3d11_device, nullptr, nullptr);
  if (FAILED(hr)) {
    base::UmaHistogramSparse(
        "Media.MediaFoundationRenderer.D3D11CreateDeviceFailed", hr);
    if (hr == DXGI_ERROR_UNSUPPORTED) {
      // If hardware device creation fails, try creating a software device.
      // HWDRM cases require hardware security, which is not applicable for a
      // basic software GPU adapter without hardware-level security. Using 0 for
      // creation_flags is acceptable for basic video rendering, as warp devices
      // lack video support, and the warp adapter is a software GPU so
      // D3D11_CREATE_DEVICE_BGRA_SUPPORT and
      // D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS don't
      // apply.
      RETURN_IF_FAILED(D3D11CreateDevice(
          adapter_to_use.Get(),
          adapter_to_use ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
          0,
          /*creation_flags=*/0, feature_levels, std::size(feature_levels),
          D3D11_SDK_VERSION, &d3d11_device, nullptr, nullptr));
    } else {
      RETURN_IF_FAILED(hr);
    }
  }
  RETURN_IF_FAILED(media::SetDebugName(d3d11_device.Get(), "Media_MFRenderer"));

  ComPtr<ID3D10Multithread> multithreaded_device;
  RETURN_IF_FAILED(d3d11_device.As(&multithreaded_device));
  multithreaded_device->SetMultithreadProtected(TRUE);

  return dxgi_device_manager_->ResetDevice(d3d11_device.Get(),
                                           device_reset_token);
}

HRESULT MediaFoundationRenderer::InitializeVirtualVideoWindow() {
  if (!InitializeVideoWindowClass())
    return kErrorInitializeVideoWindowClass;

  virtual_video_window_ =
      CreateWindowEx(WS_EX_NOPARENTNOTIFY | WS_EX_LAYERED | WS_EX_TRANSPARENT |
                         WS_EX_NOREDIRECTIONBITMAP,
                     reinterpret_cast<wchar_t*>(g_video_window_class), L"",
                     WS_POPUP | WS_DISABLED | WS_CLIPSIBLINGS, 0, 0, 1, 1,
                     nullptr, nullptr, nullptr, nullptr);
  if (!virtual_video_window_) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    DLOG(ERROR) << "Failed to create virtual window: " << PrintHr(hr);
    return hr;
  }

  return S_OK;
}

void MediaFoundationRenderer::SetCdm(CdmContext* cdm_context,
                                     CdmAttachedCB cdm_attached_cb) {
  DVLOG_FUNC(1);

  if (cdm_context_ || !cdm_context) {
    DLOG(ERROR) << "Failed in checking CdmContext.";
    std::move(cdm_attached_cb).Run(false);
    return;
  }

  cdm_context_ = cdm_context;

  if (waiting_for_mf_cdm_) {
    cdm_proxy_ = cdm_context_->GetMediaFoundationCdmProxy();
    if (!cdm_proxy_) {
      DLOG(ERROR) << "CDM does not support MediaFoundationCdmProxy";
      std::move(cdm_attached_cb).Run(false);
      return;
    }

    OnCdmProxyReceived();
  }

  std::move(cdm_attached_cb).Run(true);
}

void MediaFoundationRenderer::SetLatencyHint(
    std::optional<base::TimeDelta> latency_hint) {
  DVLOG_FUNC(1);

  if (latency_hint.has_value()) {
    DLOG_IF(WARNING, mf_media_engine_)
        << "Latency hint is not utilized after MF media engine creation.";
    CHECK(*latency_hint >= base::Milliseconds(0));
  }
  latency_hint_ = latency_hint;
}

void MediaFoundationRenderer::OnCdmProxyReceived() {
  DVLOG_FUNC(1);
  DCHECK(cdm_proxy_);

  if (!waiting_for_mf_cdm_ || !content_protection_manager_) {
    OnError(PIPELINE_ERROR_INVALID_STATE,
            ErrorReason::kCdmProxyReceivedInInvalidState,
            kErrorCdmProxyReceivedInInvalidState);
    return;
  }

  waiting_for_mf_cdm_ = false;
  content_protection_manager_->SetCdmProxy(cdm_proxy_);
  mf_source_->SetCdmProxy(cdm_proxy_);

  HRESULT hr = SetSourceOnMediaEngine();
  if (FAILED(hr)) {
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER,
            ErrorReason::kFailedToSetSourceOnMediaEngine, hr);
    return;
  }
}

void MediaFoundationRenderer::Flush(base::OnceClosure flush_cb) {
  DVLOG_FUNC(2);

  HRESULT hr = PauseInternal();
  // Ignore any Pause() error. We can continue to flush |mf_source_| instead of
  // stopping the playback with error.
  DVLOG_IF(1, FAILED(hr)) << "Failed to pause playback on flush: "
                          << PrintHr(hr);

  mf_source_->FlushStreams();
  std::move(flush_cb).Run();
}

void MediaFoundationRenderer::SetMediaFoundationRenderingMode(
    MediaFoundationRenderingMode render_mode) {
  ComPtr<IMFMediaEngineEx> mf_media_engine_ex;
  HRESULT hr = mf_media_engine_.As(&mf_media_engine_ex);

  if (mf_media_engine_->HasVideo()) {
    if (render_mode == MediaFoundationRenderingMode::FrameServer) {
      // cannot change to frameserver if force_dcomp_mode_for_testing_ is true
      DCHECK(!force_dcomp_mode_for_testing_);

      // Make sure we reinitialize the texture pool
      hr = InitializeTexturePool(native_video_size_);
    } else if (render_mode == MediaFoundationRenderingMode::DirectComposition) {
      // If needed renegotiate the DComp visual and send it to the client for
      // presentation
    } else {
      DVLOG(1) << "Rendering mode: " << static_cast<int>(render_mode)
               << " is unsupported";
      MEDIA_LOG(ERROR, media_log_)
          << "MediaFoundationRenderer SetMediaFoundationRenderingMode: "
          << static_cast<int>(render_mode)
          << " is not defined. No change to the rendering mode.";
      hr = E_NOT_SET;
    }

    if (SUCCEEDED(hr)) {
      hr = mf_media_engine_ex->EnableWindowlessSwapchainMode(
          render_mode == MediaFoundationRenderingMode::DirectComposition);
      if (SUCCEEDED(hr)) {
        rendering_mode_ = render_mode;
        MEDIA_LOG(INFO, media_log_)
            << "Set MediaFoundationRenderingMode: " << rendering_mode_;
      }
    }
  }
}

bool MediaFoundationRenderer::InFrameServerMode() {
  return rendering_mode_ == MediaFoundationRenderingMode::FrameServer;
}

void MediaFoundationRenderer::StartPlayingFrom(base::TimeDelta time) {
  double current_time = time.InSecondsF();
  DVLOG_FUNC(2) << "current_time=" << current_time;

  // Note: It is okay for |waiting_for_mf_cdm_| to be true here. The
  // MFMediaEngine supports calls to Play/SetCurrentTime before a source is set
  // (it will apply the relevant changes to the playback state once a source is
  // set on it).

  // SetCurrentTime() completes asynchronously. When the seek operation starts,
  // the MFMediaEngine sends an MF_MEDIA_ENGINE_EVENT_SEEKING event. When the
  // seek operation completes, the MFMediaEngine sends an
  // MF_MEDIA_ENGINE_EVENT_SEEKED event.
  HRESULT hr = mf_media_engine_->SetCurrentTime(current_time);
  if (FAILED(hr)) {
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER,
            ErrorReason::kFailedToSetCurrentTime, hr);
    return;
  }

  hr = mf_media_engine_->Play();
  if (FAILED(hr)) {
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER, ErrorReason::kFailedToPlay, hr);
    return;
  }
}

void MediaFoundationRenderer::SetPlaybackRate(double playback_rate) {
  DVLOG_FUNC(2) << "playback_rate=" << playback_rate;

  // If the Media Engine's topology has not finished loading then
  // the call to SetPlaybackRate may be overwritten. To work around this
  // we call SetDefaultPlaybackRate which would be picked up when transitioning
  // to the Play state.
  HRESULT hr = mf_media_engine_->SetDefaultPlaybackRate(playback_rate);
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to set default playback rate: " << PrintHr(hr);
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER,
            ErrorReason::kFailedToSetPlaybackRate, hr);
    return;
  }

  hr = mf_media_engine_->SetPlaybackRate(playback_rate);

  if (SUCCEEDED(hr)) {
    playback_rate_ = playback_rate;
  } else {
    DVLOG_IF(1, FAILED(hr)) << "Failed to set playback rate: " << PrintHr(hr);
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER,
            ErrorReason::kFailedToSetPlaybackRate, hr);
  }
}

void MediaFoundationRenderer::GetDCompSurface(GetDCompSurfaceCB callback) {
  DVLOG_FUNC(1);

  HRESULT hr = SetDCompModeInternal();
  if (FAILED(hr)) {
    base::UmaHistogramSparse(
        "Media.MediaFoundationRenderer.FailedToSetDCompMode", hr);
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER, ErrorReason::kFailedToSetDCompMode,
            hr);
    std::move(callback).Run(base::win::ScopedHandle(), PrintHr(hr));
    return;
  }

  HANDLE surface_handle = INVALID_HANDLE_VALUE;
  hr = GetDCompSurfaceInternal(&surface_handle);
  // The handle could still be invalid after a non failure (e.g. S_FALSE) is
  // returned. See https://crbug.com/1307065.
  if (FAILED(hr) || IsInvalidHandle(surface_handle)) {
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER,
            ErrorReason::kFailedToGetDCompSurface, hr);
    std::move(callback).Run(base::win::ScopedHandle(), PrintHr(hr));
    return;
  }

  // Only need read & execute access right for the handle to be duplicated
  // without breaking in sandbox_win.cc!CheckDuplicateHandle().
  const base::ProcessHandle process = ::GetCurrentProcess();
  HANDLE duplicated_handle = INVALID_HANDLE_VALUE;
  const BOOL result = ::DuplicateHandle(
      process, surface_handle, process, &duplicated_handle,
      GENERIC_READ | GENERIC_EXECUTE, false, DUPLICATE_CLOSE_SOURCE);
  if (!result || IsInvalidHandle(surface_handle)) {
    hr = ::GetLastError();
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER,
            ErrorReason::kFailedToDuplicateHandle, hr);
    std::move(callback).Run(base::win::ScopedHandle(), PrintHr(hr));
    return;
  }

  std::move(callback).Run(base::win::ScopedHandle(duplicated_handle), "");
}

// TODO(crbug.com/40126181): Investigate if we need to add
// OnSelectedVideoTracksChanged() to media renderer.mojom.
void MediaFoundationRenderer::SetVideoStreamEnabled(bool enabled) {
  DVLOG_FUNC(1) << "enabled=" << enabled;
  if (!mf_source_)
    return;

  const bool needs_restart = mf_source_->SetVideoStreamEnabled(enabled);
  if (needs_restart) {
    // If the media source indicates that we need to restart playback (e.g due
    // to a newly enabled stream being EOS), queue a pause and play operation.
    PauseInternal();
    mf_media_engine_->Play();
  }
}

void MediaFoundationRenderer::SetOutputRect(const gfx::Rect& output_rect,
                                            SetOutputRectCB callback) {
  DVLOG_FUNC(2);

  // Call SetWindowPos to reposition the video from output_rect.
  if (virtual_video_window_ &&
      !::SetWindowPos(virtual_video_window_, HWND_BOTTOM, output_rect.x(),
                      output_rect.y(), output_rect.width(),
                      output_rect.height(), SWP_NOACTIVATE)) {
    DLOG(ERROR) << "Failed to SetWindowPos: "
                << PrintHr(HRESULT_FROM_WIN32(GetLastError()));
    std::move(callback).Run(false);
    return;
  }

  if (FAILED(UpdateVideoStream(output_rect.size()))) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

HRESULT MediaFoundationRenderer::InitializeTexturePool(const gfx::Size& size) {
  DXGIDeviceScopedHandle dxgi_device_handle(dxgi_device_manager_.Get());
  ComPtr<ID3D11Device> d3d11_device = dxgi_device_handle.GetDevice();

  if (d3d11_device.Get() == nullptr) {
    return E_UNEXPECTED;
  }

  // TODO(crbug.com/40808656): change |size| to instead use the required
  // size of the output (for example if the video is only 1280x720 instead
  // of a source frame of 1920x1080 we'd use the 1280x720 texture size).
  // However we also need to investigate the scenario of WebGL and 360 video
  // where they need the original frame size instead of the window size due
  // to later image processing.
  RETURN_IF_FAILED(texture_pool_.Initialize(d3d11_device.Get(),
                                            initialized_frame_pool_cb_, size));

  return S_OK;
}

HRESULT MediaFoundationRenderer::UpdateVideoStream(const gfx::Size rect_size) {
  if (current_video_rect_size_ == rect_size) {
    return S_OK;
  }

  current_video_rect_size_ = rect_size;

  ComPtr<IMFMediaEngineEx> mf_media_engine_ex;
  RETURN_IF_FAILED(mf_media_engine_.As(&mf_media_engine_ex));

  RECT dest_rect = {0, 0, rect_size.width(), rect_size.height()};

  // https://learn.microsoft.com/en-us/windows/win32/api/mfmediaengine/nf-mfmediaengine-imfmediaengineex-updatevideostream
  // Updates the source rectangle, destination rectangle, and border color for
  // the video. Source is set to Null so the entire frame is displayed.
  // Position is not set because SetWindowPos sets the position already.
  // Destination rectangle relative to the top-left corner of the window
  // rect set in SetWindowPos.
  RETURN_IF_FAILED(mf_media_engine_ex->UpdateVideoStream(
      /*pSrc=*/nullptr, &dest_rect, /*pBorderClr=*/nullptr));
  if (rendering_mode_ == MediaFoundationRenderingMode::FrameServer) {
    RETURN_IF_FAILED(InitializeTexturePool(native_video_size_));
  }
  return S_OK;
}

HRESULT MediaFoundationRenderer::SetDCompModeInternal() {
  DVLOG_FUNC(1);

  ComPtr<IMFMediaEngineEx> media_engine_ex;
  RETURN_IF_FAILED(mf_media_engine_.As(&media_engine_ex));
  RETURN_IF_FAILED(media_engine_ex->EnableWindowlessSwapchainMode(true));
  return S_OK;
}

HRESULT MediaFoundationRenderer::GetDCompSurfaceInternal(
    HANDLE* surface_handle) {
  DVLOG_FUNC(1);

  ComPtr<IMFMediaEngineEx> media_engine_ex;
  RETURN_IF_FAILED(mf_media_engine_.As(&media_engine_ex));
  RETURN_IF_FAILED(media_engine_ex->GetVideoSwapchainHandle(surface_handle));
  return S_OK;
}

HRESULT MediaFoundationRenderer::PopulateStatistics(
    PipelineStatistics& statistics) {
  ComPtr<IMFMediaEngineEx> media_engine_ex;
  RETURN_IF_FAILED(mf_media_engine_.As(&media_engine_ex));

  base::win::ScopedPropVariant frames_rendered;
  RETURN_IF_FAILED(media_engine_ex->GetStatistics(
      MF_MEDIA_ENGINE_STATISTIC_FRAMES_RENDERED, frames_rendered.Receive()));
  base::win::ScopedPropVariant frames_dropped;
  RETURN_IF_FAILED(media_engine_ex->GetStatistics(
      MF_MEDIA_ENGINE_STATISTIC_FRAMES_DROPPED, frames_dropped.Receive()));

  statistics.video_frames_decoded =
      frames_rendered.get().ulVal + frames_dropped.get().ulVal;
  statistics.video_frames_dropped = frames_dropped.get().ulVal;
  DVLOG_FUNC(3) << "video_frames_decoded=" << statistics.video_frames_decoded
                << ", video_frames_dropped=" << statistics.video_frames_dropped;
  return S_OK;
}

void MediaFoundationRenderer::SendStatistics() {
  PipelineStatistics new_stats = {};
  HRESULT hr = PopulateStatistics(new_stats);
  if (FAILED(hr)) {
    LIMITED_MEDIA_LOG(INFO, media_log_, populate_statistics_failure_count_, 3)
        << "MediaFoundationRenderer failed to populate stats: " + PrintHr(hr);
    return;
  }

  const int kSignificantPlaybackFrames = 5400;  // About 30 fps for 3 minutes.
  if (!has_reported_significant_playback_ && cdm_proxy_ &&
      new_stats.video_frames_decoded >= kSignificantPlaybackFrames) {
    has_reported_significant_playback_ = true;
    cdm_proxy_->OnSignificantPlayback();
  }

  if (statistics_ != new_stats) {
    // OnStatisticsUpdate() expects delta values.
    PipelineStatistics delta;
    delta.video_frames_decoded = base::ClampSub(
        new_stats.video_frames_decoded, statistics_.video_frames_decoded);
    delta.video_frames_dropped = base::ClampSub(
        new_stats.video_frames_dropped, statistics_.video_frames_dropped);
    statistics_ = new_stats;
    renderer_client_->OnStatisticsUpdate(delta);
  }
}

void MediaFoundationRenderer::StartSendingStatistics() {
  DVLOG_FUNC(2);

  // Clear `statistics_` to reset the base for OnStatisticsUpdate(), this is
  // needed since flush will clear the internal stats in MediaFoundation.
  statistics_ = PipelineStatistics();

  const auto kPipelineStatsPollingPeriod = base::Milliseconds(500);
  statistics_timer_.Start(FROM_HERE, kPipelineStatsPollingPeriod, this,
                          &MediaFoundationRenderer::SendStatistics);
}

void MediaFoundationRenderer::StopSendingStatistics() {
  DVLOG_FUNC(2);
  statistics_timer_.Stop();
}

void MediaFoundationRenderer::SetVolume(float volume) {
  DVLOG_FUNC(2) << "volume=" << volume;
  volume_ = volume;
  if (!mf_media_engine_)
    return;

  HRESULT hr = mf_media_engine_->SetVolume(volume_);
  DVLOG_IF(1, FAILED(hr)) << "Failed to set volume: " << PrintHr(hr);
}

void MediaFoundationRenderer::SetFrameReturnCallbacks(
    FrameReturnCallback frame_available_cb,
    FramePoolInitializedCallback initialized_frame_pool_cb) {
  frame_available_cb_ = std::move(frame_available_cb);
  initialized_frame_pool_cb_ = std::move(initialized_frame_pool_cb);
}

void MediaFoundationRenderer::SetGpuProcessAdapterLuid(
    LUID gpu_process_adapter_luid) {
  // TODO(wicarr, crbug.com/1342621): When the GPU adapter changes or the GPU
  // process is restarted we need to recover our Frame Server or DComp
  // textures, otherwise we'll fail to present any video frames to the user.
  gpu_process_adapter_luid_ = gpu_process_adapter_luid;
}

base::TimeDelta MediaFoundationRenderer::GetMediaTime() {
// GetCurrentTime is expanded as GetTickCount in base/win/windows_types.h
#undef GetCurrentTime
  double current_time = mf_media_engine_->GetCurrentTime();
// Restore macro definition.
#define GetCurrentTime() GetTickCount()
  auto media_time = base::Seconds(current_time);
  DVLOG_FUNC(3) << "media_time=" << media_time;
  return media_time;
}

RendererType MediaFoundationRenderer::GetRendererType() {
  return RendererType::kMediaFoundation;
}

void MediaFoundationRenderer::OnPlaybackError(PipelineStatus status,
                                              HRESULT hr) {
  DVLOG_FUNC(1) << "status=" << status << ", hr=" << hr;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  base::UmaHistogramSparse("Media.MediaFoundationRenderer.PlaybackError", hr);

  StopSendingStatistics();
  OnError(status, ErrorReason::kOnPlaybackError, hr);
}

void MediaFoundationRenderer::OnPlaybackEnded() {
  DVLOG_FUNC(2);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  StopSendingStatistics();
  renderer_client_->OnEnded();
}

void MediaFoundationRenderer::OnFormatChange() {
  DVLOG_FUNC(2);
  OnVideoNaturalSizeChange();
}

void MediaFoundationRenderer::OnLoadedData() {
  DVLOG_FUNC(2);

  // According to HTML5 <video> spec, on "loadeddata", the first frame is
  // available for the first time, so we can report natural size change and
  // set up the dcomp frame.
  OnVideoNaturalSizeChange();
}

void MediaFoundationRenderer::OnCanPlayThrough() {
  DVLOG_FUNC(2);

  // If the playback rate in Media Foundations is 0, the video renderer would
  // not pre-roll and request frames. Use Frame Step function to force
  // pre-rolling
  if (playback_rate_ == 0) {
    ComPtr<IMFMediaEngineEx> mf_media_engine_ex;

    HRESULT hr = mf_media_engine_.As(&mf_media_engine_ex);
    if (SUCCEEDED(hr)) {
      mf_media_engine_ex->FrameStep(/*Forward=*/true);
    } else {
      OnError(PIPELINE_ERROR_COULD_NOT_RENDER,
              ErrorReason::kFailedToGetMediaEngineEx, hr);
      return;
    }
  }

  // According to HTML5 <video> spec, on "canplaythrough", the video could be
  // rendered at the current playback rate all the way to its end, and it's
  // the time to report BUFFERING_HAVE_ENOUGH.
  OnBufferingStateChange(
      BufferingState::BUFFERING_HAVE_ENOUGH,
      BufferingStateChangeReason::BUFFERING_CHANGE_REASON_UNKNOWN);
}

void MediaFoundationRenderer::OnPlaying() {
  DVLOG_FUNC(2);

  has_reported_playing_ = true;

  OnBufferingStateChange(
      BufferingState::BUFFERING_HAVE_ENOUGH,
      BufferingStateChangeReason::BUFFERING_CHANGE_REASON_UNKNOWN);

  // Earliest time to request first frame to screen
  RequestNextFrame();

  // The OnPlaying callback from MediaEngineNotifyImpl lets us know that an
  // MF_MEDIA_ENGINE_EVENT_PLAYING message has been received. At this point we
  // can safely start sending Statistics as any asynchronous Flush action in
  // media engine, which would have reset the engine's statistics, will have
  // been completed.
  StartSendingStatistics();
}

void MediaFoundationRenderer::OnWaiting() {
  DVLOG_FUNC(2);
  OnBufferingStateChange(
      BufferingState::BUFFERING_HAVE_NOTHING,
      BufferingStateChangeReason::BUFFERING_CHANGE_REASON_UNKNOWN);
}

void MediaFoundationRenderer::OnTimeUpdate() {
  DVLOG_FUNC(3);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

void MediaFoundationRenderer::OnFrameStepCompleted() {
  DVLOG_FUNC(2);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Frame-Stepping causes Media engine to be in a paused state after finishing.
  // Thus play and set playback rate is needed to change the state to be
  // playing.

  // Set playback rate is call again because on start, if SetPlaybackRate of 0
  // is called before pipeline topology is setup, the playback rate of Media
  // Engine will be defaulted to 1 as setting playback rate is ignored until
  // topology is set. Thus, when frame step is finished, setting the playback
  // rate again ensures consistency.
  HRESULT hr = mf_media_engine_->Play();
  if (FAILED(hr)) {
    OnError(PIPELINE_ERROR_COULD_NOT_RENDER, ErrorReason::kFailedToPlay, hr);
    return;
  }
  SetPlaybackRate(playback_rate_);
}

void MediaFoundationRenderer::OnProtectionManagerWaiting(WaitingReason reason) {
  DVLOG_FUNC(2);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  renderer_client_->OnWaiting(reason);
}

void MediaFoundationRenderer::OnBufferingStateChange(
    BufferingState state,
    BufferingStateChangeReason reason) {
  DVLOG_FUNC(2);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (state == BufferingState::BUFFERING_HAVE_ENOUGH) {
    max_buffering_state_ = state;
  }

  if (state == BufferingState::BUFFERING_HAVE_NOTHING &&
      max_buffering_state_ != BufferingState::BUFFERING_HAVE_ENOUGH) {
    // Prevent sending BUFFERING_HAVE_NOTHING if we haven't previously sent a
    // BUFFERING_HAVE_ENOUGH state.
    return;
  }

  DVLOG_FUNC(2) << "state=" << state << ", reason=" << reason;
  renderer_client_->OnBufferingStateChange(state, reason);
}

HRESULT MediaFoundationRenderer::PauseInternal() {
  // Media Engine resets aggregate statistics when it flushes - such as a
  // transition to the Pause state & then back to Play state. To try and
  // avoid cases where we may get Media Engine's reset statistics call
  // StopSendingStatistics before transitioning to Pause.
  StopSendingStatistics();
  return mf_media_engine_->Pause();
}

void MediaFoundationRenderer::OnVideoNaturalSizeChange() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const bool has_video = mf_media_engine_->HasVideo();
  DVLOG_FUNC(2) << "has_video=" << has_video;

  // Skip if there are no video streams. This can happen because this is
  // originated from MF_MEDIA_ENGINE_EVENT_FORMATCHANGE.
  if (!has_video)
    return;

  DWORD native_width;
  DWORD native_height;
  HRESULT hr =
      mf_media_engine_->GetNativeVideoSize(&native_width, &native_height);
  if (FAILED(hr)) {
    // TODO(xhwang): Add UMA to probe if this can happen.
    DLOG(ERROR) << "Failed to get native video size from MediaEngine, using "
                   "default (640x320). hr="
                << hr;
    native_video_size_ = {640, 320};
  } else {
    native_video_size_ = {base::checked_cast<int>(native_width),
                          base::checked_cast<int>(native_height)};
  }

  // TODO(frankli): Let test code to call `UpdateVideoStream()`.
  if (force_dcomp_mode_for_testing_) {
    const gfx::Size test_size(/*width=*/640, /*height=*/320);
    // This invokes IMFMediaEngineEx::UpdateVideoStream() for video frames to
    // be presented. Otherwise, the Media Foundation video renderer will not
    // request video samples from our source.
    std::ignore = UpdateVideoStream(test_size);
  }

  if (rendering_mode_ == MediaFoundationRenderingMode::FrameServer) {
    InitializeTexturePool(native_video_size_);
  }

  renderer_client_->OnVideoNaturalSizeChange(native_video_size_);
}

void MediaFoundationRenderer::OnError(PipelineStatus status,
                                      ErrorReason reason,
                                      HRESULT hresult,
                                      PipelineStatusCallback status_cb) {
  const std::string error =
      "MediaFoundationRenderer error: " + GetErrorReasonString(reason) + " (" +
      PrintHr(hresult) + ")";

  DLOG(ERROR) << error;

  // Report to MediaLog so the error will show up in media internals and
  // MediaError.message.
  MEDIA_LOG(ERROR, media_log_) << error;

  // Report the error to UMA.
  ReportErrorReason(reason);

  // DRM_E_TEE_INVALID_HWDRM_STATE can happen during OS sleep/resume, or moving
  // video to different graphics adapters. This is not an error, so special case
  // it here.
  PipelineStatus new_status = status;
  if (hresult == DRM_E_TEE_INVALID_HWDRM_STATE) {
    // TODO(crbug.com/40870069): Remove these after the investigation is done.
    base::UmaHistogramBoolean(
        "Media.MediaFoundationRenderer.InvalidHwdrmState.HasReportedPlaying",
        has_reported_playing_);
    base::UmaHistogramCounts10000(
        "Media.MediaFoundationRenderer.InvalidHwdrmState.VideoFrameDecoded",
        statistics_.video_frames_decoded);

    new_status = PIPELINE_ERROR_HARDWARE_CONTEXT_RESET;
    if (cdm_proxy_)
      cdm_proxy_->OnHardwareContextReset();
  } else if (cdm_proxy_) {
    cdm_proxy_->OnPlaybackError(hresult);
  }

  // Attach hresult to `new_status` for logging and metrics reporting.
  new_status.WithData("hresult", static_cast<uint32_t>(hresult));

  if (status_cb)
    std::move(status_cb).Run(new_status);
  else
    renderer_client_->OnError(new_status);
}

void MediaFoundationRenderer::RequestNextFrame() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (rendering_mode_ != MediaFoundationRenderingMode::FrameServer) {
    return;
  }

  LONGLONG presentation_timestamp_in_hns = 0;
  // OnVideoStreamTick can return S_FALSE if there is no frame available.
  if (dxgi_device_manager_ == nullptr ||
      mf_media_engine_->OnVideoStreamTick(&presentation_timestamp_in_hns) !=
          S_OK) {
    return;
  }

  if (native_video_size_.IsEmpty()) {
    MEDIA_LOG(WARNING, media_log_)
        << "RequestNextFrame ignores empty native_video_size_";
    return;
  }

  // TODO(crbug.com/40808656): Change the |native_video_size_| to get the
  // correct output video size as determined by the output texture requirements.
  gfx::Size video_size = native_video_size_;

  base::UnguessableToken frame_token;
  auto d3d11_video_frame = texture_pool_.AcquireTexture(&frame_token);
  if (d3d11_video_frame.Get() == nullptr)
    return;

  RECT destination_frame_size = {0, 0, video_size.width(), video_size.height()};

  ComPtr<IDXGIKeyedMutex> texture_mutex;
  d3d11_video_frame.As(&texture_mutex);

  if (texture_mutex->AcquireSync(0, INFINITE) != S_OK) {
    texture_pool_.ReleaseTexture(frame_token);
    return;
  }

  if (FAILED(mf_media_engine_->TransferVideoFrame(
          d3d11_video_frame.Get(), nullptr, &destination_frame_size,
          nullptr))) {
    texture_mutex->ReleaseSync(0);
    texture_pool_.ReleaseTexture(frame_token);
    return;
  }
  texture_mutex->ReleaseSync(0);

// Need access to GetCurrentTime on the Media Engine.
#undef GetCurrentTime
  auto frame_timestamp = base::Seconds(mf_media_engine_->GetCurrentTime());
// Restore previous definition
#define GetCurrentTime() GetTickCount()
  frame_available_cb_.Run(frame_token, video_size, frame_timestamp);
}

void MediaFoundationRenderer::NotifyFrameReleased(
    const base::UnguessableToken& frame_token) {
  texture_pool_.ReleaseTexture(frame_token);
}

}  // namespace media
