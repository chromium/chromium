// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/remote_webauthn_message_handler.h"

#include <stdint.h>

#include <limits>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "remoting/host/mojom/webauthn_proxy.mojom.h"
#include "remoting/host/webauthn/remote_webauthn_state_change_notifier.h"
#include "remoting/proto/remote_webauthn.pb.h"
#include "remoting/protocol/message_serialization.h"

namespace remoting {

namespace {

template <typename CallbackType, typename... ResponseType>
void FindAndRunCallback(base::flat_map<uint64_t, CallbackType>& callback_map,
                        uint64_t id,
                        ResponseType&&... response) {
  auto it = callback_map.find(id);
  if (it == callback_map.end()) {
    LOG(WARNING) << "No callback found associated with ID: " << id;
    return;
  }
  std::move(it->second).Run(std::forward<ResponseType>(response)...);
  callback_map.erase(it);
}

mojom::WebAuthnExceptionDetailsPtr ProtobufErrorToMojoError(
    const protocol::RemoteWebAuthn::ExceptionDetails& pb_error) {
  auto mojo_error = mojom::WebAuthnExceptionDetails::New();
  mojo_error->name = pb_error.name();
  mojo_error->message = pb_error.message();
  return mojo_error;
}

mojom::WebAuthnExceptionDetailsPtr CreateMojoAbortError() {
  auto mojo_error = mojom::WebAuthnExceptionDetails::New();
  mojo_error->name = "AbortError";
  mojo_error->message = "Request has been canceled by the host.";
  return mojo_error;
}

}  // namespace

RemoteWebAuthnMessageHandler::RemoteWebAuthnMessageHandler(
    const std::string& name,
    std::unique_ptr<protocol::MessagePipe> pipe,
    std::unique_ptr<RemoteWebAuthnStateChangeNotifier> state_change_notifier)
    : protocol::NamedMessagePipeHandler(name, std::move(pipe)) {
  state_change_notifier_ = std::move(state_change_notifier);
  receiver_set_.set_disconnect_handler(
      base::BindRepeating(&RemoteWebAuthnMessageHandler::OnReceiverDisconnected,
                          base::Unretained(this)));
  request_cancellers_.set_disconnect_handler(base::BindRepeating(
      &RemoteWebAuthnMessageHandler::OnRequestCancellerDisconnected,
      base::Unretained(this)));
}

RemoteWebAuthnMessageHandler::~RemoteWebAuthnMessageHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!connected());
}

void RemoteWebAuthnMessageHandler::OnConnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NotifyWebAuthnStateChange();
}

void RemoteWebAuthnMessageHandler::OnIncomingMessage(
    std::unique_ptr<CompoundBuffer> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto remote_webauthn =
      protocol::ParseMessage<protocol::RemoteWebAuthn>(message.get());
  if (!remote_webauthn->has_id()) {
    LOG(ERROR) << "Response doesn't have a message ID.";
    return;
  }
  switch (remote_webauthn->message_case()) {
    case protocol::RemoteWebAuthn::kIsUvpaaResponse:
      OnIsUvpaaResponse(remote_webauthn->id(),
                        remote_webauthn->is_uvpaa_response());
      break;
    case protocol::RemoteWebAuthn::kCreateResponse:
      OnCreateResponse(remote_webauthn->id(),
                       remote_webauthn->create_response());
      break;
    case protocol::RemoteWebAuthn::kGetResponse:
      OnGetResponse(remote_webauthn->id(), remote_webauthn->get_response());
      break;
    case protocol::RemoteWebAuthn::kCancelResponse:
      OnCancelResponse(remote_webauthn->id(),
                       remote_webauthn->cancel_response());
      break;
    default:
      LOG(ERROR) << "Unknown message case: " << remote_webauthn->message_case();
  }
}

void RemoteWebAuthnMessageHandler::OnDisconnecting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Run mojo callbacks with error/default response then clean them up.
  for (auto& entry : is_uvpaa_callbacks_) {
    std::move(entry.second).Run(false);
  }
  is_uvpaa_callbacks_.clear();
  VLOG(1) << "Number of bound receivers on disconnecting: "
          << receiver_set_.size();
  receiver_set_.Clear();

  NotifyWebAuthnStateChange();
}

