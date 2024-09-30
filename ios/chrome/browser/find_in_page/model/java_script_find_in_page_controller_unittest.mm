// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/model/java_script_find_in_page_controller.h"

#import "base/apple/foundation_util.h"
#import "base/test/ios/wait_util.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "components/ukm/test_ukm_recorder.h"
#import "ios/chrome/browser/find_in_page/model/find_in_page_model.h"
#import "ios/chrome/browser/find_in_page/model/find_in_page_response_delegate.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

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
- (void)findDidStop {
}
@end

namespace {

const char kFindInPageUkmSearchMatchesEvent[] = "IOS.FindInPageSearchMatches";
const char kFindInPageUkmSearchMetric[] = "HasMatches";

class JavaScriptFindInPageControllerTest : public PlatformTest {
 protected:
  JavaScriptFindInPageControllerTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {
    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
  }
  ~JavaScriptFindInPageControllerTest() override {}

  void SetUp() override {
    PlatformTest::SetUp();
    find_in_page_controller_ =
        [[JavaScriptFindInPageController alloc] initWithWebState:web_state()];
    delegate_ = [[TestFindInPageResponseDelegate alloc] init];
    find_in_page_controller_.responseDelegate = delegate_;
    ukm::InitializeSourceUrlRecorderForWebState(web_state());
  }

  void TearDown() override {
    [find_in_page_controller_ detachFromWebState];
    test_ukm_recorder_.Purge();
    web_state_.reset();
  }

  web::WebState* web_state() { return web_state_.get(); }

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
  JavaScriptFindInPageController* find_in_page_controller_ = nil;
  TestFindInPageResponseDelegate* delegate_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

// Loads html that contains "some string", searches for it, and ensures UKM has
// logged the search as having found a match.
TEST_F(JavaScriptFindInPageControllerTest, VerifyUKMLoggedTrue) {
  test_ukm_recorder_.Purge();
  web::test::LoadHtml(@"<html><p>some string</p></html>", web_state());
  [find_in_page_controller_ findStringInPage:@"some string"];
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return delegate_.model != nil;
  }));
  [find_in_page_controller_ disableFindInPage];
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
TEST_F(JavaScriptFindInPageControllerTest, VerifyUKMLoggedFalse) {
  test_ukm_recorder_.Purge();
  web::test::LoadHtml(@"<html><p>some string</p></html>", web_state());
  [find_in_page_controller_ findStringInPage:@"nothing"];
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return delegate_.model != nil;
  }));
  [find_in_page_controller_ disableFindInPage];
  // Single false entry should be recorded for the interaction above.
  const auto& entries =
      test_ukm_recorder_.GetEntriesByName(kFindInPageUkmSearchMatchesEvent);
  ASSERT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* entry = entries[0];
  EXPECT_NE(ukm::kInvalidSourceId, entry->source_id);
  test_ukm_recorder_.ExpectEntryMetric(entry, kFindInPageUkmSearchMetric,
                                       false);
}
}  // namespace
