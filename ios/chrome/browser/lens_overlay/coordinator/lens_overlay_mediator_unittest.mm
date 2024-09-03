// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/lens_overlay/coordinator/fake_chrome_lens_overlay.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_toolbar_consumer.h"
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

- (void)disconnect {
  self.webState = nil;
}

@end

namespace {

class LensOverlayMediatorTest : public PlatformTest {
 public:
  LensOverlayMediatorTest() {
    mediator_ = [[LensOverlayMediator alloc] init];
    mock_omnibox_coordinator_ =
        [OCMockObject mockForClass:OmniboxCoordinator.class];
    mock_toolbar_consumer_ =
        [OCMockObject mockForProtocol:@protocol(LensToolbarConsumer)];
    fake_web_state_ = std::make_unique<web::FakeWebState>();

    fake_result_consumer_ = [[FakeResultConsumer alloc] init];
    fake_result_consumer_.webState = fake_web_state_.get();

    fake_chrome_lens_overlay_ = [[FakeChromeLensOverlay alloc] init];
    [fake_chrome_lens_overlay_ setLensOverlayDelegate:mediator_];
    [fake_chrome_lens_overlay_ start];

    mediator_.resultConsumer = fake_result_consumer_;
    mediator_.omniboxCoordinator = mock_omnibox_coordinator_;
    mediator_.toolbarConsumer = mock_toolbar_consumer_;
    mediator_.webState = fake_web_state_.get();
    mediator_.lensHandler = fake_chrome_lens_overlay_;
  }

  ~LensOverlayMediatorTest() override {
    [mediator_ disconnect];
    [fake_result_consumer_ disconnect];
  }

 protected:
  /// Simulates omniboxDidAcceptText and returns the generated result.
  id<ChromeLensOverlayResult> AcceptOmniboxText(
      const std::u16string& omniboxText,
      const GURL& omniboxURL,
      const GURL& resultURL,
      BOOL expectCanGoBack) {
    // Omnibox is defocused.
    OCMExpect([mock_omnibox_coordinator_ endEditing]);
    OCMExpect([mock_toolbar_consumer_ setOmniboxFocused:NO]);

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

    EXPECT_EQ(fake_result_consumer_.lastPushedURL, expectedURL);
    EXPECT_OCMOCK_VERIFY(mock_omnibox_coordinator_);
    EXPECT_OCMOCK_VERIFY(mock_toolbar_consumer_);
  }

  LensOverlayMediator* mediator_;

