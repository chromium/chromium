// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/remote_webauthn_native_messaging_host.h"

#include <memory>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "remoting/host/native_messaging/native_messaging_constants.h"
#include "remoting/host/native_messaging/native_messaging_helpers.h"
#include "remoting/host/webauthn/remote_webauthn_constants.h"

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

void RemoteWebAuthnNativeMessagingHost::OnIpcDisconnected() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // TODO(yuweih): Feed pending callbacks with error responses.
  remote_.reset();
}

void RemoteWebAuthnNativeMessagingHost::OnIsUvpaaResponse(base::Value response,
                                                          bool is_available) {
  response.SetBoolKey(kIsUvpaaResponseIsAvailableKey, is_available);
  SendMessageToClient(std::move(response));
}

bool RemoteWebAuthnNativeMessagingHost::EnsureIpcConnection() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (remote_.is_bound()) {
    return true;
  }

  auto* api = host_service_api_client_.Get();
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
