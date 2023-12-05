// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/generic_pending_associated_receiver.h"

#include <string_view>
#include <utility>

#include "base/check.h"

namespace mojo {

GenericPendingAssociatedReceiver::GenericPendingAssociatedReceiver() = default;

GenericPendingAssociatedReceiver::GenericPendingAssociatedReceiver(
    std::string_view interface_name,
    mojo::ScopedInterfaceEndpointHandle handle)
    : interface_name_(std::string(interface_name)),
      handle_(std::move(handle)) {}

GenericPendingAssociatedReceiver::GenericPendingAssociatedReceiver(
    GenericPendingAssociatedReceiver&&) = default;

GenericPendingAssociatedReceiver& GenericPendingAssociatedReceiver::operator=(
    GenericPendingAssociatedReceiver&&) = default;

GenericPendingAssociatedReceiver::~GenericPendingAssociatedReceiver() = default;

void GenericPendingAssociatedReceiver::reset() {
  interface_name_.reset();
  handle_.reset();
}

mojo::ScopedInterfaceEndpointHandle
GenericPendingAssociatedReceiver::PassHandle() {
  DCHECK(is_valid());
  interface_name_.reset();
  return std::move(handle_);
}

mojo::ScopedInterfaceEndpointHandle
GenericPendingAssociatedReceiver::PassHandleIfNameIs(
    const char* interface_name) {
  DCHECK(is_valid());
  if (interface_name_ == interface_name)
    return PassHandle();
  return mojo::ScopedInterfaceEndpointHandle();
}

}  // namespace mojo
