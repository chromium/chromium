// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the gpu
// module.

#ifndef GPU_CONFIG_GPU_FINCH_FEATURES_H_
#define GPU_CONFIG_GPU_FINCH_FEATURES_H_

#include <type_traits>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "gpu/config/gpu_config_export.h"

namespace base {
class CommandLine;
}  // namespace base

namespace gpu {
struct GpuFeatureInfo;
}  // namespace gpu

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kAggressiveShaderCacheLimits);

#if BUILDFLAG(IS_ANDROID)
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kAndroidSurfaceControl);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebViewSurfaceControl);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebViewSurfaceControlForTV);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kLimitAImageReaderMaxSizeToOne);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebViewThreadSafeMediaDefault);
#endif  // BUILDFLAG(IS_ANDROID)

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kDefaultEnableGpuRasterization);

// Enables dynamic allocation of shared image backings at runtime.
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kUseDynamicBackingAllocations);

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kUseStrongRefToSharedImageInterface);

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kEnableMSAAOnNewIntelGPUs);

#if BUILDFLAG(IS_WIN)
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kNoUndamagedOverlayPromotion);
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_IOS)
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kAdjustGpuProcessPriority);
#endif

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kClearGrShaderDiskCacheOnInvalidPrefix);

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kGpuShaderDiskCache);
GPU_CONFIG_EXPORT bool IsShaderDiskCacheEnabled(
    const base::CommandLine* command_line);

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kVulkan);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kForceEnableWebGpuInterop);

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kSkiaGraphite);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kSkiaGraphiteWinIntel);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kSkiaGraphitePrecompilation);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kSkiaGraphiteUsePersistentCache);
GPU_CONFIG_EXPORT bool SkiaGraphiteUsesPersistentCache();

struct GPU_CONFIG_EXPORT SkiaGraphiteFeatureParams {
  // Whether the Dawn "skip_validation" toggle is enabled for Skia Graphite.
  bool dawn_skip_validation = !DCHECK_IS_ON();

  // Whether Dawn backend validation is enabled for Skia Graphite.
  bool dawn_backend_validation = false;

  // Whether Dawn backend debug labels are enabled for Skia Graphite.
  // Only enable backend labels by default on DCHECK builds since it
  // can have non-trivial performance overhead e.g. with Metal.
  bool dawn_backend_debug_labels = DCHECK_IS_ON();

  // Enables automatic buffer mappings in Dawn's backend.
  bool dawn_enable_auto_map = true;

  // Maximum number of pending recordings before submitting to the GPU.
  int max_pending_recordings = 100;

  // Whether to enable deferred submissions optimization (if possible). If it's
  // false, every SI's access will require a Graphite's Context::submit() call
  // before EndAccess().
  bool enable_deferred_submit = BUILDFLAG(IS_WIN);

  // Enables MSAA on newer Intel GPUs (Gen 11+).
  bool enable_msaa_on_newer_intel = true;

#if BUILDFLAG(IS_WIN)
  // Whether we should DumpWithoutCrashing when D3D related errors are detected.
  bool dawn_dumpwc_d3d_errors = false;

  // Whether to disable D3D shader optimizations.
  bool dawn_disable_d3d_shader_optimizations = false;

  // Whether the Dawn D3D11 flush should be delayed until the end of the frame.
  bool dawn_d3d11_delay_flush = true;
#endif
};

static_assert(std::is_trivially_destructible_v<SkiaGraphiteFeatureParams>);

// Gets the initialized Skia Graphite feature parameters.
// Either IsSkiaGraphiteEnabled() or IsSkiaGraphiteWinIntelEnabled() must be
// called to populate the parameters before calling this function (or
// InitSkiaGraphiteDefaultParamsForTesting() in tests).
GPU_CONFIG_EXPORT const SkiaGraphiteFeatureParams&
GetSkiaGraphiteFeatureParams();

// Allows tests to mark the feature parameters as initialized with their default
// values without needing to call IsSkiaGraphiteEnabled(). Note that once
// initialized, the parameters cannot be overridden by calling
// IsSkiaGraphiteEnabled().
GPU_CONFIG_EXPORT void InitSkiaGraphiteDefaultParamsForTesting();

inline bool SkiaGraphiteDawnSkipValidation() {
  return GetSkiaGraphiteFeatureParams().dawn_skip_validation;
}
inline bool SkiaGraphiteDawnBackendValidation() {
  return GetSkiaGraphiteFeatureParams().dawn_backend_validation;
}
inline bool SkiaGraphiteDawnBackendDebugLabels() {
  return GetSkiaGraphiteFeatureParams().dawn_backend_debug_labels;
}
inline bool SkiaGraphiteDawnEnableAutoMap() {
  return GetSkiaGraphiteFeatureParams().dawn_enable_auto_map;
}
inline int SkiaGraphiteMaxPendingRecordings() {
  return GetSkiaGraphiteFeatureParams().max_pending_recordings;
}
inline bool SkiaGraphiteEnableDeferredSubmit() {
  return GetSkiaGraphiteFeatureParams().enable_deferred_submit;
}
inline bool SkiaGraphiteEnableMSAAOnNewerIntel() {
  return GetSkiaGraphiteFeatureParams().enable_msaa_on_newer_intel;
}

