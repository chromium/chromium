// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/platform/web_coalesced_input_event.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_hit_test_result.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/bindings/core/v8/node_or_string_or_trusted_script.h"
#include "third_party/blink/renderer/core/exported/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/dom_visual_viewport.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using blink::test::RunPendingTasks;
using testing::Mock;

namespace blink {

namespace {

class RootScrollerTest : public testing::Test,
                         private ScopedImplicitRootScrollerForTest,
                         private ScopedSetRootScrollerForTest {
 public:
  RootScrollerTest()
      : ScopedImplicitRootScrollerForTest(false),
        ScopedSetRootScrollerForTest(true),
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

  void SetAndSelectRootScroller(Document& document, Element* element) {
    document.setRootScroller(element, ASSERT_NO_EXCEPTION);
    if (document.GetFrame()) {
      LocalFrameView* root_view = document.GetFrame()->LocalFrameRoot().View();
      if (root_view)
        UpdateAllLifecyclePhases(root_view);
    }
  }

  WebViewImpl* Initialize(const String& page_name,
                          frame_test_helpers::TestWebWidgetClient* client) {
    return InitializeInternal(base_url_ + page_name, client);
  }

  WebViewImpl* Initialize(const String& page_name) {
    return InitializeInternal(base_url_ + page_name, nullptr);
  }

  WebViewImpl* Initialize() {
    return InitializeInternal("about:blank", nullptr);
  }

  static void ConfigureSettings(WebSettings* settings) {
    settings->SetJavaScriptEnabled(true);
    settings->SetPreferCompositingToLCDTextEnabled(true);
    // Android settings.
    settings->SetViewportEnabled(true);
    settings->SetViewportMetaEnabled(true);
    settings->SetShrinksViewportContentToFit(true);
    settings->SetMainFrameResizesAreOrientationChanges(true);
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
        WebWidget::LifecycleUpdateReason::kTest);
    RunPendingTasks();
  }

  WebViewImpl* GetWebView() const { return helper_.GetWebView(); }

  Page& GetPage() const { return *GetWebView()->GetPage(); }

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

  WebCoalescedInputEvent GenerateTouchGestureEvent(WebInputEvent::Type type,
                                                   int delta_x = 0,
                                                   int delta_y = 0) {
    return GenerateGestureEvent(type, WebGestureDevice::kTouchscreen, delta_x,
                                delta_y);
  }

  WebCoalescedInputEvent GenerateWheelGestureEvent(WebInputEvent::Type type,
                                                   int delta_x = 0,
                                                   int delta_y = 0) {
    return GenerateGestureEvent(type, WebGestureDevice::kTouchpad, delta_x,
                                delta_y);
  }

 protected:
  WebCoalescedInputEvent GenerateGestureEvent(WebInputEvent::Type type,
                                              WebGestureDevice device,
                                              int delta_x,
                                              int delta_y) {
    WebGestureEvent event(type, WebInputEvent::kNoModifiers,
                          WebInputEvent::GetStaticTimeStampForTests(), device);
    event.SetPositionInWidget(WebFloatPoint(100, 100));
    if (type == WebInputEvent::kGestureScrollUpdate) {
      event.data.scroll_update.delta_x = delta_x;
      event.data.scroll_update.delta_y = delta_y;
    } else if (type == WebInputEvent::kGestureScrollBegin) {
      event.data.scroll_begin.delta_x_hint = delta_x;
      event.data.scroll_begin.delta_y_hint = delta_y;
    }
    return WebCoalescedInputEvent(event);
  }

  WebViewImpl* InitializeInternal(
      const String& url,
      frame_test_helpers::TestWebWidgetClient* client) {
    helper_.InitializeAndLoad(url.Utf8(), nullptr, nullptr, client,
                              &ConfigureSettings);

    // Initialize browser controls to be shown.
    GetWebView()->ResizeWithBrowserControls(IntSize(400, 400), 50, 60, true);
    GetWebView()->GetBrowserControls().SetShownRatio(1, 1);

    UpdateAllLifecyclePhases(MainFrameView());

    return GetWebView();
  }

  void UpdateAllLifecyclePhases(LocalFrameView* view) {
    view->UpdateAllLifecyclePhases(
        DocumentLifecycle::LifecycleUpdateReason::kTest);
  }

