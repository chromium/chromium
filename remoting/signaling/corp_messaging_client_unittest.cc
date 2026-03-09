// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/corp_messaging_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "remoting/base/buildflags.h"
#include "remoting/base/certificate_helpers.h"
#include "remoting/base/http_status.h"
#include "remoting/base/internal_headers.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/base/protobuf_http_test_responder.h"
#include "remoting/proto/messaging_service.h"
#include "remoting/signaling/corp_message_channel_strategy.h"
#include "remoting/signaling/jingle_data_structures.h"
#include "remoting/signaling/signaling_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using ::testing::_;
using ::testing::Return;

using internal::HostSendMessageRequest;
using internal::HostSendMessageResponse;

constexpr char kFakeUsername[] = "fake_user";
#if BUILDFLAG(REMOTING_INTERNAL)
// Only used for validation in REMOTING_INTERNAL builds.
constexpr char kFakeAuthzToken[] = "fake_authz_token";
#endif
constexpr char kFakeAuthzTokenBase64[] = "ZmFrZV9hdXRoel90b2tlbg==";
constexpr char kFakePublicKeyBase64[] = "ZmFrZV9wdWJsaWNfa2V5";

using DoneCallback = CorpMessagingClient::DoneCallback;

}  // namespace

class CorpMessagingClientTest : public testing::Test {
 protected:
  void OnMessageReceived(const internal::PeerMessageStruct& message) {
    messaging_client_.OnMessageReceived(message);
  }

  base::test::TaskEnvironment task_environment_;
  ProtobufHttpTestResponder test_responder_;
  base::MockCallback<CorpMessagingClient::SignalingAddressChangedCallback>
      mock_on_signaling_address_changed_;
  CorpMessagingClient messaging_client_{
      kFakeUsername, kFakePublicKeyBase64,
      test_responder_.GetUrlLoaderFactory(), CreateClientCertStoreInstance(),
      mock_on_signaling_address_changed_.Get()};
};

TEST_F(CorpMessagingClientTest, TestSendMessage_Unauthenticated) {
  base::test::TestFuture<HttpStatus> status_future;

  internal::PeerMessageStruct peer_message;
  messaging_client_.SendMessage(SignalingAddress{kFakeAuthzTokenBase64},
                                std::move(peer_message),
                                status_future.GetCallback<const HttpStatus&>());
  test_responder_.AddErrorToMostRecentRequestUrl(
      HttpStatus(HttpStatus::Code::UNAUTHENTICATED, "Unauthenticated"));
  EXPECT_EQ(status_future.Get().error_code(),
            HttpStatus::Code::UNAUTHENTICATED);
}

TEST_F(CorpMessagingClientTest, TestSendMessage_SendOnePeerMessage) {
  base::test::TestFuture<HttpStatus> status_future;
  internal::PeerMessageStruct peer_message;
  messaging_client_.SendMessage(SignalingAddress{kFakeAuthzTokenBase64},
                                std::move(peer_message),
                                status_future.GetCallback<const HttpStatus&>());

  HostSendMessageRequest request;
  ASSERT_TRUE(test_responder_.GetMostRecentRequestMessage(&request));
#if BUILDFLAG(REMOTING_INTERNAL)
  // External builds use a DoNothing proto to back the request so we scope the
  // verification to internal builds only.
  ASSERT_EQ(kFakeAuthzToken, request.messaging_authz_token());
#endif

  test_responder_.AddResponseToMostRecentRequestUrl(HostSendMessageResponse());
  EXPECT_EQ(status_future.Get().error_code(), HttpStatus::Code::OK);
}

TEST_F(CorpMessagingClientTest, TestSendMessage_SendOneJingleMessage) {
  base::test::TestFuture<HttpStatus> status_future;
  internal::PeerMessageStruct peer_message;
  internal::IqStanzaStruct iq_stanza;
  iq_stanza.xml = "<iq>test</iq>";
  peer_message.payload = std::move(iq_stanza);

  messaging_client_.SendMessage(SignalingAddress{kFakeAuthzTokenBase64},
                                std::move(peer_message),
                                status_future.GetCallback<const HttpStatus&>());

  HostSendMessageRequest request;
  ASSERT_TRUE(test_responder_.GetMostRecentRequestMessage(&request));
#if BUILDFLAG(REMOTING_INTERNAL)
  // Verify that the Jingle message was wrapped in a PeerMessage.
  ASSERT_TRUE(request.has_peer_message());
#endif

  test_responder_.AddResponseToMostRecentRequestUrl(HostSendMessageResponse());
  EXPECT_EQ(status_future.Get().error_code(), HttpStatus::Code::OK);
}

