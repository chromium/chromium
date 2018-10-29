// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/clear_site_data_throttle.h"

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_task_environment.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/load_flags.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace content {

using ConsoleMessagesDelegate = ClearSiteDataThrottle::ConsoleMessagesDelegate;

namespace {

const char kClearSiteDataHeaderPrefix[] = "Clear-Site-Data: ";

const char kClearCookiesHeader[] = "Clear-Site-Data: \"cookies\"";

void WaitForUIThread() {
  base::RunLoop run_loop;
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           run_loop.QuitClosure());
  run_loop.Run();
}

// Used to verify that resource throttle delegate calls are made.
class MockResourceThrottleDelegate : public ResourceThrottle::Delegate {
 public:
  MOCK_METHOD0(Cancel, void());
  MOCK_METHOD0(CancelAndIgnore, void());
  MOCK_METHOD1(CancelWithError, void(int));
  MOCK_METHOD0(Resume, void());
};

// A slightly modified ClearSiteDataThrottle for testing with unconditional
// construction, injectable response headers, and dummy clearing functionality.
class TestThrottle : public ClearSiteDataThrottle {
 public:
  TestThrottle(net::URLRequest* request,
               std::unique_ptr<ConsoleMessagesDelegate> delegate)
      : ClearSiteDataThrottle(request, std::move(delegate)) {}
  ~TestThrottle() override {}

  void SetResponseHeaders(const std::string& headers) {
    std::string headers_with_status_code = "HTTP/1.1 200\n" + headers;
    headers_ = new net::HttpResponseHeaders(net::HttpUtil::AssembleRawHeaders(
        headers_with_status_code.c_str(), headers_with_status_code.size()));
  }

  MOCK_METHOD4(ClearSiteData,
               void(const url::Origin& origin,
                    bool clear_cookies,
                    bool clear_storage,
                    bool clear_cache));

 protected:
  const net::HttpResponseHeaders* GetResponseHeaders() const override {
    return headers_.get();
  }

  void ExecuteClearingTask(const url::Origin& origin,
                           bool clear_cookies,
                           bool clear_storage,
                           bool clear_cache,
                           base::OnceClosure callback) override {
    ClearSiteData(origin, clear_cookies, clear_storage, clear_cache);

    // NOTE: ResourceThrottle expects Resume() to be called asynchronously.
    // For the purposes of this test, synchronous call works correctly, and
    // is preferable for simplicity, so that we don't have to synchronize
    // between triggering Clear-Site-Data and verifying test expectations.
    std::move(callback).Run();
  }

 private:
  scoped_refptr<net::HttpResponseHeaders> headers_;
};

// A TestThrottle with modifiable current url.
class RedirectableTestThrottle : public TestThrottle {
 public:
  RedirectableTestThrottle(net::URLRequest* request,
                           std::unique_ptr<ConsoleMessagesDelegate> delegate)
      : TestThrottle(request, std::move(delegate)) {}

  const GURL& GetCurrentURL() const override {
    return current_url_.is_valid() ? current_url_
                                   : TestThrottle::GetCurrentURL();
  }

  void SetCurrentURLForTesting(const GURL& url) { current_url_ = url; }

 private:
  GURL current_url_;
};

// A ConsoleDelegate that outputs messages to a string |output_buffer| owned
// by the caller instead of to the console (losing the level information).
class StringConsoleMessagesDelegate : public ConsoleMessagesDelegate {
 public:
  StringConsoleMessagesDelegate(std::string* output_buffer) {
    SetOutputFormattedMessageFunctionForTesting(
        base::Bind(&StringConsoleMessagesDelegate::OutputFormattedMessage,
                   base::Unretained(output_buffer)));
  }

  ~StringConsoleMessagesDelegate() override {}

 private:
  static void OutputFormattedMessage(std::string* output_buffer,
                                     WebContents* web_contents,
                                     ConsoleMessageLevel level,
                                     const std::string& formatted_text) {
    *output_buffer += formatted_text + "\n";
  }
};

}  // namespace

class ClearSiteDataThrottleTest : public testing::Test {
 public:
  ClearSiteDataThrottleTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

