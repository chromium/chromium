// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_resource_timing.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"

namespace blink {

class PerformanceResourceTimingTest : public testing::Test {
 protected:
  AtomicString GetNextHopProtocol(const AtomicString& alpn_negotiated_protocol,
                                  const AtomicString& connection_info) {
    mojom::blink::ResourceTimingInfo info;
    info.allow_timing_details = true;
    PerformanceResourceTiming* timing = MakePerformanceResourceTiming(info);
    return timing->GetNextHopProtocol(alpn_negotiated_protocol,
                                      connection_info);
  }

  AtomicString GetNextHopProtocolWithoutTao(
      const AtomicString& alpn_negotiated_protocol,
      const AtomicString& connection_info) {
    mojom::blink::ResourceTimingInfo info;
    info.allow_timing_details = false;
    PerformanceResourceTiming* timing = MakePerformanceResourceTiming(info);
    return timing->GetNextHopProtocol(alpn_negotiated_protocol,
                                      connection_info);
  }

  void Initialize(ScriptState* script_state) { script_state_ = script_state; }

  ScriptState* GetScriptState() { return script_state_; }

 private:
  PerformanceResourceTiming* MakePerformanceResourceTiming(
      const mojom::blink::ResourceTimingInfo& info) {
    std::unique_ptr<DummyPageHolder> dummy_page_holder =
        std::make_unique<DummyPageHolder>();
    return MakeGarbageCollected<PerformanceResourceTiming>(
        info, base::TimeTicks(),
        dummy_page_holder->GetDocument()
            .GetExecutionContext()
            ->CrossOriginIsolatedCapability(),
        /*initiator_type=*/"", LocalDOMWindow::From(GetScriptState()));
  }

  Persistent<ScriptState> script_state_;
};

TEST_F(PerformanceResourceTimingTest,
       TestFallbackToConnectionInfoWhenALPNUnknown) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  AtomicString connection_info = "http/1.1";
  AtomicString alpn_negotiated_protocol = "unknown";
  EXPECT_EQ(GetNextHopProtocol(alpn_negotiated_protocol, connection_info),
            connection_info);
}

TEST_F(PerformanceResourceTimingTest,
       TestFallbackToHTTPInfoWhenALPNAndConnectionInfoUnknown) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  AtomicString connection_info = "unknown";
  AtomicString alpn_negotiated_protocol = "unknown";
  EXPECT_EQ(GetNextHopProtocol(alpn_negotiated_protocol, connection_info), "");
}

TEST_F(PerformanceResourceTimingTest, TestNoChangeWhenContainsQuic) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  AtomicString connection_info = "http/1.1";
  AtomicString alpn_negotiated_protocol = "http/2+quic/39";
  EXPECT_EQ(GetNextHopProtocol(alpn_negotiated_protocol, connection_info),
            alpn_negotiated_protocol);
}

TEST_F(PerformanceResourceTimingTest, TestNoChangeWhenOtherwise) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  AtomicString connection_info = "http/1.1";
  AtomicString alpn_negotiated_protocol = "RandomProtocol";
  EXPECT_EQ(GetNextHopProtocol(alpn_negotiated_protocol, connection_info),
            alpn_negotiated_protocol);
}

TEST_F(PerformanceResourceTimingTest, TestNextHopProtocolIsGuardedByTao) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  AtomicString connection_info = "http/1.1";
  AtomicString alpn_negotiated_protocol = "RandomProtocol";
  EXPECT_EQ(
      GetNextHopProtocolWithoutTao(alpn_negotiated_protocol, connection_info),
      "");
}

}  // namespace blink
