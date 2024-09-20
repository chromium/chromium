// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bring_android_tabs/model/bring_android_tabs_to_ios_service.h"

#import <numeric>

#import "base/i18n/number_formatting.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/testing_pref_service.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/field_trial_register.h"
#import "components/segmentation_platform/public/result.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#import "components/sessions/core/serialized_navigation_entry_test_helper.h"
#import "components/sessions/core/session_id.h"
#import "components/sessions/core/session_types.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/sync/test/test_sync_service.h"
#import "components/sync_device_info/device_info.h"
#import "components/sync_device_info/fake_device_info_tracker.h"
#import "components/sync_sessions/open_tabs_ui_delegate.h"
#import "components/sync_sessions/session_sync_service.h"
#import "components/sync_sessions/session_sync_test_helper.h"
#import "components/sync_sessions/synced_session.h"
#import "ios/chrome/browser/bring_android_tabs/model/metrics.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_config.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"
#import "url/gurl.h"

namespace {

// Number of test foreign sessions from a phone.
const size_t kPhoneSessionCount = 2;
// Number of test foreign sessions from a tablet.
const size_t kTabletSessionCount = 1;
// Maximum number of tabs that should be imported.
const int kMaxNumberOfTabs = 20;
// Maximum number of tabs that should be instant loaded.
const int kMaxNumberOfInstantLoadedTabs = 6;
// Amount of duplicate foreign tabs per session to create when needed.
const int kNumberOfDuplicatesToCreatePerSession = 1;
// Test URL.
const std::string kTestURLPrefix = "https://url";
std::string GetTestURLSpec(int index) {
  return kTestURLPrefix + base::UTF16ToUTF8(base::FormatNumber(index)) + "/";
}
// Test title.
const std::u16string kTestTitlePrefix = u"title";
std::u16string GetTestTitle(int index) {
  return kTestTitlePrefix + base::FormatNumber(index);
}

}  // namespace

namespace bring_android_tabs {

// Fake DeviceSwitcherResultDispatcher that takes a `classification_label` as
// input. DeviceSwitcherResultDispatcher is a dependency of
// BringAndroidTabsToIOSService.
class FakeDeviceSwitcherResultDispatcher
    : public segmentation_platform::DeviceSwitcherResultDispatcher {
 public:
  FakeDeviceSwitcherResultDispatcher(
      segmentation_platform::SegmentationPlatformService* segmentation_service,
      syncer::DeviceInfoTracker* device_info_tracker,
      PrefService* prefs,
      segmentation_platform::FieldTrialRegister* field_trial_register,
      const char* classification_label)
      : DeviceSwitcherResultDispatcher(segmentation_service,
                                       device_info_tracker,
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
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetControllerDelegate,
              ());
};

// Mock OpenTabsUIDelegate that takes the time the SyncedSession was last
// modified as input and creates a fake open tab. OpenTabsUIDelegate is a
// dependency of SessionSyncService.
class MockOpenTabsUIDelegate : public sync_sessions::OpenTabsUIDelegate {
 public:
  MockOpenTabsUIDelegate(int tab_per_session, base::Time modified_time)
      : sync_sessions::OpenTabsUIDelegate(),
        tab_per_session_(tab_per_session),
        modified_time_(modified_time) {
    tab_index_ = 0;
    create_duplicates_ = false;
  }

  MOCK_METHOD(bool,
              GetAllForeignSessions,
              ((std::vector<raw_ptr<const sync_sessions::SyncedSession,
                                    VectorExperimental>>*)),
              (override));

  MOCK_METHOD(bool,
              GetForeignSessionTabs,
              (const std::string&, std::vector<const sessions::SessionTab*>*),
              (override));

  MOCK_METHOD(bool,
              GetForeignTab,
              (const std::string&, SessionID, const sessions::SessionTab**));

  MOCK_METHOD(void, DeleteForeignSession, (const std::string&));
  MOCK_METHOD(std::vector<const sessions::SessionWindow*>,
              GetForeignSession,
              (const std::string&));
  MOCK_METHOD(bool, GetLocalSession, (const sync_sessions::SyncedSession**));

