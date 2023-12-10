// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/pipe_control_message_proxy.h"

#include <stddef.h>

#include <tuple>
#include <utility>

#include "mojo/public/cpp/bindings/lib/message_fragment.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/interfaces/bindings/pipe_control_messages.mojom.h"

namespace mojo {
namespace {

const char kMessageTag[] = "PipeControlMessageProxy";

Message ConstructRunOrClosePipeMessage(
    pipe_control::RunOrClosePipeInputPtr input_ptr) {
  auto params_ptr = pipe_control::RunOrClosePipeMessageParams::New();
  params_ptr->input = std::move(input_ptr);

  Message message(pipe_control::kRunOrClosePipeMessageId, 0, 0, 0, nullptr);
  internal::MessageFragment<
      pipe_control::internal::RunOrClosePipeMessageParams_Data>
      fragment(message);
  internal::Serialize<pipe_control::RunOrClosePipeMessageParamsDataView>(
      params_ptr, fragment);
  message.set_interface_id(kInvalidInterfaceId);
  message.set_heap_profiler_tag(kMessageTag);
  message.SerializeHandles(/*group_controller=*/nullptr);
  return message;
}

}  // namespace

PipeControlMessageProxy::PipeControlMessageProxy(MessageReceiver* receiver)
    : receiver_(receiver) {}

void PipeControlMessageProxy::NotifyPeerEndpointClosed(
    InterfaceId id,
    const std::optional<DisconnectReason>& reason) {
  Message message(ConstructPeerEndpointClosedMessage(id, reason));
  message.set_heap_profiler_tag(kMessageTag);
  std::ignore = receiver_->Accept(&message);
}

void PipeControlMessageProxy::PausePeerUntilFlushCompletes(PendingFlush flush) {
  auto input = pipe_control::RunOrClosePipeInput::NewPauseUntilFlushCompletes(
      pipe_control::PauseUntilFlushCompletes::New(flush.PassPipe()));
  Message message(ConstructRunOrClosePipeMessage(std::move(input)));
  std::ignore = receiver_->Accept(&message);
}

void PipeControlMessageProxy::FlushAsync(AsyncFlusher flusher) {
  auto input = pipe_control::RunOrClosePipeInput::NewFlushAsync(
      pipe_control::FlushAsync::New(flusher.PassPipe()));
  Message message(ConstructRunOrClosePipeMessage(std::move(input)));
  std::ignore = receiver_->Accept(&message);
}

// static
Message PipeControlMessageProxy::ConstructPeerEndpointClosedMessage(
    InterfaceId id,
    const std::optional<DisconnectReason>& reason) {
  auto event = pipe_control::PeerAssociatedEndpointClosedEvent::New();
  event->id = id;
  if (reason) {
    event->disconnect_reason = pipe_control::DisconnectReason::New();
    event->disconnect_reason->custom_reason = reason->custom_reason;
    event->disconnect_reason->description = reason->description;
  }

  auto input =
      pipe_control::RunOrClosePipeInput::NewPeerAssociatedEndpointClosedEvent(
          std::move(event));

  return ConstructRunOrClosePipeMessage(std::move(input));
}

}  // namespace mojo
