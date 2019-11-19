// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/generic_pending_receiver.h"

namespace mojo {

GenericPendingReceiver::GenericPendingReceiver() = default;

GenericPendingReceiver::GenericPendingReceiver(
    base::StringPiece interface_name,
    mojo::ScopedMessagePipeHandle receiving_pipe)
    : interface_name_(interface_name.as_string()),
      pipe_(std::move(receiving_pipe)) {}

GenericPendingReceiver::GenericPendingReceiver(GenericPendingReceiver&&) =
    default;

GenericPendingReceiver::~GenericPendingReceiver() = default;

GenericPendingReceiver& GenericPendingReceiver::operator=(
    GenericPendingReceiver&&) = default;

void GenericPendingReceiver::reset() {
  interface_name_.reset();
  pipe_.reset();
}

mojo::ScopedMessagePipeHandle GenericPendingReceiver::PassPipe() {
  DCHECK(is_valid());
  interface_name_.reset();
  return std::move(pipe_);
}

mojo::ScopedMessagePipeHandle GenericPendingReceiver::PassPipeIfNameIs(
    const char* interface_name) {
  DCHECK(is_valid());
  if (interface_name_ == interface_name)
    return PassPipe();
  return mojo::ScopedMessagePipeHandle();
}

}  // namespace mojo
