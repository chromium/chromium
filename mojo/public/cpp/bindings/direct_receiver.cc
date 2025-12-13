// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/direct_receiver.h"

#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/debug/crash_logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/core/ipcz_driver/driver.h"
#include "mojo/core/ipcz_driver/transport.h"
#include "mojo/public/cpp/system/handle.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::internal {

namespace {

using Transport = core::ipcz_driver::Transport;

// The output of Transport::CreatePair().
using TransportPair =
    std::pair<scoped_refptr<Transport>, scoped_refptr<Transport>>;

TransportPair CreateTransportPair() {
  const Transport::EndpointType global_node_type =
      core::GetIpczNodeOptions().is_broker
          ? Transport::EndpointType::kBroker
          : Transport::EndpointType::kNonBroker;
  const Transport::EndpointType local_node_type =
      Transport::EndpointType::kNonBroker;

  TransportPair transports =
      Transport::CreatePair(global_node_type, local_node_type);
  transports.first->set_remote_process(base::Process::Current());
  transports.second->set_remote_process(base::Process::Current());
  return transports;
}

#if BUILDFLAG(IS_WIN)

bool g_use_precreated_transport = false;

class TransportPairStorage {
 public:
  static TransportPairStorage& Get();

  // Creates a TransportPair and stores it to be used inside the sandbox.
  void CreateTransportPairBeforeSandbox();

  // Returns a TransportPair that was created outside the sandbox. Asserts if
  // there are none available.
  TransportPair TakeTransportPair();

 private:
  base::Lock lock_;
  std::optional<TransportPair> transport_pair_ GUARDED_BY(lock_);
};

// static
TransportPairStorage& TransportPairStorage::Get() {
  static base::NoDestructor<TransportPairStorage> instance;
  return *instance;
}

void TransportPairStorage::CreateTransportPairBeforeSandbox() {
  base::AutoLock lock(lock_);
  CHECK(!transport_pair_.has_value());
  transport_pair_ = CreateTransportPair();
}

TransportPair TransportPairStorage::TakeTransportPair() {
  base::AutoLock lock(lock_);
  return std::exchange(transport_pair_, std::nullopt).value();
}

#endif  // BUILDFLAG(IS_WIN)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(MojoAdoptPipeResult)
enum class MojoAdoptPipeResult {
  kSuccess = 0,
  kTransportNotConnected = 1,
  kPutFailed = 2,
  kMaxValue = kPutFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/others/enums.xml:MojoAdoptPipeResult)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(MojoMergePortalsResult)
enum class MojoMergePortalsResult {
  kSuccess = 0,
  kNotAttempted = 1,
  kGetFailed = 2,
  kMaxValue = kGetFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/others/enums.xml:MojoMergePortalsResult)

void LogAdoptPipeResult(MojoAdoptPipeResult result) {
  base::UmaHistogramEnumeration("Mojo.DirectReceiver.AdoptPipeResult", result);
}

void LogMergePortalsResult(MojoMergePortalsResult result) {
  base::UmaHistogramEnumeration("Mojo.DirectReceiver.MergePortalsResult",
                                result);
}

thread_local ThreadLocalNode* g_thread_local_node;

}  // namespace

