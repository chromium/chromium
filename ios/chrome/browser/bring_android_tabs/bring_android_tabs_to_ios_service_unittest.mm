// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bring_android_tabs/bring_android_tabs_to_ios_service.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/testing_pref_service.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/field_trial_register.h"
#import "components/segmentation_platform/public/result.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#import "components/sessions/core/session_id.h"
#import "components/sessions/core/session_types.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/driver/sync_service.h"
#import "components/sync/driver/sync_user_settings.h"
#import "components/sync/test/test_sync_service.h"
#import "components/sync_device_info/device_info.h"
#import "components/sync_sessions/open_tabs_ui_delegate.h"
#import "components/sync_sessions/session_sync_service.h"
#import "components/sync_sessions/session_sync_test_helper.h"
#import "components/sync_sessions/synced_session.h"
#import "ios/chrome/browser/bring_android_tabs/features.h"
#import "ios/chrome/browser/bring_android_tabs/metrics.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/segmentation_platform/segmentation_platform_config.h"
#import "ios/chrome/browser/sync/session_sync_service_factory.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace bring_android_tabs {

// Fake DeviceSwitcherResultDispatcher that takes a `classification_label` as
// input. DeviceSwitcherResultDispatcher is a dependency of
// BringAndroidTabsToIOSService.
class FakeDeviceSwitcherResultDispatcher
    : public segmentation_platform::DeviceSwitcherResultDispatcher {
 public:
  FakeDeviceSwitcherResultDispatcher(
      segmentation_platform::SegmentationPlatformService* segmentation_service,
      syncer::SyncService* sync_service,
      PrefService* prefs,
      segmentation_platform::FieldTrialRegister* field_trial_register,
      const char* classification_label)
      : DeviceSwitcherResultDispatcher(segmentation_service,
                                       sync_service,
                                       prefs,
                                       field_trial_register) {
    classification_label_ = classification_label;
  }

  // Returns a classification result with a successful PredictionStatus and
  // label `classification_label`.
  segmentation_platform::ClassificationResult GetCachedClassificationResult()
      override {
    segmentation_platform::ClassificationResult classification_result =
        segmentation_platform::ClassificationResult(
            segmentation_platform::PredictionStatus::kSucceeded);
    classification_result.ordered_labels = {classification_label_};
    return classification_result;
  }

 private:
  const char* classification_label_;
};

// Mock SessionSyncService used to override the call to GetOpenTabsUIDelegate().
// SessionSyncService is a dependency of BringAndroidTabsToIOSService.
class MockSessionSyncService : public sync_sessions::SessionSyncService {
 public:
  MOCK_METHOD(sync_sessions::OpenTabsUIDelegate*,
              GetOpenTabsUIDelegate,
              (),
              (override));
  MOCK_METHOD(syncer::GlobalIdMapper*, GetGlobalIdMapper, (), (const));
  MOCK_METHOD(base::CallbackListSubscription,
              SubscribeToForeignSessionsChanged,
              (const base::RepeatingClosure&));
  MOCK_METHOD(base::WeakPtr<syncer::ModelTypeControllerDelegate>,
              GetControllerDelegate,
              ());
  MOCK_METHOD(void, ProxyTabsStateChanged, (syncer::DataTypeController::State));
};

// Mock OpenTabsUIDelegate that takes the time the SyncedSession was last
// modified as input and creates a fake open tab. OpenTabsUIDelegate is a
// dependency of SessionSyncService.
class MockOpenTabsUIDelegate : public sync_sessions::OpenTabsUIDelegate {
 public:
  MockOpenTabsUIDelegate(base::Time modified_time)
      : sync_sessions::OpenTabsUIDelegate() {
    modified_time_ = modified_time;
  }

  MOCK_METHOD(bool,
              GetAllForeignSessions,
              (std::vector<const sync_sessions::SyncedSession*>*),
              (override));

  MOCK_METHOD(bool,
              GetForeignSessionTabs,
              (const std::string&, std::vector<const sessions::SessionTab*>*),
              (override));

  MOCK_METHOD(bool,
              GetForeignTab,
              (const std::string&, SessionID, const sessions::SessionTab**));

  MOCK_METHOD(void, DeleteForeignSession, (const std::string&));
  MOCK_METHOD(bool,
              GetForeignSession,
              (const std::string&,
               std::vector<const sessions::SessionWindow*>*));
  MOCK_METHOD(bool, GetLocalSession, (const sync_sessions::SyncedSession**));

