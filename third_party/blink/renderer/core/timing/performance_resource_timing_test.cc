// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_resource_timing.h"

#include "base/time/time.h"
#include "services/network/public/mojom/service_worker_router_info.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_mojo_handler.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class PerformanceResourceTimingTest : public testing::Test {
 protected:
  AtomicString GetNextHopProtocol(const AtomicString& alpn_negotiated_protocol,
                                  const AtomicString& connection_info) {
    mojom::blink::ResourceTimingInfo info;
    info.allow_timing_details = true;
    PerformanceResourceTiming* timing =
        MakePerformanceResourceTiming(info.Clone());
    return timing->GetNextHopProtocol(alpn_negotiated_protocol,
                                      connection_info);
  }

  AtomicString GetNextHopProtocolWithoutTao(
      const AtomicString& alpn_negotiated_protocol,
      const AtomicString& connection_info) {
    mojom::blink::ResourceTimingInfo info;
    info.allow_timing_details = false;
    PerformanceResourceTiming* timing =
        MakePerformanceResourceTiming(info.Clone());
    return timing->GetNextHopProtocol(alpn_negotiated_protocol,
                                      connection_info);
  }

  void Initialize(ScriptState* script_state) {
    script_state_ = script_state;
    auto* window = LocalDOMWindow::From(script_state_);
    ASSERT_TRUE(window);
    performance_ = DOMWindowPerformance::performance(*window);
    ASSERT_TRUE(performance_);
  }

  ScriptState* GetScriptState() { return script_state_; }

  PerformanceResourceTiming* MakePerformanceResourceTiming(
      mojom::blink::ResourceTimingInfoPtr info) {
    std::unique_ptr<DummyPageHolder> dummy_page_holder =
        std::make_unique<DummyPageHolder>();
    return MakeGarbageCollected<PerformanceResourceTiming>(
        std::move(info), g_empty_atom,
        base::TimeTicks() + base::Milliseconds(100),
        dummy_page_holder->GetDocument()
            .GetExecutionContext()
            ->CrossOriginIsolatedCapability(),
        dummy_page_holder->GetDocument().GetExecutionContext(),
        performance_->NavigationId().web_exposed_id);
  }

  test::TaskEnvironment task_environment_;
  Persistent<ScriptState> script_state_;
  Persistent<WindowPerformance> performance_;
};

TEST_F(PerformanceResourceTimingTest,
       TestFallbackToConnectionInfoWhenALPNUnknown) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  AtomicString connection_info("http/1.1");
  AtomicString alpn_negotiated_protocol("unknown");
  EXPECT_EQ(GetNextHopProtocol(alpn_negotiated_protocol, connection_info),
            connection_info);
}

TEST_F(PerformanceResourceTimingTest,
       TestFallbackToHTTPInfoWhenALPNAndConnectionInfoUnknown) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  AtomicString connection_info("unknown");
  AtomicString alpn_negotiated_protocol("unknown");
  EXPECT_EQ(GetNextHopProtocol(alpn_negotiated_protocol, connection_info), "");
}

TEST_F(PerformanceResourceTimingTest, TestNoChangeWhenContainsQuic) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  AtomicString connection_info("http/1.1");
  AtomicString alpn_negotiated_protocol("http/2+quic/39");
  EXPECT_EQ(GetNextHopProtocol(alpn_negotiated_protocol, connection_info),
            alpn_negotiated_protocol);
}

TEST_F(PerformanceResourceTimingTest, TestNoChangeWhenOtherwise) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  AtomicString connection_info("http/1.1");
  AtomicString alpn_negotiated_protocol("RandomProtocol");
  EXPECT_EQ(GetNextHopProtocol(alpn_negotiated_protocol, connection_info),
            alpn_negotiated_protocol);
}

TEST_F(PerformanceResourceTimingTest, TestNextHopProtocolIsGuardedByTao) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  AtomicString connection_info("http/1.1");
  AtomicString alpn_negotiated_protocol("RandomProtocol");
  EXPECT_EQ(
      GetNextHopProtocolWithoutTao(alpn_negotiated_protocol, connection_info),
      "");
}

