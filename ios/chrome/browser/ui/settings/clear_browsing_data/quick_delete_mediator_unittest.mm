// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/browsing_data/core/counters/autofill_counter.h"
#import "components/browsing_data/core/counters/history_counter.h"
#import "components/browsing_data/core/counters/passwords_counter.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/history/core/browser/history_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"
#import "ios/chrome/browser/browsing_data/model/tabs_counter.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/fake_browsing_data_counter_wrapper_producer.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_consumer.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

// Unittests for the Quick Delete Mediator, namely for testing the construction
// of the summaries that rely on counters for the several browsing data types
// that could be deleted in Quick Delete.
class QuickDeleteMediatorTest : public PlatformTest {
 public:
  QuickDeleteMediatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ios::HistoryServiceFactory::GetInstance(),
                              ios::HistoryServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();

    history_service_ = ios::HistoryServiceFactory::GetForProfile(
        profile_.get(), ServiceAccessType::EXPLICIT_ACCESS);

    password_store_ =
        new testing::NiceMock<password_manager::MockPasswordStoreInterface>();

    // Reset prefs related to quick delete so tests start with the same state
    // every time.
    resetQuickDeletePrefs();

    consumer_ = OCMStrictProtocolMock(@protocol(QuickDeleteConsumer));
    OCMStub([consumer_ setTimeRange:browsing_data::TimePeriod::LAST_HOUR]);
    OCMStub([consumer_
        setBrowsingDataSummary:l10n_util::GetNSString(
                                   IDS_CLEAR_BROWSING_DATA_CALCULATING)]);
    OCMStub([consumer_ setShouldShowFooter:NO]);
    OCMStub([consumer_ setHistorySelection:NO]);
    OCMStub([consumer_ setTabsSelection:NO]);
    OCMStub([consumer_ setSiteDataSelection:NO]);
    OCMStub([consumer_ setCacheSelection:NO]);
    OCMStub([consumer_ setPasswordsSelection:NO]);
    OCMStub([consumer_ setAutofillSelection:NO]);

    fake_browsing_data_counter_wrapper_producer_ =
        [[FakeBrowsingDataCounterWrapperProducer alloc]
            initWithProfile:profile_.get()];

    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForProfile(profile_.get());
    BrowsingDataRemover* browsing_data_remover =
        BrowsingDataRemoverFactory::GetForProfile(profile_.get());
    DiscoverFeedService* discover_feed_service =
        DiscoverFeedServiceFactory::GetForProfile(profile_.get());

