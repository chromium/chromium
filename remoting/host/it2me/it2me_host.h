// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IT2ME_IT2ME_HOST_H_
#define REMOTING_HOST_IT2ME_IT2ME_HOST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/it2me/it2me_confirmation_dialog.h"
#include "remoting/host/it2me/it2me_confirmation_dialog_proxy.h"
#include "remoting/host/register_support_host_request.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/port_range.h"
#include "remoting/protocol/validating_authenticator.h"
#include "remoting/signaling/signal_strategy.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace remoting {

class ChromotingHost;
class ChromotingHostContext;
class DesktopEnvironmentFactory;
class HostEventLogger;
class HostStatusLogger;
class LogToServer;
class RegisterSupportHostRequest;
class RsaKeyPair;

namespace protocol {
struct IceConfig;
}  // namespace protocol

// These state values are duplicated in host_session.js.  Remember to update
// both copies when making changes.
enum It2MeHostState {
  kDisconnected,
  kStarting,
  kRequestedAccessCode,
  kReceivedAccessCode,
  kConnecting,
  kConnected,
  kError,
  kInvalidDomainError,
};

// Internal implementation of the plugin's It2Me host function.
class It2MeHost : public base::RefCountedThreadSafe<It2MeHost>,
                  public HostStatusObserver {
 public:
  class Observer {
   public:
    virtual void OnClientAuthenticated(const std::string& client_username) = 0;
    virtual void OnStoreAccessCode(const std::string& access_code,
                                   base::TimeDelta access_code_lifetime) = 0;
    virtual void OnNatPolicyChanged(bool nat_traversal_enabled) = 0;
    virtual void OnStateChanged(It2MeHostState state,
                                protocol::ErrorCode error_code) = 0;
  };

  It2MeHost();

  // Enable, disable, or query whether or not the confirm, continue, and
  // disconnect dialogs are shown.
  void set_enable_dialogs(bool enable);
  bool enable_dialogs() const { return enable_dialogs_; }

  // Enable or disable whether or not the session should be terminated if local
  // input is detected.
  void set_terminate_upon_input(bool terminate_upon_input);

  // Methods called by the script object, from the plugin thread.

  // Creates It2Me host structures and starts the host.
  //
  // XmppLogToServer cannot be created and used in different sequence, so pass
  // in a factory callback instead.
  virtual void Connect(
      std::unique_ptr<ChromotingHostContext> context,
      std::unique_ptr<base::DictionaryValue> policies,
      std::unique_ptr<It2MeConfirmationDialogFactory> dialog_factory,
      std::unique_ptr<RegisterSupportHostRequest> register_request,
      std::unique_ptr<LogToServer> log_to_server,
      base::WeakPtr<It2MeHost::Observer> observer,
      std::unique_ptr<SignalStrategy> signal_strategy,
      const std::string& username,
      const protocol::IceConfig& ice_config);

  // Disconnects and shuts down the host.
  virtual void Disconnect();

  // remoting::HostStatusObserver implementation.
  void OnAccessDenied(const std::string& jid) override;
  void OnClientConnected(const std::string& jid) override;
  void OnClientDisconnected(const std::string& jid) override;

  void SetStateForTesting(It2MeHostState state,
                          protocol::ErrorCode error_code) {
    SetState(state, error_code);
  }

  // Returns the callback used for validating the connection.  Do not run the
  // returned callback after this object has been destroyed.
  protocol::ValidatingAuthenticator::ValidationCallback
  GetValidationCallbackForTesting();

  // Called when initial policies are read and when they change.
  void OnPolicyUpdate(std::unique_ptr<base::DictionaryValue> policies);

 protected:
  friend class base::RefCountedThreadSafe<It2MeHost>;

  ~It2MeHost() override;

  ChromotingHostContext* host_context() { return host_context_.get(); }
  base::WeakPtr<It2MeHost::Observer> observer() { return observer_; }

 private:
  friend class MockIt2MeHost;
  friend class It2MeHostTest;

  // Updates state of the host. Can be called only on the network thread.
  void SetState(It2MeHostState state, protocol::ErrorCode error_code);

  // Returns true if the host is in a post-starting, non-error state.
  bool IsRunning() const;

  // Processes the result of the confirmation dialog.
  void OnConfirmationResult(
      const protocol::ValidatingAuthenticator::ResultCallback& result_callback,
      It2MeConfirmationDialog::Result result);

  // Task posted to the network thread from Connect().
  void ConnectOnNetworkThread(
      const std::string& username,
      const protocol::IceConfig& ice_config,
      std::unique_ptr<RegisterSupportHostRequest> register_request);

  // Called when the support host registration completes.
  void OnReceivedSupportID(const std::string& support_id,
                           const base::TimeDelta& lifetime,
                           protocol::ErrorCode error_code);

  // Handlers for NAT traversal and domain policies.
  void UpdateNatPolicy(bool nat_traversal_enabled);
  void UpdateHostDomainListPolicy(std::vector<std::string> host_domain_list);
  void UpdateClientDomainListPolicy(
      std::vector<std::string> client_domain_list);
  void UpdateHostUdpPortRangePolicy(const std::string& port_range_string);

  void DisconnectOnNetworkThread();

  // Uses details of the connection and current policies to determine if the
  // connection should be accepted or rejected.
  void ValidateConnectionDetails(
      const std::string& remote_jid,
      const protocol::ValidatingAuthenticator::ResultCallback& result_callback);

  // Caller supplied fields.
  std::unique_ptr<ChromotingHostContext> host_context_;
  base::WeakPtr<It2MeHost::Observer> observer_;
  std::unique_ptr<SignalStrategy> signal_strategy_;
  std::unique_ptr<LogToServer> log_to_server_;

  It2MeHostState state_ = kDisconnected;

  scoped_refptr<RsaKeyPair> host_key_pair_;
  std::unique_ptr<RegisterSupportHostRequest> register_request_;
  std::unique_ptr<HostStatusLogger> host_status_logger_;
  std::unique_ptr<DesktopEnvironmentFactory> desktop_environment_factory_;
  std::unique_ptr<HostEventLogger> host_event_logger_;

  std::unique_ptr<ChromotingHost> host_;
  int failed_login_attempts_ = 0;

  std::unique_ptr<It2MeConfirmationDialogFactory> confirmation_dialog_factory_;
  std::unique_ptr<It2MeConfirmationDialogProxy> confirmation_dialog_proxy_;

  // Host the current nat traversal policy setting.
  bool nat_traversal_enabled_ = false;

  // The client and host domain policy setting.
  std::vector<std::string> required_client_domain_list_;
  std::vector<std::string> required_host_domain_list_;

  // The host port range policy setting.
  PortRange udp_port_range_;

  // Tracks the JID of the remote user when in a connecting state.
  std::string connecting_jid_;

  bool enable_dialogs_ = true;
  bool terminate_upon_input_ = false;

  DISALLOW_COPY_AND_ASSIGN(It2MeHost);
};

// Having a factory interface makes it possible for the test to provide a mock
// implementation of the It2MeHost.
class It2MeHostFactory {
 public:
  It2MeHostFactory();
  virtual ~It2MeHostFactory();

  virtual scoped_refptr<It2MeHost> CreateIt2MeHost();

 private:
  DISALLOW_COPY_AND_ASSIGN(It2MeHostFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_IT2ME_HOST_H_
