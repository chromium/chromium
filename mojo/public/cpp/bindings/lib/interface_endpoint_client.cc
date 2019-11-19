// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/interface_endpoint_client.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/associated_group_controller.h"
#include "mojo/public/cpp/bindings/interface_endpoint_controller.h"
#include "mojo/public/cpp/bindings/lib/task_runner_helper.h"
#include "mojo/public/cpp/bindings/lib/validation_util.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"

namespace mojo {

// ----------------------------------------------------------------------------

namespace {

void DetermineIfEndpointIsConnected(
    const base::WeakPtr<InterfaceEndpointClient>& client,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(client && !client->encountered_error());
}

// When receiving an incoming message which expects a repsonse,
// InterfaceEndpointClient creates a ResponderThunk object and passes it to the
// incoming message receiver. When the receiver finishes processing the message,
// it can provide a response using this object.
class ResponderThunk : public MessageReceiverWithStatus {
 public:
  explicit ResponderThunk(
      const base::WeakPtr<InterfaceEndpointClient>& endpoint_client,
      scoped_refptr<base::SequencedTaskRunner> runner)
      : endpoint_client_(endpoint_client),
        accept_was_invoked_(false),
        task_runner_(std::move(runner)) {}
  ~ResponderThunk() override {
    if (!accept_was_invoked_) {
      // The Service handled a message that was expecting a response
      // but did not send a response.
      // We raise an error to signal the calling application that an error
      // condition occurred. Without this the calling application would have no
      // way of knowing it should stop waiting for a response.
      if (task_runner_->RunsTasksInCurrentSequence()) {
        // Please note that even if this code is run from a different task
        // runner on the same thread as |task_runner_|, it is okay to directly
        // call InterfaceEndpointClient::RaiseError(), because it will raise
        // error from the correct task runner asynchronously.
        if (endpoint_client_) {
          endpoint_client_->RaiseError();
        }
      } else {
        task_runner_->PostTask(
            FROM_HERE, base::BindOnce(&InterfaceEndpointClient::RaiseError,
                                      endpoint_client_));
      }
    }
  }

  // Allows this thunk to be attached to a ConnectionGroup as a means of keeping
  // the group from idling while the response is pending.
  void set_connection_group(ConnectionGroup::Ref connection_group) {
    connection_group_ = std::move(connection_group);
  }

  // MessageReceiver implementation:
  bool PrefersSerializedMessages() override {
    return endpoint_client_ && endpoint_client_->PrefersSerializedMessages();
  }

  bool Accept(Message* message) override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    accept_was_invoked_ = true;
    DCHECK(message->has_flag(Message::kFlagIsResponse));

    bool result = false;

    if (endpoint_client_)
      result = endpoint_client_->Accept(message);

    return result;
  }

  // MessageReceiverWithStatus implementation:
  bool IsConnected() override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    return endpoint_client_ && !endpoint_client_->encountered_error();
  }

  void IsConnectedAsync(base::OnceCallback<void(bool)> callback) override {
    if (task_runner_->RunsTasksInCurrentSequence()) {
      DetermineIfEndpointIsConnected(endpoint_client_, std::move(callback));
    } else {
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&DetermineIfEndpointIsConnected,
                                    endpoint_client_, std::move(callback)));
    }
  }

 private:
  base::WeakPtr<InterfaceEndpointClient> endpoint_client_;
  bool accept_was_invoked_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  ConnectionGroup::Ref connection_group_;

  DISALLOW_COPY_AND_ASSIGN(ResponderThunk);
};

}  // namespace

// ----------------------------------------------------------------------------

InterfaceEndpointClient::SyncResponseInfo::SyncResponseInfo(
    bool* in_response_received)
    : response_received(in_response_received) {}

InterfaceEndpointClient::SyncResponseInfo::~SyncResponseInfo() {}

// ----------------------------------------------------------------------------

InterfaceEndpointClient::HandleIncomingMessageThunk::HandleIncomingMessageThunk(
    InterfaceEndpointClient* owner)
    : owner_(owner) {}

InterfaceEndpointClient::HandleIncomingMessageThunk::
    ~HandleIncomingMessageThunk() {}

bool InterfaceEndpointClient::HandleIncomingMessageThunk::Accept(
    Message* message) {
  return owner_->HandleValidatedMessage(message);
}

// ----------------------------------------------------------------------------

