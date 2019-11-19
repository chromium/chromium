// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_INFO_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_GPU_INFO_MOJOM_TRAITS_H_

#include "base/time/time.h"
#include "build/build_config.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/common/dx_diag_node_mojom_traits.h"
#include "gpu/ipc/common/gpu_info.mojom.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<gpu::mojom::GpuDeviceDataView, gpu::GPUInfo::GPUDevice> {
  static bool Read(gpu::mojom::GpuDeviceDataView data,
                   gpu::GPUInfo::GPUDevice* out);

  static uint32_t vendor_id(const gpu::GPUInfo::GPUDevice& input) {
    return input.vendor_id;
  }

  static uint32_t device_id(const gpu::GPUInfo::GPUDevice& input) {
    return input.device_id;
  }

#if defined(OS_WIN)
  static uint32_t sub_sys_id(const gpu::GPUInfo::GPUDevice& input) {
    return input.sub_sys_id;
  }

  static uint32_t revision(const gpu::GPUInfo::GPUDevice& input) {
    return input.revision;
  }
#endif  // OS_WIN

  static bool active(const gpu::GPUInfo::GPUDevice& input) {
    return input.active;
  }

  static const std::string& vendor_string(
      const gpu::GPUInfo::GPUDevice& input) {
    return input.vendor_string;
  }

  static const std::string& device_string(
      const gpu::GPUInfo::GPUDevice& input) {
    return input.device_string;
  }

  static const std::string& driver_vendor(
      const gpu::GPUInfo::GPUDevice& input) {
    return input.driver_vendor;
  }

  static const std::string& driver_version(
      const gpu::GPUInfo::GPUDevice& input) {
    return input.driver_version;
  }

  static int cuda_compute_capability_major(
      const gpu::GPUInfo::GPUDevice& input) {
    return input.cuda_compute_capability_major;
  }
};

template <>
struct EnumTraits<gpu::mojom::VideoCodecProfile, gpu::VideoCodecProfile> {
  static gpu::mojom::VideoCodecProfile ToMojom(
      gpu::VideoCodecProfile video_codec_profile);
  static bool FromMojom(gpu::mojom::VideoCodecProfile input,
                        gpu::VideoCodecProfile* out);
};

template <>
struct StructTraits<gpu::mojom::VideoDecodeAcceleratorSupportedProfileDataView,
                    gpu::VideoDecodeAcceleratorSupportedProfile> {
  static bool Read(
      gpu::mojom::VideoDecodeAcceleratorSupportedProfileDataView data,
      gpu::VideoDecodeAcceleratorSupportedProfile* out);

  static gpu::VideoCodecProfile profile(
      const gpu::VideoDecodeAcceleratorSupportedProfile& input) {
    return input.profile;
  }

  static const gfx::Size& max_resolution(
      const gpu::VideoDecodeAcceleratorSupportedProfile& input) {
    return input.max_resolution;
  }

  static const gfx::Size& min_resolution(
      const gpu::VideoDecodeAcceleratorSupportedProfile& input) {
    return input.min_resolution;
  }

  static bool encrypted_only(
      const gpu::VideoDecodeAcceleratorSupportedProfile& input) {
    return input.encrypted_only;
  }
};

template <>
struct StructTraits<gpu::mojom::VideoDecodeAcceleratorCapabilitiesDataView,
                    gpu::VideoDecodeAcceleratorCapabilities> {
  static bool Read(gpu::mojom::VideoDecodeAcceleratorCapabilitiesDataView data,
                   gpu::VideoDecodeAcceleratorCapabilities* out);

  static uint32_t flags(const gpu::VideoDecodeAcceleratorCapabilities& input) {
    return input.flags;
  }

  static std::vector<gpu::VideoDecodeAcceleratorSupportedProfile>
      supported_profiles(const gpu::VideoDecodeAcceleratorCapabilities& input) {
    return input.supported_profiles;
  }
};

template <>
struct StructTraits<gpu::mojom::VideoEncodeAcceleratorSupportedProfileDataView,
                    gpu::VideoEncodeAcceleratorSupportedProfile> {
  static bool Read(
      gpu::mojom::VideoEncodeAcceleratorSupportedProfileDataView data,
      gpu::VideoEncodeAcceleratorSupportedProfile* out);

  static gpu::VideoCodecProfile profile(
      const gpu::VideoEncodeAcceleratorSupportedProfile& input) {
    return input.profile;
  }

  static const gfx::Size& min_resolution(
      const gpu::VideoEncodeAcceleratorSupportedProfile& input) {
    return input.min_resolution;
  }

  static const gfx::Size& max_resolution(
      const gpu::VideoEncodeAcceleratorSupportedProfile& input) {
    return input.max_resolution;
  }

  static uint32_t max_framerate_numerator(
      const gpu::VideoEncodeAcceleratorSupportedProfile& input) {
    return input.max_framerate_numerator;
  }

  static uint32_t max_framerate_denominator(
      const gpu::VideoEncodeAcceleratorSupportedProfile& input) {
    return input.max_framerate_denominator;
  }
};

