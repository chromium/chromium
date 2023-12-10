// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_GENERIC_PENDING_ASSOCIATED_RECEIVER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_GENERIC_PENDING_ASSOCIATED_RECEIVER_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/runtime_features.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace mojo {

// GenericPendingAssociatedReceiver encapsulates a pairing of a receiving
// associated interface endpoint with the name of the mojom interface assumed by
// the corresponding remote endpoint.
//
// This is used by mojom C++ bindings to represent
// |mojo_base.mojom.GenericAssociatedPendingReceiver|, and it serves as a
// semi-safe wrapper for transporting arbitrary associated interface receivers
// in a generic object.
//
// It is intended to be used in the (relatively rare) scenario where an
// interface needs to support sharing its message ordering with interfaces
// defined at higher application layers, such that knowledge of those associated
// interface(s) would constitute a layering violation.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) GenericPendingAssociatedReceiver {
 public:
  GenericPendingAssociatedReceiver();
  GenericPendingAssociatedReceiver(std::string_view interface_name,
                                   mojo::ScopedInterfaceEndpointHandle handle);

  template <typename Interface>
  GenericPendingAssociatedReceiver(
      mojo::PendingAssociatedReceiver<Interface> receiver)
      : GenericPendingAssociatedReceiver(Interface::Name_,
                                         receiver.PassHandle()) {}

  GenericPendingAssociatedReceiver(const GenericPendingAssociatedReceiver&) =
      delete;
  GenericPendingAssociatedReceiver(GenericPendingAssociatedReceiver&&);
  GenericPendingAssociatedReceiver& operator=(
      const GenericPendingAssociatedReceiver&) = delete;
  GenericPendingAssociatedReceiver& operator=(
      GenericPendingAssociatedReceiver&&);
  ~GenericPendingAssociatedReceiver();

  bool is_valid() const { return handle_.is_valid(); }
  explicit operator bool() const { return is_valid(); }

  void reset();

  const std::optional<std::string>& interface_name() const {
    return interface_name_;
  }

  // Takes ownership of the endpoint, invalidating this object.
  mojo::ScopedInterfaceEndpointHandle PassHandle();

  // Takes ownership of the endpoint, strongly typed as an `Interface` receiver,
  // if and only if that interface's name matches the stored interface name.
  template <typename Interface>
  mojo::PendingAssociatedReceiver<Interface> As() {
    if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
      return mojo::PendingAssociatedReceiver<Interface>();
    }
    return mojo::PendingAssociatedReceiver<Interface>(
        PassHandleIfNameIs(Interface::Name_));
  }

 private:
  mojo::ScopedInterfaceEndpointHandle PassHandleIfNameIs(
      const char* interface_name);

  std::optional<std::string> interface_name_;
  mojo::ScopedInterfaceEndpointHandle handle_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_GENERIC_PENDING_ASSOCIATED_RECEIVER_H_