  FakeResultConsumer* fake_result_consumer_;
  FakeChromeLensOverlay* fake_chrome_lens_overlay_;
  id mock_omnibox_coordinator_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  OCMockObject<LensToolbarConsumer>* mock_toolbar_consumer_;
};

/// Tests that the omnibox and toolbar are updated on omnibox focus.
TEST_F(LensOverlayMediatorTest, FocusOmnibox) {
  // Focus from LensToolbarMutator.
  OCMExpect([mock_omnibox_coordinator_ focusOmnibox]);
  OCMExpect([mock_toolbar_consumer_ setOmniboxFocused:YES]);
  [mediator_ focusOmnibox];
  EXPECT_OCMOCK_VERIFY(mock_omnibox_coordinator_);
  EXPECT_OCMOCK_VERIFY(mock_toolbar_consumer_);

  // Focus from OmniboxFocusDelegate.
  OCMExpect([mock_omnibox_coordinator_ focusOmnibox]);
  OCMExpect([mock_toolbar_consumer_ setOmniboxFocused:YES]);
  [mediator_ omniboxDidBecomeFirstResponder];
  EXPECT_OCMOCK_VERIFY(mock_omnibox_coordinator_);
  EXPECT_OCMOCK_VERIFY(mock_toolbar_consumer_);
}

/// Tests that the omnibox and toolbar are updated on omnibox defocus.
TEST_F(LensOverlayMediatorTest, DefocusOmnibox) {
  // Defocus from LensToolbarMutator.
  OCMExpect([mock_omnibox_coordinator_ endEditing]);
  OCMExpect([mock_toolbar_consumer_ setOmniboxFocused:NO]);
  [mediator_ defocusOmnibox];
  EXPECT_OCMOCK_VERIFY(mock_omnibox_coordinator_);
  EXPECT_OCMOCK_VERIFY(mock_toolbar_consumer_);

  // Defocus from OmniboxFocusDelegate.
  OCMExpect([mock_omnibox_coordinator_ endEditing]);
  OCMExpect([mock_toolbar_consumer_ setOmniboxFocused:NO]);
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

// Tests going back with web navigation on the same ChromeLensOverlayResult.
TEST_F(LensOverlayMediatorTest, HistoryStackWebNavigation) {
  GURL URL1 = GURL("https://url.com/1");
  UpdateLensSelection(URL1, /*expectCanGoBack=*/NO);

  // One navigation.
  GURL URL2 = GURL("https://url.com/2");
  SimulateWebNavigation(URL2, /*expectCanGoBack=*/YES);
  GoBack(/*expectedURL=*/URL1, /*expectCanGoBack=*/NO,
         /*expectedResultReload=*/nil);

  // Two navigation.
  GURL URL3 = GURL("https://url.com/3");
  SimulateWebNavigation(URL3, /*expectCanGoBack=*/YES);
  SimulateWebNavigation(GURL("https://url.com/X"), /*expectCanGoBack=*/YES);

  GoBack(/*expectedURL=*/URL3, /*expectCanGoBack=*/YES,
         /*expectedResultReload=*/nil);
  GoBack(/*expectedURL=*/URL1, /*expectCanGoBack=*/NO,
         /*expectedResultReload=*/nil);
}

// Tests going back with a mix of omnibox, web, lensSelection updates.
TEST_F(LensOverlayMediatorTest, HistoryStackMixed) {
  // Lens selection.
  // lensResult1/URL1
  GURL URL1 = GURL("https://url.com/1");
  id<ChromeLensOverlayResult> lensResult1 =
      UpdateLensSelection(URL1, /*expectCanGoBack=*/NO);

  // Web navigation.
  // lensResult1/URL1 > lensResult1/URL2
  GURL URL2 = GURL("https://url.com/2");
  SimulateWebNavigation(/*URL=*/URL2, /*expectCanGoBack=*/YES);

  // Omnibox navigation.
  // lensResult1/URL1 > lensResult1/URL2 > lensResult2/URL3
  GURL URL3 = GURL("https://url.com/3");
  id<ChromeLensOverlayResult> lensResult2 =
      AcceptOmniboxText(/*text=*/u"search terms",
                        /*omniboxURL=*/GURL("https://some-url.com"),
                        /*resultURL=*/URL3,
                        /*expectCanGoBack=*/YES);

  // Web navigation.
  // lensResult1/URL1 > lensResult1/URL2 > lensResult2/URL3 > lensResult2/URL4
  GURL URL4 = GURL("https://url.com/4");
  SimulateWebNavigation(/*URL=*/URL4, /*expectCanGoBack=*/YES);

  // Lens selection.
  // lensResult1/URL1 > lensResult1/URL2 > lensResult2/URL3 > lensResult2/URL4 >
  // lensResultX/URLX
  UpdateLensSelection(GURL("https://url.com/X"), /*expectCanGoBack=*/YES);

  GoBack(/*expectedURL=*/URL4, /*expectCanGoBack=*/YES,
         /*expectedResultReload=*/lensResult2);

  // Reloading a result will create a new one. So `lensResult2` is expected to
  // be reloaded. After the first goBack, the history stack is:
  // lensResult1/URL1 > lensResult1/URL2 > lensResult2/URL3 > lensResult3/URL4
  GoBack(/*expectedURL=*/URL3, /*expectCanGoBack=*/YES,
         /*expectedResultReload=*/lensResult2);
  GoBack(/*expectedURL=*/URL2, /*expectCanGoBack=*/YES,
         /*expectedResultReload=*/lensResult1);
  GoBack(/*expectedURL=*/URL1, /*expectCanGoBack=*/NO,
         /*expectedResultReload=*/lensResult1);
}

}  // namespace
