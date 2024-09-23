// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/visual_viewport.h"

#include <memory>
#include <string>

#include "cc/layers/picture_layer.h"
#include "cc/layers/scrollbar_layer_base.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/widget/device_emulation_params.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/web/web_ax_context.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_and_raster_invalidation_test.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay_mobile.h"
#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"
#include "third_party/blink/renderer/core/testing/color_scheme_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/testing/find_cc_layer.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

using testing::_;
using testing::PrintToString;
using testing::Mock;
using testing::UnorderedElementsAre;
using blink::url_test_helpers::ToKURL;

namespace blink {

::std::ostream& operator<<(::std::ostream& os, const ContextMenuData& data) {
  return os << "Context menu location: [" << data.mouse_position.x() << ", "
            << data.mouse_position.y() << "]";
}

namespace {

const cc::EffectNode* GetEffectNode(const cc::Layer* layer) {
  return layer->layer_tree_host()->property_trees()->effect_tree().Node(
      layer->effect_tree_index());
}

class VisualViewportTest : public testing::Test,
                           public PaintTestConfigurations {
 public:
  VisualViewportTest() : base_url_("http://www.test.com/") {}

  void InitializeWithDesktopSettings() {
    helper_.InitializeWithSettings(&ConfigureSettings);
    WebView()->SetDefaultPageScaleLimits(1, 4);
  }

  void InitializeWithAndroidSettings(
      void (*override_settings_func)(WebSettings*) = nullptr) {
    if (!override_settings_func)
      override_settings_func = &ConfigureAndroidSettings;
    helper_.InitializeWithSettings(override_settings_func);
    WebView()->SetDefaultPageScaleLimits(0.25f, 5);
  }

  ~VisualViewportTest() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  void NavigateTo(const std::string& url) {
    frame_test_helpers::LoadFrame(WebView()->MainFrameImpl(), url);
  }

  void UpdateAllLifecyclePhases() {
    WebView()->MainFrameViewWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  void UpdateAllLifecyclePhasesExceptPaint() {
    WebView()->MainFrameViewWidget()->UpdateLifecycle(
        WebLifecycleUpdate::kPrePaint, DocumentUpdateReason::kTest);
  }

  PaintArtifactCompositor* paint_artifact_compositor() {
    LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();
    return frame_view.GetPaintArtifactCompositor();
  }

  void ForceFullCompositingUpdate() { UpdateAllLifecyclePhases(); }

  void RegisterMockedHttpURLLoad(const std::string& fileName) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |helper_|.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), blink::test::CoreTestDataPath(),
        WebString::FromUTF8(fileName));
  }

  void RegisterMockedHttpURLLoad(const std::string& url,
                                 const std::string& fileName) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |helper_|.
    url_test_helpers::RegisterMockedURLLoad(
        ToKURL(url),
        blink::test::CoreTestDataPath(WebString::FromUTF8(fileName)));
  }

  WebViewImpl* WebView() const { return helper_.GetWebView(); }
  LocalFrame* GetFrame() const { return helper_.LocalMainFrame()->GetFrame(); }

  static void ConfigureSettings(WebSettings* settings) {
    settings->SetJavaScriptEnabled(true);
    settings->SetLCDTextPreference(LCDTextPreference::kIgnored);
  }

  static void ConfigureAndroidSettings(WebSettings* settings) {
    ConfigureSettings(settings);
    frame_test_helpers::WebViewHelper::UpdateAndroidCompositingSettings(
        settings);
  }

  const DisplayItemClient& ScrollingBackgroundClient(const Document* document) {
    return document->GetLayoutView()
        ->GetScrollableArea()
        ->GetScrollingBackgroundDisplayItemClient();
  }

 protected:
  test::TaskEnvironment task_environment_;
  std::string base_url_;
  frame_test_helpers::WebViewHelper helper_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(VisualViewportTest);

// Test that resizing the VisualViewport works as expected and that resizing the
// WebView resizes the VisualViewport.
TEST_P(VisualViewportTest, TestResize) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(320, 240));
  WebView()->ResizeWithBrowserControls(
      gfx::Size(320, 240), gfx::Size(320, 240),
      WebView()->GetBrowserControls().Params());
  UpdateAllLifecyclePhases();

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();

  gfx::Size web_view_size = WebView()->MainFrameViewWidget()->Size();

  // Make sure the visual viewport was initialized.
  EXPECT_EQ(web_view_size, visual_viewport.Size());

  // Resizing the WebView should change the VisualViewport.
  web_view_size = gfx::Size(640, 480);
  WebView()->MainFrameViewWidget()->Resize(web_view_size);
  WebView()->ResizeWithBrowserControls(
      web_view_size, web_view_size, WebView()->GetBrowserControls().Params());
  UpdateAllLifecyclePhases();
  EXPECT_EQ(web_view_size, WebView()->MainFrameViewWidget()->Size());
  EXPECT_EQ(web_view_size, visual_viewport.Size());

  // Resizing the visual viewport shouldn't affect the WebView.
  gfx::Size new_viewport_size = gfx::Size(320, 200);
  visual_viewport.SetSize(new_viewport_size);
  EXPECT_EQ(web_view_size, WebView()->MainFrameViewWidget()->Size());
  EXPECT_EQ(new_viewport_size, visual_viewport.Size());
}

// Make sure that the visibleContentRect method acurately reflects the scale and
// scroll location of the viewport with and without scrollbars.
TEST_P(VisualViewportTest, TestVisibleContentRect) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();
  InitializeWithDesktopSettings();

  RegisterMockedHttpURLLoad("200-by-300.html");
  NavigateTo(base_url_ + "200-by-300.html");

  gfx::Size size(150, 100);
  // Vertical scrollbar width and horizontal scrollbar height.
  gfx::Size scrollbar_size(15, 15);

  WebView()->ResizeWithBrowserControls(
      size, size, WebView()->GetBrowserControls().Params());
  UpdateAllLifecyclePhases();

  // Scroll layout viewport and verify visibleContentRect.
  WebView()->MainFrameImpl()->SetScrollOffset(gfx::PointF(0, 50));

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_EQ(gfx::Rect(gfx::Point(0, 0), size - scrollbar_size),
            visual_viewport.VisibleContentRect(kExcludeScrollbars));
  EXPECT_EQ(gfx::Rect(gfx::Point(0, 0), size),
            visual_viewport.VisibleContentRect(kIncludeScrollbars));

  WebView()->SetPageScaleFactor(2.0);

  // Scroll visual viewport and verify visibleContentRect.
  size = gfx::ScaleToFlooredSize(size, 0.5);
  scrollbar_size = gfx::ScaleToFlooredSize(scrollbar_size, 0.5);
  visual_viewport.SetLocation(gfx::PointF(10, 10));
  EXPECT_EQ(gfx::Rect(gfx::Point(10, 10), size - scrollbar_size),
            visual_viewport.VisibleContentRect(kExcludeScrollbars));
  EXPECT_EQ(gfx::Rect(gfx::Point(10, 10), size),
            visual_viewport.VisibleContentRect(kIncludeScrollbars));
}

// This tests that shrinking the WebView while the page is fully scrolled
// doesn't move the viewport up/left, it should keep the visible viewport
// unchanged from the user's perspective (shrinking the LocalFrameView will
// clamp the VisualViewport so we need to counter scroll the LocalFrameView to
// make it appear to stay still). This caused bugs like crbug.com/453859.
TEST_P(VisualViewportTest, TestResizeAtFullyScrolledPreservesViewportLocation) {
  InitializeWithDesktopSettings();
  WebView()->ResizeWithBrowserControls(
      gfx::Size(800, 600), gfx::Size(800, 600),
      WebView()->GetBrowserControls().Params());
  UpdateAllLifecyclePhases();

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");

  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();

  visual_viewport.SetScale(2);

  // Fully scroll both viewports.
  frame_view.LayoutViewport()->SetScrollOffset(
      ScrollOffset(10000, 10000), mojom::blink::ScrollType::kProgrammatic);
  visual_viewport.Move(gfx::Vector2dF(10000, 10000));

  // Sanity check.
  ASSERT_EQ(ScrollOffset(400, 300), visual_viewport.GetScrollOffset());
  ASSERT_EQ(ScrollOffset(200, 1400),
            frame_view.LayoutViewport()->GetScrollOffset());

  gfx::Point expected_location =
      frame_view.GetScrollableArea()->VisibleContentRect().origin();

  // Shrink the WebView, this should cause both viewports to shrink and
  // WebView should do whatever it needs to do to preserve the visible
  // location.
  WebView()->ResizeWithBrowserControls(
      gfx::Size(700, 550), gfx::Size(800, 600),
      WebView()->GetBrowserControls().Params());
  UpdateAllLifecyclePhases();

  EXPECT_EQ(expected_location,
            frame_view.GetScrollableArea()->VisibleContentRect().origin());

  WebView()->ResizeWithBrowserControls(
      gfx::Size(800, 600), gfx::Size(800, 600),
      WebView()->GetBrowserControls().Params());
  UpdateAllLifecyclePhases();

  EXPECT_EQ(expected_location,
            frame_view.GetScrollableArea()->VisibleContentRect().origin());
}

// Test that the VisualViewport works as expected in case of a scaled
// and scrolled viewport - scroll down.
TEST_P(VisualViewportTest, TestResizeAfterVerticalScroll) {
  /*
                 200                                 200
        |                   |               |                   |
        |                   |               |                   |
        |                   | 800           |                   | 800
        |-------------------|               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |   -------->   |                   |
        | 300               |               |                   |
        |                   |               |                   |
        |               400 |               |                   |
        |                   |               |-------------------|
        |                   |               |      75           |
        | 50                |               | 50             100|
        o-----              |               o----               |
        |    |              |               |   |  25           |
        |    |100           |               |-------------------|
        |    |              |               |                   |
        |    |              |               |                   |
        --------------------                --------------------

     */
  InitializeWithAndroidSettings();

  RegisterMockedHttpURLLoad("200-by-800-viewport.html");
  NavigateTo(base_url_ + "200-by-800-viewport.html");

  WebView()->MainFrameViewWidget()->Resize(gfx::Size(100, 200));

  // Scroll main frame to the bottom of the document
  WebView()->MainFrameImpl()->SetScrollOffset(gfx::PointF(0, 400));
  EXPECT_EQ(ScrollOffset(0, 400),
            GetFrame()->View()->LayoutViewport()->GetScrollOffset());

  WebView()->SetPageScaleFactor(2.0);

  // Scroll visual viewport to the bottom of the main frame
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.SetLocation(gfx::PointF(0, 300));
  EXPECT_VECTOR2DF_EQ(ScrollOffset(0, 300), visual_viewport.GetScrollOffset());

  // Verify the initial size of the visual viewport in the CSS pixels
  EXPECT_SIZEF_EQ(gfx::SizeF(50, 100), visual_viewport.VisibleRect().size());

  // Verify the paint property nodes and GeometryMapper cache.
  {
    UpdateAllLifecyclePhases();
    EXPECT_EQ(gfx::Transform::MakeScale(2),
              visual_viewport.GetPageScaleNode()->Matrix());
    EXPECT_EQ(gfx::Vector2dF(0, -300),
              visual_viewport.GetScrollTranslationNode()->Get2dTranslation());
    auto expected_projection = gfx::Transform::MakeScale(2);
    expected_projection.Translate(0, -300);
    EXPECT_EQ(expected_projection,
              GeometryMapper::SourceToDestinationProjection(
                  *visual_viewport.GetScrollTranslationNode(),
                  TransformPaintPropertyNode::Root()));
  }

  // Perform the resizing
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(200, 100));

  // After resizing the scale changes 2.0 -> 4.0
  EXPECT_SIZEF_EQ(gfx::SizeF(50, 25), visual_viewport.VisibleRect().size());

  EXPECT_EQ(ScrollOffset(0, 625),
            GetFrame()->View()->LayoutViewport()->GetScrollOffset());
  EXPECT_VECTOR2DF_EQ(ScrollOffset(0, 75), visual_viewport.GetScrollOffset());

  // Verify the paint property nodes and GeometryMapper cache.
  {
    UpdateAllLifecyclePhases();
    EXPECT_EQ(gfx::Transform::MakeScale(4),
              visual_viewport.GetPageScaleNode()->Matrix());
    EXPECT_EQ(gfx::Vector2dF(0, -75),
              visual_viewport.GetScrollTranslationNode()->Get2dTranslation());
    auto expected_projection = gfx::Transform::MakeScale(4);
    expected_projection.Translate(0, -75);
    EXPECT_EQ(expected_projection,
              GeometryMapper::SourceToDestinationProjection(
                  *visual_viewport.GetScrollTranslationNode(),
                  TransformPaintPropertyNode::Root()));
  }
}

