// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/ios/session/remoting_client_session_delegate.h"

#import "remoting/ios/session/remoting_client.h"

#include "base/strings/sys_string_conversions.h"
#include "remoting/client/chromoting_client_runtime.h"

using base::SysUTF8ToNSString;

namespace remoting {

RemotingClientSessionDelegate::RemotingClientSessionDelegate(
    RemotingClient* client)
    : client_(client), weak_factory_(this) {
  runtime_ = ChromotingClientRuntime::GetInstance();
}

RemotingClientSessionDelegate::~RemotingClientSessionDelegate() {
  client_ = nil;
}

void RemotingClientSessionDelegate::OnConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());

  [client_ onConnectionState:state error:error];
}

void RemotingClientSessionDelegate::CommitPairingCredentials(
    const std::string& host,
    const std::string& id,
    const std::string& secret) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());

  [client_ commitPairingCredentialsForHost:SysUTF8ToNSString(host)
                                        id:SysUTF8ToNSString(id)
                                    secret:SysUTF8ToNSString(secret)];
}

void RemotingClientSessionDelegate::FetchSecret(
    bool pairing_supported,
    const protocol::SecretFetchedCallback& secret_fetched_callback) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());

  [client_ fetchSecretWithPairingSupported:pairing_supported
                                  callback:secret_fetched_callback];
}

void RemotingClientSessionDelegate::SetCapabilities(
    const std::string& capabilities) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());

  [client_ setCapabilities:SysUTF8ToNSString(capabilities)];
}

void RemotingClientSessionDelegate::HandleExtensionMessage(
    const std::string& type,
    const std::string& message) {
  DCHECK(runtime_->ui_task_runner()->BelongsToCurrentThread());

  [client_ handleExtensionMessageOfType:SysUTF8ToNSString(type)
                                message:SysUTF8ToNSString(message)];
}

base::WeakPtr<RemotingClientSessionDelegate>
RemotingClientSessionDelegate::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace remoting
