// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_coalesced_input_event.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/dev_tools_emulator.h"
#include "third_party/blink/renderer/core/layout/layout_custom_scrollbar_part.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay_mock.h"
#include "third_party/blink/renderer/core/testing/color_scheme_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

namespace {

class StubWebThemeEngine : public WebThemeEngine {
 public:
  StubWebThemeEngine() { painted_color_scheme_.fill(WebColorScheme::kLight); }

  WebSize GetSize(Part part) override {
    switch (part) {
      case kPartScrollbarHorizontalThumb:
        return blink::WebSize(kMinimumHorizontalLength, 15);
      case kPartScrollbarVerticalThumb:
        return blink::WebSize(15, kMinimumVerticalLength);
      default:
        return WebSize();
    }
  }
  void GetOverlayScrollbarStyle(ScrollbarStyle* style) override {
    style->fade_out_delay = base::TimeDelta();
    style->fade_out_duration = base::TimeDelta();
    style->thumb_thickness = 3;
    style->scrollbar_margin = 0;
    style->color = SkColorSetARGB(128, 64, 64, 64);
  }
  static constexpr int kMinimumHorizontalLength = 51;
  static constexpr int kMinimumVerticalLength = 52;

  void Paint(cc::PaintCanvas*,
             Part part,
             State,
             const WebRect&,
             const ExtraParams*,
             blink::WebColorScheme color_scheme) override {
    // Make  sure we don't overflow the array.
    DCHECK(part <= kPartProgressBar);
    painted_color_scheme_[part] = color_scheme;
  }

  WebColorScheme GetPaintedPartColorScheme(Part part) const {
    return painted_color_scheme_[part];
  }

  blink::PreferredColorScheme PreferredColorScheme() const override {
    return preferred_color_scheme_;
  }

  void SetPreferredColorScheme(
      const blink::PreferredColorScheme preferred_color_scheme) override {
    preferred_color_scheme_ = preferred_color_scheme;
  }

 private:
  std::array<WebColorScheme, kPartProgressBar + 1> painted_color_scheme_;
  blink::PreferredColorScheme preferred_color_scheme_ =
      blink::PreferredColorScheme::kNoPreference;
};

constexpr int StubWebThemeEngine::kMinimumHorizontalLength;
constexpr int StubWebThemeEngine::kMinimumVerticalLength;

class ScrollbarTestingPlatformSupport : public TestingPlatformSupport {
 public:
  WebThemeEngine* ThemeEngine() override { return &mock_theme_engine_; }

 private:
  StubWebThemeEngine mock_theme_engine_;
};

}  // namespace

class ScrollbarsTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();
    // We don't use the mock scrollbar theme in this file, but use the normal
    // scrollbar theme with mock WebThemeEngine, for better control of testing
    // environment. This is after SimTest::SetUp() to override the mock overlay
    // scrollbar settings initialized there.
    mock_overlay_scrollbars_ =
        std::make_unique<ScopedMockOverlayScrollbars>(false);
    original_overlay_scrollbars_enabled_ =
        ScrollbarThemeSettings::OverlayScrollbarsEnabled();
  }

  void TearDown() override {
    ScrollbarThemeSettings::SetOverlayScrollbarsEnabled(
        original_overlay_scrollbars_enabled_);
    mock_overlay_scrollbars_.reset();
    SimTest::TearDown();
  }

  void SetOverlayScrollbarsEnabled(bool b) {
    ScrollbarThemeSettings::SetOverlayScrollbarsEnabled(b);
  }

  HitTestResult HitTest(int x, int y) {
    return WebView().CoreHitTestResultAt(gfx::Point(x, y));
  }

  EventHandler& GetEventHandler() {
    return GetDocument().GetFrame()->GetEventHandler();
  }

  void HandleMouseMoveEvent(int x, int y) {
    WebMouseEvent event(WebInputEvent::kMouseMove, WebFloatPoint(x, y),
                        WebFloatPoint(x, y),
                        WebPointerProperties::Button::kNoButton, 0,
                        WebInputEvent::kNoModifiers, base::TimeTicks::Now());
    event.SetFrameScale(1);
    GetEventHandler().HandleMouseMoveEvent(event, Vector<WebMouseEvent>(),
                                           Vector<WebMouseEvent>());
  }

  void HandleMousePressEvent(int x, int y) {
    WebMouseEvent event(
        WebInputEvent::kMouseDown, WebFloatPoint(x, y), WebFloatPoint(x, y),
        WebPointerProperties::Button::kLeft, 0,
        WebInputEvent::Modifiers::kLeftButtonDown, base::TimeTicks::Now());
    event.SetFrameScale(1);
    GetEventHandler().HandleMousePressEvent(event);
  }

  void HandleContextMenuEvent(int x, int y) {
    WebMouseEvent event(
        WebInputEvent::kMouseDown, WebFloatPoint(x, y), WebFloatPoint(x, y),
        WebPointerProperties::Button::kNoButton, 0,
        WebInputEvent::Modifiers::kNoModifiers, base::TimeTicks::Now());
    event.SetFrameScale(1);
    GetEventHandler().SendContextMenuEvent(event);
  }

  void HandleMouseReleaseEvent(int x, int y) {
    WebMouseEvent event(
        WebInputEvent::kMouseUp, WebFloatPoint(x, y), WebFloatPoint(x, y),
        WebPointerProperties::Button::kLeft, 0,
        WebInputEvent::Modifiers::kNoModifiers, base::TimeTicks::Now());
    event.SetFrameScale(1);
    GetEventHandler().HandleMouseReleaseEvent(event);
  }

  void HandleMouseMiddlePressEvent(int x, int y) {
    WebMouseEvent event(
        WebInputEvent::kMouseDown, WebFloatPoint(x, y), WebFloatPoint(x, y),
        WebPointerProperties::Button::kMiddle, 0,
        WebInputEvent::Modifiers::kMiddleButtonDown, base::TimeTicks::Now());
    event.SetFrameScale(1);
    GetEventHandler().HandleMousePressEvent(event);
  }

  void HandleMouseMiddleReleaseEvent(int x, int y) {
    WebMouseEvent event(
        WebInputEvent::kMouseUp, WebFloatPoint(x, y), WebFloatPoint(x, y),
        WebPointerProperties::Button::kMiddle, 0,
        WebInputEvent::Modifiers::kMiddleButtonDown, base::TimeTicks::Now());
    event.SetFrameScale(1);
    GetEventHandler().HandleMouseReleaseEvent(event);
  }

  void HandleMouseLeaveEvent() {
    WebMouseEvent event(
        WebInputEvent::kMouseMove, WebFloatPoint(1, 1), WebFloatPoint(1, 1),
        WebPointerProperties::Button::kLeft, 0,
        WebInputEvent::Modifiers::kLeftButtonDown, base::TimeTicks::Now());
    event.SetFrameScale(1);
    GetEventHandler().HandleMouseLeaveEvent(event);
  }

  WebCoalescedInputEvent GenerateWheelGestureEvent(
      WebInputEvent::Type type,
      const IntPoint& position,
      ScrollOffset offset = ScrollOffset()) {
    return GenerateGestureEvent(type, WebGestureDevice::kTouchpad, position,
                                offset);
  }

  WebCoalescedInputEvent GenerateTouchGestureEvent(
      WebInputEvent::Type type,
      const IntPoint& position,
      ScrollOffset offset = ScrollOffset()) {
    return GenerateGestureEvent(type, WebGestureDevice::kTouchscreen, position,
                                offset);
  }

  ui::CursorType CursorType() {
    return GetDocument()
        .GetFrame()
        ->GetChromeClient()
        .LastSetCursorForTesting()
        .GetType();
  }

  ScrollbarTheme& GetScrollbarTheme() {
    return GetDocument().GetPage()->GetScrollbarTheme();
  }

 protected:
  WebCoalescedInputEvent GenerateGestureEvent(WebInputEvent::Type type,
                                              WebGestureDevice device,
                                              const IntPoint& position,
                                              ScrollOffset offset) {
    WebGestureEvent event(type, WebInputEvent::kNoModifiers,
                          base::TimeTicks::Now(), device);

    event.SetPositionInWidget(WebFloatPoint(position.X(), position.Y()));

    if (type == WebInputEvent::kGestureScrollUpdate) {
      event.data.scroll_update.delta_x = offset.Width();
      event.data.scroll_update.delta_y = offset.Height();
    } else if (type == WebInputEvent::kGestureScrollBegin) {
      event.data.scroll_begin.delta_x_hint = offset.Width();
      event.data.scroll_begin.delta_y_hint = offset.Height();
    }
    return WebCoalescedInputEvent(event);
  }

 private:
  ScopedTestingPlatformSupport<ScrollbarTestingPlatformSupport> platform;
  std::unique_ptr<ScopedMockOverlayScrollbars> mock_overlay_scrollbars_;
  bool original_overlay_scrollbars_enabled_;
};

class ScrollbarsTestWithVirtualTimer : public ScrollbarsTest {
 public:
  void SetUp() override {
    ScrollbarsTest::SetUp();
    WebView().Scheduler()->EnableVirtualTime();
  }

  void TearDown() override {
    WebView().Scheduler()->DisableVirtualTimeForTesting();
    ScrollbarsTest::TearDown();
  }

  void TimeAdvance() {
    WebView().Scheduler()->SetVirtualTimePolicy(
        PageScheduler::VirtualTimePolicy::kAdvance);
  }

  void StopVirtualTimeAndExitRunLoop() {
    WebView().Scheduler()->SetVirtualTimePolicy(
        PageScheduler::VirtualTimePolicy::kPause);
    test::ExitRunLoop();
  }

  // Some task queues may have repeating v8 tasks that run forever so we impose
  // a hard (virtual) time limit.
  void RunTasksForPeriod(base::TimeDelta delay) {
    TimeAdvance();
    scheduler::GetSingleThreadTaskRunnerForTesting()->PostDelayedTask(
        FROM_HERE,
        WTF::Bind(
            &ScrollbarsTestWithVirtualTimer::StopVirtualTimeAndExitRunLoop,
            WTF::Unretained(this)),
        delay);
    test::EnterRunLoop();
  }
};

// Try to force enable/disable overlay. Skip the test if the desired setting
// is not supported by the platform.
#define ENABLE_OVERLAY_SCROLLBARS(b)                                           \
  do {                                                                         \
    SetOverlayScrollbarsEnabled(b);                                            \
    if (WebView().GetPage()->GetScrollbarTheme().UsesOverlayScrollbars() != b) \
      return;                                                                  \
  } while (false)

TEST_F(ScrollbarsTest, DocumentStyleRecalcPreservesScrollbars) {
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style> body { width: 1600px; height: 1200px; } </style>)HTML");
  auto* layout_viewport = GetDocument().View()->LayoutViewport();

  Compositor().BeginFrame();
  ASSERT_TRUE(layout_viewport->VerticalScrollbar() &&
              layout_viewport->HorizontalScrollbar());

  // Forces recalc of LayoutView's computed style in Document::updateStyle,
  // without invalidating layout.
  MainFrame().ExecuteScriptAndReturnValue(WebScriptSource(
      "document.querySelector('style').sheet.insertRule('body {}', 1);"));

  Compositor().BeginFrame();
  ASSERT_TRUE(layout_viewport->VerticalScrollbar() &&
              layout_viewport->HorizontalScrollbar());
}

