// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_OWNER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_OWNER_H_

namespace content {

class NavigationRequest;

// An interface for RenderFrameHostImpl to communicate with FrameTreeNode owning
// it (e.g. to initiate or cancel a navigation in the frame).
//
// As main RenderFrameHostImpl can be moved between different FrameTreeNodes
// (i.e.during prerender activations), RenderFrameHostImpl should not reference
// FrameTreeNode directly to prevent accident violation of implicit "associated
// FTN says the same" assumptions. Instead, a targeted interface is exposed
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
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_OWNER_H_