ThreadLocalNode::ThreadLocalNode(base::PassKey<ThreadLocalNode>) {
  CHECK(IsDirectReceiverSupported());
  CHECK(!g_thread_local_node);
  g_thread_local_node = this;

  scoped_refptr<Transport> global_transport;
  scoped_refptr<Transport> local_transport;
#if BUILDFLAG(IS_WIN)
  if (g_use_precreated_transport) {
    std::tie(global_transport, local_transport) =
        TransportPairStorage::Get().TakeTransportPair();
    // Leak the node in case it needs to outlive the last DirectReceiver,
    // since in a sandboxed process it can't be recreated.
    AddRef();
  } else {
    std::tie(global_transport, local_transport) = CreateTransportPair();
  }
#else
  std::tie(global_transport, local_transport) = CreateTransportPair();
#endif
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
  const core::IpczNodeOptions& global_node_options = core::GetIpczNodeOptions();
  IpczConnectNodeFlags local_connect_flags;
  IpczConnectNodeFlags global_connect_flags;
  if (global_node_options.is_broker) {
    global_connect_flags = IPCZ_NO_FLAGS;
    local_connect_flags = IPCZ_CONNECT_NODE_TO_BROKER;
  } else {
    global_connect_flags = IPCZ_CONNECT_NODE_SHARE_BROKER;
    local_connect_flags = IPCZ_CONNECT_NODE_INHERIT_BROKER;
    if (!global_node_options.use_local_shared_memory_allocation) {
      local_connect_flags |= IPCZ_CONNECT_NODE_TO_ALLOCATION_DELEGATE;
    }
  }

  // We want the new local node to receive all I/O directly on the current
  // thread. Since this is the first transport connected on that node, all
  // other connections made by ipcz on behalf of this node will also bind I/O
  // to this thread.
  local_transport->OverrideIOTaskRunner(
      base::SingleThreadTaskRunner::GetCurrentDefault());

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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const IpczAPI& ipcz = core::GetIpczAPI();

  IpczHandle portal_to_adopt = pipe.release().value();

  // `portal_to_adopt` is currently routed to the global node. To update its
  // routing to the local node, it needs to be sent over the link to that node
  // (by writing it to `global_portal_` and reading it from `local_portal_`).
  // But this is asynchronous, and AdoptPipe needs to return a portal routed to
  // the local node immediately.
  //
  // Create a new portal pair within our local node. One of these portals is
  // returned and the other will be merged with `portal_to_adopt` once it's
  // transferred to the local node. This allows us to synchronously return a
  // pipe while the portal transfer remains asynchronous.
  //
  // TODO(crbug.com/40266729): Find a way to make the transfer synchronous.
  // This would require copying the portal-transfer logic from
  // RemoteRouterLink::AcceptParcel, which currently only runs as part of
  // deserializing messages over a link.
  IpczHandle portal_to_bind, portal_to_merge;
  const IpczResult open_result =
      ipcz.OpenPortals(node_->value(), IPCZ_NO_FLAGS, nullptr, &portal_to_bind,
                       &portal_to_merge);
  // OpenPortals should only fail due to invalid arguments, which is a coding
  // error.
  CHECK_EQ(open_result, IPCZ_RESULT_OK);

  // Stash the portal for later merge.
  const uint64_t merge_id = next_merge_id_++;
  pending_merges_[merge_id] = ScopedHandle{Handle{portal_to_merge}};

  // Send `portal_to_adopt` to the local node along with our unique merge ID.
  const IpczResult put_result = ipcz.Put(
      global_portal_->value(), &merge_id, sizeof(merge_id),
      /*handles=*/&portal_to_adopt, /*num_handles=*/1, IPCZ_NO_FLAGS, nullptr);
  if (put_result != IPCZ_RESULT_OK) {
    // If the *first* attempt to write data to `global_portal_` fails with
    // IPCZ_RESULT_NOT_FOUND, it's probably because the underlying transport
    // never connected in the first place.
    LogAdoptPipeResult(merge_id == 1 && put_result == IPCZ_RESULT_NOT_FOUND
                           ? MojoAdoptPipeResult::kTransportNotConnected
                           : MojoAdoptPipeResult::kPutFailed);
    LogMergePortalsResult(MojoMergePortalsResult::kNotAttempted);

    // Put() only takes ownership of `portal_to_adopt` on success, so it's safe
    // to keep using it.
    return ScopedMessagePipeHandle{MessagePipeHandle{portal_to_adopt}};
  }

  LogAdoptPipeResult(MojoAdoptPipeResult::kSuccess);
  return ScopedMessagePipeHandle{MessagePipeHandle{portal_to_bind}};
}

void ThreadLocalNode::ReplacePortalForTesting(ScopedHandle dummy_portal) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  global_portal_ = std::move(dummy_portal);
}

void ThreadLocalNode::WatchForIncomingTransfers() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  if (get_result != IPCZ_RESULT_OK) {
    LogMergePortalsResult(MojoMergePortalsResult::kGetFailed);
    return;
  }
  CHECK_EQ(num_bytes, sizeof(merge_id));
  CHECK_EQ(num_portals, 1u);
  CHECK_NE(portal, IPCZ_INVALID_HANDLE);

  auto it = pending_merges_.find(merge_id);
  CHECK(it != pending_merges_.end());
  const IpczResult merge_result = ipcz.MergePortals(
      portal, it->second.release().value(), IPCZ_NO_FLAGS, nullptr);
  // MergePortals should only fail due to invalid arguments or unmet
  // preconditions, which are coding errors.
  CHECK_EQ(merge_result, IPCZ_RESULT_OK);
  pending_merges_.erase(it);
  LogMergePortalsResult(MojoMergePortalsResult::kSuccess);
}

}  // namespace mojo::internal

namespace mojo {

bool IsDirectReceiverSupported() {
  return core::IsMojoIpczEnabled();
}

#if BUILDFLAG(IS_WIN)

void CreateDirectReceiverTransportBeforeSandbox() {
  CHECK(!internal::g_use_precreated_transport);
  internal::g_use_precreated_transport = true;
  if (IsDirectReceiverSupported()) {
    internal::TransportPairStorage::Get().CreateTransportPairBeforeSandbox();
  }
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace mojo
