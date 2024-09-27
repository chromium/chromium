// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_info_mojom_traits.h"
#include "build/build_config.h"

#include "base/notreached.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/ipc/common/vulkan_info_mojom_traits.h"
#endif

namespace mojo {

// static
bool StructTraits<gpu::mojom::GpuDeviceDataView, gpu::GPUInfo::GPUDevice>::Read(
    gpu::mojom::GpuDeviceDataView data,
    gpu::GPUInfo::GPUDevice* out) {
  out->vendor_id = data.vendor_id();
  out->device_id = data.device_id();
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  out->revision = data.revision();
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_WIN)
  out->sub_sys_id = data.sub_sys_id();
#endif  // BUILDFLAG(IS_WIN)
  out->active = data.active();
  return data.ReadVendorString(&out->vendor_string) &&
         data.ReadDeviceString(&out->device_string) &&
#if BUILDFLAG(IS_WIN)
         data.ReadLuid(&out->luid) &&
#endif  // BUILDFLAG(IS_WIN)
         data.ReadDriverVendor(&out->driver_vendor) &&
         data.ReadDriverVersion(&out->driver_version) &&
         data.ReadGpuPreference(&out->gpu_preference);
}

// static
gpu::mojom::VideoCodecProfile
EnumTraits<gpu::mojom::VideoCodecProfile, gpu::VideoCodecProfile>::ToMojom(
    gpu::VideoCodecProfile video_codec_profile) {
  switch (video_codec_profile) {
    case gpu::VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN:
      return gpu::mojom::VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN;
    case gpu::VideoCodecProfile::H264PROFILE_BASELINE:
      return gpu::mojom::VideoCodecProfile::H264PROFILE_BASELINE;
    case gpu::VideoCodecProfile::H264PROFILE_MAIN:
      return gpu::mojom::VideoCodecProfile::H264PROFILE_MAIN;
    case gpu::VideoCodecProfile::H264PROFILE_EXTENDED:
      return gpu::mojom::VideoCodecProfile::H264PROFILE_EXTENDED;
    case gpu::VideoCodecProfile::H264PROFILE_HIGH:
      return gpu::mojom::VideoCodecProfile::H264PROFILE_HIGH;
    case gpu::VideoCodecProfile::H264PROFILE_HIGH10PROFILE:
      return gpu::mojom::VideoCodecProfile::H264PROFILE_HIGH10PROFILE;
    case gpu::VideoCodecProfile::H264PROFILE_HIGH422PROFILE:
      return gpu::mojom::VideoCodecProfile::H264PROFILE_HIGH422PROFILE;
    case gpu::VideoCodecProfile::H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return gpu::mojom::VideoCodecProfile::
          H264PROFILE_HIGH444PREDICTIVEPROFILE;
    case gpu::VideoCodecProfile::H264PROFILE_SCALABLEBASELINE:
      return gpu::mojom::VideoCodecProfile::H264PROFILE_SCALABLEBASELINE;
    case gpu::VideoCodecProfile::H264PROFILE_SCALABLEHIGH:
      return gpu::mojom::VideoCodecProfile::H264PROFILE_SCALABLEHIGH;
    case gpu::VideoCodecProfile::H264PROFILE_STEREOHIGH:
      return gpu::mojom::VideoCodecProfile::H264PROFILE_STEREOHIGH;
    case gpu::VideoCodecProfile::H264PROFILE_MULTIVIEWHIGH:
      return gpu::mojom::VideoCodecProfile::H264PROFILE_MULTIVIEWHIGH;
    case gpu::VideoCodecProfile::VP8PROFILE_ANY:
      return gpu::mojom::VideoCodecProfile::VP8PROFILE_ANY;
    case gpu::VideoCodecProfile::VP9PROFILE_PROFILE0:
      return gpu::mojom::VideoCodecProfile::VP9PROFILE_PROFILE0;
    case gpu::VideoCodecProfile::VP9PROFILE_PROFILE1:
      return gpu::mojom::VideoCodecProfile::VP9PROFILE_PROFILE1;
    case gpu::VideoCodecProfile::VP9PROFILE_PROFILE2:
      return gpu::mojom::VideoCodecProfile::VP9PROFILE_PROFILE2;
    case gpu::VideoCodecProfile::VP9PROFILE_PROFILE3:
      return gpu::mojom::VideoCodecProfile::VP9PROFILE_PROFILE3;
    case gpu::VideoCodecProfile::HEVCPROFILE_MAIN:
      return gpu::mojom::VideoCodecProfile::HEVCPROFILE_MAIN;
    case gpu::VideoCodecProfile::HEVCPROFILE_MAIN10:
      return gpu::mojom::VideoCodecProfile::HEVCPROFILE_MAIN10;
    case gpu::VideoCodecProfile::HEVCPROFILE_MAIN_STILL_PICTURE:
      return gpu::mojom::VideoCodecProfile::HEVCPROFILE_MAIN_STILL_PICTURE;
    case gpu::VideoCodecProfile::HEVCPROFILE_REXT:
      return gpu::mojom::VideoCodecProfile::HEVCPROFILE_REXT;
    case gpu::VideoCodecProfile::HEVCPROFILE_HIGH_THROUGHPUT:
      return gpu::mojom::VideoCodecProfile::HEVCPROFILE_HIGH_THROUGHPUT;
    case gpu::VideoCodecProfile::HEVCPROFILE_MULTIVIEW_MAIN:
      return gpu::mojom::VideoCodecProfile::HEVCPROFILE_MULTIVIEW_MAIN;
    case gpu::VideoCodecProfile::HEVCPROFILE_SCALABLE_MAIN:
      return gpu::mojom::VideoCodecProfile::HEVCPROFILE_SCALABLE_MAIN;
    case gpu::VideoCodecProfile::HEVCPROFILE_3D_MAIN:
      return gpu::mojom::VideoCodecProfile::HEVCPROFILE_3D_MAIN;
    case gpu::VideoCodecProfile::HEVCPROFILE_SCREEN_EXTENDED:
      return gpu::mojom::VideoCodecProfile::HEVCPROFILE_SCREEN_EXTENDED;
    case gpu::VideoCodecProfile::HEVCPROFILE_SCALABLE_REXT:
      return gpu::mojom::VideoCodecProfile::HEVCPROFILE_SCALABLE_REXT;
    case gpu::VideoCodecProfile::HEVCPROFILE_HIGH_THROUGHPUT_SCREEN_EXTENDED:
      return gpu::mojom::VideoCodecProfile::
          HEVCPROFILE_HIGH_THROUGHPUT_SCREEN_EXTENDED;
    case gpu::VideoCodecProfile::DOLBYVISION_PROFILE0:
      return gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE0;
    case gpu::VideoCodecProfile::DOLBYVISION_PROFILE5:
      return gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE5;
    case gpu::VideoCodecProfile::DOLBYVISION_PROFILE7:
      return gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE7;
    case gpu::VideoCodecProfile::DOLBYVISION_PROFILE8:
      return gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE8;
    case gpu::VideoCodecProfile::DOLBYVISION_PROFILE9:
      return gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE9;
    case gpu::VideoCodecProfile::THEORAPROFILE_ANY:
      return gpu::mojom::VideoCodecProfile::THEORAPROFILE_ANY;
    case gpu::VideoCodecProfile::AV1PROFILE_PROFILE_MAIN:
      return gpu::mojom::VideoCodecProfile::AV1PROFILE_PROFILE_MAIN;
    case gpu::VideoCodecProfile::AV1PROFILE_PROFILE_HIGH:
      return gpu::mojom::VideoCodecProfile::AV1PROFILE_PROFILE_HIGH;
    case gpu::VideoCodecProfile::AV1PROFILE_PROFILE_PRO:
      return gpu::mojom::VideoCodecProfile::AV1PROFILE_PROFILE_PRO;
    case gpu::VideoCodecProfile::VVCPROFILE_MAIN10:
      return gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN10;
    case gpu::VideoCodecProfile::VVCPROFILE_MAIN12:
      return gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN12;
    case gpu::VideoCodecProfile::VVCPROFILE_MAIN12_INTRA:
      return gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN12_INTRA;
    case gpu::VideoCodecProfile::VVCPROIFLE_MULTILAYER_MAIN10:
      return gpu::mojom::VideoCodecProfile::VVCPROIFLE_MULTILAYER_MAIN10;
    case gpu::VideoCodecProfile::VVCPROFILE_MAIN10_444:
      return gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN10_444;
    case gpu::VideoCodecProfile::VVCPROFILE_MAIN12_444:
      return gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN12_444;
    case gpu::VideoCodecProfile::VVCPROFILE_MAIN16_444:
      return gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN16_444;
    case gpu::VideoCodecProfile::VVCPROFILE_MAIN12_444_INTRA:
      return gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN12_444_INTRA;
    case gpu::VideoCodecProfile::VVCPROFILE_MAIN16_444_INTRA:
      return gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN16_444_INTRA;
    case gpu::VideoCodecProfile::VVCPROFILE_MULTILAYER_MAIN10_444:
      return gpu::mojom::VideoCodecProfile::VVCPROFILE_MULTILAYER_MAIN10_444;
    case gpu::VideoCodecProfile::VVCPROFILE_MAIN10_STILL_PICTURE:
      return gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN10_STILL_PICTURE;
    case gpu::VideoCodecProfile::VVCPROFILE_MAIN12_STILL_PICTURE:
      return gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN12_STILL_PICTURE;
    case gpu::VideoCodecProfile::VVCPROFILE_MAIN10_444_STILL_PICTURE:
      return gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN10_444_STILL_PICTURE;
    case gpu::VideoCodecProfile::VVCPROFILE_MAIN12_444_STILL_PICTURE:
      return gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN12_444_STILL_PICTURE;
    case gpu::VideoCodecProfile::VVCPROFILE_MAIN16_444_STILL_PICTURE:
      return gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN12_444_STILL_PICTURE;
  }
  NOTREACHED_IN_MIGRATION()
      << "Invalid VideoCodecProfile:" << video_codec_profile;
  return gpu::mojom::VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN;
}

