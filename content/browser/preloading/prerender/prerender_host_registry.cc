// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_host_registry.h"

#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/observer_list.h"
#include "base/system/sys_info.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_conversion_helper.h"
#include "build/build_config.h"
#include "content/browser/preloading/preloading_attempt_impl.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

bool DeviceHasEnoughMemoryForPrerender() {
  // This method disallows prerendering on low-end devices if the
  // kPrerender2MemoryControls feature is enabled.
  if (!base::FeatureList::IsEnabled(blink::features::kPrerender2MemoryControls))
    return true;

  // Use the same default threshold as the back/forward cache. See comments in
  // DeviceHasEnoughMemoryForBackForwardCache().
  static constexpr int kDefaultMemoryThresholdMb =
#if BUILDFLAG(IS_ANDROID)
      1700;
#else
      0;
#endif

  // The default is overridable by field trial param.
  int memory_threshold_mb = base::GetFieldTrialParamByFeatureAsInt(
      blink::features::kPrerender2MemoryControls,
      blink::features::kPrerender2MemoryThresholdParamName,
      kDefaultMemoryThresholdMb);

  return base::SysInfo::AmountOfPhysicalMemoryMB() > memory_threshold_mb;
}

}  // namespace

PrerenderHostRegistry::PrerenderHostRegistry() {
  DCHECK(blink::features::IsPrerender2Enabled());
}

PrerenderHostRegistry::~PrerenderHostRegistry() {
  for (Observer& obs : observers_)
    obs.OnRegistryDestroyed();
}

void PrerenderHostRegistry::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrerenderHostRegistry::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

