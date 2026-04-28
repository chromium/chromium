// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_STATE_KEEP_ALIVE_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_STATE_KEEP_ALIVE_H_

#include "base/memory/safe_ref.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/remote_frame.mojom.h"

namespace content {

class PolicyContainerHost;
class RenderFrameHostImpl;
class SiteInstanceImpl;
class StoragePartitionImpl;

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

  blink::LocalFrameToken frame_token() { return frame_token_; }

  PolicyContainerHost* policy_container_host() {
    return policy_container_host_.get();
  }

  SiteInstanceImpl* source_site_instance() {
    return source_site_instance_.get();
  }

 private:
  friend class RenderFrameHostImpl;

  // A NavigationStateKeepAlive is created when
  // RenderFrameHostImpl::IssueKeepAliveHandle is called. The Mojo pending
  // receiver is bound to `this`, and stored on StoragePartition.
  NavigationStateKeepAlive(
      const blink::LocalFrameToken& token,
      scoped_refptr<PolicyContainerHost> policy_container_host,
      scoped_refptr<SiteInstanceImpl> source_site_instance);

  // The frame token for the RenderFrameHost this state is associated with.
  const blink::LocalFrameToken frame_token_;

  // The StoragePartition `this` belongs to. This pointer is stored so that
  // `this` can remove itself from its StoragePartition's frame token map upon
  // destruction. Looking up the StoragePartition at the time poses a risk of
  // recreating a StoragePartition map during BrowserContext shutdown.
  // StoragePartition owns `this`, so the pointer is guaranteed to stay valid.
  // A SafeRef would be ideal to use here, but `this` gets destructed after
  // StoragePartition's WeakPtrFactory goes away.
  raw_ptr<StoragePartitionImpl> storage_partition_;

  // Navigation objects kept alive by `this`. All are parts of navigation state
  // from a RenderFrameHost that is potentially needed after the RenderFrameHost
  // goes away.
  //
  // A newly created document may inherit the PolicyContainerHost of the
  // previous document.
  scoped_refptr<PolicyContainerHost> policy_container_host_;

  // The source SiteInstance is passed in to RenderFrameProxyHost::OpenURL.
  scoped_refptr<SiteInstanceImpl> source_site_instance_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_STATE_KEEP_ALIVE_H_
