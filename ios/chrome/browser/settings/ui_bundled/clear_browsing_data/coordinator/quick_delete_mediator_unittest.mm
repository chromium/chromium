// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/coordinator/quick_delete_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/browsing_data/core/counters/autofill_counter.h"
#import "components/browsing_data/core/counters/history_counter.h"
#import "components/browsing_data/core/counters/passwords_counter.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/history/core/browser/history_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/search_engines_test_environment.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"
#import "ios/chrome/browser/browsing_data/model/tabs_counter.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/coordinator/quick_delete_util.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/model/fake_browsing_data_counter_wrapper_producer.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/public/features.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/ui/quick_delete_consumer.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

using quick_delete_util::DefaultSearchEngineState;

// Unittests for the Quick Delete Mediator, namely for testing the construction
// of the summaries that rely on counters for the several browsing data types
// that could be deleted in Quick Delete.
class QuickDeleteMediatorTest : public PlatformTest {
 public:
  QuickDeleteMediatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ios::HistoryServiceFactory::GetInstance(),
                              ios::HistoryServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    profile_ = std::move(builder).Build();

    auth_service_ = AuthenticationServiceFactory::GetForProfile(profile_.get());

    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    fake_identity_ = [FakeSystemIdentity fakeIdentity1];
    system_identity_manager->AddIdentity(fake_identity_);

    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    history_service_ = ios::HistoryServiceFactory::GetForProfile(
        profile_.get(), ServiceAccessType::EXPLICIT_ACCESS);

    password_store_ =
        new testing::NiceMock<password_manager::MockPasswordStoreInterface>();

    // Reset prefs related to quick delete so tests start with the same state
    // every time.
    ResetQuickDeletePrefs();

    consumer_ = OCMProtocolMock(@protocol(QuickDeleteConsumer));
    OCMStub([consumer_ setTimeRange:browsing_data::TimePeriod::LAST_HOUR]);
    OCMStub([consumer_
        setBrowsingDataSummary:l10n_util::GetNSString(
                                   IDS_CLEAR_BROWSING_DATA_CALCULATING)]);
    OCMStub([consumer_ setShouldShowFooter:NO]);
    OCMStub([consumer_ setHistorySelection:NO]);
    OCMStub([consumer_ setTabsSelection:NO]);
    OCMStub([consumer_ setSiteDataSelection:NO]);
    OCMStub([consumer_ setCacheSelection:NO]);
    if (!IsPasswordRemovalFromDeleteBrowsingDataEnabled()) {
      OCMStub([consumer_ setPasswordsSelection:NO]);
    }
    OCMStub([consumer_ setAutofillSelection:NO]);

