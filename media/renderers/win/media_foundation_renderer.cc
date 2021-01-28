// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/win/media_foundation_renderer.h"

#include <Audioclient.h>
#include <mferror.h>
#include <memory>
#include <string>

#include "base/callback_helpers.h"
#include "base/guid.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/windows_version.h"
#include "base/win/wrapped_window_proc.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_context.h"
#include "media/base/timestamp_constants.h"
#include "media/base/win/mf_helpers.h"

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

}  // namespace

// static
bool MediaFoundationRenderer::IsSupported() {
  return base::win::GetVersion() >= base::win::Version::WIN10;
}

MediaFoundationRenderer::MediaFoundationRenderer(
    bool muted,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    bool force_dcomp_mode_for_testing)
    : muted_(muted),
      task_runner_(std::move(task_runner)),
      force_dcomp_mode_for_testing_(force_dcomp_mode_for_testing) {
  DVLOG_FUNC(1);
}

MediaFoundationRenderer::~MediaFoundationRenderer() {
  DVLOG_FUNC(1);

  // Perform shutdown/cleanup in the order (shutdown/detach/destroy) we wanted
  // without depending on the order of destructors being invoked. We also need
  // to invoke MFShutdown() after shutdown/cleanup of MF related objects.

  StopSendingStatistics();

  if (mf_media_engine_extension_)
    mf_media_engine_extension_->Shutdown();
  if (mf_media_engine_notify_)
    mf_media_engine_notify_->Shutdown();
  if (mf_media_engine_)
    mf_media_engine_->Shutdown();

  if (mf_source_)
    mf_source_->DetachResource();

  if (dxgi_device_manager_) {
    dxgi_device_manager_.Reset();
    MFUnlockDXGIDeviceManager();
  }
  if (virtual_video_window_)
    DestroyWindow(virtual_video_window_);
}

void MediaFoundationRenderer::Initialize(MediaResource* media_resource,
                                         RendererClient* client,
                                         PipelineStatusCallback init_cb) {
  DVLOG_FUNC(1);

  renderer_client_ = client;

  HRESULT hr = CreateMediaEngine(media_resource);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create media engine: " << PrintHr(hr);
    std::move(init_cb).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
  } else {
    SetVolume(volume_);
    std::move(init_cb).Run(PIPELINE_OK);
  }
}

