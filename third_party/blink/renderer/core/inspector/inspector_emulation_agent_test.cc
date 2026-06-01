// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_emulation_agent.h"

#include "base/feature_list.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/inspector/inspector_session_state.h"
#include "third_party/blink/renderer/core/inspector/protocol/protocol.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_cpu_throttler.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/inspector_protocol/crdtp/span.h"

namespace blink {

namespace {

String BuildExpectedAcceptHeader(bool include_webp, bool include_avif) {
  StringBuilder sb;
#if BUILDFLAG(ENABLE_JXL_DECODER)
  if (base::FeatureList::IsEnabled(blink::features::kJXLImageFormat)) {
    sb.Append("image/jxl,");
  }
#endif
#if BUILDFLAG(ENABLE_DAV1D_DECODER)
  if (include_avif) {
    sb.Append("image/avif,");
  }
#endif
  if (include_webp) {
    sb.Append("image/webp,");
  }
  sb.Append("image/apng,image/svg+xml,image/*,*/*;q=0.8");
  return sb.ToString();
}

class DummyFrontendChannel : public protocol::FrontendChannel {
 public:
  void SendProtocolResponse(
      int call_id,
      std::unique_ptr<protocol::Serializable> message) override {}
  void SendProtocolNotification(
      std::unique_ptr<protocol::Serializable> message) override {}
  void FlushProtocolNotifications() override {}
};

}  // namespace

class InspectorEmulationAgentTest : public testing::Test {};

TEST_F(InspectorEmulationAgentTest, ModifiesAcceptHeader) {
  HashSet<String> disabled_types;

  EXPECT_EQ(InspectorEmulationAgent::OverrideAcceptImageHeader(&disabled_types),
            BuildExpectedAcceptHeader(/*include_webp=*/true,
                                      /*include_avif=*/true));

  disabled_types.insert("image/webp");
  EXPECT_EQ(InspectorEmulationAgent::OverrideAcceptImageHeader(&disabled_types),
            BuildExpectedAcceptHeader(/*include_webp=*/false,
                                      /*include_avif=*/true));

  disabled_types.insert("image/avif");
  EXPECT_EQ(InspectorEmulationAgent::OverrideAcceptImageHeader(&disabled_types),
            BuildExpectedAcceptHeader(/*include_webp=*/false,
                                      /*include_avif=*/false));

  disabled_types.erase("image/webp");
  EXPECT_EQ(InspectorEmulationAgent::OverrideAcceptImageHeader(&disabled_types),
            BuildExpectedAcceptHeader(/*include_webp=*/true,
                                      /*include_avif=*/false));
}

// Fuchsia does not support CPU throttling.
#if !BUILDFLAG(IS_FUCHSIA)
TEST_F(InspectorEmulationAgentTest, MultiSessionCPUThrottlingRestoreTest) {
  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper helper_;
  WebViewImpl* web_view = helper_.Initialize();
  WebLocalFrameImpl* web_frame = web_view->MainFrameImpl();
  LocalFrame* frame = web_frame->GetFrame();
  auto* virtual_time_controller =
      web_view->Scheduler()->GetVirtualTimeController();

  DummyFrontendChannel channel_a;
  protocol::UberDispatcher dispatcher_a(&channel_a);
  auto reattach_state_a = mojom::blink::DevToolsSessionState::New();
  InspectorSessionState session_state_a(std::move(reattach_state_a));

  auto* agent_a = MakeGarbageCollected<InspectorEmulationAgent>(
      web_frame, *virtual_time_controller);
  agent_a->Init(frame->GetProbeSink(), &dispatcher_a, &session_state_a);

  DummyFrontendChannel channel_b;
  protocol::UberDispatcher dispatcher_b(&channel_b);
  auto reattach_state_b = mojom::blink::DevToolsSessionState::New();
  InspectorSessionState session_state_b(std::move(reattach_state_b));

  auto* agent_b = MakeGarbageCollected<InspectorEmulationAgent>(
      web_frame, *virtual_time_controller);
  agent_b->Init(frame->GetProbeSink(), &dispatcher_b, &session_state_b);

  // Initially, throttling thread should not exist (no throttling active).
  EXPECT_DOUBLE_EQ(blink::scheduler::ThreadCPUThrottler::GetInstance()
                       ->GetThrottlingRateForTesting(),
                   1.0);

  // Session A sets CPU throttling rate to 4.0.
  agent_a->setCPUThrottlingRate(4.0);

  // Throttling thread must now be running target-wide at 400%.
  EXPECT_DOUBLE_EQ(blink::scheduler::ThreadCPUThrottler::GetInstance()
                       ->GetThrottlingRateForTesting(),
                   4.0);

  // Since agent_b has the default CPU throttling rate of 1.0, disable() should
  // NOT reset the global throttling state.
  agent_b->disable();

  // Throttling state target-wide must remain active at 400% (from Session A)!
  EXPECT_DOUBLE_EQ(blink::scheduler::ThreadCPUThrottler::GetInstance()
                       ->GetThrottlingRateForTesting(),
                   4.0);

  // Similarly, Restore() should NOT reset the global throttling state.
  agent_b->Restore();

  // Throttling state target-wide must remain active at 400% (from Session A)!
  EXPECT_DOUBLE_EQ(blink::scheduler::ThreadCPUThrottler::GetInstance()
                       ->GetThrottlingRateForTesting(),
                   4.0);

  // Disable agent_a. Throttler thread is destroyed since A's disable resets
  // rate to 1.
  agent_a->disable();
  EXPECT_DOUBLE_EQ(blink::scheduler::ThreadCPUThrottler::GetInstance()
                       ->GetThrottlingRateForTesting(),
                   1.0);

  agent_a->Dispose();
  agent_b->Dispose();
  helper_.Reset();
}
#endif

}  // namespace blink
