// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_mediator.h"

#import "base/base64url.h"
#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/feature_engagement/test/test_tracker.h"
#import "components/search_engines/search_engines_test_environment.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/lens_overlay/coordinator/fake_chrome_lens_overlay.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_omnibox_client.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_web_provider.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_toolbar_consumer.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_coordinator.h"
#import "ios/public/provider/chrome/browser/lens/lens_overlay_result.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

/// Fake LensWebProvider.
@interface FakeLensWebProviderImpl : NSObject <LensWebProvider>
@property(nonatomic, assign) web::WebState* webState;
@end
@implementation FakeLensWebProviderImpl
@end

@interface FakeResultConsumer : NSObject <LensOverlayResultConsumer>
@property(nonatomic, assign) GURL lastPushedURL;
@property(nonatomic, assign) web::FakeWebState* webState;

- (void)disconnect;
@end

@implementation FakeResultConsumer

- (void)loadResultsURL:(GURL)URL {
  self.lastPushedURL = URL;
  if (self.webState) {
    web::FakeNavigationContext navigationContext;
    navigationContext.SetWebState(self.webState);
    navigationContext.SetUrl(URL);
    navigationContext.SetIsSameDocument(NO);
    self.webState->OnNavigationStarted(&navigationContext);
    self.webState->OnNavigationFinished(&navigationContext);
  }
}

- (void)handleSearchRequestStarted {
  // NO-OP
}

- (void)handleSearchRequestErrored {
  // NO-OP
}

- (void)disconnect {
  self.webState = nil;
}

@end

namespace {

class LensOverlayMediatorTest : public PlatformTest {
 public:
  LensOverlayMediatorTest() {
    mediator_ = [[LensOverlayMediator alloc] initWithIsIncognito:NO];
    mediator_.templateURLService =
        search_engines_test_environment_.template_url_service();
    mock_omnibox_coordinator_ =
        [OCMockObject mockForClass:OmniboxCoordinator.class];
    mock_toolbar_consumer_ =
        [OCMockObject mockForProtocol:@protocol(LensToolbarConsumer)];
    mock_lens_commands_ =
        [OCMockObject mockForProtocol:@protocol(LensOverlayCommands)];
    fake_web_state_ = std::make_unique<web::FakeWebState>();

    fake_result_consumer_ = [[FakeResultConsumer alloc] init];
    fake_result_consumer_.webState = fake_web_state_.get();

    fake_chrome_lens_overlay_ = [[FakeChromeLensOverlay alloc] init];
    [fake_chrome_lens_overlay_ setLensOverlayDelegate:mediator_];
    [fake_chrome_lens_overlay_ start];

    tracker_ = feature_engagement::CreateTestTracker();

    fake_web_provider_ = [[FakeLensWebProviderImpl alloc] init];
    fake_web_provider_.webState = fake_web_state_.get();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();

    lens_omnibox_client_ = std::make_unique<LensOmniboxClient>(
        profile_.get(), tracker_.get(), fake_web_provider_, nil);

    mediator_.resultConsumer = fake_result_consumer_;
    mediator_.omniboxCoordinator = mock_omnibox_coordinator_;
    mediator_.toolbarConsumer = mock_toolbar_consumer_;
    mediator_.webState = fake_web_state_.get();
    mediator_.lensHandler = fake_chrome_lens_overlay_;
    mediator_.commandsHandler = mock_lens_commands_;
    mediator_.omniboxClient = lens_omnibox_client_.get();
  }

  ~LensOverlayMediatorTest() override {
    [mediator_ disconnect];
    [fake_result_consumer_ disconnect];
  }

 protected:
  /// Expects an omnibox focus event.
  void ExpectOmniboxFocus() {
    OCMExpect([mock_omnibox_coordinator_ focusOmnibox]);
    OCMExpect([mock_toolbar_consumer_ setOmniboxFocused:YES]);
    OCMExpect([mock_omnibox_coordinator_ animatee]);
  }

