// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_state_keep_alive.h"

#include <utility>

#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/site_instance_group.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/storage_partition_impl.h"

namespace content {

NavigationStateKeepAlive::NavigationStateKeepAlive(
    const blink::LocalFrameToken& token,
    scoped_refptr<PolicyContainerHost> policy_container_host,
    scoped_refptr<SiteInstanceImpl> source_site_instance)
    : frame_token_(token),
      storage_partition_(static_cast<StoragePartitionImpl*>(
          source_site_instance->GetBrowserContext()->GetStoragePartition(
              source_site_instance.get()))),
      policy_container_host_(std::move(policy_container_host)),
      source_site_instance_(std::move(source_site_instance)) {
  CHECK(source_site_instance_->group());
  source_site_instance_->group()->IncrementKeepAliveCount();
}

NavigationStateKeepAlive::~NavigationStateKeepAlive() {
  // The SiteInstance should have a group, but during shutdown, this may not be
  // the case. Because destruction notification through BrowserContext happens
  // later than this call, having a group is not enforced.
  if (source_site_instance_->group()) {
    source_site_instance_->group()->DecrementKeepAliveCount();
  }

  // There are two pointers to `this` in StoragePartition. One in the
  // ReceiverSet, which owns `this`, and another in the
  // NavigationStateKeepAliveMap. When `this`  gets removed from the
  // ReceiverSet, also remove the map entry to avoid dangling pointers.
  storage_partition_->RemoveKeepAliveHandleFromMap(frame_token_, this);
}

}  // namespace content
