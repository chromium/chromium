// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/pending_remote_state.h"

namespace mojo {
namespace internal {

PendingRemoteState::PendingRemoteState() = default;

PendingRemoteState::PendingRemoteState(ScopedMessagePipeHandle pipe,
                                       uint32_t version)
    : pipe(std::move(pipe)), version(version) {}

PendingRemoteState::PendingRemoteState(PendingRemoteState&&) noexcept = default;

PendingRemoteState::~PendingRemoteState() = default;

PendingRemoteState& PendingRemoteState::operator=(
    PendingRemoteState&& other) noexcept {
  reset();
  pipe = std::move(other.pipe);
  version = other.version;
  other.version = 0;
  return *this;
}

void PendingRemoteState::reset() {
  pipe.reset();
  version = 0;
}

}  // namespace internal
}  // namespace mojo