class ScrollbarsWebWidgetClient
    : public frame_test_helpers::TestWebWidgetClient {
 public:
  // WebWidgetClient overrides.
  void ConvertWindowToViewport(WebFloatRect* rect) override {
    rect->x *= device_scale_factor_;
    rect->y *= device_scale_factor_;
    rect->width *= device_scale_factor_;
    rect->height *= device_scale_factor_;
  }

  void set_device_scale_factor(float device_scale_factor) {
    device_scale_factor_ = device_scale_factor;
  }

 private:
  float device_scale_factor_;
};

TEST(ScrollbarsTestWithOwnWebViewHelper, ScrollbarSizeForUseZoomDSF) {
  ScrollbarsWebWidgetClient client;
  client.set_device_scale_factor(1.f);

  frame_test_helpers::WebViewHelper web_view_helper;
  // Needed so visual viewport supplies its own scrollbars. We don't support
  // this setting changing after initialization, so we must set it through
  // WebViewHelper.
  web_view_helper.set_viewport_enabled(true);

  WebViewImpl* web_view_impl =
      web_view_helper.Initialize(nullptr, nullptr, &client);

  web_view_impl->MainFrameWidget()->Resize(IntSize(800, 600));

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view_impl->MainFrameImpl(),
                                     "<!DOCTYPE html>"
                                     "<style>"
                                     "  body {"
                                     "    width: 1600px;"
                                     "    height: 1200px;"
                                     "  }"
                                     "</style>"
                                     "<body>"
                                     "</body>",
                                     base_url);
  web_view_impl->MainFrameWidget()->UpdateAllLifecyclePhases(
      WebWidget::LifecycleUpdateReason::kTest);

  Document* document =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame())->GetDocument();

  VisualViewport& visual_viewport = document->GetPage()->GetVisualViewport();
  int horizontal_scrollbar =
      visual_viewport.LayerForHorizontalScrollbar()->bounds().height();
  int vertical_scrollbar =
      visual_viewport.LayerForVerticalScrollbar()->bounds().width();

  const float device_scale = 3.5f;
  client.set_device_scale_factor(device_scale);
  web_view_impl->MainFrameWidget()->Resize(IntSize(400, 300));

  EXPECT_EQ(clampTo<int>(std::floor(horizontal_scrollbar * device_scale)),
            visual_viewport.LayerForHorizontalScrollbar()->bounds().height());
  EXPECT_EQ(clampTo<int>(std::floor(vertical_scrollbar * device_scale)),
            visual_viewport.LayerForVerticalScrollbar()->bounds().width());

  client.set_device_scale_factor(1.f);
  web_view_impl->MainFrameWidget()->Resize(IntSize(800, 600));

  EXPECT_EQ(horizontal_scrollbar,
            visual_viewport.LayerForHorizontalScrollbar()->bounds().height());
  EXPECT_EQ(vertical_scrollbar,
            visual_viewport.LayerForVerticalScrollbar()->bounds().width());
}

// Ensure that causing a change in scrollbar existence causes a nested layout
// to recalculate the existence of the opposite scrollbar. The bug here was
// caused by trying to avoid the layout when overlays are enabled but not
// checking whether the scrollbars should be custom - which do take up layout
// space. https://crbug.com/668387.
TEST_F(ScrollbarsTest, CustomScrollbarsCauseLayoutOnExistenceChange) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);

  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      ::-webkit-scrollbar {
          height: 16px;
          width: 16px
      }
      ::-webkit-scrollbar-thumb {
          background-color: rgba(0,0,0,.2);
      }
      html, body{
        margin: 0;
        height: 100%;
      }
      .box {
        width: 100%;
        height: 100%;
      }
      .transformed {
        transform: translateY(100px);
      }
    </style>
    <div id='box' class='box'></div>
  )HTML");

  ScrollableArea* layout_viewport = GetDocument().View()->LayoutViewport();

  Compositor().BeginFrame();

  ASSERT_FALSE(layout_viewport->VerticalScrollbar());
  ASSERT_FALSE(layout_viewport->HorizontalScrollbar());

  // Adding translation will cause a vertical scrollbar to appear but not dirty
  // layout otherwise. Ensure the change of scrollbar causes a layout to
  // recalculate the page width with the vertical scrollbar added.
  MainFrame().ExecuteScript(WebScriptSource(
      "document.getElementById('box').className = 'box transformed';"));
  Compositor().BeginFrame();

  ASSERT_TRUE(layout_viewport->VerticalScrollbar());
  ASSERT_FALSE(layout_viewport->HorizontalScrollbar());
}

TEST_F(ScrollbarsTest, TransparentBackgroundUsesDarkOverlayColorTheme) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);

  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  WebView().SetBaseBackgroundColorOverride(SK_ColorTRANSPARENT);
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body{
        height: 300%;
      }
    </style>
  )HTML");
  Compositor().BeginFrame();

  ScrollableArea* layout_viewport = GetDocument().View()->LayoutViewport();

  EXPECT_EQ(kScrollbarOverlayColorThemeDark,
            layout_viewport->GetScrollbarOverlayColorTheme());
}

TEST_F(ScrollbarsTest, BodyBackgroundChangesOverlayColorTheme) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <body style='background:white'></body>
  )HTML");
  Compositor().BeginFrame();

  ScrollableArea* layout_viewport = GetDocument().View()->LayoutViewport();

  EXPECT_EQ(kScrollbarOverlayColorThemeDark,
            layout_viewport->GetScrollbarOverlayColorTheme());

  MainFrame().ExecuteScriptAndReturnValue(
      WebScriptSource("document.body.style.backgroundColor = 'black';"));

  Compositor().BeginFrame();
  EXPECT_EQ(kScrollbarOverlayColorThemeLight,
            layout_viewport->GetScrollbarOverlayColorTheme());
}

// Ensure overlay scrollbar change to display:none correctly.
TEST_F(ScrollbarsTest, OverlayScrollbarChangeToDisplayNoneDynamically) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);

  WebView().MainFrameWidget()->Resize(WebSize(200, 200));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    .noscrollbars::-webkit-scrollbar { display: none; }
    #div{ height: 100px; width:100px; overflow:scroll; }
    .big{ height: 2000px; }
    body { overflow:scroll; }
    </style>
    <div id='div'>
      <div class='big'>
      </div>
    </div>
    <div class='big'>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();
  Element* div = document.getElementById("div");

  // Ensure we have overlay scrollbar for div and root.
  ScrollableArea* scrollable_div =
      ToLayoutBox(div->GetLayoutObject())->GetScrollableArea();

  ScrollableArea* scrollable_root = GetDocument().View()->LayoutViewport();

  DCHECK(scrollable_div->VerticalScrollbar());
  DCHECK(scrollable_div->VerticalScrollbar()->IsOverlayScrollbar());

  DCHECK(!scrollable_div->HorizontalScrollbar());

  DCHECK(scrollable_root->VerticalScrollbar());
  DCHECK(scrollable_root->VerticalScrollbar()->IsOverlayScrollbar());

  // For PaintLayer Overlay Scrollbar we will remove the scrollbar when it is
  // not necessary even with overflow:scroll.
  DCHECK(!scrollable_root->HorizontalScrollbar());

  // Set display:none.
  div->setAttribute(html_names::kClassAttr, "noscrollbars");
  document.body()->setAttribute(html_names::kClassAttr, "noscrollbars");
  Compositor().BeginFrame();

  EXPECT_TRUE(scrollable_div->VerticalScrollbar());
  EXPECT_TRUE(scrollable_div->VerticalScrollbar()->IsCustomScrollbar());
  EXPECT_TRUE(scrollable_div->VerticalScrollbar()->FrameRect().IsEmpty());

  EXPECT_TRUE(scrollable_div->HorizontalScrollbar());
  EXPECT_TRUE(scrollable_div->HorizontalScrollbar()->IsCustomScrollbar());
  EXPECT_TRUE(scrollable_div->HorizontalScrollbar()->FrameRect().IsEmpty());

  EXPECT_TRUE(scrollable_root->VerticalScrollbar());
  EXPECT_TRUE(scrollable_root->VerticalScrollbar()->IsCustomScrollbar());
  EXPECT_TRUE(scrollable_root->VerticalScrollbar()->FrameRect().IsEmpty());

  EXPECT_TRUE(scrollable_root->HorizontalScrollbar());
  EXPECT_TRUE(scrollable_root->HorizontalScrollbar()->IsCustomScrollbar());
  EXPECT_TRUE(scrollable_root->HorizontalScrollbar()->FrameRect().IsEmpty());
}

TEST_F(ScrollbarsTest, scrollbarIsNotHandlingTouchpadScroll) {
  WebView().MainFrameWidget()->Resize(WebSize(200, 200));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
     #scrollable { height: 100px; width: 100px; overflow: scroll; }
     #content { height: 200px; width: 200px;}
    </style>
    <div id='scrollable'>
     <div id='content'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();
  Element* scrollable = document.getElementById("scrollable");

  ScrollableArea* scrollable_area =
      ToLayoutBox(scrollable->GetLayoutObject())->GetScrollableArea();
  DCHECK(scrollable_area->VerticalScrollbar());
  WebGestureEvent scroll_begin(
      WebInputEvent::kGestureScrollBegin, WebInputEvent::kNoModifiers,
      base::TimeTicks::Now(), WebGestureDevice::kTouchpad);
  scroll_begin.SetPositionInWidget(
      WebFloatPoint(scrollable->OffsetLeft() + scrollable->OffsetWidth() - 2,
                    scrollable->OffsetTop()));
  scroll_begin.SetPositionInScreen(
      WebFloatPoint(scrollable->OffsetLeft() + scrollable->OffsetWidth() - 2,
                    scrollable->OffsetTop()));
  scroll_begin.data.scroll_begin.delta_x_hint = 0;
  scroll_begin.data.scroll_begin.delta_y_hint = 10;
  scroll_begin.SetFrameScale(1);
  GetEventHandler().HandleGestureScrollEvent(scroll_begin);
  DCHECK(!GetEventHandler().IsScrollbarHandlingGestures());
  bool should_update_capture = false;
  DCHECK(!scrollable_area->VerticalScrollbar()->GestureEvent(
      scroll_begin, &should_update_capture));
}

TEST_F(ScrollbarsTest, HidingScrollbarsOnScrollableAreaDisablesScrollbars) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);

  WebView().MainFrameWidget()->Resize(WebSize(800, 600));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #scroller { overflow: scroll; width: 1000px; height: 1000px }
      #spacer { width: 2000px; height: 2000px }
    </style>
    <div id='scroller'>
      <div id='spacer'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();
  LocalFrameView* frame_view = WebView().MainFrameImpl()->GetFrameView();
  Element* scroller = document.getElementById("scroller");
  ScrollableArea* scroller_area =
      ToLayoutBox(scroller->GetLayoutObject())->GetScrollableArea();
  ScrollableArea* frame_scroller_area = frame_view->LayoutViewport();

  // Scrollbars are hidden at start.
  scroller_area->SetScrollbarsHiddenIfOverlay(true);
  frame_scroller_area->SetScrollbarsHiddenIfOverlay(true);
  ASSERT_TRUE(scroller_area->HorizontalScrollbar());
  ASSERT_TRUE(scroller_area->VerticalScrollbar());
  ASSERT_TRUE(frame_scroller_area->HorizontalScrollbar());
  ASSERT_TRUE(frame_scroller_area->VerticalScrollbar());

  EXPECT_TRUE(frame_scroller_area->ScrollbarsHiddenIfOverlay());
  EXPECT_FALSE(frame_scroller_area->HorizontalScrollbar()
                   ->ShouldParticipateInHitTesting());
  EXPECT_FALSE(frame_scroller_area->VerticalScrollbar()
                   ->ShouldParticipateInHitTesting());

  EXPECT_TRUE(scroller_area->ScrollbarsHiddenIfOverlay());
  EXPECT_FALSE(
      scroller_area->HorizontalScrollbar()->ShouldParticipateInHitTesting());
  EXPECT_FALSE(
      scroller_area->VerticalScrollbar()->ShouldParticipateInHitTesting());

  frame_scroller_area->SetScrollbarsHiddenIfOverlay(false);
  EXPECT_TRUE(frame_scroller_area->HorizontalScrollbar()
                  ->ShouldParticipateInHitTesting());
  EXPECT_TRUE(frame_scroller_area->VerticalScrollbar()
                  ->ShouldParticipateInHitTesting());
  frame_scroller_area->SetScrollbarsHiddenIfOverlay(true);
  EXPECT_FALSE(frame_scroller_area->HorizontalScrollbar()
                   ->ShouldParticipateInHitTesting());
  EXPECT_FALSE(frame_scroller_area->VerticalScrollbar()
                   ->ShouldParticipateInHitTesting());

  scroller_area->SetScrollbarsHiddenIfOverlay(false);
  EXPECT_TRUE(
      scroller_area->HorizontalScrollbar()->ShouldParticipateInHitTesting());
  EXPECT_TRUE(
      scroller_area->VerticalScrollbar()->ShouldParticipateInHitTesting());
  scroller_area->SetScrollbarsHiddenIfOverlay(true);
  EXPECT_FALSE(
      scroller_area->HorizontalScrollbar()->ShouldParticipateInHitTesting());
  EXPECT_FALSE(
      scroller_area->VerticalScrollbar()->ShouldParticipateInHitTesting());
}

