// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_navigation_manager.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/uuid.h"
#import "components/variations/scoped_variations_ids_provider.h"
#import "components/variations/variations_ids_provider.h"
#import "ios/chrome/browser/lens_overlay/coordinator/fake_chrome_lens_overlay_result.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_navigation_mutator.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "net/base/url_util.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

@interface MockLensOverlayNavigationMutator
    : NSObject <LensOverlayNavigationMutator>
/// Latest `URL` loaded.
@property(nonatomic, assign) GURL latestLoadedURL;
/// Latest text loaded in the omnibox.
@property(nonatomic, assign) NSString* latestLoadedOmniboxText;
@end

@implementation MockLensOverlayNavigationMutator
- (void)loadURL:(GURL)URL omniboxText:(NSString*)omniboxText {
  _latestLoadedURL = URL;
  _latestLoadedOmniboxText = omniboxText;
}

// Methods below are mocked with OCMock.
- (void)loadLensResult:(id<ChromeLensOverlayResult>)result {
}
- (void)reloadLensResult:(id<ChromeLensOverlayResult>)result {
}
- (void)onBackNavigationAvailabilityMaybeChanged:(BOOL)canGoBack {
}
@end

class LensOverlayNavigationManagerTest : public PlatformTest {
 protected:
  LensOverlayNavigationManagerTest() {
    fake_web_state_ = std::make_unique<web::FakeWebState>();
    mutator_ = [[MockLensOverlayNavigationMutator alloc] init];
    mock_mutator_ = OCMPartialMock(mutator_);
    manager_ = std::make_unique<LensOverlayNavigationManager>(mutator_);
    manager_->SetWebState(fake_web_state_.get());
  }

  /// Returns a random URL.
  GURL GenerateRandomURL() {
    return GURL("https://some-url.com/" +
                base::Uuid::GenerateRandomV4().AsLowercaseString());
  }

  /// Generates a Lens result.
  id<ChromeLensOverlayResult> GenerateResult(int identifier) {
    FakeChromeLensOverlayResult* result =
        [[FakeChromeLensOverlayResult alloc] init];
    result.isTextSelection = identifier % 2 == 0;
    result.queryText = @"";
    result.selectionRect = CGRectMake(identifier, identifier, 10, 10);
    result.searchResultURL = GenerateRandomURL();
    return result;
  }

  /// Simulates lens generated result.
  void SimulateLensDidGenerateResult(id<ChromeLensOverlayResult> result,
                                     BOOL expect_load,
                                     BOOL expect_can_go_back) {
    if (expect_load) {
      OCMExpect([mock_mutator_ loadLensResult:result]);
    }
    manager_->LensOverlayDidGenerateResult(result);
    if (expect_load) {
      SimulateWebNavigation(result.searchResultURL, expect_can_go_back);
    }
    EXPECT_OCMOCK_VERIFY(mock_mutator_);
  }

  /// Navigates to `URL`.
  void WebNavigation(const GURL& URL) {
    web::FakeNavigationContext context;
    context.SetWebState(fake_web_state_.get());
    context.SetUrl(URL);
    context.SetIsSameDocument(NO);
    fake_web_state_->OnNavigationStarted(&context);
    fake_web_state_->OnNavigationFinished(&context);
  }

  /// Simulates a web navigation that create a new navigation entry.
  void SimulateWebNavigation(const GURL& URL, BOOL expect_can_go_back) {
    OCMExpect([mock_mutator_
        onBackNavigationAvailabilityMaybeChanged:expect_can_go_back]);
    WebNavigation(URL);
    EXPECT_OCMOCK_VERIFY(mock_mutator_);
  }

  /// Simulates a unimodal omnibox navigation.
  void SimulateUnimodalOmniboxNavigation(const GURL& URL,
                                         const std::u16string& omnibox_text,
                                         BOOL expect_can_go_back) {
    OCMExpect([mock_mutator_
        onBackNavigationAvailabilityMaybeChanged:expect_can_go_back]);
    mutator_.latestLoadedURL = GURL();
    mutator_.latestLoadedOmniboxText = @"";

    manager_->LoadUnimodalOmniboxNavigation(URL, omnibox_text);

    // Expects `lns_surface` query param to be added in the URL.
    GURL expected_url =
        net::AppendOrReplaceQueryParameter(URL, "lns_surface", "4");

    EXPECT_OCMOCK_VERIFY(mock_mutator_);
    EXPECT_EQ(mutator_.latestLoadedURL, expected_url);
    EXPECT_TRUE([mutator_.latestLoadedOmniboxText
        isEqualToString:base::SysUTF16ToNSString(omnibox_text)]);

    // Simulates the web navigation that happens with `loadURL:omniboxText:`.
    // This navigation should not add a new navigation entries.
    WebNavigation(expected_url);
  }

