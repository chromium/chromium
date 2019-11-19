// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_INFO_H_
#define GPU_CONFIG_GPU_INFO_H_

// Provides access to the GPU information for the system
// on which chrome is currently running.

#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "gpu/config/dx_diag_node.h"
#include "gpu/gpu_export.h"
#include "gpu/vulkan/buildflags.h"
#include "ui/gfx/geometry/size.h"

#if defined(USE_X11)
typedef unsigned long VisualID;
#endif

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/config/vulkan_info.h"
#endif

namespace gpu {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GpuSeriesType {
  kUnknown = 0,
  // Intel 6th gen
  kIntelSandybridge = 1,
  // Intel 7th gen
  kIntelBaytrail = 2,
  kIntelIvybridge = 3,
  kIntelHaswell = 4,
  // Intel 8th gen
  kIntelCherrytrail = 5,
  kIntelBroadwell = 6,
  // Intel 9th gen
  kIntelApollolake = 7,
  kIntelSkylake = 8,
  kIntelGeminilake = 9,
  kIntelKabylake = 10,
  kIntelCoffeelake = 11,
  kIntelWhiskeylake = 12,
  kIntelCometlake = 13,
  // Intel 10th gen
  kIntelCannonlake = 14,
  // Intel 11th gen
  kIntelIcelake = 15,
  // Please also update |gpu_series_map| in process_json.py.
  kMaxValue = kIntelIcelake,
};

// Video profile.  This *must* match media::VideoCodecProfile.
enum VideoCodecProfile {
  VIDEO_CODEC_PROFILE_UNKNOWN = -1,
  VIDEO_CODEC_PROFILE_MIN = VIDEO_CODEC_PROFILE_UNKNOWN,
  H264PROFILE_BASELINE = 0,
  H264PROFILE_MAIN,
  H264PROFILE_EXTENDED,
  H264PROFILE_HIGH,
  H264PROFILE_HIGH10PROFILE,
  H264PROFILE_HIGH422PROFILE,
  H264PROFILE_HIGH444PREDICTIVEPROFILE,
  H264PROFILE_SCALABLEBASELINE,
  H264PROFILE_SCALABLEHIGH,
  H264PROFILE_STEREOHIGH,
  H264PROFILE_MULTIVIEWHIGH,
  VP8PROFILE_ANY,
  VP9PROFILE_PROFILE0,
  VP9PROFILE_PROFILE1,
  VP9PROFILE_PROFILE2,
  VP9PROFILE_PROFILE3,
  HEVCPROFILE_MAIN,
  HEVCPROFILE_MAIN10,
  HEVCPROFILE_MAIN_STILL_PICTURE,
  DOLBYVISION_PROFILE0,
  DOLBYVISION_PROFILE4,
  DOLBYVISION_PROFILE5,
  DOLBYVISION_PROFILE7,
  THEORAPROFILE_ANY,
  AV1PROFILE_PROFILE_MAIN,
  AV1PROFILE_PROFILE_HIGH,
  AV1PROFILE_PROFILE_PRO,
  DOLBYVISION_PROFILE8,
  DOLBYVISION_PROFILE9,
  VIDEO_CODEC_PROFILE_MAX = DOLBYVISION_PROFILE9,
};

// Specification of a decoding profile supported by a hardware decoder.
struct GPU_EXPORT VideoDecodeAcceleratorSupportedProfile {
  VideoCodecProfile profile;
  gfx::Size max_resolution;
  gfx::Size min_resolution;
  bool encrypted_only;
};

using VideoDecodeAcceleratorSupportedProfiles =
    std::vector<VideoDecodeAcceleratorSupportedProfile>;

struct GPU_EXPORT VideoDecodeAcceleratorCapabilities {
  VideoDecodeAcceleratorCapabilities();
  VideoDecodeAcceleratorCapabilities(
      const VideoDecodeAcceleratorCapabilities& other);
  ~VideoDecodeAcceleratorCapabilities();
  VideoDecodeAcceleratorSupportedProfiles supported_profiles;
  uint32_t flags;
};

// Specification of an encoding profile supported by a hardware encoder.
struct GPU_EXPORT VideoEncodeAcceleratorSupportedProfile {
  VideoCodecProfile profile;
  gfx::Size min_resolution;
  gfx::Size max_resolution;
  uint32_t max_framerate_numerator;
  uint32_t max_framerate_denominator;
};
using VideoEncodeAcceleratorSupportedProfiles =
    std::vector<VideoEncodeAcceleratorSupportedProfile>;

enum class ImageDecodeAcceleratorType {
  kUnknown = 0,
  kJpeg = 1,
  kWebP = 2,
  kMaxValue = kWebP,
};

enum class ImageDecodeAcceleratorSubsampling {
  k420 = 0,
  k422 = 1,
  k444 = 2,
  kMaxValue = k444,
};

// Specification of an image decoding profile supported by a hardware decoder.
struct GPU_EXPORT ImageDecodeAcceleratorSupportedProfile {
  ImageDecodeAcceleratorSupportedProfile();
  ImageDecodeAcceleratorSupportedProfile(
      const ImageDecodeAcceleratorSupportedProfile& other);
  ImageDecodeAcceleratorSupportedProfile(
      ImageDecodeAcceleratorSupportedProfile&& other);
  ~ImageDecodeAcceleratorSupportedProfile();
  ImageDecodeAcceleratorSupportedProfile& operator=(
      const ImageDecodeAcceleratorSupportedProfile& other);
  ImageDecodeAcceleratorSupportedProfile& operator=(
      ImageDecodeAcceleratorSupportedProfile&& other);

