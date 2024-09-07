// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_hit_test_result.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_node_string_trustedscript.h"
#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/dom_visual_viewport.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/frame/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using blink::test::RunPendingTasks;
using testing::Mock;

namespace blink {

namespace {

class RootScrollerTest : public testing::Test,
                         private ScopedImplicitRootScrollerForTest {
 public:
  RootScrollerTest()
      : ScopedImplicitRootScrollerForTest(true),
        base_url_("http://www.test.com/") {
    RegisterMockedHttpURLLoad("overflow-scrolling.html");
    RegisterMockedHttpURLLoad("root-scroller.html");
    RegisterMockedHttpURLLoad("root-scroller-rotation.html");
    RegisterMockedHttpURLLoad("root-scroller-iframe.html");
    RegisterMockedHttpURLLoad("root-scroller-child.html");
  }

  ~RootScrollerTest() override {
    features_backup_.Restore();
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  WebViewImpl* Initialize(const String& page_name) {
    return InitializeInternal(base_url_ + page_name);
  }

  WebViewImpl* Initialize() { return InitializeInternal("about:blank"); }

  static void ConfigureSettings(WebSettings* settings) {
    settings->SetJavaScriptEnabled(true);
    frame_test_helpers::WebViewHelper::UpdateAndroidCompositingSettings(
        settings);
  }

  void RegisterMockedHttpURLLoad(const String& file_name) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |helper_|.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString(base_url_), test::CoreTestDataPath(), WebString(file_name));
  }

  void ExecuteScript(const WebString& code) {
    ExecuteScript(code, *MainWebFrame());
  }

  void ExecuteScript(const WebString& code, WebLocalFrame& frame) {
    frame.ExecuteScript(WebScriptSource(code));
    frame.View()->MainFrameWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
    RunPendingTasks();
  }

  WebViewImpl* GetWebView() const { return helper_->GetWebView(); }

  Page& GetPage() const { return *GetWebView()->GetPage(); }

  PaintLayerScrollableArea* GetScrollableArea(const Element& element) const {
    return To<LayoutBoxModelObject>(element.GetLayoutObject())
        ->GetScrollableArea();
  }

  LocalFrame* MainFrame() const {
    return GetWebView()->MainFrameImpl()->GetFrame();
  }

  WebLocalFrame* MainWebFrame() const { return GetWebView()->MainFrameImpl(); }

  LocalFrameView* MainFrameView() const {
    return GetWebView()->MainFrameImpl()->GetFrame()->View();
  }

  VisualViewport& GetVisualViewport() const {
    return GetPage().GetVisualViewport();
  }

  BrowserControls& GetBrowserControls() const {
    return GetPage().GetBrowserControls();
  }

  Node* EffectiveRootScroller(Document* doc) const {
    return &doc->GetRootScrollerController().EffectiveRootScroller();
  }

  WebGestureEvent GenerateTouchGestureEvent(WebInputEvent::Type type,
                                            int delta_x = 0,
                                            int delta_y = 0) {
    WebGestureEvent event(type, WebInputEvent::kNoModifiers,
                          WebInputEvent::GetStaticTimeStampForTests(),
                          WebGestureDevice::kTouchscreen);
    event.SetPositionInWidget(gfx::PointF(100, 100));
    if (type == WebInputEvent::Type::kGestureScrollUpdate) {
      event.data.scroll_update.delta_x = delta_x;
      event.data.scroll_update.delta_y = delta_y;
    } else if (type == WebInputEvent::Type::kGestureScrollBegin) {
      event.data.scroll_begin.delta_x_hint = delta_x;
      event.data.scroll_begin.delta_y_hint = delta_y;
    }
    return event;
  }

  void SetCreateWebFrameWidgetCallback(
      const frame_test_helpers::CreateTestWebFrameWidgetCallback&
          create_widget_callback) {
    create_widget_callback_ = create_widget_callback;
  }

  bool UsesCompositedScrolling(
      const PaintLayerScrollableArea* scrollable_area) {
    auto* property_trees =
        MainFrameView()->RootCcLayer()->layer_tree_host()->property_trees();
    auto* scroll_node =
        property_trees->scroll_tree_mutable().FindNodeFromElementId(
            scrollable_area->GetScrollElementId());
    return scroll_node->is_composited;
  }

 protected:
  WebViewImpl* InitializeInternal(const String& url) {
    helper_ = std::make_unique<frame_test_helpers::WebViewHelper>(
        create_widget_callback_);

    helper_->InitializeAndLoad(url.Utf8(), nullptr, nullptr,
                               &ConfigureSettings);

    // Initialize browser controls to be shown.
    gfx::Size viewport_size = gfx::Size(400, 400);
    GetWebView()->ResizeWithBrowserControls(viewport_size, 50, 60, true);
    GetWebView()->GetBrowserControls().SetShownRatio(1, 1);
    helper_->GetMainFrameWidget()->UpdateCompositorViewportRect(
        gfx::Rect(viewport_size));

    UpdateAllLifecyclePhases(MainFrameView());

    return GetWebView();
  }

  void UpdateAllLifecyclePhases(LocalFrameView* view) {
    view->UpdateAllLifecyclePhasesForTest();
  }

  test::TaskEnvironment task_environment_;
  String base_url_;
  frame_test_helpers::CreateTestWebFrameWidgetCallback create_widget_callback_;
  std::unique_ptr<frame_test_helpers::WebViewHelper> helper_;
  RuntimeEnabledFeatures::Backup features_backup_;
};

// Test that the document Node should be the default effective root scroller.
TEST_F(RootScrollerTest, TestDefaultRootScroller) {
  Initialize("overflow-scrolling.html");

  EXPECT_EQ(MainFrame()->GetDocument(),
            EffectiveRootScroller(MainFrame()->GetDocument()));
}

// Make sure that replacing the documentElement doesn't change the effective
// root scroller when no root scroller is set.
TEST_F(RootScrollerTest, defaultEffectiveRootScrollerIsDocumentNode) {
  Initialize("overflow-scrolling.html");

  Document* document = MainFrame()->GetDocument();
  Element* iframe = document->CreateRawElement(html_names::kIFrameTag);

  EXPECT_EQ(MainFrame()->GetDocument(),
            EffectiveRootScroller(MainFrame()->GetDocument()));

  // Replace the documentElement with the iframe. The effectiveRootScroller
  // should remain the same.
  HeapVector<Member<V8UnionNodeOrStringOrTrustedScript>> nodes;
  nodes.push_back(
      MakeGarbageCollected<V8UnionNodeOrStringOrTrustedScript>(iframe));
  document->documentElement()->replaceWith(nodes, ASSERT_NO_EXCEPTION);

  UpdateAllLifecyclePhases(MainFrameView());

  EXPECT_EQ(MainFrame()->GetDocument(),
            EffectiveRootScroller(MainFrame()->GetDocument()));
}

// Tests that a DIV which becomes the implicit root scroller will properly
// control url bar and bottom bar hiding and overscroll.
TEST_F(RootScrollerTest, BrowserControlsAndOverscroll) {
  Initialize("root-scroller.html");
  UpdateAllLifecyclePhases(MainFrameView());

  Element* container =
      MainFrame()->GetDocument()->getElementById(AtomicString("container"));
  ASSERT_EQ(container, EffectiveRootScroller(MainFrame()->GetDocument()));

  // Content is 1000x1000, WebView is 400x400 but hiding the 50px top controls
  // and the 60px bottom controls makes it 400x510 so max scroll is 490px.
  double maximum_scroll = 490;

  auto* widget = helper_->GetMainFrameWidget();
  auto* layer_tree_host = helper_->GetLayerTreeHost();
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());

  widget->DispatchThroughCcInputHandler(
      GenerateTouchGestureEvent(WebInputEvent::Type::kGestureScrollBegin));
  {
    // Scrolling over the #container DIV should cause the browser controls to
    // hide.
    EXPECT_FLOAT_EQ(1, GetBrowserControls().TopShownRatio());
    EXPECT_FLOAT_EQ(1, GetBrowserControls().BottomShownRatio());
    widget->DispatchThroughCcInputHandler(
        GenerateTouchGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, 0,
                                  -GetBrowserControls().TopHeight()));
    layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                      base::OnceClosure());

    EXPECT_FLOAT_EQ(0, GetBrowserControls().TopShownRatio());
    EXPECT_FLOAT_EQ(0, GetBrowserControls().BottomShownRatio());
  }

  {
    // Make sure we're actually scrolling the DIV and not the LocalFrameView.
    widget->DispatchThroughCcInputHandler(GenerateTouchGestureEvent(
        WebInputEvent::Type::kGestureScrollUpdate, 0, -100));
    layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                      base::OnceClosure());

    EXPECT_FLOAT_EQ(100, container->scrollTop());
    EXPECT_FLOAT_EQ(0,
                    MainFrameView()->LayoutViewport()->GetScrollOffset().y());
  }

  {
    // Scroll 50 pixels past the end. Ensure we report the 50 pixels as
    // overscroll.
    widget->DispatchThroughCcInputHandler(GenerateTouchGestureEvent(
        WebInputEvent::Type::kGestureScrollUpdate, 0, -440));
    layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                      base::OnceClosure());

    EXPECT_TRUE(
        widget->last_overscroll()->Equals(mojom::blink::DidOverscrollParams(
            gfx::Vector2dF(0, 50), gfx::Vector2dF(0, 50), gfx::Vector2dF(),
            gfx::PointF(100, 100), cc::OverscrollBehavior())));

    EXPECT_FLOAT_EQ(maximum_scroll, container->scrollTop());
    EXPECT_FLOAT_EQ(0,
                    MainFrameView()->LayoutViewport()->GetScrollOffset().y());
  }

  {
    // Continue the gesture overscroll.
    widget->DispatchThroughCcInputHandler(GenerateTouchGestureEvent(
        WebInputEvent::Type::kGestureScrollUpdate, 0, -20));
    layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                      base::OnceClosure());

    EXPECT_TRUE(
        widget->last_overscroll()->Equals(mojom::blink::DidOverscrollParams(
            gfx::Vector2dF(0, 70), gfx::Vector2dF(0, 20), gfx::Vector2dF(),
            gfx::PointF(100, 100), cc::OverscrollBehavior())));

    EXPECT_FLOAT_EQ(maximum_scroll, container->scrollTop());
    EXPECT_FLOAT_EQ(0,
                    MainFrameView()->LayoutViewport()->GetScrollOffset().y());
  }

  widget->DispatchThroughCcInputHandler(
      GenerateTouchGestureEvent(WebInputEvent::Type::kGestureScrollEnd));
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());

  {
    // Make sure a new gesture scroll still won't scroll the frameview and
    // overscrolls.
    widget->DispatchThroughCcInputHandler(
        GenerateTouchGestureEvent(WebInputEvent::Type::kGestureScrollBegin));

    widget->DispatchThroughCcInputHandler(GenerateTouchGestureEvent(
        WebInputEvent::Type::kGestureScrollUpdate, 0, -30));
    layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                      base::OnceClosure());

    EXPECT_TRUE(
        widget->last_overscroll()->Equals(mojom::blink::DidOverscrollParams(
            gfx::Vector2dF(0, 30), gfx::Vector2dF(0, 30), gfx::Vector2dF(),
            gfx::PointF(100, 100), cc::OverscrollBehavior())));

    EXPECT_FLOAT_EQ(maximum_scroll, container->scrollTop());
    EXPECT_FLOAT_EQ(0,
                    MainFrameView()->LayoutViewport()->GetScrollOffset().y());

    widget->DispatchThroughCcInputHandler(
        GenerateTouchGestureEvent(WebInputEvent::Type::kGestureScrollEnd));
    layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                      base::OnceClosure());
  }

  {
    // Scrolling up should show the browser controls.
    widget->DispatchThroughCcInputHandler(
        GenerateTouchGestureEvent(WebInputEvent::Type::kGestureScrollBegin));

    EXPECT_FLOAT_EQ(0, GetBrowserControls().TopShownRatio());
    EXPECT_FLOAT_EQ(0, GetBrowserControls().BottomShownRatio());

    widget->DispatchThroughCcInputHandler(GenerateTouchGestureEvent(
        WebInputEvent::Type::kGestureScrollUpdate, 0, 30));
    layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                      base::OnceClosure());

    EXPECT_FLOAT_EQ(0.6, GetBrowserControls().TopShownRatio());
    EXPECT_FLOAT_EQ(0.6, GetBrowserControls().BottomShownRatio());

    widget->DispatchThroughCcInputHandler(
        GenerateTouchGestureEvent(WebInputEvent::Type::kGestureScrollEnd));
  }

  // Reset manually to avoid lifetime issues with custom WebViewClient.
  helper_->Reset();
}

