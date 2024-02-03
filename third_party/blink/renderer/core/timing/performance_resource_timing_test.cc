// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_resource_timing.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
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

  void Initialize(ScriptState* script_state) { script_state_ = script_state; }

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
        dummy_page_holder->GetDocument().GetExecutionContext());
  }

  test::TaskEnvironment task_environment_;
  Persistent<ScriptState> script_state_;
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
}  // namespace blink
