// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SHARED_ASSOCIATED_REMOTE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SHARED_ASSOCIATED_REMOTE_H_

#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace mojo {

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
      PendingAssociatedRemote<Interface> pending_remote)
      : remote_(pending_remote.is_valid()
                    ? SharedRemoteBase<AssociatedRemote<Interface>>::Create(
                          std::move(pending_remote))
                    : nullptr) {}
  SharedAssociatedRemote(
      PendingAssociatedRemote<Interface> pending_remote,
      scoped_refptr<base::SequencedTaskRunner> bind_task_runner)
      : remote_(pending_remote.is_valid()
                    ? SharedRemoteBase<AssociatedRemote<Interface>>::Create(
                          std::move(pending_remote),
                          std::move(bind_task_runner))
                    : nullptr) {}

  bool is_bound() const { return remote_ != nullptr; }
  explicit operator bool() const { return is_bound(); }

  Interface* get() const { return remote_->get(); }
  Interface* operator->() const { return get(); }
  Interface& operator*() const { return *get(); }

  // Clears this SharedAssociatedRemote. Note that this does *not* necessarily
  // close the remote's endpoint as other SharedAssociatedRemote instances may
  // reference the same underlying endpoint.
  void reset() { remote_.reset(); }

 private:
  scoped_refptr<SharedRemoteBase<AssociatedRemote<Interface>>> remote_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SHARED_ASSOCIATED_REMOTE_H_