// static
bool EnumTraits<gpu::mojom::VideoCodecProfile, gpu::VideoCodecProfile>::
    FromMojom(gpu::mojom::VideoCodecProfile input,
              gpu::VideoCodecProfile* out) {
  switch (input) {
    case gpu::mojom::VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN:
      *out = gpu::VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_BASELINE:
      *out = gpu::VideoCodecProfile::H264PROFILE_BASELINE;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_MAIN:
      *out = gpu::VideoCodecProfile::H264PROFILE_MAIN;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_EXTENDED:
      *out = gpu::VideoCodecProfile::H264PROFILE_EXTENDED;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_HIGH:
      *out = gpu::VideoCodecProfile::H264PROFILE_HIGH;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_HIGH10PROFILE:
      *out = gpu::VideoCodecProfile::H264PROFILE_HIGH10PROFILE;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_HIGH422PROFILE:
      *out = gpu::VideoCodecProfile::H264PROFILE_HIGH422PROFILE;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_HIGH444PREDICTIVEPROFILE:
      *out = gpu::VideoCodecProfile::H264PROFILE_HIGH444PREDICTIVEPROFILE;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_SCALABLEBASELINE:
      *out = gpu::VideoCodecProfile::H264PROFILE_SCALABLEBASELINE;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_SCALABLEHIGH:
      *out = gpu::VideoCodecProfile::H264PROFILE_SCALABLEHIGH;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_STEREOHIGH:
      *out = gpu::VideoCodecProfile::H264PROFILE_STEREOHIGH;
      return true;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_MULTIVIEWHIGH:
      *out = gpu::VideoCodecProfile::H264PROFILE_MULTIVIEWHIGH;
      return true;
    case gpu::mojom::VideoCodecProfile::VP8PROFILE_ANY:
      *out = gpu::VideoCodecProfile::VP8PROFILE_ANY;
      return true;
    case gpu::mojom::VideoCodecProfile::VP9PROFILE_PROFILE0:
      *out = gpu::VideoCodecProfile::VP9PROFILE_PROFILE0;
      return true;
    case gpu::mojom::VideoCodecProfile::VP9PROFILE_PROFILE1:
      *out = gpu::VideoCodecProfile::VP9PROFILE_PROFILE1;
      return true;
    case gpu::mojom::VideoCodecProfile::VP9PROFILE_PROFILE2:
      *out = gpu::VideoCodecProfile::VP9PROFILE_PROFILE2;
      return true;
    case gpu::mojom::VideoCodecProfile::VP9PROFILE_PROFILE3:
      *out = gpu::VideoCodecProfile::VP9PROFILE_PROFILE3;
      return true;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_MAIN:
      *out = gpu::VideoCodecProfile::HEVCPROFILE_MAIN;
      return true;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_MAIN10:
      *out = gpu::VideoCodecProfile::HEVCPROFILE_MAIN10;
      return true;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_MAIN_STILL_PICTURE:
      *out = gpu::VideoCodecProfile::HEVCPROFILE_MAIN_STILL_PICTURE;
      return true;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_REXT:
      *out = gpu::VideoCodecProfile::HEVCPROFILE_REXT;
      return true;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_HIGH_THROUGHPUT:
      *out = gpu::VideoCodecProfile::HEVCPROFILE_HIGH_THROUGHPUT;
      return true;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_MULTIVIEW_MAIN:
      *out = gpu::VideoCodecProfile::HEVCPROFILE_MULTIVIEW_MAIN;
      return true;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_SCALABLE_MAIN:
      *out = gpu::VideoCodecProfile::HEVCPROFILE_SCALABLE_MAIN;
      return true;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_3D_MAIN:
      *out = gpu::VideoCodecProfile::HEVCPROFILE_3D_MAIN;
      return true;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_SCREEN_EXTENDED:
      *out = gpu::VideoCodecProfile::HEVCPROFILE_SCREEN_EXTENDED;
      return true;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_SCALABLE_REXT:
      *out = gpu::VideoCodecProfile::HEVCPROFILE_SCALABLE_REXT;
      return true;
    case gpu::mojom::VideoCodecProfile::
        HEVCPROFILE_HIGH_THROUGHPUT_SCREEN_EXTENDED:
      *out =
          gpu::VideoCodecProfile::HEVCPROFILE_HIGH_THROUGHPUT_SCREEN_EXTENDED;
      return true;
    case gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE0:
      *out = gpu::VideoCodecProfile::DOLBYVISION_PROFILE0;
      return true;
    case gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE5:
      *out = gpu::VideoCodecProfile::DOLBYVISION_PROFILE5;
      return true;
    case gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE7:
      *out = gpu::VideoCodecProfile::DOLBYVISION_PROFILE7;
      return true;
    case gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE8:
      *out = gpu::VideoCodecProfile::DOLBYVISION_PROFILE8;
      return true;
    case gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE9:
      *out = gpu::VideoCodecProfile::DOLBYVISION_PROFILE9;
      return true;
    case gpu::mojom::VideoCodecProfile::THEORAPROFILE_ANY:
      *out = gpu::VideoCodecProfile::THEORAPROFILE_ANY;
      return true;
    case gpu::mojom::VideoCodecProfile::AV1PROFILE_PROFILE_MAIN:
      *out = gpu::VideoCodecProfile::AV1PROFILE_PROFILE_MAIN;
      return true;
    case gpu::mojom::VideoCodecProfile::AV1PROFILE_PROFILE_HIGH:
      *out = gpu::VideoCodecProfile::AV1PROFILE_PROFILE_HIGH;
      return true;
    case gpu::mojom::VideoCodecProfile::AV1PROFILE_PROFILE_PRO:
      *out = gpu::VideoCodecProfile::AV1PROFILE_PROFILE_PRO;
      return true;
    case gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN10:
      *out = gpu::VideoCodecProfile::VVCPROFILE_MAIN10;
      return true;
    case gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN12:
      *out = gpu::VideoCodecProfile::VVCPROFILE_MAIN12;
      return true;
    case gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN12_INTRA:
      *out = gpu::VideoCodecProfile::VVCPROFILE_MAIN12_INTRA;
      return true;
    case gpu::mojom::VideoCodecProfile::VVCPROIFLE_MULTILAYER_MAIN10:
      *out = gpu::VideoCodecProfile::VVCPROIFLE_MULTILAYER_MAIN10;
      return true;
    case gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN10_444:
      *out = gpu::VideoCodecProfile::VVCPROFILE_MAIN10_444;
      return true;
    case gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN12_444:
      *out = gpu::VideoCodecProfile::VVCPROFILE_MAIN12_444;
      return true;
    case gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN16_444:
      *out = gpu::VideoCodecProfile::VVCPROFILE_MAIN16_444;
      return true;
    case gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN12_444_INTRA:
      *out = gpu::VideoCodecProfile::VVCPROFILE_MAIN12_444_INTRA;
      return true;
    case gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN16_444_INTRA:
      *out = gpu::VideoCodecProfile::VVCPROFILE_MAIN16_444_INTRA;
      return true;
    case gpu::mojom::VideoCodecProfile::VVCPROFILE_MULTILAYER_MAIN10_444:
      *out = gpu::VideoCodecProfile::VVCPROFILE_MULTILAYER_MAIN10_444;
      return true;
    case gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN10_STILL_PICTURE:
      *out = gpu::VideoCodecProfile::VVCPROFILE_MAIN10_STILL_PICTURE;
      return true;
    case gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN12_STILL_PICTURE:
      *out = gpu::VideoCodecProfile::VVCPROFILE_MAIN12_STILL_PICTURE;
      return true;
    case gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN10_444_STILL_PICTURE:
      *out = gpu::VideoCodecProfile::VVCPROFILE_MAIN10_444_STILL_PICTURE;
      return true;
    case gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN12_444_STILL_PICTURE:
      *out = gpu::VideoCodecProfile::VVCPROFILE_MAIN12_444_STILL_PICTURE;
      return true;
    case gpu::mojom::VideoCodecProfile::VVCPROFILE_MAIN16_444_STILL_PICTURE:
      *out = gpu::VideoCodecProfile::VVCPROFILE_MAIN12_444_STILL_PICTURE;
      return true;
  }
  NOTREACHED_IN_MIGRATION() << "Invalid VideoCodecProfile: " << input;
  return false;
}

