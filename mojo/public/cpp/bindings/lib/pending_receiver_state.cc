// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/pending_receiver_state.h"

namespace mojo {
namespace internal {

PendingReceiverState::PendingReceiverState() = default;

PendingReceiverState::PendingReceiverState(ScopedMessagePipeHandle pipe)
    : pipe(std::move(pipe)) {}

PendingReceiverState::PendingReceiverState(PendingReceiverState&&) noexcept =
    default;

PendingReceiverState::~PendingReceiverState() = default;

PendingReceiverState& PendingReceiverState::operator=(
    PendingReceiverState&&) noexcept = default;

void PendingReceiverState::reset() {
  pipe.reset();
  connection_group.reset();
}

}  // namespace internal
}  // namespace mojo
