// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CLIENT_SESSION_H_
#define REMOTING_HOST_CLIENT_SESSION_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/base/errors.h"
#include "remoting/base/local_session_policies_provider.h"
#include "remoting/base/session_policies.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/host_experiment_session_plugin.h"
#include "remoting/host/peer_session.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/pairing_registry.h"
#include "remoting/protocol/session.h"

namespace remoting {

class DesktopEnvironmentFactory;
class HostExtension;

namespace protocol {
class IceConfigFetcher;
}  // namespace protocol

// A ClientSession keeps a reference to a connection to a client, and maintains
// per-client state.
class ClientSession : public protocol::Session::EventHandler,
                      public PeerSession::EventHandler {
 public:
  // Callback interface for passing events to the ChromotingHost.
  class EventHandler {
   public:
    // Called after authentication has started.
    virtual void OnSessionAuthenticating(ClientSession* client) = 0;

    // Called after authentication has finished successfully.
    virtual void OnSessionAuthenticated(ClientSession* client) = 0;

    // Called after we've finished connecting all channels.
    virtual void OnSessionChannelsConnected(ClientSession* client) = 0;

    // Called after authentication has failed. Must not tear down this
    // object. OnSessionClosed() is notified after this handler
    // returns.
    virtual void OnSessionAuthenticationFailed(ClientSession* client) = 0;

    // Called after connection has failed or after the client closed it.
    virtual void OnSessionClosed(ClientSession* client) = 0;

    // Called on notification of a route change event, when a channel is
    // connected.
    virtual void OnSessionRouteChange(
        ClientSession* client,
        const std::string& channel_name,
        const protocol::TransportRoute& route) = 0;

    // Called when session policies are received. Returns nullopt if the session
    // policies are valid; otherwise returns an error code, which will be used
    // to close the session with.
    virtual std::optional<ErrorCode> OnSessionPoliciesReceived(
        const SessionPolicies& policies) = 0;

   protected:
    virtual ~EventHandler() {}
  };

  // `event_handler` and `desktop_environment_factory` must outlive `this`.
  // All `HostExtension`s in `extensions` must outlive `this`.
  ClientSession(
      EventHandler* event_handler,
      std::unique_ptr<protocol::Session> session,
      std::unique_ptr<protocol::IceConfigFetcher> ice_config_fetcher,
      scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
      DesktopEnvironmentFactory* desktop_environment_factory,
      const DesktopEnvironmentOptions& desktop_environment_options,
      scoped_refptr<protocol::PairingRegistry> pairing_registry,
      const std::vector<raw_ptr<HostExtension, VectorExperimental>>& extensions,
      const LocalSessionPoliciesProvider* local_session_policies_provider);

  ClientSession(const ClientSession&) = delete;
  ClientSession& operator=(const ClientSession&) = delete;

  ~ClientSession() override;

  PeerSession* peer_session() const { return peer_session_.get(); }

  // PeerSession::EventHandler interface.
  void OnSessionChannelsConnected() override;
  void OnSessionClosed(protocol::ErrorCode error,
                       std::string_view error_details,
                       const SourceLocation& error_location) override;
  void OnSessionRouteChange(const std::string& channel_name,
                            const protocol::TransportRoute& route) override;

  const std::string& client_jid() const;
  void DisconnectSession(ErrorCode error,
                         std::string_view error_details,
                         const SourceLocation& error_location);

  void OnSessionServicesClientConnected(
      mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver);

  // protocol::Session::EventHandler interface.
  void OnSessionStateChange(protocol::Session::State state) override;

  void set_peer_session_for_testing(std::unique_ptr<PeerSession> peer_session) {
    peer_session_for_tests_ = std::move(peer_session);
  }

  bool is_authenticated() const { return is_authenticated_; }

  bool channels_connected() const { return channels_connected_; }

  const SessionPolicies& effective_policies_for_tests() const {
    return effective_policies_;
  }

 private:
  friend class ClientSessionTest;
  friend class ChromotingHostTest;

  void OnConnectionAuthenticating();
  void OnConnectionAuthenticated(const SessionPolicies* session_policies);

  void OnLocalSessionPoliciesChanged(const SessionPolicies& new_policies);

  raw_ptr<EventHandler> event_handler_;

  // Used to create a DesktopEnvironment instance for this session.
  raw_ptr<DesktopEnvironmentFactory> desktop_environment_factory_;

  // The DesktopEnvironmentOptions used to initialize DesktopEnvironment.
  DesktopEnvironmentOptions desktop_environment_options_;

  // The pairing registry for PIN-less authentication.
  scoped_refptr<protocol::PairingRegistry> pairing_registry_;

  // HostExtensions passed when creating the session.
  std::vector<raw_ptr<HostExtension, VectorExperimental>> extensions_;

  // Set to true if the client was authenticated successfully.
  bool is_authenticated_ = false;

  // Set to true after all data channels have been connected.
  bool channels_connected_ = false;

  std::unique_ptr<protocol::IceConfigFetcher> ice_config_fetcher_;

  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;

  std::unique_ptr<PeerSession> peer_session_for_tests_;

  // The signaling session.
  std::unique_ptr<protocol::Session> session_;

  // Object to control the peer session. It is created after AUTHENTICATED.
  std::unique_ptr<PeerSession> peer_session_;

  // True if ClientSession teardown has already begun. Prevents re-entrant
  // execution of OnConnectionClosed() when callbacks fire during teardown.
  bool is_closing_ = false;

  std::string client_jid_;

  HostExperimentSessionPlugin host_experiment_session_plugin_;

  SessionPolicies effective_policies_;

  raw_ptr<const LocalSessionPoliciesProvider> local_session_policies_provider_;

  // If `effective_policies` does not come from local session policies, the
  // subscription will be null and OnLocalSessionPoliciesChanged() will never
  // be called.
  base::CallbackListSubscription local_session_policy_update_subscription_;

  std::vector<mojo::PendingReceiver<mojom::ChromotingSessionServices>>
      pending_session_services_receivers_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to disable callbacks to `this` once DisconnectSession() has been
  // called.
  base::WeakPtrFactory<ClientSession> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_CLIENT_SESSION_H_
