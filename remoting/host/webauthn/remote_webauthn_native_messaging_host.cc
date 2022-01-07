// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/remote_webauthn_native_messaging_host.h"

#include <memory>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "remoting/host/native_messaging/native_messaging_constants.h"
#include "remoting/host/native_messaging/native_messaging_helpers.h"
#include "remoting/host/webauthn/remote_webauthn_constants.h"
#include "remoting/host/webauthn/remote_webauthn_message_handler.h"

namespace remoting {

RemoteWebAuthnNativeMessagingHost::RemoteWebAuthnNativeMessagingHost(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner) {}

RemoteWebAuthnNativeMessagingHost::~RemoteWebAuthnNativeMessagingHost() {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void RemoteWebAuthnNativeMessagingHost::OnMessage(const std::string& message) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  std::string type;
  base::Value request;
  if (!ParseNativeMessageJson(message, type, request)) {
    return;
  }

  base::Value response = CreateNativeMessageResponse(request);
  if (response.is_none()) {
    return;
  }

  if (type == kHelloMessage) {
    ProcessHello(std::move(response));
  } else if (type == kIsUvpaaMessageType) {
    ProcessIsUvpaa(request, std::move(response));
  } else if (type == kGetRemoteStateMessageType) {
    ProcessGetRemoteState(std::move(response));
  } else if (type == kCreateMessageType) {
    ProcessCreate(request, std::move(response));
  } else {
    LOG(ERROR) << "Unsupported request type: " << type;
  }
}

void RemoteWebAuthnNativeMessagingHost::Start(
    extensions::NativeMessageHost::Client* client) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_ = client;
}

scoped_refptr<base::SingleThreadTaskRunner>
RemoteWebAuthnNativeMessagingHost::task_runner() const {
  return task_runner_;
}

void RemoteWebAuthnNativeMessagingHost::ProcessHello(base::Value response) {
  // Hello request: {id: string, type: 'hello'}
  // Hello response: {id: string, type: 'helloResponse', hostVersion: string}

  DCHECK(task_runner_->BelongsToCurrentThread());

  ProcessNativeMessageHelloResponse(response);
  SendMessageToClient(std::move(response));
}

void RemoteWebAuthnNativeMessagingHost::ProcessIsUvpaa(
    const base::Value& request,
    base::Value response) {
  // IsUvpaa request: {id: string, type: 'isUvpaa'}
  // IsUvpaa response:
  //   {id: string, type: 'isUvpaaResponse', isAvailable: boolean}

  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!EnsureIpcConnection()) {
    response.SetBoolKey(kIsUvpaaResponseIsAvailableKey, false);
    SendMessageToClient(std::move(response));
    return;
  }

  remote_->IsUserVerifyingPlatformAuthenticatorAvailable(
      base::BindOnce(&RemoteWebAuthnNativeMessagingHost::OnIsUvpaaResponse,
                     base::Unretained(this), std::move(response)));
}

void RemoteWebAuthnNativeMessagingHost::ProcessCreate(
    const base::Value& request,
    base::Value response) {
  // Create request: {id: string, type: 'create', requestData: string}
  // Create response: {
  //   id: string, type: 'createResponse', responseData?: string,
  //   errorName?: string}

  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!EnsureIpcConnection()) {
    // TODO(yuweih): See if this is the right error to use here.
    response.SetStringKey(kCreateResponseErrorNameKey, "InvalidStateError");
    SendMessageToClient(std::move(response));
    return;
  }

  const std::string* request_data =
      request.FindStringKey(kCreateRequestDataKey);
  if (!request_data) {
    LOG(ERROR) << "Request data not found in create request.";
    // navigator.credentials.create() throws NotSupportedError if the parameter
    // is unexpected.
    response.SetStringKey(kCreateResponseErrorNameKey, "NotSupportedError");
    SendMessageToClient(std::move(response));
    return;
  }

  remote_->Create(
      *request_data,
      base::BindOnce(&RemoteWebAuthnNativeMessagingHost::OnCreateResponse,
                     base::Unretained(this), std::move(response)));
}

