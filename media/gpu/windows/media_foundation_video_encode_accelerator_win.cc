// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/media_foundation_video_encode_accelerator_win.h"

#pragma warning(push)
#pragma warning(disable : 4800)  // Disable warning for added padding.

#include <codecapi.h>
#include <d3d11_1.h>
#include <mferror.h>
#include <mftransform.h>
#include <objbase.h>

#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "base/cxx17_backports.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "gpu/ipc/common/dxgi_helpers.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/base/win/mf_helpers.h"
#include "media/base/win/mf_initializer.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "third_party/libvpx/source/libvpx/vp9/ratectrl_rtc.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "vp9_video_rate_control_wrapper.h"
#if BUILDFLAG(ENABLE_LIBAOM)
#include "av1_video_rate_control_wrapper.h"
#include "third_party/libaom/source/libaom/av1/ratectrl_rtc.h"
#endif

#define NOTIFY_RETURN_ON_FAILURE(cond, log, ret) \
  do {                                           \
    if (cond) {                                  \
      MEDIA_LOG(ERROR, media_log.get()) << log;  \
      NotifyError(kPlatformFailureError);        \
      return ret;                                \
    }                                            \
  } while (0)

#define NOTIFY_RETURN_ON_HR_FAILURE(hresult, log, ret) \
  NOTIFY_RETURN_ON_FAILURE(FAILED(hresult), log, ret)

namespace media {

namespace {
const uint32_t kDefaultGOPLength = 3000;
const uint32_t kDefaultTargetBitrate = 5000000u;
const VideoEncodeAccelerator::SupportedRateControlMode kSupportedProfileModes =
    VideoEncodeAccelerator::kConstantMode |
    VideoEncodeAccelerator::kVariableMode;
const size_t kMaxFrameRateNumerator = 30;
const size_t kMaxFrameRateDenominator = 1;
const size_t kMaxResolutionWidth = 1920;
const size_t kMaxResolutionHeight = 1088;
const size_t kMinResolutionWidth = 32;
const size_t kMinResolutionHeight = 32;
const size_t kNumInputBuffers = 3;
// Media Foundation uses 100 nanosecond units for time, see
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms697282(v=vs.85).aspx.
const size_t kOneMicrosecondInMFSampleTimeUnits = 10;
const size_t kPrefixNALLocatedBytePos = 3;
constexpr uint64_t kH264MaxQp = 51;
constexpr uint64_t kVP9MaxQp = 63;
constexpr uint64_t kAV1MaxQp = 63;

// Quantizer parameter used in libvpx vp9 rate control, whose range is 0-63.
// These are based on WebRTC's defaults, cite from
// third_party/webrtc/media/engine/webrtc_video_engine.h.
constexpr uint8_t kVP9MinQuantizer = 2;
constexpr uint8_t kVP9MaxQuantizer = 56;
// Default value from
// //third_party/webrtc/modules/video_coding/codecs/av1/libaom_av1_encoder.cc,
constexpr uint8_t kAV1MinQuantizer = 10;
// //third_party/webrtc/media/engine/webrtc_video_engine.h.
constexpr uint8_t kAV1MaxQuantizer = 56;

constexpr const wchar_t* const kMediaFoundationVideoEncoderDLLs[] = {
    L"mf.dll",
    L"mfplat.dll",
};

static const CLSID kIntelAV1HybridEncoderCLSID = {
    0x62c053ce,
    0x5357,
    0x4794,
    {0x8c, 0x5a, 0xfb, 0xef, 0xfe, 0xff, 0xb8, 0x2d}};

eAVEncH264VProfile GetH264VProfile(VideoCodecProfile profile,
                                   bool is_constrained_h264) {
  switch (profile) {
    case H264PROFILE_BASELINE:
      return is_constrained_h264 ? eAVEncH264VProfile_ConstrainedBase
                                 : eAVEncH264VProfile_Base;
    case H264PROFILE_MAIN:
      return eAVEncH264VProfile_Main;
    case H264PROFILE_HIGH: {
      // eAVEncH264VProfile_High requires Windows 8.
      if (base::win::GetVersion() < base::win::Version::WIN8) {
        return eAVEncH264VProfile_unknown;
      }
      return eAVEncH264VProfile_High;
    }
    default:
      return eAVEncH264VProfile_unknown;
  }
}

// Convert AV1/VP9 qindex (0-255) to the quantizer parameter input in MF
// AVEncVideoEncodeQP. AVEncVideoEncodeQP maps it to libvpx qp tuning parameter
// and thus the range is 0-63.
uint8_t QindextoAVEncQP(uint8_t q_index) {
  // The following computation is based on the table in
  // //third_party/libvpx/source/libvpx/vp9/encoder/vp9_quantize.c.
  // //third_party/libaom/source/libaom/av1/encoder/av1_quantize.c
  // {
  //   0,   4,   8,   12,  16,  20,  24,  28,  32,  36,  40,  44,  48,
  //   52,  56,  60,  64,  68,  72,  76,  80,  84,  88,  92,  96,  100,
  //   104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 148, 152,
  //   156, 160, 164, 168, 172, 176, 180, 184, 188, 192, 196, 200, 204,
  //   208, 212, 216, 220, 224, 228, 232, 236, 240, 244, 249, 255,
  // };
  if (q_index <= 244)
    return (q_index + 3) / 4;
  if (q_index <= 249)
    return 62;
  return 63;
}

bool IsValidQp(VideoCodec codec, uint64_t qp) {
  switch (codec) {
    case VideoCodec::kH264:
      return qp <= kH264MaxQp;
    case VideoCodec::kVP9:
      return qp <= kVP9MaxQp;
    case VideoCodec::kAV1:
      return qp <= kAV1MaxQp;
    default:
      return false;
  }
}
// Only eAVEncVP9VProfile_420_8 is supported on Intel graphics.
eAVEncVP9VProfile GetVP9VProfile(VideoCodecProfile profile) {
  switch (profile) {
    case VP9PROFILE_PROFILE0:
      return eAVEncVP9VProfile_420_8;
    default:
      return eAVEncVP9VProfile_unknown;
  }
}

// Only eAVEncH265Vprofile_Main_420_8 is supported.
eAVEncH265VProfile GetHEVCProfile(VideoCodecProfile profile) {
  switch (profile) {
    case HEVCPROFILE_MAIN:
      return eAVEncH265VProfile_Main_420_8;
    default:
      return eAVEncH265VProfile_unknown;
  }
}

bool IsSvcSupported(IMFActivate* activate) {
#if defined(ARCH_CPU_X86)
  // x86 systems sometimes crash in video drivers here.
  // More info: https://crbug.com/1253748
  return false;
#else
  // crbug.com/1350257
  TRACE_EVENT0("catan_investigation", "IsSvcSupported");
  Microsoft::WRL::ComPtr<IMFTransform> encoder;
  Microsoft::WRL::ComPtr<ICodecAPI> codec_api;
  HRESULT hr = activate->ActivateObject(IID_PPV_ARGS(&encoder));
  if (FAILED(hr))
    return false;

  bool result = false;
  hr = encoder.As(&codec_api);
  if (SUCCEEDED(hr)) {
    result = (codec_api->IsSupported(&CODECAPI_AVEncVideoTemporalLayerCount) ==
              S_OK);
    if (result) {
      VARIANT min, max, step;
      VariantInit(&min);
      VariantInit(&max);
      VariantInit(&step);

      hr = codec_api->GetParameterRange(&CODECAPI_AVEncVideoTemporalLayerCount,
                                        &min, &max, &step);
      if (hr != S_OK || min.ulVal > 1 || max.ulVal < 3)
        result = false;

      VariantClear(&min);
      VariantClear(&max);
      VariantClear(&step);
    }
  }

  activate->ShutdownObject();
  return result;
#endif  // defined(ARCH_CPU_X86)
}

GUID VideoCodecToMFSubtype(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kH264:
      return MFVideoFormat_H264;
    case VideoCodec::kVP8:
      return MFVideoFormat_VP80;
    case VideoCodec::kVP9:
      return MFVideoFormat_VP90;
    case VideoCodec::kHEVC:
      return MFVideoFormat_HEVC;
    case VideoCodec::kAV1:
      return MFVideoFormat_AV1;
    default:
      return GUID_NULL;
  }
}

MediaFoundationVideoEncodeAccelerator::DriverVendor GetDriverVendor(
    IMFActivate* encoder) {
  using DriverVendor = MediaFoundationVideoEncodeAccelerator::DriverVendor;
  base::win::ScopedCoMem<WCHAR> vendor_id;
  UINT32 id_length;
  encoder->GetAllocatedString(MFT_ENUM_HARDWARE_VENDOR_ID_Attribute, &vendor_id,
                              &id_length);
  if (id_length != 8)  // Normal vendor ids have length 8.
    return DriverVendor::kOther;
  if (!_wcsnicmp(vendor_id.get(), L"VEN_10DE", id_length))
    return DriverVendor::kNvidia;
  if (!_wcsnicmp(vendor_id.get(), L"VEN_1002", id_length))
    return DriverVendor::kAMD;
  if (!_wcsnicmp(vendor_id.get(), L"VEN_8086 ", id_length))
    return DriverVendor::kIntel;
  return DriverVendor::kOther;
}

uint32_t EnumerateHardwareEncoders(VideoCodec codec,
                                   IMFActivate*** pp_activate,
                                   bool compatible_with_win7) {
  DVLOG(3) << __func__;

  if (!compatible_with_win7 &&
      base::win::GetVersion() < base::win::Version::WIN8) {
    return 0;
  }

  if (codec != VideoCodec::kH264 && codec != VideoCodec::kVP9 &&
      codec != VideoCodec::kAV1
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
      && codec != VideoCodec::kHEVC
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
  ) {
    DLOG(ERROR) << "Enumerating unsupported hardware encoders.";
    return 0;
  }

  for (const wchar_t* mfdll : kMediaFoundationVideoEncoderDLLs) {
    if (!::GetModuleHandle(mfdll)) {
      DLOG(ERROR) << mfdll << " is required for encoding";
      return 0;
    }
  }

  if (!InitializeMediaFoundation())
    return 0;

  uint32_t flags = MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER;
  MFT_REGISTER_TYPE_INFO input_info;
  input_info.guidMajorType = MFMediaType_Video;
  input_info.guidSubtype = MFVideoFormat_NV12;
  MFT_REGISTER_TYPE_INFO output_info;
  output_info.guidMajorType = MFMediaType_Video;
  output_info.guidSubtype = VideoCodecToMFSubtype(codec);

  uint32_t count = 0;
  HRESULT hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, flags, &input_info,
                         &output_info, pp_activate, &count);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to enumerate hardware encoders for "
                << GetCodecName(codec)
                << ", hr=" << logging::SystemErrorCodeToString(hr);
  }

  return count;
}

