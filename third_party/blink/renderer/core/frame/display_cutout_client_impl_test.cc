// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/display_cutout_client_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom-blink.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_storage.h"
#include "third_party/blink/renderer/core/inspector/inspector_network_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_session_state.h"
#include "third_party/blink/renderer/core/inspector/protocol/audits.h"
#include "third_party/blink/renderer/core/inspector/protocol/protocol.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "ui/gfx/geometry/insets.h"

namespace blink {
namespace {

class DisplayCutoutClientImplTest : public SimTest {
 protected:
  class DummyFrontendChannel : public protocol::FrontendChannel {
   public:
    void SendProtocolResponse(
        int call_id,
        std::unique_ptr<protocol::Serializable> message) override {}
    void SendProtocolNotification(
        std::unique_ptr<protocol::Serializable> message) override {}
    void FlushProtocolNotifications() override {}
  };

  void SetUp() override {
    SimTest::SetUp();
    LocalFrame* frame = GetDocument().GetFrame();
    auto* inspected_frames = MakeGarbageCollected<InspectedFrames>(frame);
    auto* network_agent = MakeGarbageCollected<InspectorNetworkAgent>(
        inspected_frames, /*worker_or_worklet_global_scope=*/nullptr,
        /*client_is_trusted=*/true);
    audits_agent_ = MakeGarbageCollected<InspectorAuditsAgent>(
        network_agent, &GetDocument().GetPage()->GetInspectorIssueStorage(),
        inspected_frames, /*web_autofill_client=*/nullptr);
    dispatcher_ = std::make_unique<protocol::UberDispatcher>(&channel_);
    session_state_ = std::make_unique<InspectorSessionState>(
        mojom::blink::DevToolsSessionState::New());
    audits_agent_->Init(frame->GetProbeSink(), dispatcher_.get(),
                        session_state_.get(), V8SessionHolder(),
                        ClientIsTrusted(true));
    audits_agent_->enable();
  }

  void TearDown() override {
    if (audits_agent_) {
      audits_agent_->disable();
      audits_agent_->Dispose();
    }
    SimTest::TearDown();
  }

  DummyFrontendChannel channel_;
  std::unique_ptr<protocol::UberDispatcher> dispatcher_;
  std::unique_ptr<InspectorSessionState> session_state_;
  Persistent<InspectorAuditsAgent> audits_agent_;

  void ForceViewportFitCover() {
    ViewportDescription description =
        GetDocument().GetViewportData().GetViewportDescription();
    description.SetViewportFit(mojom::ViewportFit::kCover);
    GetDocument().GetViewportData().SetViewportDescription(description);
  }

  bool HasViewportFitCoverMissingSafeAreaInsetTopIssue() {
    auto& storage = GetDocument().GetPage()->GetInspectorIssueStorage();
    for (wtf_size_t i = 0; i < storage.size(); ++i) {
      auto* issue = storage.at(i);
      if (issue->getCode() ==
          protocol::Audits::InspectorIssueCodeEnum::
              ViewportFitCoverMissingSafeAreaInsetTopIssue) {
        return true;
      }
    }
    return false;
  }
};

TEST_F(DisplayCutoutClientImplTest, FiresWhenFixedHeaderOccludesTopInset) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      header { position: fixed; top: 0; left: 0; right: 0; height: 40px; }
    </style>
    <header>app</header>
    <div style="height: 4000px"></div>
  )HTML");
  ForceViewportFitCover();
  Compositor().BeginFrame();

  DisplayCutoutClientImpl::MaybeReportMissingSafeAreaInsetTopIssue(
      GetDocument().GetFrame(), gfx::Insets().set_top(24));
  EXPECT_TRUE(HasViewportFitCoverMissingSafeAreaInsetTopIssue());
}

TEST_F(DisplayCutoutClientImplTest, DoesNotFireWhenCssConsumesSafeAreaTop) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      header {
        position: fixed; top: 0; left: 0; right: 0; height: 40px;
        padding-top: env(safe-area-inset-top);
      }
    </style>
    <header>app</header>
    <div style="height: 4000px"></div>
  )HTML");
  ForceViewportFitCover();
  Compositor().BeginFrame();

  DisplayCutoutClientImpl::MaybeReportMissingSafeAreaInsetTopIssue(
      GetDocument().GetFrame(), gfx::Insets().set_top(24));
  EXPECT_FALSE(HasViewportFitCoverMissingSafeAreaInsetTopIssue());
}

