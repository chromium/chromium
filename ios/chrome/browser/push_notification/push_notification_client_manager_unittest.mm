// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UserNotifications/UserNotifications.h>

#import "ios/chrome/browser/push_notification/push_notification_client.h"
#import "ios/chrome/browser/push_notification/push_notification_client_manager.h"
#import "ios/chrome/browser/push_notification/test_push_notification_client.h"
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
    size_t number_of_clients = manager->GetPushNotificationClients().size();

    for (size_t i = 0; i < number_of_clients; i++) {
      manager->RemovePushNotificationClient(
          GetClient(manager, i)->GetClientId());
    }
  }
  std::unique_ptr<PushNotificationClientManager> manager =
      std::make_unique<PushNotificationClientManager>();
};

TEST_F(PushNotificationClientManagerTest, AddClient) {
  const unsigned long number_of_clients = 1;
  GenerateClients(manager, number_of_clients);

  ASSERT_EQ(number_of_clients, manager->GetPushNotificationClients().size());
}

TEST_F(PushNotificationClientManagerTest, AddMultipleClients) {
  const unsigned long number_of_clients = 5;
  GenerateClients(manager, number_of_clients);

  ASSERT_EQ(number_of_clients, manager->GetPushNotificationClients().size());
}

TEST_F(PushNotificationClientManagerTest, HandleNotificationReception) {
  GenerateClients(manager, 1);
  ASSERT_EQ(UIBackgroundFetchResultNoData,
            manager->HandleNotificationReception(nil));
}

TEST_F(PushNotificationClientManagerTest,
       HandleNotificationReceptionWithNewData) {
  const unsigned long number_of_clients = 5;
  GenerateClients(manager, number_of_clients);

  TestPushNotificationClient* client = GetClient(manager, 0);
  client->SetBackgroundFetchResult(UIBackgroundFetchResultNewData);
  ASSERT_EQ(UIBackgroundFetchResultNewData,
            manager->HandleNotificationReception(nil));
}

TEST_F(PushNotificationClientManagerTest,
       HandleNotificationReceptionWithFailure) {
  const unsigned long number_of_clients = 5;
  GenerateClients(manager, number_of_clients);

  TestPushNotificationClient* client = GetClient(manager, 0);
  client->SetBackgroundFetchResult(UIBackgroundFetchResultFailed);
  ASSERT_EQ(UIBackgroundFetchResultFailed,
            manager->HandleNotificationReception(nil));
}

TEST_F(PushNotificationClientManagerTest,
       HandleNotificationReceptionWithNewDataAndFailure) {
  const unsigned long number_of_clients = 5;
  GenerateClients(manager, number_of_clients);

  GetClient(manager, 0)
      ->SetBackgroundFetchResult(UIBackgroundFetchResultNewData);
  GetClient(manager, 1)
      ->SetBackgroundFetchResult(UIBackgroundFetchResultFailed);
  ASSERT_EQ(UIBackgroundFetchResultNewData,
            manager->HandleNotificationReception(nil));
}

TEST_F(PushNotificationClientManagerTest, HandleNotificationInteraction) {
  const unsigned long number_of_clients = 1;
  GenerateClients(manager, number_of_clients);

  manager->HandleNotificationInteraction(nil);
  ASSERT_TRUE(GetClient(manager, 0)->HasNotificationReceivedInteraction());
}

TEST_F(PushNotificationClientManagerTest,
       HandleNotificationInteractionWithMultipleClients) {
  const unsigned long number_of_clients = 5;
  GenerateClients(manager, number_of_clients);

  manager->HandleNotificationInteraction(nil);
  for (size_t i = 0; i < manager->GetPushNotificationClients().size(); i++) {
    ASSERT_TRUE(GetClient(manager, i)->HasNotificationReceivedInteraction());
  }
}

// TODO(crbug.com/1449081): Fails on ASAN
#if defined(ADDRESS_SANITIZER)
#define MAYBE_BrowserReady DISABLED_BrowserReady
#else
#define MAYBE_BrowserReady BrowserReady
#endif
TEST_F(PushNotificationClientManagerTest, MAYBE_BrowserReady) {
  GenerateClients(manager, 1);
  EXPECT_FALSE(GetClient(manager, 0)->IsBrowserReady());
  manager->OnSceneActiveForegroundBrowserReady();
  EXPECT_TRUE(GetClient(manager, 0)->IsBrowserReady());
}
