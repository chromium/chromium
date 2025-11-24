// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_service.h"

#import <UIKit/UIKit.h>

#import "base/functional/bind.h"
#import "base/json/values_util.h"
#import "base/test/task_environment.h"
#import "base/time/clock.h"
#import "base/time/time.h"
#import "components/desktop_to_mobile_promos/pref_names.h"
#import "components/desktop_to_mobile_promos/promos_types.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/sync_device_info/device_info.h"
#import "components/sync_device_info/fake_device_info_sync_service.h"
#import "components/sync_device_info/fake_device_info_tracker.h"
#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Expects `time1` and `time2` to be within `delta` of each other.
#define EXPECT_TIME_WITHIN(time1, time2, delta) \
  EXPECT_LT((time1 - time2).magnitude(), delta)

// Test suite for the `CrossPlatformPromosService`.
class CrossPlatformPromosServiceTest : public PlatformTest {
 public:
  CrossPlatformPromosServiceTest() {
    TestProfileIOS::Builder builder;

    builder.AddTestingFactory(
        DeviceInfoSyncServiceFactory::GetInstance(),
        base::BindOnce(
            &CrossPlatformPromosServiceTest::CreateFakeDeviceInfoSyncService,
            base::Unretained(this)));

    profile_ = std::move(builder).Build();
    prefs_ = profile_->GetPrefs();

    // Get the fake service instance created by the factory.
    device_info_sync_service_ = static_cast<syncer::FakeDeviceInfoSyncService*>(
        DeviceInfoSyncServiceFactory::GetForProfile(profile_.get()));

    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());

    // Set local device info using the injected fake service.
    fake_device_info_tracker_ = static_cast<syncer::FakeDeviceInfoTracker*>(
        device_info_sync_service_->GetDeviceInfoTracker());
    local_device_info_ = device_info_sync_service_->GetLocalDeviceInfoProvider()
                             ->GetLocalDeviceInfo();
    fake_device_info_tracker_->Add(local_device_info_);
    local_device_guid_ = local_device_info_->guid();

    service_ = std::make_unique<CrossPlatformPromosService>(profile_.get());

    StubPrepareToPresentModal();
  }

  // Factory function to create the FakeDeviceInfoSyncService.
  std::unique_ptr<KeyedService> CreateFakeDeviceInfoSyncService(
      ProfileIOS* context) {
    return std::make_unique<syncer::FakeDeviceInfoSyncService>();
  }

  // Simulate that the app was foregrounded.
  void SimulateAppForegrounded() {
    service_->OnApplicationWillEnterForeground();
  }

  // Creates a mock command handler and starts dispatching to it.
  id MockHandler(Protocol* protocol) {
    id mock_handler = OCMProtocolMock(protocol);
    [browser_->GetCommandDispatcher() startDispatchingToTarget:mock_handler
                                                   forProtocol:protocol];
    return mock_handler;
  }

  // Stubs the `-prepareToPresentModalWithSnackbarDismissal:` method from
  // `ApplicationCommands` so that it immediately calls the completion block.
  void StubPrepareToPresentModal() {
    id mock_application_handler = MockHandler(@protocol(ApplicationCommands));
    OCMStub([mock_application_handler
        prepareToPresentModalWithSnackbarDismissal:NO
                                        completion:[OCMArg invokeBlock]]);
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<PrefService> prefs_;
  raw_ptr<syncer::FakeDeviceInfoSyncService> device_info_sync_service_;
  raw_ptr<syncer::FakeDeviceInfoTracker> fake_device_info_tracker_;
  raw_ptr<const syncer::DeviceInfo> local_device_info_;
  std::unique_ptr<CrossPlatformPromosService> service_;
  std::unique_ptr<TestBrowser> browser_;
  std::string local_device_guid_;
};

// Tests that foregrounding the app records a new active day.
TEST_F(CrossPlatformPromosServiceTest, RecordActiveDay_AddNewDay) {
  SimulateAppForegrounded();
  const base::Value::List& active_days =
      prefs_->GetList(prefs::kCrossPlatformPromosActiveDays);
  EXPECT_EQ(1u, active_days.size());
}

// Tests that multiple app foregrounds doesn't add duplicate days.
TEST_F(CrossPlatformPromosServiceTest, RecordActiveDay_AddDuplicateDay) {
  // Advance to noon on a new day to avoid timezone issues at midnight.
  base::Time now = task_environment_.GetMockClock()->Now();
  base::Time tomorrow = (now + base::Days(1)).LocalMidnight();
  task_environment_.FastForwardBy((tomorrow - now) + base::Hours(12));

  SimulateAppForegrounded();
  task_environment_.FastForwardBy(base::Seconds(1));
  SimulateAppForegrounded();
  const base::Value::List& active_days =
      prefs_->GetList(prefs::kCrossPlatformPromosActiveDays);
  EXPECT_EQ(1u, active_days.size());
}

// Tests that old active days are pruned.
TEST_F(CrossPlatformPromosServiceTest, RecordActiveDay_PruneOldDays) {
  SimulateAppForegrounded();
  task_environment_.FastForwardBy(base::Days(30));
  SimulateAppForegrounded();
  const base::Value::List& active_days =
      prefs_->GetList(prefs::kCrossPlatformPromosActiveDays);
  EXPECT_EQ(1u, active_days.size());
  std::optional<base::Time> stored_time = base::ValueToTime(active_days[0]);
  EXPECT_LT(stored_time.value() - base::Time::Now(), base::Days(1));
}

