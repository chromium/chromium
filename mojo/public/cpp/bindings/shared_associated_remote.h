// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SHARED_ASSOCIATED_REMOTE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SHARED_ASSOCIATED_REMOTE_H_

#include <tuple>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/runtime_features.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace mojo {

namespace internal {

template <typename Interface>
struct SharedRemoteTraits<AssociatedRemote<Interface>> {
  static void BindDisconnected(AssociatedRemote<Interface>& remote) {
    std::ignore = remote.BindNewEndpointAndPassDedicatedReceiver();
  }
};

}  // namespace internal

// SharedAssociatedRemote wraps a non-thread-safe AssociatedRemote and proxies
// messages to it. Unlike normal AssociatedRemote objects,
// SharedAssociatedRemote is copyable and usable from any thread, but has some
// additional overhead and latency in message transmission as a trade-off.
//
// Async calls are posted to the sequence that the underlying AssociatedRemote
// is bound to, and responses are posted back to the calling sequence. Sync
// calls are dispatched directly if the call is made on the sequence that the
// wrapped AssociatedRemote is bound to, or posted otherwise. It's important to
// be aware that sync calls block both the calling sequence and the bound
// AssociatedRemote's sequence. That means that you cannot make sync calls
// through a SharedAssociatedRemote if the underlying AssociatedRemote is bound
// to a sequence that cannot block, like the IPC thread.
template <typename Interface>
class SharedAssociatedRemote {
 public:
  SharedAssociatedRemote() = default;
  explicit SharedAssociatedRemote(
      PendingAssociatedRemote<Interface> pending_remote,
      scoped_refptr<base::SequencedTaskRunner> bind_task_runner =
          base::SequencedTaskRunner::GetCurrentDefault()) {
    if (pending_remote.is_valid())
      Bind(std::move(pending_remote), std::move(bind_task_runner));
  }

  bool is_bound() const { return remote_ != nullptr; }
  explicit operator bool() const { return is_bound(); }

  Interface* get() const { return remote_->get(); }
  Interface* operator->() const { return get(); }
  Interface& operator*() const { return *get(); }

  void set_disconnect_handler(
      base::OnceClosure handler,
      scoped_refptr<base::SequencedTaskRunner> handler_task_runner) {
    remote_->set_disconnect_handler(std::move(handler),
                                    std::move(handler_task_runner));
  }

  // Clears this SharedAssociatedRemote. Note that this does *not* necessarily
  // close the remote's endpoint as other SharedAssociatedRemote instances may
  // reference the same underlying endpoint.
  void reset() { remote_.reset(); }

  // Disconnects the SharedAssociatedRemote. This leaves the object in a usable
  // state -- i.e. it's still safe to dereference and make calls -- but severs
  // the underlying connection so that no new replies will be received and all
  // outgoing messages will be discarded. This is useful when you want to force
  // a disconnection like with reset(), but you don't want the
  // SharedAssociatedRemote to become unbound.
  void Disconnect() { remote_->Disconnect(); }

  // Creates a new pair of endpoints and binds this SharedAssociatedRemote to
  // one of them, on `task_runner`. The other is returned as a receiver.
  mojo::PendingAssociatedReceiver<Interface> BindNewEndpointAndPassReceiver(
      scoped_refptr<base::SequencedTaskRunner> bind_task_runner =
          base::SequencedTaskRunner::GetCurrentDefault()) {
    if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
      return PendingAssociatedReceiver<Interface>();
    }
    mojo::PendingAssociatedRemote<Interface> remote;
    auto receiver = remote.InitWithNewEndpointAndPassReceiver();
    Bind(std::move(remote), std::move(bind_task_runner));
    return receiver;
  }

  // Binds to `pending_remote` on `bind_task_runner`.
  void Bind(PendingAssociatedRemote<Interface> pending_remote,
            scoped_refptr<base::SequencedTaskRunner> bind_task_runner =
                base::SequencedTaskRunner::GetCurrentDefault()) {
    DCHECK(!remote_);
    DCHECK(pending_remote.is_valid());
    if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
      remote_.reset();
      return;
    }
    remote_ = SharedRemoteBase<AssociatedRemote<Interface>>::Create(
        std::move(pending_remote), std::move(bind_task_runner));
  }

 private:
  scoped_refptr<SharedRemoteBase<AssociatedRemote<Interface>>> remote_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SHARED_ASSOCIATED_REMOTE_H_