// Tests that removing the element that is the root scroller from the DOM tree
// changes the effective root scroller.
TEST_F(RootScrollerTest, TestRemoveRootScrollerFromDom) {
  Initialize("root-scroller.html");

  Element* container =
      MainFrame()->GetDocument()->getElementById(AtomicString("container"));
  UpdateAllLifecyclePhases(MainFrameView());

  EXPECT_EQ(container, EffectiveRootScroller(MainFrame()->GetDocument()));

  MainFrame()->GetDocument()->body()->RemoveChild(container);
  UpdateAllLifecyclePhases(MainFrameView());

  EXPECT_NE(container, EffectiveRootScroller(MainFrame()->GetDocument()));
}

// Test that the effective root scroller resets to the document Node when the
// current root scroller element becomes invalid as a scroller.
TEST_F(RootScrollerTest, TestRootScrollerBecomesInvalid) {
  Initialize("root-scroller.html");

  Element* container =
      MainFrame()->GetDocument()->getElementById(AtomicString("container"));

  {
    EXPECT_EQ(container, EffectiveRootScroller(MainFrame()->GetDocument()));

    ExecuteScript(
        "document.querySelector('#container').style.display = 'inline'");
    UpdateAllLifecyclePhases(MainFrameView());

    EXPECT_EQ(MainFrame()->GetDocument(),
              EffectiveRootScroller(MainFrame()->GetDocument()));
  }

  ExecuteScript("document.querySelector('#container').style.display = 'block'");
  UpdateAllLifecyclePhases(MainFrameView());

  {
    EXPECT_EQ(container, EffectiveRootScroller(MainFrame()->GetDocument()));

    ExecuteScript("document.querySelector('#container').style.width = '98%'");
    UpdateAllLifecyclePhases(MainFrameView());

    EXPECT_EQ(MainFrame()->GetDocument(),
              EffectiveRootScroller(MainFrame()->GetDocument()));
  }
}

// Ensures that disconnecting the element currently set as the root scroller
// recomputes the effective root scroller, before a lifecycle update.
TEST_F(RootScrollerTest, RemoveCurrentRootScroller) {
  Initialize();

  WebURL base_url = url_test_helpers::ToKURL("http://www.test.com/");
  frame_test_helpers::LoadHTMLString(GetWebView()->MainFrameImpl(),
                                     R"HTML(
                                     <!DOCTYPE html>
                                     <style>
                                       body,html {
                                         width: 100%;
                                         height: 100%;
                                         margin: 0px;
                                       }
                                       #container {
                                         width: 100%;
                                         height: 100%;
                                         position: absolute;
                                         overflow: auto;
                                       }
                                       #spacer {
                                         width: 200vw;
                                         height: 200vh;
                                       }
                                     </style>
                                     <div id='container'>
                                       <div id='spacer'></diiv>
                                     </div>)HTML",
                                     base_url);

  RootScrollerController& controller =
      MainFrame()->GetDocument()->GetRootScrollerController();
  Element* container =
      MainFrame()->GetDocument()->getElementById(AtomicString("container"));
  UpdateAllLifecyclePhases(MainFrameView());
  ASSERT_EQ(container, controller.EffectiveRootScroller());

  // Remove the div from the document. It should be demoted from the effective
  // root scroller. The effective will fallback to the document Node.
  {
    MainFrame()->GetDocument()->body()->setTextContent("");
    EXPECT_EQ(MainFrame()->GetDocument(), controller.EffectiveRootScroller());
  }
}

// Ensures that the root scroller always gets composited with scrolling layers.
// This is necessary since we replace the Frame scrolling layers in CC as the
// OuterViewport, we need something to replace them with.
TEST_F(RootScrollerTest, AlwaysCreateCompositedScrollingLayers) {
  Initialize();
  GetPage().GetSettings().SetPreferCompositingToLCDTextForTesting(false);

  WebURL base_url = url_test_helpers::ToKURL("http://www.test.com/");
  frame_test_helpers::LoadHTMLString(GetWebView()->MainFrameImpl(),
                                     R"HTML(
      <!DOCTYPE html>
      <style>
        body,html {
          width: 100%;
          height: 100%;
          margin: 0px;
        }
        #container {
          width: 98%;
          height: 100%;
          position: absolute;
          overflow: auto;
        }
        #spacer {
          width: 200vw;
          height: 200vh;
        }
      </style>
      <div id='container'>
        <div id='spacer'></div>
      </div>)HTML",
                                     base_url);

  GetWebView()->ResizeWithBrowserControls(gfx::Size(400, 400), 50, 0, true);
  UpdateAllLifecyclePhases(MainFrameView());

  Element* container =
      MainFrame()->GetDocument()->getElementById(AtomicString("container"));

  PaintLayerScrollableArea* container_scroller = GetScrollableArea(*container);
  ASSERT_FALSE(UsesCompositedScrolling(container_scroller));

  ExecuteScript("document.querySelector('#container').style.width = '100%'");
  ASSERT_EQ(container, EffectiveRootScroller(MainFrame()->GetDocument()));

  ASSERT_TRUE(UsesCompositedScrolling(container_scroller));

  ExecuteScript("document.querySelector('#container').style.width = '98%'");
  ASSERT_EQ(MainFrame()->GetDocument(),
            EffectiveRootScroller(MainFrame()->GetDocument()));

  EXPECT_FALSE(UsesCompositedScrolling(container_scroller));
}

// Make sure that if an effective root scroller becomes a remote frame, it's
// immediately demoted.
TEST_F(RootScrollerTest, IFrameSwapToRemote) {
  Initialize("root-scroller-iframe.html");
  Element* iframe =
      MainFrame()->GetDocument()->getElementById(AtomicString("iframe"));

  ASSERT_EQ(iframe, EffectiveRootScroller(MainFrame()->GetDocument()));

  // Swap in a remote frame. Make sure we revert back to the document.
  {
    frame_test_helpers::SwapRemoteFrame(MainWebFrame()->FirstChild(),
                                        frame_test_helpers::CreateRemote());
    UpdateAllLifecyclePhases(MainFrameView());
    EXPECT_EQ(MainFrame()->GetDocument(),
              EffectiveRootScroller(MainFrame()->GetDocument()));
    GetWebView()->ResizeWithBrowserControls(gfx::Size(400, 450), 50, 0, false);
    UpdateAllLifecyclePhases(MainFrameView());
    EXPECT_EQ(MainFrame()->GetDocument(),
              EffectiveRootScroller(MainFrame()->GetDocument()));
  }
}

// Tests that removing the root scroller element from the DOM resets the
// effective root scroller without waiting for any lifecycle events.
TEST_F(RootScrollerTest, RemoveRootScrollerFromDom) {
  Initialize("root-scroller-iframe.html");

  {
    auto* iframe = To<HTMLFrameOwnerElement>(
        MainFrame()->GetDocument()->getElementById(AtomicString("iframe")));

    ASSERT_EQ(iframe, EffectiveRootScroller(MainFrame()->GetDocument()));

    iframe->contentDocument()->body()->setInnerHTML("");

    // If the root scroller wasn't updated by the DOM removal above, this
    // will touch the disposed root scroller's ScrollableArea.
    MainFrameView()->GetRootFrameViewport()->ServiceScrollAnimations(0);
  }
}

// Tests that we still have a global root scroller layer when the HTML element
// has no layout object. crbug.com/637036.
TEST_F(RootScrollerTest, DocumentElementHasNoLayoutObject) {
  Initialize("overflow-scrolling.html");

  // There's no rootScroller set on this page so we should default to the
  // document Node, which means we should use the layout viewport. Ensure this
  // happens even if the <html> element has no LayoutObject.
  ExecuteScript("document.documentElement.style.display = 'none';");

  const TopDocumentRootScrollerController& global_controller =
      MainFrame()->GetDocument()->GetPage()->GlobalRootScrollerController();

  EXPECT_EQ(MainFrame()->GetDocument(), global_controller.GlobalRootScroller());
}

// On Android, the main scrollbars are owned by the visual viewport and the
// LocalFrameView's disabled. This functionality should extend to a rootScroller
// that isn't the main LocalFrameView.
TEST_F(RootScrollerTest, UseVisualViewportScrollbars) {
  Initialize("root-scroller.html");

  Element* container =
      MainFrame()->GetDocument()->getElementById(AtomicString("container"));
  ASSERT_EQ(container, EffectiveRootScroller(MainFrame()->GetDocument()));

  ScrollableArea* container_scroller = GetScrollableArea(*container);
  EXPECT_FALSE(container_scroller->HorizontalScrollbar());
  EXPECT_FALSE(container_scroller->VerticalScrollbar());
  EXPECT_GT(container_scroller->MaximumScrollOffset().x(), 0);
  EXPECT_GT(container_scroller->MaximumScrollOffset().y(), 0);
}