  // Returns a fake tab with timestamp `modified_time_`.
  sessions::SessionTab* Tab(SessionID session_id) {
    sessions::SessionTab* tab = new sessions::SessionTab();
    tab->window_id = session_id;
    tab->tab_id = session_id;
    tab->tab_visual_index = 100;
    tab->current_navigation_index = 1000;
    tab->pinned = false;
    tab->extension_app_id = "fake";
    tab->user_agent_override.ua_string_override = "fake";
    tab->timestamp = modified_time_;
    tab->navigations.resize(100);
    tab->session_storage_persistent_id = "fake";
    return tab;
  }

  // Returns a fake session with modified time `modified_time_` and form factor
  // type `device_form_factor`.
  sync_sessions::SyncedSession* Session(
      sync_pb::SyncEnums::DeviceType device_type,
      syncer::DeviceInfo::FormFactor device_form_factor) {
    sync_sessions::SyncedSession* session = new sync_sessions::SyncedSession();
    session->SetDeviceTypeAndFormFactor(device_type, device_form_factor);
    session->SetModifiedTime(modified_time_);
    return session;
  }

  // Mocks the responses for GetAllForeignSessions() and
  // GetForeignSessionTabs().
  void MockResponses() {
    ON_CALL(*this, GetAllForeignSessions)
        .WillByDefault([this](std::vector<const sync_sessions::SyncedSession*>*
                                  sessions) {
          sessions->push_back(Session(sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
                                      syncer::DeviceInfo::FormFactor::kPhone));
          sessions->push_back(Session(sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
                                      syncer::DeviceInfo::FormFactor::kPhone));
          sessions->push_back(Session(sync_pb::SyncEnums_DeviceType_TYPE_TABLET,
                                      syncer::DeviceInfo::FormFactor::kTablet));
          return true;
        });
    ON_CALL(*this, GetForeignSessionTabs)
        .WillByDefault([this](const std::string& tag,
                              std::vector<const sessions::SessionTab*>* tabs) {
          tabs->push_back(Tab(SessionID::FromSerializedValue(100)));
          tabs->push_back(Tab(SessionID::FromSerializedValue(150)));
          return true;
        });
  }

 private:
  base::Time modified_time_;
};

}  // namespace bring_android_tabs

// Test fixture for BringAndroidTabsToIOSService.
class BringAndroidTabsToIOSServiceTest : public PlatformTest {
 protected:
  BringAndroidTabsToIOSServiceTest() : PlatformTest() {
    test_sync_service_ = std::make_unique<syncer::TestSyncService>();
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    segmentation_platform::DeviceSwitcherResultDispatcher::RegisterProfilePrefs(
        prefs_->registry());
    prefs_->registry()->RegisterBooleanPref(
        prefs::kIosBringAndroidTabsPromptDisplayed, false);
  }

  // Helper method that creates an instance of BringAndroidTabsToIOSService,
  // loads the user's tabs, and returns the number of tabs loaded (if any).
  // Also records that the prompt is displayed if at least one tab is loaded.
  int NumberOfTabsLoaded(bool is_android_switcher, bool tabs_recently_active) {
    // Enable feature.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kBringYourOwnTabsIOS);

    // Create the fake tab.
    base::Time session_time = tabs_recently_active
                                  ? base::Time::Now()
                                  : base::Time::Now() - base::Days(14);
    auto tabs_delegate =
        std::make_unique<bring_android_tabs::MockOpenTabsUIDelegate>(
            session_time);
    tabs_delegate->MockResponses();

    // Create BringAndroidTabsToIOSService dependencies.
    auto session_sync_service =
        std::make_unique<bring_android_tabs::MockSessionSyncService>();
    ON_CALL(*session_sync_service, GetOpenTabsUIDelegate)
        .WillByDefault(testing::Return(tabs_delegate.get()));
    const char* classification_label =
        is_android_switcher
            ? segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel
            : segmentation_platform::DeviceSwitcherModel::kIosPhoneChromeLabel;
    segmentation_platform::IOSFieldTrialRegisterImpl field_trial_register_ =
        segmentation_platform::IOSFieldTrialRegisterImpl();
    bring_android_tabs::FakeDeviceSwitcherResultDispatcher dispatcher(
        &segmentation_platform_service_, test_sync_service_.get(), prefs_.get(),
        &field_trial_register_, classification_label);

    // Create the BringAndroidTabsToIOSService and load tabs.
    BringAndroidTabsToIOSService bring_android_tabs_service(
        &dispatcher, test_sync_service_.get(), session_sync_service.get(),
        prefs_.get());
    bring_android_tabs_service.LoadTabs();
    int number_of_tabs = bring_android_tabs_service.GetNumberOfAndroidTabs();
    // Record that the prompt has displayed.
    if (number_of_tabs > 0) {
      bring_android_tabs_service.OnBringAndroidTabsPromptDisplayed();
    }
    return number_of_tabs;
  }

