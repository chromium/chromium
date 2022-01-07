// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/remote_webauthn_message_handler.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "remoting/proto/remote_webauthn.pb.h"
#include "remoting/protocol/message_serialization.h"

namespace {

template <typename CallbackType, typename ResponseType>
void FindAndRunCallback(base::flat_map<uint64_t, CallbackType>& callback_map,
                        uint64_t id,
                        ResponseType response) {
  auto it = callback_map.find(id);
  if (it == callback_map.end()) {
    LOG(WARNING) << "No callback found associated with ID: " << id;
    return;
  }
  std::move(it->second).Run(std::move(response));
  callback_map.erase(it);
}

}  // namespace

namespace remoting {

RemoteWebAuthnMessageHandler::RemoteWebAuthnMessageHandler(
    const std::string& name,
    std::unique_ptr<protocol::MessagePipe> pipe)
    : protocol::NamedMessagePipeHandler(name, std::move(pipe)) {
  receiver_set_.set_disconnect_handler(
      base::BindRepeating(&RemoteWebAuthnMessageHandler::OnReceiverDisconnected,
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

void RemoteWebAuthnMessageHandler::Create(const std::string& request_data,
                                          CreateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  uint64_t id = AssignNextMessageId();
  create_callbacks_[id] = std::move(callback);

  protocol::RemoteWebAuthn remote_webauthn;
  remote_webauthn.set_id(id);
  remote_webauthn.mutable_create_request()->set_request_details_json(
      request_data);
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
}

void RemoteWebAuthnMessageHandler::NotifyWebAuthnStateChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  extension_notifier_.NotifyStateChange();
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
    case protocol::RemoteWebAuthn::CreateResponse::ResultCase::kErrorName:
      mojo_response =
          mojom::WebAuthnCreateResponse::NewErrorName(response.error_name());
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

  FindAndRunCallback(create_callbacks_, id, std::move(mojo_response));
}

uint64_t RemoteWebAuthnMessageHandler::AssignNextMessageId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return current_message_id_++;
}

}  // namespace remoting