  /// Go back and expect lens `result` to be reloaded.
  void GoBackExpectingLensResultReload(id<ChromeLensOverlayResult> result,
                                       BOOL expect_can_go_back) {
    OCMExpect([mock_mutator_ reloadLensResult:result]);
    OCMExpect([mock_mutator_
        onBackNavigationAvailabilityMaybeChanged:expect_can_go_back]);
    manager_->GoBack();
    EXPECT_OCMOCK_VERIFY(mock_mutator_);
  }

  /// Go back and expect `URL` to be reloaded.
  void GoBackExpectingURLReload(const GURL& URL, BOOL expect_can_go_back) {
    mutator_.latestLoadedURL = GURL();
    OCMExpect([mock_mutator_
        onBackNavigationAvailabilityMaybeChanged:expect_can_go_back]);
    manager_->GoBack();
    EXPECT_OCMOCK_VERIFY(mock_mutator_);
    EXPECT_EQ(mutator_.latestLoadedURL, URL);
  }

  MockLensOverlayNavigationMutator* mutator_;
  id mock_mutator_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  std::unique_ptr<LensOverlayNavigationManager> manager_;

  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
};

// Tests simulating a lens navigation.
TEST_F(LensOverlayNavigationManagerTest, LensNavigation) {
  id<ChromeLensOverlayResult> result = GenerateResult(1);
  SimulateLensDidGenerateResult(result, /*expect_load=*/YES,
                                /*expect_can_go_back=*/NO);
}

// Tests go back on a lens navigation.
TEST_F(LensOverlayNavigationManagerTest, LensNavigationBack) {
  id<ChromeLensOverlayResult> result1 = GenerateResult(1);
  SimulateLensDidGenerateResult(result1, /*expect_load=*/YES,
                                /*expect_can_go_back=*/NO);

  id<ChromeLensOverlayResult> result2 = GenerateResult(2);
  SimulateLensDidGenerateResult(result2, /*expect_load=*/YES,
                                /*expect_can_go_back=*/YES);

  // Go back on result1.
  GoBackExpectingLensResultReload(result1, /*expect_can_go_back=*/NO);

  // Lens did generate result for result1 reload.
  id<ChromeLensOverlayResult> result1b = GenerateResult(1);
  SimulateLensDidGenerateResult(result1b, /*expect_load=*/YES,
                                /*expect_can_go_back=*/NO);
}

// Tests multiple go back on Lens navigation, where the results are not reloaded
// in order.
TEST_F(LensOverlayNavigationManagerTest, LensNavigationBackDiscarded) {
  id<ChromeLensOverlayResult> result1 = GenerateResult(1);
  SimulateLensDidGenerateResult(result1, /*expect_load=*/YES,
                                /*expect_can_go_back=*/NO);

  id<ChromeLensOverlayResult> result2 = GenerateResult(2);
  SimulateLensDidGenerateResult(result2, /*expect_load=*/YES,
                                /*expect_can_go_back=*/YES);

  id<ChromeLensOverlayResult> result3 = GenerateResult(3);
  SimulateLensDidGenerateResult(result3, /*expect_load=*/YES,
                                /*expect_can_go_back=*/YES);

  // Go back on result2.
  GoBackExpectingLensResultReload(result2, /*expect_can_go_back=*/YES);

  // Go back on result1.
  GoBackExpectingLensResultReload(result1, /*expect_can_go_back=*/NO);

  // Reload result1 succeed.
  id<ChromeLensOverlayResult> result1b = GenerateResult(1);
  SimulateLensDidGenerateResult(result1b, /*expect_load=*/YES,
                                /*expect_can_go_back=*/NO);

  // Reload result2 discarded.
  id<ChromeLensOverlayResult> result2b = GenerateResult(2);
  SimulateLensDidGenerateResult(result2b, /*expect_load=*/NO,
                                /*expect_can_go_back=*/NO);
}

// Tests go back on a Lens navigation and start a new navigation before reload.
TEST_F(LensOverlayNavigationManagerTest, LensNavigationBackNoLoad) {
  id<ChromeLensOverlayResult> result1 = GenerateResult(1);
  SimulateLensDidGenerateResult(result1, /*expect_load=*/YES,
                                /*expect_can_go_back=*/NO);

  id<ChromeLensOverlayResult> result2 = GenerateResult(2);
  SimulateLensDidGenerateResult(result2, /*expect_load=*/YES,
                                /*expect_can_go_back=*/YES);

  // Go back on result1.
  GoBackExpectingLensResultReload(result1, /*expect_can_go_back=*/NO);

  // Navigate to result3.
  id<ChromeLensOverlayResult> result3 = GenerateResult(3);
  SimulateLensDidGenerateResult(result3, /*expect_load=*/YES,
                                /*expect_can_go_back=*/YES);

  // Reload result1 not loaded.
  id<ChromeLensOverlayResult> result1b = GenerateResult(1);
  SimulateLensDidGenerateResult(result1b, /*expect_load=*/NO,
                                /*expect_can_go_back=*/YES);
}