 private:
  TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(ClearSiteDataThrottleTest);
};

TEST_F(ClearSiteDataThrottleTest, MaybeCreateThrottleForRequest) {
  // Create a URL request.
  GURL url("https://www.example.com");
  net::TestURLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      url, net::DEFAULT_PRIORITY, nullptr, TRAFFIC_ANNOTATION_FOR_TESTS));

  // We will not create the throttle for an empty ResourceRequestInfo.
  EXPECT_FALSE(
      ClearSiteDataThrottle::MaybeCreateThrottleForRequest(request.get()));

  // We can create the throttle for a valid ResourceRequestInfo.
  ResourceRequestInfo::AllocateForTesting(request.get(), RESOURCE_TYPE_IMAGE,
                                          nullptr, 0, 0, 0, false, true, true,
                                          false, nullptr);
  EXPECT_TRUE(
      ClearSiteDataThrottle::MaybeCreateThrottleForRequest(request.get()));
}

TEST_F(ClearSiteDataThrottleTest, ParseHeaderAndExecuteClearingTask) {
  struct TestCase {
    const char* header;
    bool cookies;
    bool storage;
    bool cache;
  };

  std::vector<TestCase> standard_test_cases = {
      // One data type.
      {"\"cookies\"", true, false, false},
      {"\"storage\"", false, true, false},
      {"\"cache\"", false, false, true},

      // Two data types.
      {"\"cookies\", \"storage\"", true, true, false},
      {"\"cookies\", \"cache\"", true, false, true},
      {"\"storage\", \"cache\"", false, true, true},

      // Three data types.
      {"\"storage\", \"cache\", \"cookies\"", true, true, true},
      {"\"cache\", \"cookies\", \"storage\"", true, true, true},
      {"\"cookies\", \"storage\", \"cache\"", true, true, true},

      // The wildcard datatype is not yet shipped.
      {"\"*\", \"storage\"", false, true, false},
      {"\"cookies\", \"*\", \"storage\"", true, true, false},
      {"\"*\", \"cookies\", \"*\"", true, false, false},

      // Different formatting.
      {"\"cookies\"", true, false, false},

      // Duplicates.
      {"\"cookies\", \"cookies\"", true, false, false},

      // Other JSON-formatted items in the list.
      {"\"storage\", { \"other_params\": {} }", false, true, false},

      // Unknown types are ignored, but we still proceed with the deletion for
      // those that we recognize.
      {"\"cache\", \"foo\"", false, false, true},
  };

  std::vector<TestCase> experimental_test_cases = {
      // Wildcard.
      {"\"*\"", true, true, true},
      {"\"*\", \"storage\"", true, true, true},
      {"\"cache\", \"*\", \"storage\"", true, true, true},
      {"\"*\", \"cookies\", \"*\"", true, true, true},
  };

  const std::vector<TestCase>* test_case_sets[] = {&standard_test_cases,
                                                   &experimental_test_cases};

  for (const std::vector<TestCase>* test_cases : test_case_sets) {
    base::test::ScopedCommandLine scoped_command_line;
    if (test_cases == &experimental_test_cases) {
      scoped_command_line.GetProcessCommandLine()->AppendSwitch(
          switches::kEnableExperimentalWebPlatformFeatures);
    }

    for (const TestCase& test_case : *test_cases) {
      SCOPED_TRACE(test_case.header);

      // Test that ParseHeader works correctly.
      bool actual_cookies;
      bool actual_storage;
      bool actual_cache;

      GURL url("https://example.com");
      ConsoleMessagesDelegate console_delegate;

      EXPECT_TRUE(ClearSiteDataThrottle::ParseHeaderForTesting(
          test_case.header, &actual_cookies, &actual_storage, &actual_cache,
          &console_delegate, url));

      EXPECT_EQ(test_case.cookies, actual_cookies);
      EXPECT_EQ(test_case.storage, actual_storage);
      EXPECT_EQ(test_case.cache, actual_cache);

      // Test that a call with the above parameters actually reaches
      // ExecuteClearingTask().
      net::TestURLRequestContext context;
      std::unique_ptr<net::URLRequest> request(context.CreateRequest(
          url, net::DEFAULT_PRIORITY, nullptr, TRAFFIC_ANNOTATION_FOR_TESTS));
      TestThrottle throttle(request.get(),
                            std::make_unique<ConsoleMessagesDelegate>());
      MockResourceThrottleDelegate delegate;
      throttle.set_delegate_for_testing(&delegate);
      throttle.SetResponseHeaders(std::string(kClearSiteDataHeaderPrefix) +
                                  test_case.header);

      EXPECT_CALL(throttle,
                  ClearSiteData(url::Origin::Create(url), test_case.cookies,
                                test_case.storage, test_case.cache));
      bool defer;
      throttle.WillProcessResponse(&defer);
      EXPECT_TRUE(defer);

      testing::Mock::VerifyAndClearExpectations(&throttle);
    }
  }
}