    template_url_service_ =
        search_engines_test_environment_.template_url_service();
    CreateMediator(template_url_service_);
  }

  // Creates a QuickDeleteMediator with a given `template_url_service`.
  // `template_url_service` can be a nullptr.
  void CreateMediator(raw_ptr<TemplateURLService> template_url_service) {
    fake_browsing_data_counter_wrapper_producer_ =
        [[FakeBrowsingDataCounterWrapperProducer alloc]
            initWithProfile:profile_.get()];

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_.get());
    BrowsingDataRemover* browsing_data_remover =
        BrowsingDataRemoverFactory::GetForProfile(profile_.get());
    DiscoverFeedService* discover_feed_service =
        DiscoverFeedServiceFactory::GetForProfile(profile_.get());
    if (template_url_service) {
      template_url_service->Load();
    }
    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForProfile(profile_.get());

    mediator_ =
        [[QuickDeleteMediator alloc] initWithPrefs:profile_.get()->GetPrefs()
                browsingDataCounterWrapperProducer:
                    fake_browsing_data_counter_wrapper_producer_
                                   identityManager:identity_manager
                               browsingDataRemover:browsing_data_remover
                               discoverFeedService:discover_feed_service
                                templateURLService:template_url_service
                     canPerformRadialWipeAnimation:NO
                                   uiBlockerTarget:scene_state_
                          featureEngagementTracker:tracker];
  }

  // Creates a QuickDeleteMediator with a valid template URL Service.
  void CreateMediator() { CreateMediator(template_url_service_); }

  ~QuickDeleteMediatorTest() override {
    EXPECT_OCMOCK_VERIFY(consumer_);
    ResetQuickDeletePrefs();

    mediator_.consumer = nil;
    [mediator_ disconnect];
    mediator_ = nil;

    scene_state_ = nil;
    auth_service_ = nullptr;
    history_service_ = nullptr;
    template_url_service_ = nullptr;

    password_store_->ShutdownOnUIThread();
    password_store_ = nullptr;

    profile_ = nullptr;
  }

  PrefService* prefs() { return profile_->GetPrefs(); }

  void ResetQuickDeletePrefs() {
    prefs()->SetInteger(browsing_data::prefs::kDeleteTimePeriod,
                        static_cast<int>(browsing_data::TimePeriod::LAST_HOUR));
    prefs()->SetBoolean(browsing_data::prefs::kDeleteBrowsingHistory, false);
    prefs()->SetBoolean(browsing_data::prefs::kCloseTabs, false);
    prefs()->SetBoolean(browsing_data::prefs::kDeleteCookies, false);
    prefs()->SetBoolean(browsing_data::prefs::kDeleteCache, false);
    prefs()->SetBoolean(browsing_data::prefs::kDeletePasswords, false);
    prefs()->SetBoolean(browsing_data::prefs::kDeleteFormData, false);
  }

  browsing_data::TimePeriod time_range() {
    return static_cast<browsing_data::TimePeriod>(
        prefs()->GetInteger(browsing_data::prefs::kDeleteTimePeriod));
  }

  // Triggers the history callback passed to
  // `FakeBrowsingDataCounterWrapperProducer` with a `HistoryResult` with
  // `num_unique_domains`.
  void TriggerUpdateUICallbackForHistoryResults(int num_unique_domains) {
    // Add stub counter result for browsing history.
    browsing_data::HistoryCounter history_counter(
        history_service_,
        browsing_data::HistoryCounter::GetUpdatedWebHistoryServiceCallback(),
        nullptr);
    // Initialize the counter in advanced tab so the correct pref name is
    // returned.
    history_counter.Init(prefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
                         browsing_data::BrowsingDataCounter::ResultCallback());
    const browsing_data::HistoryCounter::HistoryResult historyResult(
        &history_counter, 0, false, false, "google.com", num_unique_domains);
    OCMExpect([consumer_
        setHistorySummary:quick_delete_util::GetCounterTextFromResult(
                              historyResult, time_range())]);
    [fake_browsing_data_counter_wrapper_producer_
        triggerUpdateUICallbackForResult:historyResult];
  }

  // Triggers the tabs callback passed to
  // `FakeBrowsingDataCounterWrapperProducer` with a `FinishedyResult` with
  // `num_tabs`.
  void TriggerUpdateUICallbackForTabsResults(int num_tabs) {
    TabsCounter tabs_counter(
        BrowserListFactory::GetForProfile(profile_.get()),
        SessionRestorationServiceFactory::GetForProfile(profile_.get()));
    const TabsCounter::TabsResult tabsResult(&tabs_counter, num_tabs,
                                             /*num_windows=*/0, {});
    OCMExpect(
        [consumer_ setTabsSummary:quick_delete_util::GetCounterTextFromResult(
                                      tabsResult, time_range())]);
    [fake_browsing_data_counter_wrapper_producer_
        triggerUpdateUICallbackForResult:tabsResult];
  }

  // Triggers the passwords callback passed to
  // `FakeBrowsingDataCounterWrapperProducer` with a `PasswordsResult` with
  // `num_passwords`.
  void TriggerUpdateUICallbackForPasswordsResults(int num_passwords) {
    // Add stub counter result for Passwords.
    browsing_data::PasswordsCounter passwords_counter(
        password_store_.get(), nullptr, nullptr, nullptr);
    const browsing_data::PasswordsCounter::PasswordsResult passwordsResult(
        &passwords_counter, num_passwords, 0, false,
        std::vector<std::string>(num_passwords, "test.com"), {});
    OCMExpect([consumer_
        setPasswordsSummary:quick_delete_util::GetCounterTextFromResult(
                                passwordsResult, time_range())]);
    [fake_browsing_data_counter_wrapper_producer_
        triggerUpdateUICallbackForResult:passwordsResult];
  }

  // Triggers the autofill callback passed to
  // `FakeBrowsingDataCounterWrapperProducer` with an `AutofillResult` with
  // `num_suggestions`, `num_cards` and `num_addresses`.
  void TriggerUpdateUICallbackForAutofillResults(int num_suggestions,
                                                 int num_cards,
                                                 int num_addresses) {
    browsing_data::AutofillCounter autofill_counter(nullptr, nullptr, nullptr,
                                                    nullptr);
    const browsing_data::AutofillCounter::AutofillResult autofillResult(
        &autofill_counter, num_suggestions, num_cards, num_addresses,
        /*num_entities=*/0, false);
    OCMExpect([consumer_
        setAutofillSummary:quick_delete_util::GetCounterTextFromResult(
                               autofillResult, time_range())]);
    [fake_browsing_data_counter_wrapper_producer_
        triggerUpdateUICallbackForResult:autofillResult];
  }

  // Sets the default search engine to not be Google.
  void SetDseToNonGoogle() {
    TemplateURLData non_google_provider_data;
    non_google_provider_data.SetURL(
        "https://www.nongoogle.com/?q={searchTerms}");
    non_google_provider_data.suggestions_url =
        "https://www.nongoogle.com/suggest/?q={searchTerms}";

    auto* non_google_provider = template_url_service_->Add(
        std::make_unique<TemplateURL>(non_google_provider_data));
    template_url_service_->SetUserSelectedDefaultSearchProvider(
        non_google_provider);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  QuickDeleteMediator* mediator_;
  id consumer_;
  id<SystemIdentity> fake_identity_;
  SceneState* scene_state_;
  FakeBrowsingDataCounterWrapperProducer*
      fake_browsing_data_counter_wrapper_producer_;
  raw_ptr<AuthenticationService> auth_service_;
  raw_ptr<history::HistoryService> history_service_;
  raw_ptr<TemplateURLService> template_url_service_;
  scoped_refptr<password_manager::MockPasswordStoreInterface> password_store_;
};