// Per
// https://learn.microsoft.com/en-us/windows/win32/medfound/handling-stream-changes,
// encoders should only accept an input type that matches the currently
// configured output type. If we want to change the frame rate, a
// stream restart flow is needed, which in turn generates a key-frame on the
// stream restart. This is not friendly for WebRTC encoding, which adjusts the
// encoding frame rate frequently.
// To mitigate this, we only configure the frame rate during HMFT
// initialization. On subsequent frame rate update request, if new frame rate is
// larger than currently configured frame rate and bitrate is kept unchanged,
// this implies average encoded frame size should decrease proportionally. Since
// we don't actually configure the new frame rate into HMFT(to avoid stream
// restart), we emulate this average frame size decrease by proportionally
// decreasing the target/peak bitrate(which does not require stream restart).
// This is similar for frame rate update request that is lower than currently
// configured, by increasing bitrate to emulate average frame size increase.
// See https://crbug.com/1295815 for more details.
uint32_t AdjustBitrateToFrameRate(uint32_t bitrate,
                                  uint32_t configured_framerate,
                                  uint32_t requested_framerate) {
  if (requested_framerate == 0u) {
    return 0u;
  }

  return bitrate * configured_framerate / requested_framerate;
}

VideoRateControlWrapper::RateControlConfig CreateRateControllerConfig(
    const VideoBitrateAllocation& bitrate_allocation,
    gfx::Size size,
    uint32_t frame_rate,
    int num_temporal_layers,
    VideoCodec codec) {
  // Fill rate control config variables.
  VideoRateControlWrapper::RateControlConfig config;
  config.width = size.width();
  config.height = size.height();
  config.target_bandwidth = bitrate_allocation.GetSumBps() / 1000;
  config.framerate = frame_rate;
  config.ss_number_layers = 1;
  config.ts_number_layers = num_temporal_layers;
  switch (codec) {
    case VideoCodec::kVP9: {
      config.max_quantizer = kVP9MaxQuantizer;
      config.min_quantizer = kVP9MinQuantizer;
      break;
    }
    case VideoCodec::kAV1: {
      config.max_quantizer = kAV1MaxQuantizer;
      config.min_quantizer = kAV1MinQuantizer;
      break;
    }
    default:
      NOTREACHED();
      break;
  }
  int bitrate_sum = 0;
  for (int tid = 0; tid < num_temporal_layers; ++tid) {
    bitrate_sum += bitrate_allocation.GetBitrateBps(0, tid);
    config.layer_target_bitrate[tid] = bitrate_sum / 1000;
    config.ts_rate_decimator[tid] = 1u << (num_temporal_layers - tid - 1);
    config.min_quantizers[tid] = config.min_quantizer;
    config.max_quantizers[tid] = config.max_quantizer;
  }
  return config;
}

}  // namespace

class MediaFoundationVideoEncodeAccelerator::EncodeOutput {
 public:
  EncodeOutput(uint32_t size,
               bool key_frame,
               base::TimeDelta timestamp,
               int temporal_id = 0)
      : keyframe(key_frame),
        capture_timestamp(timestamp),
        temporal_layer_id(temporal_id),
        data_(size) {}

  EncodeOutput(const EncodeOutput&) = delete;
  EncodeOutput& operator=(const EncodeOutput&) = delete;

  uint8_t* memory() { return data_.data(); }

  int size() const { return static_cast<int>(data_.size()); }
  void SetQp(int32_t qp_val) { frame_qp.emplace(qp_val); }

  const bool keyframe;
  const base::TimeDelta capture_timestamp;
  const int temporal_layer_id;
  absl::optional<int32_t> frame_qp;

 private:
  std::vector<uint8_t> data_;
};

struct MediaFoundationVideoEncodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef() = delete;

  BitstreamBufferRef(int32_t id,
                     base::WritableSharedMemoryMapping mapping,
                     size_t size)
      : id(id), mapping(std::move(mapping)), size(size) {}

  BitstreamBufferRef(const BitstreamBufferRef&) = delete;
  BitstreamBufferRef& operator=(const BitstreamBufferRef&) = delete;

  const int32_t id;
  const base::WritableSharedMemoryMapping mapping;
  const size_t size;
};

// TODO(zijiehe): Respect |compatible_with_win7_| in the implementation. Some
// attributes are not supported by Windows 7, setting them will return errors.
// See bug: http://crbug.com/777659.
MediaFoundationVideoEncodeAccelerator::MediaFoundationVideoEncodeAccelerator(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    CHROME_LUID luid)
    : compatible_with_win7_(
          gpu_preferences.enable_media_foundation_vea_on_windows7),
      frame_rate_(kMaxFrameRateNumerator / kMaxFrameRateDenominator),
      bitrate_allocation_(Bitrate::Mode::kConstant),
      input_required_(false),
      main_client_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      encoder_thread_task_runner_(base::ThreadPool::CreateCOMSTATaskRunner(
          {},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED)),
      luid_(luid) {
  DETACH_FROM_SEQUENCE(encode_sequence_checker_);
  encoder_weak_ptr_ = encoder_task_weak_factory_.GetWeakPtr();
  bitrate_allocation_.SetBitrate(0, 0, kDefaultTargetBitrate);
}

MediaFoundationVideoEncodeAccelerator::
    ~MediaFoundationVideoEncodeAccelerator() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!encoder_task_weak_factory_.HasWeakPtrs());
}

VideoEncodeAccelerator::SupportedProfiles
MediaFoundationVideoEncodeAccelerator::GetSupportedProfiles() {
  TRACE_EVENT0("gpu,startup",
               "MediaFoundationVideoEncodeAccelerator::GetSupportedProfiles");
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SupportedProfiles profiles;

  for (auto codec : {VideoCodec::kH264, VideoCodec::kVP9, VideoCodec::kAV1,
                     VideoCodec::kHEVC}) {
    auto codec_profiles = GetSupportedProfilesForCodec(codec);
    profiles.insert(profiles.end(), codec_profiles.begin(),
                    codec_profiles.end());
  }

  ReleaseEncoderResources();
  return profiles;
}

VideoEncodeAccelerator::SupportedProfiles
MediaFoundationVideoEncodeAccelerator::GetSupportedProfilesForCodec(
    VideoCodec codec) {
  SupportedProfiles profiles;

  if (codec == VideoCodec::kHEVC) {
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    if (!base::FeatureList::IsEnabled(kPlatformHEVCEncoderSupport)) {
      return profiles;
    }
#else
    return profiles;
#endif  // BULIDFLAG(ENABLE_PLATFORM_HEVC)
  }

  IMFActivate** pp_activate = nullptr;
  uint32_t encoder_count =
      EnumerateHardwareEncoders(codec, &pp_activate, compatible_with_win7_);
  if (!encoder_count) {
    DVLOG(1)
        << "Hardware encode acceleration is not available on this platform for "
        << GetCodecName(codec);
    return profiles;
  }

  bool svc_supported = false;
  if (pp_activate) {
    for (UINT32 i = 0; i < encoder_count; i++) {
      if (pp_activate[i]) {
        // crbug.com/1373780: Nvidia HEVC encoder reports supporting 3 temporal
        // layers, but will fail initialization if configured to encoded with
        // more than one temporal layers, thus we block Nvidia HEVC encoder for
        // temporal SVC encoding.
        bool flawy_svc =
            (codec == VideoCodec::kHEVC) &&
            (GetDriverVendor(pp_activate[i]) == DriverVendor::kNvidia);
        if (!svc_supported && !flawy_svc && IsSvcSupported(pp_activate[i]))
          svc_supported = true;

        // Release the enumerated instances if any.
        // According to Windows Dev Center,
        // https://docs.microsoft.com/en-us/windows/win32/api/mfapi/nf-mfapi-mftenumex
        // The caller must release the pointers.
        pp_activate[i]->Release();
        pp_activate[i] = nullptr;
      }
    }
    CoTaskMemFree(pp_activate);
  }

  SupportedProfile profile;
  // More profiles can be supported here, but they should be available in SW
  // fallback as well.
  profile.max_framerate_numerator = kMaxFrameRateNumerator;
  profile.max_framerate_denominator = kMaxFrameRateDenominator;
  profile.rate_control_modes = kSupportedProfileModes;
  profile.max_resolution = gfx::Size(kMaxResolutionWidth, kMaxResolutionHeight);
  profile.min_resolution = gfx::Size(kMinResolutionWidth, kMinResolutionHeight);
  if (svc_supported) {
    profile.scalability_modes.push_back(SVCScalabilityMode::kL1T2);
    profile.scalability_modes.push_back(SVCScalabilityMode::kL1T3);
  }
  if (codec == VideoCodec::kH264) {
    profile.profile = H264PROFILE_BASELINE;
    profiles.push_back(profile);

    profile.profile = H264PROFILE_MAIN;
    profiles.push_back(profile);

    profile.profile = H264PROFILE_HIGH;
    profiles.push_back(profile);
  } else if (codec == VideoCodec::kVP9) {
    profile.profile = VP9PROFILE_PROFILE0;
    profiles.push_back(profile);
  } else if (codec == VideoCodec::kAV1) {
    profile.profile = AV1PROFILE_PROFILE_MAIN;
    profiles.push_back(profile);
  } else if (codec == VideoCodec::kHEVC) {
    profile.profile = HEVCPROFILE_MAIN;
    profiles.push_back(profile);
  }
  return profiles;
}