// On Android, the main scrollbars are owned by the visual viewport and the
// LocalFrameView's disabled. This functionality should extend to a rootScroller
// that's a nested iframe.
TEST_F(RootScrollerTest, UseVisualViewportScrollbarsIframe) {
  Initialize("root-scroller-iframe.html");

  Element* iframe =
      MainFrame()->GetDocument()->getElementById(AtomicString("iframe"));
  auto* child_frame =
      To<LocalFrame>(To<HTMLFrameOwnerElement>(iframe)->ContentFrame());

  ASSERT_EQ(iframe, EffectiveRootScroller(MainFrame()->GetDocument()));
  UpdateAllLifecyclePhases(MainFrameView());

  ScrollableArea* container_scroller = child_frame->View()->LayoutViewport();

  EXPECT_FALSE(container_scroller->HorizontalScrollbar());
  EXPECT_FALSE(container_scroller->VerticalScrollbar());
  EXPECT_GT(container_scroller->MaximumScrollOffset().x(), 0);
  EXPECT_GT(container_scroller->MaximumScrollOffset().y(), 0);
}

TEST_F(RootScrollerTest, TopControlsAdjustmentAppliedToRootScroller) {
  Initialize();

  WebURL base_url = url_test_helpers::ToKURL("http://www.test.com/");
  frame_test_helpers::LoadHTMLString(GetWebView()->MainFrameImpl(),
                                     "<!DOCTYPE html>"
                                     "<style>"
                                     "  body, html {"
                                     "    width: 100%;"
                                     "    height: 100%;"
                                     "    margin: 0px;"
                                     "  }"
                                     "  #container {"
                                     "    width: 100%;"
                                     "    height: 100%;"
                                     "    overflow: auto;"
                                     "  }"
                                     "</style>"
                                     "<div id='container'>"
                                     "  <div style='height:1000px'>test</div>"
                                     "</div>",
                                     base_url);

  GetWebView()->ResizeWithBrowserControls(gfx::Size(400, 400), 50, 50, true);

  auto* widget = helper_->GetMainFrameWidget();
  auto* layer_tree_host = helper_->GetLayerTreeHost();
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());

  Element* container =
      MainFrame()->GetDocument()->getElementById(AtomicString("container"));
  ASSERT_EQ(container, EffectiveRootScroller(MainFrame()->GetDocument()));

  ScrollableArea* container_scroller = GetScrollableArea(*container);

  // Hide the top controls and scroll down maximally. We should account for the
  // change in maximum scroll offset due to the top controls hiding. That is,
  // since the controls are hidden, the "content area" is taller so the maximum
  // scroll offset should shrink.
  ASSERT_EQ(1000 - 400, container_scroller->MaximumScrollOffset().y());

  widget->DispatchThroughCcInputHandler(
      GenerateTouchGestureEvent(WebInputEvent::Type::kGestureScrollBegin));

  ASSERT_EQ(1, GetBrowserControls().TopShownRatio());
  ASSERT_EQ(1, GetBrowserControls().BottomShownRatio());

  widget->DispatchThroughCcInputHandler(
      GenerateTouchGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, 0,
                                -GetBrowserControls().TopHeight()));
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());

  ASSERT_EQ(0, GetBrowserControls().TopShownRatio());
  ASSERT_EQ(0, GetBrowserControls().BottomShownRatio());

  // TODO(crbug.com/1364851): This should be 1000 - 500, but the main thread's
  // maximum scroll offset does not account for the hidden bottom bar.
  EXPECT_EQ(1000 - 450, container_scroller->MaximumScrollOffset().y());

  widget->DispatchThroughCcInputHandler(GenerateTouchGestureEvent(
      WebInputEvent::Type::kGestureScrollUpdate, 0, -3000));
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());

  // The compositor input handler correctly accounts for both top and bottom bar
  // in the calculation of scroll bounds. This is the true maximum.
  EXPECT_EQ(1000 - 500, container_scroller->GetScrollOffset().y());

  widget->DispatchThroughCcInputHandler(
      GenerateTouchGestureEvent(WebInputEvent::Type::kGestureScrollEnd));
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());

  GetWebView()->ResizeWithBrowserControls(gfx::Size(400, 450), 50, 50, false);

  // TODO(crbug.com/1364851): This should be 1000 - 500, but the main thread's
  // maximum scroll offset does not account for the hidden bottom bar.
  EXPECT_EQ(1000 - 450, container_scroller->MaximumScrollOffset().y());
}

TEST_F(RootScrollerTest, RotationAnchoring) {
  Initialize("root-scroller-rotation.html");

  auto* widget = helper_->GetMainFrameWidget();
  auto* layer_tree_host = helper_->GetLayerTreeHost();
  ScrollableArea* container_scroller;

  {
    GetWebView()->ResizeWithBrowserControls(gfx::Size(250, 1000), 0, 0, true);
    UpdateAllLifecyclePhases(MainFrameView());

    Element* container =
        MainFrame()->GetDocument()->getElementById(AtomicString("container"));
    ASSERT_EQ(container, EffectiveRootScroller(MainFrame()->GetDocument()));

    container_scroller = GetScrollableArea(*container);
  }

  Element* target =
      MainFrame()->GetDocument()->getElementById(AtomicString("target"));

  // Zoom in and scroll the viewport so that the target is fully in the
  // viewport and the visual viewport is fully scrolled within the layout
  // viepwort.
  {
    int scroll_x = 250 * 4;
    int scroll_y = 1000 * 4;

    GetWebView()->SetPageScaleFactor(2);
    layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                      base::OnceClosure());

    widget->DispatchThroughCcInputHandler(
        GenerateTouchGestureEvent(WebInputEvent::Type::kGestureScrollBegin));
    widget->DispatchThroughCcInputHandler(GenerateTouchGestureEvent(
        WebInputEvent::Type::kGestureScrollUpdate, -scroll_x, -scroll_y));
    widget->DispatchThroughCcInputHandler(
        GenerateTouchGestureEvent(WebInputEvent::Type::kGestureScrollEnd));
    layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                      base::OnceClosure());

    // The visual viewport should be 1.5 screens scrolled so that the target
    // occupies the bottom quadrant of the layout viewport.
    ASSERT_EQ((250 * 3) / 2, container_scroller->GetScrollOffset().x());
    ASSERT_EQ((1000 * 3) / 2, container_scroller->GetScrollOffset().y());

    // The visual viewport should have scrolled the last half layout viewport.
    ASSERT_EQ((250) / 2, GetVisualViewport().GetScrollOffset().x());
    ASSERT_EQ((1000) / 2, GetVisualViewport().GetScrollOffset().y());
  }

  // Now do a rotation resize.
  GetWebView()->ResizeWithBrowserControls(gfx::Size(1000, 250), 50, 0, false);
  UpdateAllLifecyclePhases(MainFrameView());

  // The visual viewport should remain fully filled by the target.
  DOMRect* rect = target->GetBoundingClientRect();
  EXPECT_EQ(rect->left(), GetVisualViewport().GetScrollOffset().x());
  EXPECT_EQ(rect->top(), GetVisualViewport().GetScrollOffset().y());
}

// Tests that we don't crash if the default documentElement isn't a valid root
// scroller. This can happen in some edge cases where documentElement isn't
// <html>. crbug.com/668553.
TEST_F(RootScrollerTest, InvalidDefaultRootScroller) {
  Initialize("overflow-scrolling.html");

  Document* document = MainFrame()->GetDocument();

  Element* br = document->CreateRawElement(html_names::kBrTag);
  document->ReplaceChild(br, document->documentElement());
  UpdateAllLifecyclePhases(MainFrameView());
  Element* html = document->CreateRawElement(html_names::kHTMLTag);
  Element* body = document->CreateRawElement(html_names::kBodyTag);
  html->AppendChild(body);
  body->AppendChild(br);
  document->AppendChild(html);
  UpdateAllLifecyclePhases(MainFrameView());
}

// Makes sure that when an iframe becomes the effective root scroller, its
// FrameView stops sizing layout to the frame rect and uses its parent's layout
// size instead. This allows matching the layout size semantics of the root
// FrameView since its layout size can differ from the frame rect due to
// resizes by the URL bar.
TEST_F(RootScrollerTest, IFrameRootScrollerGetsNonFixedLayoutSize) {
  Initialize("root-scroller-iframe.html");
  UpdateAllLifecyclePhases(MainFrameView());

  auto* iframe = To<HTMLFrameOwnerElement>(
      MainFrame()->GetDocument()->getElementById(AtomicString("iframe")));
  auto* iframe_view = To<LocalFrame>(iframe->ContentFrame())->View();

  ASSERT_EQ(gfx::Size(400, 400), iframe_view->GetLayoutSize());
  ASSERT_EQ(gfx::Size(400, 400), iframe_view->Size());

  // Make the iframe the rootscroller. This should cause the iframe's layout
  // size to be manually controlled.
  {
    ASSERT_EQ(iframe, EffectiveRootScroller(MainFrame()->GetDocument()));
    EXPECT_FALSE(iframe_view->LayoutSizeFixedToFrameSize());
    EXPECT_EQ(gfx::Size(400, 400), iframe_view->GetLayoutSize());
    EXPECT_EQ(gfx::Size(400, 400), iframe_view->Size());
  }

  // Hide the URL bar, the iframe's frame rect should expand but the layout
  // size should remain the same.
  {
    GetWebView()->ResizeWithBrowserControls(gfx::Size(400, 450), 50, 0, false);
    UpdateAllLifecyclePhases(MainFrameView());
    EXPECT_EQ(gfx::Size(400, 400), iframe_view->GetLayoutSize());
    EXPECT_EQ(gfx::Size(400, 450), iframe_view->Size());
  }

  // Simulate a rotation. This time the layout size should reflect the resize.
  {
    GetWebView()->ResizeWithBrowserControls(gfx::Size(450, 400), 50, 0, false);
    UpdateAllLifecyclePhases(MainFrameView());
    EXPECT_EQ(gfx::Size(450, 350), iframe_view->GetLayoutSize());
    EXPECT_EQ(gfx::Size(450, 400), iframe_view->Size());

    // "Un-rotate" for following tests.
    GetWebView()->ResizeWithBrowserControls(gfx::Size(400, 450), 50, 0, false);
    UpdateAllLifecyclePhases(MainFrameView());
  }

  // Show the URL bar again. The frame rect should match the viewport.
  {
    GetWebView()->ResizeWithBrowserControls(gfx::Size(400, 400), 50, 0, true);
    UpdateAllLifecyclePhases(MainFrameView());
    EXPECT_EQ(gfx::Size(400, 400), iframe_view->GetLayoutSize());
    EXPECT_EQ(gfx::Size(400, 400), iframe_view->Size());
  }

  // Hide the URL bar and reset the rootScroller. The iframe should go back to
  // tracking layout size by frame rect.
  {
    GetWebView()->ResizeWithBrowserControls(gfx::Size(400, 450), 50, 0, false);
    UpdateAllLifecyclePhases(MainFrameView());
    EXPECT_EQ(gfx::Size(400, 400), iframe_view->GetLayoutSize());
    EXPECT_EQ(gfx::Size(400, 450), iframe_view->Size());
    ExecuteScript("document.querySelector('#iframe').style.opacity = '0.5'");
    ASSERT_EQ(MainFrame()->GetDocument(),
              EffectiveRootScroller(MainFrame()->GetDocument()));
    EXPECT_TRUE(iframe_view->LayoutSizeFixedToFrameSize());
    EXPECT_EQ(gfx::Size(400, 400), iframe_view->GetLayoutSize());
    EXPECT_EQ(gfx::Size(400, 400), iframe_view->Size());
  }
}