// Test that the VisualViewport works as expected in case if a scaled
// and scrolled viewport - scroll right.
TEST_P(VisualViewportTest, TestResizeAfterHorizontalScroll) {
  /*
                 200                                 200
        ---------------o-----               ---------------o-----
        |              |    |               |            25|    |
        |              |    |               |              -----|
        |           100|    |               |100             50 |
        |              |    |               |                   |
        |              ---- |               |-------------------|
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |400                |   --------->  |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |-------------------|               |                   |
        |                   |               |                   |

     */
  InitializeWithAndroidSettings();

  RegisterMockedHttpURLLoad("200-by-800-viewport.html");
  NavigateTo(base_url_ + "200-by-800-viewport.html");

  WebView()->MainFrameViewWidget()->Resize(gfx::Size(100, 200));

  // Outer viewport takes the whole width of the document.

  WebView()->SetPageScaleFactor(2.0);

  // Scroll visual viewport to the right edge of the frame
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.SetLocation(gfx::PointF(150, 0));
  EXPECT_VECTOR2DF_EQ(ScrollOffset(150, 0), visual_viewport.GetScrollOffset());

  // Verify the initial size of the visual viewport in the CSS pixels
  EXPECT_SIZEF_EQ(gfx::SizeF(50, 100), visual_viewport.VisibleRect().size());

  // Verify the paint property nodes and GeometryMapper cache.
  {
    UpdateAllLifecyclePhases();
    EXPECT_EQ(gfx::Transform::MakeScale(2),
              visual_viewport.GetPageScaleNode()->Matrix());
    EXPECT_EQ(gfx::Vector2dF(-150, 0),
              visual_viewport.GetScrollTranslationNode()->Get2dTranslation());
    auto expected_projection = gfx::Transform::MakeScale(2);
    expected_projection.Translate(-150, 0);
    EXPECT_EQ(expected_projection,
              GeometryMapper::SourceToDestinationProjection(
                  *visual_viewport.GetScrollTranslationNode(),
                  TransformPaintPropertyNode::Root()));
  }

  WebView()->MainFrameViewWidget()->Resize(gfx::Size(200, 100));

  // After resizing the scale changes 2.0 -> 4.0
  EXPECT_SIZEF_EQ(gfx::SizeF(50, 25), visual_viewport.VisibleRect().size());

  EXPECT_EQ(ScrollOffset(0, 0),
            GetFrame()->View()->LayoutViewport()->GetScrollOffset());
  EXPECT_VECTOR2DF_EQ(ScrollOffset(150, 0), visual_viewport.GetScrollOffset());

  // Verify the paint property nodes and GeometryMapper cache.
  {
    UpdateAllLifecyclePhases();
    EXPECT_EQ(gfx::Transform::MakeScale(4),
              visual_viewport.GetPageScaleNode()->Matrix());
    EXPECT_EQ(gfx::Vector2dF(-150, 0),
              visual_viewport.GetScrollTranslationNode()->Get2dTranslation());
    auto expected_projection = gfx::Transform::MakeScale(4);
    expected_projection.Translate(-150, 0);
    EXPECT_EQ(expected_projection,
              GeometryMapper::SourceToDestinationProjection(
                  *visual_viewport.GetScrollTranslationNode(),
                  TransformPaintPropertyNode::Root()));
  }
}

// Make sure that the visibleRect method acurately reflects the scale and scroll
// location of the viewport.
TEST_P(VisualViewportTest, TestVisibleRect) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(320, 240));

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();

  // Initial visible rect should be the whole frame.
  EXPECT_EQ(WebView()->MainFrameViewWidget()->Size(), visual_viewport.Size());

  // Viewport is whole frame.
  gfx::Size size = gfx::Size(400, 200);
  WebView()->MainFrameViewWidget()->Resize(size);
  UpdateAllLifecyclePhases();
  visual_viewport.SetSize(size);

  // Scale the viewport to 2X; size should not change.
  gfx::RectF expected_rect((gfx::SizeF(size)));
  expected_rect.Scale(0.5);
  visual_viewport.SetScale(2);
  EXPECT_EQ(2, visual_viewport.Scale());
  EXPECT_EQ(size, visual_viewport.Size());
  EXPECT_RECTF_EQ(expected_rect, visual_viewport.VisibleRect());

  // Move the viewport.
  expected_rect.set_origin(gfx::PointF(5, 7));
  visual_viewport.SetLocation(expected_rect.origin());
  EXPECT_RECTF_EQ(expected_rect, visual_viewport.VisibleRect());

  expected_rect.set_origin(gfx::PointF(200, 100));
  visual_viewport.SetLocation(expected_rect.origin());
  EXPECT_RECTF_EQ(expected_rect, visual_viewport.VisibleRect());

  // Scale the viewport to 3X to introduce some non-int values.
  gfx::PointF oldLocation = expected_rect.origin();
  expected_rect = gfx::RectF(gfx::SizeF(size));
  expected_rect.Scale(1 / 3.0f);
  expected_rect.set_origin(oldLocation);
  visual_viewport.SetScale(3);
  EXPECT_RECTF_EQ(expected_rect, visual_viewport.VisibleRect());

  expected_rect.set_origin(gfx::PointF(0.25f, 0.333f));
  visual_viewport.SetLocation(expected_rect.origin());
  EXPECT_RECTF_EQ(expected_rect, visual_viewport.VisibleRect());
}

TEST_P(VisualViewportTest, TestFractionalScrollOffsetIsNotOverwritten) {
  ScopedFractionalScrollOffsetsForTest fractional_scroll_offsets(true);
  InitializeWithAndroidSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(200, 250));

  RegisterMockedHttpURLLoad("200-by-800-viewport.html");
  NavigateTo(base_url_ + "200-by-800-viewport.html");

  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();
  frame_view.LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 10.5), mojom::blink::ScrollType::kProgrammatic);
  frame_view.LayoutViewport()->ScrollableArea::SetScrollOffset(
      ScrollOffset(10, 30.5), mojom::blink::ScrollType::kCompositor);

  EXPECT_EQ(30.5, frame_view.LayoutViewport()->GetScrollOffset().y());
}

// Test that the viewport's scroll offset is always appropriately bounded such
// that the visual viewport always stays within the bounds of the main frame.
TEST_P(VisualViewportTest, TestOffsetClamping) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(320, 240));

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(
      WebView()->MainFrameImpl(),
      "<!DOCTYPE html>"
      "<meta name='viewport' content='width=2000'>",
      base_url);
  ForceFullCompositingUpdate();

  // Visual viewport should be initialized to same size as frame so no scrolling
  // possible. At minimum scale, the viewport is 1280x960.
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  ASSERT_EQ(0.25, visual_viewport.Scale());
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), visual_viewport.VisibleRect().origin());

  visual_viewport.SetLocation(gfx::PointF(-1, -2));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), visual_viewport.VisibleRect().origin());

  visual_viewport.SetLocation(gfx::PointF(100, 200));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), visual_viewport.VisibleRect().origin());

  visual_viewport.SetLocation(gfx::PointF(-5, 10));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), visual_viewport.VisibleRect().origin());

  // Scale to 2x. The viewport's visible rect should now have a size of 160x120.
  visual_viewport.SetScale(2);
  gfx::PointF location(10, 50);
  visual_viewport.SetLocation(location);
  EXPECT_POINTF_EQ(location, visual_viewport.VisibleRect().origin());

  visual_viewport.SetLocation(gfx::PointF(10000, 10000));
  EXPECT_POINTF_EQ(gfx::PointF(1120, 840),
                   visual_viewport.VisibleRect().origin());

  visual_viewport.SetLocation(gfx::PointF(-2000, -2000));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), visual_viewport.VisibleRect().origin());

  // Make sure offset gets clamped on scale out. Scale to 1.25 so the viewport
  // is 256x192.
  visual_viewport.SetLocation(gfx::PointF(1120, 840));
  visual_viewport.SetScale(1.25);
  EXPECT_POINTF_EQ(gfx::PointF(1024, 768),
                   visual_viewport.VisibleRect().origin());

  // Scale out smaller than 1.
  visual_viewport.SetScale(0.25);
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), visual_viewport.VisibleRect().origin());
}

// Test that the viewport can be scrolled around only within the main frame in
// the presence of viewport resizes, as would be the case if the on screen
// keyboard came up.
TEST_P(VisualViewportTest, TestOffsetClampingWithResize) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(320, 240));

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  // Visual viewport should be initialized to same size as frame so no scrolling
  // possible.
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), visual_viewport.VisibleRect().origin());

  // Shrink the viewport vertically. The resize shouldn't affect the location,
  // but it should allow vertical scrolling.
  visual_viewport.SetSize(gfx::Size(320, 200));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), visual_viewport.VisibleRect().origin());
  visual_viewport.SetLocation(gfx::PointF(10, 20));
  EXPECT_POINTF_EQ(gfx::PointF(0, 20), visual_viewport.VisibleRect().origin());
  visual_viewport.SetLocation(gfx::PointF(0, 100));
  EXPECT_POINTF_EQ(gfx::PointF(0, 40), visual_viewport.VisibleRect().origin());
  visual_viewport.SetLocation(gfx::PointF(0, 10));
  EXPECT_POINTF_EQ(gfx::PointF(0, 10), visual_viewport.VisibleRect().origin());
  visual_viewport.SetLocation(gfx::PointF(0, -100));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), visual_viewport.VisibleRect().origin());

  // Repeat the above but for horizontal dimension.
  visual_viewport.SetSize(gfx::Size(280, 240));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), visual_viewport.VisibleRect().origin());
  visual_viewport.SetLocation(gfx::PointF(10, 20));
  EXPECT_POINTF_EQ(gfx::PointF(10, 0), visual_viewport.VisibleRect().origin());
  visual_viewport.SetLocation(gfx::PointF(100, 0));
  EXPECT_POINTF_EQ(gfx::PointF(40, 0), visual_viewport.VisibleRect().origin());
  visual_viewport.SetLocation(gfx::PointF(10, 0));
  EXPECT_POINTF_EQ(gfx::PointF(10, 0), visual_viewport.VisibleRect().origin());
  visual_viewport.SetLocation(gfx::PointF(-100, 0));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), visual_viewport.VisibleRect().origin());

  // Now with both dimensions.
  visual_viewport.SetSize(gfx::Size(280, 200));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), visual_viewport.VisibleRect().origin());
  visual_viewport.SetLocation(gfx::PointF(10, 20));
  EXPECT_POINTF_EQ(gfx::PointF(10, 20), visual_viewport.VisibleRect().origin());
  visual_viewport.SetLocation(gfx::PointF(100, 100));
  EXPECT_POINTF_EQ(gfx::PointF(40, 40), visual_viewport.VisibleRect().origin());
  visual_viewport.SetLocation(gfx::PointF(10, 3));
  EXPECT_POINTF_EQ(gfx::PointF(10, 3), visual_viewport.VisibleRect().origin());
  visual_viewport.SetLocation(gfx::PointF(-10, -4));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), visual_viewport.VisibleRect().origin());
}

// Test that the viewport is scrollable but bounded appropriately within the
// main frame when we apply both scaling and resizes.
TEST_P(VisualViewportTest, TestOffsetClampingWithResizeAndScale) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(320, 240));

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  // Visual viewport should be initialized to same size as WebView so no
  // scrolling possible.
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), visual_viewport.VisibleRect().origin());

  // Zoom in to 2X so we can scroll the viewport to 160x120.
  visual_viewport.SetScale(2);
  visual_viewport.SetLocation(gfx::PointF(200, 200));
  EXPECT_POINTF_EQ(gfx::PointF(160, 120),
                   visual_viewport.VisibleRect().origin());

  // Now resize the viewport to make it 10px smaller. Since we're zoomed in by
  // 2X it should allow us to scroll by 5px more.
  visual_viewport.SetSize(gfx::Size(310, 230));
  visual_viewport.SetLocation(gfx::PointF(200, 200));
  EXPECT_POINTF_EQ(gfx::PointF(165, 125),
                   visual_viewport.VisibleRect().origin());

  // The viewport can be larger than the main frame (currently 320, 240) though
  // typically the scale will be clamped to prevent it from actually being
  // larger.
  visual_viewport.SetSize(gfx::Size(330, 250));
  EXPECT_EQ(gfx::Size(330, 250), visual_viewport.Size());

  // Resize both the viewport and the frame to be larger.
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(640, 480));
  UpdateAllLifecyclePhases();
  EXPECT_EQ(WebView()->MainFrameViewWidget()->Size(), visual_viewport.Size());
  EXPECT_EQ(WebView()->MainFrameViewWidget()->Size(),
            GetFrame()->View()->FrameRect().size());
  visual_viewport.SetLocation(gfx::PointF(1000, 1000));
  EXPECT_POINTF_EQ(gfx::PointF(320, 240),
                   visual_viewport.VisibleRect().origin());

  // Make sure resizing the viewport doesn't change its offset if the resize
  // doesn't make the viewport go out of bounds.
  visual_viewport.SetLocation(gfx::PointF(200, 200));
  visual_viewport.SetSize(gfx::Size(880, 560));
  EXPECT_POINTF_EQ(gfx::PointF(200, 200),
                   visual_viewport.VisibleRect().origin());
}

// The main LocalFrameView's size should be set such that its the size of the
// visual viewport at minimum scale. If there's no explicit minimum scale set,
// the LocalFrameView should be set to the content width and height derived by
// the aspect ratio.
TEST_P(VisualViewportTest, TestFrameViewSizedToContent) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(320, 240));

  RegisterMockedHttpURLLoad("200-by-300-viewport.html");
  NavigateTo(base_url_ + "200-by-300-viewport.html");

  WebView()->MainFrameViewWidget()->Resize(gfx::Size(600, 800));
  UpdateAllLifecyclePhases();

  // Note: the size is ceiled and should match the behavior in CC's
  // LayerImpl::bounds().
  EXPECT_EQ(gfx::Size(200, 267),
            WebView()->MainFrameImpl()->GetFrameView()->FrameRect().size());
}

// The main LocalFrameView's size should be set such that its the size of the
// visual viewport at minimum scale. On Desktop, the minimum scale is set at 1
// so make sure the LocalFrameView is sized to the viewport.
TEST_P(VisualViewportTest, TestFrameViewSizedToMinimumScale) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(320, 240));

  RegisterMockedHttpURLLoad("200-by-300.html");
  NavigateTo(base_url_ + "200-by-300.html");

  WebView()->MainFrameViewWidget()->Resize(gfx::Size(100, 160));
  UpdateAllLifecyclePhases();

  EXPECT_EQ(gfx::Size(100, 160),
            WebView()->MainFrameImpl()->GetFrameView()->FrameRect().size());
}

// Test that attaching a new frame view resets the size of the inner viewport
// scroll layer. crbug.com/423189.
TEST_P(VisualViewportTest, TestAttachingNewFrameSetsInnerScrollLayerSize) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(320, 240));

  // Load a wider page first, the navigation should resize the scroll layer to
  // the smaller size on the second navigation.
  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");
  UpdateAllLifecyclePhases();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.SetScale(2);
  visual_viewport.Move(ScrollOffset(50, 60));

  // Move and scale the viewport to make sure it gets reset in the navigation.
  EXPECT_EQ(ScrollOffset(50, 60), visual_viewport.GetScrollOffset());
  EXPECT_EQ(2, visual_viewport.Scale());

  // Navigate again, this time the LocalFrameView should be smaller.
  RegisterMockedHttpURLLoad("viewport-device-width.html");
  NavigateTo(base_url_ + "viewport-device-width.html");
  UpdateAllLifecyclePhases();

  // Ensure the scroll contents size matches the frame view's size.
  EXPECT_EQ(gfx::Size(320, 240), visual_viewport.LayerForScrolling()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 320, 240),
            visual_viewport.GetScrollNode()->ContentsRect());

  // Ensure the location and scale were reset.
  EXPECT_EQ(ScrollOffset(), visual_viewport.GetScrollOffset());
  EXPECT_EQ(1, visual_viewport.Scale());
}

