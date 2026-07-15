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
class BrowserContextImpl;

// A keepalive handle for state that may be referenced during a navigation,
// since a navigation can outlive its initiating frame. The lifetime of the
// keepalive handle is tied to a Mojo message pipe; closing the message pipe
// will destroy the corresponding keepalive object. Typically, this means
// that an active navigation retains a mojo::Remote endpoint for a
// `blink::mojom::NavigationStateKeepAliveHandle`.
// Anything kept alive by this class needs to be owned by this class, either
// explicitly or collectively (e.g. by scoped_refptr).
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
  NavigationStateKeepAlive(
      scoped_refptr<InitiatorNavigationState> initiator_navigation_state,
      BrowserContextImpl* browser_context);

  // The BrowserContextImpl `this` belongs to. This pointer is stored so that
  // `this` can remove itself from its BrowserContextImpl's frame token map upon
  // destruction. BrowserContextImpl owns `this`, so the pointer is guaranteed
  // to stay valid. A SafeRef would be ideal to use here, but `this` gets
  // destructed after BrowserContextImpl's WeakPtrFactory goes away.
  raw_ptr<BrowserContextImpl> browser_context_;

  // Navigation objects kept alive by `this`. All are parts of navigation state
  // from a RenderFrameHost that is potentially needed after the RenderFrameHost
  // goes away.
  scoped_refptr<InitiatorNavigationState> initiator_navigation_state_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_STATE_KEEP_ALIVE_H_
