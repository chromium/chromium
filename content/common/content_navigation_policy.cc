// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_navigation_policy.h"

#include <bitset>

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "net/base/features.h"

namespace features {
BASE_FEATURE(kBackForwardCache_NoMemoryLimit_Trial,
             "BackForwardCache_NoMemoryLimit_Trial",
             base::FEATURE_ENABLED_BY_DEFAULT);
}

namespace content {

bool DeviceHasEnoughMemoryForBackForwardCache() {
  // This method make sure that the physical memory of device is greater than
  // the allowed threshold and enables back-forward cache if the feature
  // kBackForwardCacheMemoryControls is enabled.
  // It is important to check the base::FeatureList to avoid activating any
  // field trial groups if BFCache is disabled due to memory threshold.
  if (base::FeatureList::IsEnabled(features::kBackForwardCacheMemoryControls)) {
    // On Android, BackForwardCache is enabled for devices with 1200MB memory or
    // above.
    int default_memory_threshold_mb =
#if BUILDFLAG(IS_ANDROID)
        1200;
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
  bool has_enough_memory = DeviceHasEnoughMemoryForBackForwardCache();
  if (!has_enough_memory) {
    // When the device does not have enough memory for BackForwardCache, return
    // false so we won't try to put things in the back/forward cache.
    // Also, trigger the activation of the BackForwardCache_NoMemoryLimit_Trial
    // field trial by querying the feature flag. With this, we guarantee that
    // all devices that do not have enough memory for BackForwardCache will be
    // included in that field trial. See case #1 in the comment for the
    // BackForwardCache_NoMemoryLimit_Trial in the header file for more details.
    base::FeatureList::IsEnabled(
        features::kBackForwardCache_NoMemoryLimit_Trial);
    return false;
  }

  if (IsBackForwardCacheDisabledByCommandLine())
    return false;

  // The feature needs to be checked last, because checking the feature
  // activates the field trial and assigns the client either to a control or an
  // experiment group - such assignment should be final. This allows us to keep
  // the BackForwardCache field trial to include only devices that have enough
  // memory for BackForwardCache, and those devices only.
  if (base::FeatureList::IsEnabled(features::kBackForwardCache)) {
    // When the device does have enough memory for BackForwardCache, return
    // true so we won't try to put things in the back/forward cache. Also,
    // trigger the activation of the BackForwardCache_NoMemoryLimit_Trial field
    // trial by querying the feature flag. With this, we guarantee that all
    // devices that do have enough memory for BackForwardCache and have the
    // BackForwardCache feature flag enabled will be included in that field
    // trial. See case #2 in the comment for the
    // BackForwardCache_NoMemoryLimit_Trial in the header file for more details.
    base::FeatureList::IsEnabled(
        features::kBackForwardCache_NoMemoryLimit_Trial);
    return true;
  }
  return false;
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
        {RenderDocumentLevel::kNonLocalRootSubframe, "non-local-root-subframe"},
        {RenderDocumentLevel::kSubframe, "subframe"},
        {RenderDocumentLevel::kAllFrames, "all-frames"}};
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

bool ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
    bool is_main_frame,
    bool is_local_root,
    bool has_committed_any_navigation,
    bool must_be_replaced) {
  if (must_be_replaced) {
    return true;
  }
  if (!has_committed_any_navigation) {
    return false;
  }
  RenderDocumentLevel level = GetRenderDocumentLevel();
  if (is_main_frame) {
    CHECK(is_local_root);
    return level >= RenderDocumentLevel::kAllFrames;
  }
  if (is_local_root) {
    return level >= RenderDocumentLevel::kSubframe;
  }
  return level >= RenderDocumentLevel::kNonLocalRootSubframe;
}

bool ShouldCreateNewHostForAllFrames() {
  return GetRenderDocumentLevel() >= RenderDocumentLevel::kAllFrames;
}

bool ShouldSkipEarlyCommitPendingForCrashedFrame() {
  static bool skip_early_commit_pending_for_crashed_frame =
      base::FeatureList::IsEnabled(
          features::kSkipEarlyCommitPendingForCrashedFrame);
  return skip_early_commit_pending_for_crashed_frame;
}

static constexpr base::FeatureParam<NavigationQueueingFeatureLevel>::Option
    kNavigationQueueingFeatureLevels[] = {
        {NavigationQueueingFeatureLevel::kNone, "none"},
        {NavigationQueueingFeatureLevel::kAvoidRedundantCancellations,
         "avoid-redundant"},
        {NavigationQueueingFeatureLevel::kFull, "full"}};
const base::FeatureParam<NavigationQueueingFeatureLevel>
    kNavigationQueueingFeatureLevelParam{
        &features::kQueueNavigationsWhileWaitingForCommit, "queueing_level",
        NavigationQueueingFeatureLevel::kFull,
        &kNavigationQueueingFeatureLevels};

NavigationQueueingFeatureLevel GetNavigationQueueingFeatureLevel() {
  if (GetRenderDocumentLevel() >= RenderDocumentLevel::kNonLocalRootSubframe) {
    // When RenderDocument is enabled with a level of "non-local-root-subframe"
    // or more, navigation queueing needs to be enabled too, to avoid crashes.
    return NavigationQueueingFeatureLevel::kFull;
  }
  if (base::FeatureList::IsEnabled(
          features::kQueueNavigationsWhileWaitingForCommit)) {
    return kNavigationQueueingFeatureLevelParam.Get();
  }
  return NavigationQueueingFeatureLevel::kNone;
}

bool ShouldAvoidRedundantNavigationCancellations() {
  // If the experimental early RenderFrameHost swap for history navigations is
  // turned on, this must return true so that when the old RFH is unloaded as
  // part of the early swap, this doesn't cancel the navigation that's still
  // ongoing in the new RFH.
  if (base::FeatureList::IsEnabled(
          features::kEarlyDocumentSwapForBackForwardTransitions)) {
    return true;
  }

  return GetNavigationQueueingFeatureLevel() >=
         NavigationQueueingFeatureLevel::kAvoidRedundantCancellations;
}

bool ShouldQueueNavigationsWhenPendingCommitRFHExists() {
  return GetNavigationQueueingFeatureLevel() ==
         NavigationQueueingFeatureLevel::kFull;
}

bool ShouldRestrictCanAccessDataForOriginToUIThread() {
  // Only restrict calls to the UI thread if the feature is enabled, and if the
  // new blob URL support is enabled.
  return base::FeatureList::IsEnabled(
             features::kRestrictCanAccessDataForOriginToUIThread) &&
         base::FeatureList::IsEnabled(
             net::features::kSupportPartitionedBlobUrl);
}

bool ShouldCreateSiteInstanceForDataUrls() {
  return base::FeatureList::IsEnabled(features::kSiteInstanceGroupsForDataUrls);
}

}  // namespace content
