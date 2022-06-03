// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_PTR_INFO_H_
#define MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_PTR_INFO_H_

#include <cstddef>
#include <cstdint>
#include <utility>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/lib/pending_remote_state.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

// DEPRECATED: Do not introduce new uses of this type. Instead use the
// PendingRemote type defined in pending_remote.h. Mojom files which pass
// interface endpoints (i.e. raw "Interface" types) should be updated to instead
// pass a "pending_remote<Interface>".
//
// InterfacePtrInfo stores necessary information to communicate with a remote
// interface implementation, which could be used to construct an InterfacePtr.
template <typename Interface>
class InterfacePtrInfo {
 public:
  InterfacePtrInfo() = default;
  InterfacePtrInfo(std::nullptr_t) : InterfacePtrInfo() {}

  InterfacePtrInfo(ScopedMessagePipeHandle handle, uint32_t version)
      : state_(std::move(handle), version) {}

  InterfacePtrInfo(InterfacePtrInfo&& other) = default;

  InterfacePtrInfo(const InterfacePtrInfo&) = delete;
  InterfacePtrInfo& operator=(const InterfacePtrInfo&) = delete;

  ~InterfacePtrInfo() {}

  InterfacePtrInfo& operator=(InterfacePtrInfo&& other) = default;

  bool is_valid() const { return state_.pipe.is_valid(); }

  ScopedMessagePipeHandle PassHandle() { return std::move(state_.pipe); }
  const ScopedMessagePipeHandle& handle() const { return state_.pipe; }
  void set_handle(ScopedMessagePipeHandle handle) {
    state_.pipe = std::move(handle);
  }

  uint32_t version() const { return state_.version; }
  void set_version(uint32_t version) { state_.version = version; }

  // Allow InterfacePtrInfo<> to be used in boolean expressions.
  explicit operator bool() const { return state_.pipe.is_valid(); }

  internal::PendingRemoteState* internal_state() { return &state_; }

 private:
  internal::PendingRemoteState state_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_PTR_INFO_H_
