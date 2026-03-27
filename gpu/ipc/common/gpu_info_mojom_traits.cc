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
gpu::mojom::SkiaBackendType
EnumTraits<gpu::mojom::SkiaBackendType, gpu::SkiaBackendType>::ToMojom(
    gpu::SkiaBackendType type) {
  switch (type) {
    case gpu::SkiaBackendType::kUnknown:
      return gpu::mojom::SkiaBackendType::kUnknown;
    case gpu::SkiaBackendType::kNone:
      return gpu::mojom::SkiaBackendType::kNone;
    case gpu::SkiaBackendType::kGaneshGL:
      return gpu::mojom::SkiaBackendType::kGaneshGL;
    case gpu::SkiaBackendType::kGaneshVulkan:
      return gpu::mojom::SkiaBackendType::kGaneshVulkan;
    case gpu::SkiaBackendType::kGraphiteDawnVulkan:
      return gpu::mojom::SkiaBackendType::kGraphiteDawnVulkan;
    case gpu::SkiaBackendType::kGraphiteDawnMetal:
      return gpu::mojom::SkiaBackendType::kGraphiteDawnMetal;
    case gpu::SkiaBackendType::kGraphiteDawnD3D11:
      return gpu::mojom::SkiaBackendType::kGraphiteDawnD3D11;
    case gpu::SkiaBackendType::kGraphiteDawnD3D12:
      return gpu::mojom::SkiaBackendType::kGraphiteDawnD3D12;
    case gpu::SkiaBackendType::kGraphiteDawnOpenGLES:
      return gpu::mojom::SkiaBackendType::kGraphiteDawnOpenGLES;
  }
  NOTREACHED() << "Invalid SkiaBackendType:" << static_cast<int>(type);
}

// static
gpu::SkiaBackendType
EnumTraits<gpu::mojom::SkiaBackendType, gpu::SkiaBackendType>::FromMojom(
    gpu::mojom::SkiaBackendType input) {
  switch (input) {
    case gpu::mojom::SkiaBackendType::kUnknown:
      return gpu::SkiaBackendType::kUnknown;
    case gpu::mojom::SkiaBackendType::kNone:
      return gpu::SkiaBackendType::kNone;
    case gpu::mojom::SkiaBackendType::kGaneshGL:
      return gpu::SkiaBackendType::kGaneshGL;
    case gpu::mojom::SkiaBackendType::kGaneshVulkan:
      return gpu::SkiaBackendType::kGaneshVulkan;
    case gpu::mojom::SkiaBackendType::kGraphiteDawnVulkan:
      return gpu::SkiaBackendType::kGraphiteDawnVulkan;
    case gpu::mojom::SkiaBackendType::kGraphiteDawnMetal:
      return gpu::SkiaBackendType::kGraphiteDawnMetal;
    case gpu::mojom::SkiaBackendType::kGraphiteDawnD3D11:
      return gpu::SkiaBackendType::kGraphiteDawnD3D11;
    case gpu::mojom::SkiaBackendType::kGraphiteDawnD3D12:
      return gpu::SkiaBackendType::kGraphiteDawnD3D12;
    case gpu::mojom::SkiaBackendType::kGraphiteDawnOpenGLES:
      return gpu::SkiaBackendType::kGraphiteDawnOpenGLES;
  }
  NOTREACHED() << "Invalid SkiaBackendType: " << input;
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
    case gpu::VideoCodecProfile::DOLBYVISION_PROFILE10:
      return gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE10;
    case gpu::VideoCodecProfile::DOLBYVISION_PROFILE20:
      return gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE20;
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
}

