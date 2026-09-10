// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_STATE_KEEP_ALIVE_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_STATE_KEEP_ALIVE_H_

#include "base/memory/safe_ref.h"
#include "content/public/browser/initiator_navigation_state.h"
#include "third_party/blink/public/mojom/frame/remote_frame.mojom.h"

namespace content {

class RenderFrameHostImpl;

// A keepalive handle for InitiatorNavigationState that may be referenced during
// a navigation, since a navigation can outlive its initiating frame. The
// lifetime of the keepalive handle is tied to a Mojo message pipe; closing the
// message pipe will destroy the corresponding keepalive object. Typically, this
// means that an active navigation retains a mojo::Remote endpoint for a
// `blink::mojom::NavigationStateKeepAliveHandle` until the corresponding
// InitiatorNavigationState is retrieved in the browser process upon reception
// of the navigation IPC.
class NavigationStateKeepAlive
    : public blink::mojom::NavigationStateKeepAliveHandle {
 public:
  NavigationStateKeepAlive(const NavigationStateKeepAlive&) = delete;
  NavigationStateKeepAlive& operator=(const NavigationStateKeepAlive&) = delete;

  ~NavigationStateKeepAlive() override;

  scoped_refptr<InitiatorNavigationState> initiator_navigation_state() const {
    return initiator_navigation_state_;
  }

 private:
  friend class RenderFrameHostImpl;

  // A NavigationStateKeepAlive is created when
  // RenderFrameHostImpl::IssueKeepAliveHandle is called. The Mojo pending
  // receiver is bound to `this`, and stored on StoragePartition.
  explicit NavigationStateKeepAlive(
      scoped_refptr<InitiatorNavigationState> initiator_navigation_state);

  // The InitiatorNavigationState being kept alive by `this`.
  scoped_refptr<InitiatorNavigationState> initiator_navigation_state_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_STATE_KEEP_ALIVE_H_
