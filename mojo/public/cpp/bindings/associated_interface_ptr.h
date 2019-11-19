// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_PTR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_PTR_H_

#include <stdint.h>

#include <cstddef>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/associated_interface_request.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/lib/associated_interface_ptr_state.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

// Represents the client side of an associated interface. It is similar to
// InterfacePtr, except that it doesn't own a message pipe handle.
template <typename Interface>
class AssociatedInterfacePtr {
 public:
  using InterfaceType = Interface;
  using PtrInfoType = AssociatedInterfacePtrInfo<Interface>;
  using Proxy = typename Interface::Proxy_;

  // Constructs an unbound AssociatedInterfacePtr.
  AssociatedInterfacePtr() {}
  AssociatedInterfacePtr(std::nullptr_t) {}

  AssociatedInterfacePtr(AssociatedInterfacePtr&& other) {
    internal_state_.Swap(&other.internal_state_);
  }

  explicit AssociatedInterfacePtr(PtrInfoType&& info) { Bind(std::move(info)); }

  AssociatedInterfacePtr& operator=(AssociatedInterfacePtr&& other) {
    reset();
    internal_state_.Swap(&other.internal_state_);
    return *this;
  }

  // Assigning nullptr to this class causes it to closes the associated
  // interface (if any) and returns the pointer to the unbound state.
  AssociatedInterfacePtr& operator=(std::nullptr_t) {
    reset();
    return *this;
  }

  ~AssociatedInterfacePtr() {}

  // Sets up this object as the client side of an associated interface.
  // Calling with an invalid |info| has the same effect as reset(). In this
  // case, the AssociatedInterfacePtr is not considered as bound.
  //
  // Optionally, |runner| is a SequencedTaskRunner bound to the current sequence
  // on which all callbacks and connection error notifications will be
  // dispatched. It is only useful to specify this to use a different
  // SequencedTaskRunner than SequencedTaskRunnerHandle::Get().
  //
  // NOTE: The corresponding AssociatedInterfaceRequest must be sent over
  // another interface before using this object to make calls. Please see the
  // comments of MakeRequest(AssociatedInterfacePtr<Interface>*) for more
  // details.
  void Bind(AssociatedInterfacePtrInfo<Interface> info,
            scoped_refptr<base::SequencedTaskRunner> runner = nullptr) {
    reset();

    if (info.is_valid())
      internal_state_.Bind(std::move(info), std::move(runner));
  }

  bool is_bound() const { return internal_state_.is_bound(); }

  Proxy* get() const { return internal_state_.instance(); }

  // Functions like a pointer to Interface. Must already be bound.
  Proxy* operator->() const { return get(); }
  Proxy& operator*() const { return *get(); }

  // Returns the version number of the interface that the remote side supports.
  uint32_t version() const { return internal_state_.version(); }

  // Queries the max version that the remote side supports. On completion, the
  // result will be returned as the input of |callback|. The version number of
  // this object will also be updated.
  void QueryVersion(base::OnceCallback<void(uint32_t)> callback) {
    internal_state_.QueryVersion(std::move(callback));
  }

  // If the remote side doesn't support the specified version, it will close the
  // associated interface asynchronously. This does nothing if it's already
  // known that the remote side supports the specified version, i.e., if
  // |version <= this->version()|.
  //
  // After calling RequireVersion() with a version not supported by the remote
  // side, all subsequent calls to interface methods will be ignored.
  void RequireVersion(uint32_t version) {
    internal_state_.RequireVersion(version);
  }

  // Sends a message on the underlying message pipe and runs the current
  // message loop until its response is received. This can be used in tests to
  // verify that no message was sent on a message pipe in response to some
  // stimulus.
  void FlushForTesting() { internal_state_.FlushForTesting(); }

  // Closes the associated interface (if any) and returns the pointer to the
  // unbound state.
  void reset() {
    State doomed;
    internal_state_.Swap(&doomed);
  }

  // Similar to the method above, but also specifies a disconnect reason.
  void ResetWithReason(uint32_t custom_reason, const std::string& description) {
    if (internal_state_.is_bound())
      internal_state_.CloseWithReason(custom_reason, description);
    reset();
  }

  // Indicates whether an error has been encountered. If true, method calls made
  // on this interface will be dropped (and may already have been dropped).
  bool encountered_error() const { return internal_state_.encountered_error(); }

  // Registers a handler to receive error notifications.
  //
  // This method may only be called after the AssociatedInterfacePtr has been
  // bound.
  void set_connection_error_handler(base::OnceClosure error_handler) {
    internal_state_.set_connection_error_handler(std::move(error_handler));
  }

  void set_connection_error_with_reason_handler(
      ConnectionErrorWithReasonCallback error_handler) {
    internal_state_.set_connection_error_with_reason_handler(
        std::move(error_handler));
  }

