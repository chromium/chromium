// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_HOST_REGISTRY_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_HOST_REGISTRY_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/timer/timer.h"
#include "base/types/pass_key.h"
#include "content/browser/preloading/preloading_confidence.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_export.h"
#include "content/common/frame.mojom-forward.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-shared.h"

class GURL;

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class HttpResponseHeaders;
}

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace content {

class FrameTree;
class NavigationRequest;
class PrerenderCancellationReason;
class PrerenderHost;
class PrerenderNewTabHandle;
class RenderFrameHostImpl;
class StoredPage;
struct PrerenderAttributes;

// PrerenderHostRegistry creates and retains a prerender host, and reserves it
// for NavigationRequest to activate the prerendered page. This is created per
// WebContentsImpl and owned by it.
//
// The APIs of this class are categorized into two: APIs for triggers and APIs
// for activators.
//
// - Triggers (e.g., SpeculationHostImpl) start prerendering by
//   CreateAndStartHost() and notify the registry of destruction of the trigger
//   by CancelHosts().
// - Activators (i.e., NavigationRequest) can reserve the prerender host on
//   activation start by ReserveHostToActivate(), activate it by
//   ActivateReservedHost(), and notify the registry of completion of the
//   activation by OnActivationFinished().
class CONTENT_EXPORT PrerenderHostRegistry : public WebContentsObserver {
 public:
  // The time to allow prerendering kept alive in the background. All the hosts
  // that this PrerenderHostRegistry holds will be terminated when the timer
  // exceeds this. The timeout value differs depending on the trigger type. The
  // value for an embedder was determined by
  // PageLoad.Clients.Prerender.NavigationToActivation.*.
  // The value for speculation rules was determined to align with the default
  // value of BFCache's eviction timer
  // (kDefaultTimeToLiveInBackForwardCacheInSeconds).
  static constexpr base::TimeDelta kTimeToLiveInBackgroundForEmbedder =
      base::Seconds(19);
  static constexpr base::TimeDelta kTimeToLiveInBackgroundForSpeculationRules =
      base::Seconds(600);

  using PassKey = base::PassKey<PrerenderHostRegistry>;

  explicit PrerenderHostRegistry(WebContents&);
  ~PrerenderHostRegistry() override;

  PrerenderHostRegistry(const PrerenderHostRegistry&) = delete;
  PrerenderHostRegistry& operator=(const PrerenderHostRegistry&) = delete;
  PrerenderHostRegistry(PrerenderHostRegistry&&) = delete;
  PrerenderHostRegistry& operator=(PrerenderHostRegistry&&) = delete;

  class Observer : public base::CheckedObserver {
   public:
    // Called once per CreateAndStartHost() call. Indicates the registry
    // received a request to create a prerender but does not necessarily mean a
    // host was created. If a host was created, it is guaranteed to be in the
    // registry at the time this is called.
    virtual void OnTrigger(const GURL& url) {}

    // Called when CancelHosts() actually cancels each host.
    virtual void OnCancel(FrameTreeNodeId host_frame_tree_node_id,
                          const PrerenderCancellationReason& reason) {}

    // Called from the registry's destructor. The observer
    // should drop any reference to the registry.
    virtual void OnRegistryDestroyed() {}
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // For triggers.
  // Creates and starts a host. Returns the root frame tree node id of the
  // prerendered page, which can be used as the id of the host.
  // `preloading_attempt` is the attempt corresponding to this prerender, the
  // default value is set to nullptr as every case of prerendering trigger is
  // not yet integrated with PreloadingAttempt.
  // TODO(crbug.com/40234240): Remove the default value as nullptr for
  // preloading_attempt once new-tab-prerender is integrated with Preloading
  // APIs.
  FrameTreeNodeId CreateAndStartHost(
      const PrerenderAttributes& attributes,
      PreloadingAttempt* preloading_attempt = nullptr);

  // Creates and starts a host in a new WebContents so that a navigation in a
  // new tab will be able to activate it. PrerenderHostRegistry associated with
  // the new WebContents manages the started host, and `this`
  // PrerenderHostRegistry manages PrerenderNewTabHandle that owns the
  // WebContents (see `prerender_new_tab_handle_by_frame_tree_node_id_`).
  FrameTreeNodeId CreateAndStartHostForNewTab(
      const PrerenderAttributes& attributes,
      const PreloadingPredictor& creating_predictor,
      const PreloadingPredictor& enacting_predictor,
      PreloadingConfidence confidence);

  // Cancels the host registered for `frame_tree_node_id`. The host is
  // immediately removed from the map of non-reserved hosts but asynchronously
  // destroyed so that prerendered pages can cancel themselves without concern
  // for self destruction.
  // Returns true if a cancelation has occurred.
  bool CancelHost(FrameTreeNodeId frame_tree_node_id,
                  PrerenderFinalStatus final_status);
  // Same as CancelHost, but can pass a detailed reason for recording if given.
  bool CancelHost(FrameTreeNodeId frame_tree_node_id,
                  const PrerenderCancellationReason& reason);

  // Cancels the existing hosts specified in the vector with the same reason.
  // Returns a subset of `frame_tree_node_ids` that were actually cancelled.
  std::set<FrameTreeNodeId> CancelHosts(
      const std::vector<FrameTreeNodeId>& frame_tree_node_ids,
      const PrerenderCancellationReason& reason);

  // Applies CancelHost for all existing PrerenderHost.
  void CancelAllHosts(PrerenderFinalStatus final_status);

  // For activators. Finds the host to activate for a navigation for the given
  // NavigationRequest. Returns the root frame tree node id of the prerendered
  // page, which can be used as the id of the host. This doesn't reserve the
  // host so it can be destroyed or activated by another navigation. This also
  // cancels all the prerender hosts except the one to be activated. See also
  // comments on ReserveHostToActivate().
  FrameTreeNodeId FindPotentialHostToActivate(
      NavigationRequest& navigation_request);

  // For activators. Reserves the host to activate for a navigation for the
  // given NavigationRequest. Returns the root frame tree node id of the
  // prerendered page, which can be used as the id of the host. Returns an
  // invalid FrameTreeNodeId if it's not found or not ready for activation yet.
  // The caller is responsible for calling OnActivationFinished() with the id to
  // release the reserved host. This also cancels all the prerender hosts except
  // the one to be activated.
  //
  // TODO(crbug.com/40177514): Consider returning the ownership of the reserved
  // host and letting NavigationRequest own it instead of PrerenderHostRegistry.
  FrameTreeNodeId ReserveHostToActivate(NavigationRequest& navigation_request,
                                        FrameTreeNodeId expected_host_id);

  // For activators.
  // Activates the host reserved by ReserveHostToActivate() and returns the
  // StoredPage containing the page that was activated on success, or nullptr
  // on failure.
  std::unique_ptr<StoredPage> ActivateReservedHost(
      FrameTreeNodeId frame_tree_node_id,
      NavigationRequest& navigation_request);

  RenderFrameHostImpl* GetRenderFrameHostForReservedHost(
      FrameTreeNodeId frame_tree_node_id);

  // For activators.
  // Called from the destructor of NavigationRequest that reserved the host.
  // `frame_tree_node_id` should be the id returned by ReserveHostToActivate().
  void OnActivationFinished(FrameTreeNodeId frame_tree_node_id);

  // Returns the non-reserved host with the given id. Returns nullptr if the id
  // does not match any non-reserved host.
  PrerenderHost* FindNonReservedHostById(FrameTreeNodeId frame_tree_node_id);

  // Returns true if this registry reserves a host for activation.
  bool HasReservedHost() const;

  // Returns the ownership of a pre-created WebContentsImpl that contains a
  // prerendered page that corresponds to the given params for a new tab
  // navigation, if it exists.
  std::unique_ptr<WebContentsImpl> TakePreCreatedWebContentsForNewTabIfExists(
      const mojom::CreateNewWindowParams& create_new_window_params,
      const WebContents::CreateParams& web_contents_create_params);

  // Returns the FrameTrees owned by this registry's prerender hosts.
  std::vector<FrameTree*> GetPrerenderFrameTrees();

  // Returns the non-reserved host for `prerendering_url`. Returns nullptr if
  // the URL doesn't match any non-reserved host.
  PrerenderHost* FindHostByUrlForTesting(const GURL& prerendering_url);

  // Returns whether prerender_new_tab_handle_by_frame_tree_node_id_ has the
  // given id.
  bool HasNewTabHandleByIdForTesting(FrameTreeNodeId frame_tree_node_id);

  // Cancels all hosts.
  void CancelAllHostsForTesting();

  // Gets the trigger type from the reserved PrerenderHost.
  PreloadingTriggerType GetPrerenderTriggerType(
      FrameTreeNodeId frame_tree_node_id);
  // Gets the embedder histogram suffix from the reserved PrerenderHost. Only
  // used for metrics.
  const std::string& GetPrerenderEmbedderHistogramSuffix(
      FrameTreeNodeId frame_tree_node_id);

  // Represents the group of prerender limit calculated by PreloadingTriggerType
  // and SpeculationEagerness on GetPrerenderLimitGroup.
  // Currently, this is used when kPrerender2NewLimitAndScheduler is enabled.
  enum class PrerenderLimitGroup {
    kSpeculationRulesEager,
    kSpeculationRulesNonEager,
    kEmbedder,
  };

  // May be called when it is believed to be likely that the user will perform a
  // back navigation due to the trigger indicated by `predictor` (e.g. they're
  // hovering over a back button).
  void BackNavigationLikely(PreloadingPredictor predictor);

  base::WeakPtr<PrerenderHostRegistry> GetWeakPtr();

  // Only used for tests.
  base::OneShotTimer* GetEmbedderTimerForTesting() {
    return &timeout_timer_for_embedder_;
  }
  base::OneShotTimer* GetSpeculationRulesTimerForTesting() {
    return &timeout_timer_for_speculation_rules_;
  }
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  bool HasOngoingHttpCacheQueryForTesting() const {
    return !!http_cache_query_loader_;
  }

  bool PrerenderCanBeStartedWhenInitiatorIsInBackground();

 private:
  // WebContentsObserver implementation:
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(Visibility visibility) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  bool CancelHostInternal(FrameTreeNodeId frame_tree_node_id,
                          const PrerenderCancellationReason& reason);
  bool CancelNewTabHostInternal(FrameTreeNodeId frame_tree_node_id,
                                const PrerenderCancellationReason& reason);

  // Returns true if `navigation_request` can activate `host`.
  bool CanNavigationActivateHost(NavigationRequest& navigation_request,
                                 PrerenderHost& host);

  void ScheduleToDeleteAbandonedHost(
      std::unique_ptr<PrerenderHost> prerender_host,
      const PrerenderCancellationReason& cancellation_reason);
  void DeleteAbandonedHosts();

  void NotifyTrigger(const GURL& url);
  void NotifyCancel(FrameTreeNodeId host_frame_tree_node_id,
                    const PrerenderCancellationReason& reason);

  // Pops one PrerenderHost from the queue and starts the prerendering if
  // there's no running prerender and `kNoFrameTreeNode` is passed as
  // `frame_tree_node_id`. If the given `frame_tree_node_id` is valid, this
  // function starts prerendering for the id. Returns starting prerender host id
  // when it succeeds, and returns an invalid FrameTreeNodeId if it's cancelled.
  FrameTreeNodeId StartPrerendering(FrameTreeNodeId frame_tree_node_id);

  // Cancels the existing hosts that were triggered by `trigger_types`.
  void CancelHostsForTriggers(std::vector<PreloadingTriggerType> trigger_types,
                              const PrerenderCancellationReason& reason);

  // Calculates PrerenderLimitGroup by PreloadingTriggerType and
  // SpeculationEagerness.
  // Currently, this is only used under kPrerender2NewLimitAndScheduler.
  PrerenderLimitGroup GetPrerenderLimitGroup(
      PreloadingTriggerType trigger_type,
      std::optional<blink::mojom::SpeculationEagerness> eagerness);

  // Returns the number of hosts that prerender_host_by_frame_tree_node_id_
  // holds by limit group.
  int GetHostCountByLimitGroup(PrerenderLimitGroup limit_group);

  // Returns whether a certain type of PreloadingTriggerType is allowed to be
  // added to PrerenderHostRegistry according to the limit of the given
  // PreloadingTriggerType.
  // If kPrerender2NewLimitAndScheduler is enabled, SpeculationEagerness is
  // additionally considered to apply the new limits and behaviors according to
  // PrerenderLimitGroup.
  bool IsAllowedToStartPrerenderingForTrigger(
      PreloadingTriggerType trigger_type,
      std::optional<blink::mojom::SpeculationEagerness> eagerness);

  // Called when we have the HTTP cache result of the main resource of the back
  // navigation queried by `BackNavigationLikely`.
  void OnBackResourceCacheResult(
      PreloadingPredictor predictor,
      base::WeakPtr<PreloadingAttempt> attempt,
      GURL back_url,
      scoped_refptr<net::HttpResponseHeaders> headers);

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  scoped_refptr<base::SingleThreadTaskRunner> GetTimerTaskRunner();

  // Holds the frame_tree_node_id of running PrerenderHost. Reset to an invalid
  // value when there's no running PrerenderHost. Tracks only the host id of
  // speculation rules triggers and ignores requests from embedder because
  // embedder requests are more urgent and we'd like to handle embedder
  // prerender independently from speculation rules requests.
  FrameTreeNodeId running_prerender_host_id_;

  // Holds the ids of upcoming prerender requests. The requests from embedder
  // trigger are prioritized and pushed to the front of the queue, while the
  // requests from the speculation rules are appended to the back. This may
  // contain ids of cancelled requests. You can identify cancelled requests by
  // checking if an id is in `prerender_host_by_frame_tree_node_id_`.
  base::circular_deque<FrameTreeNodeId> pending_prerenders_;

  // Hosts that are not reserved for activation yet. This map also includes the
  // hosts still waiting for their start.
  // TODO(crbug.com/40150744): Expire prerendered contents if they are
  // not used for a while.
  base::flat_map<FrameTreeNodeId, std::unique_ptr<PrerenderHost>>
      prerender_host_by_frame_tree_node_id_;

  // Holds the host id of non-eager prerenders by their arrival order.
  // Currently, it is used to calculate the oldest prerender on
  // GetOldestHostPerLimitGroup for kPrerender2NewLimitAndScheduler.
  base::circular_deque<FrameTreeNodeId>
      non_eager_prerender_host_id_by_arrival_order_;

  // The host that is reserved for activation.
  std::unique_ptr<PrerenderHost> reserved_prerender_host_;

  // Handles that manage WebContents for prerendering in new tabs.
  base::flat_map<FrameTreeNodeId, std::unique_ptr<PrerenderNewTabHandle>>
      prerender_new_tab_handle_by_frame_tree_node_id_;

  // Hosts that are scheduled to be deleted asynchronously.
  // Design note: PrerenderHostRegistry should explicitly manage the hosts to be
  // deleted instead of depending on the deletion helpers like DeleteSoon() to
  // asynchronously destruct them before this instance is deleted. The helpers
  // could let the hosts and their FrameTrees outlive WebContentsImpl (the owner
  // of the registry) and results in UAF.
  std::vector<std::unique_ptr<PrerenderHost>> to_be_deleted_hosts_;

  // Starts running the timers when prerendering gets hidden.
  base::OneShotTimer timeout_timer_for_embedder_;
  base::OneShotTimer timeout_timer_for_speculation_rules_;
  // Only used for tests. This task runner is used for precise injection in
  // tests and for timing control.
  scoped_refptr<base::SingleThreadTaskRunner> timer_task_runner_for_testing_;

  // A pending cache-only load of a URL, used to identify whether there is an
  // entry for it in the HTTP cache.
  std::unique_ptr<network::SimpleURLLoader> http_cache_query_loader_;

  base::MemoryPressureListener memory_pressure_listener_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<PrerenderHostRegistry> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_HOST_REGISTRY_H_