void RemoteWebAuthnMessageHandler::
    IsUserVerifyingPlatformAuthenticatorAvailable(
        IsUserVerifyingPlatformAuthenticatorAvailableCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  uint64_t id = AssignNextMessageId();
  is_uvpaa_callbacks_[id] = std::move(callback);

  protocol::RemoteWebAuthn remote_webauthn;
  remote_webauthn.set_id(id);
  // This simply creates the is_uvpaa_request.
  remote_webauthn.mutable_is_uvpaa_request();
  Send(remote_webauthn, base::DoNothing());
}

void RemoteWebAuthnMessageHandler::Create(
    const std::string& request_data,
    mojo::PendingReceiver<mojom::WebAuthnRequestCanceller> request_canceller,
    CreateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  uint64_t id = AssignNextMessageId();
  create_callbacks_[id] = std::move(callback);
  AddRequestCanceller(id, std::move(request_canceller));

  protocol::RemoteWebAuthn remote_webauthn;
  remote_webauthn.set_id(id);
  remote_webauthn.mutable_create_request()->set_request_details_json(
      request_data);
  Send(remote_webauthn, base::DoNothing());
}

void RemoteWebAuthnMessageHandler::Get(
    const std::string& request_data,
    mojo::PendingReceiver<mojom::WebAuthnRequestCanceller> request_canceller,
    GetCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  uint64_t id = AssignNextMessageId();
  get_callbacks_[id] = std::move(callback);
  AddRequestCanceller(id, std::move(request_canceller));

  protocol::RemoteWebAuthn remote_webauthn;
  remote_webauthn.set_id(id);
  remote_webauthn.mutable_get_request()->set_request_details_json(request_data);
  Send(remote_webauthn, base::DoNothing());
}

void RemoteWebAuthnMessageHandler::Cancel(CancelCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  uint64_t id = request_cancellers_.current_context();

  if (!base::Contains(create_callbacks_, id) &&
      !base::Contains(get_callbacks_, id)) {
    LOG(ERROR) << "No ongoing request is associated with message ID " << id;
    std::move(callback).Run(false);
    RemoveRequestCancellerByMessageId(id);
    return;
  }

  cancel_callbacks_[id] = std::move(callback);

  protocol::RemoteWebAuthn remote_webauthn;
  remote_webauthn.set_id(id);
  // This creates an empty cancel request.
  remote_webauthn.mutable_cancel_request();
  Send(remote_webauthn, base::DoNothing());
}

void RemoteWebAuthnMessageHandler::AddReceiver(
    mojo::PendingReceiver<mojom::WebAuthnProxy> receiver) {
  if (!connected()) {
    LOG(WARNING)
        << "Pending receiver rejected since message handler is not connected.";
    return;
  }
  mojo::ReceiverId id = receiver_set_.Add(this, std::move(receiver));
  VLOG(1) << "New receiver added. Receiver ID: " << id;
}

void RemoteWebAuthnMessageHandler::ClearReceivers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receiver_set_.Clear();
  request_cancellers_.Clear();
  message_id_to_request_canceller_.clear();
  is_uvpaa_callbacks_.clear();
  create_callbacks_.clear();
  get_callbacks_.clear();
  cancel_callbacks_.clear();
}

void RemoteWebAuthnMessageHandler::NotifyWebAuthnStateChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  state_change_notifier_->NotifyStateChange();
}

base::WeakPtr<RemoteWebAuthnMessageHandler>
RemoteWebAuthnMessageHandler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void RemoteWebAuthnMessageHandler::OnReceiverDisconnected() {
  VLOG(1) << "Receiver disconnected. Receiver ID: "
          << receiver_set_.current_receiver();
}

void RemoteWebAuthnMessageHandler::OnIsUvpaaResponse(
    uint64_t id,
    const protocol::RemoteWebAuthn_IsUvpaaResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  FindAndRunCallback(is_uvpaa_callbacks_, id, response.is_available());
}

