// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "cc/paint/record_paint_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/dev_tools_emulator.h"
#include "third_party/blink/renderer/core/layout/custom_scrollbar.h"
#include "third_party/blink/renderer/core/layout/layout_custom_scrollbar_part.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay_mock.h"
#include "third_party/blink/renderer/core/testing/color_scheme_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_helper.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-blink.h"

namespace blink {

namespace {

class StubWebThemeEngine : public WebThemeEngine {
 public:
  StubWebThemeEngine() {
    painted_color_scheme_.fill(mojom::blink::ColorScheme::kLight);
  }

  gfx::Size GetSize(Part part) override {
    switch (part) {
      case kPartScrollbarHorizontalThumb:
        return gfx::Size(kMinimumHorizontalLength, 15);
      case kPartScrollbarVerticalThumb:
        return gfx::Size(15, kMinimumVerticalLength);
      default:
        return gfx::Size();
    }
  }
  static constexpr int kMinimumHorizontalLength = 51;
  static constexpr int kMinimumVerticalLength = 52;

  void Paint(cc::PaintCanvas*,
             Part part,
             State,
             const gfx::Rect&,
             const ExtraParams*,
             mojom::blink::ColorScheme color_scheme,
             bool in_forced_colors,
             const ui::ColorProvider* color_provider,
             const std::optional<SkColor>& accent_color) override {
    // Make  sure we don't overflow the array.
    DCHECK(part <= kPartProgressBar);
    painted_color_scheme_[part] = color_scheme;
  }

  mojom::blink::ColorScheme GetPaintedPartColorScheme(Part part) const {
    return painted_color_scheme_[part];
  }

  SkColor4f GetScrollbarThumbColor(State,
                                   const ExtraParams*,
                                   const ui::ColorProvider*) const override {
    return SkColors::kRed;
  }

 private:
  std::array<mojom::blink::ColorScheme, kPartProgressBar + 1>
      painted_color_scheme_;
};

constexpr int StubWebThemeEngine::kMinimumHorizontalLength;
constexpr int StubWebThemeEngine::kMinimumVerticalLength;

class ScopedStubThemeEngine {
 public:
  ScopedStubThemeEngine() {
    old_theme_ = WebThemeEngineHelper::SwapNativeThemeEngineForTesting(
        std::make_unique<StubWebThemeEngine>());
  }

  ~ScopedStubThemeEngine() {
    WebThemeEngineHelper::SwapNativeThemeEngineForTesting(
        std::move(old_theme_));
  }

 private:
  std::unique_ptr<WebThemeEngine> old_theme_;
};

}  // namespace

class ScrollbarsTest : public PaintTestConfigurations, public SimTest {
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
    SetOverlayScrollbarsEnabled(original_overlay_scrollbars_enabled_);
    mock_overlay_scrollbars_.reset();
    SimTest::TearDown();
  }

  void SetOverlayScrollbarsEnabled(bool enabled) {
    if (enabled != ScrollbarThemeSettings::OverlayScrollbarsEnabled()) {
      ScrollbarThemeSettings::SetOverlayScrollbarsEnabled(enabled);
      Page::UsesOverlayScrollbarsChanged();
    }
  }

  HitTestResult HitTest(int x, int y) {
    return WebView().MainFrameViewWidget()->CoreHitTestResultAt(
        gfx::PointF(x, y));
  }

  EventHandler& GetEventHandler() {
    return GetDocument().GetFrame()->GetEventHandler();
  }

  void HandleMouseMoveEvent(int x, int y) {
    WebMouseEvent event(WebInputEvent::Type::kMouseMove, gfx::PointF(x, y),
                        gfx::PointF(x, y),
                        WebPointerProperties::Button::kNoButton, 0,
                        WebInputEvent::kNoModifiers, base::TimeTicks::Now());
    event.SetFrameScale(1);
    GetEventHandler().HandleMouseMoveEvent(event, Vector<WebMouseEvent>(),
                                           Vector<WebMouseEvent>());
  }

  void HandleMousePressEvent(int x,
                             int y,
                             WebPointerProperties::Button button =
                                 WebPointerProperties::Button::kLeft) {
    WebMouseEvent event(WebInputEvent::Type::kMouseDown, gfx::PointF(x, y),
                        gfx::PointF(x, y), button, 0,
                        WebInputEvent::Modifiers::kLeftButtonDown,
                        base::TimeTicks::Now());
    event.SetFrameScale(1);
    GetEventHandler().HandleMousePressEvent(event);
  }

  void HandleContextMenuEvent(int x, int y) {
    WebMouseEvent event(
        WebInputEvent::Type::kMouseDown, gfx::PointF(x, y), gfx::PointF(x, y),
        WebPointerProperties::Button::kNoButton, 0,
        WebInputEvent::Modifiers::kNoModifiers, base::TimeTicks::Now());
    event.SetFrameScale(1);
    GetEventHandler().SendContextMenuEvent(event);
  }

  void HandleMouseReleaseEvent(int x,
                               int y,
                               WebPointerProperties::Button button =
                                   WebPointerProperties::Button::kLeft) {
    WebMouseEvent event(WebInputEvent::Type::kMouseUp, gfx::PointF(x, y),
                        gfx::PointF(x, y), button, 0,
                        WebInputEvent::Modifiers::kNoModifiers,
                        base::TimeTicks::Now());
    event.SetFrameScale(1);
    GetEventHandler().HandleMouseReleaseEvent(event);
  }

  void HandleMouseMiddlePressEvent(int x, int y) {
    WebMouseEvent event(
        WebInputEvent::Type::kMouseDown, gfx::PointF(x, y), gfx::PointF(x, y),
        WebPointerProperties::Button::kMiddle, 0,
        WebInputEvent::Modifiers::kMiddleButtonDown, base::TimeTicks::Now());
    event.SetFrameScale(1);
    GetEventHandler().HandleMousePressEvent(event);
  }

  void HandleMouseMiddleReleaseEvent(int x, int y) {
    WebMouseEvent event(
        WebInputEvent::Type::kMouseUp, gfx::PointF(x, y), gfx::PointF(x, y),
        WebPointerProperties::Button::kMiddle, 0,
        WebInputEvent::Modifiers::kMiddleButtonDown, base::TimeTicks::Now());
    event.SetFrameScale(1);
    GetEventHandler().HandleMouseReleaseEvent(event);
  }

  void HandleMouseLeaveEvent() {
    WebMouseEvent event(WebInputEvent::Type::kMouseLeave, gfx::PointF(1, 1),
                        gfx::PointF(1, 1), WebPointerProperties::Button::kLeft,
                        0, WebInputEvent::Modifiers::kLeftButtonDown,
                        base::TimeTicks::Now());
    event.SetFrameScale(1);
    GetEventHandler().HandleMouseLeaveEvent(event);
  }

  WebGestureEvent GenerateWheelGestureEvent(
      WebInputEvent::Type type,
      const gfx::Point& position,
      ScrollOffset offset = ScrollOffset()) {
    return GenerateGestureEvent(type, WebGestureDevice::kTouchpad, position,
                                offset);
  }

  WebCoalescedInputEvent GenerateTouchGestureEvent(
      WebInputEvent::Type type,
      const gfx::Point& position,
      ScrollOffset offset = ScrollOffset()) {
    return WebCoalescedInputEvent(
        GenerateGestureEvent(type, WebGestureDevice::kTouchscreen, position,
                             offset),
        ui::LatencyInfo());
  }

  ui::mojom::blink::CursorType CursorType() {
    return GetDocument()
        .GetFrame()
        ->GetChromeClient()
        .LastSetCursorForTesting()
        .type();
  }

  ScrollbarTheme& GetScrollbarTheme() {
    return GetDocument().GetPage()->GetScrollbarTheme();
  }

  PaintLayerScrollableArea* GetScrollableArea(const Element& element) const {
    return element.GetLayoutBox()->GetScrollableArea();
  }

 protected:
  WebGestureEvent GenerateGestureEvent(WebInputEvent::Type type,
                                       WebGestureDevice device,
                                       const gfx::Point& position,
                                       ScrollOffset offset) {
    WebGestureEvent event(type, WebInputEvent::kNoModifiers,
                          base::TimeTicks::Now(), device);

    event.SetPositionInWidget(gfx::PointF(position.x(), position.y()));

    if (type == WebInputEvent::Type::kGestureScrollUpdate) {
      event.data.scroll_update.delta_x = offset.x();
      event.data.scroll_update.delta_y = offset.y();
    } else if (type == WebInputEvent::Type::kGestureScrollBegin) {
      event.data.scroll_begin.delta_x_hint = offset.x();
      event.data.scroll_begin.delta_y_hint = offset.y();
    }
    return event;
  }

 private:
  ScopedStubThemeEngine scoped_theme_;
  std::unique_ptr<ScopedMockOverlayScrollbars> mock_overlay_scrollbars_;
  bool original_overlay_scrollbars_enabled_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(ScrollbarsTest);

class ScrollbarsTestWithVirtualTimer : public ScrollbarsTest {
 public:
  void SetUp() override {
    ScrollbarsTest::SetUp();
    GetVirtualTimeController()->EnableVirtualTime(base::Time());
  }

  void TearDown() override {
    GetVirtualTimeController()->DisableVirtualTimeForTesting();
    ScrollbarsTest::TearDown();
  }

  void TimeAdvance() {
    GetVirtualTimeController()->SetVirtualTimePolicy(
        VirtualTimeController::VirtualTimePolicy::kAdvance);
  }

  void StopVirtualTimeAndExitRunLoop(base::OnceClosure quit_closure) {
    GetVirtualTimeController()->SetVirtualTimePolicy(
        VirtualTimeController::VirtualTimePolicy::kPause);
    std::move(quit_closure).Run();
  }

  // Some task queues may have repeating v8 tasks that run forever so we impose
  // a hard (virtual) time limit.
  void RunTasksForPeriod(base::TimeDelta delay) {
    base::RunLoop loop;
    TimeAdvance();
    scheduler::GetSingleThreadTaskRunnerForTesting()->PostDelayedTask(
        FROM_HERE,
        WTF::BindOnce(
            &ScrollbarsTestWithVirtualTimer::StopVirtualTimeAndExitRunLoop,
            WTF::Unretained(this), loop.QuitClosure()),
        delay);
    loop.Run();
  }

