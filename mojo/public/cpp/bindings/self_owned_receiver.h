// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SELF_OWNED_RECEIVER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SELF_OWNED_RECEIVER_H_

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace mojo {

template <typename Interface>
using SelfOwnedReceiverRef = StrongBindingPtr<Interface>;

template <typename Interface, typename Impl>
SelfOwnedReceiverRef<Interface> MakeSelfOwnedReceiver(
    std::unique_ptr<Impl> impl,
    PendingReceiver<Interface> receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
  return StrongBinding<Interface>::Create(std::move(impl), std::move(receiver),
                                          std::move(task_runner));
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SELF_OWNED_RECEIVER_H_
