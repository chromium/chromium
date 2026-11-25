// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_selection/model/web_selection_java_script_feature.h"

#import <Foundation/Foundation.h>

#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
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
NSString* const kPageHTML =
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
}

class TestWebSelectionJavaScriptFeatureObserver
    : public WebSelectionJavaScriptFeatureObserver {
 public:
  TestWebSelectionJavaScriptFeatureObserver(web::WebState* web_state)
      : web_state_(web_state) {}

  void OnSelectionRetrieved(web::WebState* web_state,
                            WebSelectionResponse* response) override {
    EXPECT_EQ(web_state, web_state_);
    response_ = response;
    ++response_count_;
  }

  WebSelectionResponse* GetLastResponse() { return response_; }
  int response_count() const { return response_count_; }

 private:
  raw_ptr<web::WebState> web_state_;
  WebSelectionResponse* response_;
  int response_count_ = 0;
};

// Tests for the WebSelectionJavaScriptFeature.
class WebSelectionJavaScriptFeatureTest : public PlatformTest {
 public:
  WebSelectionJavaScriptFeatureTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {
    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);

    selection_observer_ =
        std::make_unique<TestWebSelectionJavaScriptFeatureObserver>(
            web_state_.get());
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
TEST_F(WebSelectionJavaScriptFeatureTest, GetNoSelection) {
  WebSelectionJavaScriptFeature::GetInstance()->GetSelectedText(web_state());
  task_environment_.AdvanceClock(base::Seconds(1));
  task_environment_.RunUntilIdle();
  WebSelectionResponse* response = selection_observer_->GetLastResponse();
  // There is no selection, so the observer should not be called.
  ASSERT_FALSE(response);
}

// Tests that selection in main frame is returned correctly.
TEST_F(WebSelectionJavaScriptFeatureTest, GetSelectionMainFrame) {
  web::test::ExecuteJavaScript(@"window.getSelection().selectAllChildren("
                                "document.getElementById('selectid'));",
                               web_state());
  __block WebSelectionResponse* response = nil;
  WebSelectionJavaScriptFeature::GetInstance()->GetSelectedText(web_state());

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^{
        response = selection_observer_->GetLastResponse();
        return response != nil;
      }));

  EXPECT_TRUE(response.valid);
  EXPECT_NSEQ(@"selection", response.selectedText);
  EXPECT_FALSE(CGRectEqualToRect(response.sourceRect, CGRectZero));
}

// Tests that selection in iframe is returned correctly.
TEST_F(WebSelectionJavaScriptFeatureTest, GetSelectionIFrame) {
  web::test::ExecuteJavaScript(
      @"subWindow = document.getElementById('frame').contentWindow;"
       "subWindow.document.getSelection().selectAllChildren("
       "  subWindow.document.getElementById('frameselectid'));",
      web_state());
  __block WebSelectionResponse* response = nil;
  WebSelectionJavaScriptFeature::GetInstance()->GetSelectedText(web_state());

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^{
        response = selection_observer_->GetLastResponse();
        return response != nil;
      }));

  EXPECT_TRUE(response.valid);
  EXPECT_NSEQ(@"frame selection", response.selectedText);
  EXPECT_FALSE(CGRectEqualToRect(response.sourceRect, CGRectZero));
}

// Tests that malformed messages posted directly to the WebSelection handler are
// ignored without crashing or notifying observers.
TEST_F(WebSelectionJavaScriptFeatureTest, IgnoreMalformedMessages) {
  web::test::ExecuteJavaScript(
      @"const h = window.webkit.messageHandlers.WebSelection;"
       "for (const payload of ["
       "  'not-a-dict', 42, 3.14, true, [1, 2, 3],"
       "  {}, {selectedText: 123}, {selectedText: null},"
       "  {selectedText: 'hi'},"
       "  {selectedText: 'hi', selectionRect: 'invalid'},"
       "  {selectedText: 'hi', selectionRect: null},"
       "  {selectedText: 'hi', selectionRect: {}},"
       "  {selectedText: 'hi', selectionRect: {x: 'invalid', y: 0, width: 1,"
       "                                        height: 1}},"
       "  {selectedText: 'hi', selectionRect: {x: 0, y: 0, width: 1}},"
       "]) { h.postMessage(payload); }"
       "window.getSelection().selectAllChildren("
       "  document.getElementById('selectid'));",
      web_state());

  // Trigger a valid selection response afterwards to flush prior IPC messages
  // sent from the same frame.
  ASSERT_TRUE(WebSelectionJavaScriptFeature::GetInstance()->GetSelectedText(
      web_state()));

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^{
        return selection_observer_->GetLastResponse() != nil;
      }));

  EXPECT_EQ(1, selection_observer_->response_count());
  EXPECT_TRUE(selection_observer_->GetLastResponse().valid);
  EXPECT_NSEQ(@"selection",
              selection_observer_->GetLastResponse().selectedText);
}