  VirtualTimeController* GetVirtualTimeController() {
    return WebView().Scheduler()->GetVirtualTimeController();
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(ScrollbarsTestWithVirtualTimer);

// Try to force enable/disable overlay. Skip the test if the desired setting
// is not supported by the platform.
#define ENABLE_OVERLAY_SCROLLBARS(b)                                           \
  do {                                                                         \
    SetOverlayScrollbarsEnabled(b);                                            \
    if (WebView().GetPage()->GetScrollbarTheme().UsesOverlayScrollbars() != b) \
      return;                                                                  \
  } while (false)

TEST_P(ScrollbarsTest, DocumentStyleRecalcPreservesScrollbars) {
  v8::HandleScope handle_scope(
      WebView().GetPage()->GetAgentGroupScheduler().Isolate());
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
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

TEST_P(ScrollbarsTest, ScrollbarsUpdatedOnOverlaySettingsChange) {
  ENABLE_OVERLAY_SCROLLBARS(true);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style> body { height: 3000px; } </style>)HTML");

  Compositor().BeginFrame();
  auto* layout_viewport = GetDocument().View()->LayoutViewport();
  EXPECT_TRUE(layout_viewport->VerticalScrollbar()->IsOverlayScrollbar());

  ENABLE_OVERLAY_SCROLLBARS(false);
  Compositor().BeginFrame();
  EXPECT_FALSE(layout_viewport->VerticalScrollbar()->IsOverlayScrollbar());
}

TEST(ScrollbarsTestWithOwnWebViewHelper, ScrollbarSizeF) {
  test::TaskEnvironment task_environment;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform;
  frame_test_helpers::WebViewHelper web_view_helper;
  // Needed so visual viewport supplies its own scrollbars. We don't support
  // this setting changing after initialization, so we must set it through
  // WebViewHelper.
  web_view_helper.set_viewport_enabled(true);

  WebViewImpl* web_view_impl = web_view_helper.Initialize();

  web_view_impl->MainFrameViewWidget()->SetDeviceScaleFactorForTesting(1.f);
  web_view_impl->MainFrameViewWidget()->Resize(gfx::Size(800, 600));

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
  web_view_impl->MainFrameViewWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  Document* document =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame())->GetDocument();

  VisualViewport& visual_viewport = document->GetPage()->GetVisualViewport();
  int horizontal_scrollbar =
      visual_viewport.LayerForHorizontalScrollbar()->bounds().height();
  int vertical_scrollbar =
      visual_viewport.LayerForVerticalScrollbar()->bounds().width();

  const float device_scale = 3.5f;
  web_view_impl->MainFrameViewWidget()->SetDeviceScaleFactorForTesting(
      device_scale);
  web_view_impl->MainFrameViewWidget()->Resize(gfx::Size(400, 300));

  EXPECT_EQ(ClampTo<int>(std::floor(horizontal_scrollbar * device_scale)),
            visual_viewport.LayerForHorizontalScrollbar()->bounds().height());
  EXPECT_EQ(ClampTo<int>(std::floor(vertical_scrollbar * device_scale)),
            visual_viewport.LayerForVerticalScrollbar()->bounds().width());

  web_view_impl->MainFrameViewWidget()->SetDeviceScaleFactorForTesting(1.f);
  web_view_impl->MainFrameViewWidget()->Resize(gfx::Size(800, 600));

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
TEST_P(ScrollbarsTest, CustomScrollbarsCauseLayoutOnExistenceChange) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
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

TEST_P(ScrollbarsTest, TransparentBackgroundUsesLightOverlayColorScheme) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  WebView().SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
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

  EXPECT_EQ(mojom::blink::ColorScheme::kLight,
            layout_viewport->GetOverlayScrollbarColorScheme());
}

TEST_P(ScrollbarsTest, BodyBackgroundChangesOverlayColorTheme) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);

  v8::HandleScope handle_scope(
      WebView().GetPage()->GetAgentGroupScheduler().Isolate());
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <body style='background:white'></body>
  )HTML");
  Compositor().BeginFrame();

  ScrollableArea* layout_viewport = GetDocument().View()->LayoutViewport();

  EXPECT_EQ(mojom::blink::ColorScheme::kLight,
            layout_viewport->GetOverlayScrollbarColorScheme());

  MainFrame().ExecuteScriptAndReturnValue(
      WebScriptSource("document.body.style.backgroundColor = 'black';"));

  Compositor().BeginFrame();
  EXPECT_EQ(mojom::blink::ColorScheme::kDark,
            layout_viewport->GetOverlayScrollbarColorScheme());
}

// Ensure overlay scrollbar change to display:none correctly.
TEST_P(ScrollbarsTest, OverlayScrollbarChangeToDisplayNoneDynamically) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
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
  Element* div = document.getElementById(AtomicString("div"));

  // Ensure we have overlay scrollbar for div and root.
  auto* scrollable_div = GetScrollableArea(*div);

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
  div->setAttribute(html_names::kClassAttr, AtomicString("noscrollbars"));
  document.body()->setAttribute(html_names::kClassAttr,
                                AtomicString("noscrollbars"));
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

// Ensure that overlay scrollbars are not created, even in overflow:scroll,
// situations when there's no overflow. Specifically, after style-only changes.
TEST_P(ScrollbarsTest, OverlayScrolblarNotCreatedInUnscrollableAxis) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #target {
        width: 100px;
        height: 100px;
        overflow-y: scroll;
        opacity: 0.5;
      }
    </style>
    <div id="target"></div>
  )HTML");

  Compositor().BeginFrame();

  auto* target = GetDocument().getElementById(AtomicString("target"));
  auto* scrollable_area = target->GetLayoutBox()->GetScrollableArea();

  ASSERT_FALSE(scrollable_area->VerticalScrollbar());
  ASSERT_FALSE(scrollable_area->HorizontalScrollbar());

  // Mutate the opacity so that we cause a style-only change.
  target->setAttribute(html_names::kStyleAttr, AtomicString("opacity: 0.9"));
  Compositor().BeginFrame();

  ASSERT_FALSE(scrollable_area->VerticalScrollbar());
  ASSERT_FALSE(scrollable_area->HorizontalScrollbar());
}

TEST_P(ScrollbarsTest, HidingScrollbarsOnScrollableAreaDisablesScrollbars) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));

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
  Element* scroller = document.getElementById(AtomicString("scroller"));
  auto* scroller_area = GetScrollableArea(*scroller);
  ScrollableArea* frame_scroller_area = frame_view->LayoutViewport();

  // Scrollbars are hidden at start.
  scroller_area->SetScrollbarsHiddenForTesting(true);
  frame_scroller_area->SetScrollbarsHiddenForTesting(true);
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

  frame_scroller_area->SetScrollbarsHiddenForTesting(false);
  EXPECT_TRUE(frame_scroller_area->HorizontalScrollbar()
                  ->ShouldParticipateInHitTesting());
  EXPECT_TRUE(frame_scroller_area->VerticalScrollbar()
                  ->ShouldParticipateInHitTesting());
  frame_scroller_area->SetScrollbarsHiddenForTesting(true);
  EXPECT_FALSE(frame_scroller_area->HorizontalScrollbar()
                   ->ShouldParticipateInHitTesting());
  EXPECT_FALSE(frame_scroller_area->VerticalScrollbar()
                   ->ShouldParticipateInHitTesting());

  scroller_area->SetScrollbarsHiddenForTesting(false);
  EXPECT_TRUE(
      scroller_area->HorizontalScrollbar()->ShouldParticipateInHitTesting());
  EXPECT_TRUE(
      scroller_area->VerticalScrollbar()->ShouldParticipateInHitTesting());
  scroller_area->SetScrollbarsHiddenForTesting(true);
  EXPECT_FALSE(
      scroller_area->HorizontalScrollbar()->ShouldParticipateInHitTesting());
  EXPECT_FALSE(
      scroller_area->VerticalScrollbar()->ShouldParticipateInHitTesting());
}

// Ensure mouse cursor should be pointer when hovering over the scrollbar.
TEST_P(ScrollbarsTest, MouseOverScrollbarInCustomCursorElement) {
  // Skip this test if scrollbars don't allow hit testing on the platform.
  if (!WebView().GetPage()->GetScrollbarTheme().AllowsHitTest())
    return;

  WebView().MainFrameViewWidget()->Resize(gfx::Size(250, 250));

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

  Element* div = document.getElementById(AtomicString("d1"));

  // Ensure hittest has DIV and scrollbar.
  HitTestResult hit_test_result = HitTest(195, 5);

  EXPECT_EQ(hit_test_result.InnerElement(), div);
  EXPECT_TRUE(hit_test_result.GetScrollbar());

  HandleMouseMoveEvent(195, 5);

  EXPECT_EQ(ui::mojom::blink::CursorType::kPointer, CursorType());
}

// Ensure mouse cursor should be override when hovering over the custom
// scrollbar.
TEST_P(ScrollbarsTest, MouseOverCustomScrollbarInCustomCursorElement) {
  // Skip this test if scrollbars don't allow hit testing on the platform.
  if (!WebView().GetPage()->GetScrollbarTheme().AllowsHitTest())
    return;

  WebView().MainFrameViewWidget()->Resize(gfx::Size(250, 250));

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

  Element* div = document.getElementById(AtomicString("d1"));

  // Ensure hittest has DIV and scrollbar.
  HitTestResult hit_test_result = HitTest(195, 5);

  EXPECT_EQ(hit_test_result.InnerElement(), div);
  EXPECT_TRUE(hit_test_result.GetScrollbar());

  HandleMouseMoveEvent(195, 5);

  EXPECT_EQ(ui::mojom::blink::CursorType::kMove, CursorType());
}

// Ensure mouse cursor should be custom style when hovering over the custom
// scrollbar with custom cursor style.
TEST_P(ScrollbarsTest, MouseOverCustomScrollbarWithCustomCursor) {
  // Skip this test if scrollbars don't allow hit testing on the platform.
  if (!WebView().GetPage()->GetScrollbarTheme().AllowsHitTest()) {
    return;
  }

  WebView().MainFrameViewWidget()->Resize(gfx::Size(250, 250));

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
      cursor: pointer;
    }
    </style>
    <div id='d1'>
        <div id='d2'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();

  Element* div = document.getElementById(AtomicString("d1"));

  // Ensure hittest has DIV and scrollbar.
  HitTestResult hit_test_result = HitTest(195, 5);

  EXPECT_EQ(hit_test_result.InnerElement(), div);
  EXPECT_TRUE(hit_test_result.GetScrollbar());
  HandleMouseMoveEvent(195, 5);
  EXPECT_EQ(ui::mojom::blink::CursorType::kHand, CursorType());
}

