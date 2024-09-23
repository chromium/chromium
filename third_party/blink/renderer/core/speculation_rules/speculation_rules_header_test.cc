// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/speculation_rules_header.h"

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rules_metrics.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {
namespace {

using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::ResultOf;

class ScopedRegisterMockedURLLoads {
 public:
  ScopedRegisterMockedURLLoads() {
    url_test_helpers::RegisterMockedURLLoad(
        KURL("https://thirdparty-speculationrules.test/"
             "single_url_prefetch.json"),
        test::CoreTestDataPath("speculation_rules/single_url_prefetch.json"),
        "application/speculationrules+json");
    url_test_helpers::RegisterMockedURLLoad(
        KURL("https://thirdparty-speculationrules.test/"
             "single_url_prefetch_not_a_string_no_vary_search_hint.json"),
        test::CoreTestDataPath(
            "speculation_rules/"
            "single_url_prefetch_not_a_string_no_vary_search_hint.json"),
        "application/speculationrules+json");
    url_test_helpers::RegisterMockedURLLoad(
        KURL("https://thirdparty-speculationrules.test/"
             "single_url_prefetch_invalid_no_vary_search_hint.json"),
        test::CoreTestDataPath(
            "speculation_rules/"
            "single_url_prefetch_invalid_no_vary_search_hint.json"),
        "application/speculationrules+json");

    url_test_helpers::RegisterMockedURLLoad(
        KURL("https://speculationrules.test/"
             "single_url_prefetch_relative.json"),
        test::CoreTestDataPath(
            "speculation_rules/single_url_prefetch_relative.json"),
        "application/speculationrules+json");

    url_test_helpers::RegisterMockedURLLoad(
        KURL("https://speculationrules.test/document_rule_prefetch.json"),
        test::CoreTestDataPath("speculation_rules/document_rule_prefetch.json"),
        "application/speculationrules+json");

    KURL redirect_url(
        "https://speculationrules.test/"
        "redirect/single_url_prefetch_relative.json");
    ResourceResponse redirect(redirect_url);
    redirect.SetHttpStatusCode(net::HTTP_MOVED_PERMANENTLY);
    redirect.SetHttpHeaderField(
        http_names::kLocation,
        AtomicString("../single_url_prefetch_relative.json"));
    url_test_helpers::RegisterMockedURLLoadWithCustomResponse(
        redirect_url, "", WrappedResourceResponse(std::move(redirect)));

    KURL not_found_url("https://speculationrules.test/404");
    ResourceResponse not_found(not_found_url);
    not_found.SetHttpStatusCode(net::HTTP_NOT_FOUND);
    url_test_helpers::RegisterMockedURLLoadWithCustomResponse(
        not_found_url, "", WrappedResourceResponse(std::move(not_found)));

    KURL net_error_url("https://speculationrules.test/neterror");
    WebURLError error(net::ERR_INTERNET_DISCONNECTED, net_error_url);
    URLLoaderMockFactory::GetSingletonInstance()->RegisterErrorURL(
        net_error_url, WebURLResponse(), error);
  }

  ~ScopedRegisterMockedURLLoads() {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }
};

class ConsoleCapturingChromeClient : public EmptyChromeClient {
 public:
  void AddMessageToConsole(LocalFrame*,
                           mojom::ConsoleMessageSource,
                           mojom::ConsoleMessageLevel,
                           const String& message,
                           unsigned line_number,
                           const String& source_id,
                           const String& stack_trace) override {
    messages_.push_back(message);
  }

  const Vector<String>& ConsoleMessages() const { return messages_; }

 private:
  Vector<String> messages_;
};

TEST(SpeculationRulesHeaderTest, NoMetricsWithoutHeader) {
  test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  auto* chrome_client = MakeGarbageCollected<ConsoleCapturingChromeClient>();
  DummyPageHolder page_holder(/*initial_view_size=*/{}, chrome_client);

  ResourceResponse document_response(KURL("https://speculation-rules.test/"));
  document_response.SetHttpStatusCode(200);
  document_response.SetMimeType(AtomicString("text/html"));
  document_response.SetTextEncodingName(AtomicString("UTF-8"));
  SpeculationRulesHeader::ProcessHeadersForDocumentResponse(
      document_response, *page_holder.GetFrame().DomWindow());

  EXPECT_FALSE(page_holder.GetDocument().IsUseCounted(
      WebFeature::kSpeculationRulesHeader));
  histogram_tester.ExpectTotalCount("Blink.SpeculationRules.LoadOutcome", 0);
  EXPECT_THAT(chrome_client->ConsoleMessages(),
              Not(Contains(ResultOf([](const auto& m) { return m.Utf8(); },
                                    HasSubstr("Speculation-Rules")))));
}

TEST(SpeculationRulesHeaderTest, UnparseableHeader) {
  test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  auto* chrome_client = MakeGarbageCollected<ConsoleCapturingChromeClient>();
  DummyPageHolder page_holder(/*initial_view_size=*/{}, chrome_client);

  ResourceResponse document_response(KURL("https://speculation-rules.test/"));
  document_response.SetHttpStatusCode(200);
  document_response.SetMimeType(AtomicString("text/html"));
  document_response.SetTextEncodingName(AtomicString("UTF-8"));
  document_response.AddHttpHeaderField(http_names::kSpeculationRules,
                                       AtomicString("_:"));
  SpeculationRulesHeader::ProcessHeadersForDocumentResponse(
      document_response, *page_holder.GetFrame().DomWindow());

  EXPECT_TRUE(page_holder.GetDocument().IsUseCounted(
      WebFeature::kSpeculationRulesHeader));
  histogram_tester.ExpectUniqueSample(
      "Blink.SpeculationRules.LoadOutcome",
      SpeculationRulesLoadOutcome::kUnparseableSpeculationRulesHeader, 1);
  EXPECT_THAT(chrome_client->ConsoleMessages(),
              Contains(ResultOf([](const auto& m) { return m.Utf8(); },
                                HasSubstr("Speculation-Rules"))));
}

TEST(SpeculationRulesHeaderTest, EmptyHeader) {
  test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  DummyPageHolder page_holder;

  ResourceResponse document_response(KURL("https://speculation-rules.test/"));
  document_response.SetHttpStatusCode(200);
  document_response.SetMimeType(AtomicString("text/html"));
  document_response.SetTextEncodingName(AtomicString("UTF-8"));
  document_response.AddHttpHeaderField(http_names::kSpeculationRules,
                                       g_empty_atom);
  SpeculationRulesHeader::ProcessHeadersForDocumentResponse(
      document_response, *page_holder.GetFrame().DomWindow());

  EXPECT_TRUE(page_holder.GetDocument().IsUseCounted(
      WebFeature::kSpeculationRulesHeader));
  histogram_tester.ExpectUniqueSample(
      "Blink.SpeculationRules.LoadOutcome",
      SpeculationRulesLoadOutcome::kEmptySpeculationRulesHeader, 1);
}

TEST(SpeculationRulesHeaderTest, InvalidItem) {
  test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  auto* chrome_client = MakeGarbageCollected<ConsoleCapturingChromeClient>();
  DummyPageHolder page_holder(/*initial_view_size=*/{}, chrome_client);

  ResourceResponse document_response(KURL("https://speculation-rules.test/"));
  document_response.SetHttpStatusCode(200);
  document_response.SetMimeType(AtomicString("text/html"));
  document_response.SetTextEncodingName(AtomicString("UTF-8"));
  document_response.AddHttpHeaderField(
      http_names::kSpeculationRules,
      AtomicString("42, :aGVsbG8=:, ?1, \"://\""));
  SpeculationRulesHeader::ProcessHeadersForDocumentResponse(
      document_response, *page_holder.GetFrame().DomWindow());

  EXPECT_TRUE(page_holder.GetDocument().IsUseCounted(
      WebFeature::kSpeculationRulesHeader));
  histogram_tester.ExpectUniqueSample(
      "Blink.SpeculationRules.LoadOutcome",
      SpeculationRulesLoadOutcome::kInvalidSpeculationRulesHeaderItem, 4);
  EXPECT_THAT(chrome_client->ConsoleMessages(),
              Contains(ResultOf([](const auto& m) { return m.Utf8(); },
                                HasSubstr("Speculation-Rules")))
                  .Times(4));
}

TEST(SpeculationRulesHeaderTest, ValidURL) {
  test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  auto* chrome_client = MakeGarbageCollected<ConsoleCapturingChromeClient>();
  DummyPageHolder page_holder(/*initial_view_size=*/{}, chrome_client);
  ScopedRegisterMockedURLLoads mock_url_loads;

  ResourceResponse document_response(KURL("https://speculation-rules.test/"));
  document_response.SetHttpStatusCode(200);
  document_response.SetMimeType(AtomicString("text/html"));
  document_response.SetTextEncodingName(AtomicString("UTF-8"));
  document_response.AddHttpHeaderField(
      http_names::kSpeculationRules,
      AtomicString("\"https://thirdparty-speculationrules.test/"
                   "single_url_prefetch.json\""));
  SpeculationRulesHeader::ProcessHeadersForDocumentResponse(
      document_response, *page_holder.GetFrame().DomWindow());
  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_TRUE(page_holder.GetDocument().IsUseCounted(
      WebFeature::kSpeculationRulesHeader));
  histogram_tester.ExpectUniqueSample("Blink.SpeculationRules.LoadOutcome",
                                      SpeculationRulesLoadOutcome::kSuccess, 1);
  histogram_tester.ExpectTotalCount("Blink.SpeculationRules.FetchTime", 1);
  EXPECT_THAT(chrome_client->ConsoleMessages(),
              Not(Contains(ResultOf([](const auto& m) { return m.Utf8(); },
                                    HasSubstr("Speculation-Rules")))));
}

TEST(SpeculationRulesHeaderTest, InvalidNvsHintError) {
  test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  auto* chrome_client = MakeGarbageCollected<ConsoleCapturingChromeClient>();
  DummyPageHolder page_holder(/*initial_view_size=*/{}, chrome_client);
  ScopedRegisterMockedURLLoads mock_url_loads;

  ResourceResponse document_response(KURL("https://speculation-rules.test/"));
  document_response.SetHttpStatusCode(200);
  document_response.SetMimeType(AtomicString("text/html"));
  document_response.SetTextEncodingName(AtomicString("UTF-8"));
  document_response.AddHttpHeaderField(
      http_names::kSpeculationRules,
      AtomicString(
          "\"https://thirdparty-speculationrules.test/"
          "single_url_prefetch_not_a_string_no_vary_search_hint.json\""));
  SpeculationRulesHeader::ProcessHeadersForDocumentResponse(
      document_response, *page_holder.GetFrame().DomWindow());
  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_TRUE(page_holder.GetDocument().IsUseCounted(
      WebFeature::kSpeculationRulesHeader));
  histogram_tester.ExpectUniqueSample("Blink.SpeculationRules.LoadOutcome",
                                      SpeculationRulesLoadOutcome::kSuccess, 1);
  histogram_tester.ExpectTotalCount("Blink.SpeculationRules.FetchTime", 1);

  EXPECT_THAT(
      chrome_client->ConsoleMessages(),
      Contains(ResultOf(
          [](const auto& m) { return m.Utf8(); },
          HasSubstr("expects_no_vary_search's value must be a string"))));
}

TEST(SpeculationRulesHeaderTest, InvalidNvsHintWarning) {
  test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  auto* chrome_client = MakeGarbageCollected<ConsoleCapturingChromeClient>();
  DummyPageHolder page_holder(/*initial_view_size=*/{}, chrome_client);
  ScopedRegisterMockedURLLoads mock_url_loads;

  ResourceResponse document_response(KURL("https://speculation-rules.test/"));
  document_response.SetHttpStatusCode(200);
  document_response.SetMimeType(AtomicString("text/html"));
  document_response.SetTextEncodingName(AtomicString("UTF-8"));
  document_response.AddHttpHeaderField(
      http_names::kSpeculationRules,
      AtomicString("\"https://thirdparty-speculationrules.test/"
                   "single_url_prefetch_invalid_no_vary_search_hint.json\""));
  SpeculationRulesHeader::ProcessHeadersForDocumentResponse(
      document_response, *page_holder.GetFrame().DomWindow());
  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_TRUE(page_holder.GetDocument().IsUseCounted(
      WebFeature::kSpeculationRulesHeader));
  histogram_tester.ExpectUniqueSample("Blink.SpeculationRules.LoadOutcome",
                                      SpeculationRulesLoadOutcome::kSuccess, 1);
  histogram_tester.ExpectTotalCount("Blink.SpeculationRules.FetchTime", 1);

  EXPECT_THAT(chrome_client->ConsoleMessages(),
              Contains(ResultOf(
                  [](const auto& m) { return m.Utf8(); },
                  HasSubstr("contains a \"params\" dictionary value"
                            " that is not a list of strings or a boolean"))));
}

TEST(SpeculationRulesHeaderTest, UsesResponseURLAsBaseURL) {
  test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  auto* chrome_client = MakeGarbageCollected<ConsoleCapturingChromeClient>();
  DummyPageHolder page_holder(/*initial_view_size=*/{}, chrome_client);
  ScopedRegisterMockedURLLoads mock_url_loads;

  ResourceResponse document_response(KURL("https://speculation-rules.test/"));
  document_response.SetHttpStatusCode(200);
  document_response.SetMimeType(AtomicString("text/html"));
  document_response.SetTextEncodingName(AtomicString("UTF-8"));
  document_response.AddHttpHeaderField(
      http_names::kSpeculationRules,
      AtomicString("\"https://speculationrules.test/"
                   "redirect/single_url_prefetch_relative.json\""));
  SpeculationRulesHeader::ProcessHeadersForDocumentResponse(
      document_response, *page_holder.GetFrame().DomWindow());
  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_TRUE(page_holder.GetDocument().IsUseCounted(
      WebFeature::kSpeculationRulesHeader));
  histogram_tester.ExpectUniqueSample("Blink.SpeculationRules.LoadOutcome",
                                      SpeculationRulesLoadOutcome::kSuccess, 1);
  histogram_tester.ExpectTotalCount("Blink.SpeculationRules.FetchTime", 1);
  EXPECT_THAT(chrome_client->ConsoleMessages(),
              Not(Contains(ResultOf([](const auto& m) { return m.Utf8(); },
                                    HasSubstr("Speculation-Rules")))));

  SpeculationRuleSet* rule_set =
      DocumentSpeculationRules::From(page_holder.GetDocument()).rule_sets()[0];
  EXPECT_EQ(
      KURL("https://speculationrules.test/single_url_prefetch_relative.json"),
      rule_set->source()->GetBaseURL());
  EXPECT_THAT(
      rule_set->prefetch_rules()[0]->urls(),
      ::testing::ElementsAre(KURL("https://speculationrules.test/next.html")));
}

TEST(SpeculationRulesHeaderTest, InvalidStatusCode) {
  test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  auto* chrome_client = MakeGarbageCollected<ConsoleCapturingChromeClient>();
  DummyPageHolder page_holder(/*initial_view_size=*/{}, chrome_client);
  ScopedRegisterMockedURLLoads mock_url_loads;

  ResourceResponse document_response(KURL("https://speculation-rules.test/"));
  document_response.SetHttpStatusCode(200);
  document_response.SetMimeType(AtomicString("text/html"));
  document_response.SetTextEncodingName(AtomicString("UTF-8"));
  document_response.AddHttpHeaderField(
      http_names::kSpeculationRules,
      AtomicString("\"https://speculationrules.test/404\""));
  SpeculationRulesHeader::ProcessHeadersForDocumentResponse(
      document_response, *page_holder.GetFrame().DomWindow());
  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_TRUE(page_holder.GetDocument().IsUseCounted(
      WebFeature::kSpeculationRulesHeader));
  histogram_tester.ExpectUniqueSample(
      "Blink.SpeculationRules.LoadOutcome",
      SpeculationRulesLoadOutcome::kLoadFailedOrCanceled, 1);
  histogram_tester.ExpectTotalCount("Blink.SpeculationRules.FetchTime", 1);
  EXPECT_THAT(chrome_client->ConsoleMessages(),
              Contains(ResultOf(
                  [](const auto& m) { return m.Utf8(); },
                  AllOf(HasSubstr("Speculation-Rules"), HasSubstr("404")))));

  EXPECT_THAT(
      DocumentSpeculationRules::From(page_holder.GetDocument()).rule_sets(),
      ::testing::IsEmpty());
}

TEST(SpeculationRulesHeaderTest, NetError) {
  test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  auto* chrome_client = MakeGarbageCollected<ConsoleCapturingChromeClient>();
  DummyPageHolder page_holder(/*initial_view_size=*/{}, chrome_client);
  ScopedRegisterMockedURLLoads mock_url_loads;

  ResourceResponse document_response(KURL("https://speculation-rules.test/"));
  document_response.SetHttpStatusCode(200);
  document_response.SetMimeType(AtomicString("text/html"));
  document_response.SetTextEncodingName(AtomicString("UTF-8"));
  document_response.AddHttpHeaderField(
      http_names::kSpeculationRules,
      AtomicString("\"https://speculationrules.test/neterror\""));
  SpeculationRulesHeader::ProcessHeadersForDocumentResponse(
      document_response, *page_holder.GetFrame().DomWindow());
  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_TRUE(page_holder.GetDocument().IsUseCounted(
      WebFeature::kSpeculationRulesHeader));
  histogram_tester.ExpectUniqueSample(
      "Blink.SpeculationRules.LoadOutcome",
      SpeculationRulesLoadOutcome::kLoadFailedOrCanceled, 1);
  histogram_tester.ExpectTotalCount("Blink.SpeculationRules.FetchTime", 1);
  EXPECT_THAT(chrome_client->ConsoleMessages(),
              Contains(ResultOf([](const auto& m) { return m.Utf8(); },
                                AllOf(HasSubstr("Speculation-Rules"),
                                      HasSubstr("INTERNET_DISCONNECTED")))));

  EXPECT_THAT(
      DocumentSpeculationRules::From(page_holder.GetDocument()).rule_sets(),
      ::testing::IsEmpty());
}

// Regression test for crbug.com/356767669.
// Order of events:
// 1) The load of the speculation rules header completes
// 2) The document detaches
// 3) SpeculationRuleLoader::NotifyFinished is called
TEST(SpeculationRulesHeaderTest, DocumentDetached) {
  test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  auto* chrome_client = MakeGarbageCollected<ConsoleCapturingChromeClient>();
  DummyPageHolder page_holder(/*initial_view_size=*/{}, chrome_client);
  ScopedRegisterMockedURLLoads mock_url_loads;

  ResourceResponse document_response(KURL("https://speculation-rules.test/"));
  document_response.SetHttpStatusCode(200);
  document_response.SetMimeType(AtomicString("text/html"));
  document_response.SetTextEncodingName(AtomicString("UTF-8"));
  document_response.AddHttpHeaderField(
      http_names::kSpeculationRules,
      AtomicString("\"https://speculationrules.test/"
                   "document_rule_prefetch.json\""));
  SpeculationRulesHeader::ProcessHeadersForDocumentResponse(
      document_response, *page_holder.GetFrame().DomWindow());

  page_holder.GetDocument()
      .GetTaskRunner(TaskType::kDOMManipulation)
      ->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                   page_holder.GetDocument().Shutdown();
                 }));
  url_test_helpers::ServeAsynchronousRequests();

  histogram_tester.ExpectUniqueSample("Blink.SpeculationRules.LoadOutcome",
                                      SpeculationRulesLoadOutcome::kSuccess, 0);
  histogram_tester.ExpectTotalCount("Blink.SpeculationRules.FetchTime", 1);
  EXPECT_THAT(chrome_client->ConsoleMessages(),
              Not(Contains(ResultOf([](const auto& m) { return m.Utf8(); },
                                    HasSubstr("Speculation-Rules")))));
}

}  // namespace
}  // namespace blink
