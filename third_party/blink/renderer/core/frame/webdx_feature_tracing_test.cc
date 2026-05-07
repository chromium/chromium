// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/webdx_feature_tracing.h"

#include "base/test/tracing/trace_event_analyzer.h"
#include "base/test/tracing/trace_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/use_counter/metrics/webdx_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/use_counter_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

using trace_analyzer::Query;
using trace_analyzer::TraceAnalyzer;
using trace_analyzer::TraceEventVector;

TEST(WebDXFeatureTracingTest, WebDXFeatureEnumToString) {
  EXPECT_EQ(
      WebDXFeatureEnumToString(mojom::blink::WebDXFeature::kViewTransitions),
      "view-transitions");
  EXPECT_EQ(WebDXFeatureEnumToString(mojom::blink::WebDXFeature::kDialog),
            "dialog");

  // Numbers with dashes
  EXPECT_EQ(WebDXFeatureEnumToString(mojom::blink::WebDXFeature::kCanvas_2d),
            "canvas-2d");
  EXPECT_EQ(WebDXFeatureEnumToString(
                mojom::blink::WebDXFeature::kCanvas_2dColorManagement),
            "canvas-2d-color-management");

  // Numbers without dashes
  EXPECT_EQ(WebDXFeatureEnumToString(mojom::blink::WebDXFeature::kFloat16array),
            "float16array");
  EXPECT_EQ(WebDXFeatureEnumToString(
                mojom::blink::WebDXFeature::kUint8arrayBase64Hex),
            "uint8array-base64-hex");
}

TEST(WebDXFeatureTracingTest, WebDXFeatureEnumToString_Draft) {
  EXPECT_EQ(WebDXFeatureEnumToString(
                mojom::blink::WebDXFeature::kDRAFT_WasmBranchHinting),
            "wasm-branch-hinting");
}

TEST(WebDXFeatureTracingTest, WebDXFeatureEnumToString_EmptyStringForObsolete) {
  EXPECT_EQ(WebDXFeatureEnumToString(
                mojom::blink::WebDXFeature::kOBSOLETE_TranslationApi),
            std::string());
}

TEST(WebDXFeatureTracingTest, MaybeEmitWebDXFeatureTraceEvent_Disabled) {
  test::TaskEnvironment task_environment;
  base::test::TracingEnvironment tracing_environment;
  auto dummy_page_holder = std::make_unique<DummyPageHolder>();

  trace_analyzer::Start("*");

  UseCounterFeature feature(
      mojom::blink::UseCounterFeatureType::kWebDXFeature,
      static_cast<uint32_t>(mojom::blink::WebDXFeature::kViewTransitions));

  MaybeEmitWebDXFeatureTraceEvent(feature, &dummy_page_holder->GetFrame());

  auto analyzer = trace_analyzer::Stop();

  TraceEventVector events;
  analyzer->FindEvents(Query::EventName() == Query::String("WebDXFeatureUsage"),
                       &events);

  EXPECT_EQ(0u, events.size());
}

TEST(WebDXFeatureTracingTest, MaybeEmitWebDXFeatureTraceEvent_V8Context) {
  test::TaskEnvironment task_environment;
  base::test::TracingEnvironment tracing_environment;
  V8TestingScope scope;

  KURL doc_url("https://example.com/v8-page.html");
  scope.GetFrame().GetDocument()->SetURL(doc_url);

  trace_analyzer::Start("blink.webdx_feature_usage");

  UseCounterFeature feature(
      mojom::blink::UseCounterFeatureType::kWebDXFeature,
      static_cast<uint32_t>(
          mojom::blink::WebDXFeature::kRequestAnimationFrame));

  // Calling inside V8 context
  MaybeEmitWebDXFeatureTraceEvent(feature, &scope.GetFrame());

  auto analyzer = trace_analyzer::Stop();

  TraceEventVector events;
  analyzer->FindEvents(Query::EventName() == Query::String("WebDXFeatureUsage"),
                       &events);

  EXPECT_EQ(1u, events.size());
  EXPECT_EQ("request-animation-frame",
            events[0]->GetKnownArgAsString("feature"));
  EXPECT_EQ("https://example.com/v8-page.html",
            events[0]->GetKnownArgAsString("url"));
  EXPECT_EQ(0, events[0]->GetKnownArgAsInt("lineNumber"));
  EXPECT_EQ(0, events[0]->GetKnownArgAsInt("columnNumber"));
}

TEST(WebDXFeatureTracingTest,
     MaybeEmitWebDXFeatureTraceEvent_DocumentUrlFallback) {
  test::TaskEnvironment task_environment;
  base::test::TracingEnvironment tracing_environment;
  auto dummy_page_holder = std::make_unique<DummyPageHolder>();

  // Set a document URL to verify fallback outside V8 context
  KURL doc_url("https://example.com/plain-page.html");
  dummy_page_holder->GetFrame().GetDocument()->SetURL(doc_url);

  trace_analyzer::Start("blink.webdx_feature_usage");

  UseCounterFeature feature(
      mojom::blink::UseCounterFeatureType::kWebDXFeature,
      static_cast<uint32_t>(mojom::blink::WebDXFeature::kViewTransitions));

  // Calling outside of V8 context
  MaybeEmitWebDXFeatureTraceEvent(feature, &dummy_page_holder->GetFrame());

  auto analyzer = trace_analyzer::Stop();

  TraceEventVector events;
  analyzer->FindEvents(Query::EventName() == Query::String("WebDXFeatureUsage"),
                       &events);

  EXPECT_EQ(1u, events.size());
  EXPECT_EQ("view-transitions", events[0]->GetKnownArgAsString("feature"));
  EXPECT_EQ("https://example.com/plain-page.html",
            events[0]->GetKnownArgAsString("url"));
  EXPECT_EQ(-1, events[0]->GetKnownArgAsInt("lineNumber"));
  EXPECT_EQ(-1, events[0]->GetKnownArgAsInt("columnNumber"));
}

TEST(WebDXFeatureTracingTest, MaybeEmitWebDXFeatureTraceEvent_NullFrame) {
  test::TaskEnvironment task_environment;
  base::test::TracingEnvironment tracing_environment;

  trace_analyzer::Start("blink.webdx_feature_usage");

  UseCounterFeature feature(
      mojom::blink::UseCounterFeatureType::kWebDXFeature,
      static_cast<uint32_t>(mojom::blink::WebDXFeature::kViewTransitions));

  // Should not crash when frame is null
  MaybeEmitWebDXFeatureTraceEvent(feature, nullptr);

  auto analyzer = trace_analyzer::Stop();

  TraceEventVector events;
  analyzer->FindEvents(Query::EventName() == Query::String("WebDXFeatureUsage"),
                       &events);

  EXPECT_EQ(1u, events.size());
  EXPECT_EQ("view-transitions", events[0]->GetKnownArgAsString("feature"));
  EXPECT_EQ("", events[0]->GetKnownArgAsString("url"));
}

}  // namespace blink
