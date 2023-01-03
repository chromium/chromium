// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IT2ME_IT2ME_HOST_H_
#define REMOTING_HOST_IT2ME_IT2ME_HOST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/it2me/it2me_confirmation_dialog.h"
#include "remoting/host/it2me/it2me_confirmation_dialog_proxy.h"
#include "remoting/host/it2me/it2me_constants.h"
#include "remoting/host/register_support_host_request.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/port_range.h"
#include "remoting/protocol/validating_authenticator.h"
#include "remoting/signaling/signal_strategy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace remoting {

class ChromotingHost;
class ChromotingHostContext;
class DesktopEnvironmentFactory;
class FtlSignalingConnector;
class HostEventLogger;
class HostEventReporter;
class HostStatusLogger;
class LogToServer;
class OAuthTokenGetter;
class RegisterSupportHostRequest;
class RsaKeyPair;

namespace protocol {
struct IceConfig;
}  // namespace protocol

// Internal implementation of the plugin's It2Me host function.
class It2MeHost : public base::RefCountedThreadSafe<It2MeHost>,
                  public HostStatusObserver {
 public:
  struct DeferredConnectContext {
    DeferredConnectContext();
    ~DeferredConnectContext();

    std::unique_ptr<LogToServer> log_to_server;
    std::unique_ptr<RegisterSupportHostRequest> register_request;
    std::unique_ptr<SignalStrategy> signal_strategy;
    std::unique_ptr<OAuthTokenGetter> oauth_token_getter;

    // Since the deferred context only provides an interface* for the signal
    // strategy, we use this boolean to indicate whether the host process should
    // own things like reconnecting signaling if there is a transient network
    // error.
    // TODO(joedow): Remove this field once delegated signaling has been
    // deprecated and removed.
    bool use_ftl_signaling = false;
  };

  using CreateDeferredConnectContext =
      base::OnceCallback<std::unique_ptr<DeferredConnectContext>(
          ChromotingHostContext*)>;

  class Observer {
   public:
    virtual void OnClientAuthenticated(const std::string& client_username) = 0;
    virtual void OnStoreAccessCode(const std::string& access_code,
                                   base::TimeDelta access_code_lifetime) = 0;
    virtual void OnNatPoliciesChanged(bool nat_traversal_enabled,
                                      bool relay_connections_allowed) = 0;
    virtual void OnStateChanged(It2MeHostState state,
                                protocol::ErrorCode error_code) = 0;
  };

  It2MeHost();

  It2MeHost(const It2MeHost&) = delete;
  It2MeHost& operator=(const It2MeHost&) = delete;

  // Enable, disable, or query whether or not the confirm, continue, and
  // disconnect dialogs are shown.
  void set_enable_dialogs(bool enable);
  bool enable_dialogs() const { return enable_dialogs_; }

  // Enable, disable, or query whether or not connection notifications are
  // shown when a remote user has connected.
  void set_enable_notifications(bool enable);
  bool enable_notifications() const { return enable_notifications_; }

  // Enable or disable whether or not the session should be terminated if local
  // input is detected.
  void set_terminate_upon_input(bool terminate_upon_input);
  bool terminate_upon_input() const { return terminate_upon_input_; }

  // Enable, disable, or query whether or not the local user session is
  // curtained when a remote user has connected.
  void set_enable_curtaining(bool enable);
  bool enable_curtaining() const { return enable_curtaining_; }

  // Indicates whether the session was initiated through the remote command
  // infrastructure for a managed device.
  void set_is_enterprise_session(bool is_enterprise_session);
  bool is_enterprise_session() const { return is_enterprise_session_; }

  // If set, only |authorized_helper| will be allowed to connect to this host.
  void set_authorized_helper(const std::string& authorized_helper);
  const std::string& authorized_helper() const { return authorized_helper_; }

  // Creates It2Me host structures and starts the host.
  virtual void Connect(
      std::unique_ptr<ChromotingHostContext> context,
      base::Value::Dict policies,
      std::unique_ptr<It2MeConfirmationDialogFactory> dialog_factory,
      base::WeakPtr<It2MeHost::Observer> observer,
      CreateDeferredConnectContext create_context,
      const std::string& username,
      const protocol::IceConfig& ice_config);

  // Disconnects and shuts down the host.
  virtual void Disconnect();

  // HostStatusObserver implementation.
  void OnClientAccessDenied(const std::string& signaling_id) override;
  void OnClientConnected(const std::string& signaling_id) override;
  void OnClientDisconnected(const std::string& signaling_id) override;

  void SetStateForTesting(It2MeHostState state,
                          protocol::ErrorCode error_code) {
    SetState(state, error_code);
  }

  // Returns the callback used for validating the connection.  Do not run the
  // returned callback after this object has been destroyed.
  protocol::ValidatingAuthenticator::ValidationCallback
  GetValidationCallbackForTesting();

  // Called when initial policies are read and when they change.
  void OnPolicyUpdate(base::Value::Dict policies);

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
      protocol::ValidatingAuthenticator::ResultCallback result_callback,
      It2MeConfirmationDialog::Result result);

  // Task posted to the network thread from Connect().
  void ConnectOnNetworkThread(const std::string& username,
                              const protocol::IceConfig& ice_config,
                              CreateDeferredConnectContext create_context);

  // Called when the support host registration completes.
  void OnReceivedSupportID(const std::string& support_id,
                           const base::TimeDelta& lifetime,
                           protocol::ErrorCode error_code);

  // Handlers for NAT traversal and domain policies.
  void UpdateNatPolicies(bool nat_policy_value, bool relay_policy_value);
  void UpdateHostDomainListPolicy(std::vector<std::string> host_domain_list);
  void UpdateClientDomainListPolicy(
      std::vector<std::string> client_domain_list);
  void UpdateHostUdpPortRangePolicy(const std::string& port_range_string);

  void DisconnectOnNetworkThread(
      protocol::ErrorCode error_code = protocol::ErrorCode::OK);

  // Uses details of the connection and current policies to determine if the
  // connection should be accepted or rejected.
  void ValidateConnectionDetails(
      const std::string& remote_jid,
      protocol::ValidatingAuthenticator::ResultCallback result_callback);

  // Caller supplied fields.
  std::unique_ptr<ChromotingHostContext> host_context_;
  base::WeakPtr<It2MeHost::Observer> observer_;
  std::unique_ptr<SignalStrategy> signal_strategy_;
  std::unique_ptr<FtlSignalingConnector> ftl_signaling_connector_;
  std::unique_ptr<LogToServer> log_to_server_;
  std::unique_ptr<OAuthTokenGetter> oauth_token_getter_;

  It2MeHostState state_ = It2MeHostState::kDisconnected;

  scoped_refptr<RsaKeyPair> host_key_pair_;
  std::unique_ptr<RegisterSupportHostRequest> register_request_;
  std::unique_ptr<HostStatusLogger> host_status_logger_;
  std::unique_ptr<DesktopEnvironmentFactory> desktop_environment_factory_;
  std::unique_ptr<HostEventLogger> host_event_logger_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<HostEventReporter> host_event_reporter_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  std::unique_ptr<ChromotingHost> host_;
  int failed_login_attempts_ = 0;

  std::unique_ptr<It2MeConfirmationDialogFactory> confirmation_dialog_factory_;
  std::unique_ptr<It2MeConfirmationDialogProxy> confirmation_dialog_proxy_;

  // Stores the current nat traversal policy value.
  bool nat_traversal_enabled_ = false;

  // Stores the current relay connections allowed policy value.
  bool relay_connections_allowed_ = false;

  // Indicates whether the session was initiated via the RemoteCommand infra.
  // This is by administrators to connect to managed enterprise devices.
  bool is_enterprise_session_ = false;

  // Only the username stored in |authorized_helper_| will be allowed to connect
  // to this host instance, if set. Note: setting this value does not override
  // any applicable Enterprise policies or other constraints.
  std::string authorized_helper_;

  // The client and host domain policy setting.
  std::vector<std::string> required_client_domain_list_;
  std::vector<std::string> required_host_domain_list_;

  // The host port range policy setting.
  PortRange udp_port_range_;

  // Stores the clipboard size policy value.
  absl::optional<size_t> max_clipboard_size_;

  // Stores the remote support connections allowed policy value.
  bool remote_support_connections_allowed_ = true;

  // Tracks the JID of the remote user when in a connecting state.
  std::string connecting_jid_;

  bool enable_dialogs_ = true;
  bool enable_notifications_ = true;
  bool terminate_upon_input_ = false;
  bool enable_curtaining_ = false;
};

// Having a factory interface makes it possible for the test to provide a mock
// implementation of the It2MeHost.
class It2MeHostFactory {
 public:
  It2MeHostFactory();

  It2MeHostFactory(const It2MeHostFactory&) = delete;
  It2MeHostFactory& operator=(const It2MeHostFactory&) = delete;

  virtual ~It2MeHostFactory();

  virtual scoped_refptr<It2MeHost> CreateIt2MeHost();
};

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_IT2ME_HOST_H_