// Ensure mouse cursor should be custom style when hovering over the custom
// scrollbar-thumb with custom cursor style.
TEST_P(ScrollbarsTest, MouseOverCustomScrollbarThumbWithCustomCursor) {
  // Skip this test if scrollbars don't allow hit testing on the platform.
  if (!WebView().GetPage()->GetScrollbarTheme().AllowsHitTest()) {
    return;
  }

  WebView().MainFrameViewWidget()->Resize(gfx::Size(250, 250));

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
      cursor: pointer;
    }

    ::-webkit-scrollbar-thumb {
      background: none;
      height: 5px;
      width: 5px;
      cursor: auto;
    }
    </style>
    <div id='d1'>
        <div id='d2'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();

  Element* div = document.getElementById(AtomicString("d1"));
  // Ensure hittest has DIV and scrollbar.
  HitTestResult hit_test_result = HitTest(195, 5);

  EXPECT_EQ(hit_test_result.InnerElement(), div);
  EXPECT_TRUE(hit_test_result.GetScrollbar());
  HandleMouseMoveEvent(195, 5);
  EXPECT_EQ(hit_test_result.GetScrollbar()->HoveredPart(), kThumbPart);

  EXPECT_EQ(ui::mojom::blink::CursorType::kPointer, CursorType());
}

// Ensure mouse cursor should be custom style when hovering over the custom
// scrollbar-track-piece with custom cursor style.
TEST_P(ScrollbarsTest, MouseOverCustomScrollbarTrackPieceWithCustomCursor) {
  // Skip this test if scrollbars don't allow hit testing on the platform.
  if (!WebView().GetPage()->GetScrollbarTheme().AllowsHitTest()) {
    return;
  }

  WebView().MainFrameViewWidget()->Resize(gfx::Size(250, 250));

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
      cursor: pointer;
    }

    ::-webkit-scrollbar-thumb {
      background: none;
      height: 5px;
      width: 5px;
      cursor: auto;
    }

    ::-webkit-scrollbar-track-piece {
      background: none;
      height: 5px;
      width: 5px;
      cursor: text;
    }

    ::-webkit-scrollbar-track-piece:start {
      background: none;
      height: 5px;
      width: 5px;
      cursor: help;
    }

    </style>
    <div id='d1'>
        <div id='d2'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();

  Element* div = document.getElementById(AtomicString("d1"));

  div->scrollTo(0, 100);
  // Ensure hittest has DIV and scrollbar.
  HitTestResult hit_test_result = HitTest(195, 5);

  EXPECT_EQ(hit_test_result.InnerElement(), div);
  EXPECT_TRUE(hit_test_result.GetScrollbar());

  HandleMouseMoveEvent(195, 5);
  EXPECT_EQ(hit_test_result.GetScrollbar()->HoveredPart(), kBackTrackPart);
  EXPECT_EQ(ui::mojom::blink::CursorType::kHelp, CursorType());

  HandleMouseMoveEvent(195, 190);
  EXPECT_EQ(hit_test_result.GetScrollbar()->HoveredPart(), kForwardTrackPart);
  EXPECT_EQ(ui::mojom::blink::CursorType::kIBeam, CursorType());
}

// Ensure mouse cursor should inherit the style set by the custom
// scrollbar-track when hovering over the custom scrollbar-track-piece
// that has no style set.
TEST_P(ScrollbarsTest, MouseOverCustomScrollbarTrackPieceWithoutStyle) {
  // Skip this test if scrollbars don't allow hit testing on the platform.
  if (!WebView().GetPage()->GetScrollbarTheme().AllowsHitTest()) {
    return;
  }

  WebView().MainFrameViewWidget()->Resize(gfx::Size(250, 250));

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
      cursor: pointer;
    }

    ::-webkit-scrollbar-thumb {
      background: none;
      height: 5px;
      width: 5px;
      cursor: auto;
    }

    ::-webkit-scrollbar-track {
      background: none;
      height: 5px;
      width: 5px;
      cursor: help;
    }
    </style>
    <div id='d1'>
        <div id='d2'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();

  Element* div = document.getElementById(AtomicString("d1"));
  // Ensure hittest has DIV and scrollbar.
  HitTestResult hit_test_result = HitTest(195, 190);

  EXPECT_EQ(hit_test_result.InnerElement(), div);
  EXPECT_TRUE(hit_test_result.GetScrollbar());
  HandleMouseMoveEvent(195, 190);

  EXPECT_EQ(hit_test_result.GetScrollbar()->HoveredPart(), kForwardTrackPart);
  EXPECT_EQ(ui::mojom::blink::CursorType::kHelp, CursorType());
}

// Ensure mouse cursor should inherit the style set by the custom scrollbar
// when hovering over the custom scrollbar-track-piece that both
// scrollbar-track and scrollbar-track-piece has no style set.
TEST_P(ScrollbarsTest,
       MouseOverCustomScrollbarTrackPieceBothTrackAndTrackPieceWithoutStyle) {
  // Skip this test if scrollbars don't allow hit testing on the platform.
  if (!WebView().GetPage()->GetScrollbarTheme().AllowsHitTest()) {
    return;
  }

  WebView().MainFrameViewWidget()->Resize(gfx::Size(250, 250));

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
      cursor: pointer;
    }

    ::-webkit-scrollbar-thumb {
      background: none;
      height: 5px;
      width: 5px;
      cursor: auto;
    }
    </style>
    <div id='d1'>
        <div id='d2'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();

  Element* div = document.getElementById(AtomicString("d1"));
  // Ensure hittest has DIV and scrollbar.
  HitTestResult hit_test_result = HitTest(195, 190);

  EXPECT_EQ(hit_test_result.InnerElement(), div);
  EXPECT_TRUE(hit_test_result.GetScrollbar());
  HandleMouseMoveEvent(195, 190);

  EXPECT_EQ(hit_test_result.GetScrollbar()->HoveredPart(), kForwardTrackPart);
  EXPECT_EQ(ui::mojom::blink::CursorType::kHand, CursorType());
}

// Ensure mouse cursor should be custom style when hovering over the custom
// scrollbar-button with custom cursor style;
TEST_P(ScrollbarsTest, MouseOverCustomScrollbarButtonTrackWithCustomCursor) {
  // Skip this test if scrollbars don't allow hit testing on the platform.
  if (!WebView().GetPage()->GetScrollbarTheme().AllowsHitTest()) {
    return;
  }

  WebView().MainFrameViewWidget()->Resize(gfx::Size(250, 250));

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
      cursor: pointer;
    }

    ::-webkit-scrollbar-thumb {
      background: none;
      height: 5px;
      width: 5px;
      cursor: auto;
    }

    ::-webkit-scrollbar-button {
      background: none;
      height: 5px;
      width: 5px;
      cursor: help;
      display: block;
    }
    </style>
    <div id='d1'>
        <div id='d2'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();

  Element* div = document.getElementById(AtomicString("d1"));
  // Ensure hittest has DIV and scrollbar.
  HitTestResult hit_test_result = HitTest(195, 2);

  EXPECT_EQ(hit_test_result.InnerElement(), div);
  EXPECT_TRUE(hit_test_result.GetScrollbar());

  HandleMouseMoveEvent(195, 2);

  EXPECT_EQ(ui::mojom::blink::CursorType::kHelp, CursorType());
}

// Ensure mouse cursor should be custom style when hovering over the custom
// scrollbar-corner with custom cursor style;
TEST_P(ScrollbarsTest, MouseOverCustomScrollbarCornerTrackWithCustomCursor) {
  // Skip this test if scrollbars don't allow hit testing on the platform.
  if (!WebView().GetPage()->GetScrollbarTheme().AllowsHitTest()) {
    return;
  }

  WebView().MainFrameViewWidget()->Resize(gfx::Size(250, 250));

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
      width: 400px;
    }
    ::-webkit-scrollbar {
      background: none;
      height: 5px;
      width: 5px;
      cursor: pointer;
    }

    ::-webkit-scrollbar-thumb {
      background: none;
      height: 5px;
      width: 5px;
      cursor: auto;
    }

    ::-webkit-scrollbar-corner {
      cursor: help;
    }
    </style>
    <div id='d1'>
        <div id='d2'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();

  Element* div = document.getElementById(AtomicString("d1"));
  // Ensure hittest has DIV and scrollbar.
  HitTestResult hit_test_result = HitTest(195, 195);

  EXPECT_EQ(hit_test_result.InnerElement(), div);
  EXPECT_TRUE(hit_test_result.IsOverScrollCorner());

  HandleMouseMoveEvent(195, 195);

  EXPECT_EQ(ui::mojom::blink::CursorType::kHelp, CursorType());
}

TEST_P(ScrollbarsTest, MouseOverCustomScrollbarCornerFrame) {
  // Skip this test if scrollbars don't allow hit testing on the platform.
  if (!WebView().GetPage()->GetScrollbarTheme().AllowsHitTest()) {
    return;
  }

  WebView().MainFrameViewWidget()->Resize(gfx::Size(250, 250));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
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
    <iframe id="iframe" srcdoc="<style>
        body { width: 200vw; height: 200vh; }
        ::-webkit-scrollbar { cursor: pointer; }
        ::-webkit-scrollbar-corner { cursor: help; }
    </style>"></iframe>
  )HTML");

  // Wait for load.
  test::RunPendingTasks();
  Compositor().BeginFrame();

  Document& iframe_document =
      *To<HTMLIFrameElement>(
           GetDocument().getElementById(AtomicString("iframe")))
           ->contentDocument();

  // Ensure hittest has DIV and scrollbar.
  HitTestResult hit_test_result = HitTest(195, 195);

  EXPECT_EQ(hit_test_result.InnerElement(), iframe_document.documentElement());
  EXPECT_TRUE(hit_test_result.IsOverScrollCorner());

  HandleMouseMoveEvent(195, 195);

  EXPECT_EQ(ui::mojom::blink::CursorType::kHelp, CursorType());
}

