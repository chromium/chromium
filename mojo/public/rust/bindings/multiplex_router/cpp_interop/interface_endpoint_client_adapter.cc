// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Design Notes
// - C++ `InterfaceEndpointClient` normally expects static
//   Mojom method metadata (names, IPC hashes). Because generic Rust associated
//   endpoints decode messages dynamically in Rust, we supply stub callbacks
//   (`DummyMethodNameCallback`, `DummyMethodInfoCallback`).
// - Message payload validation is performed when Rust
//   parses raw Mojom messages, so `PassThroughValidator` skips duplicate
//   validation on the C++ side.
// - `RustResponder`: Receives incoming C++ responses for messages originally
//   sent from Rust, unpacking the reply and forwarding it to Rust's message
//   handler.

#include "mojo/public/rust/bindings/multiplex_router/cpp_interop/interface_endpoint_client_adapter.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/rust/bindings/multiplex_router/cpp_interop/cxx.rs.h"

// Dummy functions to provide our contained `InterfaceEndpointClient`
namespace {

uint32_t DummyIpcHash() {
  return 0;
}

mojo::IPCStableHashFunction DummyMethodInfoCallback(mojo::Message&) {
  return &DummyIpcHash;
}

const char* DummyMethodNameCallback(mojo::Message&) {
  return "RustAssociatedInterface";
}

}  // namespace

namespace mojo::rust::bindings {

// This class is a necessary intermediary that C++ requires when it receives a
// response. When we send a message (from Rust) that expects a response, we'll
// register one of these with the endpoint client. When the response arrives,
// it will call `Accept`, which will simply forward the message to the actual
// Rust handler.
class RustResponder : public mojo::MessageReceiver {
 public:
  explicit RustResponder(const EndpointInfo& info) : info_(info) {}

  bool Accept(mojo::Message* message) override {
    return cxx_incoming_handler(
        info_,
        std::make_unique<mojo::rust::ScopedMessageHandleWrapper>(
            message->TakeMojoMessage()),
        nullptr);
  }

 private:
  // RAW_PTR_EXCLUSION: FFI (this is allocated by Rust). Note that this struct
  // is owned by `client_` (via its internal async responders map). `client_`
  // drops all pending responders when the pipe closes or when `client_` is
  // destroyed, which occurs before `info_` is freed.
  RAW_PTR_EXCLUSION const EndpointInfo& info_;
};

bool InterfaceEndpointClientAdapter::NoOpValidator::Accept(
    mojo::Message* message) {
  return true;
}

InterfaceEndpointClientAdapter::InterfaceEndpointClientAdapter(
    mojo::ScopedInterfaceEndpointHandle handle,
    ::rust::Box<EndpointInfo> info,
    scoped_refptr<base::SequencedTaskRunner> runner)
    : base::RefCountedDeleteOnSequence<InterfaceEndpointClientAdapter>(runner),
      id_(handle.id()),
      info_(std::move(info)),
      task_runner_(std::move(runner)),
      group_controller_(handle.group_controller()),
      client_(std::move(handle),
              /*receiver=*/this,
              /*payload_validator=*/std::make_unique<NoOpValidator>(),
              /*sync_method_ordinals=*/{},
              task_runner_,
              /*interface_version=*/0,
              /*interface_name=*/"RustAssociatedInterface",
              /*method_info_callback=*/&DummyMethodInfoCallback,
              /*method_name_callback=*/&DummyMethodNameCallback) {
  // Safe to use Unretained because `client_` is owned by `this` and destroyed
  // along with it. Using a refptr here would create a reference cycle.
  client_.set_connection_error_handler(
      base::BindOnce(&InterfaceEndpointClientAdapter::OnConnectionError,
                     base::Unretained(this)));
}

InterfaceEndpointClientAdapter::~InterfaceEndpointClientAdapter() = default;

// Receives an incoming one-way IPC message from InterfaceEndpointClient, and
// invokes the Rust incoming callback without a responder. We still pass
// the responder wrapper so that we can use it to register new endpoints.
bool InterfaceEndpointClientAdapter::Accept(mojo::Message* message) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!info_.has_value()) {
    return false;
  }
  return cxx_incoming_handler(
      *info_.value(),
      std::make_unique<mojo::rust::ScopedMessageHandleWrapper>(
          message->TakeMojoMessage()),
      std::make_unique<MojoResponderWrapper>(nullptr, task_runner_,
                                             group_controller_));
}

// Receives an incoming request IPC message from InterfaceEndpointClient with
// a responder, and invokes the Rust incoming callback with the responder
// wrapper.
bool InterfaceEndpointClientAdapter::AcceptWithResponder(
    mojo::Message* message,
    std::unique_ptr<mojo::MessageReceiverWithStatus> responder) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!info_.has_value()) {
    return false;
  }
  return cxx_incoming_handler(
      *info_.value(),
      std::make_unique<mojo::rust::ScopedMessageHandleWrapper>(
          message->TakeMojoMessage()),
      std::make_unique<MojoResponderWrapper>(std::move(responder), task_runner_,
                                             group_controller_));
}

// Invoked by InterfaceEndpointClient on pipe disconnection or error; calls
// the Rust disconnect callback.
void InterfaceEndpointClientAdapter::OnConnectionError() {
  if (info_.has_value()) {
    auto info = std::move(info_.value());
    info_.reset();
    cxx_disconnect_handler(std::move(info));
  }
}

// Unwraps an outgoing message handle from Rust and passes it to
// InterfaceEndpointClient::Accept() to send over the pipe.
void InterfaceEndpointClientAdapter::SendMessage(
    std::unique_ptr<mojo::rust::ScopedMessageHandleWrapper> message_wrapper) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&InterfaceEndpointClientAdapter::SendMessage,
                       scoped_refptr<InterfaceEndpointClientAdapter>(this),
                       std::move(message_wrapper)));
    return;
  }

  if (!info_.has_value()) {
    return;
  }

  mojo::ScopedMessageHandle handle = message_wrapper->take_handle();
  mojo::Message message = mojo::Message::CreateFromMessageHandle(&handle);

  if (message.has_flag(mojo::Message::kFlagExpectsResponse)) {
    client_.AcceptWithResponder(
        &message, std::make_unique<RustResponder>(*info_.value()));
  } else {
    client_.Accept(&message);
  }
}

// Resets the client, closing the endpoint and sending a disconnect
// notification over the IPC pipe immediately.
void InterfaceEndpointClientAdapter::Close() {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&InterfaceEndpointClientAdapter::Close,
                       scoped_refptr<InterfaceEndpointClientAdapter>(this)));
    return;
  }
  client_.CloseWithReason(0, std::string_view());
}

}  // namespace mojo::rust::bindings