// Ensure that removing the root scroller element causes an update to the
// RootFrameViewport's layout viewport immediately since old layout viewport is
// now part of a detached layout hierarchy.
TEST_F(RootScrollerTest, ImmediateUpdateOfLayoutViewport) {
  Initialize("root-scroller-iframe.html");

  auto* iframe = To<HTMLFrameOwnerElement>(
      MainFrame()->GetDocument()->getElementById(AtomicString("iframe")));

  ASSERT_EQ(iframe, EffectiveRootScroller(MainFrame()->GetDocument()));

  RootScrollerController& main_controller =
      MainFrame()->GetDocument()->GetRootScrollerController();

  auto* iframe_local_frame = To<LocalFrame>(iframe->ContentFrame());
  EXPECT_EQ(iframe, &main_controller.EffectiveRootScroller());
  EXPECT_EQ(iframe_local_frame->View()->LayoutViewport(),
            &MainFrameView()->GetRootFrameViewport()->LayoutViewport());

  // Remove the <iframe> and make sure the layout viewport reverts to the
  // LocalFrameView without a layout.
  iframe->remove();

  EXPECT_EQ(MainFrameView()->LayoutViewport(),
            &MainFrameView()->GetRootFrameViewport()->LayoutViewport());
}

class ImplicitRootScrollerSimTest : public SimTest {
 public:
  ImplicitRootScrollerSimTest() : implicit_root_scroller_for_test_(true) {}
  ~ImplicitRootScrollerSimTest() override {
    // TODO(crbug.com/1315595): Consider moving this to MainThreadIsolate.
    MemoryCache::Get()->EvictResources();
    // Clear lazily loaded style sheets.
    CSSDefaultStyleSheets::Instance().PrepareForLeakDetection();
  }
  void SetUp() override {
    SimTest::SetUp();
    WebView().GetPage()->GetSettings().SetViewportEnabled(true);
  }

 private:
  ScopedImplicitRootScrollerForTest implicit_root_scroller_for_test_;
};

// Test that the cached IsEffectiveRootScroller bit on LayoutObject is set
// correctly when the Document is the effective root scroller. It becomes the
// root scroller before Document has a LayoutView.
TEST_F(ImplicitRootScrollerSimTest, DocumentEffectiveSetsCachedBit) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
      )HTML");
  Compositor().BeginFrame();

  EXPECT_TRUE(GetDocument().GetLayoutView()->IsEffectiveRootScroller());
}

// Test that layout from outside a lifecycle wont select a new effective root
// scroller.
TEST_F(ImplicitRootScrollerSimTest, NonLifecycleLayoutDoesntCauseReselection) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            body, html {
              margin: 0;
              width: 100%;
              height: 100%;
            }
            #container {
              width: 100%;
              height: 100%;
              overflow: scroll;
            }
            #spacer {
              width: 200vw;
              height: 200vh;
            }
          </style>
          <div id="container">
            <div id="spacer"></div>
          </div>
      )HTML");
  Element* container = GetDocument().getElementById(AtomicString("container"));
  Compositor().BeginFrame();
  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  container->style()->setProperty(GetDocument().GetExecutionContext(), "width",
                                  "95%", String(), ASSERT_NO_EXCEPTION);

  ASSERT_TRUE(Compositor().NeedsBeginFrame());

  // Cause a layout.
  container->scrollTop();
  ASSERT_TRUE(Compositor().NeedsBeginFrame());

  // Shouldn't yet cause a change since we haven't done a full lifecycle update.
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  Compositor().BeginFrame();

  EXPECT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());
}

// Tests that we don't explode when a layout occurs and the effective
// rootScroller no longer has a ContentFrame(). We setup the frame tree such
// that the first iframe is the effective root scroller. The second iframe has
// an unload handler that reaches back to the common parent and causes a
// layout. This will cause us to recalculate the effective root scroller while
// the current one is valid in all ways except that it no longer has a content
// frame. This test passes if it doesn't crash. https://crbug.com/805317.
TEST_F(ImplicitRootScrollerSimTest, RecomputeEffectiveWithNoContentFrame) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  SimRequest first_request("https://example.com/first.html", "text/html");
  SimRequest second_request("https://example.com/second.html", "text/html");
  SimRequest final_request("https://newdomain.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            #first {
              width: 100%;
              height: 100%;
              border: 0;
            }
            #second {
              width: 10px;
              height: 10px;
              position: absolute;
              left: 0px;
              top: 0px;
            }
          </style>
          <iframe id="first" src="https://example.com/first.html">
          </iframe>
          <iframe id="second" src="https://example.com/second.html">
          </iframe>
          <script>
            // Dirty layout on unload
            window.addEventListener('unload', function() {
                document.getElementById("first").style.width="0";
            });
          </script>
      )HTML");

  first_request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            body {
              height: 300vh;
            }
          </style>
      )HTML");

  second_request.Complete(R"HTML(
          <!DOCTYPE html>
          <body></body>
          <script>
            window.addEventListener('unload', function() {
                // This will do a layout.
                window.top.document.getElementById("first").clientWidth;
            });
          </script>
      )HTML");

  Element* container = GetDocument().getElementById(AtomicString("first"));
  Compositor().BeginFrame();

  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // This will unload first the root, then the first frame, then the second.
  LoadURL("https://newdomain.com/test.html");
  final_request.Complete(R"HTML(
          <!DOCTYPE html>
      )HTML");
}

// Test that the element is considered to be viewport filling only if its
// padding box fills the viewport. That means it must have no border.
TEST_F(ImplicitRootScrollerSimTest, UsePaddingBoxForViewportFillingCondition) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            html,body {
              margin: 0;
              width: 100%;
              height: 100%;
            }
            #container {
              position: absolute;
              width: 100%;
              height: 100%;
              box-sizing: border-box;
              overflow: scroll;
            }
            #spacer {
              width: 200vw;
              height: 200vh;
            }
          </style>
          <div id="container">
            <div id="spacer"></div>
          </div>
      )HTML");

  Element* container = GetDocument().getElementById(AtomicString("container"));
  Compositor().BeginFrame();

  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // Setting a border should cause the element to no longer be valid as its
  // padding box doesn't fill the viewport exactly.
  container->setAttribute(html_names::kStyleAttr,
                          AtomicString("border: 1px solid black"));
  Compositor().BeginFrame();
  EXPECT_EQ(&GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());
}

// Tests that the root scroller doesn't affect visualViewport pageLeft and
// pageTop.
TEST_F(ImplicitRootScrollerSimTest, RootScrollerDoesntAffectVisualViewport) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Write(R"HTML(
          <!DOCTYPE html>
          <style>
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }

            #spacer {
              width: 1000px;
              height: 1000px;
            }

            #container {
              width: 100%;
              height: 100%;
              overflow: auto;
            }
          </style>
          <div id="container">
            <div id="spacer"></div>
          </div>
      )HTML");

  GetDocument().GetPage()->GetVisualViewport().SetScale(2);
  GetDocument().GetPage()->GetVisualViewport().SetLocation(
      gfx::PointF(100, 120));

  auto* frame = To<LocalFrame>(GetDocument().GetPage()->MainFrame());
  EXPECT_EQ(100, frame->DomWindow()->visualViewport()->pageLeft());
  EXPECT_EQ(120, frame->DomWindow()->visualViewport()->pageTop());

  request.Finish();
  Compositor().BeginFrame();

  Element* container = GetDocument().getElementById(AtomicString("container"));
  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());
  container->setScrollTop(50);
  container->setScrollLeft(60);

  ASSERT_EQ(50, container->scrollTop());
  ASSERT_EQ(60, container->scrollLeft());
  ASSERT_EQ(100, frame->DomWindow()->visualViewport()->pageLeft());
  EXPECT_EQ(120, frame->DomWindow()->visualViewport()->pageTop());
}

// Tests that we don't crash or violate lifecycle assumptions when we resize
// from within layout.
TEST_F(ImplicitRootScrollerSimTest, ResizeFromResizeAfterLayout) {
  WebView().GetSettings()->SetShrinksViewportContentToFit(true);
  WebView().SetDefaultPageScaleLimits(0.25f, 5);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Write(R"HTML(
          <!DOCTYPE html>
          <style>
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }

            #container {
              width: 100%;
              height: 100%;
              border: 0;
            }
          </style>
          <iframe id="container"
                  srcdoc="<!DOCTYPE html>
                          <style>html {height: 300%;}</style>">
          </iframe>
      )HTML");
  RunPendingTasks();
  Compositor().BeginFrame();

  Element* container = GetDocument().getElementById(AtomicString("container"));
  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());
  ASSERT_EQ(gfx::Size(800, 600), GetDocument().View()->Size());

  request.Write(R"HTML(
          <div style="width:2000px;height:1000px"></div>
      )HTML");
  request.Finish();
  Compositor().BeginFrame();

  ASSERT_EQ(gfx::Size(2000, 1500), GetDocument().View()->Size());
}

