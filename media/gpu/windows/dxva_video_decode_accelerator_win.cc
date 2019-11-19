// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/dxva_video_decode_accelerator_win.h"

#include <algorithm>
#include <memory>

#if !defined(OS_WIN)
#error This file should only be built on Windows.
#endif  // !defined(OS_WIN)

#include <codecapi.h>
#include <dxgi1_2.h>
#include <ks.h>
#include <mfapi.h>
#include <mferror.h>
#include <ntverp.h>
#include <objbase.h>
#include <stddef.h>
#include <string.h>
#include <wmcodecdsp.h>

#include "base/atomicops.h"
#include "base/base_paths_win.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_local_storage.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/win/mf_helpers.h"
#include "media/base/win/mf_initializer.h"
#include "media/filters/vp9_parser.h"
#include "media/gpu/windows/d3d11_video_device_format_support.h"
#include "media/gpu/windows/dxva_picture_buffer_win.h"
#include "media/gpu/windows/supported_profile_helpers.h"
#include "media/video/h264_parser.h"
#include "media/video/video_decode_accelerator.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gl/direct_composition_surface_win.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_switches.h"

namespace {

const wchar_t kMSVP9DecoderDLLName[] = L"MSVP9DEC.dll";

const CLSID MEDIASUBTYPE_VP90 = {
    0x30395056,
    0x0000,
    0x0010,
    {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

// The CLSID of the video processor media foundation transform which we use for
// texture color conversion in DX11.
// Defined in mfidl.h in the Windows 10 SDK. ntverp.h provides VER_PRODUCTBUILD
// to detect which SDK we are compiling with.
#if VER_PRODUCTBUILD < 10011  // VER_PRODUCTBUILD for 10.0.10158.0 SDK.
DEFINE_GUID(CLSID_VideoProcessorMFT,
            0x88753b26,
            0x5b24,
            0x49bd,
            0xb2,
            0xe7,
            0xc,
            0x44,
            0x5c,
            0x78,
            0xc9,
            0x82);
#endif

// MF_XVP_PLAYBACK_MODE
// Data type: UINT32 (treat as BOOL)
// If this attribute is TRUE, the video processor will run in playback mode
// where it allows callers to allocate output samples and allows last frame
// regeneration (repaint).
DEFINE_GUID(MF_XVP_PLAYBACK_MODE,
            0x3c5d293f,
            0xad67,
            0x4e29,
            0xaf,
            0x12,
            0xcf,
            0x3e,
            0x23,
            0x8a,
            0xcc,
            0xe9);

// Defines the GUID for the Intel H264 DXVA device.
static const GUID DXVA2_Intel_ModeH264_E = {
    0x604F8E68,
    0x4951,
    0x4c54,
    {0x88, 0xFE, 0xAB, 0xD2, 0x5C, 0x15, 0xB3, 0xD6}};

constexpr const wchar_t* const kMediaFoundationVideoDecoderDLLs[] = {
    L"mf.dll", L"mfplat.dll", L"msmpeg2vdec.dll",
};

uint64_t GetCurrentQPC() {
  LARGE_INTEGER perf_counter_now = {};
  // Use raw QueryPerformanceCounter to avoid grabbing locks or allocating
  // memory in an exception handler;
  ::QueryPerformanceCounter(&perf_counter_now);
  return perf_counter_now.QuadPart;
}

uint64_t g_last_process_output_time;
HRESULT g_last_device_removed_reason;

}  // namespace

namespace media {

static const VideoCodecProfile kSupportedProfiles[] = {
    H264PROFILE_BASELINE, H264PROFILE_MAIN, H264PROFILE_HIGH,
    VP9PROFILE_PROFILE0, VP9PROFILE_PROFILE2};

CreateDXGIDeviceManager
    DXVAVideoDecodeAccelerator::create_dxgi_device_manager_ = NULL;

enum {
  // Maximum number of iterations we allow before aborting the attempt to flush
  // the batched queries to the driver and allow torn/corrupt frames to be
  // rendered.
  kFlushDecoderSurfaceTimeoutMs = 1,
  // Maximum iterations where we try to flush the d3d device.
  kMaxIterationsForD3DFlush = 4,
  // Maximum iterations where we try to flush the ANGLE device before reusing
  // the texture.
  kMaxIterationsForANGLEReuseFlush = 16,
  // We only request 5 picture buffers from the client which are used to hold
  // the decoded samples. These buffers are then reused when the client tells
  // us that it is done with the buffer.
  kNumPictureBuffers = 5,
  // When GetTextureTarget() returns GL_TEXTURE_EXTERNAL_OES, allocated
  // PictureBuffers do not consume significant resources, so we can optimize for
  // latency more aggressively.
  kNumPictureBuffersForZeroCopy = 10,
  // The keyed mutex should always be released before the other thread
  // attempts to acquire it, so AcquireSync should always return immediately.
  kAcquireSyncWaitMs = 0,
};

// Creates a Media Foundation sample with one buffer containing a copy of the
// given Annex B stream data.
// If duration and sample time are not known, provide 0.
// |min_size| specifies the minimum size of the buffer (might be required by
// the decoder for input). If no alignment is required, provide 0.
static Microsoft::WRL::ComPtr<IMFSample> CreateInputSample(
    const uint8_t* stream,
    uint32_t size,
    uint32_t min_size,
    int alignment) {
  CHECK(stream);
  CHECK_GT(size, 0U);
  Microsoft::WRL::ComPtr<IMFSample> sample;
  sample = mf::CreateEmptySampleWithBuffer(std::max(min_size, size), alignment);
  RETURN_ON_FAILURE(sample.Get(), "Failed to create empty sample",
                    Microsoft::WRL::ComPtr<IMFSample>());

  Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
  HRESULT hr = sample->GetBufferByIndex(0, &buffer);
  RETURN_ON_HR_FAILURE(hr, "Failed to get buffer from sample",
                       Microsoft::WRL::ComPtr<IMFSample>());

  DWORD max_length = 0;
  DWORD current_length = 0;
  uint8_t* destination = NULL;
  hr = buffer->Lock(&destination, &max_length, &current_length);
  RETURN_ON_HR_FAILURE(hr, "Failed to lock buffer",
                       Microsoft::WRL::ComPtr<IMFSample>());

  CHECK_EQ(current_length, 0u);
  CHECK_GE(max_length, size);
  memcpy(destination, stream, size);

  hr = buffer->SetCurrentLength(size);
  RETURN_ON_HR_FAILURE(hr, "Failed to set buffer length",
                       Microsoft::WRL::ComPtr<IMFSample>());

  hr = buffer->Unlock();
  RETURN_ON_HR_FAILURE(hr, "Failed to unlock buffer",
                       Microsoft::WRL::ComPtr<IMFSample>());

  return sample;
}

// Helper function to create a COM object instance from a DLL. The alternative
// is to use the CoCreateInstance API which requires the COM apartment to be
// initialized which is not the case on the GPU main thread. We want to avoid
// initializing COM as it may have sideeffects.
HRESULT CreateCOMObjectFromDll(HMODULE dll,
                               const CLSID& clsid,
                               const IID& iid,
                               void** object) {
  if (!dll || !object)
    return E_INVALIDARG;

  using GetClassObject =
      HRESULT(WINAPI*)(const CLSID& clsid, const IID& iid, void** object);

  GetClassObject get_class_object = reinterpret_cast<GetClassObject>(
      GetProcAddress(dll, "DllGetClassObject"));
  RETURN_ON_FAILURE(get_class_object, "Failed to get DllGetClassObject pointer",
                    E_FAIL);

  Microsoft::WRL::ComPtr<IClassFactory> factory;
  HRESULT hr = get_class_object(clsid, IID_PPV_ARGS(&factory));
  RETURN_ON_HR_FAILURE(hr, "DllGetClassObject failed", hr);

  hr = factory->CreateInstance(NULL, iid, object);
  return hr;
}

ConfigChangeDetector::~ConfigChangeDetector() {}

// Provides functionality to detect H.264 stream configuration changes.
// TODO(ananta)
// Move this to a common place so that all VDA's can use this.
class H264ConfigChangeDetector : public ConfigChangeDetector {
 public:
  H264ConfigChangeDetector();
  ~H264ConfigChangeDetector() override;

  // Detects stream configuration changes.
  // Returns false on failure.
  bool DetectConfig(const uint8_t* stream, unsigned int size) override;
  gfx::Rect current_visible_rect(
      const gfx::Rect& container_visible_rect) const override;
  VideoColorSpace current_color_space(
      const VideoColorSpace& container_color_space) const override;

 private:
  // These fields are used to track the SPS/PPS in the H.264 bitstream and
  // are eventually compared against the SPS/PPS in the bitstream to detect
  // a change.
  int last_sps_id_;
  std::vector<uint8_t> last_sps_;
  int last_pps_id_;
  std::vector<uint8_t> last_pps_;
  // We want to indicate configuration changes only after we see IDR slices.
  // This flag tracks that we potentially have a configuration change which
  // we want to honor after we see an IDR slice.
  bool pending_config_changed_;

  std::unique_ptr<H264Parser> parser_;

  DISALLOW_COPY_AND_ASSIGN(H264ConfigChangeDetector);
};

H264ConfigChangeDetector::H264ConfigChangeDetector()
    : last_sps_id_(0), last_pps_id_(0), pending_config_changed_(false) {}

H264ConfigChangeDetector::~H264ConfigChangeDetector() {}

bool H264ConfigChangeDetector::DetectConfig(const uint8_t* stream,
                                            unsigned int size) {
  std::vector<uint8_t> sps;
  std::vector<uint8_t> pps;
  H264NALU nalu;
  bool idr_seen = false;

  if (!parser_.get())
    parser_.reset(new H264Parser);

  parser_->SetStream(stream, size);
  config_changed_ = false;

  while (true) {
    H264Parser::Result result = parser_->AdvanceToNextNALU(&nalu);

    if (result == H264Parser::kEOStream)
      break;

    if (result == H264Parser::kUnsupportedStream) {
      DLOG(ERROR) << "Unsupported H.264 stream";
      return false;
    }

    if (result != H264Parser::kOk) {
      DLOG(ERROR) << "Failed to parse H.264 stream";
      return false;
    }

    switch (nalu.nal_unit_type) {
      case H264NALU::kSPS:
        result = parser_->ParseSPS(&last_sps_id_);
        if (result == H264Parser::kUnsupportedStream) {
          DLOG(ERROR) << "Unsupported SPS";
          return false;
        }

        if (result != H264Parser::kOk) {
          DLOG(ERROR) << "Could not parse SPS";
          return false;
        }

        sps.assign(nalu.data, nalu.data + nalu.size);
        break;

      case H264NALU::kPPS:
        result = parser_->ParsePPS(&last_pps_id_);
        if (result == H264Parser::kUnsupportedStream) {
          DLOG(ERROR) << "Unsupported PPS";
          return false;
        }
        if (result != H264Parser::kOk) {
          DLOG(ERROR) << "Could not parse PPS";
          return false;
        }
        pps.assign(nalu.data, nalu.data + nalu.size);
        break;

      case H264NALU::kIDRSlice:
        idr_seen = true;
        // If we previously detected a configuration change, and see an IDR
        // slice next time around, we need to flag a configuration change.
        if (pending_config_changed_) {
          config_changed_ = true;
          pending_config_changed_ = false;
        }
        break;

      default:
        break;
    }
  }

  // TODO(sandersd): Update to match logic in VTVDA that tracks activated rather
  // than most recent SPS and PPS.
  if (!sps.empty() && sps != last_sps_) {
    if (!last_sps_.empty()) {
      // Flag configuration changes after we see an IDR slice.
      if (idr_seen) {
        config_changed_ = true;
      } else {
        pending_config_changed_ = true;
      }
    }
    last_sps_.swap(sps);
  }

  if (!pps.empty() && pps != last_pps_) {
    if (!last_pps_.empty()) {
      // Flag configuration changes after we see an IDR slice.
      if (idr_seen) {
        config_changed_ = true;
      } else {
        pending_config_changed_ = true;
      }
    }
    last_pps_.swap(pps);
  }
  return true;
}

gfx::Rect H264ConfigChangeDetector::current_visible_rect(
    const gfx::Rect& container_visible_rect) const {
  if (!parser_)
    return container_visible_rect;
  // TODO(hubbe): Is using last_sps_id_ correct here?
  const H264SPS* sps = parser_->GetSPS(last_sps_id_);
  if (!sps)
    return container_visible_rect;
  return sps->GetVisibleRect().value_or(container_visible_rect);
}

VideoColorSpace H264ConfigChangeDetector::current_color_space(
    const VideoColorSpace& container_color_space) const {
  if (!parser_)
    return container_color_space;
  // TODO(hubbe): Is using last_sps_id_ correct here?
  const H264SPS* sps = parser_->GetSPS(last_sps_id_);
  if (sps && sps->GetColorSpace().IsSpecified()) {
    return sps->GetColorSpace();
  }
  return container_color_space;
}

// Doesn't actually detect config changes, only stream metadata.
class VP9ConfigChangeDetector : public ConfigChangeDetector {
 public:
  VP9ConfigChangeDetector() : ConfigChangeDetector(), parser_(false) {}
  ~VP9ConfigChangeDetector() override {}

  // Detects stream configuration changes.
  // Returns false on failure.
  bool DetectConfig(const uint8_t* stream, unsigned int size) override {
    parser_.SetStream(stream, size, nullptr);
    Vp9FrameHeader fhdr;
    gfx::Size allocate_size;
    std::unique_ptr<DecryptConfig> null_config;
    while (parser_.ParseNextFrame(&fhdr, &allocate_size, &null_config) ==
           Vp9Parser::kOk) {
      visible_rect_ = gfx::Rect(fhdr.render_width, fhdr.render_height);
      color_space_ = fhdr.GetColorSpace();

      gfx::Size new_size(fhdr.frame_width, fhdr.frame_height);
      if (!size_.IsEmpty() && !pending_config_changed_ && !config_changed_ &&
          size_ != new_size) {
        pending_config_changed_ = true;
        DVLOG(1) << "Configuration changed from " << size_.ToString() << " to "
                 << new_size.ToString();
      }
      size_ = new_size;

      // Resolution changes can happen on any frame technically, so wait for a
      // keyframe before signaling the config change.
      if (fhdr.IsKeyframe() && pending_config_changed_) {
        config_changed_ = true;
        pending_config_changed_ = false;
      }
    }
    if (pending_config_changed_)
      DVLOG(3) << "Deferring config change until next keyframe...";
    return true;
  }

  gfx::Rect current_visible_rect(
      const gfx::Rect& container_visible_rect) const override {
    return visible_rect_.IsEmpty() ? container_visible_rect : visible_rect_;
  }

  VideoColorSpace current_color_space(
      const VideoColorSpace& container_color_space) const override {
    // For VP9, container color spaces override video stream color spaces.
    if (container_color_space.IsSpecified()) {
      return container_color_space;
    }
    return color_space_;
  }

 private:
  gfx::Size size_;
  bool pending_config_changed_ = false;
  gfx::Rect visible_rect_;
  VideoColorSpace color_space_;
  Vp9Parser parser_;
};

DXVAVideoDecodeAccelerator::PendingSampleInfo::PendingSampleInfo(
    int32_t buffer_id,
    Microsoft::WRL::ComPtr<IMFSample> sample,
    const gfx::Rect& visible_rect,
    const gfx::ColorSpace& color_space)
    : input_buffer_id(buffer_id),
      picture_buffer_id(-1),
      visible_rect(visible_rect),
      color_space(color_space),
      output_sample(sample) {}

DXVAVideoDecodeAccelerator::PendingSampleInfo::PendingSampleInfo(
    const PendingSampleInfo& other) = default;

DXVAVideoDecodeAccelerator::PendingSampleInfo::~PendingSampleInfo() {}

DXVAVideoDecodeAccelerator::DXVAVideoDecodeAccelerator(
    const GetGLContextCallback& get_gl_context_cb,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    const gpu::GpuDriverBugWorkarounds& workarounds,
    const gpu::GpuPreferences& gpu_preferences,
    MediaLog* media_log)
    : client_(NULL),
      dev_manager_reset_token_(0),
      dx11_dev_manager_reset_token_(0),
      egl_config_(NULL),
      state_(kUninitialized),
      pictures_requested_(false),
      inputs_before_decode_(0),
      sent_drain_message_(false),
      get_gl_context_cb_(get_gl_context_cb),
      make_context_current_cb_(make_context_current_cb),
      bind_image_cb_(bind_image_cb),
      media_log_(media_log),
      codec_(kUnknownVideoCodec),
      decoder_thread_("DXVAVideoDecoderThread"),
      pending_flush_(false),
      enable_low_latency_(gpu_preferences.enable_low_latency_dxva),
      support_share_nv12_textures_(
          gpu_preferences.enable_zero_copy_dxgi_video &&
          !workarounds.disable_dxgi_zero_copy_video),
      num_picture_buffers_requested_(support_share_nv12_textures_
                                         ? kNumPictureBuffersForZeroCopy
                                         : kNumPictureBuffers),
      support_copy_nv12_textures_(gpu_preferences.enable_nv12_dxgi_video &&
                                  !workarounds.disable_nv12_dxgi_video),
      support_delayed_copy_nv12_textures_(
          base::FeatureList::IsEnabled(kDelayCopyNV12Textures) &&
          !workarounds.disable_delayed_copy_nv12),
      use_dx11_(false),
      use_keyed_mutex_(false),
      using_angle_device_(false),
      using_debug_device_(false),
      enable_accelerated_vpx_decode_(
          !workarounds.disable_accelerated_vpx_decode),
      processing_config_changed_(false) {
  weak_ptr_ = weak_this_factory_.GetWeakPtr();
  memset(&input_stream_info_, 0, sizeof(input_stream_info_));
  memset(&output_stream_info_, 0, sizeof(output_stream_info_));
  use_color_info_ = base::FeatureList::IsEnabled(kVideoBlitColorAccuracy);
}

DXVAVideoDecodeAccelerator::~DXVAVideoDecodeAccelerator() {
  client_ = NULL;
}

bool DXVAVideoDecodeAccelerator::Initialize(const Config& config,
                                            Client* client) {
  if (!get_gl_context_cb_ || !make_context_current_cb_) {
    NOTREACHED() << "GL callbacks are required for this VDA";
    return false;
  }

  if (config.is_encrypted()) {
    NOTREACHED() << "Encrypted streams are not supported for this VDA";
    return false;
  }

  if (config.output_mode != Config::OutputMode::ALLOCATE) {
    NOTREACHED() << "Only ALLOCATE OutputMode is supported by this VDA";
    return false;
  }

  client_ = client;

  main_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();

  if (!config.supported_output_formats.empty() &&
      !base::Contains(config.supported_output_formats, PIXEL_FORMAT_NV12)) {
    DisableSharedTextureSupport();
    support_copy_nv12_textures_ = false;
  }

  bool profile_supported = false;
  for (const auto& supported_profile : kSupportedProfiles) {
    if (config.profile == supported_profile) {
      profile_supported = true;
      break;
    }
  }
  RETURN_ON_FAILURE(profile_supported, "Unsupported h.264 or vp9 profile",
                    false);

  if (config.profile == VP9PROFILE_PROFILE2 ||
      config.profile == VP9PROFILE_PROFILE3 ||
      config.profile == H264PROFILE_HIGH10PROFILE) {
    // Input file has more than 8 bits per channel.
    use_fp16_ = true;
  }

  // Unfortunately, the profile is currently unreliable for
  // VP9 (https://crbug.com/592074) so also try to use fp16 if HDR is on.
  if (config.target_color_space.IsHDR()) {
    use_fp16_ = true;
  }

  // Not all versions of Windows 7 and later include Media Foundation DLLs.
  // Instead of crashing while delay loading the DLL when calling MFStartup()
  // below, probe whether we can successfully load the DLL now.
  // See http://crbug.com/339678 for details.
  HMODULE dxgi_manager_dll = ::GetModuleHandle(L"MFPlat.dll");
  RETURN_ON_FAILURE(dxgi_manager_dll, "MFPlat.dll is required for decoding",
                    false);

// On Windows 8+ mfplat.dll provides the MFCreateDXGIDeviceManager API.
// On Windows 7 mshtmlmedia.dll provides it.

// TODO(ananta)
// The code below works, as in we can create the DX11 device manager for
// Windows 7. However the IMFTransform we use for texture conversion and
// copy does not exist on Windows 7. Look into an alternate approach
// and enable the code below.
#if defined(ENABLE_DX11_FOR_WIN7)
  if (base::win::GetVersion() == base::win::Version::WIN7) {
    dxgi_manager_dll = ::GetModuleHandle(L"mshtmlmedia.dll");
    RETURN_ON_FAILURE(dxgi_manager_dll,
                      "mshtmlmedia.dll is required for decoding", false);
  }
#endif
  // If we don't find the MFCreateDXGIDeviceManager API we fallback to D3D9
  // decoding.
  if (dxgi_manager_dll && !create_dxgi_device_manager_) {
    create_dxgi_device_manager_ = reinterpret_cast<CreateDXGIDeviceManager>(
        ::GetProcAddress(dxgi_manager_dll, "MFCreateDXGIDeviceManager"));
  }

  RETURN_ON_FAILURE(make_context_current_cb_.Run(),
                    "Failed to make context current", false);

  RETURN_ON_FAILURE(
      gl::g_driver_egl.ext.b_EGL_ANGLE_surface_d3d_texture_2d_share_handle,
      "EGL_ANGLE_surface_d3d_texture_2d_share_handle unavailable", false);

  RETURN_ON_FAILURE(gl::GLFence::IsSupported(), "GL fences are unsupported",
                    false);

  State state = GetState();
  RETURN_ON_FAILURE((state == kUninitialized),
                    "Initialize: invalid state: " << state, false);

  RETURN_ON_FAILURE(InitializeMediaFoundation(),
                    "Could not initialize Media Foundartion", false);

  config_ = config;

  RETURN_ON_FAILURE(InitDecoder(config.profile), "Failed to initialize decoder",
                    false);
  // Record this after we see if it works.
  UMA_HISTOGRAM_BOOLEAN("Media.DXVAVDA.UseD3D11", use_dx11_);

  RETURN_ON_FAILURE(GetStreamsInfoAndBufferReqs(),
                    "Failed to get input/output stream info.", false);

  RETURN_ON_FAILURE(
      SendMFTMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0),
      "Send MFT_MESSAGE_NOTIFY_BEGIN_STREAMING notification failed", false);

  RETURN_ON_FAILURE(
      SendMFTMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0),
      "Send MFT_MESSAGE_NOTIFY_START_OF_STREAM notification failed", false);

  if (codec_ == kCodecH264)
    config_change_detector_.reset(new H264ConfigChangeDetector);
  if (codec_ == kCodecVP9)
    config_change_detector_.reset(new VP9ConfigChangeDetector);

  SetState(kNormal);

  UMA_HISTOGRAM_ENUMERATION("Media.DXVAVDA.PictureBufferMechanism",
                            GetPictureBufferMechanism());

  return StartDecoderThread();
}

bool DXVAVideoDecodeAccelerator::CreateD3DDevManager() {
  TRACE_EVENT0("gpu", "DXVAVideoDecodeAccelerator_CreateD3DDevManager");
  // The device may exist if the last state was a config change.
  if (d3d9_.Get())
    return true;

  HRESULT hr = E_FAIL;

  hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d9_);
  RETURN_ON_HR_FAILURE(hr, "Direct3DCreate9Ex failed", false);

