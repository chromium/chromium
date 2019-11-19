// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_FTL_SERVICES_PLAYGROUND_H_
#define REMOTING_TEST_FTL_SERVICES_PLAYGROUND_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/base/grpc_support/grpc_authenticated_executor.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/signaling/ftl_messaging_client.h"
#include "remoting/signaling/ftl_registration_manager.h"

namespace remoting {

namespace test {

class TestOAuthTokenGetter;
class TestTokenStorage;

}  // namespace test

class FtlServicesPlayground {
 public:
  FtlServicesPlayground();
  ~FtlServicesPlayground();

  bool ShouldPrintHelp();
  void PrintHelp();
  void StartAndAuthenticate();

 private:
  using PeerToPeer =
      google::internal::communications::instantmessaging::v1::PeerToPeer;
  using Registration =
      google::internal::communications::instantmessaging::v1::Registration;

  void StartLoop();
  void ResetServices(base::OnceClosure on_done);

  void GetIceServer(base::OnceClosure on_done);
  void OnGetIceServerResponse(base::OnceClosure on_done,
                              const grpc::Status& status,
                              const ftl::GetICEServerResponse& response);

  void SignInGaia(base::OnceClosure on_done);
  void OnSignInGaiaResponse(base::OnceClosure on_done,
                            const grpc::Status& status);

  void PullMessages(base::OnceClosure on_done);
  void OnPullMessagesResponse(base::OnceClosure on_done,
                              const grpc::Status& status);
  void SendMessage(base::OnceClosure on_done);
  void DoSendMessage(const std::string& receiver_id,
                     const std::string& registration_id,
                     base::OnceClosure on_done,
                     bool should_keep_running);
  void OnSendMessageResponse(base::OnceCallback<void(bool)> on_continue,
                             const grpc::Status& status);
  void StartReceivingMessages(base::OnceClosure on_done);
  void StopReceivingMessages(base::OnceClosure on_done);
  void OnMessageReceived(const ftl::Id& sender_id,
                         const std::string& sender_registration_id,
                         const ftl::ChromotingMessage& message);
  void OnReceiveMessagesStreamReady();
  void OnReceiveMessagesStreamClosed(const grpc::Status& status);

  void HandleGrpcStatusError(base::OnceClosure on_done,
                             const grpc::Status& status);

  std::unique_ptr<test::TestTokenStorage> storage_;
  std::unique_ptr<test::TestOAuthTokenGetter> token_getter_;
  std::unique_ptr<GrpcAuthenticatedExecutor> executor_;

  std::unique_ptr<FtlRegistrationManager> registration_manager_;

  // Subscription must be deleted before |messaging_client_|.
  std::unique_ptr<FtlMessagingClient> messaging_client_;
  std::unique_ptr<FtlMessagingClient::MessageCallbackSubscription>
      message_subscription_;

  std::unique_ptr<PeerToPeer::Stub> peer_to_peer_stub_;

  base::OnceClosure receive_messages_done_callback_;

  base::WeakPtrFactory<FtlServicesPlayground> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(FtlServicesPlayground);
};

}  // namespace remoting

#endif  // REMOTING_TEST_FTL_SERVICES_PLAYGROUND_H_