// The main LocalFrameView's size should be set such that its the size of the
// visual viewport at minimum scale. Test that the LocalFrameView is
// appropriately sized in the presence of a viewport <meta> tag.
TEST_P(VisualViewportTest, TestFrameViewSizedToViewportMetaMinimumScale) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(320, 240));

  RegisterMockedHttpURLLoad("200-by-300-min-scale-2.html");
  NavigateTo(base_url_ + "200-by-300-min-scale-2.html");

  WebView()->MainFrameViewWidget()->Resize(gfx::Size(100, 160));
  UpdateAllLifecyclePhases();

  EXPECT_EQ(gfx::Size(50, 80),
            WebView()->MainFrameImpl()->GetFrameView()->FrameRect().size());
}

// Test that the visual viewport still gets sized in AutoSize/AutoResize mode.
TEST_P(VisualViewportTest, TestVisualViewportGetsSizeInAutoSizeMode) {
  InitializeWithDesktopSettings();

  EXPECT_EQ(gfx::Size(0, 0), WebView()->MainFrameViewWidget()->Size());
  EXPECT_EQ(gfx::Size(0, 0), GetFrame()->GetPage()->GetVisualViewport().Size());

  WebView()->EnableAutoResizeMode(gfx::Size(10, 10), gfx::Size(1000, 1000));

  RegisterMockedHttpURLLoad("200-by-300.html");
  NavigateTo(base_url_ + "200-by-300.html");

  EXPECT_EQ(gfx::Size(200, 300),
            GetFrame()->GetPage()->GetVisualViewport().Size());
}

// Test that the text selection handle's position accounts for the visual
// viewport.
TEST_P(VisualViewportTest, TestTextSelectionHandles) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(500, 800));

  RegisterMockedHttpURLLoad("pinch-viewport-input-field.html");
  NavigateTo(base_url_ + "pinch-viewport-input-field.html");

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  To<LocalFrame>(WebView()->GetPage()->MainFrame())->SetInitialFocus(false);

  gfx::Rect original_anchor;
  gfx::Rect original_focus;
  WebView()->MainFrameViewWidget()->CalculateSelectionBounds(original_anchor,
                                                             original_focus);

  WebView()->SetPageScaleFactor(2);
  visual_viewport.SetLocation(gfx::PointF(100, 400));

  gfx::Rect anchor;
  gfx::Rect focus;
  WebView()->MainFrameViewWidget()->CalculateSelectionBounds(anchor, focus);

  gfx::Point expected = original_anchor.origin();
  expected -=
      gfx::ToFlooredVector2d(visual_viewport.VisibleRect().OffsetFromOrigin());
  expected = gfx::ScaleToRoundedPoint(expected, visual_viewport.Scale());

  EXPECT_EQ(expected, anchor.origin());
  EXPECT_EQ(expected, focus.origin());

  // FIXME(bokan) - http://crbug.com/364154 - Figure out how to test text
  // selection as well rather than just carret.
}

// Test that the HistoryItem for the page stores the visual viewport's offset
// and scale.
TEST_P(VisualViewportTest, TestSavedToHistoryItem) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(200, 300));
  UpdateAllLifecyclePhases();

  RegisterMockedHttpURLLoad("200-by-300.html");
  NavigateTo(base_url_ + "200-by-300.html");

  EXPECT_FALSE(To<LocalFrame>(WebView()->GetPage()->MainFrame())
                   ->Loader()
                   .GetDocumentLoader()
                   ->GetHistoryItem()
                   ->GetViewState()
                   .has_value());

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.SetScale(2);

  EXPECT_EQ(2, To<LocalFrame>(WebView()->GetPage()->MainFrame())
                   ->Loader()
                   .GetDocumentLoader()
                   ->GetHistoryItem()
                   ->GetViewState()
                   ->page_scale_factor_);

  visual_viewport.SetLocation(gfx::PointF(10, 20));

  EXPECT_EQ(ScrollOffset(10, 20),
            To<LocalFrame>(WebView()->GetPage()->MainFrame())
                ->Loader()
                .GetDocumentLoader()
                ->GetHistoryItem()
                ->GetViewState()
                ->visual_viewport_scroll_offset_);
}

// Test restoring a HistoryItem properly restores the visual viewport's state.
TEST_P(VisualViewportTest, TestRestoredFromHistoryItem) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(200, 300));

  RegisterMockedHttpURLLoad("200-by-300.html");

  HistoryItem* item = MakeGarbageCollected<HistoryItem>();
  item->SetURL(url_test_helpers::ToKURL(base_url_ + "200-by-300.html"));
  item->SetVisualViewportScrollOffset(ScrollOffset(100, 120));
  item->SetPageScaleFactor(2);

  frame_test_helpers::LoadHistoryItem(WebView()->MainFrameImpl(), item,
                                      mojom::FetchCacheMode::kDefault);
  UpdateAllLifecyclePhases();
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_EQ(2, visual_viewport.Scale());

  EXPECT_POINTF_EQ(gfx::PointF(100, 120),
                   visual_viewport.VisibleRect().origin());
}

// Test restoring a HistoryItem without the visual viewport offset falls back to
// distributing the scroll offset between the main frame and the visual
// viewport.
TEST_P(VisualViewportTest, TestRestoredFromLegacyHistoryItem) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(100, 150));

  RegisterMockedHttpURLLoad("200-by-300-viewport.html");

  HistoryItem* item = MakeGarbageCollected<HistoryItem>();
  item->SetURL(
      url_test_helpers::ToKURL(base_url_ + "200-by-300-viewport.html"));
  // (-1, -1) will be used if the HistoryItem is an older version prior to
  // having visual viewport scroll offset.
  item->SetVisualViewportScrollOffset(ScrollOffset(-1, -1));
  item->SetScrollOffset(ScrollOffset(120, 180));
  item->SetPageScaleFactor(2);

  frame_test_helpers::LoadHistoryItem(WebView()->MainFrameImpl(), item,
                                      mojom::FetchCacheMode::kDefault);
  UpdateAllLifecyclePhases();
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_EQ(2, visual_viewport.Scale());
  EXPECT_EQ(ScrollOffset(100, 150),
            GetFrame()->View()->LayoutViewport()->GetScrollOffset());
  EXPECT_POINTF_EQ(gfx::PointF(20, 30), visual_viewport.VisibleRect().origin());
}

// Test that navigation to a new page with a different sized main frame doesn't
// clobber the history item's main frame scroll offset. crbug.com/371867
TEST_P(VisualViewportTest,
       TestNavigateToSmallerFrameViewHistoryItemClobberBug) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  UpdateAllLifecyclePhases();

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");

  LocalFrameView* frame_view = WebView()->MainFrameImpl()->GetFrameView();
  frame_view->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 1000), mojom::blink::ScrollType::kProgrammatic);

  EXPECT_EQ(gfx::Size(1000, 1000), frame_view->FrameRect().size());

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.SetScale(2);
  visual_viewport.SetLocation(gfx::PointF(350, 350));

  Persistent<HistoryItem> firstItem = WebView()
                                          ->MainFrameImpl()
                                          ->GetFrame()
                                          ->Loader()
                                          .GetDocumentLoader()
                                          ->GetHistoryItem();
  EXPECT_EQ(ScrollOffset(0, 1000), firstItem->GetViewState()->scroll_offset_);

  // Now navigate to a page which causes a smaller frame_view. Make sure that
  // navigating doesn't cause the history item to set a new scroll offset
  // before the item was replaced.
  NavigateTo("about:blank");
  frame_view = WebView()->MainFrameImpl()->GetFrameView();

  EXPECT_NE(firstItem, WebView()
                           ->MainFrameImpl()
                           ->GetFrame()
                           ->Loader()
                           .GetDocumentLoader()
                           ->GetHistoryItem());
  EXPECT_LT(frame_view->FrameRect().size().width(), 1000);
  EXPECT_EQ(ScrollOffset(0, 1000), firstItem->GetViewState()->scroll_offset_);
}

// Test that the coordinates sent into moveRangeSelection are offset by the
// visual viewport's location.
TEST_P(VisualViewportTest,
       DISABLED_TestWebFrameRangeAccountsForVisualViewportScroll) {
  InitializeWithDesktopSettings();
  WebView()->GetSettings()->SetDefaultFontSize(12);
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(640, 480));
  RegisterMockedHttpURLLoad("move_range.html");
  NavigateTo(base_url_ + "move_range.html");

  gfx::Rect base_rect;
  gfx::Rect extent_rect;

  WebView()->SetPageScaleFactor(2);
  WebLocalFrame* main_frame = WebView()->MainFrameImpl();

  // Select some text and get the base and extent rects (that's the start of
  // the range and its end). Do a sanity check that the expected text is
  // selected
  main_frame->ExecuteScript(WebScriptSource("selectRange();"));
  EXPECT_EQ("ir", main_frame->SelectionAsText().Utf8());

  WebView()->MainFrameViewWidget()->CalculateSelectionBounds(base_rect,
                                                             extent_rect);
  gfx::Point initial_point = base_rect.origin();
  gfx::Point end_point = extent_rect.origin();

  // Move the visual viewport over and make the selection in the same
  // screen-space location. The selection should change to two characters to the
  // right and down one line.
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.Move(ScrollOffset(60, 25));
  main_frame->MoveRangeSelection(initial_point, end_point);
  EXPECT_EQ("t ", main_frame->SelectionAsText().Utf8());
}

// Test that resizing the WebView causes ViewportConstrained objects to
// relayout.
TEST_P(VisualViewportTest, TestWebViewResizeCausesViewportConstrainedLayout) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(500, 300));

  RegisterMockedHttpURLLoad("pinch-viewport-fixed-pos.html");
  NavigateTo(base_url_ + "pinch-viewport-fixed-pos.html");

  LayoutObject* layout_view = GetFrame()->GetDocument()->GetLayoutView();
  EXPECT_FALSE(layout_view->NeedsLayout());

  GetFrame()->View()->Resize(gfx::Size(500, 200));
  EXPECT_TRUE(layout_view->NeedsLayout());
}

class VisualViewportMockWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  MOCK_METHOD2(UpdateContextMenuDataForTesting,
               void(const ContextMenuData&, const std::optional<gfx::Point>&));
  MOCK_METHOD0(DidChangeScrollOffset, void());
};

MATCHER_P2(ContextMenuAtLocation,
           x,
           y,
           std::string(negation ? "is" : "isn't") + " at expected location [" +
               PrintToString(x) + ", " + PrintToString(y) + "]") {
  return arg.mouse_position.x() == x && arg.mouse_position.y() == y;
}

// Test that the context menu's location is correct in the presence of visual
// viewport offset.
TEST_P(VisualViewportTest, TestContextMenuShownInCorrectLocation) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(200, 300));

  RegisterMockedHttpURLLoad("200-by-300.html");
  NavigateTo(base_url_ + "200-by-300.html");

  WebMouseEvent mouse_down_event(WebInputEvent::Type::kMouseDown,
                                 WebInputEvent::kNoModifiers,
                                 WebInputEvent::GetStaticTimeStampForTests());
  mouse_down_event.SetPositionInWidget(10, 10);
  mouse_down_event.SetPositionInScreen(110, 210);
  mouse_down_event.click_count = 1;
  mouse_down_event.button = WebMouseEvent::Button::kRight;

  // Corresponding release event (Windows shows context menu on release).
  WebMouseEvent mouse_up_event(mouse_down_event);
  mouse_up_event.SetType(WebInputEvent::Type::kMouseUp);

  WebLocalFrameClient* old_client = WebView()->MainFrameImpl()->Client();
  VisualViewportMockWebFrameClient mock_web_frame_client;
  EXPECT_CALL(
      mock_web_frame_client,
      UpdateContextMenuDataForTesting(
          ContextMenuAtLocation(mouse_down_event.PositionInWidget().x(),
                                mouse_down_event.PositionInWidget().y()),
          _));

  // Do a sanity check with no scale applied.
  WebView()->MainFrameImpl()->SetClient(&mock_web_frame_client);
  WebView()->MainFrameViewWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_down_event, ui::LatencyInfo()));
  WebView()->MainFrameViewWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_up_event, ui::LatencyInfo()));

  Mock::VerifyAndClearExpectations(&mock_web_frame_client);
  mouse_down_event.button = WebMouseEvent::Button::kLeft;
  WebView()->MainFrameViewWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_down_event, ui::LatencyInfo()));

  // Now pinch zoom into the page and move the visual viewport. The context menu
  // should still appear at the location of the event, relative to the WebView.
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  WebView()->SetPageScaleFactor(2);
  EXPECT_CALL(mock_web_frame_client, DidChangeScrollOffset());
  visual_viewport.SetLocation(gfx::PointF(60, 80));
  EXPECT_CALL(
      mock_web_frame_client,
      UpdateContextMenuDataForTesting(
          ContextMenuAtLocation(mouse_down_event.PositionInWidget().x(),
                                mouse_down_event.PositionInWidget().y()),
          _));

  mouse_down_event.button = WebMouseEvent::Button::kRight;
  WebView()->MainFrameViewWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_down_event, ui::LatencyInfo()));
  WebView()->MainFrameViewWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_up_event, ui::LatencyInfo()));

  // Reset the old client so destruction can occur naturally.
  WebView()->MainFrameImpl()->SetClient(old_client);
}

// Test that the client is notified if page scroll events.
TEST_P(VisualViewportTest, TestClientNotifiedOfScrollEvents) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(200, 300));

  RegisterMockedHttpURLLoad("200-by-300.html");
  NavigateTo(base_url_ + "200-by-300.html");

  WebLocalFrameClient* old_client = WebView()->MainFrameImpl()->Client();
  VisualViewportMockWebFrameClient mock_web_frame_client;
  WebView()->MainFrameImpl()->SetClient(&mock_web_frame_client);

  WebView()->SetPageScaleFactor(2);
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();

  EXPECT_CALL(mock_web_frame_client, DidChangeScrollOffset());
  visual_viewport.SetLocation(gfx::PointF(60, 80));
  Mock::VerifyAndClearExpectations(&mock_web_frame_client);

  // Scroll vertically.
  EXPECT_CALL(mock_web_frame_client, DidChangeScrollOffset());
  visual_viewport.SetLocation(gfx::PointF(60, 90));
  Mock::VerifyAndClearExpectations(&mock_web_frame_client);

  // Scroll horizontally.
  EXPECT_CALL(mock_web_frame_client, DidChangeScrollOffset());
  visual_viewport.SetLocation(gfx::PointF(70, 90));

  // Reset the old client so destruction can occur naturally.
  WebView()->MainFrameImpl()->SetClient(old_client);
}

