// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_HOST_REGISTRY_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_HOST_REGISTRY_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/types/pass_key.h"
#include "content/browser/preloading/prerender/prerender_attributes.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class RenderFrameHostImpl;

// PrerenderHostRegistry creates and retains a prerender host, and reserves it
// for NavigationRequest to activate the prerendered page. This is created per
// WebContentsImpl and owned by it.
//
// The APIs of this class are categorized into two: APIs for triggers and APIs
// for activators.
//
// - Triggers (e.g., SpeculationHostImpl) start prerendering by
//   CreateAndStartHost() and notify the registry of destruction of the trigger
//   by OnTriggerDestroyed().
// - Activators (i.e., NavigationRequest) can reserve the prerender host on
//   activation start by ReserveHostToActivate(), activate it by
//   ActivateReservedHost(), and notify the registry of completion of the
//   activation by OnActivationFinished().
class CONTENT_EXPORT PrerenderHostRegistry {
 public:
  using PassKey = base::PassKey<PrerenderHostRegistry>;

  PrerenderHostRegistry();
  ~PrerenderHostRegistry();

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
  // TODO(crbug.com/1325073): Remove the default value as nullptr for
  // preloading_attempt once prerendering is integrated with Preloading APIs.
  int CreateAndStartHost(const PrerenderAttributes& attributes,
                         WebContents& web_contents,
                         PreloadingAttempt* preloading_attempt = nullptr);

  // Cancels the host registered for `frame_tree_node_id`. The host is
  // immediately removed from the map of non-reserved hosts but asynchronously
  // destroyed so that prerendered pages can cancel themselves without concern
  // for self destruction.
  // Returns true if a cancelation has occurred.
  bool CancelHost(int frame_tree_node_id,
                  PrerenderHost::FinalStatus final_status);

  // Applies CancelHost for all existing PrerenderHost.
  void CancelAllHosts(PrerenderHost::FinalStatus final_status);

  // For activators.
  // Finds the host to activate for a navigation for the given
  // NavigationRequest. Returns the root frame tree node id of the prerendered
  // page, which can be used as the id of the host. This doesn't reserve the
  // host so it can be destroyed or activated by another navigation. See also
  // comments on ReserveHostToActivate().
  int FindPotentialHostToActivate(NavigationRequest& navigation_request);

  // For activators.
  // Reserves the host to activate for a navigation for the given
  // NavigationRequest. Returns the root frame tree node id of the prerendered
  // page, which can be used as the id of the host. Returns
  // RenderFrameHost::kNoFrameTreeNodeId if it's not found or not ready for
  // activation yet. The caller is responsible for calling
  // OnActivationFinished() with the id to release the reserved host.
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

  // For triggers.
  // Called from the triggers (e.g., SpeculationHostImpl) when they are
  // destroyed. `frame_tree_node_id` should be the id returned by
  // CreateAndStartHost().
  void OnTriggerDestroyed(int frame_tree_node_id);

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

  // Returns the FrameTrees owned by this registry's prerender hosts.
  std::vector<FrameTree*> GetPrerenderFrameTrees();

  // Returns the non-reserved host for `prerendering_url`. Returns nullptr if
  // the URL doesn't match any non-reserved host.
  PrerenderHost* FindHostByUrlForTesting(const GURL& prerendering_url);

  // Cancels all hosts. Since reserved hosts can't be canceled, this will
  // DCHECK when `reserved_prerender_host_by_frame_tree_node_id_` is not empty.
  // This will cancel all hosts in `prerender_host_by_frame_tree_node_id_`.
  void CancelAllHostsForTesting();

  // Gets the trigger type from the reserved PrerenderHost.
  PrerenderTriggerType GetPrerenderTriggerType(int frame_tree_node_id);
  // Gets the embedder histogram suffix from the reserved PrerenderHost. Only
  // used for metrics.
  const std::string& GetPrerenderEmbedderHistogramSuffix(
      int frame_tree_node_id);

  base::WeakPtr<PrerenderHostRegistry> GetWeakPtr();

  // Applies the callback function to all prerender hosts owned by
  // this registry.
  void ForEachPrerenderHost(
      base::RepeatingCallback<void(PrerenderHost&)> callback);

 private:
  int FindHostToActivateInternal(NavigationRequest& navigation_request);

  void ScheduleToDeleteAbandonedHost(
      std::unique_ptr<PrerenderHost> prerender_host,
      PrerenderHost::FinalStatus final_status);
  void DeleteAbandonedHosts();

  void NotifyTrigger(const GURL& url);

  // Returns whether a certain type of PrerenderTriggerType is allowed to be
  // added to PrerenderHostRegistry according to the limit of the given
  // PrerenderTriggerType.
  bool IsAllowedToStartPrerenderingForTrigger(
      PrerenderTriggerType trigger_type);

  // Hosts that are not reserved for activation yet.
  // TODO(https://crbug.com/1132746): Expire prerendered contents if they are
  // not used for a while.
  base::flat_map<int, std::unique_ptr<PrerenderHost>>
      prerender_host_by_frame_tree_node_id_;

  // Hosts that are reserved for activation.
  base::flat_map<int, std::unique_ptr<PrerenderHost>>
      reserved_prerender_host_by_frame_tree_node_id_;

  // Hosts that are scheduled to be deleted asynchronously.
  // Design note: PrerenderHostRegistry should explicitly manage the hosts to be
  // deleted instead of depending on the deletion helpers like DeleteSoon() to
  // asynchronously destruct them before this instance is deleted. The helpers
  // could let the hosts and their FrameTrees outlive WebContentsImpl (the owner
  // of the registry) and results in UAF.
  std::vector<std::unique_ptr<PrerenderHost>> to_be_deleted_hosts_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<PrerenderHostRegistry> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_HOST_REGISTRY_H_