// Tests the construction of the browsing history summary with different inputs.
TEST_F(QuickDeleteMediatorTest, TestBrowsingHistorySummary) {
  // Select browsing history for deletion.
  prefs()->SetBoolean(browsing_data::prefs::kDeleteBrowsingHistory, true);
  OCMExpect([consumer_ setHistorySelection:YES]);

  // Trigger creating the counters for browsing data types.
  mediator_.consumer = consumer_;

  // Trigger the callback for data types not in test. The summary is only
  // dispatches if all counters have returned.
  TriggerUpdateUICallbackForTabsResults(0);
  TriggerUpdateUICallbackForPasswordsResults(0);
  TriggerUpdateUICallbackForAutofillResults(0, 0, 0);

  // clang-format off
    const struct TestCase {
        int num_sites;
        bool sync_enabled;
        NSString* expected_output;
    } kTestCases[] = {
        {0, true, l10n_util::GetNSString(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_NO_DATA)},
        {0, false, l10n_util::GetNSString(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_NO_DATA)},
        {1, true, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITES_SYNCED, 1)},
        {1, false, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITES, 1)},
        {2, true, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITES_SYNCED, 2)},
        {2, false, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITES, 2)},
    };
  // clang-format on

  browsing_data::HistoryCounter counter(
      history_service_,
      browsing_data::HistoryCounter::GetUpdatedWebHistoryServiceCallback(),
      nullptr);
  // Initialize the counter in advanced tab so the correct pref name is
  // returned.
  counter.Init(prefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
               browsing_data::BrowsingDataCounter::ResultCallback());

  for (const TestCase& test_case : kTestCases) {
    const browsing_data::HistoryCounter::HistoryResult result(
        &counter, 0, test_case.sync_enabled, test_case.sync_enabled,
        "google.com", test_case.num_sites);
    OCMExpect([consumer_ setBrowsingDataSummary:test_case.expected_output]);
    OCMExpect([consumer_
        setHistorySummary:quick_delete_util::GetCounterTextFromResult(
                              result, time_range())]);
    [fake_browsing_data_counter_wrapper_producer_
        triggerUpdateUICallbackForResult:result];
    EXPECT_OCMOCK_VERIFY(consumer_);
  }
}