  hr = d3d9_->CheckDeviceFormatConversion(
      D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
      static_cast<D3DFORMAT>(MAKEFOURCC('N', 'V', '1', '2')), D3DFMT_X8R8G8B8);
  RETURN_ON_HR_FAILURE(hr, "D3D9 driver does not support H/W format conversion",
                       false);

  if (auto angle_device = gl::QueryD3D9DeviceObjectFromANGLE()) {
    using_angle_device_ = true;
    hr = angle_device.As(&d3d9_device_ex_);
    RETURN_ON_HR_FAILURE(
        hr, "QueryInterface for IDirect3DDevice9Ex from angle device failed",
        false);
  } else {
    D3DPRESENT_PARAMETERS present_params = {0};
    present_params.BackBufferWidth = 1;
    present_params.BackBufferHeight = 1;
    present_params.BackBufferFormat = D3DFMT_UNKNOWN;
    present_params.BackBufferCount = 1;
    present_params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present_params.hDeviceWindow = NULL;
    present_params.Windowed = TRUE;
    present_params.Flags = D3DPRESENTFLAG_VIDEO;
    present_params.FullScreen_RefreshRateInHz = 0;
    present_params.PresentationInterval = 0;

    hr = d3d9_->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL,
                               D3DCREATE_FPU_PRESERVE |
                                   D3DCREATE_MIXED_VERTEXPROCESSING |
                                   D3DCREATE_MULTITHREADED,
                               &present_params, NULL, &d3d9_device_ex_);
    RETURN_ON_HR_FAILURE(hr, "Failed to create D3D device", false);
  }

  hr = DXVA2CreateDirect3DDeviceManager9(&dev_manager_reset_token_,
                                         &device_manager_);
  RETURN_ON_HR_FAILURE(hr, "DXVA2CreateDirect3DDeviceManager9 failed", false);

  hr = device_manager_->ResetDevice(d3d9_device_ex_.Get(),
                                    dev_manager_reset_token_);
  RETURN_ON_HR_FAILURE(hr, "Failed to reset device", false);

  hr = d3d9_device_ex_->CreateQuery(D3DQUERYTYPE_EVENT, &query_);
  RETURN_ON_HR_FAILURE(hr, "Failed to create D3D device query", false);
  // Ensure query_ API works (to avoid an infinite loop later in
  // CopyOutputSampleDataToPictureBuffer).
  hr = query_->Issue(D3DISSUE_END);
  RETURN_ON_HR_FAILURE(hr, "Failed to issue END test query", false);

  CreateVideoProcessor();
  return true;
}

bool DXVAVideoDecodeAccelerator::CreateVideoProcessor() {
  if (!use_color_info_)
    return false;

  // TODO(Hubbe): Don't try again if we tried and failed already.
  if (video_processor_service_.Get())
    return true;
  HRESULT hr = DXVA2CreateVideoService(d3d9_device_ex_.Get(),
                                       IID_PPV_ARGS(&video_processor_service_));
  RETURN_ON_HR_FAILURE(hr, "DXVA2CreateVideoService failed", false);

  // TODO(Hubbe): Use actual video settings.
  DXVA2_VideoDesc inputDesc;
  inputDesc.SampleWidth = 1920;
  inputDesc.SampleHeight = 1080;
  inputDesc.SampleFormat.VideoChromaSubsampling =
      DXVA2_VideoChromaSubsampling_MPEG2;
  inputDesc.SampleFormat.NominalRange = DXVA2_NominalRange_16_235;
  inputDesc.SampleFormat.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT709;
  inputDesc.SampleFormat.VideoLighting = DXVA2_VideoLighting_dim;
  inputDesc.SampleFormat.VideoPrimaries = DXVA2_VideoPrimaries_BT709;
  inputDesc.SampleFormat.VideoTransferFunction = DXVA2_VideoTransFunc_709;
  inputDesc.SampleFormat.SampleFormat = DXVA2_SampleProgressiveFrame;
  inputDesc.Format = (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2');
  inputDesc.InputSampleFreq.Numerator = 30;
  inputDesc.InputSampleFreq.Denominator = 1;
  inputDesc.OutputFrameFreq.Numerator = 30;
  inputDesc.OutputFrameFreq.Denominator = 1;

  UINT guid_count = 0;
  base::win::ScopedCoMem<GUID> guids;
  hr = video_processor_service_->GetVideoProcessorDeviceGuids(
      &inputDesc, &guid_count, &guids);
  RETURN_ON_HR_FAILURE(hr, "GetVideoProcessorDeviceGuids failed", false);

  for (UINT g = 0; g < guid_count; g++) {
    DXVA2_VideoProcessorCaps caps;
    hr = video_processor_service_->GetVideoProcessorCaps(
        guids[g], &inputDesc, D3DFMT_X8R8G8B8, &caps);
    if (hr)
      continue;

    if (!(caps.VideoProcessorOperations & DXVA2_VideoProcess_YUV2RGB))
      continue;

    base::win::ScopedCoMem<D3DFORMAT> formats;
    UINT format_count = 0;
    hr = video_processor_service_->GetVideoProcessorRenderTargets(
        guids[g], &inputDesc, &format_count, &formats);
    if (hr)
      continue;

    UINT f;
    for (f = 0; f < format_count; f++) {
      if (formats[f] == D3DFMT_X8R8G8B8) {
        break;
      }
    }
    if (f == format_count)
      continue;

    // Create video processor
    hr = video_processor_service_->CreateVideoProcessor(
        guids[g], &inputDesc, D3DFMT_X8R8G8B8, 0, &processor_);
    if (hr)
      continue;

    DXVA2_ValueRange range;
    processor_->GetProcAmpRange(DXVA2_ProcAmp_Brightness, &range);
    default_procamp_values_.Brightness = range.DefaultValue;
    processor_->GetProcAmpRange(DXVA2_ProcAmp_Contrast, &range);
    default_procamp_values_.Contrast = range.DefaultValue;
    processor_->GetProcAmpRange(DXVA2_ProcAmp_Hue, &range);
    default_procamp_values_.Hue = range.DefaultValue;
    processor_->GetProcAmpRange(DXVA2_ProcAmp_Saturation, &range);
    default_procamp_values_.Saturation = range.DefaultValue;

    return true;
  }
  return false;
}

bool DXVAVideoDecodeAccelerator::CreateDX11DevManager() {
  // The device may exist if the last state was a config change.
  if (D3D11Device())
    return true;

  HRESULT hr = create_dxgi_device_manager_(&dx11_dev_manager_reset_token_,
                                           &d3d11_device_manager_);
  RETURN_ON_HR_FAILURE(hr, "MFCreateDXGIDeviceManager failed", false);

  angle_device_ = gl::QueryD3D11DeviceObjectFromANGLE();
  if (!angle_device_) {
    support_copy_nv12_textures_ = false;
  }
  if (ShouldUseANGLEDevice()) {
    RETURN_ON_FAILURE(angle_device_.Get(), "Failed to get d3d11 device", false);

    using_angle_device_ = true;
    DCHECK(!use_fp16_);
    angle_device_->GetImmediateContext(&d3d11_device_context_);

    hr = angle_device_.As(&video_device_);
    RETURN_ON_HR_FAILURE(hr, "Failed to get video device", false);
  } else {
    // This array defines the set of DirectX hardware feature levels we support.
    // The ordering MUST be preserved. All applications are assumed to support
    // 9.1 unless otherwise stated by the application.
    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1};

    UINT flags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT;

    D3D_FEATURE_LEVEL feature_level_out = D3D_FEATURE_LEVEL_11_0;
#if defined _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;

    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags,
                           feature_levels, base::size(feature_levels),
                           D3D11_SDK_VERSION, &d3d11_device_,
                           &feature_level_out, &d3d11_device_context_);
    if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING) {
      LOG(ERROR)
          << "Debug DXGI device creation failed, falling back to release.";
      flags &= ~D3D11_CREATE_DEVICE_DEBUG;
    } else {
      RETURN_ON_HR_FAILURE(hr, "Failed to create debug DX11 device", false);
    }