// Makes sure that mouse hover over an overlay scrollbar doesn't activate
// elements below (except the Element that owns the scrollbar) unless the
// scrollbar is faded out.
TEST_P(ScrollbarsTest, MouseOverLinkAndOverlayScrollbar) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);
  // Skip this test if scrollbars don't allow hit testing on the platform.
  if (!WebView().GetPage()->GetScrollbarTheme().AllowsHitTest())
    return;

  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));

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
      ->SetScrollbarsHiddenForTesting(false);

  Document& document = GetDocument();
  Element* a_tag = document.getElementById(AtomicString("a"));

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

  EXPECT_EQ(ui::mojom::blink::CursorType::kHand, CursorType());

  // Mouse over enabled overlay scrollbar. Mouse cursor should be pointer and no
  // active hover element.
  HandleMouseMoveEvent(x, y);

  EXPECT_EQ(ui::mojom::blink::CursorType::kPointer, CursorType());

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
      ->SetScrollbarsHiddenForTesting(true);

  // Ensure hittest only has link
  hit_test_result = HitTest(x, y);

  EXPECT_TRUE(hit_test_result.URLElement());
  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_FALSE(hit_test_result.GetScrollbar());

  HandleMouseMoveEvent(x, y);

  EXPECT_EQ(ui::mojom::blink::CursorType::kHand, CursorType());

  HandleMousePressEvent(x, y);

  EXPECT_TRUE(document.GetActiveElement());
  EXPECT_TRUE(document.HoverElement());
}

// Makes sure that mouse hover over an custom scrollbar doesn't change the
// activate elements.
TEST_P(ScrollbarsTest, MouseOverCustomScrollbar) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));

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

  Element* scrollbar_div = document.getElementById(AtomicString("scrollbar"));
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
TEST_P(ScrollbarsTest, MouseOverScrollbarAndIFrame) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);
  // Skip this test if scrollbars don't allow hit testing on the platform.
  if (!WebView().GetPage()->GetScrollbarTheme().AllowsHitTest())
    return;

  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));

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

  frame_resource.Complete("<!DOCTYPE html>");
  Compositor().BeginFrame();

  // Enable the Scrollbar.
  WebView()
      .MainFrameImpl()
      ->GetFrameView()
      ->LayoutViewport()
      ->SetScrollbarsHiddenForTesting(false);

  Document& document = GetDocument();
  Element* iframe = document.getElementById(AtomicString("iframe"));
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
      ->SetScrollbarsHiddenForTesting(true);

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
TEST_P(ScrollbarsTest, MouseOverScrollbarAndParentElement) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));

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

  Element* parent_div = document.getElementById(AtomicString("parent"));
  Element* child_div = document.getElementById(AtomicString("child"));
  EXPECT_TRUE(parent_div);
  EXPECT_TRUE(child_div);

  auto* scrollable_area = GetScrollableArea(*parent_div);

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
TEST_P(ScrollbarsTest, MouseOverRootScrollbar) {
  // Skip this test if scrollbars don't allow hit testing on the platform.
  if (!WebView().GetPage()->GetScrollbarTheme().AllowsHitTest())
    return;

  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));

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

TEST_P(ScrollbarsTest, MouseReleaseUpdatesScrollbarHoveredPart) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));

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

  Element* scrollbar_div = document.getElementById(AtomicString("scrollbar"));
  EXPECT_TRUE(scrollbar_div);

  auto* scrollable_area = GetScrollableArea(*scrollbar_div);

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

TEST_P(ScrollbarsTest, ContextMenuUpdatesScrollbarPressedPart) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));

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

  Element* scrollbar_div = document.getElementById(AtomicString("scroller"));
  EXPECT_TRUE(scrollbar_div);

  auto* scrollable_area = GetScrollableArea(*scrollbar_div);

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

TEST_P(ScrollbarsTest,
       CustomScrollbarInOverlayScrollbarThemeWillNotCauseDCHECKFails) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));

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
TEST_P(ScrollbarsTest, CustomScrollbarChangeToMobileByEmulator) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));

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

  Element* div = document.getElementById(AtomicString("d1"));

  auto* div_scrollable = GetScrollableArea(*div);

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
  DeviceEmulationParams params;
  params.screen_type = mojom::EmulatedScreenType::kMobile;
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
TEST_P(ScrollbarsTest, CustomScrollbarWhenStyleOwnerChange) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));

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

  Element* div = document.getElementById(AtomicString("d1"));

  auto* div_scrollable = GetScrollableArea(*div);

  DCHECK(div_scrollable->VerticalScrollbar());
  DCHECK(div_scrollable->VerticalScrollbar()->IsCustomScrollbar());
  DCHECK_EQ(div_scrollable->VerticalScrollbar()->Width(), 10);
  DCHECK(!div_scrollable->VerticalScrollbar()->IsOverlayScrollbar());
  DCHECK(!div_scrollable->VerticalScrollbar()->GetTheme().IsMockTheme());

  div->setAttribute(html_names::kClassAttr, AtomicString("custom"));
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
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
TEST_P(ScrollbarsTestWithVirtualTimer,
       DISABLED_TestNonCompositedOverlayScrollbarsFade) {
#else
TEST_P(ScrollbarsTestWithVirtualTimer, TestNonCompositedOverlayScrollbarsFade) {
#endif
  // Scrollbars are always composited in RasterInducingScroll.
  if (RuntimeEnabledFeatures::RasterInducingScrollEnabled()) {
    return;
  }

  // This test relies on mock overlay scrollbars.
  ScopedMockOverlayScrollbars mock_overlay_scrollbars(true);

  TimeAdvance();
  constexpr base::TimeDelta kMockOverlayFadeOutDelay = base::Seconds(5);

  ScrollbarTheme& theme = GetScrollbarTheme();
  ASSERT_TRUE(theme.IsMockTheme());
  ASSERT_TRUE(theme.UsesOverlayScrollbars());
  ScrollbarThemeOverlayMock& mock_overlay_theme =
      static_cast<ScrollbarThemeOverlayMock&>(theme);
  mock_overlay_theme.SetOverlayScrollbarFadeOutDelay(kMockOverlayFadeOutDelay);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(640, 480));
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
  Element* container = document.getElementById(AtomicString("container"));
  auto* scrollable_area = GetScrollableArea(*container);

  DCHECK(!scrollable_area->UsesCompositedScrolling());

  EXPECT_FALSE(scrollable_area->ScrollbarsHiddenIfOverlay());
  RunTasksForPeriod(kMockOverlayFadeOutDelay);
  EXPECT_TRUE(scrollable_area->ScrollbarsHiddenIfOverlay());

  scrollable_area->SetScrollOffset(ScrollOffset(10, 10),
                                   mojom::blink::ScrollType::kProgrammatic,
                                   mojom::blink::ScrollBehavior::kInstant);

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
  scrollable_area->SetScrollOffset(ScrollOffset(20, 20),
                                   mojom::blink::ScrollType::kProgrammatic,
                                   mojom::blink::ScrollBehavior::kInstant);
  EXPECT_FALSE(scrollable_area->ScrollbarsHiddenIfOverlay());
  scrollable_area->MouseEnteredScrollbar(*scrollable_area->VerticalScrollbar());
  RunTasksForPeriod(kMockOverlayFadeOutDelay);
  EXPECT_FALSE(scrollable_area->ScrollbarsHiddenIfOverlay());
  scrollable_area->MouseExitedScrollbar(*scrollable_area->VerticalScrollbar());
  RunTasksForPeriod(kMockOverlayFadeOutDelay);
  EXPECT_TRUE(scrollable_area->ScrollbarsHiddenIfOverlay());

  mock_overlay_theme.SetOverlayScrollbarFadeOutDelay(base::TimeDelta());
}

enum { kUseOverlayScrollbars = 1 << 10 };

class ScrollbarAppearanceTest : public ScrollbarsTest {
 protected:
  bool UsesOverlayScrollbars() const {
    return GetParam() & kUseOverlayScrollbars;
  }
};

// Test both overlay and non-overlay scrollbars.
INSTANTIATE_TEST_SUITE_P(All,
                         ScrollbarAppearanceTest,
                         ::testing::Values(0, kUseOverlayScrollbars));

// Make sure native scrollbar can change by Emulator.
// Disable on Android since Android always enable OverlayScrollbar.
#if BUILDFLAG(IS_ANDROID)
TEST_P(ScrollbarAppearanceTest,
       DISABLED_NativeScrollbarChangeToMobileByEmulator) {
#else
TEST_P(ScrollbarAppearanceTest, NativeScrollbarChangeToMobileByEmulator) {
#endif
  ENABLE_OVERLAY_SCROLLBARS(UsesOverlayScrollbars());

  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));

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
    <!-- flex creates DelayScrollOffsetClampScope to increase test coverge -->
    <div style='display: flex'>
      <div id='d1'>
        <div id='d2'/>
      </div>
    </div>
  )HTML");

  Compositor().BeginFrame();

  Document& document = GetDocument();

  ScrollableArea* root_scrollable = document.View()->LayoutViewport();

  Element* div = document.getElementById(AtomicString("d1"));

  auto* div_scrollable = GetScrollableArea(*div);

  VisualViewport& viewport = WebView().GetPage()->GetVisualViewport();

  DCHECK(root_scrollable->VerticalScrollbar());
  DCHECK(!root_scrollable->VerticalScrollbar()->IsCustomScrollbar());
  DCHECK_EQ(UsesOverlayScrollbars(),
            root_scrollable->VerticalScrollbar()->IsOverlayScrollbar());
  DCHECK(!root_scrollable->VerticalScrollbar()->GetTheme().IsMockTheme());

  DCHECK(!viewport.LayerForHorizontalScrollbar());

  DCHECK(div_scrollable->VerticalScrollbar());
  DCHECK(!div_scrollable->VerticalScrollbar()->IsCustomScrollbar());
  DCHECK_EQ(UsesOverlayScrollbars(),
            div_scrollable->VerticalScrollbar()->IsOverlayScrollbar());
  DCHECK(!div_scrollable->VerticalScrollbar()->GetTheme().IsMockTheme());

  // Turn on mobile emulator.
  DeviceEmulationParams params;
  params.screen_type = mojom::EmulatedScreenType::kMobile;
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
  DCHECK_EQ(UsesOverlayScrollbars(),
            root_scrollable->VerticalScrollbar()->IsOverlayScrollbar());
  EXPECT_FALSE(root_scrollable->VerticalScrollbar()->GetTheme().IsMockTheme());

  DCHECK(!viewport.LayerForHorizontalScrollbar());

  EXPECT_TRUE(div_scrollable->VerticalScrollbar());
  EXPECT_FALSE(div_scrollable->VerticalScrollbar()->IsCustomScrollbar());
  DCHECK_EQ(UsesOverlayScrollbars(),
            div_scrollable->VerticalScrollbar()->IsOverlayScrollbar());
  EXPECT_FALSE(div_scrollable->VerticalScrollbar()->GetTheme().IsMockTheme());
}