HRESULT MediaFoundationRenderer::CreateMediaEngine(
    MediaResource* media_resource) {
  DVLOG_FUNC(1);

  mf_session_life_time_ = InitializeMediaFoundation();
  if (!mf_session_life_time_)
    return E_FAIL;

  // TODO(frankli): Only call the followings when there is a video stream.
  RETURN_IF_FAILED(InitializeDXGIDeviceManager());
  RETURN_IF_FAILED(InitializeVirtualVideoWindow());

  // The OnXxx() callbacks are invoked by MF threadpool thread, we would like
  // to bind the callbacks to |task_runner_| MessgaeLoop.
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto weak_this = weak_factory_.GetWeakPtr();
  RETURN_IF_FAILED(MakeAndInitialize<MediaEngineNotifyImpl>(
      &mf_media_engine_notify_,
      BindToCurrentLoop(base::BindRepeating(
          &MediaFoundationRenderer::OnPlaybackError, weak_this)),
      BindToCurrentLoop(base::BindRepeating(
          &MediaFoundationRenderer::OnPlaybackEnded, weak_this)),
      BindToCurrentLoop(base::BindRepeating(
          &MediaFoundationRenderer::OnBufferingStateChange, weak_this)),
      BindToCurrentLoop(base::BindRepeating(
          &MediaFoundationRenderer::OnVideoNaturalSizeChange, weak_this)),
      BindToCurrentLoop(base::BindRepeating(
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
  }

  RETURN_IF_FAILED(
      MakeAndInitialize<MediaEngineExtension>(&mf_media_engine_extension_));
  RETURN_IF_FAILED(creation_attributes->SetUnknown(
      MF_MEDIA_ENGINE_EXTENSION, mf_media_engine_extension_.Get()));

  ComPtr<IMFMediaEngineClassFactory> class_factory;
  RETURN_IF_FAILED(CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&class_factory)));
  // TODO(frankli): Use MF_MEDIA_ENGINE_REAL_TIME_MODE for low latency hint
  // instead of 0.
  RETURN_IF_FAILED(class_factory->CreateInstance(0, creation_attributes.Get(),
                                                 &mf_media_engine_));

  auto media_resource_type_ = media_resource->GetType();
  if (media_resource_type_ != MediaResource::Type::STREAM) {
    DLOG(ERROR) << "MediaResource is not of STREAM";
    return E_INVALIDARG;
  }

  RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationSourceWrapper>(
      &mf_source_, media_resource, task_runner_));

  if (force_dcomp_mode_for_testing_)
    SetDCompMode(true, base::DoNothing());

  if (!mf_source_->HasEncryptedStream()) {
    // Supports clear stream for testing.
    return SetSourceOnMediaEngine();
  }

  // Has encrypted stream.
  RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationProtectionManager>(
      &content_protection_manager_));
  ComPtr<IMFMediaEngineProtectedContent> protected_media_engine;
  RETURN_IF_FAILED(mf_media_engine_.As(&protected_media_engine));
  RETURN_IF_FAILED(protected_media_engine->SetContentProtectionManager(
      content_protection_manager_.Get()));

  waiting_for_mf_cdm_ = true;
  if (!cdm_context_)
    return S_OK;

  // Has |cdm_context_|.
  if (!cdm_context_->GetMediaFoundationCdmProxy(
          base::BindOnce(&MediaFoundationRenderer::OnCdmProxyReceived,
                         weak_factory_.GetWeakPtr()))) {
    DLOG(ERROR) << __func__
                << ": CdmContext does not support MF CDM interface.";
    return MF_E_UNEXPECTED;
  }

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

  ComPtr<ID3D11Device> d3d11_device;
  UINT creation_flags =
      (D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT |
       D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS);
  static const D3D_FEATURE_LEVEL feature_levels[] = {
      D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,
      D3D_FEATURE_LEVEL_9_1};
  RETURN_IF_FAILED(
      D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, creation_flags,
                        feature_levels, base::size(feature_levels),
                        D3D11_SDK_VERSION, &d3d11_device, nullptr, nullptr));

  ComPtr<ID3D10Multithread> multithreaded_device;
  RETURN_IF_FAILED(d3d11_device.As(&multithreaded_device));
  multithreaded_device->SetMultithreadProtected(TRUE);

  return dxgi_device_manager_->ResetDevice(d3d11_device.Get(),
                                           device_reset_token);
}

HRESULT MediaFoundationRenderer::InitializeVirtualVideoWindow() {
  if (!InitializeVideoWindowClass())
    return E_FAIL;

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
    if (!cdm_context_->GetMediaFoundationCdmProxy(
            base::BindOnce(&MediaFoundationRenderer::OnCdmProxyReceived,
                           weak_factory_.GetWeakPtr()))) {
      DLOG(ERROR) << "Decryptor does not support MF CDM interface.";
      std::move(cdm_attached_cb).Run(false);
      return;
    }
  }

  std::move(cdm_attached_cb).Run(true);
}

void MediaFoundationRenderer::SetLatencyHint(
    base::Optional<base::TimeDelta> /*latency_hint*/) {
  // TODO(frankli): Ensure MFMediaEngine rendering pipeine is in real time mode.
  NOTIMPLEMENTED() << "We do not use the latency hint today";
}

void MediaFoundationRenderer::OnCdmProxyReceived(
    ComPtr<IMFCdmProxy> cdm_proxy) {
  DVLOG_FUNC(1);

  if (!waiting_for_mf_cdm_ || !content_protection_manager_) {
    DLOG(ERROR) << "Failed in checking internal state.";
    renderer_client_->OnError(PipelineStatus::PIPELINE_ERROR_INVALID_STATE);
    return;
  }

  waiting_for_mf_cdm_ = false;

  content_protection_manager_->SetCdmProxy(cdm_proxy.Get());
  mf_source_->SetCdmProxy(cdm_proxy.Get());
  HRESULT hr = SetSourceOnMediaEngine();
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to set source on media engine: " << PrintHr(hr);
    renderer_client_->OnError(PipelineStatus::PIPELINE_ERROR_COULD_NOT_RENDER);
    return;
  }
}