// static
bool StructTraits<gpu::mojom::VideoDecodeAcceleratorSupportedProfileDataView,
                  gpu::VideoDecodeAcceleratorSupportedProfile>::
    Read(gpu::mojom::VideoDecodeAcceleratorSupportedProfileDataView data,
         gpu::VideoDecodeAcceleratorSupportedProfile* out) {
  out->encrypted_only = data.encrypted_only();
  return data.ReadProfile(&out->profile) &&
         data.ReadMaxResolution(&out->max_resolution) &&
         data.ReadMinResolution(&out->min_resolution);
}

// static
bool StructTraits<gpu::mojom::VideoDecodeAcceleratorCapabilitiesDataView,
                  gpu::VideoDecodeAcceleratorCapabilities>::
    Read(gpu::mojom::VideoDecodeAcceleratorCapabilitiesDataView data,
         gpu::VideoDecodeAcceleratorCapabilities* out) {
  if (!data.ReadSupportedProfiles(&out->supported_profiles))
    return false;
  out->flags = data.flags();
  return true;
}

// static
bool StructTraits<gpu::mojom::VideoEncodeAcceleratorSupportedProfileDataView,
                  gpu::VideoEncodeAcceleratorSupportedProfile>::
    Read(gpu::mojom::VideoEncodeAcceleratorSupportedProfileDataView data,
         gpu::VideoEncodeAcceleratorSupportedProfile* out) {
  out->max_framerate_numerator = data.max_framerate_numerator();
  out->max_framerate_denominator = data.max_framerate_denominator();
  return data.ReadMinResolution(&out->min_resolution) &&
         data.ReadMaxResolution(&out->max_resolution) &&
         data.ReadProfile(&out->profile);
}

