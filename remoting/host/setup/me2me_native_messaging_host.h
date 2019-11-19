// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_ME2ME_NATIVE_MESSAGING_HOST_H_
#define REMOTING_HOST_SETUP_ME2ME_NATIVE_MESSAGING_HOST_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "remoting/base/oauth_client.h"
#include "remoting/host/setup/daemon_controller.h"

namespace base {
class DictionaryValue;
class ListValue;
class SingleThreadTaskRunner;
class Value;
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
  void ProcessHello(std::unique_ptr<base::DictionaryValue> message,
                    std::unique_ptr<base::DictionaryValue> response);
  void ProcessClearPairedClients(
      std::unique_ptr<base::DictionaryValue> message,
      std::unique_ptr<base::DictionaryValue> response);
  void ProcessDeletePairedClient(
      std::unique_ptr<base::DictionaryValue> message,
      std::unique_ptr<base::DictionaryValue> response);
  void ProcessGetHostName(std::unique_ptr<base::DictionaryValue> message,
                          std::unique_ptr<base::DictionaryValue> response);
  void ProcessGetPinHash(std::unique_ptr<base::DictionaryValue> message,
                         std::unique_ptr<base::DictionaryValue> response);
  void ProcessGenerateKeyPair(std::unique_ptr<base::DictionaryValue> message,
                              std::unique_ptr<base::DictionaryValue> response);
  void ProcessUpdateDaemonConfig(
      std::unique_ptr<base::DictionaryValue> message,
      std::unique_ptr<base::DictionaryValue> response);
  void ProcessGetDaemonConfig(std::unique_ptr<base::DictionaryValue> message,
                              std::unique_ptr<base::DictionaryValue> response);
  void ProcessGetPairedClients(std::unique_ptr<base::DictionaryValue> message,
                               std::unique_ptr<base::DictionaryValue> response);
  void ProcessGetUsageStatsConsent(
      std::unique_ptr<base::DictionaryValue> message,
      std::unique_ptr<base::DictionaryValue> response);
  void ProcessStartDaemon(std::unique_ptr<base::DictionaryValue> message,
                          std::unique_ptr<base::DictionaryValue> response);
  void ProcessStopDaemon(std::unique_ptr<base::DictionaryValue> message,
                         std::unique_ptr<base::DictionaryValue> response);
  void ProcessGetDaemonState(std::unique_ptr<base::DictionaryValue> message,
                             std::unique_ptr<base::DictionaryValue> response);
  void ProcessGetHostClientId(std::unique_ptr<base::DictionaryValue> message,
                              std::unique_ptr<base::DictionaryValue> response);
  void ProcessGetCredentialsFromAuthCode(
      std::unique_ptr<base::DictionaryValue> message,
      std::unique_ptr<base::DictionaryValue> response,
      bool need_user_email);
  void ProcessIt2mePermissionCheck(
      std::unique_ptr<base::DictionaryValue> message,
      std::unique_ptr<base::DictionaryValue> response);

  // These Send... methods get called on the DaemonController's internal thread,
  // or on the calling thread if called by the PairingRegistry.
  // These methods fill in the |response| dictionary from the other parameters,
  // and pass it to SendResponse().
  void SendConfigResponse(std::unique_ptr<base::DictionaryValue> response,
                          std::unique_ptr<base::DictionaryValue> config);
  void SendPairedClientsResponse(
      std::unique_ptr<base::DictionaryValue> response,
      std::unique_ptr<base::ListValue> pairings);
  void SendUsageStatsConsentResponse(
      std::unique_ptr<base::DictionaryValue> response,
      const DaemonController::UsageStatsConsent& consent);
  void SendAsyncResult(std::unique_ptr<base::DictionaryValue> response,
                       DaemonController::AsyncResult result);
  void SendBooleanResult(std::unique_ptr<base::DictionaryValue> response,
                         bool result);
  void SendCredentialsResponse(std::unique_ptr<base::DictionaryValue> response,
                               const std::string& user_email,
                               const std::string& refresh_token);
  void SendMessageToClient(std::unique_ptr<base::Value> message) const;

  void OnError(const std::string& error_message);

  // Returns whether the request was successfully sent to the elevated host.
  DelegationResult DelegateToElevatedHost(
      std::unique_ptr<base::DictionaryValue> message);

  bool needs_elevation_;

#if defined(OS_WIN)
  // Controls the lifetime of the elevated native messaging host process.
  std::unique_ptr<ElevatedNativeMessagingHost> elevated_host_;

  // Handle of the parent window.
  intptr_t parent_window_handle_;
#endif  // defined(OS_WIN)

  extensions::NativeMessageHost::Client* client_;
  std::unique_ptr<ChromotingHostContext> host_context_;

  std::unique_ptr<LogMessageHandler> log_message_handler_;

  scoped_refptr<DaemonController> daemon_controller_;

  // Used to load and update the paired clients for this host.
  scoped_refptr<protocol::PairingRegistry> pairing_registry_;

  // Used to exchange the service account authorization code for credentials.
  std::unique_ptr<OAuthClient> oauth_client_;

  base::WeakPtr<Me2MeNativeMessagingHost> weak_ptr_;
  base::WeakPtrFactory<Me2MeNativeMessagingHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Me2MeNativeMessagingHost);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_ME2ME_NATIVE_MESSAGING_HOST_H_
