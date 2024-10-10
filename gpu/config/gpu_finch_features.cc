// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_finch_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "gpu/config/gpu_switches.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_image_reader_compat.h"
#include "base/android/build_info.h"
#include "base/android/sys_utils.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "base/system/sys_info.h"
#endif  // BUILDFLAG(IS_MAC)

namespace features {
namespace {

#if BUILDFLAG(IS_ANDROID)
bool IsDeviceBlocked(const char* field, const std::string& block_list) {
  auto disable_patterns = base::SplitString(
      block_list, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& disable_pattern : disable_patterns) {
    if (base::MatchPattern(field, disable_pattern))
      return true;
  }
  return false;
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

// Used to limit GL version to 2.0 for skia raster and compositing.
BASE_FEATURE(kUseGles2ForOopR,
             "UseGles2ForOopR",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Use android SurfaceControl API for managing display compositor's buffer queue
// and using overlays on Android. Also used by webview to disable surface
// SurfaceControl.
BASE_FEATURE(kAndroidSurfaceControl,
             "AndroidSurfaceControl",
             base::FEATURE_ENABLED_BY_DEFAULT);

// https://crbug.com/1176185 List of devices on which SurfaceControl should be
// disabled.
const base::FeatureParam<std::string> kAndroidSurfaceControlDeviceBlocklist{
    &kAndroidSurfaceControl, "AndroidSurfaceControlDeviceBlocklist",
    "capri|caprip"};

// List of models on which SurfaceControl should be disabled.
const base::FeatureParam<std::string> kAndroidSurfaceControlModelBlocklist{
    &kAndroidSurfaceControl, "AndroidSurfaceControlModelBlocklist",
    "SM-F9*|SM-W202?|SCV44|SCG05|SCG11|SC-55B"};

// Hardware Overlays for WebView.
BASE_FEATURE(kWebViewSurfaceControl,
             "WebViewSurfaceControl",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebViewSurfaceControlForTV,
             "WebViewSurfaceControlForTV",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use thread-safe media path on WebView.
BASE_FEATURE(kWebViewThreadSafeMedia,
             "WebViewThreadSafeMedia",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This is used as default state because it's different for webview and chrome.
// WebView hardcodes this as enabled in AwMainDelegate.
BASE_FEATURE(kWebViewThreadSafeMediaDefault,
             "WebViewThreadSafeMediaDefault",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Used to limit AImageReader max queue size to 1 since many devices especially
// android Tv devices do not support more than 1 images.
BASE_FEATURE(kLimitAImageReaderMaxSizeToOne,
             "LimitAImageReaderMaxSizeToOne",
             base::FEATURE_ENABLED_BY_DEFAULT);

// List of devices on which to limit AImageReader max queue size to 1.
const base::FeatureParam<std::string> kLimitAImageReaderMaxSizeToOneBlocklist{
    &kLimitAImageReaderMaxSizeToOne, "LimitAImageReaderMaxSizeToOneBlocklist",
    "MIBOX|*ODROID*"};

// Used to relax the limit of AImageReader max queue size to 1 for Android Tvs.
// Currently for all android tv except the ones in this list will have max
// queue size of 1 image.
BASE_FEATURE(kRelaxLimitAImageReaderMaxSizeToOne,
             "RelaxLimitAImageReaderMaxSizeToOne",
             base::FEATURE_ENABLED_BY_DEFAULT);

// List of devices on which to relax the restriction of max queue size of 1 for
// AImageReader.
const base::FeatureParam<std::string>
    kRelaxLimitAImageReaderMaxSizeToOneSoCBlocklist{
        &kRelaxLimitAImageReaderMaxSizeToOne,
        "RelaxLimitAImageReaderMaxSizeToOneSoCBlocklist", "*Broadcom*"};
const base::FeatureParam<std::string>
    kRelaxLimitAImageReaderMaxSizeToOneManufacturerBlocklist{
        &kRelaxLimitAImageReaderMaxSizeToOne,
        "RelaxLimitAImageReaderMaxSizeToOneManufacturerBlocklist",
        "*Broadcom*"};
const base::FeatureParam<std::string>
    kRelaxLimitAImageReaderMaxSizeToOneDeviceBlocklist{
        &kRelaxLimitAImageReaderMaxSizeToOne,
        "RelaxLimitAImageReaderMaxSizeToOneDeviceBlocklist", ""};
const base::FeatureParam<std::string>
    kRelaxLimitAImageReaderMaxSizeToOneModelBlocklist{
        &kRelaxLimitAImageReaderMaxSizeToOne,
        "RelaxLimitAImageReaderMaxSizeToOneModelBlocklist", ""};

// Increase number of buffers and pipeline depth for high frame rate devices.
BASE_FEATURE(kIncreaseBufferCountForHighFrameRate,
             "IncreaseBufferCountForHighFrameRate",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<std::string>
    kDisableIncreaseBufferCountForHighFrameRate{
        &kIncreaseBufferCountForHighFrameRate,
        "DisableIncreaseBufferCountForHighFrameRate", ""};
#endif

// Enable GPU Rasterization by default. This can still be overridden by
// --enable-gpu-rasterization or --disable-gpu-rasterization.
// DefaultEnableGpuRasterization has launched on Mac, Windows, ChromeOS,
// Android and Linux.
BASE_FEATURE(kDefaultEnableGpuRasterization,
             "DefaultEnableGpuRasterization",
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if !BUILDFLAG(IS_ANDROID)
// Enables the use of out of process rasterization for canvas.
BASE_FEATURE(kCanvasOopRasterization,
             "CanvasOopRasterization",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Enables the use of MSAA in skia on Ice Lake and later intel architectures.
BASE_FEATURE(kEnableMSAAOnNewIntelGPUs,
             "EnableMSAAOnNewIntelGPUs",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kNoUndamagedOverlayPromotion,
             "NoUndamagedOverlayPromotion",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_IOS)
// If enabled, the TASK_CATEGORY_POLICY value of the GPU process will be
// adjusted to match the one from the browser process every time it changes.
BASE_FEATURE(kAdjustGpuProcessPriority,
             "AdjustGpuProcessPriority",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// When enabled, Grshader disk cache will be cleared on startup if any cache
// entry prefix does not match with the current prefix. prefix is made up of
// various parameters like chrome version, driver version etc.
BASE_FEATURE(kClearGrShaderDiskCacheOnInvalidPrefix,
             "ClearGrShaderDiskCacheOnInvalidPrefix",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the decode acceleration of JPEG images (as opposed to camera
// captures) in Chrome OS using the VA-API.
// TODO(andrescj): remove or enable by default in Chrome OS once
// https://crbug.com/868400 is resolved.
BASE_FEATURE(kVaapiJpegImageDecodeAcceleration,
             "VaapiJpegImageDecodeAcceleration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the decode acceleration of WebP images in Chrome OS using the
// VA-API.
// TODO(gildekel): remove or enable by default in Chrome OS once
// https://crbug.com/877694 is resolved.
BASE_FEATURE(kVaapiWebPImageDecodeAcceleration,
             "VaapiWebPImageDecodeAcceleration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable Vulkan graphics backend for compositing and rasterization. Defaults to
// native implementation if --use-vulkan flag is not used. Otherwise
// --use-vulkan will be followed.
// Note Android WebView uses kWebViewDrawFunctorUsesVulkan instead of this.
BASE_FEATURE(kVulkan,
             "Vulkan",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kEnableDrDc,
             "EnableDrDc",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enable WebGPU on gpu service side only. This is used with origin trial and
// enabled by default on supported platforms.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID)
#define WEBGPU_ENABLED base::FEATURE_ENABLED_BY_DEFAULT
#else
#define WEBGPU_ENABLED base::FEATURE_DISABLED_BY_DEFAULT
#endif
BASE_FEATURE(kWebGPUService, "WebGPUService", WEBGPU_ENABLED);
BASE_FEATURE(kWebGPUBlobCache, "WebGPUBlobCache", WEBGPU_ENABLED);
#undef WEBGPU_ENABLED

// List of WebGPU feature names, delimited by ,
// The FeatureParam may be overridden via Finch config, or via the command line
// For example:
//   --enable-field-trial-config \
//   --force-fieldtrial-params=WebGPU.Enabled:UnsafeFeatures/timestamp-query%2Cshader-f16
// Note that the comma should be URL-encoded.
const base::FeatureParam<std::string> kWebGPUUnsafeFeatures{
    &kWebGPUService, "UnsafeFeatures", ""};
// List of WGSL feature names, delimited by ,
// The FeatureParam may be overridden via Finch config, or via the command line
// For example:
//   --enable-field-trial-config \
//   --force-fieldtrial-params=WebGPU.Enabled:UnsafeWGSLFeatures/feature_1%2Cfeature_2
// Note that the comma should be URL-encoded.
const base::FeatureParam<std::string> kWGSLUnsafeFeatures{
    &kWebGPUService, "UnsafeWGSLFeatures", ""};

BASE_FEATURE(kWebGPUUseDXC, "WebGPUUseDXC2", base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kWebGPUUseTintIR,
             "WebGPUUseTintIR",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_ANDROID)

const base::FeatureParam<std::string> kVulkanBlockListByHardware{
    &kVulkan, "BlockListByHardware", ""};

const base::FeatureParam<std::string> kVulkanBlockListByBrand{
    &kVulkan, "BlockListByBrand", ""};

const base::FeatureParam<std::string> kVulkanBlockListByDevice{
    &kVulkan, "BlockListByDevice", ""};

const base::FeatureParam<std::string> kVulkanBlockListByAndroidBuildId{
    &kVulkan, "BlockListByAndroidBuildId", ""};

const base::FeatureParam<std::string> kVulkanBlockListByManufacturer{
    &kVulkan, "BlockListByManufacturer", ""};

const base::FeatureParam<std::string> kVulkanBlockListByModel{
    &kVulkan, "BlockListByModel", ""};

const base::FeatureParam<std::string> kVulkanBlockListByBoard{
    &kVulkan, "BlockListByBoard", ""};

const base::FeatureParam<std::string> kVulkanBlockListByAndroidBuildFP{
    &kVulkan, "BlockListByAndroidBuildFP", ""};

// Blocklists meant for DrDc.
// crbug.com/1294648, crbug.com/1397578: the screen flickers.
const base::FeatureParam<std::string> kDrDcBlockListByDevice{
    &kEnableDrDc, "BlockListByDevice",
    "LF9810_2GB|amber|chopin|secret|a03|SO-51B|on7xelte|j7xelte|F41B|doha|"
    "rk322x_box|a20s|HWMAR|HWSTK-HF|HWPOT-H|b2q|channel|galahad|a32|ellis|"
    "dandelion|tonga|RMX3231|ASUS_I006D|ASUS_I004D|bacon"};

// crbug.com/1340059, crbug.com/1340064
const base::FeatureParam<std::string> kDrDcBlockListByModel{
    &kEnableDrDc, "BlockListByModel",
    "SM-J400M|SM-J415F|ONEPLUS A3003|OCTAStream*"};

const base::FeatureParam<std::string> kDrDcBlockListByHardware{
    &kEnableDrDc, "BlockListByHardware", ""};

const base::FeatureParam<std::string> kDrDcBlockListByBrand{
    &kEnableDrDc, "BlockListByBrand", "HONOR"};

const base::FeatureParam<std::string> kDrDcBlockListByAndroidBuildId{
    &kEnableDrDc, "BlockListByAndroidBuildId", ""};

const base::FeatureParam<std::string> kDrDcBlockListByManufacturer{
    &kEnableDrDc, "BlockListByManufacturer", ""};

const base::FeatureParam<std::string> kDrDcBlockListByBoard{
    &kEnableDrDc, "BlockListByBoard", ""};

const base::FeatureParam<std::string> kDrDcBlockListByAndroidBuildFP{
    &kEnableDrDc, "BlockListByAndroidBuildFP", ""};
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_OZONE)
// On Ozone, compute SharedImage scanout support based on overlays being
// supported rather than native pixmaps being supported.
// TODO(crbug.com/330865436): It turns out that `supports_overlays` is
// currently set only in the browser process; we need to ensure that it is set
// in the GPU process before we can re-enable this feature.
BASE_FEATURE(kSharedImageSupportScanoutOnOzoneOnlyIfOverlaysSupported,
             "SharedImageSupportScanoutOnOzoneOnlyIfOverlaysSupported",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enable Skia Graphite. This will use the Dawn backend by default, but can be
// overridden with command line flags for testing on non-official developer
// builds. See --skia-graphite-backend flag in gpu_switches.h.
// Note: This can also be overridden by
// --enable-skia-graphite & --disable-skia-graphite.
BASE_FEATURE(kSkiaGraphite,
             "SkiaGraphite",
             base::FEATURE_DISABLED_BY_DEFAULT
);

BASE_FEATURE(kConditionallySkipGpuChannelFlush,
             "ConditionallySkipGpuChannelFlush",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether the Dawn "skip_validation" toggle is enabled for Skia Graphite.
const base::FeatureParam<bool> kSkiaGraphiteDawnSkipValidation{
    &kSkiaGraphite, "dawn_skip_validation", true};

// Whether Dawn backend validation is enabled for Skia Graphite.
const base::FeatureParam<bool> kSkiaGraphiteDawnBackendValidation{
    &kSkiaGraphite, "dawn_backend_validation", false};

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kSkiaGraphiteDawnUseD3D12,
             "SkiaGraphiteDawnUseD3D12",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// If enabled, SharedImages created for SW video frames have SCANOUT usage added
// only if SharedImageCapabilities indicates that there is support. Serves as
// killswitch for this rollout. Lives in //gpu as backings that are rolling out
// restrictions on supporting SCANOUT usage must check the value of this
// base::Feature.
// TODO(crbug.com/330865436): Remove post-safe rollout.
BASE_FEATURE(kSWVideoFrameAddScanoutUsageOnlyIfSupportedBySharedImage,
             "SWVideoFrameAddScanoutUsageOnlyIfSupportedBySharedImage",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable persistent storage of VkPipelineCache data.
BASE_FEATURE(kEnableVkPipelineCache,
             "EnableVkPipelineCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enabling this will make the GPU decode path use a mock implementation of
// discardable memory.
BASE_FEATURE(kNoDiscardableMemoryForGpuDecodePath,
             "NoDiscardableMemoryForGpuDecodePath",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use a 100-command limit before forcing context switch per command buffer
// instead of 20.
BASE_FEATURE(kIncreasedCmdBufferParseSlice,
             "IncreasedCmdBufferParseSlice",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Prune transfer cache entries not accessed recently. This also turns off
// similar logic in cc::GpuImageDecodeCache which is the largest (often single)
// client of transfer cache.
BASE_FEATURE(kPruneOldTransferCacheEntries,
             "PruneOldTransferCacheEntries",
             base::FEATURE_DISABLED_BY_DEFAULT);

// On platforms with delegated compositing, try to release overlays later, when
// no new frames are swapped.
BASE_FEATURE(kDeferredOverlaysRelease,
             "DeferredOverlayRelease",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Use d3d11 UpdateSubresource() (instead of a staging texture) to upload pixels
// to textures.
#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kD3DBackingUploadWithUpdateSubresource,
             "D3DBackingUploadWithUpdateSubresource",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// This feature allows viz to handle overlays' swap failures instead of loosing a context and
// restarting a gpu service.
BASE_FEATURE(kHandleOverlaysSwapFailure,
             "HandleOverlaysSwapFailure",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool UseGles2ForOopR() {
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_X86_FAMILY)
  // GLES3 is not supported on emulators with passthrough. crbug.com/1423712
  if (gl::UsePassthroughCommandDecoder(base::CommandLine::ForCurrentProcess()))
    return true;
#endif
  return base::FeatureList::IsEnabled(features::kUseGles2ForOopR);
}

bool IsUsingVulkan() {
#if BUILDFLAG(IS_ANDROID)
  // Force on if Vulkan feature is enabled from command line.
  base::FeatureList* feature_list = base::FeatureList::GetInstance();
  if (feature_list &&
      feature_list->IsFeatureOverriddenFromCommandLine(
          features::kVulkan.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE))
    return true;

  // WebView checks, which do not use (and disables) kVulkan.
  // Do this above the Android version check because there are test devices
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebViewDrawFunctorUsesVulkan)) {
    return true;
  }

  // No support for devices before Q -- exit before checking feature flags
  // so that devices are not counted in finch trials.
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_Q)
    return false;

  if (!base::FeatureList::IsEnabled(kVulkan))
    return false;

  // Check block list against build info.
  const auto* build_info = base::android::BuildInfo::GetInstance();
  if (IsDeviceBlocked(build_info->hardware(), kVulkanBlockListByHardware.Get()))
    return false;
  if (IsDeviceBlocked(build_info->brand(), kVulkanBlockListByBrand.Get()))
    return false;
  if (IsDeviceBlocked(build_info->device(), kVulkanBlockListByDevice.Get()))
    return false;
  if (IsDeviceBlocked(build_info->android_build_id(),
                      kVulkanBlockListByAndroidBuildId.Get()))
    return false;
  if (IsDeviceBlocked(build_info->manufacturer(),
                      kVulkanBlockListByManufacturer.Get()))
    return false;
  if (IsDeviceBlocked(build_info->model(), kVulkanBlockListByModel.Get()))
    return false;
  if (IsDeviceBlocked(build_info->board(), kVulkanBlockListByBoard.Get()))
    return false;
  if (IsDeviceBlocked(build_info->android_build_fp(),
                      kVulkanBlockListByAndroidBuildFP.Get()))
    return false;

  return true;

#else
  return base::FeatureList::IsEnabled(kVulkan);
#endif
}

bool IsDrDcEnabled() {
#if BUILDFLAG(IS_ANDROID)
  // Enabled on android P+.
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_P) {
    return false;
  }

  // DrDc is supported on android MediaPlayer and MCVD path only when
  // AImageReader is enabled. Also DrDc requires AImageReader max size to be
  // at least 2 for each gpu thread. Hence DrDc is disabled on devices which has
  // only 1 image.
  if (!base::android::EnableAndroidImageReader() ||
      LimitAImageReaderMaxSizeToOne()) {
    return false;
  }

  // Check block list against build info.
  const auto* build_info = base::android::BuildInfo::GetInstance();
  if (IsDeviceBlocked(build_info->device(), kDrDcBlockListByDevice.Get()))
    return false;
  if (IsDeviceBlocked(build_info->model(), kDrDcBlockListByModel.Get()))
    return false;
  if (IsDeviceBlocked(build_info->hardware(), kDrDcBlockListByHardware.Get()))
    return false;
  if (IsDeviceBlocked(build_info->brand(), kDrDcBlockListByBrand.Get()))
    return false;
  if (IsDeviceBlocked(build_info->android_build_id(),
                      kDrDcBlockListByAndroidBuildId.Get()))
    return false;
  if (IsDeviceBlocked(build_info->manufacturer(),
                      kDrDcBlockListByManufacturer.Get()))
    return false;
  if (IsDeviceBlocked(build_info->board(), kDrDcBlockListByBoard.Get()))
    return false;
  if (IsDeviceBlocked(build_info->android_build_fp(),
                      kDrDcBlockListByAndroidBuildFP.Get()))
    return false;

  if (!base::FeatureList::IsEnabled(kEnableDrDc))
    return false;

  return true;
#else
  return false;
#endif
}

bool IsUsingThreadSafeMediaForWebView() {
#if BUILDFLAG(IS_ANDROID)
  // SurfaceTexture can't be thread-safe. Also thread safe media code currently
  // requires AImageReader max size to be at least 2 since one image could be
  // accessed by each gpu thread in webview.
  if (!base::android::EnableAndroidImageReader() ||
      LimitAImageReaderMaxSizeToOne()) {
    return false;
  }

  // If the feature is overridden from command line or finch we will use its
  // value. If not we use kWebViewThreadSafeMediaDefault which is set in
  // AwMainDelegate for WebView.
  if (auto state =
          base::FeatureList::GetStateIfOverridden(kWebViewThreadSafeMedia)) {
    return *state;
  }
  return base::FeatureList::IsEnabled(kWebViewThreadSafeMediaDefault);
#else
  return false;
#endif
}

// Note that DrDc is also disabled on some of the gpus (crbug.com/1354201).
// Thread safe media will still be used on those gpus which should be fine for
// now as the lock shouldn't have much overhead and is limited to only few gpus.
// This should be fixed/updated later to account for disabled gpus.
bool NeedThreadSafeAndroidMedia() {
  return IsDrDcEnabled() || IsUsingThreadSafeMediaForWebView();
}

namespace {
bool IsSkiaGraphiteSupportedByDevice(const base::CommandLine* command_line) {
#if BUILDFLAG(IS_APPLE)
  // Graphite only works well with ANGLE Metal on Mac or iOS.
  // TODO(crbug.com/40063538): Remove this after ANGLE Metal launches fully.
  const bool is_angle_metal_enabled =
      UsePassthroughCommandDecoder() &&
      (base::FeatureList::IsEnabled(features::kDefaultANGLEMetal) ||
       command_line->GetSwitchValueASCII(switches::kUseANGLE) ==
           gl::kANGLEImplementationMetalName);
  if (!is_angle_metal_enabled) {
    return false;
  }
#if BUILDFLAG(IS_MAC)
  // The following code tries to match angle::IsMetalRendererAvailable().
  auto model_name_split = base::SysInfo::SplitHardwareModelNameDoNotUse(
      base::SysInfo::HardwareModelName());
  if (model_name_split.has_value()) {
    // We hardcode the minimum model numbers supporting Mac2 Metal GPU family
    // since ANGLE Metal requires that. We can't check if ANGLE uses Metal until
    // we initialize the GPU process, but this code runs in the browser so we
    // just do our best here to skip the feature check below if we know that
    // ANGLE can't possibly use Metal since we don't want to contaminate the
    // experiment arms with devices that won't run Graphite. Any models not in
    // the list are those that support Mac2 GPU family universally e.g. Mac
    // Mini/Studio. The 5K Retina iMac15,1 is special as it has a discrete GPU
    // and can support ANGLE Metal, but its successors can't until iMac17,1.
    const bool is_imac_15_1 = model_name_split->category == "iMac" &&
                              model_name_split->model == 15 &&
                              model_name_split->variant == 1;
    if (!is_imac_15_1) {
      static constexpr struct {
        const char* category;
        int32_t min_supported_model;
      } kModelSupportData[] = {
          {"MacBookPro", 13}, {"MacBookAir", 8}, {"MacBook", 9},
          {"iMac", 17},       {"MacPro", 6},
      };
      for (const auto& [category, min_supported_model] : kModelSupportData) {
        if (model_name_split->category == category) {
          if (model_name_split->model < min_supported_model) {
            return false;
          }
          break;
        }
      }
    }
  }
#endif  // BUILDFLAG(IS_MAC)
  return true;
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  // Graphite on Android and ChromeOS uses the Dawn Vulkan backend. Only enable
  // Graphite if device would already be using Ganesh/Vulkan.
  return IsUsingVulkan();
#elif BUILDFLAG(IS_WIN)
  return true;
#else
  // Disallow Graphite from being enabled via the base::Feature on
  // not-yet-supported platforms to avoid users experiencing undefined behavior,
  // including behavior that might prevent them from being able to return to
  // chrome://flags to disable the feature.
  if (base::FeatureList::IsEnabled(features::kSkiaGraphite)) {
    LOG(ERROR) << "Enabling Graphite on a not-yet-supported platform is "
                  "disallowed for safety";
  }
  return false;
#endif
}
}  // namespace

bool IsSkiaGraphiteEnabled(const base::CommandLine* command_line) {
  // Force disabling graphite if --disable-skia-graphite flag is specified.
  if (command_line->HasSwitch(switches::kDisableSkiaGraphite)) {
    return false;
  }

  // Force Graphite on if --enable-skia-graphite flag is specified.
  if (command_line->HasSwitch(switches::kEnableSkiaGraphite)) {
    return true;
  }

  if (!IsSkiaGraphiteSupportedByDevice(command_line)) {
    // Return early before checking "SkiaGraphite" feature so that devices
    // which don't support graphite are not included in the finch study.
    return false;
  }

  return base::FeatureList::IsEnabled(features::kSkiaGraphite);
}

// Set up such that service side purge depends on the client side purge feature
// being enabled. And enabling service side purge disables client purge
bool EnablePurgeGpuImageDecodeCache() {
  return !base::FeatureList::IsEnabled(kPruneOldTransferCacheEntries);
}
bool EnablePruneOldTransferCacheEntries() {
  return base::FeatureList::IsEnabled(kPruneOldTransferCacheEntries);
}

bool IsCanvasOopRasterizationEnabled() {
#if BUILDFLAG(IS_ANDROID)
  return true;
#else
  return base::FeatureList::IsEnabled(kCanvasOopRasterization);
#endif
}

#if BUILDFLAG(IS_ANDROID)
bool IsAndroidSurfaceControlEnabled() {
  const auto* build_info = base::android::BuildInfo::GetInstance();
  if (IsDeviceBlocked(build_info->device(),
                      kAndroidSurfaceControlDeviceBlocklist.Get()) ||
      (IsDeviceBlocked(build_info->model(),
                       kAndroidSurfaceControlModelBlocklist.Get()) &&
       // Power issue due to pre-rotate in the models has been fixed in S_V2.
       // crbug.com/1328738
       build_info->sdk_int() <= base::android::SDK_VERSION_S)) {
    return false;
  }

  if (!gfx::SurfaceControl::IsSupported())
    return false;

  // We can use surface control only with AImageReader.
  if (!base::android::EnableAndroidImageReader()) {
    return false;
  }

  // SurfaceControl requires at least 3 frames in flight.
  if (LimitAImageReaderMaxSizeToOne())
    return false;

  // On WebView we require thread-safe media to use SurfaceControl
  if (IsUsingThreadSafeMediaForWebView()) {
    // We decouple experiments between ATV and the rest of the users by using
    // different flags here.
    if (base::android::BuildInfo::GetInstance()->is_tv()) {
      return base::FeatureList::IsEnabled(kWebViewSurfaceControlForTV);
    } else {
      return base::FeatureList::IsEnabled(kWebViewSurfaceControl);
    }
  }

  return base::FeatureList::IsEnabled(kAndroidSurfaceControl);
}

// Many devices do not support more than 1 image to be acquired from the
// AImageReader.(crbug.com/1051705). This method returns true for those
// devices. Currently the list of device model names are sent from server side
// via a finch config file. There is a known device MIBOX for which max size
// should be 1 irrespecticve of the feature LimitAImageReaderMaxSizeToOne
// enabled or not. Get() returns default value even if the feature is disabled.
bool LimitAImageReaderMaxSizeToOne() {
  // Always limit image reader to 1 frame for Android TV. Many TVs doesn't work
  // with more than 1 frame and it's very hard to localize which models do.
  if (base::android::BuildInfo::GetInstance()->is_tv()) {
    // For the android Tvs which are in the below list, we are relaxing this
    // restrictions as those are able to create AImageReader with more than 1
    // images. This helps in removing the flickering seen which can happen with
    // only 1 image. Also note that we should use soc_manufacturer instead of
    // manufacturer when available as sometimes manufacturer field gets
    // modified by vendors.

    const auto* build_info = base::android::BuildInfo::GetInstance();

    if (IsDeviceBlocked(
            build_info->soc_manufacturer(),
            kRelaxLimitAImageReaderMaxSizeToOneSoCBlocklist.Get())) {
      return false;
    }
    if (IsDeviceBlocked(
            build_info->manufacturer(),
            kRelaxLimitAImageReaderMaxSizeToOneManufacturerBlocklist.Get())) {
      return false;
    }
    if (IsDeviceBlocked(
            build_info->device(),
            kRelaxLimitAImageReaderMaxSizeToOneDeviceBlocklist.Get())) {
      return false;
    }
    if (IsDeviceBlocked(
            build_info->model(),
            kRelaxLimitAImageReaderMaxSizeToOneModelBlocklist.Get())) {
      return false;
    }

    return true;
  }

  return (IsDeviceBlocked(base::android::BuildInfo::GetInstance()->model(),
                          kLimitAImageReaderMaxSizeToOneBlocklist.Get()));
}

bool IncreaseBufferCountForHighFrameRate() {
  // TODO(crbug.com/40767562): We don't have a way to dynamically adjust number
  // of buffers. So these checks, espeically the RAM one, is to limit the impact
  // of more buffers to devices that can handle them.
  // 8GB of ram with large margin for error.
  constexpr int RAM_8GB_CUTOFF = 7200 * 1024;
  static bool increase =
      base::android::BuildInfo::GetInstance()->sdk_int() >=
          base::android::SdkVersion::SDK_VERSION_R &&
      IsAndroidSurfaceControlEnabled() &&
      base::android::EnableAndroidImageReader() &&
      base::android::SysUtils::AmountOfPhysicalMemoryKB() > RAM_8GB_CUTOFF &&
      base::FeatureList::IsEnabled(kIncreaseBufferCountForHighFrameRate) &&
      !IsDeviceBlocked(base::android::BuildInfo::GetInstance()->device(),
                       kDisableIncreaseBufferCountForHighFrameRate.Get());
  return increase;
}

#endif

// When this flag is enabled, stops using gpu::SyncPointOrderData for sync point
// validation, uses gpu::TaskGraph instead.
// Graph-based validation doesn't require sync point releases are submitted to
// the scheduler prior to their corresponding waits. Therefore it allows to
// remove the synchronous flush done by VerifySyncTokens().
//
// TODO(b/324276400): Work in progress.
BASE_FEATURE(kSyncPointGraphValidation,
             "SyncPointGraphValidation",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSyncPointGraphValidationEnabled() {
  return base::FeatureList::IsEnabled(kSyncPointGraphValidation);
}

}  // namespace features
