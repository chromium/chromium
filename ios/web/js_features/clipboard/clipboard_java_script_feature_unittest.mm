// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/clipboard/clipboard_java_script_feature.h"

#import <optional>

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "ios/components/enterprise/data_controls/clipboard_enums.h"
#import "ios/web/js_features/clipboard/clipboard_constants.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_state_delegate.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {
const char kHttpsTestUrl[] = "https://chromium.test/";
}  // namespace

namespace web {

// A fake WebStateDelegate for testing clipboard interactions.
class ClipboardFakeWebStateDelegate : public web::FakeWebStateDelegate {
 public:
  ClipboardFakeWebStateDelegate() = default;

  void ShouldAllowCopy(WebState* source,
                       base::OnceCallback<void(bool)> callback) override {
    copy_called_ = true;
    std::move(callback).Run(should_allow_copy_);
  }

  void ShouldAllowPaste(web::WebState* source,
                        base::OnceCallback<void(bool)> callback) override {
    paste_called_ = true;
    std::move(callback).Run(should_allow_paste_);
  }

  void DidFinishClipboardRead(web::WebState* source) override {
    did_finish_clipboard_read_called_ = true;
  }

  bool copy_called() const { return copy_called_; }
  bool paste_called() const { return paste_called_; }
  bool did_finish_clipboard_read_called() const {
    return did_finish_clipboard_read_called_;
  }

  void set_should_allow_copy(bool allow) { should_allow_copy_ = allow; }
  void set_should_allow_paste(bool allow) { should_allow_paste_ = allow; }

 private:
  bool copy_called_ = false;
  bool paste_called_ = false;
  bool did_finish_clipboard_read_called_ = false;
  bool should_allow_copy_ = true;
  bool should_allow_paste_ = true;
};

class ClipboardJavaScriptFeatureTest : public WebTestWithWebState {
 protected:
  ClipboardJavaScriptFeatureTest()
      : WebTestWithWebState(std::make_unique<web::FakeWebClient>()),
        feature_(ClipboardJavaScriptFeature::GetInstance()) {}

  void SetUp() override {
    WebTestWithWebState::SetUp();
    static_cast<web::FakeWebClient*>(GetWebClient())
        ->SetJavaScriptFeatures({feature_});
    web_state()->SetDelegate(&delegate_);
    LoadHtml(@"<html></html>", GURL(kHttpsTestUrl));

    // Inject a script to record the result of the promise. This replaces the
    // real __gCrWeb.clipboard.resolveRequest with a test spy.
    ExecuteJavaScript(
        @"window.testState = {};"
        @"function resolveRequestSpy(id, allowed) { "
        @"window.testState.lastResult = { id: id, allowed: allowed };  }"
        @"clipboardApi = __gCrWeb.getRegisteredApi('clipboard');"
        @"clipboardApi.addFunction('resolveRequest', resolveRequestSpy);");
  }

  // Simulates a script message from the web page.
  void SimulateScriptMessage(const std::string& command,
                             std::optional<int> request_id = std::nullopt) {
    base::Value::Dict body;
    body.Set(kCommandKey, command);
    if (request_id) {
      body.Set(kRequestIdKey, *request_id);
    }
    web::WebFrame* main_frame = WaitForMainFrame();
    body.Set(kFrameIdKey, main_frame->GetFrameId());

    web::ScriptMessage message(std::make_unique<base::Value>(std::move(body)),
                               /*is_user_interacting=*/true,
                               /*is_main_frame=*/true, GURL::EmptyGURL());
    feature_->ScriptMessageReceived(web_state(), message);
  }

  // Gets the result of the last promise resolution.
  std::optional<bool> GetLastResultForRequestId(int request_id) {
    id js_result = ExecuteJavaScript(@"window.testState.lastResult;");
    NSDictionary* result_dict = base::apple::ObjCCast<NSDictionary>(js_result);
    if (!result_dict) {
      return std::nullopt;
    }

    NSNumber* result_id = base::apple::ObjCCast<NSNumber>(result_dict[@"id"]);
    if (!result_id || result_id.intValue != request_id) {
      return std::nullopt;
    }

    NSNumber* allowed =
        base::apple::ObjCCast<NSNumber>(result_dict[@"allowed"]);
    if (!allowed) {
      return std::nullopt;
    }

    return allowed.boolValue;
  }