// Tests that calling scroll into view on a visible element doesn't cause
// a scroll due to a fractional offset. Bug crbug.com/463356.
TEST_P(VisualViewportTest, ScrollIntoViewFractionalOffset) {
  InitializeWithAndroidSettings();

  WebView()->MainFrameViewWidget()->Resize(gfx::Size(1000, 1000));

  RegisterMockedHttpURLLoad("scroll-into-view.html");
  NavigateTo(base_url_ + "scroll-into-view.html");

  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();
  ScrollableArea* layout_viewport_scrollable_area = frame_view.LayoutViewport();
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  Element* inputBox =
      GetFrame()->GetDocument()->getElementById(AtomicString("box"));

  WebView()->SetPageScaleFactor(2);

  // The element is already in the view so the scrollIntoView shouldn't move
  // the viewport at all.
  WebView()->SetVisualViewportOffset(gfx::PointF(250.25f, 100.25f));
  layout_viewport_scrollable_area->SetScrollOffset(
      ScrollOffset(0, 900.75), mojom::blink::ScrollType::kProgrammatic);
  inputBox->scrollIntoViewIfNeeded(false);

  if (RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled()) {
    EXPECT_EQ(ScrollOffset(0, 900.75),
              layout_viewport_scrollable_area->GetScrollOffset());
  } else {
    EXPECT_EQ(ScrollOffset(0, 900),
              layout_viewport_scrollable_area->GetScrollOffset());
  }
  EXPECT_EQ(ScrollOffset(250.25f, 100.25f), visual_viewport.GetScrollOffset());

  // Change the fractional part of the frameview to one that would round down.
  layout_viewport_scrollable_area->SetScrollOffset(
      ScrollOffset(0, 900.125), mojom::blink::ScrollType::kProgrammatic);
  inputBox->scrollIntoViewIfNeeded(false);

  if (RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled()) {
    EXPECT_EQ(ScrollOffset(0, 900.125),
              layout_viewport_scrollable_area->GetScrollOffset());
  } else {
    EXPECT_EQ(ScrollOffset(0, 900),
              layout_viewport_scrollable_area->GetScrollOffset());
  }
  EXPECT_EQ(ScrollOffset(250.25f, 100.25f), visual_viewport.GetScrollOffset());

  // Repeat both tests above with the visual viewport at a high fractional.
  WebView()->SetVisualViewportOffset(gfx::PointF(250.875f, 100.875f));
  layout_viewport_scrollable_area->SetScrollOffset(
      ScrollOffset(0, 900.75), mojom::blink::ScrollType::kProgrammatic);
  inputBox->scrollIntoViewIfNeeded(false);

  if (RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled()) {
    EXPECT_EQ(ScrollOffset(0, 900.75),
              layout_viewport_scrollable_area->GetScrollOffset());
  } else {
    EXPECT_EQ(ScrollOffset(0, 900),
              layout_viewport_scrollable_area->GetScrollOffset());
  }
  EXPECT_EQ(ScrollOffset(250.875f, 100.875f),
            visual_viewport.GetScrollOffset());

  // Change the fractional part of the frameview to one that would round down.
  layout_viewport_scrollable_area->SetScrollOffset(
      ScrollOffset(0, 900.125), mojom::blink::ScrollType::kProgrammatic);
  inputBox->scrollIntoViewIfNeeded(false);

  if (RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled()) {
    EXPECT_EQ(ScrollOffset(0, 900.125),
              layout_viewport_scrollable_area->GetScrollOffset());
  } else {
    EXPECT_EQ(ScrollOffset(0, 900),
              layout_viewport_scrollable_area->GetScrollOffset());
  }
  EXPECT_EQ(ScrollOffset(250.875f, 100.875f),
            visual_viewport.GetScrollOffset());

  // Both viewports with a 0.5 fraction.
  WebView()->SetVisualViewportOffset(gfx::PointF(250.5f, 100.5f));
  layout_viewport_scrollable_area->SetScrollOffset(
      ScrollOffset(0, 900.5), mojom::blink::ScrollType::kProgrammatic);
  inputBox->scrollIntoViewIfNeeded(false);

  if (RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled()) {
    EXPECT_EQ(ScrollOffset(0, 900.5),
              layout_viewport_scrollable_area->GetScrollOffset());
  } else {
    EXPECT_EQ(ScrollOffset(0, 900),
              layout_viewport_scrollable_area->GetScrollOffset());
  }
  EXPECT_EQ(ScrollOffset(250.5f, 100.5f), visual_viewport.GetScrollOffset());
}

static ScrollOffset expectedMaxLayoutViewportScrollOffset(
    VisualViewport& visual_viewport,
    LocalFrameView& frame_view) {
  float aspect_ratio = visual_viewport.VisibleRect().width() /
                       visual_viewport.VisibleRect().height();
  float new_height = frame_view.FrameRect().width() / aspect_ratio;
  gfx::Size contents_size = frame_view.LayoutViewport()->ContentsSize();
  return ScrollOffset(contents_size.width() - frame_view.FrameRect().width(),
                      contents_size.height() - new_height);
}

TEST_P(VisualViewportTest, TestBrowserControlsAdjustment) {
  InitializeWithAndroidSettings();
  WebView()->ResizeWithBrowserControls(gfx::Size(500, 450), 20, 0, false);
  UpdateAllLifecyclePhases();

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");
  UpdateAllLifecyclePhases();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();

  visual_viewport.SetScale(1);
  EXPECT_EQ(gfx::SizeF(500, 450), visual_viewport.VisibleRect().size());
  EXPECT_EQ(gfx::Size(1000, 900), frame_view.FrameRect().size());

  // Simulate bringing down the browser controls by 20px.
  WebView()->MainFrameViewWidget()->ApplyViewportChangesForTesting(
      {gfx::Vector2dF(), gfx::Vector2dF(), 1, false, 1, 0,
       cc::BrowserControlsState::kBoth});
  EXPECT_EQ(gfx::SizeF(500, 430), visual_viewport.VisibleRect().size());

  // Test that the scroll bounds are adjusted appropriately: the visual viewport
  // should be shrunk by 20px to 430px. The outer viewport was shrunk to
  // maintain the
  // aspect ratio so it's height is 860px.
  visual_viewport.Move(ScrollOffset(10000, 10000));
  EXPECT_EQ(ScrollOffset(500, 860 - 430), visual_viewport.GetScrollOffset());

  // The outer viewport (LocalFrameView) should be affected as well.
  frame_view.LayoutViewport()->ScrollBy(ScrollOffset(10000, 10000),
                                        mojom::blink::ScrollType::kUser);
  EXPECT_EQ(expectedMaxLayoutViewportScrollOffset(visual_viewport, frame_view),
            frame_view.LayoutViewport()->GetScrollOffset());

  // Simulate bringing up the browser controls by 10.5px.
  WebView()->MainFrameViewWidget()->ApplyViewportChangesForTesting(
      {gfx::Vector2dF(), gfx::Vector2dF(), 1, false, -10.5f / 20, 0,
       cc::BrowserControlsState::kBoth});
  EXPECT_SIZEF_EQ(gfx::SizeF(500, 440.5f),
                  visual_viewport.VisibleRect().size());

  // maximumScrollPosition |ceil|s the browser controls adjustment.
  visual_viewport.Move(ScrollOffset(10000, 10000));
  EXPECT_VECTOR2DF_EQ(ScrollOffset(500, 881 - 441),
                      visual_viewport.GetScrollOffset());

  // The outer viewport (LocalFrameView) should be affected as well.
  frame_view.LayoutViewport()->ScrollBy(ScrollOffset(10000, 10000),
                                        mojom::blink::ScrollType::kUser);
  EXPECT_EQ(expectedMaxLayoutViewportScrollOffset(visual_viewport, frame_view),
            frame_view.LayoutViewport()->GetScrollOffset());
}

TEST_P(VisualViewportTest, TestBrowserControlsAdjustmentWithScale) {
  InitializeWithAndroidSettings();
  WebView()->ResizeWithBrowserControls(gfx::Size(500, 450), 20, 0, false);
  UpdateAllLifecyclePhases();

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");
  UpdateAllLifecyclePhases();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();

  visual_viewport.SetScale(2);
  EXPECT_EQ(gfx::SizeF(250, 225), visual_viewport.VisibleRect().size());
  EXPECT_EQ(gfx::Size(1000, 900), frame_view.FrameRect().size());

  // Simulate bringing down the browser controls by 20px. Since we're zoomed in,
  // the browser controls take up half as much space (in document-space) than
  // they do at an unzoomed level.
  WebView()->MainFrameViewWidget()->ApplyViewportChangesForTesting(
      {gfx::Vector2dF(), gfx::Vector2dF(), 1, false, 1, 0,
       cc::BrowserControlsState::kBoth});
  EXPECT_EQ(gfx::SizeF(250, 215), visual_viewport.VisibleRect().size());

  // Test that the scroll bounds are adjusted appropriately.
  visual_viewport.Move(ScrollOffset(10000, 10000));
  EXPECT_EQ(ScrollOffset(750, 860 - 215), visual_viewport.GetScrollOffset());

  // The outer viewport (LocalFrameView) should be affected as well.
  frame_view.LayoutViewport()->ScrollBy(ScrollOffset(10000, 10000),
                                        mojom::blink::ScrollType::kUser);
  ScrollOffset expected =
      expectedMaxLayoutViewportScrollOffset(visual_viewport, frame_view);
  EXPECT_EQ(expected, frame_view.LayoutViewport()->GetScrollOffset());

  // Scale back out, LocalFrameView max scroll shouldn't have changed. Visual
  // viewport should be moved up to accommodate larger view.
  WebView()->MainFrameViewWidget()->ApplyViewportChangesForTesting(
      {gfx::Vector2dF(), gfx::Vector2dF(), 0.5f, false, 0, 0,
       cc::BrowserControlsState::kBoth});
  EXPECT_EQ(1, visual_viewport.Scale());
  EXPECT_EQ(expected, frame_view.LayoutViewport()->GetScrollOffset());
  frame_view.LayoutViewport()->ScrollBy(ScrollOffset(10000, 10000),
                                        mojom::blink::ScrollType::kUser);
  EXPECT_EQ(expected, frame_view.LayoutViewport()->GetScrollOffset());

  EXPECT_EQ(ScrollOffset(500, 860 - 430), visual_viewport.GetScrollOffset());
  visual_viewport.Move(ScrollOffset(10000, 10000));
  EXPECT_EQ(ScrollOffset(500, 860 - 430), visual_viewport.GetScrollOffset());

  // Scale out, use a scale that causes fractional rects.
  WebView()->MainFrameViewWidget()->ApplyViewportChangesForTesting(
      {gfx::Vector2dF(), gfx::Vector2dF(), 0.8f, false, -1, 0,
       cc::BrowserControlsState::kBoth});
  EXPECT_EQ(gfx::SizeF(625, 562.5), visual_viewport.VisibleRect().size());

  // Bring out the browser controls by 11
  WebView()->MainFrameViewWidget()->ApplyViewportChangesForTesting(
      {gfx::Vector2dF(), gfx::Vector2dF(), 1, false, 11 / 20.f, 0,
       cc::BrowserControlsState::kBoth});
  EXPECT_EQ(gfx::SizeF(625, 548.75), visual_viewport.VisibleRect().size());

  // Ensure max scroll offsets are updated properly.
  visual_viewport.Move(ScrollOffset(10000, 10000));
  EXPECT_VECTOR2DF_EQ(ScrollOffset(375, 877.5 - 548.75),
                      visual_viewport.GetScrollOffset());

  frame_view.LayoutViewport()->ScrollBy(ScrollOffset(10000, 10000),
                                        mojom::blink::ScrollType::kUser);
  EXPECT_EQ(expectedMaxLayoutViewportScrollOffset(visual_viewport, frame_view),
            frame_view.LayoutViewport()->GetScrollOffset());
}