  // Returns a fake tab with timestamp `modified_time_`.
  sessions::SessionTab* Tab(int index) {
    sessions::SerializedNavigationEntry entry = sessions::
        SerializedNavigationEntryTestHelper::CreateNavigationForTest();
    entry.set_virtual_url(GURL(GetTestURLSpec(index)));
    entry.set_title(GetTestTitle(index));

    sessions::SessionTab* tab = new sessions::SessionTab();
    SessionID session_id = SessionID::FromSerializedValue(100 * index + 1);
    tab->window_id = session_id;
    tab->tab_id = session_id;
    tab->tab_visual_index = 100;
    tab->current_navigation_index = 1000;
    tab->pinned = false;
    tab->extension_app_id = "fake";
    tab->user_agent_override.ua_string_override = "fake";
    tab->timestamp = modified_time_;
    tab->navigations = std::vector<sessions::SerializedNavigationEntry>{entry};
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

  // Mocks foreign sessions for GetAllForeignSessions() and
  // GetForeignSessionTabs(). Three sessions will be created, with two phone
  // sessions and one tablet session. There will be `tab_per_session_` tabs in
  // each session.
  void MockForeignSessions() {
    ON_CALL(*this, GetAllForeignSessions)
        .WillByDefault(
            [this](std::vector<raw_ptr<const sync_sessions::SyncedSession,
                                       VectorExperimental>>* sessions) {
              for (size_t i = 0; i < kPhoneSessionCount; i++) {
                sessions->push_back(
                    Session(sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
                            syncer::DeviceInfo::FormFactor::kPhone));
              }
              for (size_t i = 0; i < kTabletSessionCount; i++) {
                sessions->push_back(
                    Session(sync_pb::SyncEnums_DeviceType_TYPE_TABLET,
                            syncer::DeviceInfo::FormFactor::kTablet));
              }
              return true;
            });
    ON_CALL(*this, GetForeignSessionTabs)
        .WillByDefault([this](const std::string& tag,
                              std::vector<const sessions::SessionTab*>* tabs) {
          // Use tab_index_ to not have duplicate tab titles/URLs.
          for (int i = 0; i < tab_per_session_; i++) {
            tabs->push_back(Tab(tab_index_++));
          }

          if (create_duplicates_) {
            for (int i = 0; i < kNumberOfDuplicatesToCreatePerSession; i++) {
              tabs->push_back(Tab(i));
            }
          }

          return true;
        });
  }

  void SetCreateDuplicates(bool create_duplicates) {
    create_duplicates_ = create_duplicates;
  }

 private:
  int tab_per_session_;
  int tab_index_;
  bool create_duplicates_;
  base::Time modified_time_;
};

}  // namespace bring_android_tabs

// Test fixture for BringAndroidTabsToIOSService.
class BringAndroidTabsToIOSServiceTest : public PlatformTest {
 protected:
  BringAndroidTabsToIOSServiceTest() : PlatformTest() {
    FirstRun::RemoveSentinel();
    FirstRun::ClearStateForTesting();

    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    device_info_tracker_ = std::make_unique<syncer::FakeDeviceInfoTracker>();
    test_sync_service_ = std::make_unique<syncer::TestSyncService>();
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    segmentation_platform::DeviceSwitcherResultDispatcher::RegisterProfilePrefs(
        prefs_->registry());
    prefs_->registry()->RegisterBooleanPref(
        prefs::kIosBringAndroidTabsPromptDisplayed, false);
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
  }

  // Helper method that creates a fake OpenTabsUIDelegate for testing purpose.
  void SetUpOpenTabsUIDelegate(int tab_per_session, bool tabs_recently_active) {
    // Create the fake tab.
    base::Time session_time = tabs_recently_active
                                  ? base::Time::Now()
                                  : base::Time::Now() - base::Days(14);
    open_ui_delegate_ =
        std::make_unique<bring_android_tabs::MockOpenTabsUIDelegate>(
            tab_per_session, session_time);
    open_ui_delegate_->MockForeignSessions();
  }

  // Helper method that creates a fake OpenTabsUIDelegate with duplicate foreign
  // tabs for testing purposes.
  void SetUpOpenTabsUIDelegateWithDuplicates(int tab_per_session,
                                             bool tabs_recently_active) {
    base::Time session_time = tabs_recently_active
                                  ? base::Time::Now()
                                  : base::Time::Now() - base::Days(14);
    open_ui_delegate_ =
        std::make_unique<bring_android_tabs::MockOpenTabsUIDelegate>(
            tab_per_session, session_time);
    open_ui_delegate_->SetCreateDuplicates(true);
    open_ui_delegate_->MockForeignSessions();
  }