// static
gpu::mojom::ImageDecodeAcceleratorType EnumTraits<
    gpu::mojom::ImageDecodeAcceleratorType,
    gpu::ImageDecodeAcceleratorType>::ToMojom(gpu::ImageDecodeAcceleratorType
                                                  image_type) {
  switch (image_type) {
    case gpu::ImageDecodeAcceleratorType::kJpeg:
      return gpu::mojom::ImageDecodeAcceleratorType::kJpeg;
    case gpu::ImageDecodeAcceleratorType::kWebP:
      return gpu::mojom::ImageDecodeAcceleratorType::kWebP;
    case gpu::ImageDecodeAcceleratorType::kUnknown:
      return gpu::mojom::ImageDecodeAcceleratorType::kUnknown;
  }
}

// static
bool EnumTraits<gpu::mojom::ImageDecodeAcceleratorType,
                gpu::ImageDecodeAcceleratorType>::
    FromMojom(gpu::mojom::ImageDecodeAcceleratorType input,
              gpu::ImageDecodeAcceleratorType* out) {
  switch (input) {
    case gpu::mojom::ImageDecodeAcceleratorType::kJpeg:
      *out = gpu::ImageDecodeAcceleratorType::kJpeg;
      return true;
    case gpu::mojom::ImageDecodeAcceleratorType::kWebP:
      *out = gpu::ImageDecodeAcceleratorType::kWebP;
      return true;
    case gpu::mojom::ImageDecodeAcceleratorType::kUnknown:
      *out = gpu::ImageDecodeAcceleratorType::kUnknown;
      return true;
  }
  NOTREACHED_IN_MIGRATION() << "Invalid ImageDecodeAcceleratorType: " << input;
  return false;
}

