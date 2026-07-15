// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_state_keep_alive.h"

#include <utility>

#include "content/browser/browser_context_impl.h"
#include "content/browser/renderer_host/initiator_navigation_state_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/site_instance_group.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"

namespace content {

NavigationStateKeepAlive::NavigationStateKeepAlive(
    scoped_refptr<InitiatorNavigationState> initiator_navigation_state,
    BrowserContextImpl* browser_context)
    : browser_context_(browser_context),
      initiator_navigation_state_(std::move(initiator_navigation_state)) {
  CHECK(initiator_navigation_state_);
  SiteInstanceGroup* group = static_cast<InitiatorNavigationStateImpl*>(
                                 initiator_navigation_state_.get())
                                 ->site_instance()
                                 ->group();
  CHECK(group);

  // Increment the keep alive count on the SiteInstanceGroup. This will
  // artificially increase it for the duration of the NavigationStateKeepAlive
  // lifetime and ensure that RenderFrameProxyHosts for the SiteInstanceGroup
  // will not be destroyed. This prevents an in-flight OpenURL IPC matching this
  // NavigationStateKeepAlive to be destroyed until it is received, which will
  // release the Remote associated to this NavigationStateKeepAlive and trigger
  // its destruction. In the destructor, we will balance the SiteInstanceGroup's
  // keep alive increase, which ensures the group survives until after the
  // NavigationStateKeepAlive is destroyed.
  group->IncrementKeepAliveCount();
}

NavigationStateKeepAlive::~NavigationStateKeepAlive() {
  // The InitiatorNavigationStateImpl's SiteInstance should have a group, but
  // during shutdown, this may not be the case. Because destruction notification
  // through BrowserContext happens later than this call, having a group is not
  // enforced.
  CHECK(initiator_navigation_state_);
  InitiatorNavigationStateImpl* initiator_navigation_state_impl =
      static_cast<InitiatorNavigationStateImpl*>(
          initiator_navigation_state_.get());
  if (initiator_navigation_state_impl->site_instance()->group()) {
    initiator_navigation_state_impl->site_instance()
        ->group()
        ->DecrementKeepAliveCount();
  }

  // There are two pointers to `this` in BrowserContext. One in the
  // ReceiverSet, which owns `this`, and another in the
  // NavigationStateKeepAliveMap. When `this`  gets removed from the
  // ReceiverSet, also remove the map entry to avoid dangling pointers.
  browser_context_->RemoveKeepAliveHandleFromMap(
      initiator_navigation_state_impl->frame_token(), this);
}

}  // namespace content
