// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_selection/model/web_selection_tab_helper.h"

#import <Foundation/Foundation.h>

#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/chrome/browser/web_selection/model/web_selection_java_script_feature.h"
#import "ios/chrome/browser/web_selection/model/web_selection_java_script_feature_observer.h"
#import "ios/chrome/browser/web_selection/model/web_selection_response.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
NSString* kPageHTML =
    @"<html>"
     "  <body>"
     "    This text contains a <span id='selectid'>selection</span>."
     "    <iframe id='frame' srcdoc='"
     "      <html>"
     "        <body>"
     "          This frame contains another "
     "          <span id=\"frameselectid\">frame selection</span>."
     "        </body>"
     "      </html>"
     "    '/>"
     "  </body>"
     "</html>";

NSString* kPage2HTML =
    @"<html>"
     "  <body>"
     "    This text contains a <span id='selectid'>selection2</span>."
     "  </body>"
     "</html>";

// A test Web selection observer that will count the number of call from JS.
class TestWebSelectionJavaScriptFeatureObserver
    : public WebSelectionJavaScriptFeatureObserver {
 public:
  void OnSelectionRetrieved(web::WebState* web_state,
                            WebSelectionResponse* response) override {
    number_of_calls_++;
  }

  int GetNumberOfCalls() { return number_of_calls_; }

 private:
  int number_of_calls_ = 0;
};

}  // namespace

class WebSelectionTabHelperTest : public PlatformTest {
 public:
  WebSelectionTabHelperTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {
    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    WebSelectionTabHelper::CreateForWebState(web_state_.get());
    selection_observer_ =
        std::make_unique<TestWebSelectionJavaScriptFeatureObserver>();
  }

  void SetUp() override {
    PlatformTest::SetUp();
    WebSelectionJavaScriptFeature::GetInstance()->AddObserver(
        selection_observer_.get());
    web::test::LoadHtml(kPageHTML, web_state());
  }

  void TearDown() override {
    WebSelectionJavaScriptFeature::GetInstance()->RemoveObserver(
        selection_observer_.get());
    PlatformTest::TearDown();
  }

  web::WebState* web_state() { return web_state_.get(); }

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  web::ScopedTestingWebClient web_client_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
  std::unique_ptr<TestWebSelectionJavaScriptFeatureObserver>
      selection_observer_;
};

// Tests that no selection is returned if nothing is selected.
TEST_F(WebSelectionTabHelperTest, GetNoSelection) {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    // The tab helper is only created on iOS16+.
    return;
  }
  __block WebSelectionResponse* response = nil;
  WebSelectionTabHelper::FromWebState(web_state())
      ->GetSelectedText(base::BindOnce(^(WebSelectionResponse* block_response) {
        response = block_response;
      }));

  task_environment_.AdvanceClock(base::Milliseconds(500));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(response);

  // As a call is already in progress, the second call should only have to wait
  // for 500 more ms to get the selection (1 second total).
  __block WebSelectionResponse* response2 = nil;
  WebSelectionTabHelper::FromWebState(web_state())
      ->GetSelectedText(base::BindOnce(^(WebSelectionResponse* block_response) {
        response2 = block_response;
      }));
  task_environment_.AdvanceClock(base::Milliseconds(500));
  task_environment_.RunUntilIdle();

  // The 1st query waited twice 500ms, so the response should have been received
  // at this point.
  ASSERT_TRUE(response);
  EXPECT_FALSE(response.valid);
  EXPECT_NSEQ(nil, response.selectedText);
  EXPECT_TRUE(CGRectEqualToRect(response.sourceRect, CGRectZero));

  ASSERT_TRUE(response2);
  EXPECT_FALSE(response2.valid);
  EXPECT_NSEQ(nil, response2.selectedText);
  EXPECT_TRUE(CGRectEqualToRect(response2.sourceRect, CGRectZero));

  EXPECT_EQ(0, selection_observer_->GetNumberOfCalls());
}

// Tests that selection in main frame is returned correctly.
TEST_F(WebSelectionTabHelperTest, GetSelectionMainFrame) {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    // The tab helper is only created on iOS16+.
    return;
  }
  web::test::ExecuteJavaScript(@"window.getSelection().selectAllChildren("
                                "document.getElementById('selectid'));",
                               web_state());
  __block WebSelectionResponse* response = nil;
  WebSelectionTabHelper::FromWebState(web_state())
      ->GetSelectedText(base::BindOnce(^(WebSelectionResponse* block_response) {
        response = block_response;
      }));

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, /*run_message_loop=*/true,
      ^{
        return response != nil;
      }));
  EXPECT_TRUE(response.valid);
  EXPECT_NSEQ(@"selection", response.selectedText);
  EXPECT_FALSE(CGRectEqualToRect(response.sourceRect, CGRectZero));
  EXPECT_EQ(1, selection_observer_->GetNumberOfCalls());
}