// Ensure mouse cursor should be pointer when hovering over the scrollbar.
TEST_F(ScrollbarsTest, MouseOverScrollbarInCustomCursorElement) {
  // Skip this test if scrollbars don't allow hit testing on the platform.
  if (!WebView().GetPage()->GetScrollbarTheme().AllowsHitTest())
    return;

  WebView().MainFrameWidget()->Resize(WebSize(250, 250));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    #d1 {
      width: 200px;
      height: 200px;
      overflow: auto;
      cursor: move;
    }
    #d2 {
      height: 400px;
    }
    </style>
    <div id='d1'>
        <div id='d2'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();

  Element* div = document.getElementById("d1");

  // Ensure hittest has DIV and scrollbar.
  HitTestResult hit_test_result = HitTest(195, 5);

  EXPECT_EQ(hit_test_result.InnerElement(), div);
  EXPECT_TRUE(hit_test_result.GetScrollbar());

  HandleMouseMoveEvent(195, 5);

  EXPECT_EQ(ui::CursorType::kPointer, CursorType());
}

// Ensure mouse cursor should be override when hovering over the custom
// scrollbar.
TEST_F(ScrollbarsTest, MouseOverCustomScrollbarInCustomCursorElement) {
  // Skip this test if scrollbars don't allow hit testing on the platform.
  if (!WebView().GetPage()->GetScrollbarTheme().AllowsHitTest())
    return;

  WebView().MainFrameWidget()->Resize(WebSize(250, 250));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    #d1 {
      width: 200px;
      height: 200px;
      overflow: auto;
      cursor: move;
    }
    #d2 {
      height: 400px;
    }
    ::-webkit-scrollbar {
      background: none;
      height: 5px;
      width: 5px;
    }
    ::-webkit-scrollbar-thumb {
      background-color: black;
    }
    </style>
    <div id='d1'>
        <div id='d2'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();

  Element* div = document.getElementById("d1");

  // Ensure hittest has DIV and scrollbar.
  HitTestResult hit_test_result = HitTest(195, 5);

  EXPECT_EQ(hit_test_result.InnerElement(), div);
  EXPECT_TRUE(hit_test_result.GetScrollbar());

  HandleMouseMoveEvent(195, 5);

  EXPECT_EQ(ui::CursorType::kMove, CursorType());
}

// Makes sure that mouse hover over an overlay scrollbar doesn't activate
// elements below (except the Element that owns the scrollbar) unless the
// scrollbar is faded out.
TEST_F(ScrollbarsTest, MouseOverLinkAndOverlayScrollbar) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);
  // Skip this test if scrollbars don't allow hit testing on the platform.
  if (!WebView().GetPage()->GetScrollbarTheme().AllowsHitTest())
    return;

  WebView().MainFrameWidget()->Resize(WebSize(200, 200));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <a id='a' href='javascript:void(0);' style='font-size: 20px'>
    aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
    </a>
    <div style='position: absolute; top: 1000px'>
      end
    </div>
  )HTML");

  Compositor().BeginFrame();

  // Enable the Scrollbar.
  WebView()
      .MainFrameImpl()
      ->GetFrameView()
      ->LayoutViewport()
      ->SetScrollbarsHiddenIfOverlay(false);

  Document& document = GetDocument();
  Element* a_tag = document.getElementById("a");

  // This position is on scrollbar if it's enabled, or on the <a> element.
  int x = 190;
  int y = a_tag->OffsetTop();

  // Ensure hittest only has scrollbar.
  HitTestResult hit_test_result = HitTest(x, y);

  EXPECT_FALSE(hit_test_result.URLElement());
  EXPECT_TRUE(hit_test_result.InnerElement());
  ASSERT_TRUE(hit_test_result.GetScrollbar());
  EXPECT_FALSE(hit_test_result.GetScrollbar()->IsCustomScrollbar());

  // Mouse over link. Mouse cursor should be hand.
  HandleMouseMoveEvent(a_tag->OffsetLeft(), a_tag->OffsetTop());

  EXPECT_EQ(ui::CursorType::kHand, CursorType());

  // Mouse over enabled overlay scrollbar. Mouse cursor should be pointer and no
  // active hover element.
  HandleMouseMoveEvent(x, y);

  EXPECT_EQ(ui::CursorType::kPointer, CursorType());

  HandleMousePressEvent(x, y);

  EXPECT_TRUE(document.GetActiveElement());
  EXPECT_TRUE(document.HoverElement());

  HandleMouseReleaseEvent(x, y);

  // Mouse over disabled overlay scrollbar. Mouse cursor should be hand and has
  // active hover element.
  WebView()
      .MainFrameImpl()
      ->GetFrameView()
      ->LayoutViewport()
      ->SetScrollbarsHiddenIfOverlay(true);

  // Ensure hittest only has link
  hit_test_result = HitTest(x, y);

  EXPECT_TRUE(hit_test_result.URLElement());
  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_FALSE(hit_test_result.GetScrollbar());

  HandleMouseMoveEvent(x, y);

  EXPECT_EQ(ui::CursorType::kHand, CursorType());

  HandleMousePressEvent(x, y);

  EXPECT_TRUE(document.GetActiveElement());
  EXPECT_TRUE(document.HoverElement());
}

// Makes sure that mouse hover over an custom scrollbar doesn't change the
// activate elements.
TEST_F(ScrollbarsTest, MouseOverCustomScrollbar) {
  WebView().MainFrameWidget()->Resize(WebSize(200, 200));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    #scrollbar {
      position: absolute;
      top: 0;
      left: 0;
      height: 180px;
      width: 180px;
      overflow-x: auto;
    }
    ::-webkit-scrollbar {
      width: 8px;
    }
    ::-webkit-scrollbar-thumb {
      background-color: hsla(0, 0%, 56%, 0.6);
    }
    </style>
    <div id='scrollbar'>
      <div style='position: absolute; top: 1000px;'>
        make scrollbar show
      </div>
    </div>
  )HTML");

  Compositor().BeginFrame();

  Document& document = GetDocument();

  Element* scrollbar_div = document.getElementById("scrollbar");
  EXPECT_TRUE(scrollbar_div);

  // Ensure hittest only has DIV
  HitTestResult hit_test_result = HitTest(1, 1);

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_FALSE(hit_test_result.GetScrollbar());

  // Mouse over DIV
  HandleMouseMoveEvent(1, 1);

  // DIV :hover
  EXPECT_EQ(document.HoverElement(), scrollbar_div);

  // Ensure hittest has DIV and scrollbar
  hit_test_result = HitTest(175, 1);

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_TRUE(hit_test_result.GetScrollbar());
  EXPECT_TRUE(hit_test_result.GetScrollbar()->IsCustomScrollbar());

  // Mouse over scrollbar
  HandleMouseMoveEvent(175, 1);

  // Custom not change the DIV :hover
  EXPECT_EQ(document.HoverElement(), scrollbar_div);
  EXPECT_EQ(hit_test_result.GetScrollbar()->HoveredPart(),
            ScrollbarPart::kThumbPart);
}

// Makes sure that mouse hover over an overlay scrollbar doesn't hover iframe
// below.
TEST_F(ScrollbarsTest, MouseOverScrollbarAndIFrame) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);
  // Skip this test if scrollbars don't allow hit testing on the platform.
  if (!WebView().GetPage()->GetScrollbarTheme().AllowsHitTest())
    return;

  WebView().MainFrameWidget()->Resize(WebSize(200, 200));

  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
      height: 2000px;
    }
    iframe {
      height: 200px;
      width: 200px;
    }
    </style>
    <iframe id='iframe' src='iframe.html'>
    </iframe>
  )HTML");
  Compositor().BeginFrame();

  // Enable the Scrollbar.
  WebView()
      .MainFrameImpl()
      ->GetFrameView()
      ->LayoutViewport()
      ->SetScrollbarsHiddenIfOverlay(false);

  frame_resource.Complete("<!DOCTYPE html>");
  Compositor().BeginFrame();

  Document& document = GetDocument();
  Element* iframe = document.getElementById("iframe");
  DCHECK(iframe);

  // Ensure hittest only has IFRAME.
  HitTestResult hit_test_result = HitTest(5, 5);

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_FALSE(hit_test_result.GetScrollbar());

  // Mouse over IFRAME.
  HandleMouseMoveEvent(5, 5);

  // IFRAME hover.
  EXPECT_EQ(document.HoverElement(), iframe);

  // Ensure hittest has scrollbar.
  hit_test_result = HitTest(195, 5);
  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_TRUE(hit_test_result.GetScrollbar());
  EXPECT_TRUE(hit_test_result.GetScrollbar()->Enabled());

  // Mouse over scrollbar.
  HandleMouseMoveEvent(195, 5);

  // IFRAME not hover.
  EXPECT_NE(document.HoverElement(), iframe);

  // Disable the Scrollbar.
  WebView()
      .MainFrameImpl()
      ->GetFrameView()
      ->LayoutViewport()
      ->SetScrollbarsHiddenIfOverlay(true);

  // Ensure hittest has IFRAME and no scrollbar.
  hit_test_result = HitTest(196, 5);

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_FALSE(hit_test_result.GetScrollbar());

  // Mouse over disabled scrollbar.
  HandleMouseMoveEvent(196, 5);

  // IFRAME hover.
  EXPECT_EQ(document.HoverElement(), iframe);
}