// static
gpu::mojom::ImageDecodeAcceleratorSubsampling
EnumTraits<gpu::mojom::ImageDecodeAcceleratorSubsampling,
           gpu::ImageDecodeAcceleratorSubsampling>::
    ToMojom(gpu::ImageDecodeAcceleratorSubsampling subsampling) {
  switch (subsampling) {
    case gpu::ImageDecodeAcceleratorSubsampling::k420:
      return gpu::mojom::ImageDecodeAcceleratorSubsampling::k420;
    case gpu::ImageDecodeAcceleratorSubsampling::k422:
      return gpu::mojom::ImageDecodeAcceleratorSubsampling::k422;
    case gpu::ImageDecodeAcceleratorSubsampling::k444:
      return gpu::mojom::ImageDecodeAcceleratorSubsampling::k444;
  }
}

// static
bool EnumTraits<gpu::mojom::ImageDecodeAcceleratorSubsampling,
                gpu::ImageDecodeAcceleratorSubsampling>::
    FromMojom(gpu::mojom::ImageDecodeAcceleratorSubsampling input,
              gpu::ImageDecodeAcceleratorSubsampling* out) {
  switch (input) {
    case gpu::mojom::ImageDecodeAcceleratorSubsampling::k420:
      *out = gpu::ImageDecodeAcceleratorSubsampling::k420;
      return true;
    case gpu::mojom::ImageDecodeAcceleratorSubsampling::k422:
      *out = gpu::ImageDecodeAcceleratorSubsampling::k422;
      return true;
    case gpu::mojom::ImageDecodeAcceleratorSubsampling::k444:
      *out = gpu::ImageDecodeAcceleratorSubsampling::k444;
      return true;
  }
  NOTREACHED_IN_MIGRATION()
      << "Invalid ImageDecodeAcceleratorSubsampling: " << input;
  return false;
}

