// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_finch_features.h"

#include "base/command_line.h"
#include "build/chromeos_buildflags.h"
#include "gpu/config/gpu_switches.h"

#if defined(OS_ANDROID)
#include "base/android/android_image_reader_compat.h"
#include "base/android/build_info.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#endif

namespace features {
namespace {

#if defined(OS_ANDROID)
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
#endif

}  // namespace

#if defined(OS_ANDROID)
// Used to limit GL version to 2.0 for skia raster on Android.
const base::Feature kUseGles2ForOopR{"UseGles2ForOopR",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Use android SurfaceControl API for managing display compositor's buffer queue
// and using overlays on Android. Also used by webview to disable surface
// SurfaceControl.
const base::Feature kAndroidSurfaceControl{"AndroidSurfaceControl",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// https://crbug.com/1176185 List of devices on which SurfaceControl should be
// disabled.
const base::FeatureParam<std::string> kAndroidSurfaceControlBlocklist{
    &kAndroidSurfaceControl, "AndroidSurfaceControlBlocklist", "capri|caprip"};

// Use AImageReader for MediaCodec and MediaPlyer on android.
const base::Feature kAImageReader{"AImageReader",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// If webview-draw-functor-uses-vulkan is set, use vulkan for composite and
// raster.
const base::Feature kWebViewVulkan{"WebViewVulkan",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// Used to enable/disable zero copy video path on webview for MCVD.
const base::Feature kWebViewZeroCopyVideo{"WebViewZeroCopyVideo",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// List of devices on which WebViewZeroCopyVideo should be disabled.
const base::FeatureParam<std::string> kWebViewZeroCopyVideoBlocklist{
    &kWebViewZeroCopyVideo, "WebViewZeroCopyVideoBlocklist", ""};

// Used to limit AImageReader max queue size to 1 since many devices especially
// android Tv devices do not support more than 1 images.
const base::Feature kLimitAImageReaderMaxSizeToOne{
    "LimitAImageReaderMaxSizeToOne", base::FEATURE_ENABLED_BY_DEFAULT};

// List of devices on which to limit AImageReader max queue size to 1.
const base::FeatureParam<std::string> kLimitAImageReaderMaxSizeToOneBlocklist{
    &kLimitAImageReaderMaxSizeToOne, "LimitAImageReaderMaxSizeToOneBlocklist",
    "MIBOX"};
#endif

// Enable GPU Rasterization by default. This can still be overridden by
// --enable-gpu-rasterization or --disable-gpu-rasterization.
// DefaultEnableGpuRasterization has launched on Mac, Windows, ChromeOS, and
// Android.
const base::Feature kDefaultEnableGpuRasterization{
  "DefaultEnableGpuRasterization",
#if defined(OS_MAC) || defined(OS_WIN) || BUILDFLAG(IS_ASH) || \
    defined(OS_ANDROID) || defined(OS_FUCHSIA)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enable out of process rasterization by default.  This can still be overridden
// by --disable-oop-rasterization.
const base::Feature kDefaultEnableOopRasterization{
    "DefaultEnableOopRasterization", base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_WIN)
// Use a high priority for GPU process on Windows.
const base::Feature kGpuProcessHighPriorityWin{
    "GpuProcessHighPriorityWin", base::FEATURE_ENABLED_BY_DEFAULT};

// Compute the root damage rect from the surface damage list for overlays on
// Windows.
const base::Feature kDirectCompositionUseOverlayDamageList{
    "DirectCompositionUseOverlayDamageList", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Use ThreadPriority::DISPLAY for GPU main, viz compositor and IO threads.
const base::Feature kGpuUseDisplayThreadPriority{
  "GpuUseDisplayThreadPriority",
#if defined(OS_ANDROID) || BUILDFLAG(IS_ASH) || defined(OS_WIN)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Use a different set of watchdog timeouts on V2
const base::Feature kGpuWatchdogV2NewTimeout{"GpuWatchdogV2NewTimeout",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_MAC)
// Enable use of Metal for OOP rasterization.
const base::Feature kMetal{"Metal", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Turns on skia deferred display list for out of process raster.
const base::Feature kOopRasterizationDDL{"OopRasterizationDDL",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Causes us to use the SharedImageManager, removing support for the old
// mailbox system. Any consumers of the GPU process using the old mailbox
// system will experience undefined results.
const base::Feature kSharedImageManager{"SharedImageManager",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Controls the decode acceleration of JPEG images (as opposed to camera
// captures) in Chrome OS using the VA-API.
// TODO(andrescj): remove or enable by default in Chrome OS once
// https://crbug.com/868400 is resolved.
const base::Feature kVaapiJpegImageDecodeAcceleration{
    "VaapiJpegImageDecodeAcceleration", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls the decode acceleration of WebP images in Chrome OS using the
// VA-API.
// TODO(gildekel): remove or enable by default in Chrome OS once
// https://crbug.com/877694 is resolved.
const base::Feature kVaapiWebPImageDecodeAcceleration{
    "VaapiWebPImageDecodeAcceleration", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable Vulkan graphics backend for compositing and rasterization. Defaults to
// native implementation if --use-vulkan flag is not used. Otherwise
// --use-vulkan will be followed.
// Note Android WebView uses kWebViewVulkan instead of this.
const base::Feature kVulkan{"Vulkan", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable SkiaRenderer Dawn graphics backend. On Windows this will use D3D12,
// and on Linux this will use Vulkan.
const base::Feature kSkiaDawn{"SkiaDawn", base::FEATURE_DISABLED_BY_DEFAULT};

// Used to enable shared image mailbox and disable legacy texture mailbox on
// webview.
const base::Feature kEnableSharedImageForWebview{
    "EnableSharedImageForWebview", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable GrShaderCache to use with Vulkan backend.
const base::Feature kEnableGrShaderCacheForVulkan{
    "EnableGrShaderCacheForVulkan", base::FEATURE_ENABLED_BY_DEFAULT};

bool IsUsingVulkan() {
  bool enable = base::FeatureList::IsEnabled(kVulkan);
#if defined(OS_ANDROID)
  enable = enable || (base::CommandLine::ForCurrentProcess()->HasSwitch(
                          switches::kWebViewDrawFunctorUsesVulkan) &&
                      base::FeatureList::IsEnabled(kWebViewVulkan));
#endif
  return enable;
}

#if defined(OS_ANDROID)
bool IsAImageReaderEnabled() {
  return base::FeatureList::IsEnabled(kAImageReader) &&
         base::android::AndroidImageReader::GetInstance().IsSupported();
}

bool IsAndroidSurfaceControlEnabled() {
  const auto* build_info = base::android::BuildInfo::GetInstance();
  auto disable_patterns =
      base::SplitString(kAndroidSurfaceControlBlocklist.Get(), "|",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& disable_pattern : disable_patterns) {
    if (base::MatchPattern(build_info->device(), disable_pattern))
      return false;
  }

  return IsAImageReaderEnabled() &&
         base::FeatureList::IsEnabled(kAndroidSurfaceControl) &&
         gfx::SurfaceControl::IsSupported();
}

// Many devices do not support more than 1 image to be acquired from the
// AImageReader.(crbug.com/1051705). This method returns true for those
// devices. Currently the list of device model names are sent from server side
// via a finch config file. There is a known device MIBOX for which max size
// should be 1 irrespecticve of the feature LimitAImageReaderMaxSizeToOne
// enabled or not. Get() returns default value even if the feature is disabled.
bool LimitAImageReaderMaxSizeToOne() {
  return (FieldIsInBlocklist(base::android::BuildInfo::GetInstance()->model(),
                             kLimitAImageReaderMaxSizeToOneBlocklist.Get()));
}

// Zero copy is disabled if device can not support 3 max images.
bool IsWebViewZeroCopyVideoEnabled() {
  const bool limit_max_size_to_one = LimitAImageReaderMaxSizeToOne();
  if (!IsAImageReaderEnabled() || limit_max_size_to_one)
    return false;

  if (!base::FeatureList::IsEnabled(kWebViewZeroCopyVideo))
    return false;

  return !(FieldIsInBlocklist(base::android::BuildInfo::GetInstance()->model(),
                              kWebViewZeroCopyVideoBlocklist.Get()));
}

#endif

}  // namespace features