#endif
    using_debug_device_ = !!d3d11_device_context_;
    if (!d3d11_device_context_) {
      hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags,
                             feature_levels, base::size(feature_levels),
                             D3D11_SDK_VERSION, &d3d11_device_,
                             &feature_level_out, &d3d11_device_context_);
      RETURN_ON_HR_FAILURE(hr, "Failed to create DX11 device", false);
    }

    hr = d3d11_device_.As(&video_device_);
    RETURN_ON_HR_FAILURE(hr, "Failed to get video device", false);
  }

  hr = d3d11_device_context_.As(&video_context_);
  RETURN_ON_HR_FAILURE(hr, "Failed to get video context", false);

  D3D11_FEATURE_DATA_D3D11_OPTIONS options;
  hr = D3D11Device()->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &options,
                                          sizeof(options));
  RETURN_ON_HR_FAILURE(hr, "Failed to retrieve D3D11 options", false);

  // Need extended resource sharing so we can share the NV12 texture between
  // ANGLE and the decoder context.
  if (!options.ExtendedResourceSharing)
    support_copy_nv12_textures_ = false;

  FormatSupportChecker checker(ShouldUseANGLEDevice() ? angle_device_
                                                      : d3d11_device_);
  RETURN_ON_FAILURE(checker.Initialize(), "Failed to check format supports!",
                    false);

  if (!checker.CheckOutputFormatSupport(DXGI_FORMAT_NV12))
    support_copy_nv12_textures_ = false;

  if (!checker.CheckOutputFormatSupport(DXGI_FORMAT_R16G16B16A16_FLOAT))
    use_fp16_ = false;

  // Enable multithreaded mode on the device. This ensures that accesses to
  // context are synchronized across threads. We have multiple threads
  // accessing the context, the media foundation decoder threads and the
  // decoder thread via the video format conversion transform.
  hr = D3D11Device()->QueryInterface(IID_PPV_ARGS(&multi_threaded_));
  RETURN_ON_HR_FAILURE(hr, "Failed to query ID3D10Multithread", false);
  multi_threaded_->SetMultithreadProtected(TRUE);

  hr = d3d11_device_manager_->ResetDevice(D3D11Device(),
                                          dx11_dev_manager_reset_token_);
  RETURN_ON_HR_FAILURE(hr, "Failed to reset device", false);

  D3D11_QUERY_DESC query_desc;
  query_desc.Query = D3D11_QUERY_EVENT;
  query_desc.MiscFlags = 0;
  hr = D3D11Device()->CreateQuery(&query_desc, &d3d11_query_);
  RETURN_ON_HR_FAILURE(hr, "Failed to create DX11 device query", false);

  return true;
}

void DXVAVideoDecodeAccelerator::Decode(BitstreamBuffer bitstream) {
  Decode(bitstream.ToDecoderBuffer(), bitstream.id());
}

void DXVAVideoDecodeAccelerator::Decode(scoped_refptr<DecoderBuffer> buffer,
                                        int32_t bitstream_id) {
  TRACE_EVENT0("media", "DXVAVideoDecodeAccelerator::Decode");
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  RETURN_AND_NOTIFY_ON_FAILURE(bitstream_id >= 0,
                               "Invalid bitstream, id: " << bitstream_id,
                               INVALID_ARGUMENT, );

  if (!buffer) {
    if (client_)
      client_->NotifyEndOfBitstreamBuffer(bitstream_id);
    return;
  }

  State state = GetState();
  RETURN_AND_NOTIFY_ON_FAILURE(
      (state == kNormal || state == kStopped || state == kFlushing),
      "Invalid state: " << state, ILLEGAL_STATE, );

  Microsoft::WRL::ComPtr<IMFSample> sample;
  sample = CreateInputSample(
      buffer->data(), buffer->data_size(),
      std::min<uint32_t>(buffer->data_size(), input_stream_info_.cbSize),
      input_stream_info_.cbAlignment);
  RETURN_AND_NOTIFY_ON_FAILURE(sample.Get(), "Failed to create input sample",
                               PLATFORM_FAILURE, );

  RETURN_AND_NOTIFY_ON_HR_FAILURE(
      sample->SetSampleTime(bitstream_id),
      "Failed to associate input buffer id with sample", PLATFORM_FAILURE, );

  decoder_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DXVAVideoDecodeAccelerator::DecodeInternal,
                                base::Unretained(this), sample));
}

void DXVAVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<PictureBuffer>& buffers) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  State state = GetState();
  RETURN_AND_NOTIFY_ON_FAILURE((state != kUninitialized),
                               "Invalid state: " << state, ILLEGAL_STATE, );
  RETURN_AND_NOTIFY_ON_FAILURE(
      (num_picture_buffers_requested_ <= static_cast<int>(buffers.size())),
      "Failed to provide requested picture buffers. (Got "
          << buffers.size() << ", requested " << num_picture_buffers_requested_
          << ")",
      INVALID_ARGUMENT, );

  RETURN_AND_NOTIFY_ON_FAILURE(make_context_current_cb_.Run(),
                               "Failed to make context current",
                               PLATFORM_FAILURE, );
  // Copy the picture buffers provided by the client to the available list,
  // and mark these buffers as available for use.
  for (size_t buffer_index = 0; buffer_index < buffers.size(); ++buffer_index) {
    std::unique_ptr<DXVAPictureBuffer> picture_buffer =
        DXVAPictureBuffer::Create(*this, buffers[buffer_index], egl_config_);
    RETURN_AND_NOTIFY_ON_FAILURE(picture_buffer.get(),
                                 "Failed to allocate picture buffer",
                                 PLATFORM_FAILURE, );
    if (bind_image_cb_) {
      for (uint32_t client_id : buffers[buffer_index].client_texture_ids()) {
        // The picture buffer handles the actual binding of its contents to
        // texture ids. This call just causes the texture manager to hold a
        // reference to the GLImage as long as either texture exists.
        bind_image_cb_.Run(client_id, GetTextureTarget(),
                           picture_buffer->gl_image(), false);
      }
    }

    bool inserted = output_picture_buffers_
                        .insert(std::make_pair(buffers[buffer_index].id(),
                                               std::move(picture_buffer)))
                        .second;
    DCHECK(inserted);
  }

  ProcessPendingSamples();
  if (pending_flush_ || processing_config_changed_) {
    decoder_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DXVAVideoDecodeAccelerator::FlushInternal,
                                  base::Unretained(this)));
  }
}

void DXVAVideoDecodeAccelerator::ReusePictureBuffer(int32_t picture_buffer_id) {
  TRACE_EVENT0("media", "DXVAVideoDecodeAccelerator::ReusePictureBuffer");
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  State state = GetState();
  RETURN_AND_NOTIFY_ON_FAILURE((state != kUninitialized),
                               "Invalid state: " << state, ILLEGAL_STATE, );

  if (output_picture_buffers_.empty() && stale_output_picture_buffers_.empty())
    return;

  OutputBuffers::iterator it = output_picture_buffers_.find(picture_buffer_id);
  // If we didn't find the picture id in the |output_picture_buffers_| map we
  // try the |stale_output_picture_buffers_| map, as this may have been an
  // output picture buffer from before a resolution change, that at resolution
  // change time had yet to be displayed. The client is calling us back to tell
  // us that we can now recycle this picture buffer, so if we were waiting to
  // dispose of it we now can.
  if (it == output_picture_buffers_.end()) {
    if (!stale_output_picture_buffers_.empty()) {
      it = stale_output_picture_buffers_.find(picture_buffer_id);
      RETURN_AND_NOTIFY_ON_FAILURE(it != stale_output_picture_buffers_.end(),
                                   "Invalid picture id: " << picture_buffer_id,
                                   INVALID_ARGUMENT, );
      main_thread_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &DXVAVideoDecodeAccelerator::DeferredDismissStaleBuffer,
              weak_ptr_, picture_buffer_id));
    }
    return;
  }

  if (it->second->available() || it->second->waiting_to_reuse())
    return;

  RETURN_AND_NOTIFY_ON_FAILURE(make_context_current_cb_.Run(),
                               "Failed to make context current",
                               PLATFORM_FAILURE, );
  if (use_keyed_mutex_ || using_angle_device_) {
    RETURN_AND_NOTIFY_ON_FAILURE(it->second->ReusePictureBuffer(),
                                 "Failed to reuse picture buffer",
                                 PLATFORM_FAILURE, );
    if (bind_image_cb_ && (GetPictureBufferMechanism() ==
                           PictureBufferMechanism::DELAYED_COPY_TO_NV12)) {
      // Unbind the image to ensure it will be copied again the next time it's
      // needed.
      for (uint32_t client_id :
           it->second->picture_buffer().client_texture_ids()) {
        bind_image_cb_.Run(client_id, GetTextureTarget(),
                           it->second->gl_image(), false);
      }
    }

    ProcessPendingSamples();
    if (pending_flush_) {
      decoder_thread_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&DXVAVideoDecodeAccelerator::FlushInternal,
                                    base::Unretained(this)));
    }
  } else {
    it->second->ResetReuseFence();

    WaitForOutputBuffer(picture_buffer_id, 0);
  }
}

void DXVAVideoDecodeAccelerator::WaitForOutputBuffer(int32_t picture_buffer_id,
                                                     int count) {
  TRACE_EVENT0("media", "DXVAVideoDecodeAccelerator::WaitForOutputBuffer");
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  OutputBuffers::iterator it = output_picture_buffers_.find(picture_buffer_id);
  if (it == output_picture_buffers_.end())
    return;

  DXVAPictureBuffer* picture_buffer = it->second.get();

  DCHECK(!picture_buffer->available());
  DCHECK(picture_buffer->waiting_to_reuse());

  gl::GLFence* fence = picture_buffer->reuse_fence();
  RETURN_AND_NOTIFY_ON_FAILURE(make_context_current_cb_.Run(),
                               "Failed to make context current",
                               PLATFORM_FAILURE, );
  if (count <= kMaxIterationsForANGLEReuseFlush && !fence->HasCompleted()) {
    main_thread_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DXVAVideoDecodeAccelerator::WaitForOutputBuffer,
                       weak_ptr_, picture_buffer_id, count + 1),
        base::TimeDelta::FromMilliseconds(kFlushDecoderSurfaceTimeoutMs));
    return;
  }
  RETURN_AND_NOTIFY_ON_FAILURE(picture_buffer->ReusePictureBuffer(),
                               "Failed to reuse picture buffer",
                               PLATFORM_FAILURE, );

  ProcessPendingSamples();
  if (pending_flush_ || processing_config_changed_) {
    decoder_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DXVAVideoDecodeAccelerator::FlushInternal,
                                  base::Unretained(this)));
  }
}

void DXVAVideoDecodeAccelerator::Flush() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << "DXVAVideoDecodeAccelerator::Flush";

  State state = GetState();
  RETURN_AND_NOTIFY_ON_FAILURE((state == kNormal || state == kStopped),
                               "Unexpected decoder state: " << state,
                               ILLEGAL_STATE, );

  SetState(kFlushing);

  pending_flush_ = true;

  // If we receive a flush while processing a video stream config change,  then
  // we treat this as a regular flush, i.e we process queued decode packets,
  // etc.
  // We are resetting the processing_config_changed_ flag here which means that
  // we won't be tearing down the decoder instance and recreating it to handle
  // the changed configuration. The expectation here is that after the decoder
  // is drained it should be able to handle a changed configuration.
  // TODO(ananta)
  // If a flush is sufficient to get the decoder to process video stream config
  // changes correctly, then we don't need to tear down the decoder instance
  // and recreate it. Check if this is indeed the case.
  processing_config_changed_ = false;

  decoder_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DXVAVideoDecodeAccelerator::FlushInternal,
                                base::Unretained(this)));
}