int PrerenderHostRegistry::CreateAndStartHost(
    const PrerenderAttributes& attributes,
    WebContents& web_contents,
    PreloadingAttempt* attempt) {
  std::string recorded_url =
      attributes.initiator_origin.has_value()
          ? attributes.initiator_origin.value().GetURL().spec()
          : "(empty_url)";

  TRACE_EVENT2("navigation", "PrerenderHostRegistry::CreateAndStartHost",
               "attributes", attributes, "initiator_origin", recorded_url);

  int frame_tree_node_id = RenderFrameHost::kNoFrameTreeNodeId;

  {
    // Ensure observers are notified that a trigger occurred.
    base::ScopedClosureRunner notify_trigger(
        base::BindOnce(&PrerenderHostRegistry::NotifyTrigger,
                       base::Unretained(this), attributes.prerendering_url));

    // Check whether preloading is enabled. If users disable this
    // setting, it means users do not want to preload pages.
    WebContentsImpl* web_contents_impl =
        static_cast<WebContentsImpl*>(&web_contents);
    if (web_contents_impl->IsPrerender2Disabled()) {
      if (attempt)
        attempt->SetEligibility(PreloadingEligibility::kPreloadingDisabled);
      return RenderFrameHost::kNoFrameTreeNodeId;
    }

    // Don't prerender when the trigger is in the background.
    if (web_contents.GetVisibility() == Visibility::HIDDEN) {
      RecordPrerenderHostFinalStatus(
          PrerenderHost::FinalStatus::kTriggerBackgrounded, attributes,
          ukm::kInvalidSourceId);
      if (attempt)
        attempt->SetEligibility(PreloadingEligibility::kHidden);
      return RenderFrameHost::kNoFrameTreeNodeId;
    }

    // Don't prerender on low-end devices.
    // TODO(https://crbug.com/1176120): Fallback to NoStatePrefetch
    // since the memory requirements are different.
    if (!DeviceHasEnoughMemoryForPrerender()) {
      RecordPrerenderHostFinalStatus(PrerenderHost::FinalStatus::kLowEndDevice,
                                     attributes, ukm::kInvalidSourceId);
      if (attempt)
        attempt->SetEligibility(PreloadingEligibility::kLowMemory);
      return RenderFrameHost::kNoFrameTreeNodeId;
    }

    // Don't prerender when the Data Saver setting is enabled.
    if (GetContentClient()->browser()->IsDataSaverEnabled(
            web_contents.GetBrowserContext())) {
      RecordPrerenderHostFinalStatus(
          PrerenderHost::FinalStatus::kDataSaverEnabled, attributes,
          ukm::kInvalidSourceId);
      if (attempt)
        attempt->SetEligibility(PreloadingEligibility::kDataSaverEnabled);
      return RenderFrameHost::kNoFrameTreeNodeId;
    }

    // TODO(crbug.com/1176054): Support cross-origin prerendering.
    // The initiator origin is nullopt when prerendering is initiated by the
    // browser (not by a renderer using Speculation Rules API). In that case,
    // skip the same-origin check.
    if (!attributes.IsBrowserInitiated() &&
        !attributes.initiator_origin.value().IsSameOriginWith(
            attributes.prerendering_url)) {
      RecordPrerenderHostFinalStatus(
          PrerenderHost::FinalStatus::kCrossOriginNavigation, attributes,
          ukm::kInvalidSourceId);
      if (attempt)
        attempt->SetEligibility(PreloadingEligibility::kCrossOrigin);
      return RenderFrameHost::kNoFrameTreeNodeId;
    }

    // Once all eligibility checks are completed, set the status to kEligible.
    if (attempt)
      attempt->SetEligibility(PreloadingEligibility::kEligible);

    // Check for the HoldbackStatus after checking the eligibility.
    if (base::FeatureList::IsEnabled(features::kPrerender2Holdback)) {
      if (attempt)
        attempt->SetHoldbackStatus(PreloadingHoldbackStatus::kHoldback);
      return RenderFrameHost::kNoFrameTreeNodeId;
    }
    if (attempt)
      attempt->SetHoldbackStatus(PreloadingHoldbackStatus::kAllowed);

    // Ignore prerendering requests for the same URL.
    for (auto& iter : prerender_host_by_frame_tree_node_id_) {
      if (iter.second->GetInitialUrl() == attributes.prerendering_url) {
        if (attempt) {
          attempt->SetTriggeringOutcome(
              PreloadingTriggeringOutcome::kDuplicate);
        }

        return RenderFrameHost::kNoFrameTreeNodeId;
      }
    }

    // TODO(crbug.com/1197133): Cancel the started prerender and start a new
    // one if the score of the new candidate is higher than the started one's.
    if (!IsAllowedToStartPrerenderingForTrigger(attributes.trigger_type)) {
      if (attempt) {
        // The reason we don't consider limit exceeded as an ineligibility
        // reason is because we can't replicate the behavior in our other
        // experiment groups for analysis. To prevent this we set
        // TriggeringOutcome to kFailure and look into the failure reason to
        // learn more.
        attempt->SetTriggeringOutcome(PreloadingTriggeringOutcome::kFailure);
      }
      RecordPrerenderHostFinalStatus(
          PrerenderHost::FinalStatus::kMaxNumOfRunningPrerendersExceeded,
          attributes, ukm::kInvalidSourceId);
      return RenderFrameHost::kNoFrameTreeNodeId;
    }

    auto prerender_host = std::make_unique<PrerenderHost>(
        attributes, web_contents, attempt ? attempt->GetWeakPtr() : nullptr);
    frame_tree_node_id = prerender_host->frame_tree_node_id();

    CHECK(!base::Contains(prerender_host_by_frame_tree_node_id_,
                          frame_tree_node_id));
    prerender_host_by_frame_tree_node_id_[frame_tree_node_id] =
        std::move(prerender_host);
  }

  // Outside the NotifyTrigger ScopedClosureRunner since tests use the trigger
  // to register observers.  Observers may be notified about start in the call
  // below.
  if (!prerender_host_by_frame_tree_node_id_[frame_tree_node_id]
           ->StartPrerendering()) {
    // TODO(nhiroki): Pass a more suitable cancellation reason like
    // kStartFailed.
    CancelHost(frame_tree_node_id, PrerenderHost::FinalStatus::kDestroyed);
    return RenderFrameHost::kNoFrameTreeNodeId;
  }

  // Check the current memory usage and destroy a prerendering if the entire
  // browser uses excessive memory. This occurs asynchronously.
  switch (attributes.trigger_type) {
    case PrerenderTriggerType::kSpeculationRule:
      DestroyWhenUsingExcessiveMemory(frame_tree_node_id);
      break;
    case PrerenderTriggerType::kEmbedder:
      // We don't check the memory usage for embedder triggered prerenderings
      // for now.
      break;
  }

  RecordPrerenderTriggered(attributes.initiator_ukm_id);

  return frame_tree_node_id;
}