  // Helper method that creates an instance of BringAndroidTabsToIOSService and
  // loads the user's tabs. Also records that the prompt is displayed if at
  // least one tab is loaded.
  void SetUpBringAndroidTabsServiceAndLoadTabs(bool is_android_switcher) {
    // Create BringAndroidTabsToIOSService dependencies. These dependencies are
    // only used in `LoadTabs()`, therefore they can be scoped within this
    // method.
    auto session_sync_service =
        std::make_unique<bring_android_tabs::MockSessionSyncService>();
    ON_CALL(*session_sync_service, GetOpenTabsUIDelegate)
        .WillByDefault(testing::Return(open_ui_delegate_.get()));
    const char* classification_label =
        is_android_switcher
            ? segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel
            : segmentation_platform::DeviceSwitcherModel::kIosPhoneChromeLabel;
    segmentation_platform::IOSFieldTrialRegisterImpl field_trial_register_ =
        segmentation_platform::IOSFieldTrialRegisterImpl();
    bring_android_tabs::FakeDeviceSwitcherResultDispatcher dispatcher(
        &segmentation_platform_service_, device_info_tracker_.get(),
        prefs_.get(), &field_trial_register_, classification_label);

    // Create the BringAndroidTabsToIOSService and load tabs.
    bring_android_tabs_service_ =
        std::make_unique<BringAndroidTabsToIOSService>(
            &dispatcher, test_sync_service_.get(), session_sync_service.get(),
            prefs_.get());
    bring_android_tabs_service_->LoadTabs();
    // Record that the prompt has displayed.
    if (bring_android_tabs_service_->GetNumberOfAndroidTabs() > 0) {
      bring_android_tabs_service_->OnBringAndroidTabsPromptDisplayed();
    }
  }

  // Returns the fake Url Loader for testing purpose.
  FakeUrlLoadingBrowserAgent* GetTestUrlLoader() {
    return FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));
  }

  // Sets the data types that are synced.
  void SetSelectedTypes(syncer::UserSelectableTypeSet types) {
    test_sync_service_->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false,
        /*types=*/types);
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

  BringAndroidTabsToIOSService* bring_android_tabs_to_ios_service() {
    CHECK(bring_android_tabs_service_);
    return bring_android_tabs_service_.get();
  }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<bring_android_tabs::MockOpenTabsUIDelegate> open_ui_delegate_;
  std::unique_ptr<BringAndroidTabsToIOSService> bring_android_tabs_service_;
  // Service dependencies.
  testing::NiceMock<segmentation_platform::MockSegmentationPlatformService>
      segmentation_platform_service_;
  std::unique_ptr<syncer::TestSyncService> test_sync_service_;
  std::unique_ptr<syncer::FakeDeviceInfoTracker> device_info_tracker_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  base::HistogramTester histogram_tester_;
};

// Tests that no tabs are loaded when the user has not synced their tabs.
TEST_F(BringAndroidTabsToIOSServiceTest, UserNotSynced) {
  SetUpOpenTabsUIDelegate(/*tab_per_session=*/2, /*tabs_recently_active=*/true);
  // Set something other than `kTabs` as the selected type.
  SetSelectedTypes({syncer::UserSelectableType::kPasswords});
  SetUpBringAndroidTabsServiceAndLoadTabs(/*is_android_switcher=*/true);
  EXPECT_EQ(bring_android_tabs_to_ios_service()->GetNumberOfAndroidTabs(), 0u);
  ExpectHistogram(bring_android_tabs::PromptAttemptStatus::kTabSyncDisabled);
}

// Tests that no tabs are loaded when the user is not an android switcher.
TEST_F(BringAndroidTabsToIOSServiceTest, UserNotAndroidSwitcher) {
  SetUpOpenTabsUIDelegate(/*tab_per_session=*/2, /*tabs_recently_active=*/true);
  SetUpBringAndroidTabsServiceAndLoadTabs(/*is_android_switcher=*/false);
  EXPECT_EQ(bring_android_tabs_to_ios_service()->GetNumberOfAndroidTabs(), 0u);
}

// Tests that no tabs are loaded when the user's open tabs were not recently
// opened.
TEST_F(BringAndroidTabsToIOSServiceTest, UserDoesNotHaveRecentlyOpenedTabs) {
  SetUpOpenTabsUIDelegate(/*tab_per_session=*/2,
                          /*tabs_recently_active=*/false);
  SetUpBringAndroidTabsServiceAndLoadTabs(/*is_android_switcher=*/true);
  EXPECT_EQ(bring_android_tabs_to_ios_service()->GetNumberOfAndroidTabs(), 0u);
  ExpectHistogram(bring_android_tabs::PromptAttemptStatus::kNoActiveTabs);
}

