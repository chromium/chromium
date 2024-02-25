// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/generic_pending_receiver.h"

#include <string_view>

#include "base/trace_event/trace_event.h"

namespace mojo {

GenericPendingReceiver::GenericPendingReceiver() = default;

GenericPendingReceiver::GenericPendingReceiver(
    std::string_view interface_name,
    mojo::ScopedMessagePipeHandle receiving_pipe)
    : interface_name_(std::string(interface_name)),
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

void GenericPendingReceiver::WriteIntoTrace(perfetto::TracedValue ctx) const {
  auto dict = std::move(ctx).WriteDictionary();
  dict.Add("interface_name", interface_name_);
}

}  // namespace mojo