// Tests basic implicit root scroller mode with a <div>.
TEST_F(ImplicitRootScrollerSimTest, ImplicitRootScroller) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            html {
              overflow: hidden;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            #spacer {
              width: 1000px;
              height: 1000px;
            }
            #container {
              width: 100%;
              height: 100%;
            }
          </style>
          <div id="container">
            <div id="spacer"></div>
          </div>
      )HTML");
  Compositor().BeginFrame();

  ASSERT_EQ(&GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());
  Element* container = GetDocument().getElementById(AtomicString("container"));

  // overflow: auto and overflow: scroll should cause a valid element to be
  // promoted to root scroller. Otherwise, they shouldn't, even if they're
  // otherwise a valid root scroller element.
  Vector<std::tuple<String, String, Node*>> test_cases = {
      {"overflow", "hidden", &GetDocument()},
      {"overflow", "auto", container},
      {"overflow", "scroll", container},
      {"overflow", "visible", &GetDocument()},
      // Overflow: hidden in one axis forces the other axis to auto so it should
      // be promoted.
      {"overflow-x", "hidden", container},
      {"overflow-x", "auto", container},
      {"overflow-x", "scroll", container},
      {"overflow-x", "visible", &GetDocument()},
      {"overflow-y", "hidden", container},
      {"overflow-y", "auto", container},
      {"overflow-y", "scroll", container},
      {"overflow-y", "visible", &GetDocument()}};

  for (auto test_case : test_cases) {
    String& style = std::get<0>(test_case);
    String& style_val = std::get<1>(test_case);
    Node* expected_root_scroller = std::get<2>(test_case);

    container->style()->setProperty(GetDocument().GetExecutionContext(), style,
                                    style_val, String(), ASSERT_NO_EXCEPTION);
    Compositor().BeginFrame();
    ASSERT_EQ(expected_root_scroller,
              GetDocument().GetRootScrollerController().EffectiveRootScroller())
        << "Failed to set rootScroller after setting " << std::get<0>(test_case)
        << ": " << std::get<1>(test_case);
    container->style()->setProperty(GetDocument().GetExecutionContext(),
                                    std::get<0>(test_case), String(), String(),
                                    ASSERT_NO_EXCEPTION);
    Compositor().BeginFrame();
    ASSERT_EQ(&GetDocument(),
              GetDocument().GetRootScrollerController().EffectiveRootScroller())
        << "Failed to reset rootScroller after setting "
        << std::get<0>(test_case) << ": " << std::get<1>(test_case);
  }

  // Now remove the overflowing element and rerun the tests. The container
  // element should no longer be implicitly promoted as it doesn't have any
  // overflow.
  Element* spacer = GetDocument().getElementById(AtomicString("spacer"));
  spacer->remove();

  for (auto test_case : test_cases) {
    String& style = std::get<0>(test_case);
    String& style_val = std::get<1>(test_case);
    Node* expected_root_scroller = &GetDocument();

    container->style()->setProperty(GetDocument().GetExecutionContext(), style,
                                    style_val, String(), ASSERT_NO_EXCEPTION);
    Compositor().BeginFrame();
    ASSERT_EQ(expected_root_scroller,
              GetDocument().GetRootScrollerController().EffectiveRootScroller())
        << "Failed to set rootScroller after setting " << std::get<0>(test_case)
        << ": " << std::get<1>(test_case);

    container->style()->setProperty(GetDocument().GetExecutionContext(),
                                    std::get<0>(test_case), String(), String(),
                                    ASSERT_NO_EXCEPTION);
    Compositor().BeginFrame();
    ASSERT_EQ(&GetDocument(),
              GetDocument().GetRootScrollerController().EffectiveRootScroller())
        << "Failed to reset rootScroller after setting "
        << std::get<0>(test_case) << ": " << std::get<1>(test_case);
  }
}

// Test that adding overflow to an element that would otherwise be eligable to
// be implicitly pomoted causes promotion.
TEST_F(ImplicitRootScrollerSimTest, ImplicitRootScrollerAddOverflow) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            #container {
              width: 100%;
              height: 100%;
              overflow: auto;
            }
          </style>
          <div id="container">
            <div id="spacer"></div>
          </div>
      )HTML");
  Compositor().BeginFrame();

  ASSERT_EQ(&GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "Shouldn't promote 'container' since it has no overflow.";

  Element* spacer = GetDocument().getElementById(AtomicString("spacer"));
  spacer->style()->setProperty(GetDocument().GetExecutionContext(), "height",
                               "2000px", String(), ASSERT_NO_EXCEPTION);
  spacer->style()->setProperty(GetDocument().GetExecutionContext(), "width",
                               "2000px", String(), ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  Element* container = GetDocument().getElementById(AtomicString("container"));
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "Adding overflow should cause 'container' to be promoted.";
}

// Tests that we don't crash if an implicit candidate is no longer a box. This
// test passes if it doesn't crash.
TEST_F(ImplicitRootScrollerSimTest, CandidateLosesLayoutBoxDontCrash) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            #spacer {
              width: 300px;
              height: 300px;
            }

            .box {
              width: 200px;
              height: 200px;
              overflow: scroll;
              display: block;
            }

            .nonbox {
              display: inline;
            }
          </style>
          <b id="container">
            <div id="spacer"></div>
          </b>
      )HTML");
  Element* container = GetDocument().getElementById(AtomicString("container"));

  // An overflowing box will be added to the implicit candidates list.
  container->setAttribute(html_names::kClassAttr, AtomicString("box"));
  Compositor().BeginFrame();

  // This will make change from a box to an inline. Ensure we don't crash when
  // we reevaluate the candidates list.
  container->setAttribute(html_names::kClassAttr, AtomicString("nonbox"));
  Compositor().BeginFrame();
}

// Ensure that a plugin view being considered for implicit promotion doesn't
// cause a crash. https://crbug.com/903440.
TEST_F(ImplicitRootScrollerSimTest, ConsiderEmbedCrash) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <embed id="embed" height="1" src="data:video/mp4,">
          <script>
           embed.type = "JavaScript 1.5";
           embed.src = "x";
          </script>
      )HTML");
  Compositor().BeginFrame();
  Element* embed = GetDocument().getElementById(AtomicString("embed"));
  GetDocument().GetRootScrollerController().ConsiderForImplicit(*embed);
}

// Test that a valid implicit root scroller wont be promoted/will be demoted if
// the main document has overflow.
TEST_F(ImplicitRootScrollerSimTest,
       ImplicitRootScrollerDocumentScrollsOverflow) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            #container {
              width: 100%;
              height: 100%;
              overflow: auto;
            }
            #spacer {
              width: 2000px;
              height: 2000px;
            }
          </style>
          <div id="container">
            <div id="spacer"></div>
          </div>
          <div id="overflow"></div>
      )HTML");
  Compositor().BeginFrame();

  Element* container = GetDocument().getElementById(AtomicString("container"));
  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  Element* overflow = GetDocument().getElementById(AtomicString("overflow"));
  overflow->style()->setProperty(GetDocument().GetExecutionContext(), "height",
                                 "10px", String(), ASSERT_NO_EXCEPTION);
  overflow->style()->setProperty(GetDocument().GetExecutionContext(), "width",
                                 "10px", String(), ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(&GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "Adding overflow to document should cause 'container' to be demoted.";

  overflow->remove();
  Compositor().BeginFrame();
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "Removing document overflow should cause 'container' to be promoted.";
}

// Test that we'll only implicitly promote an element if its visible.
TEST_F(ImplicitRootScrollerSimTest, ImplicitRootScrollerVisibilityCondition) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            #container {
              width: 100%;
              height: 100%;
              overflow: auto;
            }
            #spacer {
              width: 2000px;
              height: 2000px;
            }
          </style>
          <div id="container">
            <div id="spacer"></div>
          </div>
      )HTML");
  Compositor().BeginFrame();

  Element* container = GetDocument().getElementById(AtomicString("container"));
  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  container->style()->setProperty(GetDocument().GetExecutionContext(),
                                  "opacity", "0.5", String(),
                                  ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(&GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "Adding opacity to 'container' causes it to be demoted.";

  container->style()->setProperty(GetDocument().GetExecutionContext(),
                                  "opacity", "", String(), ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "Removing opacity from 'container' causes it to be promoted.";

  container->style()->setProperty(GetDocument().GetExecutionContext(),
                                  "visibility", "hidden", String(),
                                  ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(&GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "visibility:hidden causes 'container' to be demoted.";

  container->style()->setProperty(GetDocument().GetExecutionContext(),
                                  "visibility", "collapse", String(),
                                  ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(&GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "visibility:collapse doesn't cause 'container' to be promoted.";

  container->style()->setProperty(GetDocument().GetExecutionContext(),
                                  "visibility", "visible", String(),
                                  ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "visibility:visible causes promotion";
}

// Tests implicit root scroller mode for iframes.
TEST_F(ImplicitRootScrollerSimTest, ImplicitRootScrollerIframe) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            iframe {
              width: 100%;
              height: 100%;
              border: 0;
            }
          </style>
          <iframe id="container"
                  srcdoc="<!DOCTYPE html><style>html {height: 300%;}</style>">
          </iframe>
      )HTML");
  // srcdoc iframe loads via posted tasks.
  RunPendingTasks();
  Compositor().BeginFrame();

  Element* container = GetDocument().getElementById(AtomicString("container"));
  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  container->style()->setProperty(GetDocument().GetExecutionContext(), "height",
                                  "95%", String(), ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();

  ASSERT_EQ(&GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());
}

// Tests use counter for implicit root scroller. Ensure it's not counted on a
// page without an implicit root scroller.
TEST_F(ImplicitRootScrollerSimTest, UseCounterNegative) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            div {
              width: 100%;
              height: 100%;
            }
          </style>
          <div id="container"></div>
      )HTML");
  Compositor().BeginFrame();

  Element* container = GetDocument().getElementById(AtomicString("container"));
  ASSERT_NE(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kActivatedImplicitRootScroller));

  container->style()->setProperty(GetDocument().GetExecutionContext(), "height",
                                  "150%", String(), ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();

  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kActivatedImplicitRootScroller));
}

// Tests use counter for implicit root scroller. Ensure it's counted on a
// page that loads with an implicit root scroller.
TEST_F(ImplicitRootScrollerSimTest, UseCounterPositive) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            #container {
              width: 100%;
              height: 100%;
              overflow: auto;
            }
            #spacer {
              height: 2000px;
            }
          </style>
          <div id="container">
            <div id="spacer"></div>
          </div>
      )HTML");
  Compositor().BeginFrame();

  Element* container = GetDocument().getElementById(AtomicString("container"));
  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kActivatedImplicitRootScroller));

  container->style()->setProperty(GetDocument().GetExecutionContext(), "height",
                                  "150%", String(), ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();

  ASSERT_NE(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kActivatedImplicitRootScroller));
}

// Tests use counter for implicit root scroller. Ensure it's counted on a
// page that loads without an implicit root scroller but later gets one.
TEST_F(ImplicitRootScrollerSimTest, UseCounterPositiveAfterLoad) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            #container {
              width: 100%;
              height: 40%;
              overflow: auto;
            }
            #spacer {
              height: 2000px;
            }
          </style>
          <div id="container">
            <div id="spacer"></div>
          </div>
      )HTML");
  Compositor().BeginFrame();

  Element* container = GetDocument().getElementById(AtomicString("container"));
  ASSERT_NE(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kActivatedImplicitRootScroller));

  container->style()->setProperty(GetDocument().GetExecutionContext(), "height",
                                  "100%", String(), ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();

  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kActivatedImplicitRootScroller));
}

