// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_navigation_policy.h"

#include <bitset>

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

namespace content {

bool DeviceHasEnoughMemoryForBackForwardCache() {
  // This method make sure that the physical memory of device is greater than
  // the allowed threshold and enables back-forward cache if the feature
  // kBackForwardCacheMemoryControls is enabled.
  // It is important to check the base::FeatureList to avoid activating any
  // field trial groups if BFCache is disabled due to memory threshold.
  if (base::FeatureList::IsEnabled(features::kBackForwardCacheMemoryControls)) {
    // On Android, BackForwardCache is only enabled for 2GB+ high memory
    // devices. The default threshold value is set to 1700 MB to account for all
    // 2GB devices which report lower RAM due to carveouts.
    int default_memory_threshold_mb =
#if BUILDFLAG(IS_ANDROID)
        1700;
#else
        // Desktop has lower memory limitations compared to Android allowing us
        // to enable BackForwardCache for all devices.
        0;
#endif
    int memory_threshold_mb = base::GetFieldTrialParamByFeatureAsInt(
        features::kBackForwardCacheMemoryControls,
        "memory_threshold_for_back_forward_cache_in_mb",
        default_memory_threshold_mb);
    return base::SysInfo::AmountOfPhysicalMemoryMB() > memory_threshold_mb;
  }

  // If the feature kBackForwardCacheMemoryControls is not enabled, all the
  // devices are included by default.
  return true;
}

bool IsBackForwardCacheDisabledByCommandLine() {
  if (base::CommandLine::InitializedForCurrentProcess() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableBackForwardCache)) {
    return true;
  }
  return false;
}

bool IsBackForwardCacheEnabled() {
  if (!DeviceHasEnoughMemoryForBackForwardCache())
    return false;

  if (IsBackForwardCacheDisabledByCommandLine())
    return false;

  // The feature needs to be checked last, because checking the feature
  // activates the field trial and assigns the client either to a control or an
  // experiment group - such assignment should be final.
  return base::FeatureList::IsEnabled(features::kBackForwardCache);
}

bool IsSameSiteBackForwardCacheEnabled() {
  if (!IsBackForwardCacheEnabled())
    return false;

  // Same-site back-forward cache is enabled by default, but can be disabled
  // through kBackForwardCache's "enable_same_site" param.
  static constexpr base::FeatureParam<bool> enable_same_site_back_forward_cache(
      &features::kBackForwardCache, "enable_same_site", true);
  return enable_same_site_back_forward_cache.Get();
}

bool ShouldSkipSameSiteBackForwardCacheForPageWithUnload() {
  if (!IsSameSiteBackForwardCacheEnabled())
    return true;
  static constexpr base::FeatureParam<bool> skip_same_site_if_unload_exists(
      &features::kBackForwardCache, "skip_same_site_if_unload_exists", false);
  return skip_same_site_if_unload_exists.Get();
}

bool CanCrossSiteNavigationsProactivelySwapBrowsingInstances() {
  return IsProactivelySwapBrowsingInstanceEnabled() ||
         IsBackForwardCacheEnabled();
}

const char kProactivelySwapBrowsingInstanceLevelParameterName[] = "level";

constexpr base::FeatureParam<ProactivelySwapBrowsingInstanceLevel>::Option
    proactively_swap_browsing_instance_levels[] = {
        {ProactivelySwapBrowsingInstanceLevel::kDisabled, "Disabled"},
        {ProactivelySwapBrowsingInstanceLevel::kCrossSiteSwapProcess,
         "CrossSiteSwapProcess"},
        {ProactivelySwapBrowsingInstanceLevel::kCrossSiteReuseProcess,
         "CrossSiteReuseProcess"},
        {ProactivelySwapBrowsingInstanceLevel::kSameSite, "SameSite"}};
const base::FeatureParam<ProactivelySwapBrowsingInstanceLevel>
    proactively_swap_browsing_instance_level{
        &features::kProactivelySwapBrowsingInstance,
        kProactivelySwapBrowsingInstanceLevelParameterName,
        ProactivelySwapBrowsingInstanceLevel::kDisabled,
        &proactively_swap_browsing_instance_levels};

std::string GetProactivelySwapBrowsingInstanceLevelName(
    ProactivelySwapBrowsingInstanceLevel level) {
  return proactively_swap_browsing_instance_level.GetName(level);
}

std::array<std::string,
           static_cast<size_t>(ProactivelySwapBrowsingInstanceLevel::kMaxValue)>
ProactivelySwapBrowsingInstanceFeatureEnabledLevelValues() {
  return {
      GetProactivelySwapBrowsingInstanceLevelName(
          ProactivelySwapBrowsingInstanceLevel::kCrossSiteSwapProcess),
      GetProactivelySwapBrowsingInstanceLevelName(
          ProactivelySwapBrowsingInstanceLevel::kCrossSiteReuseProcess),
      GetProactivelySwapBrowsingInstanceLevelName(
          ProactivelySwapBrowsingInstanceLevel::kSameSite),
  };
}

ProactivelySwapBrowsingInstanceLevel GetProactivelySwapBrowsingInstanceLevel() {
  if (base::FeatureList::IsEnabled(features::kProactivelySwapBrowsingInstance))
    return proactively_swap_browsing_instance_level.Get();
  return ProactivelySwapBrowsingInstanceLevel::kDisabled;
}

bool IsProactivelySwapBrowsingInstanceEnabled() {
  return GetProactivelySwapBrowsingInstanceLevel() >=
         ProactivelySwapBrowsingInstanceLevel::kCrossSiteSwapProcess;
}

bool IsProactivelySwapBrowsingInstanceWithProcessReuseEnabled() {
  return GetProactivelySwapBrowsingInstanceLevel() >=
         ProactivelySwapBrowsingInstanceLevel::kCrossSiteReuseProcess;
}

bool IsProactivelySwapBrowsingInstanceOnSameSiteNavigationEnabled() {
  return GetProactivelySwapBrowsingInstanceLevel() >=
         ProactivelySwapBrowsingInstanceLevel::kSameSite;
}

const char kRenderDocumentLevelParameterName[] = "level";

constexpr base::FeatureParam<RenderDocumentLevel>::Option
    render_document_levels[] = {
        {RenderDocumentLevel::kCrashedFrame, "crashed-frame"},
        {RenderDocumentLevel::kSubframe, "subframe"}};
const base::FeatureParam<RenderDocumentLevel> render_document_level{
    &features::kRenderDocument, kRenderDocumentLevelParameterName,
    RenderDocumentLevel::kCrashedFrame, &render_document_levels};

RenderDocumentLevel GetRenderDocumentLevel() {
  if (base::FeatureList::IsEnabled(features::kRenderDocument))
    return render_document_level.Get();
  return RenderDocumentLevel::kCrashedFrame;
}

std::string GetRenderDocumentLevelName(RenderDocumentLevel level) {
  return render_document_level.GetName(level);
}

bool ShouldCreateNewHostForSameSiteSubframe() {
  return GetRenderDocumentLevel() >= RenderDocumentLevel::kSubframe;
}

bool ShouldSkipEarlyCommitPendingForCrashedFrame() {
  static bool skip_early_commit_pending_for_crashed_frame =
      base::FeatureList::IsEnabled(
          features::kSkipEarlyCommitPendingForCrashedFrame);
  return skip_early_commit_pending_for_crashed_frame;
}

}  // namespace content
