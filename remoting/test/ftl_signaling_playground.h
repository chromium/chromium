// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_FTL_SIGNALING_PLAYGROUND_H_
#define REMOTING_TEST_FTL_SIGNALING_PLAYGROUND_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/timer/timer.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/protocol/client_authentication_config.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_manager.h"
#include "remoting/signaling/ftl_messaging_client.h"
#include "remoting/signaling/ftl_registration_manager.h"
#include "remoting/signaling/signal_strategy.h"
#include "remoting/test/fake_ice_connection.h"
#include "remoting/test/fake_webrtc_connection.h"

namespace network {
class TransitionalURLLoaderFactoryOwner;
}  // namespace network

namespace remoting {

namespace test {
class TestOAuthTokenGetter;
class TestTokenStorage;
}  // namespace test

class FtlSignalingPlayground final : public SignalStrategy::Listener,
                                     public protocol::Session::EventHandler {
 public:
  FtlSignalingPlayground();

  FtlSignalingPlayground(const FtlSignalingPlayground&) = delete;
  FtlSignalingPlayground& operator=(const FtlSignalingPlayground&) = delete;

  ~FtlSignalingPlayground() override;

  bool ShouldPrintHelp();
  void PrintHelp();
  void StartLoop();

 private:
  void AcceptIncoming(base::OnceClosure on_done);
  void OnIncomingSession(
      protocol::Session* owned_session,
      protocol::SessionManager::IncomingSessionResponse* response);

  void ConnectToHost(base::OnceClosure on_done);
  void OnClientSignalingConnected();
  void FetchSecret(
      bool pairing_supported,
      const protocol::SecretFetchedCallback& secret_fetched_callback);

  void SetUpSignaling();
  void TearDownSignaling();
  void RegisterSession(std::unique_ptr<protocol::Session> session,
                       protocol::TransportRole transport_role);
  void InitializeTransport();

  // SignalStrategy::Listener interface.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override;
  bool OnSignalStrategyIncomingStanza(
      const jingle_xmpp::XmlElement* stanza) override;

  // Session::EventHandler interface.
  void OnSessionStateChange(protocol::Session::State state) override;

  void AsyncTearDownAndRunCallback();
  void TearDownAndRunCallback();

  std::unique_ptr<test::TestTokenStorage> storage_;
  std::unique_ptr<test::TestOAuthTokenGetter> token_getter_;
  std::unique_ptr<network::TransitionalURLLoaderFactoryOwner>
      url_loader_factory_owner_;

  std::unique_ptr<SignalStrategy> signal_strategy_;
  std::unique_ptr<protocol::SessionManager> session_manager_;
  std::unique_ptr<test::FakeWebrtcConnection> webrtc_connection_;
  std::unique_ptr<test::FakeIceConnection> ice_connection_;
  std::unique_ptr<protocol::Session> session_;
  protocol::TransportRole transport_role_;

  base::OnceClosure current_callback_;
  base::OnceClosure on_signaling_connected_callback_;

  base::OneShotTimer tear_down_timer_;
};

}  // namespace remoting

#endif  // REMOTING_TEST_FTL_SIGNALING_PLAYGROUND_H_