void DXVAVideoDecodeAccelerator::Reset() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << "DXVAVideoDecodeAccelerator::Reset";

  State state = GetState();
  RETURN_AND_NOTIFY_ON_FAILURE(
      (state == kNormal || state == kStopped || state == kFlushing),
      "Reset: invalid state: " << state, ILLEGAL_STATE, );

  StopDecoderThread();

  if (state == kFlushing)
    NotifyFlushDone();

  SetState(kResetting);

  // If we have pending output frames waiting for display then we drop those
  // frames and set the corresponding picture buffer as available.
  PendingOutputSamples::iterator index;
  for (index = pending_output_samples_.begin();
       index != pending_output_samples_.end(); ++index) {
    if (index->picture_buffer_id != -1) {
      OutputBuffers::iterator it =
          output_picture_buffers_.find(index->picture_buffer_id);
      if (it != output_picture_buffers_.end()) {
        DXVAPictureBuffer* picture_buffer = it->second.get();
        if (picture_buffer->state() == DXVAPictureBuffer::State::BOUND ||
            picture_buffer->state() == DXVAPictureBuffer::State::COPYING) {
          picture_buffer->ReusePictureBuffer();
        }
      }
    }
  }

  pending_output_samples_.clear();

  RETURN_AND_NOTIFY_ON_FAILURE(SendMFTMessage(MFT_MESSAGE_COMMAND_FLUSH, 0),
                               "Reset: Failed to send message.",
                               PLATFORM_FAILURE, );

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DXVAVideoDecodeAccelerator::NotifyInputBuffersDropped,
                     weak_ptr_, std::move(pending_input_buffers_)));
  pending_input_buffers_.clear();
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DXVAVideoDecodeAccelerator::NotifyResetDone, weak_ptr_));

  RETURN_AND_NOTIFY_ON_FAILURE(StartDecoderThread(),
                               "Failed to start decoder thread.",
                               PLATFORM_FAILURE, );
  SetState(kNormal);
}

void DXVAVideoDecodeAccelerator::Destroy() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  Invalidate();
  delete this;
}

bool DXVAVideoDecodeAccelerator::TryToSetupDecodeOnSeparateThread(
    const base::WeakPtr<Client>& decode_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& decode_task_runner) {
  return false;
}

GLenum DXVAVideoDecodeAccelerator::GetSurfaceInternalFormat() const {
  return GL_BGRA_EXT;
}

// static
VideoDecodeAccelerator::SupportedProfiles
DXVAVideoDecodeAccelerator::GetSupportedProfiles(
    const gpu::GpuPreferences& preferences,
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  TRACE_EVENT0("gpu,startup",
               "DXVAVideoDecodeAccelerator::GetSupportedProfiles");

  SupportedProfiles profiles;
  for (const wchar_t* mfdll : kMediaFoundationVideoDecoderDLLs) {
    if (!::GetModuleHandle(mfdll)) {
      // Windows N is missing the media foundation DLLs unless the media
      // feature pack is installed.
      DVLOG(ERROR) << mfdll << " is required for hardware video decoding";
      return profiles;
    }
  }

  // On Windows 7 the maximum resolution supported by media foundation is
  // 1920 x 1088. We use 1088 to account for 16x16 macroblocks.
  ResolutionPair max_h264_resolutions(gfx::Size(1920, 1088), gfx::Size());

  // VP9 has no default resolutions since it may not even be supported.
  ResolutionPair max_vp9_profile0_resolutions;
  ResolutionPair max_vp9_profile2_resolutions;

  GetResolutionsForDecoders(
      {DXVA2_ModeH264_E, DXVA2_Intel_ModeH264_E},
      gl::QueryD3D11DeviceObjectFromANGLE(), workarounds, &max_h264_resolutions,
      &max_vp9_profile0_resolutions, &max_vp9_profile2_resolutions);

  for (const auto& supported_profile : kSupportedProfiles) {
    const bool is_h264 = supported_profile >= H264PROFILE_MIN &&
                         supported_profile <= H264PROFILE_MAX;
    const bool is_vp9 = supported_profile >= VP9PROFILE_MIN &&
                        supported_profile <= VP9PROFILE_MAX;
    DCHECK(is_h264 || is_vp9);

    ResolutionPair max_resolutions;
    if (is_h264) {
      max_resolutions = max_h264_resolutions;
    } else if (supported_profile == VP9PROFILE_PROFILE0) {
      max_resolutions = max_vp9_profile0_resolutions;
    } else if (supported_profile == VP9PROFILE_PROFILE2) {
      max_resolutions = max_vp9_profile2_resolutions;
    }

    // Skip adding VP9 profiles if it's not supported or disabled.
    if (is_vp9 && max_resolutions.first.IsEmpty())
      continue;

    // Windows Media Foundation H.264 decoding does not support decoding videos
    // with any dimension smaller than 48 pixels:
    // http://msdn.microsoft.com/en-us/library/windows/desktop/dd797815
    //
    // TODO(dalecurtis): These values are too low. We should only be using
    // hardware decode for videos above ~360p, see http://crbug.com/684792.
    const gfx::Size min_resolution =
        is_h264 ? gfx::Size(48, 48) : gfx::Size(16, 16);

    {
      SupportedProfile profile;
      profile.profile = supported_profile;
      profile.min_resolution = min_resolution;
      profile.max_resolution = max_resolutions.first;
      profiles.push_back(profile);
    }

    const gfx::Size portrait_max_resolution = max_resolutions.second;
    if (!portrait_max_resolution.IsEmpty()) {
      SupportedProfile profile;
      profile.profile = supported_profile;
      profile.min_resolution = min_resolution;
      profile.max_resolution = portrait_max_resolution;
      profiles.push_back(profile);
    }
  }

  return profiles;
}

// static
void DXVAVideoDecodeAccelerator::PreSandboxInitialization() {
  for (const wchar_t* mfdll : kMediaFoundationVideoDecoderDLLs)
    ::LoadLibrary(mfdll);
  ::LoadLibrary(L"dxva2.dll");

  if (base::win::GetVersion() >= base::win::Version::WIN8) {
    LoadLibrary(L"msvproc.dll");
  } else {
#if defined(ENABLE_DX11_FOR_WIN7)
    LoadLibrary(L"mshtmlmedia.dll");
#endif
  }
}

bool DXVAVideoDecodeAccelerator::InitDecoder(VideoCodecProfile profile) {
  HMODULE decoder_dll = NULL;

  CLSID clsid = {};

  // Profile must fall within the valid range for one of the supported codecs.
  if (profile >= H264PROFILE_MIN && profile <= H264PROFILE_MAX) {
    // We mimic the steps CoCreateInstance uses to instantiate the object. This
    // was previously done because it failed inside the sandbox, and now is done
    // as a more minimal approach to avoid other side-effects CCI might have (as
    // we are still in a reduced sandbox).
    decoder_dll = ::GetModuleHandle(L"msmpeg2vdec.dll");
    RETURN_ON_FAILURE(decoder_dll,
                      "msmpeg2vdec.dll required for decoding is not loaded",
                      false);

    // Check version of DLL, version 6.1.7140 is blacklisted due to high crash
    // rates in browsers loading that DLL. If that is the version installed we
    // fall back to software decoding. See crbug/403440.
    std::unique_ptr<FileVersionInfo> version_info(
        FileVersionInfo::CreateFileVersionInfoForModule(decoder_dll));
    RETURN_ON_FAILURE(version_info, "unable to get version of msmpeg2vdec.dll",
                      false);
    base::string16 file_version = version_info->file_version();
    RETURN_ON_FAILURE(file_version.find(L"6.1.7140") == base::string16::npos,
                      "blacklisted version of msmpeg2vdec.dll 6.1.7140", false);
    codec_ = kCodecH264;
    clsid = __uuidof(CMSH264DecoderMFT);
  } else if (enable_accelerated_vpx_decode_ &&
             (profile >= VP9PROFILE_PROFILE0 &&
              profile <= VP9PROFILE_PROFILE3)) {
    codec_ = kCodecVP9;
    clsid = CLSID_MSVPxDecoder;
    decoder_dll = ::LoadLibrary(kMSVP9DecoderDLLName);
    if (decoder_dll)
      using_ms_vp9_mft_ = true;
  }

  if (!decoder_dll) {
    RETURN_ON_FAILURE(false, "Unsupported codec.", false);
  }

  HRESULT hr =
      CreateCOMObjectFromDll(decoder_dll, clsid, IID_PPV_ARGS(&decoder_));
  RETURN_ON_HR_FAILURE(hr, "Failed to create decoder instance", false);

  RETURN_ON_FAILURE(CheckDecoderDxvaSupport(),
                    "Failed to check decoder DXVA support", false);

  ULONG_PTR device_manager_to_use = NULL;
  if (use_dx11_) {
    CHECK(create_dxgi_device_manager_);
    if (media_log_)
      MEDIA_LOG(INFO, media_log_) << "Using D3D11 device for DXVA";
    RETURN_AND_NOTIFY_ON_FAILURE(CreateDX11DevManager(),
                                 "Failed to initialize DX11 device and manager",
                                 PLATFORM_FAILURE, false);
    device_manager_to_use =
        reinterpret_cast<ULONG_PTR>(d3d11_device_manager_.Get());
  } else {
    if (media_log_)
      MEDIA_LOG(INFO, media_log_) << "Using D3D9 device for DXVA";
    RETURN_AND_NOTIFY_ON_FAILURE(CreateD3DDevManager(),
                                 "Failed to initialize D3D device and manager",
                                 PLATFORM_FAILURE, false);
    device_manager_to_use = reinterpret_cast<ULONG_PTR>(device_manager_.Get());
  }

  hr = decoder_->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER,
                                device_manager_to_use);
  if (use_dx11_) {
    RETURN_ON_HR_FAILURE(hr, "Failed to pass DX11 manager to decoder", false);
  } else {
    RETURN_ON_HR_FAILURE(hr, "Failed to pass D3D manager to decoder", false);
  }

  if (!gl::GLSurfaceEGL::IsPixelFormatFloatSupported())
    use_fp16_ = false;

  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();

  while (true) {
    std::vector<EGLint> config_attribs = {EGL_BUFFER_SIZE,  32,
                                          EGL_RED_SIZE,     use_fp16_ ? 16 : 8,
                                          EGL_GREEN_SIZE,   use_fp16_ ? 16 : 8,
                                          EGL_BLUE_SIZE,    use_fp16_ ? 16 : 8,
                                          EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                                          EGL_ALPHA_SIZE,   0};
    if (use_fp16_) {
      config_attribs.push_back(EGL_COLOR_COMPONENT_TYPE_EXT);
      config_attribs.push_back(EGL_COLOR_COMPONENT_TYPE_FLOAT_EXT);
    }
    config_attribs.push_back(EGL_NONE);

    EGLint num_configs = 0;

    if (eglChooseConfig(egl_display, config_attribs.data(), NULL, 0,
                        &num_configs) &&
        num_configs > 0) {
      std::vector<EGLConfig> configs(num_configs);
      if (eglChooseConfig(egl_display, config_attribs.data(), configs.data(),
                          num_configs, &num_configs)) {
        egl_config_ = configs[0];
        for (int i = 0; i < num_configs; i++) {
          EGLint red_bits;
          eglGetConfigAttrib(egl_display, configs[i], EGL_RED_SIZE, &red_bits);
          // Try to pick a configuration with the right number of bits rather
          // than one that just has enough bits.
          if (red_bits == (use_fp16_ ? 16 : 8)) {
            egl_config_ = configs[i];
            break;
          }
        }
      }

      if (!num_configs) {
        if (use_fp16_) {
          // Try again, but without use_fp16_
          use_fp16_ = false;
          continue;
        }
        return false;
      }
    }

    break;
  }

  if (use_fp16_) {
    // TODO(hubbe): Share/copy P010/P016 textures.
    DisableSharedTextureSupport();
    support_copy_nv12_textures_ = false;
  }

  return SetDecoderMediaTypes();
}

bool DXVAVideoDecodeAccelerator::CheckDecoderDxvaSupport() {
  Microsoft::WRL::ComPtr<IMFAttributes> attributes;
  HRESULT hr = decoder_->GetAttributes(&attributes);
  RETURN_ON_HR_FAILURE(hr, "Failed to get decoder attributes", false);

  UINT32 dxva = 0;
  hr = attributes->GetUINT32(MF_SA_D3D_AWARE, &dxva);
  RETURN_ON_HR_FAILURE(hr, "Failed to check if decoder supports DXVA", false);

  if (codec_ == kCodecH264) {
    hr = attributes->SetUINT32(CODECAPI_AVDecVideoAcceleration_H264, TRUE);
    RETURN_ON_HR_FAILURE(hr, "Failed to enable DXVA H/W decoding", false);
  }

  if (enable_low_latency_) {
    hr = attributes->SetUINT32(CODECAPI_AVLowLatencyMode, TRUE);
    if (SUCCEEDED(hr)) {
      DVLOG(1) << "Successfully set Low latency mode on decoder.";
    } else {
      DVLOG(1) << "Failed to set Low latency mode on decoder. Error: " << hr;
    }
  }

  // Each picture buffer can store a sample, plus one in
  // pending_output_samples_. The decoder adds this number to the number of
  // reference pictures it expects to need and uses that to determine the
  // array size of the output texture.
  const int kMaxOutputSamples = num_picture_buffers_requested_ + 1;
  attributes->SetUINT32(MF_SA_MINIMUM_OUTPUT_SAMPLE_COUNT_PROGRESSIVE,
                        kMaxOutputSamples);
  attributes->SetUINT32(MF_SA_MINIMUM_OUTPUT_SAMPLE_COUNT, kMaxOutputSamples);

  auto* gl_context = get_gl_context_cb_.Run();
  RETURN_ON_FAILURE(gl_context, "Couldn't get GL context", false);

  // The decoder should use DX11 iff
  // 1. The underlying H/W decoder supports it.
  // 2. We have a pointer to the MFCreateDXGIDeviceManager function needed for
  //    this. This should always be true for Windows 8+.
  // 3. ANGLE is using DX11.
  if (create_dxgi_device_manager_ &&
      (gl_context->GetGLRenderer().find("Direct3D11") != std::string::npos)) {
    UINT32 dx11_aware = 0;
    attributes->GetUINT32(MF_SA_D3D11_AWARE, &dx11_aware);
    use_dx11_ = !!dx11_aware;
  }

  use_keyed_mutex_ =
      use_dx11_ && gl::GLSurfaceEGL::HasEGLExtension("EGL_ANGLE_keyed_mutex");

  if (!use_dx11_ ||
      !gl::g_driver_egl.ext.b_EGL_ANGLE_stream_producer_d3d_texture ||
      !gl::g_driver_egl.ext.b_EGL_KHR_stream ||
      !gl::g_driver_egl.ext.b_EGL_KHR_stream_consumer_gltexture ||
      !gl::g_driver_egl.ext.b_EGL_NV_stream_consumer_gltexture_yuv) {
    DisableSharedTextureSupport();
    support_copy_nv12_textures_ = false;
  }

  return true;
}