// Tests go back on a web navigation.
TEST_F(LensOverlayNavigationManagerTest, WebNavigationBack) {
  id<ChromeLensOverlayResult> result1 = GenerateResult(1);
  SimulateLensDidGenerateResult(result1, /*expect_load=*/YES,
                                /*expect_can_go_back=*/NO);
  // Lens navigation loads URL1.
  GURL URL1 = result1.searchResultURL;

  // Sub navigation to URL2.
  GURL URL2 = GURL("https://url.com/2");
  SimulateWebNavigation(URL2, /*expect_can_go_back=*/YES);
  // Sub navigation to URL3.
  SimulateWebNavigation(GURL("https://url.com/3"), /*expect_can_go_back=*/YES);

  GoBackExpectingURLReload(URL2, /*expect_can_go_back=*/YES);
  GoBackExpectingURLReload(URL1, /*expect_can_go_back=*/NO);
}

// Tests go back on a mix of Lens and sub navigations.
TEST_F(LensOverlayNavigationManagerTest, MixNavigationBack) {
  // Lens navigation generates result1 and loads URL1.
  id<ChromeLensOverlayResult> result1 = GenerateResult(1);
  SimulateLensDidGenerateResult(result1, /*expect_load=*/YES,
                                /*expect_can_go_back=*/NO);

  // Sub navigation to URL2.
  GURL URL2 = GURL("https://url.com/2");
  SimulateWebNavigation(URL2, /*expect_can_go_back=*/YES);

  // Lens navigation generates result2 and loads URL3.
  id<ChromeLensOverlayResult> result2 = GenerateResult(2);
  SimulateLensDidGenerateResult(result2, /*expect_load=*/YES,
                                /*expect_can_go_back=*/YES);
  GURL URL3 = result2.searchResultURL;

  // Sub navigation to URL4.
  GURL URL4 = GURL("https://url.com/3");
  SimulateWebNavigation(URL4, /*expect_can_go_back=*/YES);
  // Sub navigation to URL5.
  GURL URL5 = GURL("https://url.com/4");
  SimulateWebNavigation(URL5, /*expect_can_go_back=*/YES);

  // Go back on the sub navigations from `result2`.
  GoBackExpectingURLReload(URL4, /*expect_can_go_back=*/YES);
  GoBackExpectingURLReload(URL3, /*expect_can_go_back=*/YES);

  // Go back on result1. (result1 sub navigations are invalid)
  GoBackExpectingLensResultReload(result1, /*expect_can_go_back=*/NO);
  // Lens did generate result for result1 reload.
  id<ChromeLensOverlayResult> result1b = GenerateResult(1);
  SimulateLensDidGenerateResult(result1b, /*expect_load=*/YES,
                                /*expect_can_go_back=*/NO);
}

// Tests navigation with from a unimodal omnibox query.
TEST_F(LensOverlayNavigationManagerTest, UnimodalOmniboxNavigation) {
  id<ChromeLensOverlayResult> result1 = GenerateResult(1);
  SimulateLensDidGenerateResult(result1, /*expect_load=*/YES,
                                /*expect_can_go_back=*/NO);
  // Lens navigation loads URL1.
  GURL URL1 = result1.searchResultURL;

  // Unimodal omnibox navigation to URL2.
  GURL URL2 = GenerateRandomURL();
  SimulateUnimodalOmniboxNavigation(URL2, u"search terms",
                                    /*expect_can_go_back=*/YES);
}

// Tests go back on a unimodal omnibox navigation.
TEST_F(LensOverlayNavigationManagerTest, UnimodalOmniboxNavigationBack) {
  id<ChromeLensOverlayResult> result1 = GenerateResult(1);
  SimulateLensDidGenerateResult(result1, /*expect_load=*/YES,
                                /*expect_can_go_back=*/NO);
  // Lens navigation loads URL1.
  GURL URL1 = result1.searchResultURL;

  // Unimodal omnibox navigation to URL2.
  GURL URL2 = GenerateRandomURL();
  SimulateUnimodalOmniboxNavigation(URL2, u"search terms",
                                    /*expect_can_go_back=*/YES);

  GoBackExpectingURLReload(URL1, /*expect_can_go_back=*/NO);
}