#if BUILDFLAG(IS_WIN)
inline bool SkiaGraphiteDawnDumpWCOnD3DError() {
  return GetSkiaGraphiteFeatureParams().dawn_dumpwc_d3d_errors;
}
inline bool SkiaGraphiteDawnDisableD3DShaderOptimizations() {
  return GetSkiaGraphiteFeatureParams().dawn_disable_d3d_shader_optimizations;
}
inline bool SkiaGraphiteDawnD3D11DelayFlush() {
  return GetSkiaGraphiteFeatureParams().dawn_d3d11_delay_flush;
}

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kSkiaGraphiteDawnUseD3D12);
#endif

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kSkiaGraphiteSmallPathAtlas);
GPU_CONFIG_EXPORT extern const base::FeatureParam<int>
    kSkiaGraphiteMinPathSizeForMsaa;

// When enabled, the Graphite feature check (including blocklist) is deferred to
// the GPU process rather than evaluated in the browser process.
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kLateGraphiteFeatureCheck);

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kGpuPersistentCache);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kGpuPersistentCacheMetadata);
GPU_CONFIG_EXPORT extern const base::FeatureParam<int>
    kGpuPersistentCacheMetadataPreloadCount;

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kConditionallySkipGpuChannelFlush);

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kEnableDrDc);

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kPruneOldTransferCacheEntries);

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kRemoveGPULegacyIPC);

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kSharedImageStubHighPriority);
#endif

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebGPUService);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kAAPMBlocksWebGPU);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebGPUBlobCache);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebGPUEnableRangeAnalysisForRobustness);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebGPUAndroidOpenGLES);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebGPUUseSpirv14);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebGPUDecomposeUniformBuffers);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebGPUUseHLSL2021);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebGPUUseSpirvReconvergenceMode);
#if BUILDFLAG(IS_WIN)
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebGPUQualcommWindows);
#endif
GPU_CONFIG_EXPORT extern const base::FeatureParam<std::string>
    kWebGPUDisabledToggles;
GPU_CONFIG_EXPORT extern const base::FeatureParam<std::string>
    kWebGPUEnabledToggles;
GPU_CONFIG_EXPORT extern const base::FeatureParam<std::string>
    kWebGPUUnsafeFeatures;
GPU_CONFIG_EXPORT extern const base::FeatureParam<bool>
    kWebGPUSpontaneousWireServer;
GPU_CONFIG_EXPORT extern const base::FeatureParam<std::string>
    kWGSLUnsafeFeatures;

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kIncreasedCmdBufferParseSlice);

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kDeferredOverlaysRelease);

#if BUILDFLAG(IS_WIN)
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kD3DBackingUploadWithUpdateSubresource);
#endif

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kGPUBlockListTestGroup);
GPU_CONFIG_EXPORT extern const base::FeatureParam<int> kGPUBlockListTestGroupId;
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kGPUDriverBugListTestGroup);
GPU_CONFIG_EXPORT extern const base::FeatureParam<int>
    kGPUDriverBugListTestGroupId;

GPU_CONFIG_EXPORT bool IsUsingVulkan();
GPU_CONFIG_EXPORT bool IsForceEnableWebGpuInterop();
GPU_CONFIG_EXPORT bool IsDrDcEnabled(
    const gpu::GpuFeatureInfo& gpu_feature_info);
GPU_CONFIG_EXPORT bool ShouldEnableDrDc();
GPU_CONFIG_EXPORT bool NeedThreadSafeAndroidMedia();
GPU_CONFIG_EXPORT bool IsSkiaGraphiteEnabled(
    const base::CommandLine* command_line);
GPU_CONFIG_EXPORT bool IsSkiaGraphiteWinIntelEnabled();
GPU_CONFIG_EXPORT bool IsSkiaGraphitePrecompilationEnabled(
    const base::CommandLine* command_line);
GPU_CONFIG_EXPORT bool EnablePruneOldTransferCacheEntries();
GPU_CONFIG_EXPORT bool IsLegacyIpcDisabled();

#if BUILDFLAG(IS_ANDROID)
GPU_CONFIG_EXPORT bool IsAndroidSurfaceControlEnabled();
GPU_CONFIG_EXPORT bool LimitAImageReaderMaxSizeToOne();
GPU_CONFIG_EXPORT bool IncreaseBufferCountForHighFrameRate();
GPU_CONFIG_EXPORT bool IncreaseBufferCountForWebViewOverlays();
#endif

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kSyncPointGraphValidation);

GPU_CONFIG_EXPORT bool IsSyncPointGraphValidationEnabled();

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kANGLEPerContextBlobCache);

#if BUILDFLAG(IS_APPLE)
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kIOSurfaceMultiThreading);
#endif

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kConfigurableGPUWatchdogTimeout);
GPU_CONFIG_EXPORT extern const base::FeatureParam<int>
    kConfigurableGPUWatchdogTimeoutSeconds;

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebGPUCompatibilityMode);

GPU_CONFIG_EXPORT bool IsGraphiteContextThreadSafe();

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kSendGPUChannelEarly);
GPU_CONFIG_EXPORT extern const base::FeatureParam<bool>
    kSendGPUChannelEarlyTopChromeOnly;

}  // namespace features

#endif  // GPU_CONFIG_GPU_FINCH_FEATURES_H_