// static
gpu::VideoCodecProfile
EnumTraits<gpu::mojom::VideoCodecProfile, gpu::VideoCodecProfile>::FromMojom(
    gpu::mojom::VideoCodecProfile input) {
  switch (input) {
    case gpu::mojom::VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN:
      return gpu::VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_BASELINE:
      return gpu::VideoCodecProfile::H264PROFILE_BASELINE;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_MAIN:
      return gpu::VideoCodecProfile::H264PROFILE_MAIN;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_EXTENDED:
      return gpu::VideoCodecProfile::H264PROFILE_EXTENDED;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_HIGH:
      return gpu::VideoCodecProfile::H264PROFILE_HIGH;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_HIGH10PROFILE:
      return gpu::VideoCodecProfile::H264PROFILE_HIGH10PROFILE;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_HIGH422PROFILE:
      return gpu::VideoCodecProfile::H264PROFILE_HIGH422PROFILE;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return gpu::VideoCodecProfile::H264PROFILE_HIGH444PREDICTIVEPROFILE;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_SCALABLEBASELINE:
      return gpu::VideoCodecProfile::H264PROFILE_SCALABLEBASELINE;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_SCALABLEHIGH:
      return gpu::VideoCodecProfile::H264PROFILE_SCALABLEHIGH;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_STEREOHIGH:
      return gpu::VideoCodecProfile::H264PROFILE_STEREOHIGH;
    case gpu::mojom::VideoCodecProfile::H264PROFILE_MULTIVIEWHIGH:
      return gpu::VideoCodecProfile::H264PROFILE_MULTIVIEWHIGH;
    case gpu::mojom::VideoCodecProfile::VP8PROFILE_ANY:
      return gpu::VideoCodecProfile::VP8PROFILE_ANY;
    case gpu::mojom::VideoCodecProfile::VP9PROFILE_PROFILE0:
      return gpu::VideoCodecProfile::VP9PROFILE_PROFILE0;
    case gpu::mojom::VideoCodecProfile::VP9PROFILE_PROFILE1:
      return gpu::VideoCodecProfile::VP9PROFILE_PROFILE1;
    case gpu::mojom::VideoCodecProfile::VP9PROFILE_PROFILE2:
      return gpu::VideoCodecProfile::VP9PROFILE_PROFILE2;
    case gpu::mojom::VideoCodecProfile::VP9PROFILE_PROFILE3:
      return gpu::VideoCodecProfile::VP9PROFILE_PROFILE3;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_MAIN:
      return gpu::VideoCodecProfile::HEVCPROFILE_MAIN;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_MAIN10:
      return gpu::VideoCodecProfile::HEVCPROFILE_MAIN10;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_MAIN_STILL_PICTURE:
      return gpu::VideoCodecProfile::HEVCPROFILE_MAIN_STILL_PICTURE;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_REXT:
      return gpu::VideoCodecProfile::HEVCPROFILE_REXT;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_HIGH_THROUGHPUT:
      return gpu::VideoCodecProfile::HEVCPROFILE_HIGH_THROUGHPUT;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_MULTIVIEW_MAIN:
      return gpu::VideoCodecProfile::HEVCPROFILE_MULTIVIEW_MAIN;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_SCALABLE_MAIN:
      return gpu::VideoCodecProfile::HEVCPROFILE_SCALABLE_MAIN;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_3D_MAIN:
      return gpu::VideoCodecProfile::HEVCPROFILE_3D_MAIN;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_SCREEN_EXTENDED:
      return gpu::VideoCodecProfile::HEVCPROFILE_SCREEN_EXTENDED;
    case gpu::mojom::VideoCodecProfile::HEVCPROFILE_SCALABLE_REXT:
      return gpu::VideoCodecProfile::HEVCPROFILE_SCALABLE_REXT;
    case gpu::mojom::VideoCodecProfile::
        HEVCPROFILE_HIGH_THROUGHPUT_SCREEN_EXTENDED:
      return gpu::VideoCodecProfile::
          HEVCPROFILE_HIGH_THROUGHPUT_SCREEN_EXTENDED;
    case gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE0:
      return gpu::VideoCodecProfile::DOLBYVISION_PROFILE0;
    case gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE5:
      return gpu::VideoCodecProfile::DOLBYVISION_PROFILE5;
    case gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE7:
      return gpu::VideoCodecProfile::DOLBYVISION_PROFILE7;
    case gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE8:
      return gpu::VideoCodecProfile::DOLBYVISION_PROFILE8;
    case gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE9:
      return gpu::VideoCodecProfile::DOLBYVISION_PROFILE9;
    case gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE10:
      return gpu::VideoCodecProfile::DOLBYVISION_PROFILE10;
    case gpu::mojom::VideoCodecProfile::DOLBYVISION_PROFILE20:
      return gpu::VideoCodecProfile::DOLBYVISION_PROFILE20;
    case gpu::mojom::VideoCodecProfile::THEORAPROFILE_ANY:
      return gpu::VideoCodecProfile::THEORAPROFILE_ANY;
    case gpu::mojom::VideoCodecProfile::AV1PROFILE_PROFILE_MAIN:
      return gpu::VideoCodecProfile::AV1PROFILE_PROFILE_MAIN;
    case gpu::mojom::VideoCodecProfile::AV1PROFILE_PROFILE_HIGH:
      return gpu::VideoCodecProfile::AV1PROFILE_PROFILE_HIGH;
    case gpu::mojom::VideoCodecProfile::AV1PROFILE_PROFILE_PRO:
      return gpu::VideoCodecProfile::AV1PROFILE_PROFILE_PRO;
  }
  NOTREACHED() << "Invalid VideoCodecProfile: " << input;
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
  NOTREACHED() << "Invalid OverlaySupport: " << static_cast<int>(support);
}

gpu::OverlaySupport
EnumTraits<gpu::mojom::OverlaySupport, gpu::OverlaySupport>::FromMojom(
    gpu::mojom::OverlaySupport input) {
  switch (input) {
    case gpu::mojom::OverlaySupport::NONE:
      return gpu::OverlaySupport::kNone;
    case gpu::mojom::OverlaySupport::DIRECT:
      return gpu::OverlaySupport::kDirect;
    case gpu::mojom::OverlaySupport::SCALING:
      return gpu::OverlaySupport::kScaling;
    case gpu::mojom::OverlaySupport::SOFTWARE:
      return gpu::OverlaySupport::kSoftware;
  }
  NOTREACHED() << "Invalid OverlaySupport: " << input;
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
         data.ReadSkiaBackendType(&out->skia_backend_type) &&
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
#if BUILDFLAG(ENABLE_VULKAN)
         data.ReadVulkanInfo(&out->vulkan_info) &&
#endif
         true;
}

}  // namespace mojo