TEST_F(PerformanceResourceTimingTest, TestRequestStart) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      std::make_unique<DummyPageHolder>();

  network::mojom::blink::LoadTimingInfo timing;

  mojom::blink::ResourceTimingInfo info;

  info.allow_timing_details = true;

  info.timing = network::mojom::blink::LoadTimingInfo::New();

  info.timing->send_start = base::TimeTicks() + base::Milliseconds(1803);

  PerformanceResourceTiming* resource_timing =
      MakePerformanceResourceTiming(info.Clone());

  EXPECT_EQ(resource_timing->requestStart(), 1703);
}

TEST_F(PerformanceResourceTimingTest, TestRequestStartFalseAllowTimingDetails) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      std::make_unique<DummyPageHolder>();

  network::mojom::blink::LoadTimingInfo timing;

  mojom::blink::ResourceTimingInfo info;

  info.allow_timing_details = false;

  info.timing = network::mojom::blink::LoadTimingInfo::New();

  info.timing->send_start = base::TimeTicks() + base::Milliseconds(1000);

  PerformanceResourceTiming* resource_timing =
      MakePerformanceResourceTiming(info.Clone());

  // If info.allow_timing_details is false, requestStart is 0.
  EXPECT_EQ(resource_timing->requestStart(), 0);
}

TEST_F(PerformanceResourceTimingTest, TestRequestStartNullLoadTimingInfo) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      std::make_unique<DummyPageHolder>();

  mojom::blink::ResourceTimingInfo info;

  info.allow_timing_details = true;

  info.start_time = base::TimeTicks() + base::Milliseconds(396);

  PerformanceResourceTiming* resource_timing =
      MakePerformanceResourceTiming(info.Clone());

  // If info.timing is null, the requestStart value will fall back all the way
  // to startTime.
  EXPECT_EQ(resource_timing->requestStart(), resource_timing->startTime());

  EXPECT_EQ(resource_timing->requestStart(), 296);
}

TEST_F(PerformanceResourceTimingTest, TestRequestStartNullSendStart) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      std::make_unique<DummyPageHolder>();

  mojom::blink::ResourceTimingInfo info;

  info.allow_timing_details = true;

  info.timing = network::mojom::blink::LoadTimingInfo::New();

  info.timing->connect_timing =
      network::mojom::blink::LoadTimingInfoConnectTiming::New();

  info.timing->connect_timing->connect_end =
      base::TimeTicks() + base::Milliseconds(751);

  PerformanceResourceTiming* resource_timing =
      MakePerformanceResourceTiming(info.Clone());

  // If info.timing->send_start is null, the requestStart value will fall back
  // to connectEnd.
  EXPECT_EQ(resource_timing->requestStart(), resource_timing->connectEnd());
  EXPECT_EQ(resource_timing->requestStart(), 651);
}

TEST_F(PerformanceResourceTimingTest,
       ServiceWorkerRouterSourceUseCountersEmpty) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  Document& document = scope.GetDocument();
  ExecutionContext* context = scope.GetExecutionContext();

  mojom::blink::ResourceTimingInfo info;
  info.allow_timing_details = true;

  auto* resource_timing = MakeGarbageCollected<PerformanceResourceTiming>(
      info.Clone(), g_empty_atom, base::TimeTicks() + base::Milliseconds(100),
      context->CrossOriginIsolatedCapability(), context,
      performance_->NavigationId().web_exposed_id);

  EXPECT_TRUE(resource_timing->workerMatchedSourceType(context).empty());
  EXPECT_TRUE(resource_timing->workerMatchedRouterSource(context).empty());
  EXPECT_TRUE(resource_timing->workerFinalSourceType(context).empty());
  EXPECT_TRUE(resource_timing->workerFinalRouterSource(context).empty());

  EXPECT_FALSE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerMatchedSourceTypeNonEmpty));
  EXPECT_FALSE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerMatchedRouterSourceNonEmpty));
  EXPECT_FALSE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerMatchedBothGettersNonEmpty));
  EXPECT_FALSE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerFinalSourceTypeNonEmpty));
  EXPECT_FALSE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerFinalRouterSourceNonEmpty));
  EXPECT_FALSE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerFinalBothGettersNonEmpty));
}

