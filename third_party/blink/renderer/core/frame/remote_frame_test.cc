// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/remote_frame.h"

#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "cc/layers/layer.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/remote_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/frame/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/testing/fake_remote_frame_host.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

using blink::test::RunPendingTasks;

class RemoteFrameTest : public PageTestBase {
 protected:
  cc::Layer* RemoteFrameLayerInLastPaint(LocalFrameView& main_view) {
    for (const auto& item :
         main_view.GetPaintControllerPersistentDataForTesting()
             .GetDisplayItemList()) {
      if (item.GetType() == DisplayItem::kForeignLayerRemoteFrame) {
        return To<ForeignLayerDisplayItem>(item).GetLayer();
      }
    }
    return nullptr;
  }

  void InitializeWithHTML(LocalFrame& frame, const String& html_content) {
    frame.GetDocument()->body()->SetInnerHTMLWithoutTrustedTypes(html_content);
    frame.LocalFrameRoot().View()->UpdateAllLifecyclePhasesForTest();
  }
};

TEST_F(RemoteFrameTest, SurfaceNotInvalidatedBeforePaintOnScroll) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();

  auto* web_view = web_view_helper.GetWebView();
  web_view->Resize(gfx::Size(800, 600));
  InitializeWithHTML(*web_view->MainFrameImpl()->GetFrame(), R"HTML(
    <!DOCTYPE html>
    <iframe style="height: 2000px; border: none;"></iframe>
  )HTML");

  auto* remote_frame_impl = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(
      web_view_helper.LocalMainFrame()->FirstChild(), remote_frame_impl);
  auto* remote_frame = remote_frame_impl->GetFrame();
  remote_frame->SetReplicatedOrigin(
      WebSecurityOrigin(SecurityOrigin::CreateUniqueOpaque()), false);
  // Let the remote frame create surface layers.
  remote_frame->SetFrameSinkId(viz::FrameSinkId(1, 1),
                               /*allow_paint_holding=*/false);

  auto* main_view = web_view->MainFrameImpl()->GetFrame()->View();

  // Two lifecycles to compute and propagate the initial compositing rect.
  main_view->UpdateAllLifecyclePhasesForTest();
  main_view->UpdateAllLifecyclePhasesForTest();
  RunPendingTasks();

  auto* remote_frame_view = remote_frame->View();
  ASSERT_FALSE(remote_frame_view->NeedsFrameRectPropagation());
  gfx::Rect initial_compositing_rect = remote_frame_view->GetCompositingRect();
  EXPECT_EQ(
      initial_compositing_rect,
      remote_frame->GetPendingVisualPropertiesForTesting().compositor_viewport);
  cc::Layer* layer_before_scroll = remote_frame->GetCcLayer().get();
  ASSERT_TRUE(layer_before_scroll);
  EXPECT_EQ(layer_before_scroll, RemoteFrameLayerInLastPaint(*main_view));

  // Scroll the page so the iframe's compositing rect, changes.
  web_view->MainFrameImpl()->SetScrollOffset(gfx::PointF(0, 500));
  main_view->UpdateAllLifecyclePhasesForTest();
  RunPendingTasks();

  gfx::Rect new_compositing_rect = remote_frame_view->GetCompositingRect();
  EXPECT_NE(initial_compositing_rect, new_compositing_rect);
  EXPECT_EQ(
      new_compositing_rect,
      remote_frame->GetPendingVisualPropertiesForTesting().compositor_viewport);
  EXPECT_EQ(layer_before_scroll, RemoteFrameLayerInLastPaint(*main_view));
  EXPECT_NE(layer_before_scroll, remote_frame->GetCcLayer());
  cc::Layer* layer_after_scroll = remote_frame->GetCcLayer().get();

  EXPECT_TRUE(remote_frame_view->NeedsFrameRectPropagation());
  main_view->UpdateAllLifecyclePhasesForTest();
  RunPendingTasks();
  EXPECT_FALSE(remote_frame_view->NeedsFrameRectPropagation());
  EXPECT_EQ(layer_after_scroll, RemoteFrameLayerInLastPaint(*main_view));
  EXPECT_EQ(layer_after_scroll, remote_frame->GetCcLayer().get());
}

