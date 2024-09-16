// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/java_script_console/java_script_console_feature.h"

#import <memory>
#import <optional>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/web/model/java_script_console/java_script_console_feature_delegate.h"
#import "ios/chrome/browser/web/model/java_script_console/java_script_console_feature_factory.h"
#import "ios/chrome/browser/web/model/java_script_console/java_script_console_message.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

const char kIframeHostName[] = "about:srcdoc";
const char kTestHostName[] = "https://chromium.test/";

class FakeJavaScriptConsoleFeatureDelegate
    : public JavaScriptConsoleFeatureDelegate {
 public:
  GURL last_received_message_url() {
    if (!last_received_message_) {
      return GURL();
    }
    return last_received_message_.value().url;
  }

  NSString* last_received_message_level() {
    if (!last_received_message_) {
      return nil;
    }
    return last_received_message_.value().level;
  }

  NSString* last_received_message() {
    if (!last_received_message_) {
      return nil;
    }
    return last_received_message_.value().message;
  }

  web::WebFrame* last_received_web_frame() { return last_received_web_frame_; }

  web::WebState* last_received_web_state() { return last_received_web_state_; }

 private:
  void DidReceiveConsoleMessage(
      web::WebState* web_state,
      web::WebFrame* sender_frame,
      const JavaScriptConsoleMessage& message) override {
    last_received_message_ = std::optional<JavaScriptConsoleMessage>(
        JavaScriptConsoleMessage(message));
    last_received_web_frame_ = sender_frame;
    last_received_web_state_ = web_state;
  }

  std::optional<JavaScriptConsoleMessage> last_received_message_;
  raw_ptr<web::WebFrame> last_received_web_frame_ = nullptr;
  raw_ptr<web::WebState> last_received_web_state_ = nullptr;
};

const char kPageHtml[] =
    "<html><body>"
    "<button id=\"debug\" onclick=\"console.debug('Debug message.')\"></button>"
    "<button id=\"error\" onclick=\"console.error('Error message.')\"></button>"
    "<button id=\"info\" onclick=\"console.info('Info message.')\"></button>"
    "<button id=\"log\" onclick=\"console.log('Log message.')\"></button>"
    "<button id=\"warn\" onclick=\"console.warn('Warn message.')\"></button>"
    "</body></html>";

const char kIFramePageHtml[] =
    "<html><body><iframe srcdoc=\""
    "<button id=&quot;debug&quot; "
    "onclick=&quot;console.debug('Debug message.')&quot;></button>"
    "<button id=&quot;error&quot; "
    "onclick=&quot;console.error('Error message.')&quot;></button>"
    "<button id=&quot;info&quot; "
    "onclick=&quot;console.info('Info message.')&quot;></button>"
    "<button id=&quot;log&quot; "
    "onclick=&quot;console.log('Log message.')&quot;></button>"
    "<button id=&quot;warn&quot; "
    "onclick=&quot;console.warn('Warn message.')&quot;></button>"
    "\" /></body></html>";
}  // namespace

// Tests console messages are received by JavaScriptConsoleFeature.
class JavaScriptConsoleFeatureTest : public PlatformTest {
 protected:
  JavaScriptConsoleFeatureTest()
      : web_client_(std::make_unique<web::FakeWebClient>()) {}

  void SetUp() override {
    PlatformTest::SetUp();

    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);

    JavaScriptConsoleFeature* feature =
        JavaScriptConsoleFeatureFactory::GetInstance()->GetForProfile(
            profile_.get());
    feature->SetDelegate(&delegate_);
    GetWebClient()->SetJavaScriptFeatures({feature});
  }

  web::FakeWebClient* GetWebClient() {
    return static_cast<web::FakeWebClient*>(web_client_.Get());
  }

  bool IsDelegateStateEmpty() {
    return !delegate_.last_received_web_state() &&
           !delegate_.last_received_web_frame() &&
           !delegate_.last_received_message_url().is_valid() &&
           !delegate_.last_received_message_level() &&
           !delegate_.last_received_message();
  }

  web::WebFrame* GetWebFrameForIframe() {
    web::WebFrame* main_frame =
        web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame();
    web::WebFrame* iframe = nullptr;
    for (web::WebFrame* web_frame :
         web_state()->GetPageWorldWebFramesManager()->GetAllWebFrames()) {
      if (web_frame != main_frame) {
        iframe = web_frame;
        break;
      }
    }
    return iframe;
  }

  web::WebState* web_state() { return web_state_.get(); }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;

  FakeJavaScriptConsoleFeatureDelegate delegate_;
};