// Makes sure that mouse hover over a scrollbar also hover the element owns the
// scrollbar.
TEST_F(ScrollbarsTest, MouseOverScrollbarAndParentElement) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameWidget()->Resize(WebSize(200, 200));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    #parent {
      position: absolute;
      top: 0;
      left: 0;
      height: 180px;
      width: 180px;
      overflow-y: scroll;
    }
    </style>
    <div id='parent'>
      <div id='child' style='position: absolute; top: 1000px;'>
        make scrollbar enabled
      </div>
    </div>
  )HTML");

  Compositor().BeginFrame();

  Document& document = GetDocument();

  Element* parent_div = document.getElementById("parent");
  Element* child_div = document.getElementById("child");
  EXPECT_TRUE(parent_div);
  EXPECT_TRUE(child_div);

  ScrollableArea* scrollable_area =
      ToLayoutBox(parent_div->GetLayoutObject())->GetScrollableArea();

  EXPECT_TRUE(scrollable_area->VerticalScrollbar());
  EXPECT_FALSE(scrollable_area->VerticalScrollbar()->IsOverlayScrollbar());

  // Ensure hittest only has DIV.
  HitTestResult hit_test_result = HitTest(1, 1);

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_FALSE(hit_test_result.GetScrollbar());

  // Mouse over DIV.
  HandleMouseMoveEvent(1, 1);

  // DIV :hover.
  EXPECT_EQ(document.HoverElement(), parent_div);

  // Ensure hittest has DIV and scrollbar.
  hit_test_result = HitTest(175, 5);

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_TRUE(hit_test_result.GetScrollbar());
  EXPECT_FALSE(hit_test_result.GetScrollbar()->IsCustomScrollbar());
  EXPECT_TRUE(hit_test_result.GetScrollbar()->Enabled());

  // Mouse over scrollbar.
  HandleMouseMoveEvent(175, 5);

  // Not change the DIV :hover.
  EXPECT_EQ(document.HoverElement(), parent_div);

  // Disable the Scrollbar by remove the childDiv.
  child_div->remove();
  Compositor().BeginFrame();

  // Ensure hittest has DIV and no scrollbar.
  hit_test_result = HitTest(175, 5);

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_TRUE(hit_test_result.GetScrollbar());
  EXPECT_FALSE(hit_test_result.GetScrollbar()->Enabled());
  EXPECT_LT(hit_test_result.InnerElement()->clientWidth(), 180);

  // Mouse over disabled scrollbar.
  HandleMouseMoveEvent(175, 5);

  // Not change the DIV :hover.
  EXPECT_EQ(document.HoverElement(), parent_div);
}

// Makes sure that mouse over a root scrollbar also hover the html element.
TEST_F(ScrollbarsTest, MouseOverRootScrollbar) {
  // Skip this test if scrollbars don't allow hit testing on the platform.
  if (!WebView().GetPage()->GetScrollbarTheme().AllowsHitTest())
    return;

  WebView().MainFrameWidget()->Resize(WebSize(200, 200));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      overflow: scroll;
    }
    </style>
  )HTML");

  Compositor().BeginFrame();

  Document& document = GetDocument();

  // Ensure hittest has <html> element and scrollbar.
  HitTestResult hit_test_result = HitTest(195, 5);

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_EQ(hit_test_result.InnerElement(), document.documentElement());
  EXPECT_TRUE(hit_test_result.GetScrollbar());

  // Mouse over scrollbar.
  HandleMouseMoveEvent(195, 5);

  // Hover <html element.
  EXPECT_EQ(document.HoverElement(), document.documentElement());
}

TEST_F(ScrollbarsTest, MouseReleaseUpdatesScrollbarHoveredPart) {
  WebView().MainFrameWidget()->Resize(WebSize(200, 200));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    #scrollbar {
      position: absolute;
      top: 0;
      left: 0;
      height: 180px;
      width: 180px;
      overflow-x: auto;
    }
    ::-webkit-scrollbar {
      width: 8px;
    }
    ::-webkit-scrollbar-thumb {
      background-color: hsla(0, 0%, 56%, 0.6);
    }
    </style>
    <div id='scrollbar'>
      <div style='position: absolute; top: 1000px;'>make scrollbar
    shows</div>
    </div>
  )HTML");

  Compositor().BeginFrame();

  Document& document = GetDocument();

  Element* scrollbar_div = document.getElementById("scrollbar");
  EXPECT_TRUE(scrollbar_div);

  ScrollableArea* scrollable_area =
      ToLayoutBox(scrollbar_div->GetLayoutObject())->GetScrollableArea();

  EXPECT_TRUE(scrollable_area->VerticalScrollbar());
  Scrollbar* scrollbar = scrollable_area->VerticalScrollbar();
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kNoPart);
  EXPECT_EQ(scrollbar->HoveredPart(), ScrollbarPart::kNoPart);

  // Mouse moved over the scrollbar.
  HandleMouseMoveEvent(175, 1);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kNoPart);
  EXPECT_EQ(scrollbar->HoveredPart(), ScrollbarPart::kThumbPart);

  // Mouse pressed.
  HandleMousePressEvent(175, 1);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kThumbPart);
  EXPECT_EQ(scrollbar->HoveredPart(), ScrollbarPart::kThumbPart);

  // Mouse moved off the scrollbar while still pressed.
  HandleMouseLeaveEvent();
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kThumbPart);
  EXPECT_EQ(scrollbar->HoveredPart(), ScrollbarPart::kThumbPart);

  // Mouse released.
  HandleMouseReleaseEvent(1, 1);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kNoPart);
  EXPECT_EQ(scrollbar->HoveredPart(), ScrollbarPart::kNoPart);
}

TEST_F(ScrollbarsTest, ContextMenuUpdatesScrollbarPressedPart) {
  WebView().MainFrameWidget()->Resize(WebSize(200, 200));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body { margin: 0px }
    #scroller { overflow-x: auto; width: 180px; height: 100px }
    #spacer { height: 300px }
    ::-webkit-scrollbar { width: 8px }
    ::-webkit-scrollbar-thumb {
      background-color: hsla(0, 0%, 56%, 0.6)
    }
    </style>
    <div id='scroller'>
      <div id='spacer'></div>
    </div>
  )HTML");

  Compositor().BeginFrame();

  Document& document = GetDocument();

  Element* scrollbar_div = document.getElementById("scroller");
  EXPECT_TRUE(scrollbar_div);

  ScrollableArea* scrollable_area =
      ToLayoutBox(scrollbar_div->GetLayoutObject())->GetScrollableArea();

  EXPECT_TRUE(scrollable_area->VerticalScrollbar());
  Scrollbar* scrollbar = scrollable_area->VerticalScrollbar();
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kNoPart);

  // Mouse moved over the scrollbar.
  HandleMouseMoveEvent(175, 5);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kNoPart);

  // Press the scrollbar.
  HandleMousePressEvent(175, 5);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kThumbPart);

  // ContextMenu while still pressed.
  HandleContextMenuEvent(175, 5);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kNoPart);

  // Mouse moved off the scrollbar.
  HandleMousePressEvent(50, 5);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kNoPart);
}

TEST_F(ScrollbarsTest,
       CustomScrollbarInOverlayScrollbarThemeWillNotCauseDCHECKFails) {
  WebView().MainFrameWidget()->Resize(WebSize(200, 200));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style type='text/css'>
       ::-webkit-scrollbar {
        width: 16px;
        height: 16px;
        overflow: visible;
      }
      div {
        width: 1000px;
      }
    </style>
    <div style='position: absolute; top: 1000px;'>
      end
    </div>
  )HTML");

  // No DCHECK Fails. Issue 676678.
  Compositor().BeginFrame();
}

// Make sure root custom scrollbar can change by Emulator but div custom
// scrollbar not.
TEST_F(ScrollbarsTest, CustomScrollbarChangeToMobileByEmulator) {
  WebView().MainFrameWidget()->Resize(WebSize(200, 200));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style type='text/css'>
    body {
      height: 10000px;
      margin: 0;
    }
    #d1 {
      height: 200px;
      width: 200px;
      overflow: auto;
    }
    #d2 {
      height: 2000px;
    }
    ::-webkit-scrollbar {
      width: 10px;
    }
    </style>
    <div id='d1'>
      <div id='d2'/>
    </div>
  )HTML");

  Compositor().BeginFrame();

  Document& document = GetDocument();

  ScrollableArea* root_scrollable = document.View()->LayoutViewport();

  Element* div = document.getElementById("d1");

  ScrollableArea* div_scrollable =
      ToLayoutBox(div->GetLayoutObject())->GetScrollableArea();

  VisualViewport& viewport = WebView().GetPage()->GetVisualViewport();

  DCHECK(root_scrollable->VerticalScrollbar());
  DCHECK(root_scrollable->VerticalScrollbar()->IsCustomScrollbar());
  DCHECK(!root_scrollable->VerticalScrollbar()->IsOverlayScrollbar());
  DCHECK(!root_scrollable->VerticalScrollbar()->GetTheme().IsMockTheme());

  DCHECK(!viewport.LayerForHorizontalScrollbar());

  DCHECK(div_scrollable->VerticalScrollbar());
  DCHECK(div_scrollable->VerticalScrollbar()->IsCustomScrollbar());
  DCHECK(!div_scrollable->VerticalScrollbar()->IsOverlayScrollbar());
  DCHECK(!div_scrollable->VerticalScrollbar()->GetTheme().IsMockTheme());

  // Turn on mobile emulator.
  WebDeviceEmulationParams params;
  params.screen_position = WebDeviceEmulationParams::kMobile;
  WebView().EnableDeviceEmulation(params);

  // For root Scrollbar, mobile emulator will change them to page VisualViewport
  // scrollbar layer.
  EXPECT_TRUE(viewport.LayerForVerticalScrollbar());
  EXPECT_FALSE(root_scrollable->VerticalScrollbar());

  EXPECT_TRUE(div_scrollable->VerticalScrollbar()->IsCustomScrollbar());

  // Turn off mobile emulator.
  WebView().DisableDeviceEmulation();

  EXPECT_TRUE(root_scrollable->VerticalScrollbar());
  EXPECT_TRUE(root_scrollable->VerticalScrollbar()->IsCustomScrollbar());
  EXPECT_FALSE(root_scrollable->VerticalScrollbar()->IsOverlayScrollbar());
  EXPECT_FALSE(root_scrollable->VerticalScrollbar()->GetTheme().IsMockTheme());

  DCHECK(!viewport.LayerForHorizontalScrollbar());

  EXPECT_TRUE(div_scrollable->VerticalScrollbar());
  EXPECT_TRUE(div_scrollable->VerticalScrollbar()->IsCustomScrollbar());
  EXPECT_FALSE(div_scrollable->VerticalScrollbar()->IsOverlayScrollbar());
  EXPECT_FALSE(div_scrollable->VerticalScrollbar()->GetTheme().IsMockTheme());
}

// Ensure custom scrollbar recreate when style owner change,
TEST_F(ScrollbarsTest, CustomScrollbarWhenStyleOwnerChange) {
  WebView().MainFrameWidget()->Resize(WebSize(200, 200));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style type='text/css'>
    #d1 {
      height: 200px;
      width: 200px;
      overflow: auto;
    }
    #d2 {
      height: 2000px;
    }
    ::-webkit-scrollbar {
      width: 10px;
    }
    .custom::-webkit-scrollbar {
      width: 5px;
    }
    </style>
    <div id='d1'>
      <div id='d2'></div>
    </div>
  )HTML");

  Compositor().BeginFrame();

  Document& document = GetDocument();

  Element* div = document.getElementById("d1");

  ScrollableArea* div_scrollable =
      ToLayoutBox(div->GetLayoutObject())->GetScrollableArea();

  DCHECK(div_scrollable->VerticalScrollbar());
  DCHECK(div_scrollable->VerticalScrollbar()->IsCustomScrollbar());
  DCHECK_EQ(div_scrollable->VerticalScrollbar()->Width(), 10);
  DCHECK(!div_scrollable->VerticalScrollbar()->IsOverlayScrollbar());
  DCHECK(!div_scrollable->VerticalScrollbar()->GetTheme().IsMockTheme());

  div->setAttribute(html_names::kClassAttr, "custom");
  Compositor().BeginFrame();

  EXPECT_TRUE(div_scrollable->VerticalScrollbar()->IsCustomScrollbar());
  EXPECT_EQ(div_scrollable->VerticalScrollbar()->Width(), 5);
}