// Tests the construction of the tabs summary with different inputs.
TEST_F(QuickDeleteMediatorTest, TestTabsSummary) {
  // Select tabs for deletion.
  prefs()->SetBoolean(browsing_data::prefs::kCloseTabs, true);
  OCMExpect([consumer_ setTabsSelection:YES]);

  // Trigger creating the counters for browsing data types.
  mediator_.consumer = consumer_;

  // Trigger the callback for data types not in test. The summary is only
  // dispatches if all counters have returned.
  TriggerUpdateUICallbackForHistoryResults(0);
  TriggerUpdateUICallbackForPasswordsResults(0);
  TriggerUpdateUICallbackForAutofillResults(0, 0, 0);

  // clang-format off
    const struct TestCase {
        int num_tabs;
        int num_windows;
        NSString* expected_output;
    } kTestCases[] = {
        {0, 0, l10n_util::GetNSString(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_NO_DATA)},
        {0, 1, l10n_util::GetNSString(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_NO_DATA)},
        {1, 0, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_TABS,
                   1)},
        {1, 1, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_TABS,
                   1)},
        {2, 0, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_TABS,
                   2)},
        {2, 1, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_TABS,
                   2)},
    };
  // clang-format on

  TabsCounter counter(
      BrowserListFactory::GetForProfile(profile_.get()),
      SessionRestorationServiceFactory::GetForProfile(profile_.get()));

  for (const TestCase& test_case : kTestCases) {
    const TabsCounter::TabsResult result(&counter, test_case.num_tabs,
                                         test_case.num_windows, {});
    OCMExpect([consumer_ setBrowsingDataSummary:test_case.expected_output]);
    OCMExpect(
        [consumer_ setTabsSummary:quick_delete_util::GetCounterTextFromResult(
                                      result, time_range())]);
    [fake_browsing_data_counter_wrapper_producer_
        triggerUpdateUICallbackForResult:result];
    EXPECT_OCMOCK_VERIFY(consumer_);
  }
}

// Tests the construction of the passwords summary with different inputs.
TEST_F(QuickDeleteMediatorTest, TestPasswordsSummary) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kPasswordRemovalFromDeleteBrowsingData);

  // Select passwords for deletion.
  prefs()->SetBoolean(browsing_data::prefs::kDeletePasswords, true);
  OCMExpect([consumer_ setPasswordsSelection:YES]);

  // Trigger creating the counters for browsing data types.
  mediator_.consumer = consumer_;

  // Trigger the callback for data types not in test. The summary is only
  // dispatches if all counters have returned.
  TriggerUpdateUICallbackForTabsResults(0);
  TriggerUpdateUICallbackForHistoryResults(0);
  TriggerUpdateUICallbackForAutofillResults(0, 0, 0);

  // clang-format off
    const struct TestCase {
        int num_profile_passwords;
        int num_account_passwords;
        bool sync_enabled;
        NSString* expected_output;
    } kTestCases[] = {
        {0, 0, true, l10n_util::GetNSString(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_NO_DATA)},
        {0, 0, false, l10n_util::GetNSString(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_NO_DATA)},
        {1, 1, true, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_PASSWORDS,
                   2)},
        {1, 1, false, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_PASSWORDS,
                   2)},
        {2, 0, true, l10n_util::GetPluralNSStringF(
                        IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_PASSWORDS,
                        2)},
        {1, 0, false, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_PASSWORDS, 1)},
        {2, 0, false, l10n_util::GetPluralNSStringF(
                         IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_PASSWORDS, 2)},
    };
  // clang-format on

  browsing_data::PasswordsCounter counter(password_store_.get(), nullptr,
                                          nullptr, nullptr);

  for (const TestCase& test_case : kTestCases) {
    const browsing_data::PasswordsCounter::PasswordsResult result(
        &counter, test_case.num_profile_passwords,
        test_case.num_account_passwords, test_case.sync_enabled,
        std::vector<std::string>(test_case.num_profile_passwords, "test.com"),
        std::vector<std::string>(test_case.num_account_passwords, "test.com"));
    OCMExpect([consumer_ setBrowsingDataSummary:test_case.expected_output]);
    OCMExpect([consumer_
        setPasswordsSummary:quick_delete_util::GetCounterTextFromResult(
                                result, time_range())]);
    [fake_browsing_data_counter_wrapper_producer_
        triggerUpdateUICallbackForResult:result];
    EXPECT_OCMOCK_VERIFY(consumer_);
  }
}