// static
bool StructTraits<gpu::mojom::ImageDecodeAcceleratorSupportedProfileDataView,
                  gpu::ImageDecodeAcceleratorSupportedProfile>::
    Read(gpu::mojom::ImageDecodeAcceleratorSupportedProfileDataView data,
         gpu::ImageDecodeAcceleratorSupportedProfile* out) {
  return data.ReadImageType(&out->image_type) &&
         data.ReadMinEncodedDimensions(&out->min_encoded_dimensions) &&
         data.ReadMaxEncodedDimensions(&out->max_encoded_dimensions) &&
         data.ReadSubsamplings(&out->subsamplings);
}

#if BUILDFLAG(IS_WIN)
// static
gpu::mojom::OverlaySupport
EnumTraits<gpu::mojom::OverlaySupport, gpu::OverlaySupport>::ToMojom(
    gpu::OverlaySupport support) {
  switch (support) {
    case gpu::OverlaySupport::kNone:
      return gpu::mojom::OverlaySupport::NONE;
    case gpu::OverlaySupport::kDirect:
      return gpu::mojom::OverlaySupport::DIRECT;
    case gpu::OverlaySupport::kScaling:
      return gpu::mojom::OverlaySupport::SCALING;
    case gpu::OverlaySupport::kSoftware:
      return gpu::mojom::OverlaySupport::SOFTWARE;
  }
}

bool EnumTraits<gpu::mojom::OverlaySupport, gpu::OverlaySupport>::FromMojom(
    gpu::mojom::OverlaySupport input,
    gpu::OverlaySupport* out) {
  switch (input) {
    case gpu::mojom::OverlaySupport::NONE:
      *out = gpu::OverlaySupport::kNone;
      break;
    case gpu::mojom::OverlaySupport::DIRECT:
      *out = gpu::OverlaySupport::kDirect;
      break;
    case gpu::mojom::OverlaySupport::SCALING:
      *out = gpu::OverlaySupport::kScaling;
      break;
    case gpu::mojom::OverlaySupport::SOFTWARE:
      *out = gpu::OverlaySupport::kSoftware;
      break;
  }
  return true;
}