TEST_F(PerformanceResourceTimingTest,
       ServiceWorkerRouterSourceUseCountersNonEmptyAndBothGetters) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  Document& document = scope.GetDocument();
  ExecutionContext* context = scope.GetExecutionContext();

  mojom::blink::ResourceTimingInfo info;
  info.allow_timing_details = true;
  info.service_worker_router_info =
      network::mojom::blink::ServiceWorkerRouterInfo::New();
  info.service_worker_router_info->matched_source_type =
      network::mojom::ServiceWorkerRouterSourceType::kNetwork;
  info.service_worker_router_info->actual_source_type =
      network::mojom::ServiceWorkerRouterSourceType::kCache;

  auto* resource_timing = MakeGarbageCollected<PerformanceResourceTiming>(
      info.Clone(), g_empty_atom, base::TimeTicks() + base::Milliseconds(100),
      context->CrossOriginIsolatedCapability(), context,
      performance_->NavigationId().web_exposed_id);

  // Entry construction and toJSON() should not trigger any of the non-empty
  // getter UseCounters.
  resource_timing->toJSONForBinding(scope.GetScriptState());
  EXPECT_FALSE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerMatchedSourceTypeNonEmpty));
  EXPECT_FALSE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerMatchedRouterSourceNonEmpty));
  EXPECT_FALSE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerMatchedBothGettersNonEmpty));
  EXPECT_FALSE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerFinalSourceTypeNonEmpty));
  EXPECT_FALSE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerFinalRouterSourceNonEmpty));
  EXPECT_FALSE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerFinalBothGettersNonEmpty));

  // Reading only the legacy matched getter counts only MatchedSourceType.
  EXPECT_EQ(resource_timing->workerMatchedSourceType(context), "network");
  EXPECT_TRUE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerMatchedSourceTypeNonEmpty));
  EXPECT_FALSE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerMatchedRouterSourceNonEmpty));
  EXPECT_FALSE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerMatchedBothGettersNonEmpty));

  // Reading only the standard final getter counts only FinalRouterSource.
  EXPECT_EQ(resource_timing->workerFinalRouterSource(context), "cache");
  EXPECT_TRUE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerFinalRouterSourceNonEmpty));
  EXPECT_FALSE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerFinalSourceTypeNonEmpty));
  EXPECT_FALSE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerFinalBothGettersNonEmpty));

  // Reading the standard matched getter on the same entry now triggers both
  // MatchedRouterSource and MatchedBothGetters.
  EXPECT_EQ(resource_timing->workerMatchedRouterSource(context), "network");
  EXPECT_TRUE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerMatchedRouterSourceNonEmpty));
  EXPECT_TRUE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerMatchedBothGettersNonEmpty));

  // Reading the legacy final getter on the same entry now triggers both
  // FinalSourceType and FinalBothGetters.
  EXPECT_EQ(resource_timing->workerFinalSourceType(context), "cache");
  EXPECT_TRUE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerFinalSourceTypeNonEmpty));
  EXPECT_TRUE(document.IsUseCounted(
      WebFeature::kPerformanceResourceTimingWorkerFinalBothGettersNonEmpty));
}

// The static router source types are response-derived attributes and must be
// masked (empty) when the timing allow check failed, so that a page cannot
// distinguish a delivered opaque response from a network error.
TEST_F(PerformanceResourceTimingTest, ServiceWorkerRouterSourceIsGuardedByTao) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  ExecutionContext* context = scope.GetExecutionContext();

  mojom::blink::ResourceTimingInfo info;
  info.allow_timing_details = false;
  info.service_worker_router_info =
      network::mojom::blink::ServiceWorkerRouterInfo::New();
  info.service_worker_router_info->matched_source_type =
      network::mojom::ServiceWorkerRouterSourceType::kNetwork;
  info.service_worker_router_info->actual_source_type =
      network::mojom::ServiceWorkerRouterSourceType::kNetwork;
  info.timing = network::mojom::blink::LoadTimingInfo::New();
  info.timing->service_worker_router_evaluation_start =
      base::TimeTicks() + base::Milliseconds(200);
  info.timing->service_worker_cache_lookup_start =
      base::TimeTicks() + base::Milliseconds(300);

  auto* resource_timing = MakeGarbageCollected<PerformanceResourceTiming>(
      info.Clone(), g_empty_atom, base::TimeTicks() + base::Milliseconds(100),
      context->CrossOriginIsolatedCapability(), context,
      performance_->NavigationId().web_exposed_id);

  EXPECT_TRUE(resource_timing->workerMatchedSourceType(context).empty());
  EXPECT_TRUE(resource_timing->workerMatchedRouterSource(context).empty());
  EXPECT_TRUE(resource_timing->workerFinalSourceType(context).empty());
  EXPECT_TRUE(resource_timing->workerFinalRouterSource(context).empty());
  EXPECT_EQ(resource_timing->workerRouterEvaluationStart(), 0.0);
  EXPECT_EQ(resource_timing->workerCacheLookupStart(), 0.0);
}