// Tests the construction of the addresses summary with different inputs.
TEST_F(QuickDeleteMediatorTest, TestAddressesSummary) {
  // Select autofill for deletion.
  prefs()->SetBoolean(browsing_data::prefs::kDeleteFormData, true);

  OCMExpect([consumer_ setAutofillSelection:YES]);

  // Trigger creating the counters for browsing data types.
  mediator_.consumer = consumer_;

  // Trigger the callback for data types not in test. The summary is only
  // dispatches if all counters have returned.
  TriggerUpdateUICallbackForTabsResults(0);
  TriggerUpdateUICallbackForHistoryResults(0);
  TriggerUpdateUICallbackForPasswordsResults(0);

  // clang-format off
  const struct TestCase {
      int num_addresses;
      bool sync_enabled;
      NSString* expected_output;
  } kTestCases[] = {
        {0, true, l10n_util::GetNSString(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_NO_DATA)},
        {0, false, l10n_util::GetNSString(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_NO_DATA)},
        {1, true, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_ADRESSES, 1)},
        {1, false, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_ADRESSES, 1)},
        {2, true, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_ADRESSES, 2)},
        {2, false, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_ADRESSES, 2)},
  };
  // clang-format on

  browsing_data::AutofillCounter counter(nullptr, nullptr, nullptr, nullptr);

  for (const TestCase& test_case : kTestCases) {
    const browsing_data::AutofillCounter::AutofillResult result(
        &counter, 0, 0, test_case.num_addresses, /*num_entities=*/0,
        test_case.sync_enabled);
    OCMExpect([consumer_ setBrowsingDataSummary:test_case.expected_output]);
    OCMExpect([consumer_
        setAutofillSummary:quick_delete_util::GetCounterTextFromResult(
                               result, time_range())]);
    [fake_browsing_data_counter_wrapper_producer_
        triggerUpdateUICallbackForResult:result];
    EXPECT_OCMOCK_VERIFY(consumer_);
  }
}

// Tests the construction of the cards summary with different inputs.
TEST_F(QuickDeleteMediatorTest, TestCardsSummary) {
  // Select autofill for deletion.
  prefs()->SetBoolean(browsing_data::prefs::kDeleteFormData, true);

  OCMExpect([consumer_ setAutofillSelection:YES]);

  // Trigger creating the counters for browsing data types.
  mediator_.consumer = consumer_;

  // Trigger the callback for data types not in test. The summary is only
  // dispatches if all counters have returned.
  TriggerUpdateUICallbackForTabsResults(0);
  TriggerUpdateUICallbackForHistoryResults(0);
  if (!IsPasswordRemovalFromDeleteBrowsingDataEnabled()) {
    TriggerUpdateUICallbackForPasswordsResults(0);
  }

  // clang-format off
  const struct TestCase {
      int num_cards;
      bool sync_enabled;
      NSString* expected_output;
  } kTestCases[] = {
        {0, true, l10n_util::GetNSString(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_NO_DATA)},
        {0, false, l10n_util::GetNSString(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_NO_DATA)},
        {1, true, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_PAYMENT_METHODS, 1)},
        {1, false, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_PAYMENT_METHODS, 1)},
        {2, true, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_PAYMENT_METHODS, 2)},
        {2, false, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_PAYMENT_METHODS, 2)},
  };
  // clang-format on

  browsing_data::AutofillCounter counter(nullptr, nullptr, nullptr, nullptr);

  for (const TestCase& test_case : kTestCases) {
    const browsing_data::AutofillCounter::AutofillResult result(
        &counter, 0, test_case.num_cards, 0, /*num_entities=*/0,
        test_case.sync_enabled);

    OCMExpect([consumer_ setBrowsingDataSummary:test_case.expected_output]);
    OCMExpect([consumer_
        setAutofillSummary:quick_delete_util::GetCounterTextFromResult(
                               result, time_range())]);

    [fake_browsing_data_counter_wrapper_producer_
        triggerUpdateUICallbackForResult:result];
    EXPECT_OCMOCK_VERIFY(consumer_);
  }
}

// Tests the construction of the suggestions summary with different inputs.
TEST_F(QuickDeleteMediatorTest, TestSuggestionsSummary) {
  // Select autofill for deletion.
  prefs()->SetBoolean(browsing_data::prefs::kDeleteFormData, true);

  OCMExpect([consumer_ setAutofillSelection:YES]);

  // Trigger creating the counters for browsing data types.
  mediator_.consumer = consumer_;

  // Trigger the callback for data types not in test. The summary is only
  // dispatches if all counters have returned.
  TriggerUpdateUICallbackForTabsResults(0);
  TriggerUpdateUICallbackForHistoryResults(0);
  if (!IsPasswordRemovalFromDeleteBrowsingDataEnabled()) {
    TriggerUpdateUICallbackForPasswordsResults(0);
  }

  // clang-format off
  const struct TestCase {
      int num_suggestions;
      bool sync_enabled;
      NSString* expected_output;
  } kTestCases[] = {
        {0, true, l10n_util::GetNSString(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_NO_DATA)},
        {0, false, l10n_util::GetNSString(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_NO_DATA)},
        {1, true, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SUGGESTIONS,
                   1)},
        {1, false, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SUGGESTIONS, 1)},
        {2, true, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SUGGESTIONS,
                   2)},
        {2, false, l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SUGGESTIONS, 2)},
  };
  // clang-format on

  browsing_data::AutofillCounter counter(nullptr, nullptr, nullptr, nullptr);

  for (const TestCase& test_case : kTestCases) {
    const browsing_data::AutofillCounter::AutofillResult result(
        &counter, test_case.num_suggestions, 0, 0, /*num_entities=*/0,
        test_case.sync_enabled);
    OCMExpect([consumer_ setBrowsingDataSummary:test_case.expected_output]);
    OCMExpect([consumer_
        setAutofillSummary:quick_delete_util::GetCounterTextFromResult(
                               result, time_range())]);

    [fake_browsing_data_counter_wrapper_producer_
        triggerUpdateUICallbackForResult:result];
    EXPECT_OCMOCK_VERIFY(consumer_);
  }
}

