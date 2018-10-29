/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/core/paint/link_highlight_impl.h"

#include <memory>

#include "cc/trees/layer_tree_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/link_highlights.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_timeline.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

class LinkHighlightImplTest : public testing::Test,
                              public testing::WithParamInterface<bool>,
                              private ScopedBlinkGenPropertyTreesForTest {
 public:
  LinkHighlightImplTest() : ScopedBlinkGenPropertyTreesForTest(GetParam()) {}

 protected:
  GestureEventWithHitTestResults GetTargetedEvent(
      WebGestureEvent& touch_event) {
    WebGestureEvent scaled_event = TransformWebGestureEvent(
        web_view_helper_.GetWebView()->MainFrameImpl()->GetFrameView(),
        touch_event);
    return web_view_helper_.GetWebView()
        ->GetPage()
        ->DeprecatedLocalMainFrame()
        ->GetEventHandler()
        .TargetGestureEvent(scaled_event, true);
  }

  void SetUp() override {
    WebURL url = url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8("http://www.test.com/"), test::CoreTestDataPath(),
        WebString::FromUTF8("test_touch_link_highlight.html"));
    web_view_helper_.InitializeAndLoad(url.GetString().Utf8());
  }

  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();

    // Ensure we fully clean up while scoped settings are enabled. Without this,
    // garbage collection would occur after ScopedBlinkGenPropertyTreesForTest
    // is out of scope, so the settings would not apply in some destructors.
    web_view_helper_.Reset();
    ThreadState::Current()->CollectAllGarbage();
  }

  size_t ContentLayerCount() {
    // paint_artifact_compositor()->EnableExtraDataForTesting() should be called
    // before using this function.
    DCHECK(paint_artifact_compositor()->GetExtraDataForTesting());
    return paint_artifact_compositor()
        ->GetExtraDataForTesting()
        ->content_layers.size();
  }

  PaintArtifactCompositor* paint_artifact_compositor() {
    auto* local_frame_view = web_view_helper_.LocalMainFrame()->GetFrameView();
    return local_frame_view->GetPaintArtifactCompositorForTesting();
  }

  frame_test_helpers::WebViewHelper web_view_helper_;
};

INSTANTIATE_TEST_CASE_P(All, LinkHighlightImplTest, testing::Bool());

TEST_P(LinkHighlightImplTest, verifyWebViewImplIntegration) {
  WebViewImpl* web_view_impl = web_view_helper_.GetWebView();
  int page_width = 640;
  int page_height = 480;
  web_view_impl->Resize(WebSize(page_width, page_height));
  web_view_impl->UpdateAllLifecyclePhases();

  WebGestureEvent touch_event(WebInputEvent::kGestureShowPress,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests(),
                              kWebGestureDeviceTouchscreen);

  // The coordinates below are linked to absolute positions in the referenced
  // .html file.
  touch_event.SetPositionInWidget(WebFloatPoint(20, 20));

  ASSERT_TRUE(web_view_impl->BestTapNode(GetTargetedEvent(touch_event)));

  touch_event.SetPositionInWidget(WebFloatPoint(20, 40));
  EXPECT_FALSE(web_view_impl->BestTapNode(GetTargetedEvent(touch_event)));

  touch_event.SetPositionInWidget(WebFloatPoint(20, 20));
  // Shouldn't crash.
  web_view_impl->EnableTapHighlightAtPoint(GetTargetedEvent(touch_event));

  const auto& highlights =
      web_view_impl->GetPage()->GetLinkHighlights().link_highlights_;
  EXPECT_TRUE(highlights.at(0));
  EXPECT_TRUE(highlights.at(0)->Layer());

  // Find a target inside a scrollable div
  touch_event.SetPositionInWidget(WebFloatPoint(20, 100));
  web_view_impl->EnableTapHighlightAtPoint(GetTargetedEvent(touch_event));
  ASSERT_TRUE(highlights.at(0));

  // Enesure the timeline was added to a host.
  EXPECT_TRUE(!!web_view_impl->GetPage()
                    ->GetLinkHighlights()
                    .timeline_->GetAnimationTimeline()
                    ->animation_host());

  // Don't highlight if no "hand cursor"
  touch_event.SetPositionInWidget(
      WebFloatPoint(20, 220));  // An A-link with cross-hair cursor.
  web_view_impl->EnableTapHighlightAtPoint(GetTargetedEvent(touch_event));
  ASSERT_EQ(0U, highlights.size());

  touch_event.SetPositionInWidget(WebFloatPoint(20, 260));  // A text input box.
  web_view_impl->EnableTapHighlightAtPoint(GetTargetedEvent(touch_event));
  ASSERT_EQ(0U, highlights.size());
}