  // Helper method that checks if `prompt_attempt_status` is recorded in the
  // histogram with name `kPromptAttemptStatusHistogramName`. The metrics
  // recording in BringAndroidTabsToIOSServiceTest is only done for iPhone
  // users.
  void ExpectHistogram(
      bring_android_tabs::PromptAttemptStatus prompt_attempt_status) {
    if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE) {
      histogram_tester_.ExpectBucketCount(
          bring_android_tabs::kPromptAttemptStatusHistogramName,
          static_cast<base::HistogramBase::Sample>(prompt_attempt_status), 1);
    }
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  testing::NiceMock<segmentation_platform::MockSegmentationPlatformService>
      segmentation_platform_service_;
  std::unique_ptr<syncer::TestSyncService> test_sync_service_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  base::HistogramTester histogram_tester_;
};

// Tests that no tabs are loaded when the user has not synced their tabs.
TEST_F(BringAndroidTabsToIOSServiceTest, UserNotSynced) {
  // Set something other than `kTabs` as the selected type.
  test_sync_service_->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet(
          syncer::UserSelectableType::kPasswords));
  EXPECT_EQ(NumberOfTabsLoaded(/*is_android_switcher=*/true,
                               /*tabs_recently_active=*/true),
            0);
  ExpectHistogram(bring_android_tabs::PromptAttemptStatus::kTabSyncDisabled);
}

// Tests that no tabs are loaded when the user is not an android switcher.
TEST_F(BringAndroidTabsToIOSServiceTest, UserNotAndroidSwitcher) {
  EXPECT_EQ(NumberOfTabsLoaded(/*is_android_switcher=*/false,
                               /*tabs_recently_active=*/true),
            0);
  ExpectHistogram(bring_android_tabs::PromptAttemptStatus::kNotAndroidSwitcher);
}

// Tests that no tabs are loaded when the user's open tabs were not recently
// opened.
TEST_F(BringAndroidTabsToIOSServiceTest, UserDoesNotHaveRecentlyOpenedTabs) {
  EXPECT_EQ(NumberOfTabsLoaded(/*is_android_switcher=*/true,
                               /*tabs_recently_active=*/false),
            0);
  ExpectHistogram(bring_android_tabs::PromptAttemptStatus::kNoActiveTabs);
}

// Tests that no tabs are loaded when the user has already seen the prompt in a
// previous session.
TEST_F(BringAndroidTabsToIOSServiceTest, UserHasSeenPrompt) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE) {
    // Load tabs and record prompt is displayed.
    EXPECT_EQ(NumberOfTabsLoaded(/*is_android_switcher=*/true,
                                 /*tabs_recently_active=*/true),
              4);
    // Reload tabs.
    EXPECT_EQ(NumberOfTabsLoaded(/*is_android_switcher=*/true,
                                 /*tabs_recently_active=*/true),
              0);
    ExpectHistogram(
        bring_android_tabs::PromptAttemptStatus::kPromptShownAndDismissed);
  }
}

// Tests that the user's tab is loaded when they meet all criteria to be shown
// the Bring Android Tabs prompt. The prompt should only be shown on iPhone.
TEST_F(BringAndroidTabsToIOSServiceTest, UserMeetsAllCriteria) {
  int expected_number_of_tabs =
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE ? 4 : 0;
  EXPECT_EQ(NumberOfTabsLoaded(/*is_android_switcher=*/true,
                               /*tabs_recently_active=*/true),
            expected_number_of_tabs);
  ExpectHistogram(bring_android_tabs::PromptAttemptStatus::kSuccess);
}
