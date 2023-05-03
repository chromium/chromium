// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_DIRECT_RECEIVER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_DIRECT_RECEIVER_H_

#include <utility>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo {

namespace internal {

class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) DirectReceiverBase {
 public:
  DirectReceiverBase();
  ~DirectReceiverBase();

 protected:
  template <typename T>
  PendingReceiver<T> MoveReceiverToLocalNode(PendingReceiver<T> receiver) {
    return PendingReceiver<T>{MovePipeToLocalNode(receiver.PassPipe())};
  }

 private:
  ScopedMessagePipeHandle MovePipeToLocalNode(ScopedMessagePipeHandle pipe);
  void OnPipeMovedToLocalNode(ScopedHandle portal_to_merge);

  static void OnTrapEvent(const IpczTrapEvent* event);

  // The dedicated node created for this receiver.
  ScopedHandle local_node_;

  // A portal on the local node which is connected to `global_portal_`.
  ScopedHandle local_portal_;

  // A portal on the global node which is connected to `local_portal_`. Used to
  // transfer a pipe from the global node to the local one.
  ScopedHandle global_portal_;

  base::WeakPtrFactory<DirectReceiverBase> weak_ptr_factory_{this};
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
  friend class mojo::test::direct_receiver_unittest::ServiceImpl;
};

// DirectReceiver is a wrapper around the standard Receiver<T> type that always
// receives its messages directly from the sender without an IO-thread hop. To
// enable this safely DirectReceiver is constrained in a few ways:
//
//   - It cannot be unbound and moved once bound
//   - It's always bound on the current default task runner
//   - It must be bound on a thread which uses an IO MessagePump
//
// DirectReceiver works by creating and maintaining a separate ipcz node which
// is dedicated to the receiving endpoint and which receives I/O on its bound
// thread. This node is connected to the process's global ipcz node upon
// construction, and ipcz can then negotiate new direct connections between it
// and other nodes as needed.
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
class DirectReceiver : public internal::DirectReceiverBase {
 public:
  DirectReceiver(DirectReceiverKey, T* impl) : receiver_(impl) {}
  ~DirectReceiver() = default;

  void set_disconnect_handler(base::OnceClosure handler) {
    receiver_.set_disconnect_handler(std::move(handler));
  }

  // Binds this object to `receiver`.
  void Bind(PendingReceiver<T> receiver) {
    receiver_.Bind(MoveReceiverToLocalNode(std::move(receiver)));
  }

  Receiver<T>& receiver_for_testing() { return receiver_; }

 private:
  Receiver<T> receiver_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_DIRECT_RECEIVER_H_
