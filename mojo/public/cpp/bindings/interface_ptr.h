// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_PTR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_PTR_H_

#include <stdint.h>

#include <cstddef>
#include <string>
#include <utility>

#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"
#include "mojo/public/cpp/bindings/lib/interface_ptr_state.h"

namespace mojo {

// A pointer to a local proxy of a remote Interface implementation. Uses a
// message pipe to communicate with the remote implementation, and automatically
// closes the pipe and deletes the proxy on destruction. The pointer must be
// bound to a message pipe before the interface methods can be called. Once a
// pointer is destroyed, it is guaranteed that pending callbacks as well as the
// connection error handler (if registered) won't be called.
//
// This class is thread hostile, as is the local proxy it manages, while bound
// to a message pipe. All calls to this class or the proxy should be from the
// same sequence that bound it. If you need to move the proxy to a different
// sequence, extract the InterfacePtrInfo (containing just the message pipe and
// any version information) using PassInterface() on the original sequence, pass
// it to a different sequence, and create and bind a new InterfacePtr from that
// sequence. If an InterfacePtr is not bound to a message pipe, it may be bound
// or destroyed on any sequence.
template <typename Interface>
class InterfacePtr {
 public:
  using InterfaceType = Interface;
  using PtrInfoType = InterfacePtrInfo<Interface>;
  using Proxy = typename Interface::Proxy_;

  // Constructs an unbound InterfacePtr.
  InterfacePtr() {}
  InterfacePtr(std::nullptr_t) {}

  // Takes over the binding of another InterfacePtr.
  InterfacePtr(InterfacePtr&& other) noexcept {
    internal_state_.Swap(&other.internal_state_);
  }

  explicit InterfacePtr(
      PtrInfoType&& info,
      scoped_refptr<base::SequencedTaskRunner> runner = nullptr) noexcept {
    Bind(std::move(info), std::move(runner));
  }

  // Takes over the binding of another InterfacePtr, and closes any message pipe
  // already bound to this pointer.
  InterfacePtr& operator=(InterfacePtr&& other) noexcept {
    reset();
    internal_state_.Swap(&other.internal_state_);
    return *this;
  }

  // Assigning nullptr to this class causes it to close the currently bound
  // message pipe (if any) and returns the pointer to the unbound state.
  InterfacePtr& operator=(std::nullptr_t) {
    reset();
    return *this;
  }

  // Closes the bound message pipe (if any) on destruction.
  ~InterfacePtr() {}

  // Binds the InterfacePtr to a remote implementation of Interface.
  //
  // Calling with an invalid |info| (containing an invalid message pipe handle)
  // has the same effect as reset(). In this case, the InterfacePtr is not
  // considered as bound.
  //
  // Optionally, |runner| is a SequencedTaskRunner bound to the current sequence
  // on which all callbacks and connection error notifications will be
  // dispatched. It is only useful to specify this to use a different
  // SequencedTaskRunner than SequencedTaskRunnerHandle::Get().
  void Bind(InterfacePtrInfo<Interface> info,
            scoped_refptr<base::SequencedTaskRunner> runner = nullptr) {
    reset();
    if (info.is_valid())
      internal_state_.Bind(info.internal_state(), std::move(runner));
  }

  // Returns whether or not this InterfacePtr is bound to a message pipe.
  bool is_bound() const { return internal_state_.is_bound(); }

  // Returns a raw pointer to the local proxy. Caller does not take ownership.
  // Note that the local proxy is thread hostile, as stated above.
  Proxy* get() const { return internal_state_.instance(); }

  // Functions like a pointer to Interface. Must already be bound.
  Proxy* operator->() const { return get(); }
  Proxy& operator*() const { return *get(); }

  // Returns the version number of the interface that the remote side supports.
  uint32_t version() const { return internal_state_.version(); }

  // Queries the max version that the remote side supports. On completion, the
  // result will be returned as the input of |callback|. The version number of
  // this interface pointer will also be updated.
  void QueryVersion(base::OnceCallback<void(uint32_t)> callback) {
    internal_state_.QueryVersion(std::move(callback));
  }