bool PrerenderHostRegistry::CancelHost(
    int frame_tree_node_id,
    PrerenderHost::FinalStatus final_status) {
  TRACE_EVENT1("navigation", "PrerenderHostRegistry::CancelHost",
               "frame_tree_node_id", frame_tree_node_id);

  // Cancel must not be requested during activation.
  // TODO(https://crbug.com/1195751): This is the key assumption of the
  // synchronous prerender activation, so now this is CHECK. Change this to
  // DCHECK once the assumption is ensured in the real world.
  CHECK(!base::Contains(reserved_prerender_host_by_frame_tree_node_id_,
                        frame_tree_node_id));

  // Look up the id in the non-reserved host map, remove it from the map, and
  // record the cancellation reason.
  auto iter = prerender_host_by_frame_tree_node_id_.find(frame_tree_node_id);
  if (iter == prerender_host_by_frame_tree_node_id_.end())
    return false;

  // Remove the prerender host from the host map so that it's not used for
  // activation during asynchronous deletion.
  std::unique_ptr<PrerenderHost> prerender_host = std::move(iter->second);
  prerender_host_by_frame_tree_node_id_.erase(iter);

  // Asynchronously delete the prerender host.
  ScheduleToDeleteAbandonedHost(std::move(prerender_host), final_status);
  return true;
}

void PrerenderHostRegistry::CancelAllHosts(
    PrerenderHost::FinalStatus final_status) {
  // Should not have an activating host. See comments in CancelHost.
  CHECK(reserved_prerender_host_by_frame_tree_node_id_.empty());

  auto prerender_host_map = std::move(prerender_host_by_frame_tree_node_id_);
  for (auto& iter : prerender_host_map) {
    std::unique_ptr<PrerenderHost> prerender_host = std::move(iter.second);
    ScheduleToDeleteAbandonedHost(std::move(prerender_host), final_status);
  }
}

int PrerenderHostRegistry::FindPotentialHostToActivate(
    NavigationRequest& navigation_request) {
  TRACE_EVENT2(
      "navigation", "PrerenderHostRegistry::FindPotentialHostToActivate",
      "navigation_url", navigation_request.GetURL().spec(), "render_frame_host",
      navigation_request.frame_tree_node()->current_frame_host());
  return FindHostToActivateInternal(navigation_request);
}

int PrerenderHostRegistry::ReserveHostToActivate(
    NavigationRequest& navigation_request,
    int expected_host_id) {
  RenderFrameHostImpl* render_frame_host =
      navigation_request.frame_tree_node()->current_frame_host();
  TRACE_EVENT2("navigation", "PrerenderHostRegistry::ReserveHostToActivate",
               "navigation_url", navigation_request.GetURL().spec(),
               "render_frame_host", render_frame_host);

  // Find an available host for the navigation request.
  int host_id = FindHostToActivateInternal(navigation_request);
  if (host_id == RenderFrameHost::kNoFrameTreeNodeId)
    return RenderFrameHost::kNoFrameTreeNodeId;

  // Check if the host is what the NavigationRequest expects. The host can be
  // different when a trigger page removes the existing prerender and then
  // re-adds a new prerender for the same URL.
  //
  // NavigationRequest makes sure that the prerender is ready for activation by
  // waiting for PrerenderCommitDeferringCondition before this point. Without
  // this check, if the prerender is changed during the period,
  // NavigationRequest may attempt to activate the new prerender that is not
  // ready.
  if (host_id != expected_host_id)
    return RenderFrameHost::kNoFrameTreeNodeId;

  // Remove the host from the map of non-reserved hosts.
  std::unique_ptr<PrerenderHost> host =
      std::move(prerender_host_by_frame_tree_node_id_[host_id]);
  prerender_host_by_frame_tree_node_id_.erase(host_id);
  DCHECK_EQ(host_id, host->frame_tree_node_id());

  // Reserve the host for activation.
  auto result = reserved_prerender_host_by_frame_tree_node_id_.emplace(
      host_id, std::move(host));
  DCHECK(result.second);

  return host_id;
}

