// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/coordinator/quick_delete_other_data_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/regional_capabilities/regional_capabilities_utils.h"
#import "components/search_engines/search_engines_test_environment.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_data_util.h"
#import "components/search_engines/template_url_prepopulate_data.h"
#import "components/search_engines/template_url_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/coordinator/quick_delete_util.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/ui/quick_delete_other_data_consumer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

using quick_delete_util::DefaultSearchEngineState;

class QuickDeleteOtherDataMediatorTest : public PlatformTest {
 public:
  QuickDeleteOtherDataMediatorTest() {
    TestProfileIOS::Builder builder;
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

    template_url_service_ =
        search_engines_test_environment_.template_url_service();
    template_url_service_->Load();

    consumer_ = OCMProtocolMock(@protocol(QuickDeleteOtherDataConsumer));
  }

  // Creates a QuickDeleteOtherDataMediator.
  void CreateMediator() {
    [mediator_ disconnect];
    mediator_ = [[QuickDeleteOtherDataMediator alloc]
        initWithAuthenticationService:auth_service_
                      identityManager:IdentityManagerFactory::GetForProfile(
                                          profile_.get())
                   templateURLService:template_url_service_];
  }

  void TearDown() override {
    [mediator_ disconnect];
    PlatformTest::TearDown();
  }

  // Sets the default search engine (DSE) to Google.
  void SetDseToGoogle() {
    // Verify if DSE is already Google.
    if (template_url_service_->GetDefaultSearchProvider()->GetEngineType(
            template_url_service_->search_terms_data()) ==
        SEARCH_ENGINE_GOOGLE) {
      return;
    }
    SetDseToPrepopulatedEngine(SEARCH_ENGINE_GOOGLE);
  }

  // Sets the default search engine to a not prepopulated search engine which is
  // not Google.
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