bool DXVAVideoDecodeAccelerator::SetDecoderMediaTypes() {
  RETURN_ON_FAILURE(SetDecoderInputMediaType(),
                    "Failed to set decoder input media type", false);
  return SetDecoderOutputMediaType(MFVideoFormat_NV12) ||
         SetDecoderOutputMediaType(MFVideoFormat_P010) ||
         SetDecoderOutputMediaType(MFVideoFormat_P016);
}

bool DXVAVideoDecodeAccelerator::SetDecoderInputMediaType() {
  Microsoft::WRL::ComPtr<IMFMediaType> media_type;
  HRESULT hr = MFCreateMediaType(&media_type);
  RETURN_ON_HR_FAILURE(hr, "MFCreateMediaType failed", false);

  hr = media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  RETURN_ON_HR_FAILURE(hr, "Failed to set major input type", false);

  if (codec_ == kCodecH264) {
    hr = media_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
  } else if (codec_ == kCodecVP9) {
    hr = media_type->SetGUID(MF_MT_SUBTYPE, MEDIASUBTYPE_VP90);
  } else {
    NOTREACHED();
    RETURN_ON_FAILURE(false, "Unsupported codec on input media type.", false);
  }
  RETURN_ON_HR_FAILURE(hr, "Failed to set subtype", false);

  if (using_ms_vp9_mft_) {
    hr = MFSetAttributeSize(media_type.Get(), MF_MT_FRAME_SIZE,
                            config_.initial_expected_coded_size.width(),
                            config_.initial_expected_coded_size.height());
    RETURN_ON_HR_FAILURE(hr, "Failed to set attribute size", false);

    hr = media_type->SetUINT32(MF_MT_INTERLACE_MODE,
                               MFVideoInterlace_Progressive);
    RETURN_ON_HR_FAILURE(hr, "Failed to set interlace mode", false);
  } else {
    // Not sure about this. msdn recommends setting this value on the input
    // media type.
    hr = media_type->SetUINT32(MF_MT_INTERLACE_MODE,
                               MFVideoInterlace_MixedInterlaceOrProgressive);
    RETURN_ON_HR_FAILURE(hr, "Failed to set interlace mode", false);
  }

  // These bind flags _must_ be set before SetInputType or SetOutputType to
  // ensure that we get the proper surfaces created under the hood.
  if (GetPictureBufferMechanism() == PictureBufferMechanism::BIND) {
    Microsoft::WRL::ComPtr<IMFAttributes> out_attributes;
    HRESULT hr = decoder_->GetOutputStreamAttributes(0, &out_attributes);
    RETURN_ON_HR_FAILURE(hr, "Failed to get stream attributes", false);
    out_attributes->SetUINT32(MF_SA_D3D11_BINDFLAGS,
                              D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DECODER);
    // TODO(sunnyps): Find if we can always set resource sharing to disabled
    if (gl::DirectCompositionSurfaceWin::IsDecodeSwapChainSupported()) {
      // Decode swap chains do not support shared resources.
      out_attributes->SetUINT32(MF_SA_D3D11_SHARED, FALSE);
    } else {
      // For some reason newer Intel drivers need D3D11_BIND_DECODER textures to
      // be created with a share handle or they'll crash in
      // CreateShaderResourceView.  Technically MF_SA_D3D11_SHARED_WITHOUT_MUTEX
      // is only honored by the sample allocator, not by the media foundation
      // transform, but Microsoft's h.264 transform happens to pass it through.
      out_attributes->SetUINT32(MF_SA_D3D11_SHARED_WITHOUT_MUTEX, TRUE);
    }
  }

  hr = decoder_->SetInputType(0, media_type.Get(), 0);  // No flags
  RETURN_ON_HR_FAILURE(hr, "Failed to set decoder input type", false);
  return true;
}

bool DXVAVideoDecodeAccelerator::SetDecoderOutputMediaType(
    const GUID& subtype) {
  return SetTransformOutputType(decoder_.Get(), subtype, 0, 0);
}

bool DXVAVideoDecodeAccelerator::SendMFTMessage(MFT_MESSAGE_TYPE msg,
                                                int32_t param) {
  HRESULT hr = decoder_->ProcessMessage(msg, param);
  return SUCCEEDED(hr);
}

// Gets the minimum buffer sizes for input and output samples. The MFT will not
// allocate buffer for input nor output, so we have to do it ourselves and make
// sure they're the correct size. We only provide decoding if DXVA is enabled.
bool DXVAVideoDecodeAccelerator::GetStreamsInfoAndBufferReqs() {
  HRESULT hr = decoder_->GetInputStreamInfo(0, &input_stream_info_);
  RETURN_ON_HR_FAILURE(hr, "Failed to get input stream info", false);

  hr = decoder_->GetOutputStreamInfo(0, &output_stream_info_);
  RETURN_ON_HR_FAILURE(hr, "Failed to get decoder output stream info", false);

  DVLOG(1) << "Input stream info: ";
  DVLOG(1) << "Max latency: " << input_stream_info_.hnsMaxLatency;
  if (codec_ == kCodecH264) {
    // There should be three flags, one for requiring a whole frame be in a
    // single sample, one for requiring there be one buffer only in a single
    // sample, and one that specifies a fixed sample size. (as in cbSize)
    CHECK_EQ(input_stream_info_.dwFlags, 0x7u);
  }

  DVLOG(1) << "Min buffer size: " << input_stream_info_.cbSize;
  DVLOG(1) << "Max lookahead: " << input_stream_info_.cbMaxLookahead;
  DVLOG(1) << "Alignment: " << input_stream_info_.cbAlignment;

  DVLOG(1) << "Output stream info: ";
  // The flags here should be the same and mean the same thing, except when
  // DXVA is enabled, there is an extra 0x100 flag meaning decoder will
  // allocate its own sample.
  DVLOG(1) << "Flags: " << std::hex << std::showbase
           << output_stream_info_.dwFlags;
  if (codec_ == kCodecH264) {
    CHECK_EQ(output_stream_info_.dwFlags, 0x107u);
  }
  DVLOG(1) << "Min buffer size: " << output_stream_info_.cbSize;
  DVLOG(1) << "Alignment: " << output_stream_info_.cbAlignment;
  return true;
}

void DXVAVideoDecodeAccelerator::DoDecode(const gfx::Rect& visible_rect,
                                          const gfx::ColorSpace& color_space) {
  TRACE_EVENT0("media", "DXVAVideoDecodeAccelerator::DoDecode");
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  // This function is also called from FlushInternal in a loop which could
  // result in the state transitioning to kStopped due to no decoded output.
  State state = GetState();
  RETURN_AND_NOTIFY_ON_FAILURE(
      (state == kNormal || state == kFlushing || state == kStopped),
      "DoDecode: not in normal/flushing/stopped state", ILLEGAL_STATE, );

  if (D3D11Device())
    g_last_device_removed_reason = D3D11Device()->GetDeviceRemovedReason();

  Microsoft::WRL::ComPtr<IMFSample> output_sample;
  int retries = 10;
  while (true) {
    output_sample.Reset();
    MFT_OUTPUT_DATA_BUFFER output_data_buffer = {0};
    DWORD status = 0;
    HRESULT hr;
    g_last_process_output_time = GetCurrentQPC();
    hr = decoder_->ProcessOutput(0,  // No flags
                                 1,  // # of out streams to pull from
                                 &output_data_buffer, &status);
    IMFCollection* events = output_data_buffer.pEvents;
    if (events != NULL) {
      DVLOG(1) << "Got events from ProcessOuput, but discarding";
      events->Release();
    }
    output_sample.Attach(output_data_buffer.pSample);
    if (FAILED(hr)) {
      // A stream change needs further ProcessInput calls to get back decoder
      // output which is why we need to set the state to stopped.
      if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
        if (!SetDecoderOutputMediaType(MFVideoFormat_NV12) &&
            !SetDecoderOutputMediaType(MFVideoFormat_P010) &&
            !SetDecoderOutputMediaType(MFVideoFormat_P016)) {
          // Decoder didn't let us set NV12 output format. Not sure as to why
          // this can happen. Give up in disgust.
          NOTREACHED() << "Failed to set decoder output media type to NV12";
          SetState(kStopped);
        } else {
          if (retries-- > 0) {
            DVLOG(1) << "Received format change from the decoder, retrying.";
            continue;  // Retry
          } else {
            RETURN_AND_NOTIFY_ON_FAILURE(
                false, "Received too many format changes from decoder.",
                PLATFORM_FAILURE, );
          }
        }
        return;
      } else if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
        // No more output from the decoder. Stop playback.
        SetState(kStopped);
        return;
      } else if (hr == E_FAIL) {
        // This shouldn't happen, but does, log it and ignore it.
        // https://crbug.com/839057
        LOG(ERROR) << "Received E_FAIL in DoDecode()";
        return;
      } else {
        // Unknown error, stop playback and log error.
        SetState(kStopped);
        RETURN_AND_NOTIFY_ON_FAILURE(hr, "Unhandled error in DoDecode()",
                                     PLATFORM_FAILURE, );
      }
    }

    break;  // No more retries needed.
  }
  TRACE_EVENT_ASYNC_END0("gpu", "DXVAVideoDecodeAccelerator.Decoding", this);

  TRACE_COUNTER1("DXVA Decoding", "TotalPacketsBeforeDecode",
                 inputs_before_decode_);

  inputs_before_decode_ = 0;

  RETURN_AND_NOTIFY_ON_FAILURE(
      ProcessOutputSample(output_sample, visible_rect, color_space),
      "Failed to process output sample.", PLATFORM_FAILURE, );
}

bool DXVAVideoDecodeAccelerator::ProcessOutputSample(
    Microsoft::WRL::ComPtr<IMFSample> sample,
    const gfx::Rect& visible_rect,
    const gfx::ColorSpace& color_space) {
  RETURN_ON_FAILURE(sample, "Decode succeeded with NULL output sample", false);

  LONGLONG input_buffer_id = 0;
  RETURN_ON_HR_FAILURE(sample->GetSampleTime(&input_buffer_id),
                       "Failed to get input buffer id associated with sample",
                       false);

  {
    base::AutoLock lock(decoder_lock_);
    DCHECK(pending_output_samples_.empty());
    pending_output_samples_.push_back(
        PendingSampleInfo(input_buffer_id, sample, visible_rect, color_space));
  }

  if (pictures_requested_) {
    DVLOG(1) << "Waiting for picture slots from the client.";
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&DXVAVideoDecodeAccelerator::ProcessPendingSamples,
                       weak_ptr_));
    return true;
  }

  int width = 0;
  int height = 0;
  if (!GetVideoFrameDimensions(sample.Get(), &width, &height)) {
    RETURN_ON_FAILURE(false, "Failed to get D3D surface from output sample",
                      false);
  }

  // Go ahead and request picture buffers.
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DXVAVideoDecodeAccelerator::RequestPictureBuffers,
                     weak_ptr_, width, height));

  pictures_requested_ = true;
  return true;
}

void DXVAVideoDecodeAccelerator::ProcessPendingSamples() {
  TRACE_EVENT0("media", "DXVAVideoDecodeAccelerator::ProcessPendingSamples");
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  if (output_picture_buffers_.empty())
    return;

  RETURN_AND_NOTIFY_ON_FAILURE(make_context_current_cb_.Run(),
                               "Failed to make context current",
                               PLATFORM_FAILURE, );

  OutputBuffers::iterator index;

  for (index = output_picture_buffers_.begin();
       index != output_picture_buffers_.end() && OutputSamplesPresent();
       ++index) {
    if (index->second->available()) {
      PendingSampleInfo* pending_sample = NULL;
      {
        base::AutoLock lock(decoder_lock_);
        PendingSampleInfo& sample_info = pending_output_samples_.front();
        if (sample_info.picture_buffer_id != -1)
          continue;
        pending_sample = &sample_info;
      }

      int width = 0;
      int height = 0;
      if (!GetVideoFrameDimensions(pending_sample->output_sample.Get(), &width,
                                   &height)) {
        RETURN_AND_NOTIFY_ON_FAILURE(
            false, "Failed to get D3D surface from output sample",
            PLATFORM_FAILURE, );
      }

      if (width != index->second->size().width() ||
          height != index->second->size().height()) {
        HandleResolutionChanged(width, height);
        return;
      }

      pending_sample->picture_buffer_id = index->second->id();
      index->second->set_bound();
      index->second->set_visible_rect(pending_sample->visible_rect);
      index->second->set_color_space(pending_sample->color_space);

      if (index->second->CanBindSamples()) {
        main_thread_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(
                &DXVAVideoDecodeAccelerator::BindPictureBufferToSample,
                weak_ptr_, pending_sample->output_sample,
                pending_sample->picture_buffer_id,
                pending_sample->input_buffer_id));
        continue;
      }

      Microsoft::WRL::ComPtr<IMFMediaBuffer> output_buffer;
      HRESULT hr =
          pending_sample->output_sample->GetBufferByIndex(0, &output_buffer);
      RETURN_AND_NOTIFY_ON_HR_FAILURE(
          hr, "Failed to get buffer from output sample", PLATFORM_FAILURE, );

      Microsoft::WRL::ComPtr<IDirect3DSurface9> surface;
      ComD3D11Texture2D d3d11_texture;

      if (use_dx11_) {
        Microsoft::WRL::ComPtr<IMFDXGIBuffer> dxgi_buffer;
        hr = output_buffer.As(&dxgi_buffer);
        RETURN_AND_NOTIFY_ON_HR_FAILURE(
            hr, "Failed to get DXGIBuffer from output sample",
            PLATFORM_FAILURE, );
        hr =
            dxgi_buffer->GetResource(__uuidof(ID3D11Texture2D), &d3d11_texture);
      } else {
        hr = MFGetService(output_buffer.Get(), MR_BUFFER_SERVICE,
                          IID_PPV_ARGS(&surface));
      }
      RETURN_AND_NOTIFY_ON_HR_FAILURE(
          hr, "Failed to get surface from output sample", PLATFORM_FAILURE, );

      RETURN_AND_NOTIFY_ON_FAILURE(
          index->second->CopyOutputSampleDataToPictureBuffer(
              this, surface.Get(), d3d11_texture.Get(),
              pending_sample->input_buffer_id),
          "Failed to copy output sample", PLATFORM_FAILURE, );
    }
  }
}

void DXVAVideoDecodeAccelerator::StopOnError(
    VideoDecodeAccelerator::Error error) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DXVAVideoDecodeAccelerator::StopOnError,
                                  weak_ptr_, error));
    return;
  }

  if (client_)
    client_->NotifyError(error);
  client_ = NULL;