bool MediaFoundationVideoEncodeAccelerator::Initialize(
    const Config& config,
    Client* client,
    std::unique_ptr<MediaLog> media_log) {
  DVLOG(3) << __func__ << ": " << config.AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (PIXEL_FORMAT_I420 != config.input_format &&
      PIXEL_FORMAT_NV12 != config.input_format) {
    MEDIA_LOG(ERROR, media_log.get())
        << "Input format not supported= "
        << VideoPixelFormatToString(config.input_format);
    return false;
  }

  if (config.output_profile >= H264PROFILE_MIN &&
      config.output_profile <= H264PROFILE_MAX) {
    if (GetH264VProfile(config.output_profile, config.is_constrained_h264) ==
        eAVEncH264VProfile_unknown) {
      MEDIA_LOG(ERROR, media_log.get())
          << "Output profile not supported = " << config.output_profile;
      return false;
    }
    codec_ = VideoCodec::kH264;
  } else if (config.output_profile >= VP9PROFILE_MIN &&
             config.output_profile <= VP9PROFILE_MAX) {
    if (GetVP9VProfile(config.output_profile) == eAVEncVP9VProfile_unknown) {
      MEDIA_LOG(ERROR, media_log.get())
          << "Output profile not supported = " << config.output_profile;
      return false;
    }
    codec_ = VideoCodec::kVP9;
  } else if (config.output_profile == AV1PROFILE_PROFILE_MAIN) {
    codec_ = VideoCodec::kAV1;
  } else if (config.output_profile == HEVCPROFILE_MAIN) {
    codec_ = VideoCodec::kHEVC;
  }

  if (codec_ == VideoCodec::kUnknown) {
    MEDIA_LOG(ERROR, media_log.get())
        << "Output profile not supported = " << config.output_profile;
    return false;
  }

  if (config.HasSpatialLayer()) {
    MEDIA_LOG(ERROR, media_log.get()) << "MediaFoundation does not support "
                                         "spatial layer encoding.";
    return false;
  }
  main_client_weak_factory_ =
      std::make_unique<base::WeakPtrFactory<Client>>(client);
  main_client_ = main_client_weak_factory_->GetWeakPtr();
  input_visible_size_ = config.input_visible_size;
  if (config.initial_framerate.has_value() && config.initial_framerate.value())
    frame_rate_ = config.initial_framerate.value();
  else
    frame_rate_ = kMaxFrameRateNumerator / kMaxFrameRateDenominator;
  bitrate_allocation_ = AllocateBitrateForDefaultEncoding(config);

  bitstream_buffer_size_ = config.input_visible_size.GetArea();
  gop_length_ = config.gop_length.value_or(kDefaultGOPLength);
  low_latency_mode_ = config.require_low_delay;

  if (config.HasTemporalLayer())
    num_temporal_layers_ = config.spatial_layers.front().num_of_temporal_layers;

  // Use SW BRC only in the case CBR and non layer encoding.
  const bool use_sw_brc =
      bitrate_allocation_.GetMode() == Bitrate::Mode::kConstant &&
      base::FeatureList::IsEnabled(kMediaFoundationUseSoftwareRateCtrl) &&
      !config.HasTemporalLayer();

  if (use_sw_brc && (codec_ == VideoCodec::kVP9
#if BUILDFLAG(ENABLE_LIBAOM)
                     || codec_ == VideoCodec::kAV1
#endif
                     )) {
    VideoRateControlWrapper::RateControlConfig rate_config =
        CreateRateControllerConfig(bitrate_allocation_, input_visible_size_,
                                   frame_rate_, /*num_temporal_layers=*/1,
                                   codec_);
    if (codec_ == VideoCodec::kVP9) {
      rate_ctrl_ = VP9RateControl::Create(rate_config);
    } else if (codec_ == VideoCodec::kAV1) {
#if BUILDFLAG(ENABLE_LIBAOM)
      // If libaom is not enabled, |rate_ctrl_| will not be initialized.
      rate_ctrl_ = AV1RateControl::Create(rate_config);
#endif
    }
  }

  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MediaFoundationVideoEncodeAccelerator::EncoderInitializeTask,
          encoder_weak_ptr_, config, std::move(media_log)));

  return true;
}

void MediaFoundationVideoEncodeAccelerator::EncoderInitializeTask(
    const Config& config,
    std::unique_ptr<MediaLog> media_log) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(encode_sequence_checker_);

  HRESULT hr = S_OK;
  IMFActivate** pp_activates = nullptr;

  uint32_t encoder_count =
      EnumerateHardwareEncoders(codec_, &pp_activates, compatible_with_win7_);
  NOTIFY_RETURN_ON_FAILURE(encoder_count == 0,
                           "Failed finding a hardware encoder MFT.", );

  bool activated = ActivateAsyncEncoder(pp_activates, encoder_count,
                                        config.is_constrained_h264);
  if (pp_activates) {
    // Release the enumerated instances if any.
    // According to Windows Dev Center,
    // https://docs.microsoft.com/en-us/windows/win32/api/mfapi/nf-mfapi-mftenumex
    // The caller must release the pointers.
    for (UINT32 i = 0; i < encoder_count; i++) {
      if (pp_activates[i]) {
        pp_activates[i]->Release();
        pp_activates[i] = nullptr;
      }
    }
    CoTaskMemFree(pp_activates);
  }

  NOTIFY_RETURN_ON_FAILURE(
      !activated, "Failed activating an async hardware encoder MFT.", );
  NOTIFY_RETURN_ON_FAILURE(!SetEncoderModes(),
                           "Failed to set encoder modes.", );
  NOTIFY_RETURN_ON_FAILURE(
      !InitializeInputOutputParameters(config.output_profile,
                                       config.is_constrained_h264),
      "Failed to set input/output param.", );

  hr = MFCreateSample(&input_sample_);
  NOTIFY_RETURN_ON_HR_FAILURE(hr, "Failed to create sample", );

  if (media::IsMediaFoundationD3D11VideoCaptureEnabled()) {
    MEDIA_LOG(INFO, media_log.get())
        << "Preferred DXGI device " << luid_.HighPart << ":" << luid_.LowPart;
    dxgi_device_manager_ = DXGIDeviceManager::Create(luid_);
    NOTIFY_RETURN_ON_FAILURE(!dxgi_device_manager_,
                             "Failed to create DXGIDeviceManager", );
    auto mf_dxgi_device_manager =
        dxgi_device_manager_->GetMFDXGIDeviceManager();
    hr = encoder_->ProcessMessage(
        MFT_MESSAGE_SET_D3D_MANAGER,
        reinterpret_cast<ULONG_PTR>(mf_dxgi_device_manager.Get()));
    // If HMFT rejects setting D3D manager, fallback to non-D3D11 encoding.
    if (FAILED(hr)) {
      dxgi_resource_mapping_required_ = true;
      MEDIA_LOG(INFO, media_log.get())
          << "Couldn't set DXGIDeviceManager, fallback to non-D3D11 encoding";
    }
  }

  // Start the asynchronous processing model
  hr = encoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
  NOTIFY_RETURN_ON_HR_FAILURE(
      hr, "Couldn't set ProcessMessage MFT_MESSAGE_COMMAND_FLUSH", );
  hr = encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
  NOTIFY_RETURN_ON_HR_FAILURE(
      hr, "Couldn't set ProcessMessage MFT_MESSAGE_NOTIFY_BEGIN_STREAMING", );
  hr = encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
  NOTIFY_RETURN_ON_HR_FAILURE(
      hr, "Couldn't set ProcessMessage MFT_MESSAGE_NOTIFY_START_OF_STREAM", );
  hr = encoder_->QueryInterface(IID_PPV_ARGS(&event_generator_));
  NOTIFY_RETURN_ON_HR_FAILURE(hr, "Couldn't get event generator", );

  main_client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::RequireBitstreamBuffers, main_client_,
                                kNumInputBuffers, input_visible_size_,
                                bitstream_buffer_size_));

  encoder_info_.implementation_name = "MediaFoundationVideoEncodeAccelerator";
  // Currently, MFVEA does not support odd resolution well. The implementation
  // here reports alignment of 2 in the EncoderInfo, together with simulcast
  // layers applied.
  // See https://crbug.com/1275453 for more details.
  encoder_info_.requested_resolution_alignment = 2;
  encoder_info_.apply_alignment_to_all_simulcast_layers = true;
  encoder_info_.has_trusted_rate_controller = false;
  DCHECK(encoder_info_.is_hardware_accelerated);
  DCHECK(encoder_info_.supports_native_handle);
  DCHECK(encoder_info_.reports_average_qp);
  DCHECK(!encoder_info_.supports_simulcast);
  if (config.HasSpatialLayer() || config.HasTemporalLayer()) {
    DCHECK(!config.spatial_layers.empty());
    for (size_t i = 0; i < config.spatial_layers.size(); ++i) {
      encoder_info_.fps_allocation[i] =
          GetFpsAllocation(config.spatial_layers[i].num_of_temporal_layers);
    }
  } else {
    constexpr uint8_t kFullFramerate = 255;
    encoder_info_.fps_allocation[0] = {kFullFramerate};
  }
}

void MediaFoundationVideoEncodeAccelerator::Encode(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaFoundationVideoEncodeAccelerator::EncodeTask,
                     encoder_weak_ptr_, std::move(frame), force_keyframe));
}

void MediaFoundationVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    BitstreamBuffer buffer) {
  DVLOG(3) << __func__ << ": buffer size=" << buffer.size();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (buffer.size() < bitstream_buffer_size_) {
    DLOG(ERROR) << "Output BitstreamBuffer isn't big enough: " << buffer.size()
                << " vs. " << bitstream_buffer_size_;
    NotifyError(kInvalidArgumentError);
    return;
  }

  auto region = buffer.TakeRegion();
  auto mapping = region.Map();
  if (!mapping.IsValid()) {
    DLOG(ERROR) << "Failed mapping shared memory.";
    NotifyError(kPlatformFailureError);
    return;
  }
  // After mapping, |region| is no longer necessary and it can be
  // destroyed. |mapping| will keep the shared memory region open.

  std::unique_ptr<BitstreamBufferRef> buffer_ref(
      new BitstreamBufferRef(buffer.id(), std::move(mapping), buffer.size()));
  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MediaFoundationVideoEncodeAccelerator::UseOutputBitstreamBufferTask,
          encoder_weak_ptr_, std::move(buffer_ref)));
}

void MediaFoundationVideoEncodeAccelerator::RequestEncodingParametersChange(
    const Bitrate& bitrate,
    uint32_t framerate) {
  DVLOG(3) << __func__ << ": bitrate=" << bitrate.ToString()
           << ": framerate=" << framerate;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VideoBitrateAllocation allocation(bitrate.mode());
  allocation.SetBitrate(0, 0, bitrate.target_bps());
  if (bitrate.mode() == Bitrate::Mode::kVariable)
    allocation.SetPeakBps(bitrate.peak_bps());

  encoder_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MediaFoundationVideoEncodeAccelerator::
                                    RequestEncodingParametersChangeTask,
                                encoder_weak_ptr_, allocation, framerate));
}

void MediaFoundationVideoEncodeAccelerator::RequestEncodingParametersChange(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate) {
  DVLOG(3) << __func__ << ": bitrate=" << bitrate_allocation.GetSumBps()
           << ": framerate=" << framerate;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaFoundationVideoEncodeAccelerator::
                         RequestEncodingParametersChangeTask,
                     encoder_weak_ptr_, bitrate_allocation, framerate));
}

