// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/xmpp_push_client.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "jingle/glue/network_service_config_test_util.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/listener/push_client_observer.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifier {

namespace {

using ::testing::_;
using ::testing::Mock;
using ::testing::StrictMock;

class MockObserver : public PushClientObserver {
 public:
  MOCK_METHOD0(OnNotificationsEnabled, void());
  MOCK_METHOD1(OnNotificationsDisabled, void(NotificationsDisabledReason));
  MOCK_METHOD1(OnIncomingNotification, void(const Notification&));
  MOCK_METHOD0(OnPingResponse, void());
};

class XmppPushClientTest : public testing::Test {
 protected:
  XmppPushClientTest()
      : net_config_helper_(
            base::MakeRefCounted<net::TestURLRequestContextGetter>(
                task_environment_.GetMainThreadTaskRunner())) {
    net_config_helper_.FillInNetworkConfig(&notifier_options_.network_config);
  }

  ~XmppPushClientTest() override {}

  void SetUp() override {
    xmpp_push_client_.reset(new XmppPushClient(notifier_options_));
    xmpp_push_client_->AddObserver(&mock_observer_);
  }

  void TearDown() override {
    // Clear out any messages posted by XmppPushClient.
    base::RunLoop().RunUntilIdle();
    xmpp_push_client_->RemoveObserver(&mock_observer_);
    xmpp_push_client_.reset();
  }

  // The sockets created by the XMPP code expect an IO loop.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  jingle_glue::NetworkServiceConfigTestUtil net_config_helper_;
  NotifierOptions notifier_options_;
  StrictMock<MockObserver> mock_observer_;
  std::unique_ptr<XmppPushClient> xmpp_push_client_;
  FakeBaseTask fake_base_task_;
};

// Make sure the XMPP push client notifies its observers of incoming
// notifications properly.
TEST_F(XmppPushClientTest, OnIncomingNotification) {
  EXPECT_CALL(mock_observer_, OnIncomingNotification(_));
  xmpp_push_client_->OnNotificationReceived(Notification());
}

// Make sure the XMPP push client notifies its observers of a
// successful connection properly.
TEST_F(XmppPushClientTest, ConnectAndSubscribe) {
  EXPECT_CALL(mock_observer_, OnNotificationsEnabled());
  xmpp_push_client_->OnConnect(fake_base_task_.AsWeakPtr());
  xmpp_push_client_->OnSubscribed();
}

// Make sure the XMPP push client notifies its observers of a
// terminated connection properly.
TEST_F(XmppPushClientTest, Disconnect) {
  EXPECT_CALL(mock_observer_,
              OnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR));
  xmpp_push_client_->OnTransientDisconnection();
}

// Make sure the XMPP push client notifies its observers of
// rejected credentials properly.
TEST_F(XmppPushClientTest, RejectCredentials) {
  EXPECT_CALL(mock_observer_,
              OnNotificationsDisabled(NOTIFICATION_CREDENTIALS_REJECTED));
  xmpp_push_client_->OnCredentialsRejected();
}

// Make sure the XMPP push client notifies its observers of a
// subscription error properly.
TEST_F(XmppPushClientTest, SubscriptionError) {
  EXPECT_CALL(mock_observer_,
              OnNotificationsDisabled(TRANSIENT_NOTIFICATION_ERROR));
  xmpp_push_client_->OnSubscriptionError();
}

// Make sure nothing blows up when the XMPP push client sends a
// notification.
//
// TODO(akalin): Figure out how to test that the notification was
// actually sent.
TEST_F(XmppPushClientTest, SendNotification) {
  EXPECT_CALL(mock_observer_, OnNotificationsEnabled());

  xmpp_push_client_->OnConnect(fake_base_task_.AsWeakPtr());
  xmpp_push_client_->OnSubscribed();
  xmpp_push_client_->SendNotification(Notification());
}

// Make sure nothing blows up when the XMPP push client sends a ping.
//
// TODO(akalin): Figure out how to test that the ping was actually sent.
TEST_F(XmppPushClientTest, SendPing) {
  EXPECT_CALL(mock_observer_, OnNotificationsEnabled());

  xmpp_push_client_->OnConnect(fake_base_task_.AsWeakPtr());
  xmpp_push_client_->OnSubscribed();
  xmpp_push_client_->SendPing();
}

// Make sure nothing blows up when the XMPP push client sends a
// notification when disconnected, and the client connects.
//
// TODO(akalin): Figure out how to test that the notification was
// actually sent.
TEST_F(XmppPushClientTest, SendNotificationPending) {
  xmpp_push_client_->SendNotification(Notification());

  Mock::VerifyAndClearExpectations(&mock_observer_);

  EXPECT_CALL(mock_observer_, OnNotificationsEnabled());

  xmpp_push_client_->OnConnect(fake_base_task_.AsWeakPtr());
  xmpp_push_client_->OnSubscribed();
}

}  // namespace

}  // namespace notifier