// Tests that no tabs are loaded when the user has already seen the prompt in a
// previous session.
TEST_F(BringAndroidTabsToIOSServiceTest, UserHasSeenPrompt) {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    GTEST_SKIP() << "Feature unsupported on iPad";
  }
  size_t tab_per_session = 2;
  SetUpOpenTabsUIDelegate(/*tab_per_session=*/tab_per_session,
                          /*tabs_recently_active=*/true);
  // Load tabs and record prompt is displayed.
  SetUpBringAndroidTabsServiceAndLoadTabs(/*is_android_switcher=*/true);
  EXPECT_EQ(bring_android_tabs_to_ios_service()->GetNumberOfAndroidTabs(),
            tab_per_session * kPhoneSessionCount);
  // Simulate restart.
  SetUpBringAndroidTabsServiceAndLoadTabs(/*is_android_switcher=*/true);
  EXPECT_EQ(bring_android_tabs_to_ios_service()->GetNumberOfAndroidTabs(), 0u);
  ExpectHistogram(
      bring_android_tabs::PromptAttemptStatus::kPromptShownAndDismissed);
}

// Tests that the user's tab is loaded when they meet all criteria to be shown
// the Bring Android Tabs prompt. The prompt should only be shown on iPhone.
TEST_F(BringAndroidTabsToIOSServiceTest, UserMeetsAllCriteria) {
  int tab_per_session = 2;
  SetUpOpenTabsUIDelegate(/*tab_per_session=*/tab_per_session,
                          /*tabs_recently_active=*/true);
  size_t expected_number_of_tabs =
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE
          ? tab_per_session * kPhoneSessionCount
          : 0u;
  SetUpBringAndroidTabsServiceAndLoadTabs(/*is_android_switcher=*/true);
  EXPECT_EQ(bring_android_tabs_to_ios_service()->GetNumberOfAndroidTabs(),
            expected_number_of_tabs);
  ExpectHistogram(bring_android_tabs::PromptAttemptStatus::kSuccess);
}

// Tests that the first few tabs are instant loaded.
TEST_F(BringAndroidTabsToIOSServiceTest, InstantLoadFirstFewTabs) {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    GTEST_SKIP() << "Feature unsupported on iPad";
  }
  int tab_per_session = kMaxNumberOfInstantLoadedTabs / kPhoneSessionCount;
  SetUpOpenTabsUIDelegate(tab_per_session,
                          /*tabs_recently_active=*/true);
  SetUpBringAndroidTabsServiceAndLoadTabs(/*is_android_switcher=*/true);
  ASSERT_EQ(bring_android_tabs_to_ios_service()->GetNumberOfAndroidTabs(),
            static_cast<size_t>(kMaxNumberOfInstantLoadedTabs));
  ExpectHistogram(bring_android_tabs::PromptAttemptStatus::kSuccess);

  // Verify that the tabs are instant loaded.
  FakeUrlLoadingBrowserAgent* url_loader = GetTestUrlLoader();
  bring_android_tabs_to_ios_service()->OpenAllTabs(url_loader);
  EXPECT_EQ(kMaxNumberOfInstantLoadedTabs, url_loader->load_new_tab_call_count);
  EXPECT_EQ(GetTestURLSpec(kMaxNumberOfInstantLoadedTabs - 1),
            url_loader->last_params.web_params.url.spec());
  EXPECT_EQ(GetTestTitle(kMaxNumberOfInstantLoadedTabs - 1),
            url_loader->last_params.placeholder_title);
  EXPECT_TRUE(url_loader->last_params.instant_load);
}

