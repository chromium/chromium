// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_PENDING_RECEIVER_STATE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_PENDING_RECEIVER_STATE_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/connection_group.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {
namespace internal {

// Generic state owned by every templated InterfaceRequest or PendingReceiver
// instance.
struct COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) PendingReceiverState {
 public:
  PendingReceiverState();
  explicit PendingReceiverState(ScopedMessagePipeHandle pipe);
  PendingReceiverState(const PendingReceiverState&) = delete;
  PendingReceiverState(PendingReceiverState&&) noexcept;
  ~PendingReceiverState();

  PendingReceiverState& operator=(const PendingReceiverState&) = delete;
  PendingReceiverState& operator=(PendingReceiverState&&) noexcept;

  void reset();

  ScopedMessagePipeHandle pipe;
  ConnectionGroup::Ref connection_group;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_PENDING_RECEIVER_STATE_H_