void MediaFoundationVideoEncodeAccelerator::Destroy() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Cancel all callbacks.
  main_client_weak_factory_.reset();

  // MF resources need to be cleaned up on |encoder_thread_task_runner_|,
  // but the object itself is supposed to be deleted on this runner, so when
  // DestroyTask() is done we schedule deletion of |this|
  auto delete_self = [](MediaFoundationVideoEncodeAccelerator* self) {
    delete self;
  };
  encoder_thread_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&MediaFoundationVideoEncodeAccelerator::DestroyTask,
                     encoder_weak_ptr_),
      base::BindOnce(delete_self, base::Unretained(this)));
}

bool MediaFoundationVideoEncodeAccelerator::IsGpuFrameResizeSupported() {
  return true;
}

// static
bool MediaFoundationVideoEncodeAccelerator::PreSandboxInitialization() {
  bool result = true;
  for (const wchar_t* mfdll : kMediaFoundationVideoEncoderDLLs) {
    if (::LoadLibrary(mfdll) == nullptr) {
      result = false;
    }
  }
  return result;
}

bool MediaFoundationVideoEncodeAccelerator::ActivateAsyncEncoder(
    IMFActivate** pp_activate,
    uint32_t encoder_count,
    bool is_constrained_h264) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(encode_sequence_checker_);

  // Try to create the encoder with priority according to merit value.
  HRESULT hr = E_FAIL;
  for (UINT32 i = 0; i < encoder_count; i++) {
    auto vendor = GetDriverVendor(pp_activate[i]);
    // Skip flawky Intel hybrid AV1 encoder.
    if (codec_ == VideoCodec::kAV1 && vendor == DriverVendor::kIntel) {
      // Get the CLSID GUID of the HMFT.
      GUID mft_guid = {0};
      pp_activate[i]->GetGUID(MFT_TRANSFORM_CLSID_Attribute, &mft_guid);
      if (mft_guid == kIntelAV1HybridEncoderCLSID) {
        DLOG(WARNING) << "Skipped Intel hybrid AV1 encoder MFT.";
        continue;
      }
    }

    // Skip NVIDIA GPU due to https://crbug.com/1088650 for constrained
    // baseline profile H.264 encoding, and go to the next instance according
    // to merit value.
    if (codec_ == VideoCodec::kH264 && is_constrained_h264 &&
        vendor == DriverVendor::kNvidia) {
      DLOG(WARNING) << "Skipped NVIDIA GPU due to https://crbug.com/1088650";
      continue;
    }

    DCHECK(!encoder_);
    DCHECK(!activate_);
    hr = pp_activate[i]->ActivateObject(IID_PPV_ARGS(&encoder_));
    if (encoder_.Get() != nullptr) {
      DCHECK(SUCCEEDED(hr));
      activate_ = pp_activate[i];
      vendor_ = vendor;
      pp_activate[i] = nullptr;

      // Print the friendly name.
      base::win::ScopedCoMem<WCHAR> friendly_name;
      UINT32 name_length;
      activate_->GetAllocatedString(MFT_FRIENDLY_NAME_Attribute, &friendly_name,
                                    &name_length);
      DVLOG(3) << "Selected asynchronous hardware encoder's friendly name: "
               << friendly_name;
      // Encoder is successfully activated.
      break;
    } else {
      DCHECK(FAILED(hr));

      // The component that calls ActivateObject is
      // responsible for calling ShutdownObject,
      // https://docs.microsoft.com/en-us/windows/win32/api/mfobjects/nf-mfobjects-imfactivate-shutdownobject.
      pp_activate[i]->ShutdownObject();
    }
  }

  RETURN_ON_HR_FAILURE(hr, "Couldn't activate asynchronous hardware encoder",
                       false);
  RETURN_ON_FAILURE((encoder_.Get() != nullptr),
                    "No asynchronous hardware encoder instance created", false);

  Microsoft::WRL::ComPtr<IMFAttributes> all_attributes;
  hr = encoder_->GetAttributes(&all_attributes);
  if (SUCCEEDED(hr)) {
    // An asynchronous MFT must support dynamic format changes,
    // https://docs.microsoft.com/en-us/windows/win32/medfound/asynchronous-mfts#format-changes.
    UINT32 dynamic = FALSE;
    hr = all_attributes->GetUINT32(MFT_SUPPORT_DYNAMIC_FORMAT_CHANGE, &dynamic);
    if (!dynamic) {
      DLOG(ERROR) << "Couldn't support dynamic format change.";
      return false;
    }

    // Unlock the selected asynchronous MFTs,
    // https://docs.microsoft.com/en-us/windows/win32/medfound/asynchronous-mfts#unlocking-asynchronous-mfts.
    UINT32 async = FALSE;
    hr = all_attributes->GetUINT32(MF_TRANSFORM_ASYNC, &async);
    if (!async) {
      DLOG(ERROR) << "MFT encoder is not asynchronous.";
      return false;
    }

    hr = all_attributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);
    RETURN_ON_HR_FAILURE(hr, "Couldn't unlock transform async", false);
  }

  return true;
}

bool MediaFoundationVideoEncodeAccelerator::InitializeInputOutputParameters(
    VideoCodecProfile output_profile,
    bool is_constrained_h264) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encode_sequence_checker_);
  DCHECK(encoder_);

  DWORD input_count = 0;
  DWORD output_count = 0;
  HRESULT hr = encoder_->GetStreamCount(&input_count, &output_count);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get stream count", false);
  if (input_count < 1 || output_count < 1) {
    DLOG(ERROR) << "Stream count too few: input " << input_count << ", output "
                << output_count;
    return false;
  }

  std::vector<DWORD> input_ids(input_count, 0);
  std::vector<DWORD> output_ids(output_count, 0);
  hr = encoder_->GetStreamIDs(input_count, input_ids.data(), output_count,
                              output_ids.data());
  if (hr == S_OK) {
    input_stream_id_ = input_ids[0];
    output_stream_id_ = output_ids[0];
  } else if (hr == E_NOTIMPL) {
    input_stream_id_ = 0;
    output_stream_id_ = 0;
  } else {
    DLOG(ERROR) << "Couldn't find stream ids from hardware encoder.";
    return false;
  }

  // Initialize output parameters.
  hr = MFCreateMediaType(&imf_output_media_type_);
  RETURN_ON_HR_FAILURE(hr, "Couldn't create output media type", false);
  hr = imf_output_media_type_->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set media type", false);
  hr = imf_output_media_type_->SetGUID(MF_MT_SUBTYPE,
                                       VideoCodecToMFSubtype(codec_));
  RETURN_ON_HR_FAILURE(hr, "Couldn't set video format", false);

  if (!rate_ctrl_) {
    hr = imf_output_media_type_->SetUINT32(
        MF_MT_AVG_BITRATE,
        AdjustBitrateToFrameRate(bitrate_allocation_.GetSumBps(), frame_rate_,
                                 frame_rate_));
    RETURN_ON_HR_FAILURE(hr, "Couldn't set bitrate", false);
  }
  configured_frame_rate_ = frame_rate_;

  hr = MFSetAttributeRatio(imf_output_media_type_.Get(), MF_MT_FRAME_RATE,
                           configured_frame_rate_, 1);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame rate", false);
  hr = MFSetAttributeSize(imf_output_media_type_.Get(), MF_MT_FRAME_SIZE,
                          input_visible_size_.width(),
                          input_visible_size_.height());
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame size", false);
  hr = imf_output_media_type_->SetUINT32(MF_MT_INTERLACE_MODE,
                                         MFVideoInterlace_Progressive);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set interlace mode", false);
  if (codec_ == VideoCodec::kH264) {
    hr = imf_output_media_type_->SetUINT32(
        MF_MT_MPEG2_PROFILE,
        GetH264VProfile(output_profile, is_constrained_h264));
  } else if (codec_ == VideoCodec::kVP9) {
    hr = imf_output_media_type_->SetUINT32(MF_MT_MPEG2_PROFILE,
                                           GetVP9VProfile(output_profile));
  } else if (codec_ == VideoCodec::kHEVC) {
    hr = imf_output_media_type_->SetUINT32(MF_MT_MPEG2_PROFILE,
                                           GetHEVCProfile(output_profile));
  }
  RETURN_ON_HR_FAILURE(hr, "Couldn't set codec profile", false);
  hr = encoder_->SetOutputType(output_stream_id_, imf_output_media_type_.Get(),
                               0);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set output media type", false);

  // Initialize input parameters.
  hr = MFCreateMediaType(&imf_input_media_type_);
  RETURN_ON_HR_FAILURE(hr, "Couldn't create input media type", false);
  hr = imf_input_media_type_->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set media type", false);
  hr = imf_input_media_type_->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set video format", false);
  hr = MFSetAttributeRatio(imf_input_media_type_.Get(), MF_MT_FRAME_RATE,
                           configured_frame_rate_, 1);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame rate", false);
  hr = MFSetAttributeSize(imf_input_media_type_.Get(), MF_MT_FRAME_SIZE,
                          input_visible_size_.width(),
                          input_visible_size_.height());
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame size", false);
  hr = imf_input_media_type_->SetUINT32(MF_MT_INTERLACE_MODE,
                                        MFVideoInterlace_Progressive);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set interlace mode", false);
  hr = encoder_->SetInputType(input_stream_id_, imf_input_media_type_.Get(), 0);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set input media type", false);

  return true;
}