// Tests that a scroll all the way to the bottom of the page, while hiding the
// browser controls doesn't cause a clamp in the viewport scroll offset when the
// top controls initiated resize occurs.
TEST_P(VisualViewportTest, TestBrowserControlsAdjustmentAndResize) {
  int browser_controls_height = 20;
  int visual_viewport_height = 450;
  int layout_viewport_height = 900;
  float page_scale = 2;
  float min_page_scale = 0.5;

  InitializeWithAndroidSettings();

  // Initialize with browser controls showing and shrinking the Blink size.
  cc::BrowserControlsParams controls;
  controls.top_controls_height = browser_controls_height;
  controls.browser_controls_shrink_blink_size = true;
  // TODO(danakj): The browser (RenderWidgetHostImpl) doesn't shrink the widget
  // size by the browser controls, only the visible_viewport_size, but this test
  // shrinks and grows both.
  WebView()->ResizeWithBrowserControls(
      gfx::Size(500, visual_viewport_height - browser_controls_height),
      gfx::Size(500, visual_viewport_height - browser_controls_height),
      controls);
  UpdateAllLifecyclePhases();
  WebView()->GetBrowserControls().SetShownRatio(1, 0);

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");
  UpdateAllLifecyclePhases();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();

  visual_viewport.SetScale(page_scale);
  EXPECT_EQ(gfx::SizeF(250, (visual_viewport_height - browser_controls_height) /
                                page_scale),
            visual_viewport.VisibleRect().size());
  EXPECT_EQ(gfx::Size(1000, layout_viewport_height -
                                browser_controls_height / min_page_scale),
            frame_view.FrameRect().size());
  EXPECT_EQ(gfx::Size(500, visual_viewport_height - browser_controls_height),
            visual_viewport.Size());

  // Scroll all the way to the bottom, hiding the browser controls in the
  // process.
  visual_viewport.Move(ScrollOffset(10000, 10000));
  frame_view.LayoutViewport()->ScrollBy(ScrollOffset(10000, 10000),
                                        mojom::blink::ScrollType::kUser);
  WebView()->GetBrowserControls().SetShownRatio(0, 0);

  EXPECT_EQ(gfx::SizeF(250, visual_viewport_height / page_scale),
            visual_viewport.VisibleRect().size());

  ScrollOffset frame_view_expected =
      expectedMaxLayoutViewportScrollOffset(visual_viewport, frame_view);
  ScrollOffset visual_viewport_expected = ScrollOffset(
      750, layout_viewport_height - visual_viewport_height / page_scale);

  EXPECT_EQ(visual_viewport_expected, visual_viewport.GetScrollOffset());
  EXPECT_EQ(frame_view_expected,
            frame_view.LayoutViewport()->GetScrollOffset());

  ScrollOffset total_expected = visual_viewport_expected + frame_view_expected;

  // Resize the widget and visible viewport to match the browser controls
  // adjustment. Ensure that the total offset (i.e. what the user sees) doesn't
  // change because of clamping the offsets to valid values.
  controls.browser_controls_shrink_blink_size = false;
  WebView()->ResizeWithBrowserControls(gfx::Size(500, visual_viewport_height),
                                       gfx::Size(500, visual_viewport_height),
                                       controls);
  UpdateAllLifecyclePhases();

  EXPECT_EQ(gfx::Size(500, visual_viewport_height), visual_viewport.Size());
  EXPECT_EQ(gfx::SizeF(250, visual_viewport_height / page_scale),
            visual_viewport.VisibleRect().size());
  EXPECT_EQ(gfx::Size(1000, layout_viewport_height),
            frame_view.FrameRect().size());

  EXPECT_EQ(total_expected, visual_viewport.GetScrollOffset() +
                                frame_view.LayoutViewport()->GetScrollOffset());

  EXPECT_EQ(visual_viewport_expected, visual_viewport.GetScrollOffset());
  EXPECT_EQ(frame_view_expected,
            frame_view.LayoutViewport()->GetScrollOffset());
}

// Tests that a scroll all the way to the bottom while showing the browser
// controls doesn't cause a clamp to the viewport scroll offset when the browser
// controls initiated resize occurs.
TEST_P(VisualViewportTest, TestBrowserControlsShrinkAdjustmentAndResize) {
  int browser_controls_height = 20;
  int visual_viewport_height = 500;
  int layout_viewport_height = 1000;
  int content_height = 2000;
  float page_scale = 2;
  float min_page_scale = 0.5;

  InitializeWithAndroidSettings();

  // Initialize with browser controls hidden and not shrinking the Blink size.
  WebView()->ResizeWithBrowserControls(gfx::Size(500, visual_viewport_height),
                                       20, 0, false);
  UpdateAllLifecyclePhases();
  WebView()->GetBrowserControls().SetShownRatio(0, 0);

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");
  UpdateAllLifecyclePhases();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();

  visual_viewport.SetScale(page_scale);
  EXPECT_EQ(gfx::SizeF(250, visual_viewport_height / page_scale),
            visual_viewport.VisibleRect().size());
  EXPECT_EQ(gfx::Size(1000, layout_viewport_height),
            frame_view.FrameRect().size());
  EXPECT_EQ(gfx::Size(500, visual_viewport_height), visual_viewport.Size());

  // Scroll all the way to the bottom, showing the the browser controls in the
  // process. (This could happen via window.scrollTo during a scroll, for
  // example).
  WebView()->GetBrowserControls().SetShownRatio(1, 0);
  visual_viewport.Move(ScrollOffset(10000, 10000));
  frame_view.LayoutViewport()->ScrollBy(ScrollOffset(10000, 10000),
                                        mojom::blink::ScrollType::kUser);

  EXPECT_EQ(gfx::SizeF(250, (visual_viewport_height - browser_controls_height) /
                                page_scale),
            visual_viewport.VisibleRect().size());

  ScrollOffset frame_view_expected(
      0, content_height - (layout_viewport_height -
                           browser_controls_height / min_page_scale));
  ScrollOffset visual_viewport_expected = ScrollOffset(
      750, (layout_viewport_height - browser_controls_height / min_page_scale -
            visual_viewport.VisibleRect().height()));

  EXPECT_EQ(visual_viewport_expected, visual_viewport.GetScrollOffset());
  EXPECT_EQ(frame_view_expected,
            frame_view.LayoutViewport()->GetScrollOffset());

  ScrollOffset total_expected = visual_viewport_expected + frame_view_expected;

  // Resize the widget to match the browser controls adjustment. Ensure that the
  // total offset (i.e. what the user sees) doesn't change because of clamping
  // the offsets to valid values.
  WebView()->ResizeWithBrowserControls(
      gfx::Size(500, visual_viewport_height - browser_controls_height), 20, 0,
      true);
  UpdateAllLifecyclePhases();

  EXPECT_EQ(gfx::Size(500, visual_viewport_height - browser_controls_height),
            visual_viewport.Size());
  EXPECT_EQ(gfx::SizeF(250, (visual_viewport_height - browser_controls_height) /
                                page_scale),
            visual_viewport.VisibleRect().size());
  EXPECT_EQ(gfx::Size(1000, layout_viewport_height -
                                browser_controls_height / min_page_scale),
            frame_view.FrameRect().size());
  EXPECT_EQ(total_expected, visual_viewport.GetScrollOffset() +
                                frame_view.LayoutViewport()->GetScrollOffset());
}

// Tests that a resize due to browser controls hiding doesn't incorrectly clamp
// the main frame's scroll offset. crbug.com/428193.
TEST_P(VisualViewportTest, TestTopControlHidingResizeDoesntClampMainFrame) {
  InitializeWithAndroidSettings();
  WebView()->ResizeWithBrowserControls(
      gfx::Size(WebView()->MainFrameViewWidget()->Size()), 500, 0, false);
  UpdateAllLifecyclePhases();
  WebView()->MainFrameViewWidget()->ApplyViewportChangesForTesting(
      {gfx::Vector2dF(), gfx::Vector2dF(), 1, false, 1, 0,
       cc::BrowserControlsState::kBoth});
  WebView()->ResizeWithBrowserControls(gfx::Size(1000, 1000), 500, 0, true);
  UpdateAllLifecyclePhases();

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");
  UpdateAllLifecyclePhases();

  // Scroll the LocalFrameView to the bottom of the page but "hide" the browser
  // controls on the compositor side so the max scroll position should account
  // for the full viewport height.
  WebView()->MainFrameViewWidget()->ApplyViewportChangesForTesting(
      {gfx::Vector2dF(), gfx::Vector2dF(), 1, false, -1, 0,
       cc::BrowserControlsState::kBoth});
  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();
  frame_view.LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 10000), mojom::blink::ScrollType::kProgrammatic);
  EXPECT_EQ(500, frame_view.LayoutViewport()->GetScrollOffset().y());

  // Now send the resize, make sure the scroll offset doesn't change.
  WebView()->ResizeWithBrowserControls(gfx::Size(1000, 1500), 500, 0, false);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(500, frame_view.LayoutViewport()->GetScrollOffset().y());
}

static void ConfigureHiddenScrollbarsSettings(WebSettings* settings) {
  VisualViewportTest::ConfigureAndroidSettings(settings);
  settings->SetHideScrollbars(true);
}

// Tests that scrollbar layers are not attached to the inner viewport container
// layer when hideScrollbars WebSetting is true.
TEST_P(VisualViewportTest,
       TestScrollbarsNotAttachedWhenHideScrollbarsSettingIsTrue) {
  InitializeWithAndroidSettings(ConfigureHiddenScrollbarsSettings);
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(100, 150));
  NavigateTo("about:blank");

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_FALSE(visual_viewport.LayerForHorizontalScrollbar());
  EXPECT_FALSE(visual_viewport.LayerForVerticalScrollbar());
}

// Tests that scrollbar layers are attached to the inner viewport container
// layer when hideScrollbars WebSetting is false.
TEST_P(VisualViewportTest,
       TestScrollbarsAttachedWhenHideScrollbarsSettingIsFalse) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(100, 150));
  UpdateAllLifecyclePhases();
  NavigateTo("about:blank");

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_TRUE(visual_viewport.LayerForHorizontalScrollbar());
  EXPECT_TRUE(visual_viewport.LayerForVerticalScrollbar());
}

// Tests that the layout viewport's scroll node bounds are updated.
// crbug.com/423188.
TEST_P(VisualViewportTest, TestChangingContentSizeAffectsScrollBounds) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(100, 150));

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");

  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();

  WebView()->MainFrameImpl()->ExecuteScript(
      WebScriptSource("var content = document.getElementById(\"content\");"
                      "content.style.width = \"1500px\";"
                      "content.style.height = \"2400px\";"));
  UpdateAllLifecyclePhases();

  const auto* scroll_node =
      frame_view.GetLayoutView()->FirstFragment().PaintProperties()->Scroll();
  float scale = GetFrame()->GetPage()->GetVisualViewport().Scale();
  EXPECT_EQ(gfx::Size(100 / scale, 150 / scale),
            scroll_node->ContainerRect().size());
  EXPECT_EQ(gfx::Rect(0, 0, 1500, 2400), scroll_node->ContentsRect());
}

// Tests that resizing the visual viepwort keeps its bounds within the outer
// viewport.
TEST_P(VisualViewportTest, ResizeVisualViewportStaysWithinOuterViewport) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(100, 200));

  NavigateTo("about:blank");
  UpdateAllLifecyclePhases();

  WebView()->ResizeVisualViewport(gfx::Size(100, 100));

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.Move(ScrollOffset(0, 100));

  EXPECT_EQ(100, visual_viewport.GetScrollOffset().y());

  WebView()->ResizeVisualViewport(gfx::Size(100, 200));

  EXPECT_EQ(0, visual_viewport.GetScrollOffset().y());
}

TEST_P(VisualViewportTest, ElementBoundsInWidgetSpaceAccountsForViewport) {
  InitializeWithAndroidSettings();

  WebView()->MainFrameViewWidget()->Resize(gfx::Size(500, 800));

  RegisterMockedHttpURLLoad("pinch-viewport-input-field.html");
  NavigateTo(base_url_ + "pinch-viewport-input-field.html");

  To<LocalFrame>(WebView()->GetPage()->MainFrame())->SetInitialFocus(false);
  Element* input_element = WebView()->FocusedElement();

  gfx::Rect bounds =
      input_element->GetLayoutObject()->AbsoluteBoundingBoxRect();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  gfx::Vector2dF scroll_delta(250, 400);
  visual_viewport.SetScale(2);
  visual_viewport.SetLocation(gfx::PointAtOffsetFromOrigin(scroll_delta));

  const gfx::Rect bounds_in_viewport = input_element->BoundsInWidget();
  gfx::Rect expected_bounds = gfx::ScaleToRoundedRect(bounds, 2.f);
  gfx::Vector2dF expected_scroll_delta = scroll_delta;
  expected_scroll_delta.Scale(2.f, 2.f);

  EXPECT_EQ(gfx::ToRoundedPoint(gfx::PointF(expected_bounds.origin()) -
                                expected_scroll_delta),
            bounds_in_viewport.origin());
  EXPECT_EQ(expected_bounds.size(), bounds_in_viewport.size());
}

// Test that the various window.scroll and document.body.scroll properties and
// methods don't change with the visual viewport.
TEST_P(VisualViewportTest, visualViewportIsInert) {
  WebViewImpl* web_view_impl = helper_.InitializeWithAndroidSettings();

  web_view_impl->MainFrameViewWidget()->Resize(gfx::Size(200, 300));

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(
      web_view_impl->MainFrameImpl(),
      "<!DOCTYPE html>"
      "<meta name='viewport' content='width=200,minimum-scale=1'>"
      "<style>"
      "  body {"
      "    width: 800px;"
      "    height: 800px;"
      "    margin: 0;"
      "  }"
      "</style>",
      base_url);
  UpdateAllLifecyclePhases();
  LocalDOMWindow* window =
      web_view_impl->MainFrameImpl()->GetFrame()->DomWindow();
  auto* html = To<HTMLHtmlElement>(window->document()->documentElement());

  ASSERT_EQ(200, window->innerWidth());
  ASSERT_EQ(300, window->innerHeight());
  ASSERT_EQ(200, html->clientWidth());
  ASSERT_EQ(300, html->clientHeight());

  VisualViewport& visual_viewport = web_view_impl->MainFrameImpl()
                                        ->GetFrame()
                                        ->GetPage()
                                        ->GetVisualViewport();
  visual_viewport.SetScale(2);

  ASSERT_EQ(100, visual_viewport.VisibleRect().width());
  ASSERT_EQ(150, visual_viewport.VisibleRect().height());

  EXPECT_EQ(200, window->innerWidth());
  EXPECT_EQ(300, window->innerHeight());
  EXPECT_EQ(200, html->clientWidth());
  EXPECT_EQ(300, html->clientHeight());

  visual_viewport.SetScrollOffset(
      ScrollOffset(10, 15), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kInstant, ScrollableArea::ScrollCallback());

  ASSERT_EQ(10, visual_viewport.GetScrollOffset().x());
  ASSERT_EQ(15, visual_viewport.GetScrollOffset().y());
  EXPECT_EQ(0, window->scrollX());
  EXPECT_EQ(0, window->scrollY());

  html->setScrollLeft(5);
  html->setScrollTop(30);
  EXPECT_EQ(5, html->scrollLeft());
  EXPECT_EQ(30, html->scrollTop());
  EXPECT_EQ(10, visual_viewport.GetScrollOffset().x());
  EXPECT_EQ(15, visual_viewport.GetScrollOffset().y());

  html->setScrollLeft(5000);
  html->setScrollTop(5000);
  EXPECT_EQ(600, html->scrollLeft());
  EXPECT_EQ(500, html->scrollTop());
  EXPECT_EQ(10, visual_viewport.GetScrollOffset().x());
  EXPECT_EQ(15, visual_viewport.GetScrollOffset().y());

  html->setScrollLeft(0);
  html->setScrollTop(0);
  EXPECT_EQ(0, html->scrollLeft());
  EXPECT_EQ(0, html->scrollTop());
  EXPECT_EQ(10, visual_viewport.GetScrollOffset().x());
  EXPECT_EQ(15, visual_viewport.GetScrollOffset().y());

  window->scrollTo(5000, 5000);
  EXPECT_EQ(600, html->scrollLeft());
  EXPECT_EQ(500, html->scrollTop());
  EXPECT_EQ(10, visual_viewport.GetScrollOffset().x());
  EXPECT_EQ(15, visual_viewport.GetScrollOffset().y());
}