TEST_P(LinkHighlightImplTest, resetDuringNodeRemoval) {
  WebViewImpl* web_view_impl = web_view_helper_.GetWebView();

  int page_width = 640;
  int page_height = 480;
  web_view_impl->Resize(WebSize(page_width, page_height));
  web_view_impl->UpdateAllLifecyclePhases();

  WebGestureEvent touch_event(WebInputEvent::kGestureShowPress,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests(),
                              kWebGestureDeviceTouchscreen);
  touch_event.SetPositionInWidget(WebFloatPoint(20, 20));

  GestureEventWithHitTestResults targeted_event = GetTargetedEvent(touch_event);
  Node* touch_node = web_view_impl->BestTapNode(targeted_event);
  ASSERT_TRUE(touch_node);

  web_view_impl->EnableTapHighlightAtPoint(targeted_event);
  const auto& highlights = web_view_impl->GetPage()->GetLinkHighlights();
  ASSERT_TRUE(highlights.link_highlights_.at(0));

  GraphicsLayer* highlight_layer =
      highlights.link_highlights_.at(0)->CurrentGraphicsLayerForTesting();
  ASSERT_TRUE(highlight_layer);
  EXPECT_TRUE(highlight_layer->GetLinkHighlights().at(0));

  touch_node->remove(IGNORE_EXCEPTION_FOR_TESTING);
  web_view_impl->UpdateAllLifecyclePhases();
  EXPECT_EQ(0U, highlight_layer->GetLinkHighlights().size());
}

// A lifetime test: delete LayerTreeView while running LinkHighlights.
TEST_P(LinkHighlightImplTest, resetLayerTreeView) {
  WebViewImpl* web_view_impl = web_view_helper_.GetWebView();

  int page_width = 640;
  int page_height = 480;
  web_view_impl->Resize(WebSize(page_width, page_height));
  web_view_impl->UpdateAllLifecyclePhases();

  WebGestureEvent touch_event(WebInputEvent::kGestureShowPress,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests(),
                              kWebGestureDeviceTouchscreen);
  touch_event.SetPositionInWidget(WebFloatPoint(20, 20));

  GestureEventWithHitTestResults targeted_event = GetTargetedEvent(touch_event);
  Node* touch_node = web_view_impl->BestTapNode(targeted_event);
  ASSERT_TRUE(touch_node);

  web_view_impl->EnableTapHighlightAtPoint(targeted_event);
  const auto& highlights =
      web_view_impl->GetPage()->GetLinkHighlights().link_highlights_;
  ASSERT_TRUE(highlights.at(0));

  GraphicsLayer* highlight_layer =
      highlights.at(0)->CurrentGraphicsLayerForTesting();
  ASSERT_TRUE(highlight_layer);
  EXPECT_TRUE(highlight_layer->GetLinkHighlights().at(0));
}

TEST_P(LinkHighlightImplTest, HighlightLayerEffectNode) {
  // This is testing the blink->cc layer integration.
  if (!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled())
    return;

  int page_width = 640;
  int page_height = 480;
  WebViewImpl* web_view_impl = web_view_helper_.GetWebView();
  web_view_impl->Resize(WebSize(page_width, page_height));

  paint_artifact_compositor()->EnableExtraDataForTesting();
  web_view_impl->UpdateAllLifecyclePhases();
  size_t layer_count_before_highlight = ContentLayerCount();

  WebGestureEvent touch_event(WebInputEvent::kGestureShowPress,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests(),
                              kWebGestureDeviceTouchscreen);
  touch_event.SetPositionInWidget(WebFloatPoint(20, 20));

  GestureEventWithHitTestResults targeted_event = GetTargetedEvent(touch_event);
  Node* touch_node = web_view_impl->BestTapNode(targeted_event);
  ASSERT_TRUE(touch_node);

  web_view_impl->EnableTapHighlightAtPoint(targeted_event);
  // The highlight should create one additional layer.
  EXPECT_EQ(layer_count_before_highlight + 1, ContentLayerCount());

  const auto& highlights = web_view_impl->GetPage()->GetLinkHighlights();
  auto* highlight = highlights.link_highlights_.at(0).get();
  ASSERT_TRUE(highlight);

  // Check that the link highlight cc layer has a cc effect property tree node.
  auto* layer = highlight->Layer();
  auto effect_tree_index = layer->effect_tree_index();
  auto* property_trees = layer->layer_tree_host()->property_trees();
  EXPECT_EQ(
      effect_tree_index,
      property_trees->element_id_to_effect_node_index[layer->element_id()]);
  // The link highlight cc effect node should correspond to the blink effect
  // node.
  EXPECT_EQ(highlight->effect()->GetCompositorElementId(), layer->element_id());
  EXPECT_TRUE(highlight->effect()->RequiresCompositingForAnimation());

  touch_node->remove(IGNORE_EXCEPTION_FOR_TESTING);
  web_view_impl->UpdateAllLifecyclePhases();
  // Removing the highlight layer should drop the cc layer count by one.
  EXPECT_EQ(layer_count_before_highlight, ContentLayerCount());
}

}  // namespace blink
