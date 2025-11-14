// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <optional>

#import "base/apple/foundation_util.h"
#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "base/unguessable_token.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

@interface FakeScriptMessageHandlerForFrameRegistration
    : NSObject <WKScriptMessageHandler>

// Number of registration messages received.
@property(nonatomic, assign) int registrationsCount;

@end

@implementation FakeScriptMessageHandlerForFrameRegistration

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  ++self.registrationsCount;
}

@end

namespace {

using net::test_server::EmbeddedTestServer;
using net::test_server::HandlePrefixedRequest;
using ::testing::IsSubsetOf;
using ::testing::SizeIs;

// Delay for the registration round trip to complete, including all latencies.
constexpr base::TimeDelta kRegistrationDelay = base::Milliseconds(200);

constexpr char kMainPageBaseUrl[] = "https://chromium.test";

constexpr char kRemoteFrameRegistrationMessageHandlerName[] =
    "FrameRegistrationMessage";

// Gets delay for performing all attempts with the exponential backoff for
// retries and with some extra buffer to deal with other latencies such as the
// delay for the initial request. The delay is computed as follow:
// `base_delay_us` * (2^(`num_attempts` - 1) - 1) + `kRegistrationDelay`.
// `base_delay_us` corresponds to the delay used in the first retry in
// microseconds.
base::TimeDelta GetDelayForAllAttempts(int base_delay_us, int num_attempts) {
  return base::Microseconds(base_delay_us * ((1 << (num_attempts - 1)) - 1)) +
         kRegistrationDelay;
}

// Gets the delay for the next attempt following the previous `num_attempts`.
base::TimeDelta GetDelayForNextAttempt(int base_delay_us, int num_attempts) {
  return base::Microseconds(base_delay_us * ((1 << (num_attempts - 2))));
}

// Extract all tokens from the JS registration call `result` while skipping the
// tokens that can't be deserialized.
std::vector<autofill::RemoteFrameToken> ExtractTokensFromResult(id result) {
  if (!result) {
    return {};
  }
  NSArray<NSString*>* result_array =
      base::apple::ObjCCast<NSArray<NSString*>>(result);
  if (!result_array) {
    return {};
  }

  std::vector<autofill::RemoteFrameToken> extracted_tokens;
  for (NSString* item in result_array) {
    std::optional<base::UnguessableToken> token =
        autofill::DeserializeJavaScriptFrameId(base::SysNSStringToUTF8(item));
    if (token) {
      extracted_tokens.emplace_back(*token);
    }
  }

  return extracted_tokens;
}

// Sets up `test_server` to serve `contents` accessible at `path`.
void ServeDocument(const std::string& path,
                   const std::string& contents,
                   EmbeddedTestServer* test_server) {
  test_server->RegisterRequestHandler(base::BindRepeating(
      &HandlePrefixedRequest, "/" + path,
      base::BindRepeating(&testing::HandlePageWithHtml, contents)));
}

class ChildFrameRegistrationJavascriptTest : public web::JavascriptTest {
 protected:
  ChildFrameRegistrationJavascriptTest() {}
  ~ChildFrameRegistrationJavascriptTest() override {}

  void SetUp() override {
    web::JavascriptTest::SetUp();

    AddGCrWebScript();
    AddCommonScript();
    AddUserScript(@"autofill_form_features");
    AddUserScript(@"fill_util_test");
  }

  // Script that enables xframes on all frames and set registration attempts
  // counter.
  void SetFramesForTesting() {
    NSString* const script =
        @"__gCrWeb.getRegisteredApi('autofill_form_features')."
        @"getFunction('setAutofillAcrossIframes')(true);"
         "let registrationAttemptsCount = 0;"
         "for (const frame of document.querySelectorAll('iframe')) { "
         "frame.contentWindow.eval(\"__gCrWeb.getRegisteredApi('autofill_form_features')."
         "getFunction('setAutofillAcrossIframes')(true)\");"
         "}"
         "window.addEventListener('message', () => "
         "++registrationAttemptsCount);";
    web::test::ExecuteJavaScriptInWebView(web_view(), script);
  }

