// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_PENDING_ASSOCIATED_REMOTE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_PENDING_ASSOCIATED_REMOTE_H_

#include <stdint.h>

#include <utility>

#include "base/macros.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

template <typename T>
struct PendingAssociatedRemoteConverter;

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

  // Disabled on NaCl since it crashes old version of clang.
#if !defined(OS_NACL)
  // Move conversion operator for custom remote types. Only participates in
  // overload resolution if a typesafe conversion is supported.
  template <typename T,
            std::enable_if_t<std::is_same<
                PendingAssociatedRemote<Interface>,
                std::result_of_t<decltype (&PendingAssociatedRemoteConverter<
                                           T>::template To<Interface>)(T&&)>>::
                                 value>* = nullptr>
  PendingAssociatedRemote(T&& other)
      : PendingAssociatedRemote(
            PendingAssociatedRemoteConverter<T>::template To<Interface>(
                std::move(other))) {}
#endif  // !defined(OS_NACL)

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
  uint32_t version_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PendingAssociatedRemote);
};

// Constructs an invalid PendingAssociatedRemote of any arbitrary interface
// type. Useful as short-hand for a default constructed value.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) NullAssociatedRemote {
 public:
  template <typename Interface>
  operator PendingAssociatedRemote<Interface>() const {
    return PendingAssociatedRemote<Interface>();
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_PENDING_ASSOCIATED_REMOTE_H_
