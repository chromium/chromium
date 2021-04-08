// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRERENDER_PRERENDER_HOST_REGISTRY_H_
#define CONTENT_BROWSER_PRERENDER_PRERENDER_HOST_REGISTRY_H_

#include <map>

#include "base/callback_forward.h"
#include "base/observer_list_types.h"
#include "base/types/pass_key.h"
#include "content/browser/prerender/prerender_host.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class FrameTreeNode;
class RenderFrameHostImpl;
class WebContentsImpl;

// Prerender2:
// PrerenderHostRegistry creates and retains a prerender host, and reserves it
// for NavigationRequest to activate the prerendered page. This is created and
// owned by StoragePartitionImpl.
// TODO(https://crbug.com/1170619): Tie the registry with WebContentsImpl.
//
// The APIs of this class are categorized into two: APIs for triggers and APIs
// for activators.
//
// - Triggers (e.g., PrerenderProcessor) can request to create a new prerender
//   host by CreateAndStartHost() and cancel it by AbandonHost(Async)().
//   Triggers cannot cancel the host after it's preserved by an activator.
// - Activators (i.e., NavigationRequest) can reserve the prerender host on
//   activation start by ReserveHostToActivate() and activate it by
//   ActivateReservedHost(). They can abandon the host by
//   AbandonPreservedHost().
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
    // Called once per CreateAndStartHost() call. Does not necessarily
    // mean a host was created.
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
  int CreateAndStartHost(blink::mojom::PrerenderAttributesPtr attributes,
                         RenderFrameHostImpl& initiator_render_frame_host);

  // For triggers.
  // Destroys the host registered for `frame_tree_node_id`.
  // TODO(https://crbug.com/1169594): Distinguish two paths that cancel
  // prerendering. A prerender can be canceled due to the following reasons:
  // 1. Initiator was no longer interested. Since one prerender may have several
  // initiators, PrerenderHostRegistry should not destroy a PrerenderHost
  // instance if one of the initiators is still alive.
  // 2. Prerendering page did something undesirable. The same behavior always
  // happens regardless of which caller calls it. So PrerenderHostRegistry
  // should destroy the PrerenderHost.
  void AbandonHost(int frame_tree_node_id);

  // For triggers.
  // This is the same with AbandonHost but destroys the prerender host
  // asynchronously so that the prerendered page itself can cancel prerendering
  // without concern for self destruction.
  void AbandonHostAsync(int frame_tree_node_id,
                        PrerenderHost::FinalStatus final_status);

  // TODO(https://crbug.com/1194865): Remove the following method once the
  // workaround in the call site is removed.
  void AbandonAllHostsForWebContents(const WebContentsImpl& web_contents);

  // For activators.
  // Reserves the host to activate for a navigation for the given FrameTreeNode.
  // Returns the root frame tree node id of the prerendered page, which can be
  // used as the id of the host. Returns RenderFrameHost::kNoFrameTreeNodeId if
  // it's not found or not ready for activation yet. The caller is responsible
  // for calling ActivateReservedHost() or AbandonReservedHost() with the id to
  // release the reserved host.
  int ReserveHostToActivate(const GURL& navigation_url,
                            FrameTreeNode& frame_tree_node);

  // For activators.
  // Activates the host reserved by ReserveHostToActivate().
  // - For MPArch, this returns the BackForwardCacheImpl::Entry containing the
  //   page that was activated on success, or nullptr on failure.
  // - For multiple WebContents, this always returns nullptr.
  // TODO(crbug.com/1183519): Remove multiple WebContents
  // implementation after MPArch activation is sufficiently stable.
  //
  // `current_render_frame_host` is the RenderFrameHostImpl that will be swapped
  // out and destroyed by the activation.
  std::unique_ptr<BackForwardCacheImpl::Entry> ActivateReservedHost(
      int frame_tree_node_id,
      RenderFrameHostImpl& current_render_frame_host,
      NavigationRequest& navigation_request);

  RenderFrameHostImpl* GetRenderFrameHostForReservedHost(
      int frame_tree_node_id);

  // For activators.
  // Abandons the host reserved by ReserveHostToActivate().
  void AbandonReservedHost(int frame_tree_node_id);

  // Returns the non-reserved host. Returns nullptr if the frame tree node id
  // doesn't match any host.
  PrerenderHost* FindHostById(int frame_tree_node_id);

  // Returns the non-reserved host for `prerendering_url`. Returns nullptr if
  // the URL doesn't match any non-reserved host.
  PrerenderHost* FindHostByUrlForTesting(const GURL& prerendering_url);

 private:
  std::unique_ptr<PrerenderHost> AbandonHostInternal(int frame_tree_node_id);

  void NotifyTrigger(const GURL& url);

  // Hosts that are not reserved for activation yet.
  // TODO(https://crbug.com/1132746): Expire prerendered contents if they are
  // not used for a while.
  std::map<int, std::unique_ptr<PrerenderHost>>
      prerender_host_by_frame_tree_node_id_;
  std::map<GURL, int> frame_tree_node_id_by_url_;

  // Hosts that are reserved for activation.
  std::map<int, std::unique_ptr<PrerenderHost>>
      reserved_prerender_host_by_frame_tree_node_id_;

  base::ObserverList<Observer> observers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRERENDER_PRERENDER_HOST_REGISTRY_H_