  // Returns the number of attempts performed so far.
  int GetRegistrationAttemptsCount() {
    id result =
        web::test::ExecuteJavaScript(web_view(), @"registrationAttemptsCount");
    return static_cast<int>([result doubleValue]);
  }
};

// Tests that child frames register themselves correctly with their host frame.
TEST_F(ChildFrameRegistrationJavascriptTest, RegisterFrames) {
  NSString* html = @"<body> outer frame"
                    "  <iframe srcdoc='<body>inner frame 1</body>'></iframe>"
                    "  <iframe srcdoc='<body>inner frame 2</body>'></iframe>"
                    "</body>";

  ASSERT_TRUE(LoadHtml(html, GURL(kMainPageBaseUrl)));

  id result = web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.getRegisteredApi('fill_test_api')."
                  @"getFunction('registerAllChildFrames')();");

  ASSERT_TRUE(result);
  NSArray<NSString*>* result_array =
      base::apple::ObjCCast<NSArray<NSString*>>(result);
  ASSERT_TRUE(result_array);
  EXPECT_EQ(2u, [result_array count]);
  for (NSString* item in result_array) {
    ASSERT_EQ(32u, [item length]);
    uint64_t unused;
    EXPECT_TRUE(base::HexStringToUInt64(
        base::SysNSStringToUTF8([item substringToIndex:16]), &unused));
    EXPECT_TRUE(base::HexStringToUInt64(
        base::SysNSStringToUTF8([item substringFromIndex:16]), &unused));
  }
}

// Tests that the registration tokens are cached and can be reused.
TEST_F(ChildFrameRegistrationJavascriptTest, RegisterFrames_Cache) {
  NSString* html = @"<body> outer frame"
                    "  <iframe srcdoc='<body>inner frame 1</body>'></iframe>"
                    "  <iframe srcdoc='<body>inner frame 2</body>'></iframe>"
                    "</body>";

  ASSERT_TRUE(LoadHtml(html, GURL(kMainPageBaseUrl)));

  // Do first registration and extract the tokens.
  id result1 = web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.getRegisteredApi('fill_test_api')."
                  @"getFunction('registerAllChildFrames')();");
  ASSERT_TRUE(result1);
  std::vector<autofill::RemoteFrameToken> remote_tokens_round1 =
      ExtractTokensFromResult(result1);
  EXPECT_THAT(remote_tokens_round1, SizeIs(2));

  // Do second registration and extract the tokens.
  id result2 = web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.getRegisteredApi('fill_test_api')."
                  @"getFunction('registerAllChildFrames')();");
  ASSERT_TRUE(result2);
  std::vector<autofill::RemoteFrameToken> remote_tokens_round2 =
      ExtractTokensFromResult(result2);
  EXPECT_THAT(remote_tokens_round2, SizeIs(2));

  // Verify that the cached tokens were reused when registring a second time.
  EXPECT_THAT(remote_tokens_round2, IsSubsetOf(remote_tokens_round1));
}

