// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/logging.h"
#include "base/notreached.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_util.h"

#if defined(OS_MAC)
#include <GLES2/gl2.h>
#include <GLES2/gl2extchromium.h>
#endif  // OS_MAC

namespace {

void EnumerateGPUDevice(const gpu::GPUInfo::GPUDevice& device,
                        gpu::GPUInfo::Enumerator* enumerator) {
  enumerator->BeginGPUDevice();
  enumerator->AddInt("vendorId", device.vendor_id);
  enumerator->AddInt("deviceId", device.device_id);
#if defined(OS_WIN) || defined(OS_CHROMEOS)
  enumerator->AddInt("revision", device.revision);
#endif
#if defined(OS_WIN)
  enumerator->AddInt("subSysId", device.sub_sys_id);
#endif  // OS_WIN
  enumerator->AddBool("active", device.active);
  enumerator->AddString("vendorString", device.vendor_string);
  enumerator->AddString("deviceString", device.device_string);
  enumerator->AddString("driverVendor", device.driver_vendor);
  enumerator->AddString("driverVersion", device.driver_version);
  enumerator->AddInt("cudaComputeCapabilityMajor",
                     device.cuda_compute_capability_major);
  enumerator->EndGPUDevice();
}

void EnumerateVideoDecodeAcceleratorSupportedProfile(
    const gpu::VideoDecodeAcceleratorSupportedProfile& profile,
    gpu::GPUInfo::Enumerator* enumerator) {
  enumerator->BeginVideoDecodeAcceleratorSupportedProfile();
  enumerator->AddInt("profile", profile.profile);
  enumerator->AddInt("maxResolutionWidth", profile.max_resolution.width());
  enumerator->AddInt("maxResolutionHeight", profile.max_resolution.height());
  enumerator->AddInt("minResolutionWidth", profile.min_resolution.width());
  enumerator->AddInt("minResolutionHeight", profile.min_resolution.height());
  enumerator->AddBool("encrypted_only", profile.encrypted_only);
  enumerator->EndVideoDecodeAcceleratorSupportedProfile();
}

void EnumerateVideoEncodeAcceleratorSupportedProfile(
    const gpu::VideoEncodeAcceleratorSupportedProfile& profile,
    gpu::GPUInfo::Enumerator* enumerator) {
  enumerator->BeginVideoEncodeAcceleratorSupportedProfile();
  enumerator->AddInt("profile", profile.profile);
  enumerator->AddInt("minResolutionWidth", profile.min_resolution.width());
  enumerator->AddInt("minResolutionHeight", profile.min_resolution.height());
  enumerator->AddInt("maxResolutionWidth", profile.max_resolution.width());
  enumerator->AddInt("maxResolutionHeight", profile.max_resolution.height());
  enumerator->AddInt("maxFramerateNumerator", profile.max_framerate_numerator);
  enumerator->AddInt("maxFramerateDenominator",
                     profile.max_framerate_denominator);
  enumerator->EndVideoEncodeAcceleratorSupportedProfile();
}

const char* ImageDecodeAcceleratorTypeToString(
    gpu::ImageDecodeAcceleratorType type) {
  switch (type) {
    case gpu::ImageDecodeAcceleratorType::kJpeg:
      return "JPEG";
    case gpu::ImageDecodeAcceleratorType::kWebP:
      return "WebP";
    case gpu::ImageDecodeAcceleratorType::kUnknown:
      return "Unknown";
  }
  NOTREACHED() << "Invalid ImageDecodeAcceleratorType.";
  return "";
}

const char* ImageDecodeAcceleratorSubsamplingToString(
    gpu::ImageDecodeAcceleratorSubsampling subsampling) {
  switch (subsampling) {
    case gpu::ImageDecodeAcceleratorSubsampling::k420:
      return "4:2:0";
    case gpu::ImageDecodeAcceleratorSubsampling::k422:
      return "4:2:2";
    case gpu::ImageDecodeAcceleratorSubsampling::k444:
      return "4:4:4";
  }
}

void EnumerateImageDecodeAcceleratorSupportedProfile(
    const gpu::ImageDecodeAcceleratorSupportedProfile& profile,
    gpu::GPUInfo::Enumerator* enumerator) {
  enumerator->BeginImageDecodeAcceleratorSupportedProfile();
  enumerator->AddString("imageType",
                        ImageDecodeAcceleratorTypeToString(profile.image_type));
  enumerator->AddString("minEncodedDimensions",
                        profile.min_encoded_dimensions.ToString());
  enumerator->AddString("maxEncodedDimensions",
                        profile.max_encoded_dimensions.ToString());
  std::string subsamplings;
  for (size_t i = 0; i < profile.subsamplings.size(); i++) {
    if (i > 0)
      subsamplings += ", ";
    subsamplings +=
        ImageDecodeAcceleratorSubsamplingToString(profile.subsamplings[i]);
  }
  enumerator->AddString("subsamplings", subsamplings);
  enumerator->EndImageDecodeAcceleratorSupportedProfile();
}

#if defined(OS_WIN)
void EnumerateOverlayInfo(const gpu::OverlayInfo& info,
                          gpu::GPUInfo::Enumerator* enumerator) {
  enumerator->BeginOverlayInfo();
  enumerator->AddBool("directComposition", info.direct_composition);
  enumerator->AddBool("supportsOverlays", info.supports_overlays);
  enumerator->AddString("yuy2OverlaySupport",
                        gpu::OverlaySupportToString(info.yuy2_overlay_support));
  enumerator->AddString("nv12OverlaySupport",
                        gpu::OverlaySupportToString(info.nv12_overlay_support));
  enumerator->EndOverlayInfo();
}
#endif

bool IsSoftwareRenderer(uint32_t vendor_id) {
  switch (vendor_id) {
    case 0x0000:  // Info collection failed to identify a GPU
    case 0xffff:  // Chromium internal flag for software rendering
    case 0x15ad:  // VMware
    case 0x1414:  // Microsoft software renderer
      return true;
    default:
      return false;
  }
}

}  // namespace

