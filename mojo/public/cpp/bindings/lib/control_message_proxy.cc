// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/control_message_proxy.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/lib/validation_util.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/interfaces/bindings/interface_control_messages.mojom.h"

namespace mojo {
namespace internal {

namespace {

const char kMessageTag[] = "ControlMessageProxy";

bool ValidateControlResponse(Message* message) {
  ValidationContext validation_context(message->payload(),
                                       message->payload_num_bytes(), 0, 0,
                                       message, "ControlResponseValidator");
  if (!ValidateMessageIsResponse(message, &validation_context))
    return false;

  switch (message->header()->name) {
    case interface_control::kRunMessageId:
      return ValidateMessagePayload<
          interface_control::internal::RunResponseMessageParams_Data>(
          message, &validation_context);
  }
  return false;
}

using RunCallback =
    base::OnceCallback<void(interface_control::RunResponseMessageParamsPtr)>;

class RunResponseForwardToCallback : public MessageReceiver {
 public:
  explicit RunResponseForwardToCallback(RunCallback callback)
      : callback_(std::move(callback)) {}

  RunResponseForwardToCallback(const RunResponseForwardToCallback&) = delete;
  RunResponseForwardToCallback& operator=(const RunResponseForwardToCallback&) =
      delete;

  bool Accept(Message* message) override;

 private:
  RunCallback callback_;
};

bool RunResponseForwardToCallback::Accept(Message* message) {
  if (!ValidateControlResponse(message))
    return false;

  interface_control::internal::RunResponseMessageParams_Data* params =
      reinterpret_cast<
          interface_control::internal::RunResponseMessageParams_Data*>(
          message->mutable_payload());
  interface_control::RunResponseMessageParamsPtr params_ptr;
  Deserialize<interface_control::RunResponseMessageParamsDataView>(
      params, &params_ptr, message);

  std::move(callback_).Run(std::move(params_ptr));
  return true;
}

void SendRunMessage(InterfaceEndpointClient* endpoint,
                    interface_control::RunInputPtr input_ptr,
                    RunCallback callback) {
  auto params_ptr = interface_control::RunMessageParams::New();
  params_ptr->input = std::move(input_ptr);
  Message message(interface_control::kRunMessageId,
                  Message::kFlagExpectsResponse, 0, 0, nullptr);
  message.set_heap_profiler_tag(kMessageTag);
  MessageFragment<interface_control::internal::RunMessageParams_Data> fragment(
      message);
  Serialize<interface_control::RunMessageParamsDataView>(params_ptr, fragment);
  std::unique_ptr<MessageReceiver> responder =
      std::make_unique<RunResponseForwardToCallback>(std::move(callback));
  endpoint->SendControlMessageWithResponder(&message, std::move(responder));
}

Message ConstructRunOrClosePipeMessage(
    interface_control::RunOrClosePipeInputPtr input_ptr) {
  auto params_ptr = interface_control::RunOrClosePipeMessageParams::New();
  params_ptr->input = std::move(input_ptr);
  Message message(interface_control::kRunOrClosePipeMessageId, 0, 0, 0,
                  nullptr);
  message.set_heap_profiler_tag(kMessageTag);

  MessageFragment<interface_control::internal::RunOrClosePipeMessageParams_Data>
      fragment(message);
  Serialize<interface_control::RunOrClosePipeMessageParamsDataView>(params_ptr,
                                                                    fragment);
  return message;
}

void SendRunOrClosePipeMessage(
    InterfaceEndpointClient* endpoint,
    interface_control::RunOrClosePipeInputPtr input_ptr) {
  Message message(ConstructRunOrClosePipeMessage(std::move(input_ptr)));
  message.set_heap_profiler_tag(kMessageTag);
  endpoint->SendControlMessage(&message);
}

void RunVersionCallback(
    base::OnceCallback<void(uint32_t)> callback,
    interface_control::RunResponseMessageParamsPtr run_response) {
  uint32_t version = 0u;
  if (run_response->output && run_response->output->is_query_version_result())
    version = run_response->output->get_query_version_result()->version;
  std::move(callback).Run(version);
}

void RunClosure(base::OnceClosure callback,
                interface_control::RunResponseMessageParamsPtr run_response) {
  std::move(callback).Run();
}

}  // namespace

ControlMessageProxy::ControlMessageProxy(InterfaceEndpointClient* owner)
    : owner_(owner) {}

ControlMessageProxy::~ControlMessageProxy() {
  // If this is destroyed in the middle of a flush, make sure the callback is
  // still run.
  if (!pending_flush_callback_.is_null())
    RunFlushForTestingClosure();
}

void ControlMessageProxy::QueryVersion(
    base::OnceCallback<void(uint32_t)> callback) {
  auto input_ptr = interface_control::RunInput::NewQueryVersion(
      interface_control::QueryVersion::New());
  SendRunMessage(owner_, std::move(input_ptr),
                 base::BindOnce(&RunVersionCallback, std::move(callback)));
}

void ControlMessageProxy::RequireVersion(uint32_t version) {
  auto require_version = interface_control::RequireVersion::New();
  require_version->version = version;
  auto input_ptr = interface_control::RunOrClosePipeInput::NewRequireVersion(
      std::move(require_version));
  SendRunOrClosePipeMessage(owner_, std::move(input_ptr));
}

void ControlMessageProxy::FlushForTesting() {
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  FlushAsyncForTesting(run_loop.QuitClosure());
  run_loop.Run();
}

void ControlMessageProxy::FlushAsyncForTesting(base::OnceClosure callback) {
  if (encountered_error_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  auto input_ptr = interface_control::RunInput::NewFlushForTesting(
      interface_control::FlushForTesting::New());
  DCHECK(!pending_flush_callback_);
  pending_flush_callback_ = std::move(callback);
  SendRunMessage(
      owner_, std::move(input_ptr),
      base::BindOnce(
          &RunClosure,
          base::BindOnce(&ControlMessageProxy::RunFlushForTestingClosure,
                         base::Unretained(this))));
}

void ControlMessageProxy::RunFlushForTestingClosure() {
  DCHECK(!pending_flush_callback_.is_null());
  std::move(pending_flush_callback_).Run();
}

void ControlMessageProxy::EnableIdleTracking(base::TimeDelta timeout) {
  auto input = interface_control::RunOrClosePipeInput::NewEnableIdleTracking(
      interface_control::EnableIdleTracking::New(timeout.InMicroseconds()));
  SendRunOrClosePipeMessage(owner_, std::move(input));
}

void ControlMessageProxy::SendMessageAck() {
  auto input = interface_control::RunOrClosePipeInput::NewMessageAck(
      interface_control::MessageAck::New());
  SendRunOrClosePipeMessage(owner_, std::move(input));
}

void ControlMessageProxy::NotifyIdle() {
  auto input = interface_control::RunOrClosePipeInput::NewNotifyIdle(
      interface_control::NotifyIdle::New());
  SendRunOrClosePipeMessage(owner_, std::move(input));
}

void ControlMessageProxy::OnConnectionError() {
  encountered_error_ = true;
  if (!pending_flush_callback_.is_null())
    RunFlushForTestingClosure();
}

}  // namespace internal
}  // namespace mojo
