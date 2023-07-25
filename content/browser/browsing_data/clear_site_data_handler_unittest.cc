// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/clear_site_data_handler.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/features.h"
#include "net/base/load_flags.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

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
  TestHandler(
      base::RepeatingCallback<BrowserContext*()> browser_context_getter,
      base::RepeatingCallback<WebContents*()> web_contents_getter,
      const GURL& url,
      const std::string& header_value,
      int load_flags,
      const absl::optional<net::CookiePartitionKey>& cookie_partition_key,
      const absl::optional<blink::StorageKey>& storage_key,
      bool partitioned_state_allowed_only,
      base::OnceClosure callback,
      std::unique_ptr<ConsoleMessagesDelegate> delegate)
      : ClearSiteDataHandler(browser_context_getter,
                             web_contents_getter,
                             url,
                             header_value,
                             load_flags,
                             cookie_partition_key,
                             storage_key,
                             partitioned_state_allowed_only,
                             std::move(callback),
                             std::move(delegate)) {}
  ~TestHandler() override = default;

  // |HandleHeaderAndOutputConsoleMessages()| is protected and not visible in
  // test cases.
  bool DoHandleHeader() { return HandleHeaderAndOutputConsoleMessages(); }

  MOCK_METHOD7(
      ClearSiteData,
      void(const url::Origin& origin,
           const ClearSiteDataTypeSet clear_site_data_types,
           const std::set<std::string>& storage_buckets_to_remove,
           bool avoid_closing_connections,
           const absl::optional<net::CookiePartitionKey>& cookie_partition_key,
           const absl::optional<blink::StorageKey>& storage_key,
           bool partitioned_state_allowed_only));

 protected:
  void ExecuteClearingTask(
      const url::Origin& origin,
      const ClearSiteDataTypeSet clear_site_data_types,
      const std::set<std::string>& storage_buckets_to_remove,
      base::OnceClosure callback) override {
    ClearSiteData(origin, clear_site_data_types, storage_buckets_to_remove,
                  false, CookiePartitionKeyForTesting(), StorageKeyForTesting(),
                  PartitionedStateOnlyForTesting());

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
    *message_buffer_ = GetMessagesForTesting();
  }

 private:
  raw_ptr<std::vector<Message>> message_buffer_;
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

class ClearSiteDataHandlerTest
    : public testing::Test,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  ClearSiteDataHandlerTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

  ClearSiteDataHandlerTest(const ClearSiteDataHandlerTest&) = delete;
  ClearSiteDataHandlerTest& operator=(const ClearSiteDataHandlerTest&) = delete;

  bool IsClientHintsSupportEnabled() { return std::get<0>(GetParam()); }

  bool IsWildcardSupportEnabled() { return std::get<1>(GetParam()); }

  bool IsStorageBucketSupportEnabled() { return std::get<2>(GetParam()); }

 private:
  BrowserTaskEnvironment task_environment_;
};

INSTANTIATE_TEST_SUITE_P(
    ParseHeaderAndExecuteClearingTaskWithFeaturesEnabledTestSuite,
    ClearSiteDataHandlerTest,
    testing::Combine(testing::Bool(), testing::Bool(), testing::Bool()));