// Tests that child frames register themselves only once while deduping
// other attempts.
TEST_F(ChildFrameRegistrationJavascriptTest, RegisterFrames_Deduping) {
  FakeScriptMessageHandlerForFrameRegistration* msg_handler =
      [[FakeScriptMessageHandlerForFrameRegistration alloc] init];
  [web_view().configuration.userContentController
      addScriptMessageHandler:msg_handler
                         name:base::SysUTF8ToNSString(
                                  kRemoteFrameRegistrationMessageHandlerName)];

  NSString* const html =
      @"<body> outer frame"
       "  <iframe srcdoc='<body>inner frame 1</body>'></iframe>"
       "  <iframe srcdoc='<body>inner frame 2</body>'></iframe>"
       "</body>";
  ASSERT_TRUE(LoadHtml(html, GURL(kMainPageBaseUrl)));

  SetFramesForTesting();

  ASSERT_TRUE(web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.getRegisteredApi('fill_test_api')."
                  @"getFunction('registerAllChildFrames')();"));

  // Wait for both frames to register.
  ASSERT_TRUE(
      base::test::ios::WaitUntilConditionOrTimeout(kRegistrationDelay * 2, ^() {
        return msg_handler.registrationsCount == 2;
      }));
  EXPECT_EQ(2, GetRegistrationAttemptsCount());

  // Try re-registering the same frames.
  ASSERT_TRUE(web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.getRegisteredApi('fill_test_api')."
                  @"getFunction('registerAllChildFrames')();"));

  // Give enough time for the full registration round trip, if it did happen
  // (which would be an error at this point).
  base::test::ios::SpinRunLoopWithMinDelay(kRegistrationDelay);

  // Verify that no re-registration attempt was made as the token was already
  // registered with the browser (virtually at least considering the testing
  // context).
  EXPECT_EQ(2, msg_handler.registrationsCount);
  EXPECT_EQ(2, GetRegistrationAttemptsCount());
}

// Tests that child frames register themselves without retries when the child
// frames are of a different origin than their parent frame.
TEST_F(ChildFrameRegistrationJavascriptTest,
       RegisterFrames_Deduping_AcrossOrigins) {
  constexpr char kChildFrame1Path[] = "cf1";
  constexpr char kChildFrame2Path[] = "cf2";
  // Script that is injected into the frame to set up the frame for autofill
  // across iframes. There is some delay before executing the script to make
  // sure that the script dependencies were loaded into the webview.
  constexpr char kSetupScript[] = R"(
    // Number of frames that reported as ready, which will include this frame
    // if it is the main frame.
    let gFramesRdyCount = 0;
    // Set up the frame in a retry loop until the utils functions are injected.
    const timeoutFn = () => {
        if (typeof __gCrWeb != 'undefined' &&
            typeof __gCrWeb.getRegisteredApi('autofill_form_features').getFunction('setAutofillAcrossIframes') == 'function') {
          __gCrWeb.getRegisteredApi('autofill_form_features').getFunction('setAutofillAcrossIframes')(true);
          window.parent?.postMessage({type: 'frame-ready'}, '*');
          // Done.
          return;
        }
        // Retry as the util functions weren't injected yet.
        setTimeout(timeoutFn, 10);
    }
    // Start setup.
    setTimeout(timeoutFn, 10);
    // Track the frames that are set up and ready.
    window.addEventListener('message', (msg) => {
     if (msg?.data?.type === 'frame-ready') {
       ++gFramesRdyCount;
     }
    }))";

  std::string page_content = base::StringPrintf(
      "<body><form><input id='address'></form><script>%s</script></body>",
      kSetupScript);

  EmbeddedTestServer cf1_server;
  ServeDocument(kChildFrame1Path, page_content, &cf1_server);

  EmbeddedTestServer cf2_server;
  ServeDocument(kChildFrame2Path, page_content, &cf2_server);

  ASSERT_TRUE(cf1_server.Start());
  ASSERT_TRUE(cf2_server.Start());

  const std::string cf1_path =
      cf1_server.GetURL(base::StrCat({"/", kChildFrame1Path})).spec();
  const std::string cf2_path =
      cf2_server.GetURL(base::StrCat({"/", kChildFrame2Path})).spec();

  // Set up the JS message handler to capture the registration requests coming
  // from the renderer.
  FakeScriptMessageHandlerForFrameRegistration* msg_handler =
      [[FakeScriptMessageHandlerForFrameRegistration alloc] init];
  [web_view().configuration.userContentController
      addScriptMessageHandler:msg_handler
                         name:base::SysUTF8ToNSString(
                                  kRemoteFrameRegistrationMessageHandlerName)];

  // Content of the main frame.
  const std::string main_page_content =
      base::StringPrintf("<body> outer frame"
                         "  <iframe src='%s'></iframe>"
                         "  <iframe src='%s'></iframe>"
                         "  <script>%s</script>"
                         "</body>",
                         cf1_path.c_str(), cf2_path.c_str(), kSetupScript);

  EmbeddedTestServer main_test_server;
  ServeDocument("main", main_page_content, &main_test_server);
  ASSERT_TRUE(main_test_server.Start());
  const GURL main_url = main_test_server.GetURL("/main");

  // Load the main page that contains the child frames.
  LoadUrl(main_url);

  // Wait for the main frame and its child frames to be ready for testing.
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^bool {
        return [web::test::ExecuteJavaScript(
            web_view(), @"gFramesRdyCount === 3") boolValue];
      }));

  // Register the child frames of the main frame by calling the registration
  // script from the main frame. The child frames do not have child frames
  // themselves so no need to call the registration script for them.
  ASSERT_TRUE(web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.getRegisteredApi('fill_test_api')."
                  @"getFunction('registerAllChildFrames')();"));

  // Wait for both child frames to register.
  ASSERT_TRUE(
      base::test::ios::WaitUntilConditionOrTimeout(kRegistrationDelay * 2, ^() {
        return msg_handler.registrationsCount == 2;
      }));

  // Give enough time to the retry to happen if it would have been needed so we
  // can confirm that no retries were done.
  base::test::ios::SpinRunLoopWithMinDelay(
      GetDelayForAllAttempts(/*base_delay_us=*/50000, /*num_attempts=*/2));

  // Verify that no re-registration attempt was made as the token was already
  // registered with the browser.
  EXPECT_EQ(2, msg_handler.registrationsCount);
}

