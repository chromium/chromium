// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_navigation_manager.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/uuid.h"
#import "components/lens/lens_url_utils.h"
#import "components/variations/scoped_variations_ids_provider.h"
#import "components/variations/variations_ids_provider.h"
#import "ios/chrome/browser/lens_overlay/coordinator/fake_chrome_lens_overlay_result.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_navigation_mutator.h"
#import "ios/chrome/common/NSString+Chromium.h"
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

class LensOverlayNavigationManagerTest : public PlatformTest {
 protected:
  LensOverlayNavigationManagerTest() {
    fake_web_state_ = std::make_unique<web::FakeWebState>();
    mock_mutator_ = OCMProtocolMock(@protocol(LensOverlayNavigationMutator));

    // Stub calls to `loadURL:omniboxText:` and store the arguments.
    OCMStub([[mock_mutator_ ignoringNonObjectArgs] loadURL:GURL()
                                               omniboxText:[OCMArg any]])
        .andDo(^(NSInvocation* invocation) {
          GURL* url;
          [invocation getArgument:&url atIndex:2];
          latest_loaded_url_ = *url;

          __unsafe_unretained NSString* omnibox_text;
          [invocation getArgument:&omnibox_text atIndex:3];
          latest_loaded_omnibox_text_ = omnibox_text;
        });

    manager_ = std::make_unique<LensOverlayNavigationManager>(mock_mutator_);
    manager_->SetWebState(fake_web_state_.get());
  }

  /// Returns a random URL.
  GURL GenerateRandomURL() {
    return GURL("https://some-url.com/" +
                base::Uuid::GenerateRandomV4().AsLowercaseString());
  }

  /// Returns a lens overlay SRP URL with `query_text` as query parameter.
  GURL LensOverlaySRPWithQueryText(const std::string& query_text) {
    GURL url = GURL("https://www.google.com/search");
    url = net::AppendOrReplaceQueryParameter(
        url, lens::kLensSurfaceQueryParameter, "4");
    url = net::AppendOrReplaceQueryParameter(url, "q", query_text);
    url = net::AppendOrReplaceQueryParameter(url, "cs", "0");
    return url;
  }

  /// Returns a lens multimodal SRP URL with `query_text` as query parameter.
  GURL LensMultimodalSRPWithQueryText(const std::string& query_text) {
    GURL url = LensOverlaySRPWithQueryText(query_text);
    url = net::AppendOrReplaceQueryParameter(
        url, lens::kLensRequestQueryParameter, "<vsrid>");
    url = net::AppendOrReplaceQueryParameter(
        url, lens::kUnifiedDrillDownQueryParameter, "24");
    return url;
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
    latest_loaded_url_ = GURL();
    latest_loaded_omnibox_text_ = @"";

    manager_->LoadUnimodalOmniboxNavigation(URL, omnibox_text);

    // Expects `lns_surface` query param to be added in the URL.
    GURL expected_url =
        net::AppendOrReplaceQueryParameter(URL, "lns_surface", "4");

    EXPECT_OCMOCK_VERIFY(mock_mutator_);
    EXPECT_EQ(latest_loaded_url_, expected_url);
    EXPECT_TRUE([latest_loaded_omnibox_text_
        isEqualToString:[NSString cr_fromString16:omnibox_text]]);

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
    latest_loaded_url_ = GURL();
    OCMExpect([mock_mutator_
        onBackNavigationAvailabilityMaybeChanged:expect_can_go_back]);
    manager_->GoBack();
    EXPECT_OCMOCK_VERIFY(mock_mutator_);
    EXPECT_EQ(latest_loaded_url_, URL);
  }

  id mock_mutator_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  std::unique_ptr<LensOverlayNavigationManager> manager_;
  GURL latest_loaded_url_;
  NSString* latest_loaded_omnibox_text_;

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

// Tests loading lens overlay SRP updates the navigation text.
TEST_F(LensOverlayNavigationManagerTest, SRPNavigationUpdatesText) {
  id<ChromeLensOverlayResult> result1 = GenerateResult(1);
  SimulateLensDidGenerateResult(result1, /*expect_load=*/YES,
                                /*expect_can_go_back=*/NO);
  // Lens navigation loads `URL1`.
  GURL URL1 = result1.searchResultURL;

  // Web navigation to SRP `URL2` with `text1`.
  NSString* text1 = @"some query text";
  GURL URL2 = LensOverlaySRPWithQueryText(text1.cr_UTF8String);
  OCMExpect([mock_mutator_ onSRPLoadWithOmniboxText:text1 isMultimodal:NO]);
  SimulateWebNavigation(URL2, /*expect_can_go_back=*/YES);
  EXPECT_OCMOCK_VERIFY(mock_mutator_);

  // Web navigation to SRP `URL3` with `text2`.
  NSString* text2 = @"some other text";
  GURL URL3 = LensOverlaySRPWithQueryText(text2.cr_UTF8String);
  OCMExpect([mock_mutator_ onSRPLoadWithOmniboxText:text2 isMultimodal:NO]);
  SimulateWebNavigation(URL3, /*expect_can_go_back=*/YES);
  EXPECT_OCMOCK_VERIFY(mock_mutator_);

  // Go back expecting `URL2` with `text1`.
  GoBackExpectingURLReload(URL2, /*expect_can_go_back=*/YES);
  EXPECT_TRUE([latest_loaded_omnibox_text_ isEqualToString:text1]);
}

// Tests loading lens multimodal SRP updates the navigation text.
TEST_F(LensOverlayNavigationManagerTest, MultimodalSRPNavigationUpdatesText) {
  id<ChromeLensOverlayResult> result1 = GenerateResult(1);
  SimulateLensDidGenerateResult(result1, /*expect_load=*/YES,
                                /*expect_can_go_back=*/NO);
  // Lens navigation loads `URL1`.
  GURL URL1 = result1.searchResultURL;

  // Web navigation to SRP `URL2` with no text.
  NSString* text1 = @"";
  GURL URL2 = LensOverlaySRPWithQueryText(text1.cr_UTF8String);
  OCMExpect([mock_mutator_ onSRPLoadWithOmniboxText:text1 isMultimodal:NO]);
  SimulateWebNavigation(URL2, /*expect_can_go_back=*/YES);
  EXPECT_OCMOCK_VERIFY(mock_mutator_);

  // Web navigation to multi-modal SRP `URL3` with `text2`.
  NSString* text2 = @"multimodal text query";
  GURL URL3 = LensMultimodalSRPWithQueryText(text2.cr_UTF8String);
  OCMExpect([mock_mutator_ onSRPLoadWithOmniboxText:text2 isMultimodal:YES]);
  SimulateWebNavigation(URL3, /*expect_can_go_back=*/YES);
  EXPECT_OCMOCK_VERIFY(mock_mutator_);

  // Go back expecting `URL2` with no text.
  GoBackExpectingURLReload(URL2, /*expect_can_go_back=*/YES);
  EXPECT_TRUE([latest_loaded_omnibox_text_ isEqualToString:text1]);
}