TEST_F(CorpMessagingClientTest, TestIsReceivingMessages) {
  EXPECT_FALSE(messaging_client_.IsReceivingMessages());
  base::test::TestFuture<void> ready_future;
  messaging_client_.StartReceivingMessages(ready_future.GetCallback(),
                                           base::DoNothing());

  test_responder_.AddStreamResponseToMostRecentRequestUrl(
      /*messages=*/{}, HttpStatus::OK());

  EXPECT_TRUE(ready_future.Wait());
  EXPECT_TRUE(messaging_client_.IsReceivingMessages());
}

TEST_F(CorpMessagingClientTest, TestStopReceivingMessages) {
  base::test::TestFuture<void> ready_future;
  messaging_client_.StartReceivingMessages(ready_future.GetCallback(),
                                           base::DoNothing());

  test_responder_.AddStreamResponseToMostRecentRequestUrl(
      /*messages=*/{}, HttpStatus::OK());

  EXPECT_TRUE(ready_future.Wait());
  EXPECT_TRUE(messaging_client_.IsReceivingMessages());

  messaging_client_.StopReceivingMessages();
  EXPECT_FALSE(messaging_client_.IsReceivingMessages());
}

TEST_F(CorpMessagingClientTest, TestSendMessage_OverwritesMessageId) {
  base::test::TestFuture<HttpStatus> status_future;
  internal::PeerMessageStruct peer_message;
  peer_message.message_id = "existing_message_id";
  messaging_client_.SendMessage(SignalingAddress{kFakeAuthzTokenBase64},
                                std::move(peer_message),
                                status_future.GetCallback<const HttpStatus&>());

  HostSendMessageRequest request;
  ASSERT_TRUE(test_responder_.GetMostRecentRequestMessage(&request));
#if BUILDFLAG(REMOTING_INTERNAL)
  ASSERT_NE("existing_message_id", request.peer_message().message_id());
  ASSERT_FALSE(request.peer_message().message_id().empty());
#endif

  test_responder_.AddResponseToMostRecentRequestUrl(HostSendMessageResponse());
  EXPECT_EQ(status_future.Get().error_code(), HttpStatus::Code::OK);
}

TEST_F(CorpMessagingClientTest, TestRegisterMessageCallback) {
  base::MockCallback<CorpMessagingClient::MessageCallback> mock_callback;
  auto subscription =
      messaging_client_.RegisterMessageCallback(mock_callback.Get());

  internal::PeerMessageStruct peer_message;
  internal::IqStanzaStruct iq_stanza;
  iq_stanza.messaging_authz_token = kFakeAuthzTokenBase64;
  peer_message.payload = std::move(iq_stanza);

  EXPECT_CALL(mock_callback, Run(SignalingAddress(kFakeAuthzTokenBase64), _));
  OnMessageReceived(peer_message);
}

TEST_F(CorpMessagingClientTest, TestOnMessageReceived_NonIqStanza) {
  base::MockCallback<CorpMessagingClient::MessageCallback> mock_callback;
  auto subscription =
      messaging_client_.RegisterMessageCallback(mock_callback.Get());

  internal::PeerMessageStruct peer_message;
  peer_message.payload = internal::SystemTestStruct();

  // If it's not an IqStanza, SignalingAddress should be empty.
  EXPECT_CALL(mock_callback, Run(SignalingAddress(), _));
  OnMessageReceived(peer_message);
}

// Since the internals of the various messaging protos are not available in the
// public builds, complicated tests which rely on them are not worth running
// in that context.
#if BUILDFLAG(REMOTING_INTERNAL)

// TODO: joedow - Implement `StartReceivingMessages` tests after updating the
// internal helpers so we can generate server-side protos from the message
// structs.

#endif  // BUILDFLAG(REMOTING_INTERNAL)

}  // namespace remoting
