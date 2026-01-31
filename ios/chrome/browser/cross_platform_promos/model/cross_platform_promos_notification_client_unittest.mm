// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_notification_client.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "components/desktop_to_mobile_promos/promos_types.h"
#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_service.h"
#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_service_factory.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using testing::_;

namespace {

class MockCrossPlatformPromosService : public CrossPlatformPromosService {
 public:
  MockCrossPlatformPromosService(ProfileIOS* profile)
      : CrossPlatformPromosService(profile) {}
  MOCK_METHOD1(ShowLensPromo, void(Browser*));
  MOCK_METHOD1(ShowESBPromo, void(Browser*));
  MOCK_METHOD1(ShowCPEPromo, void(Browser*));
};

}  // namespace

// Test suite for the `CrossPlatformPromosNotificationClient`.
class CrossPlatformPromosNotificationClientTest : public PlatformTest {
 public:
  void SetUp() override {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        CrossPlatformPromosServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<MockCrossPlatformPromosService>(profile);
            }));
    profile_ = std::move(builder).Build();

    // Set up SceneState with activation level.
    scene_state_ = OCMClassMock([SceneState class]);
    OCMStub([scene_state_ activationLevel])
        .andReturn(SceneActivationLevelForegroundActive);

    client_ =
        std::make_unique<CrossPlatformPromosNotificationClient>(profile_.get());
  }

  void CreateBrowser() {
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    BrowserListFactory::GetForProfile(profile_.get())
        ->AddBrowser(browser_.get());
  }

  MockCrossPlatformPromosService* mock_service() {
    return static_cast<MockCrossPlatformPromosService*>(
        CrossPlatformPromosServiceFactory::GetForProfile(profile_.get()));
  }

  UNNotificationResponse* CreateMockNotificationResponse(
      desktop_to_mobile_promos::PromoType promo_type) {
    UNMutableNotificationContent* content =
        [[UNMutableNotificationContent alloc] init];
    content.userInfo = @{
      kDesktopToMobilePromoTypeKey : @(static_cast<int>(promo_type)),
      kPushNotificationClientIdKey :
          @(static_cast<int>(PushNotificationClientId::kCrossPlatformPromos))
    };

    UNNotificationRequest* request =
        [UNNotificationRequest requestWithIdentifier:@"test_id"
                                             content:content
                                             trigger:nil];

    id mock_notification = OCMClassMock([UNNotification class]);
    OCMStub([mock_notification request]).andReturn(request);

    id mock_response = OCMClassMock([UNNotificationResponse class]);
    OCMStub([mock_response notification]).andReturn(mock_notification);

    return mock_response;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<TestProfileIOS> profile_;
  id scene_state_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<CrossPlatformPromosNotificationClient> client_;
};

// Tests that the client can be instantiated.
TEST_F(CrossPlatformPromosNotificationClientTest, Instantiate) {
  EXPECT_TRUE(client_);
}

// Tests that HandleNotificationInteraction shows the promo immediately if
// browser is ready.
TEST_F(CrossPlatformPromosNotificationClientTest,
       HandleInteraction_BrowserReady) {
  CreateBrowser();
  EXPECT_CALL(*mock_service(), ShowCPEPromo(browser_.get()));

  UNNotificationResponse* response = CreateMockNotificationResponse(
      desktop_to_mobile_promos::PromoType::kPassword);

  EXPECT_TRUE(client_->HandleNotificationInteraction(response));
  histogram_tester_.ExpectUniqueSample(
      "IOS.CrossPlatformPromos.PushNotification.Interaction",
      desktop_to_mobile_promos::PromoType::kPassword, 1);
  histogram_tester_.ExpectUniqueSample(
      "IOS.CrossPlatformPromos.Promo.Shown.FromPush",
      desktop_to_mobile_promos::PromoType::kPassword, 1);
}

// Tests that HandleNotificationInteraction queues the promo if browser is not
// ready, and OnSceneActiveForegroundBrowserReady triggers it.
TEST_F(CrossPlatformPromosNotificationClientTest, HandleInteraction_ColdStart) {
  // No browser initially.
  EXPECT_CALL(*mock_service(), ShowCPEPromo(_)).Times(0);

  UNNotificationResponse* response = CreateMockNotificationResponse(
      desktop_to_mobile_promos::PromoType::kPassword);

  EXPECT_TRUE(client_->HandleNotificationInteraction(response));
  histogram_tester_.ExpectUniqueSample(
      "IOS.CrossPlatformPromos.PushNotification.Interaction",
      desktop_to_mobile_promos::PromoType::kPassword, 1);
  histogram_tester_.ExpectTotalCount(
      "IOS.CrossPlatformPromos.Promo.Shown.FromPush", 0);

  // Now create browser and signal ready.
  CreateBrowser();
  EXPECT_CALL(*mock_service(), ShowCPEPromo(browser_.get()));

  client_->OnSceneActiveForegroundBrowserReady();
  histogram_tester_.ExpectUniqueSample(
      "IOS.CrossPlatformPromos.Promo.Shown.FromPush",
      desktop_to_mobile_promos::PromoType::kPassword, 1);
}

// Tests CanHandleNotification.
TEST_F(CrossPlatformPromosNotificationClientTest, CanHandleNotification) {
  UNNotificationResponse* response = CreateMockNotificationResponse(
      desktop_to_mobile_promos::PromoType::kPassword);
  EXPECT_TRUE(client_->CanHandleNotification(response.notification));
}

// Tests GetNotificationType.
TEST_F(CrossPlatformPromosNotificationClientTest, GetNotificationType) {
  UNNotificationResponse* response = CreateMockNotificationResponse(
      desktop_to_mobile_promos::PromoType::kPassword);
  auto type = client_->GetNotificationType(response.notification);
  ASSERT_TRUE(type.has_value());
  EXPECT_EQ(type.value(), NotificationType::kCrossPlatformPromoPasswords);
}
