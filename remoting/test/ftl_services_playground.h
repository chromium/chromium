// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_FTL_SERVICES_PLAYGROUND_H_
#define REMOTING_TEST_FTL_SERVICES_PLAYGROUND_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/signaling/ftl_messaging_client.h"
#include "remoting/signaling/ftl_registration_manager.h"

namespace network {
class TransitionalURLLoaderFactoryOwner;
}  // namespace network

namespace remoting {

namespace test {

class TestOAuthTokenGetter;
class TestTokenStorage;

}  // namespace test

class FtlServicesPlayground {
 public:
  FtlServicesPlayground();

  FtlServicesPlayground(const FtlServicesPlayground&) = delete;
  FtlServicesPlayground& operator=(const FtlServicesPlayground&) = delete;

  ~FtlServicesPlayground();

  bool ShouldPrintHelp();
  void PrintHelp();
  void StartAndAuthenticate();

 private:
  void StartLoop();
  void ResetServices(base::OnceClosure on_done);

  void SignInGaia(base::OnceClosure on_done);
  void OnSignInGaiaResponse(base::OnceClosure on_done,
                            const ProtobufHttpStatus& status);

  void SendMessage(base::OnceClosure on_done);
  void DoSendMessage(const std::string& receiver_id,
                     const std::string& registration_id,
                     base::OnceClosure on_done,
                     bool should_keep_running);
  void OnSendMessageResponse(base::OnceCallback<void(bool)> on_continue,
                             const ProtobufHttpStatus& status);
  void StartReceivingMessages(base::OnceClosure on_done);
  void StopReceivingMessages(base::OnceClosure on_done);
  void OnMessageReceived(const ftl::Id& sender_id,
                         const std::string& sender_registration_id,
                         const ftl::ChromotingMessage& message);
  void OnReceiveMessagesStreamReady();
  void OnReceiveMessagesStreamClosed(const ProtobufHttpStatus& status);

  void HandleStatusError(base::OnceClosure on_done,
                         const ProtobufHttpStatus& status);

  std::unique_ptr<test::TestTokenStorage> storage_;
  std::unique_ptr<test::TestOAuthTokenGetter> token_getter_;
  std::unique_ptr<network::TransitionalURLLoaderFactoryOwner>
      url_loader_factory_owner_;

  std::unique_ptr<FtlRegistrationManager> registration_manager_;

  // Subscription must be deleted before |messaging_client_|.
  std::unique_ptr<FtlMessagingClient> messaging_client_;
  base::CallbackListSubscription message_subscription_;

  base::OnceClosure receive_messages_done_callback_;

  base::WeakPtrFactory<FtlServicesPlayground> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_TEST_FTL_SERVICES_PLAYGROUND_H_
