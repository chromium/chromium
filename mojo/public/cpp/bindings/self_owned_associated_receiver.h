// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SELF_OWNED_ASSOCIATED_RECEIVER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SELF_OWNED_ASSOCIATED_RECEIVER_H_

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"

namespace mojo {

template <typename Interface>
using SelfOwnedAssociatedReceiverRef = StrongAssociatedBindingPtr<Interface>;

template <typename Interface, typename Impl>
SelfOwnedAssociatedReceiverRef<Interface> MakeSelfOwnedAssociatedReceiver(
    std::unique_ptr<Impl> impl,
    PendingAssociatedReceiver<Interface> receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
  return StrongAssociatedBinding<Interface>::Create(
      std::move(impl), std::move(receiver), std::move(task_runner));
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SELF_OWNED_ASSOCIATED_RECEIVER_H_
