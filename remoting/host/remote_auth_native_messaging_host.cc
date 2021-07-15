// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_auth_native_messaging_host.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "remoting/host/native_messaging/native_messaging_constants.h"
#include "remoting/host/native_messaging/native_messaging_helpers.h"

namespace remoting {

RemoteAuthNativeMessagingHost::RemoteAuthNativeMessagingHost(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner) {}

RemoteAuthNativeMessagingHost::~RemoteAuthNativeMessagingHost() {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void RemoteAuthNativeMessagingHost::OnMessage(const std::string& message) {
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
  } else {
    LOG(ERROR) << "Unsupported request type: " << type;
  }
}

void RemoteAuthNativeMessagingHost::Start(
    extensions::NativeMessageHost::Client* client) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_ = client;
}

scoped_refptr<base::SingleThreadTaskRunner>
RemoteAuthNativeMessagingHost::task_runner() const {
  return task_runner_;
}

void RemoteAuthNativeMessagingHost::ProcessHello(base::Value response) {
  // Hello request: {id: string, type: 'hello'}
  // Hello response: {id: string, type: 'helloResponse', hostVersion: string}

  DCHECK(task_runner_->BelongsToCurrentThread());

  ProcessNativeMessageHelloResponse(response);
  SendMessageToClient(std::move(response));
}

void RemoteAuthNativeMessagingHost::SendMessageToClient(base::Value message) {
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