TEST_P(ClearSiteDataHandlerTest, ParseHeaderAndExecuteClearingTask) {
  std::vector<base::test::FeatureRef> features_to_enable;
  std::vector<base::test::FeatureRef> features_to_disable;
  if (IsClientHintsSupportEnabled()) {
    features_to_enable.push_back(
        network::features::kClearSiteDataClientHintsSupport);
  } else {
    features_to_disable.push_back(
        network::features::kClearSiteDataClientHintsSupport);
  }
  if (IsWildcardSupportEnabled()) {
    features_to_enable.push_back(net::features::kClearSiteDataWildcardSupport);
  } else {
    features_to_disable.push_back(net::features::kClearSiteDataWildcardSupport);
  }
  if (IsStorageBucketSupportEnabled()) {
    features_to_enable.push_back(blink::features::kStorageBuckets);
  } else {
    features_to_disable.push_back(blink::features::kStorageBuckets);
  }
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(features_to_enable, features_to_disable);

  struct TestCase {
    const char* header;
    bool cookies;
    bool storage;
    bool cache;
    bool client_hints;
    std::set<std::string> storage_buckets_to_remove;
  };

  std::set<std::string> storage_buckets_test_case_expectation = {"drafts",
                                                                 "inbox"};

  std::vector<TestCase> test_cases = {
      // One data type.
      {"\"cookies\"", true, false, false, false},
      {"\"storage\"", false, true, false, false},
      {"\"cache\"", false, false, true, false},
      {"\"clientHints\"", false, false, false, IsClientHintsSupportEnabled()},

      // Two data types.
      {"\"cookies\", \"storage\"", true, true, false, false},
      {"\"cookies\", \"cache\"", true, false, true, false},
      {"\"storage\", \"cache\"", false, true, true, false},
      {"\"cookies\", \"clientHints\"", true, false, false,
       IsClientHintsSupportEnabled()},
      {"\"storage\", \"clientHints\"", false, true, false,
       IsClientHintsSupportEnabled()},
      {"\"cache\", \"clientHints\"", false, false, true,
       IsClientHintsSupportEnabled()},

      // Three data types.
      {"\"cookies\", \"storage\", \"cache\"", true, true, true, false},
      {"\"clientHints\", \"storage\", \"cache\"", false, true, true,
       IsClientHintsSupportEnabled()},
      {"\"cookies\", \"clientHints\", \"cache\"", true, false, true,
       IsClientHintsSupportEnabled()},
      {"\"cookies\", \"storage\", \"clientHints\"", true, true, false,
       IsClientHintsSupportEnabled()},

      // Four data types.
      {"\"cookies\", \"storage\", \"cache\", \"clientHints\"", true, true, true,
       IsClientHintsSupportEnabled()},

      // Wildcard.
      {"\"*\"", IsWildcardSupportEnabled(), IsWildcardSupportEnabled(),
       IsWildcardSupportEnabled(),
       IsWildcardSupportEnabled() && IsClientHintsSupportEnabled()},
      {"\"*\", \"storage\"", IsWildcardSupportEnabled(), true,
       IsWildcardSupportEnabled(),
       IsWildcardSupportEnabled() && IsClientHintsSupportEnabled()},
      {"\"cookies\", \"*\", \"storage\"", true, true,
       IsWildcardSupportEnabled(),
       IsWildcardSupportEnabled() && IsClientHintsSupportEnabled()},
      {"\"*\", \"cookies\", \"*\"", true, IsWildcardSupportEnabled(),
       IsWildcardSupportEnabled(),
       IsWildcardSupportEnabled() && IsClientHintsSupportEnabled()},
      {"\"*\", \"clientHints\"", IsWildcardSupportEnabled(),
       IsWildcardSupportEnabled(), IsWildcardSupportEnabled(),
       IsClientHintsSupportEnabled()},

      // Different formatting.
      {"\"cookies\"", true, false, false, false},

      // Duplicates.
      {"\"cookies\", \"cookies\"", true, false, false, false},

      // Other JSON-formatted items in the list.
      {"\"storage\", { \"other_params\": {} }", false, true, false, false},

      // Unknown types are ignored, but we still proceed with the deletion for
      // those that we recognize.
      {"\"cache\", \"foo\"", false, false, true, false},

      // Storage Buckets
      {"\"storage\", \"storage:drafts\"", false, true, false, false},
      {"\"*\", \"storage:drafts\", \"storage:inbox\"",
       IsWildcardSupportEnabled(), IsWildcardSupportEnabled(),
       IsWildcardSupportEnabled(),
       IsWildcardSupportEnabled() && IsClientHintsSupportEnabled(),
       (!IsWildcardSupportEnabled() && IsStorageBucketSupportEnabled())
           ? storage_buckets_test_case_expectation
           : std::set<std::string>()},
      {"\"cookies\", \"storage:drafts", true, false, false,
       false},  // Invalid header, should end with '"'
      {"\"cookies\", \"storage:invalid_name$#$\"", true, false, false,
       false},  // Invalid bucket name

      {"\"cookies\", \"storage:drafts\", \"storage:inbox\"", true, false, false,
       false,
       IsStorageBucketSupportEnabled() ? storage_buckets_test_case_expectation
                                       : std::set<std::string>()},
  };

  if (!base::FeatureList::IsEnabled(blink::features::kStorageBuckets)) {
    storage_buckets_test_case_expectation.clear();
  }

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.header);

    // Test that ParseHeader works correctly.
    ClearSiteDataTypeSet clear_site_data_types;
    std::set<std::string> storage_buckets_to_remove = {};

    GURL url("https://example.com");
    ConsoleMessagesDelegate console_delegate;

    base::HistogramTester histogram_tester;
    bool success = ClearSiteDataHandler::ParseHeaderForTesting(
        test_case.header, &clear_site_data_types, &storage_buckets_to_remove,
        &console_delegate, url);
    if (!test_case.cookies && !test_case.storage && !test_case.cache &&
        !test_case.client_hints &&
        test_case.storage_buckets_to_remove.empty()) {
      EXPECT_FALSE(success);
      continue;
    }
    EXPECT_TRUE(success);

    EXPECT_EQ(test_case.cookies,
              clear_site_data_types.Has(ClearSiteDataType::kCookies));
    EXPECT_EQ(test_case.storage,
              clear_site_data_types.Has(ClearSiteDataType::kStorage));
    EXPECT_EQ(test_case.cache,
              clear_site_data_types.Has(ClearSiteDataType::kCache));
    EXPECT_EQ(test_case.client_hints,
              clear_site_data_types.Has(ClearSiteDataType::kClientHints));
    EXPECT_EQ(test_case.storage_buckets_to_remove, storage_buckets_to_remove);

    // Count the number of bits in a mask that are 1.
    auto count_ones_in_mask = [](int mask) {
      int count = 0;
      for (size_t i = 0; i < sizeof(mask) * 8; ++i) {
        count += (mask >> i) & 1;
      }
      return count;
    };
    histogram_tester.ExpectTotalCount("Storage.ClearSiteDataHeader.Parameters",
                                      1);
    int sample =
        histogram_tester.GetTotalSum("Storage.ClearSiteDataHeader.Parameters");
    // There should be one bit set to one for each data type seen.
    EXPECT_EQ(count_ones_in_mask(sample),
              static_cast<int>(test_case.cookies) +
                  static_cast<int>(test_case.storage) +
                  static_cast<int>(test_case.cache) +
                  static_cast<int>(!storage_buckets_to_remove.empty()) +
                  static_cast<int>(test_case.client_hints));

    // Test that a call with the above parameters actually reaches
    // ExecuteClearingTask().
    auto context = net::CreateTestURLRequestContextBuilder()->Build();
    std::unique_ptr<net::URLRequest> request(context->CreateRequest(
        url, net::DEFAULT_PRIORITY, nullptr, TRAFFIC_ANNOTATION_FOR_TESTS));
    TestHandler handler(
        base::BindRepeating(&FakeBrowserContextGetter),
        base::BindRepeating(&FakeWebContentsGetter), request->url(),
        test_case.header, request->load_flags(),
        /*cookie_partition_key=*/absl::nullopt, /*storage_key=*/absl::nullopt,
        /*partitioned_state_allowed_only=*/false, base::DoNothing(),
        std::make_unique<ConsoleMessagesDelegate>());

    EXPECT_CALL(handler,
                ClearSiteData(url::Origin::Create(url), clear_site_data_types,
                              test_case.storage_buckets_to_remove, _, _, _, _));
    bool defer = handler.DoHandleHeader();
    EXPECT_TRUE(defer);

    testing::Mock::VerifyAndClearExpectations(&handler);
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

    ClearSiteDataTypeSet clear_site_data_types;
    std::set<std::string> actual_storage_buckets_to_remove;

    ConsoleMessagesDelegate console_delegate;

    EXPECT_FALSE(ClearSiteDataHandler::ParseHeaderForTesting(
        test_case.header, &clear_site_data_types,
        &actual_storage_buckets_to_remove, &console_delegate, GURL()));

    std::string multiline_message;
    for (const auto& message : console_delegate.GetMessagesForTesting()) {
      EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError, message.level);
      multiline_message += message.text + "\n";
    }

    EXPECT_EQ(test_case.console_message, multiline_message);
  }
}