// Tests that there is no further attempts made once the max limit for attempts
// is reached.
TEST_F(ChildFrameRegistrationJavascriptTest,
       RegisterFrames_MaxAttemptsReached) {
  FakeScriptMessageHandlerForFrameRegistration* msg_handler =
      [[FakeScriptMessageHandlerForFrameRegistration alloc] init];
  [web_view().configuration.userContentController
      addScriptMessageHandler:msg_handler
                         name:base::SysUTF8ToNSString(
                                  kRemoteFrameRegistrationMessageHandlerName)];

  NSString* const html =
      @"<body> outer frame"
       "  <iframe srcdoc='<body>inner frame 1</body>'></iframe>"
       "  <iframe srcdoc='<body>inner frame 2</body>'></iframe>"
       "</body>";
  ASSERT_TRUE(LoadHtml(html, GURL(kMainPageBaseUrl)));

  SetFramesForTesting();

  // Change the setTimeout function to reduce the initial delay for registration
  // so this can be testable with the exponential backoff. Only tamper with the
  // delay when the callback function looks like the frame registration callback
  // to avoid altering the functionality for the other callers.
  {
    NSString* const script = @"const oldTimeoutFn = window.setTimeout; "
                              // Change the setTimeout function so it divides
                              // the delay to retry registrations.
                              "window.setTimeout = (fn, d, ...args) => { "
                              // Verify that the callback function is for frame
                              // registration. If yes, divide the timeout delay.
                              "  const r = /\\(\\)=>[a-z]+\\([a-z]+\\*2\\)/;"
                              "  const fs = fn.toString().replace(/\\s/g, '');"
                              "  if (r.test(fs)) { "
                              // Divide the delay by 20 iff this is for
                              // registration.
                              "    oldTimeoutFn(fn, d/20, ...args);"
                              "  } else {"
                              // Leave the delay as is otherwise so it doesn't
                              // alter the functionality for the other callers.
                              "    oldTimeoutFn(fn, d, ...args);"
                              "  }"
                              "};";
    web::test::ExecuteJavaScriptInWebView(web_view(), script);
  }

  // Set registration counter and mutate the command sent to frames to
  // invalidate the command, emulating a frame not responding.
  {
    NSString* const script =
        @"for (const frame of document.querySelectorAll('iframe')) { "
         "  const oldPostMessage = frame.contentWindow.postMessage;"
         // Wrap the window.postMessage methods of each child frame that needs
         // to be registered.
         "  frame.contentWindow.postMessage = function(message, targetOrigin, "
         "     transfer) {"
         // Only replace the command if this is for sending registration ack.
         // The frame receiving the mutated command won't respond, emulating a
         // unresponsive frame.
         "    if (message.command === 'registerAsChildFrame') {"
         "      ++registrationAttemptsCount;"
         "      message.command = 'invalid';"
         "      oldPostMessage(message, targetOrigin, transfer);"
         "    }"
         "  }"
         "}";
    web::test::ExecuteJavaScriptInWebView(web_view(), script);
  }

  const int base_delay_us = 2500;
  const int num_attempts_expected = 9;

  ASSERT_TRUE(web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.getRegisteredApi('fill_test_api')."
                  @"getFunction('registerAllChildFrames')();"));

  // Wait on the expected registration attempts to be done.
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      GetDelayForAllAttempts(base_delay_us, num_attempts_expected) +
          base::Seconds(1),
      ^() {
        return GetRegistrationAttemptsCount() == 18;
      }));

  // Give some time for the next attempt in the case the limit isn't respected.
  // Needs to be the next registration delay after 9 attempts plus some
  // buffer to deal with other latencies.
  base::test::ios::SpinRunLoopWithMinDelay(
      GetDelayForNextAttempt(base_delay_us, num_attempts_expected + 1) +
      kRegistrationDelay);

  // Verify that no registration did really happen since the frames didn't
  // respond.
  EXPECT_EQ(0, msg_handler.registrationsCount);

  // Verify that only 18 attempts (9 per frame) were made.
  EXPECT_EQ(18, GetRegistrationAttemptsCount());
}

