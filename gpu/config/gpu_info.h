// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_INFO_H_
#define GPU_CONFIG_GPU_INFO_H_

// Provides access to the GPU information for the system
// on which chrome is currently running.

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/clang_profiling_buildflags.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "gpu/gpu_export.h"
#include "gpu/vulkan/buildflags.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gpu_preference.h"

#if BUILDFLAG(IS_WIN)
#include <dxgi.h>

#include "base/win/windows_types.h"
#endif

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/config/vulkan_info.h"
#endif

namespace gpu {

// These values are persistent to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This should match enum IntelGpuSeriesType in
//  \tools\metrics\histograms\metadata\gpu\enums.xml
enum class IntelGpuSeriesType {
  kUnknown = 0,
  // Intel 4th gen
  kBroadwater = 16,
  kEaglelake = 17,
  // Intel 5th gen
  kIronlake = 18,
  // Intel 6th gen
  kSandybridge = 1,
  // Intel 7th gen
  kBaytrail = 2,
  kIvybridge = 3,
  kHaswell = 4,
  // Intel 8th gen
  kCherrytrail = 5,
  kBroadwell = 6,
  // Intel 9th gen
  kApollolake = 7,
  kSkylake = 8,
  kGeminilake = 9,
  kKabylake = 10,
  kAmberlake = 23,
  kCoffeelake = 11,
  kWhiskeylake = 12,
  kCometlake = 13,
  // Intel 10th gen
  kCannonlake = 14,
  // Intel 11th gen
  kIcelake = 15,
  kElkhartlake = 19,
  kJasperlake = 20,
  // Intel 12th gen
  kTigerlake = 21,
  kRocketlake = 24,
  kDG1 = 25,
  kAlderlake = 22,
  kAlchemist = 26,
  kRaptorlake = 27,
  kMeteorlake = 28,
  kArrowlake = 30,
  // Intel 13th gen
  kLunarlake = 29,
  kBattlemage = 31,
  // Please also update |gpu_series_map| in process_json.py.
  kMaxValue = kBattlemage,
};

// Video profile.  This *must* match media::VideoCodecProfile.
enum VideoCodecProfile {
  VIDEO_CODEC_PROFILE_UNKNOWN = -1,
  VIDEO_CODEC_PROFILE_MIN = VIDEO_CODEC_PROFILE_UNKNOWN,
  H264PROFILE_BASELINE = 0,
  H264PROFILE_MAIN = 1,
  H264PROFILE_EXTENDED = 2,
  H264PROFILE_HIGH = 3,
  H264PROFILE_HIGH10PROFILE = 4,
  H264PROFILE_HIGH422PROFILE = 5,
  H264PROFILE_HIGH444PREDICTIVEPROFILE = 6,
  H264PROFILE_SCALABLEBASELINE = 7,
  H264PROFILE_SCALABLEHIGH = 8,
  H264PROFILE_STEREOHIGH = 9,
  H264PROFILE_MULTIVIEWHIGH = 10,
  VP8PROFILE_ANY = 11,
  VP9PROFILE_PROFILE0 = 12,
  VP9PROFILE_PROFILE1 = 13,
  VP9PROFILE_PROFILE2 = 14,
  VP9PROFILE_PROFILE3 = 15,
  HEVCPROFILE_MAIN = 16,
  HEVCPROFILE_MAIN10 = 17,
  HEVCPROFILE_MAIN_STILL_PICTURE = 18,
  DOLBYVISION_PROFILE0 = 19,
  // Deprecated: DOLBYVISION_PROFILE4 = 20,
  DOLBYVISION_PROFILE5 = 21,
  DOLBYVISION_PROFILE7 = 22,
  THEORAPROFILE_ANY = 23,
  AV1PROFILE_PROFILE_MAIN = 24,
  AV1PROFILE_PROFILE_HIGH = 25,
  AV1PROFILE_PROFILE_PRO = 26,
  DOLBYVISION_PROFILE8 = 27,
  DOLBYVISION_PROFILE9 = 28,
  HEVCPROFILE_REXT = 29,
  HEVCPROFILE_HIGH_THROUGHPUT = 30,
  HEVCPROFILE_MULTIVIEW_MAIN = 31,
  HEVCPROFILE_SCALABLE_MAIN = 32,
  HEVCPROFILE_3D_MAIN = 33,
  HEVCPROFILE_SCREEN_EXTENDED = 34,
  HEVCPROFILE_SCALABLE_REXT = 35,
  HEVCPROFILE_HIGH_THROUGHPUT_SCREEN_EXTENDED = 36,
  VVCPROFILE_MAIN10 = 37,
  VVCPROFILE_MAIN12 = 38,
  VVCPROFILE_MAIN12_INTRA = 39,
  VVCPROIFLE_MULTILAYER_MAIN10 = 40,
  VVCPROFILE_MAIN10_444 = 41,
  VVCPROFILE_MAIN12_444 = 42,
  VVCPROFILE_MAIN16_444 = 43,
  VVCPROFILE_MAIN12_444_INTRA = 44,
  VVCPROFILE_MAIN16_444_INTRA = 45,
  VVCPROFILE_MULTILAYER_MAIN10_444 = 46,
  VVCPROFILE_MAIN10_STILL_PICTURE = 47,
  VVCPROFILE_MAIN12_STILL_PICTURE = 48,
  VVCPROFILE_MAIN10_444_STILL_PICTURE = 49,
  VVCPROFILE_MAIN12_444_STILL_PICTURE = 50,
  VVCPROFILE_MAIN16_444_STILL_PICTURE = 51,
  VIDEO_CODEC_PROFILE_MAX = VVCPROFILE_MAIN16_444_STILL_PICTURE,
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
  bool is_software_codec;
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

#if BUILDFLAG(IS_WIN)
enum class OverlaySupport {
  kNone = 0,
  kDirect = 1,
  kScaling = 2,
  kSoftware = 3
};

GPU_EXPORT const char* OverlaySupportToString(OverlaySupport support);

struct GPU_EXPORT OverlayInfo {
  OverlayInfo() = default;
  OverlayInfo(const OverlayInfo& other) = default;
  OverlayInfo& operator=(const OverlayInfo& other) = default;
  bool operator==(const OverlayInfo& other) const {
    return direct_composition == other.direct_composition &&
           supports_overlays == other.supports_overlays &&
           yuy2_overlay_support == other.yuy2_overlay_support &&
           nv12_overlay_support == other.nv12_overlay_support &&
           bgra8_overlay_support == other.bgra8_overlay_support &&
           rgb10a2_overlay_support == other.rgb10a2_overlay_support &&
           p010_overlay_support == other.p010_overlay_support;
  }
  bool operator!=(const OverlayInfo& other) const { return !(*this == other); }

