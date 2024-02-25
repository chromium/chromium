// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/direct_receiver.h"

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/core/ipcz_driver/driver.h"
#include "mojo/core/ipcz_driver/transport.h"
#include "mojo/public/cpp/system/handle.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::internal {

namespace {

thread_local ThreadLocalNode* g_thread_local_node;

}  // namespace

ThreadLocalNode::ThreadLocalNode(base::PassKey<ThreadLocalNode>)
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  CHECK(IsDirectReceiverSupported());
  CHECK(!g_thread_local_node);
  g_thread_local_node = this;

  // Create a new (non-broker) node which we will connect below to the global
  // Mojo ipcz node in this process.
  const IpczAPI& ipcz = core::GetIpczAPI();
  const IpczCreateNodeOptions create_options = {
      .size = sizeof(create_options),
      .memory_flags = IPCZ_MEMORY_FIXED_PARCEL_CAPACITY,
  };
  IpczHandle node;
  const IpczResult create_result = ipcz.CreateNode(
      &core::ipcz_driver::kDriver, IPCZ_NO_FLAGS, &create_options, &node);
  CHECK_EQ(create_result, IPCZ_RESULT_OK);
  node_.reset(Handle(node));

  // Create a new transport pair to connect the two nodes.
  using Transport = core::ipcz_driver::Transport;
  const core::IpczNodeOptions& global_node_options = core::GetIpczNodeOptions();
  const Transport::EndpointType local_node_type =
      Transport::EndpointType::kNonBroker;
  IpczConnectNodeFlags local_connect_flags;
  Transport::EndpointType global_node_type;
  IpczConnectNodeFlags global_connect_flags;
  if (global_node_options.is_broker) {
    global_node_type = Transport::EndpointType::kBroker;
    global_connect_flags = IPCZ_NO_FLAGS;
    local_connect_flags = IPCZ_CONNECT_NODE_TO_BROKER;
  } else {
    global_node_type = Transport::EndpointType::kNonBroker;
    global_connect_flags = IPCZ_CONNECT_NODE_SHARE_BROKER;
    local_connect_flags = IPCZ_CONNECT_NODE_INHERIT_BROKER;
    if (!global_node_options.use_local_shared_memory_allocation) {
      local_connect_flags |= IPCZ_CONNECT_NODE_TO_ALLOCATION_DELEGATE;
    }
  }
  auto [global_transport, local_transport] =
      Transport::CreatePair(global_node_type, local_node_type);
  global_transport->set_remote_process(base::Process::Current());
  local_transport->set_remote_process(base::Process::Current());

  // We want the new local node to receive all I/O directly on the current
  // thread. Since this is the first transport connected on that node, all
  // other connections made by ipcz on behalf of this node will also bind I/O
  // to this thread.
  local_transport->OverrideIOTaskRunner(task_runner_);

  // Finally, establish mutual connection between the global and local nodes
  // and retain a portal going in either direction. These portals will be
  // used to move each DirectReceiver's own portal from the global node to the
  // local node.
  IpczHandle global_portal;
  const IpczResult global_connect_result = ipcz.ConnectNode(
      core::GetIpczNode(),
      Transport::ReleaseAsHandle(std::move(global_transport)),
      /*num_initial_portals=*/1, global_connect_flags, nullptr, &global_portal);
  CHECK_EQ(global_connect_result, IPCZ_RESULT_OK);
  global_portal_.reset(Handle(global_portal));

  IpczHandle local_portal;
  const IpczResult local_connect_result = ipcz.ConnectNode(
      node_->value(), Transport::ReleaseAsHandle(std::move(local_transport)),
      /*num_initial_portals=*/1, local_connect_flags, nullptr, &local_portal);
  CHECK_EQ(local_connect_result, IPCZ_RESULT_OK);
  local_portal_.reset(Handle(local_portal));

  WatchForIncomingTransfers();
}

ThreadLocalNode::~ThreadLocalNode() {
  CHECK(task_runner_->BelongsToCurrentThread());
  g_thread_local_node = nullptr;
}

// static
scoped_refptr<ThreadLocalNode> ThreadLocalNode::Get() {
  if (g_thread_local_node) {
    return base::WrapRefCounted(g_thread_local_node);
  }
  return base::MakeRefCounted<ThreadLocalNode>(
      base::PassKey<ThreadLocalNode>{});
}

// static
bool ThreadLocalNode::CurrentThreadHasInstance() {
  return g_thread_local_node != nullptr;
}