#if !BUILDFLAG(IS_MAC)
// Ensure that the minimum length for a scrollbar thumb comes from the
// WebThemeEngine. Note, Mac scrollbars differ from all other platforms so this
// test doesn't apply there. https://crbug.com/682209.
TEST_P(ScrollbarAppearanceTest, ThemeEngineDefinesMinimumThumbLength) {
  ScopedStubThemeEngine scoped_theme;
  ENABLE_OVERLAY_SCROLLBARS(UsesOverlayScrollbars());

  v8::HandleScope handle_scope(
      WebView().GetPage()->GetAgentGroupScheduler().Isolate());
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
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
  ScopedStubThemeEngine scoped_theme;
  ENABLE_OVERLAY_SCROLLBARS(UsesOverlayScrollbars());

  v8::HandleScope handle_scope(
      WebView().GetPage()->GetAgentGroupScheduler().Isolate());
  WebView().MainFrameViewWidget()->Resize(gfx::Size(1000, 1000));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style> body { margin: 0px; height: 10000000px; } </style>)HTML");
  ScrollableArea* scrollable_area = GetDocument().View()->LayoutViewport();

  Compositor().BeginFrame();

  scrollable_area->SetScrollOffset(ScrollOffset(0, 10000000),
                                   mojom::blink::ScrollType::kProgrammatic);

  Compositor().BeginFrame();

  int scroll_y = scrollable_area->GetScrollOffset().y();
  ASSERT_EQ(9999000, scroll_y);

  Scrollbar* scrollbar = scrollable_area->VerticalScrollbar();
  ASSERT_TRUE(scrollbar);

  int max_thumb_position = WebView().MainFrameViewWidget()->Size().height() -
                           StubWebThemeEngine::kMinimumVerticalLength;
  max_thumb_position -= scrollbar->GetTheme().ScrollbarMargin(
                            scrollbar->ScaleFromDIP(), EScrollbarWidth::kAuto) *
                        2;

  EXPECT_EQ(max_thumb_position,
            scrollbar->GetTheme().ThumbPosition(*scrollbar));
}
#endif

// A body with width just under the window width should not have scrollbars.
TEST_P(ScrollbarsTest, WideBodyShouldNotHaveScrollbars) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
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
TEST_P(ScrollbarsTest, TallBodyShouldNotHaveScrollbars) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
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
TEST_P(ScrollbarsTest, TallAndWideBodyShouldNotHaveScrollbars) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
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
TEST_P(ScrollbarsTest, BodySizeEqualWindowSizeShouldNotHaveScrollbars) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
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
TEST_P(ScrollbarsTest, WidePercentageBodyShouldHaveScrollbar) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
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
TEST_P(ScrollbarsTest, WidePercentageAndTallBodyShouldHaveScrollbar) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
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
TEST_P(ScrollbarsTest, TallPercentageBodyShouldHaveScrollbar) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
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
TEST_P(ScrollbarsTest, TallPercentageAndWideBodyShouldHaveScrollbar) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
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
TEST_P(ScrollbarsTest, TallAndWidePercentageBodyShouldHaveScrollbars) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
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

TEST_P(ScrollbarsTest, MouseOverIFrameScrollbar) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));

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
  Element* iframe = document.getElementById(AtomicString("iframe"));
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

TEST_P(ScrollbarsTest, AutosizeTest) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(0, 0));
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
  WebView().EnableAutoResizeMode(gfx::Size(100, 100), gfx::Size(100, 200));

  // Note, the frame autosizer doesn't work correctly with subpixel sizes so
  // even though the container is a fraction larger than the frame, we don't
  // consider that for overflow.
  {
    Compositor().BeginFrame();
    EXPECT_FALSE(layout_viewport->VerticalScrollbar());
    EXPECT_FALSE(layout_viewport->HorizontalScrollbar());
    EXPECT_EQ(100, frame_view->FrameRect().width());
    EXPECT_EQ(150, frame_view->FrameRect().height());
  }

  // Subsequent autosizes should be stable. Specifically checking the condition
  // from https://crbug.com/811478.
  {
    frame_view->SetNeedsLayout();
    Compositor().BeginFrame();
    EXPECT_FALSE(layout_viewport->VerticalScrollbar());
    EXPECT_FALSE(layout_viewport->HorizontalScrollbar());
    EXPECT_EQ(100, frame_view->FrameRect().width());
    EXPECT_EQ(150, frame_view->FrameRect().height());
  }

  // Try again.
  {
    frame_view->SetNeedsLayout();
    Compositor().BeginFrame();
    EXPECT_FALSE(layout_viewport->VerticalScrollbar());
    EXPECT_FALSE(layout_viewport->HorizontalScrollbar());
    EXPECT_EQ(100, frame_view->FrameRect().width());
    EXPECT_EQ(150, frame_view->FrameRect().height());
  }
}

TEST_P(ScrollbarsTest, AutosizeAlmostRemovableScrollbar) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);
  WebView().EnableAutoResizeMode(gfx::Size(25, 25), gfx::Size(800, 600));

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

TEST_P(ScrollbarsTest, AutosizeExpandingContentScrollable) {
  ENABLE_OVERLAY_SCROLLBARS(true);

  SimRequest resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  resource.Complete(R"HTML(
    <style>
    body { margin: 0 }
    #spacer { width: 100px; height: 100px; }
    </style>
    <div id="spacer"></div>
  )HTML");
  test::RunPendingTasks();

  LocalFrameView* frame_view = WebView().MainFrameImpl()->GetFrameView();
  ScrollableArea* layout_viewport = frame_view->LayoutViewport();

  WebView().EnableAutoResizeMode(gfx::Size(800, 600), gfx::Size(800, 600));
  Compositor().BeginFrame();

  // Not scrollable due to no overflow.
  EXPECT_FALSE(layout_viewport->UserInputScrollable(kVerticalScrollbar));

  GetDocument()
      .getElementById(AtomicString("spacer"))
      ->setAttribute(html_names::kStyleAttr, AtomicString("height: 900px"));
  Compositor().BeginFrame();

  // Now scrollable due to overflow.
  EXPECT_TRUE(layout_viewport->UserInputScrollable(kVerticalScrollbar));
}

TEST_P(ScrollbarsTest,
       HideTheOverlayScrollbarNotCrashAfterPLSADisposedPaintLayer) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
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
  Element* div = document.getElementById(AtomicString("div"));
  auto* scrollable_div = GetScrollableArea(*div);

  scrollable_div->SetScrollbarsHiddenForTesting(false);
  ASSERT_TRUE(scrollable_div);
  ASSERT_TRUE(scrollable_div->GetPageScrollbarTheme().UsesOverlayScrollbars());
  ASSERT_TRUE(scrollable_div->VerticalScrollbar());

  EXPECT_FALSE(scrollable_div->ScrollbarsHiddenIfOverlay());

  // Set display:none calls Dispose().
  div->setAttribute(html_names::kClassAttr, AtomicString("hide"));
  Compositor().BeginFrame();

  // After paint layer in scrollable dispose, we can still call scrollbar hidden
  // just not change scrollbar.
  scrollable_div->SetScrollbarsHiddenForTesting(true);

  EXPECT_FALSE(scrollable_div->ScrollbarsHiddenIfOverlay());
}

TEST_P(ScrollbarsTest, PLSADisposeShouldClearPointerInLayers) {
  SetPreferCompositingToLCDText(true);
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    /* transform keeps the composited layer */
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
  Element* div = document.getElementById(AtomicString("div"));
  auto* scrollable_div = GetScrollableArea(*div);

  ASSERT_TRUE(scrollable_div);

  PaintLayer* paint_layer = scrollable_div->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_EQ(scrollable_div, paint_layer->GetScrollableArea());

  div->setAttribute(html_names::kClassAttr, AtomicString("hide"));
  document.UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  EXPECT_FALSE(paint_layer->GetScrollableArea());
}

TEST_P(ScrollbarsTest, OverlayScrollbarHitTest) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);
  // Skip this test if scrollbars don't allow hit testing on the platform.
  if (!WebView().GetPage()->GetScrollbarTheme().AllowsHitTest())
    return;

  WebView().MainFrameViewWidget()->Resize(gfx::Size(300, 300));

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

  frame_resource.Complete("<!DOCTYPE html><body style='height: 999px'></body>");
  Compositor().BeginFrame();

  // Enable the main frame scrollbar.
  WebView()
      .MainFrameImpl()
      ->GetFrameView()
      ->LayoutViewport()
      ->SetScrollbarsHiddenForTesting(false);

  // Enable the iframe scrollbar.
  auto* iframe_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("iframe")));
  iframe_element->contentDocument()
      ->View()
      ->LayoutViewport()
      ->SetScrollbarsHiddenForTesting(false);

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

TEST_P(ScrollbarsTest, RecorderedOverlayScrollbarHitTest) {
  ENABLE_OVERLAY_SCROLLBARS(true);
  // Skip this test if scrollbars don't allow hit testing on the platform.
  if (!WebView().GetPage()->GetScrollbarTheme().AllowsHitTest())
    return;

  SimRequest resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>body { margin: 0; }</style>
    <div id="target" style="width: 200px; height: 200px; overflow: scroll">
      <div id="stacked" style="position: relative; height: 400px">
      </div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  auto* target =
      GetDocument().getElementById(AtomicString("target"))->GetLayoutBox();
  target->GetScrollableArea()->SetScrollbarsHiddenForTesting(false);
  ASSERT_TRUE(target->Layer()->NeedsReorderOverlayOverflowControls());

  // Hit test on and off the main frame scrollbar.
  HitTestResult result = HitTest(195, 5);
  EXPECT_TRUE(result.GetScrollbar());
  EXPECT_EQ(target->GetNode(), result.InnerNode());
  result = HitTest(150, 5);
  EXPECT_FALSE(result.GetScrollbar());
  EXPECT_EQ(GetDocument().getElementById(AtomicString("stacked")),
            result.InnerNode());
}

TEST_P(ScrollbarsTest,
       AllowMiddleButtonPressOnScrollbarWhenDisableMiddleClickAutoScroll) {
  ScopedMiddleClickAutoscrollForTest middle_click_autoscroll(false);
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
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

  // allow press scrollbar with middle button.
  HandleMouseMoveEvent(195, 5);
  HandleMouseMiddlePressEvent(195, 5);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kThumbPart);
  HandleMouseMiddleReleaseEvent(195, 5);
}

TEST_P(ScrollbarsTest,
       NotAllowMiddleButtonPressOnScrollbarWhenEnableMiddleClickAutoScroll) {
  ScopedMiddleClickAutoscrollForTest middle_click_autoscroll(true);
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
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
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kNoPart);
  HandleMouseMiddleReleaseEvent(195, 5);
}

