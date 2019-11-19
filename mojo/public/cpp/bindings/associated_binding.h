// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_BINDING_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_BINDING_H_

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/component_export.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/associated_interface_request.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/raw_ptr_impl_ref_traits.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace mojo {

class MessageFilter;
class MessageReceiver;

// Base class used to factor out code in AssociatedBinding<T> expansions, in
// particular for Bind().
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) AssociatedBindingBase {
 public:
  AssociatedBindingBase();
  virtual ~AssociatedBindingBase();

  // Sets a message filter to be notified of each incoming message before
  // dispatch. If a filter returns |false| from Accept(), the message is not
  // dispatched and the pipe is closed. Filters cannot be removed once added
  // and only one can be set.
  void SetFilter(std::unique_ptr<MessageFilter> filter);

  // Closes the associated interface. Puts this object into a state where it can
  // be rebound.
  void Close();

  // Similar to the method above, but also specifies a disconnect reason.
  void CloseWithReason(uint32_t custom_reason, const std::string& description);

  // Sets an error handler that will be called if a connection error occurs.
  //
  // This method may only be called after this AssociatedBinding has been bound
  // to a message pipe. The error handler will be reset when this
  // AssociatedBinding is unbound or closed.
  void set_connection_error_handler(base::OnceClosure error_handler);

  void set_connection_error_with_reason_handler(
      ConnectionErrorWithReasonCallback error_handler);

  // Indicates whether the associated binding has been completed.
  bool is_bound() const { return !!endpoint_client_; }

  explicit operator bool() const { return !!endpoint_client_; }

  // Sends a message on the underlying message pipe and runs the current
  // message loop until its response is received. This can be used in tests to
  // verify that no message was sent on a message pipe in response to some
  // stimulus.
  void FlushForTesting();

 protected:
  void BindImpl(ScopedInterfaceEndpointHandle handle,
                MessageReceiverWithResponderStatus* receiver,
                std::unique_ptr<MessageReceiver> payload_validator,
                bool expect_sync_requests,
                scoped_refptr<base::SequencedTaskRunner> runner,
                uint32_t interface_version,
                const char* interface_name);

  std::unique_ptr<InterfaceEndpointClient> endpoint_client_;
};

// Represents the implementation side of an associated interface. It is similar
// to Binding, except that it doesn't own a message pipe handle.
//
// When you bind this class to a request, optionally you can specify a
// base::SequencedTaskRunner. This task runner must belong to the same
// sequence. It will be used to dispatch incoming method calls and connection
// error notification. It is useful when you attach multiple task runners to a
// single thread for the purposes of task scheduling. Please note that
// incoming synchronous method calls may not be run from this task runner, when
// they reenter outgoing synchronous calls on the same thread.
template <typename Interface,
          typename ImplRefTraits = RawPtrImplRefTraits<Interface>>
class AssociatedBinding : public AssociatedBindingBase {
 public:
  using ImplPointerType = typename ImplRefTraits::PointerType;

  // Constructs an incomplete associated binding that will use the
  // implementation |impl|. It may be completed with a subsequent call to the
  // |Bind| method.
  explicit AssociatedBinding(ImplPointerType impl) {
    stub_.set_sink(std::move(impl));
  }

  // Constructs a completed associated binding of |impl|. |impl| must outlive
  // the binding.
  AssociatedBinding(ImplPointerType impl,
                    AssociatedInterfaceRequest<Interface> request,
                    scoped_refptr<base::SequencedTaskRunner> runner = nullptr)
      : AssociatedBinding(std::move(impl)) {
    Bind(std::move(request), std::move(runner));
  }

  ~AssociatedBinding() override {}

  // Sets up this object as the implementation side of an associated interface.
  void Bind(AssociatedInterfaceRequest<Interface> request,
            scoped_refptr<base::SequencedTaskRunner> runner = nullptr) {
    BindImpl(request.PassHandle(), &stub_,
             base::WrapUnique(new typename Interface::RequestValidator_()),
             Interface::HasSyncMethods_, std::move(runner), Interface::Version_,
             Interface::Name_);
  }

  // Unbinds and returns the associated interface request so it can be
  // used in another context, such as on another sequence or with a different
  // implementation. Puts this object into a state where it can be rebound.
  AssociatedInterfaceRequest<Interface> Unbind() {
    DCHECK(endpoint_client_);
    AssociatedInterfaceRequest<Interface> request(
        endpoint_client_->PassHandle());
    endpoint_client_.reset();
    return request;
  }

  // Returns the interface implementation that was previously specified.
  Interface* impl() { return ImplRefTraits::GetRawPointer(&stub_.sink()); }

  // Allows test code to swap the interface implementation.
  ImplPointerType SwapImplForTesting(ImplPointerType new_impl) {
    Interface* old_impl = impl();
    stub_.set_sink(std::move(new_impl));
    return old_impl;
  }

 private:
  typename Interface::template Stub_<ImplRefTraits> stub_;

  DISALLOW_COPY_AND_ASSIGN(AssociatedBinding);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_BINDING_H_
