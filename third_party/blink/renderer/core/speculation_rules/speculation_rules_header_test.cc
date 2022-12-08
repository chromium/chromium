// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/speculation_rules_header.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rules_metrics.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
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
  ScopedSpeculationRulesFetchFromHeaderForTest enable_fetch_from_header(true);
  base::HistogramTester histogram_tester;
  auto* chrome_client = MakeGarbageCollected<ConsoleCapturingChromeClient>();
  DummyPageHolder page_holder(/*initial_view_size=*/{}, chrome_client);

  ResourceResponse document_response(KURL("https://speculation-rules.test/"));
  document_response.SetHttpStatusCode(200);
  document_response.SetMimeType("text/html");
  document_response.SetTextEncodingName("UTF-8");
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
  ScopedSpeculationRulesFetchFromHeaderForTest enable_fetch_from_header(true);
  base::HistogramTester histogram_tester;
  auto* chrome_client = MakeGarbageCollected<ConsoleCapturingChromeClient>();
  DummyPageHolder page_holder(/*initial_view_size=*/{}, chrome_client);

  ResourceResponse document_response(KURL("https://speculation-rules.test/"));
  document_response.SetHttpStatusCode(200);
  document_response.SetMimeType("text/html");
  document_response.SetTextEncodingName("UTF-8");
  document_response.AddHttpHeaderField(http_names::kSpeculationRules, "_:");
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
  ScopedSpeculationRulesFetchFromHeaderForTest enable_fetch_from_header(true);
  base::HistogramTester histogram_tester;
  DummyPageHolder page_holder;

  ResourceResponse document_response(KURL("https://speculation-rules.test/"));
  document_response.SetHttpStatusCode(200);
  document_response.SetMimeType("text/html");
  document_response.SetTextEncodingName("UTF-8");
  document_response.AddHttpHeaderField(http_names::kSpeculationRules, "");
  SpeculationRulesHeader::ProcessHeadersForDocumentResponse(
      document_response, *page_holder.GetFrame().DomWindow());

  EXPECT_TRUE(page_holder.GetDocument().IsUseCounted(
      WebFeature::kSpeculationRulesHeader));
  histogram_tester.ExpectUniqueSample(
      "Blink.SpeculationRules.LoadOutcome",
      SpeculationRulesLoadOutcome::kEmptySpeculationRulesHeader, 1);
}

TEST(SpeculationRulesHeaderTest, InvalidItem) {
  ScopedSpeculationRulesFetchFromHeaderForTest enable_fetch_from_header(true);
  base::HistogramTester histogram_tester;
  auto* chrome_client = MakeGarbageCollected<ConsoleCapturingChromeClient>();
  DummyPageHolder page_holder(/*initial_view_size=*/{}, chrome_client);

  ResourceResponse document_response(KURL("https://speculation-rules.test/"));
  document_response.SetHttpStatusCode(200);
  document_response.SetMimeType("text/html");
  document_response.SetTextEncodingName("UTF-8");
  document_response.AddHttpHeaderField(http_names::kSpeculationRules,
                                       "42, :aGVsbG8=:, ?1, \"://\"");
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
  ScopedSpeculationRulesFetchFromHeaderForTest enable_fetch_from_header(true);
  base::HistogramTester histogram_tester;
  auto* chrome_client = MakeGarbageCollected<ConsoleCapturingChromeClient>();
  DummyPageHolder page_holder(/*initial_view_size=*/{}, chrome_client);
  ScopedRegisterMockedURLLoads mock_url_loads;

  ResourceResponse document_response(KURL("https://speculation-rules.test/"));
  document_response.SetHttpStatusCode(200);
  document_response.SetMimeType("text/html");
  document_response.SetTextEncodingName("UTF-8");
  document_response.AddHttpHeaderField(
      http_names::kSpeculationRules,
      "\"https://thirdparty-speculationrules.test/single_url_prefetch.json\"");
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

}  // namespace
}  // namespace blink