  String base_url_;
  std::unique_ptr<frame_test_helpers::TestWebViewClient> view_client_;
  frame_test_helpers::WebViewHelper helper_;
  RuntimeEnabledFeatures::Backup features_backup_;
};

// Test that no root scroller element is set if setRootScroller isn't called on
// any elements. The document Node should be the default effective root
// scroller.
TEST_F(RootScrollerTest, TestDefaultRootScroller) {
  Initialize("overflow-scrolling.html");

  ASSERT_EQ(nullptr, MainFrame()->GetDocument()->rootScroller());
  EXPECT_EQ(MainFrame()->GetDocument(),
            EffectiveRootScroller(MainFrame()->GetDocument()));
}

// Make sure that replacing the documentElement doesn't change the effective
// root scroller when no root scroller is set.
TEST_F(RootScrollerTest, defaultEffectiveRootScrollerIsDocumentNode) {
  Initialize("root-scroller.html");

  Document* document = MainFrame()->GetDocument();
  Element* iframe = document->CreateRawElement(html_names::kIFrameTag);

  EXPECT_EQ(MainFrame()->GetDocument(),
            EffectiveRootScroller(MainFrame()->GetDocument()));

  // Replace the documentElement with the iframe. The effectiveRootScroller
  // should remain the same.
  HeapVector<NodeOrStringOrTrustedScript> nodes;
  nodes.push_back(NodeOrStringOrTrustedScript::FromNode(iframe));
  document->documentElement()->ReplaceWith(nodes, ASSERT_NO_EXCEPTION);

  UpdateAllLifecyclePhases(MainFrameView());

  EXPECT_EQ(MainFrame()->GetDocument(),
            EffectiveRootScroller(MainFrame()->GetDocument()));
}

class OverscrollTestWebWidgetClient
    : public frame_test_helpers::TestWebWidgetClient {
 public:
  MOCK_METHOD4(DidOverscroll,
               void(const WebFloatSize&,
                    const WebFloatSize&,
                    const WebFloatPoint&,
                    const WebFloatSize&));
};

// Tests that setting an element as the root scroller causes it to control url
// bar hiding and overscroll.
TEST_F(RootScrollerTest, TestSetRootScroller) {
  OverscrollTestWebWidgetClient client;
  Initialize("root-scroller.html", &client);

  Element* container = MainFrame()->GetDocument()->getElementById("container");
  SetAndSelectRootScroller(*MainFrame()->GetDocument(), container);
  ASSERT_EQ(container, MainFrame()->GetDocument()->rootScroller());

  // Content is 1000x1000, WebView size is 400x400 but hiding the top controls
  // makes it 400x450 so max scroll is 550px.
  double maximum_scroll = 550;

  GetWebView()->MainFrameWidget()->HandleInputEvent(
      GenerateTouchGestureEvent(WebInputEvent::kGestureScrollBegin));

  {
    // Scrolling over the #container DIV should cause the browser controls to
    // hide.
    EXPECT_FLOAT_EQ(1, GetBrowserControls().TopShownRatio());
    EXPECT_FLOAT_EQ(1, GetBrowserControls().BottomShownRatio());
    GetWebView()->MainFrameWidget()->HandleInputEvent(
        GenerateTouchGestureEvent(WebInputEvent::kGestureScrollUpdate, 0,
                                  -GetBrowserControls().TopHeight()));
    EXPECT_FLOAT_EQ(0, GetBrowserControls().TopShownRatio());
    EXPECT_FLOAT_EQ(0, GetBrowserControls().BottomShownRatio());
  }

  {
    // Make sure we're actually scrolling the DIV and not the LocalFrameView.
    GetWebView()->MainFrameWidget()->HandleInputEvent(GenerateTouchGestureEvent(
        WebInputEvent::kGestureScrollUpdate, 0, -100));
    EXPECT_FLOAT_EQ(100, container->scrollTop());
    EXPECT_FLOAT_EQ(
        0, MainFrameView()->LayoutViewport()->GetScrollOffset().Height());
  }

  {
    // Scroll 50 pixels past the end. Ensure we report the 50 pixels as
    // overscroll.
    EXPECT_CALL(client, DidOverscroll(WebFloatSize(0, 50), WebFloatSize(0, 50),
                                      WebFloatPoint(100, 100), WebFloatSize()));
    GetWebView()->MainFrameWidget()->HandleInputEvent(GenerateTouchGestureEvent(
        WebInputEvent::kGestureScrollUpdate, 0, -500));
    EXPECT_FLOAT_EQ(maximum_scroll, container->scrollTop());
    EXPECT_FLOAT_EQ(
        0, MainFrameView()->LayoutViewport()->GetScrollOffset().Height());
    Mock::VerifyAndClearExpectations(&client);
  }

  {
    // Continue the gesture overscroll.
    EXPECT_CALL(client, DidOverscroll(WebFloatSize(0, 20), WebFloatSize(0, 70),
                                      WebFloatPoint(100, 100), WebFloatSize()));
    GetWebView()->MainFrameWidget()->HandleInputEvent(
        GenerateTouchGestureEvent(WebInputEvent::kGestureScrollUpdate, 0, -20));
    EXPECT_FLOAT_EQ(maximum_scroll, container->scrollTop());
    EXPECT_FLOAT_EQ(
        0, MainFrameView()->LayoutViewport()->GetScrollOffset().Height());
    Mock::VerifyAndClearExpectations(&client);
  }

  GetWebView()->MainFrameWidget()->HandleInputEvent(
      GenerateTouchGestureEvent(WebInputEvent::kGestureScrollEnd));

  {
    // Make sure a new gesture scroll still won't scroll the frameview and
    // overscrolls.
    GetWebView()->MainFrameWidget()->HandleInputEvent(
        GenerateTouchGestureEvent(WebInputEvent::kGestureScrollBegin));

    EXPECT_CALL(client, DidOverscroll(WebFloatSize(0, 30), WebFloatSize(0, 30),
                                      WebFloatPoint(100, 100), WebFloatSize()));
    GetWebView()->MainFrameWidget()->HandleInputEvent(
        GenerateTouchGestureEvent(WebInputEvent::kGestureScrollUpdate, 0, -30));
    EXPECT_FLOAT_EQ(maximum_scroll, container->scrollTop());
    EXPECT_FLOAT_EQ(
        0, MainFrameView()->LayoutViewport()->GetScrollOffset().Height());
    Mock::VerifyAndClearExpectations(&client);

    GetWebView()->MainFrameWidget()->HandleInputEvent(
        GenerateTouchGestureEvent(WebInputEvent::kGestureScrollEnd));
  }

  {
    // Scrolling up should show the browser controls.
    GetWebView()->MainFrameWidget()->HandleInputEvent(
        GenerateTouchGestureEvent(WebInputEvent::kGestureScrollBegin));

    EXPECT_FLOAT_EQ(0, GetBrowserControls().TopShownRatio());
    EXPECT_FLOAT_EQ(0, GetBrowserControls().BottomShownRatio());
    GetWebView()->MainFrameWidget()->HandleInputEvent(
        GenerateTouchGestureEvent(WebInputEvent::kGestureScrollUpdate, 0, 30));
    EXPECT_FLOAT_EQ(0.6, GetBrowserControls().TopShownRatio());
    EXPECT_FLOAT_EQ(0.6, GetBrowserControls().BottomShownRatio());

    GetWebView()->MainFrameWidget()->HandleInputEvent(
        GenerateTouchGestureEvent(WebInputEvent::kGestureScrollEnd));
  }

  // Reset manually to avoid lifetime issues with custom WebViewClient.
  helper_.Reset();
}

// Tests that removing the element that is the root scroller from the DOM tree
// doesn't remove it as the root scroller but it does change the effective root
// scroller.
TEST_F(RootScrollerTest, TestRemoveRootScrollerFromDom) {
  Initialize("root-scroller.html");

  ASSERT_EQ(nullptr, MainFrame()->GetDocument()->rootScroller());

  Element* container = MainFrame()->GetDocument()->getElementById("container");
  SetAndSelectRootScroller(*MainFrame()->GetDocument(), container);
  UpdateAllLifecyclePhases(MainFrameView());

  EXPECT_EQ(container, MainFrame()->GetDocument()->rootScroller());
  EXPECT_EQ(container, EffectiveRootScroller(MainFrame()->GetDocument()));

  MainFrame()->GetDocument()->body()->RemoveChild(container);
  UpdateAllLifecyclePhases(MainFrameView());

  EXPECT_EQ(container, MainFrame()->GetDocument()->rootScroller());
  EXPECT_NE(container, EffectiveRootScroller(MainFrame()->GetDocument()));
}

// Tests that setting an element that isn't a valid scroller as the root
// scroller doesn't change the effective root scroller.
TEST_F(RootScrollerTest, TestSetRootScrollerOnInvalidElement) {
  Initialize("root-scroller.html");

  {
    // Set to a non-block element. Should be rejected and a console message
    // logged.
    Element* element = MainFrame()->GetDocument()->getElementById("nonBlock");
    SetAndSelectRootScroller(*MainFrame()->GetDocument(), element);
    EXPECT_EQ(element, MainFrame()->GetDocument()->rootScroller());
    EXPECT_NE(element, EffectiveRootScroller(MainFrame()->GetDocument()));
  }

  {
    // Set to an element with no size.
    Element* element = MainFrame()->GetDocument()->getElementById("empty");
    SetAndSelectRootScroller(*MainFrame()->GetDocument(), element);
    EXPECT_EQ(element, MainFrame()->GetDocument()->rootScroller());
    EXPECT_NE(element, EffectiveRootScroller(MainFrame()->GetDocument()));
  }
}

// Test that the effective root scroller resets to the document Node when the
// current root scroller element becomes invalid as a scroller.
TEST_F(RootScrollerTest, TestRootScrollerBecomesInvalid) {
  Initialize("root-scroller.html");

  Element* container = MainFrame()->GetDocument()->getElementById("container");

  ASSERT_EQ(nullptr, MainFrame()->GetDocument()->rootScroller());
  ASSERT_EQ(MainFrame()->GetDocument(),
            EffectiveRootScroller(MainFrame()->GetDocument()));

  {
    SetAndSelectRootScroller(*MainFrame()->GetDocument(), container);

    EXPECT_EQ(container, MainFrame()->GetDocument()->rootScroller());
    EXPECT_EQ(container, EffectiveRootScroller(MainFrame()->GetDocument()));

    ExecuteScript(
        "document.querySelector('#container').style.display = 'inline'");
    UpdateAllLifecyclePhases(MainFrameView());

    EXPECT_EQ(container, MainFrame()->GetDocument()->rootScroller());
    EXPECT_EQ(MainFrame()->GetDocument(),
              EffectiveRootScroller(MainFrame()->GetDocument()));
  }

  ExecuteScript("document.querySelector('#container').style.display = 'block'");
  SetAndSelectRootScroller(*MainFrame()->GetDocument(), nullptr);
  EXPECT_EQ(nullptr, MainFrame()->GetDocument()->rootScroller());
  EXPECT_EQ(MainFrame()->GetDocument(),
            EffectiveRootScroller(MainFrame()->GetDocument()));

  {
    SetAndSelectRootScroller(*MainFrame()->GetDocument(), container);

    EXPECT_EQ(container, MainFrame()->GetDocument()->rootScroller());
    EXPECT_EQ(container, EffectiveRootScroller(MainFrame()->GetDocument()));

    ExecuteScript("document.querySelector('#container').style.width = '98%'");
    UpdateAllLifecyclePhases(MainFrameView());

    EXPECT_EQ(container, MainFrame()->GetDocument()->rootScroller());
    EXPECT_EQ(MainFrame()->GetDocument(),
              EffectiveRootScroller(MainFrame()->GetDocument()));
  }
}

// Tests that setting the root scroller of the top document to an element that
// belongs to a nested document works.
TEST_F(RootScrollerTest, TestSetRootScrollerOnElementInIframe) {
  Initialize("root-scroller-iframe.html");

  ASSERT_EQ(nullptr, MainFrame()->GetDocument()->rootScroller());

  {
    // Trying to set an element from a nested document should fail.
    auto* iframe = To<HTMLFrameOwnerElement>(
        MainFrame()->GetDocument()->getElementById("iframe"));
    Element* inner_container =
        iframe->contentDocument()->getElementById("container");

    SetAndSelectRootScroller(*MainFrame()->GetDocument(), inner_container);

    EXPECT_EQ(inner_container, MainFrame()->GetDocument()->rootScroller());
    EXPECT_EQ(inner_container,
              EffectiveRootScroller(MainFrame()->GetDocument()));
  }

  {
    // Setting the iframe itself should also work.
    auto* iframe = To<HTMLFrameOwnerElement>(
        MainFrame()->GetDocument()->getElementById("iframe"));

    SetAndSelectRootScroller(*MainFrame()->GetDocument(), iframe);

    EXPECT_EQ(iframe, MainFrame()->GetDocument()->rootScroller());
    EXPECT_EQ(iframe, EffectiveRootScroller(MainFrame()->GetDocument()));
  }
}

// Tests that setting a valid element as the root scroller on a document within
// an iframe works as expected.
TEST_F(RootScrollerTest, TestRootScrollerWithinIframe) {
  Initialize("root-scroller-iframe.html");

  ASSERT_EQ(nullptr, MainFrame()->GetDocument()->rootScroller());

  {
    auto* iframe = To<HTMLFrameOwnerElement>(
        MainFrame()->GetDocument()->getElementById("iframe"));

    EXPECT_EQ(iframe->contentDocument(),
              EffectiveRootScroller(iframe->contentDocument()));

    Element* inner_container =
        iframe->contentDocument()->getElementById("container");
    SetAndSelectRootScroller(*iframe->contentDocument(), inner_container);

    EXPECT_EQ(inner_container, iframe->contentDocument()->rootScroller());
    EXPECT_EQ(inner_container,
              EffectiveRootScroller(iframe->contentDocument()));
  }
}

// Tests that setting an iframe as the root scroller makes the iframe the
// effective root scroller in the parent frame.
TEST_F(RootScrollerTest, SetRootScrollerIframeBecomesEffective) {
  Initialize("root-scroller-iframe.html");
  ASSERT_EQ(nullptr, MainFrame()->GetDocument()->rootScroller());

  {
    // Try to set the root scroller in the main frame to be the iframe
    // element.
    auto* iframe = To<HTMLFrameOwnerElement>(
        MainFrame()->GetDocument()->getElementById("iframe"));

    SetAndSelectRootScroller(*MainFrame()->GetDocument(), iframe);

    EXPECT_EQ(iframe, MainFrame()->GetDocument()->rootScroller());
    EXPECT_EQ(iframe, EffectiveRootScroller(MainFrame()->GetDocument()));

    Element* container = iframe->contentDocument()->getElementById("container");

    SetAndSelectRootScroller(*iframe->contentDocument(), container);

    EXPECT_EQ(container, iframe->contentDocument()->rootScroller());
    EXPECT_EQ(container, EffectiveRootScroller(iframe->contentDocument()));
    EXPECT_EQ(iframe, MainFrame()->GetDocument()->rootScroller());
    EXPECT_EQ(iframe, EffectiveRootScroller(MainFrame()->GetDocument()));
  }
}

// Tests that the global root scroller is correctly calculated when getting the
// root scroller layer and that the viewport apply scroll is set on it.
TEST_F(RootScrollerTest, SetRootScrollerIframeUsesCorrectLayerAndCallback) {
  // TODO(bokan): The expectation and actual in the checks here are backwards.
  Initialize("root-scroller-iframe.html");
  ASSERT_EQ(nullptr, MainFrame()->GetDocument()->rootScroller());

  auto* iframe = To<HTMLFrameOwnerElement>(
      MainFrame()->GetDocument()->getElementById("iframe"));
  Element* container = iframe->contentDocument()->getElementById("container");

  const TopDocumentRootScrollerController& main_controller =
      MainFrame()->GetDocument()->GetPage()->GlobalRootScrollerController();

  // No root scroller set, the document node should be the global root and the
  // main LocalFrameView's scroll layer should be the layer to use.
  {
    EXPECT_TRUE(main_controller.IsViewportScrollCallback(
        MainFrame()->GetDocument()->GetApplyScroll()));
  }

  // Set a root scroller in the iframe. Since the main document didn't set a
  // root scroller, the global root scroller shouldn't change.
  {
    SetAndSelectRootScroller(*iframe->contentDocument(), container);

    EXPECT_TRUE(main_controller.IsViewportScrollCallback(
        MainFrame()->GetDocument()->GetApplyScroll()));
  }

  // Setting the iframe as the root scroller in the main frame should now
  // link the root scrollers so the container should now be the global root
  // scroller.
  {
    SetAndSelectRootScroller(*MainFrame()->GetDocument(), iframe);

    EXPECT_FALSE(main_controller.IsViewportScrollCallback(
        MainFrame()->GetDocument()->GetApplyScroll()));
    EXPECT_TRUE(
        main_controller.IsViewportScrollCallback(container->GetApplyScroll()));
  }

  // Unsetting the root scroller in the iframe should reset its effective root
  // scroller to the iframe's document node and thus it becomes the global root
  // scroller.
  {
    SetAndSelectRootScroller(*iframe->contentDocument(), nullptr);
    EXPECT_FALSE(
        main_controller.IsViewportScrollCallback(container->GetApplyScroll()));
    EXPECT_FALSE(main_controller.IsViewportScrollCallback(
        MainFrame()->GetDocument()->GetApplyScroll()));
    EXPECT_TRUE(main_controller.IsViewportScrollCallback(
        iframe->contentDocument()->GetApplyScroll()));
  }

  // Finally, unsetting the main frame's root scroller should reset it to the
  // document node and corresponding layer.
  {
    SetAndSelectRootScroller(*MainFrame()->GetDocument(), nullptr);
    EXPECT_TRUE(main_controller.IsViewportScrollCallback(
        MainFrame()->GetDocument()->GetApplyScroll()));
    EXPECT_FALSE(
        main_controller.IsViewportScrollCallback(container->GetApplyScroll()));
    EXPECT_FALSE(main_controller.IsViewportScrollCallback(
        iframe->contentDocument()->GetApplyScroll()));
  }
}

// Ensures that disconnecting the element currently set as the root scroller
// recomputes the effective root scroller, before a lifecycle update.
TEST_F(RootScrollerTest, RemoveCurrentRootScroller) {
  Initialize();

  WebURL base_url = url_test_helpers::ToKURL("http://www.test.com/");
  frame_test_helpers::LoadHTMLString(GetWebView()->MainFrameImpl(),
                                     "<!DOCTYPE html>"
                                     "<style>"
                                     "  body {"
                                     "    margin: 0px;"
                                     "  }"
                                     "  #container {"
                                     "    width: 100%;"
                                     "    height: 100%;"
                                     "    position: absolute;"
                                     "    overflow: auto;"
                                     "  }"
                                     "</style>"
                                     "<div id='container'></div>",
                                     base_url);

  RootScrollerController& controller =
      MainFrame()->GetDocument()->GetRootScrollerController();
  Element* container = MainFrame()->GetDocument()->getElementById("container");

  // Set the div as the rootScroller. After a lifecycle update it will be the
  // effective root scroller.
  {
    MainFrame()->GetDocument()->setRootScroller(container, ASSERT_NO_EXCEPTION);
    ASSERT_EQ(container, controller.Get());
    UpdateAllLifecyclePhases(MainFrameView());
    ASSERT_EQ(container, controller.EffectiveRootScroller());
  }

  // Remove the div from the document. It should remain the
  // document.rootScroller, however, it should be demoted from the effective
  // root scroller. The effective will fallback to the document Node.
  {
    MainFrame()->GetDocument()->body()->setTextContent("");
    EXPECT_EQ(container, controller.Get());
    EXPECT_EQ(MainFrame()->GetDocument(), controller.EffectiveRootScroller());
  }
}

// Ensures that the root scroller always gets composited with scrolling layers.
// This is necessary since we replace the Frame scrolling layers in CC as the
// OuterViewport, we need something to replace them with.
TEST_F(RootScrollerTest, AlwaysCreateCompositedScrollingLayers) {
  Initialize();

  WebURL base_url = url_test_helpers::ToKURL("http://www.test.com/");
  frame_test_helpers::LoadHTMLString(GetWebView()->MainFrameImpl(),
                                     "<!DOCTYPE html>"
                                     "<style>"
                                     "  body {"
                                     "    margin: 0px;"
                                     "  }"
                                     "  #container {"
                                     "    width: 100%;"
                                     "    height: 100%;"
                                     "    position: absolute;"
                                     "    overflow: auto;"
                                     "  }"
                                     "</style>"
                                     "<div id='container'></div>",
                                     base_url);

  GetWebView()->ResizeWithBrowserControls(IntSize(400, 400), 50, 0, true);
  UpdateAllLifecyclePhases(MainFrameView());

  Element* container = MainFrame()->GetDocument()->getElementById("container");

  PaintLayerScrollableArea* container_scroller =
      ToLayoutBox(container->GetLayoutObject())->GetScrollableArea();
  PaintLayer* layer = container_scroller->Layer();

  ASSERT_FALSE(layer->HasCompositedLayerMapping());

  SetAndSelectRootScroller(*MainFrame()->GetDocument(), container);

  ASSERT_TRUE(layer->HasCompositedLayerMapping());
  EXPECT_TRUE(layer->GetCompositedLayerMapping()->ScrollingContentsLayer());
  EXPECT_TRUE(layer->GetCompositedLayerMapping()->ScrollingLayer());

  SetAndSelectRootScroller(*MainFrame()->GetDocument(), nullptr);

  EXPECT_FALSE(layer->HasCompositedLayerMapping());
}

TEST_F(RootScrollerTest, TestSetRootScrollerCausesViewportLayerChange) {
  // TODO(bokan): Need a test that changing root scrollers actually sets the
  // outer viewport layer on the compositor, even in the absence of other
  // compositing changes. crbug.com/505516
}

// Tests that trying to set an element as the root scroller of a document inside
// an iframe fails when that element belongs to the parent document.
// TODO(bokan): Recent changes mean this is now possible but should be fixed.
TEST_F(RootScrollerTest,
       DISABLED_TestSetRootScrollerOnElementFromOutsideIframe) {
  Initialize("root-scroller-iframe.html");

  ASSERT_EQ(nullptr, MainFrame()->GetDocument()->rootScroller());
  {
    // Try to set the the root scroller of the child document to be the
    // <iframe> element in the parent document.
    auto* iframe = To<HTMLFrameOwnerElement>(
        MainFrame()->GetDocument()->getElementById("iframe"));
    Element* body =
        MainFrame()->GetDocument()->QuerySelector("body", ASSERT_NO_EXCEPTION);

    EXPECT_EQ(nullptr, iframe->contentDocument()->rootScroller());

    iframe->contentDocument()->setRootScroller(iframe);

    EXPECT_EQ(iframe, iframe->contentDocument()->rootScroller());

    // Try to set the root scroller of the child document to be the
    // <body> element of the parent document.
    iframe->contentDocument()->setRootScroller(body);

    EXPECT_EQ(body, iframe->contentDocument()->rootScroller());
  }
}

// Do a basic sanity check that setting as root scroller an iframe that's remote
// doesn't crash or otherwise fail catastrophically.
TEST_F(RootScrollerTest, RemoteIFrame) {
  Initialize("root-scroller-iframe.html");

  // Initialization: Replace the iframe with a remote frame.
  MainWebFrame()->FirstChild()->Swap(frame_test_helpers::CreateRemote());

  // Set the root scroller in the local main frame to the iframe (which is
  // remote). Make sure we don't promote a remote frame to the root scroller.
  {
    Element* iframe = MainFrame()->GetDocument()->getElementById("iframe");
    SetAndSelectRootScroller(*MainFrame()->GetDocument(), iframe);
    EXPECT_EQ(iframe, MainFrame()->GetDocument()->rootScroller());
    EXPECT_EQ(MainFrame()->GetDocument(),
              EffectiveRootScroller(MainFrame()->GetDocument()));
    UpdateAllLifecyclePhases(MainFrameView());
  }
}

// Make sure that if an effective root scroller becomes a remote frame, it's
// immediately demoted.
TEST_F(RootScrollerTest, IFrameSwapToRemote) {
  Initialize("root-scroller-iframe.html");
  Element* iframe = MainFrame()->GetDocument()->getElementById("iframe");

  {
    SetAndSelectRootScroller(*MainFrame()->GetDocument(), iframe);
    ASSERT_EQ(iframe, EffectiveRootScroller(MainFrame()->GetDocument()));
  }

  // Swap in a remote frame. Make sure we revert back to the document.
  {
    MainWebFrame()->FirstChild()->Swap(frame_test_helpers::CreateRemote());
    UpdateAllLifecyclePhases(MainFrameView());
    EXPECT_EQ(MainFrame()->GetDocument(),
              EffectiveRootScroller(MainFrame()->GetDocument()));
    GetWebView()->ResizeWithBrowserControls(IntSize(400, 450), 50, 0, false);
    UpdateAllLifecyclePhases(MainFrameView());
    EXPECT_EQ(MainFrame()->GetDocument(),
              EffectiveRootScroller(MainFrame()->GetDocument()));
  }
}

// Do a basic sanity check that the scrolling and root scroller machinery
// doesn't fail catastrophically in site isolation when the main frame is
// remote. Setting a root scroller in OOPIF isn't implemented yet but we should
// still scroll as before and not crash. TODO(crbug.com/730269): appears to
// segfault during teardown on TSAN.
#if defined(THREAD_SANITIZER)
TEST_F(RootScrollerTest, DISABLED_RemoteMainFrame) {
#else
TEST_F(RootScrollerTest, RemoteMainFrame) {
#endif
  WebLocalFrameImpl* local_frame;
  WebFrameWidget* widget;

  Initialize("root-scroller-iframe.html");

  // Initialization: Set the main frame to be a RemoteFrame and add a local
  // child.
  {
    WebRemoteFrameImpl* remote_main_frame = frame_test_helpers::CreateRemote();
    helper_.LocalMainFrame()->Swap(remote_main_frame);
    remote_main_frame->SetReplicatedOrigin(
        WebSecurityOrigin(SecurityOrigin::CreateUniqueOpaque()), false);
    local_frame = frame_test_helpers::CreateLocalChild(*remote_main_frame);

    frame_test_helpers::LoadFrame(
        local_frame, base_url_.Utf8() + "root-scroller-child.html");
    widget = local_frame->FrameWidget();
    widget->Resize(WebSize(400, 400));
  }

  Document* document = local_frame->GetFrameView()->GetFrame().GetDocument();
  Element* container = document->getElementById("container");

  // Try scrolling in the iframe.
  {
    widget->HandleInputEvent(
        GenerateWheelGestureEvent(WebInputEvent::kGestureScrollBegin, 0, -100));
    widget->HandleInputEvent(GenerateWheelGestureEvent(
        WebInputEvent::kGestureScrollUpdate, 0, -100));
    widget->HandleInputEvent(
        GenerateWheelGestureEvent(WebInputEvent::kGestureScrollEnd));
    EXPECT_EQ(100, container->scrollTop());
  }

  // Set the container Element as the root scroller.
  {
    SetAndSelectRootScroller(*document, container);
    EXPECT_EQ(container, document->rootScroller());
  }

  // Try scrolling in the iframe now that it has a root scroller set.
  {
    widget->HandleInputEvent(
        GenerateWheelGestureEvent(WebInputEvent::kGestureScrollBegin, 0, -100));
    widget->HandleInputEvent(GenerateWheelGestureEvent(
        WebInputEvent::kGestureScrollUpdate, 0, -100));
    widget->HandleInputEvent(
        GenerateWheelGestureEvent(WebInputEvent::kGestureScrollEnd));

    // TODO(bokan): This doesn't work right now because we notice in
    // Element::nativeApplyScroll that the container is the
    // effectiveRootScroller but the only way we expect to get to
    // nativeApplyScroll is if the effective scroller had its applyScroll
    // ViewportScrollCallback removed.  Keep the scrolls to guard crashes
    // but the expectations on when a ViewportScrollCallback have changed
    // and should be updated.
    // EXPECT_EQ(200, container->scrollTop());
  }
}

// Ensure a non-main local root doesn't interfere with the global root
// scroller. This happens in this situation: Local <- Remote <- Local. This
// tests the crash in https://crbug.com/800566.
TEST_F(RootScrollerTest, NonMainLocalRootLifecycle) {
  WebLocalFrameImpl* non_main_local_root = nullptr;

  // Setup a Local <- Remote <- Local frame hierarchy.
  {
    Initialize();
    WebURL base_url = url_test_helpers::ToKURL("http://www.test.com/");
    frame_test_helpers::LoadHTMLString(GetWebView()->MainFrameImpl(),
                                       R"HTML(
                                              <!DOCTYPE html>
                                              <iframe></iframe>
                                          )HTML",
                                       base_url);
    UpdateAllLifecyclePhases(MainFrameView());

    WebRemoteFrameImpl* remote_frame = frame_test_helpers::CreateRemote();
    WebLocalFrameImpl* child =
        To<WebLocalFrameImpl>(helper_.LocalMainFrame()->FirstChild());
    child->Swap(remote_frame);
    remote_frame->SetReplicatedOrigin(
        WebSecurityOrigin(SecurityOrigin::CreateUniqueOpaque()), false);

    non_main_local_root = frame_test_helpers::CreateLocalChild(*remote_frame);
    ASSERT_EQ(non_main_local_root->LocalRoot(), non_main_local_root);
    ASSERT_TRUE(non_main_local_root->Parent());
  }

  const TopDocumentRootScrollerController& global_controller =
      MainFrame()->GetDocument()->GetPage()->GlobalRootScrollerController();

  ASSERT_EQ(MainFrame()->GetDocument(), global_controller.GlobalRootScroller());

  UpdateAllLifecyclePhases(MainFrameView());

  // Put the local main frame into Layout clean and have the non-main local
  // root do a complete lifecycle update.
  helper_.LocalMainFrame()->GetFrameView()->SetNeedsLayout();
  helper_.LocalMainFrame()->GetFrameView()->UpdateLifecycleToLayoutClean();
  UpdateAllLifecyclePhases(non_main_local_root->GetFrameView());
  UpdateAllLifecyclePhases(helper_.LocalMainFrame()->GetFrameView());

  EXPECT_EQ(MainFrame()->GetDocument(), global_controller.GlobalRootScroller());
}

// Tests that removing the root scroller element from the DOM resets the
// effective root scroller without waiting for any lifecycle events.
TEST_F(RootScrollerTest, RemoveRootScrollerFromDom) {
  Initialize("root-scroller-iframe.html");

  {
    auto* iframe = To<HTMLFrameOwnerElement>(
        MainFrame()->GetDocument()->getElementById("iframe"));
    Element* inner_container =
        iframe->contentDocument()->getElementById("container");

    SetAndSelectRootScroller(*MainFrame()->GetDocument(), iframe);
    SetAndSelectRootScroller(*iframe->contentDocument(), inner_container);

    ASSERT_EQ(iframe, MainFrame()->GetDocument()->rootScroller());
    ASSERT_EQ(iframe, EffectiveRootScroller(MainFrame()->GetDocument()));
    ASSERT_EQ(inner_container, iframe->contentDocument()->rootScroller());
    ASSERT_EQ(inner_container,
              EffectiveRootScroller(iframe->contentDocument()));

    iframe->contentDocument()->body()->SetInnerHTMLFromString("");

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

  Element* container = MainFrame()->GetDocument()->getElementById("container");
  SetAndSelectRootScroller(*MainFrame()->GetDocument(), container);

  ScrollableArea* container_scroller =
      ToLayoutBox(container->GetLayoutObject())->GetScrollableArea();
  EXPECT_FALSE(container_scroller->HorizontalScrollbar());
  EXPECT_FALSE(container_scroller->VerticalScrollbar());
  EXPECT_GT(container_scroller->MaximumScrollOffset().Width(), 0);
  EXPECT_GT(container_scroller->MaximumScrollOffset().Height(), 0);
}

// On Android, the main scrollbars are owned by the visual viewport and the
// LocalFrameView's disabled. This functionality should extend to a rootScroller
// that's a nested iframe.
TEST_F(RootScrollerTest, UseVisualViewportScrollbarsIframe) {
  Initialize("root-scroller-iframe.html");

  Element* iframe = MainFrame()->GetDocument()->getElementById("iframe");
  auto* child_frame =
      To<LocalFrame>(To<HTMLFrameOwnerElement>(iframe)->ContentFrame());

  SetAndSelectRootScroller(*MainFrame()->GetDocument(), iframe);

  WebLocalFrame* child_web_frame =
      MainWebFrame()->FirstChild()->ToWebLocalFrame();
  ExecuteScript(
      "document.getElementById('container').style.width = '200%';"
      "document.getElementById('container').style.height = '200%';",
      *child_web_frame);

  UpdateAllLifecyclePhases(MainFrameView());

  ScrollableArea* container_scroller = child_frame->View()->LayoutViewport();

  EXPECT_FALSE(container_scroller->HorizontalScrollbar());
  EXPECT_FALSE(container_scroller->VerticalScrollbar());
  EXPECT_GT(container_scroller->MaximumScrollOffset().Width(), 0);
  EXPECT_GT(container_scroller->MaximumScrollOffset().Height(), 0);
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

  GetWebView()->ResizeWithBrowserControls(IntSize(400, 400), 50, 50, true);
  UpdateAllLifecyclePhases(MainFrameView());

  Element* container = MainFrame()->GetDocument()->getElementById("container");
  SetAndSelectRootScroller(*MainFrame()->GetDocument(), container);

  ScrollableArea* container_scroller =
      ToLayoutBox(container->GetLayoutObject())->GetScrollableArea();

  // Hide the top controls and scroll down maximally. We should account for the
  // change in maximum scroll offset due to the top controls hiding. That is,
  // since the controls are hidden, the "content area" is taller so the maximum
  // scroll offset should shrink.
  ASSERT_EQ(1000 - 400, container_scroller->MaximumScrollOffset().Height());

  GetWebView()->MainFrameWidget()->HandleInputEvent(
      GenerateTouchGestureEvent(WebInputEvent::kGestureScrollBegin));
  ASSERT_EQ(1, GetBrowserControls().TopShownRatio());
  ASSERT_EQ(1, GetBrowserControls().BottomShownRatio());
  GetWebView()->MainFrameWidget()->HandleInputEvent(
      GenerateTouchGestureEvent(WebInputEvent::kGestureScrollUpdate, 0,
                                -GetBrowserControls().TopHeight()));
  ASSERT_EQ(0, GetBrowserControls().TopShownRatio());
  ASSERT_EQ(0, GetBrowserControls().BottomShownRatio());
  EXPECT_EQ(1000 - 450, container_scroller->MaximumScrollOffset().Height());

  GetWebView()->MainFrameWidget()->HandleInputEvent(
      GenerateTouchGestureEvent(WebInputEvent::kGestureScrollUpdate, 0, -3000));
  EXPECT_EQ(1000 - 450, container_scroller->GetScrollOffset().Height());

  GetWebView()->MainFrameWidget()->HandleInputEvent(
      GenerateTouchGestureEvent(WebInputEvent::kGestureScrollEnd));
  GetWebView()->ResizeWithBrowserControls(IntSize(400, 450), 50, 50, false);
  EXPECT_EQ(1000 - 450, container_scroller->MaximumScrollOffset().Height());
}

TEST_F(RootScrollerTest, RotationAnchoring) {
  Initialize("root-scroller-rotation.html");

  ScrollableArea* container_scroller;

  {
    GetWebView()->ResizeWithBrowserControls(IntSize(250, 1000), 0, 0, true);
    UpdateAllLifecyclePhases(MainFrameView());

    Element* container =
        MainFrame()->GetDocument()->getElementById("container");
    SetAndSelectRootScroller(*MainFrame()->GetDocument(), container);

    container_scroller =
        ToLayoutBox(container->GetLayoutObject())->GetScrollableArea();
  }

  Element* target = MainFrame()->GetDocument()->getElementById("target");

  // Zoom in and scroll the viewport so that the target is fully in the
  // viewport and the visual viewport is fully scrolled within the layout
  // viepwort.
  {
    int scroll_x = 250 * 4;
    int scroll_y = 1000 * 4;

    GetWebView()->SetPageScaleFactor(2);
    GetWebView()->MainFrameWidget()->HandleInputEvent(
        GenerateTouchGestureEvent(WebInputEvent::kGestureScrollBegin));
    GetWebView()->MainFrameWidget()->HandleInputEvent(GenerateTouchGestureEvent(
        WebInputEvent::kGestureScrollUpdate, -scroll_x, -scroll_y));
    GetWebView()->MainFrameWidget()->HandleInputEvent(
        GenerateTouchGestureEvent(WebInputEvent::kGestureScrollEnd));

    // The visual viewport should be 1.5 screens scrolled so that the target
    // occupies the bottom quadrant of the layout viewport.
    ASSERT_EQ((250 * 3) / 2, container_scroller->GetScrollOffset().Width());
    ASSERT_EQ((1000 * 3) / 2, container_scroller->GetScrollOffset().Height());

    // The visual viewport should have scrolled the last half layout viewport.
    ASSERT_EQ((250) / 2, GetVisualViewport().GetScrollOffset().Width());
    ASSERT_EQ((1000) / 2, GetVisualViewport().GetScrollOffset().Height());
  }

  // Now do a rotation resize.
  GetWebView()->ResizeWithBrowserControls(IntSize(1000, 250), 50, 0, false);
  UpdateAllLifecyclePhases(MainFrameView());

  // The visual viewport should remain fully filled by the target.
  DOMRect* rect = target->getBoundingClientRect();
  EXPECT_EQ(rect->left(), GetVisualViewport().GetScrollOffset().Width());
  EXPECT_EQ(rect->top(), GetVisualViewport().GetScrollOffset().Height());
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

  Document* document = MainFrame()->GetDocument();
  auto* iframe = To<HTMLFrameOwnerElement>(
      MainFrame()->GetDocument()->getElementById("iframe"));
  auto* iframe_view = To<LocalFrame>(iframe->ContentFrame())->View();

  ASSERT_EQ(IntSize(400, 400), iframe_view->GetLayoutSize());
  ASSERT_EQ(IntSize(400, 400), iframe_view->Size());
  ASSERT_TRUE(iframe_view->LayoutSizeFixedToFrameSize());

  // Make the iframe the rootscroller. This should cause the iframe's layout
  // size to be manually controlled.
  {
    SetAndSelectRootScroller(*document, iframe);
    EXPECT_FALSE(iframe_view->LayoutSizeFixedToFrameSize());
    EXPECT_EQ(IntSize(400, 400), iframe_view->GetLayoutSize());
    EXPECT_EQ(IntSize(400, 400), iframe_view->Size());
  }

  // Hide the URL bar, the iframe's frame rect should expand but the layout
  // size should remain the same.
  {
    GetWebView()->ResizeWithBrowserControls(IntSize(400, 450), 50, 0, false);
    UpdateAllLifecyclePhases(MainFrameView());
    EXPECT_EQ(IntSize(400, 400), iframe_view->GetLayoutSize());
    EXPECT_EQ(IntSize(400, 450), iframe_view->Size());
  }

  // Simulate a rotation. This time the layout size should reflect the resize.
  {
    GetWebView()->ResizeWithBrowserControls(IntSize(450, 400), 50, 0, false);
    UpdateAllLifecyclePhases(MainFrameView());
    EXPECT_EQ(IntSize(450, 350), iframe_view->GetLayoutSize());
    EXPECT_EQ(IntSize(450, 400), iframe_view->Size());

    // "Un-rotate" for following tests.
    GetWebView()->ResizeWithBrowserControls(IntSize(400, 450), 50, 0, false);
    UpdateAllLifecyclePhases(MainFrameView());
  }

  // Show the URL bar again. The frame rect should match the viewport.
  {
    GetWebView()->ResizeWithBrowserControls(IntSize(400, 400), 50, 0, true);
    UpdateAllLifecyclePhases(MainFrameView());
    EXPECT_EQ(IntSize(400, 400), iframe_view->GetLayoutSize());
    EXPECT_EQ(IntSize(400, 400), iframe_view->Size());
  }

  // Hide the URL bar and reset the rootScroller. The iframe should go back to
  // tracking layout size by frame rect.
  {
    GetWebView()->ResizeWithBrowserControls(IntSize(400, 450), 50, 0, false);
    UpdateAllLifecyclePhases(MainFrameView());
    EXPECT_EQ(IntSize(400, 400), iframe_view->GetLayoutSize());
    EXPECT_EQ(IntSize(400, 450), iframe_view->Size());
    SetAndSelectRootScroller(*document, nullptr);
    EXPECT_TRUE(iframe_view->LayoutSizeFixedToFrameSize());
    EXPECT_EQ(IntSize(400, 400), iframe_view->GetLayoutSize());
    EXPECT_EQ(IntSize(400, 400), iframe_view->Size());
  }
}

// Ensure that removing the root scroller element causes an update to the RFV's
// layout viewport immediately since old layout viewport is now part of a
// detached layout hierarchy.
TEST_F(RootScrollerTest, ImmediateUpdateOfLayoutViewport) {
  Initialize("root-scroller-iframe.html");

  Document* document = MainFrame()->GetDocument();
  auto* iframe = To<HTMLFrameOwnerElement>(
      MainFrame()->GetDocument()->getElementById("iframe"));

  SetAndSelectRootScroller(*document, iframe);

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

class RootScrollerSimTest : public SimTest {
 public:
  RootScrollerSimTest() : implicit_root_scroller_for_test_(false) {}

  void SetUp() override {
    SimTest::SetUp();
    WebView().GetPage()->GetSettings().SetViewportEnabled(true);
  }

 private:
  ScopedImplicitRootScrollerForTest implicit_root_scroller_for_test_;
};

// Test that setting a root scroller causes us to request a begin frame.
// However, until a frame is produced, the effective root scroller should
// not change.
TEST_F(RootScrollerSimTest, SetCausesNeedsBeginFrame) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
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
          </style>
          <div id="container"></div>
      )HTML");
  Compositor().BeginFrame();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  Element* container = GetDocument().getElementById("container");
  GetDocument().setRootScroller(container);

  // Setting the root scroller should cause us to need a new frame but we
  // shouldn't have set the effective yet.
  EXPECT_TRUE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  Compositor().BeginFrame();

  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());
}