  /// Expects an omnibox defocus event.
  void ExpectOmniboxDefocus() {
    OCMExpect([mock_omnibox_coordinator_ endEditing]);
    OCMExpect([mock_toolbar_consumer_ setOmniboxFocused:NO]);
    OCMExpect([mock_omnibox_coordinator_ animatee]);
  }

  /// Simulates omniboxDidAcceptText and returns the generated result.
  id<ChromeLensOverlayResult> AcceptOmniboxText(
      const std::u16string& omniboxText,
      const GURL& omniboxURL,
      const GURL& resultURL,
      BOOL expectCanGoBack) {
    // Omnibox is defocused.
    ExpectOmniboxDefocus();

    // UI is updated.
    OCMExpect([mock_omnibox_coordinator_ setThumbnailImage:[OCMArg any]]);
    OCMExpect([mock_omnibox_coordinator_ updateOmniboxState]);
    OCMExpect([mock_toolbar_consumer_ setCanGoBack:expectCanGoBack]);

    fake_chrome_lens_overlay_.resultURL = resultURL;
    [mediator_ omniboxDidAcceptText:omniboxText
                     destinationURL:omniboxURL
                   thumbnailRemoved:NO];

    EXPECT_EQ(fake_result_consumer_.lastPushedURL, resultURL);
    EXPECT_OCMOCK_VERIFY(mock_omnibox_coordinator_);
    EXPECT_OCMOCK_VERIFY(mock_toolbar_consumer_);

    return fake_chrome_lens_overlay_.lastResult;
  }

  /// Simulates new lens selection and returns the generated result.
  id<ChromeLensOverlayResult> UpdateLensSelection(const GURL& resultURL,
                                                  BOOL expectCanGoBack) {
    OCMExpect([mock_omnibox_coordinator_ setThumbnailImage:[OCMArg any]]);
    OCMExpect([mock_omnibox_coordinator_ updateOmniboxState]);
    OCMExpect([mock_toolbar_consumer_ setCanGoBack:expectCanGoBack]);

    fake_chrome_lens_overlay_.resultURL = resultURL;
    [fake_chrome_lens_overlay_ simulateSelectionUpdate];

    EXPECT_EQ(fake_result_consumer_.lastPushedURL, resultURL);
    EXPECT_OCMOCK_VERIFY(mock_omnibox_coordinator_);
    EXPECT_OCMOCK_VERIFY(mock_toolbar_consumer_);

    return fake_chrome_lens_overlay_.lastResult;
  }

  /// Simulates the delayed delivery of suggest signals after the results.
  id<ChromeLensOverlayResult> UpdateLensSignals(const GURL& resultURL,
                                                BOOL expectCanGoBack) {
    OCMExpect([mock_omnibox_coordinator_ setThumbnailImage:[OCMArg any]]);
    OCMExpect([mock_omnibox_coordinator_ updateOmniboxState]);
    OCMExpect([mock_toolbar_consumer_ setCanGoBack:expectCanGoBack]);

    fake_chrome_lens_overlay_.resultURL = resultURL;
    [fake_chrome_lens_overlay_ simulateSelectionUpdate];

    EXPECT_EQ(fake_result_consumer_.lastPushedURL, resultURL);
    EXPECT_OCMOCK_VERIFY(mock_omnibox_coordinator_);
    EXPECT_OCMOCK_VERIFY(mock_toolbar_consumer_);

    return fake_chrome_lens_overlay_.lastResult;
  }

  /// Simulates a web navigation.
  void SimulateWebNavigation(const GURL& URL, BOOL expectCanGoBack) {
    OCMExpect([mock_toolbar_consumer_ setCanGoBack:expectCanGoBack]);
    OCMExpect([mock_omnibox_coordinator_ updateOmniboxState]);

    web::FakeNavigationContext navigationContext;
    navigationContext.SetWebState(fake_web_state_.get());
    navigationContext.SetUrl(URL);
    navigationContext.SetIsSameDocument(NO);
    fake_web_state_->OnNavigationStarted(&navigationContext);
    fake_web_state_->OnNavigationFinished(&navigationContext);

    EXPECT_OCMOCK_VERIFY(mock_omnibox_coordinator_);
    EXPECT_OCMOCK_VERIFY(mock_toolbar_consumer_);
  }