bool StructTraits<gpu::mojom::OverlayInfoDataView, gpu::OverlayInfo>::Read(
    gpu::mojom::OverlayInfoDataView data,
    gpu::OverlayInfo* out) {
  out->direct_composition = data.direct_composition();
  out->supports_overlays = data.supports_overlays();
  return data.ReadYuy2OverlaySupport(&out->yuy2_overlay_support) &&
         data.ReadNv12OverlaySupport(&out->nv12_overlay_support) &&
         data.ReadBgra8OverlaySupport(&out->bgra8_overlay_support) &&
         data.ReadRgb10a2OverlaySupport(&out->rgb10a2_overlay_support) &&
         data.ReadP010OverlaySupport(&out->p010_overlay_support);
}
#endif

bool StructTraits<gpu::mojom::GpuInfoDataView, gpu::GPUInfo>::Read(
    gpu::mojom::GpuInfoDataView data,
    gpu::GPUInfo* out) {
  out->optimus = data.optimus();
  out->amd_switchable = data.amd_switchable();
  out->gl_reset_notification_strategy = data.gl_reset_notification_strategy();
  out->sandboxed = data.sandboxed();
  out->in_process_gpu = data.in_process_gpu();
  out->passthrough_cmd_decoder = data.passthrough_cmd_decoder();
  out->can_support_threaded_texture_mailbox =
      data.can_support_threaded_texture_mailbox();
  out->jpeg_decode_accelerator_supported =
      data.jpeg_decode_accelerator_supported();

  out->subpixel_font_rendering = data.subpixel_font_rendering();
  out->visibility_callback_call_count = data.visibility_callback_call_count();

#if BUILDFLAG(IS_WIN)
  out->directml_feature_level = data.directml_feature_level();
  out->d3d12_feature_level = data.d3d12_feature_level();
  out->vulkan_version = data.vulkan_version();
  out->shared_image_d3d = data.shared_image_d3d();
#endif
#if BUILDFLAG(ENABLE_VULKAN)
  out->hardware_supports_vulkan = data.hardware_supports_vulkan();
#endif

  return data.ReadInitializationTime(&out->initialization_time) &&
         data.ReadGpu(&out->gpu) &&
         data.ReadSecondaryGpus(&out->secondary_gpus) &&
         data.ReadNpus(&out->npus) &&
         data.ReadPixelShaderVersion(&out->pixel_shader_version) &&
         data.ReadVertexShaderVersion(&out->vertex_shader_version) &&
         data.ReadMaxMsaaSamples(&out->max_msaa_samples) &&
         data.ReadMachineModelName(&out->machine_model_name) &&
         data.ReadMachineModelVersion(&out->machine_model_version) &&
         data.ReadDisplayType(&out->display_type) &&
         data.ReadGlVersion(&out->gl_version) &&
         data.ReadGlVendor(&out->gl_vendor) &&
         data.ReadGlRenderer(&out->gl_renderer) &&
         data.ReadGlExtensions(&out->gl_extensions) &&
         data.ReadGlWsVendor(&out->gl_ws_vendor) &&
         data.ReadGlWsVersion(&out->gl_ws_version) &&
         data.ReadGlWsExtensions(&out->gl_ws_extensions) &&
         data.ReadGlImplementationParts(&out->gl_implementation_parts) &&
         data.ReadDirectRenderingVersion(&out->direct_rendering_version) &&
#if BUILDFLAG(IS_WIN)
         data.ReadOverlayInfo(&out->overlay_info) &&
#endif
         data.ReadVideoDecodeAcceleratorSupportedProfiles(
             &out->video_decode_accelerator_supported_profiles) &&
         data.ReadVideoEncodeAcceleratorSupportedProfiles(
             &out->video_encode_accelerator_supported_profiles) &&
         data.ReadImageDecodeAcceleratorSupportedProfiles(
             &out->image_decode_accelerator_supported_profiles) &&
#if BUILDFLAG(ENABLE_VULKAN)
         data.ReadVulkanInfo(&out->vulkan_info) &&
#endif
         true;
}

}  // namespace mojo