TEST_P(ScrollbarsTest, NotAllowNonLeftButtonPressOnScrollbar) {
  ScopedMiddleClickAutoscrollForTest middle_click_autoscroll(true);
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
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

  // Not allow press scrollbar with non-left button.
  HandleMouseMoveEvent(195, 5);
  HandleMousePressEvent(195, 5, WebPointerProperties::Button::kForward);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kNoPart);
  HandleMouseReleaseEvent(195, 5, WebPointerProperties::Button::kForward);
}

// Ensure Scrollbar not release press by middle button down.
TEST_P(ScrollbarsTest, MiddleDownShouldNotAffectScrollbarPress) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
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
  WebMouseEvent event(WebInputEvent::Type::kMouseMove, gfx::PointF(5, 5),
                      gfx::PointF(5, 5), WebPointerProperties::Button::kLeft, 0,
                      WebInputEvent::Modifiers::kLeftButtonDown,
                      base::TimeTicks::Now());
  event.SetFrameScale(1);
  GetEventHandler().HandleMouseMoveEvent(event, Vector<WebMouseEvent>(),
                                         Vector<WebMouseEvent>());
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kThumbPart);

  // Middle click should not release scrollbar press state.
  HandleMouseMiddlePressEvent(5, 5);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kThumbPart);

  // Middle button release should release scrollbar press state.
  HandleMouseMiddleReleaseEvent(5, 5);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kNoPart);
}

TEST_P(ScrollbarsTest, UseCounterNegativeWhenThumbIsNotScrolledWithMouse) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
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
  auto& widget = GetWebFrameWidget();
  widget.DispatchThroughCcInputHandler(
      GenerateWheelGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                                gfx::Point(100, 100), ScrollOffset(0, -100)));
  widget.DispatchThroughCcInputHandler(
      GenerateWheelGestureEvent(WebInputEvent::Type::kGestureScrollUpdate,
                                gfx::Point(100, 100), ScrollOffset(0, -100)));
  widget.DispatchThroughCcInputHandler(GenerateWheelGestureEvent(
      WebInputEvent::Type::kGestureScrollEnd, gfx::Point(100, 100)));
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
  // Let injected scroll gesture run.
  widget.FlushInputHandlerTasks();
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kVerticalScrollbarThumbScrollingWithMouse));

  // Clicking on the horizontal scrollbar won't trigger the UseCounter.
  HandleMousePressEvent(175, 195);
  EXPECT_EQ(horizontal_scrollbar->PressedPart(),
            ScrollbarPart::kForwardTrackPart);
  HandleMouseReleaseEvent(175, 195);
  // Let injected scroll gesture run.
  widget.FlushInputHandlerTasks();
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

TEST_P(ScrollbarsTest, UseCounterPositiveWhenThumbIsScrolledWithMouse) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
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

TEST_P(ScrollbarsTest, UseCounterNegativeWhenThumbIsNotScrolledWithTouch) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
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
  WebView().MainFrameViewWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::Type::kGestureTapDown, gfx::Point(195, 175)));
  EXPECT_EQ(vertical_scrollbar->PressedPart(),
            ScrollbarPart::kForwardTrackPart);
  WebView().MainFrameViewWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::Type::kGestureTapCancel, gfx::Point(195, 175)));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kVerticalScrollbarThumbScrollingWithTouch));

  // Tapping on the horizontal scrollbar won't trigger the UseCounter.
  WebView().MainFrameViewWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::Type::kGestureTapDown, gfx::Point(175, 195)));
  EXPECT_EQ(horizontal_scrollbar->PressedPart(),
            ScrollbarPart::kForwardTrackPart);
  WebView().MainFrameViewWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::Type::kGestureTapCancel, gfx::Point(175, 195)));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kHorizontalScrollbarThumbScrollingWithTouch));

  // Tapping outside the scrollbar and then releasing over the thumb of the
  // vertical scrollbar won't trigger the UseCounter.
  WebView().MainFrameViewWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::Type::kGestureTapDown, gfx::Point(50, 50)));
  WebView().MainFrameViewWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::Type::kGestureTapCancel, gfx::Point(195, 5)));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kVerticalScrollbarThumbScrollingWithTouch));

  // Tapping outside the scrollbar and then releasing over the thumb of the
  // horizontal scrollbar won't trigger the UseCounter.
  WebView().MainFrameViewWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::Type::kGestureTapDown, gfx::Point(50, 50)));
  WebView().MainFrameViewWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::Type::kGestureTapCancel, gfx::Point(5, 195)));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kHorizontalScrollbarThumbScrollingWithTouch));
}

TEST_P(ScrollbarsTest, UseCounterPositiveWhenThumbIsScrolledWithTouch) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
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
  WebView().MainFrameViewWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::Type::kGestureTapDown, gfx::Point(195, 5)));
  EXPECT_EQ(vertical_scrollbar->PressedPart(), ScrollbarPart::kThumbPart);
  WebView().MainFrameViewWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::Type::kGestureTapCancel, gfx::Point(195, 5)));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kVerticalScrollbarThumbScrollingWithTouch));

  // Clicking the thumb on the horizontal scrollbar will trigger the UseCounter.
  WebView().MainFrameViewWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::Type::kGestureTapDown, gfx::Point(5, 195)));
  EXPECT_EQ(horizontal_scrollbar->PressedPart(), ScrollbarPart::kThumbPart);
  WebView().MainFrameViewWidget()->HandleInputEvent(GenerateTouchGestureEvent(
      WebInputEvent::Type::kGestureTapCancel, gfx::Point(5, 195)));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kHorizontalScrollbarThumbScrollingWithTouch));
}

TEST_P(ScrollbarsTest, UseCounterCustomScrollbarPercentSize) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      ::-webkit-scrollbar { width: 10px; height: 10%; }
      ::-webkit-scrollbar-thumb { min-width: 10%; min-height: 10px; }
    </style>
    <div id="target" style="width: 100px; height: 100px; overflow: auto">
      <div id="child" style="width: 50px; height: 50px"></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  // No scrollbars initially.
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kCustomScrollbarPercentThickness));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCustomScrollbarPartPercentLength));

  // Show vertical scrollbar which uses fixed lengths for thickness
  // (width: 10px) and thumb minimum length (min-height: 10px).
  auto* child = GetDocument().getElementById(AtomicString("child"));
  child->setAttribute(html_names::kStyleAttr,
                      AtomicString("width: 50px; height: 200px"));
  Compositor().BeginFrame();
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kCustomScrollbarPercentThickness));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCustomScrollbarPartPercentLength));

  // Show horizontal scrollbar which uses percent lengths for thickness
  // (height: 10%) and thumb minimum length (min-width: 10%).
  child->setAttribute(html_names::kStyleAttr,
                      AtomicString("width: 200px; height: 50px"));
  Compositor().BeginFrame();
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kCustomScrollbarPercentThickness));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kCustomScrollbarPartPercentLength));
}

TEST_P(ScrollbarsTest, CheckScrollCornerIfThereIsNoScrollbar) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
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

  auto* element = GetDocument().getElementById(AtomicString("container"));
  auto* scrollable_container = GetScrollableArea(*element);

  // There should initially be a scrollbar and a scroll corner.
  EXPECT_TRUE(scrollable_container->HasScrollbar());
  EXPECT_TRUE(scrollable_container->ScrollCorner());

  // Make the container non-scrollable so the scrollbar and corner disappear.
  element->setAttribute(html_names::kStyleAttr, AtomicString("width: 100px;"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  EXPECT_FALSE(scrollable_container->HasScrollbar());
  EXPECT_FALSE(scrollable_container->ScrollCorner());
}

TEST_P(ScrollbarsTest, NoNeedsBeginFrameForCustomScrollbarAfterBeginFrame) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      ::-webkit-scrollbar { height: 20px; }
      ::-webkit-scrollbar-thumb { background-color: blue; }
      #target { width: 200px; height: 200px; overflow: scroll; }
    </style>
    <div id="target">
      <div style="width: 500px; height: 500px"></div>
    </div>
  )HTML");

  while (Compositor().NeedsBeginFrame())
    Compositor().BeginFrame();

  auto* target = GetDocument().getElementById(AtomicString("target"));
  auto* scrollbar = To<CustomScrollbar>(
      target->GetLayoutBox()->GetScrollableArea()->HorizontalScrollbar());
  LayoutCustomScrollbarPart* thumb = scrollbar->GetPart(kThumbPart);
  auto thumb_size = thumb->Size();
  EXPECT_FALSE(thumb->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(Compositor().NeedsBeginFrame());

  WebView().MainFrameViewWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(thumb->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(Compositor().NeedsBeginFrame());

  target->setAttribute(html_names::kStyleAttr, AtomicString("width: 400px"));
  EXPECT_TRUE(Compositor().NeedsBeginFrame());
  Compositor().BeginFrame();
  EXPECT_FALSE(thumb->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_NE(thumb_size, thumb->Size());
}

TEST_P(ScrollbarsTest, CustomScrollbarHypotheticalThickness) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #target1::-webkit-scrollbar { width: 22px; height: 33px; }
      #target2::-webkit-scrollbar:horizontal { height: 13px; }
      ::-webkit-scrollbar:vertical { width: 21px; }
    </style>
    <div id="target1" style="width: 60px; height: 70px; overflow: scroll"></div>
    <div id="target2" style="width: 80px; height: 90px; overflow: scroll"></div>
  )HTML");

  Compositor().BeginFrame();

  auto* target1 = GetDocument().getElementById(AtomicString("target1"));
  auto* scrollable_area1 = target1->GetLayoutBox()->GetScrollableArea();
  EXPECT_EQ(
      33, CustomScrollbar::HypotheticalScrollbarThickness(
              scrollable_area1, kHorizontalScrollbar, target1->GetLayoutBox()));
  EXPECT_EQ(22,
            CustomScrollbar::HypotheticalScrollbarThickness(
                scrollable_area1, kVerticalScrollbar, target1->GetLayoutBox()));

  auto* target2 = GetDocument().getElementById(AtomicString("target2"));
  auto* scrollable_area2 = target2->GetLayoutBox()->GetScrollableArea();
  EXPECT_EQ(
      13, CustomScrollbar::HypotheticalScrollbarThickness(
              scrollable_area2, kHorizontalScrollbar, target2->GetLayoutBox()));
  EXPECT_EQ(21,
            CustomScrollbar::HypotheticalScrollbarThickness(
                scrollable_area2, kVerticalScrollbar, target2->GetLayoutBox()));
}

