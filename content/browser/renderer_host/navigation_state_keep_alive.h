// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_STATE_KEEP_ALIVE_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_STATE_KEEP_ALIVE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/remote_frame.mojom.h"

namespace content {

class PolicyContainerHost;
class RenderFrameHostImpl;

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

 private:
  friend class RenderFrameHostImpl;

  // A NavigationStateKeepAlive is created when
  // RenderFrameHostImpl::IssueKeepAliveHandle is called. The Mojo pending
  // receiver is bound to `this`, and stored on StoragePartition.
  NavigationStateKeepAlive(
      const blink::LocalFrameToken& token,
      scoped_refptr<PolicyContainerHost> policy_container_host);

  // The frame token for the RenderFrameHost this state is associated with.
  const blink::LocalFrameToken frame_token_;

  // The PolicyContainerHost kept alive by `this`.
  // TODO(crbug.com/323753235, yangsharon): Keep a SiteInstanceImpl alive here.
  scoped_refptr<PolicyContainerHost> policy_container_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_STATE_KEEP_ALIVE_H_