// Tests that debug console message details are received from the main frame.
TEST_F(JavaScriptConsoleFeatureTest, DebugMessageReceivedMainFrame) {
  ASSERT_TRUE(IsDelegateStateEmpty());

  web::test::LoadHtml(base::SysUTF8ToNSString(kPageHtml), web_state());
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "debug"));

  EXPECT_EQ(web_state(), delegate_.last_received_web_state());
  web::WebFrame* web_frame =
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame();
  EXPECT_EQ(web_frame, delegate_.last_received_web_frame());
  EXPECT_EQ(kTestHostName, delegate_.last_received_message_url());
  EXPECT_NSEQ(@"debug", delegate_.last_received_message_level());
  EXPECT_NSEQ(@"Debug message.", delegate_.last_received_message());
}

// Tests that error console message details are received from the main frame.
TEST_F(JavaScriptConsoleFeatureTest, ErrorMessageReceivedMainFrame) {
  ASSERT_TRUE(IsDelegateStateEmpty());

  web::test::LoadHtml(base::SysUTF8ToNSString(kPageHtml), web_state());
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "error"));

  EXPECT_EQ(web_state(), delegate_.last_received_web_state());
  web::WebFrame* web_frame =
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame();
  EXPECT_EQ(web_frame, delegate_.last_received_web_frame());
  EXPECT_EQ(kTestHostName, delegate_.last_received_message_url());
  EXPECT_NSEQ(@"error", delegate_.last_received_message_level());
  EXPECT_NSEQ(@"Error message.", delegate_.last_received_message());
}

// Tests that info console message details are received from the main frame.
TEST_F(JavaScriptConsoleFeatureTest, InfoMessageReceivedMainFrame) {
  ASSERT_TRUE(IsDelegateStateEmpty());

  web::test::LoadHtml(base::SysUTF8ToNSString(kPageHtml), web_state());
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "info"));

  EXPECT_EQ(web_state(), delegate_.last_received_web_state());
  web::WebFrame* web_frame =
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame();
  EXPECT_EQ(web_frame, delegate_.last_received_web_frame());
  EXPECT_EQ(kTestHostName, delegate_.last_received_message_url());
  EXPECT_NSEQ(@"info", delegate_.last_received_message_level());
  EXPECT_NSEQ(@"Info message.", delegate_.last_received_message());
}

// Tests that log console message details are received from the main frame.
TEST_F(JavaScriptConsoleFeatureTest, LogMessageReceivedMainFrame) {
  ASSERT_TRUE(IsDelegateStateEmpty());

  web::test::LoadHtml(base::SysUTF8ToNSString(kPageHtml), web_state());
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "log"));

  EXPECT_EQ(web_state(), delegate_.last_received_web_state());
  web::WebFrame* web_frame =
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame();
  EXPECT_EQ(web_frame, delegate_.last_received_web_frame());
  EXPECT_EQ(kTestHostName, delegate_.last_received_message_url());
  EXPECT_NSEQ(@"log", delegate_.last_received_message_level());
  EXPECT_NSEQ(@"Log message.", delegate_.last_received_message());
}

// Tests that warning console message details are received from the main frame.
TEST_F(JavaScriptConsoleFeatureTest, WarnMessageReceivedMainFrame) {
  ASSERT_TRUE(IsDelegateStateEmpty());

  web::test::LoadHtml(base::SysUTF8ToNSString(kPageHtml), web_state());
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "warn"));

  EXPECT_EQ(web_state(), delegate_.last_received_web_state());
  web::WebFrame* web_frame =
      web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame();
  EXPECT_EQ(web_frame, delegate_.last_received_web_frame());
  EXPECT_EQ(kTestHostName, delegate_.last_received_message_url());
  EXPECT_NSEQ(@"warn", delegate_.last_received_message_level());
  EXPECT_NSEQ(@"Warn message.", delegate_.last_received_message());
}