#ifdef _DEBUG
  if (using_debug_device_) {
    // MSDN says that this needs to be casted twice, then GetMessage should
    // be called with a malloc.
    Microsoft::WRL::ComPtr<ID3D11Debug> debug_layer;
    if (SUCCEEDED(d3d11_device_.As(&debug_layer))) {
      Microsoft::WRL::ComPtr<ID3D11InfoQueue> message_layer;
      if (SUCCEEDED(debug_layer.As(&message_layer))) {
        uint64_t message_count = message_layer->GetNumStoredMessages();
        for (uint64_t i = 0; i < message_count; i++) {
          SIZE_T message_size;
          message_layer->GetMessage(i, nullptr, &message_size);
          D3D11_MESSAGE* message =
              reinterpret_cast<D3D11_MESSAGE*>(malloc(message_size));
          if (message) {
            message_layer->GetMessage(i, message, &message_size);
            if (media_log_) {
              MEDIA_LOG(INFO, media_log_) << message->pDescription;
            } else {
              DVLOG(1) << message->pDescription;
            }
            free(message);
          }
        }
      }
    }
  }
#endif

  if (GetState() != kUninitialized) {
    Invalidate();
  }
}

void DXVAVideoDecodeAccelerator::Invalidate() {
  if (GetState() == kUninitialized)
    return;

  // Best effort to make the GL context current.
  make_context_current_cb_.Run();

  StopDecoderThread();
  weak_this_factory_.InvalidateWeakPtrs();
  weak_ptr_ = weak_this_factory_.GetWeakPtr();
  pending_output_samples_.clear();
  decoder_.Reset();
  config_change_detector_.reset();

  // If we are processing a config change, then leave the d3d9/d3d11 objects
  // along with the output picture buffers intact as they can be reused. The
  // output picture buffers may need to be recreated in case the video
  // resolution changes. We already handle that in the
  // HandleResolutionChanged() function.
  if (GetState() != kConfigChange) {
    output_picture_buffers_.clear();
    stale_output_picture_buffers_.clear();
    // We want to continue processing pending input after detecting a config
    // change.
    pending_input_buffers_.clear();
    pictures_requested_ = false;
    if (use_dx11_) {
      d3d11_processor_.Reset();
      enumerator_.Reset();
      video_context_.Reset();
      video_device_.Reset();
      d3d11_device_context_.Reset();
      d3d11_device_.Reset();
      d3d11_device_manager_.Reset();
      d3d11_query_.Reset();
      multi_threaded_.Reset();
      processor_width_ = processor_height_ = 0;
    } else {
      d3d9_.Reset();
      d3d9_device_ex_.Reset();
      device_manager_.Reset();
      query_.Reset();
    }
  }
  sent_drain_message_ = false;
  SetState(kUninitialized);
}

void DXVAVideoDecodeAccelerator::StopDecoderThread() {
  // Try to determine what, if any exception last happened before a hang. See
  // http://crbug.com/613701
  uint64_t last_process_output_time = g_last_process_output_time;
  HRESULT last_device_removed_reason = g_last_device_removed_reason;
  LARGE_INTEGER perf_frequency;
  ::QueryPerformanceFrequency(&perf_frequency);
  uint32_t output_array_size = output_array_size_;
  size_t sample_count;
  {
    base::AutoLock lock(decoder_lock_);
    sample_count = pending_output_samples_.size();
  }
  size_t stale_output_picture_buffers_size =
      stale_output_picture_buffers_.size();
  PictureBufferMechanism mechanism = GetPictureBufferMechanism();

  base::debug::Alias(&last_process_output_time);
  base::debug::Alias(&last_device_removed_reason);
  base::debug::Alias(&perf_frequency.QuadPart);
  base::debug::Alias(&output_array_size);
  base::debug::Alias(&sample_count);
  base::debug::Alias(&stale_output_picture_buffers_size);
  base::debug::Alias(&mechanism);
  decoder_thread_.Stop();
}

void DXVAVideoDecodeAccelerator::NotifyInputBufferRead(int input_buffer_id) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  if (client_)
    client_->NotifyEndOfBitstreamBuffer(input_buffer_id);
}

void DXVAVideoDecodeAccelerator::NotifyFlushDone() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  if (client_ && pending_flush_) {
    pending_flush_ = false;
    {
      base::AutoLock lock(decoder_lock_);
      sent_drain_message_ = false;
    }

    client_->NotifyFlushDone();
  }
}

void DXVAVideoDecodeAccelerator::NotifyResetDone() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  if (client_)
    client_->NotifyResetDone();
}

void DXVAVideoDecodeAccelerator::RequestPictureBuffers(int width, int height) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  // This task could execute after the decoder has been torn down.
  if (GetState() != kUninitialized && client_) {
    // When sharing NV12 textures, the client needs to provide 2 texture IDs
    // per picture buffer, 1 for the Y channel and 1 for the UV channels.
    // They're shared to ANGLE using EGL_NV_stream_consumer_gltexture_yuv, so
    // they need to be GL_TEXTURE_EXTERNAL_OES.
    bool provide_nv12_textures =
        GetPictureBufferMechanism() != PictureBufferMechanism::COPY_TO_RGB;
    client_->ProvidePictureBuffers(
        num_picture_buffers_requested_,
        provide_nv12_textures ? PIXEL_FORMAT_NV12 : PIXEL_FORMAT_UNKNOWN,
        provide_nv12_textures ? 2 : 1, gfx::Size(width, height),
        GetTextureTarget());
  }
}

void DXVAVideoDecodeAccelerator::NotifyPictureReady(
    int picture_buffer_id,
    int input_buffer_id,
    const gfx::Rect& visible_rect,
    const gfx::ColorSpace& color_space,
    bool allow_overlay) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  // This task could execute after the decoder has been torn down.
  if (GetState() != kUninitialized && client_) {
    Picture picture(picture_buffer_id, input_buffer_id, visible_rect,
                    color_space, allow_overlay);
    client_->PictureReady(picture);
  }
}

void DXVAVideoDecodeAccelerator::NotifyInputBuffersDropped(
    const PendingInputs& pending_buffers) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  if (!client_)
    return;

  for (const auto& buffer : pending_buffers) {
    LONGLONG input_buffer_id = 0;
    RETURN_ON_HR_FAILURE(buffer->GetSampleTime(&input_buffer_id),
                         "Failed to get buffer id associated with sample", );
    client_->NotifyEndOfBitstreamBuffer(input_buffer_id);
  }
}

void DXVAVideoDecodeAccelerator::DecodePendingInputBuffers() {
  TRACE_EVENT0("media",
               "DXVAVideoDecodeAccelerator::DecodePendingInputBuffers");
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(!processing_config_changed_);

  State state = GetState();
  RETURN_AND_NOTIFY_ON_FAILURE((state != kUninitialized),
                               "Invalid state: " << state, ILLEGAL_STATE, );

  if (pending_input_buffers_.empty() || OutputSamplesPresent())
    return;

  PendingInputs pending_input_buffers_copy;
  std::swap(pending_input_buffers_, pending_input_buffers_copy);

  for (PendingInputs::iterator it = pending_input_buffers_copy.begin();
       it != pending_input_buffers_copy.end(); ++it) {
    DecodeInternal(*it);
  }
}

void DXVAVideoDecodeAccelerator::FlushInternal() {
  TRACE_EVENT0("media", "DXVAVideoDecodeAccelerator::FlushInternal");
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  // We allow only one output frame to be present at any given time. If we have
  // an output frame, then we cannot complete the flush at this time.
  if (OutputSamplesPresent())
    return;

  // First drain the pending input because once the drain message is sent below,
  // the decoder will ignore further input until it's drained.
  // If we are processing a video configuration change, then we should just
  // the drain the decoder.
  if (!processing_config_changed_ && !pending_input_buffers_.empty()) {
    decoder_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&DXVAVideoDecodeAccelerator::DecodePendingInputBuffers,
                       base::Unretained(this)));
    decoder_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DXVAVideoDecodeAccelerator::FlushInternal,
                                  base::Unretained(this)));
    return;
  }

  {
    base::AutoLock lock(decoder_lock_);
    if (!sent_drain_message_) {
      RETURN_AND_NOTIFY_ON_FAILURE(SendMFTMessage(MFT_MESSAGE_COMMAND_DRAIN, 0),
                                   "Failed to send drain message",
                                   PLATFORM_FAILURE, );
      sent_drain_message_ = true;
    }
  }

  // Attempt to retrieve an output frame from the decoder. If we have one,
  // return and proceed when the output frame is processed. If we don't have a
  // frame then we are done.
  DoDecode(current_visible_rect_, current_color_space_.ToGfxColorSpace());
  if (OutputSamplesPresent())
    return;

  if (!processing_config_changed_) {
    SetState(kFlushing);

    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DXVAVideoDecodeAccelerator::NotifyFlushDone,
                                  weak_ptr_));
  } else {
    processing_config_changed_ = false;
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DXVAVideoDecodeAccelerator::ConfigChanged,
                                  weak_ptr_, config_));
  }

  SetState(kNormal);
}

void DXVAVideoDecodeAccelerator::DecodeInternal(
    const Microsoft::WRL::ComPtr<IMFSample>& sample) {
  TRACE_EVENT0("media", "DXVAVideoDecodeAccelerator::DecodeInternal");
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  if (GetState() == kUninitialized)
    return;

  if (OutputSamplesPresent() || !pending_input_buffers_.empty()) {
    pending_input_buffers_.push_back(sample);
    return;
  }

  // Check if the resolution, bit rate, etc changed in the stream. If yes we
  // reinitialize the decoder to ensure that the stream decodes correctly.
  bool config_changed = false;

  HRESULT hr = CheckConfigChanged(sample.Get(), &config_changed);
  RETURN_AND_NOTIFY_ON_HR_FAILURE(hr, "Failed to check video stream config",
                                  PLATFORM_FAILURE, );

  processing_config_changed_ = config_changed;

  if (config_changed) {
    pending_input_buffers_.push_back(sample);
    FlushInternal();
    return;
  }

  gfx::Rect visible_rect;
  VideoColorSpace color_space = config_.container_color_space;
  if (config_change_detector_) {
    visible_rect = config_change_detector_->current_visible_rect(visible_rect);
    color_space = config_change_detector_->current_color_space(color_space);
  }
  current_visible_rect_ = visible_rect;
  current_color_space_ = color_space;

  if (!inputs_before_decode_) {
    TRACE_EVENT_ASYNC_BEGIN0("gpu", "DXVAVideoDecodeAccelerator.Decoding",
                             this);
  }
  inputs_before_decode_++;
  hr = decoder_->ProcessInput(0, sample.Get(), 0);
  // As per msdn if the decoder returns MF_E_NOTACCEPTING then it means that it
  // has enough data to produce one or more output samples. In this case the
  // recommended options are to
  // 1. Generate new output by calling IMFTransform::ProcessOutput until it
  //    returns MF_E_TRANSFORM_NEED_MORE_INPUT.
  // 2. Flush the input data
  // We implement the first option, i.e to retrieve the output sample and then
  // process the input again. Failure in either of these steps is treated as a
  // decoder failure.
  if (hr == MF_E_NOTACCEPTING) {
    DoDecode(visible_rect, color_space.ToGfxColorSpace());
    // If the DoDecode call resulted in an output frame then we should not
    // process any more input until that frame is copied to the target surface.
    if (!OutputSamplesPresent()) {
      State state = GetState();
      RETURN_AND_NOTIFY_ON_FAILURE(
          (state == kStopped || state == kNormal || state == kFlushing),
          "Failed to process output. Unexpected decoder state: " << state,
          PLATFORM_FAILURE, );
      hr = decoder_->ProcessInput(0, sample.Get(), 0);
    }
    // If we continue to get the MF_E_NOTACCEPTING error we do the following:-
    // 1. Add the input sample to the pending queue.
    // 2. If we don't have any output samples we post the
    //    DecodePendingInputBuffers task to process the pending input samples.
    //    If we have an output sample then the above task is posted when the
    //    output samples are sent to the client.
    // This is because we only support 1 pending output sample at any
    // given time due to the limitation with the Microsoft media foundation
    // decoder where it recycles the output Decoder surfaces.
    if (hr == MF_E_NOTACCEPTING) {
      pending_input_buffers_.push_back(sample);
      decoder_thread_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&DXVAVideoDecodeAccelerator::DecodePendingInputBuffers,
                         base::Unretained(this)));
      return;
    }
  }
  RETURN_AND_NOTIFY_ON_HR_FAILURE(hr, "Failed to process input sample",
                                  PLATFORM_FAILURE, );

  DoDecode(visible_rect, color_space.ToGfxColorSpace());

  State state = GetState();
  RETURN_AND_NOTIFY_ON_FAILURE(
      (state == kStopped || state == kNormal || state == kFlushing),
      "Failed to process output. Unexpected decoder state: " << state,
      ILLEGAL_STATE, );

  LONGLONG input_buffer_id = 0;
  RETURN_ON_HR_FAILURE(
      sample->GetSampleTime(&input_buffer_id),
      "Failed to get input buffer id associated with sample", );
  // The Microsoft Media foundation decoder internally buffers up to 30 frames
  // before returning a decoded frame. We need to inform the client that this
  // input buffer is processed as it may stop sending us further input.
  // Note: This may break clients which expect every input buffer to be
  // associated with a decoded output buffer.
  // TODO(ananta)
  // Do some more investigation into whether it is possible to get the MFT
  // decoder to emit an output packet for every input packet.
  // http://code.google.com/p/chromium/issues/detail?id=108121
  // http://code.google.com/p/chromium/issues/detail?id=150925
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DXVAVideoDecodeAccelerator::NotifyInputBufferRead,
                     weak_ptr_, input_buffer_id));
}

void DXVAVideoDecodeAccelerator::HandleResolutionChanged(int width,
                                                         int height) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DXVAVideoDecodeAccelerator::DismissStaleBuffers,
                     weak_ptr_, false));

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DXVAVideoDecodeAccelerator::RequestPictureBuffers,
                     weak_ptr_, width, height));
}

void DXVAVideoDecodeAccelerator::DismissStaleBuffers(bool force) {
  RETURN_AND_NOTIFY_ON_FAILURE(make_context_current_cb_.Run(),
                               "Failed to make context current",
                               PLATFORM_FAILURE, );

  OutputBuffers::iterator index;

  for (index = output_picture_buffers_.begin();
       index != output_picture_buffers_.end(); ++index) {
    if (force || index->second->available()) {
      DVLOG(1) << "Dismissing picture id: " << index->second->id();
      client_->DismissPictureBuffer(index->second->id());
    } else {
      // Move to |stale_output_picture_buffers_| for deferred deletion.
      stale_output_picture_buffers_.insert(
          std::make_pair(index->first, std::move(index->second)));
    }
  }

  output_picture_buffers_.clear();
}