// Make sure overlay scrollbars on non-composited scrollers fade out and set
// the hidden bit as needed.
// To avoid TSAN/ASAN race issue, this test use Virtual Time and give scrollbar
// a huge fadeout delay.
// Disable on Android since VirtualTime not work for Android.
// http://crbug.com/633321
#if defined(OS_ANDROID)
TEST_F(ScrollbarsTestWithVirtualTimer,
       DISABLED_TestNonCompositedOverlayScrollbarsFade) {
#else
TEST_F(ScrollbarsTestWithVirtualTimer, TestNonCompositedOverlayScrollbarsFade) {
#endif
  // This test relies on mock overlay scrollbars.
  ScopedMockOverlayScrollbars mock_overlay_scrollbars(true);

  TimeAdvance();
  constexpr base::TimeDelta kMockOverlayFadeOutDelay =
      base::TimeDelta::FromSeconds(5);

  ScrollbarTheme& theme = GetScrollbarTheme();
  ASSERT_TRUE(theme.IsMockTheme());
  ASSERT_TRUE(theme.UsesOverlayScrollbars());
  ScrollbarThemeOverlayMock& mock_overlay_theme =
      static_cast<ScrollbarThemeOverlayMock&>(theme);
  mock_overlay_theme.SetOverlayScrollbarFadeOutDelay(kMockOverlayFadeOutDelay);

  WebView().MainFrameWidget()->Resize(WebSize(640, 480));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  RunTasksForPeriod(kMockOverlayFadeOutDelay);
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #space {
        width: 1000px;
        height: 1000px;
      }
      #container {
        width: 200px;
        height: 200px;
        overflow: scroll;
        /* Ensure the scroller is non-composited. */
        border: border: 2px solid;
        border-radius: 25px;
      }
      div { height:1000px; width: 200px; }
    </style>
    <div id='container'>
      <div id='space'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();
  Element* container = document.getElementById("container");
  ScrollableArea* scrollable_area =
      ToLayoutBox(container->GetLayoutObject())->GetScrollableArea();

  DCHECK(!scrollable_area->UsesCompositedScrolling());

  EXPECT_FALSE(scrollable_area->ScrollbarsHiddenIfOverlay());
  RunTasksForPeriod(kMockOverlayFadeOutDelay);
  EXPECT_TRUE(scrollable_area->ScrollbarsHiddenIfOverlay());

  scrollable_area->SetScrollOffset(ScrollOffset(10, 10), kProgrammaticScroll,
                                   kScrollBehaviorInstant);

  EXPECT_FALSE(scrollable_area->ScrollbarsHiddenIfOverlay());
  RunTasksForPeriod(kMockOverlayFadeOutDelay);
  EXPECT_TRUE(scrollable_area->ScrollbarsHiddenIfOverlay());

  MainFrame().ExecuteScript(WebScriptSource(
      "document.getElementById('space').style.height = '500px';"));
  Compositor().BeginFrame();

  EXPECT_TRUE(scrollable_area->ScrollbarsHiddenIfOverlay());

  MainFrame().ExecuteScript(WebScriptSource(
      "document.getElementById('container').style.height = '300px';"));
  Compositor().BeginFrame();

  EXPECT_FALSE(scrollable_area->ScrollbarsHiddenIfOverlay());
  RunTasksForPeriod(kMockOverlayFadeOutDelay);
  EXPECT_TRUE(scrollable_area->ScrollbarsHiddenIfOverlay());

  // Non-composited scrollbars don't fade out while mouse is over.
  EXPECT_TRUE(scrollable_area->VerticalScrollbar());
  scrollable_area->SetScrollOffset(ScrollOffset(20, 20), kProgrammaticScroll,
                                   kScrollBehaviorInstant);
  EXPECT_FALSE(scrollable_area->ScrollbarsHiddenIfOverlay());
  scrollable_area->MouseEnteredScrollbar(*scrollable_area->VerticalScrollbar());
  RunTasksForPeriod(kMockOverlayFadeOutDelay);
  EXPECT_FALSE(scrollable_area->ScrollbarsHiddenIfOverlay());
  scrollable_area->MouseExitedScrollbar(*scrollable_area->VerticalScrollbar());
  RunTasksForPeriod(kMockOverlayFadeOutDelay);
  EXPECT_TRUE(scrollable_area->ScrollbarsHiddenIfOverlay());

  mock_overlay_theme.SetOverlayScrollbarFadeOutDelay(base::TimeDelta());
}

class ScrollbarAppearanceTest
    : public ScrollbarsTest,
      public testing::WithParamInterface</*use_overlay_scrollbars=*/bool> {};

// Test both overlay and non-overlay scrollbars.
INSTANTIATE_TEST_SUITE_P(All, ScrollbarAppearanceTest, testing::Bool());

// Make sure native scrollbar can change by Emulator.
// Disable on Android since Android always enable OverlayScrollbar.
#if defined(OS_ANDROID)
TEST_P(ScrollbarAppearanceTest,
       DISABLED_NativeScrollbarChangeToMobileByEmulator) {
#else
TEST_P(ScrollbarAppearanceTest, NativeScrollbarChangeToMobileByEmulator) {
#endif
  bool use_overlay_scrollbar = GetParam();
  ENABLE_OVERLAY_SCROLLBARS(use_overlay_scrollbar);

  WebView().MainFrameWidget()->Resize(WebSize(200, 200));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style type='text/css'>
    body {
      height: 10000px;
      margin: 0;
    }
    #d1 {
      height: 200px;
      width: 200px;
      overflow: auto;
    }
    #d2 {
      height: 2000px;
    }
    </style>
    <div id='d1'>
      <div id='d2'/>
    </div>
  )HTML");

  Compositor().BeginFrame();

  Document& document = GetDocument();

  ScrollableArea* root_scrollable = document.View()->LayoutViewport();

  Element* div = document.getElementById("d1");

  ScrollableArea* div_scrollable =
      ToLayoutBox(div->GetLayoutObject())->GetScrollableArea();

  VisualViewport& viewport = WebView().GetPage()->GetVisualViewport();

  DCHECK(root_scrollable->VerticalScrollbar());
  DCHECK(!root_scrollable->VerticalScrollbar()->IsCustomScrollbar());
  DCHECK_EQ(use_overlay_scrollbar,
            root_scrollable->VerticalScrollbar()->IsOverlayScrollbar());
  DCHECK(!root_scrollable->VerticalScrollbar()->GetTheme().IsMockTheme());

  DCHECK(!viewport.LayerForHorizontalScrollbar());

  DCHECK(div_scrollable->VerticalScrollbar());
  DCHECK(!div_scrollable->VerticalScrollbar()->IsCustomScrollbar());
  DCHECK_EQ(use_overlay_scrollbar,
            div_scrollable->VerticalScrollbar()->IsOverlayScrollbar());
  DCHECK(!div_scrollable->VerticalScrollbar()->GetTheme().IsMockTheme());

  // Turn on mobile emulator.
  WebDeviceEmulationParams params;
  params.screen_position = WebDeviceEmulationParams::kMobile;
  WebView().EnableDeviceEmulation(params);

  // For root Scrollbar, mobile emulator will change them to page VisualViewport
  // scrollbar layer.
  EXPECT_TRUE(viewport.LayerForHorizontalScrollbar());

  // Ensure div scrollbar also change to mobile overlay theme.
  EXPECT_TRUE(div_scrollable->VerticalScrollbar()->IsOverlayScrollbar());
  EXPECT_TRUE(div_scrollable->VerticalScrollbar()->IsSolidColor());

  // Turn off mobile emulator.
  WebView().DisableDeviceEmulation();

  EXPECT_TRUE(root_scrollable->VerticalScrollbar());
  EXPECT_FALSE(root_scrollable->VerticalScrollbar()->IsCustomScrollbar());
  DCHECK_EQ(use_overlay_scrollbar,
            root_scrollable->VerticalScrollbar()->IsOverlayScrollbar());
  EXPECT_FALSE(root_scrollable->VerticalScrollbar()->GetTheme().IsMockTheme());

  DCHECK(!viewport.LayerForHorizontalScrollbar());

  EXPECT_TRUE(div_scrollable->VerticalScrollbar());
  EXPECT_FALSE(div_scrollable->VerticalScrollbar()->IsCustomScrollbar());
  DCHECK_EQ(use_overlay_scrollbar,
            div_scrollable->VerticalScrollbar()->IsOverlayScrollbar());
  EXPECT_FALSE(div_scrollable->VerticalScrollbar()->GetTheme().IsMockTheme());
}

#if !defined(OS_MACOSX)
// Ensure that the minimum length for a scrollbar thumb comes from the
// WebThemeEngine. Note, Mac scrollbars differ from all other platforms so this
// test doesn't apply there. https://crbug.com/682209.
TEST_P(ScrollbarAppearanceTest, ThemeEngineDefinesMinimumThumbLength) {
  ScopedTestingPlatformSupport<ScrollbarTestingPlatformSupport> platform;
  ENABLE_OVERLAY_SCROLLBARS(GetParam());

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style> body { width: 1000000px; height: 1000000px; } </style>)HTML");
  ScrollableArea* scrollable_area = GetDocument().View()->LayoutViewport();

  Compositor().BeginFrame();
  ASSERT_TRUE(scrollable_area->VerticalScrollbar());
  ASSERT_TRUE(scrollable_area->HorizontalScrollbar());

  ScrollbarTheme& theme = scrollable_area->VerticalScrollbar()->GetTheme();
  EXPECT_EQ(StubWebThemeEngine::kMinimumHorizontalLength,
            theme.ThumbLength(*scrollable_area->HorizontalScrollbar()));
  EXPECT_EQ(StubWebThemeEngine::kMinimumVerticalLength,
            theme.ThumbLength(*scrollable_area->VerticalScrollbar()));
}

// Ensure thumb position is correctly calculated even at ridiculously large
// scales.
TEST_P(ScrollbarAppearanceTest, HugeScrollingThumbPosition) {
  ScopedTestingPlatformSupport<ScrollbarTestingPlatformSupport> platform;
  ENABLE_OVERLAY_SCROLLBARS(GetParam());

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  WebView().MainFrameWidget()->Resize(WebSize(1000, 1000));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style> body { margin: 0px; height: 10000000px; } </style>)HTML");
  ScrollableArea* scrollable_area = GetDocument().View()->LayoutViewport();

  Compositor().BeginFrame();

  scrollable_area->SetScrollOffset(ScrollOffset(0, 10000000),
                                   kProgrammaticScroll);

  Compositor().BeginFrame();

  int scroll_y = scrollable_area->GetScrollOffset().Height();
  ASSERT_EQ(9999000, scroll_y);

  Scrollbar* scrollbar = scrollable_area->VerticalScrollbar();
  ASSERT_TRUE(scrollbar);

  int maximumThumbPosition = WebView().MainFrameWidget()->Size().height -
                             StubWebThemeEngine::kMinimumVerticalLength;
  maximumThumbPosition -= scrollbar->GetTheme().ScrollbarMargin() * 2;

  EXPECT_EQ(maximumThumbPosition,
            scrollbar->GetTheme().ThumbPosition(*scrollbar));
}
#endif

// A body with width just under the window width should not have scrollbars.
TEST_F(ScrollbarsTest, WideBodyShouldNotHaveScrollbars) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
      background: blue;
      height: 10px;
      width: 799px;
    }
  )HTML");
  Compositor().BeginFrame();
  auto* layout_viewport = GetDocument().View()->LayoutViewport();
  EXPECT_FALSE(layout_viewport->VerticalScrollbar());
  EXPECT_FALSE(layout_viewport->HorizontalScrollbar());
}