  // Fields common to all image types.
  // Type of image to which this profile applies, e.g., JPEG.
  ImageDecodeAcceleratorType image_type;
  // Minimum and maximum supported pixel dimensions of the encoded image.
  gfx::Size min_encoded_dimensions;
  gfx::Size max_encoded_dimensions;

  // Fields specific to |image_type| == kJpeg.
  // The supported chroma subsampling formats, e.g. 4:2:0.
  std::vector<ImageDecodeAcceleratorSubsampling> subsamplings;
};
using ImageDecodeAcceleratorSupportedProfiles =
    std::vector<ImageDecodeAcceleratorSupportedProfile>;

#if defined(OS_WIN)
enum class OverlaySupport { kNone = 0, kDirect = 1, kScaling = 2 };

GPU_EXPORT const char* OverlaySupportToString(OverlaySupport support);

struct GPU_EXPORT Dx12VulkanVersionInfo {
  bool IsEmpty() const { return !d3d12_feature_level && !vulkan_version; }

  // True if the GPU driver supports DX12.
  bool supports_dx12 = false;

  // True if the GPU driver supports Vulkan.
  bool supports_vulkan = false;

  // The supported d3d feature level in the gpu driver;
  uint32_t d3d12_feature_level = 0;

  // The support Vulkan API version in the gpu driver;
  uint32_t vulkan_version = 0;
};
#endif

struct GPU_EXPORT GPUInfo {
  struct GPU_EXPORT GPUDevice {
    GPUDevice();
    GPUDevice(const GPUDevice& other);
    GPUDevice(GPUDevice&& other) noexcept;
    ~GPUDevice() noexcept;
    GPUDevice& operator=(const GPUDevice& other);
    GPUDevice& operator=(GPUDevice&& other) noexcept;

    // The DWORD (uint32_t) representing the graphics card vendor id.
    uint32_t vendor_id = 0u;

    // The DWORD (uint32_t) representing the graphics card device id.
    // Device ids are unique to vendor, not to one another.
    uint32_t device_id = 0u;

#if defined(OS_WIN)
    // The graphics card subsystem id.
    // The lower 16 bits represents the subsystem vendor id.
    uint32_t sub_sys_id = 0u;

    // The graphics card revision number.
    uint32_t revision = 0u;
#endif  // OS_WIN

    // Whether this GPU is the currently used one.
    // Currently this field is only supported and meaningful on OS X.
    bool active = false;

    // The strings that describe the GPU.
    // In Linux these strings are obtained through libpci.
    // In Win/MacOSX, these two strings are not filled at the moment.
    // In Android, these are respectively GL_VENDOR and GL_RENDERER.
    std::string vendor_string;
    std::string device_string;

    std::string driver_vendor;
    std::string driver_version;

    // NVIDIA CUDA compute capability, major version. 0 if undetermined. Can be
    // used to determine the hardware generation that the GPU belongs to.
    int cuda_compute_capability_major = 0;
  };

  GPUInfo();
  GPUInfo(const GPUInfo& other);
  ~GPUInfo();

  // The currently active gpu.
  GPUDevice& active_gpu();
  const GPUDevice& active_gpu() const;

  bool IsInitialized() const;

  // The amount of time taken to get from the process starting to the message
  // loop being pumped.
  base::TimeDelta initialization_time;

  // Computer has NVIDIA Optimus
  bool optimus;

  // Computer has AMD Dynamic Switchable Graphics
  bool amd_switchable;

  // Primary GPU, for exmaple, the discrete GPU in a dual GPU machine.
  GPUDevice gpu;

  // Secondary GPUs, for example, the integrated GPU in a dual GPU machine.
  std::vector<GPUDevice> secondary_gpus;

  // The version of the pixel/fragment shader used by the gpu.
  std::string pixel_shader_version;

  // The version of the vertex shader used by the gpu.
  std::string vertex_shader_version;

  // The maximum multisapling sample count, either through ES3 or
  // EXT_multisampled_render_to_texture MSAA.
  std::string max_msaa_samples;

  // The machine model identifier. They can contain any character, including
  // whitespaces.  Currently it is supported on MacOSX and Android.
  // Android examples: "Naxus 5", "XT1032".
  // On MacOSX, the version is stripped out of the model identifier, for
  // example, the original identifier is "MacBookPro7,2", and we put
  // "MacBookPro" as machine_model_name, and "7.2" as machine_model_version.
  std::string machine_model_name;

  // The version of the machine model. Currently it is supported on MacOSX.
  // See machine_model_name's comment.
  std::string machine_model_version;

  // The GL_VERSION string.
  std::string gl_version;

