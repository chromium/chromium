// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_ME2ME_NATIVE_MESSAGING_HOST_H_
#define REMOTING_HOST_SETUP_ME2ME_NATIVE_MESSAGING_HOST_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "build/build_config.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "remoting/base/oauth_client.h"
#include "remoting/host/setup/daemon_controller.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

namespace protocol {
class PairingRegistry;
}  // namespace protocol

class ChromotingHostContext;
class ElevatedNativeMessagingHost;
class LogMessageHandler;

// Implementation of the me2me native messaging host.
class Me2MeNativeMessagingHost : public extensions::NativeMessageHost {
 public:
  Me2MeNativeMessagingHost(
      bool needs_elevation,
      intptr_t parent_window_handle,
      std::unique_ptr<ChromotingHostContext> host_context,
      scoped_refptr<DaemonController> daemon_controller,
      scoped_refptr<protocol::PairingRegistry> pairing_registry,
      std::unique_ptr<OAuthClient> oauth_client);

  Me2MeNativeMessagingHost(const Me2MeNativeMessagingHost&) = delete;
  Me2MeNativeMessagingHost& operator=(const Me2MeNativeMessagingHost&) = delete;

  ~Me2MeNativeMessagingHost() override;

  // extensions::NativeMessageHost implementation.
  void OnMessage(const std::string& message) override;
  void Start(extensions::NativeMessageHost::Client* client) override;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override;

 private:
  enum DelegationResult {
    DELEGATION_SUCCESS,
    DELEGATION_CANCELLED,
    DELEGATION_FAILED,
  };

  // These "Process.." methods handle specific request types. The |response|
  // dictionary is pre-filled by ProcessMessage() with the parts of the
  // response already known ("id" and "type" fields).
  void ProcessHello(base::Value::Dict message, base::Value::Dict response);
  void ProcessClearPairedClients(base::Value::Dict message,
                                 base::Value::Dict response);
  void ProcessDeletePairedClient(base::Value::Dict message,
                                 base::Value::Dict response);
  void ProcessGetHostName(base::Value::Dict message,
                          base::Value::Dict response);
  void ProcessGetPinHash(base::Value::Dict message, base::Value::Dict response);
  void ProcessGenerateKeyPair(base::Value::Dict message,
                              base::Value::Dict response);
  void ProcessUpdateDaemonConfig(base::Value::Dict message,
                                 base::Value::Dict response);
  void ProcessGetDaemonConfig(base::Value::Dict message,
                              base::Value::Dict response);
  void ProcessGetPairedClients(base::Value::Dict message,
                               base::Value::Dict response);
  void ProcessGetUsageStatsConsent(base::Value::Dict message,
                                   base::Value::Dict response);
  void ProcessStartDaemon(base::Value::Dict message,
                          base::Value::Dict response);
  void ProcessStopDaemon(base::Value::Dict message, base::Value::Dict response);
  void ProcessGetDaemonState(base::Value::Dict message,
                             base::Value::Dict response);
  void ProcessGetHostClientId(base::Value::Dict message,
                              base::Value::Dict response);
  void ProcessGetCredentialsFromAuthCode(base::Value::Dict message,
                                         base::Value::Dict response,
                                         bool need_user_email);
  void ProcessIt2mePermissionCheck(base::Value::Dict message,
                                   base::Value::Dict response);

  // These Send... methods get called on the DaemonController's internal thread,
  // or on the calling thread if called by the PairingRegistry.
  // These methods fill in the |response| dictionary from the other parameters,
  // and pass it to SendResponse().
  void SendConfigResponse(base::Value::Dict response,
                          std::optional<base::Value::Dict> config);
  void SendPairedClientsResponse(base::Value::Dict response,
                                 base::Value::List pairings);
  void SendUsageStatsConsentResponse(
      base::Value::Dict response,
      const DaemonController::UsageStatsConsent& consent);
  void SendAsyncResult(base::Value::Dict response,
                       DaemonController::AsyncResult result);
  void SendBooleanResult(base::Value::Dict response, bool result);
  void SendCredentialsResponse(base::Value::Dict response,
                               const std::string& user_email,
                               const std::string& refresh_token);
  void SendMessageToClient(base::Value::Dict message) const;

  void OnError(const std::string& error_message);

  // Returns whether the request was successfully sent to the elevated host.
  DelegationResult DelegateToElevatedHost(base::Value::Dict message);

  bool needs_elevation_;

#if BUILDFLAG(IS_WIN)
  // Controls the lifetime of the elevated native messaging host process.
  std::unique_ptr<ElevatedNativeMessagingHost> elevated_host_;

  // Handle of the parent window.
  intptr_t parent_window_handle_;
#endif  // BUILDFLAG(IS_WIN)

  raw_ptr<extensions::NativeMessageHost::Client> client_;
  std::unique_ptr<ChromotingHostContext> host_context_;

  std::unique_ptr<LogMessageHandler> log_message_handler_;

  scoped_refptr<DaemonController> daemon_controller_;

  // Used to load and update the paired clients for this host.
  scoped_refptr<protocol::PairingRegistry> pairing_registry_;

  // Used to exchange the service account authorization code for credentials.
  std::unique_ptr<OAuthClient> oauth_client_;

  base::WeakPtr<Me2MeNativeMessagingHost> weak_ptr_;
  base::WeakPtrFactory<Me2MeNativeMessagingHost> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_ME2ME_NATIVE_MESSAGING_HOST_H_