// Tests that if there are more tabs than fitting in the device screen but less
// than maximum tabs that should be loaded, all tabs are opened, but the ones
// that overflow the screen have been lazy loaded.
TEST_F(BringAndroidTabsToIOSServiceTest, LazyLoadSomeInvisibleTabs) {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    GTEST_SKIP() << "Feature unsupported on iPad";
  }
  int tab_per_session = kMaxNumberOfInstantLoadedTabs / kPhoneSessionCount + 1;
  SetUpOpenTabsUIDelegate(tab_per_session, /*tabs_recently_active=*/true);
  SetUpBringAndroidTabsServiceAndLoadTabs(/*is_android_switcher=*/true);
  ExpectHistogram(bring_android_tabs::PromptAttemptStatus::kSuccess);

  // Open first 7 of the 8 tabs. The last tab should be lazy loaded.
  std::vector<size_t> indices(kMaxNumberOfInstantLoadedTabs + 1);
  std::iota(std::begin(indices), std::end(indices), 0);
  FakeUrlLoadingBrowserAgent* url_loader = GetTestUrlLoader();
  bring_android_tabs_to_ios_service()->OpenTabsAtIndices(indices, url_loader);
  EXPECT_EQ(kMaxNumberOfInstantLoadedTabs + 1,
            url_loader->load_new_tab_call_count);
  int last_tab_index = kMaxNumberOfInstantLoadedTabs;
  EXPECT_EQ(GetTestURLSpec(last_tab_index),
            url_loader->last_params.web_params.url.spec());
  EXPECT_EQ(GetTestTitle(last_tab_index),
            url_loader->last_params.placeholder_title);
  EXPECT_FALSE(url_loader->last_params.instant_load);
}

// Tests that when there are many foreign tabs, only 20 tabs would be brought
// over.
TEST_F(BringAndroidTabsToIOSServiceTest, AvoidOpenTooManyTabs) {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    GTEST_SKIP() << "Feature unsupported on iPad";
  }
  // We allow 20 tabs in total, so we set the number of sessions that exceeds
  // this number.
  int tab_per_session = kMaxNumberOfTabs / kPhoneSessionCount + 1;
  SetUpOpenTabsUIDelegate(tab_per_session,
                          /*tabs_recently_active=*/true);
  SetUpBringAndroidTabsServiceAndLoadTabs(/*is_android_switcher=*/true);
  EXPECT_EQ(bring_android_tabs_to_ios_service()->GetNumberOfAndroidTabs(),
            static_cast<size_t>(kMaxNumberOfTabs));
  ExpectHistogram(bring_android_tabs::PromptAttemptStatus::kSuccess);

  // Verify that only 20 tabs are loaded and can be opened.
  FakeUrlLoadingBrowserAgent* url_loader = GetTestUrlLoader();
  bring_android_tabs_to_ios_service()->OpenAllTabs(url_loader);
  EXPECT_EQ(kMaxNumberOfTabs, url_loader->load_new_tab_call_count);

  int last_tab_index = kMaxNumberOfTabs - 1;
  EXPECT_EQ(GetTestURLSpec(last_tab_index),
            url_loader->last_params.web_params.url.spec());
  EXPECT_EQ(GetTestTitle(last_tab_index),
            url_loader->last_params.placeholder_title);
  EXPECT_FALSE(url_loader->last_params.instant_load);
}

// Tests that when there are foreign tabs with repeating title and URLs, they
// are not opened.
TEST_F(BringAndroidTabsToIOSServiceTest, AvoidOpenDuplicateTabs) {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
    GTEST_SKIP() << "Feature unsupported on iPad";
  }

  int tab_per_session = 2;
  SetUpOpenTabsUIDelegateWithDuplicates(tab_per_session,
                                        /*tabs_recently_active=*/true);
  SetUpBringAndroidTabsServiceAndLoadTabs(/*is_android_switcher=*/true);

  EXPECT_EQ(bring_android_tabs_to_ios_service()->GetNumberOfAndroidTabs(),
            static_cast<size_t>(tab_per_session * kPhoneSessionCount));
  ExpectHistogram(bring_android_tabs::PromptAttemptStatus::kSuccess);
  FakeUrlLoadingBrowserAgent* url_loader = GetTestUrlLoader();
  bring_android_tabs_to_ios_service()->OpenAllTabs(url_loader);
  EXPECT_EQ(static_cast<int>(tab_per_session * kPhoneSessionCount),
            url_loader->load_new_tab_call_count);

  // Verify that only deduplicated tabs are loaded, by looking at the last
  // loaded tab.
  int last_tab_index = (tab_per_session * kPhoneSessionCount) - 1;
  EXPECT_EQ(GetTestURLSpec(last_tab_index),
            url_loader->last_params.web_params.url.spec());
  EXPECT_EQ(GetTestTitle(last_tab_index),
            url_loader->last_params.placeholder_title);
}