InterfaceEndpointClient::InterfaceEndpointClient(
    ScopedInterfaceEndpointHandle handle,
    MessageReceiverWithResponderStatus* receiver,
    std::unique_ptr<MessageReceiver> payload_validator,
    bool expect_sync_requests,
    scoped_refptr<base::SequencedTaskRunner> runner,
    uint32_t interface_version,
    const char* interface_name)
    : expect_sync_requests_(expect_sync_requests),
      handle_(std::move(handle)),
      incoming_receiver_(receiver),
      dispatcher_(&thunk_),
      task_runner_(std::move(runner)),
      control_message_handler_(this, interface_version),
      interface_name_(interface_name) {
  DCHECK(handle_.is_valid());

  // TODO(yzshen): the way to use validator (or message filter in general)
  // directly is a little awkward.
  if (payload_validator)
    dispatcher_.SetValidator(std::move(payload_validator));

  if (handle_.pending_association()) {
    handle_.SetAssociationEventHandler(base::BindOnce(
        &InterfaceEndpointClient::OnAssociationEvent, base::Unretained(this)));
  } else {
    InitControllerIfNecessary();
  }
}

InterfaceEndpointClient::~InterfaceEndpointClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (controller_)
    handle_.group_controller()->DetachEndpointClient(handle_);
}

AssociatedGroup* InterfaceEndpointClient::associated_group() {
  if (!associated_group_)
    associated_group_ = std::make_unique<AssociatedGroup>(handle_);
  return associated_group_.get();
}

ScopedInterfaceEndpointHandle InterfaceEndpointClient::PassHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!has_pending_responders());

  if (!handle_.is_valid())
    return ScopedInterfaceEndpointHandle();

  handle_.SetAssociationEventHandler(
      ScopedInterfaceEndpointHandle::AssociationEventCallback());

  if (controller_) {
    controller_ = nullptr;
    handle_.group_controller()->DetachEndpointClient(handle_);
  }

  return std::move(handle_);
}

void InterfaceEndpointClient::SetFilter(std::unique_ptr<MessageFilter> filter) {
  dispatcher_.SetFilter(std::move(filter));
}

void InterfaceEndpointClient::RaiseError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!handle_.pending_association())
    handle_.group_controller()->RaiseError();
}

void InterfaceEndpointClient::CloseWithReason(uint32_t custom_reason,
                                              const std::string& description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto handle = PassHandle();
  handle.ResetWithReason(custom_reason, description);
}

bool InterfaceEndpointClient::PrefersSerializedMessages() {
  auto* controller = handle_.group_controller();
  return controller && controller->PrefersSerializedMessages();
}

void InterfaceEndpointClient::SendControlMessage(Message* message) {
  SendMessage(message, true /* is_control_message */);
}

void InterfaceEndpointClient::SendControlMessageWithResponder(
    Message* message,
    std::unique_ptr<MessageReceiver> responder) {
  SendMessageWithResponder(message, true /* is_control_message */,
                           std::move(responder));
}

bool InterfaceEndpointClient::Accept(Message* message) {
  return SendMessage(message, false /* is_control_message */);
}

bool InterfaceEndpointClient::AcceptWithResponder(
    Message* message,
    std::unique_ptr<MessageReceiver> responder) {
  return SendMessageWithResponder(message, false /* is_control_message */,
                                  std::move(responder));
}

bool InterfaceEndpointClient::SendMessage(Message* message,
                                          bool is_control_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!message->has_flag(Message::kFlagExpectsResponse));
  DCHECK(!handle_.pending_association());

  // This has to been done even if connection error has occurred. For example,
  // the message contains a pending associated request. The user may try to use
  // the corresponding associated interface pointer after sending this message.
  // That associated interface pointer has to join an associated group in order
  // to work properly.
  if (!message->associated_endpoint_handles()->empty())
    message->SerializeAssociatedEndpointHandles(handle_.group_controller());

  if (encountered_error_)
    return false;

  InitControllerIfNecessary();

#if DCHECK_IS_ON()
  // TODO(https://crbug.com/695289): Send |next_call_location_| in a control
  // message before calling |SendMessage()| below.
#endif

  message->set_heap_profiler_tag(interface_name_);
  if (!controller_->SendMessage(message))
    return false;

  if (!is_control_message && idle_handler_)
    ++num_unacked_messages_;

  return true;
}