  // Returns the main frame of `web_state()`.
  WebFrame* WaitForMainFrame() {
    __block web::WebFrame* main_frame = nullptr;
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
      web::WebFramesManager* frames_manager = web_state()->GetWebFramesManager(
          web::ContentWorld::kPageContentWorld);
      main_frame = frames_manager->GetMainWebFrame();
      return main_frame != nullptr;
    }));
    return main_frame;
  }

  raw_ptr<ClipboardJavaScriptFeature> feature_;
  ClipboardFakeWebStateDelegate delegate_;
  base::HistogramTester histogram_tester_;
};

// Tests that a "write" command is allowed when the delegate allows it.
TEST_F(ClipboardJavaScriptFeatureTest, WriteAllowed) {
  delegate_.set_should_allow_copy(true);
  SimulateScriptMessage(kWriteCommand, 1);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return delegate_.copy_called();
  }));

  std::optional<bool> result = GetLastResultForRequestId(1);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.value());
  histogram_tester_.ExpectUniqueSample(
      "IOS.WebState.Clipboard.Copy.Source",
      static_cast<int>(data_controls::ClipboardSource::kClipboardAPI), 1);
  histogram_tester_.ExpectUniqueSample("IOS.WebState.Clipboard.Copy.Outcome",
                                       true, 1);
}

// Tests that a "write" command is denied when the delegate denies it.
TEST_F(ClipboardJavaScriptFeatureTest, WriteDenied) {
  delegate_.set_should_allow_copy(false);
  SimulateScriptMessage(kWriteCommand, 2);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return delegate_.copy_called();
  }));

  std::optional<bool> result = GetLastResultForRequestId(2);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result.value());
  histogram_tester_.ExpectUniqueSample(
      "IOS.WebState.Clipboard.Copy.Source",
      static_cast<int>(data_controls::ClipboardSource::kClipboardAPI), 1);
  histogram_tester_.ExpectUniqueSample("IOS.WebState.Clipboard.Copy.Outcome",
                                       false, 1);
}

// Tests that a "read" command is allowed when the delegate allows it.
TEST_F(ClipboardJavaScriptFeatureTest, ReadAllowed) {
  delegate_.set_should_allow_paste(true);
  SimulateScriptMessage(kReadCommand, 3);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return delegate_.paste_called();
  }));

  std::optional<bool> result = GetLastResultForRequestId(3);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.value());
  histogram_tester_.ExpectUniqueSample(
      "IOS.WebState.Clipboard.Paste.Source",
      static_cast<int>(data_controls::ClipboardSource::kClipboardAPI), 1);
  histogram_tester_.ExpectUniqueSample("IOS.WebState.Clipboard.Paste.Outcome",
                                       true, 1);
}

// Tests that a "read" command is denied when the delegate denies it.
TEST_F(ClipboardJavaScriptFeatureTest, ReadDenied) {
  delegate_.set_should_allow_paste(false);
  SimulateScriptMessage(kReadCommand, 4);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return delegate_.paste_called();
  }));

  std::optional<bool> result = GetLastResultForRequestId(4);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result.value());
  histogram_tester_.ExpectUniqueSample(
      "IOS.WebState.Clipboard.Paste.Source",
      static_cast<int>(data_controls::ClipboardSource::kClipboardAPI), 1);
}

// Tests that commands are allowed if there is no delegate.
TEST_F(ClipboardJavaScriptFeatureTest, NoDelegate) {
  web_state()->SetDelegate(nullptr);

  SimulateScriptMessage(kWriteCommand, 5);
  std::optional<bool> write_result = GetLastResultForRequestId(5);
  ASSERT_TRUE(write_result.has_value());
  EXPECT_TRUE(write_result.value());

  SimulateScriptMessage(kReadCommand, 6);
  std::optional<bool> read_result = GetLastResultForRequestId(6);
  ASSERT_TRUE(read_result.has_value());
  EXPECT_TRUE(read_result.value());
}

// Tests that the DidFinishClipboardRead delegate method is called.
TEST_F(ClipboardJavaScriptFeatureTest, DidFinishClipboardRead) {
  ASSERT_FALSE(delegate_.did_finish_clipboard_read_called());
  SimulateScriptMessage(kDidFinishClipboardReadCommand);
  EXPECT_TRUE(delegate_.did_finish_clipboard_read_called());
}

// Tests that the DidFinishClipboardRead delegate method is not called for other
// commands.
TEST_F(ClipboardJavaScriptFeatureTest,
       DidFinishClipboardReadNotCalledForRequests) {
  SimulateScriptMessage(kWriteCommand, 1);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return delegate_.copy_called();
  }));
  EXPECT_FALSE(delegate_.did_finish_clipboard_read_called());

  SimulateScriptMessage(kReadCommand, 2);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return delegate_.paste_called();
  }));
  EXPECT_FALSE(delegate_.did_finish_clipboard_read_called());
}

}  // namespace web