namespace gpu {

#if defined(OS_WIN)
const char* OverlaySupportToString(gpu::OverlaySupport support) {
  switch (support) {
    case gpu::OverlaySupport::kNone:
      return "NONE";
    case gpu::OverlaySupport::kDirect:
      return "DIRECT";
    case gpu::OverlaySupport::kScaling:
      return "SCALING";
    case gpu::OverlaySupport::kSoftware:
      return "SOFTWARE";
  }
}
#endif  // OS_WIN

#if defined(OS_MAC)
GPU_EXPORT bool ValidateMacOSSpecificTextureTarget(int target) {
  switch (target) {
    case GL_TEXTURE_2D:
    case GL_TEXTURE_RECTANGLE_ARB:
      return true;

    default:
      return false;
  }
}
#endif  // OS_MAC

VideoDecodeAcceleratorCapabilities::VideoDecodeAcceleratorCapabilities()
    : flags(0) {}

VideoDecodeAcceleratorCapabilities::VideoDecodeAcceleratorCapabilities(
    const VideoDecodeAcceleratorCapabilities& other) = default;

VideoDecodeAcceleratorCapabilities::~VideoDecodeAcceleratorCapabilities() =
    default;

ImageDecodeAcceleratorSupportedProfile::ImageDecodeAcceleratorSupportedProfile()
    : image_type(ImageDecodeAcceleratorType::kUnknown) {}

ImageDecodeAcceleratorSupportedProfile::ImageDecodeAcceleratorSupportedProfile(
    const ImageDecodeAcceleratorSupportedProfile& other) = default;

ImageDecodeAcceleratorSupportedProfile::ImageDecodeAcceleratorSupportedProfile(
    ImageDecodeAcceleratorSupportedProfile&& other) = default;

ImageDecodeAcceleratorSupportedProfile::
    ~ImageDecodeAcceleratorSupportedProfile() = default;

ImageDecodeAcceleratorSupportedProfile& ImageDecodeAcceleratorSupportedProfile::
operator=(const ImageDecodeAcceleratorSupportedProfile& other) = default;

ImageDecodeAcceleratorSupportedProfile& ImageDecodeAcceleratorSupportedProfile::
operator=(ImageDecodeAcceleratorSupportedProfile&& other) = default;

GPUInfo::GPUDevice::GPUDevice() = default;

GPUInfo::GPUDevice::GPUDevice(const GPUInfo::GPUDevice& other) = default;

GPUInfo::GPUDevice::GPUDevice(GPUInfo::GPUDevice&& other) noexcept = default;

GPUInfo::GPUDevice::~GPUDevice() noexcept = default;

GPUInfo::GPUDevice& GPUInfo::GPUDevice::operator=(
    const GPUInfo::GPUDevice& other) = default;

GPUInfo::GPUDevice& GPUInfo::GPUDevice::operator=(
    GPUInfo::GPUDevice&& other) noexcept = default;

GPUInfo::GPUInfo()
    : optimus(false),
      amd_switchable(false),
      gl_reset_notification_strategy(0),
      software_rendering(false),
      sandboxed(false),
      in_process_gpu(true),
      passthrough_cmd_decoder(false),
#if defined(OS_MAC)
      macos_specific_texture_target(gpu::GetPlatformSpecificTextureTarget()),
#endif  // OS_MAC
      jpeg_decode_accelerator_supported(false),
      oop_rasterization_supported(false),
      subpixel_font_rendering(true) {
}

GPUInfo::GPUInfo(const GPUInfo& other) = default;

GPUInfo::~GPUInfo() = default;

GPUInfo::GPUDevice& GPUInfo::active_gpu() {
  return const_cast<GPUInfo::GPUDevice&>(
      const_cast<const GPUInfo&>(*this).active_gpu());
}

const GPUInfo::GPUDevice& GPUInfo::active_gpu() const {
  if (gpu.active || secondary_gpus.empty())
    return gpu;
  for (const auto& secondary_gpu : secondary_gpus) {
    if (secondary_gpu.active)
      return secondary_gpu;
  }
  DVLOG(2) << "No active GPU found, returning primary GPU.";
  return gpu;
}

bool GPUInfo::IsInitialized() const {
  return gpu.vendor_id != 0 || !gl_vendor.empty();
}

bool GPUInfo::UsesSwiftShader() const {
  return gl_renderer.find("SwiftShader") != std::string::npos;
}

unsigned int GPUInfo::GpuCount() const {
  unsigned int gpu_count = 0;
  if (!IsSoftwareRenderer(gpu.vendor_id))
    ++gpu_count;
  for (const auto& secondary_gpu : secondary_gpus) {
    if (!IsSoftwareRenderer(secondary_gpu.vendor_id))
      ++gpu_count;
  }
  return gpu_count;
}

void GPUInfo::EnumerateFields(Enumerator* enumerator) const {
  struct GPUInfoKnownFields {
    base::TimeDelta initialization_time;
    bool optimus;
    bool amd_switchable;
    GPUDevice gpu;
    std::vector<GPUDevice> secondary_gpus;
    std::string pixel_shader_version;
    std::string vertex_shader_version;
    std::string max_msaa_samples;
    std::string machine_model_name;
    std::string machine_model_version;
    std::string gl_version_string;
    std::string gl_vendor;
    std::string gl_renderer;
    std::string gl_extensions;
    std::string gl_ws_vendor;
    std::string gl_ws_version;
    std::string gl_ws_extensions;
    uint32_t gl_reset_notification_strategy;
    bool software_rendering;
    std::string direct_rendering_version;
    bool sandboxed;
    bool in_process_gpu;
    bool passthrough_cmd_decoder;
    bool can_support_threaded_texture_mailbox;
#if defined(OS_MAC)
    uint32_t macos_specific_texture_target;
#endif  // OS_MAC
#if defined(OS_WIN)
    DxDiagNode dx_diagnostics;
    uint32_t d3d12_feature_level;
    uint32_t vulkan_version;
    OverlayInfo overlay_info;
#endif

    VideoDecodeAcceleratorCapabilities video_decode_accelerator_capabilities;
    VideoEncodeAcceleratorSupportedProfiles
        video_encode_accelerator_supported_profiles;
    bool jpeg_decode_accelerator_supported;

    ImageDecodeAcceleratorSupportedProfiles
        image_decode_accelerator_supported_profiles;

    bool oop_rasterization_supported;
    bool subpixel_font_rendering;

#if BUILDFLAG(ENABLE_VULKAN)
    base::Optional<VulkanInfo> vulkan_info;
#endif
  };

  // If this assert fails then most likely something below needs to be updated.
  // Note that this assert is only approximate. If a new field is added to
  // GPUInfo which fits within the current padding then it will not be caught.
  static_assert(
      sizeof(GPUInfo) == sizeof(GPUInfoKnownFields),
      "fields have changed in GPUInfo, GPUInfoKnownFields must be updated");

  // Required fields (according to DevTools protocol) first.
  enumerator->AddString("machineModelName", machine_model_name);
  enumerator->AddString("machineModelVersion", machine_model_version);
  EnumerateGPUDevice(gpu, enumerator);
  for (const auto& secondary_gpu : secondary_gpus)
    EnumerateGPUDevice(secondary_gpu, enumerator);

  enumerator->BeginAuxAttributes();
  enumerator->AddTimeDeltaInSecondsF("initializationTime", initialization_time);
  enumerator->AddBool("optimus", optimus);
  enumerator->AddBool("amdSwitchable", amd_switchable);
  enumerator->AddString("pixelShaderVersion", pixel_shader_version);
  enumerator->AddString("vertexShaderVersion", vertex_shader_version);
  enumerator->AddString("maxMsaaSamples", max_msaa_samples);
  enumerator->AddString("glVersion", gl_version);
  enumerator->AddString("glVendor", gl_vendor);
  enumerator->AddString("glRenderer", gl_renderer);
  enumerator->AddString("glExtensions", gl_extensions);
  enumerator->AddString("glWsVendor", gl_ws_vendor);
  enumerator->AddString("glWsVersion", gl_ws_version);
  enumerator->AddString("glWsExtensions", gl_ws_extensions);
  enumerator->AddInt(
      "glResetNotificationStrategy",
      static_cast<int>(gl_reset_notification_strategy));
  // TODO(kbr): add performance_stats.
  enumerator->AddBool("softwareRendering", software_rendering);
  enumerator->AddString("directRenderingVersion", direct_rendering_version);
  enumerator->AddBool("sandboxed", sandboxed);
  enumerator->AddBool("inProcessGpu", in_process_gpu);
  enumerator->AddBool("passthroughCmdDecoder", passthrough_cmd_decoder);
  enumerator->AddBool("canSupportThreadedTextureMailbox",
                      can_support_threaded_texture_mailbox);
#if defined(OS_MAC)
  enumerator->AddInt("macOSSpecificTextureTarget",
                     macos_specific_texture_target);
#endif  // OS_MAC
  // TODO(kbr): add dx_diagnostics on Windows.
#if defined(OS_WIN)
  EnumerateOverlayInfo(overlay_info, enumerator);
  enumerator->AddBool("supportsDx12", d3d12_feature_level != 0);
  enumerator->AddBool("supportsVulkan", vulkan_version != 0);
  enumerator->AddString("dx12FeatureLevel",
                        gpu::D3DFeatureLevelToString(d3d12_feature_level));
  enumerator->AddString("vulkanVersion",
                        gpu::VulkanVersionToString(vulkan_version));
#endif
  enumerator->AddInt("videoDecodeAcceleratorFlags",
                     video_decode_accelerator_capabilities.flags);

  // TODO(crbug.com/966839): Fix the two supported profile dumping below.
  for (const auto& profile :
       video_decode_accelerator_capabilities.supported_profiles)
    EnumerateVideoDecodeAcceleratorSupportedProfile(profile, enumerator);
  for (const auto& profile : video_encode_accelerator_supported_profiles)
    EnumerateVideoEncodeAcceleratorSupportedProfile(profile, enumerator);
  enumerator->AddBool("jpegDecodeAcceleratorSupported",
      jpeg_decode_accelerator_supported);
  for (const auto& profile : image_decode_accelerator_supported_profiles)
    EnumerateImageDecodeAcceleratorSupportedProfile(profile, enumerator);
  enumerator->AddBool("oopRasterizationSupported", oop_rasterization_supported);
  enumerator->AddBool("subpixelFontRendering", subpixel_font_rendering);
#if BUILDFLAG(ENABLE_VULKAN)
  if (vulkan_info) {
    auto blob = vulkan_info->Serialize();
    enumerator->AddBinary("vulkanInfo", base::span<const uint8_t>(blob));
  }
#endif
  enumerator->EndAuxAttributes();
}

}  // namespace gpu