template <>
struct EnumTraits<gpu::mojom::ImageDecodeAcceleratorType,
                  gpu::ImageDecodeAcceleratorType> {
  static gpu::mojom::ImageDecodeAcceleratorType ToMojom(
      gpu::ImageDecodeAcceleratorType image_type);
  static bool FromMojom(gpu::mojom::ImageDecodeAcceleratorType input,
                        gpu::ImageDecodeAcceleratorType* out);
};

template <>
struct EnumTraits<gpu::mojom::ImageDecodeAcceleratorSubsampling,
                  gpu::ImageDecodeAcceleratorSubsampling> {
  static gpu::mojom::ImageDecodeAcceleratorSubsampling ToMojom(
      gpu::ImageDecodeAcceleratorSubsampling subsampling);
  static bool FromMojom(gpu::mojom::ImageDecodeAcceleratorSubsampling input,
                        gpu::ImageDecodeAcceleratorSubsampling* out);
};

template <>
struct StructTraits<gpu::mojom::ImageDecodeAcceleratorSupportedProfileDataView,
                    gpu::ImageDecodeAcceleratorSupportedProfile> {
  static bool Read(
      gpu::mojom::ImageDecodeAcceleratorSupportedProfileDataView data,
      gpu::ImageDecodeAcceleratorSupportedProfile* out);

  static gpu::ImageDecodeAcceleratorType image_type(
      const gpu::ImageDecodeAcceleratorSupportedProfile& input) {
    return input.image_type;
  }

  static const gfx::Size& min_encoded_dimensions(
      const gpu::ImageDecodeAcceleratorSupportedProfile& input) {
    return input.min_encoded_dimensions;
  }

  static const gfx::Size& max_encoded_dimensions(
      const gpu::ImageDecodeAcceleratorSupportedProfile& input) {
    return input.max_encoded_dimensions;
  }

  static std::vector<gpu::ImageDecodeAcceleratorSubsampling> subsamplings(
      const gpu::ImageDecodeAcceleratorSupportedProfile& input) {
    return input.subsamplings;
  }
};

#if defined(OS_WIN)
template <>
struct EnumTraits<gpu::mojom::OverlaySupport, gpu::OverlaySupport> {
  static gpu::mojom::OverlaySupport ToMojom(gpu::OverlaySupport support);
  static bool FromMojom(gpu::mojom::OverlaySupport input,
                        gpu::OverlaySupport* out);
};

template <>
struct StructTraits<gpu::mojom::Dx12VulkanVersionInfoDataView,
                    gpu::Dx12VulkanVersionInfo> {
  static bool Read(gpu::mojom::Dx12VulkanVersionInfoDataView data,
                   gpu::Dx12VulkanVersionInfo* out);

  static bool supports_dx12(const gpu::Dx12VulkanVersionInfo& input) {
    return input.supports_dx12;
  }

  static bool supports_vulkan(const gpu::Dx12VulkanVersionInfo& input) {
    return input.supports_vulkan;
  }

  static uint32_t d3d12_feature_level(const gpu::Dx12VulkanVersionInfo& input) {
    return input.d3d12_feature_level;
  }

  static uint32_t vulkan_version(const gpu::Dx12VulkanVersionInfo& input) {
    return input.vulkan_version;
  }
};
#endif

template <>
struct StructTraits<gpu::mojom::GpuInfoDataView, gpu::GPUInfo> {
  static bool Read(gpu::mojom::GpuInfoDataView data, gpu::GPUInfo* out);

  static base::TimeDelta initialization_time(const gpu::GPUInfo& input) {
    return input.initialization_time;
  }

  static bool optimus(const gpu::GPUInfo& input) { return input.optimus; }

  static bool amd_switchable(const gpu::GPUInfo& input) {
    return input.amd_switchable;
  }

  static const gpu::GPUInfo::GPUDevice& gpu(const gpu::GPUInfo& input) {
    return input.gpu;
  }

  static const std::vector<gpu::GPUInfo::GPUDevice>& secondary_gpus(
      const gpu::GPUInfo& input) {
    return input.secondary_gpus;
  }

  static const std::string& pixel_shader_version(const gpu::GPUInfo& input) {
    return input.pixel_shader_version;
  }

  static const std::string& vertex_shader_version(const gpu::GPUInfo& input) {
    return input.vertex_shader_version;
  }