  // Unbinds and returns the associated interface pointer information which
  // could be used to setup an AssociatedInterfacePtr again. This method may be
  // used to move the proxy to a different sequence.
  //
  // It is an error to call PassInterface() while there are pending responses.
  // TODO: fix this restriction, it's not always obvious when there is a
  // pending response.
  AssociatedInterfacePtrInfo<Interface> PassInterface() {
    DCHECK(!internal_state_.has_pending_callbacks());
    State state;
    internal_state_.Swap(&state);

    return state.PassInterface();
  }

  // DO NOT USE. Exposed only for internal use and for testing.
  internal::AssociatedInterfacePtrState<Interface>* internal_state() {
    return &internal_state_;
  }

  // Allow AssociatedInterfacePtr<> to be used in boolean expressions.
  explicit operator bool() const { return internal_state_.is_bound(); }

 private:
  typedef internal::AssociatedInterfacePtrState<Interface> State;
  mutable State internal_state_;

  DISALLOW_COPY_AND_ASSIGN(AssociatedInterfacePtr);
};

// Creates an associated interface. The returned request is supposed to be sent
// over another interface (either associated or non-associated).
//
// NOTE: |ptr| must NOT be used to make calls before the request is sent.
// Violating that will lead to crash. On the other hand, as soon as the request
// is sent, |ptr| is usable. There is no need to wait until the request is bound
// to an implementation at the remote side.
template <typename Interface>
AssociatedInterfaceRequest<Interface> MakeRequest(
    AssociatedInterfacePtr<Interface>* ptr,
    scoped_refptr<base::SequencedTaskRunner> runner = nullptr) {
  AssociatedInterfacePtrInfo<Interface> ptr_info;
  auto request = MakeRequest(&ptr_info);
  ptr->Bind(std::move(ptr_info), std::move(runner));
  return request;
}

// Creates an associated interface. One of the two endpoints is supposed to be
// sent over another interface (either associated or non-associated); while the
// other is used locally.
//
// NOTE: If |ptr_info| is used locally and bound to an AssociatedInterfacePtr,
// the interface pointer must NOT be used to make calls before the request is
// sent. Please see NOTE of the previous function for more details.
template <typename Interface>
AssociatedInterfaceRequest<Interface> MakeRequest(
    AssociatedInterfacePtrInfo<Interface>* ptr_info) {
  ScopedInterfaceEndpointHandle handle0;
  ScopedInterfaceEndpointHandle handle1;
  ScopedInterfaceEndpointHandle::CreatePairPendingAssociation(&handle0,
                                                              &handle1);

  ptr_info->set_handle(std::move(handle0));
  ptr_info->set_version(0);

  return AssociatedInterfaceRequest<Interface>(std::move(handle1));
}

// Like MakeRequest() above, but it creates a dedicated message pipe. The
// returned request can be bound directly to an implementation, without being
// first passed through a message pipe endpoint.
//
// This function has two main uses:
//
//  * In testing, where the returned request is bound to e.g. a mock and there
//    are no other interfaces involved.
//
//  * When discarding messages sent on an interface, which can be done by
//    discarding the returned request.
template <typename Interface>
AssociatedInterfaceRequest<Interface> MakeRequestAssociatedWithDedicatedPipe(
    AssociatedInterfacePtr<Interface>* ptr) {
  MessagePipe pipe;
  scoped_refptr<internal::MultiplexRouter> router0 =
      new internal::MultiplexRouter(
          std::move(pipe.handle0), internal::MultiplexRouter::MULTI_INTERFACE,
          false, base::SequencedTaskRunnerHandle::Get());
  scoped_refptr<internal::MultiplexRouter> router1 =
      new internal::MultiplexRouter(
          std::move(pipe.handle1), internal::MultiplexRouter::MULTI_INTERFACE,
          true, base::SequencedTaskRunnerHandle::Get());

  ScopedInterfaceEndpointHandle endpoint0, endpoint1;
  ScopedInterfaceEndpointHandle::CreatePairPendingAssociation(&endpoint0,
                                                              &endpoint1);
  InterfaceId id = router1->AssociateInterface(std::move(endpoint0));
  endpoint0 = router0->CreateLocalEndpointHandle(id);

  ptr->Bind(AssociatedInterfacePtrInfo<Interface>(std::move(endpoint0),
                                                  Interface::Version_));
  return AssociatedInterfaceRequest<Interface>(std::move(endpoint1));
}

// |handle| is supposed to be the request of an associated interface. This
// method associates the interface with a dedicated, disconnected message pipe.
// That way, the corresponding associated interface pointer of |handle| can
// safely make calls (although those calls are silently dropped).
COMPONENT_EXPORT(MOJO_CPP_BINDINGS)
void AssociateWithDisconnectedPipe(ScopedInterfaceEndpointHandle handle);

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_INTERFACE_PTR_H_