// Tests that there is no further attempts made once the max registration
// capacity is reached.
TEST_F(ChildFrameRegistrationJavascriptTest,
       RegisterFrames_MaxRegistrationCapacityReached) {
  FakeScriptMessageHandlerForFrameRegistration* msg_handler =
      [[FakeScriptMessageHandlerForFrameRegistration alloc] init];
  [web_view().configuration.userContentController
      addScriptMessageHandler:msg_handler
                         name:base::SysUTF8ToNSString(
                                  kRemoteFrameRegistrationMessageHandlerName)];

  // Make page with 120 frames.
  NSMutableString* html =
      [NSMutableString stringWithString:@"<body> outer frame"];
  for (size_t i = 0; i < 120; ++i) {
    [html
        appendString:[NSString stringWithFormat:@" <iframe srcdoc='<body>inner "
                                                @"frame %zu</body>'></iframe>",
                                                i + 1]];
  }
  [html appendString:@"</body>"];

  ASSERT_TRUE(LoadHtml(html, GURL(kMainPageBaseUrl)));

  SetFramesForTesting();

  ASSERT_TRUE(web::test::ExecuteJavaScript(
      web_view(), @"__gCrWeb.getRegisteredApi('fill_test_api')."
                  @"getFunction('registerAllChildFrames')();"));

  // Wait on the first 100 frames to be registered.
  ASSERT_TRUE(
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(1), ^() {
        return msg_handler.registrationsCount == 100;
      }));

  // Give some time for more registrations in the case the limit isn't
  // respected.
  base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(500));

  // Verify that only 100 registrations (1 per frame) were made where the ones
  // exceeding the max capacity of the logbook were dropped.
  EXPECT_EQ(100, msg_handler.registrationsCount);
  EXPECT_EQ(100, GetRegistrationAttemptsCount());
}

}  // namespace