void MediaFoundationRenderer::Flush(base::OnceClosure flush_cb) {
  DVLOG_FUNC(2);

  HRESULT hr = mf_media_engine_->Pause();
  // Ignore any Pause() error. We can continue to flush |mf_source_| instead of
  // stopping the playback with error.
  DVLOG_IF(1, FAILED(hr)) << "Failed to pause playback on flush: "
                          << PrintHr(hr);

  StopSendingStatistics();
  mf_source_->FlushStreams();
  std::move(flush_cb).Run();
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
    DLOG(ERROR) << "Failed to SetCurrentTime: " << PrintHr(hr);
    renderer_client_->OnError(PipelineStatus::PIPELINE_ERROR_COULD_NOT_RENDER);
    return;
  }

  hr = mf_media_engine_->Play();
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to start playback: " << PrintHr(hr);
    renderer_client_->OnError(PipelineStatus::PIPELINE_ERROR_COULD_NOT_RENDER);
    return;
  }

  StartSendingStatistics();
}

void MediaFoundationRenderer::SetPlaybackRate(double playback_rate) {
  DVLOG_FUNC(2) << "playback_rate=" << playback_rate;

  HRESULT hr = mf_media_engine_->SetPlaybackRate(playback_rate);
  // Ignore error so that the media continues to play rather than stopped.
  DVLOG_IF(1, FAILED(hr)) << "Failed to set playback rate: " << PrintHr(hr);
}