bool MediaFoundationVideoEncodeAccelerator::SetEncoderModes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encode_sequence_checker_);
  DCHECK(encoder_);

  HRESULT hr = encoder_.As(&codec_api_);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get ICodecAPI", false);

  VARIANT var;
  var.vt = VT_UI4;
  switch (bitrate_allocation_.GetMode()) {
    case Bitrate::Mode::kConstant:
      if (rate_ctrl_)
        var.ulVal = eAVEncCommonRateControlMode_Quality;
      else
        var.ulVal = eAVEncCommonRateControlMode_CBR;
      break;
    case Bitrate::Mode::kVariable: {
      DCHECK(!rate_ctrl_);
      var.ulVal = eAVEncCommonRateControlMode_PeakConstrainedVBR;
      break;
    }
  }
  hr = codec_api_->SetValue(&CODECAPI_AVEncCommonRateControlMode, &var);
  if (!compatible_with_win7_) {
    // Though CODECAPI_AVEncCommonRateControlMode is supported by Windows 7, but
    // according to a discussion on MSDN,
    // https://social.msdn.microsoft.com/Forums/windowsdesktop/en-US/6da521e9-7bb3-4b79-a2b6-b31509224638/win7-h264-encoder-imfsinkwriter-cant-use-quality-vbr-encoding?forum=mediafoundationdevelopment
    // setting it on Windows 7 returns error.
    RETURN_ON_HR_FAILURE(hr, "Couldn't set CommonRateControlMode", false);
  }

  // Intel drivers want the layer count to be set explicitly for H.264/HEVC,
  // even if it's one.
  const bool set_svc_layer_count =
      (num_temporal_layers_ > 1) ||
      (vendor_ == DriverVendor::kIntel &&
       (codec_ == VideoCodec::kH264 || codec_ == VideoCodec::kHEVC));
  if (set_svc_layer_count) {
    var.ulVal = num_temporal_layers_;
    hr = codec_api_->SetValue(&CODECAPI_AVEncVideoTemporalLayerCount, &var);
    if (!compatible_with_win7_) {
      RETURN_ON_HR_FAILURE(hr, "Couldn't set temporal layer count", false);
    }
  }

  if (!rate_ctrl_) {
    var.ulVal = AdjustBitrateToFrameRate(bitrate_allocation_.GetSumBps(),
                                         configured_frame_rate_, frame_rate_);
    hr = codec_api_->SetValue(&CODECAPI_AVEncCommonMeanBitRate, &var);
    if (!compatible_with_win7_) {
      RETURN_ON_HR_FAILURE(hr, "Couldn't set bitrate", false);
    }
  }

  if (bitrate_allocation_.GetMode() == Bitrate::Mode::kVariable) {
    var.ulVal = AdjustBitrateToFrameRate(bitrate_allocation_.GetPeakBps(),
                                         configured_frame_rate_, frame_rate_);
    hr = codec_api_->SetValue(&CODECAPI_AVEncCommonMaxBitRate, &var);
    if (!compatible_with_win7_) {
      RETURN_ON_HR_FAILURE(hr, "Couldn't set bitrate", false);
    }
  }

  if (S_OK == codec_api_->IsModifiable(&CODECAPI_AVEncAdaptiveMode)) {
    var.ulVal = eAVEncAdaptiveMode_Resolution;
    hr = codec_api_->SetValue(&CODECAPI_AVEncAdaptiveMode, &var);
    if (!compatible_with_win7_) {
      RETURN_ON_HR_FAILURE(hr, "Couldn't set adaptive mode", false);
    }
  }

  var.ulVal = gop_length_;
  hr = codec_api_->SetValue(&CODECAPI_AVEncMPVGOPSize, &var);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set keyframe interval", false);

  if (S_OK == codec_api_->IsModifiable(&CODECAPI_AVLowLatencyMode)) {
    var.vt = VT_BOOL;
    var.boolVal = low_latency_mode_ ? VARIANT_TRUE : VARIANT_FALSE;
    hr = codec_api_->SetValue(&CODECAPI_AVLowLatencyMode, &var);
    if (!compatible_with_win7_) {
      RETURN_ON_HR_FAILURE(hr, "Couldn't set low latency mode", false);
    }
  }

  return true;
}

void MediaFoundationVideoEncodeAccelerator::NotifyError(
    VideoEncodeAccelerator::Error error) {
  main_client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::NotifyError, main_client_, error));
}

void MediaFoundationVideoEncodeAccelerator::EncodeTask(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(encode_sequence_checker_);

  bool input_delivered = false;
  HRESULT hr = E_FAIL;
  if (input_required_) {
    // Hardware MFT is waiting for this coming input.
    hr = ProcessInput(std::move(frame), force_keyframe);
    if (FAILED(hr)) {
      NotifyError(kPlatformFailureError);
      RETURN_ON_HR_FAILURE(hr, "Couldn't encode", );
    }

    DVLOG(3) << "Sent for encode " << hr;
    input_delivered = true;
    input_required_ = false;
  } else {
    Microsoft::WRL::ComPtr<IMFMediaEvent> media_event;
    hr = event_generator_->GetEvent(MF_EVENT_FLAG_NO_WAIT, &media_event);
    if (FAILED(hr)) {
      DLOG(WARNING) << "Abandoned input frame for video encoder.";
      return;
    }

    MediaEventType event_type;
    hr = media_event->GetType(&event_type);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to get the type of media event.";
      return;
    }

    // Always deliver the current input into HMFT.
    if (event_type == METransformNeedInput) {
      hr = ProcessInput(std::move(frame), force_keyframe);
      if (FAILED(hr)) {
        NotifyError(kPlatformFailureError);
        RETURN_ON_HR_FAILURE(hr, "Couldn't encode", );
      }

      DVLOG(3) << "Sent for encode " << hr;
      input_delivered = true;
    } else if (event_type == METransformHaveOutput) {
      ProcessOutput();
      input_delivered =
          TryToDeliverInputFrame(std::move(frame), force_keyframe);
    }
  }

  if (!input_delivered) {
    DLOG(ERROR) << "Failed to deliver input frame to video encoder";
    return;
  }

  TryToReturnBitstreamBuffer();
}

HRESULT MediaFoundationVideoEncodeAccelerator::ProcessInput(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(encode_sequence_checker_);

  HRESULT hr = PopulateInputSampleBuffer(frame);
  RETURN_ON_HR_FAILURE(hr, "Couldn't populate input sample buffer", hr);

  input_sample_->SetSampleTime(frame->timestamp().InMicroseconds() *
                               kOneMicrosecondInMFSampleTimeUnits);
  UINT64 sample_duration = 0;
  hr = MFFrameRateToAverageTimePerFrame(frame_rate_, 1, &sample_duration);
  RETURN_ON_HR_FAILURE(hr, "Couldn't calculate sample duration", E_FAIL);
  input_sample_->SetSampleDuration(sample_duration);

  if (force_keyframe) {
    VARIANT var;
    var.vt = VT_UI4;
    var.ulVal = 1;
    hr = codec_api_->SetValue(&CODECAPI_AVEncVideoForceKeyFrame, &var);
    if (!compatible_with_win7_ && FAILED(hr)) {
      LOG(WARNING) << "Failed to set CODECAPI_AVEncVideoForceKeyFrame, "
                      "HRESULT: 0x"
                   << std::hex << hr;
    }
  }
  if (rate_ctrl_) {
    VideoRateControlWrapper::FrameParams frame_params{};
    frame_params.frame_type =
        force_keyframe
            ? VideoRateControlWrapper::FrameParams::FrameType::kKeyFrame
            : VideoRateControlWrapper::FrameParams::FrameType::kInterFrame;
    int qp = rate_ctrl_->ComputeQP(frame_params);
    VARIANT var;
    var.vt = VT_UI8;
    var.ulVal = QindextoAVEncQP(static_cast<uint8_t>(qp));
    hr = codec_api_->SetValue(&CODECAPI_AVEncVideoEncodeQP, &var);
    RETURN_ON_HR_FAILURE(hr, "Couldn't set current layer QP", hr);
    hr = input_sample_->SetUINT64(MFSampleExtension_VideoEncodeQP, var.ulVal);
    RETURN_ON_HR_FAILURE(hr, "Couldn't set input sample attribute QP", hr);
  }

  return encoder_->ProcessInput(input_stream_id_, input_sample_.Get(), 0);
}

HRESULT MediaFoundationVideoEncodeAccelerator::PopulateInputSampleBuffer(
    scoped_refptr<VideoFrame> frame) {
  if (frame->storage_type() !=
          VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER &&
      !frame->IsMappable()) {
    DLOG(ERROR) << "Unsupported video frame storage type";
    return MF_E_INVALID_STREAM_DATA;
  }

  if (frame->format() != PIXEL_FORMAT_NV12 &&
      frame->format() != PIXEL_FORMAT_I420) {
    DLOG(ERROR) << "Unsupported video frame format";
    return MF_E_INVALID_STREAM_DATA;
  }

  if (frame->storage_type() ==
      VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER) {
    gfx::GpuMemoryBuffer* gmb = frame->GetGpuMemoryBuffer();
    if (!gmb) {
      DLOG(ERROR) << "Failed to get GMB for input frame";
      return MF_E_INVALID_STREAM_DATA;
    }

    if (gmb->GetType() != gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE &&
        gmb->GetType() != gfx::GpuMemoryBufferType::SHARED_MEMORY_BUFFER) {
      DLOG(ERROR) << "Unsupported GMB type";
      return MF_E_INVALID_STREAM_DATA;
    }

    if (gmb->GetType() == gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE &&
        dxgi_device_manager_ != nullptr) {
      if (!dxgi_resource_mapping_required_) {
        return PopulateInputSampleBufferGpu(std::move(frame));
      } else {
        return CopyInputSampleBufferFromGpu(*(frame.get()));
      }
    }

    // ConvertToMemoryMappedFrame() doesn't copy pixel data,
    // it just maps GPU buffer owned by |frame| and presents it as mapped
    // view in CPU memory. |frame| will unmap the buffer when destructed.
    frame = ConvertToMemoryMappedFrame(std::move(frame));
    if (!frame) {
      DLOG(ERROR) << "Failed to map shared memory GMB";
      return E_FAIL;
    }
  }

  const auto kTargetPixelFormat = PIXEL_FORMAT_NV12;
  Microsoft::WRL::ComPtr<IMFMediaBuffer> input_buffer;
  HRESULT hr = input_sample_->GetBufferByIndex(0, &input_buffer);
  if (FAILED(hr)) {
    // Allocate a new buffer.
    MFT_INPUT_STREAM_INFO input_stream_info;
    hr = encoder_->GetInputStreamInfo(input_stream_id_, &input_stream_info);
    RETURN_ON_HR_FAILURE(hr, "Couldn't get input stream info", hr);
    hr = MFCreateAlignedMemoryBuffer(
        input_stream_info.cbSize ? input_stream_info.cbSize
                                 : VideoFrame::AllocationSize(
                                       kTargetPixelFormat, input_visible_size_),
        input_stream_info.cbAlignment == 0 ? input_stream_info.cbAlignment
                                           : input_stream_info.cbAlignment - 1,
        &input_buffer);
    RETURN_ON_HR_FAILURE(hr, "Failed to create memory buffer", hr);
    hr = input_sample_->AddBuffer(input_buffer.Get());
    RETURN_ON_HR_FAILURE(hr, "Failed to add buffer to sample", hr);
  }

  // Establish plain pointers into the input buffer, where we will copy pixel
  // data to.
  MediaBufferScopedPointer scoped_buffer(input_buffer.Get());
  DCHECK(scoped_buffer.get());
  uint8_t* dst_y = scoped_buffer.get();
  size_t dst_y_stride = VideoFrame::RowBytes(
      VideoFrame::kYPlane, kTargetPixelFormat, input_visible_size_.width());
  uint8_t* dst_uv =
      scoped_buffer.get() +
      dst_y_stride * VideoFrame::Rows(VideoFrame::kYPlane, kTargetPixelFormat,
                                      input_visible_size_.height());
  size_t dst_uv_stride = VideoFrame::RowBytes(
      VideoFrame::kUVPlane, kTargetPixelFormat, input_visible_size_.width());
  uint8_t* end = dst_uv + dst_uv_stride * frame->rows(VideoFrame::kUVPlane);
  DCHECK_GE(static_cast<ptrdiff_t>(scoped_buffer.max_length()),
            end - scoped_buffer.get());

  // Set up a VideoFrame with the data pointing into the input buffer.
  // We need it to ease copying and scaling by reusing ConvertAndScaleFrame()
  auto frame_in_buffer = VideoFrame::WrapExternalYuvData(
      kTargetPixelFormat, input_visible_size_, gfx::Rect(input_visible_size_),
      input_visible_size_, dst_y_stride, dst_uv_stride, dst_y, dst_uv,
      frame->timestamp());

  auto status = ConvertAndScaleFrame(*frame, *frame_in_buffer, resize_buffer_);
  if (!status.is_ok()) {
    DLOG(ERROR) << "ConvertAndScaleFrame failed with error code: "
                << static_cast<uint32_t>(status.code());
    return E_FAIL;
  }
  return S_OK;
}