ScopedMessagePipeHandle ThreadLocalNode::AdoptPipe(
    ScopedMessagePipeHandle pipe) {
  const IpczAPI& ipcz = core::GetIpczAPI();

  // Create a new portal pair within our local node. One of these portals is
  // returned and the other will be merged with `pipe` once it's transferred
  // to the local node. This allows us to synchronously return a pipe while
  // the portal transfer remains asynchronous.
  IpczHandle portal_to_bind, portal_to_merge;
  const IpczResult open_result =
      ipcz.OpenPortals(node_->value(), IPCZ_NO_FLAGS, nullptr, &portal_to_bind,
                       &portal_to_merge);
  CHECK_EQ(open_result, IPCZ_RESULT_OK);

  // Stash the portal for later merge.
  const uint64_t merge_id = next_merge_id_++;
  pending_merges_[merge_id] = ScopedHandle{Handle{portal_to_merge}};

  // Send `pipe` to the local node along with our unique merge ID.
  IpczHandle portal = pipe.release().value();
  const IpczResult put_result =
      ipcz.Put(global_portal_->value(), &merge_id, sizeof(merge_id),
               /*handles=*/&portal, /*num_handles=*/1, IPCZ_NO_FLAGS, nullptr);
  CHECK_EQ(put_result, IPCZ_RESULT_OK);

  return ScopedMessagePipeHandle{MessagePipeHandle{portal_to_bind}};
}

void ThreadLocalNode::WatchForIncomingTransfers() {
  // Set up a trap so that when a portal arrives on the local node we can
  // retrieve it and merge it with the appropriate stashed portal.
  const IpczAPI& ipcz = core::GetIpczAPI();
  const IpczTrapConditions conditions = {
      .size = sizeof(conditions),
      .flags = IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS,
      .min_local_parcels = 0,
  };
  auto context = std::make_unique<base::WeakPtr<ThreadLocalNode>>(
      weak_ptr_factory_.GetWeakPtr());
  for (;;) {
    const IpczResult trap_result =
        ipcz.Trap(local_portal_->value(), &conditions, &OnTrapEvent,
                  reinterpret_cast<uintptr_t>(context.get()), IPCZ_NO_FLAGS,
                  nullptr, nullptr, nullptr);
    if (trap_result == IPCZ_RESULT_OK) {
      context.release();
      return;
    }

    // Can't set a trap because there's already at least one transfer available.
    // Process it and try again.
    CHECK_EQ(trap_result, IPCZ_RESULT_FAILED_PRECONDITION);
    OnTransferredPortalAvailable();
  }
}

// static
void ThreadLocalNode::OnTrapEvent(const IpczTrapEvent* event) {
  // There is now a parcel available on the local portal for this node, which
  // which must be a parcel containing some transferred pipe's portal. Since we
  // we know I/O (and therefore this event) is happening on the same thread
  // that owns the the ThreadLocalNode, it's safe to test the WeakPtr here.
  auto weak_node_ptr = base::WrapUnique(
      reinterpret_cast<base::WeakPtr<ThreadLocalNode>*>(event->context));
  const base::WeakPtr<ThreadLocalNode>& weak_node = *weak_node_ptr;
  if (!weak_node) {
    return;
  }

  weak_node->OnTransferredPortalAvailable();
  weak_node->WatchForIncomingTransfers();
}

void ThreadLocalNode::OnTransferredPortalAvailable() {
  // Retrieve the moved pipe from the message sitting on our local portal and
  // merge it with the appropriate stashed portal.
  IpczHandle portal;
  uint64_t merge_id = 0;
  size_t num_bytes = sizeof(merge_id);
  size_t num_portals = 1;
  const IpczAPI& ipcz = core::GetIpczAPI();
  const IpczResult get_result = ipcz.Get(
      local_portal_->value(), IPCZ_NO_FLAGS, nullptr, &merge_id, &num_bytes,
      /*handles=*/&portal, /*num_handles=*/&num_portals, /*parcel=*/nullptr);
  CHECK_EQ(get_result, IPCZ_RESULT_OK);
  CHECK_EQ(num_bytes, sizeof(merge_id));
  CHECK_EQ(num_portals, 1u);
  CHECK_NE(portal, IPCZ_INVALID_HANDLE);

  auto it = pending_merges_.find(merge_id);
  CHECK(it != pending_merges_.end());
  const IpczResult merge_result = ipcz.MergePortals(
      portal, it->second.release().value(), IPCZ_NO_FLAGS, nullptr);
  CHECK_EQ(merge_result, IPCZ_RESULT_OK);
  pending_merges_.erase(it);
}

}  // namespace mojo::internal

namespace mojo {

bool IsDirectReceiverSupported() {
  return core::IsMojoIpczEnabled();
}

}  // namespace mojo