bool InterfaceEndpointClient::SendMessageWithResponder(
    Message* message,
    bool is_control_message,
    std::unique_ptr<MessageReceiver> responder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(message->has_flag(Message::kFlagExpectsResponse));
  DCHECK(!handle_.pending_association());

  // Please see comments in Accept().
  if (!message->associated_endpoint_handles()->empty())
    message->SerializeAssociatedEndpointHandles(handle_.group_controller());

  if (encountered_error_)
    return false;

  InitControllerIfNecessary();

  // Reserve 0 in case we want it to convey special meaning in the future.
  uint64_t request_id = next_request_id_++;
  if (request_id == 0)
    request_id = next_request_id_++;

  message->set_request_id(request_id);
  message->set_heap_profiler_tag(interface_name_);

#if DCHECK_IS_ON()
  // TODO(https://crbug.com/695289): Send |next_call_location_| in a control
  // message before calling |SendMessage()| below.
#endif

  bool is_sync = message->has_flag(Message::kFlagIsSync);
  if (!controller_->SendMessage(message))
    return false;

  if (!is_control_message && idle_handler_)
    ++num_unacked_messages_;

  if (!is_sync || force_outgoing_messages_async_) {
    async_responders_[request_id] = std::move(responder);
    return true;
  }

  SyncCallRestrictions::AssertSyncCallAllowed();

  bool response_received = false;
  sync_responses_.insert(std::make_pair(
      request_id, std::make_unique<SyncResponseInfo>(&response_received)));

  base::WeakPtr<InterfaceEndpointClient> weak_self =
      weak_ptr_factory_.GetWeakPtr();
  controller_->SyncWatch(&response_received);
  // Make sure that this instance hasn't been destroyed.
  if (weak_self) {
    DCHECK(base::Contains(sync_responses_, request_id));
    auto iter = sync_responses_.find(request_id);
    DCHECK_EQ(&response_received, iter->second->response_received);
    if (response_received) {
      ignore_result(responder->Accept(&iter->second->response));
    } else {
      DVLOG(1) << "Mojo sync call returns without receiving a response. "
               << "Typcially it is because the interface has been "
               << "disconnected.";
    }
    sync_responses_.erase(iter);
  }

  return true;
}

bool InterfaceEndpointClient::HandleIncomingMessage(Message* message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return dispatcher_.Accept(message);
}

void InterfaceEndpointClient::NotifyError(
    const base::Optional<DisconnectReason>& reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (encountered_error_)
    return;
  encountered_error_ = true;

  // Response callbacks may hold on to resource, and there's no need to keep
  // them alive any longer. Note that it's allowed that a pending response
  // callback may own this endpoint, so we simply move the responders onto the
  // stack here and let them be destroyed when the stack unwinds.
  AsyncResponderMap responders = std::move(async_responders_);

  control_message_proxy_.OnConnectionError();

  if (error_handler_) {
    std::move(error_handler_).Run();
  } else if (error_with_reason_handler_) {
    if (reason) {
      std::move(error_with_reason_handler_)
          .Run(reason->custom_reason, reason->description);
    } else {
      std::move(error_with_reason_handler_).Run(0, std::string());
    }
  }
}

void InterfaceEndpointClient::QueryVersion(
    base::OnceCallback<void(uint32_t)> callback) {
  control_message_proxy_.QueryVersion(std::move(callback));
}

void InterfaceEndpointClient::RequireVersion(uint32_t version) {
  control_message_proxy_.RequireVersion(version);
}

void InterfaceEndpointClient::FlushForTesting() {
  control_message_proxy_.FlushForTesting();
}

void InterfaceEndpointClient::FlushAsyncForTesting(base::OnceClosure callback) {
  control_message_proxy_.FlushAsyncForTesting(std::move(callback));
}

void InterfaceEndpointClient::SetIdleHandler(base::TimeDelta timeout,
                                             base::RepeatingClosure handler) {
  // We allow for idle handler replacement and changing the timeout duration.
  control_message_proxy_.EnableIdleTracking(timeout);
  idle_handler_ = std::move(handler);
}

void InterfaceEndpointClient::SetIdleTrackingEnabledCallback(
    IdleTrackingEnabledCallback callback) {
  idle_tracking_enabled_callback_ = std::move(callback);
}

bool InterfaceEndpointClient::AcceptEnableIdleTracking(
    base::TimeDelta timeout) {
  // If this is the first time EnableIdleTracking was received, set up the
  // ConnectionGroup and give a ref to our owner.
  if (idle_tracking_enabled_callback_) {
    idle_tracking_connection_group_ = ConnectionGroup::Create(
        base::BindRepeating(&InterfaceEndpointClient::MaybeStartIdleTimer,
                            weak_ptr_factory_.GetWeakPtr()),
        task_runner_);
    std::move(idle_tracking_enabled_callback_)
        .Run(idle_tracking_connection_group_.WeakCopy());
  }

  idle_timeout_ = timeout;
  return true;
}

bool InterfaceEndpointClient::AcceptMessageAck() {
  if (!idle_handler_ || num_unacked_messages_ == 0)
    return false;

  --num_unacked_messages_;
  return true;
}