  // The GL_VENDOR string.
  std::string gl_vendor;

  // The GL_RENDERER string.
  std::string gl_renderer;

  // The GL_EXTENSIONS string.
  std::string gl_extensions;

  // GL window system binding vendor.  "" if not available.
  std::string gl_ws_vendor;

  // GL window system binding version.  "" if not available.
  std::string gl_ws_version;

  // GL window system binding extensions.  "" if not available.
  std::string gl_ws_extensions;

  // GL reset notification strategy as defined by GL_ARB_robustness. 0 if GPU
  // reset detection or notification not available.
  uint32_t gl_reset_notification_strategy;

  bool software_rendering;

  // Empty means unknown. Defined on X11 as
  // - "1" means indirect (versions can't be all zero)
  // - "2" means some type of direct rendering, but version cannot not be
  //    reliably determined
  // - "2.1", "2.2", "2.3" for DRI, DRI2, DRI3 respectively
  std::string direct_rendering_version;

  // Whether the gpu process is running in a sandbox.
  bool sandboxed;

  // True if the GPU is running in the browser process instead of its own.
  bool in_process_gpu;

  // True if the GPU process is using the passthrough command decoder.
  bool passthrough_cmd_decoder;

  // True only on android when extensions for threaded mailbox sharing are
  // present. Threaded mailbox sharing is used on Android only, so this check
  // is only implemented on Android.
  bool can_support_threaded_texture_mailbox = false;

#if defined(OS_WIN)
  // True if we use direct composition surface on Windows.
  bool direct_composition = false;

  // True if we use direct composition surface overlays on Windows.
  bool supports_overlays = false;
  OverlaySupport yuy2_overlay_support = OverlaySupport::kNone;
  OverlaySupport nv12_overlay_support = OverlaySupport::kNone;

  // The information returned by the DirectX Diagnostics Tool.
  DxDiagNode dx_diagnostics;

  Dx12VulkanVersionInfo dx12_vulkan_version_info;
#endif

  VideoDecodeAcceleratorCapabilities video_decode_accelerator_capabilities;
  VideoEncodeAcceleratorSupportedProfiles
      video_encode_accelerator_supported_profiles;
  bool jpeg_decode_accelerator_supported;

  ImageDecodeAcceleratorSupportedProfiles
      image_decode_accelerator_supported_profiles;

#if defined(USE_X11)
  VisualID system_visual;
  VisualID rgba_visual;
#endif

  bool oop_rasterization_supported;

  bool subpixel_font_rendering;

#if BUILDFLAG(ENABLE_VULKAN)
  base::Optional<VulkanInfo> vulkan_info;
#endif

  // Note: when adding new members, please remember to update EnumerateFields
  // in gpu_info.cc.

  // In conjunction with EnumerateFields, this allows the embedder to
  // enumerate the values in this structure without having to embed
  // references to its specific member variables. This simplifies the
  // addition of new fields to this type.
  class Enumerator {
   public:
    // The following methods apply to the "current" object. Initially this
    // is the root object, but calls to BeginGPUDevice/EndGPUDevice and
    // BeginAuxAttributes/EndAuxAttributes change the object to which these
    // calls should apply.
    virtual void AddInt64(const char* name, int64_t value) = 0;
    virtual void AddInt(const char* name, int value) = 0;
    virtual void AddString(const char* name, const std::string& value) = 0;
    virtual void AddBool(const char* name, bool value) = 0;
    virtual void AddTimeDeltaInSecondsF(const char* name,
                                        const base::TimeDelta& value) = 0;
    virtual void AddBinary(const char* name,
                           const base::span<const uint8_t>& blob) = 0;

    // Markers indicating that a GPUDevice is being described.
    virtual void BeginGPUDevice() = 0;
    virtual void EndGPUDevice() = 0;

    // Markers indicating that a VideoDecodeAcceleratorSupportedProfile is
    // being described.
    virtual void BeginVideoDecodeAcceleratorSupportedProfile() = 0;
    virtual void EndVideoDecodeAcceleratorSupportedProfile() = 0;

    // Markers indicating that a VideoEncodeAcceleratorSupportedProfile is
    // being described.
    virtual void BeginVideoEncodeAcceleratorSupportedProfile() = 0;
    virtual void EndVideoEncodeAcceleratorSupportedProfile() = 0;

    // Markers indicating that an ImageDecodeAcceleratorSupportedProfile is
    // being described.
    virtual void BeginImageDecodeAcceleratorSupportedProfile() = 0;
    virtual void EndImageDecodeAcceleratorSupportedProfile() = 0;

    // Markers indicating that "auxiliary" attributes of the GPUInfo
    // (according to the DevTools protocol) are being described.
    virtual void BeginAuxAttributes() = 0;
    virtual void EndAuxAttributes() = 0;

    virtual void BeginDx12VulkanVersionInfo() = 0;
    virtual void EndDx12VulkanVersionInfo() = 0;

   protected:
    virtual ~Enumerator() = default;
  };

  // Outputs the fields in this structure to the provided enumerator.
  void EnumerateFields(Enumerator* enumerator) const;
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_INFO_H_