// Test that the cached IsEffectiveRootScroller bit on LayoutObject is set
// correctly when the Document is the effective root scroller. It becomes the
// root scroller before Document has a LayoutView.
TEST_F(RootScrollerSimTest, DocumentEffectiveSetsCachedBit) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
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
TEST_F(RootScrollerSimTest, NonLifecycleLayoutDoesntCauseReselection) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
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
          </style>
          <div id="container"></div>
      )HTML");
  Element* container = GetDocument().getElementById("container");
  GetDocument().setRootScroller(container);
  Compositor().BeginFrame();
  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  container->style()->setProperty(&GetDocument(), "width", "95%", String(),
                                  ASSERT_NO_EXCEPTION);

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
TEST_F(RootScrollerSimTest, RecomputeEffectiveWithNoContentFrame) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
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
            iframe {
              width: 100%;
              height: 100%;
              border: 0;
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

  Element* container = GetDocument().getElementById("first");
  GetDocument().GetRootScrollerController().Set(container);
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
TEST_F(RootScrollerSimTest, UsePaddingBoxForViewportFillingCondition) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            body {
              margin: 0;
            }
            #container {
              position: absolute;
              width: 100%;
              height: 100%;
              box-sizing: border-box;
              overflow: scroll;
            }
          </style>
          <div id="container"></div>
      )HTML");

  Element* container = GetDocument().getElementById("container");
  GetDocument().setRootScroller(container);
  Compositor().BeginFrame();

  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // Setting a border should cause the element to no longer be valid as its
  // padding box doesn't fill the viewport exactly.
  container->setAttribute(html_names::kStyleAttr, "border: 1px solid black");
  Compositor().BeginFrame();
  EXPECT_EQ(&GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());
}