// Tests that when the app becomes active for 16 days, the pref gets set.
TEST_F(CrossPlatformPromosServiceTest, OnAppDidBecomeActive) {
  // The pref should be unset initially.
  EXPECT_EQ(base::Time(),
            prefs_->GetTime(prefs::kCrossPlatformPromosIOS16thActiveDay));

  base::Time first_day = base::Time::Now();

  // Record 16 active days.
  for (int i = 0; i < 16; ++i) {
    SimulateAppForegrounded();
    task_environment_.FastForwardBy(base::Days(1));
  }

  // The pref should now be set to a Time within the last day.
  base::Time active_16th_day =
      prefs_->GetTime(prefs::kCrossPlatformPromosIOS16thActiveDay);
  EXPECT_TIME_WITHIN(active_16th_day, first_day, base::Days(1));
}

// Tests that the Lens promo is triggered when the pref changes.
TEST_F(CrossPlatformPromosServiceTest, MaybeShowPromo_Lens) {
  id mock_handler = MockHandler(@protocol(BrowserCoordinatorCommands));
  OCMExpect([mock_handler showLensPromo]);

  base::Value::Dict dict;
  dict.Set(prefs::kIOSPromoReminderPromoType,
           static_cast<int>(desktop_to_mobile_promos::PromoType::kLens));
  dict.Set(prefs::kIOSPromoReminderDeviceGUID, local_device_guid_);
  prefs_->SetDict(prefs::kIOSPromoReminder, std::move(dict));

  // Trigger the promo.
  service_->MaybeShowPromo();

  EXPECT_OCMOCK_VERIFY(mock_handler);
}

// Tests that the Enhanced Browsing promo is triggered when the pref changes.
TEST_F(CrossPlatformPromosServiceTest, MaybeShowPromo_ESB) {
  id mock_handler = MockHandler(@protocol(BrowserCoordinatorCommands));
  OCMExpect([mock_handler showEnhancedSafeBrowsingPromo]);

  base::Value::Dict dict;
  dict.Set(
      prefs::kIOSPromoReminderPromoType,
      static_cast<int>(desktop_to_mobile_promos::PromoType::kEnhancedBrowsing));
  dict.Set(prefs::kIOSPromoReminderDeviceGUID, local_device_guid_);
  prefs_->SetDict(prefs::kIOSPromoReminder, std::move(dict));

  // Trigger the promo.
  service_->MaybeShowPromo();

  EXPECT_OCMOCK_VERIFY(mock_handler);
}

// Tests that the Password promo is triggered when the pref changes.
TEST_F(CrossPlatformPromosServiceTest, MaybeShowPromo_Password) {
  id mock_handler = MockHandler(@protocol(CredentialProviderPromoCommands));
  OCMExpect(
      [mock_handler showCredentialProviderPromoWithTrigger:
                        CredentialProviderPromoTrigger::TipsNotification]);

  base::Value::Dict dict;
  dict.Set(prefs::kIOSPromoReminderPromoType,
           static_cast<int>(desktop_to_mobile_promos::PromoType::kPassword));
  dict.Set(prefs::kIOSPromoReminderDeviceGUID, local_device_guid_);
  prefs_->SetDict(prefs::kIOSPromoReminder, std::move(dict));

  // Trigger the promo.
  service_->MaybeShowPromo();

  EXPECT_OCMOCK_VERIFY(mock_handler);
}

// Tests that the promo type pref is cleared after showing a promo.
TEST_F(CrossPlatformPromosServiceTest, MaybeShowPromo_ClearsPref) {
  id mock_handler = MockHandler(@protocol(BrowserCoordinatorCommands));
  OCMStub([mock_handler showLensPromo]);

  base::Value::Dict dict;
  dict.Set(prefs::kIOSPromoReminderPromoType,
           static_cast<int>(desktop_to_mobile_promos::PromoType::kLens));
  dict.Set(prefs::kIOSPromoReminderDeviceGUID, local_device_guid_);
  prefs_->SetDict(prefs::kIOSPromoReminder, std::move(dict));

  // Trigger the promo.
  service_->MaybeShowPromo();

  // Verify the pref is cleared.
  const base::Value::Dict& promo_reminder =
      prefs_->GetDict(prefs::kIOSPromoReminder);
  EXPECT_FALSE(promo_reminder.FindInt(prefs::kIOSPromoReminderPromoType));
}

// Tests that the promo is NOT shown if the device GUID doesn't match.
TEST_F(CrossPlatformPromosServiceTest, MaybeShowPromo_WrongGUID) {
  id mock_handler = MockHandler(@protocol(BrowserCoordinatorCommands));
  OCMReject([mock_handler showLensPromo]);

  base::Value::Dict dict;
  dict.Set(prefs::kIOSPromoReminderPromoType,
           static_cast<int>(desktop_to_mobile_promos::PromoType::kLens));
  dict.Set(prefs::kIOSPromoReminderDeviceGUID, "wrong_guid");
  prefs_->SetDict(prefs::kIOSPromoReminder, std::move(dict));

  // Trigger the promo.
  service_->MaybeShowPromo();

  EXPECT_OCMOCK_VERIFY(mock_handler);
}
