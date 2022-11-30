// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_OWNER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_OWNER_H_

#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom-forward.h"

namespace content {

class NavigationRequest;
class Navigator;
class RenderFrameHostManager;

// An interface for RenderFrameHostImpl to communicate with FrameTreeNode owning
// it (e.g. to initiate or cancel a navigation in the frame).
//
// As main RenderFrameHostImpl can be moved between different FrameTreeNodes
// (i.e.during prerender activations), RenderFrameHostImpl should not reference
// FrameTreeNode directly to prevent accident violation of implicit "associated
// FTN stays the same" assumptions. Instead, a targeted interface is exposed
// instead.
//
// If you need to store information which should persist during prerender
// activations and same-BrowsingContext navigations, consider using
// BrowsingContextState instead.
class RenderFrameHostOwner {
 public:
  RenderFrameHostOwner() = default;
  virtual ~RenderFrameHostOwner() = default;

  virtual void RestartNavigationAsCrossDocument(
      std::unique_ptr<NavigationRequest> navigation_request) = 0;

  virtual Navigator& GetCurrentNavigator() = 0;

  virtual RenderFrameHostManager& GetRenderFrameHostManager() = 0;

  virtual void SetFocusedFrame(SiteInstanceGroup* source) = 0;

  virtual bool UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType update_type,
      blink::mojom::UserActivationNotificationType notification_type) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_OWNER_H_
