// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/windows/mf_video_encoder_util.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/native_library.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_variant.h"
#include "base/win/win_util.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/base/win/mf_helpers.h"
#include "media/base/win/mf_initializer.h"
#include "media/gpu/windows/mf_video_encoder_switches.h"

namespace media {

// AVEncVideoEncodeQP maps QP to libvpx qp tuning parameter
// and thus the range is 0-63.
uint8_t QindextoAVEncQP(VideoCodec codec, uint8_t q_index) {
  if (codec == VideoCodec::kAV1 || codec == VideoCodec::kVP9) {
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
    if (q_index <= 244) {
      return (q_index + 3) / 4;
    }
    if (q_index <= 249) {
      return 62;
    }
    return 63;
  }
  return q_index;
}

// This is the inverse of QindextoAVEncQP() function.
uint8_t AVEncQPtoQindex(VideoCodec codec, uint8_t avenc_qp) {
  if (codec == VideoCodec::kAV1 || codec == VideoCodec::kVP9) {
    uint8_t q_index = avenc_qp * 4;
    if (q_index == 248) {
      q_index = 249;
    } else if (q_index == 252) {
      q_index = 255;
    }
    return q_index;
  }
  return avenc_qp;
}

// According to AV1/VP9's bitstream specification, the valid range of qp
// value (defined as base_q_idx) should be 0-255.
bool IsValidQp(VideoCodec codec, uint64_t qp) {
  switch (codec) {
    case VideoCodec::kH264:
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case VideoCodec::kHEVC:
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
      return qp <= kH26xMaxQp;
    case VideoCodec::kVP9:
      return qp <= kVP9MaxQIndex;
    case VideoCodec::kAV1:
      return qp <= kAV1MaxQIndex;
    default:
      return false;
  }
}

uint8_t GetMaxQuantizer(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kH264:
      return kH264MaxQuantizer;
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case VideoCodec::kHEVC:
      return kH265MaxQuantizer;
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case VideoCodec::kVP9:
      return kVP9MaxQuantizer;
    case VideoCodec::kAV1:
      return kAV1MaxQuantizer;
    default:
      return 0;  // Return invalid value for unsupported codec.
  }
}