RenderFrameHostImpl* PrerenderHostRegistry::GetRenderFrameHostForReservedHost(
    int frame_tree_node_id) {
  auto iter =
      reserved_prerender_host_by_frame_tree_node_id_.find(frame_tree_node_id);
  if (iter == reserved_prerender_host_by_frame_tree_node_id_.end()) {
    return nullptr;
  }
  return iter->second->GetPrerenderedMainFrameHost();
}

std::unique_ptr<StoredPage> PrerenderHostRegistry::ActivateReservedHost(
    int frame_tree_node_id,
    NavigationRequest& navigation_request) {
  auto iter =
      reserved_prerender_host_by_frame_tree_node_id_.find(frame_tree_node_id);
  CHECK(iter != reserved_prerender_host_by_frame_tree_node_id_.end());
  std::unique_ptr<PrerenderHost> prerender_host = std::move(iter->second);
  reserved_prerender_host_by_frame_tree_node_id_.erase(iter);
  return prerender_host->Activate(navigation_request);
}

void PrerenderHostRegistry::OnTriggerDestroyed(int frame_tree_node_id) {
  // TODO(https://crbug.com/1169594): Since one prerender may have several
  // triggers, PrerenderHostRegistry should not destroy a PrerenderHost instance
  // if one of the triggers is still alive.

  // Look up the id in the non-reserved host map and remove it from the map if
  // it's found.
  auto found = prerender_host_by_frame_tree_node_id_.find(frame_tree_node_id);
  if (found != prerender_host_by_frame_tree_node_id_.end()) {
    DCHECK(!base::Contains(reserved_prerender_host_by_frame_tree_node_id_,
                           frame_tree_node_id));

    // Remove the prerender host from the host maps so that it's not used for
    // activation during asynchronous deletion.
    std::unique_ptr<PrerenderHost> prerender_host = std::move(found->second);
    prerender_host_by_frame_tree_node_id_.erase(found);

    // Asynchronously delete the prerender host.
    ScheduleToDeleteAbandonedHost(
        std::move(prerender_host),
        PrerenderHost::FinalStatus::kTriggerDestroyed);
  }

  // Don't remove the host from the reserved host map. Unlike use of the
  // disallowed features in prerendered pages, the destruction of the trigger
  // doesn't spoil prerendering, so let it keep ongoing.
}

void PrerenderHostRegistry::OnActivationFinished(int frame_tree_node_id) {
  // OnActivationFinished() should not be called for non-reserved hosts.
  DCHECK(!base::Contains(prerender_host_by_frame_tree_node_id_,
                         frame_tree_node_id));
  reserved_prerender_host_by_frame_tree_node_id_.erase(frame_tree_node_id);
}

PrerenderHost* PrerenderHostRegistry::FindNonReservedHostById(
    int frame_tree_node_id) {
  auto id_iter = prerender_host_by_frame_tree_node_id_.find(frame_tree_node_id);
  if (id_iter == prerender_host_by_frame_tree_node_id_.end())
    return nullptr;
  return id_iter->second.get();
}

PrerenderHost* PrerenderHostRegistry::FindReservedHostById(
    int frame_tree_node_id) {
  auto iter =
      reserved_prerender_host_by_frame_tree_node_id_.find(frame_tree_node_id);
  if (iter == reserved_prerender_host_by_frame_tree_node_id_.end())
    return nullptr;
  return iter->second.get();
}

std::vector<FrameTree*> PrerenderHostRegistry::GetPrerenderFrameTrees() {
  std::vector<FrameTree*> result;
  for (auto& i : prerender_host_by_frame_tree_node_id_) {
    result.push_back(&i.second->GetPrerenderFrameTree());
  }
  for (auto& i : reserved_prerender_host_by_frame_tree_node_id_) {
    result.push_back(&i.second->GetPrerenderFrameTree());
  }
  return result;
}