TEST_F(ClearSiteDataThrottleTest, InvalidHeader) {
  struct TestCase {
    const char* header;
    const char* console_message;
  } test_cases[] = {{"", "No recognized types specified.\n"},
                    {"\"unclosed",
                     "Unrecognized type: \"unclosed.\n"
                     "No recognized types specified.\n"},
                    {"\"passwords\"",
                     "Unrecognized type: \"passwords\".\n"
                     "No recognized types specified.\n"},
                    // The wildcard datatype is not yet shipped.
                    {"[ \"*\" ]",
                     "Unrecognized type: [ \"*\" ].\n"
                     "No recognized types specified.\n"},
                    {"[ \"list\" ]",
                     "Unrecognized type: [ \"list\" ].\n"
                     "No recognized types specified.\n"},
                    {"{ \"cookies\": [ \"a\" ] }",
                     "Unrecognized type: { \"cookies\": [ \"a\" ] }.\n"
                     "No recognized types specified.\n"},
                    {"\"кукис\", \"сторидж\", \"кэш\"",
                     "Must only contain ASCII characters.\n"}};

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.header);

    bool actual_cookies;
    bool actual_storage;
    bool actual_cache;

    ConsoleMessagesDelegate console_delegate;

    EXPECT_FALSE(ClearSiteDataThrottle::ParseHeaderForTesting(
        test_case.header, &actual_cookies, &actual_storage, &actual_cache,
        &console_delegate, GURL()));

    std::string multiline_message;
    for (const auto& message : console_delegate.messages()) {
      EXPECT_EQ(CONSOLE_MESSAGE_LEVEL_ERROR, message.level);
      multiline_message += message.text + "\n";
    }

    EXPECT_EQ(test_case.console_message, multiline_message);
  }
}

TEST_F(ClearSiteDataThrottleTest, LoadDoNotSaveCookies) {
  net::TestURLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("https://www.example.com"), net::DEFAULT_PRIORITY, nullptr,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  std::unique_ptr<ConsoleMessagesDelegate> scoped_console_delegate(
      new ConsoleMessagesDelegate());
  const ConsoleMessagesDelegate* console_delegate =
      scoped_console_delegate.get();
  TestThrottle throttle(request.get(), std::move(scoped_console_delegate));
  MockResourceThrottleDelegate delegate;
  throttle.set_delegate_for_testing(&delegate);
  throttle.SetResponseHeaders(kClearCookiesHeader);

  EXPECT_CALL(throttle, ClearSiteData(_, _, _, _));
  bool defer;
  throttle.WillProcessResponse(&defer);
  EXPECT_TRUE(defer);
  EXPECT_EQ(1u, console_delegate->messages().size());
  EXPECT_EQ(
      "Cleared data types: \"cookies\". "
      "Clearing channel IDs and HTTP authentication cache is currently "
      "not supported, as it breaks active network connections.",
      console_delegate->messages().front().text);
  EXPECT_EQ(console_delegate->messages().front().level,
            CONSOLE_MESSAGE_LEVEL_INFO);
  testing::Mock::VerifyAndClearExpectations(&throttle);

  request->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES);
  EXPECT_CALL(throttle, ClearSiteData(_, _, _, _)).Times(0);
  throttle.WillProcessResponse(&defer);
  EXPECT_FALSE(defer);
  EXPECT_EQ(2u, console_delegate->messages().size());
  EXPECT_EQ(
      "The request's credentials mode prohibits modifying cookies "
      "and other local data.",
      console_delegate->messages().rbegin()->text);
  EXPECT_EQ(CONSOLE_MESSAGE_LEVEL_ERROR,
            console_delegate->messages().rbegin()->level);
  testing::Mock::VerifyAndClearExpectations(&throttle);
}

