// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/find_in_page_controller.h"

#import "base/mac/foundation_util.h"
#import "base/test/ios/wait_util.h"
#include "components/ukm/ios/ukm_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/find_in_page/find_in_page_model.h"
#import "ios/chrome/browser/find_in_page/find_in_page_response_delegate.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

@interface TestFindInPageResponseDelegate
    : NSObject <FindInPageResponseDelegate>
@property(nonatomic, strong) FindInPageModel* model;
@end

@implementation TestFindInPageResponseDelegate
- (void)findDidFinishWithUpdatedModel:(FindInPageModel*)model {
  self.model = model;
}
@end

namespace {

const char kFindInPageUkmSearchMatchesEvent[] = "IOS.FindInPageSearchMatches";
const char kFindInPageUkmSearchMetric[] = "HasMatches";

class FindInPageControllerTest : public ChromeWebTest {
 protected:
  FindInPageControllerTest()
      : ChromeWebTest(std::make_unique<ChromeWebClient>()) {}
  ~FindInPageControllerTest() override {}

  void SetUp() override {
    ChromeWebTest::SetUp();
    find_in_page_controller_ =
        [[FindInPageController alloc] initWithWebState:web_state()];
    delegate_ = [[TestFindInPageResponseDelegate alloc] init];
    find_in_page_controller_.responseDelegate = delegate_;
    ukm::InitializeSourceUrlRecorderForWebState(web_state());
  }

  void TearDown() override {
    test_ukm_recorder_.Purge();
    ChromeWebTest::TearDown();
  }

  FindInPageController* find_in_page_controller_ = nil;
  TestFindInPageResponseDelegate* delegate_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

// Loads html that contains "some string", searches for it, and ensures UKM has
// logged the search as having found a match.
TEST_F(FindInPageControllerTest, VerifyUKMLoggedTrue) {
  test_ukm_recorder_.Purge();
  LoadHtml(@"<html><p>some string</p></html>");
  [find_in_page_controller_ findStringInPage:@"some string"
                           completionHandler:^{
                           }];
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return delegate_.model != nil;
  }));
  // Single true entry should be recorded for the interaction above.
  const auto& entries =
      test_ukm_recorder_.GetEntriesByName(kFindInPageUkmSearchMatchesEvent);
  ASSERT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* entry = entries[0];
  EXPECT_NE(ukm::kInvalidSourceId, entry->source_id);
  test_ukm_recorder_.ExpectEntryMetric(entry, kFindInPageUkmSearchMetric, true);
}

// Loads html that contains "some string", searches for something that does not
// match, and ensures UKM has not logged the search as having found a match.
TEST_F(FindInPageControllerTest, VerifyUKMLoggedFalse) {
  test_ukm_recorder_.Purge();
  LoadHtml(@"<html><p>some string</p></html>");
  [find_in_page_controller_ findStringInPage:@"nothing"
                           completionHandler:^{
                           }];
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return delegate_.model != nil;
  }));
  // Single false entry should be recorded for the interaction above.
  const auto& entries =
      test_ukm_recorder_.GetEntriesByName(kFindInPageUkmSearchMatchesEvent);
  ASSERT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* entry = entries[0];
  EXPECT_NE(ukm::kInvalidSourceId, entry->source_id);
  test_ukm_recorder_.ExpectEntryMetric(entry, kFindInPageUkmSearchMetric,
                                       false);
}
}
