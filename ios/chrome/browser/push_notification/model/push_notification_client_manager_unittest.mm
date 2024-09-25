// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_client_manager.h"

#import <UserNotifications/UserNotifications.h>

#import "ios/chrome/browser/push_notification/model/push_notification_client.h"
#import "ios/chrome/browser/push_notification/model/test_push_notification_client.h"
#import "testing/platform_test.h"

namespace {

void GenerateClients(std::unique_ptr<PushNotificationClientManager>& manager,
                     size_t number_of_clients) {
  for (size_t i = 0; i < number_of_clients; i++) {
    std::unique_ptr<PushNotificationClient> client =
        std::make_unique<TestPushNotificationClient>(i + 1);
    manager->AddPushNotificationClient(std::move(client));
  }
}

TestPushNotificationClient* GetClient(
    std::unique_ptr<PushNotificationClientManager>& manager,
    size_t index) {
  return const_cast<TestPushNotificationClient*>(
      static_cast<const TestPushNotificationClient*>(
          manager->GetPushNotificationClients()[index]));
}

}  // namespace

class PushNotificationClientManagerTest : public PlatformTest {
 protected:
  PushNotificationClientManagerTest() {
    size_t number_of_clients = manager_->GetPushNotificationClients().size();

    for (size_t i = 0; i < number_of_clients; i++) {
      manager_->RemovePushNotificationClient(
          GetClient(manager_, 0)->GetClientId());
    }
  }
  std::unique_ptr<PushNotificationClientManager> manager_ =
      std::make_unique<PushNotificationClientManager>();
};

TEST_F(PushNotificationClientManagerTest, AddClient) {
  const unsigned long number_of_clients = 1;
  GenerateClients(manager_, number_of_clients);

  ASSERT_EQ(number_of_clients, manager_->GetPushNotificationClients().size());
}

TEST_F(PushNotificationClientManagerTest, AddMultipleClients) {
  const unsigned long number_of_clients = 5;
  GenerateClients(manager_, number_of_clients);

  ASSERT_EQ(number_of_clients, manager_->GetPushNotificationClients().size());
}

TEST_F(PushNotificationClientManagerTest,
       HandleNotificationReceptionWithInvalidData) {
  const unsigned long number_of_clients = 5;
  GenerateClients(manager_, number_of_clients);

  ASSERT_EQ(UIBackgroundFetchResultFailed,
            manager_->HandleNotificationReception(nil));
}

TEST_F(PushNotificationClientManagerTest, HandleNotificationInteraction) {
  const unsigned long number_of_clients = 1;
  GenerateClients(manager_, number_of_clients);

  manager_->HandleNotificationInteraction(nil);
  ASSERT_TRUE(GetClient(manager_, 0)->HasNotificationReceivedInteraction());
}

TEST_F(PushNotificationClientManagerTest,
       HandleNotificationInteractionWithMultipleClients) {
  const unsigned long number_of_clients = 5;
  GenerateClients(manager_, number_of_clients);

  manager_->HandleNotificationInteraction(nil);
  for (size_t i = 0; i < manager_->GetPushNotificationClients().size(); i++) {
    ASSERT_TRUE(GetClient(manager_, i)->HasNotificationReceivedInteraction());
  }
}

TEST_F(PushNotificationClientManagerTest, BrowserReady) {
  GenerateClients(manager_, 1);
  EXPECT_FALSE(GetClient(manager_, 0)->IsBrowserReady());
  manager_->OnSceneActiveForegroundBrowserReady();
  EXPECT_TRUE(GetClient(manager_, 0)->IsBrowserReady());
}
