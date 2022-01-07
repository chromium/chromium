// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_NATIVE_MESSAGING_HOST_H_
#define REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_NATIVE_MESSAGING_HOST_H_

#include <memory>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/chromoting_host_services_client.h"
#include "remoting/host/mojom/webauthn_proxy.mojom-forward.h"
#include "remoting/host/mojom/webauthn_proxy.mojom.h"

namespace remoting {

// Native messaging host for handling remote authentication requests and sending
// them to the remoting host process via mojo.
class RemoteWebAuthnNativeMessagingHost final
    : public extensions::NativeMessageHost {
 public:
  explicit RemoteWebAuthnNativeMessagingHost(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  RemoteWebAuthnNativeMessagingHost(const RemoteWebAuthnNativeMessagingHost&) =
      delete;
  RemoteWebAuthnNativeMessagingHost& operator=(
      const RemoteWebAuthnNativeMessagingHost&) = delete;
  ~RemoteWebAuthnNativeMessagingHost() override;

  void OnMessage(const std::string& message) override;
  void Start(extensions::NativeMessageHost::Client* client) override;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override;

 private:
  void ProcessHello(base::Value response);
  void ProcessGetRemoteState(base::Value response);
  void ProcessIsUvpaa(const base::Value& request, base::Value response);
  void ProcessCreate(const base::Value& request, base::Value response);

  void OnQueryVersionResult(uint32_t version);
  void OnIpcDisconnected();
  void OnIsUvpaaResponse(base::Value response, bool is_available);
  void OnCreateResponse(base::Value response,
                        mojom::WebAuthnCreateResponsePtr remote_response);

  void QueryNextRemoteState();
  void SendNextRemoteState(bool is_remoted);

  // Attempts to connect to the IPC server if the connection has not been
  // established. Returns a boolean indicating whether there is a valid IPC
  // connection to the crd host
  bool EnsureIpcConnection();
  void SendMessageToClient(base::Value message);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  raw_ptr<extensions::NativeMessageHost::Client> client_ = nullptr;
  ChromotingHostServicesClient host_service_api_client_;
  mojo::Remote<mojom::WebAuthnProxy> remote_;

  // Pending getRemoteStateResponses to be sent.
  base::queue<base::Value> get_remote_state_responses_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_NATIVE_MESSAGING_HOST_H_