void DXVAVideoDecodeAccelerator::DeferredDismissStaleBuffer(
    int32_t picture_buffer_id) {
  RETURN_AND_NOTIFY_ON_FAILURE(make_context_current_cb_.Run(),
                               "Failed to make context current",
                               PLATFORM_FAILURE, );

  OutputBuffers::iterator it =
      stale_output_picture_buffers_.find(picture_buffer_id);
  DCHECK(it != stale_output_picture_buffers_.end());
  DVLOG(1) << "Dismissing picture id: " << it->second->id();
  client_->DismissPictureBuffer(it->second->id());
  stale_output_picture_buffers_.erase(it);
}

DXVAVideoDecodeAccelerator::State DXVAVideoDecodeAccelerator::GetState() {
  static_assert(sizeof(State) == sizeof(long), "mismatched type sizes");
  State state = static_cast<State>(
      InterlockedAdd(reinterpret_cast<volatile long*>(&state_), 0));
  return state;
}

void DXVAVideoDecodeAccelerator::SetState(State new_state) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DXVAVideoDecodeAccelerator::SetState,
                                  weak_ptr_, new_state));
    return;
  }

  static_assert(sizeof(State) == sizeof(long), "mismatched type sizes");
  ::InterlockedExchange(reinterpret_cast<volatile long*>(&state_), new_state);
  DCHECK_EQ(state_, new_state);
}

bool DXVAVideoDecodeAccelerator::StartDecoderThread() {
  decoder_thread_.init_com_with_mta(true);
  decoder_thread_.Start();
  decoder_thread_task_runner_ = decoder_thread_.task_runner();
  if (!decoder_thread_task_runner_) {
    LOG(ERROR) << "Failed to initialize decoder thread";
    return false;
  }
  return true;
}

bool DXVAVideoDecodeAccelerator::OutputSamplesPresent() {
  base::AutoLock lock(decoder_lock_);
  return !pending_output_samples_.empty();
}

void DXVAVideoDecodeAccelerator::CopySurface(
    IDirect3DSurface9* src_surface,
    IDirect3DSurface9* dest_surface,
    int picture_buffer_id,
    int input_buffer_id,
    const gfx::ColorSpace& color_space) {
  TRACE_EVENT0("media", "DXVAVideoDecodeAccelerator::CopySurface");
  if (!decoder_thread_task_runner_->BelongsToCurrentThread()) {
    decoder_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&DXVAVideoDecodeAccelerator::CopySurface,
                       base::Unretained(this), base::Unretained(src_surface),
                       base::Unretained(dest_surface), picture_buffer_id,
                       input_buffer_id, color_space));
    return;
  }

  HRESULT hr;
  if (processor_) {
    D3DSURFACE_DESC src_desc;
    src_surface->GetDesc(&src_desc);
    int width = src_desc.Width;
    int height = src_desc.Height;
    RECT rect = {0, 0, width, height};
    DXVA2_VideoSample sample = {0};
    sample.End = 1000;
    if (use_color_info_) {
      sample.SampleFormat = gfx::ColorSpaceWin::GetExtendedFormat(color_space);
    } else {
      sample.SampleFormat.SampleFormat = DXVA2_SampleProgressiveFrame;
    }

    sample.SrcSurface = src_surface;
    sample.SrcRect = rect;
    sample.DstRect = rect;
    sample.PlanarAlpha = DXVA2_Fixed32OpaqueAlpha();

    DXVA2_VideoProcessBltParams params = {0};
    params.TargetFrame = 0;
    params.TargetRect = rect;
    params.ConstrictionSize = {width, height};
    params.BackgroundColor = {0, 0, 0, 0xFFFF};
    params.ProcAmpValues = default_procamp_values_;

    params.Alpha = DXVA2_Fixed32OpaqueAlpha();

    hr = processor_->VideoProcessBlt(dest_surface, &params, &sample, 1, NULL);
    if (hr != S_OK) {
      LOG(ERROR) << "VideoProcessBlt failed with code " << hr
                 << "  E_INVALIDARG= " << E_INVALIDARG;

      // Release the processor and fall back to StretchRect()
      processor_ = nullptr;
    }
  }

  if (!processor_) {
    hr = d3d9_device_ex_->StretchRect(src_surface, NULL, dest_surface, NULL,
                                      D3DTEXF_NONE);
    RETURN_ON_HR_FAILURE(hr, "Colorspace conversion via StretchRect failed", );
  }
  // Ideally, this should be done immediately before the draw call that uses
  // the texture. Flush it once here though.
  hr = query_->Issue(D3DISSUE_END);
  RETURN_ON_HR_FAILURE(hr, "Failed to issue END", );

  // If we are sharing the ANGLE device we don't need to wait for the Flush to
  // complete.
  if (using_angle_device_) {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&DXVAVideoDecodeAccelerator::CopySurfaceComplete,
                       weak_ptr_, base::Unretained(src_surface),
                       base::Unretained(dest_surface), picture_buffer_id,
                       input_buffer_id));
    return;
  }

  // Flush the decoder device to ensure that the decoded frame is copied to the
  // target surface.
  decoder_thread_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DXVAVideoDecodeAccelerator::FlushDecoder,
                     base::Unretained(this), 0, base::Unretained(src_surface),
                     base::Unretained(dest_surface), picture_buffer_id,
                     input_buffer_id),
      base::TimeDelta::FromMilliseconds(kFlushDecoderSurfaceTimeoutMs));
}

void DXVAVideoDecodeAccelerator::CopySurfaceComplete(
    IDirect3DSurface9* src_surface,
    IDirect3DSurface9* dest_surface,
    int picture_buffer_id,
    int input_buffer_id) {
  TRACE_EVENT0("media", "DXVAVideoDecodeAccelerator::CopySurfaceComplete");
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  // The output buffers may have changed in the following scenarios:-
  // 1. A resolution change.
  // 2. Decoder instance was destroyed.
  // Ignore copy surface notifications for such buffers.
  OutputBuffers::iterator it = output_picture_buffers_.find(picture_buffer_id);
  if (it == output_picture_buffers_.end())
    return;

  // If the picture buffer is marked as available it probably means that there
  // was a Reset operation which dropped the output frame.
  DXVAPictureBuffer* picture_buffer = it->second.get();
  if (picture_buffer->available())
    return;

  RETURN_AND_NOTIFY_ON_FAILURE(make_context_current_cb_.Run(),
                               "Failed to make context current",
                               PLATFORM_FAILURE, );

  DCHECK(!output_picture_buffers_.empty());

  bool result = picture_buffer->CopySurfaceComplete(src_surface, dest_surface);
  RETURN_AND_NOTIFY_ON_FAILURE(result, "Failed to complete copying surface",
                               PLATFORM_FAILURE, );

  NotifyPictureReady(
      picture_buffer->id(), input_buffer_id, picture_buffer->visible_rect(),
      picture_buffer->color_space(), picture_buffer->AllowOverlay());

  {
    base::AutoLock lock(decoder_lock_);
    if (!pending_output_samples_.empty())
      pending_output_samples_.pop_front();
  }

  if (pending_flush_ || processing_config_changed_) {
    decoder_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DXVAVideoDecodeAccelerator::FlushInternal,
                                  base::Unretained(this)));
    return;
  }
  decoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DXVAVideoDecodeAccelerator::DecodePendingInputBuffers,
                     base::Unretained(this)));
}

void DXVAVideoDecodeAccelerator::BindPictureBufferToSample(
    Microsoft::WRL::ComPtr<IMFSample> sample,
    int picture_buffer_id,
    int input_buffer_id) {
  TRACE_EVENT0("media",
               "DXVAVideoDecodeAccelerator::BindPictureBufferToSample");
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  // The output buffers may have changed in the following scenarios:-
  // 1. A resolution change.
  // 2. Decoder instance was destroyed.
  // Ignore copy surface notifications for such buffers.
  OutputBuffers::iterator it = output_picture_buffers_.find(picture_buffer_id);
  if (it == output_picture_buffers_.end())
    return;

  // If the picture buffer is marked as available it probably means that there
  // was a Reset operation which dropped the output frame.
  DXVAPictureBuffer* picture_buffer = it->second.get();
  if (picture_buffer->available())
    return;

  RETURN_AND_NOTIFY_ON_FAILURE(make_context_current_cb_.Run(),
                               "Failed to make context current",
                               PLATFORM_FAILURE, );

  DCHECK(!output_picture_buffers_.empty());

  bool result = picture_buffer->BindSampleToTexture(this, sample);
  RETURN_AND_NOTIFY_ON_FAILURE(result, "Failed to complete copying surface",
                               PLATFORM_FAILURE, );

  NotifyPictureReady(
      picture_buffer->id(), input_buffer_id, picture_buffer->visible_rect(),
      picture_buffer->color_space(), picture_buffer->AllowOverlay());

  {
    base::AutoLock lock(decoder_lock_);
    if (!pending_output_samples_.empty())
      pending_output_samples_.pop_front();
  }

  if (pending_flush_ || processing_config_changed_) {
    decoder_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DXVAVideoDecodeAccelerator::FlushInternal,
                                  base::Unretained(this)));
    return;
  }
  decoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DXVAVideoDecodeAccelerator::DecodePendingInputBuffers,
                     base::Unretained(this)));
}

bool DXVAVideoDecodeAccelerator::CopyTexture(
    ID3D11Texture2D* src_texture,
    ID3D11Texture2D* dest_texture,
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> dest_keyed_mutex,
    uint64_t keyed_mutex_value,
    int picture_buffer_id,
    int input_buffer_id,
    const gfx::ColorSpace& color_space) {
  TRACE_EVENT0("media", "DXVAVideoDecodeAccelerator::CopyTexture");
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  DCHECK(use_dx11_);

  // The media foundation H.264 decoder outputs YUV12 textures which we
  // cannot copy into ANGLE as they expect ARGB textures. In D3D land
  // the StretchRect API in the IDirect3DDevice9Ex interface did the color
  // space conversion for us. Sadly in DX11 land the API does not provide
  // a straightforward way to do this.

  D3D11_TEXTURE2D_DESC source_desc;
  src_texture->GetDesc(&source_desc);
  if (!InitializeID3D11VideoProcessor(source_desc.Width, source_desc.Height,
                                      color_space)) {
    RETURN_AND_NOTIFY_ON_FAILURE(false,
                                 "Failed to initialize D3D11 video processor.",
                                 PLATFORM_FAILURE, false);
  }

  OutputBuffers::iterator it = output_picture_buffers_.find(picture_buffer_id);
  if (it != output_picture_buffers_.end()) {
    it->second->set_color_space(dx11_converter_output_color_space_);
  }

  // The input to the video processor is the output sample.
  Microsoft::WRL::ComPtr<IMFSample> input_sample_for_conversion;
  {
    base::AutoLock lock(decoder_lock_);
    PendingSampleInfo& sample_info = pending_output_samples_.front();
    input_sample_for_conversion = sample_info.output_sample;
  }

  decoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DXVAVideoDecodeAccelerator::CopyTextureOnDecoderThread,
                     base::Unretained(this), base::Unretained(dest_texture),
                     dest_keyed_mutex, keyed_mutex_value,
                     input_sample_for_conversion, picture_buffer_id,
                     input_buffer_id));
  return true;
}

void DXVAVideoDecodeAccelerator::CopyTextureOnDecoderThread(
    ID3D11Texture2D* dest_texture,
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> dest_keyed_mutex,
    uint64_t keyed_mutex_value,
    Microsoft::WRL::ComPtr<IMFSample> input_sample,
    int picture_buffer_id,
    int input_buffer_id) {
  TRACE_EVENT0("media",
               "DXVAVideoDecodeAccelerator::CopyTextureOnDecoderThread");
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  HRESULT hr = E_FAIL;

  DCHECK(use_dx11_);
  DCHECK(!!input_sample);
  DCHECK(d3d11_processor_.Get());

  if (dest_keyed_mutex) {
    HRESULT hr =
        dest_keyed_mutex->AcquireSync(keyed_mutex_value, kAcquireSyncWaitMs);
    RETURN_AND_NOTIFY_ON_FAILURE(
        hr == S_OK, "D3D11 failed to acquire keyed mutex for texture.",
        PLATFORM_FAILURE, );
  }

  Microsoft::WRL::ComPtr<IMFMediaBuffer> output_buffer;
  hr = input_sample->GetBufferByIndex(0, &output_buffer);
  RETURN_AND_NOTIFY_ON_HR_FAILURE(hr, "Failed to get buffer from output sample",
                                  PLATFORM_FAILURE, );

  Microsoft::WRL::ComPtr<IMFDXGIBuffer> dxgi_buffer;
  hr = output_buffer.As(&dxgi_buffer);
  RETURN_AND_NOTIFY_ON_HR_FAILURE(
      hr, "Failed to get DXGIBuffer from output sample", PLATFORM_FAILURE, );
  UINT index = 0;
  hr = dxgi_buffer->GetSubresourceIndex(&index);
  RETURN_AND_NOTIFY_ON_HR_FAILURE(hr, "Failed to get resource index",
                                  PLATFORM_FAILURE, );

  ComD3D11Texture2D dx11_decoding_texture;
  hr = dxgi_buffer->GetResource(IID_PPV_ARGS(&dx11_decoding_texture));
  RETURN_AND_NOTIFY_ON_HR_FAILURE(
      hr, "Failed to get resource from output sample", PLATFORM_FAILURE, );

  D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_view_desc = {
      D3D11_VPOV_DIMENSION_TEXTURE2D};
  output_view_desc.Texture2D.MipSlice = 0;
  ComD3D11VideoProcessorOutputView output_view;
  hr = video_device_->CreateVideoProcessorOutputView(
      dest_texture, enumerator_.Get(), &output_view_desc, &output_view);
  RETURN_AND_NOTIFY_ON_HR_FAILURE(hr, "Failed to get output view",
                                  PLATFORM_FAILURE, );

  D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input_view_desc = {0};
  input_view_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
  input_view_desc.Texture2D.ArraySlice = index;
  input_view_desc.Texture2D.MipSlice = 0;
  ComD3D11VideoProcessorInputView input_view;
  hr = video_device_->CreateVideoProcessorInputView(
      dx11_decoding_texture.Get(), enumerator_.Get(), &input_view_desc,
      &input_view);
  RETURN_AND_NOTIFY_ON_HR_FAILURE(hr, "Failed to get input view",
                                  PLATFORM_FAILURE, );

  D3D11_VIDEO_PROCESSOR_STREAM streams = {0};
  streams.Enable = TRUE;
  streams.pInputSurface = input_view.Get();

  hr = video_context_->VideoProcessorBlt(d3d11_processor_.Get(),
                                         output_view.Get(), 0, 1, &streams);

  RETURN_AND_NOTIFY_ON_HR_FAILURE(hr, "VideoProcessBlit failed",
                                  PLATFORM_FAILURE, );

  if (dest_keyed_mutex) {
    HRESULT hr = dest_keyed_mutex->ReleaseSync(keyed_mutex_value + 1);
    RETURN_AND_NOTIFY_ON_FAILURE(hr == S_OK, "Failed to release keyed mutex.",
                                 PLATFORM_FAILURE, );

    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&DXVAVideoDecodeAccelerator::CopySurfaceComplete,
                       weak_ptr_, nullptr, nullptr, picture_buffer_id,
                       input_buffer_id));
  } else {
    d3d11_device_context_->Flush();
    d3d11_device_context_->End(d3d11_query_.Get());

    decoder_thread_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DXVAVideoDecodeAccelerator::FlushDecoder,
                       base::Unretained(this), 0, nullptr, nullptr,
                       picture_buffer_id, input_buffer_id),
        base::TimeDelta::FromMilliseconds(kFlushDecoderSurfaceTimeoutMs));
  }
}