eAVEncH264VProfile GetH264VProfile(VideoCodecProfile profile,
                                   bool is_constrained_h264) {
  switch (profile) {
    case H264PROFILE_BASELINE:
      return is_constrained_h264 ? eAVEncH264VProfile_ConstrainedBase
                                 : eAVEncH264VProfile_Base;
    case H264PROFILE_MAIN:
      return eAVEncH264VProfile_Main;
    case H264PROFILE_HIGH:
      return eAVEncH264VProfile_High;
    default:
      return eAVEncH264VProfile_unknown;
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

DriverVendor GetDriverVendor(IMFActivate* encoder) {
  DCHECK(encoder);
  base::win::ScopedCoMem<WCHAR> vendor_id;
  UINT32 id_length;
  encoder->GetAllocatedString(MFT_ENUM_HARDWARE_VENDOR_ID_Attribute, &vendor_id,
                              &id_length);
  if (id_length != 8) {  // Normal vendor ids have length 8.
    return DriverVendor::kOther;
  }
  if (!_wcsnicmp(vendor_id.get(), L"VEN_10DE", id_length)) {
    return DriverVendor::kNvidia;
  }
  if (!_wcsnicmp(vendor_id.get(), L"VEN_1002", id_length)) {
    return DriverVendor::kAMD;
  }
  if (!_wcsnicmp(vendor_id.get(), L"VEN_8086 ", id_length)) {
    return DriverVendor::kIntel;
  }
  if (!_wcsnicmp(vendor_id.get(), L"VEN_QCOM", id_length)) {
    return DriverVendor::kQualcomm;
  }
  return DriverVendor::kOther;
}

// The driver tells us how many temporal layers it supports, but we may need to
// reduce this limit to avoid bad or untested drivers.
int GetMaxTemporalLayerVendorLimit(
    DriverVendor vendor,
    VideoCodec codec,
    const gpu::GpuDriverBugWorkarounds& workarounds) {
#if defined(ARCH_CPU_X86)
  // x86 systems sometimes crash in video drivers here.
  // More info: https://crbug.com/1253748
  return 1;
#else
  // crbug.com/1373780: Nvidia HEVC encoder reports supporting 3 temporal
  // layers, but will fail initialization if configured to encoded with
  // more than one temporal layers, thus we block Nvidia HEVC encoder for
  // temporal SVC encoding.
  if (codec == VideoCodec::kHEVC && vendor == DriverVendor::kNvidia) {
    return 1;
  }

  // Qualcomm HEVC and AV1 encoders report temporal layer support, but will
  // fail the tests currently, so block from temporal SVC encoding.
  if ((codec == VideoCodec::kHEVC || codec == VideoCodec::kAV1) &&
      vendor == DriverVendor::kQualcomm) {
    return 1;
  }

  // Intel drivers with issues of dynamically changing bitrate at CBR mode for
  // HEVC should be blocked from L1T3 encoding, as there is no SW BRC support
  // for that at present.
  if (codec == VideoCodec::kHEVC && vendor == DriverVendor::kIntel &&
      workarounds.disable_hevc_hmft_cbr_encoding) {
    return 2;
  }

  // Temporal layer encoding is disabled for VP9 unless a flag is enabled.
  //
  // For example, the Intel VP9 HW encoder reports supporting 3 temporal layers
  // but the number of temporal layers we allow depends on feature flags. At the
  // time of writing, Intel L1T3 may not be spec-compliant.
  // - See https://crbug.com/1425117 for temporal layer foundation (L1T2/L1T3).
  // - See https://crbug.com/1501767 for L1T2 rollout (not L1T3).
  if (codec == VideoCodec::kVP9) {
    if (vendor == DriverVendor::kIntel &&
        workarounds.disable_vp9_hmft_temporal_encoding) {
      return 1;
    }

    if (base::FeatureList::IsEnabled(kMediaFoundationVP9L1T3Support)) {
      return 3;
    }
    if (base::FeatureList::IsEnabled(kMediaFoundationVP9L1T2Support)) {
      return 2;
    }
    return 1;
  }

  if (codec == VideoCodec::kAV1) {
    // Whenever you add to the allow-list a new temporal layer limit, make sure
    // you update output bitstream metadata to indicate whether the encoded AV1
    // bitstream follows WebRTC SVC spec.
    if (vendor != DriverVendor::kIntel) {
      return 1;
    }
    if (base::FeatureList::IsEnabled(kMediaFoundationAV1L1T3Support)) {
      return 3;
    }
    if (base::FeatureList::IsEnabled(kMediaFoundationAV1L1T2Support)) {
      return 2;
    }
    return 1;
  }

  // No driver/codec specific limit to enforce.
  return 3;
#endif
}

int GetNumSupportedTemporalLayers(
    IMFActivate* activate,
    VideoCodec codec,
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  DCHECK(activate);
  auto vendor = GetDriverVendor(activate);
  int max_temporal_layer_vendor_limit =
      GetMaxTemporalLayerVendorLimit(vendor, codec, workarounds);
  if (max_temporal_layer_vendor_limit == 1) {
    return 1;
  }

  ComMFTransform encoder;
  ComCodecAPI codec_api;
  HRESULT hr = activate->ActivateObject(IID_PPV_ARGS(&encoder));
  if (FAILED(hr)) {
    // Log to VLOG since errors are expected as part of GetSupportedProfiles().
    DVLOG(2) << "Failed to activate encoder: " << PrintHr(hr);
    return 1;
  }

  hr = encoder.As(&codec_api);
  if (FAILED(hr)) {
    // Log to VLOG since errors are expected as part of GetSupportedProfiles().
    DVLOG(2) << "Failed to get encoder as CodecAPI: " << PrintHr(hr);
    return 1;
  }

  if (codec_api->IsSupported(&CODECAPI_AVEncVideoTemporalLayerCount) != S_OK) {
    return 1;
  }

  base::win::ScopedVariant min, max, step;
  if (FAILED(codec_api->GetParameterRange(
          &CODECAPI_AVEncVideoTemporalLayerCount, min.AsInput(), max.AsInput(),
          step.AsInput()))) {
    return 1;
  }

  // Temporal encoding is only considered supported if the driver reports at
  // least a span of 1-3 temporal layers.
  if (V_UI4(min.ptr()) > 1u || V_UI4(max.ptr()) < 3u) {
    return 1;
  }
  return max_temporal_layer_vendor_limit;
}

bool IsIntelHybridAV1Encoder(IMFActivate* activate) {
  DCHECK(activate);
  if (GetDriverVendor(activate) == DriverVendor::kIntel) {
    // Get the CLSID GUID of the HMFT.
    GUID mft_guid = {0};
    activate->GetGUID(MFT_TRANSFORM_CLSID_Attribute, &mft_guid);
    if (mft_guid == kIntelAV1HybridEncoderCLSID) {
      return true;
    }
  }
  return false;
}

using MFTEnum2Type = decltype(&MFTEnum2);
MFTEnum2Type GetMFTEnum2Function() {
  static const MFTEnum2Type kMFTEnum2Func = []() {
    auto mf_dll = base::LoadSystemLibrary(L"mfplat.dll");
    return mf_dll ? reinterpret_cast<MFTEnum2Type>(
                        base::GetFunctionPointerFromNativeLibrary(mf_dll,
                                                                  "MFTEnum2"))
                  : nullptr;
  }();
  return kMFTEnum2Func;
}

// If MFTEnum2 is unavailable, this uses MFTEnumEx and doesn't fill any
// adapter information if there are more than one adapters.
std::vector<Microsoft::WRL::ComPtr<IMFActivate>>
EnumerateHardwareEncodersLegacy(VideoCodec codec) {
  std::vector<Microsoft::WRL::ComPtr<IMFActivate>> encoders;

  uint32_t flags = MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER;
  MFT_REGISTER_TYPE_INFO input_info;
  input_info.guidMajorType = MFMediaType_Video;
  input_info.guidSubtype = MFVideoFormat_NV12;
  MFT_REGISTER_TYPE_INFO output_info;
  output_info.guidMajorType = MFMediaType_Video;
  output_info.guidSubtype = VideoCodecToMFSubtype(codec);

  Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
  auto hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to create DXGI Factory";
    return encoders;
  }

  LUID single_adapter_luid{0, 0};
  int num_adapters = 0;

  Microsoft::WRL::ComPtr<IDXGIAdapter> temp_adapter;
  for (UINT adapter_idx = 0;
       SUCCEEDED(factory->EnumAdapters(adapter_idx, &temp_adapter));
       adapter_idx++) {
    ++num_adapters;

    DXGI_ADAPTER_DESC desc;
    hr = temp_adapter->GetDesc(&desc);
    if (FAILED(hr)) {
      continue;
    }

    if (desc.VendorId == 0x1414 && desc.DeviceId == 0x8c) {
      // Skip MS software adapters.
      --num_adapters;
    } else {
      single_adapter_luid = desc.AdapterLuid;
    }
  }

  IMFActivate** pp_activates = nullptr;
  uint32_t count = 0;
  hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, flags, &input_info, &output_info,
                 &pp_activates, &count);

  if (FAILED(hr)) {
    // Log to VLOG since errors are expected as part of
    // GetSupportedProfiles().
    DVLOG(2) << "Failed to enumerate hardware encoders for "
             << GetCodecName(codec) << " : " << PrintHr(hr);
    return encoders;
  }

  for (UINT32 i = 0; i < count; i++) {
    if (codec == VideoCodec::kAV1 && IsIntelHybridAV1Encoder(pp_activates[i])) {
      continue;
    }

    // We can still infer the MFT's adapter LUID if there's only one adapter
    // in the system.
    if (num_adapters == 1) {
      pp_activates[i]->SetBlob(MFT_ENUM_ADAPTER_LUID,
                               reinterpret_cast<BYTE*>(&single_adapter_luid),
                               sizeof(LUID));
    }
    encoders.push_back(pp_activates[i]);
  }

  if (pp_activates) {
    CoTaskMemFree(pp_activates);
  }

  return encoders;
}