  /// Go back in the history stack.
  void GoBack(const GURL& expectedURL,
              BOOL expectCanGoBack,
              id<ChromeLensOverlayResult> expectedResultReload) {
    OCMExpect([mock_omnibox_coordinator_ updateOmniboxState]);
    OCMExpect([mock_toolbar_consumer_ setCanGoBack:expectCanGoBack]);

    if (expectedResultReload) {
      OCMExpect([mock_omnibox_coordinator_ setThumbnailImage:[OCMArg any]]);
    }

    [mediator_ goBack];

    if (expectedResultReload) {
      EXPECT_EQ(fake_chrome_lens_overlay_.lastReload, expectedResultReload);
    }

    EXPECT_OCMOCK_VERIFY(mock_omnibox_coordinator_);
    EXPECT_OCMOCK_VERIFY(mock_toolbar_consumer_);
  }

  base::test::TaskEnvironment task_environment_;

  LensOverlayMediator* mediator_;

  FakeResultConsumer* fake_result_consumer_;
  FakeChromeLensOverlay* fake_chrome_lens_overlay_;
  id mock_omnibox_coordinator_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  OCMockObject<LensToolbarConsumer>* mock_toolbar_consumer_;
  OCMockObject<LensOverlayCommands>* mock_lens_commands_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  std::unique_ptr<feature_engagement::Tracker> tracker_;
  FakeLensWebProviderImpl* fake_web_provider_;
  std::unique_ptr<LensOmniboxClient> lens_omnibox_client_;
  std::unique_ptr<TestProfileIOS> profile_;
};

/// Tests that the omnibox and toolbar are updated on omnibox focus.
TEST_F(LensOverlayMediatorTest, FocusOmnibox) {
  // Focus from LensToolbarMutator.
  ExpectOmniboxFocus();
  [mediator_ focusOmnibox];

  // Focus from OmniboxFocusDelegate.
  ExpectOmniboxFocus();
  [mediator_ omniboxDidBecomeFirstResponder];
  EXPECT_OCMOCK_VERIFY(mock_omnibox_coordinator_);
  EXPECT_OCMOCK_VERIFY(mock_toolbar_consumer_);
}

/// Tests that the omnibox and toolbar are updated on omnibox defocus.
TEST_F(LensOverlayMediatorTest, DefocusOmnibox) {
  // Defocus from LensToolbarMutator.
  ExpectOmniboxDefocus();
  [mediator_ defocusOmnibox];

  // Defocus from OmniboxFocusDelegate.
  ExpectOmniboxDefocus();
  [mediator_ omniboxDidResignFirstResponder];
  EXPECT_OCMOCK_VERIFY(mock_omnibox_coordinator_);
  EXPECT_OCMOCK_VERIFY(mock_toolbar_consumer_);
}

// Tests simulating web navigation.
TEST_F(LensOverlayMediatorTest, WebNavigation) {
  SimulateWebNavigation(/*URL=*/GURL("https://some-url.com"),
                        /*expectCanGoBack=*/NO);
}

// Tests simulating omnibox navigation.
TEST_F(LensOverlayMediatorTest, OmniboxDidAcceptText) {
  AcceptOmniboxText(/*text=*/u"search terms",
                    /*omniboxURL=*/GURL("https://some-url.com"),
                    /*resultURL=*/GURL("https://some-url.com"),
                    /*expectCanGoBack=*/NO);
}

// Tests simulating selection update.
TEST_F(LensOverlayMediatorTest, SelectionUpdate) {
  UpdateLensSelection(/*resultURL=*/GURL("https://some-url.com"),
                      /*expectCanGoBack=*/NO);
}

// Tests going back with a mix of omnibox, lensSelection updates.
TEST_F(LensOverlayMediatorTest, HistoryStackMixed) {
  // Lens selection.
  // lensResult1/URL1
  GURL URL1 = GURL("https://url.com/1");
  id<ChromeLensOverlayResult> lensResult1 =
      UpdateLensSelection(URL1, /*expectCanGoBack=*/NO);

  // Omnibox navigation.
  // lensResult1/URL1 > lensResult2/URL2
  GURL URL2 = GURL("https://url.com/2");
  id<ChromeLensOverlayResult> lensResult2 =
      AcceptOmniboxText(/*text=*/u"search terms",
                        /*omniboxURL=*/GURL("https://some-url.com"),
                        /*resultURL=*/URL2,
                        /*expectCanGoBack=*/YES);

  // Lens selection.
  // lensResult1/URL1 > lensResult2/URL2 > lensResultX/URLX
  UpdateLensSelection(GURL("https://url.com/X"), /*expectCanGoBack=*/YES);

  GoBack(/*expectedURL=*/URL2, /*expectCanGoBack=*/YES,
         /*expectedResultReload=*/lensResult2);
  GoBack(/*expectedURL=*/URL1, /*expectCanGoBack=*/NO,
         /*expectedResultReload=*/lensResult1);
}

// Tests changing default search engine closes the overlay.
TEST_F(LensOverlayMediatorTest, SearchEngineChange) {
  TemplateURLService* template_url_service =
      search_engines_test_environment_.template_url_service();

  template_url_service->Load();
  // Verify that Google is the default search provider.
  ASSERT_EQ(SEARCH_ENGINE_GOOGLE,
            template_url_service->GetDefaultSearchProvider()->GetEngineType(
                template_url_service->search_terms_data()));

  // Keep a reference to the Google default search provider.
  const TemplateURL* google_provider =
      template_url_service->GetDefaultSearchProvider();

  OCMExpect([mock_lens_commands_
      destroyLensUI:[OCMArg any]
             reason:lens::LensOverlayDismissalSource::
                        kDefaultSearchEngineChange]);

  // Change the default search provider to a non-Google one.
  TemplateURLData non_google_provider_data;
  non_google_provider_data.SetURL("https://www.nongoogle.com/?q={searchTerms}");
  non_google_provider_data.suggestions_url =
      "https://www.nongoogle.com/suggest/?q={searchTerms}";
  auto* non_google_provider = template_url_service->Add(
      std::make_unique<TemplateURL>(non_google_provider_data));
  template_url_service->SetUserSelectedDefaultSearchProvider(
      non_google_provider);

  EXPECT_OCMOCK_VERIFY(mock_lens_commands_);

  // Change the default search provider back to Google.
  template_url_service->SetUserSelectedDefaultSearchProvider(
      const_cast<TemplateURL*>(google_provider));
}

// Tests that suggest signals are forwarded correctly.
TEST_F(LensOverlayMediatorTest, SuggestSignals) {
  // Populate a selection.
  UpdateLensSelection(/*resultURL=*/GURL("https://some-url.com"),
                      /*expectCanGoBack=*/NO);

  // Pretend the signlas were successfully fetched from the server.
  NSData* signals = [@"xyz" dataUsingEncoding:NSUTF8StringEncoding];
  [fake_chrome_lens_overlay_ simulateSuggestSignalsUpdate:signals];

  // Expected signals are base64 URL encoded, but otherwise identical to what
  // was passed.
  std::string expected_signals;
  Base64UrlEncode("xyz", base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                  &expected_signals);

  EXPECT_TRUE(lens_omnibox_client_->GetLensOverlaySuggestInputs()
                  ->has_encoded_image_signals());
  EXPECT_EQ(expected_signals,
            lens_omnibox_client_->GetLensOverlaySuggestInputs()
                ->encoded_image_signals());

  // On a new selection, the signals reset until they are fetched again.
  UpdateLensSelection(/*resultURL=*/GURL("https://some-other-url.com"),
                      /*expectCanGoBack=*/YES);
  EXPECT_FALSE(lens_omnibox_client_->GetLensOverlaySuggestInputs());
}

}  // namespace
