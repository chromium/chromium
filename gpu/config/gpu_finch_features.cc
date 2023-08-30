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
#endif

namespace features {
namespace {

#if BUILDFLAG(IS_ANDROID)
bool FieldIsInBlocklist(const char* current_value, std::string blocklist_str) {
  std::vector<std::string> blocklist = base::SplitString(
      blocklist_str, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const std::string& blocklisted_value : blocklist) {
    if (base::StartsWith(current_value, blocklisted_value,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return true;
    }
  }
  return false;
}

bool IsDeviceBlocked(const char* field, const std::string& block_list) {
  auto disable_patterns = base::SplitString(
      block_list, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& disable_pattern : disable_patterns) {
    if (base::MatchPattern(field, disable_pattern))
      return true;
  }
  return false;
}

#endif

}  // namespace

// Used to limit GL version to 2.0 for skia raster and compositing.
BASE_FEATURE(kUseGles2ForOopR,
             "UseGles2ForOopR",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

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

// Use thread-safe media path on WebView.
BASE_FEATURE(kWebViewThreadSafeMedia,
             "WebViewThreadSafeMedia",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This is used as default state because it's different for webview and chrome.
// WebView hardcodes this as enabled in AwMainDelegate.
BASE_FEATURE(kWebViewThreadSafeMediaDefault,
             "WebViewThreadSafeMediaDefault",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use AImageReader for MediaCodec and MediaPlyer on android.
BASE_FEATURE(kAImageReader, "AImageReader", base::FEATURE_ENABLED_BY_DEFAULT);

// If webview-draw-functor-uses-vulkan is set, use vulkan for composite and
// raster.
BASE_FEATURE(kWebViewVulkan, "WebViewVulkan", base::FEATURE_ENABLED_BY_DEFAULT);

// Used to limit AImageReader max queue size to 1 since many devices especially
// android Tv devices do not support more than 1 images.
BASE_FEATURE(kLimitAImageReaderMaxSizeToOne,
             "LimitAImageReaderMaxSizeToOne",
             base::FEATURE_ENABLED_BY_DEFAULT);

// List of devices on which to limit AImageReader max queue size to 1.
const base::FeatureParam<std::string> kLimitAImageReaderMaxSizeToOneBlocklist{
    &kLimitAImageReaderMaxSizeToOne, "LimitAImageReaderMaxSizeToOneBlocklist",
    "MIBOX|*ODROID*"};

// Increase number of buffers and pipeline depth for high frame rate devices.
BASE_FEATURE(kIncreaseBufferCountForHighFrameRate,
             "IncreaseBufferCountForHighFrameRate",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<std::string>
    kDisableIncreaseBufferCountForHighFrameRate{
        &kIncreaseBufferCountForHighFrameRate,
        "DisableIncreaseBufferCountForHighFrameRate", ""};
#endif

// Use shorter timeout when performDeferredCleanup, and enable
// performDeferredCleanup for Android WebView.
BASE_FEATURE(kAggressiveSkiaGpuResourcePurge,
             "AggressiveSkiaGpuResourcePurge",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enables the use of out of process rasterization for canvas.
BASE_FEATURE(kCanvasOopRasterization,
             "CanvasOopRasterization",
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_IOS) || BUILDFLAG(IS_WIN) || \
    (BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_OZONE)
// Detect front buffering condition and set buffer usage as such.
// This is a killswitch to be removed once launched.
BASE_FEATURE(kOzoneFrontBufferUsage,
             "OzoneFrontBufferUsage",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_OZONE)

// Enables the use of MSAA in skia on Ice Lake and later intel architectures.
BASE_FEATURE(kEnableMSAAOnNewIntelGPUs,
             "EnableMSAAOnNewIntelGPUs",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the use of ANGLE validation for non-WebGL contexts.
BASE_FEATURE(kDefaultEnableANGLEValidation,
             "DefaultEnableANGLEValidation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disables MSAA in Graphite if MSAA is reported as being slow for the device.
BASE_FEATURE(kDisableSlowMSAAInGraphite,
             "DisableSlowMSAAInGraphite",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables canvas to free its resources by default when it's running in
// the background.
BASE_FEATURE(kCanvasContextLostInBackground,
             "CanvasContextLostInBackground",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// Use a high priority for GPU process on Windows.
BASE_FEATURE(kGpuProcessHighPriorityWin,
             "GpuProcessHighPriorityWin",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Disable overlay promotion for clear video quads when their MPO quad would
// move.
BASE_FEATURE(kDisableVideoOverlayIfMoving,
             "DisableVideoOverlayIfMoving",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNoUndamagedOverlayPromotion,
             "NoUndamagedOverlayPromotion",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use a DCompPresenter as the root surface, instead of a
// DirectCompositionSurfaceWin. DCompPresenter is surface-less and the actual
// allocation of the root surface will be owned by DirectRenderer.
BASE_FEATURE(kDCompPresenter,
             "DCompPresenter",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_IOS)
// If enabled, the TASK_CATEGORY_POLICY value of the GPU process will be
// adjusted to match the one from the browser process every time it changes.
BASE_FEATURE(kAdjustGpuProcessPriority,
             "AdjustGpuProcessPriority",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Causes us to use the SharedImageManager, removing support for the old
// mailbox system. Any consumers of the GPU process using the old mailbox
// system will experience undefined results.
BASE_FEATURE(kSharedImageManager,
             "SharedImageManager",
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
// Note Android WebView uses kWebViewVulkan instead of this.
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

BASE_FEATURE(kForceGpuMainThreadToNormalPriorityDrDc,
             "ForceGpuMainThreadToNormalPriorityDrDc",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kEnableDrDcVulkan,
             "EnableDrDcVulkan",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// Enable WebGPU on gpu service side only. This is used with origin trial and
// enabled by default on supported platforms.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
#define WEBGPU_ENABLED base::FEATURE_ENABLED_BY_DEFAULT
#else
#define WEBGPU_ENABLED base::FEATURE_DISABLED_BY_DEFAULT
#endif
BASE_FEATURE(kWebGPUService, "WebGPUService", WEBGPU_ENABLED);
BASE_FEATURE(kWebGPUBlobCache, "WebGPUBlobCache", WEBGPU_ENABLED);
#undef WEBGPU_ENABLED

BASE_FEATURE(kWebGPUUseDXC, "WebGPUUseDXC", base::FEATURE_DISABLED_BY_DEFAULT);

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

// Enable Skia Graphite. This will use the Dawn backend by default, but can be
// overridden with command line flags for testing on non-official developer
// builds. See --skia-graphite-backend flag in gpu_switches.h.
BASE_FEATURE(kSkiaGraphite,
             "SkiaGraphite",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kSkiaGraphiteDawnUseD3D12,
             "SkiaGraphiteDawnUseD3D12",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enable GrShaderCache to use with Vulkan backend.
BASE_FEATURE(kEnableGrShaderCacheForVulkan,
             "EnableGrShaderCacheForVulkan",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable report only mode on the GPU watchdog instead of pausing the watchdog
// thread during GPU startup.
BASE_FEATURE(kEnableWatchdogReportOnlyModeOnGpuInit,
             "EnableWatchdogReportOnlyModeOnGpuInit",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable persistent storage of VkPipelineCache data.
BASE_FEATURE(kEnableVkPipelineCache,
             "EnableVkPipelineCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable Skia reduceOpsTaskSplitting to reduce render passes.
BASE_FEATURE(kReduceOpsTaskSplitting,
             "ReduceOpsTaskSplitting",
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

// Kill switch for forcing restart GPU with context loss.
// See https://crbug.com/1172229 for detail.
BASE_FEATURE(kForceRestartGpuKillSwitch,
             "ForceRestartGpuKillSwitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Using the new SchedulerDfs GPU scheduler.
BASE_FEATURE(kUseGpuSchedulerDfs,
             "UseGpuSchedulerDfs",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// Use the ClientGmb interface to create GpuMemoryBuffers. This is supposed to
// reduce number of IPCs happening while creating GpuMemoryBuffers by allowing
// Renderers to do IPC directly to GPU process.
BASE_FEATURE(kUseClientGmbInterface,
             "UseClientGmbInterface",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable YUV<->RGB conversion for video clients through passthrough command
// decoder.
BASE_FEATURE(kPassthroughYuvRgbConversion,
             "PassthroughYuvRgbConversion",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When the application is in background, whether to perform immediate GPU
// cleanup when executing deferred requests.
BASE_FEATURE(kGpuCleanupInBackground,
             "GpuCleanupInBackground",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// When enabled, the validating command decoder always returns true
// from IsGL_REDSupportedOnFBOs in feature_info.cc on Android.
BASE_FEATURE(kCmdDecoderSkipGLRedMesaWorkaroundOnAndroid,
             "CmdDecoderSkipGLRedMesaWorkaroundOnAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

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
          switches::kWebViewDrawFunctorUsesVulkan) &&
      base::FeatureList::IsEnabled(kWebViewVulkan)) {
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
  if (!IsAImageReaderEnabled() || LimitAImageReaderMaxSizeToOne())
    return false;

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
  return IsUsingVulkan() ? base::FeatureList::IsEnabled(kEnableDrDcVulkan)
                         : true;
#else
  return false;
#endif
}

bool IsGpuMainThreadForcedToNormalPriorityDrDc() {
  // GPU main thread priority is forced to NORMAL only when DrDc is enabled. In
  // that case DrDc thread continues to use DISPLAY thread priority and hence
  // have higher thread priority than GPU main.
  return IsDrDcEnabled() &&
         base::FeatureList::IsEnabled(kForceGpuMainThreadToNormalPriorityDrDc);
}

bool IsUsingThreadSafeMediaForWebView() {
#if BUILDFLAG(IS_ANDROID)
  // SurfaceTexture can't be thread-safe. Also thread safe media code currently
  // requires AImageReader max size to be at least 2 since one image could be
  // accessed by each gpu thread in webview.
  if (!IsAImageReaderEnabled() || LimitAImageReaderMaxSizeToOne())
    return false;

  // If the feature is overridden from command line or finch we will use its
  // value. If not we use kWebViewThreadSafeMediaDefault which is set in
  // AwMainDelegate for WebView.
  base::FeatureList* feature_list = base::FeatureList::GetInstance();
  if (feature_list &&
      feature_list->IsFeatureOverridden(kWebViewThreadSafeMedia.name))
    return base::FeatureList::IsEnabled(kWebViewThreadSafeMedia);

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

bool IsANGLEValidationEnabled() {
  return base::FeatureList::IsEnabled(kDefaultEnableANGLEValidation) &&
         UsePassthroughCommandDecoder();
}

#if BUILDFLAG(IS_ANDROID)
bool IsAImageReaderEnabled() {
  // Device Hammer_Energy_2 seems to be very crash with image reader during
  // gl::GLImageEGL::BindTexImage(). Disable image reader on that device for
  // now. crbug.com/1323921
  // TODO(crbug.com/1323921): Can we revisit this now that GLImage no longer
  // exists?
  if (IsDeviceBlocked(base::android::BuildInfo::GetInstance()->device(),
                      "Hammer_Energy_2")) {
    return false;
  }

  return base::FeatureList::IsEnabled(kAImageReader) &&
         base::android::AndroidImageReader::GetInstance().IsSupported();
}

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
  if (!IsAImageReaderEnabled())
    return false;

  // SurfaceControl requires at least 3 frames in flight.
  if (LimitAImageReaderMaxSizeToOne())
    return false;

  // On WebView we require thread-safe media to use SurfaceControl
  if (IsUsingThreadSafeMediaForWebView()) {
    return base::FeatureList::IsEnabled(kWebViewSurfaceControl);
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
  if (base::android::BuildInfo::GetInstance()->is_tv())
    return true;

  return (FieldIsInBlocklist(base::android::BuildInfo::GetInstance()->model(),
                             kLimitAImageReaderMaxSizeToOneBlocklist.Get()));
}

bool IncreaseBufferCountForHighFrameRate() {
  // TODO(crbug.com/1211332): We don't have a way to dynamically adjust number
  // of buffers. So these checks, espeically the RAM one, is to limit the impact
  // of more buffers to devices that can handle them.
  // 8GB of ram with large margin for error.
  constexpr int RAM_8GB_CUTOFF = 7200 * 1024;
  static bool increase =
      base::android::BuildInfo::GetInstance()->sdk_int() >=
          base::android::SdkVersion::SDK_VERSION_R &&
      IsAndroidSurfaceControlEnabled() && IsAImageReaderEnabled() &&
      base::android::SysUtils::AmountOfPhysicalMemoryKB() > RAM_8GB_CUTOFF &&
      base::FeatureList::IsEnabled(kIncreaseBufferCountForHighFrameRate) &&
      !IsDeviceBlocked(base::android::BuildInfo::GetInstance()->device(),
                       kDisableIncreaseBufferCountForHighFrameRate.Get());
  return increase;
}

#endif

}  // namespace features