// Test that we correctly recompute the cached bits and thus the root scroller
// properties in the event of a layout tree reattachment which causes the
// LayoutObject to be disposed and replaced with a new one.
TEST_F(ImplicitRootScrollerSimTest, LayoutTreeReplaced) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <style>
            ::-webkit-scrollbar {
            }
            #rootscroller {
              width: 100%;
              height: 100%;
              overflow: auto;
              position: absolute;
              left: 0;
              top: 0;
            }
            #spacer {
              height: 20000px;
              width: 10px;
            }
          </style>
          <div id="rootscroller">
            <div id="spacer"></div>
          </div>
      )HTML");
  Compositor().BeginFrame();

  Element* scroller =
      GetDocument().getElementById(AtomicString("rootscroller"));
  ASSERT_EQ(scroller,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());
  ASSERT_TRUE(scroller->GetLayoutObject()->IsEffectiveRootScroller());
  ASSERT_TRUE(scroller->GetLayoutObject()->IsGlobalRootScroller());

  // This will cause the layout tree to be rebuilt and reattached which creates
  // new LayoutObjects. Ensure the bits are reapplied to the new layout
  // objects after they're recreated.
  GetDocument().setDesignMode("on");
  Compositor().BeginFrame();

  EXPECT_TRUE(scroller->GetLayoutObject()->IsEffectiveRootScroller());
  EXPECT_TRUE(scroller->GetLayoutObject()->IsGlobalRootScroller());
}

// Tests that if we have multiple valid candidates for implicit promotion, we
// don't promote either.
TEST_F(ImplicitRootScrollerSimTest, DontPromoteWhenMultipleAreValid) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            iframe {
              position: absolute;
              left: 0;
              top: 0;
              width: 100%;
              height: 100%;
              border: 0;
            }
          </style>
          <iframe id="container"
                  srcdoc="<!DOCTYPE html><style>html {height: 300%;}</style>">
          </iframe>
          <iframe id="container2"
                  srcdoc="<!DOCTYPE html><style>html {height: 300%;}</style>">
          </iframe>
      )HTML");
  // srcdoc iframe loads via posted tasks.
  RunPendingTasks();
  Compositor().BeginFrame();

  // Since both iframes are valid candidates, neither should be promoted.
  ASSERT_EQ(&GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // Now make the second one invalid, that should cause the first to be
  // promoted.
  Element* container2 =
      GetDocument().getElementById(AtomicString("container2"));
  container2->style()->setProperty(GetDocument().GetExecutionContext(),
                                   "height", "95%", String(),
                                   ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();

  Element* container = GetDocument().getElementById(AtomicString("container"));
  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());
}

// Test that when a valid iframe becomes loaded and thus should be promoted, it
// becomes the root scroller, without needing an intervening layout.
TEST_F(ImplicitRootScrollerSimTest, IframeLoadedWithoutLayout) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_request("https://example.com/test.html", "text/html");
  SimRequest child_request("https://example.com/child.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            iframe {
              width: 100%;
              height: 100%;
              border: 0;
            }
          </style>
          <iframe id="container" src="child.html">
          </iframe>
      )HTML");
  Compositor().BeginFrame();
  ASSERT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "The iframe isn't yet scrollable.";

  // Ensure that it gets promoted when the new FrameView is connected even
  // though there's no layout in the parent to trigger it.
  child_request.Complete(R"HTML(
        <!DOCTYPE html>
        <style>
          body {
            height: 1000px;
          }
        </style>
  )HTML");

  Compositor().BeginFrame();
  EXPECT_EQ(GetDocument().getElementById(AtomicString("container")),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "Once loaded, the iframe should be promoted.";
}

// Ensure that navigating an iframe while it is the effective root scroller,
// causes it to remain the effective root scroller after the navigation (to a
// page where it remains valid) is finished.
TEST_F(ImplicitRootScrollerSimTest, NavigateToValidRemainsRootScroller) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_request("https://example.com/test.html", "text/html");
  SimRequest child_request("https://example.com/child.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            iframe {
              width: 100%;
              height: 100%;
              border: 0;
            }
          </style>
          <iframe id="container" src="child.html">
          </iframe>
      )HTML");
  child_request.Complete(R"HTML(
        <!DOCTYPE html>
        <style>
          body {
            height: 1000px;
          }
        </style>
  )HTML");
  Compositor().BeginFrame();
  ASSERT_EQ(GetDocument().getElementById(AtomicString("container")),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // Navigate the child frame. When it's loaded, the FrameView should swap.
  // Ensure that we remain the root scroller even though there's no layout in
  // the parent.
  SimRequest child_request2("https://example.com/child-next.html", "text/html");
  frame_test_helpers::LoadFrameDontWait(
      WebView().MainFrameImpl()->FirstChild()->ToWebLocalFrame(),
      KURL("https://example.com/child-next.html"));

  child_request2.Write(R"HTML(
        <!DOCTYPE html>
  )HTML");
  Compositor().BeginFrame();
  EXPECT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "The iframe should be demoted once a navigation is committed";

  // Ensure that it gets promoted when the new FrameView is connected even
  // though there's no layout in the parent to trigger it.
  child_request2.Write(R"HTML(
        <style>
          body {
            height: 2000px;
          }
        </style>
  )HTML");
  child_request2.Finish();
  Compositor().BeginFrame();

  EXPECT_EQ(GetDocument().getElementById(AtomicString("container")),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "Once loaded, the iframe should be promoted again.";
}

// Ensure that scroll restoration logic in the document does not apply
// to the implicit root scroller, but rather to the document's LayoutViewport.
TEST_F(ImplicitRootScrollerSimTest, ScrollRestorationIgnoresImplicit) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_request("https://example.com/test.html", "text/html");
  SimRequest child_request("https://example.com/child.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            iframe {
              width: 100%;
              height: 100%;
              border: 0;
            }
          </style>
          <iframe id="container" src="child.html">
          </iframe>
      )HTML");
  child_request.Complete(R"HTML(
        <!DOCTYPE html>
        <style>
          body {
            height: 1000px;
          }
        </style>
  )HTML");
  Compositor().BeginFrame();
  ASSERT_EQ(GetDocument().getElementById(AtomicString("container")),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  HistoryItem::ViewState view_state;
  view_state.scroll_offset_ = ScrollOffset(10, 20);

  GetDocument()
      .View()
      ->GetScrollableArea()
      ->SetPendingHistoryRestoreScrollOffset(
          view_state, true, mojom::blink::ScrollBehavior::kAuto);
  GetDocument().View()->LayoutViewport()->SetPendingHistoryRestoreScrollOffset(
      view_state, true, mojom::blink::ScrollBehavior::kAuto);
  GetDocument().View()->ScheduleAnimation();

  Compositor().BeginFrame();
  EXPECT_EQ(ScrollOffset(0, 0),
            GetDocument().View()->GetScrollableArea()->GetScrollOffset());

  GetDocument().domWindow()->scrollTo(0, 20);
  GetDocument().View()->ScheduleAnimation();
  // Check that an implicit scroll offset is not saved.
  // TODO(chrishtr): probably it should?
  Compositor().BeginFrame();
  EXPECT_FALSE(GetDocument()
                   .GetFrame()
                   ->Loader()
                   .GetDocumentLoader()
                   ->GetHistoryItem()
                   ->GetViewState());
}

// Test that a root scroller is considered to fill the viewport at both the URL
// bar shown and URL bar hidden height.
TEST_F(ImplicitRootScrollerSimTest,
       RootScrollerFillsViewportAtBothURLBarStates) {
  WebView().ResizeWithBrowserControls(gfx::Size(800, 600), 50, 0, true);
  SimRequest main_request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            #container {
              width: 100%;
              height: 100%;
              overflow: auto;
              border: 0;
            }
          </style>
          <div id="container">
            <div style="height: 2000px;"></div>
          </div>
          <script>
            onresize = () => {
              document.getElementById("container").style.height =
                  window.innerHeight + "px";
            };
          </script>
      )HTML");
  Element* container = GetDocument().getElementById(AtomicString("container"));
  Compositor().BeginFrame();

  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // Simulate hiding the top controls. The root scroller should remain valid at
  // the new height.
  WebView().GetPage()->GetBrowserControls().SetShownRatio(0, 0);
  WebView().ResizeWithBrowserControls(gfx::Size(800, 650), 50, 50, false);
  Compositor().BeginFrame();
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // Simulate showing the top controls. The root scroller should remain valid.
  WebView().GetPage()->GetBrowserControls().SetShownRatio(1, 1);
  WebView().ResizeWithBrowserControls(gfx::Size(800, 600), 50, 50, true);
  Compositor().BeginFrame();
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // Set the height explicitly to a new value in-between. The root scroller
  // should be demoted.
  container->style()->setProperty(GetDocument().GetExecutionContext(), "height",
                                  "601px", String(), ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // Reset back to valid and hide the top controls. Zoom to 2x. Ensure we're
  // still considered valid.
  container->style()->setProperty(GetDocument().GetExecutionContext(), "height",
                                  "", String(), ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());
  EXPECT_EQ(To<LayoutBox>(container->GetLayoutObject())->Size().height, 600);
  WebView().MainFrameWidget()->SetZoomLevel(ZoomFactorToZoomLevel(2.0));
  WebView().GetPage()->GetBrowserControls().SetShownRatio(0, 0);
  WebView().ResizeWithBrowserControls(gfx::Size(800, 650), 50, 50, false);
  Compositor().BeginFrame();
  EXPECT_EQ(container->clientHeight(), 325);
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());
}

