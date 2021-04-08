// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRERENDER_PRERENDER_HOST_H_
#define CONTENT_BROWSER_PRERENDER_PRERENDER_HOST_H_

#include <memory>

#include "base/observer_list_types.h"
#include "base/optional.h"
#include "base/types/pass_key.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom.h"
#include "url/gurl.h"

namespace content {

class FrameTree;
class NavigationController;
class PrerenderHostRegistry;
class RenderFrameHostImpl;
class WebContentsImpl;

// Prerender2:
// PrerenderHost creates a new WebContents and starts prerendering with that.
// Then navigation code is expected to find this host from PrerenderHostRegistry
// and activate the prerendered WebContents upon navigation. This is created per
// request from a renderer process via PrerenderProcessor or will directly be
// created for browser-initiated prerendering (this code path is not implemented
// yet). This is owned by PrerenderHostRegistry.
//
// TODO(https://crbug.com/1132746): This class has two different ways of
// prerendering the page: a dedicated WebContents instance or using a separate
// FrameTree instance (MPArch). You can choose one or the other via the feature
// parameter "implementation". The MPArch code is still in its very early stages
// but will eventually completely replace the WebContents approach.
class CONTENT_EXPORT PrerenderHost : public WebContentsObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called on the page activation.
    virtual void OnActivated() {}

    // Called from the PrerenderHost's destructor. The observer should drop any
    // reference to the host.
    virtual void OnHostDestroyed() {}
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class FinalStatus {
    kActivated = 0,
    kDestroyed = 1,
    kLowEndDevice = 2,
    kCrossOriginRedirect = 3,
    kCrossOriginNavigation = 4,
    kInvalidSchemeRedirect = 5,
    kInvalidSchemeNavigation = 6,
    kInProgressNavigation = 7,
    kNavigationRequestFailure = 8,
    kNavigationRequestBlockedByCsp = 9,
    kMainFrameNavigation = 10,
    kDisallowedMojoInterface = 11,
    kPlugin = 12,
    kMaxValue = kPlugin
  };

  PrerenderHost(blink::mojom::PrerenderAttributesPtr attributes,
                RenderFrameHostImpl& initiator_render_frame_host);
  ~PrerenderHost() override;

  PrerenderHost(const PrerenderHost&) = delete;
  PrerenderHost& operator=(const PrerenderHost&) = delete;
  PrerenderHost(PrerenderHost&&) = delete;
  PrerenderHost& operator=(PrerenderHost&&) = delete;

  void StartPrerendering();

  // WebContentsObserver implementation:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  // Activates the prerendered contents. This must be called after this host
  // gets ready for activation. `old_render_frame_host` is the RenderFrameHost
  // that will be swapped out and destroyed by the activation. For MPArch
  // implementation, returns the activating page prepared for cross-FrameTree
  // transfer. For multiple WebContents implementation, always returns nullptr.
  //
  // TODO(https://crbug.com/1154501): WebContents implementation will need to be
  // removed.
  //
  // TODO(https://crbug.com/1170277): Potentially update implementation so that
  // the |old_render_frame_host| parameter is not required.
  //
  // TODO(https://crbug.com/1181263): Refactor BackForwardCacheImpl::Entry into
  // a generic "page" object to make clear that the same logic is also used for
  // prerendering.
  std::unique_ptr<BackForwardCacheImpl::Entry> ActivatePrerenderedContents(
      RenderFrameHostImpl& old_render_frame_host,
      NavigationRequest& navigation_request);

  // Returns the main RenderFrameHost of the prerendered page.
  // This must be called after StartPrerendering() and before
  // ActivatePrerenderedContents().
  RenderFrameHostImpl* GetPrerenderedMainFrameHost();

  // Tells the reason of the destruction of this host. PrerenderHostRegistry
  // uses this before abandoning the host.
  void RecordFinalStatus(base::PassKey<PrerenderHostRegistry>,
                         FinalStatus status);

  // Waits until the page load finishes.
  void WaitForLoadStopForTesting();

  const GURL& GetInitialUrl() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool IsAssociatedWith(const WebContentsImpl& web_contents);

  url::Origin initiator_origin() const { return initiator_origin_; }

  int frame_tree_node_id() const { return frame_tree_node_id_; }

  bool is_ready_for_activation() const { return is_ready_for_activation_; }

 private:
  // There are two implementations of this interface. One holds the page in a
  // WebContents, and one holds it in a FrameTree (for MPArch).
  // TODO(https://crbug.com/1170277): Remove once MPArch is the only
  // implementation.
  class PageHolderInterface;
  class MPArchPageHolder;
  class WebContentsPageHolder;

  void RecordFinalStatus(FinalStatus status);

  // Returns the frame tree associated with |prerendered_contents_|;
  FrameTree* GetPrerenderedFrameTree();

  void CreatePageHolder(WebContentsImpl& web_contents);

  NavigationController& GetNavigationController();

  const blink::mojom::PrerenderAttributesPtr attributes_;
  const url::Origin initiator_origin_;
  const int initiator_process_id_;
  const blink::LocalFrameToken initiator_frame_token_;

  // Indicates if `page_holder_` is ready for activation.
  bool is_ready_for_activation_ = false;

  // The ID of the root node of the frame tree for the prerendered page `this`
  // is hosting. Since PrerenderHost has 1:1 correspondence with FrameTree,
  // this is also used for the ID of this PrerenderHost.
  int frame_tree_node_id_ = RenderFrameHost::kNoFrameTreeNodeId;

  base::Optional<FinalStatus> final_status_;

  std::unique_ptr<PageHolderInterface> page_holder_;

  base::ObserverList<Observer> observers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRERENDER_PRERENDER_HOST_H_