TEST_F(DisplayCutoutClientImplTest, DoesNotFireForStickyWithoutTopConstraint) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <body style="margin: 0">
    <div style="position: sticky; height: 40px">scrolls away</div>
    <div style="height: 4000px"></div>
    </body>
  )HTML");
  ForceViewportFitCover();
  Compositor().BeginFrame();

  DisplayCutoutClientImpl::MaybeReportMissingSafeAreaInsetTopIssue(
      GetDocument().GetFrame(), gfx::Insets().set_top(24));
  EXPECT_FALSE(HasViewportFitCoverMissingSafeAreaInsetTopIssue());
}

TEST_F(DisplayCutoutClientImplTest, DoesNotFireForInvisibleOverlays) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      .hidden { position: fixed; top: 0; left: 0; right: 0; height: 40px;
                visibility: hidden; }
      .clear { position: fixed; top: 0; left: 0; right: 0; height: 40px;
               opacity: 0; }
    </style>
    <div class="hidden">overlay</div>
    <div class="clear">overlay</div>
    <div style="margin-top: 200px; height: 4000px">content</div>
  )HTML");
  ForceViewportFitCover();
  Compositor().BeginFrame();

  DisplayCutoutClientImpl::MaybeReportMissingSafeAreaInsetTopIssue(
      GetDocument().GetFrame(), gfx::Insets().set_top(24));
  EXPECT_FALSE(HasViewportFitCoverMissingSafeAreaInsetTopIssue());
}

TEST_F(DisplayCutoutClientImplTest, FiresWhenOnlyMediaControlsUseSafeAreaTop) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      header { position: fixed; top: 0; left: 0; right: 0; height: 40px; }
    </style>
    <header>app</header>
    <video controls></video>
    <div style="height: 4000px"></div>
  )HTML");
  ForceViewportFitCover();
  Compositor().BeginFrame();

  DisplayCutoutClientImpl::MaybeReportMissingSafeAreaInsetTopIssue(
      GetDocument().GetFrame(), gfx::Insets().set_top(24));
  EXPECT_TRUE(HasViewportFitCoverMissingSafeAreaInsetTopIssue());
}

TEST_F(DisplayCutoutClientImplTest, DoesNotFireWithoutViewportFitCover) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <header style="position: fixed; top: 0; height: 40px">app</header>
  )HTML");
  Compositor().BeginFrame();

  DisplayCutoutClientImpl::MaybeReportMissingSafeAreaInsetTopIssue(
      GetDocument().GetFrame(), gfx::Insets().set_top(24));
  EXPECT_FALSE(HasViewportFitCoverMissingSafeAreaInsetTopIssue());
}

TEST_F(DisplayCutoutClientImplTest, DoesNotFireWhenNoTopInset) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <header style="position: fixed; top: 0; height: 40px">app</header>
  )HTML");
  ForceViewportFitCover();
  Compositor().BeginFrame();

  DisplayCutoutClientImpl::MaybeReportMissingSafeAreaInsetTopIssue(
      GetDocument().GetFrame(), gfx::Insets().set_bottom(24));
  EXPECT_FALSE(HasViewportFitCoverMissingSafeAreaInsetTopIssue());
}

TEST_F(DisplayCutoutClientImplTest, DoesNotFireWhenNoOcclusionCandidate) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <div style="margin-top: 200px; height: 4000px">content</div>
  )HTML");
  ForceViewportFitCover();
  Compositor().BeginFrame();

  DisplayCutoutClientImpl::MaybeReportMissingSafeAreaInsetTopIssue(
      GetDocument().GetFrame(), gfx::Insets().set_top(24));
  EXPECT_FALSE(HasViewportFitCoverMissingSafeAreaInsetTopIssue());
}

TEST_F(DisplayCutoutClientImplTest,
       FiresForNonScrollableViewportWithHeaderInInsetBand) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <body style="margin: 0">
    <h1 style="margin: 0; height: 30px">Title</h1>
    </body>
  )HTML");
  ForceViewportFitCover();
  Compositor().BeginFrame();

  DisplayCutoutClientImpl::MaybeReportMissingSafeAreaInsetTopIssue(
      GetDocument().GetFrame(), gfx::Insets().set_top(24));
  EXPECT_TRUE(HasViewportFitCoverMissingSafeAreaInsetTopIssue());
}

}  // namespace
}  // namespace blink
