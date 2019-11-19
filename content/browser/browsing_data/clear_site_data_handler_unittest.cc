// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/clear_site_data_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_thread.h"
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

using ConsoleMessagesDelegate = ClearSiteDataHandler::ConsoleMessagesDelegate;
using Message = ClearSiteDataHandler::ConsoleMessagesDelegate::Message;

namespace {

const char kClearCookiesHeader[] = "\"cookies\"";

BrowserContext* FakeBrowserContextGetter() {
  return nullptr;
}

WebContents* FakeWebContentsGetter() {
  return nullptr;
}

// A slightly modified ClearSiteDataHandler for testing with dummy clearing
// functionality.
class TestHandler : public ClearSiteDataHandler {
 public:
  TestHandler(base::RepeatingCallback<BrowserContext*()> browser_context_getter,
              base::RepeatingCallback<WebContents*()> web_contents_getter,
              const GURL& url,
              const std::string& header_value,
              int load_flags,
              base::OnceClosure callback,
              std::unique_ptr<ConsoleMessagesDelegate> delegate)
      : ClearSiteDataHandler(browser_context_getter,
                             web_contents_getter,
                             url,
                             header_value,
                             load_flags,
                             std::move(callback),
                             std::move(delegate)) {}
  ~TestHandler() override = default;

  // |HandleHeaderAndOutputConsoleMessages()| is protected and not visible in
  // test cases.
  bool DoHandleHeader() { return HandleHeaderAndOutputConsoleMessages(); }

  MOCK_METHOD4(ClearSiteData,
               void(const url::Origin& origin,
                    bool clear_cookies,
                    bool clear_storage,
                    bool clear_cache));

 protected:
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
};

// A ConsoleDelegate that copies message to a vector |message_buffer| owned by
// the caller instead of outputs to the console.
// We need this override because otherwise messages are outputted as soon as
// request finished, and we don't have a chance to check them.
class VectorConsoleMessagesDelegate : public ConsoleMessagesDelegate {
 public:
  VectorConsoleMessagesDelegate(std::vector<Message>* message_buffer)
      : message_buffer_(message_buffer) {}
  ~VectorConsoleMessagesDelegate() override = default;

  void OutputMessages(const base::RepeatingCallback<WebContents*()>&
                          web_contents_getter) override {
    *message_buffer_ = messages();
  }

 private:
  std::vector<Message>* message_buffer_;
};

// A ConsoleDelegate that outputs messages to a string |output_buffer| owned
// by the caller instead of to the console (losing the level information).
class StringConsoleMessagesDelegate : public ConsoleMessagesDelegate {
 public:
  StringConsoleMessagesDelegate(std::string* output_buffer) {
    SetOutputFormattedMessageFunctionForTesting(base::BindRepeating(
        &StringConsoleMessagesDelegate::OutputFormattedMessage,
        base::Unretained(output_buffer)));
  }

  ~StringConsoleMessagesDelegate() override {}

 private:
  static void OutputFormattedMessage(std::string* output_buffer,
                                     WebContents* web_contents,
                                     blink::mojom::ConsoleMessageLevel level,
                                     const std::string& formatted_text) {
    *output_buffer += formatted_text + "\n";
  }
};

}  // namespace

class ClearSiteDataHandlerTest : public testing::Test {
 public:
  ClearSiteDataHandlerTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

 private:
  BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ClearSiteDataHandlerTest);
};

TEST_F(ClearSiteDataHandlerTest, ParseHeaderAndExecuteClearingTask) {
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

      EXPECT_TRUE(ClearSiteDataHandler::ParseHeaderForTesting(
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
      TestHandler handler(base::BindRepeating(&FakeBrowserContextGetter),
                          base::BindRepeating(&FakeWebContentsGetter),
                          request->url(), test_case.header,
                          request->load_flags(), base::DoNothing(),
                          std::make_unique<ConsoleMessagesDelegate>());

      EXPECT_CALL(handler,
                  ClearSiteData(url::Origin::Create(url), test_case.cookies,
                                test_case.storage, test_case.cache));
      bool defer = handler.DoHandleHeader();
      EXPECT_TRUE(defer);

      testing::Mock::VerifyAndClearExpectations(&handler);
    }
  }
}

TEST_F(ClearSiteDataHandlerTest, InvalidHeader) {
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

    EXPECT_FALSE(ClearSiteDataHandler::ParseHeaderForTesting(
        test_case.header, &actual_cookies, &actual_storage, &actual_cache,
        &console_delegate, GURL()));

    std::string multiline_message;
    for (const auto& message : console_delegate.messages()) {
      EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError, message.level);
      multiline_message += message.text + "\n";
    }

    EXPECT_EQ(test_case.console_message, multiline_message);
  }
}