// Handle case where video frame is backed by a GPU texture, but needs to be
// copied to CPU memory, if HMFT does not accept texture from adapter different
// from that is currently used for encoding.
HRESULT MediaFoundationVideoEncodeAccelerator::CopyInputSampleBufferFromGpu(
    const VideoFrame& frame) {
  DCHECK_EQ(frame.storage_type(),
            VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER);
  DCHECK(frame.HasGpuMemoryBuffer());
  DCHECK_EQ(frame.GetGpuMemoryBuffer()->GetType(),
            gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE);
  DCHECK(dxgi_device_manager_);

  gfx::GpuMemoryBufferHandle buffer_handle =
      frame.GetGpuMemoryBuffer()->CloneHandle();

  auto d3d_device = dxgi_device_manager_->GetDevice();
  if (!d3d_device) {
    DLOG(ERROR) << "Failed to get device from MF DXGI device manager";
    return E_HANDLE;
  }
  Microsoft::WRL::ComPtr<ID3D11Device1> device1;
  HRESULT hr = d3d_device.As(&device1);

  RETURN_ON_HR_FAILURE(hr, "Failed to query ID3D11Device1", hr);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture;
  hr = device1->OpenSharedResource1(buffer_handle.dxgi_handle.Get(),
                                    IID_PPV_ARGS(&input_texture));
  RETURN_ON_HR_FAILURE(hr, "Failed to open shared GMB D3D texture", hr);

  // Check if we need to scale the input texture
  D3D11_TEXTURE2D_DESC input_desc = {};
  input_texture->GetDesc(&input_desc);

  Microsoft::WRL::ComPtr<ID3D11Texture2D> sample_texture;
  if (input_desc.Width != static_cast<uint32_t>(input_visible_size_.width()) ||
      input_desc.Height !=
          static_cast<uint32_t>(input_visible_size_.height())) {
    hr = PerformD3DScaling(input_texture.Get());
    RETURN_ON_HR_FAILURE(hr, "Failed to perform D3D video processing", hr);
    sample_texture = scaled_d3d11_texture_;
  } else {
    sample_texture = input_texture;
  }

  const auto kTargetPixelFormat = PIXEL_FORMAT_NV12;
  Microsoft::WRL::ComPtr<IMFMediaBuffer> input_buffer;

  // Allocate a new buffer.
  MFT_INPUT_STREAM_INFO input_stream_info;
  hr = encoder_->GetInputStreamInfo(input_stream_id_, &input_stream_info);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get input stream info", hr);
  hr = MFCreateAlignedMemoryBuffer(
      input_stream_info.cbSize
          ? input_stream_info.cbSize
          : VideoFrame::AllocationSize(kTargetPixelFormat, input_visible_size_),
      input_stream_info.cbAlignment == 0 ? input_stream_info.cbAlignment
                                         : input_stream_info.cbAlignment - 1,
      &input_buffer);
  RETURN_ON_HR_FAILURE(hr, "Failed to create memory buffer for input sample",
                       hr);

  MediaBufferScopedPointer scoped_buffer(input_buffer.Get());
  bool copy_succeeded = gpu::CopyD3D11TexToMem(
      sample_texture.Get(), scoped_buffer.get(), scoped_buffer.max_length(),
      d3d_device.Get(), &staging_texture_);
  if (!copy_succeeded) {
    DLOG(ERROR) << "Failed to copy sample to memory.";
    return E_FAIL;
  }
  size_t copied_bytes =
      input_visible_size_.width() * input_visible_size_.height() * 3 / 2;
  hr = input_buffer->SetCurrentLength(copied_bytes);
  RETURN_ON_HR_FAILURE(hr, "Failed to set current buffer length", hr);
  hr = input_sample_->RemoveAllBuffers();
  RETURN_ON_HR_FAILURE(hr, "Failed to remove buffers from sample", hr);
  hr = input_sample_->AddBuffer(input_buffer.Get());
  RETURN_ON_HR_FAILURE(hr, "Failed to add buffer to sample", hr);
  return S_OK;
}

// Handle case where video frame is backed by a GPU texture
HRESULT MediaFoundationVideoEncodeAccelerator::PopulateInputSampleBufferGpu(
    scoped_refptr<VideoFrame> frame) {
  DCHECK_EQ(frame->storage_type(),
            VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER);
  DCHECK(frame->HasGpuMemoryBuffer());
  DCHECK_EQ(frame->GetGpuMemoryBuffer()->GetType(),
            gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE);
  DCHECK(dxgi_device_manager_);

  gfx::GpuMemoryBufferHandle buffer_handle =
      frame->GetGpuMemoryBuffer()->CloneHandle();

  auto d3d_device = dxgi_device_manager_->GetDevice();
  if (!d3d_device) {
    DLOG(ERROR) << "Failed to get device from MF DXGI device manager";
    return E_HANDLE;
  }

  Microsoft::WRL::ComPtr<ID3D11Device1> device1;
  HRESULT hr = d3d_device.As(&device1);
  RETURN_ON_HR_FAILURE(hr, "Failed to query ID3D11Device1", hr);

  Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture;
  hr = device1->OpenSharedResource1(buffer_handle.dxgi_handle.Get(),
                                    IID_PPV_ARGS(&input_texture));
  RETURN_ON_HR_FAILURE(hr, "Failed to open shared GMB D3D texture", hr);

  // Check if we need to scale the input texture
  D3D11_TEXTURE2D_DESC input_desc = {};
  input_texture->GetDesc(&input_desc);

  Microsoft::WRL::ComPtr<ID3D11Texture2D> sample_texture;
  if (input_desc.Width != static_cast<uint32_t>(input_visible_size_.width()) ||
      input_desc.Height !=
          static_cast<uint32_t>(input_visible_size_.height())) {
    hr = PerformD3DScaling(input_texture.Get());
    RETURN_ON_HR_FAILURE(hr, "Failed to perform D3D video processing", hr);
    sample_texture = scaled_d3d11_texture_;
  } else {
    sample_texture = input_texture;
  }

  Microsoft::WRL::ComPtr<IMFMediaBuffer> input_buffer;
  hr = MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D),
                                 sample_texture.Get(), 0, FALSE, &input_buffer);
  RETURN_ON_HR_FAILURE(hr, "Failed to create MF DXGI surface buffer", hr);

  // Some encoder MFTs (e.g. Qualcomm) depend on the sample buffer having a
  // valid current length. Call GetMaxLength() to compute the plane size.
  DWORD buffer_length = 0;
  hr = input_buffer->GetMaxLength(&buffer_length);
  RETURN_ON_HR_FAILURE(hr, "Failed to get max buffer length", hr);
  hr = input_buffer->SetCurrentLength(buffer_length);
  RETURN_ON_HR_FAILURE(hr, "Failed to set current buffer length", hr);

  hr = input_sample_->RemoveAllBuffers();
  RETURN_ON_HR_FAILURE(hr, "Failed to remove buffers from sample", hr);
  hr = input_sample_->AddBuffer(input_buffer.Get());
  RETURN_ON_HR_FAILURE(hr, "Failed to add buffer to sample", hr);
  return S_OK;
}

int MediaFoundationVideoEncodeAccelerator::AssignTemporalIdBySvcSpec(
    bool keyframe) {
  int result = 0;

  if (keyframe)
    outputs_since_keyframe_count_ = 0;

  switch (num_temporal_layers_) {
    case 1:
      return 0;
    case 2: {
      const static std::array<int, 2> kTwoTemporalLayers = {0, 1};
      result = kTwoTemporalLayers[outputs_since_keyframe_count_ %
                                  kTwoTemporalLayers.size()];
      break;
    }
    case 3: {
      const static std::array<int, 4> kThreeTemporalLayers = {0, 2, 1, 2};
      result = kThreeTemporalLayers[outputs_since_keyframe_count_ %
                                    kThreeTemporalLayers.size()];
      break;
    }
  }
  outputs_since_keyframe_count_++;
  return result;
}