TEST_F(ClearSiteDataThrottleTest, InvalidOrigin) {
  struct TestCase {
    const char* origin;
    bool expect_success;
    std::string error_message;  // Tested only if |expect_success| = false.
  } kTestCases[] = {
      // The throttle only works on secure origins.
      {"https://secure-origin.com", true, ""},
      {"filesystem:https://secure-origin.com/temporary/", true, ""},

      // That includes localhost.
      {"http://localhost", true, ""},

      // Not on insecure origins.
      {"http://insecure-origin.com", false,
       "Not supported for insecure origins."},
      {"filesystem:http://insecure-origin.com/temporary/", false,
       "Not supported for insecure origins."},

      // Not on unique origins.
      {"data:unique-origin;", false, "Not supported for unique origins."},
  };

  net::TestURLRequestContext context;

  for (const TestCase& test_case : kTestCases) {
    std::unique_ptr<net::URLRequest> request(
        context.CreateRequest(GURL(test_case.origin), net::DEFAULT_PRIORITY,
                              nullptr, TRAFFIC_ANNOTATION_FOR_TESTS));
    std::unique_ptr<ConsoleMessagesDelegate> scoped_console_delegate(
        new ConsoleMessagesDelegate());
    const ConsoleMessagesDelegate* console_delegate =
        scoped_console_delegate.get();
    TestThrottle throttle(request.get(), std::move(scoped_console_delegate));
    MockResourceThrottleDelegate delegate;
    throttle.set_delegate_for_testing(&delegate);
    throttle.SetResponseHeaders(kClearCookiesHeader);

    EXPECT_CALL(throttle, ClearSiteData(_, _, _, _))
        .Times(test_case.expect_success ? 1 : 0);

    bool defer;
    throttle.WillProcessResponse(&defer);

    EXPECT_EQ(defer, test_case.expect_success);
    EXPECT_EQ(console_delegate->messages().size(), 1u);
    EXPECT_EQ(test_case.expect_success ? CONSOLE_MESSAGE_LEVEL_INFO
                                       : CONSOLE_MESSAGE_LEVEL_ERROR,
              console_delegate->messages().front().level);
    if (!test_case.expect_success) {
      EXPECT_EQ(test_case.error_message,
                console_delegate->messages().front().text);
    }
    testing::Mock::VerifyAndClearExpectations(&throttle);
  }
}