// Tests that the root scroller doesn't affect visualViewport pageLeft and
// pageTop.
TEST_F(RootScrollerSimTest, RootScrollerDoesntAffectVisualViewport) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
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
      FloatPoint(100, 120));

  auto* frame = To<LocalFrame>(GetDocument().GetPage()->MainFrame());
  EXPECT_EQ(100, frame->DomWindow()->visualViewport()->pageLeft());
  EXPECT_EQ(120, frame->DomWindow()->visualViewport()->pageTop());

  request.Finish();
  Compositor().BeginFrame();

  Element* container = GetDocument().getElementById("container");
  GetDocument().setRootScroller(container);

  Compositor().BeginFrame();

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
TEST_F(RootScrollerSimTest, ResizeFromResizeAfterLayout) {
  WebView().GetSettings()->SetShrinksViewportContentToFit(true);
  WebView().SetDefaultPageScaleLimits(0.25f, 5);

  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
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
              border: 0;
            }
          </style>
          <iframe id="container"
                  srcdoc="<!DOCTYPE html>
                          <style>html {height: 300%;}</style>">
          </iframe>
      )HTML");
  Element* container = GetDocument().getElementById("container");
  GetDocument().setRootScroller(container);
  Compositor().BeginFrame();
  RunPendingTasks();
  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());
  ASSERT_EQ(IntSize(800, 600), GetDocument().View()->Size());

  request.Write(R"HTML(
          <div style="width:2000px;height:1000px"></div>
      )HTML");
  request.Finish();
  Compositor().BeginFrame();

  ASSERT_EQ(IntSize(2000, 1500), GetDocument().View()->Size());
}