void DXVAVideoDecodeAccelerator::FlushDecoder(int iterations,
                                              IDirect3DSurface9* src_surface,
                                              IDirect3DSurface9* dest_surface,
                                              int picture_buffer_id,
                                              int input_buffer_id) {
  TRACE_EVENT0("media", "DXVAVideoDecodeAccelerator::FlushDecoder");
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  // The DXVA decoder has its own device which it uses for decoding. ANGLE
  // has its own device which we don't have access to.
  // The above code attempts to copy the decoded picture into a surface
  // which is owned by ANGLE. As there are multiple devices involved in
  // this, the StretchRect call above is not synchronous.
  // We attempt to flush the batched operations to ensure that the picture is
  // copied to the surface owned by ANGLE.
  // We need to do this in a loop and call flush multiple times.
  // We have seen the GetData call for flushing the command buffer fail to
  // return success occassionally on multi core machines, leading to an
  // infinite loop.
  // Workaround is to have an upper limit of 4 on the number of iterations to
  // wait for the Flush to finish.

  HRESULT hr = E_FAIL;
  if (use_dx11_) {
    BOOL query_data = 0;
    hr = d3d11_device_context_->GetData(d3d11_query_.Get(), &query_data,
                                        sizeof(BOOL), 0);
    if (FAILED(hr))
      DCHECK(false);
  } else {
    hr = query_->GetData(NULL, 0, D3DGETDATA_FLUSH);
  }

  if ((hr == S_FALSE) && (++iterations < kMaxIterationsForD3DFlush)) {
    decoder_thread_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &DXVAVideoDecodeAccelerator::FlushDecoder, base::Unretained(this),
            iterations, base::Unretained(src_surface),
            base::Unretained(dest_surface), picture_buffer_id, input_buffer_id),
        base::TimeDelta::FromMilliseconds(kFlushDecoderSurfaceTimeoutMs));
    return;
  }

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DXVAVideoDecodeAccelerator::CopySurfaceComplete,
                     weak_ptr_, base::Unretained(src_surface),
                     base::Unretained(dest_surface), picture_buffer_id,
                     input_buffer_id));
}

bool DXVAVideoDecodeAccelerator::InitializeID3D11VideoProcessor(
    int width,
    int height,
    const gfx::ColorSpace& color_space) {
  // This code path is never used by PictureBufferMechanism::BIND paths.
  DCHECK_NE(GetPictureBufferMechanism(), PictureBufferMechanism::BIND);

  if (width < processor_width_ || height != processor_height_) {
    d3d11_processor_.Reset();
    enumerator_.Reset();
    processor_width_ = 0;
    processor_height_ = 0;

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC desc;
    desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    desc.InputFrameRate.Numerator = 60;
    desc.InputFrameRate.Denominator = 1;
    desc.InputWidth = width;
    desc.InputHeight = height;
    desc.OutputFrameRate.Numerator = 60;
    desc.OutputFrameRate.Denominator = 1;
    desc.OutputWidth = width;
    desc.OutputHeight = height;
    desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    HRESULT hr =
        video_device_->CreateVideoProcessorEnumerator(&desc, &enumerator_);
    RETURN_ON_HR_FAILURE(hr, "Failed to enumerate video processors", false);

    // TODO(Hubbe): Find correct index
    hr = video_device_->CreateVideoProcessor(enumerator_.Get(), 0,
                                             &d3d11_processor_);
    RETURN_ON_HR_FAILURE(hr, "Failed to create video processor.", false);
    processor_width_ = width;
    processor_height_ = height;

    video_context_->VideoProcessorSetStreamAutoProcessingMode(
        d3d11_processor_.Get(), 0, false);
  }

  // If we're copying textures or just not using color space information, set
  // the same color space on input and output.
  if ((!use_color_info_ && !use_fp16_) ||
      GetPictureBufferMechanism() == PictureBufferMechanism::COPY_TO_NV12 ||
      GetPictureBufferMechanism() ==
          PictureBufferMechanism::DELAYED_COPY_TO_NV12) {
    const auto d3d11_color_space =
        gfx::ColorSpaceWin::GetD3D11ColorSpace(color_space);
    video_context_->VideoProcessorSetOutputColorSpace(d3d11_processor_.Get(),
                                                      &d3d11_color_space);
    video_context_->VideoProcessorSetStreamColorSpace(d3d11_processor_.Get(), 0,
                                                      &d3d11_color_space);
    dx11_converter_output_color_space_ = color_space;
    return true;
  }

  // This path is only used for copying to RGB textures.
  DCHECK_EQ(GetPictureBufferMechanism(), PictureBufferMechanism::COPY_TO_RGB);

  // On platforms prior to Windows 10 we won't have a ID3D11VideoContext1.
  ComD3D11VideoContext1 video_context1;
  if (FAILED(video_context_.As(&video_context1))) {
    auto d3d11_color_space =
        gfx::ColorSpaceWin::GetD3D11ColorSpace(color_space);
    video_context_->VideoProcessorSetStreamColorSpace(d3d11_processor_.Get(), 0,
                                                      &d3d11_color_space);

    // Since older platforms won't have HDR, just use SRGB.
    dx11_converter_output_color_space_ = gfx::ColorSpace::CreateSRGB();
    d3d11_color_space = gfx::ColorSpaceWin::GetD3D11ColorSpace(
        dx11_converter_output_color_space_);
    video_context_->VideoProcessorSetOutputColorSpace(d3d11_processor_.Get(),
                                                      &d3d11_color_space);
    return true;
  }

  // Since the video processor doesn't support HLG, lets just do the YUV->RGB
  // conversion and let the output color space be HLG. This won't work well
  // unless color management is on, but if color management is off we don't
  // support HLG anyways.
  if (color_space == gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                                     gfx::ColorSpace::TransferID::ARIB_STD_B67,
                                     gfx::ColorSpace::MatrixID::BT709,
                                     gfx::ColorSpace::RangeID::LIMITED)) {
    video_context1->VideoProcessorSetStreamColorSpace1(
        d3d11_processor_.Get(), 0,
        DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020);
    video_context1->VideoProcessorSetOutputColorSpace1(
        d3d11_processor_.Get(), DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
    dx11_converter_output_color_space_ = color_space.GetAsFullRangeRGB();
    return true;
  }

  if (use_fp16_ && config_.target_color_space.IsHDR() && color_space.IsHDR()) {
    // Note, we only use the SCRGBLinear output color space when the input is
    // PQ, because nvidia drivers will not convert G22 to G10 for some reason.
    dx11_converter_output_color_space_ = gfx::ColorSpace::CreateSCRGBLinear();
  } else {
    dx11_converter_output_color_space_ = gfx::ColorSpace::CreateSRGB();
  }

  DVLOG(2) << "input color space: " << color_space << " DXGIColorSpace: "
           << gfx::ColorSpaceWin::GetDXGIColorSpace(color_space);
  DVLOG(2) << "output color space:" << dx11_converter_output_color_space_
           << " DXGIColorSpace: "
           << gfx::ColorSpaceWin::GetDXGIColorSpace(
                  dx11_converter_output_color_space_);

  video_context1->VideoProcessorSetStreamColorSpace1(
      d3d11_processor_.Get(), 0,
      gfx::ColorSpaceWin::GetDXGIColorSpace(color_space));
  video_context1->VideoProcessorSetOutputColorSpace1(
      d3d11_processor_.Get(), gfx::ColorSpaceWin::GetDXGIColorSpace(
                                  dx11_converter_output_color_space_));

  return true;
}

bool DXVAVideoDecodeAccelerator::GetVideoFrameDimensions(IMFSample* sample,
                                                         int* width,
                                                         int* height) {
  Microsoft::WRL::ComPtr<IMFMediaBuffer> output_buffer;
  HRESULT hr = sample->GetBufferByIndex(0, &output_buffer);
  RETURN_ON_HR_FAILURE(hr, "Failed to get buffer from output sample", false);

  if (use_dx11_) {
    Microsoft::WRL::ComPtr<IMFDXGIBuffer> dxgi_buffer;
    ComD3D11Texture2D d3d11_texture;
    hr = output_buffer.As(&dxgi_buffer);
    RETURN_ON_HR_FAILURE(hr, "Failed to get DXGIBuffer from output sample",
                         false);
    hr = dxgi_buffer->GetResource(__uuidof(ID3D11Texture2D), &d3d11_texture);
    RETURN_ON_HR_FAILURE(hr, "Failed to get D3D11Texture from output buffer",
                         false);
    D3D11_TEXTURE2D_DESC d3d11_texture_desc;
    d3d11_texture->GetDesc(&d3d11_texture_desc);
    *width = d3d11_texture_desc.Width;
    *height = d3d11_texture_desc.Height;
    output_array_size_ = d3d11_texture_desc.ArraySize;
  } else {
    Microsoft::WRL::ComPtr<IDirect3DSurface9> surface;
    hr = MFGetService(output_buffer.Get(), MR_BUFFER_SERVICE,
                      IID_PPV_ARGS(&surface));
    RETURN_ON_HR_FAILURE(hr, "Failed to get D3D surface from output sample",
                         false);
    D3DSURFACE_DESC surface_desc;
    hr = surface->GetDesc(&surface_desc);
    RETURN_ON_HR_FAILURE(hr, "Failed to get surface description", false);
    *width = surface_desc.Width;
    *height = surface_desc.Height;
  }
  return true;
}

bool DXVAVideoDecodeAccelerator::SetTransformOutputType(IMFTransform* transform,
                                                        const GUID& output_type,
                                                        int width,
                                                        int height) {
  HRESULT hr = E_FAIL;
  Microsoft::WRL::ComPtr<IMFMediaType> media_type;

  for (uint32_t i = 0;
       SUCCEEDED(transform->GetOutputAvailableType(0, i, &media_type)); ++i) {
    GUID out_subtype = {0};
    hr = media_type->GetGUID(MF_MT_SUBTYPE, &out_subtype);
    RETURN_ON_HR_FAILURE(hr, "Failed to get output major type", false);

    if (out_subtype == output_type) {
      if (width && height) {
        hr = MFSetAttributeSize(media_type.Get(), MF_MT_FRAME_SIZE, width,
                                height);
        RETURN_ON_HR_FAILURE(hr, "Failed to set media type attributes", false);
      }
      hr = transform->SetOutputType(0, media_type.Get(), 0);  // No flags
      RETURN_ON_HR_FAILURE(hr, "Failed to set output type", false);
      return true;
    }
    media_type.Reset();
  }
  return false;
}

HRESULT DXVAVideoDecodeAccelerator::CheckConfigChanged(IMFSample* sample,
                                                       bool* config_changed) {
  if (!config_change_detector_) {
    *config_changed = false;
    return S_OK;
  }

  Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
  HRESULT hr = sample->GetBufferByIndex(0, &buffer);
  RETURN_ON_HR_FAILURE(hr, "Failed to get buffer from input sample", hr);

  mf::MediaBufferScopedPointer scoped_media_buffer(buffer.Get());

  if (!config_change_detector_->DetectConfig(
          scoped_media_buffer.get(), scoped_media_buffer.current_length())) {
    RETURN_ON_HR_FAILURE(E_FAIL, "Failed to detect H.264 stream config",
                         E_FAIL);
  }
  *config_changed = config_change_detector_->config_changed();
  return S_OK;
}

void DXVAVideoDecodeAccelerator::ConfigChanged(const Config& config) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  SetState(kConfigChange);
  Invalidate();
  Initialize(config_, client_);
  decoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DXVAVideoDecodeAccelerator::DecodePendingInputBuffers,
                     base::Unretained(this)));
}

uint32_t DXVAVideoDecodeAccelerator::GetTextureTarget() const {
  switch (GetPictureBufferMechanism()) {
    case PictureBufferMechanism::BIND:
    case PictureBufferMechanism::DELAYED_COPY_TO_NV12:
    case PictureBufferMechanism::COPY_TO_NV12:
      return GL_TEXTURE_EXTERNAL_OES;
    case PictureBufferMechanism::COPY_TO_RGB:
      return GL_TEXTURE_2D;
  }
  NOTREACHED();
  return 0;
}

void DXVAVideoDecodeAccelerator::DisableSharedTextureSupport() {
  support_share_nv12_textures_ = false;
  num_picture_buffers_requested_ = kNumPictureBuffers;
}

DXVAVideoDecodeAccelerator::PictureBufferMechanism
DXVAVideoDecodeAccelerator::GetPictureBufferMechanism() const {
  if (use_fp16_)
    return PictureBufferMechanism::COPY_TO_RGB;
  if (support_share_nv12_textures_)
    return PictureBufferMechanism::BIND;
  if (support_delayed_copy_nv12_textures_ && support_copy_nv12_textures_)
    return PictureBufferMechanism::DELAYED_COPY_TO_NV12;
  if (support_copy_nv12_textures_)
    return PictureBufferMechanism::COPY_TO_NV12;
  return PictureBufferMechanism::COPY_TO_RGB;
}

bool DXVAVideoDecodeAccelerator::ShouldUseANGLEDevice() const {
  switch (GetPictureBufferMechanism()) {
    case PictureBufferMechanism::BIND:
    case PictureBufferMechanism::DELAYED_COPY_TO_NV12:
      return true;
    case PictureBufferMechanism::COPY_TO_NV12:
    case PictureBufferMechanism::COPY_TO_RGB:
      return false;
  }
  NOTREACHED();
  return false;
}
ID3D11Device* DXVAVideoDecodeAccelerator::D3D11Device() const {
  return ShouldUseANGLEDevice() ? angle_device_.Get() : d3d11_device_.Get();
}

}  // namespace media