  static const std::string& max_msaa_samples(const gpu::GPUInfo& input) {
    return input.max_msaa_samples;
  }

  static const std::string& machine_model_name(const gpu::GPUInfo& input) {
    return input.machine_model_name;
  }

  static const std::string& machine_model_version(const gpu::GPUInfo& input) {
    return input.machine_model_version;
  }

  static const std::string& gl_version(const gpu::GPUInfo& input) {
    return input.gl_version;
  }

  static const std::string& gl_vendor(const gpu::GPUInfo& input) {
    return input.gl_vendor;
  }

  static const std::string& gl_renderer(const gpu::GPUInfo& input) {
    return input.gl_renderer;
  }

  static const std::string& gl_extensions(const gpu::GPUInfo& input) {
    return input.gl_extensions;
  }

  static const std::string& gl_ws_vendor(const gpu::GPUInfo& input) {
    return input.gl_ws_vendor;
  }

  static const std::string& gl_ws_version(const gpu::GPUInfo& input) {
    return input.gl_ws_version;
  }

  static const std::string& gl_ws_extensions(const gpu::GPUInfo& input) {
    return input.gl_ws_extensions;
  }

  static uint32_t gl_reset_notification_strategy(const gpu::GPUInfo& input) {
    return input.gl_reset_notification_strategy;
  }

  static bool software_rendering(const gpu::GPUInfo& input) {
    return input.software_rendering;
  }

  static const std::string& direct_rendering_version(
      const gpu::GPUInfo& input) {
    return input.direct_rendering_version;
  }

  static bool sandboxed(const gpu::GPUInfo& input) { return input.sandboxed; }

  static bool in_process_gpu(const gpu::GPUInfo& input) {
    return input.in_process_gpu;
  }

  static bool passthrough_cmd_decoder(const gpu::GPUInfo& input) {
    return input.passthrough_cmd_decoder;
  }

  static bool can_support_threaded_texture_mailbox(const gpu::GPUInfo& input) {
    return input.can_support_threaded_texture_mailbox;
  }

#if defined(OS_WIN)
  static bool direct_composition(const gpu::GPUInfo& input) {
    return input.direct_composition;
  }

  static bool supports_overlays(const gpu::GPUInfo& input) {
    return input.supports_overlays;
  }

  static gpu::OverlaySupport yuy2_overlay_support(const gpu::GPUInfo& input) {
    return input.yuy2_overlay_support;
  }

  static gpu::OverlaySupport nv12_overlay_support(const gpu::GPUInfo& input) {
    return input.nv12_overlay_support;
  }

  static const gpu::DxDiagNode& dx_diagnostics(const gpu::GPUInfo& input) {
    return input.dx_diagnostics;
  }

  static const gpu::Dx12VulkanVersionInfo& dx12_vulkan_version_info(
      const gpu::GPUInfo& input) {
    return input.dx12_vulkan_version_info;
  }
#endif

  static const gpu::VideoDecodeAcceleratorCapabilities&
  video_decode_accelerator_capabilities(const gpu::GPUInfo& input) {
    return input.video_decode_accelerator_capabilities;
  }

  static std::vector<gpu::VideoEncodeAcceleratorSupportedProfile>
  video_encode_accelerator_supported_profiles(const gpu::GPUInfo& input) {
    return input.video_encode_accelerator_supported_profiles;
  }

  static bool jpeg_decode_accelerator_supported(const gpu::GPUInfo& input) {
    return input.jpeg_decode_accelerator_supported;
  }

  static std::vector<gpu::ImageDecodeAcceleratorSupportedProfile>
  image_decode_accelerator_supported_profiles(const gpu::GPUInfo& input) {
    return input.image_decode_accelerator_supported_profiles;
  }

  static uint64_t system_visual(const gpu::GPUInfo& input) {
#if defined(USE_X11)
    return input.system_visual;
#endif
    return 0;
  }

  static uint64_t rgba_visual(const gpu::GPUInfo& input) {
#if defined(USE_X11)
    return input.rgba_visual;
#endif
    return 0;
  }

  static bool oop_rasterization_supported(const gpu::GPUInfo& input) {
    return input.oop_rasterization_supported;
  }

  static bool subpixel_font_rendering(const gpu::GPUInfo& input) {
    return input.subpixel_font_rendering;
  }

#if BUILDFLAG(ENABLE_VULKAN)
  static const base::Optional<gpu::VulkanInfo> vulkan_info(
      const gpu::GPUInfo& input) {
    return input.vulkan_info;
  }
#endif
};

}  // namespace mojo
#endif  // GPU_IPC_COMMON_GPU_INFO_MOJOM_TRAITS_H_