// A body with height just under the window height should not have scrollbars.
TEST_F(ScrollbarsTest, TallBodyShouldNotHaveScrollbars) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
      background: blue;
      height: 599px;
      width: 10px;
    }
  )HTML");
  Compositor().BeginFrame();
  auto* layout_viewport = GetDocument().View()->LayoutViewport();
  EXPECT_FALSE(layout_viewport->VerticalScrollbar());
  EXPECT_FALSE(layout_viewport->HorizontalScrollbar());
}

// A body with dimensions just barely inside the window dimensions should not
// have scrollbars.
TEST_F(ScrollbarsTest, TallAndWideBodyShouldNotHaveScrollbars) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
      background: blue;
      height: 599px;
      width: 799px;
    }
  )HTML");
  Compositor().BeginFrame();
  auto* layout_viewport = GetDocument().View()->LayoutViewport();
  EXPECT_FALSE(layout_viewport->VerticalScrollbar());
  EXPECT_FALSE(layout_viewport->HorizontalScrollbar());
}

// A body with dimensions equal to the window dimensions should not have
// scrollbars.
TEST_F(ScrollbarsTest, BodySizeEqualWindowSizeShouldNotHaveScrollbars) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
      background: blue;
      height: 600px;
      width: 800px;
    }
  )HTML");
  Compositor().BeginFrame();
  auto* layout_viewport = GetDocument().View()->LayoutViewport();
  EXPECT_FALSE(layout_viewport->VerticalScrollbar());
  EXPECT_FALSE(layout_viewport->HorizontalScrollbar());
}

// A body with percentage width extending beyond the window width should cause a
// horizontal scrollbar.
TEST_F(ScrollbarsTest, WidePercentageBodyShouldHaveScrollbar) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      html { height: 100%; }
      body {
        margin: 0;
        width: 101%;
        height: 10px;
      }
    </style>
  )HTML");
  Compositor().BeginFrame();
  auto* layout_viewport = GetDocument().View()->LayoutViewport();
  EXPECT_FALSE(layout_viewport->VerticalScrollbar());
  EXPECT_TRUE(layout_viewport->HorizontalScrollbar());
}

// Similar to |WidePercentageBodyShouldHaveScrollbar| but with a body height
// equal to the window height.
TEST_F(ScrollbarsTest, WidePercentageAndTallBodyShouldHaveScrollbar) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      html { height: 100%; }
      body {
        margin: 0;
        width: 101%;
        height: 100%;
      }
    </style>
  )HTML");
  Compositor().BeginFrame();
  auto* layout_viewport = GetDocument().View()->LayoutViewport();
  EXPECT_FALSE(layout_viewport->VerticalScrollbar());
  EXPECT_TRUE(layout_viewport->HorizontalScrollbar());
}

// A body with percentage height extending beyond the window height should cause
// a vertical scrollbar.
TEST_F(ScrollbarsTest, TallPercentageBodyShouldHaveScrollbar) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      html { height: 100%; }
      body {
        margin: 0;
        width: 10px;
        height: 101%;
      }
    </style>
  )HTML");
  Compositor().BeginFrame();
  auto* layout_viewport = GetDocument().View()->LayoutViewport();
  EXPECT_TRUE(layout_viewport->VerticalScrollbar());
  EXPECT_FALSE(layout_viewport->HorizontalScrollbar());
}

// Similar to |TallPercentageBodyShouldHaveScrollbar| but with a body width
// equal to the window width.
TEST_F(ScrollbarsTest, TallPercentageAndWideBodyShouldHaveScrollbar) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      html { height: 100%; }
      body {
        margin: 0;
        width: 100%;
        height: 101%;
      }
    </style>
  )HTML");
  Compositor().BeginFrame();
  auto* layout_viewport = GetDocument().View()->LayoutViewport();
  EXPECT_TRUE(layout_viewport->VerticalScrollbar());
  EXPECT_FALSE(layout_viewport->HorizontalScrollbar());
}

// A body with percentage dimensions extending beyond the window dimensions
// should cause scrollbars.
TEST_F(ScrollbarsTest, TallAndWidePercentageBodyShouldHaveScrollbars) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      html { height: 100%; }
      body {
        margin: 0;
        width: 101%;
        height: 101%;
      }
    </style>
  )HTML");
  Compositor().BeginFrame();
  auto* layout_viewport = GetDocument().View()->LayoutViewport();
  EXPECT_TRUE(layout_viewport->VerticalScrollbar());
  EXPECT_TRUE(layout_viewport->HorizontalScrollbar());
}

TEST_F(ScrollbarsTest, MouseOverIFrameScrollbar) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameWidget()->Resize(WebSize(800, 600));

  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
    }
    iframe {
      width: 200px;
      height: 200px;
    }
    </style>
    <iframe id='iframe' src='iframe.html'>
    </iframe>
  )HTML");

  frame_resource.Complete(R"HTML(
  <!DOCTYPE html>
  <style>
  body {
    margin: 0;
    height :500px;
  }
  </style>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();
  Element* iframe = document.getElementById("iframe");
  DCHECK(iframe);

  // Ensure hittest has scrollbar.
  HitTestResult hit_test_result = HitTest(196, 10);
  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_TRUE(hit_test_result.GetScrollbar());
  EXPECT_TRUE(hit_test_result.GetScrollbar()->Enabled());

  // Mouse over scrollbar.
  HandleMouseMoveEvent(196, 5);

  // IFRAME hover.
  EXPECT_EQ(document.HoverElement(), iframe);
}

TEST_F(ScrollbarsTest, AutosizeTest) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameWidget()->Resize(WebSize(0, 0));
  SimRequest resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body, html {
      width: 100%;
      margin: 0;
    }
    #container {
      width: 100.7px;
      height: 150px;
    }
    </style>
    <div id="container"></div>
  )HTML");

  DCHECK(!GetScrollbarTheme().UsesOverlayScrollbars());

  // Needs to dispatch the load event so FramViewAutoSizeInfo doesn't prevent
  // down-sizing.
  test::RunPendingTasks();

  LocalFrameView* frame_view = WebView().MainFrameImpl()->GetFrameView();
  ScrollableArea* layout_viewport = frame_view->LayoutViewport();

  // Enable auto size mode where the frame is resized such that the content
  // doesn't need scrollbars (up to a maximum).
  WebView().EnableAutoResizeMode(WebSize(100, 100), WebSize(100, 200));

  // Note, the frame autosizer doesn't work correctly with subpixel sizes so
  // even though the container is a fraction larger than the frame, we don't
  // consider that for overflow.
  {
    Compositor().BeginFrame();
    EXPECT_FALSE(layout_viewport->VerticalScrollbar());
    EXPECT_FALSE(layout_viewport->HorizontalScrollbar());
    EXPECT_EQ(100, frame_view->FrameRect().Width());
    EXPECT_EQ(150, frame_view->FrameRect().Height());
  }

  // Subsequent autosizes should be stable. Specifically checking the condition
  // from https://crbug.com/811478.
  {
    frame_view->SetNeedsLayout();
    Compositor().BeginFrame();
    EXPECT_FALSE(layout_viewport->VerticalScrollbar());
    EXPECT_FALSE(layout_viewport->HorizontalScrollbar());
    EXPECT_EQ(100, frame_view->FrameRect().Width());
    EXPECT_EQ(150, frame_view->FrameRect().Height());
  }

  // Try again.
  {
    frame_view->SetNeedsLayout();
    Compositor().BeginFrame();
    EXPECT_FALSE(layout_viewport->VerticalScrollbar());
    EXPECT_FALSE(layout_viewport->HorizontalScrollbar());
    EXPECT_EQ(100, frame_view->FrameRect().Width());
    EXPECT_EQ(150, frame_view->FrameRect().Height());
  }
}

TEST_F(ScrollbarsTest, AutosizeAlmostRemovableScrollbar) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);
  WebView().EnableAutoResizeMode(WebSize(25, 25), WebSize(800, 600));

  SimRequest resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  resource.Complete(R"HTML(
    <style>
    body { margin: 0; padding: 15px }
    #b1, #b2 { display: inline-block; width: 205px; height: 45px; }
    #b1 { background: #888; }
    #b2 { background: #bbb; }
    #spacer { width: 400px; height: 490px; background: #eee; }
    </style>
    <div id="b1"></div><div id="b2"></div>
    <div id="spacer"></div>
  )HTML");

  // Finish loading.
  test::RunPendingTasks();

  LocalFrameView* frame_view = WebView().MainFrameImpl()->GetFrameView();
  ScrollableArea* layout_viewport = frame_view->LayoutViewport();

  // Check three times to verify stability.
  for (int i = 0; i < 3; i++) {
    frame_view->SetNeedsLayout();
    Compositor().BeginFrame();
    EXPECT_TRUE(layout_viewport->VerticalScrollbar());
    EXPECT_FALSE(layout_viewport->HorizontalScrollbar());
    EXPECT_EQ(445, frame_view->Width());
    EXPECT_EQ(600, frame_view->Height());
  }
}

TEST_F(ScrollbarsTest,
       HideTheOverlayScrollbarNotCrashAfterPLSADisposedPaintLayer) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);

  WebView().MainFrameWidget()->Resize(WebSize(200, 200));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    #div{ height: 100px; overflow-y:scroll; }
    .big{ height: 2000px; }
    .hide { display: none; }
    </style>
    <div id='div'>
      <div class='big'>
      </div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();
  Element* div = document.getElementById("div");
  PaintLayerScrollableArea* scrollable_div =
      ToLayoutBox(div->GetLayoutObject())->GetScrollableArea();

  scrollable_div->SetScrollbarsHiddenIfOverlay(false);
  ASSERT_TRUE(scrollable_div);
  ASSERT_TRUE(scrollable_div->GetPageScrollbarTheme().UsesOverlayScrollbars());
  ASSERT_TRUE(scrollable_div->VerticalScrollbar());

  EXPECT_FALSE(scrollable_div->ScrollbarsHiddenIfOverlay());

  // Set display:none calls Dispose().
  div->setAttribute(html_names::kClassAttr, "hide");
  Compositor().BeginFrame();

  // After paint layer in scrollable dispose, we can still call scrollbar hidden
  // just not change scrollbar.
  scrollable_div->SetScrollbarsHiddenIfOverlay(true);

  EXPECT_FALSE(scrollable_div->ScrollbarsHiddenIfOverlay());
}

TEST_F(ScrollbarsTest, PLSADisposeShouldClearPointerInLayers) {
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);
  WebView().MainFrameWidget()->Resize(WebSize(200, 200));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    /* transform keeps the graphics layer */
    #div { width: 100px; height: 100px; will-change: transform; }
    .scroller{ overflow: scroll; }
    .big{ height: 2000px; }
    /* positioned so we still keep the PaintLayer */
    .hide { overflow: visible; position: absolute; }
    </style>
    <div id='div' class='scroller' style='z-index:1'>
      <div class='big'>
      </div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();
  Element* div = document.getElementById("div");
  PaintLayerScrollableArea* scrollable_div =
      ToLayoutBox(div->GetLayoutObject())->GetScrollableArea();

  ASSERT_TRUE(scrollable_div);

  PaintLayer* paint_layer = scrollable_div->Layer();
  ASSERT_TRUE(paint_layer);

  cc::Layer* graphics_layer = scrollable_div->LayerForScrolling();
  ASSERT_TRUE(graphics_layer);

  div->setAttribute(html_names::kClassAttr, "hide");
  document.UpdateStyleAndLayout();

  EXPECT_FALSE(paint_layer->GetScrollableArea());
}

