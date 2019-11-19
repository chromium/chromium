// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_PENDING_ASSOCIATED_RECEIVER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_PENDING_ASSOCIATED_RECEIVER_H_

#include <stdint.h>

#include <utility>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_interface_request.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace mojo {

// PendingAssociatedReceiver represents an unbound associated interface
// endpoint that will receive and queue messages. An AssociatedReceiver can
// consume this object to begin receiving method calls from a corresponding
// AssociatedRemote.
template <typename Interface>
class PendingAssociatedReceiver {
 public:
  PendingAssociatedReceiver() = default;
  PendingAssociatedReceiver(PendingAssociatedReceiver&& other)
      : handle_(std::move(other.handle_)) {}
  explicit PendingAssociatedReceiver(ScopedInterfaceEndpointHandle handle)
      : handle_(std::move(handle)) {}

  // Temporary implicit move constructor to aid in converting from use of
  // InterfaceRequest<Interface> to PendingReceiver.
  PendingAssociatedReceiver(AssociatedInterfaceRequest<Interface>&& request)
      : PendingAssociatedReceiver(request.PassHandle()) {}

  ~PendingAssociatedReceiver() = default;

  PendingAssociatedReceiver& operator=(PendingAssociatedReceiver&& other) {
    handle_ = std::move(other.handle_);
    return *this;
  }

  bool is_valid() const { return handle_.is_valid(); }
  explicit operator bool() const { return is_valid(); }

  // Temporary implicit conversion operator to
  // AssociatedInterfaceRequest<Interface> to aid in converting usage to
  // PendingAssociatedReceiver.
  operator AssociatedInterfaceRequest<Interface>() {
    return AssociatedInterfaceRequest<Interface>(PassHandle());
  }

  ScopedInterfaceEndpointHandle PassHandle() { return std::move(handle_); }
  const ScopedInterfaceEndpointHandle& handle() const { return handle_; }
  void set_handle(ScopedInterfaceEndpointHandle handle) {
    handle_ = std::move(handle);
  }

  // Hangs up this endpoint, invalidating the PendingAssociatedReceiver.
  void reset() { handle_.reset(); }

  // Similar to above but provides additional metadata in case the remote
  // endpoint wants details about why this endpoint hung up.
  void ResetWithReason(uint32_t custom_reason, const std::string& description) {
    handle_.ResetWithReason(custom_reason, description);
  }

 private:
  ScopedInterfaceEndpointHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(PendingAssociatedReceiver);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_PENDING_ASSOCIATED_RECEIVER_H_