// When a static router cache hit fails the timing allow check, fetchStart must
// not fall back to the (masked, zero) responseStart, which would make it
// smaller than startTime.
TEST_F(PerformanceResourceTimingTest,
       ServiceWorkerRouterCacheFetchStartWithoutTao) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  ExecutionContext* context = scope.GetExecutionContext();

  mojom::blink::ResourceTimingInfo info;
  info.allow_timing_details = false;
  info.start_time = base::TimeTicks() + base::Milliseconds(600);
  info.service_worker_router_info =
      network::mojom::blink::ServiceWorkerRouterInfo::New();
  info.service_worker_router_info->matched_source_type =
      network::mojom::ServiceWorkerRouterSourceType::kCache;
  info.service_worker_router_info->actual_source_type =
      network::mojom::ServiceWorkerRouterSourceType::kCache;
  info.timing = network::mojom::blink::LoadTimingInfo::New();

  auto* resource_timing = MakeGarbageCollected<PerformanceResourceTiming>(
      info.Clone(), g_empty_atom, base::TimeTicks() + base::Milliseconds(100),
      context->CrossOriginIsolatedCapability(), context,
      performance_->NavigationId().web_exposed_id);

  EXPECT_EQ(resource_timing->responseStart(), 0.0);
  EXPECT_EQ(resource_timing->fetchStart(), resource_timing->startTime());
  EXPECT_EQ(resource_timing->fetchStart(), 500);
}

using PerformanceResourceTimingSimTest = SimTest;

TEST_F(PerformanceResourceTimingSimTest, FailedSubframeNavigationContentType) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"(
    <iframe id="subframe" src="https://example.com/404.html"></iframe>
  )");

  auto* iframe = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("subframe")));
  ASSERT_TRUE(iframe);
  ASSERT_TRUE(iframe->ContentFrame());

  auto* mojo_handler =
      MakeGarbageCollected<LocalFrameMojoHandler>(*GetDocument().GetFrame());
  static_cast<mojom::blink::LocalFrame*>(mojo_handler)
      ->AddResourceTimingEntryForFailedSubframeNavigation(
          iframe->ContentFrame()->GetFrameToken(),
          KURL("https://example.com/404.html"),
          /*start_time=*/base::TimeTicks::Now(),
          /*redirect_time=*/base::TimeTicks::Now(),
          /*request_start=*/base::TimeTicks::Now(),
          /*response_start=*/base::TimeTicks::Now(),
          /*completion_time=*/base::TimeTicks::Now(),
          /*response_code=*/404,
          /*mime_type=*/"text/html",
          network::mojom::blink::LoadTimingInfo::New(),
          net::HttpConnectionInfo::kHTTP1_1,
          /*alpn_negotiated_protocol=*/"http/1.1",
          /*is_secure_transport=*/true,
          /*is_validated=*/false,
          /*normalized_server_timing=*/g_empty_string,
          /*resource_lengths=*/nullptr);

  PerformanceEntryVector entries =
      DOMWindowPerformance::performance(*GetDocument().domWindow())
          ->getEntriesByType(performance_entry_names::kResource);
  ASSERT_EQ(entries.size(), 1u);
  ASSERT_EQ(entries[0]->EntryTypeEnum(), PerformanceEntry::kResource);
  auto* resource_timing =
      static_cast<PerformanceResourceTiming*>(entries[0].Get());
  EXPECT_EQ(resource_timing->responseStatus(), 404);
  EXPECT_EQ(resource_timing->contentType(), "text/html");

  iframe->remove();
}

}  // namespace blink