    mediator_ =
        [[QuickDeleteMediator alloc] initWithPrefs:profile_.get()->GetPrefs()
                browsingDataCounterWrapperProducer:
                    fake_browsing_data_counter_wrapper_producer_
                                   identityManager:identityManager
                               browsingDataRemover:browsing_data_remover
                               discoverFeedService:discover_feed_service
                    canPerformTabsClosureAnimation:NO];
  }

  ~QuickDeleteMediatorTest() override {
    EXPECT_OCMOCK_VERIFY(consumer_);
    resetQuickDeletePrefs();

    mediator_.consumer = nil;
    [mediator_ disconnect];
    mediator_ = nil;

    history_service_ = nil;

    task_environment_.RunUntilIdle();

    password_store_->ShutdownOnUIThread();
    password_store_ = nil;

    profile_ = nil;
  }

  PrefService* prefs() { return profile_->GetPrefs(); }

  void resetQuickDeletePrefs() {
    prefs()->SetInteger(browsing_data::prefs::kDeleteTimePeriod,
                        static_cast<int>(browsing_data::TimePeriod::LAST_HOUR));
    prefs()->SetBoolean(browsing_data::prefs::kDeleteBrowsingHistory, false);
    prefs()->SetBoolean(browsing_data::prefs::kCloseTabs, false);
    prefs()->SetBoolean(browsing_data::prefs::kDeleteCookies, false);
    prefs()->SetBoolean(browsing_data::prefs::kDeleteCache, false);
    prefs()->SetBoolean(browsing_data::prefs::kDeletePasswords, false);
    prefs()->SetBoolean(browsing_data::prefs::kDeleteFormData, false);
  }

  browsing_data::TimePeriod timeRange() {
    return static_cast<browsing_data::TimePeriod>(
        prefs()->GetInteger(browsing_data::prefs::kDeleteTimePeriod));
  }

  // Triggers the history callback passed to
  // `FakeBrowsingDataCounterWrapperProducer` with a `HistoryResult` with
  // `num_history_items`.
  void triggerUpdateUICallbackForHistoryResults(int num_history_items) {
    // Add stub counter result for browsing history.
    browsing_data::HistoryCounter historyCounter(
        history_service_,
        browsing_data::HistoryCounter::GetUpdatedWebHistoryServiceCallback(),
        nullptr);
    // Initialize the counter in advanced tab so the correct pref name is
    // returned.
    historyCounter.Init(prefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
                        browsing_data::BrowsingDataCounter::ResultCallback());
    const browsing_data::HistoryCounter::HistoryResult historyResult(
        &historyCounter, num_history_items, false, false);
    OCMExpect([consumer_
        setHistorySummary:quick_delete_util::GetCounterTextFromResult(
                              historyResult, timeRange())]);
    [fake_browsing_data_counter_wrapper_producer_
        triggerUpdateUICallbackForResult:historyResult];
  }

  // Triggers the tabs callback passed to
  // `FakeBrowsingDataCounterWrapperProducer` with a `FinishedyResult` with
  // `num_tabs`.
  void triggerUpdateUICallbackForTabsResults(int num_tabs) {
    TabsCounter tabsCounter(
        BrowserListFactory::GetForProfile(profile_.get()),
        SessionRestorationServiceFactory::GetForProfile(profile_.get()));
    const TabsCounter::TabsResult tabsResult(&tabsCounter, num_tabs,
                                             /*num_windows=*/0, {});
    OCMExpect(
        [consumer_ setTabsSummary:quick_delete_util::GetCounterTextFromResult(
                                      tabsResult, timeRange())]);
    [fake_browsing_data_counter_wrapper_producer_
        triggerUpdateUICallbackForResult:tabsResult];
  }

  // Triggers the passwords callback passed to
  // `FakeBrowsingDataCounterWrapperProducer` with a `PasswordsResult` with
  // `num_passwords`.
  void triggerUpdateUICallbackForPasswordsResults(int num_passwords) {
    // Add stub counter result for Passwords.
    browsing_data::PasswordsCounter passwordsCounter(password_store_.get(),
                                                     nullptr, nullptr, nullptr);
    const browsing_data::PasswordsCounter::PasswordsResult passwordsResult(
        &passwordsCounter, num_passwords, 0, false,
        std::vector<std::string>(num_passwords, "test.com"), {});
    OCMExpect([consumer_
        setPasswordsSummary:quick_delete_util::GetCounterTextFromResult(
                                passwordsResult, timeRange())]);
    [fake_browsing_data_counter_wrapper_producer_
        triggerUpdateUICallbackForResult:passwordsResult];
  }

  // Triggers the autofill callback passed to
  // `FakeBrowsingDataCounterWrapperProducer` with an `AutofillResult` with
  // `num_suggestions`, `num_cards` and `num_addresses`.
  void triggerUpdateUICallbackForAutofillResults(int num_suggestions,
                                                 int num_cards,
                                                 int num_addresses) {
    browsing_data::AutofillCounter autofillCounter(nullptr, nullptr, nullptr,
                                                   nullptr);
    const browsing_data::AutofillCounter::AutofillResult autofillResult(
        &autofillCounter, num_suggestions, num_cards, num_addresses, 0, false);
    OCMExpect([consumer_
        setAutofillSummary:quick_delete_util::GetCounterTextFromResult(
                               autofillResult, timeRange())]);
    [fake_browsing_data_counter_wrapper_producer_
        triggerUpdateUICallbackForResult:autofillResult];
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  QuickDeleteMediator* mediator_;
  id consumer_;
  raw_ptr<history::HistoryService> history_service_;
  scoped_refptr<password_manager::MockPasswordStoreInterface> password_store_;
  FakeBrowsingDataCounterWrapperProducer*
      fake_browsing_data_counter_wrapper_producer_;
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
  triggerUpdateUICallbackForTabsResults(0);
  triggerUpdateUICallbackForPasswordsResults(0);
  triggerUpdateUICallbackForAutofillResults(0, 0, 0);

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
        &counter, test_case.num_sites, test_case.sync_enabled,
        test_case.sync_enabled);
    OCMExpect([consumer_ setBrowsingDataSummary:test_case.expected_output]);
    OCMExpect([consumer_
        setHistorySummary:quick_delete_util::GetCounterTextFromResult(
                              result, timeRange())]);
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
  triggerUpdateUICallbackForHistoryResults(0);
  triggerUpdateUICallbackForPasswordsResults(0);
  triggerUpdateUICallbackForAutofillResults(0, 0, 0);

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
                                        result, timeRange())]);
      [fake_browsing_data_counter_wrapper_producer_
          triggerUpdateUICallbackForResult:result];
      EXPECT_OCMOCK_VERIFY(consumer_);
    }
}