std::vector<Microsoft::WRL::ComPtr<IMFActivate>> EnumerateHardwareEncoders(
    VideoCodec codec) {
  std::vector<Microsoft::WRL::ComPtr<IMFActivate>> encoders;

  if (!InitializeMediaFoundation()) {
    return encoders;
  }

  MFTEnum2Type mftenum2_func = GetMFTEnum2Function();
  if (!mftenum2_func) {
    return EnumerateHardwareEncodersLegacy(codec);
  }

  uint32_t flags = MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER;
  MFT_REGISTER_TYPE_INFO input_info;
  input_info.guidMajorType = MFMediaType_Video;
  input_info.guidSubtype = MFVideoFormat_NV12;
  MFT_REGISTER_TYPE_INFO output_info;
  output_info.guidMajorType = MFMediaType_Video;
  output_info.guidSubtype = VideoCodecToMFSubtype(codec);

  Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
  auto hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
  if (FAILED(hr)) {
    DVLOG(2) << "Failed to create DXGI Factory";
    return encoders;
  }

  Microsoft::WRL::ComPtr<IDXGIAdapter> temp_adapter;
  for (UINT adapter_idx = 0;
       SUCCEEDED(factory->EnumAdapters(adapter_idx, &temp_adapter));
       adapter_idx++) {
    DXGI_ADAPTER_DESC desc;
    hr = temp_adapter->GetDesc(&desc);
    if (FAILED(hr)) {
      DVLOG(2) << "Failed to get description for adapter " << adapter_idx;
      continue;
    }

    Microsoft::WRL::ComPtr<IMFAttributes> attributes;
    hr = MFCreateAttributes(&attributes, 1);
    if (FAILED(hr = attributes->SetBlob(
                   MFT_ENUM_ADAPTER_LUID,
                   reinterpret_cast<BYTE*>(&desc.AdapterLuid), sizeof(LUID)))) {
      continue;
    }

    IMFActivate** pp_activates = nullptr;
    uint32_t count = 0;
    // MFTEnum2() function call.
    hr = mftenum2_func(MFT_CATEGORY_VIDEO_ENCODER, flags, &input_info,
                       &output_info, attributes.Get(), &pp_activates, &count);

    if (FAILED(hr)) {
      // Log to VLOG since errors are expected as part of
      // GetSupportedProfiles().
      DVLOG(2) << "Failed to enumerate hardware encoders for "
               << GetCodecName(codec) << " at a adapter #" << adapter_idx
               << " : " << PrintHr(hr);
      continue;
    }

    for (UINT32 i = 0; i < count; i++) {
      if (codec == VideoCodec::kAV1 &&
          IsIntelHybridAV1Encoder(pp_activates[i])) {
        continue;
      }
      // It's safe to ignore return value here.
      // if SetBlob fails, the IMFActivate won't have a valid adapter LUID
      // which will fail the check for preferred adapter LUID, so the
      // MFDXGIDeviceManager will not be set for MFT, which is a safe option.
      pp_activates[i]->SetBlob(MFT_ENUM_ADAPTER_LUID,
                               reinterpret_cast<BYTE*>(&desc.AdapterLuid),
                               sizeof(LUID));
      encoders.push_back(pp_activates[i]);
    }

    if (pp_activates) {
      CoTaskMemFree(pp_activates);
    }
  }

  return encoders;
}

