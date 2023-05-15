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

// Helpers for the trap set by MovePipeToLocalNode.
struct TrapContext {
  base::WeakPtr<DirectReceiverBase> weak_receiver;
  ScopedHandle portal_to_merge;
};

}  // namespace

DirectReceiverBase::DirectReceiverBase() {
  CHECK(core::IsMojoIpczEnabled());

  // Create a new (non-broker) node which we will connect below to the global
  // Mojo ipcz node in this process.
  const IpczAPI& ipcz = core::GetIpczAPI();
  const IpczCreateNodeOptions create_options = {
      .size = sizeof(create_options),
      .memory_flags = IPCZ_MEMORY_FIXED_PARCEL_CAPACITY,
  };
  IpczHandle node;
  const IpczResult create_result =
      ipcz.CreateNode(&core::ipcz_driver::kDriver, IPCZ_INVALID_DRIVER_HANDLE,
                      IPCZ_NO_FLAGS, &create_options, &node);
  CHECK_EQ(create_result, IPCZ_RESULT_OK);
  local_node_.reset(Handle(node));

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
  // thread. Since this is the first transport connected on that node, all other
  // connections made by ipcz on behalf of this node will also bind I/O to this
  // thread.
  local_transport->OverrideIOTaskRunner(
      base::SingleThreadTaskRunner::GetCurrentDefault());

  // Finally, establish mutual connection between the global and local nodes and
  // retain a portal going in either direction. These portals will be used to
  // move the DirectReceiver's own portal from the global node to the local
  // node.
  IpczHandle global_portal;
  const IpczResult global_connect_result = ipcz.ConnectNode(
      core::GetIpczNode(),
      Transport::ReleaseAsHandle(std::move(global_transport)),
      /*num_initial_portals=*/1, global_connect_flags, nullptr, &global_portal);
  CHECK_EQ(global_connect_result, IPCZ_RESULT_OK);
  global_portal_.reset(Handle(global_portal));

  IpczHandle local_portal;
  const IpczResult local_connect_result = ipcz.ConnectNode(
      local_node_->value(),
      Transport::ReleaseAsHandle(std::move(local_transport)),
      /*num_initial_portals=*/1, local_connect_flags, nullptr, &local_portal);
  CHECK_EQ(local_connect_result, IPCZ_RESULT_OK);
  local_portal_.reset(Handle(local_portal));
}

DirectReceiverBase::~DirectReceiverBase() = default;

ScopedMessagePipeHandle DirectReceiverBase::MovePipeToLocalNode(
    ScopedMessagePipeHandle pipe) {
  const IpczAPI& ipcz = core::GetIpczAPI();

  // Create a new portal pair within our local node. One of these portals is
  // returned, and the other will be merged with `pipe` once it's transferred
  // to the local node. This allows us to synchronously return a pipe while the
  // portal transfer remains asynchronous.
  IpczHandle portal_to_bind, portal_to_merge;
  const IpczResult open_result =
      ipcz.OpenPortals(local_node_->value(), IPCZ_NO_FLAGS, nullptr,
                       &portal_to_bind, &portal_to_merge);
  CHECK_EQ(open_result, IPCZ_RESULT_OK);

  // Set up a trap so that when `pipe` arrives on the local node, we can
  // retrieve it and merge it with one of the above portals.
  const IpczTrapConditions conditions = {
      .size = sizeof(conditions),
      .flags = IPCZ_TRAP_ABOVE_MIN_LOCAL_PARCELS,
      .min_local_parcels = 0,
  };
  std::unique_ptr<TrapContext> context{new TrapContext{
      .weak_receiver = weak_ptr_factory_.GetWeakPtr(),
      .portal_to_merge = ScopedHandle{Handle{portal_to_merge}}}};
  const IpczResult trap_result =
      ipcz.Trap(local_portal_->value(), &conditions, &OnTrapEvent,
                reinterpret_cast<uintptr_t>(context.release()), IPCZ_NO_FLAGS,
                nullptr, nullptr, nullptr);
  CHECK_EQ(trap_result, IPCZ_RESULT_OK);

  // Finally, send the pipe to the local node.
  IpczHandle portal = pipe.release().value();
  const IpczResult put_result =
      ipcz.Put(global_portal_->value(), /*data=*/nullptr, /*num_bytes=*/0,
               /*handles=*/&portal, /*num_handles=*/1, IPCZ_NO_FLAGS, nullptr);
  CHECK_EQ(put_result, IPCZ_RESULT_OK);

  return ScopedMessagePipeHandle{MessagePipeHandle{portal_to_bind}};
}

void DirectReceiverBase::OnPipeMovedToLocalNode(ScopedHandle portal_to_merge) {
  // Retrieve the moved pipe from the message sitting on our local portal and
  // merge it with a dangling peer of our receiver's bound portal.
  IpczHandle portal;
  size_t num_portals = 1;
  const IpczAPI& ipcz = core::GetIpczAPI();
  const IpczResult get_result = ipcz.Get(
      local_portal_->value(), IPCZ_NO_FLAGS, nullptr, /*data=*/nullptr,
      /*num_bytes=*/nullptr, /*handles=*/&portal, /*num_handles=*/&num_portals,
      /*parcel=*/nullptr);
  CHECK_EQ(get_result, IPCZ_RESULT_OK);
  CHECK_EQ(num_portals, 1u);
  CHECK_NE(portal, IPCZ_INVALID_HANDLE);

  const IpczResult merge_result = ipcz.MergePortals(
      portal, portal_to_merge.release().value(), IPCZ_NO_FLAGS, nullptr);
  CHECK_EQ(merge_result, IPCZ_RESULT_OK);
}

// static
void DirectReceiverBase::OnTrapEvent(const IpczTrapEvent* event) {
  // There is now a parcel available on the local node for this receiver, which
  // must be the parcel containing the transferred pipe's portal. Since we know
  // I/O (and therefore this event) is happening on the same thread that owns
  // the DirectReceiverBase, it's safe to test the WeakPtr here.
  auto context =
      base::WrapUnique(reinterpret_cast<TrapContext*>(event->context));
  if (!context->weak_receiver) {
    return;
  }

  context->weak_receiver->OnPipeMovedToLocalNode(
      std::move(context->portal_to_merge));
}

}  // namespace mojo::internal