TEST_F(ClearSiteDataThrottleTest, DeferAndResume) {
  enum Stage { START, REDIRECT, RESPONSE };

  struct TestCase {
    Stage stage;
    std::string response_headers;
    bool should_defer;
  } kTestCases[] = {
      // The throttle never interferes while the request is starting. Response
      // headers are ignored, because URLRequest is not supposed to have any
      // at this stage in the first place.
      {START, "", false},
      {START, kClearCookiesHeader, false},

      // The throttle does not defer redirects if there are no interesting
      // response headers.
      {REDIRECT, "", false},
      {REDIRECT, "Set-Cookie: abc=123;", false},
      {REDIRECT, "Content-Type: image/png;", false},

      // That includes malformed Clear-Site-Data headers or header values
      // that do not lead to deletion.
      {REDIRECT, "Clear-Site-Data: cookies", false},
      {REDIRECT, "Clear-Site-Data: \"unknown type\"", false},

      // However, redirects are deferred for valid Clear-Site-Data headers.
      {REDIRECT, "Clear-Site-Data: \"cookies\", \"unknown type\"", true},
      {REDIRECT,
       base::StringPrintf("Content-Type: image/png;\n%s", kClearCookiesHeader),
       true},
      {REDIRECT,
       base::StringPrintf("%s\nContent-Type: image/png;", kClearCookiesHeader),
       true},

      // Multiple instances of the header will be parsed correctly.
      {REDIRECT,
       base::StringPrintf("%s\n%s", kClearCookiesHeader, kClearCookiesHeader),
       true},

      // Final response headers are treated the same way as in the case
      // of redirect.
      {REDIRECT, "Set-Cookie: abc=123;", false},
      {REDIRECT, "Clear-Site-Data: cookies", false},
      {REDIRECT, kClearCookiesHeader, true},
  };

  struct TestOrigin {
    const char* origin;
    bool valid;
  } kTestOrigins[] = {
      // The throttle only works on secure origins.
      {"https://secure-origin.com", true},
      {"filesystem:https://secure-origin.com/temporary/", true},

      // That includes localhost.
      {"http://localhost", true},

      // Not on insecure origins.
      {"http://insecure-origin.com", false},
      {"filesystem:http://insecure-origin.com/temporary/", false},

      // Not on unique origins.
      {"data:unique-origin;", false},
  };

  net::TestURLRequestContext context;

  for (const TestOrigin& test_origin : kTestOrigins) {
    for (const TestCase& test_case : kTestCases) {
      SCOPED_TRACE(base::StringPrintf("Origin=%s\nStage=%d\nHeaders:\n%s",
                                      test_origin.origin, test_case.stage,
                                      test_case.response_headers.c_str()));

      std::unique_ptr<net::URLRequest> request(
          context.CreateRequest(GURL(test_origin.origin), net::DEFAULT_PRIORITY,
                                nullptr, TRAFFIC_ANNOTATION_FOR_TESTS));
      TestThrottle throttle(request.get(),
                            std::make_unique<ConsoleMessagesDelegate>());
      throttle.SetResponseHeaders(test_case.response_headers);

      MockResourceThrottleDelegate delegate;
      throttle.set_delegate_for_testing(&delegate);

      // Whether we should defer is always conditional on the origin
      // being valid.
      bool expected_defer = test_case.should_defer && test_origin.valid;

      // If we expect loading to be deferred, then we also expect data to be
      // cleared and the load to eventually resume.
      if (expected_defer) {
        testing::Expectation expectation = EXPECT_CALL(
            throttle,
            ClearSiteData(url::Origin::Create(GURL(test_origin.origin)), _, _,
                          _));
        EXPECT_CALL(delegate, Resume()).After(expectation);
      } else {
        EXPECT_CALL(throttle, ClearSiteData(_, _, _, _)).Times(0);
        EXPECT_CALL(delegate, Resume()).Times(0);
      }

      bool actual_defer = false;

      switch (test_case.stage) {
        case START: {
          throttle.WillStartRequest(&actual_defer);
          break;
        }
        case REDIRECT: {
          net::RedirectInfo redirect_info;
          throttle.WillRedirectRequest(redirect_info, &actual_defer);
          break;
        }
        case RESPONSE: {
          throttle.WillProcessResponse(&actual_defer);
          break;
        }
      }

      EXPECT_EQ(expected_defer, actual_defer);
      testing::Mock::VerifyAndClearExpectations(&delegate);
    }
  }
}