TEST_F(QuickDeleteMediatorTest,
       TestBrowsingHistorySummaryWithPasswordsUnselected) {
  // Select browsing history for deletion, but not passwords.
  prefs()->SetBoolean(browsing_data::prefs::kDeleteBrowsingHistory, true);

  OCMExpect([consumer_ setHistorySelection:YES]);

  // Trigger creating the counters for browsing data types.
  mediator_.consumer = consumer_;

  // Trigger the callback for data types not in test. The summary is only
  // dispatches if all counters have returned.
  TriggerUpdateUICallbackForTabsResults(0);
  TriggerUpdateUICallbackForAutofillResults(0, 0, 0);

  int num_unique_domains = 2;
  int num_passwords = 1;

  // Since passwords is not selected for deletion, then it shouldn't be
  // returned.
  OCMExpect([consumer_
      setBrowsingDataSummary:l10n_util::GetPluralNSStringF(
                                 IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITES,
                                 num_unique_domains)]);

  TriggerUpdateUICallbackForHistoryResults(num_unique_domains);
  TriggerUpdateUICallbackForPasswordsResults(num_passwords);
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the correct summary is displayed with several types of data.
TEST_F(QuickDeleteMediatorTest, TestSummaryWithSeveralTypes) {
  // Select both browsing history and tabs for deletion.
  prefs()->SetBoolean(browsing_data::prefs::kDeleteBrowsingHistory, true);
  prefs()->SetBoolean(browsing_data::prefs::kCloseTabs, true);

  OCMExpect([consumer_ setHistorySelection:YES]);
  OCMExpect([consumer_ setTabsSelection:YES]);

  // Trigger creating the counters for browsing data types.
  mediator_.consumer = consumer_;

  // Trigger the callback for data types not in test. The summary is only
  // dispatched if all counters have returned.
  TriggerUpdateUICallbackForAutofillResults(0, 0, 0);
  if (!IsPasswordRemovalFromDeleteBrowsingDataEnabled()) {
    TriggerUpdateUICallbackForPasswordsResults(0);
  }

  int num_unique_domains = 2;
  int num_tabs = 1;

  // Both browsing history and tabs are selected for deletion and as such
  // should show up in the summary.
  NSString* expectedSummary = [NSString
      stringWithFormat:@"%@%@%@",
                       l10n_util::GetPluralNSStringF(
                           IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITES,
                           num_unique_domains),
                       l10n_util::GetNSString(
                           IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SEPARATOR),
                       l10n_util::GetPluralNSStringF(
                           IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_TABS,
                           num_tabs)];
  OCMExpect([consumer_ setBrowsingDataSummary:expectedSummary]);
  TriggerUpdateUICallbackForHistoryResults(num_unique_domains);
  TriggerUpdateUICallbackForTabsResults(num_tabs);
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that `setPasswordsSelection` is not called when the feature
// `kPasswordRemovalFromDeleteBrowsingData` is enabled.
TEST_F(QuickDeleteMediatorTest,
       SetPasswordsSelectionNotCalledWithThePasswordRemovalFeatureEnabled) {
  base::test::ScopedFeatureList feature_list(
      kPasswordRemovalFromDeleteBrowsingData);

  CreateMediator();

  // Regardless of the pref value, `setPasswordsSelection` will not be called by
  // the consumer. The initial value `NO` will not change when the feature flag
  // is on.
  prefs()->SetBoolean(browsing_data::prefs::kDeletePasswords, true);
  prefs()->SetBoolean(browsing_data::prefs::kDeleteCache, true);

  OCMReject([consumer_ setPasswordsSelection:[OCMArg any]]);
  OCMExpect([consumer_ setCacheSelection:YES]);

  mediator_.consumer = consumer_;
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the password counter is not created when the feature
// `kPasswordRemovalFromDeleteBrowsingData` is enabled.
TEST_F(QuickDeleteMediatorTest,
       NoPasswordCounterWithThePasswordRemovalFeatureEnabled) {
  base::test::ScopedFeatureList feature_list(
      kPasswordRemovalFromDeleteBrowsingData);

  CreateMediator();

  // Setting the consumers calls the `createCounters` method.
  mediator_.consumer = consumer_;

  const std::set<std::string>& registered_prefs =
      [fake_browsing_data_counter_wrapper_producer_ registeredPrefNames];

  // Expect counters for history, tabs, cache and autofill.
  EXPECT_THAT(
      registered_prefs,
      testing::UnorderedElementsAre(
          browsing_data::prefs::kDeleteBrowsingHistory,
          browsing_data::prefs::kCloseTabs, browsing_data::prefs::kDeleteCache,
          browsing_data::prefs::kDeleteFormData));

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the password counter is created when the feature
// `kPasswordRemovalFromDeleteBrowsingData` is disabled.
TEST_F(QuickDeleteMediatorTest,
       PasswordCounterCreatedWithThePasswordRemovalFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kPasswordRemovalFromDeleteBrowsingData);

  CreateMediator();

  // Setting the consumers calls the `createCounters` method.
  mediator_.consumer = consumer_;

  const std::set<std::string>& registered_prefs =
      [fake_browsing_data_counter_wrapper_producer_ registeredPrefNames];

  // Expect counters for history, tabs, cache, autofill and passwords.
  EXPECT_THAT(
      registered_prefs,
      testing::UnorderedElementsAre(
          browsing_data::prefs::kDeleteBrowsingHistory,
          browsing_data::prefs::kCloseTabs, browsing_data::prefs::kDeleteCache,
          browsing_data::prefs::kDeleteFormData,
          browsing_data::prefs::kDeletePasswords));

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Verifies that the consumer receives the correct title and subtitle for the
// "Manage other data" cell when the DSE changes.
TEST_F(QuickDeleteMediatorTest, TestStringsWhenDseChanges) {
  base::test::ScopedFeatureList feature_list(
      kPasswordRemovalFromDeleteBrowsingData);

  CreateMediator();

  // Verify that Google is the default search provider initially.
  ASSERT_EQ(SEARCH_ENGINE_GOOGLE,
            template_url_service_->GetDefaultSearchProvider()->GetEngineType(
                template_url_service_->search_terms_data()));

  // Keep a reference to the Google default search provider.
  const TemplateURL* google_provider =
      template_url_service_->GetDefaultSearchProvider();

  mediator_.consumer = consumer_;

  // Set expectations on `consumer_` for when the DSE is not Google.
  OCMExpect([consumer_
      setManageOtherDataTitle:l10n_util::GetNSString(
                                  IDS_SETTINGS_MANAGE_OTHER_DATA_LABEL)]);
  OCMExpect([consumer_
      setManageOtherDataSubtitle:
          l10n_util::GetNSString(IDS_SETTINGS_MANAGE_OTHER_DATA_SUB_LABEL)]);

  // Change the default search provider to a non-Google one.
  SetDseToNonGoogle();

  // Set expectations on `consumer_` for when the DSE is Google and the user is
  // signed out.
  OCMExpect([consumer_
      setManageOtherDataTitle:
          l10n_util::GetNSString(IDS_SETTINGS_MANAGE_OTHER_GOOGLE_DATA_LABEL)]);
  OCMExpect([consumer_
      setManageOtherDataSubtitle:l10n_util::GetNSString(
                                     IDS_SETTINGS_MANAGE_PASSWORDS_SUB_LABEL)]);

  // Change the default search provider back to Google.
  template_url_service_->SetUserSelectedDefaultSearchProvider(
      const_cast<TemplateURL*>(google_provider));

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Verifies that the consumer receives the correct title and subtitle for the
// "Manage other data" cell when the user's sign-in status changes.
TEST_F(QuickDeleteMediatorTest,
       TestStringsWhenSignInStatusChangesAndDseIsGoogle) {
  base::test::ScopedFeatureList feature_list(
      kPasswordRemovalFromDeleteBrowsingData);

  CreateMediator();

  // Verify that Google is the default search provider initially.
  ASSERT_EQ(SEARCH_ENGINE_GOOGLE,
            template_url_service_->GetDefaultSearchProvider()->GetEngineType(
                template_url_service_->search_terms_data()));

  mediator_.consumer = consumer_;

  // A change in the sign-in status doesn't impact the title.
  OCMReject([consumer_ setManageOtherDataTitle:[OCMArg any]]);
  OCMExpect([consumer_
      setManageOtherDataSubtitle:
          l10n_util::GetNSString(IDS_SETTINGS_MANAGE_OTHER_DATA_SUB_LABEL)]);

  auth_service_->SignIn(fake_identity_, signin_metrics::AccessPoint::kSettings);

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests parameters for the QuickDeleteMediatorManageOtherDataTest.
struct ManageOtherDataTestParams {
  std::string test_name;
  DefaultSearchEngineState dse_state;
  bool signed_in;
  int expected_title_id;
  int expected_subtitle_id;
};

// Parameterized test fixture to verify that the "Manage other data" cell
// displays the correct strings.
class QuickDeleteMediatorManageOtherDataTest
    : public QuickDeleteMediatorTest,
      public ::testing::WithParamInterface<ManageOtherDataTestParams> {};

// Verifies that the consumer receives the correct title and subtitle for the
// "Manage other data" cell based on the user's sign-in status and default
// search engine state.
TEST_P(QuickDeleteMediatorManageOtherDataTest, TestStrings) {
  base::test::ScopedFeatureList feature_list(
      kPasswordRemovalFromDeleteBrowsingData);
  const ManageOtherDataTestParams& params = GetParam();

  // Set up DSE.
  switch (params.dse_state) {
    case DefaultSearchEngineState::kError:
      CreateMediator(/*template_url_service=*/nullptr);
      break;
    case DefaultSearchEngineState::kGoogle:
      CreateMediator();  // Google is default in test environment.
      break;
    case DefaultSearchEngineState::kNotGoogle:
      SetDseToNonGoogle();
      CreateMediator();
      break;
  }

  // Set up sign-in status.
  if (params.signed_in) {
    auth_service_->SignIn(fake_identity_,
                          signin_metrics::AccessPoint::kSettings);
  }

  OCMExpect([consumer_ setManageOtherDataTitle:l10n_util::GetNSString(
                                                   params.expected_title_id)]);
  OCMExpect(
      [consumer_ setManageOtherDataSubtitle:l10n_util::GetNSString(
                                                params.expected_subtitle_id)]);

  mediator_.consumer = consumer_;

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Instantiates the test suite with various combinations of search engine states
// and sign-in statuses to ensure the UI strings are correct.
INSTANTIATE_TEST_SUITE_P(
    AllVariants,
    QuickDeleteMediatorManageOtherDataTest,
    ::testing::ValuesIn<ManageOtherDataTestParams>({
        {.test_name = "Google_SignedOut",
         .dse_state = DefaultSearchEngineState::kGoogle,
         .signed_in = false,
         .expected_title_id = IDS_SETTINGS_MANAGE_OTHER_GOOGLE_DATA_LABEL,
         .expected_subtitle_id = IDS_SETTINGS_MANAGE_PASSWORDS_SUB_LABEL},

        {.test_name = "Google_SignedIn",
         .dse_state = DefaultSearchEngineState::kGoogle,
         .signed_in = true,
         .expected_title_id = IDS_SETTINGS_MANAGE_OTHER_GOOGLE_DATA_LABEL,
         .expected_subtitle_id = IDS_SETTINGS_MANAGE_OTHER_DATA_SUB_LABEL},

        {.test_name = "NonGoogle",
         .dse_state = DefaultSearchEngineState::kNotGoogle,
         .signed_in = false,
         .expected_title_id = IDS_SETTINGS_MANAGE_OTHER_DATA_LABEL,
         .expected_subtitle_id = IDS_SETTINGS_MANAGE_OTHER_DATA_SUB_LABEL},

        {.test_name = "NullDSE_SignedOut",
         .dse_state = DefaultSearchEngineState::kError,
         .signed_in = false,
         .expected_title_id = IDS_SETTINGS_MANAGE_OTHER_DATA_LABEL,
         .expected_subtitle_id = IDS_SETTINGS_MANAGE_PASSWORDS_SUB_LABEL},

        {.test_name = "NullDSE_SignedIn",
         .dse_state = DefaultSearchEngineState::kError,
         .signed_in = true,
         .expected_title_id = IDS_SETTINGS_MANAGE_OTHER_DATA_LABEL,
         .expected_subtitle_id =
             IDS_IOS_CLEAR_BROWSING_DATA_MANAGE_OTHER_DATA_SUBTITLE_UNKNOWN_DSE},
    }),
    [](const ::testing::TestParamInfo<ManageOtherDataTestParams>& info) {
      return info.param.test_name;
    });