  // True if we use direct composition surface on Windows.
  bool direct_composition = false;

  // True if we use direct composition surface overlays on Windows.
  bool supports_overlays = false;
  OverlaySupport yuy2_overlay_support = OverlaySupport::kNone;
  OverlaySupport nv12_overlay_support = OverlaySupport::kNone;
  OverlaySupport bgra8_overlay_support = OverlaySupport::kNone;
  OverlaySupport rgb10a2_overlay_support = OverlaySupport::kNone;
  OverlaySupport p010_overlay_support = OverlaySupport::kNone;
};

#endif

#if BUILDFLAG(IS_MAC)
GPU_EXPORT bool ValidateMacOSSpecificTextureTarget(int target);
#endif  // BUILDFLAG(IS_MAC)

struct GPU_EXPORT GPUInfo {
  struct GPU_EXPORT GPUDevice {
    GPUDevice();
    GPUDevice(const GPUDevice& other);
    GPUDevice(GPUDevice&& other) noexcept;
    ~GPUDevice() noexcept;
    GPUDevice& operator=(const GPUDevice& other);
    GPUDevice& operator=(GPUDevice&& other) noexcept;

    bool IsSoftwareRenderer() const;

    // The DWORD (uint32_t) representing the graphics card vendor id.
    uint32_t vendor_id = 0u;

    // The DWORD (uint32_t) representing the graphics card device id.
    // Device ids are unique to vendor, not to one another.
    uint32_t device_id = 0u;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
    // The graphics card revision number.
    uint32_t revision = 0u;
#endif

#if BUILDFLAG(IS_WIN)
    // The graphics card subsystem id.
    // The lower 16 bits represents the subsystem vendor id.
    uint32_t sub_sys_id = 0u;