uint32_t CalculateMaxFramerateFromMacroBlocksPerSecond(
    const FramerateAndResolution& max_framerate_and_resolution,
    uint32_t max_macroblocks_per_second) {
  constexpr uint64_t kMacroBlockWidth = 16u;
  constexpr uint64_t kMacroBlockHeight = 16u;

  uint64_t max_possible_framerate = std::floor(
      (max_macroblocks_per_second * kMacroBlockWidth * kMacroBlockHeight) /
      max_framerate_and_resolution.resolution.Area64());

  return std::clamp(static_cast<uint32_t>(max_possible_framerate), 1u,
                    max_framerate_and_resolution.frame_rate);
}

std::vector<FramerateAndResolution> GetMaxFramerateAndResolutionsFromMFT(
    VideoCodec codec,
    IMFTransform* encoder,
    bool allow_set_output_type) {
  DCHECK(encoder);
  std::vector<FramerateAndResolution> framerate_and_resolutions;
  if (allow_set_output_type) {
    Microsoft::WRL::ComPtr<IMFMediaType> media_type;

    RETURN_ON_HR_FAILURE(MFCreateMediaType(&media_type),
                         "Create media type failed", framerate_and_resolutions);
    RETURN_ON_HR_FAILURE(
        media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
        "Set major type failed", framerate_and_resolutions);
    RETURN_ON_HR_FAILURE(
        media_type->SetGUID(MF_MT_SUBTYPE, VideoCodecToMFSubtype(codec)),
        "Set guid for sub type failed", framerate_and_resolutions);
    RETURN_ON_HR_FAILURE(
        MFSetAttributeSize(
            media_type.Get(), MF_MT_FRAME_SIZE,
            kDefaultMaxFramerateAndResolution.resolution.width(),
            kDefaultMaxFramerateAndResolution.resolution.height()),
        "Set attribute size failed", framerate_and_resolutions);
    // Frame rate,30, is dummy value for pass through.
    RETURN_ON_HR_FAILURE(
        MFSetAttributeRatio(
            media_type.Get(), MF_MT_FRAME_RATE,
            /*unNumerator=*/kDefaultMaxFramerateAndResolution.frame_rate,
            /*unDenominator=*/1),
        "Set attribute ratio failed", framerate_and_resolutions);
    RETURN_ON_HR_FAILURE(media_type->SetUINT32(MF_MT_AVG_BITRATE, 9000000),
                         "Set avg bitrate failed", framerate_and_resolutions);
    RETURN_ON_HR_FAILURE(media_type->SetUINT32(MF_MT_INTERLACE_MODE,
                                               MFVideoInterlace_Progressive),
                         "Set interlace mode failed",
                         framerate_and_resolutions);

    if (codec != VideoCodec::kVP9) {
      UINT32 max_level;
      switch (codec) {
        case VideoCodec::kH264:
          max_level = eAVEncH264VLevel5_2;
          break;
        case VideoCodec::kAV1:
          max_level = eAVEncAV1VLevel6_3;
          break;
        case VideoCodec::kHEVC:
          max_level = eAVEncH265VLevel6_2;
          break;
        default:
          NOTREACHED();
      }
      RETURN_ON_HR_FAILURE(media_type->SetUINT32(MF_MT_VIDEO_LEVEL, max_level),
                           "Set video level failed", framerate_and_resolutions);
    }

    RETURN_ON_HR_FAILURE(
        encoder->SetOutputType(/*stream_id=*/0, media_type.Get(), 0),
        "Set output type failed", framerate_and_resolutions);
  }

  Microsoft::WRL::ComPtr<IMFAttributes> attributes;
  RETURN_ON_HR_FAILURE(encoder->GetAttributes(&attributes),
                       "Get attributes failed", framerate_and_resolutions);
  uint32_t max_macroblocks_per_second =
      MFGetAttributeUINT32(attributes.Get(), MF_VIDEO_MAX_MB_PER_SEC, 0);
  max_macroblocks_per_second &=
      0x0fffffff;  // Only lower 28 bits are supported.

  std::vector<FramerateAndResolution> max_framerate_and_resolutions;
  if (codec == VideoCodec::kH264) {
    max_framerate_and_resolutions.push_back(kLegacy2KMaxFramerateAndResolution);
    max_framerate_and_resolutions.push_back(kLegacy4KMaxFramerateAndResolution);
  } else if (codec == VideoCodec::kVP9) {
    max_framerate_and_resolutions.push_back(
        kVP9Modern2KMaxFramerateAndResolution);
    max_framerate_and_resolutions.push_back(
        kVP9Modern4KMaxFramerateAndResolution);
    max_framerate_and_resolutions.push_back(
        kVP9Modern8KMaxFramerateAndResolution);
  } else {
    max_framerate_and_resolutions.push_back(kModern2KMaxFramerateAndResolution);
    max_framerate_and_resolutions.push_back(kModern4KMaxFramerateAndResolution);
    max_framerate_and_resolutions.push_back(kModern8KMaxFramerateAndResolution);
  }

  for (auto& max_framerate_and_resolution : max_framerate_and_resolutions) {
    FramerateAndResolution framerate_and_resolution = {
        CalculateMaxFramerateFromMacroBlocksPerSecond(
            max_framerate_and_resolution, max_macroblocks_per_second),
        max_framerate_and_resolution.resolution};

    // Only if the calculated framerate >= the default framerate, we then
    // consider this resolution & framerate combination is supported.
    if (framerate_and_resolution.frame_rate >=
        (kDefaultFrameRateNumerator / kDefaultFrameRateDenominator)) {
      framerate_and_resolutions.push_back(framerate_and_resolution);
    }
  }

  // If the received value of `max_macroblocks_per_second` equals to zero,
  // assign a default value here.
  if (framerate_and_resolutions.empty()) {
    framerate_and_resolutions.push_back(kDefaultMaxFramerateAndResolution);
  }

  return framerate_and_resolutions;
}