// Tests that when a new frame is created, it is created with the intended size
// (i.e. viewport at minimum scale, 100x200 / 0.5).
TEST_P(VisualViewportTest, TestMainFrameInitializationSizing) {
  InitializeWithAndroidSettings();

  WebView()->MainFrameViewWidget()->Resize(gfx::Size(100, 200));

  RegisterMockedHttpURLLoad("content-width-1000-min-scale.html");
  NavigateTo(base_url_ + "content-width-1000-min-scale.html");

  WebLocalFrameImpl* local_frame = WebView()->MainFrameImpl();
  // The shutdown() calls are a hack to prevent this test from violating
  // invariants about frame state during navigation/detach.
  local_frame->GetFrame()->GetDocument()->Shutdown();
  local_frame->CreateFrameView();

  LocalFrameView& frame_view = *local_frame->GetFrameView();
  EXPECT_EQ(gfx::Size(200, 400), frame_view.FrameRect().size());
  frame_view.Dispose();
}

// Tests that the maximum scroll offset of the viewport can be fractional.
TEST_P(VisualViewportTest, FractionalMaxScrollOffset) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(101, 201));
  NavigateTo("about:blank");

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  ScrollableArea* scrollable_area = &visual_viewport;

  WebView()->SetPageScaleFactor(1.0);
  EXPECT_EQ(ScrollOffset(), scrollable_area->MaximumScrollOffset());

  WebView()->SetPageScaleFactor(2);
  EXPECT_EQ(ScrollOffset(101. / 2., 201. / 2.),
            scrollable_area->MaximumScrollOffset());
}

// Tests that the scroll offset is consistent when scale specified.
TEST_P(VisualViewportTest, MaxScrollOffsetAtScale) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(101, 201));
  NavigateTo("about:blank");

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();

  WebView()->SetPageScaleFactor(0.1);
  EXPECT_EQ(ScrollOffset(), visual_viewport.MaximumScrollOffsetAtScale(1.0));

  WebView()->SetPageScaleFactor(2);
  EXPECT_EQ(ScrollOffset(), visual_viewport.MaximumScrollOffsetAtScale(1.0));

  WebView()->SetPageScaleFactor(5);
  EXPECT_EQ(ScrollOffset(), visual_viewport.MaximumScrollOffsetAtScale(1.0));

  WebView()->SetPageScaleFactor(10);
  EXPECT_EQ(ScrollOffset(101. / 2., 201. / 2.),
            visual_viewport.MaximumScrollOffsetAtScale(2.0));
}

TEST_P(VisualViewportTest, AccessibilityHitTestWhileZoomedIn) {
  InitializeWithDesktopSettings();

  RegisterMockedHttpURLLoad("hit-test.html");
  NavigateTo(base_url_ + "hit-test.html");

  WebView()->MainFrameViewWidget()->Resize(gfx::Size(500, 500));
  UpdateAllLifecyclePhases();

  WebDocument web_doc = WebView()->MainFrameImpl()->GetDocument();
  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();

  WebAXContext ax_context(web_doc, ui::kAXModeComplete);

  WebView()->SetPageScaleFactor(2);
  WebView()->SetVisualViewportOffset(gfx::PointF(200, 230));
  frame_view.LayoutViewport()->SetScrollOffset(
      ScrollOffset(400, 1100), mojom::blink::ScrollType::kProgrammatic);

  // FIXME(504057): PaintLayerScrollableArea dirties the compositing state.
  ForceFullCompositingUpdate();

  // Because of where the visual viewport is located, this should hit the bottom
  // right target (target 4).
  WebAXObject hitNode =
      WebAXObject::FromWebDocument(web_doc).HitTest(gfx::Point(154, 165));
  ax::mojom::NameFrom name_from;
  WebVector<WebAXObject> name_objects;
  EXPECT_EQ(std::string("Target4"),
            hitNode.GetName(name_from, name_objects).Utf8());
}

// Tests that the maximum scroll offset of the viewport can be fractional.
TEST_P(VisualViewportTest, TestCoordinateTransforms) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");

  VisualViewport& visual_viewport = WebView()->GetPage()->GetVisualViewport();
  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();

  // At scale = 1 the transform should be a no-op.
  visual_viewport.SetScale(1);
  EXPECT_POINTF_EQ(gfx::PointF(314, 273),
                   visual_viewport.ViewportToRootFrame(gfx::PointF(314, 273)));
  EXPECT_POINTF_EQ(gfx::PointF(314, 273),
                   visual_viewport.RootFrameToViewport(gfx::PointF(314, 273)));

  // At scale = 2.
  visual_viewport.SetScale(2);
  EXPECT_POINTF_EQ(gfx::PointF(55, 75),
                   visual_viewport.ViewportToRootFrame(gfx::PointF(110, 150)));
  EXPECT_POINTF_EQ(gfx::PointF(110, 150),
                   visual_viewport.RootFrameToViewport(gfx::PointF(55, 75)));

  // At scale = 2 and with the visual viewport offset.
  visual_viewport.SetLocation(gfx::PointF(10, 12));
  EXPECT_POINTF_EQ(gfx::PointF(50, 62),
                   visual_viewport.ViewportToRootFrame(gfx::PointF(80, 100)));
  EXPECT_POINTF_EQ(gfx::PointF(80, 100),
                   visual_viewport.RootFrameToViewport(gfx::PointF(50, 62)));

  // Test points that will cause non-integer values.
  EXPECT_POINTF_EQ(gfx::PointF(50.5, 62.4),
                   visual_viewport.ViewportToRootFrame(gfx::PointF(81, 100.8)));
  EXPECT_POINTF_EQ(gfx::PointF(81, 100.8), visual_viewport.RootFrameToViewport(
                                               gfx::PointF(50.5, 62.4)));

  // Scrolling the main frame should have no effect.
  frame_view.LayoutViewport()->SetScrollOffset(
      ScrollOffset(100, 120), mojom::blink::ScrollType::kProgrammatic);
  EXPECT_POINTF_EQ(gfx::PointF(50, 62),
                   visual_viewport.ViewportToRootFrame(gfx::PointF(80, 100)));
  EXPECT_POINTF_EQ(gfx::PointF(80, 100),
                   visual_viewport.RootFrameToViewport(gfx::PointF(50, 62)));
}

// Tests that the window dimensions are available before a full layout occurs.
// More specifically, it checks that the innerWidth and innerHeight window
// properties will trigger a layout which will cause an update to viewport
// constraints and a refreshed initial scale. crbug.com/466718
TEST_P(VisualViewportTest, WindowDimensionsOnLoad) {
  InitializeWithAndroidSettings();
  RegisterMockedHttpURLLoad("window_dimensions.html");
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  NavigateTo(base_url_ + "window_dimensions.html");

  Element* output =
      GetFrame()->GetDocument()->getElementById(AtomicString("output"));
  DCHECK(output);
  EXPECT_EQ("1600x1200", output->innerHTML());
}

// Similar to above but make sure the initial scale is updated with the content
// width for a very wide page. That is, make that innerWidth/Height actually
// trigger a layout of the content, and not just an update of the viepwort.
// crbug.com/466718
TEST_P(VisualViewportTest, WindowDimensionsOnLoadWideContent) {
  InitializeWithAndroidSettings();
  RegisterMockedHttpURLLoad("window_dimensions_wide_div.html");
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  NavigateTo(base_url_ + "window_dimensions_wide_div.html");

  Element* output =
      GetFrame()->GetDocument()->getElementById(AtomicString("output"));
  DCHECK(output);
  EXPECT_EQ("2000x1500", output->innerHTML());
}

TEST_P(VisualViewportTest, ResizeWithScrollAnchoring) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(800, 600));

  RegisterMockedHttpURLLoad("icb-relative-content.html");
  NavigateTo(base_url_ + "icb-relative-content.html");

  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();
  frame_view.LayoutViewport()->SetScrollOffset(
      ScrollOffset(700, 500), mojom::blink::ScrollType::kProgrammatic);

  WebView()->MainFrameViewWidget()->Resize(gfx::Size(800, 300));
  UpdateAllLifecyclePhases();
  EXPECT_EQ(ScrollOffset(700, 200),
            frame_view.LayoutViewport()->GetScrollOffset());
}

// Make sure a composited background-attachment:fixed background gets resized
// by browser controls.
TEST_P(VisualViewportTest, ResizeCompositedAndFixedBackground) {
  WebViewImpl* web_view_impl = helper_.InitializeWithAndroidSettings();

  int page_width = 640;
  int page_height = 480;
  float browser_controls_height = 50.0f;
  int smallest_height = page_height - browser_controls_height;

  web_view_impl->ResizeWithBrowserControls(gfx::Size(page_width, page_height),
                                           browser_controls_height, 0, false);
  UpdateAllLifecyclePhases();

  RegisterMockedHttpURLLoad("http://example.com/foo.png", "white-1x1.png");
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view_impl->MainFrameImpl(),
                                     "<!DOCTYPE html>"
                                     "<style>"
                                     "  body {"
                                     "    background: url('foo.png');"
                                     "    background-attachment: fixed;"
                                     "    background-size: cover;"
                                     "    background-repeat: no-repeat;"
                                     "  }"
                                     "  div { height:1000px; width: 200px; }"
                                     "</style>"
                                     "<div></div>",
                                     base_url);

  UpdateAllLifecyclePhases();
  Document* document =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame())->GetDocument();
  VisualViewport& visual_viewport =
      web_view_impl->GetPage()->GetVisualViewport();
  auto* background_layer = visual_viewport.LayerForScrolling();
  ASSERT_TRUE(background_layer);

  ASSERT_EQ(page_width, background_layer->bounds().width());
  ASSERT_EQ(page_height, background_layer->bounds().height());
  ASSERT_EQ(page_width, document->View()->GetLayoutSize().width());
  ASSERT_EQ(smallest_height, document->View()->GetLayoutSize().height());

  web_view_impl->ResizeWithBrowserControls(
      gfx::Size(page_width, smallest_height), browser_controls_height, 0, true);
  UpdateAllLifecyclePhases();

  // The layout size should not have changed.
  ASSERT_EQ(page_width, document->View()->GetLayoutSize().width());
  ASSERT_EQ(smallest_height, document->View()->GetLayoutSize().height());

  // The background layer's size should have changed though.
  EXPECT_EQ(page_width, background_layer->bounds().width());
  EXPECT_EQ(smallest_height, background_layer->bounds().height());

  web_view_impl->ResizeWithBrowserControls(gfx::Size(page_width, page_height),
                                           browser_controls_height, 0, true);
  UpdateAllLifecyclePhases();

  // The background layer's size should change again.
  EXPECT_EQ(page_width, background_layer->bounds().width());
  EXPECT_EQ(page_height, background_layer->bounds().height());
}

static void ConfigureViewportNonCompositing(WebSettings* settings) {
  frame_test_helpers::WebViewHelper::UpdateAndroidCompositingSettings(settings);
  settings->SetLCDTextPreference(LCDTextPreference::kStronglyPreferred);
}

// Make sure a non-composited background-attachment:fixed background gets
// resized by browser controls.
TEST_P(VisualViewportTest, ResizeNonCompositedAndFixedBackground) {
  WebViewImpl* web_view_impl =
      helper_.InitializeWithSettings(&ConfigureViewportNonCompositing);

  int page_width = 640;
  int page_height = 480;
  float browser_controls_height = 50.0f;
  int smallest_height = page_height - browser_controls_height;

  web_view_impl->ResizeWithBrowserControls(gfx::Size(page_width, page_height),
                                           browser_controls_height, 0, false);
  UpdateAllLifecyclePhases();

  RegisterMockedHttpURLLoad("http://example.com/foo.png", "white-1x1.png");
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view_impl->MainFrameImpl(),
                                     "<!DOCTYPE html>"
                                     "<style>"
                                     "  body {"
                                     "    margin: 0px;"
                                     "    background: url('foo.png');"
                                     "    background-attachment: fixed;"
                                     "    background-size: cover;"
                                     "    background-repeat: no-repeat;"
                                     "  }"
                                     "  div { height:1000px; width: 200px; }"
                                     "</style>"
                                     "<div></div>",
                                     base_url);
  UpdateAllLifecyclePhases();
  Document* document =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame())->GetDocument();
  document->View()->SetTracksRasterInvalidations(true);
  web_view_impl->ResizeWithBrowserControls(
      gfx::Size(page_width, smallest_height), browser_controls_height, 0, true);
  UpdateAllLifecyclePhases();

  // The layout size should not have changed.
  ASSERT_EQ(page_width, document->View()->GetLayoutSize().width());
  ASSERT_EQ(smallest_height, document->View()->GetLayoutSize().height());

  // Fixed-attachment background is affected by viewport size.
  {
    const auto& raster_invalidations =
        GetRasterInvalidationTracking(*GetFrame()->View())->Invalidations();
    EXPECT_THAT(
        raster_invalidations,
        UnorderedElementsAre(RasterInvalidationInfo{
            ScrollingBackgroundClient(document).Id(),
            ScrollingBackgroundClient(document).DebugName(),
            gfx::Rect(0, 0, 640, 1000), PaintInvalidationReason::kBackground}));
  }

  document->View()->SetTracksRasterInvalidations(false);

  document->View()->SetTracksRasterInvalidations(true);
  web_view_impl->ResizeWithBrowserControls(gfx::Size(page_width, page_height),
                                           browser_controls_height, 0, true);
  UpdateAllLifecyclePhases();

  // Fixed-attachment background is affected by viewport size.
  {
    const auto& raster_invalidations =
        GetRasterInvalidationTracking(*GetFrame()->View())->Invalidations();
    EXPECT_THAT(
        raster_invalidations,
        UnorderedElementsAre(RasterInvalidationInfo{
            ScrollingBackgroundClient(document).Id(),
            ScrollingBackgroundClient(document).DebugName(),
            gfx::Rect(0, 0, 640, 1000), PaintInvalidationReason::kBackground}));
  }

  document->View()->SetTracksRasterInvalidations(false);
}