    // The graphics card LUID. This is a unique identifier for the graphics card
    // that is guaranteed to be unique until the computer is restarted. The LUID
    // is used over the vendor id and device id because the device id is only
    // unique relative its vendor, not to each other. If there are more than one
    // of the same exact graphics card, they all have the same vendor id and
    // device id but different LUIDs.
    CHROME_LUID luid;
#endif  // BUILDFLAG(IS_WIN)

    // The 64-bit ID used for GPU selection by ANGLE_platform_angle_device_id.
    // On Mac this matches the registry ID of an IOGraphicsAccelerator2 or
    // AGXAccelerator.
    // On Windows this matches the concatenated LUID.
    uint64_t system_device_id = 0ULL;

    // Whether this GPU is the currently used one.
    // Currently this field is only supported and meaningful on OS X and on
    // Windows using Angle with D3D11.
    bool active = false;

    // The strings that describe the GPU.
    // In Linux these strings are obtained through libpci.
    // In Win/MacOSX, these two strings are not filled at the moment.
    // In Android, these are respectively GL_VENDOR and GL_RENDERER.
    std::string vendor_string;
    std::string device_string;

    std::string driver_vendor;
    std::string driver_version;

    // If this device is identified as high performance or low power GPU.
    gl::GpuPreference gpu_preference = gl::GpuPreference::kNone;
  };

  GPUInfo();
  GPUInfo(const GPUInfo& other);
  ~GPUInfo();

  // The currently active gpu.
  GPUDevice& active_gpu();
  const GPUDevice& active_gpu() const;

  bool IsInitialized() const;

  bool UsesSwiftShader() const;

  unsigned int GpuCount() const;

  const GPUDevice* GetGpuByPreference(gl::GpuPreference preference) const;

#if BUILDFLAG(IS_WIN)
  GPUDevice* FindGpuByLuid(DWORD low_part, LONG high_part);
#endif  // BUILDFLAG(IS_WIN)

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

  // NPU adapters.
  std::vector<GPUDevice> npus;

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

  // The DisplayType requested from ANGLE.
  std::string display_type;

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

  gl::GLImplementationParts gl_implementation_parts;

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

// Whether the browser was built with ASAN or not.
#if defined(ADDRESS_SANITIZER)
  bool is_asan = true;
#else
  bool is_asan = false;
#endif

// Whether the browser was built with Clang coverage enabled or not.
#if BUILDFLAG(USE_CLANG_COVERAGE) || BUILDFLAG(CLANG_PROFILING)
  bool is_clang_coverage = true;
#else
  bool is_clang_coverage = false;
#endif

#if defined(ARCH_CPU_64_BITS)
  uint32_t target_cpu_bits = 64;
#elif defined(ARCH_CPU_32_BITS)
  uint32_t target_cpu_bits = 32;
#elif defined(ARCH_CPU_31_BITS)
  uint32_t target_cpu_bits = 31;
#endif

#if BUILDFLAG(IS_WIN)
  // The supported DirectML feature level in the gpu driver;
  uint32_t directml_feature_level = 0;

  // The supported d3d feature level in the gpu driver;
  uint32_t d3d12_feature_level = 0;

  // The support Vulkan API version in the gpu driver;
  uint32_t vulkan_version = 0;

  // The GPU hardware overlay info.
  OverlayInfo overlay_info;

  // Are d3d shared images supported.
  bool shared_image_d3d = false;
#endif
  VideoDecodeAcceleratorSupportedProfiles
      video_decode_accelerator_supported_profiles;

  // DO NOT use for anything but diagnostics/metrics like chrome://gpu,
  // it's not populated at start up and can be unreliable for a while.
  VideoEncodeAcceleratorSupportedProfiles
      video_encode_accelerator_supported_profiles;
  bool jpeg_decode_accelerator_supported;

  ImageDecodeAcceleratorSupportedProfiles
      image_decode_accelerator_supported_profiles;

  bool subpixel_font_rendering;

  uint32_t visibility_callback_call_count = 0;

#if BUILDFLAG(ENABLE_VULKAN)
  bool hardware_supports_vulkan = false;

  std::optional<VulkanInfo> vulkan_info;
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

    virtual void BeginOverlayInfo() = 0;
    virtual void EndOverlayInfo() = 0;

   protected:
    virtual ~Enumerator() = default;
  };

  // Outputs the fields in this structure to the provided enumerator.
  void EnumerateFields(Enumerator* enumerator) const;
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_INFO_H_
