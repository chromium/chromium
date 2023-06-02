// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_DIRECT_RECEIVER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_DIRECT_RECEIVER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <utility>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace blink {
class WidgetInputHandlerImpl;
}

namespace cc::mojo_embedder {
class AsyncLayerTreeFrameSink;
}

namespace mojo {

namespace internal {

// Encapsulates a thread-local ipcz node which is brought up lazily by any
// DirectReceiver when binding a pipe on a specific thread. The underlying node
// is ref-counted such that a thread's node is automatically torn down when its
// last DirectReceiver goes away.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) ThreadLocalNode
    : public base::RefCounted<ThreadLocalNode> {
 public:
  // Construction requires a PassKey private to this class. In practice,
  // constructed only within the static `Get()` method as needed.`
  explicit ThreadLocalNode(base::PassKey<ThreadLocalNode>);

  // Gets the current thread's ThreadLocalNode instance, initializing a new one
  // if one doesn't already exist.
  static scoped_refptr<ThreadLocalNode> Get();

  // Indicates whether a ThreadLocalNode instance exists for the current thread.
  static bool CurrentThreadHasInstance();

  IpczHandle node() const { return node_->value(); }

  // Transfers `pipe` from the process's global node to the thread-local ipcz
  // node owned by this object and returns a new pipe handle for it.
  ScopedMessagePipeHandle AdoptPipe(ScopedMessagePipeHandle pipe);

 private:
  friend class base::RefCounted<ThreadLocalNode>;

  ~ThreadLocalNode();

  void WatchForIncomingTransfers();
  static void OnTrapEvent(const IpczTrapEvent* event);
  void OnTransferredPortalAvailable();

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // A dedicated node created for this object.
  ScopedHandle node_;

  // A portal on the local node which is connected to `global_portal_`. Used to
  // receive pipes from the global node.
  ScopedHandle local_portal_;

  // A portal on the global node which is connected to `local_portal_`. Used to
  // transfer pipes from the global node to the local one.
  ScopedHandle global_portal_;

  // Tracks pending portal merges. See AdoptPortal() implementation for gritty
  // details.
  uint64_t next_merge_id_ = 0;
  std::map<uint64_t, ScopedHandle> pending_merges_;

  base::WeakPtrFactory<ThreadLocalNode> weak_ptr_factory_{this};
};

}  // namespace internal

namespace test::direct_receiver_unittest {
class ServiceImpl;
}  // namespace test::direct_receiver_unittest

// Key object that must be provided to construct a DirectReceiver instance.
// See notes on DirectReceiver below to understand why this is guarded.
class DirectReceiverKey {
 private:
  DirectReceiverKey() = default;

  // Update this list and get a mojo/OWNERS approval in order to gain access to
  // DirectReceiver construction.
  friend class cc::mojo_embedder::AsyncLayerTreeFrameSink;
  friend class mojo::test::direct_receiver_unittest::ServiceImpl;
  friend class blink::WidgetInputHandlerImpl;
};

// DirectReceiver is a wrapper around the standard Receiver<T> type that always
// receives its messages directly from the sender without an IO-thread hop. To
// enable this safely DirectReceiver is constrained in a few ways:
//
//   - It cannot be unbound and moved once bound
//   - It's always bound on the current default task runner
//   - It must be bound on a thread which uses an IO MessagePump
//
// As long as any DirectReceiver exists on a thread, there is a thread-local
// ThreadLocalNode instance which lives on that thread to receive IPC directly
// from out-of-process peers. When a DirectReceiver is bound, it transfers its
// pipe to that node so that its IPCs are routed to the thread-local node
// instead if the global node, thus ensuring that the receiver's messages are
// received directly on its bound thread.
//
// SUBTLE: DirectReceiver internally allocates a LIMITED SYSTEM RESOURCE on many
// systems (including Android and Chrome OS) and must therefore be used
// sparingly. All usage must be approved by Mojo OWNERS, with access controlled
// by the friend list in DirectReceiverKey above.
//
// EVEN MORE SUBTLE: Any Mojo interface endpoints received in messages to a
// DirectReceiver will also permanently receive I/O on the DirectReceiver's
// thread. While they may be bound on any thread and otherwise behave like any
// other Receiver, their incoming messages will hop through the DirectReceiver's
// thread just as messages to other Receivers normally hop through the global IO
// thread. Unless you're going to bind them all to the same thread as the
// DirectReceiver, passing pipes to your DirectReceiver is likely a BAD IDEA.
template <typename T>
class DirectReceiver {
 public:
  DirectReceiver(DirectReceiverKey, T* impl) : receiver_(impl) {}
  ~DirectReceiver() = default;

  void set_disconnect_handler(base::OnceClosure handler) {
    receiver_.set_disconnect_handler(std::move(handler));
  }

  // Binds this object to `receiver` to receive IPC directly on the calling
  // thread.
  void Bind(PendingReceiver<T> receiver) {
    receiver_.Bind(PendingReceiver<T>{node_->AdoptPipe(receiver.PassPipe())});
  }

  internal::ThreadLocalNode& node_for_testing() { return *node_; }
  Receiver<T>& receiver_for_testing() { return receiver_; }

 private:
  const scoped_refptr<internal::ThreadLocalNode> node_{
      internal::ThreadLocalNode::Get()};
  Receiver<T> receiver_;
};

// Indicates whether DirectReceiver can be supported in the calling process.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS) bool IsDirectReceiverSupported();

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_DIRECT_RECEIVER_H_