// Make sure a browser control resize with background-attachment:not-fixed
// background doesn't cause invalidation or layout.
TEST_P(VisualViewportTest, ResizeNonFixedBackgroundNoLayoutOrInvalidation) {
  WebViewImpl* web_view_impl = helper_.InitializeWithAndroidSettings();

  int page_width = 640;
  int page_height = 480;
  float browser_controls_height = 50.0f;
  int smallest_height = page_height - browser_controls_height;

  web_view_impl->ResizeWithBrowserControls(gfx::Size(page_width, page_height),
                                           browser_controls_height, 0, false);
  UpdateAllLifecyclePhases();

  RegisterMockedHttpURLLoad("http://example.com/foo.png", "white-1x1.png");
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  // This time the background is the default attachment.
  frame_test_helpers::LoadHTMLString(web_view_impl->MainFrameImpl(),
                                     "<!DOCTYPE html>"
                                     "<style>"
                                     "  body {"
                                     "    margin: 0px;"
                                     "    background: url('foo.png');"
                                     "    background-size: cover;"
                                     "    background-repeat: no-repeat;"
                                     "  }"
                                     "  div { height:1000px; width: 200px; }"
                                     "</style>"
                                     "<div></div>",
                                     base_url);
  UpdateAllLifecyclePhases();
  Document* document =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame())->GetDocument();

  // A resize will do a layout synchronously so manually check that we don't
  // setNeedsLayout from viewportSizeChanged.
  document->View()->ViewportSizeChanged();
  unsigned needs_layout_objects = 0;
  unsigned total_objects = 0;
  bool is_subtree = false;
  EXPECT_FALSE(document->View()->NeedsLayout());
  document->View()->CountObjectsNeedingLayout(needs_layout_objects,
                                              total_objects, is_subtree);
  EXPECT_EQ(0u, needs_layout_objects);

  UpdateAllLifecyclePhases();
  // Do a real resize to check for invalidations.
  document->View()->SetTracksRasterInvalidations(true);
  web_view_impl->ResizeWithBrowserControls(
      gfx::Size(page_width, smallest_height), browser_controls_height, 0, true);
  UpdateAllLifecyclePhases();

  // The layout size should not have changed.
  ASSERT_EQ(page_width, document->View()->GetLayoutSize().width());
  ASSERT_EQ(smallest_height, document->View()->GetLayoutSize().height());

  EXPECT_FALSE(
      GetRasterInvalidationTracking(*GetFrame()->View())->HasInvalidations());

  document->View()->SetTracksRasterInvalidations(false);
}

TEST_P(VisualViewportTest, InvalidateLayoutViewWhenDocumentSmallerThanView) {
  WebViewImpl* web_view_impl = helper_.InitializeWithAndroidSettings();

  int page_width = 320;
  int page_height = 590;
  float browser_controls_height = 50.0f;
  int largest_height = page_height + browser_controls_height;

  web_view_impl->ResizeWithBrowserControls(gfx::Size(page_width, page_height),
                                           browser_controls_height, 0, true);
  UpdateAllLifecyclePhases();

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view_impl->MainFrameImpl(),
                                     "<div style='height: 20px'>Text</div>",
                                     base_url);
  UpdateAllLifecyclePhases();
  Document* document =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame())->GetDocument();

  // Do a resize to check for invalidations.
  document->View()->SetTracksRasterInvalidations(true);
  web_view_impl->ResizeWithBrowserControls(
      gfx::Size(page_width, largest_height), browser_controls_height, 0, false);
  UpdateAllLifecyclePhases();

  // The layout size should not have changed.
  ASSERT_EQ(page_width, document->View()->GetLayoutSize().width());
  ASSERT_EQ(page_height, document->View()->GetLayoutSize().height());

  // Incremental raster invalidation is needed because the resize exposes
  // unpainted area of background.
  {
    const auto& raster_invalidations =
        GetRasterInvalidationTracking(*GetFrame()->View())->Invalidations();
    EXPECT_THAT(raster_invalidations,
                UnorderedElementsAre(RasterInvalidationInfo{
                    ScrollingBackgroundClient(document).Id(),
                    ScrollingBackgroundClient(document).DebugName(),
                    gfx::Rect(0, 590, 320, 50),
                    PaintInvalidationReason::kIncremental}));
  }

  document->View()->SetTracksRasterInvalidations(false);

  // Resize back to the original size.
  document->View()->SetTracksRasterInvalidations(true);
  web_view_impl->ResizeWithBrowserControls(gfx::Size(page_width, page_height),
                                           browser_controls_height, 0, false);
  UpdateAllLifecyclePhases();

  // No raster invalidation is needed because of no change within the root
  // scrolling layer.
  EXPECT_FALSE(
      GetRasterInvalidationTracking(*GetFrame()->View())->HasInvalidations());

  document->View()->SetTracksRasterInvalidations(false);
}

// Ensure we create transform node for overscroll elasticity properly.
TEST_P(VisualViewportTest, EnsureOverscrollElasticityTransformNode) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  NavigateTo("about:blank");
  UpdateAllLifecyclePhases();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_EQ(visual_viewport.GetOverscrollType() == OverscrollType::kTransform,
            !!visual_viewport.GetOverscrollElasticityTransformNode());

  visual_viewport.SetOverscrollTypeForTesting(OverscrollType::kNone);
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(visual_viewport.GetOverscrollElasticityTransformNode());

  visual_viewport.SetOverscrollTypeForTesting(OverscrollType::kTransform);
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(visual_viewport.GetOverscrollElasticityTransformNode());
}

// Ensure we create effect node for scrollbar properly.
TEST_P(VisualViewportTest, EnsureEffectNodeForScrollbars) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  NavigateTo("about:blank");
  UpdateAllLifecyclePhases();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  auto* vertical_scrollbar = visual_viewport.LayerForVerticalScrollbar();
  auto* horizontal_scrollbar = visual_viewport.LayerForHorizontalScrollbar();
  ASSERT_TRUE(vertical_scrollbar);
  ASSERT_TRUE(horizontal_scrollbar);

  auto& theme = ScrollbarThemeOverlayMobile::GetInstance();
  int scrollbar_thickness = theme.ScrollbarThickness(
      visual_viewport.ScaleFromDIP(), EScrollbarWidth::kAuto);

  EXPECT_EQ(vertical_scrollbar->effect_tree_index(),
            vertical_scrollbar->layer_tree_host()
                ->property_trees()
                ->effect_tree()
                .FindNodeFromElementId((visual_viewport.GetScrollbarElementId(
                    ScrollbarOrientation::kVerticalScrollbar)))
                ->id);
  EXPECT_EQ(vertical_scrollbar->offset_to_transform_parent(),
            gfx::Vector2dF(400 - scrollbar_thickness, 0));

  EXPECT_EQ(horizontal_scrollbar->effect_tree_index(),
            horizontal_scrollbar->layer_tree_host()
                ->property_trees()
                ->effect_tree()
                .FindNodeFromElementId(visual_viewport.GetScrollbarElementId(
                    ScrollbarOrientation::kHorizontalScrollbar))
                ->id);
  EXPECT_EQ(horizontal_scrollbar->offset_to_transform_parent(),
            gfx::Vector2dF(0, 400 - scrollbar_thickness));

  EXPECT_EQ(GetEffectNode(vertical_scrollbar)->parent_id,
            GetEffectNode(horizontal_scrollbar)->parent_id);
}

// Make sure we don't crash when the visual viewport's height is 0. This can
// happen transiently in autoresize mode and cause a crash. This test passes if
// it doesn't crash.
TEST_P(VisualViewportTest, AutoResizeNoHeightUsesMinimumHeight) {
  InitializeWithDesktopSettings();
  WebView()->ResizeWithBrowserControls(gfx::Size(0, 0), 0, 0, false);
  UpdateAllLifecyclePhases();
  WebView()->EnableAutoResizeMode(gfx::Size(25, 25), gfx::Size(100, 100));
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(WebView()->MainFrameImpl(),
                                     "<!DOCTYPE html>"
                                     "<style>"
                                     "  body {"
                                     "    margin: 0px;"
                                     "  }"
                                     "  div { height:110vh; width: 110vw; }"
                                     "</style>"
                                     "<div></div>",
                                     base_url);
}

// When a provisional frame is committed, it will get swapped in. At that
// point, the VisualViewport will be reset but the Document is in a detached
// state with no domWindow(). Ensure we correctly reset the viewport properties
// but don't crash trying to enqueue resize and scroll events in the document.
// https://crbug.com/1175916.
TEST_P(VisualViewportTest, SwapMainFrame) {
  InitializeWithDesktopSettings();

  WebView()->SetPageScaleFactor(2.0f);
  WebView()->SetVisualViewportOffset(gfx::PointF(10, 20));

  WebLocalFrame* local_frame =
      helper_.CreateProvisional(*helper_.LocalMainFrame());

  // Commit the provisional frame so it gets swapped in.
  RegisterMockedHttpURLLoad("200-by-300.html");
  frame_test_helpers::LoadFrame(local_frame, base_url_ + "200-by-300.html");

  EXPECT_EQ(WebView()->PageScaleFactor(), 1.0f);
  EXPECT_EQ(WebView()->VisualViewportOffset().x(), 0.0f);
  EXPECT_EQ(WebView()->VisualViewportOffset().y(), 0.0f);
}

// Similar to above but checks the case where a page is loaded such that it
// will zoom out as a result of loading and layout (i.e. loading a desktop page
// on Android).
TEST_P(VisualViewportTest, SwapMainFrameLoadZoomedOut) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(100, 150));

  WebLocalFrame* local_frame =
      helper_.CreateProvisional(*helper_.LocalMainFrame());

  // Commit the provisional frame so it gets swapped in.
  RegisterMockedHttpURLLoad("200-by-300.html");
  frame_test_helpers::LoadFrame(local_frame, base_url_ + "200-by-300.html");

  EXPECT_EQ(WebView()->PageScaleFactor(), 0.5f);
  EXPECT_EQ(WebView()->VisualViewportOffset().x(), 0.0f);
  EXPECT_EQ(WebView()->VisualViewportOffset().y(), 0.0f);
}

class VisualViewportSimTest : public SimTest {
 public:
  VisualViewportSimTest() {}

  void SetUp() override {
    SimTest::SetUp();
    frame_test_helpers::WebViewHelper::UpdateAndroidCompositingSettings(
        WebView().GetSettings());
    WebView().SetDefaultPageScaleLimits(0.25f, 5);
  }
};

// Test that we correctly size the visual viewport's scrolling contents layer
// when the layout viewport is smaller.
TEST_F(VisualViewportSimTest, ScrollingContentsSmallerThanContainer) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <meta name="viewport" content="width=320">
          <style>
            body {
              height: 2000px;
            }
          </style>
      )HTML");
  Compositor().BeginFrame();

  ASSERT_EQ(1.25f, WebView().MinimumPageScaleFactor());

  VisualViewport& visual_viewport = WebView().GetPage()->GetVisualViewport();
  EXPECT_EQ(gfx::Size(320, 480), visual_viewport.LayerForScrolling()->bounds());

  EXPECT_EQ(gfx::Rect(0, 0, 400, 600),
            visual_viewport.GetScrollNode()->ContainerRect());
  EXPECT_EQ(gfx::Rect(0, 0, 320, 480),
            visual_viewport.GetScrollNode()->ContentsRect());

  WebView().MainFrameViewWidget()->ApplyViewportChangesForTesting(
      {gfx::Vector2dF(1, 1), gfx::Vector2dF(), 2, false, 1, 0,
       cc::BrowserControlsState::kBoth});
  EXPECT_EQ(gfx::Size(320, 480), visual_viewport.LayerForScrolling()->bounds());

  EXPECT_EQ(gfx::Rect(0, 0, 400, 600),
            visual_viewport.GetScrollNode()->ContainerRect());
  EXPECT_EQ(gfx::Rect(0, 0, 320, 480),
            visual_viewport.GetScrollNode()->ContentsRect());
}

class VisualViewportScrollIntoViewTest
    : public VisualViewportSimTest,
      public ::testing::WithParamInterface<
          std::vector<base::test::FeatureRef>> {
 public:
  VisualViewportScrollIntoViewTest() {
    feature_list_.InitWithFeatures(
        GetParam(),
        /*disabled_features=*/std::vector<base::test::FeatureRef>());
  }

  void SetUp() override {
    VisualViewportSimTest::SetUp();

    // Setup a fixed-position element that's outside of an inset visual
    // viewport.
    WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 600));
    SimRequest request("https://example.com/test.html", "text/html");
    LoadURL("https://example.com/test.html");
    request.Complete(R"HTML(
              <!DOCTYPE html>
              <style>
               #bottom {
                    position: fixed;
                    bottom: 0;
                                width: 100%;
                                height: 20px;
                                text-align: center;
                }
              </style>
              <body>
                 <div id="bottom">Layout bottom</div>
              </body>
          )HTML");
    Compositor().BeginFrame();

    // Shrink the height such that the fixed element is now off screen.
    WebView().ResizeVisualViewport(gfx::Size(400, 600 - 100));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    VisualViewportScrollIntoViewTest,
    testing::Values(std::vector<base::test::FeatureRef>{},
                    std::vector<base::test::FeatureRef>{
                        features::kMultiSmoothScrollIntoView}));

TEST_P(VisualViewportScrollIntoViewTest, ScrollingToFixed) {
  VisualViewport& visual_viewport = WebView().GetPage()->GetVisualViewport();
  EXPECT_EQ(0.f, visual_viewport.GetScrollOffset().y());
  WebDocument web_doc = WebView().MainFrameImpl()->GetDocument();
  Element* bottom_element = web_doc.GetElementById("bottom");
  bool is_for_scroll_sequence =
      !RuntimeEnabledFeatures::MultiSmoothScrollIntoViewEnabled();
  auto scroll_params = scroll_into_view_util::CreateScrollIntoViewParams(
      ScrollAlignment::ToEdgeIfNeeded(), ScrollAlignment::ToEdgeIfNeeded(),
      mojom::blink::ScrollType::kProgrammatic,
      /*make_visible_in_visual_viewport=*/true,
      mojom::blink::ScrollBehavior::kInstant, is_for_scroll_sequence);
  if (is_for_scroll_sequence) {
    GetDocument().GetFrame()->CreateNewSmoothScrollSequence();
  }
  WebView().GetPage()->GetVisualViewport().ScrollIntoView(
      bottom_element->BoundingBox(), PhysicalBoxStrut(), scroll_params);
  if (is_for_scroll_sequence) {
    visual_viewport.GetSmoothScrollSequencer()->RunQueuedAnimations();
  }
  EXPECT_EQ(100.f, visual_viewport.GetScrollOffset().y());
}