bool InterfaceEndpointClient::AcceptNotifyIdle() {
  if (!idle_handler_)
    return false;

  // We have outstanding unacked messages, so quietly ignore this NotifyIdle.
  if (num_unacked_messages_ > 0)
    return true;

  // With no outstanding unacked messages, a NotifyIdle received implies that
  // the peer really is idle. We can invoke our idle handler.
  idle_handler_.Run();
  return true;
}

void InterfaceEndpointClient::MaybeStartIdleTimer() {
  // Something has happened to interrupt the current idle state, if any. We
  // either restart the idle timer (if idle again) or clear it so it doesn't
  // fire.
  if (idle_tracking_connection_group_ &&
      idle_tracking_connection_group_.HasZeroRefs()) {
    DCHECK(idle_timeout_);
    notify_idle_timer_.emplace();
    notify_idle_timer_->Start(
        FROM_HERE, *idle_timeout_,
        base::BindOnce(&InterfaceEndpointClient::MaybeSendNotifyIdle,
                       base::Unretained(this)));
  } else {
    notify_idle_timer_.reset();
  }
}

void InterfaceEndpointClient::MaybeSendNotifyIdle() {
  if (idle_tracking_connection_group_ &&
      idle_tracking_connection_group_.HasZeroRefs()) {
    control_message_proxy_.NotifyIdle();
  }
}

void InterfaceEndpointClient::InitControllerIfNecessary() {
  if (controller_ || handle_.pending_association())
    return;

  controller_ = handle_.group_controller()->AttachEndpointClient(handle_, this,
                                                                 task_runner_);
  if (expect_sync_requests_)
    controller_->AllowWokenUpBySyncWatchOnSameThread();
}

void InterfaceEndpointClient::OnAssociationEvent(
    ScopedInterfaceEndpointHandle::AssociationEvent event) {
  if (event == ScopedInterfaceEndpointHandle::ASSOCIATED) {
    InitControllerIfNecessary();
  } else if (event ==
             ScopedInterfaceEndpointHandle::PEER_CLOSED_BEFORE_ASSOCIATION) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&InterfaceEndpointClient::NotifyError,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          handle_.disconnect_reason()));
  }
}

bool InterfaceEndpointClient::HandleValidatedMessage(Message* message) {
  DCHECK_EQ(handle_.id(), message->interface_id());

  if (encountered_error_) {
    // This message is received after error has been encountered. For associated
    // interfaces, this means the remote side sends a
    // PeerAssociatedEndpointClosed event but continues to send more messages
    // for the same interface. Close the pipe because this shouldn't happen.
    DVLOG(1) << "A message is received for an interface after it has been "
             << "disconnected. Closing the pipe.";
    return false;
  }

  auto weak_self = weak_ptr_factory_.GetWeakPtr();
  bool accepted_interface_message = false;
  bool has_response = false;
  if (message->has_flag(Message::kFlagExpectsResponse)) {
    has_response = true;
    auto responder = std::make_unique<ResponderThunk>(
        weak_ptr_factory_.GetWeakPtr(), task_runner_);
    if (mojo::internal::ControlMessageHandler::IsControlMessage(message)) {
      return control_message_handler_.AcceptWithResponder(message,
                                                          std::move(responder));
    } else {
      if (idle_tracking_connection_group_)
        responder->set_connection_group(idle_tracking_connection_group_);
      accepted_interface_message = incoming_receiver_->AcceptWithResponder(
          message, std::move(responder));
    }
  } else if (message->has_flag(Message::kFlagIsResponse)) {
    uint64_t request_id = message->request_id();

    if (message->has_flag(Message::kFlagIsSync) &&
        !force_outgoing_messages_async_) {
      auto it = sync_responses_.find(request_id);
      if (it == sync_responses_.end())
        return false;
      it->second->response = std::move(*message);
      *it->second->response_received = true;
      return true;
    }

    auto it = async_responders_.find(request_id);
    if (it == async_responders_.end())
      return false;
    std::unique_ptr<MessageReceiver> responder = std::move(it->second);
    async_responders_.erase(it);
    return responder->Accept(message);
  } else {
    if (mojo::internal::ControlMessageHandler::IsControlMessage(message))
      return control_message_handler_.Accept(message);

    accepted_interface_message = incoming_receiver_->Accept(message);
  }

  if (weak_self && accepted_interface_message &&
      idle_tracking_connection_group_) {
    control_message_proxy_.SendMessageAck();
    if (!has_response)
      MaybeStartIdleTimer();
  }

  return accepted_interface_message;
}

}  // namespace mojo