TEST_F(ScrollbarsTest, OverlayScrollbarHitTest) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);
  // Skip this test if scrollbars don't allow hit testing on the platform.
  if (!WebView().GetPage()->GetScrollbarTheme().AllowsHitTest())
    return;

  WebView().MainFrameWidget()->Resize(WebSize(300, 300));

  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    body {
      margin: 0;
      height: 2000px;
    }
    iframe {
      height: 200px;
      width: 200px;
    }
    </style>
    <iframe id='iframe' src='iframe.html'>
    </iframe>
  )HTML");
  Compositor().BeginFrame();

  // Enable the main frame scrollbar.
  WebView()
      .MainFrameImpl()
      ->GetFrameView()
      ->LayoutViewport()
      ->SetScrollbarsHiddenIfOverlay(false);

  frame_resource.Complete("<!DOCTYPE html><body style='height: 999px'></body>");
  Compositor().BeginFrame();

  // Enable the iframe scrollbar.
  auto* iframe_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("iframe"));
  iframe_element->contentDocument()
      ->View()
      ->LayoutViewport()
      ->SetScrollbarsHiddenIfOverlay(false);

  // Hit test on and off the main frame scrollbar.
  HitTestResult hit_test_result = HitTest(295, 5);
  EXPECT_TRUE(hit_test_result.GetScrollbar());
  hit_test_result = HitTest(250, 5);
  EXPECT_FALSE(hit_test_result.GetScrollbar());

  // Hit test on and off the iframe scrollbar.
  hit_test_result = HitTest(195, 5);
  EXPECT_TRUE(hit_test_result.GetScrollbar());
  hit_test_result = HitTest(150, 5);
  EXPECT_FALSE(hit_test_result.GetScrollbar());
}

TEST_F(ScrollbarsTest, AllowMiddleButtonPressOnScrollbar) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameWidget()->Resize(WebSize(200, 200));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    #big {
      height: 800px;
    }
    </style>
    <div id='big'>
    </div>
  )HTML");
  Compositor().BeginFrame();

  ScrollableArea* scrollable_area =
      WebView().MainFrameImpl()->GetFrameView()->LayoutViewport();

  Scrollbar* scrollbar = scrollable_area->VerticalScrollbar();
  ASSERT_TRUE(scrollbar);
  ASSERT_TRUE(scrollbar->Enabled());

  // Not allow press scrollbar with middle button.
  HandleMouseMoveEvent(195, 5);
  HandleMouseMiddlePressEvent(195, 5);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kThumbPart);
  HandleMouseMiddleReleaseEvent(195, 5);
}

// Ensure Scrollbar not release press by middle button down.
TEST_F(ScrollbarsTest, MiddleDownShouldNotAffectScrollbarPress) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameWidget()->Resize(WebSize(200, 200));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    #big {
      height: 800px;
    }
    </style>
    <div id='big'>
    </div>
  )HTML");
  Compositor().BeginFrame();

  ScrollableArea* scrollable_area =
      WebView().MainFrameImpl()->GetFrameView()->LayoutViewport();

  Scrollbar* scrollbar = scrollable_area->VerticalScrollbar();
  ASSERT_TRUE(scrollbar);
  ASSERT_TRUE(scrollbar->Enabled());

  // Press on scrollbar then move mouse out of scrollbar and middle click
  // should not release the press state. Then relase mouse left button should
  // release the scrollbar press state.

  // Move mouse to thumb.
  HandleMouseMoveEvent(195, 5);
  HandleMousePressEvent(195, 5);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kThumbPart);

  // Move mouse out of scrollbar with press.
  WebMouseEvent event(WebInputEvent::kMouseMove, WebFloatPoint(5, 5),
                      WebFloatPoint(5, 5), WebPointerProperties::Button::kLeft,
                      0, WebInputEvent::Modifiers::kLeftButtonDown,
                      base::TimeTicks::Now());
  event.SetFrameScale(1);
  GetEventHandler().HandleMouseLeaveEvent(event);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kThumbPart);

  // Middle click should not release scrollbar press state.
  HandleMouseMiddlePressEvent(5, 5);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kThumbPart);

  // Middle button release should release scrollbar press state.
  HandleMouseMiddleReleaseEvent(5, 5);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kNoPart);
}

TEST_F(ScrollbarsTest, UseCounterNegativeWhenThumbIsNotScrolledWithMouse) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameWidget()->Resize(WebSize(200, 200));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
     #content { height: 350px; width: 350px; }
    </style>
    <div id='scrollable'>
     <div id='content'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  ScrollableArea* scrollable_area =
      WebView().MainFrameImpl()->GetFrameView()->LayoutViewport();
  EXPECT_TRUE(scrollable_area->VerticalScrollbar());
  EXPECT_TRUE(scrollable_area->HorizontalScrollbar());
  Scrollbar* vertical_scrollbar = scrollable_area->VerticalScrollbar();
  Scrollbar* horizontal_scrollbar = scrollable_area->HorizontalScrollbar();
  EXPECT_EQ(vertical_scrollbar->PressedPart(), ScrollbarPart::kNoPart);
  EXPECT_EQ(horizontal_scrollbar->PressedPart(), ScrollbarPart::kNoPart);

  // Scrolling the page with a mouse wheel won't trigger the UseCounter.
  WebView().MainFrameWidget()->HandleInputEvent(
      GenerateWheelGestureEvent(WebInputEvent::kGestureScrollBegin,
                                IntPoint(100, 100), ScrollOffset(0, -100)));
  WebView().MainFrameWidget()->HandleInputEvent(
      GenerateWheelGestureEvent(WebInputEvent::kGestureScrollUpdate,
                                IntPoint(100, 100), ScrollOffset(0, -100)));
  WebView().MainFrameWidget()->HandleInputEvent(GenerateWheelGestureEvent(
      WebInputEvent::kGestureScrollEnd, IntPoint(100, 100)));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kVerticalScrollbarThumbScrollingWithMouse));

  // Hovering over the vertical scrollbar won't trigger the UseCounter.
  HandleMouseMoveEvent(195, 5);
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kVerticalScrollbarThumbScrollingWithMouse));

  // Hovering over the horizontal scrollbar won't trigger the UseCounter.
  HandleMouseMoveEvent(5, 195);
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kHorizontalScrollbarThumbScrollingWithMouse));

  // Clicking on the vertical scrollbar won't trigger the UseCounter.
  HandleMousePressEvent(195, 175);
  EXPECT_EQ(vertical_scrollbar->PressedPart(),
            ScrollbarPart::kForwardTrackPart);
  HandleMouseReleaseEvent(195, 175);
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kVerticalScrollbarThumbScrollingWithMouse));

  // Clicking on the horizontal scrollbar won't trigger the UseCounter.
  HandleMousePressEvent(175, 195);
  EXPECT_EQ(horizontal_scrollbar->PressedPart(),
            ScrollbarPart::kForwardTrackPart);
  HandleMouseReleaseEvent(175, 195);
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kHorizontalScrollbarThumbScrollingWithMouse));

  // Clicking outside the scrollbar and then releasing over the thumb of the
  // vertical scrollbar won't trigger the UseCounter.
  HandleMousePressEvent(50, 50);
  HandleMouseMoveEvent(195, 5);
  HandleMouseReleaseEvent(195, 5);
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kVerticalScrollbarThumbScrollingWithMouse));

  // Clicking outside the scrollbar and then releasing over the thumb of the
  // horizontal scrollbar won't trigger the UseCounter.
  HandleMousePressEvent(50, 50);
  HandleMouseMoveEvent(5, 195);
  HandleMouseReleaseEvent(5, 195);
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kHorizontalScrollbarThumbScrollingWithMouse));
}

TEST_F(ScrollbarsTest, UseCounterPositiveWhenThumbIsScrolledWithMouse) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameWidget()->Resize(WebSize(200, 200));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
     #content { height: 350px; width: 350px; }
    </style>
    <div id='scrollable'>
     <div id='content'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  ScrollableArea* scrollable_area =
      WebView().MainFrameImpl()->GetFrameView()->LayoutViewport();
  EXPECT_TRUE(scrollable_area->VerticalScrollbar());
  EXPECT_TRUE(scrollable_area->HorizontalScrollbar());
  Scrollbar* vertical_scrollbar = scrollable_area->VerticalScrollbar();
  Scrollbar* horizontal_scrollbar = scrollable_area->HorizontalScrollbar();
  EXPECT_EQ(vertical_scrollbar->PressedPart(), ScrollbarPart::kNoPart);
  EXPECT_EQ(horizontal_scrollbar->PressedPart(), ScrollbarPart::kNoPart);

  // Clicking the thumb on the vertical scrollbar will trigger the UseCounter.
  HandleMousePressEvent(195, 5);
  EXPECT_EQ(vertical_scrollbar->PressedPart(), ScrollbarPart::kThumbPart);
  HandleMouseReleaseEvent(195, 5);
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kVerticalScrollbarThumbScrollingWithMouse));

  // Clicking the thumb on the horizontal scrollbar will trigger the UseCounter.
  HandleMousePressEvent(5, 195);
  EXPECT_EQ(horizontal_scrollbar->PressedPart(), ScrollbarPart::kThumbPart);
  HandleMouseReleaseEvent(5, 195);
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kHorizontalScrollbarThumbScrollingWithMouse));
}

TEST_F(ScrollbarsTest, UseCounterNegativeWhenThumbIsNotScrolledWithTouch) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameWidget()->Resize(WebSize(200, 200));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
     #content { height: 350px; width: 350px; }
    </style>
    <div id='scrollable'>
     <div id='content'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  ScrollableArea* scrollable_area =
      WebView().MainFrameImpl()->GetFrameView()->LayoutViewport();
  EXPECT_TRUE(scrollable_area->VerticalScrollbar());
  EXPECT_TRUE(scrollable_area->HorizontalScrollbar());
  Scrollbar* vertical_scrollbar = scrollable_area->VerticalScrollbar();
  Scrollbar* horizontal_scrollbar = scrollable_area->HorizontalScrollbar();
  EXPECT_EQ(vertical_scrollbar->PressedPart(), ScrollbarPart::kNoPart);
  EXPECT_EQ(horizontal_scrollbar->PressedPart(), ScrollbarPart::kNoPart);

  // Tapping on the vertical scrollbar won't trigger the UseCounter.
  WebView().MainFrameWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::kGestureTapDown, IntPoint(195, 175)));
  EXPECT_EQ(vertical_scrollbar->PressedPart(),
            ScrollbarPart::kForwardTrackPart);
  WebView().MainFrameWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::kGestureTapCancel, IntPoint(195, 175)));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kVerticalScrollbarThumbScrollingWithTouch));

  // Tapping on the horizontal scrollbar won't trigger the UseCounter.
  WebView().MainFrameWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::kGestureTapDown, IntPoint(175, 195)));
  EXPECT_EQ(horizontal_scrollbar->PressedPart(),
            ScrollbarPart::kForwardTrackPart);
  WebView().MainFrameWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::kGestureTapCancel, IntPoint(175, 195)));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kHorizontalScrollbarThumbScrollingWithTouch));

  // Tapping outside the scrollbar and then releasing over the thumb of the
  // vertical scrollbar won't trigger the UseCounter.
  WebView().MainFrameWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::kGestureTapDown, IntPoint(50, 50)));
  WebView().MainFrameWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::kGestureTapCancel, IntPoint(195, 5)));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kVerticalScrollbarThumbScrollingWithTouch));

  // Tapping outside the scrollbar and then releasing over the thumb of the
  // horizontal scrollbar won't trigger the UseCounter.
  WebView().MainFrameWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::kGestureTapDown, IntPoint(50, 50)));
  WebView().MainFrameWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::kGestureTapCancel, IntPoint(5, 195)));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kHorizontalScrollbarThumbScrollingWithTouch));
}

