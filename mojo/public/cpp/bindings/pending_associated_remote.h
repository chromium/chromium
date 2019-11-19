// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_PENDING_ASSOCIATED_REMOTE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_PENDING_ASSOCIATED_REMOTE_H_

#include <stdint.h>

#include <utility>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace mojo {

// PendingAssociatedRemote represents an unbound associated interface endpoint
// that will be used to send messages. An AssociatedRemote can consume this
// object to begin issuing method calls to a corresponding AssociatedReceiver.
template <typename Interface>
class PendingAssociatedRemote {
 public:
  PendingAssociatedRemote() = default;
  PendingAssociatedRemote(PendingAssociatedRemote&& other)
      : handle_(std::move(other.handle_)), version_(other.version_) {}
  PendingAssociatedRemote(ScopedInterfaceEndpointHandle handle,
                          uint32_t version)
      : handle_(std::move(handle)), version_(version) {}

  // Temporary helper for transitioning away from old types. Intentionally an
  // implicit constructor.
  PendingAssociatedRemote(AssociatedInterfacePtrInfo<Interface>&& ptr_info)
      : PendingAssociatedRemote(ptr_info.PassHandle(), ptr_info.version()) {}

  ~PendingAssociatedRemote() = default;

  PendingAssociatedRemote& operator=(PendingAssociatedRemote&& other) {
    handle_ = std::move(other.handle_);
    version_ = other.version_;
    return *this;
  }

  bool is_valid() const { return handle_.is_valid(); }
  explicit operator bool() const { return is_valid(); }

  void reset() { handle_.reset(); }

  // Temporary helper for transitioning away from old bindings types. This is
  // intentionally an implicit conversion.
  operator AssociatedInterfacePtrInfo<Interface>() {
    return AssociatedInterfacePtrInfo<Interface>(PassHandle(), version());
  }

  ScopedInterfaceEndpointHandle PassHandle() { return std::move(handle_); }
  const ScopedInterfaceEndpointHandle& handle() const { return handle_; }
  void set_handle(ScopedInterfaceEndpointHandle handle) {
    handle_ = std::move(handle);
  }

  uint32_t version() const { return version_; }
  void set_version(uint32_t version) { version_ = version; }

  PendingAssociatedReceiver<Interface> InitWithNewEndpointAndPassReceiver() {
    ScopedInterfaceEndpointHandle receiver_handle;
    ScopedInterfaceEndpointHandle::CreatePairPendingAssociation(
        &handle_, &receiver_handle);
    set_version(0);
    return PendingAssociatedReceiver<Interface>(std::move(receiver_handle));
  }

 private:
  ScopedInterfaceEndpointHandle handle_;
  uint32_t version_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PendingAssociatedRemote);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_PENDING_ASSOCIATED_REMOTE_H_