// Tests that implicit is continually reevaluating whether to promote or demote
// a scroller.
TEST_F(ImplicitRootScrollerSimTest, ContinuallyReevaluateImplicitPromotion) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            html {
              overflow: hidden;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            #container {
              width: 100%;
              height: 100%;
            }
            #parent {
              width: 100%;
              height: 100%;
            }
          </style>
          <div id="parent">
            <div id="container">
              <div id="spacer"></div>
            </div>
          </div>
      )HTML");
  Compositor().BeginFrame();

  Element* parent = GetDocument().getElementById(AtomicString("parent"));
  Element* container = GetDocument().getElementById(AtomicString("container"));
  Element* spacer = GetDocument().getElementById(AtomicString("spacer"));

  // The container isn't yet scrollable.
  ASSERT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // The container now has overflow but still doesn't scroll.
  spacer->style()->setProperty(GetDocument().GetExecutionContext(), "height",
                               "2000px", String(), ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // The container is now scrollable and should be promoted.
  container->style()->setProperty(GetDocument().GetExecutionContext(),
                                  "overflow", "auto", String(),
                                  ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // The container is now not viewport-filling so it should be demoted.
  container->style()->setProperty(GetDocument().GetExecutionContext(),
                                  "transform", "translateX(-50px)", String(),
                                  ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // The container is viewport-filling again so it should be promoted.
  parent->style()->setProperty(GetDocument().GetExecutionContext(), "transform",
                               "translateX(50px)", String(),
                               ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // No longer scrollable so demote.
  container->style()->setProperty(GetDocument().GetExecutionContext(),
                                  "overflow", "hidden", String(),
                                  ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());
}

// Tests that implicit mode correctly recognizes when an iframe becomes
// scrollable.
TEST_F(ImplicitRootScrollerSimTest, IframeScrollingAffectsPromotion) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            iframe {
              width: 100%;
              height: 100%;
              border: 0;
            }
          </style>
          <iframe id="container"
                  srcdoc="<!DOCTYPE html><style>html {overflow: hidden; height: 300%;}</style>">
          </iframe>
      )HTML");

  // srcdoc iframe loads via posted tasks.
  RunPendingTasks();
  Compositor().BeginFrame();

  auto* container = To<HTMLFrameOwnerElement>(
      GetDocument().getElementById(AtomicString("container")));
  Element* inner_html_element = container->contentDocument()->documentElement();

  // Shouldn't be promoted since it's not scrollable.
  EXPECT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // Allows scrolling now so promote.
  inner_html_element->style()->setProperty(
      To<LocalDOMWindow>(container->contentWindow()), "overflow", "auto",
      String(), ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // Demote again.
  inner_html_element->style()->setProperty(
      To<LocalDOMWindow>(container->contentWindow()), "overflow", "hidden",
      String(), ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());
}

// Loads with a larger than the ICB (but otherwise valid) implicit root
// scrolling iframe. When the iframe is promoted (which happens at the end of
// layout) its layout size is changed which makes it easy to violate lifecycle
// assumptions.  (e.g. NeedsLayout at the end of layout)
TEST_F(ImplicitRootScrollerSimTest, PromotionChangesLayoutSize) {
  WebView().ResizeWithBrowserControls(gfx::Size(800, 650), 50, 0, false);
  SimRequest main_request("https://example.com/test.html", "text/html");
  SimRequest child_request("https://example.com/child.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            iframe {
              width: 100%;
              height: 650px;
              border: 0;
            }
          </style>
          <iframe id="container" src="child.html">
          </iframe>
      )HTML");
  child_request.Complete(R"HTML(
        <!DOCTYPE html>
        <style>
          body {
            height: 1000px;
          }
        </style>
  )HTML");

  Compositor().BeginFrame();
  EXPECT_EQ(GetDocument().getElementById(AtomicString("container")),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "Once loaded, the iframe should be promoted.";
}

// Tests that bottom-fixed objects inside of an iframe root scroller and frame
// are marked as being affected by top controls movement. Those inside a
// non-rootScroller iframe should not be marked as such.
TEST_F(ImplicitRootScrollerSimTest, BottomFixedAffectedByTopControls) {
  WebView().ResizeWithBrowserControls(gfx::Size(800, 650), 50, 0, false);
  SimRequest main_request("https://example.com/test.html", "text/html");
  SimRequest child_request1("https://example.com/child1.html", "text/html");
  SimRequest child_request2("https://example.com/child2.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            #container1 {
              width: 100%;
              height: 100%;
              border: 0;
            }
            #container2 {
              position: absolute;
              width: 10px;
              height: 10px;
              left: 100px;
              top: 100px;
              border: 0;
            }
            #fixed {
              position: fixed;
              bottom: 10px;
              left: 10px;
              width: 10px;
              height: 10px;
              background-color: red;
            }
          </style>
          <iframe id="container1" src="child1.html">
          </iframe>
          <iframe id="container2" src="child2.html">
          </iframe>
          <div id="fixed"></div>
      )HTML");
  child_request1.Complete(R"HTML(
        <!DOCTYPE html>
        <style>
          body {
            height: 1000px;
          }
          #fixed {
            width: 50px;
            height: 50px;
            position: fixed;
            bottom: 0px;
            left: 0px;
          }
        </style>
        <div id="fixed"></div>
  )HTML");
  child_request2.Complete(R"HTML(
        <!DOCTYPE html>
        <style>
          body {
            height: 1000px;
          }
          #fixed {
            width: 50px;
            height: 50px;
            position: fixed;
            bottom: 0px;
            left: 0px;
          }
        </style>
        <div id="fixed"></div>
  )HTML");

  Compositor().BeginFrame();

  Element* container1 =
      GetDocument().getElementById(AtomicString("container1"));
  Element* container2 =
      GetDocument().getElementById(AtomicString("container2"));
  ASSERT_EQ(container1,
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "The #container1 iframe must be promoted.";

  Document* child1_document =
      To<HTMLFrameOwnerElement>(container1)->contentDocument();
  Document* child2_document =
      To<HTMLFrameOwnerElement>(container2)->contentDocument();
  LayoutObject* fixed_layout =
      GetDocument().getElementById(AtomicString("fixed"))->GetLayoutObject();
  LayoutObject* fixed_layout1 =
      child1_document->getElementById(AtomicString("fixed"))->GetLayoutObject();
  LayoutObject* fixed_layout2 =
      child2_document->getElementById(AtomicString("fixed"))->GetLayoutObject();

  EXPECT_TRUE(fixed_layout->FirstFragment()
                  .PaintProperties()
                  ->PaintOffsetTranslation()
                  ->IsAffectedByOuterViewportBoundsDelta());
  EXPECT_TRUE(fixed_layout1->FirstFragment()
                  .PaintProperties()
                  ->PaintOffsetTranslation()
                  ->IsAffectedByOuterViewportBoundsDelta());
  EXPECT_FALSE(fixed_layout2->FirstFragment()
                   .PaintProperties()
                   ->PaintOffsetTranslation()
                   ->IsAffectedByOuterViewportBoundsDelta());
}