bool MediaFoundationVideoEncodeAccelerator::AssignTemporalId(
    Microsoft::WRL::ComPtr<IMFMediaBuffer> output_buffer,
    size_t size,
    int* temporal_id,
    bool keyframe) {
  *temporal_id = 0;

  // H264, HEVC, VP9 and AV1 have hardware SVC support on windows. H264 can
  // parse the information from Nalu(7.3.1 NAL unit syntax); AV1 can parse the
  // OBU(5.3.3. OBU extension header syntax), it's future work. Unfortunately,
  // VP9 spec doesn't provide the temporal information, we can only assign it
  // based on spec.
  if (codec_ == VideoCodec::kH264) {
    // See the 7.3.1 NAL unit syntax in H264 spec.
    // https://www.itu.int/rec/T-REC-H.264
    MediaBufferScopedPointer scoped_buffer(output_buffer.Get());
    h264_parser_.SetStream(scoped_buffer.get(), size);
    H264NALU nalu;
    H264Parser::Result result;
    while ((result = h264_parser_.AdvanceToNextNALU(&nalu)) !=
           H264Parser::kEOStream) {
      // Fallback to software when the stream is invalid.
      if (result == H264Parser::Result::kInvalidStream)
        return false;

      if (nalu.nal_unit_type == H264NALU::kPrefix) {
        *temporal_id = (nalu.data[kPrefixNALLocatedBytePos] & 0b1110'0000) >> 5;
        return true;
      }
    }
  }
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  else if (codec_ == VideoCodec::kHEVC) {
    // See section 7.3.1.1, NAL unit syntax in H265 spec.
    // https://www.itu.int/rec/T-REC-H.265
    // Unlike AVC, HEVC stores the temporal ID information in VCL NAL unit
    // header instead of using prefix NAL unit.
    MediaBufferScopedPointer scoped_buffer(output_buffer.Get());
    h265_nalu_parser_.SetStream(scoped_buffer.get(), size);
    H265NALU nalu;
    H265NaluParser::Result result;
    while ((result = h265_nalu_parser_.AdvanceToNextNALU(&nalu)) !=
           H265NaluParser::kEOStream) {
      if (result == H265NaluParser::Result::kInvalidStream) {
        return false;
      }
      // We only check VCL NAL units
      if (nalu.nal_unit_type <= H265NALU::RSV_VCL31) {
        *temporal_id = nalu.nuh_temporal_id_plus1 - 1;
        return true;
      }
    }
  }
#endif

  // If we run to this point, it means that we have not assigned temporalId
  // through parsing stream, we always return true once we parse out temporalId.
  // Now we will assign the ID based on spec.
  *temporal_id = AssignTemporalIdBySvcSpec(keyframe);

  return true;
}

void MediaFoundationVideoEncodeAccelerator::ProcessOutput() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(encode_sequence_checker_);

  MFT_OUTPUT_DATA_BUFFER output_data_buffer = {0};
  output_data_buffer.dwStreamID = output_stream_id_;
  output_data_buffer.dwStatus = 0;
  output_data_buffer.pEvents = nullptr;
  output_data_buffer.pSample = nullptr;
  DWORD status = 0;
  HRESULT hr = encoder_->ProcessOutput(0, 1, &output_data_buffer, &status);
  if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
    hr = S_OK;
    Microsoft::WRL::ComPtr<IMFMediaType> media_type;
    for (DWORD type_index = 0; SUCCEEDED(hr); ++type_index) {
      hr = encoder_->GetOutputAvailableType(output_stream_id_, type_index,
                                            &media_type);
      if (SUCCEEDED(hr)) {
        break;
      }
    }
    hr = encoder_->SetOutputType(output_stream_id_, media_type.Get(), 0);
    return;
  }

  RETURN_ON_HR_FAILURE(hr, "Couldn't get encoded data", );
  DVLOG(3) << "Got encoded data " << hr;

  Microsoft::WRL::ComPtr<IMFMediaBuffer> output_buffer;
  hr = output_data_buffer.pSample->GetBufferByIndex(0, &output_buffer);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get buffer by index", );

  DWORD size = 0;
  hr = output_buffer->GetCurrentLength(&size);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get buffer length", );
  DCHECK_NE(size, 0u);
  if (rate_ctrl_) {
    // Notify SW BRC about recent encoded frame size.
    rate_ctrl_->PostEncodeUpdate(size);
  }

  base::TimeDelta timestamp;
  LONGLONG sample_time;
  hr = output_data_buffer.pSample->GetSampleTime(&sample_time);
  if (SUCCEEDED(hr)) {
    timestamp =
        base::Microseconds(sample_time / kOneMicrosecondInMFSampleTimeUnits);
  }

  // For HMFT that continuously reports valid QP, update encoder info so that
  // WebRTC will not use bandwidth quality scaler for resolution adaptation.
  uint64_t frame_qp = 0xfffful;
  bool should_notify_encoder_info_change = false;
  hr = output_data_buffer.pSample->GetUINT64(MFSampleExtension_VideoEncodeQP,
                                             &frame_qp);
  if (vendor_ == DriverVendor::kIntel) {
    if (codec_ == VideoCodec::kH264) {
      if ((FAILED(hr) || !IsValidQp(codec_, frame_qp)) &&
          encoder_info_.reports_average_qp) {
        should_notify_encoder_info_change = true;
        encoder_info_.reports_average_qp = false;
      }
    } else if (codec_ == VideoCodec::kVP9 || codec_ == VideoCodec::kAV1) {
      if (!rate_ctrl_)
        encoder_info_.reports_average_qp = false;
    }
  }
  if (!encoder_info_sent_ || should_notify_encoder_info_change) {
    main_client_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Client::NotifyEncoderInfoChange,
                                  main_client_, encoder_info_));
    encoder_info_sent_ = true;
  }
  // Bits 0-15: Default QP.
  if (SUCCEEDED(hr)) {
    frame_qp = frame_qp & 0xfffful;
  }

  const bool keyframe = MFGetAttributeUINT32(
      output_data_buffer.pSample, MFSampleExtension_CleanPoint, false);
  int temporal_id = 0;
  if (!AssignTemporalId(output_buffer, size, &temporal_id, keyframe)) {
    DLOG(ERROR) << "Parse temporalId failed.";
    NotifyError(VideoEncodeAccelerator::Error::kPlatformFailureError);
    return;
  }
  DVLOG(3) << "Encoded data with size:" << size << " keyframe " << keyframe;

  // If no bit stream buffer presents, queue the output first.
  if (bitstream_buffer_queue_.empty()) {
    DVLOG(3) << "No bitstream buffers.";

    // We need to copy the output so that encoding can continue.
    auto encode_output =
        std::make_unique<EncodeOutput>(size, keyframe, timestamp, temporal_id);
    {
      MediaBufferScopedPointer scoped_buffer(output_buffer.Get());
      memcpy(encode_output->memory(), scoped_buffer.get(), size);
      if (IsValidQp(codec_, frame_qp)) {
        encode_output->SetQp(frame_qp);
      }
    }
    encoder_output_queue_.push_back(std::move(encode_output));
    output_data_buffer.pSample->Release();
    output_data_buffer.pSample = nullptr;
    return;
  }

  // Immediately return encoded buffer with BitstreamBuffer to client.
  std::unique_ptr<MediaFoundationVideoEncodeAccelerator::BitstreamBufferRef>
      buffer_ref = std::move(bitstream_buffer_queue_.front());
  bitstream_buffer_queue_.pop_front();

  {
    MediaBufferScopedPointer scoped_buffer(output_buffer.Get());
    if (!buffer_ref->mapping.IsValid() || !scoped_buffer.get()) {
      DLOG(ERROR) << "Failed to copy bitstream media buffer.";
      return;
    }

    memcpy(buffer_ref->mapping.memory(), scoped_buffer.get(), size);
  }

  output_data_buffer.pSample->Release();
  output_data_buffer.pSample = nullptr;

  BitstreamBufferMetadata md(size, keyframe, timestamp);
  if (IsValidQp(codec_, frame_qp)) {
    md.qp = static_cast<int32_t>(frame_qp);
  }

  if (temporalScalableCoding()) {
    if (codec_ == VideoCodec::kH264) {
      md.h264.emplace().temporal_idx = temporal_id;
    } else if (codec_ == VideoCodec::kHEVC) {
      md.h265.emplace().temporal_idx = temporal_id;
    }
  }
  main_client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::BitstreamBufferReady, main_client_,
                                buffer_ref->id, md));
}

bool MediaFoundationVideoEncodeAccelerator::TryToDeliverInputFrame(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe) {
  bool input_delivered = false;
  Microsoft::WRL::ComPtr<IMFMediaEvent> media_event;
  MediaEventType event_type;
  do {
    HRESULT hr =
        event_generator_->GetEvent(MF_EVENT_FLAG_NO_WAIT, &media_event);
    if (FAILED(hr)) {
      break;
    }

    hr = media_event->GetType(&event_type);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to get the type of media event.";
      break;
    }

    switch (event_type) {
      case METransformHaveOutput: {
        ProcessOutput();
        continue;
      }
      case METransformNeedInput: {
        hr = ProcessInput(std::move(frame), force_keyframe);
        if (FAILED(hr)) {
          NotifyError(kPlatformFailureError);
          RETURN_ON_HR_FAILURE(hr, "Couldn't encode", false);
        }

        DVLOG(3) << "Sent for encode " << hr;
        return true;
      }
      default:
        break;
    }
  } while (true);

  return input_delivered;
}

void MediaFoundationVideoEncodeAccelerator::TryToReturnBitstreamBuffer() {
  // Try to fetch the encoded frame in time.
  bool output_processed = false;
  do {
    Microsoft::WRL::ComPtr<IMFMediaEvent> media_event;
    MediaEventType event_type;
    HRESULT hr =
        event_generator_->GetEvent(MF_EVENT_FLAG_NO_WAIT, &media_event);
    if (FAILED(hr)) {
      if (!output_processed) {
        continue;
      } else {
        break;
      }
    }

    hr = media_event->GetType(&event_type);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to get the type of media event.";
      break;
    }

    switch (event_type) {
      case METransformHaveOutput: {
        ProcessOutput();
        output_processed = true;
        break;
      }
      case METransformNeedInput: {
        input_required_ = true;
        continue;
      }
      default:
        break;
    }
  } while (true);
}

void MediaFoundationVideoEncodeAccelerator::UseOutputBitstreamBufferTask(
    std::unique_ptr<BitstreamBufferRef> buffer_ref) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(encode_sequence_checker_);

  // If there is already EncodeOutput waiting, copy its output first.
  if (!encoder_output_queue_.empty()) {
    std::unique_ptr<MediaFoundationVideoEncodeAccelerator::EncodeOutput>
        encode_output = std::move(encoder_output_queue_.front());
    encoder_output_queue_.pop_front();
    memcpy(buffer_ref->mapping.memory(), encode_output->memory(),
           encode_output->size());

    BitstreamBufferMetadata md(encode_output->size(), encode_output->keyframe,
                               encode_output->capture_timestamp);
    if (encode_output->frame_qp) {
      md.qp = *encode_output->frame_qp;
    }
    if (temporalScalableCoding())
      md.h264.emplace().temporal_idx = encode_output->temporal_layer_id;
    main_client_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Client::BitstreamBufferReady, main_client_,
                                  buffer_ref->id, md));
    return;
  }

  bitstream_buffer_queue_.push_back(std::move(buffer_ref));
}