// Tests the construction of the passwords summary with different inputs.
TEST_F(QuickDeleteMediatorTest, TestPasswordsSummary) {
  // Select passwords for deletion.
  prefs()->SetBoolean(browsing_data::prefs::kDeletePasswords, true);
  OCMExpect([consumer_ setPasswordsSelection:YES]);

  // Trigger creating the counters for browsing data types.
  mediator_.consumer = consumer_;

  // Trigger the callback for data types not in test. The summary is only
  // dispatches if all counters have returned.
  triggerUpdateUICallbackForTabsResults(0);
  triggerUpdateUICallbackForHistoryResults(0);
  triggerUpdateUICallbackForAutofillResults(0, 0, 0);

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
                                result, timeRange())]);
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
  triggerUpdateUICallbackForTabsResults(0);
  triggerUpdateUICallbackForHistoryResults(0);
  triggerUpdateUICallbackForPasswordsResults(0);

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
          &counter, 0, 0, test_case.num_addresses, 0, test_case.sync_enabled);
      OCMExpect([consumer_ setBrowsingDataSummary:test_case.expected_output]);
      OCMExpect([consumer_
          setAutofillSummary:quick_delete_util::GetCounterTextFromResult(
                                 result, timeRange())]);
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
  triggerUpdateUICallbackForTabsResults(0);
  triggerUpdateUICallbackForHistoryResults(0);
  triggerUpdateUICallbackForPasswordsResults(0);

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
          &counter, 0, test_case.num_cards, 0, 0, test_case.sync_enabled);

      OCMExpect([consumer_ setBrowsingDataSummary:test_case.expected_output]);
      OCMExpect([consumer_
          setAutofillSummary:quick_delete_util::GetCounterTextFromResult(
                                 result, timeRange())]);

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
  triggerUpdateUICallbackForTabsResults(0);
  triggerUpdateUICallbackForHistoryResults(0);
  triggerUpdateUICallbackForPasswordsResults(0);

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
          &counter, test_case.num_suggestions, 0, 0, 0, test_case.sync_enabled);
      OCMExpect([consumer_ setBrowsingDataSummary:test_case.expected_output]);
      OCMExpect([consumer_
          setAutofillSummary:quick_delete_util::GetCounterTextFromResult(
                                 result, timeRange())]);

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
  triggerUpdateUICallbackForTabsResults(0);
  triggerUpdateUICallbackForAutofillResults(0, 0, 0);

  int num_history_items = 2;
  int num_passwords = 1;

  // Since passwords is not selected for deletion, then it shouldn't be
  // returned.
  OCMExpect([consumer_
      setBrowsingDataSummary:l10n_util::GetPluralNSStringF(
                                 IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITES,
                                 num_history_items)]);

  triggerUpdateUICallbackForHistoryResults(num_history_items);
  triggerUpdateUICallbackForPasswordsResults(num_passwords);
  EXPECT_OCMOCK_VERIFY(consumer_);
}

TEST_F(QuickDeleteMediatorTest, TestSummaryWithSeveralTypes) {
  // Select both browsing history and passowrds for deletion.
  prefs()->SetBoolean(browsing_data::prefs::kDeleteBrowsingHistory, true);
  prefs()->SetBoolean(browsing_data::prefs::kDeletePasswords, true);

  OCMExpect([consumer_ setHistorySelection:YES]);
  OCMExpect([consumer_ setPasswordsSelection:YES]);

  // Trigger creating the counters for browsing data types.
  mediator_.consumer = consumer_;

  // Trigger the callback for data types not in test. The summary is only
  // dispatches if all counters have returned.
  triggerUpdateUICallbackForTabsResults(0);
  triggerUpdateUICallbackForAutofillResults(0, 0, 0);

  int num_history_items = 2;
  int num_passwords = 1;

  // Both browsing history and passwords are selected for deletion and as such
  // should show up in the summary.
  NSString* expectedSummary = [NSString
      stringWithFormat:@"%@%@%@",
                       l10n_util::GetPluralNSStringF(
                           IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITES,
                           num_history_items),
                       l10n_util::GetNSString(
                           IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SEPARATOR),
                       l10n_util::GetPluralNSStringF(
                           IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_PASSWORDS,
                           num_passwords)];
  OCMExpect([consumer_ setBrowsingDataSummary:expectedSummary]);
  triggerUpdateUICallbackForHistoryResults(num_history_items);
  triggerUpdateUICallbackForPasswordsResults(num_passwords);
  EXPECT_OCMOCK_VERIFY(consumer_);
}
