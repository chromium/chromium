// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_DIRECT_RECEIVER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_DIRECT_RECEIVER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string_view>
#include <utility>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
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

namespace cc::slim {
class FrameSinkImpl;
}

namespace viz {
class CompositorFrameSinkImpl;
class FrameSinkManagerImpl;
}

namespace mojo {

namespace internal {

// Encapsulates a thread-local ipcz node which is brought up lazily by any
// DirectReceiver when binding a pipe on a specific thread. The underlying node
// is ref-counted such that a thread's node is automatically torn down when its
// last DirectReceiver goes away. (Except in sandboxed processes on Windows,
// which only allow a fixed number of ThreadLocalNodes, so once created they're
// never deleted.)
// TODO(crbug.com/446199357): Remove the refcounting completely. It's unneeded
// complexity.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) ThreadLocalNode
    : public base::RefCounted<ThreadLocalNode> {
 public:
  // Construction requires a PassKey private to this class. In practice,
  // constructed only within the static `Get()` method as needed.
  explicit ThreadLocalNode(base::PassKey<ThreadLocalNode>);

  // Gets the current thread's ThreadLocalNode instance, initializing a new one
  // if one doesn't already exist.
  static scoped_refptr<ThreadLocalNode> Get();

  // Indicates whether a ThreadLocalNode instance exists for the current thread.
  static bool CurrentThreadHasInstance();

  IpczHandle node() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return node_->value();
  }

  // Transfers `pipe` from the process's global node to the thread-local ipcz
  // node owned by this object and returns a new pipe handle for it. If the
  // transfer fails synchronously, falls back to a non-direct receiver by
  // returning `pipe` unchanged. (Note that if the transfer fails
  // *asynchronously*, the new pipe handle will be returned but black-hole any
  // messages sent to it.)
  ScopedMessagePipeHandle AdoptPipe(ScopedMessagePipeHandle pipe);

  // Overwrites the portal used to transfer `pipe` to the thread-local node
  // with a dummy handle, to test how AdoptPipe() handles failures.
  void ReplacePortalForTesting(ScopedHandle dummy_portal);

 private:
  friend class base::RefCounted<ThreadLocalNode>;

  ~ThreadLocalNode();

  void WatchForIncomingTransfers();
  static void OnTrapEvent(const IpczTrapEvent* event);
  void OnTransferredPortalAvailable();

  THREAD_CHECKER(thread_checker_);

  // A dedicated node created for this object.
  ScopedHandle node_ GUARDED_BY_CONTEXT(thread_checker_);

  // A portal on the local node which is connected to `global_portal_`. Used to
  // receive pipes from the global node.
  ScopedHandle local_portal_ GUARDED_BY_CONTEXT(thread_checker_);

  // A portal on the global node which is connected to `local_portal_`. Used to
  // transfer pipes from the global node to the local one.
  ScopedHandle global_portal_ GUARDED_BY_CONTEXT(thread_checker_);

  // Tracks pending portal merges. See AdoptPortal() implementation for gritty
  // details.
  uint64_t next_merge_id_ GUARDED_BY_CONTEXT(thread_checker_) = 0;
  std::map<uint64_t, ScopedHandle> pending_merges_
      GUARDED_BY_CONTEXT(thread_checker_);

  base::WeakPtrFactory<ThreadLocalNode> weak_ptr_factory_
      GUARDED_BY_CONTEXT(thread_checker_){this};
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
  friend class cc::slim::FrameSinkImpl;
  friend class mojo::test::direct_receiver_unittest::ServiceImpl;
  friend class blink::WidgetInputHandlerImpl;
  friend class viz::CompositorFrameSinkImpl;
  friend class viz::FrameSinkManagerImpl;
};

// DirectReceiver is a wrapper around the standard Receiver<T> type that always
// receives its messages directly from the sender without an IO-thread hop. To
// enable this safely DirectReceiver is constrained in a few ways:
//
//   - It cannot be unbound and moved once bound
//   - It's always bound on the current default task runner
//   - It must be bound on a thread whose MessagePump exposes an IOWatcher
//
// As long as any DirectReceiver exists on a thread, there is a thread-local
// ThreadLocalNode instance which lives on that thread to receive IPC directly
// from out-of-process peers. When one of these DirectReceivers is bound to a
// pipe, it indicates that the pipe will be receiving messages on the thread.
// For that to happen the pipe is 'transferred' to the ThreadLocalNode.
//
// TODO(crbug.com/40266729): Find a way to transfer without creating another
// ipcz pipe.
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
  static_assert(
      T::kSupportsDirectReceiver,
      "This interface must be marked with the [DirectReceiver] attribute.");

 public:
  // Creates a DirectReceiver bound to the current thread.
  DirectReceiver(DirectReceiverKey, T* impl) : receiver_(impl) {}
  ~DirectReceiver() = default;

  void set_disconnect_handler(base::OnceClosure handler) {
    receiver_.set_disconnect_handler(std::move(handler));
  }

  bool is_bound() const { return receiver_.is_bound(); }

  void ReportBadMessage(std::string_view error) {
    receiver_.ReportBadMessage(error);
  }

  // Binds this object to `receiver` to receive IPC directly on the calling
  // thread, which must be the same thread the DirectReceiver was created on.
  void Bind(PendingReceiver<T> receiver) {
    receiver_.Bind(receiver.is_valid() ? PendingReceiver<T>(node_->AdoptPipe(
                                             receiver.PassPipe()))
                                       : std::move(receiver));
  }

  void ResetWithReason(uint32_t custom_reason_code,
                       std::string_view description) {
    receiver_.ResetWithReason(custom_reason_code, description);
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

#if BUILDFLAG(IS_WIN)

// The Windows sandbox blocks named pipe creation, so in a sandboxed process
// this must be called before the sandbox is locked down. This is safe to call
// in an unsandboxed process but is not required.
//
// This restricts DirectReceiver to a single thread, although the number of
// threads could be increased by creating more transports if needed.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS)
void CreateDirectReceiverTransportBeforeSandbox();

#endif  // BUILDFLAG(IS_WIN)

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_DIRECT_RECEIVER_H_