// For infinite scrolling page (load more content when scroll to bottom), user
// press on scrollbar button should keep scrolling after content loaded.
// Disable on Android since VirtualTime not work for Android.
// http://crbug.com/633321
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
TEST_P(ScrollbarsTestWithVirtualTimer,
       DISABLED_PressScrollbarButtonOnInfiniteScrolling) {
#else
TEST_P(ScrollbarsTestWithVirtualTimer,
       PressScrollbarButtonOnInfiniteScrolling) {
#endif
  TimeAdvance();
  GetDocument().GetFrame()->GetSettings()->SetScrollAnimatorEnabled(false);
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  RunTasksForPeriod(base::Milliseconds(1000));
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
  scrollable_area->SetScrollOffset(ScrollOffset(0, 400),
                                   mojom::blink::ScrollType::kProgrammatic,
                                   mojom::blink::ScrollBehavior::kInstant);
  EXPECT_EQ(scrollable_area->ScrollOffsetInt(), gfx::Vector2d(0, 200));

  HandleMouseMoveEvent(195, 195);
  HandleMousePressEvent(195, 195);
  ASSERT_EQ(scrollbar->PressedPart(), ScrollbarPart::kForwardButtonEndPart);

  // Wait for 2 delay.
  RunTasksForPeriod(base::Milliseconds(1000));
  RunTasksForPeriod(base::Milliseconds(1000));
  // Change #big size.
  MainFrame().ExecuteScript(WebScriptSource(
      "document.getElementById('big').style.height = '1000px';"));
  Compositor().BeginFrame();

  RunTasksForPeriod(base::Milliseconds(1000));
  RunTasksForPeriod(base::Milliseconds(1000));

  // Verify that the scrollbar autopress timer requested some scrolls via
  // gestures. The button was pressed for 2 seconds and the timer fires
  // every 250ms - we should have at least 7 injected gesture updates.
  EXPECT_GT(GetWebFrameWidget().GetInjectedScrollEvents().size(), 6u);

  // Let injected scroll gestures run.
  GetWebFrameWidget().FlushInputHandlerTasks();
}

class ScrollbarTrackMarginsTest : public ScrollbarsTest {
 public:
  void PrepareTest(const String& track_style) {
    WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));

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

    Element* div = GetDocument().getElementById(AtomicString("d1"));
    ASSERT_TRUE(div);

    auto* div_scrollable = GetScrollableArea(*div);

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

  Persistent<LayoutCustomScrollbarPart> horizontal_track_;
  Persistent<LayoutCustomScrollbarPart> vertical_track_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(ScrollbarTrackMarginsTest);

TEST_P(ScrollbarTrackMarginsTest,
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

TEST_P(ScrollbarTrackMarginsTest,
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

TEST_P(ScrollbarColorSchemeTest, ThemeEnginePaint) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();

  ScopedStubThemeEngine scoped_theme;

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
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

  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);

  Compositor().BeginFrame();

  auto* theme_engine = static_cast<StubWebThemeEngine*>(
      WebThemeEngineHelper::GetNativeThemeEngine());
  EXPECT_EQ(mojom::blink::ColorScheme::kDark,
            theme_engine->GetPaintedPartColorScheme(
                WebThemeEngine::kPartScrollbarHorizontalThumb));
  EXPECT_EQ(mojom::blink::ColorScheme::kDark,
            theme_engine->GetPaintedPartColorScheme(
                WebThemeEngine::kPartScrollbarVerticalThumb));
  EXPECT_EQ(mojom::blink::ColorScheme::kDark,
            theme_engine->GetPaintedPartColorScheme(
                WebThemeEngine::kPartScrollbarCorner));
}

// Test scrollbar-gutter values with classic scrollbars and horizontal-tb text.
TEST_P(ScrollbarsTest, ScrollbarGutterWithHorizontalTextAndClassicScrollbars) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      div {
        width: 100px;
        height: 100px;
        overflow: auto;
        writing-mode: horizontal-tb;
      }
      #auto {
        scrollbar-gutter: auto;
      }
      #stable {
        scrollbar-gutter: stable;
      }
      #stable_both_edges {
        scrollbar-gutter: stable both-edges;
      }
    </style>
    <div id="auto"></div>
    <div id="stable"></div>
    <div id="stable_both_edges"></div>
  )HTML");
  Compositor().BeginFrame();
  auto* auto_ = GetDocument().getElementById(AtomicString("auto"));
  auto* box_auto = auto_->GetLayoutBox();
  EXPECT_EQ(box_auto->OffsetWidth(), 100);
  EXPECT_EQ(box_auto->ClientWidth(), 100);
  PhysicalBoxStrut box_auto_scrollbars = box_auto->ComputeScrollbars();
  EXPECT_EQ(box_auto_scrollbars.top, 0);
  EXPECT_EQ(box_auto_scrollbars.bottom, 0);
  EXPECT_EQ(box_auto_scrollbars.left, 0);
  EXPECT_EQ(box_auto_scrollbars.right, 0);

  auto* stable = GetDocument().getElementById(AtomicString("stable"));
  auto* box_stable = stable->GetLayoutBox();
  EXPECT_EQ(box_stable->OffsetWidth(), 100);
  EXPECT_EQ(box_stable->ClientWidth(), 85);
  PhysicalBoxStrut box_stable_scrollbars = box_stable->ComputeScrollbars();
  EXPECT_EQ(box_stable_scrollbars.top, 0);
  EXPECT_EQ(box_stable_scrollbars.bottom, 0);
  EXPECT_EQ(box_stable_scrollbars.left, 0);
  EXPECT_EQ(box_stable_scrollbars.right, 15);

  auto* stable_both_edges =
      GetDocument().getElementById(AtomicString("stable_both_edges"));
  auto* box_stable_both_edges = stable_both_edges->GetLayoutBox();
  EXPECT_EQ(box_stable_both_edges->OffsetWidth(), 100);
  EXPECT_EQ(box_stable_both_edges->ClientWidth(), 70);
  PhysicalBoxStrut box_stable_both_edges_scrollbars =
      box_stable_both_edges->ComputeScrollbars();
  EXPECT_EQ(box_stable_both_edges_scrollbars.top, 0);
  EXPECT_EQ(box_stable_both_edges_scrollbars.bottom, 0);
  EXPECT_EQ(box_stable_both_edges_scrollbars.left, 15);
  EXPECT_EQ(box_stable_both_edges_scrollbars.right, 15);
}

// Test scrollbar-gutter values with classic scrollbars and vertical-rl text.
TEST_P(ScrollbarsTest, ScrollbarGutterWithVerticalTextAndClassicScrollbars) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      div {
        width: 100px;
        height: 100px;
        overflow: auto;
        writing-mode: vertical-rl;
      }
      #auto {
        scrollbar-gutter: auto;
      }
      #stable {
        scrollbar-gutter: stable;
      }
      #stable_both_edges {
        scrollbar-gutter: stable both-edges;
      }
    </style>
    <div id="auto"></div>
    <div id="stable"></div>
    <div id="stable_both_edges"></div>
  )HTML");
  Compositor().BeginFrame();
  auto* auto_ = GetDocument().getElementById(AtomicString("auto"));
  auto* box_auto = auto_->GetLayoutBox();
  EXPECT_EQ(box_auto->OffsetHeight(), 100);
  EXPECT_EQ(box_auto->ClientHeight(), 100);
  PhysicalBoxStrut box_auto_scrollbars = box_auto->ComputeScrollbars();
  EXPECT_EQ(box_auto_scrollbars.top, 0);
  EXPECT_EQ(box_auto_scrollbars.bottom, 0);
  EXPECT_EQ(box_auto_scrollbars.left, 0);
  EXPECT_EQ(box_auto_scrollbars.right, 0);

  auto* stable = GetDocument().getElementById(AtomicString("stable"));
  auto* box_stable = stable->GetLayoutBox();
  EXPECT_EQ(box_stable->OffsetHeight(), 100);
  EXPECT_EQ(box_stable->ClientHeight(), 85);
  PhysicalBoxStrut box_stable_scrollbars = box_stable->ComputeScrollbars();
  EXPECT_EQ(box_stable_scrollbars.top, 0);
  EXPECT_EQ(box_stable_scrollbars.bottom, 15);
  EXPECT_EQ(box_stable_scrollbars.left, 0);
  EXPECT_EQ(box_stable_scrollbars.right, 0);

  auto* stable_both_edges =
      GetDocument().getElementById(AtomicString("stable_both_edges"));
  auto* box_stable_both_edges = stable_both_edges->GetLayoutBox();
  EXPECT_EQ(box_stable_both_edges->OffsetHeight(), 100);
  EXPECT_EQ(box_stable_both_edges->ClientHeight(), 70);
  PhysicalBoxStrut box_stable_both_edges_scrollbars =
      box_stable_both_edges->ComputeScrollbars();
  EXPECT_EQ(box_stable_both_edges_scrollbars.top, 15);
  EXPECT_EQ(box_stable_both_edges_scrollbars.bottom, 15);
  EXPECT_EQ(box_stable_both_edges_scrollbars.left, 0);
  EXPECT_EQ(box_stable_both_edges_scrollbars.right, 0);
}