TEST_F(ScrollbarsTest, UseCounterPositiveWhenThumbIsScrolledWithTouch) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameWidget()->Resize(WebSize(200, 200));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
     #content { height: 350px; width: 350px; }
    </style>
    <div id='scrollable'>
     <div id='content'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  ScrollableArea* scrollable_area =
      WebView().MainFrameImpl()->GetFrameView()->LayoutViewport();
  EXPECT_TRUE(scrollable_area->VerticalScrollbar());
  EXPECT_TRUE(scrollable_area->HorizontalScrollbar());
  Scrollbar* vertical_scrollbar = scrollable_area->VerticalScrollbar();
  Scrollbar* horizontal_scrollbar = scrollable_area->HorizontalScrollbar();
  EXPECT_EQ(vertical_scrollbar->PressedPart(), ScrollbarPart::kNoPart);
  EXPECT_EQ(horizontal_scrollbar->PressedPart(), ScrollbarPart::kNoPart);

  // Clicking the thumb on the vertical scrollbar will trigger the UseCounter.
  WebView().MainFrameWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::kGestureTapDown, IntPoint(195, 5)));
  EXPECT_EQ(vertical_scrollbar->PressedPart(), ScrollbarPart::kThumbPart);
  WebView().MainFrameWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::kGestureTapCancel, IntPoint(195, 5)));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kVerticalScrollbarThumbScrollingWithTouch));

  // Clicking the thumb on the horizontal scrollbar will trigger the UseCounter.
  WebView().MainFrameWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::kGestureTapDown, IntPoint(5, 195)));
  EXPECT_EQ(horizontal_scrollbar->PressedPart(), ScrollbarPart::kThumbPart);
  WebView().MainFrameWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::kGestureTapCancel, IntPoint(5, 195)));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kHorizontalScrollbarThumbScrollingWithTouch));
}

TEST_F(ScrollbarsTest, CheckScrollCornerIfThereIsNoScrollbar) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);

  WebView().MainFrameWidget()->Resize(WebSize(200, 200));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #container {
        width: 50px;
        height: 100px;
        overflow-x: auto;
      }
      #content {
        width: 75px;
        height: 50px;
        background-color: green;
      }
      #container::-webkit-scrollbar {
        height: 8px;
        width: 8px;
      }
      #container::-webkit-scrollbar-corner {
        background: transparent;
      }
    </style>
    <div id='container'>
        <div id='content'></div>
    </div>
  )HTML");

  Compositor().BeginFrame();

  auto* element = GetDocument().getElementById("container");
  PaintLayerScrollableArea* scrollable_container =
      ToLayoutBox(element->GetLayoutObject())->GetScrollableArea();

  // There should initially be a scrollbar and a scroll corner.
  EXPECT_TRUE(scrollable_container->HasScrollbar());
  EXPECT_TRUE(scrollable_container->ScrollCorner());

  // Make the container non-scrollable so the scrollbar and corner disappear.
  element->setAttribute(html_names::kStyleAttr, "width: 100px;");
  GetDocument().UpdateStyleAndLayout();

  EXPECT_FALSE(scrollable_container->HasScrollbar());
  EXPECT_FALSE(scrollable_container->ScrollCorner());
}

// For infinite scrolling page (load more content when scroll to bottom), user
// press on scrollbar button should keep scrolling after content loaded.
// Disable on Android since VirtualTime not work for Android.
// http://crbug.com/633321
#if defined(OS_ANDROID)
TEST_F(ScrollbarsTestWithVirtualTimer,
       DISABLED_PressScrollbarButtonOnInfiniteScrolling) {
#else
TEST_F(ScrollbarsTestWithVirtualTimer,
       PressScrollbarButtonOnInfiniteScrolling) {
#endif
  TimeAdvance();
  GetDocument().GetFrame()->GetSettings()->SetScrollAnimatorEnabled(false);
  WebView().MainFrameWidget()->Resize(WebSize(200, 200));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  RunTasksForPeriod(base::TimeDelta::FromMilliseconds(1000));
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body{
      margin: 0;
    }
    ::-webkit-scrollbar {
      width: 30px;
      height: 30px;
    }
    ::-webkit-scrollbar-button {
      width: 30px;
      height: 30px;
      background: #00FF00;
      display: block;
    }
    ::-webkit-scrollbar-thumb {
      background: #0000FF;
    }
    ::-webkit-scrollbar-track {
      background: #aaaaaa;
    }
    #big {
      height: 400px;
    }
    </style>
    <div id='big'>
    </div>
  )HTML");

  Compositor().BeginFrame();

  ScrollableArea* scrollable_area =
      WebView().MainFrameImpl()->GetFrameView()->LayoutViewport();
  Scrollbar* scrollbar = scrollable_area->VerticalScrollbar();

  // Scroll to bottom.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 400), kProgrammaticScroll,
                                   kScrollBehaviorInstant);
  EXPECT_EQ(scrollable_area->ScrollOffsetInt(), IntSize(0, 200));

  HandleMouseMoveEvent(195, 195);
  HandleMousePressEvent(195, 195);
  ASSERT_EQ(scrollbar->PressedPart(), ScrollbarPart::kForwardButtonEndPart);

  // Wait for 2 delay.
  RunTasksForPeriod(base::TimeDelta::FromMilliseconds(1000));
  RunTasksForPeriod(base::TimeDelta::FromMilliseconds(1000));
  // Change #big size.
  MainFrame().ExecuteScript(WebScriptSource(
      "document.getElementById('big').style.height = '1000px';"));
  Compositor().BeginFrame();

  RunTasksForPeriod(base::TimeDelta::FromMilliseconds(1000));
  RunTasksForPeriod(base::TimeDelta::FromMilliseconds(1000));

  // Verify that the scrollbar autopress timer requested some scrolls via
  // gestures. The button was pressed for 2 seconds and the timer fires
  // every 250ms - we should have at least 7 injected gesture updates.
  EXPECT_GT(WebWidgetClient().GetInjectedScrollGestureData().size(), 6u);
}

class ScrollbarTrackMarginsTest : public ScrollbarsTest {
 public:
  void PrepareTest(const String& track_style) {
    WebView().MainFrameWidget()->Resize(WebSize(200, 200));

    SimRequest request("https://example.com/test.html", "text/html");
    LoadURL("https://example.com/test.html");
    request.Complete(R"HTML(
      <!DOCTYPE html>
        <style>
        ::-webkit-scrollbar {
          width: 10px;
        })HTML" + track_style +
                     R"HTML(
        #d1 {
          position: absolute;
          left: 0;
          right: 0;
          top: 0;
          bottom: 0;
          overflow-x:scroll;
          overflow-y:scroll;
        }
      </style>
      <div id='d1'/>
    )HTML");

    // No DCHECK failure. Issue 801123.
    Compositor().BeginFrame();

    Element* div = GetDocument().getElementById("d1");
    ASSERT_TRUE(div);

    ScrollableArea* div_scrollable =
        ToLayoutBox(div->GetLayoutObject())->GetScrollableArea();

    ASSERT_TRUE(div_scrollable->HorizontalScrollbar());
    CustomScrollbar* horizontal_scrollbar =
        To<CustomScrollbar>(div_scrollable->HorizontalScrollbar());
    horizontal_track_ = horizontal_scrollbar->GetPart(kTrackBGPart);
    ASSERT_TRUE(horizontal_track_);

    ASSERT_TRUE(div_scrollable->VerticalScrollbar());
    CustomScrollbar* vertical_scrollbar =
        To<CustomScrollbar>(div_scrollable->VerticalScrollbar());
    vertical_track_ = vertical_scrollbar->GetPart(kTrackBGPart);
    ASSERT_TRUE(vertical_track_);
  }

  LayoutCustomScrollbarPart* horizontal_track_ = nullptr;
  LayoutCustomScrollbarPart* vertical_track_ = nullptr;
};

TEST_F(ScrollbarTrackMarginsTest,
       CustomScrollbarFractionalMarginsWillNotCauseDCHECKFailure) {
  PrepareTest(R"CSS(
    ::-webkit-scrollbar-track {
      margin-left: 10.2px;
      margin-top: 20.4px;
      margin-right: 30.6px;
      margin-bottom: 40.8px;
    })CSS");

  EXPECT_EQ(10, horizontal_track_->MarginLeft());
  EXPECT_EQ(31, horizontal_track_->MarginRight());
  EXPECT_EQ(20, vertical_track_->MarginTop());
  EXPECT_EQ(41, vertical_track_->MarginBottom());
}

TEST_F(ScrollbarTrackMarginsTest,
       CustomScrollbarScaledMarginsWillNotCauseDCHECKFailure) {
  WebView().SetZoomFactorForDeviceScaleFactor(1.25f);

  PrepareTest(R"CSS(
    ::-webkit-scrollbar-track {
      margin-left: 11px;
      margin-top: 21px;
      margin-right: 31px;
      margin-bottom: 41px;
    })CSS");

  EXPECT_EQ(14, horizontal_track_->MarginLeft());
  EXPECT_EQ(39, horizontal_track_->MarginRight());
  EXPECT_EQ(26, vertical_track_->MarginTop());
  EXPECT_EQ(51, vertical_track_->MarginBottom());
}

class ScrollbarColorSchemeTest : public ScrollbarAppearanceTest {};

INSTANTIATE_TEST_SUITE_P(NonOverlay,
                         ScrollbarColorSchemeTest,
                         testing::Values(false));

#if defined(OS_ANDROID) || defined(OS_MACOSX)
// Not able to paint non-overlay scrollbars through ThemeEngine on Android or
// Mac.
#define MAYBE_ThemeEnginePaint DISABLED_ThemeEnginePaint
#else
#define MAYBE_ThemeEnginePaint ThemeEnginePaint
#endif

TEST_P(ScrollbarColorSchemeTest, MAYBE_ThemeEnginePaint) {
  ScopedTestingPlatformSupport<ScrollbarTestingPlatformSupport> platform;
  ScopedCSSColorSchemeForTest css_feature_scope(true);

  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #scrollable {
        width: 100px;
        height: 100px;
        overflow: scroll;
        color-scheme: dark;
      }
      #filler {
        width: 200px;
        height: 200px;
      }
    </style>
    <div id="scrollable">
      <div id="filler"></div>
    </div>
  )HTML");

  ColorSchemeHelper color_scheme_helper;
  color_scheme_helper.SetPreferredColorScheme(GetDocument(),
                                              PreferredColorScheme::kDark);

  Compositor().BeginFrame();

  auto* theme_engine =
      static_cast<StubWebThemeEngine*>(Platform::Current()->ThemeEngine());
  EXPECT_EQ(WebColorScheme::kDark,
            theme_engine->GetPaintedPartColorScheme(
                WebThemeEngine::kPartScrollbarHorizontalThumb));
  EXPECT_EQ(WebColorScheme::kDark,
            theme_engine->GetPaintedPartColorScheme(
                WebThemeEngine::kPartScrollbarVerticalThumb));
  EXPECT_EQ(WebColorScheme::kDark, theme_engine->GetPaintedPartColorScheme(
                                       WebThemeEngine::kPartScrollbarCorner));
}

}  // namespace blink