// Tests that selection in iframe is returned correctly.
TEST_F(WebSelectionTabHelperTest, GetSelectionIFrame) {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    // The tab helper is only created on iOS16+.
    return;
  }
  web::test::ExecuteJavaScript(
      @"subWindow = document.getElementById('frame').contentWindow;"
       "subWindow.document.getSelection().selectAllChildren("
       "  subWindow.document.getElementById('frameselectid'));",
      web_state());
  __block WebSelectionResponse* response = nil;
  WebSelectionTabHelper::FromWebState(web_state())
      ->GetSelectedText(base::BindOnce(^(WebSelectionResponse* block_response) {
        response = block_response;
      }));

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, /*run_message_loop=*/true,
      ^{
        return response != nil;
      }));
  EXPECT_TRUE(response.valid);
  EXPECT_NSEQ(@"frame selection", response.selectedText);
  EXPECT_FALSE(CGRectEqualToRect(response.sourceRect, CGRectZero));
  EXPECT_EQ(1, selection_observer_->GetNumberOfCalls());
}

// Tests that selection is passed to the correct tab helper.
// Also tests that getting twice the selection on the same webState does not
// trigger additional JS calls.
TEST_F(WebSelectionTabHelperTest, GetMultipleWebStateSelections) {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    // The tab helper is only created on iOS16+.
    return;
  }
  web::WebState::CreateParams params(profile_.get());
  auto web_state2 = web::WebState::Create(params);
  WebSelectionTabHelper::CreateForWebState(web_state2.get());
  web::test::LoadHtml(kPage2HTML, web_state2.get());

  web::test::ExecuteJavaScript(@"window.getSelection().selectAllChildren("
                                "document.getElementById('selectid'));",
                               web_state());
  web::test::ExecuteJavaScript(@"window.getSelection().selectAllChildren("
                                "document.getElementById('selectid'));",
                               web_state2.get());

  __block WebSelectionResponse* response = nil;
  WebSelectionTabHelper::FromWebState(web_state())
      ->GetSelectedText(base::BindOnce(^(WebSelectionResponse* block_response) {
        response = block_response;
      }));

  __block WebSelectionResponse* response2 = nil;
  WebSelectionTabHelper::FromWebState(web_state2.get())
      ->GetSelectedText(base::BindOnce(^(WebSelectionResponse* block_response) {
        response2 = block_response;
      }));

  // No additional call to selection_observer should be done for this selection
  // retrieval.
  __block WebSelectionResponse* response3 = nil;
  WebSelectionTabHelper::FromWebState(web_state())
      ->GetSelectedText(base::BindOnce(^(WebSelectionResponse* block_response) {
        response3 = block_response;
      }));

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, /*run_message_loop=*/true,
      ^{
        return response != nil && response2 != nil && response3 != nil;
      }));

  EXPECT_TRUE(response.valid);
  EXPECT_NSEQ(@"selection", response.selectedText);
  EXPECT_FALSE(CGRectEqualToRect(response.sourceRect, CGRectZero));

  EXPECT_TRUE(response2.valid);
  EXPECT_NSEQ(@"selection2", response2.selectedText);
  EXPECT_FALSE(CGRectEqualToRect(response2.sourceRect, CGRectZero));

  EXPECT_TRUE(response3.valid);
  EXPECT_NSEQ(@"selection", response3.selectedText);
  EXPECT_FALSE(CGRectEqualToRect(response3.sourceRect, CGRectZero));

  EXPECT_EQ(response, response3);

  EXPECT_EQ(2, selection_observer_->GetNumberOfCalls());

  __block WebSelectionResponse* response4 = nil;
  WebSelectionTabHelper::FromWebState(web_state())
      ->GetSelectedText(base::BindOnce(^(WebSelectionResponse* block_response) {
        response4 = block_response;
      }));

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, /*run_message_loop=*/true,
      ^{
        return response4 != nil;
      }));

  EXPECT_TRUE(response4.valid);
  EXPECT_NSEQ(@"selection", response4.selectedText);
  EXPECT_FALSE(CGRectEqualToRect(response4.sourceRect, CGRectZero));
  EXPECT_EQ(3, selection_observer_->GetNumberOfCalls());
}