PrerenderHost* PrerenderHostRegistry::FindHostByUrlForTesting(
    const GURL& prerendering_url) {
  for (auto& iter : prerender_host_by_frame_tree_node_id_) {
    if (iter.second->GetInitialUrl() == prerendering_url)
      return iter.second.get();
  }
  return nullptr;
}

void PrerenderHostRegistry::CancelAllHostsForTesting() {
  DCHECK(reserved_prerender_host_by_frame_tree_node_id_.empty())
      << "It is not possible to cancel reserved hosts, so they must not exist "
         "when trying to cancel all hosts";

  for (auto& iter : prerender_host_by_frame_tree_node_id_) {
    // Asynchronously delete the prerender host.
    ScheduleToDeleteAbandonedHost(
        std::move(iter.second),
        PrerenderHost::FinalStatus::kCancelAllHostsForTesting);
  }
  // After we're done scheduling deletion, clear the map.
  prerender_host_by_frame_tree_node_id_.clear();
}

base::WeakPtr<PrerenderHostRegistry> PrerenderHostRegistry::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PrerenderHostRegistry::ForEachPrerenderHost(
    base::RepeatingCallback<void(PrerenderHost&)> callback) {
  for (auto& iter : prerender_host_by_frame_tree_node_id_) {
    callback.Run(*iter.second);
  }

  for (auto& iter : reserved_prerender_host_by_frame_tree_node_id_) {
    callback.Run(*iter.second);
  }
}

int PrerenderHostRegistry::FindHostToActivateInternal(
    NavigationRequest& navigation_request) {
  RenderFrameHostImpl* render_frame_host =
      navigation_request.frame_tree_node()->current_frame_host();
  TRACE_EVENT2("navigation",
               "PrerenderHostRegistry::FindHostToActivateInternal",
               "navigation_url", navigation_request.GetURL().spec(),
               "render_frame_host", render_frame_host);

  // Disallow activation when the navigation is for a nested browsing context
  // (e.g., iframes, fenced frames). This is because nested browsing contexts
  // such as iframes are supposed to be created in the parent's browsing context
  // group and can script with the parent, but prerendered pages are created in
  // new browsing context groups. And also, we disallow activation when the
  // navigation is for a fenced frame to prevent the communication path from the
  // embedding page to the fenced frame.
  if (!navigation_request.IsInPrimaryMainFrame())
    return RenderFrameHost::kNoFrameTreeNodeId;

  // Disallow activation when the navigation happens in the prerendering frame
  // tree.
  if (navigation_request.IsInPrerenderedMainFrame())
    return RenderFrameHost::kNoFrameTreeNodeId;

  // Disallow activation when other auxiliary browsing contexts (e.g., pop-up
  // windows) exist in the same browsing context group. This is because these
  // browsing contexts should be able to script each other, but prerendered
  // pages are created in new browsing context groups.
  SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  if (site_instance->GetRelatedActiveContentsCount() != 1u)
    return RenderFrameHost::kNoFrameTreeNodeId;

  // Find an available host for the navigation URL.
  PrerenderHost* host = nullptr;
  for (const auto& [_, it_prerender_host] :
       prerender_host_by_frame_tree_node_id_) {
    if (it_prerender_host->IsUrlMatch(navigation_request.GetURL())) {
      host = it_prerender_host.get();
      break;
    }
  }
  if (!host)
    return RenderFrameHost::kNoFrameTreeNodeId;

  // Compare navigation params from activation with the navigation params
  // from the initial prerender navigation. If they don't match, the navigation
  // should not activate the prerendered page.
  if (!host->AreInitialPrerenderNavigationParamsCompatibleWithNavigation(
          navigation_request)) {
    return RenderFrameHost::kNoFrameTreeNodeId;
  }

  if (!host->IsFramePolicyCompatibleWithPrimaryFrameTree()) {
    return RenderFrameHost::kNoFrameTreeNodeId;
  }

  return host->frame_tree_node_id();
}

