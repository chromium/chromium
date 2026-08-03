// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_finch_features.h"

#include <string_view>

#include "base/byte_size.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/synchronization/atomic_flag.h"
#include "build/build_config.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_switches.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_info.h"
#include "base/android/device_info.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace features {
namespace {

SkiaGraphiteFeatureParams g_skia_graphite_feature_params;

base::AtomicFlag& GetGraphiteParamsInitFlag() {
  static base::NoDestructor<base::AtomicFlag> flag;
  return *flag;
}

void InitSkiaGraphiteFeatureParams(const base::Feature* feature) {
  if (GetGraphiteParamsInitFlag().IsSet()) {
    return;
  }

  if (!feature) {
    g_skia_graphite_feature_params = SkiaGraphiteFeatureParams();
    GetGraphiteParamsInitFlag().Set();
    return;
  }

  g_skia_graphite_feature_params.dawn_skip_validation =
      base::FeatureParam<bool>(
          feature, "dawn_skip_validation",
          g_skia_graphite_feature_params.dawn_skip_validation)
          .Get();
  g_skia_graphite_feature_params.dawn_backend_validation =
      base::FeatureParam<bool>(
          feature, "dawn_backend_validation",
          g_skia_graphite_feature_params.dawn_backend_validation)
          .Get();
  g_skia_graphite_feature_params.dawn_backend_debug_labels =
      base::FeatureParam<bool>(
          feature, "dawn_backend_debug_labels",
          g_skia_graphite_feature_params.dawn_backend_debug_labels)
          .Get();
  g_skia_graphite_feature_params.dawn_enable_auto_map =
      base::FeatureParam<bool>(
          feature, "dawn_enable_auto_map",
          g_skia_graphite_feature_params.dawn_enable_auto_map)
          .Get();
  g_skia_graphite_feature_params.max_pending_recordings =
      base::FeatureParam<int>(
          feature, "max_pending_recordings",
          g_skia_graphite_feature_params.max_pending_recordings)
          .Get();
  g_skia_graphite_feature_params.enable_deferred_submit =
      base::FeatureParam<bool>(
          feature, "enable_deferred_submit",
          g_skia_graphite_feature_params.enable_deferred_submit)
          .Get();
  g_skia_graphite_feature_params.enable_msaa_on_newer_intel =
      base::FeatureParam<bool>(
          feature, "enable_msaa_on_newer_intel",
          g_skia_graphite_feature_params.enable_msaa_on_newer_intel)
          .Get();
#if BUILDFLAG(IS_WIN)
  g_skia_graphite_feature_params.dawn_dumpwc_d3d_errors =
      base::FeatureParam<bool>(
          feature, "dawn_dumpwc_d3d_errors",
          g_skia_graphite_feature_params.dawn_dumpwc_d3d_errors)
          .Get();
  g_skia_graphite_feature_params.dawn_disable_d3d_shader_optimizations =
      base::FeatureParam<bool>(
          feature, "dawn_disable_d3d_shader_optimizations",
          g_skia_graphite_feature_params.dawn_disable_d3d_shader_optimizations)
          .Get();
  g_skia_graphite_feature_params.dawn_d3d11_delay_flush =
      base::FeatureParam<bool>(
          feature, "dawn_d3d11_delay_flush",
          g_skia_graphite_feature_params.dawn_d3d11_delay_flush)
          .Get();
  g_skia_graphite_feature_params.flush_d3d11_tile_raster_commands_to_driver =
      base::FeatureParam<bool>(
          feature, "flush_d3d11_tile_raster_commands_to_driver",
          g_skia_graphite_feature_params
              .flush_d3d11_tile_raster_commands_to_driver)
          .Get();
#endif

  GetGraphiteParamsInitFlag().Set();
}

#if BUILDFLAG(IS_ANDROID)
bool IsDeviceBlocked(std::string_view field, std::string_view block_list) {
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

// More aggressive behavior for the shader cache: increase size, and do not
// purge as much in case of memory pressure.
BASE_FEATURE(kAggressiveShaderCacheLimits, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Use android SurfaceControl API for managing display compositor's buffer queue
// and using overlays on Android. Also used by webview to disable surface
// SurfaceControl.
BASE_FEATURE(kAndroidSurfaceControl, base::FEATURE_ENABLED_BY_DEFAULT);

// Hardware Overlays for WebView.
BASE_FEATURE(kWebViewSurfaceControl, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebViewSurfaceControlForTV, base::FEATURE_DISABLED_BY_DEFAULT);

// This is used as default state because it's different for webview and chrome.
// WebView hardcodes this as enabled in AwMainDelegate.
BASE_FEATURE(kWebViewThreadSafeMediaDefault, base::FEATURE_DISABLED_BY_DEFAULT);

// Used to limit AImageReader max queue size to 1 since many devices especially
// android Tv devices do not support more than 1 images.
BASE_FEATURE(kLimitAImageReaderMaxSizeToOne, base::FEATURE_ENABLED_BY_DEFAULT);

// List of devices on which to limit AImageReader max queue size to 1.
const base::FeatureParam<std::string> kLimitAImageReaderMaxSizeToOneBlocklist{
    &kLimitAImageReaderMaxSizeToOne, "LimitAImageReaderMaxSizeToOneBlocklist",
    "MIBOX|*ODROID*"};

// Used to relax the limit of AImageReader max queue size to 1 for Android Tvs.
// Currently for all android tv except the ones in this list will have max
// queue size of 1 image.
BASE_FEATURE(kRelaxLimitAImageReaderMaxSizeToOne,
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
        "RelaxLimitAImageReaderMaxSizeToOneDeviceBlocklist",
        "G08|G10|G17|BRAVIA_CT1"};
const base::FeatureParam<std::string>
    kRelaxLimitAImageReaderMaxSizeToOneModelBlocklist{
        &kRelaxLimitAImageReaderMaxSizeToOne,
        "RelaxLimitAImageReaderMaxSizeToOneModelBlocklist", ""};

#endif

// When enabled, gives GpuChannel/Host its own dedicated Mojo pipe instead
// of associating with an unused IPC::Channel.
BASE_FEATURE(kRemoveGPULegacyIPC, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
// Feature flag to control whether SharedImageStub sequence uses high priority
// on ChromeOS and Linux. Enabled by default.
BASE_FEATURE(kSharedImageStubHighPriority, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enable GPU Rasterization by default. This can still be overridden by
// --enable-gpu-rasterization or --disable-gpu-rasterization.
// DefaultEnableGpuRasterization has launched on Mac, Windows, ChromeOS,
// Android and Linux.
BASE_FEATURE(kDefaultEnableGpuRasterization,
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(USE_WEBGPU_ON_VULKAN_VIA_GL_INTEROP)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enables the use of MSAA in skia on Ice Lake and later intel architectures.

BASE_FEATURE(kEnableMSAAOnNewIntelGPUs,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kNoUndamagedOverlayPromotion, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_IOS)
// If enabled, the TASK_CATEGORY_POLICY value of the GPU process will be
// adjusted to match the one from the browser process every time it changes.
BASE_FEATURE(kAdjustGpuProcessPriority, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// When enabled, Grshader disk cache will be cleared on startup if any cache
// entry prefix does not match with the current prefix. prefix is made up of
// various parameters like chrome version, driver version etc.
BASE_FEATURE(kClearGrShaderDiskCacheOnInvalidPrefix,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Chrome will use the shader disk cache. This feature provides a
// kill-switch for working around issues with the disk cache and assessing the
// performance value of the disk cache. The --disable-gpu-shader-disk-cache flag
// overrides this feature and forces the disk cache to be disabled.
BASE_FEATURE(kGpuShaderDiskCache, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsShaderDiskCacheEnabled(const base::CommandLine* command_line) {
  if (command_line->HasSwitch(switches::kDisableGpuShaderDiskCache)) {
    return false;
  }

  return base::FeatureList::IsEnabled(kGpuShaderDiskCache);
}

// Enable Vulkan graphics backend for compositing and rasterization. Defaults to
// native implementation if --use-vulkan flag is not used. Otherwise
// --use-vulkan will be followed.
// Note Android WebView uses kWebViewDrawFunctorUsesVulkan instead of this.
BASE_FEATURE(kVulkan,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Force enable WebGPU interop when enabled. When disabled the webgpu interop
// mechanism will default to auto detection in 'GetWebGPUOnVulkanViaGLInterop'
// function.
BASE_FEATURE(kForceEnableWebGpuInterop, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableDrDc,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#elif BUILDFLAG(IS_MAC)
             // DrDC will not be running if Graphite is disabled on Mac.
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             // NOT SUPPORTED. DO NOT ENABLE!
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enable WebGPU on gpu service side only. This is used with origin trial and
// enabled by default on supported platforms.
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(USE_WEBGPU_ON_VULKAN_VIA_GL_INTEROP)
#define WEBGPU_ENABLED base::FEATURE_ENABLED_BY_DEFAULT
#else
#define WEBGPU_ENABLED base::FEATURE_DISABLED_BY_DEFAULT
#endif
BASE_FEATURE(kWebGPUService, WEBGPU_ENABLED);
BASE_FEATURE(kWebGPUBlobCache, WEBGPU_ENABLED);
#undef WEBGPU_ENABLED

// Feature enforces WebGPU security in Android Advanced Protection Mode.
// Disable feature by default for Finch testing.
BASE_FEATURE(kAAPMBlocksWebGPU, base::FEATURE_ENABLED_BY_DEFAULT);

// List of Dawn toggles for WebGPU, delimited by ,
// The FeatureParam may be overridden via Finch config, or via the command line
// For example:
//   --enable-field-trial-config \
//   --force-fieldtrial-params=WebGPU.Enabled:DisabledToggles/toggle1%2Ctoggle2
// Note that the comma should be URL-encoded.
const base::FeatureParam<std::string> kWebGPUDisabledToggles{
    &kWebGPUService, "DisabledToggles", ""};
const base::FeatureParam<std::string> kWebGPUEnabledToggles{
    &kWebGPUService, "EnabledToggles", ""};
// List of WebGPU feature names, delimited by ,
// The FeatureParam may be overridden via Finch config, or via the command line
// For example:
//   --enable-field-trial-config \
//   --force-fieldtrial-params=WebGPU.Enabled:UnsafeFeatures/timestamp-query%2Cshader-f16
// Note that the comma should be URL-encoded.
const base::FeatureParam<std::string> kWebGPUUnsafeFeatures{
    &kWebGPUService, "UnsafeFeatures", ""};
// Whether to enable Dawn's spontaneous wire mode on the server side for faster
// async resolution and timed wait any on the client side.
const base::FeatureParam<bool> kWebGPUSpontaneousWireServer{
    &kWebGPUService, "DawnSpontaneousWireServer", true};
// List of WGSL feature names, delimited by ,
// The FeatureParam may be overridden via Finch config, or via the command line
// For example:
//   --enable-field-trial-config \
//   --force-fieldtrial-params=WebGPU.Enabled:UnsafeWGSLFeatures/feature_1%2Cfeature_2
// Note that the comma should be URL-encoded.
const base::FeatureParam<std::string> kWGSLUnsafeFeatures{
    &kWebGPUService, "UnsafeWGSLFeatures", ""};

BASE_FEATURE(kWebGPUEnableRangeAnalysisForRobustness,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebGPUUseSpirv14, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebGPUDecomposeUniformBuffers, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebGPUUseHLSL2021, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebGPUUseSpirvReconvergenceMode,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)

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

// Enable Skia Graphite with the platform's default Dawn backend.
// Note: This can be overridden by --enable-skia-graphite and
// --disable-skia-graphite which take precedence over the feature flag, and the
// Dawn backend can be overridden with the --skia-graphite-dawn-backend flag.
BASE_FEATURE(kSkiaGraphite,
#if BUILDFLAG(IS_APPLE)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Controls Skia Graphite specifically for Intel GPUs on Windows.
// On Windows, the status of Graphite on Intel GPUs won't be controlled
// by the standard SkiaGraphite feature, but by this feature flag
// instead. This feature only works if `kLateGraphiteFeatureCheck` is
// also enabled.
BASE_FEATURE(kSkiaGraphiteWinIntel,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows CompoundImageBacking to allocate backings during runtime if a
// compatible backing to serve clients requested usage is not already present.
BASE_FEATURE(kUseDynamicBackingAllocations, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, this feature allows ClientSharedImage to store and use a
// scoped_refptr to SharedImageInterface, instead of the raw_ptr as used in
// SharedImageInterfaceHolder.
BASE_FEATURE(kUseStrongRefToSharedImageInterface,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable atlasing of small paths on Skia Graphite. Only meaningful if
// SkiaGraphite is also enabled.
BASE_FEATURE(kSkiaGraphiteSmallPathAtlas, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the Graphite feature check (including blocklist) is deferred to
// the GPU process rather than evaluated in the browser process.
BASE_FEATURE(kLateGraphiteFeatureCheck,
#if BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enable Skia Graphite's Pipeline precompilation feature.
// Note: This is only meaningful when Skia Graphite is enabled but can then also
// be overridden by
// --enable-skia-graphite-precompilation and
// --disable-skia-graphite-precompilation.
BASE_FEATURE(kSkiaGraphitePrecompilation, base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to use PersistentCache for Skia Graphite's pipeline cache.
BASE_FEATURE(kSkiaGraphiteUsePersistentCache,
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

bool SkiaGraphiteUsesPersistentCache() {
  return base::FeatureList::IsEnabled(kSkiaGraphiteUsePersistentCache);
}

BASE_FEATURE(kConditionallySkipGpuChannelFlush,
// To enable on ChromeOS, test failures must be investigated
// (crrev.com/c/5435673).
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

const SkiaGraphiteFeatureParams& GetSkiaGraphiteFeatureParams() {
  DCHECK(GetGraphiteParamsInitFlag().IsSet());
  return g_skia_graphite_feature_params;
}

void InitSkiaGraphiteDefaultParamsForTesting() {
  InitSkiaGraphiteFeatureParams(nullptr);
}

const base::FeatureParam<int> kSkiaGraphiteMinPathSizeForMsaa{
    &kSkiaGraphiteSmallPathAtlas, "min_path_size_for_msaa", 0};

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kSkiaGraphiteDawnUseD3D12, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Whether to use the GpuPersistentCache for caching GPU process shader blobs.
// Usage for Graphite is controlled independently with
// kSkiaGraphiteDawnUsePersistentCache.
BASE_FEATURE(kGpuPersistentCache,
#if BUILDFLAG(IS_WIN)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kGpuPersistentCacheMetadata, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kGpuPersistentCacheMetadataPreloadCount{
    &kGpuPersistentCacheMetadata, "preload_count", 50};

// Use a 100-command limit before forcing context switch per command buffer
// instead of 20.
BASE_FEATURE(kIncreasedCmdBufferParseSlice, base::FEATURE_DISABLED_BY_DEFAULT);

// Prune transfer cache entries not accessed recently. This also turns off
// similar logic in cc::GpuImageDecodeCache which is the largest (often single)
// client of transfer cache.
BASE_FEATURE(kPruneOldTransferCacheEntries, base::FEATURE_ENABLED_BY_DEFAULT);

// On platforms with delegated compositing, try to release overlays later, when
// no new frames are swapped.
BASE_FEATURE(kDeferredOverlaysRelease,
             "DeferredOverlayRelease",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This feature allows enabling specific entries in
// software_rendering_list.json, via experimentation. The entries must have
// test_group property and test_group feature parameter should be set in the
// experiment for the entries that need to be enabled.
BASE_FEATURE(kGPUBlockListTestGroup, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kGPUBlockListTestGroupId{&kGPUBlockListTestGroup,
                                                       "test_group", 0};

// This feature allows enabling specific entries in gpu_driver_bug_list.json,
// via experimentation. The entries must have test_group property and
// test_group feature parameter should be set in the experiment for the entries
// that need to be enabled.
BASE_FEATURE(kGPUDriverBugListTestGroup, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kGPUDriverBugListTestGroupId{
    &kGPUDriverBugListTestGroup, "test_group", 0};

#if BUILDFLAG(IS_LINUX)
bool IsForceEnableWebGpuInterop() {
  return base::FeatureList::IsEnabled(kForceEnableWebGpuInterop);
}
#endif

bool IsUsingVulkan() {
#if BUILDFLAG(IS_ANDROID)
  // Force on if Vulkan feature is enabled from command line.
  base::FeatureList* feature_list = base::FeatureList::GetInstance();
  if (feature_list &&
      feature_list->IsFeatureOverriddenFromCommandLine(
          features::kVulkan.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE)) {
    return true;
  }

  // WebView checks, which do not use (and disables) kVulkan.
  // Do this above the Android version check because there are test devices
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebViewDrawFunctorUsesVulkan)) {
    return true;
  }
#endif

#if BUILDFLAG(ENABLE_VULKAN)
  return base::FeatureList::IsEnabled(kVulkan);
#else
  return false;
#endif
}

bool IsUsingThreadSafeMediaForWebView() {
#if BUILDFLAG(IS_ANDROID)
  // Thread safe media code currently
  // requires AImageReader max size to be at least 2 since one image could be
  // accessed by each gpu thread in webview.
  if (LimitAImageReaderMaxSizeToOne()) {
    return false;
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
  // If GpuFeatureInfo is available, replace ShouldEnableDrDc() with
  // IsDrDcEnabled(gpu_feature_info) which is set after checking drdc
  // workarounds;
  return ShouldEnableDrDc() || IsUsingThreadSafeMediaForWebView();
}

namespace {
bool IsSkiaGraphiteSupportedByDevice(const base::CommandLine* command_line) {
#if BUILDFLAG(IS_APPLE)
  // Graphite only works well with ANGLE Metal on Mac or iOS.
  // TODO(https://crbug.com/40063538): Remove this after ANGLE Metal launches
  // fully.
  const bool is_angle_metal_selected =
      base::FeatureList::IsEnabled(features::kDefaultANGLEMetal) ||
      command_line->GetSwitchValueASCII(switches::kUseANGLE) ==
          gl::kANGLEImplementationMetalName;
  return UsePassthroughCommandDecoder() && is_angle_metal_selected;
#elif BUILDFLAG(IS_ANDROID)
  // Desktop Android isn't ready to pick up the fieldtrial_testing_config.json
  // change that enables graphite. However, it's the same platform as regular
  // Android and does. Skip enabling the feature there for now.
  if (base::android::device_info::is_desktop()) {
    return false;
  }

  // Graphite on Android uses the Dawn Vulkan backend. Only enable Graphite if
  // device would already be using Ganesh/Vulkan.
  return IsUsingVulkan();
#elif BUILDFLAG(IS_CHROMEOS)
  // Graphite on ChromeOS uses the Dawn Vulkan backend. Only enable Graphite if
  // device would already be using Ganesh/Vulkan.
  return IsUsingVulkan();
#elif BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
  // Graphite on Windows ARM requires further research.
  return false;
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
    // The flag enable-skia-graphite is for testing purpose, so set the params
    // to default values.
    InitSkiaGraphiteFeatureParams(nullptr);
    return true;
  }

  if (!IsSkiaGraphiteSupportedByDevice(command_line)) {
    // Return early before checking "SkiaGraphite" feature so that devices
    // which don't support graphite are not included in the finch study.
    return false;
  }

  if (base::FeatureList::IsEnabled(kSkiaGraphite)) {
    InitSkiaGraphiteFeatureParams(&kSkiaGraphite);
    return true;
  }
  return false;
}

bool IsSkiaGraphiteWinIntelEnabled() {
  if (base::FeatureList::IsEnabled(kSkiaGraphiteWinIntel)) {
    InitSkiaGraphiteFeatureParams(&kSkiaGraphiteWinIntel);
    return true;
  }
  return false;
}

bool IsDrDcEnabled(const gpu::GpuFeatureInfo& gpu_feature_info) {
  return gpu_feature_info.status_values
             [gpu::GPU_FEATURE_TYPE_DIRECT_RENDERING_DISPLAY_COMPOSITOR] ==
         gpu::kGpuFeatureStatusEnabled;
}

bool ShouldEnableDrDc() {
#if BUILDFLAG(IS_ANDROID)
  // Enabled on android P+.
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_P) {
    return false;
  }

  // DrDc requires AImageReader max size to be
  // at least 2 for each gpu thread. Hence DrDc is disabled on devices which has
  // only 1 image.
  if (LimitAImageReaderMaxSizeToOne()) {
    return false;
  }

  // Check block list against build info.
  if (IsDeviceBlocked(base::android::android_info::device(),
                      kDrDcBlockListByDevice.Get())) {
    return false;
  }
  if (IsDeviceBlocked(base::android::android_info::model(),
                      kDrDcBlockListByModel.Get())) {
    return false;
  }
  if (IsDeviceBlocked(base::android::android_info::hardware(),
                      kDrDcBlockListByHardware.Get())) {
    return false;
  }
  if (IsDeviceBlocked(base::android::android_info::brand(),
                      kDrDcBlockListByBrand.Get())) {
    return false;
  }
  if (IsDeviceBlocked(base::android::android_info::android_build_id(),
                      kDrDcBlockListByAndroidBuildId.Get())) {
    return false;
  }
  if (IsDeviceBlocked(base::android::android_info::manufacturer(),
                      kDrDcBlockListByManufacturer.Get())) {
    return false;
  }
  if (IsDeviceBlocked(base::android::android_info::board(),
                      kDrDcBlockListByBoard.Get())) {
    return false;
  }
  if (IsDeviceBlocked(base::android::android_info::android_build_fp(),
                      kDrDcBlockListByAndroidBuildFP.Get())) {
    return false;
  }
#endif

  return base::FeatureList::IsEnabled(kEnableDrDc);
}

bool IsSkiaGraphitePrecompilationEnabled(
    const base::CommandLine* command_line) {
  // Force disabling Graphite Precompilation if
  // --disable-skia-graphite-precompilation flag is specified.
  if (command_line->HasSwitch(switches::kDisableSkiaGraphitePrecompilation)) {
    return false;
  }

  // Force Graphite Precompilation on if --enable-skia-graphite-precompilation
  // flag is specified.
  if (command_line->HasSwitch(switches::kEnableSkiaGraphitePrecompilation)) {
    return true;
  }

  return base::FeatureList::IsEnabled(features::kSkiaGraphitePrecompilation);
}

bool EnablePruneOldTransferCacheEntries() {
  return base::FeatureList::IsEnabled(kPruneOldTransferCacheEntries);
}

bool IsLegacyIpcDisabled() {
  return base::FeatureList::IsEnabled(kRemoveGPULegacyIPC);
}

#if BUILDFLAG(IS_ANDROID)
bool IsAndroidSurfaceControlEnabled() {
  if (base::android::android_info::sdk_int() <=
          base::android::android_info::SDK_VERSION_S &&
      (IsDeviceBlocked(base::android::android_info::device(), "capri|caprip") ||
       IsDeviceBlocked(base::android::android_info::model(),
                       "SM-F9*|SM-W202?|SCV44|SCG05|SCG11|SC-55B"))) {
    return false;
  }

  if (!gfx::SurfaceControl::IsSupported())
    return false;

  // SurfaceControl requires at least 3 frames in flight.
  if (LimitAImageReaderMaxSizeToOne())
    return false;

  // Check conditions that egl_android_native_fence_sync is enabled.
  // LINT.IfChange(AndroidSurfaceControlCondition)
  if (base::SysInfo::GetAndroidHardwareEGL() == "swiftshader" ||
      base::SysInfo::GetAndroidHardwareEGL() == "emulation") {
    return false;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAndroidNativeFenceSyncForTesting)) {
    return false;
  }
  // LINT.ThenChange(//ui/gl/gl_display.cc:AndroidSurfaceControlCondition)

  // On WebView we require thread-safe media to use SurfaceControl
  if (IsUsingThreadSafeMediaForWebView()) {
    // MediaTek devices have problems of importing overlay-able images to the
    // Vulkan. Don't use SurfaceControl there until this is resolved.
    if (base::StartsWith(base::android::android_info::model(), "mt") &&
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kWebViewDrawFunctorUsesVulkan)) {
      return false;
    }

    // We decouple experiments between ATV and the rest of the users by using
    // different flags here.
    if (base::android::device_info::is_tv()) {
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
  // The feature is enabled by default, if it was overridden by user we should
  // not limit regardless if it will work or not.
  base::FeatureList* feature_list = base::FeatureList::GetInstance();
  if (feature_list && feature_list->IsFeatureOverriddenFromCommandLine(
                          kLimitAImageReaderMaxSizeToOne.name)) {
    return base::FeatureList::IsEnabled(kLimitAImageReaderMaxSizeToOne);
  }

  // Always limit image reader to 1 frame for Android TV. Many TVs doesn't work
  // with more than 1 frame and it's very hard to localize which models do.
  if (base::android::device_info::is_tv()) {
    // For the android Tvs which are in the below list, we are relaxing this
    // restrictions as those are able to create AImageReader with more than 1
    // images. This helps in removing the flickering seen which can happen with
    // only 1 image. Also note that we should use soc_manufacturer instead of
    // manufacturer when available as sometimes manufacturer field gets
    // modified by vendors.

    if (IsDeviceBlocked(
            base::android::android_info::soc_manufacturer(),
            kRelaxLimitAImageReaderMaxSizeToOneSoCBlocklist.Get())) {
      return false;
    }
    if (IsDeviceBlocked(
            base::android::android_info::manufacturer(),
            kRelaxLimitAImageReaderMaxSizeToOneManufacturerBlocklist.Get())) {
      return false;
    }
    if (IsDeviceBlocked(
            base::android::android_info::device(),
            kRelaxLimitAImageReaderMaxSizeToOneDeviceBlocklist.Get())) {
      return false;
    }
    if (IsDeviceBlocked(
            base::android::android_info::model(),
            kRelaxLimitAImageReaderMaxSizeToOneModelBlocklist.Get())) {
      return false;
    }

    return true;
  }

  return (IsDeviceBlocked(base::android::android_info::model(),
                          kLimitAImageReaderMaxSizeToOneBlocklist.Get()));
}

bool IncreaseBufferCountForHighFrameRate() {
  // TODO(crbug.com/40767562): We don't have a way to dynamically adjust number
  // of buffers. So these checks, espeically the RAM one, is to limit the impact
  // of more buffers to devices that can handle them.
  // 8GB of ram with large margin for error.
  constexpr base::ByteSize RAM_8GB_CUTOFF = base::MiBU(7200);
  static bool increase =
      base::android::android_info::sdk_int() >=
          base::android::android_info::SDK_VERSION_R &&
      IsAndroidSurfaceControlEnabled() &&
      base::SysInfo::AmountOfTotalPhysicalMemory() > RAM_8GB_CUTOFF;
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
BASE_FEATURE(kSyncPointGraphValidation, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSyncPointGraphValidationEnabled() {
  return base::FeatureList::IsEnabled(kSyncPointGraphValidation);
}

BASE_FEATURE(kANGLEPerContextBlobCache, base::FEATURE_DISABLED_BY_DEFAULT);

// Support thread safety for graphite::context by sharing the same
// graphite::context as well as its wrapper class GraphiteSharedContext between
// GpuMain and CompositorGpuThread. Note: When this feature is disabled,
// each thread creates its own graphite::context and the context wrapper.
BASE_FEATURE(kGraphiteContextIsThreadSafe,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGraphiteContextThreadSafe() {
  return base::FeatureList::IsEnabled(features::kGraphiteContextIsThreadSafe);
}

BASE_FEATURE(kWebGPUCompatibilityMode, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebGPUAndroidOpenGLES, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kWebGPUQualcommWindows, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Enables runtime configuration of the GPU watchdog timeout via
// experimentation.
BASE_FEATURE(kConfigurableGPUWatchdogTimeout,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kConfigurableGPUWatchdogTimeoutSeconds{
    &kConfigurableGPUWatchdogTimeout, "watchdog_timeout_seconds", 30};

// Enables the optimization where GPU channels are sent to renderer processes
// early when the renderer process is being initialized, instead of waiting
// for the renderer to request the GPU channel to the browser process.
BASE_FEATURE(kSendGPUChannelEarly, base::FEATURE_DISABLED_BY_DEFAULT);
// If true, only enable the early GPU channel optimization for topchrome WebUI
// renderers.
const base::FeatureParam<bool> kSendGPUChannelEarlyTopChromeOnly{
    &kSendGPUChannelEarly, "for_topchrome_webui_only", false};

}  // namespace features