// Test scrollbar-gutter values with overlay scrollbars and horizontal-tb text.
TEST_P(ScrollbarsTest, ScrollbarGutterWithHorizontalTextAndOverlayScrollbars) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      div {
        width: 100px;
        height: 100px;
        overflow: auto;
        writing-mode: horizontal-tb;
      }
      #auto {
        scrollbar-gutter: auto;
      }
      #stable {
        scrollbar-gutter: stable;
      }
      #stable_both_edges {
        scrollbar-gutter: stable both-edges;
      }
    </style>
    <div id="auto"></div>
    <div id="stable"></div>
    <div id="stable_both_edges"></div>
  )HTML");
  Compositor().BeginFrame();
  auto* auto_ = GetDocument().getElementById(AtomicString("auto"));
  auto* box_auto = auto_->GetLayoutBox();
  EXPECT_EQ(box_auto->OffsetWidth(), 100);
  EXPECT_EQ(box_auto->ClientWidth(), 100);
  PhysicalBoxStrut box_auto_scrollbars = box_auto->ComputeScrollbars();
  EXPECT_EQ(box_auto_scrollbars.top, 0);
  EXPECT_EQ(box_auto_scrollbars.bottom, 0);
  EXPECT_EQ(box_auto_scrollbars.left, 0);
  EXPECT_EQ(box_auto_scrollbars.right, 0);

  auto* stable = GetDocument().getElementById(AtomicString("stable"));
  auto* box_stable = stable->GetLayoutBox();
  EXPECT_EQ(box_stable->OffsetWidth(), 100);
  EXPECT_EQ(box_stable->ClientWidth(), 100);
  PhysicalBoxStrut box_stable_scrollbars = box_stable->ComputeScrollbars();
  EXPECT_EQ(box_stable_scrollbars.top, 0);
  EXPECT_EQ(box_stable_scrollbars.bottom, 0);
  EXPECT_EQ(box_stable_scrollbars.left, 0);
  EXPECT_EQ(box_stable_scrollbars.right, 0);

  auto* stable_both_edges =
      GetDocument().getElementById(AtomicString("stable_both_edges"));
  auto* box_stable_both_edges = stable_both_edges->GetLayoutBox();
  EXPECT_EQ(box_stable_both_edges->OffsetWidth(), 100);
  EXPECT_EQ(box_stable_both_edges->ClientWidth(), 100);
  PhysicalBoxStrut box_stable_both_edges_scrollbars =
      box_stable_both_edges->ComputeScrollbars();
  EXPECT_EQ(box_stable_both_edges_scrollbars.top, 0);
  EXPECT_EQ(box_stable_both_edges_scrollbars.bottom, 0);
  EXPECT_EQ(box_stable_both_edges_scrollbars.left, 0);
  EXPECT_EQ(box_stable_both_edges_scrollbars.right, 0);
}

// Test scrollbar-gutter values with overlay scrollbars and vertical-rl text.
TEST_P(ScrollbarsTest, ScrollbarGutterWithVerticalTextAndOverlayScrollbars) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  ENABLE_OVERLAY_SCROLLBARS(true);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      div {
        width: 100px;
        height: 100px;
        overflow: auto;
        writing-mode: vertical-rl;
      }
      #auto {
        scrollbar-gutter: auto;
      }
      #stable {
        scrollbar-gutter: stable;
      }
      #stable_both_edges {
        scrollbar-gutter: stable both-edges;
      }
    </style>
    <div id="auto"></div>
    <div id="stable"></div>
    <div id="stable_both_edges"></div>
  )HTML");
  Compositor().BeginFrame();
  auto* auto_ = GetDocument().getElementById(AtomicString("auto"));
  auto* box_auto = auto_->GetLayoutBox();
  EXPECT_EQ(box_auto->OffsetHeight(), 100);
  EXPECT_EQ(box_auto->ClientHeight(), 100);
  PhysicalBoxStrut box_auto_scrollbars = box_auto->ComputeScrollbars();
  EXPECT_EQ(box_auto_scrollbars.top, 0);
  EXPECT_EQ(box_auto_scrollbars.bottom, 0);
  EXPECT_EQ(box_auto_scrollbars.left, 0);
  EXPECT_EQ(box_auto_scrollbars.right, 0);

  auto* stable = GetDocument().getElementById(AtomicString("stable"));
  auto* box_stable = stable->GetLayoutBox();
  EXPECT_EQ(box_stable->OffsetHeight(), 100);
  EXPECT_EQ(box_stable->ClientHeight(), 100);
  PhysicalBoxStrut box_stable_scrollbars = box_stable->ComputeScrollbars();
  EXPECT_EQ(box_stable_scrollbars.top, 0);
  EXPECT_EQ(box_stable_scrollbars.bottom, 0);
  EXPECT_EQ(box_stable_scrollbars.left, 0);
  EXPECT_EQ(box_stable_scrollbars.right, 0);

  auto* stable_both_edges =
      GetDocument().getElementById(AtomicString("stable_both_edges"));
  auto* box_stable_both_edges = stable_both_edges->GetLayoutBox();
  EXPECT_EQ(box_stable_both_edges->OffsetHeight(), 100);
  EXPECT_EQ(box_stable_both_edges->ClientHeight(), 100);
  PhysicalBoxStrut box_stable_both_edges_scrollbars =
      box_stable_both_edges->ComputeScrollbars();
  EXPECT_EQ(box_stable_both_edges_scrollbars.top, 0);
  EXPECT_EQ(box_stable_both_edges_scrollbars.bottom, 0);
  EXPECT_EQ(box_stable_both_edges_scrollbars.left, 0);
  EXPECT_EQ(box_stable_both_edges_scrollbars.right, 0);
}

// Test events on the additional gutter created by the "both-edges" keyword of
// scrollbar-gutter.
TEST_P(ScrollbarsTest, ScrollbarGutterBothEdgesKeywordWithClassicScrollbars) {
  // This test requires that scrollbars take up space.
  ENABLE_OVERLAY_SCROLLBARS(false);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        margin: 0;
      }
      #container {
        scrollbar-gutter: stable both-edges;
        width: 200px;
        height: 200px;
        overflow: auto;
        writing-mode: horizontal-tb;
        direction: ltr;
      }
      #content {
        width: 100%;
        height: 300px;
      }
    </style>
    <div id="container">
      <div id="content">
    </div>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();
  Element* container = document.getElementById(AtomicString("container"));

  auto* scrollable_container = GetScrollableArea(*container);
  scrollable_container->SetScrollbarsHiddenForTesting(false);

  if (WebView().GetPage()->GetScrollbarTheme().AllowsHitTest()) {
    // Scrollbar on the right side.
    HitTestResult hit_test_result = HitTest(195, 5);
    EXPECT_EQ(hit_test_result.InnerElement(), container);
    EXPECT_TRUE(hit_test_result.GetScrollbar());
    EXPECT_TRUE(hit_test_result.GetScrollbar()->Enabled());

    // Empty gutter on the left side, where the events will take place.
    hit_test_result = HitTest(5, 5);
    EXPECT_EQ(hit_test_result.InnerElement(), container);
    EXPECT_FALSE(hit_test_result.GetScrollbar());
  }

  EXPECT_EQ(container->scrollTop(), 0);

  // Scroll down.
  auto& widget = GetWebFrameWidget();
  widget.DispatchThroughCcInputHandler(
      GenerateWheelGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                                gfx::Point(5, 5), ScrollOffset(0, -100)));
  widget.DispatchThroughCcInputHandler(
      GenerateWheelGestureEvent(WebInputEvent::Type::kGestureScrollUpdate,
                                gfx::Point(5, 5), ScrollOffset(0, -100)));
  widget.DispatchThroughCcInputHandler(GenerateWheelGestureEvent(
      WebInputEvent::Type::kGestureScrollEnd, gfx::Point(5, 5)));

  Compositor().BeginFrame();
  EXPECT_EQ(container->scrollTop(), 100);

  // Scroll up.
  widget.DispatchThroughCcInputHandler(
      GenerateWheelGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                                gfx::Point(5, 5), ScrollOffset(0, 100)));
  widget.DispatchThroughCcInputHandler(
      GenerateWheelGestureEvent(WebInputEvent::Type::kGestureScrollUpdate,
                                gfx::Point(5, 5), ScrollOffset(0, 100)));
  widget.DispatchThroughCcInputHandler(GenerateWheelGestureEvent(
      WebInputEvent::Type::kGestureScrollEnd, gfx::Point(195, 5)));

  Compositor().BeginFrame();
  EXPECT_EQ(container->scrollTop(), 0);
}

TEST_P(ScrollbarsTest, ScrollbarsRestoredAfterCapturePaintPreview) {
  ENABLE_OVERLAY_SCROLLBARS(false);

  ResizeView(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        margin: 0;
      }
      #content {
        width: 1200px;
        height: 1200px;
      }
    </style>
    <div id="content">A</div>
  )HTML");
  Compositor().BeginFrame();

  Document& document = GetDocument();
  LocalFrameView* frame_view = document.View();
  PaintLayerScrollableArea* layout_viewport = frame_view->LayoutViewport();
  HTMLElement* content_div =
      To<HTMLElement>(document.getElementById(AtomicString("content")));

  ASSERT_TRUE(layout_viewport->VerticalScrollbar() &&
              layout_viewport->HorizontalScrollbar());

  // Make layout dirty.
  content_div->setInnerText("B");

  cc::RecordPaintCanvas canvas;
  MainFrame().CapturePaintPreview(gfx::Rect(1000, 1000), &canvas, false, false);

  // Scrollbars are removed during the capture (see LocalFrame::ClipsContent).
  ASSERT_FALSE(layout_viewport->VerticalScrollbar() ||
               layout_viewport->HorizontalScrollbar());
  ASSERT_TRUE(frame_view->NeedsLayout());

  // Update lifecycle to restore the scrollbars.
  Compositor().BeginFrame();
  ASSERT_TRUE(layout_viewport->VerticalScrollbar() &&
              layout_viewport->HorizontalScrollbar());
}

// Tests that when overlay scrollbars are on, Scrollbar::UsedColorScheme follows
// the overlay theme, and when overlay scrollbars are disabled, the function
// returns the scrollable area's color scheme.
TEST_P(ScrollbarsTest, ScrollbarsUsedColorSchemeFollowsOverlayTheme) {
  ENABLE_OVERLAY_SCROLLBARS(true);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body { height: 3000px; background-color: white; }
      :root{ color-scheme: dark;}
    </style>)HTML");

  Compositor().BeginFrame();
  auto* layout_viewport = GetDocument().View()->LayoutViewport();
  EXPECT_TRUE(layout_viewport->VerticalScrollbar()->IsOverlayScrollbar());
  // With a white background, the overlay scrollbar theme should compute to
  // light despite the dark preferred color scheme.
  EXPECT_EQ(mojom::blink::ColorScheme::kLight,
            layout_viewport->GetOverlayScrollbarColorScheme());
  EXPECT_EQ(mojom::blink::ColorScheme::kLight,
            layout_viewport->VerticalScrollbar()->UsedColorScheme());

  ENABLE_OVERLAY_SCROLLBARS(false);
  Compositor().BeginFrame();
  EXPECT_FALSE(layout_viewport->VerticalScrollbar()->IsOverlayScrollbar());
  // Non overlay scrollbars used color scheme should follow the preferred
  // scrollable area's color scheme.
  EXPECT_EQ(mojom::blink::ColorScheme::kLight,
            layout_viewport->GetOverlayScrollbarColorScheme());
  EXPECT_EQ(mojom::blink::ColorScheme::kDark,
            layout_viewport->VerticalScrollbar()->UsedColorScheme());
}

}  // namespace blink