void RemoteWebAuthnMessageHandler::OnCreateResponse(
    uint64_t id,
    const protocol::RemoteWebAuthn_CreateResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojom::WebAuthnCreateResponsePtr mojo_response;
  switch (response.result_case()) {
    case protocol::RemoteWebAuthn::CreateResponse::ResultCase::kError:
      mojo_response = mojom::WebAuthnCreateResponse::NewErrorDetails(
          ProtobufErrorToMojoError(response.error()));
      break;
    case protocol::RemoteWebAuthn::CreateResponse::ResultCase::kResponseJson:
      mojo_response = mojom::WebAuthnCreateResponse::NewResponseData(
          response.response_json());
      break;
    case protocol::RemoteWebAuthn::CreateResponse::ResultCase::RESULT_NOT_SET:
      // Do nothing and send a nullptr to the mojo client. This means the remote
      // create() call has yielded `null`, which is still a valid response
      // according to the spec.
      break;
    default:
      NOTREACHED() << "Unknown create result case: " << response.result_case();
  }

  RemoveRequestCancellerByMessageId(id);
  FindAndRunCallback(create_callbacks_, id, std::move(mojo_response));
}

void RemoteWebAuthnMessageHandler::OnGetResponse(
    uint64_t id,
    const protocol::RemoteWebAuthn_GetResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojom::WebAuthnGetResponsePtr mojo_response;
  switch (response.result_case()) {
    case protocol::RemoteWebAuthn::GetResponse::ResultCase::kError:
      mojo_response = mojom::WebAuthnGetResponse::NewErrorDetails(
          ProtobufErrorToMojoError(response.error()));
      break;
    case protocol::RemoteWebAuthn::GetResponse::ResultCase::kResponseJson:
      mojo_response =
          mojom::WebAuthnGetResponse::NewResponseData(response.response_json());
      break;
    case protocol::RemoteWebAuthn::GetResponse::ResultCase::RESULT_NOT_SET:
      // Do nothing and send a nullptr to the mojo client. This means the remote
      // get() call has yielded `null`, which is still a valid response
      // according to the spec.
      break;
    default:
      NOTREACHED() << "Unknown get result case: " << response.result_case();
  }

  RemoveRequestCancellerByMessageId(id);
  FindAndRunCallback(get_callbacks_, id, std::move(mojo_response));
}

void RemoteWebAuthnMessageHandler::OnCancelResponse(
    uint64_t id,
    const protocol::RemoteWebAuthn_CancelResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!response.was_canceled()) {
    LOG(WARNING) << "Client failed to cancel request with ID " << id;
    FindAndRunCallback(cancel_callbacks_, id, /* was_canceled= */ false);
    // Don't remove request canceller here since cancelation might succeed after
    // retrying.
    return;
  }

  bool cancelling_create_request = base::Contains(create_callbacks_, id);
  bool cancelling_get_request = base::Contains(get_callbacks_, id);

  if (cancelling_create_request || cancelling_get_request) {
    FindAndRunCallback(cancel_callbacks_, id, /* was_canceled= */ true);
    if (cancelling_create_request) {
      FindAndRunCallback(create_callbacks_, id,
                         mojom::WebAuthnCreateResponse::NewErrorDetails(
                             CreateMojoAbortError()));
    }
    if (cancelling_get_request) {
      // The ID should belong to only one callback list.
      DCHECK(!cancelling_create_request);
      FindAndRunCallback(
          get_callbacks_, id,
          mojom::WebAuthnGetResponse::NewErrorDetails(CreateMojoAbortError()));
    }
    RemoveRequestCancellerByMessageId(id);
    return;
  }

  LOG(WARNING) << "Can't find cancelable request associated with ID " << id;
  FindAndRunCallback(cancel_callbacks_, id, /* was_canceled= */ false);
  RemoveRequestCancellerByMessageId(id);
}

uint64_t RemoteWebAuthnMessageHandler::AssignNextMessageId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return current_message_id_++;
}

void RemoteWebAuthnMessageHandler::AddRequestCanceller(
    uint64_t message_id,
    mojo::PendingReceiver<mojom::WebAuthnRequestCanceller> request_canceller) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  message_id_to_request_canceller_[message_id] = request_cancellers_.Add(
      this, std::move(request_canceller), /* context= */ message_id);
}

void RemoteWebAuthnMessageHandler::RemoveRequestCancellerByMessageId(
    uint64_t message_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = message_id_to_request_canceller_.find(message_id);
  if (it != message_id_to_request_canceller_.end()) {
    request_cancellers_.Remove(it->second);
    message_id_to_request_canceller_.erase(it);
  } else {
    LOG(WARNING) << "Cannot find receiver for message ID " << message_id;
  }
}

void RemoteWebAuthnMessageHandler::OnRequestCancellerDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  message_id_to_request_canceller_.erase(request_cancellers_.current_context());
}

}  // namespace remoting
