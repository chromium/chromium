// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_info_struct_traits.h"
#include "build/build_config.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<gpu::mojom::GpuDeviceDataView, gpu::GPUInfo::GPUDevice>::Read(
    gpu::mojom::GpuDeviceDataView data,
    gpu::GPUInfo::GPUDevice* out) {
  out->vendor_id = data.vendor_id();
  out->device_id = data.device_id();
  out->active = data.active();
  out->cuda_compute_capability_major = data.cuda_compute_capability_major();
  return data.ReadVendorString(&out->vendor_string) &&
         data.ReadDeviceString(&out->device_string) &&
         data.ReadDriverVendor(&out->driver_vendor) &&
         data.ReadDriverVersion(&out->driver_version) &&
         data.ReadDriverDate(&out->driver_date);
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
    case gpu::VideoCodecProfile::DOLBYVISION_PROFILE0:
      return gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE0;
    case gpu::VideoCodecProfile::DOLBYVISION_PROFILE4:
      return gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE4;
    case gpu::VideoCodecProfile::DOLBYVISION_PROFILE5:
      return gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE5;
    case gpu::VideoCodecProfile::DOLBYVISION_PROFILE7:
      return gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE7;
    case gpu::VideoCodecProfile::THEORAPROFILE_ANY:
      return gpu::mojom::VideoCodecProfile::THEORAPROFILE_ANY;
    case gpu::VideoCodecProfile::AV1PROFILE_PROFILE_MAIN:
      return gpu::mojom::VideoCodecProfile::AV1PROFILE_PROFILE_MAIN;
    case gpu::VideoCodecProfile::AV1PROFILE_PROFILE_HIGH:
      return gpu::mojom::VideoCodecProfile::AV1PROFILE_PROFILE_HIGH;
    case gpu::VideoCodecProfile::AV1PROFILE_PROFILE_PRO:
      return gpu::mojom::VideoCodecProfile::AV1PROFILE_PROFILE_PRO;
  }
  NOTREACHED() << "Invalid VideoCodecProfile:" << video_codec_profile;
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
    case gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE0:
      *out = gpu::VideoCodecProfile::DOLBYVISION_PROFILE0;
      return true;
    case gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE4:
      *out = gpu::VideoCodecProfile::DOLBYVISION_PROFILE4;
      return true;
    case gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE5:
      *out = gpu::VideoCodecProfile::DOLBYVISION_PROFILE5;
      return true;
    case gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE7:
      *out = gpu::VideoCodecProfile::DOLBYVISION_PROFILE7;
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
  }
  NOTREACHED() << "Invalid VideoCodecProfile: " << input;
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
  return data.ReadProfile(&out->profile) &&
         data.ReadMaxResolution(&out->max_resolution);
}

#if defined(OS_WIN)
// static
gpu::mojom::OverlayFormat
EnumTraits<gpu::mojom::OverlayFormat, gpu::OverlayFormat>::ToMojom(
    gpu::OverlayFormat format) {
  switch (format) {
    case gpu::OverlayFormat::kBGRA:
      return gpu::mojom::OverlayFormat::BGRA;
    case gpu::OverlayFormat::kYUY2:
      return gpu::mojom::OverlayFormat::YUY2;
    case gpu::OverlayFormat::kNV12:
      return gpu::mojom::OverlayFormat::NV12;
  }
}

bool EnumTraits<gpu::mojom::OverlayFormat, gpu::OverlayFormat>::FromMojom(
    gpu::mojom::OverlayFormat input,
    gpu::OverlayFormat* out) {
  switch (input) {
    case gpu::mojom::OverlayFormat::BGRA:
      *out = gpu::OverlayFormat::kBGRA;
      break;
    case gpu::mojom::OverlayFormat::YUY2:
      *out = gpu::OverlayFormat::kYUY2;
      break;
    case gpu::mojom::OverlayFormat::NV12:
      *out = gpu::OverlayFormat::kNV12;
      break;
  }
  return true;
}

// static
bool StructTraits<
    gpu::mojom::OverlayCapabilityDataView,
    gpu::OverlayCapability>::Read(gpu::mojom::OverlayCapabilityDataView data,
                                  gpu::OverlayCapability* out) {
  out->is_scaling_supported = data.is_scaling_supported();
  return data.ReadFormat(&out->format);
}

// static
bool StructTraits<gpu::mojom::Dx12VulkanVersionInfoDataView,
                  gpu::Dx12VulkanVersionInfo>::
    Read(gpu::mojom::Dx12VulkanVersionInfoDataView data,
         gpu::Dx12VulkanVersionInfo* out) {
  out->supports_dx12 = data.supports_dx12();
  out->supports_vulkan = data.supports_vulkan();
  out->d3d12_feature_level = data.d3d12_feature_level();
  out->vulkan_version = data.vulkan_version();
  return true;
}
#endif

bool StructTraits<gpu::mojom::GpuInfoDataView, gpu::GPUInfo>::Read(
    gpu::mojom::GpuInfoDataView data,
    gpu::GPUInfo* out) {
  out->optimus = data.optimus();
  out->amd_switchable = data.amd_switchable();
  out->gl_reset_notification_strategy = data.gl_reset_notification_strategy();
  out->software_rendering = data.software_rendering();
  out->direct_rendering = data.direct_rendering();
  out->sandboxed = data.sandboxed();
  out->in_process_gpu = data.in_process_gpu();
  out->passthrough_cmd_decoder = data.passthrough_cmd_decoder();
  out->can_support_threaded_texture_mailbox =
      data.can_support_threaded_texture_mailbox();
  out->jpeg_decode_accelerator_supported =
      data.jpeg_decode_accelerator_supported();

#if defined(USE_X11)
  out->system_visual = data.system_visual();
  out->rgba_visual = data.rgba_visual();
#endif
  out->oop_rasterization_supported = data.oop_rasterization_supported();

#if defined(OS_WIN)
  out->direct_composition_overlays = data.direct_composition_overlays();
#endif

  return data.ReadInitializationTime(&out->initialization_time) &&
         data.ReadGpu(&out->gpu) &&
         data.ReadSecondaryGpus(&out->secondary_gpus) &&
         data.ReadPixelShaderVersion(&out->pixel_shader_version) &&
         data.ReadVertexShaderVersion(&out->vertex_shader_version) &&
         data.ReadMaxMsaaSamples(&out->max_msaa_samples) &&
         data.ReadMachineModelName(&out->machine_model_name) &&
         data.ReadMachineModelVersion(&out->machine_model_version) &&
         data.ReadGlVersion(&out->gl_version) &&
         data.ReadGlVendor(&out->gl_vendor) &&
         data.ReadGlRenderer(&out->gl_renderer) &&
         data.ReadGlExtensions(&out->gl_extensions) &&
         data.ReadGlWsVendor(&out->gl_ws_vendor) &&
         data.ReadGlWsVersion(&out->gl_ws_version) &&
         data.ReadGlWsExtensions(&out->gl_ws_extensions) &&
#if defined(OS_WIN)
         data.ReadOverlayCapabilities(&out->overlay_capabilities) &&
         data.ReadDxDiagnostics(&out->dx_diagnostics) &&
         data.ReadDx12VulkanVersionInfo(&out->dx12_vulkan_version_info) &&
#endif
         data.ReadVideoDecodeAcceleratorCapabilities(
             &out->video_decode_accelerator_capabilities) &&
         data.ReadVideoEncodeAcceleratorSupportedProfiles(
             &out->video_encode_accelerator_supported_profiles);
}

}  // namespace mojo