void RemoteWebAuthnNativeMessagingHost::ProcessGetRemoteState(
    base::Value response) {
  // GetRemoteState request: {id: string, type: 'getRemoteState'}
  // GetRemoteState response: {id: string, type: 'getRemoteStateResponse'}

  DCHECK(task_runner_->BelongsToCurrentThread());

  // We query and report the remote state one at a time to prevent race
  // conditions caused by multiple requests coming in while there is already a
  // pending request (e.g. WebAuthn channel connected and AttachToDesktop on
  // Windows).
  get_remote_state_responses_.push(std::move(response));
  if (get_remote_state_responses_.size() == 1) {
    QueryNextRemoteState();
  }
  // Otherwise it means there is already a pending remote state request.
}

void RemoteWebAuthnNativeMessagingHost::OnQueryVersionResult(uint32_t version) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  SendNextRemoteState(true);
}

void RemoteWebAuthnNativeMessagingHost::OnIpcDisconnected() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // TODO(yuweih): Feed pending callbacks with error responses.
  remote_.reset();
  if (!get_remote_state_responses_.empty()) {
    SendNextRemoteState(false);
  }
}

void RemoteWebAuthnNativeMessagingHost::OnIsUvpaaResponse(base::Value response,
                                                          bool is_available) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  response.SetBoolKey(kIsUvpaaResponseIsAvailableKey, is_available);
  SendMessageToClient(std::move(response));
}

void RemoteWebAuthnNativeMessagingHost::OnCreateResponse(
    base::Value response,
    mojom::WebAuthnCreateResponsePtr remote_response) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // If |remote_response| is null, it means that the remote create() call has
  // yielded `null`, which is still a valid response according to the spec. In
  // this case we just send back an empty create response.
  if (!remote_response.is_null()) {
    switch (remote_response->which()) {
      case mojom::WebAuthnCreateResponse::Tag::kErrorName:
        response.SetStringKey(kCreateResponseErrorNameKey,
                              remote_response->get_error_name());
        break;
      case mojom::WebAuthnCreateResponse::Tag::kResponseData:
        response.SetStringKey(kCreateResponseDataKey,
                              remote_response->get_response_data());
        break;
      default:
        NOTREACHED() << "Unexpected create response tag: "
                     << static_cast<uint32_t>(remote_response->which());
    }
  }

  SendMessageToClient(std::move(response));
}

void RemoteWebAuthnNativeMessagingHost::QueryNextRemoteState() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!EnsureIpcConnection()) {
    SendNextRemoteState(false);
    return;
  }

  // QueryVersion() is simply used to determine if the receiving end actually
  // accepts the connection. If it doesn't, then the callback will be silently
  // dropped, and OnIpcDisconnected() will be called instead.
  remote_.QueryVersion(
      base::BindOnce(&RemoteWebAuthnNativeMessagingHost::OnQueryVersionResult,
                     base::Unretained(this)));
}

void RemoteWebAuthnNativeMessagingHost::SendNextRemoteState(bool is_remoted) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!get_remote_state_responses_.empty());

  auto response = std::move(get_remote_state_responses_.front());
  get_remote_state_responses_.pop();
  DCHECK(response.is_dict());

  response.SetBoolKey(kGetRemoteStateResponseIsRemotedKey, is_remoted);
  SendMessageToClient(std::move(response));
  if (!get_remote_state_responses_.empty()) {
    QueryNextRemoteState();
  }
}

bool RemoteWebAuthnNativeMessagingHost::EnsureIpcConnection() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (remote_.is_bound()) {
    return true;
  }

  auto* api = host_service_api_client_.GetSessionServices();
  if (!api) {
    return false;
  }
  api->BindWebAuthnProxy(remote_.BindNewPipeAndPassReceiver());
  remote_.set_disconnect_handler(
      base::BindOnce(&RemoteWebAuthnNativeMessagingHost::OnIpcDisconnected,
                     base::Unretained(this)));
  return true;
}

void RemoteWebAuthnNativeMessagingHost::SendMessageToClient(
    base::Value message) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!message.is_none());

  std::string message_json;
  if (!base::JSONWriter::Write(message, &message_json)) {
    LOG(ERROR) << "Failed to write message to JSON";
    return;
  }
  client_->PostMessageFromNativeHost(message_json);
}

}  // namespace remoting
