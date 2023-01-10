// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_HOST_REGISTRY_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_HOST_REGISTRY_H_

#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "base/types/pass_key.h"
#include "content/browser/preloading/prerender/prerender_attributes.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/common/content_export.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"
#include "url/gurl.h"

namespace content {

class PrerenderNewTabHandle;
class RenderFrameHostImpl;
class PrerenderCancellationReason;

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
  // value of BFCache's eviction timer.
  static constexpr base::TimeDelta kTimeToLiveInBackgroundForEmbedder =
      base::Seconds(19);
  static constexpr base::TimeDelta kTimeToLiveInBackgroundForSpeculationRules =
      base::Seconds(180);

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
  // TODO(crbug.com/1350676): Remove the default value as nullptr for
  // preloading_attempt once new-tab-prerender is integrated with Preloading
  // APIs.
  int CreateAndStartHost(const PrerenderAttributes& attributes,
                         PreloadingAttempt* preloading_attempt = nullptr);

  // Creates and starts a host in a new WebContents so that a navigation in a
  // new tab will be able to activate it. PrerenderHostRegistry associated with
  // the new WebContents manages the started host, and `this`
  // PrerenderHostRegistry manages PrerenderNewTabHandle that owns the
  // WebContents (see `prerender_new_tab_handle_by_frame_tree_node_id_`).
  int CreateAndStartHostForNewTab(const PrerenderAttributes& attributes);

  // Cancels the host registered for `frame_tree_node_id`. The host is
  // immediately removed from the map of non-reserved hosts but asynchronously
  // destroyed so that prerendered pages can cancel themselves without concern
  // for self destruction.
  // Returns true if a cancelation has occurred.
  bool CancelHost(int frame_tree_node_id, PrerenderFinalStatus final_status);
  // Same as CancelHost, but can pass a detailed reason for recording if given.
  bool CancelHost(int frame_tree_node_id,
                  const PrerenderCancellationReason& reason);

  // Cancels the existing hosts specified in the vector with the same reason.
  // Returns a subset of `frame_tree_node_ids` that were actually cancelled.
  std::set<int> CancelHosts(const std::vector<int>& frame_tree_node_ids,
                            const PrerenderCancellationReason& reason);

  // Cancels the existing hosts that were triggered by `trigger_type`.
  void CancelHostsForTrigger(PrerenderTriggerType trigger_type,
                             const PrerenderCancellationReason& reason);

  // Applies CancelHost for all existing PrerenderHost.
  void CancelAllHosts(PrerenderFinalStatus final_status);

  // For activators. Finds the host to activate for a navigation for the given
  // NavigationRequest. Returns the root frame tree node id of the prerendered
  // page, which can be used as the id of the host. This doesn't reserve the
  // host so it can be destroyed or activated by another navigation. This also
  // cancels all the prerender hosts except the one to be activated. See also
  // comments on ReserveHostToActivate().
  int FindPotentialHostToActivate(NavigationRequest& navigation_request);

  // For activators. Reserves the host to activate for a navigation for the
  // given NavigationRequest. Returns the root frame tree node id of the
  // prerendered page, which can be used as the id of the host. Returns
  // RenderFrameHost::kNoFrameTreeNodeId if it's not found or not ready for
  // activation yet. The caller is responsible for calling
  // OnActivationFinished() with the id to release the reserved host. This also
  // cancels all the prerender hosts except the one to be activated.
  //
  // TODO(https://crbug.com/1198815): Consider returning the ownership of the
  // reserved host and letting NavigationRequest own it instead of
  // PrerenderHostRegistry.
  int ReserveHostToActivate(NavigationRequest& navigation_request,
                            int expected_host_id);

  // For activators.
  // Activates the host reserved by ReserveHostToActivate() and returns the
  // StoredPage containing the page that was activated on success, or nullptr
  // on failure.
  std::unique_ptr<StoredPage> ActivateReservedHost(
      int frame_tree_node_id,
      NavigationRequest& navigation_request);

  RenderFrameHostImpl* GetRenderFrameHostForReservedHost(
      int frame_tree_node_id);

  // For activators.
  // Called from the destructor of NavigationRequest that reserved the host.
  // `frame_tree_node_id` should be the id returned by ReserveHostToActivate().
  void OnActivationFinished(int frame_tree_node_id);