void PrerenderHostRegistry::ScheduleToDeleteAbandonedHost(
    std::unique_ptr<PrerenderHost> prerender_host,
    PrerenderHost::FinalStatus final_status) {
  prerender_host->RecordFinalStatus(PassKey(), final_status);

  // Asynchronously delete the prerender host.
  to_be_deleted_hosts_.push_back(std::move(prerender_host));
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PrerenderHostRegistry::DeleteAbandonedHosts,
                                weak_factory_.GetWeakPtr()));
}

void PrerenderHostRegistry::DeleteAbandonedHosts() {
  to_be_deleted_hosts_.clear();
}

void PrerenderHostRegistry::NotifyTrigger(const GURL& url) {
  for (Observer& obs : observers_)
    obs.OnTrigger(url);
}

PrerenderTriggerType PrerenderHostRegistry::GetPrerenderTriggerType(
    int frame_tree_node_id) {
  PrerenderHost* prerender_host = FindReservedHostById(frame_tree_node_id);
  DCHECK(prerender_host);

  return prerender_host->trigger_type();
}

const std::string& PrerenderHostRegistry::GetPrerenderEmbedderHistogramSuffix(
    int frame_tree_node_id) {
  PrerenderHost* prerender_host = FindReservedHostById(frame_tree_node_id);
  DCHECK(prerender_host);

  return prerender_host->embedder_histogram_suffix();
}

bool PrerenderHostRegistry::IsAllowedToStartPrerenderingForTrigger(
    PrerenderTriggerType trigger_type) {
  int trigger_type_count = 0;
  for (const auto& host_by_id : prerender_host_by_frame_tree_node_id_) {
    if (host_by_id.second->trigger_type() == trigger_type)
      ++trigger_type_count;
  }

  switch (trigger_type) {
    case PrerenderTriggerType::kSpeculationRule:
      // The number of prerenders triggered by speculation rules is limited to a
      // Finch config param.
      return trigger_type_count <
             base::GetFieldTrialParamByFeatureAsInt(
                 blink::features::kPrerender2,
                 blink::features::kPrerender2MaxNumOfRunningSpeculationRules,
                 1);
    case PrerenderTriggerType::kEmbedder:
      // Currently the number of prerenders triggered by an embedder is limited
      // to two.
      return trigger_type_count < 2;
  }
}

void PrerenderHostRegistry::DestroyWhenUsingExcessiveMemory(
    int frame_tree_node_id) {
  if (!base::FeatureList::IsEnabled(blink::features::kPrerender2MemoryControls))
    return;

  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestPrivateMemoryFootprint(
          base::kNullProcessId,
          base::BindOnce(&PrerenderHostRegistry::DidReceiveMemoryDump,
                         weak_factory_.GetWeakPtr(), frame_tree_node_id));
}

void PrerenderHostRegistry::DidReceiveMemoryDump(
    int frame_tree_node_id,
    bool success,
    std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump) {
  DCHECK(
      base::FeatureList::IsEnabled(blink::features::kPrerender2MemoryControls));
  // Stop a prerendering when we can't get the current memory usage.
  if (!success) {
    CancelHost(frame_tree_node_id,
               PrerenderHost::FinalStatus::kFailToGetMemoryUsage);
    return;
  }

  int64_t private_footprint_total_kb = 0;
  for (const auto& pmd : dump->process_dumps()) {
    private_footprint_total_kb += pmd.os_dump().private_footprint_kb;
  }

  // TODO(crbug.com/1273341): Finalize the threshold after the experiment
  // completes. The default acceptable percent is 20% of the system memory.
  int acceptable_percent_of_system_memory =
      base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kPrerender2MemoryControls,
          blink::features::
              kPrerender2MemoryAcceptablePercentOfSystemMemoryParamName,
          20);

  // When the current memory usage is higher than
  // `acceptable_percent_of_system_memory` % of the system memory, cancel a
  // prerendering with `frame_tree_node_id`.
  if (private_footprint_total_kb * 1024 >=
      acceptable_percent_of_system_memory * 0.01 *
          base::SysInfo::AmountOfPhysicalMemory()) {
    CancelHost(frame_tree_node_id,
               PrerenderHost::FinalStatus::kMemoryLimitExceeded);
  }
}

}  // namespace content