class ImplicitRootScrollerSimTest : public SimTest {
 public:
  ImplicitRootScrollerSimTest()
      : root_scroller_for_test_(false),
        implicit_root_scroller_for_test_(true) {}

  void SetUp() override {
    SimTest::SetUp();
    WebView().GetPage()->GetSettings().SetViewportEnabled(true);
  }

 private:
  ScopedSetRootScrollerForTest root_scroller_for_test_;
  ScopedImplicitRootScrollerForTest implicit_root_scroller_for_test_;
};

// Tests basic implicit root scroller mode with a <div>.
TEST_F(ImplicitRootScrollerSimTest, ImplicitRootScroller) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
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
  Element* container = GetDocument().getElementById("container");

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

    container->style()->setProperty(&GetDocument(), style, style_val, String(),
                                    ASSERT_NO_EXCEPTION);
    Compositor().BeginFrame();
    ASSERT_EQ(expected_root_scroller,
              GetDocument().GetRootScrollerController().EffectiveRootScroller())
        << "Failed to set rootScroller after setting " << std::get<0>(test_case)
        << ": " << std::get<1>(test_case);
    container->style()->setProperty(&GetDocument(), std::get<0>(test_case),
                                    String(), String(), ASSERT_NO_EXCEPTION);
    Compositor().BeginFrame();
    ASSERT_EQ(&GetDocument(),
              GetDocument().GetRootScrollerController().EffectiveRootScroller())
        << "Failed to reset rootScroller after setting "
        << std::get<0>(test_case) << ": " << std::get<1>(test_case);
  }

  // Now remove the overflowing element and rerun the tests. The container
  // element should no longer be implicitly promoted as it doesn't have any
  // overflow.
  Element* spacer = GetDocument().getElementById("spacer");
  spacer->remove();

  for (auto test_case : test_cases) {
    String& style = std::get<0>(test_case);
    String& style_val = std::get<1>(test_case);
    Node* expected_root_scroller = &GetDocument();

    container->style()->setProperty(&GetDocument(), style, style_val, String(),
                                    ASSERT_NO_EXCEPTION);
    Compositor().BeginFrame();
    ASSERT_EQ(expected_root_scroller,
              GetDocument().GetRootScrollerController().EffectiveRootScroller())
        << "Failed to set rootScroller after setting " << std::get<0>(test_case)
        << ": " << std::get<1>(test_case);

    container->style()->setProperty(&GetDocument(), std::get<0>(test_case),
                                    String(), String(), ASSERT_NO_EXCEPTION);
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
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
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

  Element* spacer = GetDocument().getElementById("spacer");
  spacer->style()->setProperty(&GetDocument(), "height", "2000px", String(),
                               ASSERT_NO_EXCEPTION);
  spacer->style()->setProperty(&GetDocument(), "width", "2000px", String(),
                               ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  Element* container = GetDocument().getElementById("container");
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "Adding overflow should cause 'container' to be promoted.";
}

// Tests that we don't crash if an implicit candidate is no longer a box. This
// test passes if it doesn't crash.
TEST_F(ImplicitRootScrollerSimTest, CandidateLosesLayoutBoxDontCrash) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
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
  Element* container = GetDocument().getElementById("container");

  // An overflowing box will be added to the implicit candidates list.
  container->setAttribute(html_names::kClassAttr, "box");
  Compositor().BeginFrame();

  // This will make change from a box to an inline. Ensure we don't crash when
  // we reevaluate the candidates list.
  container->setAttribute(html_names::kClassAttr, "nonbox");
  Compositor().BeginFrame();
}