  // If the remote side doesn't support the specified version, it will close its
  // end of the message pipe asynchronously. This does nothing if it's already
  // known that the remote side supports the specified version, i.e., if
  // |version <= this->version()|.
  //
  // After calling RequireVersion() with a version not supported by the remote
  // side, all subsequent calls to interface methods will be ignored.
  void RequireVersion(uint32_t version) {
    internal_state_.RequireVersion(version);
  }

  // Sends a no-op message on the underlying message pipe and runs the current
  // message loop until its response is received. This can be used in tests to
  // verify that no message was sent on a message pipe in response to some
  // stimulus.
  void FlushForTesting() { internal_state_.FlushForTesting(); }

  // Same as |FlushForTesting()| but will call |callback| when the flush is
  // complete.
  void FlushAsyncForTesting(base::OnceClosure callback) {
    internal_state_.FlushAsyncForTesting(std::move(callback));
  }

  // Closes the bound message pipe, if any.
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

  // Whether there are any associated interfaces running on the pipe currently.
  bool HasAssociatedInterfaces() const {
    return internal_state_.HasAssociatedInterfaces();
  }

  // Returns true if bound and awaiting a response to a message.
  bool IsExpectingResponse() { return internal_state_.has_pending_callbacks(); }

  // Indicates whether the message pipe has encountered an error. If true,
  // method calls made on this interface will be dropped (and may already have
  // been dropped).
  bool encountered_error() const { return internal_state_.encountered_error(); }

  // Registers a handler to receive error notifications. The handler will be
  // called from the sequence that owns this InterfacePtr.
  //
  // This method may only be called after the InterfacePtr has been bound to a
  // message pipe.
  void set_connection_error_handler(base::OnceClosure error_handler) {
    internal_state_.set_connection_error_handler(std::move(error_handler));
  }

  void set_connection_error_with_reason_handler(
      ConnectionErrorWithReasonCallback error_handler) {
    internal_state_.set_connection_error_with_reason_handler(
        std::move(error_handler));
  }

  // Unbinds the InterfacePtr and returns the information which could be used
  // to setup an InterfacePtr again. This method may be used to move the proxy
  // to a different sequence (see class comments for details).
  //
  // It is an error to call PassInterface() while:
  //   - there are pending responses; or
  //     TODO: fix this restriction, it's not always obvious when there is a
  //     pending response.
  //   - there are associated interfaces running.
  //     TODO(yzshen): For now, users need to make sure there is no one holding
  //     on to associated interface endpoint handles at both sides of the
  //     message pipe in order to call this method. We need a way to forcefully
  //     invalidate associated interface endpoint handles.
  InterfacePtrInfo<Interface> PassInterface() {
    CHECK(!HasAssociatedInterfaces());
    CHECK(!internal_state_.has_pending_callbacks());
    State state;
    internal_state_.Swap(&state);

    return state.PassInterface();
  }

  bool Equals(const InterfacePtr& other) const {
    if (this == &other)
      return true;

    // Now that the two refer to different objects, they are equivalent if
    // and only if they are both null.
    return !(*this) && !other;
  }

  // DO NOT USE. Exposed only for internal use and for testing.
  internal::InterfacePtrState<Interface>* internal_state() {
    return &internal_state_;
  }

  // Allow InterfacePtr<> to be used in boolean expressions.
  explicit operator bool() const { return internal_state_.is_bound(); }

 private:
  typedef internal::InterfacePtrState<Interface> State;
  mutable State internal_state_;

  DISALLOW_COPY_AND_ASSIGN(InterfacePtr);
};

// If |info| is valid (containing a valid message pipe handle), returns an
// InterfacePtr bound to it. Otherwise, returns an unbound InterfacePtr.
template <typename Interface>
InterfacePtr<Interface> MakeProxy(
    InterfacePtrInfo<Interface> info,
    scoped_refptr<base::SequencedTaskRunner> runner = nullptr) {
  InterfacePtr<Interface> ptr;
  if (info.is_valid())
    ptr.Bind(std::move(info), std::move(runner));
  return std::move(ptr);
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_PTR_H_