TEST_F(RemoteFrameTest,
       NestedLocalFramesSurfaceNotInvalidatedBeforePaintOnScroll) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();

  auto* web_view = web_view_helper.GetWebView();
  web_view->Resize(gfx::Size(800, 600));
  InitializeWithHTML(*web_view->MainFrameImpl()->GetFrame(), R"HTML(
    <!DOCTYPE html>
    <iframe style="height: 2000px; border: none;"></iframe>
  )HTML");

  auto* outer_frame =
      To<WebLocalFrameImpl>(web_view_helper.LocalMainFrame()->FirstChild());
  InitializeWithHTML(*outer_frame->GetFrame(), R"HTML(
    <!DOCTYPE html>
    <iframe style="height: 2000px; border: none;"></iframe>
  )HTML");

  auto* middle_frame = To<WebLocalFrameImpl>(outer_frame->FirstChild());
  InitializeWithHTML(*middle_frame->GetFrame(), R"HTML(
    <!DOCTYPE html>
    <iframe style="height: 2000px; border: none;"></iframe>
  )HTML");

  auto* remote_frame_impl = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(middle_frame->FirstChild(),
                                      remote_frame_impl);
  auto* remote_frame = remote_frame_impl->GetFrame();
  remote_frame->SetReplicatedOrigin(
      WebSecurityOrigin(SecurityOrigin::CreateUniqueOpaque()), false);
  // Let the remote frame create surface layers.
  remote_frame->SetFrameSinkId(viz::FrameSinkId(1, 1),
                               /*allow_paint_holding=*/false);

  auto* main_view = web_view->MainFrameImpl()->GetFrame()->View();

  // Two lifecycles to compute and propagate the initial compositing rect.
  main_view->UpdateAllLifecyclePhasesForTest();
  main_view->UpdateAllLifecyclePhasesForTest();
  RunPendingTasks();

  auto* remote_frame_view = remote_frame->View();
  ASSERT_FALSE(remote_frame_view->NeedsFrameRectPropagation());
  gfx::Rect initial_compositing_rect = remote_frame_view->GetCompositingRect();
  EXPECT_EQ(
      initial_compositing_rect,
      remote_frame->GetPendingVisualPropertiesForTesting().compositor_viewport);
  cc::Layer* layer_before_scroll = remote_frame->GetCcLayer().get();
  ASSERT_TRUE(layer_before_scroll);
  EXPECT_EQ(layer_before_scroll, RemoteFrameLayerInLastPaint(*main_view));

  // Scroll the page so the nested iframe's compositing rect changes.
  web_view->MainFrameImpl()->SetScrollOffset(gfx::PointF(0, 500));
  main_view->UpdateAllLifecyclePhasesForTest();
  RunPendingTasks();

  gfx::Rect new_compositing_rect = remote_frame_view->GetCompositingRect();
  EXPECT_NE(initial_compositing_rect, new_compositing_rect);
  EXPECT_EQ(
      new_compositing_rect,
      remote_frame->GetPendingVisualPropertiesForTesting().compositor_viewport);
  EXPECT_EQ(layer_before_scroll, RemoteFrameLayerInLastPaint(*main_view));
  EXPECT_NE(layer_before_scroll, remote_frame->GetCcLayer());
  cc::Layer* layer_after_scroll = remote_frame->GetCcLayer().get();

  EXPECT_TRUE(remote_frame_view->NeedsFrameRectPropagation());
  main_view->UpdateAllLifecyclePhasesForTest();
  RunPendingTasks();
  EXPECT_FALSE(remote_frame_view->NeedsFrameRectPropagation());
  EXPECT_EQ(layer_after_scroll, RemoteFrameLayerInLastPaint(*main_view));
  EXPECT_EQ(layer_after_scroll, remote_frame->GetCcLayer().get());
}

namespace {

// Intercepts mojom::blink::RemoteFrameHost calls in unit tests to count how
// many CheckCompleted() IPCs are sent to the browser process.
class CheckCompletedCountingRemoteFrameHost : public FakeRemoteFrameHost {
 public:
  CheckCompletedCountingRemoteFrameHost() = default;
  ~CheckCompletedCountingRemoteFrameHost() override = default;

