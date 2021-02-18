// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRERENDER_PRERENDER_HOST_H_
#define CONTENT_BROWSER_PRERENDER_PRERENDER_HOST_H_

#include <memory>

#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom.h"
#include "url/gurl.h"

namespace content {

class FrameTree;
class NavigationController;
class RenderFrameHostImpl;
class WebContents;
class FrameTree;

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
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class FinalStatus {
    kActivated = 0,
    kDestroyed = 1,
    kLowEndDevice = 2,
    kMaxValue = kLowEndDevice
  };

  PrerenderHost(blink::mojom::PrerenderAttributesPtr attributes,
                const url::Origin& initiator_origin,
                WebContentsImpl& web_contents);
  ~PrerenderHost() override;

  PrerenderHost(const PrerenderHost&) = delete;
  PrerenderHost& operator=(const PrerenderHost&) = delete;
  PrerenderHost(PrerenderHost&&) = delete;
  PrerenderHost& operator=(PrerenderHost&&) = delete;

  void StartPrerendering();

  // WebContentsObserver implementation:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  // Activates the prerendered contents. Returns false when activation didn't
  // occur for some reason. This must be called after this host gets ready for
  // activation. `current_render_frame_host` is the RenderFrameHost that will
  // be swapped out and destroyed by the activation.
  bool ActivatePrerenderedContents(
      RenderFrameHostImpl& current_render_frame_host);

  // Exposes the main RenderFrameHost of the prerendered page for testing.
  // This must be called after StartPrerendering() and before
  // ActivatePrerenderedContents().
  RenderFrameHostImpl* GetPrerenderedMainFrameHostForTesting();

  // Waits until the page load finishes.
  void WaitForLoadStopForTesting();

  const GURL& GetInitialUrl() const;

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
  const GlobalFrameRoutingId initiator_render_frame_host_id_;
  const url::Origin initiator_origin_;

  // Indicates if `page_holder_` is ready for activation.
  bool is_ready_for_activation_ = false;

  // The ID of the root node of the frame tree for the prerendered page `this`
  // is hosting. Since PrerenderHost has 1:1 correspondence with FrameTree,
  // this is also used for the ID of this PrerenderHost.
  int frame_tree_node_id_ = RenderFrameHost::kNoFrameTreeNodeId;

  base::Optional<FinalStatus> final_status_;

  std::unique_ptr<PageHolderInterface> page_holder_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRERENDER_PRERENDER_HOST_H_