TEST_F(ClearSiteDataHandlerTest, ClearCookieSuccess) {
  net::TestURLRequestContext context;
  std::unique_ptr<net::URLRequest> request(
      context.CreateRequest(GURL("https://example.com"), net::DEFAULT_PRIORITY,
                            nullptr, TRAFFIC_ANNOTATION_FOR_TESTS));
  std::vector<Message> message_buffer;
  TestHandler handler(
      base::BindRepeating(&FakeBrowserContextGetter),
      base::BindRepeating(&FakeWebContentsGetter), request->url(),
      kClearCookiesHeader, request->load_flags(), base::DoNothing(),
      std::make_unique<VectorConsoleMessagesDelegate>(&message_buffer));

  EXPECT_CALL(handler, ClearSiteData(_, _, _, _));
  bool defer = handler.DoHandleHeader();
  EXPECT_TRUE(defer);
  EXPECT_EQ(1u, message_buffer.size());
  EXPECT_EQ(
      "Cleared data types: \"cookies\". "
      "Clearing channel IDs and HTTP authentication cache is currently "
      "not supported, as it breaks active network connections.",
      message_buffer.front().text);
  EXPECT_EQ(message_buffer.front().level,
            blink::mojom::ConsoleMessageLevel::kInfo);
  testing::Mock::VerifyAndClearExpectations(&handler);
}

TEST_F(ClearSiteDataHandlerTest, LoadDoNotSaveCookies) {
  net::TestURLRequestContext context;
  std::unique_ptr<net::URLRequest> request(
      context.CreateRequest(GURL("https://example.com"), net::DEFAULT_PRIORITY,
                            nullptr, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES);
  std::vector<Message> message_buffer;
  TestHandler handler(
      base::BindRepeating(&FakeBrowserContextGetter),
      base::BindRepeating(&FakeWebContentsGetter), request->url(),
      kClearCookiesHeader, request->load_flags(), base::DoNothing(),
      std::make_unique<VectorConsoleMessagesDelegate>(&message_buffer));

  EXPECT_CALL(handler, ClearSiteData(_, _, _, _)).Times(0);
  bool defer = handler.DoHandleHeader();
  EXPECT_FALSE(defer);
  EXPECT_EQ(1u, message_buffer.size());
  EXPECT_EQ(
      "The request's credentials mode prohibits modifying cookies "
      "and other local data.",
      message_buffer.front().text);
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            message_buffer.front().level);
  testing::Mock::VerifyAndClearExpectations(&handler);
}

TEST_F(ClearSiteDataHandlerTest, InvalidOrigin) {
  struct TestCase {
    const char* origin;
    bool expect_success;
    std::string error_message;  // Tested only if |expect_success| = false.
  } kTestCases[] = {
      // The handler only works on secure origins.
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
    std::vector<Message> message_buffer;
    TestHandler handler(
        base::BindRepeating(&FakeBrowserContextGetter),
        base::BindRepeating(&FakeWebContentsGetter), request->url(),
        kClearCookiesHeader, request->load_flags(), base::DoNothing(),
        std::make_unique<VectorConsoleMessagesDelegate>(&message_buffer));

    EXPECT_CALL(handler, ClearSiteData(_, _, _, _))
        .Times(test_case.expect_success ? 1 : 0);

    bool defer = handler.DoHandleHeader();

    EXPECT_EQ(defer, test_case.expect_success);
    EXPECT_EQ(message_buffer.size(), 1u);
    EXPECT_EQ(test_case.expect_success
                  ? blink::mojom::ConsoleMessageLevel::kInfo
                  : blink::mojom::ConsoleMessageLevel::kError,
              message_buffer.front().level);
    if (!test_case.expect_success) {
      EXPECT_EQ(test_case.error_message, message_buffer.front().text);
    }
    testing::Mock::VerifyAndClearExpectations(&handler);
  }
}

// Verifies that console outputs from various actions on different URLs
// are correctly pretty-printed to the console.
TEST_F(ClearSiteDataHandlerTest, FormattedConsoleOutput) {
  struct TestCase {
    const char* header;
    const char* url;
    const char* output;
  } kTestCases[] = {
      // Successful deletion outputs one line, and in case of cookies, also
      // a disclaimer about omitted data (https://crbug.com/798760).
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

  // TODO(crbug.com/876931): Delay output until next frame for navigations.
  bool kHandlerTypeIsNavigation[] = {false};

  for (bool navigation : kHandlerTypeIsNavigation) {
    SCOPED_TRACE(navigation ? "Navigation test." : "Subresource test.");

    net::TestURLRequestContext context;
    std::unique_ptr<net::URLRequest> request(
        context.CreateRequest(GURL(kTestCases[0].url), net::DEFAULT_PRIORITY,
                              nullptr, TRAFFIC_ANNOTATION_FOR_TESTS));

    std::string output_buffer;
    std::string last_seen_console_output;

    // |NetworkServiceClient| creates a new |ClearSiteDataHandler| for each
    // navigation, redirect, or subresource header responses.
    for (size_t i = 0; i < base::size(kTestCases); i++) {
      TestHandler handler(
          base::BindRepeating(&FakeBrowserContextGetter),
          base::BindRepeating(&FakeWebContentsGetter), GURL(kTestCases[i].url),
          kTestCases[i].header, request->load_flags(), base::DoNothing(),
          std::make_unique<StringConsoleMessagesDelegate>(&output_buffer));
      handler.DoHandleHeader();

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

    // At the end, the console must contain all messages regardless of whether
    // it was a navigation or a subresource request.
    std::string expected_output;
    for (struct TestCase& test_case : kTestCases)
      expected_output += test_case.output;
    EXPECT_EQ(expected_output, output_buffer);
  }
}

}  // namespace content