// Tests that debug console message details are received from an iframe.
TEST_F(JavaScriptConsoleFeatureTest, DebugMessageReceivedIFrame) {
  ASSERT_TRUE(IsDelegateStateEmpty());

  web::test::LoadHtml(base::SysUTF8ToNSString(kIFramePageHtml), web_state());
  ASSERT_TRUE(web::test::TapWebViewElementWithIdInIframe(web_state(), "debug"));

  ASSERT_EQ(web_state(), delegate_.last_received_web_state());

  web::WebFrame* iframe = GetWebFrameForIframe();
  ASSERT_TRUE(iframe);
  EXPECT_EQ(iframe, delegate_.last_received_web_frame());

  EXPECT_EQ(kIframeHostName, delegate_.last_received_message_url());
  EXPECT_NSEQ(@"debug", delegate_.last_received_message_level());
  EXPECT_NSEQ(@"Debug message.", delegate_.last_received_message());
}

// Tests that error console message details are received from an iframe.
TEST_F(JavaScriptConsoleFeatureTest, ErrorMessageReceivedIFrame) {
  ASSERT_TRUE(IsDelegateStateEmpty());

  web::test::LoadHtml(base::SysUTF8ToNSString(kIFramePageHtml), web_state());
  ASSERT_TRUE(web::test::TapWebViewElementWithIdInIframe(web_state(), "error"));

  ASSERT_EQ(web_state(), delegate_.last_received_web_state());

  web::WebFrame* iframe = GetWebFrameForIframe();
  ASSERT_TRUE(iframe);
  EXPECT_EQ(iframe, delegate_.last_received_web_frame());

  EXPECT_EQ(kIframeHostName, delegate_.last_received_message_url());
  EXPECT_NSEQ(@"error", delegate_.last_received_message_level());
  EXPECT_NSEQ(@"Error message.", delegate_.last_received_message());
}

// Tests that info console message details are received from an iframe.
TEST_F(JavaScriptConsoleFeatureTest, InfoMessageReceivedIFrame) {
  ASSERT_TRUE(IsDelegateStateEmpty());

  web::test::LoadHtml(base::SysUTF8ToNSString(kIFramePageHtml), web_state());
  ASSERT_TRUE(web::test::TapWebViewElementWithIdInIframe(web_state(), "info"));

  ASSERT_EQ(web_state(), delegate_.last_received_web_state());

  web::WebFrame* iframe = GetWebFrameForIframe();
  ASSERT_TRUE(iframe);
  EXPECT_EQ(iframe, delegate_.last_received_web_frame());

  EXPECT_EQ(kIframeHostName, delegate_.last_received_message_url());
  EXPECT_NSEQ(@"info", delegate_.last_received_message_level());
  EXPECT_NSEQ(@"Info message.", delegate_.last_received_message());
}

// Tests that log console message details are received from an iframe.
TEST_F(JavaScriptConsoleFeatureTest, LogMessageReceivedIFrame) {
  ASSERT_TRUE(IsDelegateStateEmpty());

  web::test::LoadHtml(base::SysUTF8ToNSString(kIFramePageHtml), web_state());
  ASSERT_TRUE(web::test::TapWebViewElementWithIdInIframe(web_state(), "log"));

  ASSERT_EQ(web_state(), delegate_.last_received_web_state());

  web::WebFrame* iframe = GetWebFrameForIframe();
  ASSERT_TRUE(iframe);
  EXPECT_EQ(iframe, delegate_.last_received_web_frame());

  EXPECT_EQ(kIframeHostName, delegate_.last_received_message_url());
  EXPECT_NSEQ(@"log", delegate_.last_received_message_level());
  EXPECT_NSEQ(@"Log message.", delegate_.last_received_message());
}

// Tests that warning console message details are received from an iframe.
TEST_F(JavaScriptConsoleFeatureTest, WarnMessageReceivedIFrame) {
  ASSERT_TRUE(IsDelegateStateEmpty());

  web::test::LoadHtml(base::SysUTF8ToNSString(kIFramePageHtml), web_state());
  ASSERT_TRUE(web::test::TapWebViewElementWithIdInIframe(web_state(), "warn"));

  ASSERT_EQ(web_state(), delegate_.last_received_web_state());

  web::WebFrame* iframe = GetWebFrameForIframe();
  ASSERT_TRUE(iframe);
  EXPECT_EQ(iframe, delegate_.last_received_web_frame());

  EXPECT_EQ(kIframeHostName, delegate_.last_received_message_url());
  EXPECT_NSEQ(@"warn", delegate_.last_received_message_level());
  EXPECT_NSEQ(@"Warn message.", delegate_.last_received_message());
}