// Verifies that console outputs from various actions on different URLs
// are correctly pretty-printed to the console.
TEST_F(ClearSiteDataThrottleTest, FormattedConsoleOutput) {
  struct TestCase {
    const char* header;
    const char* url;
    const char* output;
  } kTestCases[] = {
      // Successful deletion outputs one line, and in case of cookies, also
      // a disclaimer about omitted data (crbug.com/798760).
      {"\"cookies\"", "https://origin1.com/foo",
       "Clear-Site-Data header on 'https://origin1.com/foo': "
       "Cleared data types: \"cookies\". "
       "Clearing channel IDs and HTTP authentication cache is currently "
       "not supported, as it breaks active network connections.\n"},

      // Another successful deletion.
      {"\"storage\"", "https://origin2.com/foo",
       "Clear-Site-Data header on 'https://origin2.com/foo': "
       "Cleared data types: \"storage\".\n"},

      // Redirect to the same URL. Unsuccessful deletion outputs two lines.
      {"\"foo\"", "https://origin2.com/foo",
       "Clear-Site-Data header on 'https://origin2.com/foo': "
       "Unrecognized type: \"foo\".\n"
       "Clear-Site-Data header on 'https://origin2.com/foo': "
       "No recognized types specified.\n"},

      // Redirect to another URL. Another unsuccessful deletion.
      {"\"some text\"", "https://origin3.com/bar",
       "Clear-Site-Data header on 'https://origin3.com/bar': "
       "Unrecognized type: \"some text\".\n"
       "Clear-Site-Data header on 'https://origin3.com/bar': "
       "No recognized types specified.\n"},

      // Yet another on the same URL.
      {"\"passwords\"", "https://origin3.com/bar",
       "Clear-Site-Data header on 'https://origin3.com/bar': "
       "Unrecognized type: \"passwords\".\n"
       "Clear-Site-Data header on 'https://origin3.com/bar': "
       "No recognized types specified.\n"},

      // Successful deletion on the same URL.
      {"\"cache\"", "https://origin3.com/bar",
       "Clear-Site-Data header on 'https://origin3.com/bar': "
       "Cleared data types: \"cache\".\n"},

      // Redirect to the original URL.
      // Successful deletion outputs one line.
      {"", "https://origin1.com/foo",
       "Clear-Site-Data header on 'https://origin1.com/foo': "
       "No recognized types specified.\n"}};

  bool kThrottleTypeIsNavigation[] = {true, false};

  for (bool navigation : kThrottleTypeIsNavigation) {
    SCOPED_TRACE(navigation ? "Navigation test." : "Subresource test.");

    net::TestURLRequestContext context;
    std::unique_ptr<net::URLRequest> request(
        context.CreateRequest(GURL(kTestCases[0].url), net::DEFAULT_PRIORITY,
                              nullptr, TRAFFIC_ANNOTATION_FOR_TESTS));
    ResourceRequestInfo::AllocateForTesting(
        request.get(),
        navigation ? RESOURCE_TYPE_SUB_FRAME : RESOURCE_TYPE_IMAGE, nullptr, 0,
        0, 0, false, true, true, false, nullptr);

    std::string output_buffer;
    std::unique_ptr<RedirectableTestThrottle> throttle =
        std::make_unique<RedirectableTestThrottle>(
            request.get(),
            std::make_unique<StringConsoleMessagesDelegate>(&output_buffer));

    MockResourceThrottleDelegate delegate;
    throttle->set_delegate_for_testing(&delegate);

    std::string last_seen_console_output;

    // Simulate redirecting the throttle through the above origins with the
    // corresponding response headers.
    bool defer;
    throttle->WillStartRequest(&defer);

    for (size_t i = 0; i < arraysize(kTestCases); i++) {
      throttle->SetResponseHeaders(std::string(kClearSiteDataHeaderPrefix) +
                                   kTestCases[i].header);

      // TODO(msramek): There is probably a better way to do this inside
      // URLRequest.
      throttle->SetCurrentURLForTesting(GURL(kTestCases[i].url));

      net::RedirectInfo redirect_info;
      if (i < arraysize(kTestCases) - 1)
        throttle->WillRedirectRequest(redirect_info, &defer);
      else
        throttle->WillProcessResponse(&defer);

      // Wait for any messages to be output.
      WaitForUIThread();

      // For navigations, the console should be still empty. For subresource
      // requests, messages should be added progressively.
      if (navigation) {
        EXPECT_TRUE(output_buffer.empty());
      } else {
        EXPECT_EQ(last_seen_console_output + kTestCases[i].output,
                  output_buffer);
      }

      last_seen_console_output = output_buffer;
    }

    throttle.reset();
    WaitForUIThread();

    // At the end, the console must contain all messages regardless of whether
    // it was a navigation or a subresource request.
    std::string expected_output;
    for (struct TestCase& test_case : kTestCases)
      expected_output += test_case.output;
    EXPECT_EQ(expected_output, output_buffer);
  }
}

}  // namespace content