TEST_P(VisualViewportScrollIntoViewTest, ScrollingToFixedFromJavascript) {
  VisualViewport& visual_viewport = WebView().GetPage()->GetVisualViewport();
  EXPECT_EQ(0.f, visual_viewport.GetScrollOffset().y());
  GetDocument().getElementById(AtomicString("bottom"))->scrollIntoView();
  EXPECT_EQ(100.f, visual_viewport.GetScrollOffset().y());
}

TEST_P(VisualViewportTest, DeviceEmulation) {
  InitializeWithAndroidSettings();

  WebView()->MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  NavigateTo("about:blank");
  UpdateAllLifecyclePhases();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_FALSE(visual_viewport.GetDeviceEmulationTransformNode());
  EXPECT_FALSE(
      GetFrame()->View()->VisualViewportOrOverlayNeedsRepaintForTesting());

  DeviceEmulationParams params;
  params.viewport_offset = gfx::PointF();
  params.viewport_scale = 1.f;
  WebView()->EnableDeviceEmulation(params);

  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(visual_viewport.GetDeviceEmulationTransformNode());
  EXPECT_FALSE(
      GetFrame()->View()->VisualViewportOrOverlayNeedsRepaintForTesting());
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(
      GetFrame()->View()->VisualViewportOrOverlayNeedsRepaintForTesting());

  // Set device mulation with viewport offset should repaint visual viewport.
  params.viewport_offset = gfx::PointF(314, 159);
  WebView()->EnableDeviceEmulation(params);

  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(
      GetFrame()->View()->VisualViewportOrOverlayNeedsRepaintForTesting());
  ASSERT_TRUE(visual_viewport.GetDeviceEmulationTransformNode());
  EXPECT_EQ(gfx::Transform::MakeTranslation(-params.viewport_offset.x(),
                                            -params.viewport_offset.y()),
            visual_viewport.GetDeviceEmulationTransformNode()->Matrix());
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(
      GetFrame()->View()->VisualViewportOrOverlayNeedsRepaintForTesting());

  // Change device emulation with scale should not repaint visual viewport.
  params.viewport_offset = gfx::PointF();
  params.viewport_scale = 1.5f;
  WebView()->EnableDeviceEmulation(params);

  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(
      GetFrame()->View()->VisualViewportOrOverlayNeedsRepaintForTesting());
  ASSERT_TRUE(visual_viewport.GetDeviceEmulationTransformNode());
  EXPECT_EQ(gfx::Transform::MakeScale(1.5f),
            visual_viewport.GetDeviceEmulationTransformNode()->Matrix());
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(
      GetFrame()->View()->VisualViewportOrOverlayNeedsRepaintForTesting());

  // Set an identity device emulation transform and ensure the transform
  // paint property node is cleared and repaint visual viewport.
  WebView()->EnableDeviceEmulation(DeviceEmulationParams());
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(
      GetFrame()->View()->VisualViewportOrOverlayNeedsRepaintForTesting());
  EXPECT_FALSE(visual_viewport.GetDeviceEmulationTransformNode());
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(
      GetFrame()->View()->VisualViewportOrOverlayNeedsRepaintForTesting());
}

TEST_P(VisualViewportTest, PaintScrollbar) {
  InitializeWithAndroidSettings();

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  frame_test_helpers::LoadHTMLString(WebView()->MainFrameImpl(),
                                     R"HTML(
        <!DOCTYPE html>"
        <meta name='viewport' content='width=device-width, initial-scale=1'>
        <body style='width: 2000px; height: 2000px'></body>
      )HTML",
                                     base_url);
  UpdateAllLifecyclePhases();

  auto check_scrollbar = [](const cc::Layer* scrollbar, float scale) {
    EXPECT_TRUE(scrollbar->draws_content());
    EXPECT_EQ(cc::HitTestOpaqueness::kTransparent,
              scrollbar->hit_test_opaqueness());
    EXPECT_TRUE(scrollbar->IsScrollbarLayerForTesting());
    EXPECT_EQ(
        cc::ScrollbarOrientation::kVertical,
        static_cast<const cc::ScrollbarLayerBase*>(scrollbar)->orientation());
    EXPECT_EQ(gfx::Size(7, 393), scrollbar->bounds());
    EXPECT_EQ(gfx::Vector2dF(393, 0), scrollbar->offset_to_transform_parent());

    // ScreenSpaceTransform is in the device emulation transform space, so it's
    // not affected by device emulation scale.
    gfx::Transform screen_space_transform;
    screen_space_transform.Translate(393, 0);
    EXPECT_EQ(screen_space_transform, scrollbar->ScreenSpaceTransform());

    gfx::Transform transform;
    transform.Scale(scale, scale);
    EXPECT_EQ(transform, scrollbar->layer_tree_host()
                             ->property_trees()
                             ->transform_tree()
                             .Node(scrollbar->transform_tree_index())
                             ->local);
  };

  // The last layer should be the vertical scrollbar.
  const cc::Layer* scrollbar =
      GetFrame()->View()->RootCcLayer()->children().back().get();
  check_scrollbar(scrollbar, 1.f);

  // Apply device emulation scale.
  DeviceEmulationParams params;
  params.viewport_offset = gfx::PointF();
  params.viewport_scale = 1.5f;
  WebView()->EnableDeviceEmulation(params);
  UpdateAllLifecyclePhases();
  ASSERT_EQ(scrollbar,
            GetFrame()->View()->RootCcLayer()->children().back().get());
  check_scrollbar(scrollbar, 1.5f);

  params.viewport_scale = 1.f;
  WebView()->EnableDeviceEmulation(params);
  UpdateAllLifecyclePhases();
  ASSERT_EQ(scrollbar,
            GetFrame()->View()->RootCcLayer()->children().back().get());
  check_scrollbar(scrollbar, 1.f);

  params.viewport_scale = 0.75f;
  WebView()->EnableDeviceEmulation(params);
  UpdateAllLifecyclePhases();
  ASSERT_EQ(scrollbar,
            GetFrame()->View()->RootCcLayer()->children().back().get());
  check_scrollbar(scrollbar, 0.75f);
}

// When a pinch-zoom occurs, the viewport scale and translation nodes can be
// directly updated without a PaintArtifactCompositor update.
TEST_P(VisualViewportTest, DirectPinchZoomPropertyUpdate) {
  InitializeWithAndroidSettings();

  RegisterMockedHttpURLLoad("200-by-800-viewport.html");
  NavigateTo(base_url_ + "200-by-800-viewport.html");

  WebView()->MainFrameViewWidget()->Resize(gfx::Size(100, 200));

  // Scroll visual viewport to the right edge of the frame
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.SetScaleAndLocation(2.f, true, gfx::PointF(150, 10));

  EXPECT_VECTOR2DF_EQ(ScrollOffset(150, 10), visual_viewport.GetScrollOffset());
  EXPECT_EQ(2.f, visual_viewport.Scale());
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Update the scale and location and ensure that a PaintArtifactCompositor
  // update is not required.
  visual_viewport.SetScaleAndLocation(3.f, true, gfx::PointF(120, 10));
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  EXPECT_VECTOR2DF_EQ(ScrollOffset(120, 10), visual_viewport.GetScrollOffset());
  EXPECT_EQ(3.f, visual_viewport.Scale());
}

// |TransformPaintPropertyNode::in_subtree_of_page_scale| should be false for
// the page scale transform node and all ancestors, and should be true for
// descendants of the page scale transform node.
TEST_P(VisualViewportTest, InSubtreeOfPageScale) {
  InitializeWithAndroidSettings();
  RegisterMockedHttpURLLoad("200-by-800-viewport.html");
  NavigateTo(base_url_ + "200-by-800-viewport.html");

  UpdateAllLifecyclePhases();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  const auto* page_scale = visual_viewport.GetPageScaleNode();
  // The page scale is not in its own subtree.
  EXPECT_FALSE(page_scale->IsInSubtreeOfPageScale());
  // Ancestors of the page scale are not in the page scale's subtree.
  for (const auto* ancestor = page_scale->UnaliasedParent(); ancestor;
       ancestor = ancestor->UnaliasedParent()) {
    EXPECT_FALSE(ancestor->IsInSubtreeOfPageScale());
  }

  const auto* view = GetFrame()->View()->GetLayoutView();
  const auto& view_contents_transform =
      view->FirstFragment().ContentsProperties().Transform();
  // Descendants of the page scale node should have |IsInSubtreeOfPageScale|.
  EXPECT_TRUE(ToUnaliased(view_contents_transform).IsInSubtreeOfPageScale());
  for (const auto* ancestor = view_contents_transform.UnaliasedParent();
       ancestor != page_scale; ancestor = ancestor->UnaliasedParent()) {
    EXPECT_TRUE(ancestor->IsInSubtreeOfPageScale());
  }
}

TEST_F(VisualViewportSimTest, UsedColorSchemeFromRootElement) {
  ColorSchemeHelper color_scheme_helper(*(WebView().GetPage()));
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 600));

  const VisualViewport& visual_viewport =
      WebView().GetPage()->GetVisualViewport();

  EXPECT_EQ(mojom::blink::ColorScheme::kLight,
            visual_viewport.UsedColorSchemeScrollbars());

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            html { color-scheme: dark }
          </style>
      )HTML");
  Compositor().BeginFrame();

  EXPECT_EQ(mojom::blink::ColorScheme::kDark,
            visual_viewport.UsedColorSchemeScrollbars());
}

TEST_F(VisualViewportSimTest, ScrollbarThumbColorFromRootElement) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 600));

  const VisualViewport& visual_viewport =
      WebView().GetPage()->GetVisualViewport();

  EXPECT_EQ(std::nullopt, visual_viewport.CSSScrollbarThumbColor());

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            html { scrollbar-color: rgb(255 0 0) transparent }
          </style>
      )HTML");
  Compositor().BeginFrame();

  EXPECT_EQ(blink::Color(255, 0, 0), visual_viewport.CSSScrollbarThumbColor());
}

TEST_P(VisualViewportTest, SetLocationBeforePrePaint) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(100, 100));
  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");

  // Simulate that the visual viewport is just created and FrameLoader is
  // restoring the previously saved scale and scroll state.
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.DisposeImpl();
  ASSERT_FALSE(visual_viewport.LayerForScrolling());
  visual_viewport.SetScaleAndLocation(1.75, false, gfx::PointF(12, 34));
  EXPECT_EQ(gfx::PointF(12, 34), visual_viewport.ScrollPosition());

  UpdateAllLifecyclePhases();
  EXPECT_EQ(gfx::PointF(12, 34), visual_viewport.ScrollPosition());
  // When we create the scrolling layer, we should update its scroll offset.
  ASSERT_TRUE(visual_viewport.LayerForScrolling());

  auto* layer_tree_host = GetFrame()->View()->RootCcLayer()->layer_tree_host();
  EXPECT_EQ(
      gfx::PointF(12, 34),
      layer_tree_host->property_trees()->scroll_tree().current_scroll_offset(
          visual_viewport.GetScrollElementId()));
}

TEST_P(VisualViewportTest, ScrollbarGeometryOnSizeChange) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(100, 100));
  UpdateAllLifecyclePhases();
  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");

  auto& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_EQ(gfx::Size(100, 100), visual_viewport.Size());
  auto* horizontal_scrollbar = visual_viewport.LayerForHorizontalScrollbar();
  auto* vertical_scrollbar = visual_viewport.LayerForVerticalScrollbar();
  ASSERT_TRUE(horizontal_scrollbar);
  ASSERT_TRUE(vertical_scrollbar);
  EXPECT_EQ(gfx::Vector2dF(0, 93),
            horizontal_scrollbar->offset_to_transform_parent());
  EXPECT_EQ(gfx::Vector2dF(93, 0),
            vertical_scrollbar->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(93, 7), horizontal_scrollbar->bounds());
  EXPECT_EQ(gfx::Size(7, 93), vertical_scrollbar->bounds());

  // Simulate hiding of the top controls.
  WebView()->MainFrameViewWidget()->Resize(gfx::Size(100, 120));
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(
      GetFrame()->View()->VisualViewportOrOverlayNeedsRepaintForTesting());
  UpdateAllLifecyclePhases();
  EXPECT_EQ(gfx::Size(100, 120), visual_viewport.Size());
  ASSERT_EQ(horizontal_scrollbar,
            visual_viewport.LayerForHorizontalScrollbar());
  ASSERT_EQ(vertical_scrollbar, visual_viewport.LayerForVerticalScrollbar());
  EXPECT_EQ(gfx::Vector2dF(0, 113),
            horizontal_scrollbar->offset_to_transform_parent());
  EXPECT_EQ(gfx::Vector2dF(93, 0),
            vertical_scrollbar->offset_to_transform_parent());
  EXPECT_EQ(gfx::Size(93, 7), horizontal_scrollbar->bounds());
  EXPECT_EQ(gfx::Size(7, 113), vertical_scrollbar->bounds());
}

TEST_F(VisualViewportSimTest, PreferredOverlayScrollbarColorTheme) {
  ColorSchemeHelper color_scheme_helper(*(WebView().GetPage()));
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <meta name="color-scheme" content="light dark">
          <style>
            html { height: 2000px; }
          </style>
      )HTML");
  Compositor().BeginFrame();

  const VisualViewport& visual_viewport =
      WebView().GetPage()->GetVisualViewport();
  EXPECT_EQ(mojom::blink::ColorScheme::kDark,
            visual_viewport.GetOverlayScrollbarColorScheme());

  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kLight);
  Compositor().BeginFrame();
  EXPECT_EQ(mojom::blink::ColorScheme::kLight,
            visual_viewport.GetOverlayScrollbarColorScheme());
}

}  // namespace
}  // namespace blink