// Ideally, we should query the API to get the minimum resolution of each codec
// under each vendor. However, since there is no such API available, based on
// the actual results, although the minimum resolutions of different codecs for
// each vendor vary, but the results always remain consistent among different
// GPU models. Therefore, we can just hardcode these values within the function.
gfx::Size GetMinResolution(VideoCodec codec, DriverVendor vendor) {
  switch (codec) {
    case VideoCodec::kH264:
      switch (vendor) {
        case DriverVendor::kAMD:
          // Below result based on: AMD Radeon(TM) Graphics (Ryzen 7 Pro 4750U),
          // AMD Radeon(TM) Graphics (Ryzen 9 9950X), AMD Radeon(TM) RX 6600.
          return gfx::Size(128, 128);
        case DriverVendor::kIntel:
          // Below result based on: Intel UHD Graphics 750, Intel Arc(TM) A380,
          // Intel Arc(TM) Graphic (Ultra5 125H), Intel(R) Iris(R) Xe MAX
          // Graphics.
          return gfx::Size(18, 18);
        case DriverVendor::kNvidia:
          // Below result based on: NVIDIA RTX 3050, NVIDIA RTX 3070, NVIDIA RTX
          // 4080.
          return gfx::Size(146, 50);
        case DriverVendor::kQualcomm:
          // Below result based on: Qualcomm(R) Adreno(TM) 690, Qualcomm(R)
          // Adreno(TM) X1-85.
          return gfx::Size(96, 96);
        case DriverVendor::kOther:
          return kDefaultMinResolution;
      }
    case VideoCodec::kHEVC:
      switch (vendor) {
        case DriverVendor::kAMD:
          // Below result based on: AMD Radeon(TM) Graphics (Ryzen 7 Pro 4750U),
          // AMD Radeon(TM) Graphics (Ryzen 9 9950X), AMD Radeon(TM) RX 6600.
          return gfx::Size(130, 128);
        case DriverVendor::kIntel:
          // Below result based on: Intel UHD Graphics 750, Intel Arc(TM) A380,
          // Intel Arc(TM) Graphic (Ultra5 125H), Intel(R) Iris(R) Xe MAX
          // Graphics.
          return gfx::Size(66, 114);
        case DriverVendor::kNvidia:
          // Below result based on: NVIDIA RTX 3050, NVIDIA RTX 3070, NVIDIA RTX
          // 4080.
          return gfx::Size(130, 34);
        case DriverVendor::kQualcomm:
          // Below result based on: Qualcomm(R) Adreno(TM) 690, Qualcomm(R)
          // Adreno(TM) X1-85.
          return gfx::Size(96, 96);
        case DriverVendor::kOther:
          return kDefaultMinResolution;
      }
    case VideoCodec::kVP9:
      switch (vendor) {
        case DriverVendor::kAMD:
          // Below result based on: AMD Radeon(TM) Graphics (Ryzen 9 9950X).
          return gfx::Size(66, 66);
        case DriverVendor::kIntel:
          // Below result based on: Intel UHD Graphics 750, Intel Arc(TM) A380,
          // Intel Arc(TM) Graphic (Ultra5 125H), Intel(R) Iris(R) Xe MAX
          // Graphics.
          //
          // NOTE: Intel UHD Graphics 750, Intel Arc(TM) A380, Intel(R) Iris(R)
          // Xe MAX Graphics actually only requires >= 66x66, but Intel Arc(TM)
          // Graphic (Ultra5 125H) requires >= 66x120.
          return gfx::Size(66, 120);
        case DriverVendor::kNvidia:
          // Below result based on: NVIDIA RTX 4080.
          return gfx::Size(66, 66);
        // As of the testing date, no Qualcomm hardware supports VP9 encoding.
        case DriverVendor::kQualcomm:
        case DriverVendor::kOther:
          return kDefaultMinResolution;
      }
    case VideoCodec::kAV1:
      switch (vendor) {
        case DriverVendor::kAMD:
          // Below result based on: AMD Radeon(TM) Graphics (Ryzen 9 9950X).
          return gfx::Size(114, 82);
        case DriverVendor::kIntel:
          // Below result based on: Intel Arc(TM) A380, Intel Arc(TM) Graphic
          // (Ultra5 125H).
          return gfx::Size(114, 82);
        case DriverVendor::kNvidia:
          // Below result based on: NVIDIA RTX 4080.
          return gfx::Size(130, 66);
        case DriverVendor::kQualcomm:
          // Below result based on: Qualcomm(R) Adreno(TM) X1-85.
          return gfx::Size(256, 128);
        case DriverVendor::kOther:
          return kDefaultMinResolution;
      }
    default:
      NOTREACHED();
  }
}

size_t GetMFTGuidHash(IMFActivate* activate) {
  DCHECK(activate);

  GUID mft_guid = {0};
  activate->GetGUID(MFT_TRANSFORM_CLSID_Attribute, &mft_guid);
  return base::FastHash(
      base::as_byte_span(base::win::WStringFromGUID(mft_guid)));
}

}  // namespace media
