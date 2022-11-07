// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_HOST_H_
#define REMOTING_HOST_CHROMOTING_HOST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/backoff_entry.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/client_session.h"
#include "remoting/host/host_extension.h"
#include "remoting/host/host_status_monitor.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/pairing_registry.h"
#include "remoting/protocol/session_manager.h"
#include "remoting/protocol/transport_context.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace named_mojo_ipc_server {
class IpcServer;
}

namespace remoting {

namespace protocol {
class InputStub;
}  // namespace protocol

class DesktopEnvironmentFactory;

// A class to implement the functionality of a host process.
//
// Here's the work flow of this class:
// 1. We should load the saved GAIA ID token or if this is the first
//    time the host process runs we should prompt user for the
//    credential. We will use this token or credentials to authenticate
//    and register the host.
//
// 2. We listen for an incoming connection using libjingle. We will create
//    a ConnectionToClient object that wraps around libjingle for transport.
//    A VideoFramePump is created with an Encoder and a webrtc::DesktopCapturer.
//    A ConnectionToClient is added to the ScreenRecorder for transporting
//    the screen captures. An InputStub is created and registered with the
//    ConnectionToClient to receive mouse / keyboard events from the remote
//    client.
//    After we have done all the initialization we'll start the ScreenRecorder.
//    We'll then enter the running state of the host process.
//
// 3. When the user is disconnected, we will pause the ScreenRecorder
//    and try to terminate the threads we have created. This will allow
//    all pending tasks to complete. After all of that has completed, we
//    return to the idle state. We then go to step (2) to wait for a new
//    incoming connection.
class ChromotingHost : public ClientSession::EventHandler,
                       public mojom::ChromotingHostServices {
 public:
  typedef std::vector<std::unique_ptr<ClientSession>> ClientSessions;

  // |desktop_environment_factory| must outlive this object.
  ChromotingHost(
      DesktopEnvironmentFactory* desktop_environment_factory,
      std::unique_ptr<protocol::SessionManager> session_manager,
      scoped_refptr<protocol::TransportContext> transport_context,
      scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner,
      const DesktopEnvironmentOptions& options);

  ChromotingHost(const ChromotingHost&) = delete;
  ChromotingHost& operator=(const ChromotingHost&) = delete;

  ~ChromotingHost() override;

  // Asynchronously starts the host.
  //
  // Once this is called, the host will start listening for inbound connections.
  //
  // This method can only be called once during the lifetime of this object.
  void Start(const std::string& host_owner);

  // Starts running the ChromotingHostServices server and listening for incoming
  // IPC binding requests.
  // It must be started exactly once across all Chromoting processes.
  void StartChromotingHostServices();

  scoped_refptr<HostStatusMonitor> status_monitor() { return status_monitor_; }
  const DesktopEnvironmentOptions& desktop_environment_options() const {
    return desktop_environment_options_;
  }

  // Registers a host extension.
  void AddExtension(std::unique_ptr<HostExtension> extension);

  // Sets the authenticator factory to use for incoming
  // connections. Incoming connections are rejected until
  // authenticator factory is set. Must be called on the network
  // thread after the host is started. Must not be called more than
  // once per host instance because it may not be safe to delete
  // factory before all authenticators it created are deleted.
  void SetAuthenticatorFactory(
      std::unique_ptr<protocol::AuthenticatorFactory> authenticator_factory);

  // Sets the maximum duration of any session. By default, a session has no
  // maximum duration.
  void SetMaximumSessionDuration(const base::TimeDelta& max_session_duration);

  ////////////////////////////////////////////////////////////////////////////
  // ClientSession::EventHandler implementation.
  void OnSessionAuthenticating(ClientSession* client) override;
  void OnSessionAuthenticated(ClientSession* client) override;
  void OnSessionChannelsConnected(ClientSession* client) override;
  void OnSessionAuthenticationFailed(ClientSession* client) override;
  void OnSessionClosed(ClientSession* session) override;
  void OnSessionRouteChange(ClientSession* session,
                            const std::string& channel_name,
                            const protocol::TransportRoute& route) override;

  // mojom::ChromotingHostServices implementation.
  void BindSessionServices(
      mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver)
      override;

  // Callback for SessionManager to accept incoming sessions.
  void OnIncomingSession(
      protocol::Session* session,
      protocol::SessionManager::IncomingSessionResponse* response);

  // The host uses a pairing registry to generate and store pairing information
  // for clients for PIN-less authentication.
  scoped_refptr<protocol::PairingRegistry> pairing_registry() const {
    return pairing_registry_;
  }
  void set_pairing_registry(
      scoped_refptr<protocol::PairingRegistry> pairing_registry) {
    pairing_registry_ = pairing_registry;
  }

  const ClientSessions& client_sessions_for_tests() { return clients_; }

  scoped_refptr<protocol::TransportContext> transport_context_for_tests() {
    return transport_context_;
  }

 private:
  // Returns the currently connected client session, or nullptr if not found.
  ClientSession* GetConnectedClientSession() const;

  friend class ChromotingHostTest;

  // Unless specified otherwise, all members of this class must be
  // used on the network thread only.

  // Parameters specified when the host was created.
  raw_ptr<DesktopEnvironmentFactory> desktop_environment_factory_;
  std::unique_ptr<protocol::SessionManager> session_manager_;
  scoped_refptr<protocol::TransportContext> transport_context_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner_;

  scoped_refptr<HostStatusMonitor> status_monitor_;

  // The connections to remote clients.
  ClientSessions clients_;

  // True if the host has been started.
  bool started_ = false;

  // Login backoff state.
  net::BackoffEntry login_backoff_;

  // Options to initialize a DesktopEnvironment.
  const DesktopEnvironmentOptions desktop_environment_options_;

  // The maximum duration of any session.
  base::TimeDelta max_session_duration_;

  // The pairing registry for PIN-less authentication.
  scoped_refptr<protocol::PairingRegistry> pairing_registry_;

  // List of host extensions.
  std::vector<std::unique_ptr<HostExtension>> extensions_;

  // IPC server that runs the CRD host service API. Non-null if the server name
  // is set and the host is started.
  std::unique_ptr<named_mojo_ipc_server::IpcServer> ipc_server_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ChromotingHost> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMOTING_HOST_H_