TEST_F(ClearSiteDataHandlerTest, ClearCookieSuccess) {
  auto context = net::CreateTestURLRequestContextBuilder()->Build();
  std::unique_ptr<net::URLRequest> request(
      context->CreateRequest(GURL("https://example.com"), net::DEFAULT_PRIORITY,
                             nullptr, TRAFFIC_ANNOTATION_FOR_TESTS));
  std::vector<Message> message_buffer;
  TestHandler handler(
      base::BindRepeating(&FakeBrowserContextGetter),
      base::BindRepeating(&FakeWebContentsGetter), request->url(),
      kClearCookiesHeader, request->load_flags(),
      /*cookie_partition_key=*/absl::nullopt, /*storage_key=*/absl::nullopt,
      /*partitioned_state_allowed_only=*/false, base::DoNothing(),
      std::make_unique<VectorConsoleMessagesDelegate>(&message_buffer));

  EXPECT_CALL(handler, ClearSiteData(_, _, _, _, _, _, _));
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
  auto context = net::CreateTestURLRequestContextBuilder()->Build();
  std::unique_ptr<net::URLRequest> request(
      context->CreateRequest(GURL("https://example.com"), net::DEFAULT_PRIORITY,
                             nullptr, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES);
  std::vector<Message> message_buffer;
  TestHandler handler(
      base::BindRepeating(&FakeBrowserContextGetter),
      base::BindRepeating(&FakeWebContentsGetter), request->url(),
      kClearCookiesHeader, request->load_flags(),
      /*cookie_partition_key=*/absl::nullopt, /*storage_key=*/absl::nullopt,
      /*partitioned_state_allowed_only=*/false, base::DoNothing(),
      std::make_unique<VectorConsoleMessagesDelegate>(&message_buffer));

  EXPECT_CALL(handler, ClearSiteData(_, _, _, _, _, _, _)).Times(0);
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

  auto context = net::CreateTestURLRequestContextBuilder()->Build();

  for (const TestCase& test_case : kTestCases) {
    std::unique_ptr<net::URLRequest> request(
        context->CreateRequest(GURL(test_case.origin), net::DEFAULT_PRIORITY,
                               nullptr, TRAFFIC_ANNOTATION_FOR_TESTS));
    std::vector<Message> message_buffer;
    TestHandler handler(
        base::BindRepeating(&FakeBrowserContextGetter),
        base::BindRepeating(&FakeWebContentsGetter), request->url(),
        kClearCookiesHeader, request->load_flags(),
        /*cookie_partition_key=*/absl::nullopt, /*storage_key=*/absl::nullopt,
        /*partitioned_state_allowed_only=*/false, base::DoNothing(),
        std::make_unique<VectorConsoleMessagesDelegate>(&message_buffer));

    EXPECT_CALL(handler, ClearSiteData(_, _, _, _, _, _, _))
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
    bool wildcard;
    bool client_hints;
  } kTestCases[] = {
      // Successful deletion outputs one line, and in case of cookies, also
      // a disclaimer about omitted data (https://crbug.com/798760).
      {"\"cookies\"", "https://origin1.com/foo",
       "Clear-Site-Data header on 'https://origin1.com/foo': "
       "Cleared data types: \"cookies\". "
       "Clearing channel IDs and HTTP authentication cache is currently "
       "not supported, as it breaks active network connections.\n",
       false, false},

      // Another successful deletion.
      {"\"storage\"", "https://origin2.com/foo",
       "Clear-Site-Data header on 'https://origin2.com/foo': "
       "Cleared data types: \"storage\".\n",
       false, false},

      // Redirect to the same URL. Unsuccessful deletion outputs two lines.
      {"\"foo\"", "https://origin2.com/foo",
       "Clear-Site-Data header on 'https://origin2.com/foo': "
       "Unrecognized type: \"foo\".\n"
       "Clear-Site-Data header on 'https://origin2.com/foo': "
       "No recognized types specified.\n",
       false, false},

      // Redirect to another URL. Another unsuccessful deletion.
      {"\"some text\"", "https://origin3.com/bar",
       "Clear-Site-Data header on 'https://origin3.com/bar': "
       "Unrecognized type: \"some text\".\n"
       "Clear-Site-Data header on 'https://origin3.com/bar': "
       "No recognized types specified.\n",
       false, false},

      // Yet another on the same URL.
      {"\"passwords\"", "https://origin3.com/bar",
       "Clear-Site-Data header on 'https://origin3.com/bar': "
       "Unrecognized type: \"passwords\".\n"
       "Clear-Site-Data header on 'https://origin3.com/bar': "
       "No recognized types specified.\n",
       false, false},

      // Successful deletion on the same URL.
      {"\"cache\"", "https://origin3.com/bar",
       "Clear-Site-Data header on 'https://origin3.com/bar': "
       "Cleared data types: \"cache\".\n",
       false, false},

      // Failed deletion as client hint support is off.
      {"\"clientHints\"", "https://origin3.com/bar",
       "Clear-Site-Data header on 'https://origin3.com/bar': Unrecognized "
       "type: \"clientHints\".\nClear-Site-Data header on "
       "'https://origin3.com/bar': No recognized types specified.\n",
       false, false},

      // Successful deletion as client hint support is on.
      {"\"clientHints\"", "https://origin3.com/bar",
       "Clear-Site-Data header on 'https://origin3.com/bar': "
       "Cleared data types: \"clientHints\".\n",
       false, true},

      // Failed deletion as experimental types are disabled here.
      {"\"*\"", "https://origin3.com/bar",
       "Clear-Site-Data header on 'https://origin3.com/bar': Unrecognized "
       "type: \"*\".\nClear-Site-Data header on 'https://origin3.com/bar': No "
       "recognized types specified.\n",
       false, false},
      {"\"*\"", "https://origin3.com/bar",
       "Clear-Site-Data header on 'https://origin3.com/bar': Unrecognized "
       "type: \"*\".\nClear-Site-Data header on 'https://origin3.com/bar': No "
       "recognized types specified.\n",
       false, true},

      // Successful deletion with experimental types on.
      {"\"*\"", "https://origin3.com/bar",
       "Clear-Site-Data header on 'https://origin3.com/bar': Cleared data "
       "types: \"cookies\", \"storage\", \"cache\". Clearing channel IDs and "
       "HTTP authentication cache is currently not supported, as it breaks "
       "active network connections.\n",
       true, false},

      // Successful deletion with experimental types and client hint support on.
      {"\"*\"", "https://origin3.com/bar",
       "Clear-Site-Data header on 'https://origin3.com/bar': Cleared data "
       "types: \"cookies\", \"storage\", \"cache\", \"clientHints\". Clearing "
       "channel IDs and HTTP authentication cache is currently not supported, "
       "as it breaks active network connections.\n",
       true, true},

      // Redirect to the original URL.
      // Successful deletion outputs one line.
      {"", "https://origin1.com/foo",
       "Clear-Site-Data header on 'https://origin1.com/foo': "
       "No recognized types specified.\n",
       false, false},
  };

  // TODO(crbug.com/876931): Delay output until next frame for navigations.
  bool kHandlerTypeIsNavigation[] = {false};

  for (bool navigation : kHandlerTypeIsNavigation) {
    SCOPED_TRACE(navigation ? "Navigation test." : "Subresource test.");

    auto context = net::CreateTestURLRequestContextBuilder()->Build();
    std::unique_ptr<net::URLRequest> request(
        context->CreateRequest(GURL(kTestCases[0].url), net::DEFAULT_PRIORITY,
                               nullptr, TRAFFIC_ANNOTATION_FOR_TESTS));

    std::string output_buffer;
    std::string last_seen_console_output;

    // |NetworkServiceClient| creates a new |ClearSiteDataHandler| for each
    // navigation, redirect, or subresource header responses.
    for (const auto& test : kTestCases) {
      std::vector<base::test::FeatureRef> enabled_features;
      std::vector<base::test::FeatureRef> disabled_features;
      if (test.wildcard) {
        enabled_features.push_back(
            net::features::kClearSiteDataWildcardSupport);
      } else {
        disabled_features.push_back(
            net::features::kClearSiteDataWildcardSupport);
      }
      if (test.client_hints) {
        enabled_features.push_back(
            network::features::kClearSiteDataClientHintsSupport);
      } else {
        disabled_features.push_back(
            network::features::kClearSiteDataClientHintsSupport);
      }
      base::test::ScopedFeatureList scoped_feature_list;
      scoped_feature_list.InitWithFeatures(enabled_features, disabled_features);
      TestHandler handler(
          base::BindRepeating(&FakeBrowserContextGetter),
          base::BindRepeating(&FakeWebContentsGetter), GURL(test.url),
          test.header, request->load_flags(),
          /*cookie_partition_key=*/absl::nullopt, /*storage_key=*/absl::nullopt,
          /*partitioned_state_allowed_only=*/false, base::DoNothing(),
          std::make_unique<StringConsoleMessagesDelegate>(&output_buffer));
      handler.DoHandleHeader();

      // For navigations, the console should be still empty. For subresource
      // requests, messages should be added progressively.
      if (navigation) {
        EXPECT_TRUE(output_buffer.empty());
      } else {
        EXPECT_EQ(last_seen_console_output + test.output, output_buffer);
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

TEST_F(ClearSiteDataHandlerTest, CookiePartitionKey) {
  absl::optional<net::CookiePartitionKey> cookie_partition_keys[] = {
      absl::nullopt,
      net::CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com")),
  };
  const GURL kTestURL("https://www.bar.com");

  for (const auto& cookie_partition_key : cookie_partition_keys) {
    auto context = net::CreateTestURLRequestContextBuilder()->Build();
    std::unique_ptr<net::URLRequest> request(
        context->CreateRequest(kTestURL, net::DEFAULT_PRIORITY, nullptr,
                               TRAFFIC_ANNOTATION_FOR_TESTS));
    std::string output_buffer;
    TestHandler handler(
        base::BindRepeating(&FakeBrowserContextGetter),
        base::BindRepeating(&FakeWebContentsGetter), kTestURL, "\"cookies\"",
        request->load_flags(), cookie_partition_key,
        /*storage_key=*/absl::nullopt,
        /*partitioned_state_allowed_only=*/false, base::DoNothing(),
        std::make_unique<StringConsoleMessagesDelegate>(&output_buffer));
    EXPECT_CALL(handler, ClearSiteData(_, _, _, _, cookie_partition_key, _, _));
    EXPECT_TRUE(handler.DoHandleHeader());
  }
}

TEST_F(ClearSiteDataHandlerTest, StorageKey) {
  absl::optional<blink::StorageKey> storage_keys[] = {
      absl::nullopt,
      blink::StorageKey::CreateFromStringForTesting("https://example.com")};
  const GURL kTestURL("https://example.com");

  for (const auto& storage_key : storage_keys) {
    auto context = net::CreateTestURLRequestContextBuilder()->Build();
    std::unique_ptr<net::URLRequest> request(
        context->CreateRequest(kTestURL, net::DEFAULT_PRIORITY, nullptr,
                               TRAFFIC_ANNOTATION_FOR_TESTS));
    std::string output_buffer;
    TestHandler handler(
        base::BindRepeating(&FakeBrowserContextGetter),
        base::BindRepeating(&FakeWebContentsGetter), kTestURL, "\"storage\"",
        request->load_flags(), /*cookie_partition_key=*/absl::nullopt,
        storage_key,
        /*partitioned_state_allowed_only=*/false, base::DoNothing(),
        std::make_unique<StringConsoleMessagesDelegate>(&output_buffer));
    EXPECT_CALL(handler, ClearSiteData(_, _, _, _, _, storage_key, _));
    EXPECT_TRUE(handler.DoHandleHeader());
  }
}

TEST_F(ClearSiteDataHandlerTest, ThirdPartyCookieBlockingEnabled) {
  bool test_cases[] = {true, false};
  const GURL kTestURL("https://example.com");

  for (const auto partitioned_state_allowed_only : test_cases) {
    auto context = net::CreateTestURLRequestContextBuilder()->Build();
    std::unique_ptr<net::URLRequest> request(
        context->CreateRequest(kTestURL, net::DEFAULT_PRIORITY, nullptr,
                               TRAFFIC_ANNOTATION_FOR_TESTS));
    std::string output_buffer;
    TestHandler handler(
        base::BindRepeating(&FakeBrowserContextGetter),
        base::BindRepeating(&FakeWebContentsGetter), kTestURL, "\"storage\"",
        request->load_flags(), /*cookie_partition_key=*/absl::nullopt,
        /*storage_key=*/absl::nullopt, partitioned_state_allowed_only,
        base::DoNothing(),
        std::make_unique<StringConsoleMessagesDelegate>(&output_buffer));
    EXPECT_CALL(handler, ClearSiteData(_, _, _, _, _, _,
                                       partitioned_state_allowed_only));
    EXPECT_TRUE(handler.DoHandleHeader());
  }
}

}  // namespace content