  // FakeRemoteFrameHost:
  void CheckCompleted() override { ++check_completed_count_; }

  size_t check_completed_count() const { return check_completed_count_; }

 private:
  size_t check_completed_count_ = 0;
};

// Attaches a child RemoteFrame to `parent` bound to `host` so tests can inspect
// RemoteFrameHost IPCs sent on `parent` or on the newly created child.
WebRemoteFrameImpl* CreateRemoteChildWithHost(
    WebRemoteFrameImpl& parent,
    CheckCompletedCountingRemoteFrameHost& host,
    bool is_loading) {
  auto replicated_state = mojom::blink::FrameReplicationState::New();
  replicated_state->origin = SecurityOrigin::CreateUniqueOpaque();
  return parent.CreateRemoteChild(
      mojom::blink::TreeScopeType::kDocument, RemoteFrameToken(), is_loading,
      base::UnguessableToken::Create(), /*opener=*/nullptr,
      host.BindNewAssociatedRemote(),
      mojo::AssociatedRemote<mojom::blink::RemoteFrame>()
          .BindNewEndpointAndPassDedicatedReceiver(),
      std::move(replicated_state), mojom::blink::FrameOwnerProperties::New());
}

// Base helper initialized before SimTest / TaskEnvironment so ScopedFeatureList
// is configured prior to worker thread creation.
class ScopedCheckCompletedFeatureState {
 protected:
  explicit ScopedCheckCompletedFeatureState(bool enabled) {
    scoped_feature_list_.InitWithFeatureState(
        features::kSkipRedundantRemoteFrameCheckCompleted, enabled);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class RemoteFrameDidStopLoadingTest : public ScopedCheckCompletedFeatureState,
                                      public ::testing::TestWithParam<bool> {
 protected:
  RemoteFrameDidStopLoadingTest()
      : ScopedCheckCompletedFeatureState(/*enabled=*/GetParam()) {}

 private:
  test::TaskEnvironment task_environment_;
};

class RemoteFrameDidStopLoadingSimTest
    : public ScopedCheckCompletedFeatureState,
      public SimTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  RemoteFrameDidStopLoadingSimTest()
      : ScopedCheckCompletedFeatureState(/*enabled=*/GetParam()) {}
};

}  // namespace

// Verifies behavior in a renderer process that holds only RemoteFrame proxies
// along the ancestor chain (such as a sibling OOPIF's renderer process).
// When the feature is enabled, RemoteFrame::DidStopLoading() updates the
// proxy's loading state without sending a redundant RemoteFrameHost IPC.
TEST_P(RemoteFrameDidStopLoadingTest, NonAncestorProcessRemoteParent) {
  CheckCompletedCountingRemoteFrameHost root_host;
  CheckCompletedCountingRemoteFrameHost child_host;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeRemoteWithOpenerAndAssociatedRemoteAndReceivers(
      /*opener=*/nullptr, SecurityOrigin::CreateUniqueOpaque(),
      /*web_view_client=*/nullptr, root_host.BindNewAssociatedRemote(),
      mojo::AssociatedRemote<mojom::blink::RemoteFrame>()
          .BindNewEndpointAndPassDedicatedReceiver());

  WebRemoteFrameImpl* remote_root = web_view_helper.RemoteMainFrame();
  WebRemoteFrameImpl* remote_child =
      CreateRemoteChildWithHost(*remote_root, child_host, /*is_loading=*/true);
  ASSERT_TRUE(remote_child->GetFrame()->IsLoading());

  // Stopping load on a root RemoteFrame (no parent) sends no IPC in any mode.
  remote_root->GetFrame()->DidStartLoading();
  remote_root->GetFrame()->DidStopLoading();
  EXPECT_FALSE(remote_root->GetFrame()->IsLoading());

  // Stopping load on the child RemoteFrame whose parent is also a RemoteFrame
  // skips the CheckCompleted() IPC when enabled and sends it when disabled.
  remote_child->GetFrame()->DidStopLoading();
  EXPECT_FALSE(remote_child->GetFrame()->IsLoading());

  root_host.FlushForTesting();
  child_host.FlushForTesting();
  EXPECT_EQ(GetParam() ? 0u : 1u, root_host.check_completed_count());
  EXPECT_EQ(0u, child_host.check_completed_count());

  web_view_helper.Reset();
}

INSTANTIATE_TEST_SUITE_P(, RemoteFrameDidStopLoadingTest, ::testing::Bool());

// Verifies that when the immediate parent is a LocalFrame, DidStopLoading()
// completes the parent Document's load and dispatches the window load event
// identically whether the feature is enabled or disabled, without sending any
// RemoteFrameHost::CheckCompleted() IPC.
TEST_P(RemoteFrameDidStopLoadingSimTest, DirectLocalParentCompletesLoad) {
  CheckCompletedCountingRemoteFrameHost child_host;

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Write(R"HTML(
    <!DOCTYPE html>
    <div id="status">loading</div>
    <script>
      window.addEventListener('load', () => {
        document.getElementById('status').textContent = 'loaded';
      });
    </script>
    <iframe></iframe>
  )HTML");

  // Swap the subframe to a RemoteFrame that is still loading before the main
  // Document finishes parsing so the main Document blocks completion on it.
  WebRemoteFrameImpl* remote_child = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(MainFrame().FirstChild(), remote_child,
                                      child_host.BindNewAssociatedRemote());
  remote_child->GetFrame()->DidStartLoading();

  main_resource.Finish();

  // The main Document has finished parsing, but remains loading because its
  // RemoteFrame child has not finished loading yet.
  EXPECT_TRUE(GetDocument().GetFrame()->IsLoading());
  EXPECT_FALSE(GetDocument().IsLoadCompleted());
  EXPECT_EQ("loading", GetElementById("status")->textContent());

  // Notifying DidStopLoading() on the direct child completes the LocalFrame
  // parent in-process.
  remote_child->GetFrame()->DidStopLoading();
  EXPECT_FALSE(remote_child->GetFrame()->IsLoading());
  EXPECT_FALSE(GetDocument().GetFrame()->IsLoading());
  EXPECT_TRUE(GetDocument().IsLoadCompleted());
  EXPECT_EQ("loaded", GetElementById("status")->textContent());

  child_host.FlushForTesting();
  EXPECT_EQ(0u, child_host.check_completed_count());

  WebViewHelper().Reset();
}

// Verifies a 3-level frame tree (LocalFrame A -> RemoteFrame B -> RemoteFrame
// C) where B has already finished loading earlier and A's Document is waiting
// solely on grandchild C in AllDescendantsAreComplete(). When enabled, C's
// DidStopLoading() walks past RemoteFrame B to invoke LocalFrame A's
// CheckCompleted() in-process, completing A's load without sending a
// RemoteFrameHost::CheckCompleted() IPC on B.
TEST_P(RemoteFrameDidStopLoadingSimTest,
       NestedRemoteGrandchildCompletesLocalAncestorWhenMiddleComplete) {
  CheckCompletedCountingRemoteFrameHost middle_host;
  CheckCompletedCountingRemoteFrameHost grandchild_host;

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Write(R"HTML(
    <!DOCTYPE html>
    <div id="status">loading</div>
    <script>
      window.addEventListener('load', () => {
        document.getElementById('status').textContent = 'loaded';
      });
    </script>
    <iframe></iframe>
  )HTML");

  // Middle RemoteFrame B is already not loading (`IsLoading() == false`), while
  // nested grandchild RemoteFrame C is still loading (`is_loading = true`).
  WebRemoteFrameImpl* remote_middle = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(MainFrame().FirstChild(), remote_middle,
                                      middle_host.BindNewAssociatedRemote());
  ASSERT_FALSE(remote_middle->GetFrame()->IsLoading());

  WebRemoteFrameImpl* remote_grandchild = CreateRemoteChildWithHost(
      *remote_middle, grandchild_host, /*is_loading=*/true);
  ASSERT_TRUE(remote_grandchild->GetFrame()->IsLoading());

  main_resource.Finish();

  // AllDescendantsAreComplete() traverses all descendants, so LocalFrame A must
  // wait for grandchild C even though direct child B is not loading.
  EXPECT_TRUE(GetDocument().GetFrame()->IsLoading());
  EXPECT_FALSE(GetDocument().IsLoadCompleted());
  EXPECT_EQ("loading", GetElementById("status")->textContent());

  remote_grandchild->GetFrame()->DidStopLoading();
  EXPECT_FALSE(remote_grandchild->GetFrame()->IsLoading());

  middle_host.FlushForTesting();
  grandchild_host.FlushForTesting();
  EXPECT_EQ(0u, grandchild_host.check_completed_count());

  if (GetParam()) {
    // With the optimization enabled, LocalFrame A completes immediately via the
    // in-process ancestor walk and sends zero CheckCompleted() IPCs on B.
    EXPECT_FALSE(GetDocument().GetFrame()->IsLoading());
    EXPECT_TRUE(GetDocument().IsLoadCompleted());
    EXPECT_EQ("loaded", GetElementById("status")->textContent());
    EXPECT_EQ(0u, middle_host.check_completed_count());
  } else {
    // With the optimization disabled, C sends CheckCompleted() on B's
    // RemoteFrameHost (which in a full browser would bounce through B's process
    // back to A).
    EXPECT_EQ(1u, middle_host.check_completed_count());
  }

  WebViewHelper().Reset();
}

// Verifies that in a 3-level frame tree (LocalFrame A -> RemoteFrame B ->
// RemoteFrame C) where BOTH B and C are still loading, C's DidStopLoading()
// does not prematurely complete LocalFrame A while B is still loading; A only
// completes once B also stops loading.
TEST_P(RemoteFrameDidStopLoadingSimTest,
       NestedRemoteGrandchildWaitsForLoadingMiddleParent) {
  CheckCompletedCountingRemoteFrameHost middle_host;
  CheckCompletedCountingRemoteFrameHost grandchild_host;

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Write(R"HTML(
    <!DOCTYPE html>
    <div id="status">loading</div>
    <script>
      window.addEventListener('load', () => {
        document.getElementById('status').textContent = 'loaded';
      });
    </script>
    <iframe></iframe>
  )HTML");

  WebRemoteFrameImpl* remote_middle = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(MainFrame().FirstChild(), remote_middle,
                                      middle_host.BindNewAssociatedRemote());
  remote_middle->GetFrame()->DidStartLoading();

  WebRemoteFrameImpl* remote_grandchild = CreateRemoteChildWithHost(
      *remote_middle, grandchild_host, /*is_loading=*/true);

  main_resource.Finish();

  EXPECT_TRUE(GetDocument().GetFrame()->IsLoading());
  EXPECT_FALSE(GetDocument().IsLoadCompleted());

  // Grandchild C finishes first while middle frame B is still loading.
  // LocalFrame A must remain loading in both modes.
  remote_grandchild->GetFrame()->DidStopLoading();
  EXPECT_FALSE(remote_grandchild->GetFrame()->IsLoading());
  EXPECT_TRUE(remote_middle->GetFrame()->IsLoading());
  EXPECT_TRUE(GetDocument().GetFrame()->IsLoading());
  EXPECT_FALSE(GetDocument().IsLoadCompleted());
  EXPECT_EQ("loading", GetElementById("status")->textContent());

  middle_host.FlushForTesting();
  EXPECT_EQ(GetParam() ? 0u : 1u, middle_host.check_completed_count());

  // Once middle frame B finishes loading, LocalFrame A completes its load.
  remote_middle->GetFrame()->DidStopLoading();
  EXPECT_FALSE(remote_middle->GetFrame()->IsLoading());
  EXPECT_FALSE(GetDocument().GetFrame()->IsLoading());
  EXPECT_TRUE(GetDocument().IsLoadCompleted());
  EXPECT_EQ("loaded", GetElementById("status")->textContent());

  middle_host.FlushForTesting();
  EXPECT_EQ(GetParam() ? 0u : 1u, middle_host.check_completed_count());

  WebViewHelper().Reset();
}

INSTANTIATE_TEST_SUITE_P(, RemoteFrameDidStopLoadingSimTest, ::testing::Bool());

}  // namespace blink