  // Sets the default search engine to a prepopulated search engine which is not
  // Google.
  void SetDseToPrepopulatedEngine(SearchEngineType engine_type) {
    auto prepopulated_engines =
        regional_capabilities::GetAllPrepopulatedEngines();
    TemplateURL* defaultSearchEngine = nullptr;
    for (const auto* engine : prepopulated_engines) {
      if (engine->type == engine_type) {
        defaultSearchEngine =
            template_url_service_->Add(std::make_unique<TemplateURL>(
                *TemplateURLDataFromPrepopulatedEngine(*engine)));

        template_url_service_->SetUserSelectedDefaultSearchProvider(
            defaultSearchEngine);
        break;
      }
    }
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  QuickDeleteOtherDataMediator* mediator_;
  id consumer_;
  id<SystemIdentity> fake_identity_;
  raw_ptr<AuthenticationService> auth_service_;
  raw_ptr<TemplateURLService> template_url_service_;
};

// Tests parameters for the QuickDeleteOtherDataMediatorPageTitleTest.
struct PageTitleTestParams {
  std::string test_name;
  DefaultSearchEngineState dse_state;
  int expected_title_id;
};

// Parameterized test fixture to verify that the "Quick Delete Other Data" page
// title is displayed correctly.
class QuickDeleteOtherDataMediatorPageTitleTest
    : public QuickDeleteOtherDataMediatorTest,
      public ::testing::WithParamInterface<PageTitleTestParams> {};

// Verifies that the consumer receives the correct title for the "Quick Delete
// Other Data" page based on the default search engine state.
TEST_P(QuickDeleteOtherDataMediatorPageTitleTest, TestOtherDataPageTitle) {
  const PageTitleTestParams& params = GetParam();

  // Set up the default search engine state.
  switch (params.dse_state) {
    case DefaultSearchEngineState::kError:
      template_url_service_->ApplyDefaultSearchChangeForTesting(
          nullptr, DefaultSearchManager::FROM_POLICY);
      CreateMediator();
      break;
    case DefaultSearchEngineState::kGoogle:
      SetDseToGoogle();
      CreateMediator();
      break;
    case DefaultSearchEngineState::kNotGoogle:
      SetDseToNonGoogle();
      CreateMediator();
      break;
  }

  OCMExpect([consumer_
      setOtherDataPageTitle:l10n_util::GetNSString(params.expected_title_id)]);

  mediator_.consumer = consumer_;

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Instantiates the test suite with various combinations of search engine states
// to ensure the UI string is correct.
INSTANTIATE_TEST_SUITE_P(
    AllVariants,
    QuickDeleteOtherDataMediatorPageTitleTest,
    ::testing::ValuesIn<PageTitleTestParams>({
        {.test_name = "Google",
         .dse_state = DefaultSearchEngineState::kGoogle,
         .expected_title_id = IDS_SETTINGS_OTHER_GOOGLE_DATA_TITLE},
        {.test_name = "NonGoogle",
         .dse_state = DefaultSearchEngineState::kNotGoogle,
         .expected_title_id = IDS_SETTINGS_OTHER_DATA_TITLE},
        {.test_name = "NullDSE",
         .dse_state = DefaultSearchEngineState::kError,
         .expected_title_id = IDS_SETTINGS_OTHER_DATA_TITLE},
    }),
    [](const ::testing::TestParamInfo<PageTitleTestParams>& info) {
      return info.param.test_name;
    });

// Verifies that the consumer receives the correct title for the "Quick Delete
// Other Data" page when the default search engine (DSE) changes.
TEST_F(QuickDeleteOtherDataMediatorTest, TestTitleWhenDseChanges) {
  // Set the default search engine to Google.
  SetDseToGoogle();

  CreateMediator();

  OCMExpect([consumer_
      setOtherDataPageTitle:l10n_util::GetNSString(
                                IDS_SETTINGS_OTHER_GOOGLE_DATA_TITLE)]);

  mediator_.consumer = consumer_;

  OCMExpect(
      [consumer_ setOtherDataPageTitle:l10n_util::GetNSString(
                                           IDS_SETTINGS_OTHER_DATA_TITLE)]);

  // Change the default search provider to a non-Google one.
  SetDseToNonGoogle();

  OCMExpect([consumer_
      setOtherDataPageTitle:l10n_util::GetNSString(
                                IDS_SETTINGS_OTHER_GOOGLE_DATA_TITLE)]);

  // Change the default search provider back to Google.
  SetDseToGoogle();

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests parameters for the QuickDeleteOtherDataMediatorSearchHistoryCellTest.
struct SearchHistoryCellTestParams {
  std::string test_name;
  DefaultSearchEngineState dse_state;
  bool prepopulated_search_engine;
  bool signed_in;
  bool expected_visibility;
  int expected_subtitle_id;
};

// Parameterized test fixture to verify that the "Search history" cell is
// displayed correctly with the correct subtitle.
class QuickDeleteOtherDataMediatorSearchHistoryCellTest
    : public QuickDeleteOtherDataMediatorTest,
      public ::testing::WithParamInterface<SearchHistoryCellTestParams> {
 protected:
  void setUpEnvironmentForParams(const SearchHistoryCellTestParams& params);
};

void QuickDeleteOtherDataMediatorSearchHistoryCellTest::
    setUpEnvironmentForParams(const SearchHistoryCellTestParams& params) {
  // Set up the default search engine state.
  switch (params.dse_state) {
    case DefaultSearchEngineState::kError:
      // Set the default engine to null.
      template_url_service_->ApplyDefaultSearchChangeForTesting(
          nullptr, DefaultSearchManager::FROM_POLICY);
      CreateMediator();
      break;
    case DefaultSearchEngineState::kGoogle:
      SetDseToGoogle();
      CreateMediator();
      break;
    case DefaultSearchEngineState::kNotGoogle:
      if (params.prepopulated_search_engine) {
        // Set the default search engine to Bing.
        SetDseToPrepopulatedEngine(SEARCH_ENGINE_BING);
      } else {
        // Set the default search engine to a not prepopulated search engine.
        SetDseToNonGoogle();
      }
      CreateMediator();
      break;
  }

  // Set up sign-in status.
  if (params.signed_in) {
    auth_service_->SignIn(fake_identity_,
                          signin_metrics::AccessPoint::kSettings);
  }
}

// Verifies that the consumer receives the correct visibility for the "Search
// history" cell based on the default search engine state and the user's sign-in
// status.
TEST_P(QuickDeleteOtherDataMediatorSearchHistoryCellTest, TestCellVisibility) {
  const SearchHistoryCellTestParams& params = GetParam();

  setUpEnvironmentForParams(params);

  OCMExpect(
      [consumer_ setShouldShowSearchHistoryCell:params.expected_visibility]);

  mediator_.consumer = consumer_;

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Verifies that the consumer receives the correct subtitle for the "Search
// history" cell based on the default search engine state and the user's sign-in
// status.
TEST_P(QuickDeleteOtherDataMediatorSearchHistoryCellTest, TestSubtitle) {
  const SearchHistoryCellTestParams& params = GetParam();

  setUpEnvironmentForParams(params);

  switch (params.dse_state) {
    case DefaultSearchEngineState::kError:
      // "Search history" cell is not shown when DSE is null.
      OCMExpect([consumer_ setSearchHistoryCellSubtitle:nil]);
      break;
    case DefaultSearchEngineState::kGoogle:
    case DefaultSearchEngineState::kNotGoogle:
      if (params.prepopulated_search_engine) {
        const TemplateURL* defaultSearchEngine =
            template_url_service_->GetDefaultSearchProvider();
        OCMExpect([consumer_
            setSearchHistoryCellSubtitle:l10n_util::GetNSStringF(
                                             params.expected_subtitle_id,
                                             defaultSearchEngine
                                                 ->short_name())]);
        break;
      }
      OCMExpect([consumer_
          setSearchHistoryCellSubtitle:l10n_util::GetNSString(
                                           params.expected_subtitle_id)]);
      break;
  }

  mediator_.consumer = consumer_;

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Instantiates the test suite with various combinations of search engine states
// and sign-in status to ensure the UI string and visibility are correct.
INSTANTIATE_TEST_SUITE_P(
    AllVariants,
    QuickDeleteOtherDataMediatorSearchHistoryCellTest,
    ::testing::ValuesIn<SearchHistoryCellTestParams>({
        {.test_name = "Google_SignedOut",
         .dse_state = DefaultSearchEngineState::kGoogle,
         .prepopulated_search_engine = false,
         .signed_in = false,
         .expected_visibility = false,
         .expected_subtitle_id = IDS_SETTINGS_MANAGE_IN_YOUR_GOOGLE_ACCOUNT},

        {.test_name = "Google_SignedIn",
         .dse_state = DefaultSearchEngineState::kGoogle,
         .prepopulated_search_engine = false,
         .signed_in = true,
         .expected_visibility = true,
         .expected_subtitle_id = IDS_SETTINGS_MANAGE_IN_YOUR_GOOGLE_ACCOUNT},

        {.test_name = "NonGoogle_SignedOut_PrepopulatedDse",
         .dse_state = DefaultSearchEngineState::kNotGoogle,
         .prepopulated_search_engine = true,
         .signed_in = false,
         .expected_visibility = true,
         .expected_subtitle_id =
             IDS_SETTINGS_CLEAR_NON_GOOGLE_SEARCH_HISTORY_PREPOPULATED_DSE},

        {.test_name = "NonGoogle_SignedIn_PrepopulatedDse",
         .dse_state = DefaultSearchEngineState::kNotGoogle,
         .prepopulated_search_engine = true,
         .signed_in = true,
         .expected_visibility = true,
         .expected_subtitle_id =
             IDS_SETTINGS_CLEAR_NON_GOOGLE_SEARCH_HISTORY_PREPOPULATED_DSE},

        {.test_name = "NonGoogle_SignedOut_NotPrepopulatedDse",
         .dse_state = DefaultSearchEngineState::kNotGoogle,
         .prepopulated_search_engine = false,
         .signed_in = false,
         .expected_visibility = true,
         .expected_subtitle_id =
             IDS_SETTINGS_CLEAR_NON_GOOGLE_SEARCH_HISTORY_NON_PREPOPULATED_DSE},

        {.test_name = "NonGoogle_SignedIn_NotPrepopulatedDse",
         .dse_state = DefaultSearchEngineState::kNotGoogle,
         .prepopulated_search_engine = false,
         .signed_in = true,
         .expected_visibility = true,
         .expected_subtitle_id =
             IDS_SETTINGS_CLEAR_NON_GOOGLE_SEARCH_HISTORY_NON_PREPOPULATED_DSE},

        {.test_name = "NullDSE_SignedOut",
         .dse_state = DefaultSearchEngineState::kError,
         .prepopulated_search_engine = false,
         .signed_in = false,
         .expected_visibility = false,
         .expected_subtitle_id = 0},

        {.test_name = "NullDSE_SignedIn",
         .dse_state = DefaultSearchEngineState::kError,
         .prepopulated_search_engine = false,
         .signed_in = true,
         .expected_visibility = false,
         .expected_subtitle_id = 0},
    }),
    [](const ::testing::TestParamInfo<SearchHistoryCellTestParams>& info) {
      return info.param.test_name;
    });

// Verifies that the consumer is told not to show the "My Activity" cell when
// the user is signed out.
TEST_F(QuickDeleteOtherDataMediatorTest,
       TestMyActivityCellVisibilityWhenUserIsSignedOut) {
  // Set the default search engine to Google.
  SetDseToGoogle();

  CreateMediator();

  OCMExpect([consumer_ setShouldShowMyActivityCell:NO]);

  mediator_.consumer = consumer_;

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Verifies that the consumer is told that the "My Activity" cell should be
// shown when the user is signed in.
TEST_F(QuickDeleteOtherDataMediatorTest,
       TestMyActivityCellVisibilityWhenUserIsSignedIn) {
  // Set the default search engine to Google.
  SetDseToGoogle();

  CreateMediator();

  OCMExpect([consumer_ setShouldShowMyActivityCell:YES]);

  auth_service_->SignIn(fake_identity_, signin_metrics::AccessPoint::kSettings);

  mediator_.consumer = consumer_;

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Verifies that the consumer is told to update the visibility of the "my
// activity" cell and the "Search history" cell when the user's sign-in status
// changes.
TEST_F(QuickDeleteOtherDataMediatorTest,
       TestCellVisibilityWhenSignInStatusChange) {
  // Set the default search engine to Google.
  SetDseToGoogle();

  CreateMediator();

  OCMExpect([consumer_ setShouldShowMyActivityCell:NO]);
  OCMExpect([consumer_ setShouldShowSearchHistoryCell:NO]);

  mediator_.consumer = consumer_;

  OCMExpect([consumer_ setShouldShowMyActivityCell:YES]);
  OCMExpect([consumer_ setShouldShowSearchHistoryCell:YES]);

  auth_service_->SignIn(fake_identity_, signin_metrics::AccessPoint::kSettings);

  OCMExpect([consumer_ setShouldShowMyActivityCell:NO]);
  OCMExpect([consumer_ setShouldShowSearchHistoryCell:NO]);

  auth_service_->SignOut(signin_metrics::ProfileSignout::kTest, nil);

  EXPECT_OCMOCK_VERIFY(consumer_);
}