  // Returns the non-reserved host with the given id. Returns nullptr if the id
  // does not match any non-reserved host.
  PrerenderHost* FindNonReservedHostById(int frame_tree_node_id);

  // Returns the reserved host with the given id. Returns nullptr if the id
  // does not match any reserved host.
  PrerenderHost* FindReservedHostById(int frame_tree_node_id);

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

  // Cancels all hosts.
  void CancelAllHostsForTesting();

  // Gets the trigger type from the reserved PrerenderHost.
  PrerenderTriggerType GetPrerenderTriggerType(int frame_tree_node_id);
  // Gets the embedder histogram suffix from the reserved PrerenderHost. Only
  // used for metrics.
  const std::string& GetPrerenderEmbedderHistogramSuffix(
      int frame_tree_node_id);

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

 private:
  // WebContentsObserver implementation:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(Visibility visibility) override;
  void ResourceLoadComplete(
      RenderFrameHost* render_frame_host,
      const GlobalRequestID& request_id,
      const blink::mojom::ResourceLoadInfo& resource_load_info) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  int FindHostToActivateInternal(NavigationRequest& navigation_request);

  void ScheduleToDeleteAbandonedHost(
      std::unique_ptr<PrerenderHost> prerender_host,
      const PrerenderCancellationReason& cancellation_reason);
  void DeleteAbandonedHosts();

  void NotifyTrigger(const GURL& url);

  // Pops one PrerenderHost from the queue and starts the prerendering if
  // there's no running prerender and `kNoFrameTreeNode` is passed as
  // `frame_tree_node_id`. If the given `frame_tree_node_id` is valid, this
  // function starts prerendering for the id. Returns starting prerender host id
  // when it succeeds, and returns `RenderFrameHost::kNoFrameTreeNodeId` if it's
  // cancelled.
  int StartPrerendering(int frame_tree_node_id);

  // Returns whether a certain type of PrerenderTriggerType is allowed to be
  // added to PrerenderHostRegistry according to the limit of the given
  // PrerenderTriggerType.
  bool IsAllowedToStartPrerenderingForTrigger(
      PrerenderTriggerType trigger_type);

  // Destroys a host when the current memory usage is higher than a certain
  // threshold.
  void DestroyWhenUsingExcessiveMemory(int frame_tree_node_id);
  void DidReceiveMemoryDump(
      int frame_tree_node_id,
      bool success,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump);

  scoped_refptr<base::SingleThreadTaskRunner> GetTimerTaskRunner();

  // Holds the frame_tree_node_id of running PrerenderHost. Reset to
  // RenderFrameHost::kNoFrameTreeNodeId when there's no running PrerenderHost.
  // Tracks only the host id of speculation rules triggers and ignores requests
  // from embedder because embedder requests are more urgent and we'd like to
  // handle embedder prerender independently from speculation rules requests.
  // This is valid only when kPrerender2SequentialPrerendering is enabled.
  int running_prerender_host_id_ = RenderFrameHost::kNoFrameTreeNodeId;

  // Holds the ids of upcoming prerender requests. The requests from embedder
  // trigger are prioritized and pushed to the front of the queue, while the
  // requests from the speculation rules are appended to the back. This may
  // contain ids of cancelled requests. You can identify cancelled requests by
  // checking if an id is in `prerender_host_by_frame_tree_node_id_`.
  // This is valid only when kPrerender2SequentialPrerendering is enabled.
  base::circular_deque<int> pending_prerenders_;

  // Hosts that are not reserved for activation yet. This map also includes the
  // hosts still waiting for their start.
  // TODO(https://crbug.com/1132746): Expire prerendered contents if they are
  // not used for a while.
  base::flat_map<int, std::unique_ptr<PrerenderHost>>
      prerender_host_by_frame_tree_node_id_;

  // The host that is reserved for activation.
  std::unique_ptr<PrerenderHost> reserved_prerender_host_;

  // Handles that manage WebContents for prerendering in new tabs.
  base::flat_map<int, std::unique_ptr<PrerenderNewTabHandle>>
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

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<PrerenderHostRegistry> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_HOST_REGISTRY_H_