// Ensure that a plugin view being considered for implicit promotion doesn't
// cause a crash. https://crbug.com/903440.
TEST_F(ImplicitRootScrollerSimTest, ConsiderEmbedCrash) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
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
  Element* embed = GetDocument().getElementById("embed");
  GetDocument().GetRootScrollerController().ConsiderForImplicit(*embed);
}

// Test that a valid implicit root scroller wont be promoted/will be demoted if
// the main document has overflow.
TEST_F(ImplicitRootScrollerSimTest,
       ImplicitRootScrollerDocumentScrollsOverflow) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
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

  Element* container = GetDocument().getElementById("container");
  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  Element* overflow = GetDocument().getElementById("overflow");
  overflow->style()->setProperty(&GetDocument(), "height", "10px", String(),
                                 ASSERT_NO_EXCEPTION);
  overflow->style()->setProperty(&GetDocument(), "width", "10px", String(),
                                 ASSERT_NO_EXCEPTION);
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
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
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

  Element* container = GetDocument().getElementById("container");
  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  container->style()->setProperty(&GetDocument(), "opacity", "0.5", String(),
                                  ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(&GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "Adding opacity to 'container' causes it to be demoted.";

  container->style()->setProperty(&GetDocument(), "opacity", "", String(),
                                  ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "Removing opacity from 'container' causes it to be promoted.";

  container->style()->setProperty(&GetDocument(), "visibility", "hidden",
                                  String(), ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(&GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "visibility:hidden causes 'container' to be demoted.";

  container->style()->setProperty(&GetDocument(), "visibility", "collapse",
                                  String(), ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(&GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "visibility:collapse doesn't cause 'container' to be promoted.";

  container->style()->setProperty(&GetDocument(), "visibility", "visible",
                                  String(), ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "visibility:visible causes promotion";
}

// Tests implicit root scroller mode for iframes.
TEST_F(ImplicitRootScrollerSimTest, ImplicitRootScrollerIframe) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
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

  Element* container = GetDocument().getElementById("container");
  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  container->style()->setProperty(&GetDocument(), "height", "95%", String(),
                                  ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();

  ASSERT_EQ(&GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());
}

// Tests use counter for implicit root scroller. Ensure it's not counted on a
// page without an implicit root scroller.
TEST_F(ImplicitRootScrollerSimTest, UseCounterNegative) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
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

  Element* container = GetDocument().getElementById("container");
  ASSERT_NE(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kActivatedImplicitRootScroller));

  container->style()->setProperty(&GetDocument(), "height", "150%", String(),
                                  ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();

  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kActivatedImplicitRootScroller));
}

// Tests use counter for implicit root scroller. Ensure it's counted on a
// page that loads with an implicit root scroller.
TEST_F(ImplicitRootScrollerSimTest, UseCounterPositive) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
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

  Element* container = GetDocument().getElementById("container");
  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kActivatedImplicitRootScroller));

  container->style()->setProperty(&GetDocument(), "height", "150%", String(),
                                  ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();

  ASSERT_NE(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kActivatedImplicitRootScroller));
}

// Tests use counter for implicit root scroller. Ensure it's counted on a
// page that loads without an implicit root scroller but later gets one.
TEST_F(ImplicitRootScrollerSimTest, UseCounterPositiveAfterLoad) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
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

  Element* container = GetDocument().getElementById("container");
  ASSERT_NE(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kActivatedImplicitRootScroller));

  container->style()->setProperty(&GetDocument(), "height", "100%", String(),
                                  ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();

  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kActivatedImplicitRootScroller));
}

