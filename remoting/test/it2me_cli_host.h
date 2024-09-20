// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_IT2ME_CLI_HOST_H_
#define REMOTING_TEST_IT2ME_CLI_HOST_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "remoting/base/oauth_token_getter.h"

namespace remoting {

namespace test {
class TestOAuthTokenGetter;
class TestTokenStorage;
}  // namespace test

class AutoThreadTaskRunner;

class It2MeCliHost final : public extensions::NativeMessageHost::Client {
 public:
  static bool ShouldPrintHelp();
  static void PrintHelp();

  It2MeCliHost();

  It2MeCliHost(const It2MeCliHost&) = delete;
  It2MeCliHost& operator=(const It2MeCliHost&) = delete;

  ~It2MeCliHost() override;

  void Start();

 private:
  // extensions::NativeMessageHost::Client:
  // Invoked when native host sends a message
  void PostMessageFromNativeHost(const std::string& message) override;
  void CloseChannel(const std::string& error_message) override;

  // Sends message to host in separate task.
  void SendMessageToHost(const std::string& type, base::Value::Dict params);
  // Actually sends message to host.
  void DoSendMessage(const std::string& json);
  void OnProtocolBroken(const std::string& message);

  void StartCRDHostAndGetCode(OAuthTokenGetter::Status status,
                              const std::string& user_email,
                              const std::string& access_token,
                              const std::string& scopes);

  // Shuts down host in a separate task.
  void ShutdownHost();
  // Actually shuts down a host.
  void DoShutdownHost();

  // Handlers for messages from host
  void OnHelloResponse();
  void OnDisconnectResponse();

  void OnStateError(const std::string& error_state,
                    const base::Value::Dict& message);
  void OnStateRemoteConnected(const base::Value::Dict& message);
  void OnStateRemoteDisconnected();
  void OnStateReceivedAccessCode(const base::Value::Dict& message);

  std::unique_ptr<test::TestTokenStorage> storage_;
  std::unique_ptr<test::TestOAuthTokenGetter> token_getter_;
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner_;
  std::unique_ptr<extensions::NativeMessageHost> host_;

  // Filled structure with parameters for "connect" message.
  base::Value::Dict connect_params_;

  // Determines actions when receiving messages from CRD host,
  // if command is still running (no error / access code), then
  // callbacks have to be called.
  bool command_awaiting_crd_access_code_;
  // True if remote session was established.
  bool remote_connected_;

  base::WeakPtrFactory<It2MeCliHost> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_TEST_IT2ME_CLI_HOST_H_
