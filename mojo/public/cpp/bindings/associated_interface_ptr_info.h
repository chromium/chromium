// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_PTR_INFO_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_PTR_INFO_H_

#include <stdint.h>
#include <utility>

#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace mojo {

// DEPRECATED: Do not introduce new uses of this type. Instead use the
// AssociatedPendingRemote type defined in associated_pending_remote.h. Mojom
// files which pass associated interface endpoints
// (i.e. "associated Interface" syntax) should be updated to instead pass
// a "pending_associated_remote<Interface>".
//
// AssociatedInterfacePtrInfo stores necessary information to construct an
// associated interface pointer. It is similar to InterfacePtrInfo except that
// it doesn't own a message pipe handle.
template <typename Interface>
class AssociatedInterfacePtrInfo {
 public:
  AssociatedInterfacePtrInfo(AssociatedInterfacePtrInfo&& other)
      : handle_(std::move(other.handle_)),
        version_(std::exchange(other.version_, 0u)) {}

  AssociatedInterfacePtrInfo(ScopedInterfaceEndpointHandle handle,
                             uint32_t version)
      : handle_(std::move(handle)), version_(version) {}

  AssociatedInterfacePtrInfo(const AssociatedInterfacePtrInfo&) = delete;
  AssociatedInterfacePtrInfo& operator=(const AssociatedInterfacePtrInfo&) =
      delete;

  ~AssociatedInterfacePtrInfo() {}

  AssociatedInterfacePtrInfo& operator=(AssociatedInterfacePtrInfo&& other) {
    if (this != &other) {
      handle_ = std::move(other.handle_);
      version_ = std::exchange(other.version_, 0u);
    }

    return *this;
  }

  bool is_valid() const { return handle_.is_valid(); }

  ScopedInterfaceEndpointHandle PassHandle() { return std::move(handle_); }
  uint32_t version() const { return version_; }

 private:
  ScopedInterfaceEndpointHandle handle_;
  uint32_t version_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_PTR_INFO_H_
