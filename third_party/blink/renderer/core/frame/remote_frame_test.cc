// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/remote_frame.h"

#include "cc/layers/layer.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/remote_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

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

}  // namespace blink
