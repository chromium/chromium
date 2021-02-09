// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_PENDING_ASSOCIATED_RECEIVER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_PENDING_ASSOCIATED_RECEIVER_H_

#include <stdint.h>

#include <utility>

#include "base/macros.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_interface_request.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

template <typename T>
struct PendingAssociatedReceiverConverter;

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

  // Disabled on NaCl since it crashes old version of clang.
#if !defined(OS_NACL)
  // Move conversion operator for custom receiver types. Only participates in
  // overload resolution if a typesafe conversion is supported.
  template <typename T,
            std::enable_if_t<std::is_same<
                PendingAssociatedReceiver<Interface>,
                std::result_of_t<decltype (&PendingAssociatedReceiverConverter<
                                           T>::template To<Interface>)(T&&)>>::
                                 value>* = nullptr>
  PendingAssociatedReceiver(T&& other)
      : PendingAssociatedReceiver(
            PendingAssociatedReceiverConverter<T>::template To<Interface>(
                std::move(other))) {}
#endif  // !defined(OS_NACL)

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

  // Associates this endpoint with a dedicated message pipe. This allows the
  // entangled AssociatedReceiver/AssociatedRemote endpoints to be used without
  // ever being associated with any other mojom interfaces.
  //
  // Needless to say, messages sent between the two entangled endpoints will not
  // be ordered with respect to any other mojom interfaces. This is generally
  // useful for ignoring calls on an associated remote or for binding associated
  // endpoints in tests.
  void EnableUnassociatedUsage() {
    DCHECK(is_valid());

    MessagePipe pipe;
    scoped_refptr<internal::MultiplexRouter> router0 =
        new internal::MultiplexRouter(
            std::move(pipe.handle0), internal::MultiplexRouter::MULTI_INTERFACE,
            false, base::SequencedTaskRunnerHandle::Get());
    scoped_refptr<internal::MultiplexRouter> router1 =
        new internal::MultiplexRouter(
            std::move(pipe.handle1), internal::MultiplexRouter::MULTI_INTERFACE,
            true, base::SequencedTaskRunnerHandle::Get());

    InterfaceId id = router1->AssociateInterface(PassHandle());
    set_handle(router0->CreateLocalEndpointHandle(id));
  }

 private:
  ScopedInterfaceEndpointHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(PendingAssociatedReceiver);
};

// Constructs an invalid PendingAssociatedReceiver of any arbitrary interface
// type. Useful as short-hand for a default constructed value.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) NullAssociatedReceiver {
 public:
  template <typename Interface>
  operator PendingAssociatedReceiver<Interface>() const {
    return PendingAssociatedReceiver<Interface>();
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_PENDING_ASSOCIATED_RECEIVER_H_