void MediaFoundationVideoEncodeAccelerator::RequestEncodingParametersChangeTask(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(encode_sequence_checker_);
  DCHECK(imf_output_media_type_);
  DCHECK(imf_input_media_type_);
  DCHECK(encoder_);
  RETURN_ON_FAILURE(
      bitrate_allocation.GetMode() == bitrate_allocation_.GetMode(),
      "Invalid bitrate mode", );
  framerate =
      base::clamp(framerate, 1u, static_cast<uint32_t>(kMaxFrameRateNumerator));

  if (framerate == frame_rate_ && bitrate_allocation == bitrate_allocation_)
    return;

  bitrate_allocation_ = bitrate_allocation;
  frame_rate_ = framerate;
  // For SW BRC we don't reconfigure the encoder.
  if (rate_ctrl_) {
    rate_ctrl_->UpdateRateControl(CreateRateControllerConfig(
        bitrate_allocation_, input_visible_size_, frame_rate_,
        /*num_temporal_layers=*/1, codec_));
    return;
  }

  VARIANT var;
  var.vt = VT_UI4;
  var.ulVal = AdjustBitrateToFrameRate(bitrate_allocation_.GetSumBps(),
                                       configured_frame_rate_, framerate);
  HRESULT hr = codec_api_->SetValue(&CODECAPI_AVEncCommonMeanBitRate, &var);
  if (!compatible_with_win7_) {
    RETURN_ON_HR_FAILURE(hr, "Couldn't update mean bitrate", );
  }

  if (bitrate_allocation_.GetMode() == Bitrate::Mode::kVariable) {
    var.ulVal = AdjustBitrateToFrameRate(bitrate_allocation_.GetPeakBps(),
                                         configured_frame_rate_, framerate);
    hr = codec_api_->SetValue(&CODECAPI_AVEncCommonMaxBitRate, &var);
    if (!compatible_with_win7_) {
      RETURN_ON_HR_FAILURE(hr, "Couldn't set max bitrate", );
    }
  }
}

void MediaFoundationVideoEncodeAccelerator::DestroyTask() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(encode_sequence_checker_);

  // Cancel all encoder thread callbacks.
  encoder_task_weak_factory_.InvalidateWeakPtrs();

  ReleaseEncoderResources();
}

void MediaFoundationVideoEncodeAccelerator::ReleaseEncoderResources() {
  while (!bitstream_buffer_queue_.empty())
    bitstream_buffer_queue_.pop_front();
  while (!encoder_output_queue_.empty())
    encoder_output_queue_.pop_front();

  if (activate_.Get() != nullptr) {
    activate_->ShutdownObject();
    activate_->Release();
    activate_.Reset();
  }
  encoder_.Reset();
  codec_api_.Reset();
  event_generator_.Reset();
  imf_input_media_type_.Reset();
  imf_output_media_type_.Reset();
  input_sample_.Reset();
  output_sample_.Reset();
}

HRESULT MediaFoundationVideoEncodeAccelerator::InitializeD3DVideoProcessing(
    ID3D11Texture2D* input_texture) {
  D3D11_TEXTURE2D_DESC input_desc = {};
  input_texture->GetDesc(&input_desc);
  if (vp_desc_.InputWidth == input_desc.Width &&
      vp_desc_.InputHeight == input_desc.Height) {
    return S_OK;
  }

  // Input/output framerates are dummy values for passthrough.
  D3D11_VIDEO_PROCESSOR_CONTENT_DESC vp_desc = {
      .InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE,
      .InputFrameRate = {60, 1},
      .InputWidth = input_desc.Width,
      .InputHeight = input_desc.Height,
      .OutputFrameRate = {60, 1},
      .OutputWidth = static_cast<UINT>(input_visible_size_.width()),
      .OutputHeight = static_cast<UINT>(input_visible_size_.height()),
      .Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL};

  Microsoft::WRL::ComPtr<ID3D11Device> texture_device;
  input_texture->GetDevice(&texture_device);
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device;
  HRESULT hr = texture_device.As(&video_device);
  RETURN_ON_HR_FAILURE(hr, "Failed to query for ID3D11VideoDevice", hr);

  Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>
      video_processor_enumerator;
  hr = video_device->CreateVideoProcessorEnumerator(
      &vp_desc, &video_processor_enumerator);
  RETURN_ON_HR_FAILURE(hr, "CreateVideoProcessorEnumerator failed", hr);

  Microsoft::WRL::ComPtr<ID3D11VideoProcessor> video_processor;
  hr = video_device->CreateVideoProcessor(video_processor_enumerator.Get(), 0,
                                          &video_processor);
  RETURN_ON_HR_FAILURE(hr, "CreateVideoProcessor failed", hr);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
  texture_device->GetImmediateContext(&device_context);
  Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context;
  hr = device_context.As(&video_context);
  RETURN_ON_HR_FAILURE(hr, "Failed to query for ID3D11VideoContext", hr);

  // Auto stream processing (the default) can hurt power consumption.
  video_context->VideoProcessorSetStreamAutoProcessingMode(
      video_processor.Get(), 0, FALSE);

  D3D11_TEXTURE2D_DESC scaled_desc = {
      .Width = static_cast<UINT>(input_visible_size_.width()),
      .Height = static_cast<UINT>(input_visible_size_.height()),
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = DXGI_FORMAT_NV12,
      .SampleDesc = {1, 0},
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
      .CPUAccessFlags = 0,
      .MiscFlags = 0};
  Microsoft::WRL::ComPtr<ID3D11Texture2D> scaled_d3d11_texture;
  hr = texture_device->CreateTexture2D(&scaled_desc, nullptr,
                                       &scaled_d3d11_texture);
  RETURN_ON_HR_FAILURE(hr, "Failed to create texture", hr);

  hr = SetDebugName(scaled_d3d11_texture.Get(),
                    "MFVideoEncodeAccelerator_ScaledTexture");
  RETURN_ON_HR_FAILURE(hr, "Failed to set debug name", hr);

  D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_desc = {};
  output_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
  output_desc.Texture2D.MipSlice = 0;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> vp_output_view;
  hr = video_device->CreateVideoProcessorOutputView(
      scaled_d3d11_texture.Get(), video_processor_enumerator.Get(),
      &output_desc, &vp_output_view);
  RETURN_ON_HR_FAILURE(hr, "CreateVideoProcessorOutputView failed", hr);

  video_device_ = std::move(video_device);
  video_processor_enumerator_ = std::move(video_processor_enumerator);
  video_processor_ = std::move(video_processor);
  video_context_ = std::move(video_context);
  vp_desc_ = std::move(vp_desc);
  scaled_d3d11_texture_ = std::move(scaled_d3d11_texture);
  vp_output_view_ = std::move(vp_output_view);
  return S_OK;
}

HRESULT MediaFoundationVideoEncodeAccelerator::PerformD3DScaling(
    ID3D11Texture2D* input_texture) {
  HRESULT hr = InitializeD3DVideoProcessing(input_texture);
  RETURN_ON_HR_FAILURE(hr, "Couldn't initialize D3D video processing", hr);

  // Set the color space for passthrough.
  auto src_color_space = gfx::ColorSpace::CreateSRGB();
  auto output_color_space = gfx::ColorSpace::CreateSRGB();

  D3D11_VIDEO_PROCESSOR_COLOR_SPACE src_d3d11_color_space =
      gfx::ColorSpaceWin::GetD3D11ColorSpace(src_color_space);
  video_context_->VideoProcessorSetStreamColorSpace(video_processor_.Get(), 0,
                                                    &src_d3d11_color_space);
  D3D11_VIDEO_PROCESSOR_COLOR_SPACE output_d3d11_color_space =
      gfx::ColorSpaceWin::GetD3D11ColorSpace(output_color_space);
  video_context_->VideoProcessorSetOutputColorSpace(video_processor_.Get(),
                                                    &output_d3d11_color_space);

  {
    absl::optional<gpu::DXGIScopedReleaseKeyedMutex> release_keyed_mutex;
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex;
    hr = input_texture->QueryInterface(IID_PPV_ARGS(&keyed_mutex));
    if (SUCCEEDED(hr)) {
      // The producer may still be using this texture for a short period of
      // time, so wait long enough to hopefully avoid glitches. For example,
      // all levels of the texture share the same keyed mutex, so if the
      // hardware decoder acquired the mutex to decode into a different array
      // level then it still may block here temporarily.
      constexpr int kMaxSyncTimeMs = 100;
      hr = keyed_mutex->AcquireSync(0, kMaxSyncTimeMs);
      RETURN_ON_HR_FAILURE(hr, "Failed to acquire keyed mutex", hr);
      release_keyed_mutex.emplace(std::move(keyed_mutex), 0);
    }

    // Setup |video_context_| for VPBlt operation.
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input_desc = {};
    input_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    input_desc.Texture2D.ArraySlice = 0;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> input_view;
    hr = video_device_->CreateVideoProcessorInputView(
        input_texture, video_processor_enumerator_.Get(), &input_desc,
        &input_view);
    RETURN_ON_HR_FAILURE(hr, "CreateVideoProcessorInputView failed", hr);

    D3D11_VIDEO_PROCESSOR_STREAM stream = {.Enable = true,
                                           .OutputIndex = 0,
                                           .InputFrameOrField = 0,
                                           .PastFrames = 0,
                                           .FutureFrames = 0,
                                           .pInputSurface = input_view.Get()};

    D3D11_TEXTURE2D_DESC input_texture_desc = {};
    input_texture->GetDesc(&input_texture_desc);
    RECT source_rect = {0, 0, static_cast<LONG>(input_texture_desc.Width),
                        static_cast<LONG>(input_texture_desc.Height)};
    video_context_->VideoProcessorSetStreamSourceRect(video_processor_.Get(), 0,
                                                      TRUE, &source_rect);

    D3D11_TEXTURE2D_DESC output_texture_desc = {};
    scaled_d3d11_texture_->GetDesc(&output_texture_desc);
    RECT dest_rect = {0, 0, static_cast<LONG>(output_texture_desc.Width),
                      static_cast<LONG>(output_texture_desc.Height)};
    video_context_->VideoProcessorSetOutputTargetRect(video_processor_.Get(),
                                                      TRUE, &dest_rect);
    video_context_->VideoProcessorSetStreamDestRect(video_processor_.Get(), 0,
                                                    TRUE, &dest_rect);

    hr = video_context_->VideoProcessorBlt(
        video_processor_.Get(), vp_output_view_.Get(), 0, 1, &stream);
    RETURN_ON_HR_FAILURE(hr, "VideoProcessorBlt failed", hr);
  }

  return hr;
}

}  // namespace media