// Tests that if we have multiple valid candidates for implicit promotion, we
// don't promote either.
TEST_F(ImplicitRootScrollerSimTest, DontPromoteWhenMultipleAreValid) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
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
  Element* container2 = GetDocument().getElementById("container2");
  container2->style()->setProperty(&GetDocument(), "height", "95%", String(),
                                   ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();

  Element* container = GetDocument().getElementById("container");
  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());
}

// Test that when a valid iframe becomes loaded and thus should be promoted, it
// becomes the root scroller, without needing an intervening layout.
TEST_F(ImplicitRootScrollerSimTest, IframeLoadedWithoutLayout) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
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
  EXPECT_EQ(GetDocument().getElementById("container"),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "Once loaded, the iframe should be promoted.";
}

// Ensure that navigating an iframe while it is the effective root scroller,
// causes it to remain the effective root scroller after the navigation (to a
// page where it remains valid) is finished.
TEST_F(ImplicitRootScrollerSimTest, NavigateToValidRemainsRootScroller) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
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
  ASSERT_EQ(GetDocument().getElementById("container"),
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

  EXPECT_EQ(GetDocument().getElementById("container"),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "Once loaded, the iframe should be promoted again.";
}

// Test that a root scroller is considered to fill the viewport at both the URL
// bar shown and URL bar hidden height.
TEST_F(ImplicitRootScrollerSimTest,
       RootScrollerFillsViewportAtBothURLBarStates) {
  WebView().ResizeWithBrowserControls(IntSize(800, 600), 50, 0, true);
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
  Element* container = GetDocument().getElementById("container");
  GetDocument().setRootScroller(container);

  Compositor().BeginFrame();

  ASSERT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // Simulate hiding the top controls. The root scroller should remain valid at
  // the new height.
  WebView().GetPage()->GetBrowserControls().SetShownRatio(0, 0);
  WebView().ResizeWithBrowserControls(IntSize(800, 650), 50, 50, false);
  Compositor().BeginFrame();
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // Simulate showing the top controls. The root scroller should remain valid.
  WebView().GetPage()->GetBrowserControls().SetShownRatio(1, 1);
  WebView().ResizeWithBrowserControls(IntSize(800, 600), 50, 50, true);
  Compositor().BeginFrame();
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // Set the height explicitly to a new value in-between. The root scroller
  // should be demoted.
  container->style()->setProperty(&GetDocument(), "height", "601px", String(),
                                  ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // Reset back to valid and hide the top controls. Zoom to 2x. Ensure we're
  // still considered valid.
  container->style()->setProperty(&GetDocument(), "height", "", String(),
                                  ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());
  EXPECT_EQ(ToLayoutBox(container->GetLayoutObject())->Size().Height(), 600);
  WebView().SetZoomLevel(PageZoomFactorToZoomLevel(2.0));
  WebView().GetPage()->GetBrowserControls().SetShownRatio(0, 0);
  WebView().ResizeWithBrowserControls(IntSize(800, 650), 50, 50, false);
  Compositor().BeginFrame();
  EXPECT_EQ(container->clientHeight(), 325);
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());
}

// Tests that implicit is continually reevaluating whether to promote or demote
// a scroller.
TEST_F(ImplicitRootScrollerSimTest, ContinuallyReevaluateImplicitPromotion) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
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

  Element* parent = GetDocument().getElementById("parent");
  Element* container = GetDocument().getElementById("container");
  Element* spacer = GetDocument().getElementById("spacer");

  // The container isn't yet scrollable.
  ASSERT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // The container now has overflow but still doesn't scroll.
  spacer->style()->setProperty(&GetDocument(), "height", "2000px", String(),
                               ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // The container is now scrollable and should be promoted.
  container->style()->setProperty(&GetDocument(), "overflow", "auto", String(),
                                  ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // The container is now not viewport-filling so it should be demoted.
  container->style()->setProperty(&GetDocument(), "transform",
                                  "translateX(-50px)", String(),
                                  ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // The container is viewport-filling again so it should be promoted.
  parent->style()->setProperty(&GetDocument(), "transform", "translateX(50px)",
                               String(), ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // No longer scrollable so demote.
  container->style()->setProperty(&GetDocument(), "overflow", "hidden",
                                  String(), ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());
}

// Tests that implicit mode correctly recognizes when an iframe becomes
// scrollable.
TEST_F(ImplicitRootScrollerSimTest, IframeScrollingAffectsPromotion) {
  WebView().MainFrameWidget()->Resize(WebSize(800, 600));
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

  auto* container =
      To<HTMLFrameOwnerElement>(GetDocument().getElementById("container"));
  Element* inner_html_element = container->contentDocument()->documentElement();

  // Shouldn't be promoted since it's not scrollable.
  EXPECT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // Allows scrolling now so promote.
  inner_html_element->style()->setProperty(container->contentDocument(),
                                           "overflow", "auto", String(),
                                           ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(container,
            GetDocument().GetRootScrollerController().EffectiveRootScroller());

  // Demote again.
  inner_html_element->style()->setProperty(container->contentDocument(),
                                           "overflow", "hidden", String(),
                                           ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();
  EXPECT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller());
}

// Loads with a larger than the ICB (but otherwise valid) implicit root
// scrolling iframe. When the iframe is promoted (which happens at the end of
// layout) its layout size is changed which makes it easy to violate lifecycle
// assumptions.  (e.g. NeedsLayout at the end of layout)
TEST_F(ImplicitRootScrollerSimTest, PromotionChangesLayoutSize) {
  WebView().ResizeWithBrowserControls(IntSize(800, 650), 50, 0, false);
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
  EXPECT_EQ(GetDocument().getElementById("container"),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "Once loaded, the iframe should be promoted.";
}

// Tests that bottom-fixed objects inside of an iframe root scroller and frame
// are marked as being affected by top controls movement. Those inside a
// non-rootScroller iframe should not be marked as such.
TEST_F(ImplicitRootScrollerSimTest, BottomFixedAffectedByTopControls) {
  WebView().ResizeWithBrowserControls(IntSize(800, 650), 50, 0, false);
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

  Element* container1 = GetDocument().getElementById("container1");
  Element* container2 = GetDocument().getElementById("container2");
  ASSERT_EQ(container1,
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "The #container1 iframe must be promoted.";

  Document* child1_document =
      To<HTMLFrameOwnerElement>(container1)->contentDocument();
  Document* child2_document =
      To<HTMLFrameOwnerElement>(container2)->contentDocument();
  LayoutObject* fixed =
      GetDocument().getElementById("fixed")->GetLayoutObject();
  LayoutObject* fixed1 =
      child1_document->getElementById("fixed")->GetLayoutObject();
  LayoutObject* fixed2 =
      child2_document->getElementById("fixed")->GetLayoutObject();

  EXPECT_TRUE(fixed->FirstFragment()
                  .ContentsProperties()
                  .Transform()
                  .IsAffectedByOuterViewportBoundsDelta());
  EXPECT_TRUE(fixed1->FirstFragment()
                  .ContentsProperties()
                  .Transform()
                  .IsAffectedByOuterViewportBoundsDelta());
  EXPECT_FALSE(fixed2->FirstFragment()
                   .ContentsProperties()
                   .Transform()
                   .IsAffectedByOuterViewportBoundsDelta());
}

// Ensure that we're using the content box for an iframe. Promotion will cause
// the content to use the layout size of the parent frame so having padding or
// a border would cause us to relayout.
TEST_F(ImplicitRootScrollerSimTest, IframeUsesContentBox) {
  WebView().ResizeWithBrowserControls(IntSize(800, 600), 0, 0, false);
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

  Element* iframe = GetDocument().getElementById("container");

  ASSERT_EQ(iframe,
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "The iframe should start off promoted.";

  // Adding padding should cause the iframe to be demoted.
  {
    iframe->setAttribute(html_names::kStyleAttr, "padding-left: 20%");
    Compositor().BeginFrame();

    EXPECT_NE(iframe,
              GetDocument().GetRootScrollerController().EffectiveRootScroller())
        << "The iframe should be demoted once it has padding.";
  }

  // Replacing padding with a border should also ensure the iframe remains
  // demoted.
  {
    iframe->setAttribute(html_names::kStyleAttr, "border: 5px solid black");
    Compositor().BeginFrame();

    EXPECT_NE(iframe,
              GetDocument().GetRootScrollerController().EffectiveRootScroller())
        << "The iframe should be demoted once it has border.";
  }

  // Removing the border should now cause the iframe to be promoted once again.
  iframe->setAttribute(html_names::kStyleAttr, "");
  Compositor().BeginFrame();

  ASSERT_EQ(iframe,
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "The iframe should once again be promoted when border is removed";
}

// Test that we don't promote any elements implicitly if the main document has
// vertical scrolling.
TEST_F(ImplicitRootScrollerSimTest, OverflowInMainDocumentRestrictsImplicit) {
  WebView().ResizeWithBrowserControls(IntSize(800, 600), 50, 0, true);
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

  Element* spacer = GetDocument().getElementById("spacer");
  spacer->style()->setProperty(&GetDocument(), "height", "100%", String(),
                               ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();

  EXPECT_EQ(GetDocument().getElementById("container"),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "Once vertical overflow is removed, the iframe should be promoted.";
}

// Test that we overflow in the document allows promotion only so long as the
// document isn't scrollable.
TEST_F(ImplicitRootScrollerSimTest, OverflowHiddenDoesntRestrictImplicit) {
  WebView().ResizeWithBrowserControls(IntSize(800, 600), 50, 0, true);
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
  EXPECT_EQ(GetDocument().getElementById("container"),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "iframe should be promoted since document's overflow is hidden.";

  Element* html = GetDocument().documentElement();
  html->style()->setProperty(&GetDocument(), "overflow", "auto", String(),
                             ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();

  EXPECT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "iframe should now be demoted since main document scrolls overflow.";

  html->style()->setProperty(&GetDocument(), "overflow", "visible", String(),
                             ASSERT_NO_EXCEPTION);
  Compositor().BeginFrame();

  EXPECT_EQ(GetDocument(),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "iframe should remain demoted since overflow:visible on document "
      << "allows scrolling.";
}

// Test that any non-document, clipping ancestor prevents implicit promotion.
TEST_F(ImplicitRootScrollerSimTest, ClippingAncestorPreventsPromotion) {
  WebView().ResizeWithBrowserControls(IntSize(800, 600), 50, 0, true);
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
    Element* ancestor = GetDocument().getElementById("ancestor");
    Element* iframe = GetDocument().getElementById("container");

    ASSERT_EQ(iframe,
              GetDocument().GetRootScrollerController().EffectiveRootScroller())
        << "iframe should start off promoted.";

    ancestor->style()->setProperty(&GetDocument(), style, style_val, String(),
                                   ASSERT_NO_EXCEPTION);
    Compositor().BeginFrame();

    EXPECT_EQ(GetDocument(),
              GetDocument().GetRootScrollerController().EffectiveRootScroller())
        << "iframe should be demoted since ancestor has " << style << ": "
        << style_val;

    ancestor->style()->setProperty(&GetDocument(), style, String(), String(),
                                   ASSERT_NO_EXCEPTION);
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
  WebView().ResizeWithBrowserControls(IntSize(1442, 2349), 196, 0, true);

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

  EXPECT_EQ(GetDocument().getElementById("container"),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "<iframe> should be promoted when URL bar is hidden";

  WebView().ResizeWithBrowserControls(IntSize(1442, 2545), 196, 0, false);
  Compositor().BeginFrame();

  EXPECT_EQ(GetDocument().getElementById("container"),
            GetDocument().GetRootScrollerController().EffectiveRootScroller())
      << "<iframe> should remain promoted when URL bar is hidden";
}

class RootScrollerHitTest : public RootScrollerSimTest {
 public:
  void CheckHitTestAtBottomOfScreen(Element* target) {
    HideTopControlsWithMaximalScroll();

    // Do a hit test at the very bottom of the screen. This should be outside
    // the root scroller's LayoutBox since inert top controls won't resize the
    // ICB but, since we expaned the clip, we should still be able to hit the
    // target.
    gfx::Point point(200, 445);
    WebSize tap_area(20, 20);
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
    WebView().MainFrameWidget()->ApplyViewportChanges(
        {gfx::ScrollOffset(), gfx::Vector2dF(), 1, false, -1, -1,
         cc::BrowserControlsState::kBoth});
    ASSERT_EQ(0, GetBrowserControls().TopShownRatio());
    ASSERT_EQ(0, GetBrowserControls().BottomShownRatio());

    Node* scroller = GetDocument()
                         .GetPage()
                         ->GlobalRootScrollerController()
                         .GlobalRootScroller();
    ScrollableArea* scrollable_area =
        ToLayoutBox(scroller->GetLayoutObject())->GetScrollableArea();
    scrollable_area->DidScroll(FloatPoint(0, 100000));

    WebView().ResizeWithBrowserControls(IntSize(400, 450), 50, 50, false);

    Compositor().BeginFrame();
  }
};

// Test that hit testing in the area revealed at the bottom of the screen
// revealed by hiding the URL bar works properly when using a root scroller
// when the target and scroller are in the same PaintLayer.
TEST_F(RootScrollerHitTest, HitTestInAreaRevealedByURLBarSameLayer) {
  WebView().ResizeWithBrowserControls(IntSize(400, 400), 50, 50, true);
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

  Element* container = GetDocument().getElementById("container");
  Element* target = GetDocument().getElementById("target");
  GetDocument().setRootScroller(container, ASSERT_NO_EXCEPTION);

  Compositor().BeginFrame();

  // This test checks hit testing while the target is in the same PaintLayer as
  // the root scroller.
  ASSERT_EQ(ToLayoutBox(target->GetLayoutObject())->EnclosingLayer(),
            ToLayoutBox(container->GetLayoutObject())->Layer());

  CheckHitTestAtBottomOfScreen(target);
}

// Test that hit testing in the area revealed at the bottom of the screen
// revealed by hiding the URL bar works properly when using a root scroller
// when the target and scroller are in different PaintLayers.
TEST_F(RootScrollerHitTest, HitTestInAreaRevealedByURLBarDifferentLayer) {
  WebView().ResizeWithBrowserControls(IntSize(400, 400), 50, 50, true);
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

  Element* container = GetDocument().getElementById("container");
  Element* target = GetDocument().getElementById("target");
  GetDocument().setRootScroller(container, ASSERT_NO_EXCEPTION);

  Compositor().BeginFrame();

  // Ensure the target and container weren't put into the same layer.
  ASSERT_NE(ToLayoutBox(target->GetLayoutObject())->EnclosingLayer(),
            ToLayoutBox(container->GetLayoutObject())->Layer());

  CheckHitTestAtBottomOfScreen(target);
}

// Test that hit testing in the area revealed at the bottom of the screen
// revealed by hiding the URL bar works properly when using a root scroller
// inside an iframe, when the target and scroller are in different PaintLayers.
TEST_F(RootScrollerHitTest, HitTestHideURLBarDifferentLayerIframe) {
  WebView().ResizeWithBrowserControls(IntSize(400, 400), 50, 50, true);
  GetBrowserControls().SetShownRatio(1, 1);
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

  // Add a target at the bottom of the root scroller that's the size of the url
  // bar. We'll test that hiding the URL bar appropriately adjusts clipping so
  // that we can hit this target.
  child_request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            body, html {
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
              will-change: transform;
            }
          </style>
          <div id='container'>
            <div id='spacer'></div>
            <div id='target'></div>
          </div>
      )HTML");

  Element* container = GetDocument().getElementById("container");
  GetDocument().setRootScroller(container, ASSERT_NO_EXCEPTION);

  Document* child_document =
      To<HTMLFrameOwnerElement>(container)->contentDocument();
  Element* child_container = child_document->getElementById("container");
  child_document->setRootScroller(child_container, ASSERT_NO_EXCEPTION);

  Compositor().BeginFrame();

  // Ensure the target and container weren't put into the same layer.
  Element* target = child_document->getElementById("target");
  ASSERT_NE(ToLayoutBox(target->GetLayoutObject())->EnclosingLayer(),
            ToLayoutBox(child_container->GetLayoutObject())->Layer());

  CheckHitTestAtBottomOfScreen(target);
}

// Test that hit testing in the area revealed at the bottom of the screen
// revealed by hiding the URL bar works properly when using a root scroller
// inside an iframe, when the target and scroller are in the same PaintLayer.
TEST_F(RootScrollerHitTest, HitTestHideURLBarSameLayerIframe) {
  WebView().ResizeWithBrowserControls(IntSize(400, 400), 50, 50, true);
  GetBrowserControls().SetShownRatio(1, 1);
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

  // Add a target at the bottom of the root scroller that's the size of the url
  // bar. We'll test that hiding the URL bar appropriately adjusts clipping so
  // that we can hit this target.
  child_request.Complete(R"HTML(
          <!DOCTYPE html>
          <style>
            body, html {
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

  Element* container = GetDocument().getElementById("container");
  GetDocument().setRootScroller(container, ASSERT_NO_EXCEPTION);

  Document* child_document =
      To<HTMLFrameOwnerElement>(container)->contentDocument();
  Element* child_container = child_document->getElementById("container");
  child_document->setRootScroller(child_container, ASSERT_NO_EXCEPTION);

  Compositor().BeginFrame();

  // Ensure the target and container weren't put into the same layer.
  Element* target = child_document->getElementById("target");
  ASSERT_EQ(ToLayoutBox(target->GetLayoutObject())->EnclosingLayer(),
            ToLayoutBox(child_container->GetLayoutObject())->Layer());

  CheckHitTestAtBottomOfScreen(target);
}

}  // namespace

}  // namespace blink
