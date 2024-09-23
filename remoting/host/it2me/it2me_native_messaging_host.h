// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IT2ME_IT2ME_NATIVE_MESSAGING_HOST_H_
#define REMOTING_HOST_IT2ME_IT2ME_NATIVE_MESSAGING_HOST_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "remoting/host/it2me/it2me_host.h"
#include "remoting/protocol/errors.h"
#include "remoting/signaling/delegating_signal_strategy.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "remoting/host/native_messaging/log_message_handler.h"
#endif

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class ChromotingHostContext;
class ElevatedNativeMessagingHost;
class PolicyWatcher;

// Implementation of the native messaging host process.
class It2MeNativeMessagingHost : public It2MeHost::Observer,
                                 public extensions::NativeMessageHost {
 public:
  It2MeNativeMessagingHost(bool is_process_elevated,
                           std::unique_ptr<PolicyWatcher> policy_watcher,
                           std::unique_ptr<ChromotingHostContext> host_context,
                           std::unique_ptr<It2MeHostFactory> host_factory);

  It2MeNativeMessagingHost(const It2MeNativeMessagingHost&) = delete;
  It2MeNativeMessagingHost& operator=(const It2MeNativeMessagingHost&) = delete;

  ~It2MeNativeMessagingHost() override;

  // extensions::NativeMessageHost implementation.
  void OnMessage(const std::string& message) override;
  void Start(Client* client) override;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override;

  // It2MeHost::Observer implementation.
  void OnClientAuthenticated(const std::string& client_username) override;
  void OnStoreAccessCode(const std::string& access_code,
                         base::TimeDelta access_code_lifetime) override;
  void OnNatPoliciesChanged(bool nat_traversal_enabled,
                            bool relay_connections_allowed) override;
  void OnStateChanged(It2MeHostState state,
                      protocol::ErrorCode error_code) override;

  // Set a callback to be called when a policy error notification has been
  // processed.
  void SetPolicyErrorClosureForTesting(base::OnceClosure closure);

 private:
  // These "Process.." methods handle specific request types. The |response|
  // dictionary is pre-filled by ProcessMessage() with the parts of the
  // response already known ("id" and "type" fields).
  void ProcessHello(base::Value::Dict message,
                    base::Value::Dict response) const;
  void ProcessConnect(base::Value::Dict message, base::Value::Dict response);
  void ProcessDisconnect(base::Value::Dict message, base::Value::Dict response);
  void ProcessIncomingIq(base::Value::Dict message, base::Value::Dict response);
  void SendErrorAndExit(base::Value::Dict response,
                        const protocol::ErrorCode error_code) const;
  void SendPolicyErrorAndExit() const;
  void SendMessageToClient(base::Value::Dict message) const;

  // Callback for DelegatingSignalStrategy.
  void SendOutgoingIq(const std::string& iq);

  // Called when initial policies are read and when they change.
  void OnPolicyUpdate(base::Value::Dict policies);

  // Called when malformed policies are detected.
  void OnPolicyError();

  // Returns whether the request was successfully sent to the elevated host.
  bool DelegateToElevatedHost(base::Value::Dict message);

  // Creates a delegated signal strategy from the values stored in |message|.
  // Returns nullptr on failure.
  std::unique_ptr<SignalStrategy> CreateDelegatedSignalStrategy(
      const base::Value::Dict& message);

  // Extracts OAuth access token from the message passed from the client.
  std::string ExtractAccessToken(const base::Value::Dict& message);

  // Returns the value of the 'allow_elevated_host' platform policy or empty.
  std::optional<bool> GetAllowElevatedHostPolicyValue();

  // Indicates whether the current process is already elevated.
  bool is_process_elevated_ = false;

  // Forward messages to an |elevated_host_|.
  bool use_elevated_host_ = false;

#if BUILDFLAG(IS_WIN)
  // Controls the lifetime of the elevated native messaging host process.
  // Note: 'elevated' in this instance means having the UiAccess privilege, not
  // being run as a higher privilege user.
  std::unique_ptr<ElevatedNativeMessagingHost> elevated_host_;
#endif  // BUILDFLAG(IS_WIN)

  raw_ptr<Client> client_ = nullptr;
  DelegatingSignalStrategy::IqCallback incoming_message_callback_;
  std::unique_ptr<ChromotingHostContext> host_context_;
  std::unique_ptr<It2MeHostFactory> factory_;
  scoped_refptr<It2MeHost> it2me_host_;

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Don't install a log message handler on ChromeOS because we run in the
  // browser process and don't want to intercept all its log messages.
  std::unique_ptr<LogMessageHandler> log_message_handler_;
#endif

  // Cached, read-only copies of |it2me_host_| session state.
  It2MeHostState state_ = It2MeHostState::kDisconnected;
  std::string access_code_;
  base::TimeDelta access_code_lifetime_;
  std::string client_username_;

  // Indicates whether or not a policy has ever been read. This is to ensure
  // that on startup, we do not accidentally start a connection before we have
  // queried our policy restrictions.
  bool policy_received_ = false;

  // Used to retrieve Chrome policies set for the local machine.
  std::unique_ptr<PolicyWatcher> policy_watcher_;

  // On startup, it is possible to have Connect() called before the policy read
  // is completed.  Rather than just failing, we thunk the connection call so
  // it can be executed after at least one successful policy read. This
  // variable contains the thunk if it is necessary.
  base::OnceClosure pending_connect_;

  base::OnceClosure policy_error_closure_for_testing_;

  base::WeakPtr<It2MeNativeMessagingHost> weak_ptr_;
  base::WeakPtrFactory<It2MeNativeMessagingHost> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_IT2ME_NATIVE_MESSAGING_HOST_H_