void MediaFoundationRenderer::SetDCompMode(bool enabled,
                                           SetDCompModeCB callback) {
  DVLOG_FUNC(1);

  HRESULT hr = SetDCompModeInternal(enabled);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to set DComp mode: " << PrintHr(hr);
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

void MediaFoundationRenderer::GetDCompSurface(GetDCompSurfaceCB callback) {
  DVLOG_FUNC(1);

  HANDLE surface_handle = INVALID_HANDLE_VALUE;
  HRESULT hr = GetDCompSurfaceInternal(&surface_handle);
  DVLOG_IF(1, FAILED(hr)) << "Failed to get DComp surface: " << PrintHr(hr);
  std::move(callback).Run(std::move(surface_handle));
}

// TODO(crbug.com/1070030): Investigate if we need to add
// OnSelectedVideoTracksChanged() to media renderer.mojom.
void MediaFoundationRenderer::SetVideoStreamEnabled(bool enabled) {
  DVLOG_FUNC(1) << "enabled=" << enabled;
  if (!mf_source_)
    return;

  const bool needs_restart = mf_source_->SetVideoStreamEnabled(enabled);
  if (needs_restart) {
    // If the media source indicates that we need to restart playback (e.g due
    // to a newly enabled stream being EOS), queue a pause and play operation.
    mf_media_engine_->Pause();
    mf_media_engine_->Play();
  }
}

void MediaFoundationRenderer::SetOutputParams(const gfx::Rect& output_rect) {
  DVLOG_FUNC(2);

  HRESULT hr = SetOutputParamsInternal(output_rect);
  DVLOG_IF(1, FAILED(hr)) << "Failed to set output parameters: " << PrintHr(hr);
}

HRESULT MediaFoundationRenderer::SetOutputParamsInternal(
    const gfx::Rect& output_rect) {
  DVLOG_FUNC(2);

  if (virtual_video_window_ &&
      !::SetWindowPos(virtual_video_window_, HWND_BOTTOM, output_rect.x(),
                      output_rect.y(), output_rect.width(),
                      output_rect.height(), SWP_NOACTIVATE)) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  // TODO(frankli): Update MFMediaEngineEx with |output_rect| change and update
  // renderer client with output size.

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

HRESULT MediaFoundationRenderer::SetDCompModeInternal(bool enabled) {
  DVLOG_FUNC(1) << "enabled=" << enabled;

  ComPtr<IMFMediaEngineEx> media_engine_ex;
  RETURN_IF_FAILED(mf_media_engine_.As(&media_engine_ex));
  RETURN_IF_FAILED(media_engine_ex->EnableWindowlessSwapchainMode(enabled));
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
  statistics.video_frames_decoded = frames_rendered.get().ulVal;
  statistics.video_frames_dropped = frames_dropped.get().ulVal;
  return S_OK;
}

void MediaFoundationRenderer::SendStatistics() {
  PipelineStatistics new_stats = {};
  HRESULT hr = PopulateStatistics(new_stats);
  if (FAILED(hr)) {
    DVLOG(3) << "Unable to populate pipeline stats: " << PrintHr(hr);
    return;
  }

  if (statistics_ != new_stats) {
    statistics_ = new_stats;
    renderer_client_->OnStatisticsUpdate(statistics_);
  }
}

void MediaFoundationRenderer::StartSendingStatistics() {
  const auto kPipelineStatsPollingPeriod =
      base::TimeDelta::FromMilliseconds(500);
  statistics_timer_.Start(FROM_HERE, kPipelineStatsPollingPeriod, this,
                          &MediaFoundationRenderer::SendStatistics);
}

void MediaFoundationRenderer::StopSendingStatistics() {
  statistics_timer_.Stop();
}

void MediaFoundationRenderer::SetVolume(float volume) {
  volume_ = volume;
  float set_volume = muted_ ? 0 : volume_;
  DVLOG_FUNC(2) << "set_volume=" << set_volume;
  if (!mf_media_engine_)
    return;

  HRESULT hr = mf_media_engine_->SetVolume(set_volume);
  DVLOG_IF(1, FAILED(hr)) << "Failed to set volume: " << PrintHr(hr);
}

base::TimeDelta MediaFoundationRenderer::GetMediaTime() {
// GetCurrentTime is expanded as GetTickCount in base/win/windows_types.h
#undef GetCurrentTime
  double current_time = mf_media_engine_->GetCurrentTime();
// Restore macro definition.
#define GetCurrentTime() GetTickCount()
  auto media_time = base::TimeDelta::FromSecondsD(current_time);
  DVLOG_FUNC(3) << "media_time=" << media_time;
  return media_time;
}

void MediaFoundationRenderer::OnPlaybackError(PipelineStatus status) {
  DVLOG_FUNC(1) << "status=" << status;

  renderer_client_->OnError(status);
  StopSendingStatistics();
}

void MediaFoundationRenderer::OnPlaybackEnded() {
  DVLOG_FUNC(2);

  renderer_client_->OnEnded();
  StopSendingStatistics();
}

void MediaFoundationRenderer::OnBufferingStateChange(
    BufferingState state,
    BufferingStateChangeReason reason) {
  DVLOG_FUNC(2);

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

void MediaFoundationRenderer::OnVideoNaturalSizeChange() {
  DVLOG_FUNC(2);

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
    native_video_size_ = {native_width, native_height};
  }

  // TODO(frankli): Use actual dest rect provided by client instead of video
  // size. Will fix the following in another CL.
  ComPtr<IMFMediaEngineEx> mf_media_engine_ex;
  hr = mf_media_engine_.As(&mf_media_engine_ex);
  if (FAILED(hr)) {
    DLOG(ERROR) << PrintHr(hr);
    return;
  }

  RECT video_dest_rect = {0};
  video_dest_rect.right = native_video_size_.width();
  video_dest_rect.bottom = native_video_size_.height();
  hr =
      mf_media_engine_ex->UpdateVideoStream(nullptr, &video_dest_rect, nullptr);
  if (FAILED(hr)) {
    DLOG(ERROR) << PrintHr(hr);
    return;
  }

  renderer_client_->OnVideoNaturalSizeChange(native_video_size_);
  return;
}

void MediaFoundationRenderer::OnTimeUpdate() {}

}  // namespace media
