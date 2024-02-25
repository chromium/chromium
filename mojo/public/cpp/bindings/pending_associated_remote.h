// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_PENDING_ASSOCIATED_REMOTE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_PENDING_ASSOCIATED_REMOTE_H_

#include <stdint.h>

#include <type_traits>
#include <utility>

#include "base/compiler_specific.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"
#include "mojo/public/cpp/bindings/runtime_features.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

template <typename T>
class PendingAssociatedReceiver;

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

  // Disabled on NaCl since it crashes old version of clang.
#if !BUILDFLAG(IS_NACL)
  // Move conversion operator for custom remote types. Only participates in
  // overload resolution if a typesafe conversion is supported.
  template <typename T,
            std::enable_if_t<std::is_same<
                PendingAssociatedRemote<Interface>,
                std::invoke_result_t<decltype(&PendingAssociatedRemoteConverter<
                                              T>::template To<Interface>),
                                     T&&>>::value>* = nullptr>
  PendingAssociatedRemote(T&& other)
      : PendingAssociatedRemote(
            PendingAssociatedRemoteConverter<T>::template To<Interface>(
                std::move(other))) {}
#endif  // !BUILDFLAG(IS_NACL)

  PendingAssociatedRemote(const PendingAssociatedRemote&) = delete;
  PendingAssociatedRemote& operator=(const PendingAssociatedRemote&) = delete;

  ~PendingAssociatedRemote() = default;

  PendingAssociatedRemote& operator=(PendingAssociatedRemote&& other) {
    handle_ = std::move(other.handle_);
    version_ = other.version_;
    return *this;
  }

  bool is_valid() const { return handle_.is_valid(); }
  explicit operator bool() const { return is_valid(); }

  void reset() { handle_.reset(); }

  ScopedInterfaceEndpointHandle PassHandle() { return std::move(handle_); }
  const ScopedInterfaceEndpointHandle& handle() const { return handle_; }
  void set_handle(ScopedInterfaceEndpointHandle handle) {
    handle_ = std::move(handle);
  }

  uint32_t version() const { return version_; }
  void set_version(uint32_t version) { version_ = version; }

  [[nodiscard]] REINITIALIZES_AFTER_MOVE PendingAssociatedReceiver<Interface>
  InitWithNewEndpointAndPassReceiver();

  // Associates this endpoint with a dedicated message pipe. This allows the
  // entangled AssociatedReceiver/AssociatedRemote endpoints to be used
  // without ever being associated with any other mojom interfaces.
  //
  // Needless to say, messages sent between the two entangled endpoints will
  // not be ordered with respect to any other mojom interfaces. This is
  // generally useful for ignoring calls on an associated remote or for
  // binding associated endpoints in tests.
  void EnableUnassociatedUsage() {
    DCHECK(is_valid());

    MessagePipe pipe;
    scoped_refptr<internal::MultiplexRouter> router0 =
        internal::MultiplexRouter::CreateAndStartReceiving(
            std::move(pipe.handle0), internal::MultiplexRouter::MULTI_INTERFACE,
            false, base::SequencedTaskRunner::GetCurrentDefault());
    scoped_refptr<internal::MultiplexRouter> router1 =
        internal::MultiplexRouter::CreateAndStartReceiving(
            std::move(pipe.handle1), internal::MultiplexRouter::MULTI_INTERFACE,
            true, base::SequencedTaskRunner::GetCurrentDefault());

    InterfaceId id = router1->AssociateInterface(PassHandle());
    set_handle(router0->CreateLocalEndpointHandle(id));
  }

 private:
  ScopedInterfaceEndpointHandle handle_;
  uint32_t version_ = 0;
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

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace mojo {

template <typename Interface>
PendingAssociatedReceiver<Interface>
PendingAssociatedRemote<Interface>::InitWithNewEndpointAndPassReceiver() {
  if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
    return PendingAssociatedReceiver<Interface>();
  }
  ScopedInterfaceEndpointHandle receiver_handle;
  ScopedInterfaceEndpointHandle::CreatePairPendingAssociation(&handle_,
                                                              &receiver_handle);
  set_version(0);
  return PendingAssociatedReceiver<Interface>(std::move(receiver_handle));
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_PENDING_ASSOCIATED_REMOTE_H_