// Ensure that we're using the content box for an iframe. Promotion will cause
// the content to use the layout size of the parent frame so having padding or
// a border would cause us to relayout.
TEST_F(ImplicitRootScrollerSimTest, IframeUsesContentBox) {
  WebView().ResizeWithBrowserControls(gfx::Size(800, 600), 0, 0, false);
  SimRequest main_request("https://example.com/test.html", "text/html");
  SimRequest child_request("https://example.com/child.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_request.Complete(R"HTML(
          <!DOCTYPE>
          <style>
            iframe {
              position: absolute;
              top: 0;
              left: 0;
              width: 100%;
              height: 100%;
              border: none;
              box-sizing: border-box;

            }
            body, html {
              margin: 0;
              width: 100%;
              height: 100%;
              overflow:hidden;
            }

          </style>
          <iframe id="container" src="child.html">
      )HTML");
  child_request.Complete(R"HTML(
        <!DOCTYPE html>
        <style>
          div {
            border: 5px solid black;
            background-color: red;
            width: 99%;
            height: 100px;
          }
          html {
            height: 200%;
          }
        </style>
        <div></div>
  )HTML");

  Compositor().BeginFrame();

  Element* iframe = GetDocument().getElementById(AtomicString("container"));

  ASSERT_EQ(iframe,
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "The iframe should start off promoted.";

  // Adding padding should cause the iframe to be demoted.
  {
    iframe->setAttribute(html_names::kStyleAttr,
                         AtomicString("padding-left: 20%"));
    Compositor().BeginFrame();

    EXPECT_NE(iframe,
              GetDocument().GetRootScrollerController().EffectiveRootScroller())
        << "The iframe should be demoted once it has padding.";
  }

  // Replacing padding with a border should also ensure the iframe remains
  // demoted.
  {
    iframe->setAttribute(html_names::kStyleAttr,
                         AtomicString("border: 5px solid black"));
    Compositor().BeginFrame();

    EXPECT_NE(iframe,
              GetDocument().GetRootScrollerController().EffectiveRootScroller())
        << "The iframe should be demoted once it has border.";
  }

  // Removing the border should now cause the iframe to be promoted once again.
  iframe->setAttribute(html_names::kStyleAttr, g_empty_atom);
  Compositor().BeginFrame();

  ASSERT_EQ(iframe,
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "The iframe should once again be promoted when border is removed";
}

// Test that we don't promote any elements implicitly if the main document has
// vertical scrolling.
TEST_F(ImplicitRootScrollerSimTest, OverflowInMainDocumentRestrictsImplicit) {
  WebView().ResizeWithBrowserControls(gfx::Size(800, 600), 50, 0, true);
  SimRequest main_request("https://example.com/test.html", "text/html");
  SimRequest child_request("https://example.com/child.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            iframe {
              width: 100%;
              height: 100%;
              border: 0;
            }
            div {
              position: absolute;
              left: 0;
              top: 0;
              height: 150%;
              width: 150%;
            }
          </style>
          <iframe id="container" src="child.html">
          </iframe>
          <div id="spacer"></div>
      )HTML");
  child_request.Complete(R"HTML(
        <!DOCTYPE html>
        <style>
          body {
            height: 1000px;
          }
        </style>
  )HTML");

  Compositor().BeginFrame();
  EXPECT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "iframe shouldn't be promoted due to overflow in the main document.";

  Element* spacer = GetDocument().getElementById(AtomicString("spacer"));
  spacer->style()->setProperty(GetDocument().GetExecutionContext(), "height",
                               "100%", String(), ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();

  EXPECT_EQ(GetDocument().getElementById(AtomicString("container")),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "Once vertical overflow is removed, the iframe should be promoted.";
}

// Test that we overflow in the document allows promotion only so long as the
// document isn't scrollable.
TEST_F(ImplicitRootScrollerSimTest, OverflowHiddenDoesntRestrictImplicit) {
  WebView().ResizeWithBrowserControls(gfx::Size(800, 600), 50, 0, true);
  SimRequest main_request("https://example.com/test.html", "text/html");
  SimRequest child_request("https://example.com/child.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            html {
              overflow: hidden;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            iframe {
              width: 100%;
              height: 100%;
              border: 0;
            }
            #spacer {
              position: absolute;
              left: 0;
              top: 0;
              height: 150%;
              width: 150%;
            }
          </style>
          <iframe id="container" src="child.html">
          </iframe>
          <div id="spacer"></div>
      )HTML");
  child_request.Complete(R"HTML(
        <!DOCTYPE html>
        <style>
          body {
            height: 1000px;
          }
        </style>
  )HTML");

  Compositor().BeginFrame();
  EXPECT_EQ(GetDocument().getElementById(AtomicString("container")),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "iframe should be promoted since document's overflow is hidden.";

  Element* html = GetDocument().documentElement();
  html->style()->setProperty(GetDocument().GetExecutionContext(), "overflow",
                             "auto", String(), ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();

  EXPECT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "iframe should now be demoted since main document scrolls overflow.";

  html->style()->setProperty(GetDocument().GetExecutionContext(), "overflow",
                             "visible", String(), ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();

  EXPECT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "iframe should remain demoted since overflow:visible on document "
      << "allows scrolling.";
}

// Test that any non-document, clipping ancestor prevents implicit promotion.
TEST_F(ImplicitRootScrollerSimTest, ClippingAncestorPreventsPromotion) {
  WebView().ResizeWithBrowserControls(gfx::Size(800, 600), 50, 0, true);
  SimRequest main_request("https://example.com/test.html", "text/html");
  SimRequest child_request("https://example.com/child.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            html {
              overflow: hidden;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            iframe {
              width: 100%;
              height: 100%;
              border: 0;
            }
            #ancestor {
              position: absolute;
              width: 100%;
              height: 100%;
              overflow: visible;
              /* opacity ensures #ancestor doesn't get considered for root
               * scroller promotion. */
              opacity: 0.5;
            }
            #spacer {
              height: 150%;
              width: 150%;
            }
          </style>
          <div id="ancestor">
            <iframe id="container" src="child.html"></iframe>
            <div id="spacer"></div>
          </div>
      )HTML");
  child_request.Complete(R"HTML(
        <!DOCTYPE html>
        <style>
          body {
            height: 1000px;
          }
        </style>
  )HTML");
  Compositor().BeginFrame();

  // Each of these style-value pairs should prevent promotion of the iframe.
  Vector<std::tuple<String, String>> test_cases = {
      {"overflow", "scroll"},
      {"overflow", "hidden"},
      {"overflow", "auto"},
      {"contain", "paint"},
      {"-webkit-mask-image", "linear-gradient(black 25%, transparent 50%)"},
      {"clip", "rect(10px, 290px, 190px, 10px"},
      {"clip-path", "circle(40%)"}};

  for (auto test_case : test_cases) {
    String& style = std::get<0>(test_case);
    String& style_val = std::get<1>(test_case);
    Element* ancestor = GetDocument().getElementById(AtomicString("ancestor"));
    Element* iframe = GetDocument().getElementById(AtomicString("container"));

    ASSERT_EQ(iframe,
              GetDocument().GetRootScrollerController().EffectiveRootScroller())
        << "iframe should start off promoted.";

    ancestor->style()->setProperty(GetDocument().GetExecutionContext(), style,
                                   style_val, String(), ASSERT_NO_EXCEPTION);
    Compositor().BeginFrame();

    EXPECT_EQ(GetDocument(),
              GetDocument().GetRootScrollerController().EffectiveRootScroller())
        << "iframe should be demoted since ancestor has " << style << ": "
        << style_val;

    ancestor->style()->setProperty(GetDocument().GetExecutionContext(), style,
                                   String(), String(), ASSERT_NO_EXCEPTION);
    Compositor().BeginFrame();
    ASSERT_EQ(iframe,
              GetDocument().GetRootScrollerController().EffectiveRootScroller())
        << "iframe should be promoted since ancestor removed " << style << ": "
        << style_val;
  }
}

TEST_F(ImplicitRootScrollerSimTest, AppliedAtFractionalZoom) {
  // Matches Pixel 2XL screen size of 412x671 at 3.5 DevicePixelRatio.
  WebView().SetZoomFactorForDeviceScaleFactor(3.5f);
  WebView().ResizeWithBrowserControls(gfx::Size(1442, 2349), 196, 0, true);

  SimRequest main_request("https://example.com/test.html", "text/html");
  SimRequest child_request("https://example.com/child.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            iframe {
              border: 0;
              display: block;
            }
          </style>
          <iframe id="container" src="child.html">
          </iframe>
          <script>
            // innerHeight is non-fractional so pages don't have a great way to
            // set the size to "exctly" 100%. Ensure we still promote in this
            // common pattern.
            function resize_handler() {
              document.getElementById("container").style.height =
                  window.innerHeight + "px";
              document.getElementById("container").style.width =
                  window.innerWidth + "px";
            }

            resize_handler();
            window.addEventHandler('resize', resize_handler);
          </script>
      )HTML");

  child_request.Complete(R"HTML(
        <!DOCTYPE html>
        <style>
          body {
            height: 1000px;
          }
        </style>
  )HTML");

  Compositor().BeginFrame();
  PaintLayerScrollableArea* area = GetDocument().View()->LayoutViewport();
  ASSERT_FALSE(area->HasVerticalOverflow());

  EXPECT_EQ(GetDocument().getElementById(AtomicString("container")),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "<iframe> should be promoted when URL bar is hidden";

  WebView().ResizeWithBrowserControls(gfx::Size(1442, 2545), 196, 0, false);
  Compositor().BeginFrame();

  EXPECT_EQ(GetDocument().getElementById(AtomicString("container")),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "<iframe> should remain promoted when URL bar is hidden";
}

// Ensure that a scrollable fieldset doesn't get promoted to root scroller.
// With FieldsetNG, a scrollable fieldset creates an anonymous LayoutBox that
// doesn't have an associated Node. RootScroller is premised on the fact that a
// scroller is associated with a Node. It'd be non-trivial work to make this
// work without a clear benefit so for now ensure it doesn't get promoted and
// doesn't cause any crashes. https://crbug.com/1125621.
TEST_F(ImplicitRootScrollerSimTest, FieldsetNGCantBeRootScroller) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            ::-webkit-scrollbar {
              width: 0px;
              height: 0px;
            }
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            fieldset {
              width: 100%;
              height: 100%;
              overflow: scroll;
              border: 0;
              margin: 0;
              padding: 0;
            }
            div {
              height: 200%;
            }
          </style>
          <fieldset>
            <div></div>
          </fieldset>
      )HTML");
  Compositor().BeginFrame();

  EXPECT_TRUE(GetDocument().GetLayoutView()->IsEffectiveRootScroller());
}

class RootScrollerHitTest : public ImplicitRootScrollerSimTest {
 public:
  void CheckHitTestAtBottomOfScreen(Element* target) {
    HideTopControlsWithMaximalScroll();

    // Do a hit test at the very bottom of the screen. This should be outside
    // the root scroller's LayoutBox since inert top controls won't resize the
    // ICB but, since we expaned the clip, we should still be able to hit the
    // target.
    gfx::Point point(200, 445);
    gfx::Size tap_area(20, 20);
    WebHitTestResult result = WebView().HitTestResultForTap(point, tap_area);

    Node* hit_node = result.GetNode().Unwrap<Node>();
    EXPECT_EQ(target, hit_node);
  }

  BrowserControls& GetBrowserControls() {
    return GetDocument().GetPage()->GetBrowserControls();
  }

 private:
  void HideTopControlsWithMaximalScroll() {
    // Do a scroll gesture that hides the top controls and scrolls all the way
    // to the bottom.
    ASSERT_EQ(1, GetBrowserControls().TopShownRatio());
    ASSERT_EQ(1, GetBrowserControls().BottomShownRatio());
    WebView().MainFrameWidget()->ApplyViewportChangesForTesting(
        {gfx::Vector2dF(), gfx::Vector2dF(), 1, false, -1, -1,
         cc::BrowserControlsState::kBoth});
    ASSERT_EQ(0, GetBrowserControls().TopShownRatio());
    ASSERT_EQ(0, GetBrowserControls().BottomShownRatio());

    Node* scroller = GetDocument()
                         .GetPage()
                         ->GlobalRootScrollerController()
                         .GlobalRootScroller();
    ScrollableArea* scrollable_area =
        To<LayoutBox>(scroller->GetLayoutObject())->GetScrollableArea();
    scrollable_area->DidCompositorScroll(gfx::PointF(0, 100000));

    WebView().ResizeWithBrowserControls(gfx::Size(400, 450), 50, 50, false);

    Compositor().BeginFrame();
  }
};

// Test that hit testing in the area revealed at the bottom of the screen
// revealed by hiding the URL bar works properly when using a root scroller
// when the target and scroller are in the same PaintLayer.
TEST_F(RootScrollerHitTest, HitTestInAreaRevealedByURLBarSameLayer) {
  WebView().ResizeWithBrowserControls(gfx::Size(400, 400), 50, 50, true);
  GetBrowserControls().SetShownRatio(1, 1);
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");

  // Add a target at the bottom of the root scroller that's the size of the url
  // bar. We'll test that hiding the URL bar appropriately adjusts clipping so
  // that we can hit this target.
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            body, html {
              width: 100%;
              height: 100%;
              margin: 0px;
            }
            #spacer {
              height: 1000px;
            }
            #container {
              position: absolute;
              width: 100%;
              height: 100%;
              overflow: auto;
            }
            #target {
              width: 100%;
              height: 50px;
            }
          </style>
          <div id='container'>
            <div id='spacer'></div>
            <div id='target'></div>
          </div>
      )HTML");

  Compositor().BeginFrame();
  Element* container = GetDocument().getElementById(AtomicString("container"));
  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // This test checks hit testing while the target is in the same PaintLayer as
  // the root scroller.
  ASSERT_EQ(To<LayoutBox>(target->GetLayoutObject())->EnclosingLayer(),
            To<LayoutBox>(container->GetLayoutObject())->Layer());

  CheckHitTestAtBottomOfScreen(target);
}

// Test that hit testing in the area revealed at the bottom of the screen
// revealed by hiding the URL bar works properly when using a root scroller
// when the target and scroller are in different PaintLayers.
TEST_F(RootScrollerHitTest, HitTestInAreaRevealedByURLBarDifferentLayer) {
  WebView().ResizeWithBrowserControls(gfx::Size(400, 400), 50, 50, true);
  GetBrowserControls().SetShownRatio(1, 1);
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");

  // Add a target at the bottom of the root scroller that's the size of the url
  // bar. We'll test that hiding the URL bar appropriately adjusts clipping so
  // that we can hit this target.
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            body, html {
              height: 100%;
              width: 100%;
              margin: 0px;
            }
            #spacer {
              height: 1000px;
            }
            #container {
              position: absolute;
              width: 100%;
              height: 100%;
              overflow: auto;
            }
            #target {
              width: 100%;
              height: 50px;
              will-change: transform;
            }
          </style>
          <div id='container'>
            <div id='spacer'></div>
            <div id='target'></div>
          </div>
      )HTML");

  Compositor().BeginFrame();
  Element* container = GetDocument().getElementById(AtomicString("container"));
  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // Ensure the target and container weren't put into the same layer.
  ASSERT_NE(To<LayoutBox>(target->GetLayoutObject())->EnclosingLayer(),
            To<LayoutBox>(container->GetLayoutObject())->Layer());

  CheckHitTestAtBottomOfScreen(target);
}

}  // namespace

}  // namespace blink
