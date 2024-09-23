/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/public/web/web_view.h"

#include <limits>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/test/test_ukm_recorder_factory.h"
#include "cc/trees/layer_tree_host.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/common/widget/device_emulation_params.h"
#include "third_party/blink/public/mojom/frame/tree_scope_type.mojom-blink.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/mojom/input/touch_event.mojom-blink.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_drag_data.h"
#include "third_party/blink/public/public_buildflags.h"
#include "third_party/blink/public/test/test_web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_hit_test_result.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_non_composited_widget_client.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/public/web/web_widget.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document.h"
#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/css/media_query_list_listener.h"
#include "third_party/blink/renderer/core/css/media_query_matcher.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/exported/web_page_popup_impl.h"
#include "third_party/blink/renderer/core/exported/web_settings_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/forms/external_date_time_chooser.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/forms/internal_popup_menu.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/inspector/dev_tools_emulator.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_hidden_state.h"
#include "third_party/blink/renderer/core/page/page_popup_client.h"
#include "third_party/blink/renderer/core/page/print_context.h"
#include "third_party/blink/renderer/core/page/scoped_browsing_context_group_pauser.h"
#include "third_party/blink/renderer/core/page/scoped_page_pauser.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/testing/color_scheme_helper.h"
#include "third_party/blink/renderer/core/testing/fake_web_plugin.h"
#include "third_party/blink/renderer/core/testing/mock_clipboard_host.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/event_timing.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/cursors.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-blink.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-blink.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "v8/include/v8.h"

#if BUILDFLAG(ENABLE_UNHANDLED_TAP)
#include "third_party/blink/public/mojom/unhandled_tap_notifier/unhandled_tap_notifier.mojom-blink.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)

using blink::frame_test_helpers::LoadFrame;
using blink::test::RunPendingTasks;
using blink::url_test_helpers::RegisterMockedURLLoad;
using blink::url_test_helpers::ToKURL;

namespace blink {

enum HorizontalScrollbarState {
  kNoHorizontalScrollbar,
  kVisibleHorizontalScrollbar,
};

enum VerticalScrollbarState {
  kNoVerticalScrollbar,
  kVisibleVerticalScrollbar,
};

class TestData {
 public:
  void SetWebView(WebView* web_view) { web_view_ = To<WebViewImpl>(web_view); }
  void SetSize(const gfx::Size& new_size) { size_ = new_size; }
  HorizontalScrollbarState GetHorizontalScrollbarState() const {
    return web_view_->HasHorizontalScrollbar() ? kVisibleHorizontalScrollbar
                                               : kNoHorizontalScrollbar;
  }
  VerticalScrollbarState GetVerticalScrollbarState() const {
    return web_view_->HasVerticalScrollbar() ? kVisibleVerticalScrollbar
                                             : kNoVerticalScrollbar;
  }
  int Width() const { return size_.width(); }
  int Height() const { return size_.height(); }

 private:
  gfx::Size size_;
  WebViewImpl* web_view_;
};

class AutoResizeWebViewClient : public WebViewClient {
 public:
  // WebViewClient methods
  void DidAutoResize(const gfx::Size& new_size) override {
    test_data_.SetSize(new_size);
  }

  // Local methods
  TestData& GetTestData() { return test_data_; }

 private:
  TestData test_data_;
};

class WebViewTest : public testing::Test {
 public:
  // Observer that remembers the most recent visibility callback, if any.
  class MockWebViewObserver : public WebViewObserver {
   public:
    explicit MockWebViewObserver(WebView* web_view)
        : WebViewObserver(web_view) {}
    ~MockWebViewObserver() override = default;

    blink::mojom::PageVisibilityState page_visibility_and_clear() {
      auto t = *page_visibility_;
      page_visibility_.reset();
      return t;
    }

    // WebViewObserver
    void OnPageVisibilityChanged(
        blink::mojom::PageVisibilityState page_visibility) override {
      page_visibility_ = page_visibility;
    }

    // We live on the stack, so do nothing here.
    void OnDestruct() override {}

   private:
    std::optional<blink::mojom::PageVisibilityState> page_visibility_;
  };

  explicit WebViewTest(frame_test_helpers::CreateTestWebFrameWidgetCallback
                           create_web_frame_callback = base::NullCallback())
      : web_view_helper_(std::move(create_web_frame_callback)) {}

  void SetUp() override {
    test_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    // Advance clock so time is not 0.
    test_task_runner_->FastForwardBy(base::Seconds(1));
    EventTiming::SetTickClockForTesting(test_task_runner_->GetMockTickClock());
  }

  void TearDown() override {
    EventTiming::SetTickClockForTesting(nullptr);
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
    web_view_helper_.Reset();
    MemoryCache::Get()->EvictResources();
    // Clear lazily loaded style sheets.
    CSSDefaultStyleSheets::Instance().PrepareForLeakDetection();
    ThreadState::Current()->CollectAllGarbageForTesting();
  }

 protected:
  void SetViewportSize(const gfx::Size& size) {
    cc::LayerTreeHost* layer_tree_host = web_view_helper_.GetLayerTreeHost();
    layer_tree_host->SetViewportRectAndScale(
        gfx::Rect(size), /*device_scale_factor=*/1.f,
        layer_tree_host->local_surface_id_from_parent());
  }

  std::string RegisterMockedHttpURLLoad(const std::string& file_name) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |web_view_helper_|.
    return url_test_helpers::RegisterMockedURLLoadFromBase(
               WebString::FromUTF8(base_url_), test::CoreTestDataPath(),
               WebString::FromUTF8(file_name))
        .GetString()
        .Utf8();
  }

  void TestAutoResize(const gfx::Size& min_auto_resize,
                      const gfx::Size& max_auto_resize,
                      const std::string& page_width,
                      const std::string& page_height,
                      int expected_width,
                      int expected_height,
                      HorizontalScrollbarState expected_horizontal_state,
                      VerticalScrollbarState expected_vertical_state);

  void TestTextInputType(WebTextInputType expected_type,
                         const std::string& html_file);
  void TestInputMode(WebTextInputMode expected_input_mode,
                     const std::string& html_file);
  void TestInputAction(ui::TextInputAction expected_input_action,
                       const std::string& html_file);
  bool SimulateGestureAtElement(WebInputEvent::Type, Element*);
  bool SimulateGestureAtElementById(WebInputEvent::Type, const WebString& id);
  WebGestureEvent BuildTapEvent(WebInputEvent::Type,
                                int tap_event_count,
                                const gfx::PointF& position_in_widget);
  bool SimulateTapEventAtElement(WebInputEvent::Type,
                                 int tap_event_count,
                                 Element*);
  bool SimulateTapEventAtElementById(WebInputEvent::Type,
                                     int tap_event_count,
                                     const WebString& id);

  ExternalDateTimeChooser* GetExternalDateTimeChooser(
      WebViewImpl* web_view_impl);

  void UpdateAllLifecyclePhases() {
    web_view_helper_.GetWebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  InteractiveDetector* GetTestInteractiveDetector(Document& document) {
    InteractiveDetector* detector(InteractiveDetector::From(document));
    EXPECT_NE(nullptr, detector);
    detector->SetTaskRunnerForTesting(test_task_runner_);
    detector->SetTickClockForTesting(test_task_runner_->GetMockTickClock());
    return detector;
  }

  std::string base_url_{"http://www.test.com/"};
  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper web_view_helper_;
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
};

static bool HitTestIsContentEditable(WebView* view, int x, int y) {
  gfx::PointF hit_point(x, y);
  WebHitTestResult hit_test_result =
      view->MainFrameWidget()->HitTestResultAt(hit_point);
  return hit_test_result.IsContentEditable();
}

static std::string HitTestElementId(WebView* view, int x, int y) {
  gfx::PointF hit_point(x, y);
  WebHitTestResult hit_test_result =
      view->MainFrameWidget()->HitTestResultAt(hit_point);
  return hit_test_result.GetNode().To<WebElement>().GetAttribute("id").Utf8();
}

static Color OutlineColor(Element* element) {
  return element->GetComputedStyle()->VisitedDependentColor(
      GetCSSPropertyOutlineColor());
}

TEST_F(WebViewTest, HitTestContentEditableImageMaps) {
  std::string url =
      RegisterMockedHttpURLLoad("content-editable-image-maps.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(url);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 500));

  EXPECT_EQ("areaANotEditable", HitTestElementId(web_view, 25, 25));
  EXPECT_FALSE(HitTestIsContentEditable(web_view, 25, 25));
  EXPECT_EQ("imageANotEditable", HitTestElementId(web_view, 75, 25));
  EXPECT_FALSE(HitTestIsContentEditable(web_view, 75, 25));

  EXPECT_EQ("areaBNotEditable", HitTestElementId(web_view, 25, 125));
  EXPECT_FALSE(HitTestIsContentEditable(web_view, 25, 125));
  EXPECT_EQ("imageBEditable", HitTestElementId(web_view, 75, 125));
  EXPECT_TRUE(HitTestIsContentEditable(web_view, 75, 125));

  EXPECT_EQ("areaCNotEditable", HitTestElementId(web_view, 25, 225));
  EXPECT_FALSE(HitTestIsContentEditable(web_view, 25, 225));
  EXPECT_EQ("imageCNotEditable", HitTestElementId(web_view, 75, 225));
  EXPECT_FALSE(HitTestIsContentEditable(web_view, 75, 225));

  EXPECT_EQ("areaDEditable", HitTestElementId(web_view, 25, 325));
  EXPECT_TRUE(HitTestIsContentEditable(web_view, 25, 325));
  EXPECT_EQ("imageDNotEditable", HitTestElementId(web_view, 75, 325));
  EXPECT_FALSE(HitTestIsContentEditable(web_view, 75, 325));
}

static std::string HitTestAbsoluteUrl(WebView* view, int x, int y) {
  gfx::PointF hit_point(x, y);
  WebHitTestResult hit_test_result =
      view->MainFrameWidget()->HitTestResultAt(hit_point);
  return hit_test_result.AbsoluteImageURL().GetString().Utf8();
}

static WebElement HitTestUrlElement(WebView* view, int x, int y) {
  gfx::PointF hit_point(x, y);
  WebHitTestResult hit_test_result =
      view->MainFrameWidget()->HitTestResultAt(hit_point);
  return hit_test_result.UrlElement();
}

TEST_F(WebViewTest, ImageMapUrls) {
  std::string url = RegisterMockedHttpURLLoad("image-map.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(url);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(400, 400));

  std::string image_url =
      "data:image/gif;base64,R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs=";

  EXPECT_EQ("area", HitTestElementId(web_view, 25, 25));
  EXPECT_EQ("area",
            HitTestUrlElement(web_view, 25, 25).GetAttribute("id").Utf8());
  EXPECT_EQ(image_url, HitTestAbsoluteUrl(web_view, 25, 25));

  EXPECT_EQ("image", HitTestElementId(web_view, 75, 25));
  EXPECT_TRUE(HitTestUrlElement(web_view, 75, 25).IsNull());
  EXPECT_EQ(image_url, HitTestAbsoluteUrl(web_view, 75, 25));
}

TEST_F(WebViewTest, BrokenImage) {
  url_test_helpers::RegisterMockedErrorURLLoad(
      KURL(ToKURL(base_url_), "non_existent.png"));
  std::string url = RegisterMockedHttpURLLoad("image-broken.html");

  WebViewImpl* web_view = web_view_helper_.Initialize();
  web_view->GetSettings()->SetLoadsImagesAutomatically(true);
  LoadFrame(web_view->MainFrameImpl(), url);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(400, 400));

  std::string image_url = "http://www.test.com/non_existent.png";

  EXPECT_EQ("image", HitTestElementId(web_view, 25, 25));
  EXPECT_TRUE(HitTestUrlElement(web_view, 25, 25).IsNull());
  EXPECT_EQ(image_url, HitTestAbsoluteUrl(web_view, 25, 25));
}

TEST_F(WebViewTest, BrokenInputImage) {
  url_test_helpers::RegisterMockedErrorURLLoad(
      KURL(ToKURL(base_url_), "non_existent.png"));
  std::string url = RegisterMockedHttpURLLoad("input-image-broken.html");

  WebViewImpl* web_view = web_view_helper_.Initialize();
  web_view->GetSettings()->SetLoadsImagesAutomatically(true);
  LoadFrame(web_view->MainFrameImpl(), url);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(400, 400));

  std::string image_url = "http://www.test.com/non_existent.png";

  EXPECT_EQ("image", HitTestElementId(web_view, 25, 25));
  EXPECT_TRUE(HitTestUrlElement(web_view, 25, 25).IsNull());
  EXPECT_EQ(image_url, HitTestAbsoluteUrl(web_view, 25, 25));
}

TEST_F(WebViewTest, SetBaseBackgroundColor) {
  const SkColor kDarkCyan = SkColorSetARGB(0xFF, 0x22, 0x77, 0x88);
  const SkColor kTranslucentPutty = SkColorSetARGB(0x80, 0xBF, 0xB1, 0x96);

  WebViewImpl* web_view = web_view_helper_.Initialize();
  EXPECT_EQ(SK_ColorWHITE, web_view->BackgroundColor());

  web_view->SetPageBaseBackgroundColor(SK_ColorBLUE);
  EXPECT_EQ(SK_ColorBLUE, web_view->BackgroundColor());

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(
      web_view->MainFrameImpl(),
      "<html><head><style>body "
      "{background-color:#227788}</style></head></"
      "html>",
      base_url);
  EXPECT_EQ(kDarkCyan, web_view->BackgroundColor());

  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(),
                                     "<html><head><style>body "
                                     "{background-color:rgba(255,0,0,0.5)}</"
                                     "style></head></html>",
                                     base_url);
  // Expected: red (50% alpha) blended atop base of SK_ColorBLUE.
  EXPECT_EQ(0xFF80007F, web_view->BackgroundColor());

  web_view->SetPageBaseBackgroundColor(kTranslucentPutty);
  // Expected: red (50% alpha) blended atop kTranslucentPutty. Note the alpha.
  EXPECT_EQ(0xBFE93A31, web_view->BackgroundColor());

  web_view->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(),
                                     "<html><head><style>body "
                                     "{background-color:transparent}</style></"
                                     "head></html>",
                                     base_url);
  // Expected: transparent on top of transparent will still be transparent.
  EXPECT_EQ(SK_ColorTRANSPARENT, web_view->BackgroundColor());

  LocalFrame* frame = web_view->MainFrameImpl()->GetFrame();
  // The shutdown() calls are a hack to prevent this test
  // from violating invariants about frame state during navigation/detach.
  frame->GetDocument()->Shutdown();

  // Creating a new frame view with the background color having 0 alpha.
  frame->CreateView(gfx::Size(1024, 768), Color::kTransparent);
  EXPECT_EQ(Color::kTransparent, frame->View()->BaseBackgroundColor());
  frame->View()->Dispose();

  const Color transparent_red(100, 0, 0, 0);
  frame->CreateView(gfx::Size(1024, 768), transparent_red);
  EXPECT_EQ(transparent_red, frame->View()->BaseBackgroundColor());
  frame->View()->Dispose();
}

TEST_F(WebViewTest, SetBaseBackgroundColorBeforeMainFrame) {
  // Note: this test doesn't use WebViewHelper since it intentionally runs
  // initialization code between WebView and WebLocalFrame creation.
  WebViewClient web_view_client;
  WebViewImpl* web_view = web_view_helper_.CreateWebView(
      &web_view_client, /*compositing_enabled=*/true);
  EXPECT_NE(SK_ColorBLUE, web_view->BackgroundColor());
  // WebView does not have a frame yet; while it's possible to set the page
  // background color, it won't have any effect until a local main frame is
  // attached.
  web_view->SetPageBaseBackgroundColor(SK_ColorBLUE);
  EXPECT_NE(SK_ColorBLUE, web_view->BackgroundColor());

  frame_test_helpers::TestWebFrameClient web_frame_client;
  WebLocalFrame* frame = WebLocalFrame::CreateMainFrame(
      web_view, &web_frame_client, nullptr, mojo::NullRemote(),
      LocalFrameToken(), DocumentToken(), nullptr);
  web_frame_client.Bind(frame);

  frame_test_helpers::TestWebFrameWidget* widget =
      web_view_helper_.CreateFrameWidgetAndInitializeCompositing(frame);
  web_view->DidAttachLocalMainFrame();

  // The color should be passed to the compositor.
  cc::LayerTreeHost* host = widget->LayerTreeHostForTesting();
  EXPECT_EQ(SK_ColorBLUE, web_view->BackgroundColor());
  EXPECT_EQ(SkColors::kBlue, host->background_color());

  web_view->Close();
}

TEST_F(WebViewTest, SetBaseBackgroundColorAndBlendWithExistingContent) {
  const SkColor kAlphaRed = SkColorSetARGB(0x80, 0xFF, 0x00, 0x00);
  const SkColor kAlphaGreen = SkColorSetARGB(0x80, 0x00, 0xFF, 0x00);
  const int kWidth = 100;
  const int kHeight = 100;

  WebViewImpl* web_view = web_view_helper_.Initialize();

  // Set WebView background to green with alpha.
  web_view->SetPageBaseBackgroundColor(kAlphaGreen);
  web_view->GetSettings()->SetShouldClearDocumentBackground(false);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(kWidth, kHeight));
  UpdateAllLifecyclePhases();

  // Set canvas background to red with alpha.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kWidth, kHeight);
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.clear(kAlphaRed);

  PaintRecordBuilder builder;

  // Paint the root of the main frame in the way that CompositedLayerMapping
  // would.
  LocalFrameView* view = web_view_helper_.LocalMainFrame()->GetFrameView();
  PaintLayer* root_layer = view->GetLayoutView()->Layer();

  view->GetLayoutView()->GetDocument().Lifecycle().AdvanceTo(
      DocumentLifecycle::kInPaint);
  PaintLayerPainter(*root_layer).Paint(builder.Context());
  view->GetLayoutView()->GetDocument().Lifecycle().AdvanceTo(
      DocumentLifecycle::kPaintClean);
  builder.EndRecording().Playback(&canvas);

  // The result should be a blend of red and green.
  SkColor color = bitmap.getColor(kWidth / 2, kHeight / 2);
  EXPECT_TRUE(SkColorGetR(color));
  EXPECT_TRUE(SkColorGetG(color));
}

TEST_F(WebViewTest, SetBaseBackgroundColorWithColorScheme) {
  WebViewImpl* web_view = web_view_helper_.Initialize();
  ColorSchemeHelper color_scheme_helper(*(web_view->GetPage()));
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kLight);
  web_view->SetPageBaseBackgroundColor(SK_ColorBLUE);

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(
      web_view->MainFrameImpl(),
      "<style>:root { color-scheme: light dark }<style>", base_url);
  UpdateAllLifecyclePhases();

  LocalFrameView* frame_view = web_view->MainFrameImpl()->GetFrame()->View();
  EXPECT_EQ(Color(0, 0, 255), frame_view->BaseBackgroundColor());

  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color(0x12, 0x12, 0x12), frame_view->BaseBackgroundColor());

  // Don't let dark color-scheme override a transparent background.
  web_view->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  EXPECT_EQ(Color::kTransparent, frame_view->BaseBackgroundColor());
  web_view->SetPageBaseBackgroundColor(SK_ColorBLUE);
  EXPECT_EQ(Color(0x12, 0x12, 0x12), frame_view->BaseBackgroundColor());

  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  Document* document = frame->GetFrame()->GetDocument();
  CHECK(document);
  color_scheme_helper.SetInForcedColors(*document, /*in_forced_colors=*/true);
  UpdateAllLifecyclePhases();

  mojom::blink::ColorScheme color_scheme = mojom::blink::ColorScheme::kLight;
  Color system_background_color = LayoutTheme::GetTheme().SystemColor(
      CSSValueID::kCanvas, color_scheme,
      web_view->GetPage()->GetColorProviderForPainting(
          color_scheme, /*in_forced_colors=*/true),
      document->IsInWebAppScope());
  EXPECT_EQ(system_background_color, frame_view->BaseBackgroundColor());

  color_scheme_helper.SetInForcedColors(*document, /*in_forced_colors=*/false);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color(0x12, 0x12, 0x12), frame_view->BaseBackgroundColor());

  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kLight);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(Color(0, 0, 255), frame_view->BaseBackgroundColor());
}

TEST_F(WebViewTest, FocusIsInactive) {
  RegisterMockedHttpURLLoad("visible_iframe.html");
  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + "visible_iframe.html");

  web_view->MainFrameWidget()->SetFocus(true);
  web_view->SetIsActive(true);
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  EXPECT_TRUE(IsA<HTMLDocument>(frame->GetFrame()->GetDocument()));

  Document* document = frame->GetFrame()->GetDocument();
  EXPECT_TRUE(document->hasFocus());
  web_view->MainFrameWidget()->SetFocus(false);
  web_view->SetIsActive(false);
  EXPECT_FALSE(document->hasFocus());
  web_view->MainFrameWidget()->SetFocus(true);
  web_view->SetIsActive(true);
  EXPECT_TRUE(document->hasFocus());
  web_view->MainFrameWidget()->SetFocus(true);
  web_view->SetIsActive(false);
  EXPECT_FALSE(document->hasFocus());
  web_view->MainFrameWidget()->SetFocus(false);
  web_view->SetIsActive(true);
  EXPECT_FALSE(document->hasFocus());
  web_view->SetIsActive(false);
  web_view->MainFrameWidget()->SetFocus(true);
  EXPECT_TRUE(document->hasFocus());
  web_view->SetIsActive(true);
  web_view->MainFrameWidget()->SetFocus(false);
  EXPECT_FALSE(document->hasFocus());
}

TEST_F(WebViewTest, DocumentHasFocus) {
  WebViewImpl* web_view = web_view_helper_.Initialize();
  web_view->MainFrameWidget()->SetFocus(true);

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(
      web_view->MainFrameImpl(),
      "<input id=input></input>"
      "<div id=log></div>"
      "<script>"
      "  document.getElementById('input').addEventListener('focus', () => {"
      "    document.getElementById('log').textContent = 'document.hasFocus(): "
      "' + document.hasFocus();"
      "  });"
      "  document.getElementById('input').addEventListener('blur', () => {"
      "    document.getElementById('log').textContent = '';"
      "  });"
      "  document.getElementById('input').focus();"
      "</script>",
      base_url);

  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  Document* document = frame->GetFrame()->GetDocument();
  WebElement log_element = frame->GetDocument().GetElementById("log");
  EXPECT_TRUE(document->hasFocus());
  EXPECT_EQ("document.hasFocus(): true", log_element.TextContent());

  web_view->SetIsActive(false);
  web_view->MainFrameWidget()->SetFocus(false);
  EXPECT_FALSE(document->hasFocus());
  EXPECT_TRUE(log_element.TextContent().IsEmpty());

  web_view->MainFrameWidget()->SetFocus(true);
  EXPECT_TRUE(document->hasFocus());
  EXPECT_EQ("document.hasFocus(): true", log_element.TextContent());
}

TEST_F(WebViewTest, PlatformColorsChangedOnDeviceEmulation) {
  WebViewImpl* web_view_impl = web_view_helper_.Initialize();
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(
      web_view_impl->MainFrameImpl(),
      "<style>"
      "  span { outline-color: -webkit-focus-ring-color; }"
      "</style>"
      "<span id='span1'></span>",
      base_url);
  UpdateAllLifecyclePhases();

  DeviceEmulationParams params;
  params.screen_type = mojom::EmulatedScreenType::kMobile;

  Document& document =
      *web_view_impl->MainFrameImpl()->GetFrame()->GetDocument();

  Element* span1 = document.getElementById(AtomicString("span1"));
  ASSERT_TRUE(span1);

  // Check non-MobileLayoutTheme color.
  Color original = LayoutTheme::GetTheme().FocusRingColor(
      span1->ComputedStyleRef().UsedColorScheme());
  EXPECT_EQ(original, OutlineColor(span1));

  // Set the focus ring color for the mobile theme to something known.
  Color custom_color = Color::FromRGB(123, 145, 167);
  {
    ScopedMobileLayoutThemeForTest mobile_layout_theme_enabled(true);
    LayoutTheme::GetTheme().SetCustomFocusRingColor(custom_color);
  }

  EXPECT_NE(custom_color, original);
  web_view_impl->EnableDeviceEmulation(params);

  // All <span>s should have the custom outline color, and not (for example)
  // the original color fetched from cache.
  auto* span2 = MakeGarbageCollected<HTMLSpanElement>(document);
  document.body()->AppendChild(span2);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(custom_color, OutlineColor(span1));
  EXPECT_EQ(custom_color, OutlineColor(span2));

  // Disable mobile emulation. All <span>s should once again have the
  // original outline color.
  web_view_impl->DisableDeviceEmulation();
  auto* span3 = MakeGarbageCollected<HTMLSpanElement>(document);
  document.body()->AppendChild(span3);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(original, OutlineColor(span1));
  EXPECT_EQ(original, OutlineColor(span2));
  EXPECT_EQ(original, OutlineColor(span3));
}

TEST_F(WebViewTest, ActiveState) {
  RegisterMockedHttpURLLoad("visible_iframe.html");
  WebView* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + "visible_iframe.html");

  ASSERT_TRUE(web_view);

  web_view->SetIsActive(true);
  EXPECT_TRUE(web_view->IsActive());

  web_view->SetIsActive(false);
  EXPECT_FALSE(web_view->IsActive());

  web_view->SetIsActive(true);
  EXPECT_TRUE(web_view->IsActive());
}

TEST_F(WebViewTest, HitTestResultAtWithPageScale) {
  std::string url = base_url_ + "specify_size.html?" + "50px" + ":" + "50px";
  url_test_helpers::RegisterMockedURLLoad(
      ToKURL(url), test::CoreTestDataPath("specify_size.html"));
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(url);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(100, 100));
  gfx::PointF hit_point(75, 75);

  // Image is at top left quandrant, so should not hit it.
  WebHitTestResult negative_result =
      web_view->MainFrameWidget()->HitTestResultAt(hit_point);
  EXPECT_FALSE(
      negative_result.GetNode().To<WebElement>().HasHTMLTagName("img"));
  negative_result.Reset();

  // Scale page up 2x so image should occupy the whole viewport.
  web_view->SetPageScaleFactor(2.0f);
  WebHitTestResult positive_result =
      web_view->MainFrameWidget()->HitTestResultAt(hit_point);
  EXPECT_TRUE(positive_result.GetNode().To<WebElement>().HasHTMLTagName("img"));
  positive_result.Reset();
}

TEST_F(WebViewTest, HitTestResultAtWithPageScaleAndPan) {
  std::string url = base_url_ + "specify_size.html?" + "50px" + ":" + "50px";
  url_test_helpers::RegisterMockedURLLoad(
      ToKURL(url), test::CoreTestDataPath("specify_size.html"));
  WebViewImpl* web_view = web_view_helper_.Initialize();
  LoadFrame(web_view->MainFrameImpl(), url);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(100, 100));
  gfx::PointF hit_point(75, 75);

  // Image is at top left quandrant, so should not hit it.
  WebHitTestResult negative_result =
      web_view->MainFrameWidget()->HitTestResultAt(hit_point);
  EXPECT_FALSE(
      negative_result.GetNode().To<WebElement>().HasHTMLTagName("img"));
  negative_result.Reset();

  // Scale page up 2x so image should occupy the whole viewport.
  web_view->SetPageScaleFactor(2.0f);
  WebHitTestResult positive_result =
      web_view->MainFrameWidget()->HitTestResultAt(hit_point);
  EXPECT_TRUE(positive_result.GetNode().To<WebElement>().HasHTMLTagName("img"));
  positive_result.Reset();

  // Pan around the zoomed in page so the image is not visible in viewport.
  web_view->SetVisualViewportOffset(gfx::PointF(100, 100));
  WebHitTestResult negative_result2 =
      web_view->MainFrameWidget()->HitTestResultAt(hit_point);
  EXPECT_FALSE(
      negative_result2.GetNode().To<WebElement>().HasHTMLTagName("img"));
  negative_result2.Reset();
}

TEST_F(WebViewTest, HitTestResultForTapWithTapArea) {
  std::string url = RegisterMockedHttpURLLoad("hit_test.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(url);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(100, 100));
  gfx::Point hit_point(55, 55);

  // Image is at top left quandrant, so should not hit it.
  WebHitTestResult negative_result =
      web_view->MainFrameWidget()->HitTestResultAt(gfx::PointF(hit_point));
  EXPECT_FALSE(
      negative_result.GetNode().To<WebElement>().HasHTMLTagName("img"));
  negative_result.Reset();

  // The tap area is 20 by 20 square, centered at 55, 55.
  gfx::Size tap_area(20, 20);
  WebHitTestResult positive_result =
      web_view->HitTestResultForTap(hit_point, tap_area);
  EXPECT_TRUE(positive_result.GetNode().To<WebElement>().HasHTMLTagName("img"));
  positive_result.Reset();

  // Move the hit point the image is just outside the tapped area now.
  hit_point = gfx::Point(61, 61);
  WebHitTestResult negative_result2 =
      web_view->HitTestResultForTap(hit_point, tap_area);
  EXPECT_FALSE(
      negative_result2.GetNode().To<WebElement>().HasHTMLTagName("img"));
  negative_result2.Reset();
}

TEST_F(WebViewTest, HitTestResultForTapWithTapAreaPageScaleAndPan) {
  std::string url = RegisterMockedHttpURLLoad("hit_test.html");
  WebViewImpl* web_view = web_view_helper_.Initialize();
  LoadFrame(web_view->MainFrameImpl(), url);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(100, 100));
  gfx::Point hit_point(55, 55);

  // Image is at top left quandrant, so should not hit it.
  WebHitTestResult negative_result =
      web_view->MainFrameWidget()->HitTestResultAt(gfx::PointF(hit_point));
  EXPECT_FALSE(
      negative_result.GetNode().To<WebElement>().HasHTMLTagName("img"));
  negative_result.Reset();

  // The tap area is 20 by 20 square, centered at 55, 55.
  gfx::Size tap_area(20, 20);
  WebHitTestResult positive_result =
      web_view->HitTestResultForTap(hit_point, tap_area);
  EXPECT_TRUE(positive_result.GetNode().To<WebElement>().HasHTMLTagName("img"));
  positive_result.Reset();

  // Zoom in and pan around the page so the image is not visible in viewport.
  web_view->SetPageScaleFactor(2.0f);
  web_view->SetVisualViewportOffset(gfx::PointF(100, 100));
  WebHitTestResult negative_result2 =
      web_view->HitTestResultForTap(hit_point, tap_area);
  EXPECT_FALSE(
      negative_result2.GetNode().To<WebElement>().HasHTMLTagName("img"));
  negative_result2.Reset();
}

void WebViewTest::TestAutoResize(
    const gfx::Size& min_auto_resize,
    const gfx::Size& max_auto_resize,
    const std::string& page_width,
    const std::string& page_height,
    int expected_width,
    int expected_height,
    HorizontalScrollbarState expected_horizontal_state,
    VerticalScrollbarState expected_vertical_state) {
  AutoResizeWebViewClient client;
  std::string url =
      base_url_ + "specify_size.html?" + page_width + ":" + page_height;
  url_test_helpers::RegisterMockedURLLoad(
      ToKURL(url), test::CoreTestDataPath("specify_size.html"));
  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(url, nullptr, &client);
  client.GetTestData().SetWebView(web_view);

  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  LocalFrameView* frame_view = frame->GetFrame()->View();
  frame_view->UpdateStyleAndLayout();
  EXPECT_FALSE(frame_view->LayoutPending());
  EXPECT_FALSE(frame_view->NeedsLayout());

  web_view->EnableAutoResizeMode(min_auto_resize, max_auto_resize);
  EXPECT_TRUE(frame_view->LayoutPending());
  EXPECT_TRUE(frame_view->NeedsLayout());
  frame_view->UpdateStyleAndLayout();

  EXPECT_TRUE(frame->GetFrame()->GetDocument()->IsHTMLDocument());

  EXPECT_EQ(expected_width, client.GetTestData().Width());
  EXPECT_EQ(expected_height, client.GetTestData().Height());

// Android disables main frame scrollbars.
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(expected_horizontal_state,
            client.GetTestData().GetHorizontalScrollbarState());
  EXPECT_EQ(expected_vertical_state,
            client.GetTestData().GetVerticalScrollbarState());
#endif

  // Explicitly reset to break dependency on locally scoped client.
  web_view_helper_.Reset();
}

TEST_F(WebViewTest, AutoResizeMinimumSize) {
  gfx::Size min_auto_resize(91, 56);
  gfx::Size max_auto_resize(403, 302);
  std::string page_width = "91px";
  std::string page_height = "56px";
  int expected_width = 91;
  int expected_height = 56;
  TestAutoResize(min_auto_resize, max_auto_resize, page_width, page_height,
                 expected_width, expected_height, kNoHorizontalScrollbar,
                 kNoVerticalScrollbar);
}

TEST_F(WebViewTest, AutoResizeHeightOverflowAndFixedWidth) {
  gfx::Size min_auto_resize(90, 95);
  gfx::Size max_auto_resize(90, 100);
  std::string page_width = "60px";
  std::string page_height = "200px";
  int expected_width = 90;
  int expected_height = 100;
  TestAutoResize(min_auto_resize, max_auto_resize, page_width, page_height,
                 expected_width, expected_height, kNoHorizontalScrollbar,
                 kVisibleVerticalScrollbar);
}

TEST_F(WebViewTest, AutoResizeFixedHeightAndWidthOverflow) {
  gfx::Size min_auto_resize(90, 100);
  gfx::Size max_auto_resize(200, 100);
  std::string page_width = "300px";
  std::string page_height = "80px";
  int expected_width = 200;
  int expected_height = 100;
  TestAutoResize(min_auto_resize, max_auto_resize, page_width, page_height,
                 expected_width, expected_height, kVisibleHorizontalScrollbar,
                 kNoVerticalScrollbar);
}

// Next three tests disabled for https://bugs.webkit.org/show_bug.cgi?id=92318 .
// It seems we can run three AutoResize tests, then the next one breaks.
TEST_F(WebViewTest, AutoResizeInBetweenSizes) {
  gfx::Size min_auto_resize(90, 95);
  gfx::Size max_auto_resize(200, 300);
  std::string page_width = "100px";
  std::string page_height = "200px";
  int expected_width = 100;
  int expected_height = 200;
  TestAutoResize(min_auto_resize, max_auto_resize, page_width, page_height,
                 expected_width, expected_height, kNoHorizontalScrollbar,
                 kNoVerticalScrollbar);
}

TEST_F(WebViewTest, AutoResizeOverflowSizes) {
  gfx::Size min_auto_resize(90, 95);
  gfx::Size max_auto_resize(200, 300);
  std::string page_width = "300px";
  std::string page_height = "400px";
  int expected_width = 200;
  int expected_height = 300;
  TestAutoResize(min_auto_resize, max_auto_resize, page_width, page_height,
                 expected_width, expected_height, kVisibleHorizontalScrollbar,
                 kVisibleVerticalScrollbar);
}

TEST_F(WebViewTest, AutoResizeMaxSize) {
  gfx::Size min_auto_resize(90, 95);
  gfx::Size max_auto_resize(200, 300);
  std::string page_width = "200px";
  std::string page_height = "300px";
  int expected_width = 200;
  int expected_height = 300;
  TestAutoResize(min_auto_resize, max_auto_resize, page_width, page_height,
                 expected_width, expected_height, kNoHorizontalScrollbar,
                 kNoVerticalScrollbar);
}

void WebViewTest::TestTextInputType(WebTextInputType expected_type,
                                    const std::string& html_file) {
  RegisterMockedHttpURLLoad(html_file);
  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + html_file);
  WebInputMethodController* controller =
      web_view->MainFrameImpl()->GetInputMethodController();
  EXPECT_EQ(kWebTextInputTypeNone, controller->TextInputType());
  EXPECT_EQ(kWebTextInputTypeNone, controller->TextInputInfo().type);
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  EXPECT_EQ(expected_type, controller->TextInputType());
  EXPECT_EQ(expected_type, controller->TextInputInfo().type);
  web_view->FocusedElement()->blur();
  EXPECT_EQ(kWebTextInputTypeNone, controller->TextInputType());
  EXPECT_EQ(kWebTextInputTypeNone, controller->TextInputInfo().type);
}

TEST_F(WebViewTest, TextInputType) {
  TestTextInputType(kWebTextInputTypeText, "input_field_default.html");
  TestTextInputType(kWebTextInputTypePassword, "input_field_password.html");
  TestTextInputType(kWebTextInputTypeEmail, "input_field_email.html");
  TestTextInputType(kWebTextInputTypeSearch, "input_field_search.html");
  TestTextInputType(kWebTextInputTypeNumber, "input_field_number.html");
  TestTextInputType(kWebTextInputTypeTelephone, "input_field_tel.html");
  TestTextInputType(kWebTextInputTypeURL, "input_field_url.html");
}

TEST_F(WebViewTest, TextInputInfoUpdateStyleAndLayout) {
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.Initialize();

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  // Here, we need to construct a document that has a special property:
  // Adding id="foo" to the <path> element will trigger creation of an SVG
  // instance tree for the use <use> element.
  // This is significant, because SVG instance trees are actually created lazily
  // during Document::updateStyleAndLayout code, thus incrementing the DOM tree
  // version and freaking out the EphemeralRange (invalidating it).
  frame_test_helpers::LoadHTMLString(
      web_view_impl->MainFrameImpl(),
      "<svg height='100%' version='1.1' viewBox='0 0 14 14' width='100%'>"
      "<use xmlns:xlink='http://www.w3.org/1999/xlink' xlink:href='#foo'></use>"
      "<path d='M 100 100 L 300 100 L 200 300 z' fill='#000'></path>"
      "</svg>"
      "<input>",
      base_url);
  web_view_impl->MainFrameImpl()->GetFrame()->SetInitialFocus(false);

  // Add id="foo" to <path>, thus triggering the condition described above.
  Document* document =
      web_view_impl->MainFrameImpl()->GetFrame()->GetDocument();
  document->body()
      ->QuerySelector(AtomicString("path"), ASSERT_NO_EXCEPTION)
      ->SetIdAttribute(AtomicString("foo"));

  // This should not DCHECK.
  EXPECT_EQ(kWebTextInputTypeText, web_view_impl->MainFrameImpl()
                                       ->GetInputMethodController()
                                       ->TextInputInfo()
                                       .type);
}

void WebViewTest::TestInputMode(WebTextInputMode expected_input_mode,
                                const std::string& html_file) {
  RegisterMockedHttpURLLoad(html_file);
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad(base_url_ + html_file);
  web_view_impl->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  EXPECT_EQ(expected_input_mode, web_view_impl->MainFrameImpl()
                                     ->GetInputMethodController()
                                     ->TextInputInfo()
                                     .input_mode);
}

TEST_F(WebViewTest, InputMode) {
  TestInputMode(WebTextInputMode::kWebTextInputModeDefault,
                "input_mode_default.html");
  TestInputMode(WebTextInputMode::kWebTextInputModeDefault,
                "input_mode_default_unknown.html");
  TestInputMode(WebTextInputMode::kWebTextInputModeNone,
                "input_mode_type_none.html");
  TestInputMode(WebTextInputMode::kWebTextInputModeText,
                "input_mode_type_text.html");
  TestInputMode(WebTextInputMode::kWebTextInputModeTel,
                "input_mode_type_tel.html");
  TestInputMode(WebTextInputMode::kWebTextInputModeUrl,
                "input_mode_type_url.html");
  TestInputMode(WebTextInputMode::kWebTextInputModeEmail,
                "input_mode_type_email.html");
  TestInputMode(WebTextInputMode::kWebTextInputModeNumeric,
                "input_mode_type_numeric.html");
  TestInputMode(WebTextInputMode::kWebTextInputModeDecimal,
                "input_mode_type_decimal.html");
  TestInputMode(WebTextInputMode::kWebTextInputModeSearch,
                "input_mode_type_search.html");
}

void WebViewTest::TestInputAction(ui::TextInputAction expected_input_action,
                                  const std::string& html_file) {
  RegisterMockedHttpURLLoad(html_file);
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad(base_url_ + html_file);
  web_view_impl->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  EXPECT_EQ(expected_input_action, web_view_impl->MainFrameImpl()
                                       ->GetInputMethodController()
                                       ->TextInputInfo()
                                       .action);
}

TEST_F(WebViewTest, TextInputAction) {
  TestInputAction(ui::TextInputAction::kDefault, "enter_key_hint_default.html");
  TestInputAction(ui::TextInputAction::kDefault,
                  "enter_key_hint_default_unknown.html");
  TestInputAction(ui::TextInputAction::kEnter, "enter_key_hint_enter.html");
  TestInputAction(ui::TextInputAction::kGo, "enter_key_hint_go.html");
  TestInputAction(ui::TextInputAction::kDone, "enter_key_hint_done.html");
  TestInputAction(ui::TextInputAction::kNext, "enter_key_hint_next.html");
  TestInputAction(ui::TextInputAction::kPrevious,
                  "enter_key_hint_previous.html");
  TestInputAction(ui::TextInputAction::kSearch, "enter_key_hint_search.html");
  TestInputAction(ui::TextInputAction::kSend, "enter_key_hint_send.html");
  TestInputAction(ui::TextInputAction::kNext, "enter_key_hint_mixed_case.html");
}

TEST_F(WebViewTest, TextInputInfoWithReplacedElements) {
  std::string url = RegisterMockedHttpURLLoad("div_with_image.html");
  url_test_helpers::RegisterMockedURLLoad(
      ToKURL("http://www.test.com/foo.png"),
      test::CoreTestDataPath("white-1x1.png"));
  WebViewImpl* web_view_impl = web_view_helper_.InitializeAndLoad(url);
  web_view_impl->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  WebTextInputInfo info = web_view_impl->MainFrameImpl()
                              ->GetInputMethodController()
                              ->TextInputInfo();

  EXPECT_EQ("foo\xef\xbf\xbc", info.value.Utf8());
}

TEST_F(WebViewTest, SetEditableSelectionOffsetsAndTextInputInfo) {
  RegisterMockedHttpURLLoad("input_field_populated.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "input_field_populated.html");
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  WebInputMethodController* active_input_method_controller =
      frame->GetInputMethodController();
  frame->SetEditableSelectionOffsets(5, 13);
  EXPECT_EQ("56789abc", frame->SelectionAsText());
  WebTextInputInfo info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("0123456789abcdefghijklmnopqrstuvwxyz", info.value);
  EXPECT_EQ(5, info.selection_start);
  EXPECT_EQ(13, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);

  RegisterMockedHttpURLLoad("content_editable_populated.html");
  web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "content_editable_populated.html");
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  frame = web_view->MainFrameImpl();
  active_input_method_controller = frame->GetInputMethodController();
  frame->SetEditableSelectionOffsets(8, 19);
  EXPECT_EQ("89abcdefghi", frame->SelectionAsText());
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("0123456789abcdefghijklmnopqrstuvwxyz", info.value);
  EXPECT_EQ(8, info.selection_start);
  EXPECT_EQ(19, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);
}

// Regression test for crbug.com/663645
TEST_F(WebViewTest, FinishComposingTextDoesNotAssert) {
  RegisterMockedHttpURLLoad("input_field_default.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "input_field_default.html");
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);

  WebInputMethodController* active_input_method_controller =
      web_view->MainFrameImpl()
          ->FrameWidget()
          ->GetActiveWebInputMethodController();

  // The test requires non-empty composition.
  std::string composition_text("hello");
  WebVector<ui::ImeTextSpan> empty_ime_text_spans;
  active_input_method_controller->SetComposition(
      WebString::FromUTF8(composition_text), empty_ime_text_spans, WebRange(),
      5, 5);

  // Do arbitrary change to make layout dirty.
  Document& document = *web_view->MainFrameImpl()->GetFrame()->GetDocument();
  Element* br = document.CreateRawElement(html_names::kBrTag);
  document.body()->AppendChild(br);

  // Should not hit assertion when calling
  // WebInputMethodController::finishComposingText with non-empty composition
  // and dirty layout.
  active_input_method_controller->FinishComposingText(
      WebInputMethodController::kKeepSelection);
}

// Regression test for https://crbug.com/873999
TEST_F(WebViewTest, LongPressOutsideInputShouldNotSelectPlaceholderText) {
  RegisterMockedHttpURLLoad("input_placeholder.html");
  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + "input_placeholder.html");
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebString input_id = WebString::FromUTF8("input");

  // Focus in input.
  EXPECT_TRUE(
      SimulateGestureAtElementById(WebInputEvent::Type::kGestureTap, input_id));

  // Long press below input.
  WebGestureEvent event(WebInputEvent::Type::kGestureLongPress,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(gfx::PointF(100, 150));
  EXPECT_EQ(WebInputEventResult::kHandledSystem,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(event, ui::LatencyInfo())));
  EXPECT_TRUE(web_view->MainFrameImpl()->SelectionAsText().IsEmpty());
}

TEST_F(WebViewTest, FinishComposingTextCursorPositionChange) {
  RegisterMockedHttpURLLoad("input_field_populated.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "input_field_populated.html");
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);

  // Set up a composition that needs to be committed.
  std::string composition_text("hello");

  WebInputMethodController* active_input_method_controller =
      web_view->MainFrameImpl()
          ->FrameWidget()
          ->GetActiveWebInputMethodController();
  WebVector<ui::ImeTextSpan> empty_ime_text_spans;
  active_input_method_controller->SetComposition(
      WebString::FromUTF8(composition_text), empty_ime_text_spans, WebRange(),
      3, 3);

  WebTextInputInfo info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("hello", info.value.Utf8());
  EXPECT_EQ(3, info.selection_start);
  EXPECT_EQ(3, info.selection_end);
  EXPECT_EQ(0, info.composition_start);
  EXPECT_EQ(5, info.composition_end);

  active_input_method_controller->FinishComposingText(
      WebInputMethodController::kKeepSelection);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ(3, info.selection_start);
  EXPECT_EQ(3, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);

  active_input_method_controller->SetComposition(
      WebString::FromUTF8(composition_text), empty_ime_text_spans, WebRange(),
      3, 3);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("helhellolo", info.value.Utf8());
  EXPECT_EQ(6, info.selection_start);
  EXPECT_EQ(6, info.selection_end);
  EXPECT_EQ(3, info.composition_start);
  EXPECT_EQ(8, info.composition_end);

  active_input_method_controller->FinishComposingText(
      WebInputMethodController::kDoNotKeepSelection);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ(8, info.selection_start);
  EXPECT_EQ(8, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);
}

TEST_F(WebViewTest, SetCompositionForNewCaretPositions) {
  RegisterMockedHttpURLLoad("input_field_populated.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "input_field_populated.html");
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  WebInputMethodController* active_input_method_controller =
      web_view->MainFrameImpl()
          ->FrameWidget()
          ->GetActiveWebInputMethodController();

  WebVector<ui::ImeTextSpan> empty_ime_text_spans;

  active_input_method_controller->CommitText("hello", empty_ime_text_spans,
                                             WebRange(), 0);
  active_input_method_controller->CommitText("world", empty_ime_text_spans,
                                             WebRange(), -5);
  WebTextInputInfo info = active_input_method_controller->TextInputInfo();

  EXPECT_EQ("helloworld", info.value.Utf8());
  EXPECT_EQ(5, info.selection_start);
  EXPECT_EQ(5, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);

  // Set up a composition that needs to be committed.
  std::string composition_text("ABC");

  // Caret is on the left of composing text.
  active_input_method_controller->SetComposition(
      WebString::FromUTF8(composition_text), empty_ime_text_spans, WebRange(),
      0, 0);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("helloABCworld", info.value.Utf8());
  EXPECT_EQ(5, info.selection_start);
  EXPECT_EQ(5, info.selection_end);
  EXPECT_EQ(5, info.composition_start);
  EXPECT_EQ(8, info.composition_end);

  // Caret is on the right of composing text.
  active_input_method_controller->SetComposition(
      WebString::FromUTF8(composition_text), empty_ime_text_spans, WebRange(),
      3, 3);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("helloABCworld", info.value.Utf8());
  EXPECT_EQ(8, info.selection_start);
  EXPECT_EQ(8, info.selection_end);
  EXPECT_EQ(5, info.composition_start);
  EXPECT_EQ(8, info.composition_end);

  // Caret is between composing text and left boundary.
  active_input_method_controller->SetComposition(
      WebString::FromUTF8(composition_text), empty_ime_text_spans, WebRange(),
      -2, -2);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("helloABCworld", info.value.Utf8());
  EXPECT_EQ(3, info.selection_start);
  EXPECT_EQ(3, info.selection_end);
  EXPECT_EQ(5, info.composition_start);
  EXPECT_EQ(8, info.composition_end);

  // Caret is between composing text and right boundary.
  active_input_method_controller->SetComposition(
      WebString::FromUTF8(composition_text), empty_ime_text_spans, WebRange(),
      5, 5);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("helloABCworld", info.value.Utf8());
  EXPECT_EQ(10, info.selection_start);
  EXPECT_EQ(10, info.selection_end);
  EXPECT_EQ(5, info.composition_start);
  EXPECT_EQ(8, info.composition_end);

  // Caret is on the left boundary.
  active_input_method_controller->SetComposition(
      WebString::FromUTF8(composition_text), empty_ime_text_spans, WebRange(),
      -5, -5);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("helloABCworld", info.value.Utf8());
  EXPECT_EQ(0, info.selection_start);
  EXPECT_EQ(0, info.selection_end);
  EXPECT_EQ(5, info.composition_start);
  EXPECT_EQ(8, info.composition_end);

  // Caret is on the right boundary.
  active_input_method_controller->SetComposition(
      WebString::FromUTF8(composition_text), empty_ime_text_spans, WebRange(),
      8, 8);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("helloABCworld", info.value.Utf8());
  EXPECT_EQ(13, info.selection_start);
  EXPECT_EQ(13, info.selection_end);
  EXPECT_EQ(5, info.composition_start);
  EXPECT_EQ(8, info.composition_end);

  // Caret exceeds the left boundary.
  active_input_method_controller->SetComposition(
      WebString::FromUTF8(composition_text), empty_ime_text_spans, WebRange(),
      -100, -100);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("helloABCworld", info.value.Utf8());
  EXPECT_EQ(0, info.selection_start);
  EXPECT_EQ(0, info.selection_end);
  EXPECT_EQ(5, info.composition_start);
  EXPECT_EQ(8, info.composition_end);

  // Caret exceeds the right boundary.
  active_input_method_controller->SetComposition(
      WebString::FromUTF8(composition_text), empty_ime_text_spans, WebRange(),
      100, 100);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("helloABCworld", info.value.Utf8());
  EXPECT_EQ(13, info.selection_start);
  EXPECT_EQ(13, info.selection_end);
  EXPECT_EQ(5, info.composition_start);
  EXPECT_EQ(8, info.composition_end);
}

TEST_F(WebViewTest, SetCompositionWithEmptyText) {
  RegisterMockedHttpURLLoad("input_field_populated.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "input_field_populated.html");
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  WebInputMethodController* active_input_method_controller =
      web_view->MainFrameImpl()
          ->FrameWidget()
          ->GetActiveWebInputMethodController();

  WebVector<ui::ImeTextSpan> empty_ime_text_spans;

  active_input_method_controller->CommitText("hello", empty_ime_text_spans,
                                             WebRange(), 0);
  WebTextInputInfo info = active_input_method_controller->TextInputInfo();

  EXPECT_EQ("hello", info.value.Utf8());
  EXPECT_EQ(5, info.selection_start);
  EXPECT_EQ(5, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);

  active_input_method_controller->SetComposition(
      WebString::FromUTF8(""), empty_ime_text_spans, WebRange(), 0, 0);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("hello", info.value.Utf8());
  EXPECT_EQ(5, info.selection_start);
  EXPECT_EQ(5, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);

  active_input_method_controller->SetComposition(
      WebString::FromUTF8(""), empty_ime_text_spans, WebRange(), -2, -2);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("hello", info.value.Utf8());
  EXPECT_EQ(3, info.selection_start);
  EXPECT_EQ(3, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);
}

TEST_F(WebViewTest, CommitTextForNewCaretPositions) {
  RegisterMockedHttpURLLoad("input_field_populated.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "input_field_populated.html");
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  WebInputMethodController* active_input_method_controller =
      web_view->MainFrameImpl()
          ->FrameWidget()
          ->GetActiveWebInputMethodController();

  WebVector<ui::ImeTextSpan> empty_ime_text_spans;

  // Caret is on the left of composing text.
  active_input_method_controller->CommitText("ab", empty_ime_text_spans,
                                             WebRange(), -2);
  WebTextInputInfo info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("ab", info.value.Utf8());
  EXPECT_EQ(0, info.selection_start);
  EXPECT_EQ(0, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);

  // Caret is on the right of composing text.
  active_input_method_controller->CommitText("c", empty_ime_text_spans,
                                             WebRange(), 1);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("cab", info.value.Utf8());
  EXPECT_EQ(2, info.selection_start);
  EXPECT_EQ(2, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);

  // Caret is on the left boundary.
  active_input_method_controller->CommitText("def", empty_ime_text_spans,
                                             WebRange(), -5);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("cadefb", info.value.Utf8());
  EXPECT_EQ(0, info.selection_start);
  EXPECT_EQ(0, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);

  // Caret is on the right boundary.
  active_input_method_controller->CommitText("g", empty_ime_text_spans,
                                             WebRange(), 6);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("gcadefb", info.value.Utf8());
  EXPECT_EQ(7, info.selection_start);
  EXPECT_EQ(7, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);

  // Caret exceeds the left boundary.
  active_input_method_controller->CommitText("hi", empty_ime_text_spans,
                                             WebRange(), -100);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("gcadefbhi", info.value.Utf8());
  EXPECT_EQ(0, info.selection_start);
  EXPECT_EQ(0, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);

  // Caret exceeds the right boundary.
  active_input_method_controller->CommitText("jk", empty_ime_text_spans,
                                             WebRange(), 100);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("jkgcadefbhi", info.value.Utf8());
  EXPECT_EQ(11, info.selection_start);
  EXPECT_EQ(11, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);
}

TEST_F(WebViewTest, CommitTextWhileComposing) {
  RegisterMockedHttpURLLoad("input_field_populated.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "input_field_populated.html");
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  WebInputMethodController* active_input_method_controller =
      web_view->MainFrameImpl()
          ->FrameWidget()
          ->GetActiveWebInputMethodController();

  WebVector<ui::ImeTextSpan> empty_ime_text_spans;
  active_input_method_controller->SetComposition(
      WebString::FromUTF8("abc"), empty_ime_text_spans, WebRange(), 0, 0);
  WebTextInputInfo info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("abc", info.value.Utf8());
  EXPECT_EQ(0, info.selection_start);
  EXPECT_EQ(0, info.selection_end);
  EXPECT_EQ(0, info.composition_start);
  EXPECT_EQ(3, info.composition_end);

  // Deletes ongoing composition, inserts the specified text and moves the
  // caret.
  active_input_method_controller->CommitText("hello", empty_ime_text_spans,
                                             WebRange(), -2);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("hello", info.value.Utf8());
  EXPECT_EQ(3, info.selection_start);
  EXPECT_EQ(3, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);

  active_input_method_controller->SetComposition(
      WebString::FromUTF8("abc"), empty_ime_text_spans, WebRange(), 0, 0);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("helabclo", info.value.Utf8());
  EXPECT_EQ(3, info.selection_start);
  EXPECT_EQ(3, info.selection_end);
  EXPECT_EQ(3, info.composition_start);
  EXPECT_EQ(6, info.composition_end);

  // Deletes ongoing composition and moves the caret.
  active_input_method_controller->CommitText("", empty_ime_text_spans,
                                             WebRange(), 2);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("hello", info.value.Utf8());
  EXPECT_EQ(5, info.selection_start);
  EXPECT_EQ(5, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);

  // Inserts the specified text and moves the caret.
  active_input_method_controller->CommitText("world", empty_ime_text_spans,
                                             WebRange(), -5);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("helloworld", info.value.Utf8());
  EXPECT_EQ(5, info.selection_start);
  EXPECT_EQ(5, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);

  // Only moves the caret.
  active_input_method_controller->CommitText("", empty_ime_text_spans,
                                             WebRange(), 5);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("helloworld", info.value.Utf8());
  EXPECT_EQ(10, info.selection_start);
  EXPECT_EQ(10, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);
}

TEST_F(WebViewTest, FinishCompositionDoesNotRevealSelection) {
  RegisterMockedHttpURLLoad("form_with_input.html");
  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + "form_with_input.html");
  web_view->MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  EXPECT_EQ(gfx::PointF(), web_view->MainFrameImpl()->GetScrollOffset());

  // Set up a composition from existing text that needs to be committed.
  Vector<ImeTextSpan> empty_ime_text_spans;
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  frame->GetFrame()->GetInputMethodController().SetCompositionFromExistingText(
      empty_ime_text_spans, 0, 3);

  // Scroll the input field out of the viewport.
  Element* element = static_cast<Element*>(
      web_view->MainFrameImpl()->GetDocument().GetElementById("btn"));
  element->scrollIntoView();
  float offset_height = web_view->MainFrameImpl()->GetScrollOffset().y();
  EXPECT_EQ(0, web_view->MainFrameImpl()->GetScrollOffset().x());
  EXPECT_LT(0, offset_height);

  WebTextInputInfo info = frame->GetInputMethodController()->TextInputInfo();
  EXPECT_EQ("hello", info.value.Utf8());

  // Verify that the input field is not scrolled back into the viewport.
  frame->FrameWidget()
      ->GetActiveWebInputMethodController()
      ->FinishComposingText(WebInputMethodController::kDoNotKeepSelection);
  EXPECT_EQ(0, web_view->MainFrameImpl()->GetScrollOffset().x());
  EXPECT_EQ(offset_height, web_view->MainFrameImpl()->GetScrollOffset().y());
}

TEST_F(WebViewTest, InsertNewLinePlacementAfterFinishComposingText) {
  RegisterMockedHttpURLLoad("text_area_populated.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "text_area_populated.html");
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);

  WebVector<ui::ImeTextSpan> empty_ime_text_spans;

  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  WebInputMethodController* active_input_method_controller =
      frame->GetInputMethodController();
  frame->SetEditableSelectionOffsets(4, 4);
  frame->SetCompositionFromExistingText(8, 12, empty_ime_text_spans);

  WebTextInputInfo info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("0123456789abcdefghijklmnopqrstuvwxyz", info.value.Utf8());
  EXPECT_EQ(4, info.selection_start);
  EXPECT_EQ(4, info.selection_end);
  EXPECT_EQ(8, info.composition_start);
  EXPECT_EQ(12, info.composition_end);

  active_input_method_controller->FinishComposingText(
      WebInputMethodController::kKeepSelection);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ(4, info.selection_start);
  EXPECT_EQ(4, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);

  std::string composition_text("\n");
  active_input_method_controller->CommitText(
      WebString::FromUTF8(composition_text), empty_ime_text_spans, WebRange(),
      0);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ(5, info.selection_start);
  EXPECT_EQ(5, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);
  EXPECT_EQ("0123\n456789abcdefghijklmnopqrstuvwxyz", info.value.Utf8());
}

TEST_F(WebViewTest, ExtendSelectionAndDelete) {
  RegisterMockedHttpURLLoad("input_field_populated.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "input_field_populated.html");
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  frame->SetEditableSelectionOffsets(10, 10);
  frame->ExtendSelectionAndDelete(5, 8);
  WebInputMethodController* active_input_method_controller =
      frame->GetInputMethodController();
  WebTextInputInfo info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("01234ijklmnopqrstuvwxyz", info.value.Utf8());
  EXPECT_EQ(5, info.selection_start);
  EXPECT_EQ(5, info.selection_end);
  frame->ExtendSelectionAndDelete(10, 0);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("ijklmnopqrstuvwxyz", info.value.Utf8());
}

TEST_F(WebViewTest, EditContextExtendSelectionAndDelete) {
  RegisterMockedHttpURLLoad("edit_context.html");
  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + "edit_context.html");
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  frame->SetEditableSelectionOffsets(10, 10);
  frame->ExtendSelectionAndDelete(5, 8);
  WebInputMethodController* active_input_method_controller =
      frame->GetInputMethodController();
  WebTextInputInfo info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("01234ijklmnopqrstuvwxyz", info.value.Utf8());
  EXPECT_EQ(5, info.selection_start);
  EXPECT_EQ(5, info.selection_end);
  frame->ExtendSelectionAndDelete(10, 0);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("ijklmnopqrstuvwxyz", info.value.Utf8());
}

TEST_F(WebViewTest, DeleteSurroundingText) {
  RegisterMockedHttpURLLoad("input_field_populated.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "input_field_populated.html");
  auto* frame = To<WebLocalFrameImpl>(web_view->MainFrame());
  WebInputMethodController* active_input_method_controller =
      frame->GetInputMethodController();
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);

  frame->SetEditableSelectionOffsets(10, 10);
  frame->DeleteSurroundingText(5, 8);
  WebTextInputInfo info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("01234ijklmnopqrstuvwxyz", info.value.Utf8());
  EXPECT_EQ(5, info.selection_start);
  EXPECT_EQ(5, info.selection_end);

  frame->SetEditableSelectionOffsets(5, 10);
  frame->DeleteSurroundingText(3, 5);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("01ijklmstuvwxyz", info.value.Utf8());
  EXPECT_EQ(2, info.selection_start);
  EXPECT_EQ(7, info.selection_end);

  frame->SetEditableSelectionOffsets(5, 5);
  frame->DeleteSurroundingText(10, 0);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("lmstuvwxyz", info.value.Utf8());
  EXPECT_EQ(0, info.selection_start);
  EXPECT_EQ(0, info.selection_end);

  frame->DeleteSurroundingText(0, 20);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("", info.value.Utf8());
  EXPECT_EQ(0, info.selection_start);
  EXPECT_EQ(0, info.selection_end);

  frame->DeleteSurroundingText(10, 10);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("", info.value.Utf8());
  EXPECT_EQ(0, info.selection_start);
  EXPECT_EQ(0, info.selection_end);
}

TEST_F(WebViewTest, SetCompositionFromExistingText) {
  RegisterMockedHttpURLLoad("input_field_populated.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "input_field_populated.html");
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  WebVector<ui::ImeTextSpan> ime_text_spans(static_cast<size_t>(1));
  ime_text_spans[0] =
      ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 0, 4,
                      ui::ImeTextSpan::Thickness::kThin,
                      ui::ImeTextSpan::UnderlineStyle::kSolid, 0, 0);
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  WebInputMethodController* active_input_method_controller =
      frame->GetInputMethodController();
  frame->SetEditableSelectionOffsets(4, 10);
  frame->SetCompositionFromExistingText(8, 12, ime_text_spans);
  WebTextInputInfo info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ(4, info.selection_start);
  EXPECT_EQ(10, info.selection_end);
  EXPECT_EQ(8, info.composition_start);
  EXPECT_EQ(12, info.composition_end);
  WebVector<ui::ImeTextSpan> empty_ime_text_spans;
  frame->SetCompositionFromExistingText(0, 0, empty_ime_text_spans);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ(4, info.selection_start);
  EXPECT_EQ(10, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);
}

TEST_F(WebViewTest, SetCompositionFromExistingTextInTextArea) {
  RegisterMockedHttpURLLoad("text_area_populated.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "text_area_populated.html");
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  WebVector<ui::ImeTextSpan> ime_text_spans(static_cast<size_t>(1));
  ime_text_spans[0] =
      ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 0, 4,
                      ui::ImeTextSpan::Thickness::kThin,
                      ui::ImeTextSpan::UnderlineStyle::kSolid, 0, 0);
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  WebInputMethodController* active_input_method_controller =
      frame->FrameWidget()->GetActiveWebInputMethodController();
  frame->SetEditableSelectionOffsets(27, 27);
  std::string new_line_text("\n");
  WebVector<ui::ImeTextSpan> empty_ime_text_spans;
  active_input_method_controller->CommitText(
      WebString::FromUTF8(new_line_text), empty_ime_text_spans, WebRange(), 0);
  WebTextInputInfo info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("0123456789abcdefghijklmnopq\nrstuvwxyz", info.value.Utf8());

  frame->SetEditableSelectionOffsets(31, 31);
  frame->SetCompositionFromExistingText(30, 34, ime_text_spans);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("0123456789abcdefghijklmnopq\nrstuvwxyz", info.value.Utf8());
  EXPECT_EQ(31, info.selection_start);
  EXPECT_EQ(31, info.selection_end);
  EXPECT_EQ(30, info.composition_start);
  EXPECT_EQ(34, info.composition_end);

  std::string composition_text("yolo");
  active_input_method_controller->CommitText(
      WebString::FromUTF8(composition_text), empty_ime_text_spans, WebRange(),
      0);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("0123456789abcdefghijklmnopq\nrsyoloxyz", info.value.Utf8());
  EXPECT_EQ(34, info.selection_start);
  EXPECT_EQ(34, info.selection_end);
  EXPECT_EQ(-1, info.composition_start);
  EXPECT_EQ(-1, info.composition_end);
}

TEST_F(WebViewTest, SetCompositionFromExistingTextInRichText) {
  RegisterMockedHttpURLLoad("content_editable_rich_text.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "content_editable_rich_text.html");
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  WebVector<ui::ImeTextSpan> ime_text_spans(static_cast<size_t>(1));
  ime_text_spans[0] =
      ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 0, 4,
                      ui::ImeTextSpan::Thickness::kThin,
                      ui::ImeTextSpan::UnderlineStyle::kSolid, 0, 0);
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  frame->SetEditableSelectionOffsets(1, 1);
  WebDocument document = web_view->MainFrameImpl()->GetDocument();
  EXPECT_FALSE(document.GetElementById("bold").IsNull());
  frame->SetCompositionFromExistingText(0, 4, ime_text_spans);
  EXPECT_FALSE(document.GetElementById("bold").IsNull());
}

TEST_F(WebViewTest, SetEditableSelectionOffsetsKeepsComposition) {
  RegisterMockedHttpURLLoad("input_field_populated.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "input_field_populated.html");
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);

  std::string composition_text_first("hello ");
  std::string composition_text_second("world");
  WebVector<ui::ImeTextSpan> empty_ime_text_spans;
  WebInputMethodController* active_input_method_controller =
      web_view->MainFrameImpl()
          ->FrameWidget()
          ->GetActiveWebInputMethodController();
  active_input_method_controller->CommitText(
      WebString::FromUTF8(composition_text_first), empty_ime_text_spans,
      WebRange(), 0);
  active_input_method_controller->SetComposition(
      WebString::FromUTF8(composition_text_second), empty_ime_text_spans,
      WebRange(), 5, 5);

  WebTextInputInfo info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("hello world", info.value.Utf8());
  EXPECT_EQ(11, info.selection_start);
  EXPECT_EQ(11, info.selection_end);
  EXPECT_EQ(6, info.composition_start);
  EXPECT_EQ(11, info.composition_end);

  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  frame->SetEditableSelectionOffsets(6, 6);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("hello world", info.value.Utf8());
  EXPECT_EQ(6, info.selection_start);
  EXPECT_EQ(6, info.selection_end);
  EXPECT_EQ(6, info.composition_start);
  EXPECT_EQ(11, info.composition_end);

  frame->SetEditableSelectionOffsets(8, 8);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("hello world", info.value.Utf8());
  EXPECT_EQ(8, info.selection_start);
  EXPECT_EQ(8, info.selection_end);
  EXPECT_EQ(6, info.composition_start);
  EXPECT_EQ(11, info.composition_end);

  frame->SetEditableSelectionOffsets(11, 11);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("hello world", info.value.Utf8());
  EXPECT_EQ(11, info.selection_start);
  EXPECT_EQ(11, info.selection_end);
  EXPECT_EQ(6, info.composition_start);
  EXPECT_EQ(11, info.composition_end);

  frame->SetEditableSelectionOffsets(6, 11);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("hello world", info.value.Utf8());
  EXPECT_EQ(6, info.selection_start);
  EXPECT_EQ(11, info.selection_end);
  EXPECT_EQ(6, info.composition_start);
  EXPECT_EQ(11, info.composition_end);

  frame->SetEditableSelectionOffsets(2, 2);
  info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ("hello world", info.value.Utf8());
  EXPECT_EQ(2, info.selection_start);
  EXPECT_EQ(2, info.selection_end);
  // Composition range should be reset by browser process or keyboard apps.
  EXPECT_EQ(6, info.composition_start);
  EXPECT_EQ(11, info.composition_end);
}

TEST_F(WebViewTest, IsSelectionAnchorFirst) {
  // TODO(xidachen): crbug.com/1150389, Make this test work with the feature.
  if (RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled())
    return;
  RegisterMockedHttpURLLoad("input_field_populated.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "input_field_populated.html");
  WebLocalFrame* frame = web_view->MainFrameImpl();

  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  frame->SetEditableSelectionOffsets(4, 10);
  EXPECT_TRUE(frame->IsSelectionAnchorFirst());
  gfx::Rect anchor;
  gfx::Rect focus;
  web_view->MainFrameViewWidget()->CalculateSelectionBounds(anchor, focus);
  frame->SelectRange(focus.origin(), anchor.origin());
  EXPECT_FALSE(frame->IsSelectionAnchorFirst());
}

TEST_F(
    WebViewTest,
    MoveFocusToNextFocusableElementForImeAndAutofillWithKeyEventListenersAndNonEditableElements) {
  const std::string test_file =
      "advance_focus_in_form_with_key_event_listeners.html";
  RegisterMockedHttpURLLoad(test_file);
  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + test_file);
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();
  WebInputMethodController* active_input_method_controller =
      web_view->MainFrameImpl()
          ->FrameWidget()
          ->GetActiveWebInputMethodController();
  const int default_text_input_flags = kWebTextInputFlagNone;

  struct FocusedElement {
    AtomicString element_id;
    int next_previous_flags;
  } focused_elements[] = {
      {AtomicString("input1"),
       default_text_input_flags | kWebTextInputFlagHaveNextFocusableElement},
      {AtomicString("contenteditable1"),
       kWebTextInputFlagHaveNextFocusableElement |
           kWebTextInputFlagHavePreviousFocusableElement},
      {AtomicString("input2"),
       default_text_input_flags | kWebTextInputFlagHaveNextFocusableElement |
           kWebTextInputFlagHavePreviousFocusableElement},
      {AtomicString("textarea1"),
       default_text_input_flags | kWebTextInputFlagHaveNextFocusableElement |
           kWebTextInputFlagHavePreviousFocusableElement},
      {AtomicString("input3"),
       default_text_input_flags | kWebTextInputFlagHaveNextFocusableElement |
           kWebTextInputFlagHavePreviousFocusableElement},
      {AtomicString("textarea2"),
       default_text_input_flags |
           kWebTextInputFlagHavePreviousFocusableElement},
  };

  // Forward Navigation in form1 with NEXT
  Element* input1 = document->getElementById(AtomicString("input1"));
  input1->Focus();
  Element* current_focus = nullptr;
  Element* next_focus = nullptr;
  int next_previous_flags;
  for (size_t i = 0; i < std::size(focused_elements); ++i) {
    current_focus = document->getElementById(focused_elements[i].element_id);
    EXPECT_EQ(current_focus, document->FocusedElement());
    next_previous_flags =
        active_input_method_controller->ComputeWebTextInputNextPreviousFlags();
    EXPECT_EQ(focused_elements[i].next_previous_flags, next_previous_flags);
    next_focus = document->GetPage()
                     ->GetFocusController()
                     .NextFocusableElementForImeAndAutofill(
                         current_focus, mojom::blink::FocusType::kForward);
    if (next_focus) {
      EXPECT_EQ(next_focus->GetIdAttribute(),
                focused_elements[i + 1].element_id);
    }
    web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
        mojom::blink::FocusType::kForward);
  }
  // Now focus will stay on previous focus itself, because it has no next
  // element.
  EXPECT_EQ(current_focus, document->FocusedElement());

  // Backward Navigation in form1 with PREVIOUS
  for (size_t i = std::size(focused_elements); i-- > 0;) {
    current_focus = document->getElementById(focused_elements[i].element_id);
    EXPECT_EQ(current_focus, document->FocusedElement());
    next_previous_flags =
        active_input_method_controller->ComputeWebTextInputNextPreviousFlags();
    EXPECT_EQ(focused_elements[i].next_previous_flags, next_previous_flags);
    next_focus = document->GetPage()
                     ->GetFocusController()
                     .NextFocusableElementForImeAndAutofill(
                         current_focus, mojom::blink::FocusType::kBackward);
    if (next_focus) {
      EXPECT_EQ(next_focus->GetIdAttribute(),
                focused_elements[i - 1].element_id);
    }
    web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
        mojom::blink::FocusType::kBackward);
  }
  // Now focus will stay on previous focus itself, because it has no previous
  // element.
  EXPECT_EQ(current_focus, document->FocusedElement());

  // Setting a non editable element as focus in form1, and ensuring editable
  // navigation is fine in forward and backward.
  Element* button1 = document->getElementById(AtomicString("button1"));
  button1->Focus();
  next_previous_flags =
      active_input_method_controller->ComputeWebTextInputNextPreviousFlags();
  EXPECT_EQ(kWebTextInputFlagHaveNextFocusableElement |
                kWebTextInputFlagHavePreviousFocusableElement,
            next_previous_flags);
  next_focus = document->GetPage()
                   ->GetFocusController()
                   .NextFocusableElementForImeAndAutofill(
                       button1, mojom::blink::FocusType::kForward);
  EXPECT_EQ(next_focus->GetIdAttribute(), "contenteditable1");
  web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
      mojom::blink::FocusType::kForward);
  Element* content_editable1 =
      document->getElementById(AtomicString("contenteditable1"));
  EXPECT_EQ(content_editable1, document->FocusedElement());
  button1->Focus();
  next_focus = document->GetPage()
                   ->GetFocusController()
                   .NextFocusableElementForImeAndAutofill(
                       button1, mojom::blink::FocusType::kBackward);
  EXPECT_EQ(next_focus->GetIdAttribute(), "input1");
  web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
      mojom::blink::FocusType::kBackward);
  EXPECT_EQ(input1, document->FocusedElement());

  Element* anchor1 = document->getElementById(AtomicString("anchor1"));
  anchor1->Focus();
  next_previous_flags =
      active_input_method_controller->ComputeWebTextInputNextPreviousFlags();
  // No Next/Previous element for elements outside form.
  EXPECT_EQ(0, next_previous_flags);
  next_focus = document->GetPage()
                   ->GetFocusController()
                   .NextFocusableElementForImeAndAutofill(
                       anchor1, mojom::blink::FocusType::kForward);
  EXPECT_EQ(next_focus, nullptr);
  web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
      mojom::blink::FocusType::kForward);
  // Since anchor is not a form control element, next/previous element will
  // be null, hence focus will stay same as it is.
  EXPECT_EQ(anchor1, document->FocusedElement());

  next_focus = document->GetPage()
                   ->GetFocusController()
                   .NextFocusableElementForImeAndAutofill(
                       anchor1, mojom::blink::FocusType::kBackward);
  EXPECT_EQ(next_focus, nullptr);
  web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
      mojom::blink::FocusType::kBackward);
  EXPECT_EQ(anchor1, document->FocusedElement());

  // Navigation of elements which are not a part of any forms. All these
  // elements compose a <form>less form.
  Element* text_area3 = document->getElementById(AtomicString("textarea3"));
  text_area3->Focus();
  next_previous_flags =
      active_input_method_controller->ComputeWebTextInputNextPreviousFlags();
  // Next/Previous elements for an element outside of a form are other
  // <form>less elements.
  EXPECT_EQ(kWebTextInputFlagHaveNextFocusableElement, next_previous_flags);
  next_focus = document->GetPage()
                   ->GetFocusController()
                   .NextFocusableElementForImeAndAutofill(
                       text_area3, mojom::blink::FocusType::kForward);
  Element* text_area4 = document->getElementById(AtomicString("textarea4"));
  Element* content_editable2 =
      document->getElementById(AtomicString("contenteditable2"));
  EXPECT_EQ(next_focus, content_editable2);
  web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
      mojom::blink::FocusType::kForward);
  EXPECT_EQ(content_editable2, document->FocusedElement());
  // No previous element to this <form>less element because there is no other
  // formless element before. Hence focus won't change wrt PREVIOUS.
  next_focus = document->GetPage()
                   ->GetFocusController()
                   .NextFocusableElementForImeAndAutofill(
                       text_area3, mojom::blink::FocusType::kBackward);
  EXPECT_EQ(next_focus, nullptr);
  web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
      mojom::blink::FocusType::kBackward);
  EXPECT_EQ(text_area3, document->FocusedElement());

  // Navigation from an element which is part of a form but not an editable
  // element.
  Element* button2 = document->getElementById(AtomicString("button2"));
  button2->Focus();
  next_previous_flags =
      active_input_method_controller->ComputeWebTextInputNextPreviousFlags();
  // No Next element for this element, due to last element outside the form.
  EXPECT_EQ(kWebTextInputFlagHavePreviousFocusableElement, next_previous_flags);
  next_focus = document->GetPage()
                   ->GetFocusController()
                   .NextFocusableElementForImeAndAutofill(
                       button2, mojom::blink::FocusType::kForward);
  EXPECT_EQ(next_focus, nullptr);
  web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
      mojom::blink::FocusType::kForward);
  // No Next element to this element within form1. Hence focus won't change wrt
  // NEXT.
  EXPECT_EQ(button2, document->FocusedElement());
  Element* text_area2 = document->getElementById(AtomicString("textarea2"));
  next_focus = document->GetPage()
                   ->GetFocusController()
                   .NextFocusableElementForImeAndAutofill(
                       button2, mojom::blink::FocusType::kBackward);
  EXPECT_EQ(next_focus, text_area2);
  web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
      mojom::blink::FocusType::kBackward);
  // Since button is a form control element from form1, ensuring focus is set
  // at correct position.
  EXPECT_EQ(text_area2, document->FocusedElement());

  document->SetFocusedElement(
      content_editable2, FocusParams(SelectionBehaviorOnFocus::kNone,
                                     mojom::blink::FocusType::kNone, nullptr));
  next_previous_flags =
      active_input_method_controller->ComputeWebTextInputNextPreviousFlags();
  // Next/Previous elements for an element outside of a form are other
  // <form>less elements before and after that element.
  EXPECT_EQ(kWebTextInputFlagHaveNextFocusableElement |
                kWebTextInputFlagHavePreviousFocusableElement,
            next_previous_flags);
  next_focus = document->GetPage()
                   ->GetFocusController()
                   .NextFocusableElementForImeAndAutofill(
                       content_editable2, mojom::blink::FocusType::kForward);
  EXPECT_EQ(next_focus, text_area4);
  web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
      mojom::blink::FocusType::kForward);
  EXPECT_EQ(text_area4, document->FocusedElement());
  next_focus = document->GetPage()
                   ->GetFocusController()
                   .NextFocusableElementForImeAndAutofill(
                       content_editable2, mojom::blink::FocusType::kBackward);
  document->SetFocusedElement(
      content_editable2, FocusParams(SelectionBehaviorOnFocus::kNone,
                                     mojom::blink::FocusType::kNone, nullptr));
  EXPECT_EQ(next_focus, text_area3);
  web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
      mojom::blink::FocusType::kBackward);
  EXPECT_EQ(text_area3, document->FocusedElement());

  // Navigation of elements which is having invalid form attribute and hence
  // is a part of the <form>less form.
  text_area4->Focus();
  next_previous_flags =
      active_input_method_controller->ComputeWebTextInputNextPreviousFlags();
  // No next element for an element outside of a form because it is the last
  // <form>less element.
  EXPECT_EQ(kWebTextInputFlagHavePreviousFocusableElement, next_previous_flags);
  next_focus = document->GetPage()
                   ->GetFocusController()
                   .NextFocusableElementForImeAndAutofill(
                       text_area4, mojom::blink::FocusType::kForward);
  EXPECT_EQ(next_focus, nullptr);
  web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
      mojom::blink::FocusType::kForward);
  // No next element. Hence focus won't change wrt NEXT.
  EXPECT_EQ(text_area4, document->FocusedElement());
  next_focus = document->GetPage()
                   ->GetFocusController()
                   .NextFocusableElementForImeAndAutofill(
                       text_area4, mojom::blink::FocusType::kBackward);
  // The previous element of a formless element is the previous formless
  // element.
  EXPECT_EQ(next_focus, content_editable2);
  web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
      mojom::blink::FocusType::kBackward);
  EXPECT_EQ(content_editable2, document->FocusedElement());

  web_view_helper_.Reset();
}

TEST_F(
    WebViewTest,
    MoveFocusToNextFocusableElementForImeAndAutofillWithNonEditableNonFormControlElements) {
  const std::string test_file =
      "advance_focus_in_form_with_key_event_listeners.html";
  RegisterMockedHttpURLLoad(test_file);
  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + test_file);
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();
  WebInputMethodController* active_input_method_controller =
      web_view->MainFrameImpl()
          ->FrameWidget()
          ->GetActiveWebInputMethodController();
  const int default_text_input_flags = kWebTextInputFlagNone;

  struct FocusedElement {
    const char* element_id;
    int next_previous_flags;
  } focused_elements[] = {
      {"textarea5",
       default_text_input_flags | kWebTextInputFlagHaveNextFocusableElement},
      {"input4", default_text_input_flags |
                     kWebTextInputFlagHaveNextFocusableElement |
                     kWebTextInputFlagHavePreviousFocusableElement},
      {"contenteditable3", kWebTextInputFlagHaveNextFocusableElement |
                               kWebTextInputFlagHavePreviousFocusableElement},
      {"input5", kWebTextInputFlagHavePreviousFocusableElement},
  };

  // Forward Navigation in form2 with NEXT
  Element* text_area5 = document->getElementById(AtomicString("textarea5"));
  text_area5->Focus();
  Element* current_focus = nullptr;
  Element* next_focus = nullptr;
  int next_previous_flags;
  for (size_t i = 0; i < std::size(focused_elements); ++i) {
    current_focus =
        document->getElementById(AtomicString(focused_elements[i].element_id));
    EXPECT_EQ(current_focus, document->FocusedElement());
    next_previous_flags =
        active_input_method_controller->ComputeWebTextInputNextPreviousFlags();
    EXPECT_EQ(focused_elements[i].next_previous_flags, next_previous_flags);
    next_focus = document->GetPage()
                     ->GetFocusController()
                     .NextFocusableElementForImeAndAutofill(
                         current_focus, mojom::blink::FocusType::kForward);
    if (next_focus) {
      EXPECT_EQ(next_focus->GetIdAttribute(),
                focused_elements[i + 1].element_id);
    }
    web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
        mojom::blink::FocusType::kForward);
  }
  // Now focus will stay on previous focus itself, because it has no next
  // element.
  EXPECT_EQ(current_focus, document->FocusedElement());

  // Backward Navigation in form1 with PREVIOUS
  for (size_t i = std::size(focused_elements); i-- > 0;) {
    current_focus =
        document->getElementById(AtomicString(focused_elements[i].element_id));
    EXPECT_EQ(current_focus, document->FocusedElement());
    next_previous_flags =
        active_input_method_controller->ComputeWebTextInputNextPreviousFlags();
    EXPECT_EQ(focused_elements[i].next_previous_flags, next_previous_flags);
    next_focus = document->GetPage()
                     ->GetFocusController()
                     .NextFocusableElementForImeAndAutofill(
                         current_focus, mojom::blink::FocusType::kBackward);
    if (next_focus) {
      EXPECT_EQ(next_focus->GetIdAttribute(),
                focused_elements[i - 1].element_id);
    }
    web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
        mojom::blink::FocusType::kBackward);
  }
  // Now focus will stay on previous focus itself, because it has no previous
  // element.
  EXPECT_EQ(current_focus, document->FocusedElement());

  // Setting a non editable element as focus in form1, and ensuring editable
  // navigation is fine in forward and backward.
  Element* anchor2 = document->getElementById(AtomicString("anchor2"));
  anchor2->Focus();
  next_previous_flags =
      active_input_method_controller->ComputeWebTextInputNextPreviousFlags();
  // No Next/Previous element for non-form control elements inside form.
  EXPECT_EQ(0, next_previous_flags);
  next_focus = document->GetPage()
                   ->GetFocusController()
                   .NextFocusableElementForImeAndAutofill(
                       anchor2, mojom::blink::FocusType::kForward);
  EXPECT_EQ(next_focus, nullptr);
  web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
      mojom::blink::FocusType::kForward);
  // Since anchor is not a form control element, next/previous element will
  // be null, hence focus will stay same as it is.
  EXPECT_EQ(anchor2, document->FocusedElement());
  next_focus = document->GetPage()
                   ->GetFocusController()
                   .NextFocusableElementForImeAndAutofill(
                       anchor2, mojom::blink::FocusType::kBackward);
  EXPECT_EQ(next_focus, nullptr);
  web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
      mojom::blink::FocusType::kBackward);
  EXPECT_EQ(anchor2, document->FocusedElement());

  web_view_helper_.Reset();
}

TEST_F(WebViewTest,
       MoveFocusToNextFocusableElementForImeAndAutofillWithTabIndexElements) {
  const std::string test_file =
      "advance_focus_in_form_with_tabindex_elements.html";
  RegisterMockedHttpURLLoad(test_file);
  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + test_file);
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();
  WebInputMethodController* active_input_method_controller =
      web_view->MainFrameImpl()
          ->FrameWidget()
          ->GetActiveWebInputMethodController();
  const int default_text_input_flags = kWebTextInputFlagNone;

  struct FocusedElement {
    const char* element_id;
    int next_previous_flags;
  } focused_elements[] = {
      {"textarea6",
       default_text_input_flags | kWebTextInputFlagHaveNextFocusableElement},
      {"input5", default_text_input_flags |
                     kWebTextInputFlagHaveNextFocusableElement |
                     kWebTextInputFlagHavePreviousFocusableElement},
      {"contenteditable4", kWebTextInputFlagHaveNextFocusableElement |
                               kWebTextInputFlagHavePreviousFocusableElement},
      {"input6", default_text_input_flags |
                     kWebTextInputFlagHavePreviousFocusableElement},
  };

  // Forward Navigation in form with NEXT which has tabindex attribute
  // which differs visual order.
  Element* text_area6 = document->getElementById(AtomicString("textarea6"));
  text_area6->Focus();
  Element* current_focus = nullptr;
  Element* next_focus = nullptr;
  int next_previous_flags;
  for (size_t i = 0; i < std::size(focused_elements); ++i) {
    current_focus =
        document->getElementById(AtomicString(focused_elements[i].element_id));
    EXPECT_EQ(current_focus, document->FocusedElement());
    next_previous_flags =
        active_input_method_controller->ComputeWebTextInputNextPreviousFlags();
    EXPECT_EQ(focused_elements[i].next_previous_flags, next_previous_flags);
    next_focus = document->GetPage()
                     ->GetFocusController()
                     .NextFocusableElementForImeAndAutofill(
                         current_focus, mojom::blink::FocusType::kForward);
    if (next_focus) {
      EXPECT_EQ(next_focus->GetIdAttribute(),
                focused_elements[i + 1].element_id);
    }
    web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
        mojom::blink::FocusType::kForward);
  }
  // No next editable element which is focusable with proper tab index, hence
  // staying on previous focus.
  EXPECT_EQ(current_focus, document->FocusedElement());

  // Backward Navigation in form with PREVIOUS which has tabindex attribute
  // which differs visual order.
  for (size_t i = std::size(focused_elements); i-- > 0;) {
    current_focus =
        document->getElementById(AtomicString(focused_elements[i].element_id));
    EXPECT_EQ(current_focus, document->FocusedElement());
    next_previous_flags =
        active_input_method_controller->ComputeWebTextInputNextPreviousFlags();
    EXPECT_EQ(focused_elements[i].next_previous_flags, next_previous_flags);
    next_focus = document->GetPage()
                     ->GetFocusController()
                     .NextFocusableElementForImeAndAutofill(
                         current_focus, mojom::blink::FocusType::kBackward);
    if (next_focus) {
      EXPECT_EQ(next_focus->GetIdAttribute(),
                focused_elements[i - 1].element_id);
    }
    web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
        mojom::blink::FocusType::kBackward);
  }
  // Now focus will stay on previous focus itself, because it has no previous
  // element.
  EXPECT_EQ(current_focus, document->FocusedElement());

  // Setting an element which has invalid tabindex and ensuring it is not
  // modifying further navigation.
  Element* content_editable5 =
      document->getElementById(AtomicString("contenteditable5"));
  content_editable5->Focus();
  Element* input6 = document->getElementById(AtomicString("input6"));
  next_focus = document->GetPage()
                   ->GetFocusController()
                   .NextFocusableElementForImeAndAutofill(
                       content_editable5, mojom::blink::FocusType::kForward);
  EXPECT_EQ(next_focus, input6);
  web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
      mojom::blink::FocusType::kForward);
  EXPECT_EQ(input6, document->FocusedElement());
  content_editable5->Focus();
  next_focus = document->GetPage()
                   ->GetFocusController()
                   .NextFocusableElementForImeAndAutofill(
                       content_editable5, mojom::blink::FocusType::kBackward);
  EXPECT_EQ(next_focus, text_area6);
  web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
      mojom::blink::FocusType::kBackward);
  EXPECT_EQ(text_area6, document->FocusedElement());

  web_view_helper_.Reset();
}

TEST_F(
    WebViewTest,
    MoveFocusToNextFocusableElementForImeAndAutofillWithDisabledAndReadonlyElements) {
  const std::string test_file =
      "advance_focus_in_form_with_disabled_and_readonly_elements.html";
  RegisterMockedHttpURLLoad(test_file);
  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + test_file);
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();
  WebInputMethodController* active_input_method_controller =
      web_view->MainFrameImpl()
          ->FrameWidget()
          ->GetActiveWebInputMethodController();

  struct FocusedElement {
    const char* element_id;
    int next_previous_flags;
  } focused_elements[] = {
      {"contenteditable6", kWebTextInputFlagHaveNextFocusableElement},
      {"contenteditable7", kWebTextInputFlagHavePreviousFocusableElement},
  };
  // Forward Navigation in form with NEXT which has has disabled/enabled
  // elements which will gets skipped during navigation.
  Element* content_editable6 =
      document->getElementById(AtomicString("contenteditable6"));
  content_editable6->Focus();
  Element* current_focus = nullptr;
  Element* next_focus = nullptr;
  int next_previous_flags;
  for (size_t i = 0; i < std::size(focused_elements); ++i) {
    current_focus =
        document->getElementById(AtomicString(focused_elements[i].element_id));
    EXPECT_EQ(current_focus, document->FocusedElement());
    next_previous_flags =
        active_input_method_controller->ComputeWebTextInputNextPreviousFlags();
    EXPECT_EQ(focused_elements[i].next_previous_flags, next_previous_flags);
    next_focus = document->GetPage()
                     ->GetFocusController()
                     .NextFocusableElementForImeAndAutofill(
                         current_focus, mojom::blink::FocusType::kForward);
    if (next_focus) {
      EXPECT_EQ(next_focus->GetIdAttribute(),
                focused_elements[i + 1].element_id);
    }
    web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
        mojom::blink::FocusType::kForward);
  }
  // No next editable element which is focusable, hence staying on previous
  // focus.
  EXPECT_EQ(current_focus, document->FocusedElement());

  // Backward Navigation in form with PREVIOUS which has has
  // disabled/enabled elements which will gets skipped during navigation.
  for (size_t i = std::size(focused_elements); i-- > 0;) {
    current_focus =
        document->getElementById(AtomicString(focused_elements[i].element_id));
    EXPECT_EQ(current_focus, document->FocusedElement());
    next_previous_flags =
        active_input_method_controller->ComputeWebTextInputNextPreviousFlags();
    EXPECT_EQ(focused_elements[i].next_previous_flags, next_previous_flags);
    next_focus = document->GetPage()
                     ->GetFocusController()
                     .NextFocusableElementForImeAndAutofill(
                         current_focus, mojom::blink::FocusType::kBackward);
    if (next_focus) {
      EXPECT_EQ(next_focus->GetIdAttribute(),
                focused_elements[i - 1].element_id);
    }
    web_view->MainFrameImpl()->GetFrame()->AdvanceFocusForIME(
        mojom::blink::FocusType::kBackward);
  }
  // Now focus will stay on previous focus itself, because it has no previous
  // element.
  EXPECT_EQ(current_focus, document->FocusedElement());

  web_view_helper_.Reset();
}

TEST_F(WebViewTest, ExitingDeviceEmulationResetsPageScale) {
  RegisterMockedHttpURLLoad("200-by-300.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad(base_url_ + "200-by-300.html");
  web_view_impl->MainFrameViewWidget()->Resize(gfx::Size(200, 300));

  float page_scale_expected = web_view_impl->PageScaleFactor();

  DeviceEmulationParams params;
  params.screen_type = mojom::EmulatedScreenType::kDesktop;
  params.device_scale_factor = 0;
  params.scale = 1;

  web_view_impl->EnableDeviceEmulation(params);

  web_view_impl->SetPageScaleFactor(2);

  web_view_impl->DisableDeviceEmulation();

  EXPECT_EQ(page_scale_expected, web_view_impl->PageScaleFactor());
}

TEST_F(WebViewTest, HistoryResetScrollAndScaleState) {
  RegisterMockedHttpURLLoad("200-by-300.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad(base_url_ + "200-by-300.html");
  web_view_impl->MainFrameViewWidget()->Resize(gfx::Size(100, 150));
  UpdateAllLifecyclePhases();
  EXPECT_EQ(gfx::PointF(), web_view_impl->MainFrameImpl()->GetScrollOffset());

  // Make the page scale and scroll with the given paremeters.
  web_view_impl->SetPageScaleFactor(2.0f);
  web_view_impl->MainFrameImpl()->SetScrollOffset(gfx::PointF(94, 111));
  EXPECT_EQ(2.0f, web_view_impl->PageScaleFactor());
  EXPECT_EQ(94, web_view_impl->MainFrameImpl()->GetScrollOffset().x());
  EXPECT_EQ(111, web_view_impl->MainFrameImpl()->GetScrollOffset().y());
  auto* main_frame_local =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  main_frame_local->Loader().SaveScrollState();
  EXPECT_EQ(2.0f, main_frame_local->Loader()
                      .GetDocumentLoader()
                      ->GetHistoryItem()
                      ->GetViewState()
                      ->page_scale_factor_);
  EXPECT_EQ(94, main_frame_local->Loader()
                    .GetDocumentLoader()
                    ->GetHistoryItem()
                    ->GetViewState()
                    ->scroll_offset_.x());
  EXPECT_EQ(111, main_frame_local->Loader()
                     .GetDocumentLoader()
                     ->GetHistoryItem()
                     ->GetViewState()
                     ->scroll_offset_.y());

  // Confirm that resetting the page state resets the saved scroll position.
  web_view_impl->ResetScrollAndScaleState();
  EXPECT_EQ(1.0f, web_view_impl->PageScaleFactor());
  EXPECT_EQ(gfx::PointF(), web_view_impl->MainFrameImpl()->GetScrollOffset());
  EXPECT_FALSE(main_frame_local->Loader()
                   .GetDocumentLoader()
                   ->GetHistoryItem()
                   ->GetViewState()
                   .has_value());
}

TEST_F(WebViewTest, BackForwardRestoreScroll) {
  RegisterMockedHttpURLLoad("back_forward_restore_scroll.html");
  WebViewImpl* web_view_impl = web_view_helper_.InitializeAndLoad(
      base_url_ + "back_forward_restore_scroll.html");
  web_view_impl->MainFrameViewWidget()->Resize(gfx::Size(640, 480));
  web_view_impl->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  // Emulate a user scroll
  web_view_impl->MainFrameImpl()->SetScrollOffset(gfx::PointF(0, 900));
  auto* main_frame_local =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  Persistent<HistoryItem> item1 =
      main_frame_local->Loader().GetDocumentLoader()->GetHistoryItem();

  // Click an anchor
  FrameLoadRequest request_a(
      main_frame_local->DomWindow(),
      ResourceRequest(main_frame_local->GetDocument()->CompleteURL("#a")));
  main_frame_local->Loader().StartNavigation(request_a);
  Persistent<HistoryItem> item2 =
      main_frame_local->Loader().GetDocumentLoader()->GetHistoryItem();

  // Go back, then forward, then back again.
  main_frame_local->Loader().GetDocumentLoader()->CommitSameDocumentNavigation(
      item1->Url(), WebFrameLoadType::kBackForward, item1.Get(),
      ClientRedirectPolicy::kNotClientRedirect,
      /*has_transient_user_activation=*/false, /*initiator_origin=*/nullptr,
      /*is_synchronously_committed=*/false, /*source_element=*/nullptr,
      mojom::blink::TriggeringEventInfo::kNotFromEvent,
      /*is_browser_initiated=*/true, /*has_ua_visual_transition,=*/false,
      /*soft_navigation_heuristics_task_id=*/std::nullopt);
  main_frame_local->Loader().GetDocumentLoader()->CommitSameDocumentNavigation(
      item2->Url(), WebFrameLoadType::kBackForward, item2.Get(),
      ClientRedirectPolicy::kNotClientRedirect,
      /*has_transient_user_activation=*/false, /*initiator_origin=*/nullptr,
      /*is_synchronously_committed=*/false, /*source_element=*/nullptr,
      mojom::blink::TriggeringEventInfo::kNotFromEvent,
      /*is_browser_initiated=*/true, /*has_ua_visual_transition,=*/false,
      /*soft_navigation_heuristics_task_id=*/std::nullopt);
  main_frame_local->Loader().GetDocumentLoader()->CommitSameDocumentNavigation(
      item1->Url(), WebFrameLoadType::kBackForward, item1.Get(),
      ClientRedirectPolicy::kNotClientRedirect,
      /*has_transient_user_activation=*/false, /*initiator_origin=*/nullptr,
      /*is_synchronously_committed=*/false, /*source_element=*/nullptr,
      mojom::blink::TriggeringEventInfo::kNotFromEvent,
      /*is_browser_initiated=*/true, /*has_ua_visual_transition,=*/false,
      /*soft_navigation_heuristics_task_id=*/std::nullopt);
  web_view_impl->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  // Click a different anchor
  FrameLoadRequest request_b(
      main_frame_local->DomWindow(),
      ResourceRequest(main_frame_local->GetDocument()->CompleteURL("#b")));
  main_frame_local->Loader().StartNavigation(request_b);
  Persistent<HistoryItem> item3 =
      main_frame_local->Loader().GetDocumentLoader()->GetHistoryItem();
  web_view_impl->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  // Go back, then forward. The scroll position should be properly set on the
  // forward navigation.
  main_frame_local->Loader().GetDocumentLoader()->CommitSameDocumentNavigation(
      item1->Url(), WebFrameLoadType::kBackForward, item1.Get(),
      ClientRedirectPolicy::kNotClientRedirect,
      /*has_transient_user_activation=*/false, /*initiator_origin=*/nullptr,
      /*is_synchronously_committed=*/false, /*source_element=*/nullptr,
      mojom::blink::TriggeringEventInfo::kNotFromEvent,
      /*is_browser_initiated=*/true, /*has_ua_visual_transition,=*/false,
      /*soft_navigation_heuristics_task_id=*/std::nullopt);

  main_frame_local->Loader().GetDocumentLoader()->CommitSameDocumentNavigation(
      item3->Url(), WebFrameLoadType::kBackForward, item3.Get(),
      ClientRedirectPolicy::kNotClientRedirect,
      /*has_transient_user_activation=*/false, /*initiator_origin=*/nullptr,
      /*is_synchronously_committed=*/false, /*source_element=*/nullptr,
      mojom::blink::TriggeringEventInfo::kNotFromEvent,
      /*is_browser_initiated=*/true, /*has_ua_visual_transition,=*/false,
      /*soft_navigation_heuristics_task_id=*/std::nullopt);
  // The scroll offset is only applied via invoking the anchor via the main
  // lifecycle, or a forced layout.
  // TODO(chrishtr): At the moment, WebLocalFrameImpl::GetScrollOffset() does
  // not force a layout. Script-exposed scroll offset-reading methods do,
  // however. It seems wrong not to force a layout.
  EXPECT_EQ(0, web_view_impl->MainFrameImpl()->GetScrollOffset().x());
  EXPECT_GT(web_view_impl->MainFrameImpl()->GetScrollOffset().y(), 2000);
}

// Tests that scroll offset modified during fullscreen is preserved when
// exiting fullscreen.
TEST_F(WebViewTest, FullscreenNoResetScroll) {
  RegisterMockedHttpURLLoad("fullscreen_style.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad(base_url_ + "fullscreen_style.html");
  web_view_impl->MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  UpdateAllLifecyclePhases();

  // Scroll the page down.
  web_view_impl->MainFrameImpl()->SetScrollOffset(gfx::PointF(0, 2000));
  ASSERT_EQ(2000, web_view_impl->MainFrameImpl()->GetScrollOffset().y());

  // Enter fullscreen.
  LocalFrame* frame = web_view_impl->MainFrameImpl()->GetFrame();
  Element* element = frame->GetDocument()->documentElement();
  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*element);
  web_view_impl->DidEnterFullscreen();
  UpdateAllLifecyclePhases();

  // Assert the scroll position on the document element doesn't change.
  ASSERT_EQ(2000, web_view_impl->MainFrameImpl()->GetScrollOffset().y());

  web_view_impl->MainFrameImpl()->SetScrollOffset(gfx::PointF(0, 2100));

  web_view_impl->DidExitFullscreen();
  UpdateAllLifecyclePhases();

  EXPECT_EQ(2100, web_view_impl->MainFrameImpl()->GetScrollOffset().y());
}

// Tests that background color is read from the backdrop on fullscreen.
TEST_F(WebViewTest, FullscreenBackgroundColor) {
  RegisterMockedHttpURLLoad("fullscreen_style.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad(base_url_ + "fullscreen_style.html");
  web_view_impl->MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  UpdateAllLifecyclePhases();
  EXPECT_EQ(SK_ColorWHITE, web_view_impl->BackgroundColor());

  // Enter fullscreen.
  LocalFrame* frame = web_view_impl->MainFrameImpl()->GetFrame();
  Element* element =
      frame->GetDocument()->getElementById(AtomicString("fullscreenElement"));
  ASSERT_TRUE(element);
  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*element);
  web_view_impl->DidEnterFullscreen();
  UpdateAllLifecyclePhases();

  EXPECT_EQ(SK_ColorYELLOW, web_view_impl->BackgroundColor());
}

static void ExitFullscreen(Document& document) {
  Fullscreen::FullyExitFullscreen(document);
  Fullscreen::DidExitFullscreen(document);
  EXPECT_EQ(Fullscreen::FullscreenElementFrom(document), nullptr);
}

// Tests that the removal from the top layer is scheduled.
TEST_F(WebViewTest, FullscreenRemovalTiming) {
  RegisterMockedHttpURLLoad("fullscreen_style.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad(base_url_ + "fullscreen_style.html");
  web_view_impl->MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  UpdateAllLifecyclePhases();

  // Enter fullscreen.
  LocalFrame* frame = web_view_impl->MainFrameImpl()->GetFrame();
  Document* document = frame->GetDocument();
  ASSERT_TRUE(document);
  Element* element =
      document->getElementById(AtomicString("fullscreenElement"));
  ASSERT_TRUE(element);
  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*element);
  web_view_impl->DidEnterFullscreen();
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(element->IsInTopLayer());

  ExitFullscreen(*document);
  EXPECT_TRUE(element->IsInTopLayer());
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(element->IsInTopLayer());
}

class PrintWebFrameClient : public frame_test_helpers::TestWebFrameClient {
 public:
  PrintWebFrameClient() = default;

  // WebLocalFrameClient overrides:
  void ScriptedPrint() override { print_called_ = true; }

  bool PrintCalled() const { return print_called_; }

 private:
  bool print_called_ = false;
};

TEST_F(WebViewTest, PrintWithXHRInFlight) {
  PrintWebFrameClient client;
  RegisterMockedHttpURLLoad("print_with_xhr_inflight.html");
  WebViewImpl* web_view_impl = web_view_helper_.InitializeAndLoad(
      base_url_ + "print_with_xhr_inflight.html", &client, nullptr);

  ASSERT_TRUE(To<LocalFrame>(web_view_impl->GetPage()->MainFrame())
                  ->GetDocument()
                  ->LoadEventFinished());
  EXPECT_TRUE(client.PrintCalled());
  web_view_helper_.Reset();
}

static void DragAndDropURL(WebViewImpl* web_view, const std::string& url) {
  WebDragData drag_data;
  WebDragData::StringItem item;
  item.type = "text/uri-list";
  item.data = WebString::FromUTF8(url);
  drag_data.AddItem(item);

  const gfx::PointF client_point;
  const gfx::PointF screen_point;
  WebFrameWidget* widget = web_view->MainFrameImpl()->FrameWidget();
  widget->DragTargetDragEnter(drag_data, client_point, screen_point,
                              kDragOperationCopy, 0, base::DoNothing());
  widget->DragTargetDrop(drag_data, client_point, screen_point, 0,
                         base::DoNothing());
  frame_test_helpers::PumpPendingRequestsForFrameToLoad(
      web_view->MainFrameImpl());
}

TEST_F(WebViewTest, DragDropURL) {
  RegisterMockedHttpURLLoad("foo.html");
  RegisterMockedHttpURLLoad("bar.html");

  const std::string foo_url = base_url_ + "foo.html";
  const std::string bar_url = base_url_ + "bar.html";

  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(foo_url);

  ASSERT_TRUE(web_view);

  // Drag and drop barUrl and verify that we've navigated to it.
  DragAndDropURL(web_view, bar_url);
  EXPECT_EQ(bar_url,
            web_view->MainFrameImpl()->GetDocument().Url().GetString().Utf8());

  // Drag and drop fooUrl and verify that we've navigated back to it.
  DragAndDropURL(web_view, foo_url);
  EXPECT_EQ(foo_url,
            web_view->MainFrameImpl()->GetDocument().Url().GetString().Utf8());

  // Disable navigation on drag-and-drop.
  auto renderer_preferences = web_view->GetRendererPreferences();
  renderer_preferences.can_accept_load_drops = false;
  web_view->SetRendererPreferences(renderer_preferences);

  // Attempt to drag and drop to barUrl and verify that no navigation has
  // occurred.
  DragAndDropURL(web_view, bar_url);
  EXPECT_EQ(foo_url,
            web_view->MainFrameImpl()->GetDocument().Url().GetString().Utf8());
}

bool WebViewTest::SimulateGestureAtElement(WebInputEvent::Type type,
                                           Element* element) {
  if (!element || !element->GetLayoutObject())
    return false;

  DCHECK(web_view_helper_.GetWebView());
  element->scrollIntoViewIfNeeded();

  gfx::Point center =
      web_view_helper_.GetWebView()
          ->MainFrameImpl()
          ->GetFrameView()
          ->FrameToScreen(element->GetLayoutObject()->AbsoluteBoundingBoxRect())
          .CenterPoint();

  WebGestureEvent event(type, WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(gfx::PointF(center));

  web_view_helper_.GetWebView()->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event, ui::LatencyInfo()));
  RunPendingTasks();
  return true;
}

bool WebViewTest::SimulateGestureAtElementById(WebInputEvent::Type type,
                                               const WebString& id) {
  DCHECK(web_view_helper_.GetWebView());
  Element* element = static_cast<Element*>(
      web_view_helper_.LocalMainFrame()->GetDocument().GetElementById(id));
  return SimulateGestureAtElement(type, element);
}

WebGestureEvent WebViewTest::BuildTapEvent(
    WebInputEvent::Type type,
    int tap_event_count,
    const gfx::PointF& position_in_widget) {
  WebGestureEvent event(type, WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(position_in_widget);

  switch (type) {
    case WebInputEvent::Type::kGestureTapDown:
      event.data.tap_down.tap_down_count = tap_event_count;
      break;
    case WebInputEvent::Type::kGestureTap:
      event.data.tap.tap_count = tap_event_count;
      break;
    default:
      break;
  }
  return event;
}

bool WebViewTest::SimulateTapEventAtElement(WebInputEvent::Type type,
                                            int tap_event_count,
                                            Element* element) {
  if (!element || !element->GetLayoutObject()) {
    return false;
  }

  DCHECK(web_view_helper_.GetWebView());
  element->scrollIntoViewIfNeeded();

  const gfx::PointF center = gfx::PointF(
      web_view_helper_.GetWebView()
          ->MainFrameImpl()
          ->GetFrameView()
          ->FrameToScreen(element->GetLayoutObject()->AbsoluteBoundingBoxRect())
          .CenterPoint());

  const WebGestureEvent event = BuildTapEvent(type, tap_event_count, center);
  web_view_helper_.GetWebView()->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event, ui::LatencyInfo()));
  RunPendingTasks();
  return true;
}

bool WebViewTest::SimulateTapEventAtElementById(WebInputEvent::Type type,
                                                int tap_event_count,
                                                const WebString& id) {
  DCHECK(web_view_helper_.GetWebView());
  auto* element = static_cast<Element*>(
      web_view_helper_.LocalMainFrame()->GetDocument().GetElementById(id));
  return SimulateTapEventAtElement(type, tap_event_count, element);
}

ExternalDateTimeChooser* WebViewTest::GetExternalDateTimeChooser(
    WebViewImpl* web_view_impl) {
  return web_view_impl->GetChromeClient()
      .GetExternalDateTimeChooserForTesting();
}

TEST_F(WebViewTest, ClientTapHandlingNullWebViewClient) {
  // Note: this test doesn't use WebViewHelper since WebViewHelper creates an
  // internal WebViewClient on demand if the supplied WebViewClient is null.
  WebViewImpl* web_view = web_view_helper_.CreateWebView(
      /*web_view_client=*/nullptr, /*compositing_enabled=*/false);
  frame_test_helpers::TestWebFrameClient web_frame_client;
  WebLocalFrame* local_frame = WebLocalFrame::CreateMainFrame(
      web_view, &web_frame_client, nullptr, mojo::NullRemote(),
      LocalFrameToken(), DocumentToken(), nullptr);
  web_frame_client.Bind(local_frame);
  WebNonCompositedWidgetClient widget_client;
  frame_test_helpers::TestWebFrameWidget* widget =
      web_view_helper_.CreateFrameWidget(local_frame);
  widget->InitializeNonCompositing(&widget_client);
  web_view->DidAttachLocalMainFrame();

  WebGestureEvent event(WebInputEvent::Type::kGestureTap,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(gfx::PointF(3, 8));
  EXPECT_EQ(WebInputEventResult::kNotHandled,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(event, ui::LatencyInfo())));
  web_view->Close();
}

TEST_F(WebViewTest, LongPressEmptyDiv) {
  RegisterMockedHttpURLLoad("long_press_empty_div.html");

  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "long_press_empty_div.html");
  web_view->SettingsImpl()->SetAlwaysShowContextMenuOnTouch(false);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebGestureEvent event(WebInputEvent::Type::kGestureLongPress,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(gfx::PointF(250, 150));

  EXPECT_EQ(WebInputEventResult::kNotHandled,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(event, ui::LatencyInfo())));
}

TEST_F(WebViewTest, LongPressEmptyDivAlwaysShow) {
  RegisterMockedHttpURLLoad("long_press_empty_div.html");

  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "long_press_empty_div.html");
  web_view->SettingsImpl()->SetAlwaysShowContextMenuOnTouch(true);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebGestureEvent event(WebInputEvent::Type::kGestureLongPress,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(gfx::PointF(250, 150));

  EXPECT_EQ(WebInputEventResult::kHandledSystem,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(event, ui::LatencyInfo())));
}

TEST_F(WebViewTest, LongPressObject) {
  RegisterMockedHttpURLLoad("long_press_object.html");

  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + "long_press_object.html");
  web_view->SettingsImpl()->SetAlwaysShowContextMenuOnTouch(true);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebGestureEvent event(WebInputEvent::Type::kGestureLongPress,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(gfx::PointF(10, 10));

  EXPECT_NE(WebInputEventResult::kHandledSystem,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(event, ui::LatencyInfo())));

  auto* element = To<HTMLElement>(static_cast<Node*>(
      web_view->MainFrameImpl()->GetDocument().GetElementById("obj")));
  EXPECT_FALSE(element->CanStartSelection());
}

TEST_F(WebViewTest, LongPressObjectFallback) {
  RegisterMockedHttpURLLoad("long_press_object_fallback.html");

  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "long_press_object_fallback.html");
  web_view->SettingsImpl()->SetAlwaysShowContextMenuOnTouch(true);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebGestureEvent event(WebInputEvent::Type::kGestureLongPress,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(gfx::PointF(10, 10));

  EXPECT_EQ(WebInputEventResult::kHandledSystem,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(event, ui::LatencyInfo())));

  auto* element = To<HTMLElement>(static_cast<Node*>(
      web_view->MainFrameImpl()->GetDocument().GetElementById("obj")));
  EXPECT_TRUE(element->CanStartSelection());
}

TEST_F(WebViewTest, LongPressImage) {
  RegisterMockedHttpURLLoad("long_press_image.html");

  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + "long_press_image.html");
  web_view->SettingsImpl()->SetAlwaysShowContextMenuOnTouch(false);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebGestureEvent event(WebInputEvent::Type::kGestureLongPress,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(gfx::PointF(10, 10));

  EXPECT_EQ(WebInputEventResult::kHandledSystem,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(event, ui::LatencyInfo())));
  EXPECT_TRUE(
      web_view->GetPage()->GetContextMenuController().ContextMenuNodeForFrame(
          web_view->MainFrameImpl()->GetFrame()));
}

TEST_F(WebViewTest, LongPressVideo) {
  RegisterMockedHttpURLLoad("long_press_video.html");

  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + "long_press_video.html");
  web_view->SettingsImpl()->SetAlwaysShowContextMenuOnTouch(false);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebGestureEvent event(WebInputEvent::Type::kGestureLongPress,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(gfx::PointF(10, 10));

  EXPECT_EQ(WebInputEventResult::kHandledSystem,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(event, ui::LatencyInfo())));
}

TEST_F(WebViewTest, LongPressLink) {
  RegisterMockedHttpURLLoad("long_press_link.html");

  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + "long_press_link.html");
  web_view->SettingsImpl()->SetAlwaysShowContextMenuOnTouch(false);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebGestureEvent event(WebInputEvent::Type::kGestureLongPress,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(gfx::PointF(500, 300));

  EXPECT_EQ(WebInputEventResult::kHandledSystem,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(event, ui::LatencyInfo())));
}

// Tests that we send touchcancel when drag start by long press.
TEST_F(WebViewTest, TouchCancelOnStartDragging) {
  RegisterMockedHttpURLLoad("long_press_draggable_div.html");

  url_test_helpers::RegisterMockedURLLoad(
      ToKURL("http://www.test.com/foo.png"),
      test::CoreTestDataPath("white-1x1.png"));
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "long_press_draggable_div.html");

  web_view->SettingsImpl()->SetTouchDragDropEnabled(true);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebPointerEvent pointer_down(
      WebInputEvent::Type::kPointerDown,
      WebPointerProperties(1, WebPointerProperties::PointerType::kTouch), 5, 5);
  pointer_down.SetPositionInWidget(250, 8);
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(pointer_down, ui::LatencyInfo()));
  web_view->MainFrameWidget()->DispatchBufferedTouchEvents();

  WebString target_id = WebString::FromUTF8("target");

  // Send long press to start dragging
  EXPECT_TRUE(SimulateGestureAtElementById(
      WebInputEvent::Type::kGestureLongPress, target_id));
  EXPECT_EQ("dragstart", web_view->MainFrameImpl()->GetDocument().Title());

  // Check pointer cancel is sent to dom.
  WebPointerEvent pointer_cancel(
      WebInputEvent::Type::kPointerCancel,
      WebPointerProperties(1, WebPointerProperties::PointerType::kTouch), 5, 5);
  pointer_cancel.SetPositionInWidget(250, 8);
  EXPECT_NE(WebInputEventResult::kHandledSuppressed,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(pointer_cancel, ui::LatencyInfo())));
  web_view->MainFrameWidget()->DispatchBufferedTouchEvents();
  EXPECT_EQ("touchcancel", web_view->MainFrameImpl()->GetDocument().Title());
}

// Tests that a touch drag context menu is enabled, a dragend shows a context
// menu when there is no drag.
TEST_F(WebViewTest, TouchDragContextMenuWithoutDrag) {
  RegisterMockedHttpURLLoad("long_press_draggable_div.html");

  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "long_press_draggable_div.html");

  web_view->SettingsImpl()->SetTouchDragDropEnabled(true);
  web_view->SettingsImpl()->SetTouchDragEndContextMenu(true);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebPointerEvent pointer_down(
      WebInputEvent::Type::kPointerDown,
      WebPointerProperties(1, WebPointerProperties::PointerType::kTouch), 5, 5);
  pointer_down.SetPositionInWidget(250, 8);
  pointer_down.SetPositionInScreen(250, 8);
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(pointer_down, ui::LatencyInfo()));
  web_view->MainFrameWidget()->DispatchBufferedTouchEvents();

  WebString target_id = WebString::FromUTF8("target");

  // Simulate long press to start dragging.
  EXPECT_TRUE(SimulateGestureAtElementById(
      WebInputEvent::Type::kGestureLongPress, target_id));
  EXPECT_EQ("dragstart", web_view->MainFrameImpl()->GetDocument().Title());

  // Simulate the end of a non-moving drag.
  const gfx::PointF dragend_point(250, 8);
  web_view->MainFrameViewWidget()->DragSourceEndedAt(
      dragend_point, dragend_point, ui::mojom::blink::DragOperation::kNone,
      base::DoNothing());
  EXPECT_TRUE(
      web_view->GetPage()->GetContextMenuController().ContextMenuNodeForFrame(
          web_view->MainFrameImpl()->GetFrame()));
}

// Tests that a dragend does not show a context menu after a drag when
// touch-drag-context-menu is enabled.
TEST_F(WebViewTest, TouchDragContextMenuAtDragEnd) {
  ScopedTouchDragAndContextMenuForTest touch_drag_and_context_menu(false);
  RegisterMockedHttpURLLoad("long_press_draggable_div.html");

  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "long_press_draggable_div.html");

  web_view->SettingsImpl()->SetTouchDragDropEnabled(true);
  web_view->SettingsImpl()->SetTouchDragEndContextMenu(true);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebPointerEvent pointer_down(
      WebInputEvent::Type::kPointerDown,
      WebPointerProperties(1, WebPointerProperties::PointerType::kTouch), 5, 5);
  pointer_down.SetPositionInWidget(250, 8);
  pointer_down.SetPositionInScreen(250, 8);
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(pointer_down, ui::LatencyInfo()));
  web_view->MainFrameWidget()->DispatchBufferedTouchEvents();

  WebString target_id = WebString::FromUTF8("target");

  // Simulate long press to start dragging.
  EXPECT_TRUE(SimulateGestureAtElementById(
      WebInputEvent::Type::kGestureLongPress, target_id));
  EXPECT_EQ("dragstart", web_view->MainFrameImpl()->GetDocument().Title());

  // Simulate the end of a drag.
  const gfx::PointF dragend_point(270, 28);
  web_view->MainFrameViewWidget()->DragSourceEndedAt(
      dragend_point, dragend_point, ui::mojom::blink::DragOperation::kNone,
      base::DoNothing());

  // TODO(https://crbug.com/1290905): When TouchDragAndContextMenu is enabled,
  // this becomes true.  This shouldn't be the case.
  EXPECT_FALSE(
      web_view->GetPage()->GetContextMenuController().ContextMenuNodeForFrame(
          web_view->MainFrameImpl()->GetFrame()));
}

TEST_F(WebViewTest, ContextMenuOnLinkAndImageLongPress) {
  ScopedTouchDragAndContextMenuForTest touch_drag_and_context_menu(false);
  RegisterMockedHttpURLLoad("long_press_links_and_images.html");

  url_test_helpers::RegisterMockedURLLoad(
      ToKURL("http://www.test.com/foo.png"),
      test::CoreTestDataPath("white-1x1.png"));
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "long_press_links_and_images.html");

  web_view->SettingsImpl()->SetTouchDragDropEnabled(true);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebString anchor_tag_id = WebString::FromUTF8("anchorTag");
  WebString image_tag_id = WebString::FromUTF8("imageTag");

  EXPECT_TRUE(SimulateGestureAtElementById(
      WebInputEvent::Type::kGestureLongPress, anchor_tag_id));
  EXPECT_EQ("contextmenu@a,", web_view->MainFrameImpl()->GetDocument().Title());

  EXPECT_TRUE(SimulateGestureAtElementById(
      WebInputEvent::Type::kGestureLongPress, image_tag_id));
  EXPECT_EQ("contextmenu@a,contextmenu@img,",
            web_view->MainFrameImpl()->GetDocument().Title());
}

TEST_F(WebViewTest, ContextMenuAndDragOnImageLongPress) {
  ScopedTouchDragOnShortPressForTest touch_drag_on_short_press(true);
  RegisterMockedHttpURLLoad("long_press_links_and_images.html");

  url_test_helpers::RegisterMockedURLLoad(
      ToKURL("http://www.test.com/foo.png"),
      test::CoreTestDataPath("white-1x1.png"));
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "long_press_links_and_images.html");

  web_view->SettingsImpl()->SetTouchDragDropEnabled(true);
  web_view->SettingsImpl()->SetModalContextMenu(false);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebString image_tag_id = WebString::FromUTF8("imageTag");

  EXPECT_TRUE(SimulateGestureAtElementById(
      WebInputEvent::Type::kGestureShortPress, image_tag_id));
  EXPECT_EQ("dragstart@img,",
            web_view->MainFrameImpl()->GetDocument().Title().Ascii());
  EXPECT_TRUE(SimulateGestureAtElementById(
      WebInputEvent::Type::kGestureLongPress, image_tag_id));
  EXPECT_EQ("dragstart@img,contextmenu@img,",
            web_view->MainFrameImpl()->GetDocument().Title().Ascii());
}

TEST_F(WebViewTest, ContextMenuAndDragOnLinkLongPress) {
  ScopedTouchDragOnShortPressForTest touch_drag_on_short_press(true);

  RegisterMockedHttpURLLoad("long_press_links_and_images.html");

  url_test_helpers::RegisterMockedURLLoad(
      ToKURL("http://www.test.com/foo.png"),
      test::CoreTestDataPath("white-1x1.png"));
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "long_press_links_and_images.html");

  web_view->SettingsImpl()->SetTouchDragDropEnabled(true);
  web_view->SettingsImpl()->SetModalContextMenu(false);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebString anchor_tag_id = WebString::FromUTF8("anchorTag");

  EXPECT_TRUE(SimulateGestureAtElementById(
      WebInputEvent::Type::kGestureShortPress, anchor_tag_id));
  EXPECT_EQ("dragstart@a,",
            web_view->MainFrameImpl()->GetDocument().Title().Ascii());
  EXPECT_TRUE(SimulateGestureAtElementById(
      WebInputEvent::Type::kGestureLongPress, anchor_tag_id));
  EXPECT_EQ("dragstart@a,contextmenu@a,",
            web_view->MainFrameImpl()->GetDocument().Title().Ascii());
}

TEST_F(WebViewTest, LongPressEmptyEditableSelection) {
  RegisterMockedHttpURLLoad("long_press_empty_editable_selection.html");

  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "long_press_empty_editable_selection.html");
  web_view->SettingsImpl()->SetAlwaysShowContextMenuOnTouch(false);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebGestureEvent event(WebInputEvent::Type::kGestureLongPress,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(gfx::PointF(10, 10));

  EXPECT_EQ(WebInputEventResult::kHandledSystem,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(event, ui::LatencyInfo())));
}

TEST_F(WebViewTest, LongPressEmptyNonEditableSelection) {
  RegisterMockedHttpURLLoad("long_press_image.html");

  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + "long_press_image.html");
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 500));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebGestureEvent event(WebInputEvent::Type::kGestureLongPress,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(gfx::PointF(300, 300));
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();

  EXPECT_EQ(WebInputEventResult::kHandledSystem,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(event, ui::LatencyInfo())));
  EXPECT_TRUE(frame->SelectionAsText().IsEmpty());
}

TEST_F(WebViewTest, LongPressSelection) {
  RegisterMockedHttpURLLoad("longpress_selection.html");

  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "longpress_selection.html");
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebString target = WebString::FromUTF8("target");
  WebString onselectstartfalse = WebString::FromUTF8("onselectstartfalse");
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();

  EXPECT_TRUE(SimulateGestureAtElementById(
      WebInputEvent::Type::kGestureLongPress, onselectstartfalse));
  EXPECT_EQ("", frame->SelectionAsText().Utf8());
  EXPECT_TRUE(SimulateGestureAtElementById(
      WebInputEvent::Type::kGestureLongPress, target));
  EXPECT_EQ("testword", frame->SelectionAsText().Utf8());
}

TEST_F(WebViewTest, DoublePressSelection) {
  ScopedTouchTextEditingRedesignForTest touch_text_editing_redesign(true);
  RegisterMockedHttpURLLoad("double_press_selection.html");

  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "double_press_selection.html");
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebString target = WebString::FromUTF8("target");
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();

  // Double press should select nearest word.
  EXPECT_TRUE(SimulateTapEventAtElementById(
      WebInputEvent::Type::kGestureTapDown, 1, target));
  EXPECT_TRUE(SimulateTapEventAtElementById(WebInputEvent::Type::kGestureTap, 1,
                                            target));
  EXPECT_TRUE(SimulateTapEventAtElementById(
      WebInputEvent::Type::kGestureTapDown, 2, target));
  EXPECT_EQ("selection", frame->SelectionAsText().Utf8());

  // Releasing double tap should keep the selection.
  EXPECT_TRUE(SimulateTapEventAtElementById(WebInputEvent::Type::kGestureTap, 2,
                                            target));
  EXPECT_EQ("selection", frame->SelectionAsText().Utf8());
}

TEST_F(WebViewTest, DoublePressSelectionOnSelectStartFalse) {
  ScopedTouchTextEditingRedesignForTest touch_text_editing_redesign(true);
  RegisterMockedHttpURLLoad("double_press_selection.html");

  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "double_press_selection.html");
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebString onselectstartfalse = WebString::FromUTF8("onselectstartfalse");
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();

  // Should not select anything when onselectstart is false.
  EXPECT_TRUE(SimulateTapEventAtElementById(
      WebInputEvent::Type::kGestureTapDown, 1, onselectstartfalse));
  EXPECT_TRUE(SimulateTapEventAtElementById(WebInputEvent::Type::kGestureTap, 1,
                                            onselectstartfalse));
  EXPECT_TRUE(SimulateTapEventAtElementById(
      WebInputEvent::Type::kGestureTapDown, 2, onselectstartfalse));
  EXPECT_EQ("", frame->SelectionAsText().Utf8());
  EXPECT_TRUE(SimulateTapEventAtElementById(WebInputEvent::Type::kGestureTap, 2,
                                            onselectstartfalse));
  EXPECT_EQ("", frame->SelectionAsText().Utf8());
}

TEST_F(WebViewTest, DoublePressSelectionPreventDefaultMouseDown) {
  ScopedTouchTextEditingRedesignForTest touch_text_editing_redesign(true);
  RegisterMockedHttpURLLoad("double_press_selection.html");

  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "double_press_selection.html");
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  web_view->MainFrameImpl()->ExecuteScript(
      WebScriptSource("document.getElementById('targetdiv').addEventListener("
                      "'mousedown', function(e) { e.preventDefault();});"));

  WebString target = WebString::FromUTF8("target");
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();

  // Double press should not select anything.
  EXPECT_TRUE(SimulateTapEventAtElementById(
      WebInputEvent::Type::kGestureTapDown, 1, target));
  EXPECT_TRUE(SimulateTapEventAtElementById(WebInputEvent::Type::kGestureTap, 1,
                                            target));
  EXPECT_TRUE(SimulateTapEventAtElementById(
      WebInputEvent::Type::kGestureTapDown, 2, target));
  EXPECT_EQ("", frame->SelectionAsText().Utf8());

  // Releasing double tap also should not select anything.
  EXPECT_TRUE(SimulateTapEventAtElementById(WebInputEvent::Type::kGestureTap, 2,
                                            target));
  EXPECT_EQ("", frame->SelectionAsText().Utf8());
}

TEST_F(WebViewTest, FinishComposingTextDoesNotDismissHandles) {
  RegisterMockedHttpURLLoad("longpress_selection.html");

  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "longpress_selection.html");
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebString target = WebString::FromUTF8("target");
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  web_view->SetIsActive(true);
  web_view->SetPageFocus(true);
  WebInputMethodController* active_input_method_controller =
      frame->FrameWidget()->GetActiveWebInputMethodController();
  EXPECT_TRUE(
      SimulateGestureAtElementById(WebInputEvent::Type::kGestureTap, target));
  WebVector<ui::ImeTextSpan> empty_ime_text_spans;
  frame->SetEditableSelectionOffsets(8, 8);
  EXPECT_TRUE(active_input_method_controller->SetComposition(
      "12345", empty_ime_text_spans, WebRange(), 8, 13));
  EXPECT_TRUE(frame->GetFrame()->GetInputMethodController().HasComposition());
  EXPECT_EQ("", frame->SelectionAsText().Utf8());
  EXPECT_FALSE(frame->GetFrame()->Selection().IsHandleVisible());
  EXPECT_TRUE(frame->GetFrame()->GetInputMethodController().HasComposition());

  EXPECT_TRUE(SimulateGestureAtElementById(
      WebInputEvent::Type::kGestureLongPress, target));
  EXPECT_EQ("testword12345", frame->SelectionAsText().Utf8());
  EXPECT_TRUE(frame->GetFrame()->Selection().IsHandleVisible());
  EXPECT_TRUE(frame->GetFrame()->GetInputMethodController().HasComposition());

  // Check that finishComposingText(KeepSelection) does not dismiss handles.
  active_input_method_controller->FinishComposingText(
      WebInputMethodController::kKeepSelection);
  EXPECT_TRUE(frame->GetFrame()->Selection().IsHandleVisible());
}

#if !BUILDFLAG(IS_MAC)
TEST_F(WebViewTest, TouchDoesntSelectEmptyTextarea) {
  RegisterMockedHttpURLLoad("longpress_textarea.html");

  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + "longpress_textarea.html");
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebString blanklinestextbox = WebString::FromUTF8("blanklinestextbox");
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();

  // Long-press on carriage returns.
  EXPECT_TRUE(SimulateGestureAtElementById(
      WebInputEvent::Type::kGestureLongPress, blanklinestextbox));
  EXPECT_TRUE(frame->SelectionAsText().IsEmpty());

  // Double-tap on carriage returns.
  WebGestureEvent event(WebInputEvent::Type::kGestureTap,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(gfx::PointF(100, 25));
  event.data.tap.tap_count = 2;

  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event, ui::LatencyInfo()));
  EXPECT_TRUE(frame->SelectionAsText().IsEmpty());

  auto* text_area_element = To<HTMLTextAreaElement>(static_cast<Node*>(
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          blanklinestextbox)));
  text_area_element->SetValue("hello");

  // Long-press past last word of textbox.
  EXPECT_TRUE(SimulateGestureAtElementById(
      WebInputEvent::Type::kGestureLongPress, blanklinestextbox));
  EXPECT_TRUE(frame->SelectionAsText().IsEmpty());

  // Double-tap past last word of textbox.
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event, ui::LatencyInfo()));
  EXPECT_TRUE(frame->SelectionAsText().IsEmpty());
}
#endif

TEST_F(WebViewTest, LongPressImageTextarea) {
  RegisterMockedHttpURLLoad("longpress_image_contenteditable.html");

  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "longpress_image_contenteditable.html");
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebString image = WebString::FromUTF8("purpleimage");

  EXPECT_TRUE(SimulateGestureAtElementById(
      WebInputEvent::Type::kGestureLongPress, image));
  WebRange range = web_view->MainFrameImpl()
                       ->GetInputMethodController()
                       ->GetSelectionOffsets();
  EXPECT_FALSE(range.IsNull());
  EXPECT_EQ(0, range.StartOffset());
  EXPECT_EQ(1, range.length());
}

TEST_F(WebViewTest, BlinkCaretAfterLongPress) {
  RegisterMockedHttpURLLoad("blink_caret_on_typing_after_long_press.html");

  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "blink_caret_on_typing_after_long_press.html");
  web_view->MainFrameViewWidget()->Resize(gfx::Size(640, 480));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebString target = WebString::FromUTF8("target");
  WebLocalFrameImpl* main_frame = web_view->MainFrameImpl();

  EXPECT_TRUE(SimulateGestureAtElementById(
      WebInputEvent::Type::kGestureLongPress, target));
  EXPECT_FALSE(main_frame->GetFrame()->Selection().IsCaretBlinkingSuspended());
}

TEST_F(WebViewTest, BlinkCaretOnClosingContextMenu) {
  RegisterMockedHttpURLLoad("form.html");
  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + "form.html");

  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  RunPendingTasks();

  // We suspend caret blinking when pressing with mouse right button.
  // Note that we do not send MouseUp event here since it will be consumed
  // by the context menu once it shows up.
  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests());

  mouse_event.button = WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(1, 1);
  mouse_event.click_count = 1;
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_event, ui::LatencyInfo()));
  RunPendingTasks();

  WebLocalFrameImpl* main_frame = web_view->MainFrameImpl();
  EXPECT_TRUE(main_frame->GetFrame()->Selection().IsCaretBlinkingSuspended());

  // Caret blinking is still suspended after showing context menu.
  web_view->MainFrameImpl()->LocalRootFrameWidget()->ShowContextMenu(
      ui::mojom::MenuSourceType::MOUSE,
      web_view->MainFrameImpl()->GetPositionInViewportForTesting());

  EXPECT_TRUE(main_frame->GetFrame()->Selection().IsCaretBlinkingSuspended());

  // Caret blinking will be resumed only after context menu is closed.
  web_view->DidCloseContextMenu();

  EXPECT_FALSE(main_frame->GetFrame()->Selection().IsCaretBlinkingSuspended());
}

TEST_F(WebViewTest, SelectionOnReadOnlyInput) {
  RegisterMockedHttpURLLoad("selection_readonly.html");
  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + "selection_readonly.html");
  web_view->MainFrameViewWidget()->Resize(gfx::Size(640, 480));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  std::string test_word = "This text should be selected.";

  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  EXPECT_EQ(test_word, frame->SelectionAsText().Utf8());

  WebRange range = web_view->MainFrameImpl()
                       ->GetInputMethodController()
                       ->GetSelectionOffsets();
  EXPECT_FALSE(range.IsNull());
  EXPECT_EQ(0, range.StartOffset());
  EXPECT_EQ(static_cast<int>(test_word.length()), range.length());
}

TEST_F(WebViewTest, KeyDownScrollsHandled) {
  RegisterMockedHttpURLLoad("content-width-1000.html");

  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + "content-width-1000.html");
  web_view->MainFrameViewWidget()->Resize(gfx::Size(100, 100));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebKeyboardEvent key_event(WebInputEvent::Type::kRawKeyDown,
                             WebInputEvent::kNoModifiers,
                             WebInputEvent::GetStaticTimeStampForTests());

  // RawKeyDown pagedown should be handled.
  key_event.windows_key_code = VKEY_NEXT;
  EXPECT_EQ(WebInputEventResult::kHandledSystem,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(key_event, ui::LatencyInfo())));
  key_event.SetType(WebInputEvent::Type::kKeyUp);
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event, ui::LatencyInfo()));

  // Coalesced KeyDown arrow-down should be handled.
  key_event.windows_key_code = VKEY_DOWN;
  key_event.SetType(WebInputEvent::Type::kKeyDown);
  EXPECT_EQ(WebInputEventResult::kHandledSystem,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(key_event, ui::LatencyInfo())));
  key_event.SetType(WebInputEvent::Type::kKeyUp);
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event, ui::LatencyInfo()));

  // Ctrl-Home should be handled...
  key_event.windows_key_code = VKEY_HOME;
  key_event.SetModifiers(WebInputEvent::kControlKey);
  key_event.SetType(WebInputEvent::Type::kRawKeyDown);
  EXPECT_EQ(WebInputEventResult::kNotHandled,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(key_event, ui::LatencyInfo())));
  key_event.SetType(WebInputEvent::Type::kKeyUp);
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event, ui::LatencyInfo()));

  // But Ctrl-Down should not.
  key_event.windows_key_code = VKEY_DOWN;
  key_event.SetModifiers(WebInputEvent::kControlKey);
  key_event.SetType(WebInputEvent::Type::kRawKeyDown);
  EXPECT_EQ(WebInputEventResult::kNotHandled,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(key_event, ui::LatencyInfo())));
  key_event.SetType(WebInputEvent::Type::kKeyUp);
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event, ui::LatencyInfo()));

  // Shift, meta, and alt should not be handled.
  key_event.windows_key_code = VKEY_NEXT;
  key_event.SetModifiers(WebInputEvent::kShiftKey);
  key_event.SetType(WebInputEvent::Type::kRawKeyDown);
  EXPECT_EQ(WebInputEventResult::kNotHandled,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(key_event, ui::LatencyInfo())));
  key_event.SetType(WebInputEvent::Type::kKeyUp);
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event, ui::LatencyInfo()));

  key_event.windows_key_code = VKEY_NEXT;
  key_event.SetModifiers(WebInputEvent::kMetaKey);
  key_event.SetType(WebInputEvent::Type::kRawKeyDown);
  EXPECT_EQ(WebInputEventResult::kNotHandled,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(key_event, ui::LatencyInfo())));
  key_event.SetType(WebInputEvent::Type::kKeyUp);
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event, ui::LatencyInfo()));

  key_event.windows_key_code = VKEY_NEXT;
  key_event.SetModifiers(WebInputEvent::kAltKey);
  key_event.SetType(WebInputEvent::Type::kRawKeyDown);
  EXPECT_EQ(WebInputEventResult::kNotHandled,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(key_event, ui::LatencyInfo())));
  key_event.SetType(WebInputEvent::Type::kKeyUp);
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event, ui::LatencyInfo()));

  // System-key labeled Alt-Down (as in Windows) should do nothing,
  // but non-system-key labeled Alt-Down (as in Mac) should be handled
  // as a page-down.
  key_event.windows_key_code = VKEY_DOWN;
  key_event.SetModifiers(WebInputEvent::kAltKey);
  key_event.is_system_key = true;
  key_event.SetType(WebInputEvent::Type::kRawKeyDown);
  EXPECT_EQ(WebInputEventResult::kNotHandled,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(key_event, ui::LatencyInfo())));
  key_event.SetType(WebInputEvent::Type::kKeyUp);
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event, ui::LatencyInfo()));

  key_event.windows_key_code = VKEY_DOWN;
  key_event.SetModifiers(WebInputEvent::kAltKey);
  key_event.is_system_key = false;
  key_event.SetType(WebInputEvent::Type::kRawKeyDown);
  EXPECT_EQ(WebInputEventResult::kHandledSystem,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(key_event, ui::LatencyInfo())));
  key_event.SetType(WebInputEvent::Type::kKeyUp);
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event, ui::LatencyInfo()));
}

class MiddleClickAutoscrollWebFrameWidget
    : public frame_test_helpers::TestWebFrameWidget {
 public:
  template <typename... Args>
  explicit MiddleClickAutoscrollWebFrameWidget(Args&&... args)
      : frame_test_helpers::TestWebFrameWidget(std::forward<Args>(args)...) {}

  // FrameWidget overrides:
  void DidChangeCursor(const ui::Cursor& cursor) override {
    last_cursor_type_ = cursor.type();
  }

  ui::mojom::blink::CursorType GetLastCursorType() const {
    return last_cursor_type_;
  }

 private:
  ui::mojom::blink::CursorType last_cursor_type_ =
      ui::mojom::blink::CursorType::kPointer;
};

class MiddleClickWebViewTest : public WebViewTest {
 public:
  MiddleClickWebViewTest()
      : WebViewTest(WTF::BindRepeating(
            &frame_test_helpers::WebViewHelper::CreateTestWebFrameWidget<
                MiddleClickAutoscrollWebFrameWidget>)) {}
};

TEST_F(MiddleClickWebViewTest, MiddleClickAutoscrollCursor) {
  ScopedMiddleClickAutoscrollForTest middle_click_autoscroll(true);
  RegisterMockedHttpURLLoad("content-width-1000.html");

  // We will be changing the size of the page to test each of the panning
  // cursor variations. For reference, content-width-1000.html is 1000px wide
  // and 2000px tall.
  // 1. 100 x 100 - The page will be scrollable in both x and y directions, so
  //      we expect to see the cursor with arrows in all four directions.
  // 2. 1010 x 100 - The page will be scrollable in the y direction, but not x,
  //      so we expect to see the cursor with only the vertical arrows.
  // 3. 100 x 2010 - The page will be scrollable in the x direction, but not y,
  //      so we expect to see the cursor with only the horizontal arrows.
  struct CursorTests {
    int resize_width;
    int resize_height;
    ui::mojom::blink::CursorType expected_cursor;
  } cursor_tests[] = {{100, 100, MiddlePanningCursor().type()},
                      {1010, 100, MiddlePanningVerticalCursor().type()},
                      {100, 2010, MiddlePanningHorizontalCursor().type()}};

  for (const CursorTests current_test : cursor_tests) {
    WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
        base_url_ + "content-width-1000.html", nullptr, nullptr);
    web_view->MainFrameWidget()->Resize(
        gfx::Size(current_test.resize_width, current_test.resize_height));
    UpdateAllLifecyclePhases();
    RunPendingTasks();

    MiddleClickAutoscrollWebFrameWidget* widget =
        static_cast<MiddleClickAutoscrollWebFrameWidget*>(
            web_view_helper_.GetMainFrameWidget());
    LocalFrame* local_frame =
        To<WebLocalFrameImpl>(web_view->MainFrame())->GetFrame();

    // Setup a mock clipboard.  On linux, middle click can paste from the
    // clipboard, so the input handler below will access the clipboard.
    PageTestBase::MockClipboardHostProvider mock_clip_host_provider(
        local_frame->GetBrowserInterfaceBroker());

    WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests());
    mouse_event.button = WebMouseEvent::Button::kMiddle;
    mouse_event.SetPositionInWidget(1, 1);
    mouse_event.click_count = 1;

    // Start middle-click autoscroll.
    web_view->MainFrameWidget()->HandleInputEvent(
        WebCoalescedInputEvent(mouse_event, ui::LatencyInfo()));
    mouse_event.SetType(WebInputEvent::Type::kMouseUp);
    web_view->MainFrameWidget()->HandleInputEvent(
        WebCoalescedInputEvent(mouse_event, ui::LatencyInfo()));

    EXPECT_EQ(current_test.expected_cursor, widget->GetLastCursorType());

    // Even if a plugin tries to change the cursor type, that should be ignored
    // during middle-click autoscroll.
    web_view->GetChromeClient().SetCursorForPlugin(PointerCursor(),
                                                   local_frame);
    EXPECT_EQ(current_test.expected_cursor, widget->GetLastCursorType());

    // End middle-click autoscroll.
    mouse_event.SetType(WebInputEvent::Type::kMouseDown);
    web_view->MainFrameWidget()->HandleInputEvent(
        WebCoalescedInputEvent(mouse_event, ui::LatencyInfo()));
    mouse_event.SetType(WebInputEvent::Type::kMouseUp);
    web_view->MainFrameWidget()->HandleInputEvent(
        WebCoalescedInputEvent(mouse_event, ui::LatencyInfo()));

    web_view->GetChromeClient().SetCursorForPlugin(IBeamCursor(), local_frame);
    EXPECT_EQ(IBeamCursor().type(), widget->GetLastCursorType());
  }

  // Explicitly reset to break dependency on locally scoped client.
  web_view_helper_.Reset();
}

TEST_F(WebViewTest, ShowPressOnTransformedLink) {
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.Initialize();
  web_view_impl->GetPage()
      ->GetSettings()
      .SetPreferCompositingToLCDTextForTesting(true);

  int page_width = 640;
  int page_height = 480;
  web_view_impl->MainFrameViewWidget()->Resize(
      gfx::Size(page_width, page_height));

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(
      web_view_impl->MainFrameImpl(),
      "<a href='http://www.test.com' style='position: absolute; left: 20px; "
      "top: 20px; width: 200px; transform:translateZ(0);'>A link to "
      "highlight</a>",
      base_url);

  WebGestureEvent event(WebInputEvent::Type::kGestureShowPress,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(gfx::PointF(20, 20));

  // Just make sure we don't hit any asserts.
  web_view_impl->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event, ui::LatencyInfo()));
}

class MockAutofillClient : public WebAutofillClient {
 public:
  MockAutofillClient() = default;

  ~MockAutofillClient() override = default;

  void TextFieldDidChange(const WebFormControlElement&) override {
    ++text_changes_;
  }
  void UserGestureObserved() override { ++user_gesture_notifications_count_; }

  bool ShouldSuppressKeyboard(const WebFormControlElement&) override {
    return should_suppress_keyboard_;
  }

  void SetShouldSuppressKeyboard(bool should_suppress_keyboard) {
    should_suppress_keyboard_ = should_suppress_keyboard;
  }

  void ClearChangeCounts() { text_changes_ = 0; }

  int TextChanges() { return text_changes_; }
  int GetUserGestureNotificationsCount() {
    return user_gesture_notifications_count_;
  }

 private:
  int text_changes_ = 0;
  int user_gesture_notifications_count_ = 0;
  bool should_suppress_keyboard_ = false;
};

TEST_F(WebViewTest, LosingFocusDoesNotTriggerAutofillTextChange) {
  RegisterMockedHttpURLLoad("input_field_populated.html");
  MockAutofillClient client;
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "input_field_populated.html");
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  frame->SetAutofillClient(&client);
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);

  // Set up a composition that needs to be committed.
  WebVector<ui::ImeTextSpan> empty_ime_text_spans;
  frame->SetEditableSelectionOffsets(4, 10);
  frame->SetCompositionFromExistingText(8, 12, empty_ime_text_spans);
  WebTextInputInfo info = frame->GetInputMethodController()->TextInputInfo();
  EXPECT_EQ(4, info.selection_start);
  EXPECT_EQ(10, info.selection_end);
  EXPECT_EQ(8, info.composition_start);
  EXPECT_EQ(12, info.composition_end);

  // Clear the focus and track that the subsequent composition commit does not
  // trigger a text changed notification for autofill.
  client.ClearChangeCounts();
  web_view->MainFrameWidget()->SetFocus(false);
  EXPECT_EQ(0, client.TextChanges());

  frame->SetAutofillClient(nullptr);
}

static void VerifySelectionAndComposition(WebViewImpl* web_view,
                                          int selection_start,
                                          int selection_end,
                                          int composition_start,
                                          int composition_end,
                                          const char* fail_message) {
  WebTextInputInfo info =
      web_view->MainFrameImpl()->GetInputMethodController()->TextInputInfo();
  EXPECT_EQ(selection_start, info.selection_start) << fail_message;
  EXPECT_EQ(selection_end, info.selection_end) << fail_message;
  EXPECT_EQ(composition_start, info.composition_start) << fail_message;
  EXPECT_EQ(composition_end, info.composition_end) << fail_message;
}

TEST_F(WebViewTest, CompositionNotCancelledByBackspace) {
  RegisterMockedHttpURLLoad("composition_not_cancelled_by_backspace.html");
  MockAutofillClient client;
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "composition_not_cancelled_by_backspace.html");
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  frame->SetAutofillClient(&client);
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);

  // Test both input elements.
  for (int i = 0; i < 2; ++i) {
    // Select composition and do sanity check.
    WebVector<ui::ImeTextSpan> empty_ime_text_spans;
    frame->SetEditableSelectionOffsets(6, 6);
    WebInputMethodController* active_input_method_controller =
        frame->FrameWidget()->GetActiveWebInputMethodController();
    EXPECT_TRUE(active_input_method_controller->SetComposition(
        "fghij", empty_ime_text_spans, WebRange(), 0, 5));
    frame->SetEditableSelectionOffsets(11, 11);
    VerifySelectionAndComposition(web_view, 11, 11, 6, 11, "initial case");

    // Press Backspace and verify composition didn't get cancelled. This is to
    // verify the fix for crbug.com/429916.
    WebKeyboardEvent key_event(WebInputEvent::Type::kRawKeyDown,
                               WebInputEvent::kNoModifiers,
                               WebInputEvent::GetStaticTimeStampForTests());
    key_event.dom_key = ui::DomKey::BACKSPACE;
    key_event.windows_key_code = VKEY_BACK;
    web_view->MainFrameWidget()->HandleInputEvent(
        WebCoalescedInputEvent(key_event, ui::LatencyInfo()));

    frame->SetEditableSelectionOffsets(6, 6);
    EXPECT_TRUE(active_input_method_controller->SetComposition(
        "fghi", empty_ime_text_spans, WebRange(), 0, 4));
    frame->SetEditableSelectionOffsets(10, 10);
    VerifySelectionAndComposition(web_view, 10, 10, 6, 10,
                                  "after pressing Backspace");

    key_event.SetType(WebInputEvent::Type::kKeyUp);
    web_view->MainFrameWidget()->HandleInputEvent(
        WebCoalescedInputEvent(key_event, ui::LatencyInfo()));

    web_view->AdvanceFocus(false);
  }

  frame->SetAutofillClient(nullptr);
}

TEST_F(WebViewTest, FinishComposingTextDoesntTriggerAutofillTextChange) {
  RegisterMockedHttpURLLoad("input_field_populated.html");
  MockAutofillClient client;
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "input_field_populated.html");
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  frame->SetAutofillClient(&client);
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);

  WebDocument document = web_view->MainFrameImpl()->GetDocument();
  auto* form = To<HTMLFormControlElement>(
      static_cast<Element*>(document.GetElementById("sample")));

  WebInputMethodController* active_input_method_controller =
      frame->FrameWidget()->GetActiveWebInputMethodController();
  // Set up a composition that needs to be committed.
  std::string composition_text("testingtext");

  WebVector<ui::ImeTextSpan> empty_ime_text_spans;
  active_input_method_controller->SetComposition(
      WebString::FromUTF8(composition_text), empty_ime_text_spans, WebRange(),
      0, static_cast<int>(composition_text.length()));

  WebTextInputInfo info = active_input_method_controller->TextInputInfo();
  EXPECT_EQ(0, info.selection_start);
  EXPECT_EQ((int)composition_text.length(), info.selection_end);
  EXPECT_EQ(0, info.composition_start);
  EXPECT_EQ((int)composition_text.length(), info.composition_end);

  form->SetAutofillState(blink::WebAutofillState::kAutofilled);
  client.ClearChangeCounts();

  active_input_method_controller->FinishComposingText(
      WebInputMethodController::kKeepSelection);
  EXPECT_EQ(0, client.TextChanges());

  EXPECT_TRUE(form->IsAutofilled());

  frame->SetAutofillClient(nullptr);
}

TEST_F(WebViewTest,
       SetCompositionFromExistingTextDoesntTriggerAutofillTextChange) {
  RegisterMockedHttpURLLoad("input_field_populated.html");
  MockAutofillClient client;
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "input_field_populated.html");
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  frame->SetAutofillClient(&client);
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);

  WebVector<ui::ImeTextSpan> empty_ime_text_spans;

  client.ClearChangeCounts();
  frame->SetCompositionFromExistingText(8, 12, empty_ime_text_spans);

  WebTextInputInfo info = frame->GetInputMethodController()->TextInputInfo();
  EXPECT_EQ("0123456789abcdefghijklmnopqrstuvwxyz", info.value.Utf8());
  EXPECT_EQ(8, info.composition_start);
  EXPECT_EQ(12, info.composition_end);

  EXPECT_EQ(0, client.TextChanges());

  WebDocument document = web_view->MainFrameImpl()->GetDocument();
  EXPECT_EQ(WebString::FromUTF8("none"),
            document.GetElementById("inputEvent").FirstChild().NodeValue());

  frame->SetAutofillClient(nullptr);
}

class ViewCreatingWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  // WebLocalFrameClient overrides.
  WebView* CreateNewWindow(
      const WebURLRequest&,
      const WebWindowFeatures&,
      const WebString& name,
      WebNavigationPolicy,
      network::mojom::blink::WebSandboxFlags,
      const SessionStorageNamespaceId&,
      bool& consumed_user_gesture,
      const std::optional<Impression>&,
      const std::optional<WebPictureInPictureWindowOptions>&,
      const WebURL&) override {
    return web_view_helper_.InitializeWithOpener(Frame());
  }
  WebView* CreatedWebView() const { return web_view_helper_.GetWebView(); }

 private:
  frame_test_helpers::WebViewHelper web_view_helper_;
};

class ViewCreatingWebViewClient : public WebViewClient {
 public:
  ViewCreatingWebViewClient() = default;

  void DidFocus() override { did_focus_called_ = true; }

  bool DidFocusCalled() const { return did_focus_called_; }

 private:
  bool did_focus_called_ = false;
};

TEST_F(WebViewTest, DoNotFocusCurrentFrameOnNavigateFromLocalFrame) {
  ViewCreatingWebFrameClient frame_client;
  ViewCreatingWebViewClient client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl =
      web_view_helper.Initialize(&frame_client, &client);

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(
      web_view_impl->MainFrameImpl(),
      "<html><body><iframe src=\"about:blank\"></iframe></body></html>",
      base_url);

  // Make a request from a local frame.
  WebURLRequest web_url_request_with_target_start(KURL("about:blank"));
  LocalFrame* local_frame =
      To<WebLocalFrameImpl>(web_view_impl->MainFrame()->FirstChild())
          ->GetFrame();
  FrameLoadRequest request_with_target_start(
      local_frame->DomWindow(),
      web_url_request_with_target_start.ToResourceRequest());
  local_frame->Tree().FindOrCreateFrameForNavigation(request_with_target_start,
                                                     AtomicString("_top"));
  EXPECT_FALSE(client.DidFocusCalled());

  web_view_helper.Reset();  // Remove dependency on locally scoped client.
}

TEST_F(WebViewTest, FocusExistingFrameOnNavigate) {
  ViewCreatingWebFrameClient frame_client;
  ViewCreatingWebViewClient client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl =
      web_view_helper.Initialize(&frame_client, &client);
  WebLocalFrameImpl* frame = web_view_impl->MainFrameImpl();
  frame->SetName("_start");

  // Make a request that will open a new window
  WebURLRequest web_url_request(KURL("about:blank"));
  FrameLoadRequest request(nullptr, web_url_request.ToResourceRequest());
  To<LocalFrame>(web_view_impl->GetPage()->MainFrame())
      ->Tree()
      .FindOrCreateFrameForNavigation(request, AtomicString("_blank"));
  ASSERT_TRUE(frame_client.CreatedWebView());
  EXPECT_FALSE(client.DidFocusCalled());

  // Make a request from the new window that will navigate the original window.
  // The original window should be focused.
  WebURLRequest web_url_request_with_target_start(KURL("about:blank"));
  FrameLoadRequest request_with_target_start(
      nullptr, web_url_request_with_target_start.ToResourceRequest());
  To<LocalFrame>(static_cast<WebViewImpl*>(frame_client.CreatedWebView())
                     ->GetPage()
                     ->MainFrame())
      ->Tree()
      .FindOrCreateFrameForNavigation(request_with_target_start,
                                      AtomicString("_start"));
  EXPECT_TRUE(client.DidFocusCalled());

  web_view_helper.Reset();  // Remove dependency on locally scoped client.
}

class ViewReusingWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  ViewReusingWebFrameClient() = default;

  // WebLocalFrameClient methods
  WebView* CreateNewWindow(
      const WebURLRequest&,
      const WebWindowFeatures&,
      const WebString& name,
      WebNavigationPolicy,
      network::mojom::blink::WebSandboxFlags,
      const SessionStorageNamespaceId&,
      bool& consumed_user_gesture,
      const std::optional<Impression>&,
      const std::optional<WebPictureInPictureWindowOptions>&,
      const WebURL&) override {
    return web_view_;
  }

  void SetWebView(WebView* view) { web_view_ = view; }

 private:
  WebView* web_view_ = nullptr;
};

TEST_F(WebViewTest,
       ReuseExistingWindowOnCreateViewUsesCorrectNavigationPolicy) {
  ViewReusingWebFrameClient frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.Initialize(&frame_client);
  frame_client.SetWebView(web_view_impl);
  LocalFrame* frame = To<LocalFrame>(web_view_impl->GetPage()->MainFrame());

  // Request a new window, but the WebViewClient will decline to and instead
  // return the current window.
  WebURLRequest web_url_request(KURL("about:blank"));
  FrameLoadRequest request(frame->DomWindow(),
                           web_url_request.ToResourceRequest());
  FrameTree::FindResult result = frame->Tree().FindOrCreateFrameForNavigation(
      request, AtomicString("_blank"));
  EXPECT_EQ(frame, result.frame);
  EXPECT_EQ(kNavigationPolicyCurrentTab, request.GetNavigationPolicy());
}

TEST_F(WebViewTest, DispatchesFocusOutFocusInOnViewToggleFocus) {
  RegisterMockedHttpURLLoad("focusout_focusin_events.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "focusout_focusin_events.html");

  web_view->MainFrameWidget()->SetFocus(true);
  web_view->MainFrameWidget()->SetFocus(false);
  web_view->MainFrameWidget()->SetFocus(true);

  WebElement element =
      web_view->MainFrameImpl()->GetDocument().GetElementById("message");
  EXPECT_EQ("focusoutfocusin", element.TextContent());
}

TEST_F(WebViewTest, DispatchesDomFocusOutDomFocusInOnViewToggleFocus) {
  RegisterMockedHttpURLLoad("domfocusout_domfocusin_events.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "domfocusout_domfocusin_events.html");

  web_view->MainFrameWidget()->SetFocus(true);
  web_view->MainFrameWidget()->SetFocus(false);
  web_view->MainFrameWidget()->SetFocus(true);

  WebElement element =
      web_view->MainFrameImpl()->GetDocument().GetElementById("message");
  EXPECT_EQ("DOMFocusOutDOMFocusIn", element.TextContent());
}

static void OpenDateTimeChooser(WebView* web_view,
                                HTMLInputElement* input_element) {
  input_element->Focus();

  WebKeyboardEvent key_event(WebInputEvent::Type::kRawKeyDown,
                             WebInputEvent::kNoModifiers,
                             WebInputEvent::GetStaticTimeStampForTests());
  key_event.dom_key = ui::DomKey::FromCharacter(' ');
  key_event.windows_key_code = VKEY_SPACE;
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event, ui::LatencyInfo()));

  key_event.SetType(WebInputEvent::Type::kKeyUp);
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event, ui::LatencyInfo()));
}

TEST_F(WebViewTest, ChooseValueFromDateTimeChooser) {
  ScopedInputMultipleFieldsUIForTest input_multiple_fields_ui(false);
  std::string url = RegisterMockedHttpURLLoad("date_time_chooser.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad(url, nullptr, nullptr);

  Document* document =
      web_view_impl->MainFrameImpl()->GetFrame()->GetDocument();

  auto* input_element =
      To<HTMLInputElement>(document->getElementById(AtomicString("date")));
  OpenDateTimeChooser(web_view_impl, input_element);
  GetExternalDateTimeChooser(web_view_impl)->ResponseHandler(true, 0);
  EXPECT_EQ("1970-01-01", input_element->Value());

  OpenDateTimeChooser(web_view_impl, input_element);
  GetExternalDateTimeChooser(web_view_impl)
      ->ResponseHandler(true, std::numeric_limits<double>::quiet_NaN());
  EXPECT_EQ("", input_element->Value());

  input_element = To<HTMLInputElement>(
      document->getElementById(AtomicString("datetimelocal")));
  OpenDateTimeChooser(web_view_impl, input_element);
  GetExternalDateTimeChooser(web_view_impl)->ResponseHandler(true, 0);
  EXPECT_EQ("1970-01-01T00:00", input_element->Value());

  OpenDateTimeChooser(web_view_impl, input_element);
  GetExternalDateTimeChooser(web_view_impl)
      ->ResponseHandler(true, std::numeric_limits<double>::quiet_NaN());
  EXPECT_EQ("", input_element->Value());

  input_element =
      To<HTMLInputElement>(document->getElementById(AtomicString("month")));
  OpenDateTimeChooser(web_view_impl, input_element);
  GetExternalDateTimeChooser(web_view_impl)->ResponseHandler(true, 0);
  EXPECT_EQ("1970-01", input_element->Value());

  OpenDateTimeChooser(web_view_impl, input_element);
  GetExternalDateTimeChooser(web_view_impl)
      ->ResponseHandler(true, std::numeric_limits<double>::quiet_NaN());
  EXPECT_EQ("", input_element->Value());

  input_element =
      To<HTMLInputElement>(document->getElementById(AtomicString("time")));
  OpenDateTimeChooser(web_view_impl, input_element);
  GetExternalDateTimeChooser(web_view_impl)->ResponseHandler(true, 0);
  EXPECT_EQ("00:00", input_element->Value());

  OpenDateTimeChooser(web_view_impl, input_element);
  GetExternalDateTimeChooser(web_view_impl)
      ->ResponseHandler(true, std::numeric_limits<double>::quiet_NaN());
  EXPECT_EQ("", input_element->Value());

  input_element =
      To<HTMLInputElement>(document->getElementById(AtomicString("week")));
  OpenDateTimeChooser(web_view_impl, input_element);
  GetExternalDateTimeChooser(web_view_impl)->ResponseHandler(true, 0);
  EXPECT_EQ("1970-W01", input_element->Value());

  OpenDateTimeChooser(web_view_impl, input_element);
  GetExternalDateTimeChooser(web_view_impl)
      ->ResponseHandler(true, std::numeric_limits<double>::quiet_NaN());
  EXPECT_EQ("", input_element->Value());

  // Clear the WebViewClient from the webViewHelper to avoid use-after-free in
  // the WebViewHelper destructor.
  web_view_helper_.Reset();
}

TEST_F(WebViewTest, DispatchesFocusBlurOnViewToggle) {
  RegisterMockedHttpURLLoad("focus_blur_events.html");
  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + "focus_blur_events.html");

  web_view->MainFrameWidget()->SetFocus(true);
  web_view->MainFrameWidget()->SetFocus(false);
  web_view->MainFrameWidget()->SetFocus(true);

  WebElement element =
      web_view->MainFrameImpl()->GetDocument().GetElementById("message");
  // Expect not to see duplication of events.
  EXPECT_EQ("blurfocus", element.TextContent());
}

class CreateChildCounterFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  CreateChildCounterFrameClient() : count_(0) {}
  WebLocalFrame* CreateChildFrame(
      mojom::blink::TreeScopeType,
      const WebString& name,
      const WebString& fallback_name,
      const FramePolicy&,
      const WebFrameOwnerProperties&,
      FrameOwnerElementType,
      WebPolicyContainerBindParams policy_container_bind_params,
      ukm::SourceId document_ukm_source_id,
      base::FunctionRef<void(
          WebLocalFrame*,
          const DocumentToken&,
          CrossVariantMojoRemote<mojom::BrowserInterfaceBrokerInterfaceBase>)>
          complete_initialization) override;

  int Count() const { return count_; }

 private:
  int count_;
};

WebLocalFrame* CreateChildCounterFrameClient::CreateChildFrame(
    mojom::blink::TreeScopeType scope,
    const WebString& name,
    const WebString& fallback_name,
    const FramePolicy& frame_policy,
    const WebFrameOwnerProperties& frame_owner_properties,
    FrameOwnerElementType frame_owner_element_type,
    WebPolicyContainerBindParams policy_container_bind_params,
    ukm::SourceId document_ukm_source_id,
    base::FunctionRef<void(
        WebLocalFrame*,
        const DocumentToken&,
        CrossVariantMojoRemote<mojom::BrowserInterfaceBrokerInterfaceBase>)>
        complete_initialization) {
  ++count_;
  return TestWebFrameClient::CreateChildFrame(
      scope, name, fallback_name, frame_policy, frame_owner_properties,
      frame_owner_element_type, std::move(policy_container_bind_params),
      document_ukm_source_id, complete_initialization);
}

TEST_F(WebViewTest, ChangeDisplayMode) {
  RegisterMockedHttpURLLoad("display_mode.html");
  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + "display_mode.html");

  String content = TestWebFrameContentDumper::DumpWebViewAsText(web_view, 21);
  EXPECT_EQ("regular-ui", content);

  web_view->MainFrameImpl()->LocalRootFrameWidget()->SetDisplayMode(
      mojom::blink::DisplayMode::kMinimalUi);
  content = TestWebFrameContentDumper::DumpWebViewAsText(web_view, 21);
  EXPECT_EQ("minimal-ui", content);
  web_view_helper_.Reset();
}

TEST_F(WebViewTest, ChangeDisplayModeChildFrame) {
  RegisterMockedHttpURLLoad("iframe-display_mode.html");
  RegisterMockedHttpURLLoad("display_mode.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "iframe-display_mode.html");

  String content = TestWebFrameContentDumper::DumpWebViewAsText(web_view, 21);
  // An iframe inserts whitespace into the content.
  EXPECT_EQ("regular-ui", content.StripWhiteSpace());

  web_view->MainFrameImpl()->LocalRootFrameWidget()->SetDisplayMode(
      mojom::blink::DisplayMode::kMinimalUi);
  content = TestWebFrameContentDumper::DumpWebViewAsText(web_view, 21);
  // An iframe inserts whitespace into the content.
  EXPECT_EQ("minimal-ui", content.StripWhiteSpace());
  web_view_helper_.Reset();
}

TEST_F(WebViewTest, ChangeDisplayModeAlertsListener) {
  RegisterMockedHttpURLLoad("display_mode_listener.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "display_mode_listener.html");

  String content = TestWebFrameContentDumper::DumpWebViewAsText(web_view, 21);
  EXPECT_EQ("regular-ui", content);

  web_view->MainFrameImpl()->LocalRootFrameWidget()->SetDisplayMode(
      mojom::blink::DisplayMode::kMinimalUi);
  content = TestWebFrameContentDumper::DumpWebViewAsText(web_view, 21);
  EXPECT_EQ("minimal-ui", content);
  web_view_helper_.Reset();
}

TEST_F(WebViewTest, ChangeDisplayModeChildFrameAlertsListener) {
  RegisterMockedHttpURLLoad("iframe-display_mode_listener.html");
  RegisterMockedHttpURLLoad("display_mode_listener.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "iframe-display_mode_listener.html");

  String content = TestWebFrameContentDumper::DumpWebViewAsText(web_view, 21);
  // An iframe inserts whitespace into the content.
  EXPECT_EQ("regular-ui", content.StripWhiteSpace());

  web_view->MainFrameImpl()->LocalRootFrameWidget()->SetDisplayMode(
      mojom::blink::DisplayMode::kMinimalUi);
  content = TestWebFrameContentDumper::DumpWebViewAsText(web_view, 21);
  // An iframe inserts whitespace into the content.
  EXPECT_EQ("minimal-ui", content.StripWhiteSpace());
  web_view_helper_.Reset();
}

TEST_F(WebViewTest, AddFrameInCloseUnload) {
  CreateChildCounterFrameClient frame_client;
  RegisterMockedHttpURLLoad("add_frame_in_unload.html");
  web_view_helper_.InitializeAndLoad(base_url_ + "add_frame_in_unload.html",
                                     &frame_client);
  web_view_helper_.Reset();
  EXPECT_EQ(0, frame_client.Count());
}

TEST_F(WebViewTest, AddFrameInCloseURLUnload) {
  CreateChildCounterFrameClient frame_client;
  RegisterMockedHttpURLLoad("add_frame_in_unload.html");
  web_view_helper_.InitializeAndLoad(base_url_ + "add_frame_in_unload.html",
                                     &frame_client);
  // Dispatch unload event.
  web_view_helper_.LocalMainFrame()->GetFrame()->ClosePageForTesting();
  EXPECT_EQ(0, frame_client.Count());
  web_view_helper_.Reset();
}

TEST_F(WebViewTest, AddFrameInNavigateUnload) {
  CreateChildCounterFrameClient frame_client;
  RegisterMockedHttpURLLoad("add_frame_in_unload.html");
  web_view_helper_.InitializeAndLoad(base_url_ + "add_frame_in_unload.html",
                                     &frame_client);
  frame_test_helpers::LoadFrame(web_view_helper_.GetWebView()->MainFrameImpl(),
                                "about:blank");
  EXPECT_EQ(0, frame_client.Count());
  web_view_helper_.Reset();
}

TEST_F(WebViewTest, AddFrameInChildInNavigateUnload) {
  CreateChildCounterFrameClient frame_client;
  RegisterMockedHttpURLLoad("add_frame_in_unload_wrapper.html");
  RegisterMockedHttpURLLoad("add_frame_in_unload.html");
  web_view_helper_.InitializeAndLoad(
      base_url_ + "add_frame_in_unload_wrapper.html", &frame_client);
  frame_test_helpers::LoadFrame(web_view_helper_.GetWebView()->MainFrameImpl(),
                                "about:blank");
  EXPECT_EQ(1, frame_client.Count());
  web_view_helper_.Reset();
}

class TouchEventConsumersWebFrameWidgetHost
    : public frame_test_helpers::TestWebFrameWidgetHost {
 public:
  int GetAndResetHasTouchEventHandlerCallCount(bool state) {
    int value = has_touch_event_handler_count_[state];
    has_touch_event_handler_count_[state] = 0;
    return value;
  }

  // mojom::FrameWidgetHost overrides:
  void SetHasTouchEventConsumers(
      mojom::blink::TouchEventConsumersPtr consumers) override {
    // Only count the times the state changes.
    bool state = consumers->has_touch_event_handlers;
    if (state != has_touch_event_handler_)
      has_touch_event_handler_count_[state]++;
    has_touch_event_handler_ = state;
  }

 private:
  int has_touch_event_handler_count_[2]{};
  bool has_touch_event_handler_ = false;
};

class TouchEventConsumersWebFrameWidget
    : public frame_test_helpers::TestWebFrameWidget {
 public:
  template <typename... Args>
  explicit TouchEventConsumersWebFrameWidget(Args&&... args)
      : frame_test_helpers::TestWebFrameWidget(std::forward<Args>(args)...) {}

  // frame_test_helpers::TestWebFrameWidget overrides.
  std::unique_ptr<frame_test_helpers::TestWebFrameWidgetHost> CreateWidgetHost()
      override {
    return std::make_unique<TouchEventConsumersWebFrameWidgetHost>();
  }

  TouchEventConsumersWebFrameWidgetHost& TouchEventWidgetHost() {
    return static_cast<TouchEventConsumersWebFrameWidgetHost&>(WidgetHost());
  }
};

class TouchEventConsumersWebViewTest : public WebViewTest {
 public:
  TouchEventConsumersWebViewTest()
      : WebViewTest(WTF::BindRepeating(
            &frame_test_helpers::WebViewHelper::CreateTestWebFrameWidget<
                TouchEventConsumersWebFrameWidget>)) {}
};

// This test verifies that FrameWidgetHost::SetHasTouchEventConsumers is called
// accordingly for various calls to EventHandlerRegistry::did{Add|Remove|
// RemoveAll}EventHandler(..., TouchEvent). Verifying that those calls are made
// correctly is the job of web_tests/fast/events/event-handler-count.html.
TEST_F(TouchEventConsumersWebViewTest, SetHasTouchEventConsumers) {
  std::string url = RegisterMockedHttpURLLoad("has_touch_event_handlers.html");
  WebViewImpl* web_view_impl = web_view_helper_.InitializeAndLoad(url);

  TouchEventConsumersWebFrameWidget* widget =
      static_cast<TouchEventConsumersWebFrameWidget*>(
          web_view_helper_.GetMainFrameWidget());
  TouchEventConsumersWebFrameWidgetHost& frame_widget_host =
      widget->TouchEventWidgetHost();

  const EventHandlerRegistry::EventHandlerClass kTouchEvent =
      EventHandlerRegistry::kTouchStartOrMoveEventBlocking;

  // The page is initialized with at least one no-handlers call.
  // In practice we get two such calls because WebViewHelper::initializeAndLoad
  // first initializes an empty frame, and then loads a document into it, so
  // there are two FrameLoader::commitProvisionalLoad calls.
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(true));

  // Adding the first document handler results in a has-handlers call.
  Document* document =
      web_view_impl->MainFrameImpl()->GetFrame()->GetDocument();
  EventHandlerRegistry* registry =
      &document->GetFrame()->GetEventHandlerRegistry();
  registry->DidAddEventHandler(*document, kTouchEvent);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(1,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(true));

  // Adding another handler has no effect.
  registry->DidAddEventHandler(*document, kTouchEvent);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(true));

  // Removing the duplicate handler has no effect.
  registry->DidRemoveEventHandler(*document, kTouchEvent);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(true));

  // Removing the final handler results in a no-handlers call.
  registry->DidRemoveEventHandler(*document, kTouchEvent);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(true));

  // Adding a handler on a div results in a has-handlers call.
  Element* parent_div = document->getElementById(AtomicString("parentdiv"));
  DCHECK(parent_div);
  registry->DidAddEventHandler(*parent_div, kTouchEvent);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(1,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(true));

  // Adding a duplicate handler on the div, clearing all document handlers
  // (of which there are none) and removing the extra handler on the div
  // all have no effect.
  registry->DidAddEventHandler(*parent_div, kTouchEvent);
  registry->DidRemoveAllEventHandlers(*document);
  registry->DidRemoveEventHandler(*parent_div, kTouchEvent);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(true));

  // Removing the final handler on the div results in a no-handlers call.
  registry->DidRemoveEventHandler(*parent_div, kTouchEvent);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(true));

  // Adding two handlers then clearing them in a single call results in a
  // has-handlers then no-handlers call.
  registry->DidAddEventHandler(*parent_div, kTouchEvent);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(1,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(true));
  registry->DidAddEventHandler(*parent_div, kTouchEvent);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(true));
  registry->DidRemoveAllEventHandlers(*parent_div);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(true));

  // Adding a handler inside of a child iframe results in a has-handlers call.
  Element* child_frame = document->getElementById(AtomicString("childframe"));
  DCHECK(child_frame);
  Document* child_document =
      To<HTMLIFrameElement>(child_frame)->contentDocument();
  Element* child_div = child_document->getElementById(AtomicString("childdiv"));
  DCHECK(child_div);
  registry->DidAddEventHandler(*child_div, kTouchEvent);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(1,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(true));

  // Adding and clearing handlers in the parent doc or elsewhere in the child
  // doc has no impact.
  registry->DidAddEventHandler(*document, kTouchEvent);
  registry->DidAddEventHandler(*child_frame, kTouchEvent);
  registry->DidAddEventHandler(*child_document, kTouchEvent);
  registry->DidRemoveAllEventHandlers(*document);
  registry->DidRemoveAllEventHandlers(*child_frame);
  registry->DidRemoveAllEventHandlers(*child_document);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(true));

  // Removing the final handler inside the child frame results in a no-handlers
  // call.
  registry->DidRemoveAllEventHandlers(*child_div);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(true));

  // Adding a handler inside the child frame results in a has-handlers call.
  registry->DidAddEventHandler(*child_document, kTouchEvent);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(1,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(true));

  // Adding a handler in the parent document and removing the one in the frame
  // has no effect.
  registry->DidAddEventHandler(*child_frame, kTouchEvent);
  registry->DidRemoveEventHandler(*child_document, kTouchEvent);
  registry->DidRemoveAllEventHandlers(*child_document);
  registry->DidRemoveAllEventHandlers(*document);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(true));

  // Now removing the handler in the parent document results in a no-handlers
  // call.
  registry->DidRemoveEventHandler(*child_frame, kTouchEvent);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0,
            frame_widget_host.GetAndResetHasTouchEventHandlerCallCount(true));
}

// This test checks that deleting nodes which have only non-JS-registered touch
// handlers also removes them from the event handler registry. Note that this
// is different from detaching and re-attaching the same node, which is covered
// by web tests under fast/events/.
TEST_F(WebViewTest, DeleteElementWithRegisteredHandler) {
  std::string url = RegisterMockedHttpURLLoad("simple_div.html");
  WebViewImpl* web_view_impl = web_view_helper_.InitializeAndLoad(url);

  Persistent<Document> document =
      web_view_impl->MainFrameImpl()->GetFrame()->GetDocument();
  Element* div = document->getElementById(AtomicString("div"));
  EventHandlerRegistry& registry =
      document->GetFrame()->GetEventHandlerRegistry();

  registry.DidAddEventHandler(*div, EventHandlerRegistry::kScrollEvent);
  EXPECT_TRUE(registry.HasEventHandlers(EventHandlerRegistry::kScrollEvent));

  DummyExceptionStateForTesting exception_state;
  div->remove(exception_state);

  // For oilpan we have to force a GC to ensure the event handlers have been
  // removed when checking below. We do a precise GC (collectAllGarbage does not
  // scan the stack) to ensure the div element dies. This is also why the
  // Document is in a Persistent since we want that to stay around.
  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_FALSE(registry.HasEventHandlers(EventHandlerRegistry::kScrollEvent));
}

// This test verifies the text input flags are correctly exposed to script.
TEST_F(WebViewTest, TextInputFlags) {
  std::string url = RegisterMockedHttpURLLoad("text_input_flags.html");
  WebViewImpl* web_view_impl = web_view_helper_.InitializeAndLoad(url);
  web_view_impl->MainFrameImpl()->GetFrame()->SetInitialFocus(false);

  WebLocalFrameImpl* frame = web_view_impl->MainFrameImpl();
  WebInputMethodController* active_input_method_controller =
      frame->GetInputMethodController();
  Document* document = frame->GetFrame()->GetDocument();

  // (A) <input>
  // (A.1) Verifies autocorrect/autocomplete/spellcheck flags are Off and
  // autocapitalize is set to none.
  auto* input_element =
      To<HTMLInputElement>(document->getElementById(AtomicString("input")));
  document->SetFocusedElement(
      input_element, FocusParams(SelectionBehaviorOnFocus::kNone,
                                 mojom::blink::FocusType::kNone, nullptr));
  web_view_impl->MainFrameWidget()->SetFocus(true);
  WebTextInputInfo info1 = active_input_method_controller->TextInputInfo();
  EXPECT_EQ(kWebTextInputFlagAutocompleteOff | kWebTextInputFlagAutocorrectOff |
                kWebTextInputFlagSpellcheckOff |
                kWebTextInputFlagAutocapitalizeNone,
            info1.flags);

  // (A.2) Verifies autocorrect/autocomplete/spellcheck flags are On and
  // autocapitalize is set to sentences.
  input_element =
      To<HTMLInputElement>(document->getElementById(AtomicString("input2")));
  document->SetFocusedElement(
      input_element, FocusParams(SelectionBehaviorOnFocus::kNone,
                                 mojom::blink::FocusType::kNone, nullptr));
  web_view_impl->MainFrameWidget()->SetFocus(true);
  WebTextInputInfo info2 = active_input_method_controller->TextInputInfo();
  EXPECT_EQ(kWebTextInputFlagAutocompleteOn | kWebTextInputFlagAutocorrectOn |
                kWebTextInputFlagSpellcheckOn |
                kWebTextInputFlagAutocapitalizeSentences,
            info2.flags);

  // (B) <textarea> Verifies the default text input flags are
  // WebTextInputFlagAutocapitalizeSentences.
  auto* text_area_element = To<HTMLTextAreaElement>(
      document->getElementById(AtomicString("textarea")));
  document->SetFocusedElement(
      text_area_element, FocusParams(SelectionBehaviorOnFocus::kNone,
                                     mojom::blink::FocusType::kNone, nullptr));
  web_view_impl->MainFrameWidget()->SetFocus(true);
  WebTextInputInfo info3 = active_input_method_controller->TextInputInfo();
  EXPECT_EQ(kWebTextInputFlagAutocapitalizeSentences, info3.flags);

  // (C) Verifies the WebTextInputInfo's don't equal.
  EXPECT_FALSE(info1.Equals(info2));
  EXPECT_FALSE(info2.Equals(info3));

  // Free the webView before freeing the NonUserInputTextUpdateWebViewClient.
  web_view_helper_.Reset();
}

// Check that the WebAutofillClient is correctly notified about first user
// gestures after load, following various input events.
TEST_F(WebViewTest, FirstUserGestureObservedKeyEvent) {
  RegisterMockedHttpURLLoad("form.html");
  MockAutofillClient client;
  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + "form.html");
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  frame->SetAutofillClient(&client);
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);

  EXPECT_EQ(0, client.GetUserGestureNotificationsCount());

  WebKeyboardEvent key_event(WebInputEvent::Type::kRawKeyDown,
                             WebInputEvent::kNoModifiers,
                             WebInputEvent::GetStaticTimeStampForTests());
  key_event.dom_key = ui::DomKey::FromCharacter(' ');
  key_event.windows_key_code = VKEY_SPACE;
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event, ui::LatencyInfo()));
  key_event.SetType(WebInputEvent::Type::kKeyUp);
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event, ui::LatencyInfo()));

  EXPECT_EQ(1, client.GetUserGestureNotificationsCount());
  frame->SetAutofillClient(nullptr);
}

TEST_F(WebViewTest, FirstUserGestureObservedMouseEvent) {
  RegisterMockedHttpURLLoad("form.html");
  MockAutofillClient client;
  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + "form.html");
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  frame->SetAutofillClient(&client);
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);

  EXPECT_EQ(0, client.GetUserGestureNotificationsCount());

  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(1, 1);
  mouse_event.click_count = 1;
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_event, ui::LatencyInfo()));
  mouse_event.SetType(WebInputEvent::Type::kMouseUp);
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_event, ui::LatencyInfo()));

  EXPECT_EQ(1, client.GetUserGestureNotificationsCount());
  frame->SetAutofillClient(nullptr);
}

TEST_F(WebViewTest, CompositionIsUserGesture) {
  RegisterMockedHttpURLLoad("input_field_populated.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "input_field_populated.html");
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  MockAutofillClient client;
  frame->SetAutofillClient(&client);
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);

  EXPECT_EQ(0, client.TextChanges());
  EXPECT_TRUE(
      frame->FrameWidget()->GetActiveWebInputMethodController()->SetComposition(
          WebString::FromUTF8("hello"), WebVector<ui::ImeTextSpan>(),
          WebRange(), 3, 3));
  EXPECT_TRUE(frame->HasTransientUserActivation());
  EXPECT_EQ(1, client.TextChanges());
  EXPECT_TRUE(frame->HasMarkedText());

  frame->SetAutofillClient(nullptr);
}

// Currently, SelectionAsText() is built upon TextIterator, but
// TestWebFrameContentDumper is built upon TextDumperForTests. Their results can
// be different, making the test fail.
// TODO(crbug.com/781434): Build a selection serializer upon TextDumperForTests.
TEST_F(WebViewTest, DISABLED_CompareSelectAllToContentAsText) {
  RegisterMockedHttpURLLoad("longpress_selection.html");
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "longpress_selection.html");

  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  frame->ExecuteScript(WebScriptSource(
      WebString::FromUTF8("document.execCommand('SelectAll', false, null)")));
  std::string actual = frame->SelectionAsText().Utf8();

  const int kMaxOutputCharacters = 1024;
  std::string expected = TestWebFrameContentDumper::DumpWebViewAsText(
                             web_view, kMaxOutputCharacters)
                             .Utf8();
  EXPECT_EQ(expected, actual);
}

TEST_F(WebViewTest, AutoResizeSubtreeLayout) {
  std::string url = RegisterMockedHttpURLLoad("subtree-layout.html");
  WebViewImpl* web_view = web_view_helper_.Initialize();

  web_view->EnableAutoResizeMode(gfx::Size(200, 200), gfx::Size(200, 200));
  LoadFrame(web_view->MainFrameImpl(), url);

  LocalFrameView* frame_view =
      web_view_helper_.LocalMainFrame()->GetFrameView();

  // Auto-resizing used to DCHECK(needsLayout()) in LayoutBlockFlow::layout.
  // This EXPECT is merely a dummy. The real test is that we don't trigger
  // asserts in debug builds.
  EXPECT_FALSE(frame_view->NeedsLayout());
}

TEST_F(WebViewTest, PreferredSize) {
  std::string url = base_url_ + "specify_size.html?100px:100px";
  url_test_helpers::RegisterMockedURLLoad(
      ToKURL(url), test::CoreTestDataPath("specify_size.html"));
  WebView* web_view = web_view_helper_.InitializeAndLoad(url);

  gfx::Size size = web_view->ContentsPreferredMinimumSize();
  EXPECT_EQ(100, size.width());
  EXPECT_EQ(100, size.height());

  web_view->MainFrameWidget()->SetZoomLevel(ZoomFactorToZoomLevel(2.0));
  UpdateAllLifecyclePhases();
  size = web_view->ContentsPreferredMinimumSize();
  EXPECT_EQ(200, size.width());
  EXPECT_EQ(200, size.height());

  // Verify that both width and height are rounded (in this case up)
  web_view->MainFrameWidget()->SetZoomLevel(ZoomFactorToZoomLevel(0.9995));
  UpdateAllLifecyclePhases();
  size = web_view->ContentsPreferredMinimumSize();
  EXPECT_EQ(100, size.width());
  EXPECT_EQ(100, size.height());

  // Verify that both width and height are rounded (in this case down)
  web_view->MainFrameWidget()->SetZoomLevel(ZoomFactorToZoomLevel(1.0005));
  UpdateAllLifecyclePhases();
  size = web_view->ContentsPreferredMinimumSize();
  EXPECT_EQ(100, size.width());
  EXPECT_EQ(100, size.height());

  url = base_url_ + "specify_size.html?1.5px:1.5px";
  url_test_helpers::RegisterMockedURLLoad(
      ToKURL(url), test::CoreTestDataPath("specify_size.html"));
  web_view = web_view_helper_.InitializeAndLoad(url);

  web_view->MainFrameWidget()->SetZoomLevel(ZoomFactorToZoomLevel(1));
  UpdateAllLifecyclePhases();
  size = web_view->ContentsPreferredMinimumSize();
  EXPECT_EQ(2, size.width());
  EXPECT_EQ(2, size.height());
}

TEST_F(WebViewTest, PreferredMinimumSizeQuirksMode) {
  WebViewImpl* web_view = web_view_helper_.Initialize();
  web_view->MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  frame_test_helpers::LoadHTMLString(
      web_view->MainFrameImpl(),
      R"HTML(<html>
        <body style="margin: 0px;">
          <div style="width: 99px; height: 100px; display: inline-block;"></div>
        </body>
      </html>)HTML",
      url_test_helpers::ToKURL("http://example.com/"));

  gfx::Size size = web_view->ContentsPreferredMinimumSize();
  EXPECT_EQ(99, size.width());
  // When in quirks mode the preferred height stretches to fill the viewport.
  EXPECT_EQ(600, size.height());
}

TEST_F(WebViewTest, PreferredSizeWithGrid) {
  WebViewImpl* web_view = web_view_helper_.Initialize();
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(),
                                     R"HTML(<!DOCTYPE html>
    <style>
      html { writing-mode: vertical-rl; }
      body { margin: 0px; }
    </style>
    <div style="width: 100px;">
      <div style="display: grid; width: 100%;">
        <div style="writing-mode: horizontal-tb; height: 100px;"></div>
      </div>
    </div>
                                   )HTML",
                                     base_url);

  gfx::Size size = web_view->ContentsPreferredMinimumSize();
  EXPECT_EQ(0, size.width());
  EXPECT_EQ(100, size.height());
}

TEST_F(WebViewTest, PreferredSizeWithNGGridSkipped) {
  WebViewImpl* web_view = web_view_helper_.Initialize();
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(),
                                     R"HTML(<!DOCTYPE html>
    <style>
      body { margin: 0px; }
    </style>
    <div style="display: inline-grid;
                padding: 1%;
                border: 5px solid black;
                grid-template-rows: 1fr 2fr">
      <svg id="target" viewBox="0 0 1 1" style="background: green;
                                                height: 100%;" >
        <circle id="c1" cx="50" cy="50" r="50"/>
      </svg>
    </div>
                                   )HTML",
                                     base_url);

  gfx::Size size = web_view->ContentsPreferredMinimumSize();
  EXPECT_EQ(10, size.width());
  EXPECT_EQ(10, size.height());
}

TEST_F(WebViewTest, PreferredSizeWithGridMinWidth) {
  WebViewImpl* web_view = web_view_helper_.Initialize();
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(),
                                     R"HTML(<!DOCTYPE html>
    <body style="margin: 0px;">
      <div style="display: inline-grid; min-width: 200px;">
        <div>item</div>
      </div>
    </body>
                                   )HTML",
                                     base_url);

  gfx::Size size = web_view->ContentsPreferredMinimumSize();
  EXPECT_EQ(200, size.width());
}

TEST_F(WebViewTest, PreferredSizeWithGridMinWidthFlexibleTracks) {
  WebViewImpl* web_view = web_view_helper_.Initialize();
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(),
                                     R"HTML(<!DOCTYPE html>
    <body style="margin: 0px;">
      <div style="display: inline-grid; min-width: 200px; grid-template-columns: 1fr;">
        <div>item</div>
      </div>
    </body>
                                   )HTML",
                                     base_url);

  gfx::Size size = web_view->ContentsPreferredMinimumSize();
  EXPECT_EQ(200, size.width());
}

#if BUILDFLAG(ENABLE_UNHANDLED_TAP)

// Helps set up any test that uses a mock Mojo implementation.
class MojoTestHelper {
 public:
  MojoTestHelper(const String& test_file,
                 frame_test_helpers::WebViewHelper& web_view_helper)
      : web_view_helper_(web_view_helper) {
    web_view_ =
        web_view_helper.InitializeAndLoad(test_file.Utf8(), &web_frame_client_);
  }

  ~MojoTestHelper() {
    web_view_helper_.Reset();  // Remove dependency on locally scoped client.
  }

  WebViewImpl* WebView() const { return web_view_; }

 private:
  WebViewImpl* web_view_;
  frame_test_helpers::WebViewHelper& web_view_helper_;
  frame_test_helpers::TestWebFrameClient web_frame_client_;
};

// Mock implementation of the UnhandledTapNotifier Mojo receiver, for testing
// the ShowUnhandledTapUIIfNeeded notification.
class MockUnhandledTapNotifierImpl : public mojom::blink::UnhandledTapNotifier {
 public:
  MockUnhandledTapNotifierImpl() = default;
  ~MockUnhandledTapNotifierImpl() override = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(mojo::PendingReceiver<mojom::blink::UnhandledTapNotifier>(
        std::move(handle)));
  }

  void ShowUnhandledTapUIIfNeeded(
      mojom::blink::UnhandledTapInfoPtr unhandled_tap_info) override {
    was_unhandled_tap_ = true;
    tapped_position_ = unhandled_tap_info->tapped_position_in_viewport;
  }
  bool WasUnhandledTap() const { return was_unhandled_tap_; }
  int GetTappedXPos() const { return tapped_position_.x(); }
  int GetTappedYPos() const { return tapped_position_.y(); }
  void Reset() {
    was_unhandled_tap_ = false;
    tapped_position_ = gfx::Point();
    receiver_.reset();
  }

 private:
  bool was_unhandled_tap_ = false;
  gfx::Point tapped_position_;

  mojo::Receiver<mojom::blink::UnhandledTapNotifier> receiver_{this};
};

// A Test Fixture for testing ShowUnhandledTapUIIfNeeded usages.
class ShowUnhandledTapTest : public WebViewTest {
 public:
  void SetUp() override {
    WebViewTest::SetUp();
    std::string test_file = "show_unhandled_tap.html";
    RegisterMockedHttpURLLoad("Ahem.ttf");
    RegisterMockedHttpURLLoad(test_file);

    mojo_test_helper_ = std::make_unique<MojoTestHelper>(
        WebString::FromUTF8(base_url_ + test_file), web_view_helper_);

    web_view_ = mojo_test_helper_->WebView();
    web_view_->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
    web_view_->MainFrameWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
    RunPendingTasks();

    WebLocalFrameImpl* web_local_frame = web_view_->MainFrameImpl();
    web_local_frame->GetFrame()
        ->GetBrowserInterfaceBroker()
        .SetBinderForTesting(
            mojom::blink::UnhandledTapNotifier::Name_,
            WTF::BindRepeating(&MockUnhandledTapNotifierImpl::Bind,
                               WTF::Unretained(&mock_notifier_)));
  }

  void TearDown() override {
    WebLocalFrameImpl* web_local_frame = web_view_->MainFrameImpl();
    web_local_frame->GetFrame()
        ->GetBrowserInterfaceBroker()
        .SetBinderForTesting(mojom::blink::UnhandledTapNotifier::Name_, {});

    WebViewTest::TearDown();
  }

 protected:
  // Tap on the given element by ID.
  void Tap(const String& element_id) {
    mock_notifier_.Reset();
    EXPECT_TRUE(SimulateGestureAtElementById(WebInputEvent::Type::kGestureTap,
                                             element_id));
  }

  // Set up a test script for the given |operation| with the given |handler|.
  void SetTestScript(const String& operation, const String& handler) {
    String test_key = operation + "-" + handler;
    web_view_->MainFrameImpl()->ExecuteScript(
        WebScriptSource(String("setTest('" + test_key + "');")));
  }

  // Test each mouse event combination with the given |handler|, and verify the
  // |expected| outcome.
  void TestEachMouseEvent(const String& handler, bool expected) {
    SetTestScript("mousedown", handler);
    Tap("target");
    EXPECT_EQ(expected, mock_notifier_.WasUnhandledTap());

    SetTestScript("mouseup", handler);
    Tap("target");
    EXPECT_EQ(expected, mock_notifier_.WasUnhandledTap());

    SetTestScript("click", handler);
    Tap("target");
    EXPECT_EQ(expected, mock_notifier_.WasUnhandledTap());
  }

  WebViewImpl* web_view_;
  MockUnhandledTapNotifierImpl mock_notifier_;

 private:
  std::unique_ptr<MojoTestHelper> mojo_test_helper_;
};

TEST_F(ShowUnhandledTapTest, ShowUnhandledTapUIIfNeeded) {
  // Scroll the bottom into view so we can distinguish window coordinates from
  // document coordinates.
  Tap("bottom");
  EXPECT_TRUE(mock_notifier_.WasUnhandledTap());
  EXPECT_EQ(64, mock_notifier_.GetTappedXPos());
  EXPECT_EQ(278, mock_notifier_.GetTappedYPos());

  // Test basic tap handling and notification.
  Tap("target");
  EXPECT_TRUE(mock_notifier_.WasUnhandledTap());
  EXPECT_EQ(144, mock_notifier_.GetTappedXPos());
  EXPECT_EQ(82, mock_notifier_.GetTappedYPos());

  // Test correct conversion of coordinates to viewport space under pinch-zoom.
  constexpr float scale = 1.5f;
  constexpr float visual_x = 6.f;
  constexpr float visual_y = 10.f;

  web_view_->SetPageScaleFactor(scale);
  web_view_->SetVisualViewportOffset(gfx::PointF(visual_x, visual_y));

  Tap("target");

  // Ensure position didn't change as a result of scroll into view.
  ASSERT_EQ(visual_x, web_view_->VisualViewportOffset().x());
  ASSERT_EQ(visual_y, web_view_->VisualViewportOffset().y());

  EXPECT_TRUE(mock_notifier_.WasUnhandledTap());

  constexpr float expected_x = 144 * scale - (scale * visual_x);
  constexpr float expected_y = 82 * scale - (scale * visual_y);
  EXPECT_EQ(expected_x, mock_notifier_.GetTappedXPos());
  EXPECT_EQ(expected_y, mock_notifier_.GetTappedYPos());
}

TEST_F(ShowUnhandledTapTest, ShowUnhandledTapUIIfNeededWithMutateDom) {
  // Test dom mutation.
  TestEachMouseEvent("mutateDom", false);

  // Test without any DOM mutation.
  TestEachMouseEvent("none", true);
}

TEST_F(ShowUnhandledTapTest, ShowUnhandledTapUIIfNeededWithMutateStyle) {
  // Test style mutation.
  TestEachMouseEvent("mutateStyle", false);

  // Test checkbox:indeterminate style mutation.
  TestEachMouseEvent("mutateIndeterminate", false);

  // Test click div with :active style.
  Tap("style_active");
  EXPECT_FALSE(mock_notifier_.WasUnhandledTap());
}

TEST_F(ShowUnhandledTapTest, ShowUnhandledTapUIIfNeededWithPreventDefault) {
  // Test swallowing.
  TestEachMouseEvent("preventDefault", false);

  // Test without any preventDefault.
  TestEachMouseEvent("none", true);
}

TEST_F(ShowUnhandledTapTest, ShowUnhandledTapUIIfNeededWithNonTriggeringNodes) {
  Tap("image");
  EXPECT_FALSE(mock_notifier_.WasUnhandledTap());

  Tap("editable");
  EXPECT_FALSE(mock_notifier_.WasUnhandledTap());

  Tap("focusable");
  EXPECT_FALSE(mock_notifier_.WasUnhandledTap());
}

#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)

TEST_F(WebViewTest, ShouldSuppressKeyboardForPasswordField) {
  RegisterMockedHttpURLLoad("input_field_password.html");
  // Pretend client has fill data for all fields it's queried.
  MockAutofillClient client;
  client.SetShouldSuppressKeyboard(true);
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "input_field_password.html");
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  frame->SetAutofillClient(&client);
  // No field is focused.
  EXPECT_FALSE(frame->ShouldSuppressKeyboardForFocusedElement());

  // Focusing a field should result in treating it autofillable.
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  EXPECT_TRUE(frame->ShouldSuppressKeyboardForFocusedElement());

  // Pretend that |client| no longer has autofill data available.
  client.SetShouldSuppressKeyboard(false);
  EXPECT_FALSE(frame->ShouldSuppressKeyboardForFocusedElement());
  frame->SetAutofillClient(nullptr);
}

TEST_F(WebViewTest, PasswordFieldEditingIsUserGesture) {
  RegisterMockedHttpURLLoad("input_field_password.html");
  MockAutofillClient client;
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "input_field_password.html");
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  frame->SetAutofillClient(&client);
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);

  WebVector<ui::ImeTextSpan> empty_ime_text_spans;

  EXPECT_EQ(0, client.TextChanges());
  EXPECT_TRUE(
      frame->FrameWidget()->GetActiveWebInputMethodController()->CommitText(
          WebString::FromUTF8("hello"), empty_ime_text_spans, WebRange(), 0));
  EXPECT_TRUE(frame->HasTransientUserActivation());
  EXPECT_EQ(1, client.TextChanges());
  frame->SetAutofillClient(nullptr);
}

// Verify that a WebView created with a ScopedPagePauser already on the
// stack defers its loads.
TEST_F(WebViewTest, CreatedDuringPagePause) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kPausePagesPerBrowsingContextGroup);

  {
    WebViewImpl* web_view = web_view_helper_.Initialize();
    EXPECT_FALSE(web_view->GetPage()->Paused());
  }

  {
    ScopedPagePauser pauser;
    WebViewImpl* web_view = web_view_helper_.Initialize();
    EXPECT_TRUE(web_view->GetPage()->Paused());
  }
}

// Similar to CreatedDuringPagePause, but pauses only pages that belong to the
// same browsing context group.
TEST_F(WebViewTest, CreatedDuringBrowsingContextGroupPause) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPausePagesPerBrowsingContextGroup);

  WebViewImpl* opener_webview = web_view_helper_.Initialize();
  EXPECT_FALSE(opener_webview->GetPage()->Paused());

  auto pauser = std::make_unique<ScopedBrowsingContextGroupPauser>(
      *opener_webview->GetPage());
  EXPECT_TRUE(opener_webview->GetPage()->Paused());

  frame_test_helpers::WebViewHelper web_view_helper2;
  WebViewImpl* webview2 =
      web_view_helper2.InitializeWithOpener(opener_webview->MainFrame());
  EXPECT_TRUE(webview2->GetPage()->Paused());

  // The following page does not belong to the same browsing context group so
  // it should not be paused.
  frame_test_helpers::WebViewHelper web_view_helper3;
  WebViewImpl* webview3 = web_view_helper3.Initialize();
  EXPECT_FALSE(webview3->GetPage()->Paused());

  // Removing the pauser should unpause pages.
  pauser.reset();
  EXPECT_FALSE(opener_webview->GetPage()->Paused());
  EXPECT_FALSE(webview2->GetPage()->Paused());
}

// Make sure the SubframeBeforeUnloadUseCounter is only incremented on subframe
// unloads. crbug.com/635029.
TEST_F(WebViewTest, SubframeBeforeUnloadUseCounter) {
  RegisterMockedHttpURLLoad("visible_iframe.html");
  RegisterMockedHttpURLLoad("single_iframe.html");
  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + "single_iframe.html");

  WebLocalFrame* frame = web_view_helper_.LocalMainFrame();
  Document* document =
      To<LocalFrame>(web_view_helper_.GetWebView()->GetPage()->MainFrame())
          ->GetDocument();

  // Add a beforeunload handler in the main frame. Make sure firing
  // beforeunload doesn't increment the subframe use counter.
  {
    frame->ExecuteScript(
        WebScriptSource("addEventListener('beforeunload', function() {});"));
    web_view->MainFrameImpl()->DispatchBeforeUnloadEvent(false);
    EXPECT_FALSE(
        document->IsUseCounted(WebFeature::kSubFrameBeforeUnloadFired));
  }

  // Add a beforeunload handler in the iframe and dispatch. Make sure we do
  // increment the use counter for subframe beforeunloads.
  {
    frame->ExecuteScript(WebScriptSource(
        "document.getElementsByTagName('iframe')[0].contentWindow."
        "addEventListener('beforeunload', function() {});"));
    To<WebLocalFrameImpl>(
        web_view->MainFrame()->FirstChild()->ToWebLocalFrame())
        ->DispatchBeforeUnloadEvent(false);

    Document* child_document = To<LocalFrame>(web_view_helper_.GetWebView()
                                                  ->GetPage()
                                                  ->MainFrame()
                                                  ->Tree()
                                                  .FirstChild())
                                   ->GetDocument();
    EXPECT_TRUE(
        child_document->IsUseCounted(WebFeature::kSubFrameBeforeUnloadFired));
  }
}

// Verify that page loads are deferred until all ScopedPagePausers are
// destroyed.
TEST_F(WebViewTest, NestedPagePauses) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kPausePagesPerBrowsingContextGroup);

  WebViewImpl* web_view = web_view_helper_.Initialize();
  EXPECT_FALSE(web_view->GetPage()->Paused());

  {
    ScopedPagePauser pauser;
    EXPECT_TRUE(web_view->GetPage()->Paused());

    {
      ScopedPagePauser pauser2;
      EXPECT_TRUE(web_view->GetPage()->Paused());
    }

    EXPECT_TRUE(web_view->GetPage()->Paused());
  }

  EXPECT_FALSE(web_view->GetPage()->Paused());
}

// Similar to NestedPagePauses but uses ScopedBrowsingContextGroupPauser
// instead.
TEST_F(WebViewTest, NestedPagePausesPerBrowsingContextGroup) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPausePagesPerBrowsingContextGroup);

  WebViewImpl* web_view = web_view_helper_.Initialize();
  EXPECT_FALSE(web_view->GetPage()->Paused());

  {
    ScopedBrowsingContextGroupPauser pauser(*web_view->GetPage());
    EXPECT_TRUE(web_view->GetPage()->Paused());

    {
      ScopedBrowsingContextGroupPauser pauser2(*web_view->GetPage());
      EXPECT_TRUE(web_view->GetPage()->Paused());
    }

    EXPECT_TRUE(web_view->GetPage()->Paused());
  }

  EXPECT_FALSE(web_view->GetPage()->Paused());
}

TEST_F(WebViewTest, ClosingPageIsPaused) {
  WebViewImpl* web_view = web_view_helper_.Initialize();
  Page* page = web_view_helper_.GetWebView()->GetPage();
  EXPECT_FALSE(page->Paused());

  web_view->SetOpenedByDOM();

  auto* main_frame = To<LocalFrame>(page->MainFrame());
  EXPECT_FALSE(main_frame->DomWindow()->closed());

  ScriptState* script_state = ToScriptStateForMainWorld(main_frame);
  ScriptState::Scope entered_context_scope(script_state);
  v8::Context::BackupIncumbentScope incumbent_context_scope(
      script_state->GetContext());

  main_frame->DomWindow()->close(script_state->GetIsolate());
  // The window should be marked closed...
  EXPECT_TRUE(main_frame->DomWindow()->closed());
  // EXPECT_TRUE(page->isClosing());
  // ...but not yet detached.
  EXPECT_TRUE(main_frame->GetPage());

  {
    ScopedPagePauser pauser;
    EXPECT_TRUE(page->Paused());
  }
}

TEST_F(WebViewTest, ForceAndResetViewport) {
  RegisterMockedHttpURLLoad("200-by-300.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad(base_url_ + "200-by-300.html");
  web_view_impl->MainFrameViewWidget()->Resize(gfx::Size(100, 150));
  SetViewportSize(gfx::Size(100, 150));
  DevToolsEmulator* dev_tools_emulator = web_view_impl->GetDevToolsEmulator();

  gfx::Transform expected_matrix;
  expected_matrix.MakeIdentity();
  EXPECT_EQ(expected_matrix, web_view_impl->GetDeviceEmulationTransform());

  // Override applies transform, sets visible rect, and disables
  // visual viewport clipping.
  gfx::Transform matrix =
      dev_tools_emulator->ForceViewportForTesting(gfx::PointF(50, 55), 2.f);
  expected_matrix = gfx::Transform::MakeScale(2.f);
  expected_matrix.Translate(-50, -55);
  EXPECT_EQ(expected_matrix, matrix);

  // Setting new override discards previous one.
  matrix = dev_tools_emulator->ForceViewportForTesting(gfx::PointF(5.4f, 10.5f),
                                                       1.5f);
  expected_matrix = gfx::Transform::MakeScale(1.5f);
  expected_matrix.Translate(-5.4f, -10.5f);
  EXPECT_EQ(expected_matrix, matrix);

  // Clearing override restores original transform, visible rect and
  // visual viewport clipping.
  matrix = dev_tools_emulator->ResetViewportForTesting();
  expected_matrix.MakeIdentity();
  EXPECT_EQ(expected_matrix, matrix);
}

TEST_F(WebViewTest, ViewportOverrideIntegratesDeviceMetricsOffsetAndScale) {
  RegisterMockedHttpURLLoad("200-by-300.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad(base_url_ + "200-by-300.html");
  web_view_impl->MainFrameViewWidget()->Resize(gfx::Size(100, 150));

  gfx::Transform expected_matrix;
  expected_matrix.MakeIdentity();
  EXPECT_EQ(expected_matrix, web_view_impl->GetDeviceEmulationTransform());

  DeviceEmulationParams emulation_params;
  emulation_params.scale = 2.f;
  web_view_impl->EnableDeviceEmulation(emulation_params);
  expected_matrix = gfx::Transform::MakeScale(2.f);
  EXPECT_EQ(expected_matrix, web_view_impl->GetDeviceEmulationTransform());

  // Device metrics offset and scale are applied before viewport override.
  emulation_params.viewport_offset = gfx::PointF(5, 10);
  emulation_params.viewport_scale = 1.5f;
  web_view_impl->EnableDeviceEmulation(emulation_params);
  expected_matrix = gfx::Transform::MakeScale(1.5f);
  expected_matrix.Translate(-5, -10);
  expected_matrix.Scale(2.f);
  EXPECT_EQ(expected_matrix, web_view_impl->GetDeviceEmulationTransform());
}

TEST_F(WebViewTest, ViewportOverrideAdaptsToScaleAndScroll) {
  RegisterMockedHttpURLLoad("200-by-300.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad(base_url_ + "200-by-300.html");
  web_view_impl->MainFrameViewWidget()->Resize(gfx::Size(100, 150));
  SetViewportSize(gfx::Size(100, 150));
  LocalFrameView* frame_view =
      web_view_impl->MainFrameImpl()->GetFrame()->View();

  gfx::Transform expected_matrix;
  expected_matrix.MakeIdentity();
  EXPECT_EQ(expected_matrix, web_view_impl->GetDeviceEmulationTransform());

  // Initial transform takes current page scale and scroll position into
  // account.
  web_view_impl->SetPageScaleFactor(1.5f);
  frame_view->LayoutViewport()->SetScrollOffset(
      ScrollOffset(100, 150), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kInstant);

  DeviceEmulationParams emulation_params;
  emulation_params.viewport_offset = gfx::PointF(50, 55);
  emulation_params.viewport_scale = 2.f;
  web_view_impl->EnableDeviceEmulation(emulation_params);
  expected_matrix = gfx::Transform::MakeScale(2.f);
  expected_matrix.Translate(-50, -55);
  expected_matrix.Translate(100, 150);
  expected_matrix.Scale(1. / 1.5f);
  EXPECT_EQ(expected_matrix, web_view_impl->GetDeviceEmulationTransform());

  // Transform adapts to scroll changes.
  frame_view->LayoutViewport()->SetScrollOffset(
      ScrollOffset(50, 55), mojom::blink::ScrollType::kProgrammatic,
      mojom::blink::ScrollBehavior::kInstant);
  expected_matrix = gfx::Transform::MakeScale(2.f);
  expected_matrix.Translate(-50, -55);
  expected_matrix.Translate(50, 55);
  expected_matrix.Scale(1. / 1.5f);
  EXPECT_EQ(expected_matrix, web_view_impl->GetDeviceEmulationTransform());

  // Transform adapts to page scale changes.
  web_view_impl->SetPageScaleFactor(2.f);
  expected_matrix = gfx::Transform::MakeScale(2.f);
  expected_matrix.Translate(-50, -55);
  expected_matrix.Translate(50, 55);
  expected_matrix.Scale(1. / 2.f);
  EXPECT_EQ(expected_matrix, web_view_impl->GetDeviceEmulationTransform());
}

TEST_F(WebViewTest, ResizeForPrintingViewportUnits) {
  WebViewImpl* web_view = web_view_helper_.Initialize();
  web_view->MainFrameViewWidget()->Resize(gfx::Size(800, 600));

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(),
                                     "<style>"
                                     "  body { margin: 0px; }"
                                     "  #vw { width: 100vw; height: 100vh; }"
                                     "</style>"
                                     "<div id=vw></div>",
                                     base_url);

  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  Document* document = frame->GetFrame()->GetDocument();
  Element* vw_element = document->getElementById(AtomicString("vw"));

  EXPECT_EQ(800, vw_element->OffsetWidth());

  gfx::Size page_size(300, 360);

  WebPrintParams print_params((gfx::SizeF(page_size)));

  gfx::Size expected_size = page_size;

  frame->PrintBegin(print_params, WebNode());

  EXPECT_EQ(expected_size.width(), vw_element->OffsetWidth());
  EXPECT_EQ(expected_size.height(), vw_element->OffsetHeight());

  web_view->MainFrameWidget()->Resize(page_size);

  EXPECT_EQ(expected_size.width(), vw_element->OffsetWidth());
  EXPECT_EQ(expected_size.height(), vw_element->OffsetHeight());

  web_view->MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  frame->PrintEnd();

  EXPECT_EQ(800, vw_element->OffsetWidth());
}

TEST_F(WebViewTest, WidthMediaQueryWithPageZoomAfterPrinting) {
  WebViewImpl* web_view = web_view_helper_.Initialize();
  web_view->MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  web_view->MainFrameWidget()->SetZoomLevel(ZoomFactorToZoomLevel(2.0));

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(),
                                     "<style>"
                                     "  @media (max-width: 600px) {"
                                     "    div { color: green }"
                                     "  }"
                                     "</style>"
                                     "<div id=d></div>",
                                     base_url);

  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  Document* document = frame->GetFrame()->GetDocument();
  Element* div = document->getElementById(AtomicString("d"));

  EXPECT_EQ(
      Color::FromRGB(0, 128, 0),
      div->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));

  gfx::SizeF page_size(300, 360);

  WebPrintParams print_params(page_size);

  frame->PrintBegin(print_params, WebNode());
  frame->PrintEnd();

  EXPECT_EQ(
      Color::FromRGB(0, 128, 0),
      div->GetComputedStyle()->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(WebViewTest, ViewportUnitsPrintingWithPageZoom) {
  WebViewImpl* web_view = web_view_helper_.Initialize();
  web_view->MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  web_view->MainFrameWidget()->SetZoomLevel(ZoomFactorToZoomLevel(2.0));

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(),
                                     "<style>"
                                     "  body { margin: 0 }"
                                     "  #t1 { width: 100% }"
                                     "  #t2 { width: 100vw }"
                                     "</style>"
                                     "<div id=t1></div>"
                                     "<div id=t2></div>",
                                     base_url);

  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  Document* document = frame->GetFrame()->GetDocument();
  Element* t1 = document->getElementById(AtomicString("t1"));
  Element* t2 = document->getElementById(AtomicString("t2"));

  EXPECT_EQ(400, t1->OffsetWidth());
  EXPECT_EQ(400, t2->OffsetWidth());

  gfx::Size page_size(600, 720);
  int expected_width = page_size.width();

  WebPrintParams print_params((gfx::SizeF(page_size)));

  frame->PrintBegin(print_params, WebNode());

  EXPECT_EQ(expected_width, t1->OffsetWidth());
  EXPECT_EQ(expected_width, t2->OffsetWidth());

  frame->PrintEnd();
}

TEST_F(WebViewTest, ResizeWithFixedPosCrash) {
  WebViewImpl* web_view = web_view_helper_.Initialize();
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(),
                                     "<div style='position:fixed;'></div>",
                                     base_url);
  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  gfx::Size page_size(300, 360);
  WebPrintParams print_params((gfx::SizeF(page_size)));
  frame->PrintBegin(print_params, WebNode());
  web_view->MainFrameWidget()->Resize(page_size);
  frame->PrintEnd();
}

TEST_F(WebViewTest, DeviceEmulationResetScrollbars) {
  WebViewImpl* web_view = web_view_helper_.Initialize();
  web_view->MainFrameViewWidget()->Resize(gfx::Size(800, 600));

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(),
                                     "<!doctype html>"
                                     "<meta name='viewport'"
                                     "    content='width=device-width'>"
                                     "<style>"
                                     "  body {margin: 0px; height:3000px;}"
                                     "</style>",
                                     base_url);
  UpdateAllLifecyclePhases();

  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  auto* frame_view = frame->GetFrameView();
  EXPECT_FALSE(frame_view->VisualViewportSuppliesScrollbars());
  EXPECT_NE(nullptr, frame_view->LayoutViewport()->VerticalScrollbar());

  DeviceEmulationParams params;
  params.screen_type = mojom::EmulatedScreenType::kMobile;
  params.device_scale_factor = 0;
  params.scale = 1;

  web_view->EnableDeviceEmulation(params);

  // The visual viewport should now proivde the scrollbars instead of the view.
  EXPECT_TRUE(frame_view->VisualViewportSuppliesScrollbars());
  EXPECT_EQ(nullptr, frame_view->LayoutViewport()->VerticalScrollbar());

  web_view->DisableDeviceEmulation();

  // The view should once again provide the scrollbars.
  EXPECT_FALSE(frame_view->VisualViewportSuppliesScrollbars());
  EXPECT_NE(nullptr, frame_view->LayoutViewport()->VerticalScrollbar());
}

TEST_F(WebViewTest, SetZoomLevelWhilePluginFocused) {
  class PluginCreatingWebFrameClient
      : public frame_test_helpers::TestWebFrameClient {
   public:
    // WebLocalFrameClient overrides:
    WebPlugin* CreatePlugin(const WebPluginParams& params) override {
      return new FakeWebPlugin(params);
    }
  };
  PluginCreatingWebFrameClient frame_client;
  WebViewImpl* web_view = web_view_helper_.Initialize(&frame_client);
  WebURL base_url = url_test_helpers::ToKURL("https://example.com/");
  frame_test_helpers::LoadHTMLString(
      web_view->MainFrameImpl(),
      "<!DOCTYPE html><html><body>"
      "<object type='application/x-webkit-test-plugin'></object>"
      "</body></html>",
      base_url);
  // Verify the plugin is loaded.
  LocalFrame* main_frame = web_view->MainFrameImpl()->GetFrame();
  auto* plugin_element =
      To<HTMLObjectElement>(main_frame->GetDocument()->body()->firstChild());
  EXPECT_TRUE(plugin_element->OwnedPlugin());
  // Focus the plugin element, and then change the zoom level on the WebView.
  plugin_element->Focus();
  EXPECT_FLOAT_EQ(1.0f, main_frame->LayoutZoomFactor());
  web_view->MainFrameWidget()->SetZoomLevel(-1.0);
  // Even though the plugin is focused, the entire frame's zoom factor should
  // still be updated.
  EXPECT_FLOAT_EQ(5.0f / 6.0f, main_frame->LayoutZoomFactor());
  web_view_helper_.Reset();  // Remove dependency on locally scoped client.
}

// Tests that a layout update that detaches a plugin doesn't crash if the
// plugin tries to execute script while being destroyed.
TEST_F(WebViewTest, DetachPluginInLayout) {
  class ScriptInDestroyPlugin : public FakeWebPlugin {
   public:
    ScriptInDestroyPlugin(WebLocalFrame* frame, const WebPluginParams& params)
        : FakeWebPlugin(params), frame_(frame) {}

    // WebPlugin overrides:
    void Destroy() override {
      frame_->ExecuteScript(WebScriptSource("console.log('done')"));
      // Deletes this.
      FakeWebPlugin::Destroy();
    }

   private:
    WebLocalFrame* frame_;  // Unowned
  };

  class PluginCreatingWebFrameClient
      : public frame_test_helpers::TestWebFrameClient {
   public:
    // WebLocalFrameClient overrides:
    WebPlugin* CreatePlugin(const WebPluginParams& params) override {
      return new ScriptInDestroyPlugin(Frame(), params);
    }

    void DidAddMessageToConsole(const WebConsoleMessage& message,
                                const WebString& source_name,
                                unsigned source_line,
                                const WebString& stack_trace) override {
      message_ = message.text;
    }

    const String& Message() const { return message_; }

   private:
    String message_;
  };

  PluginCreatingWebFrameClient frame_client;
  WebViewImpl* web_view = web_view_helper_.Initialize(&frame_client);
  WebURL base_url = url_test_helpers::ToKURL("https://example.com/");
  frame_test_helpers::LoadHTMLString(
      web_view->MainFrameImpl(),
      "<!DOCTYPE html><html><body>"
      "<object type='application/x-webkit-test-plugin'></object>"
      "</body></html>",
      base_url);
  // Verify the plugin is loaded.
  LocalFrame* main_frame = web_view->MainFrameImpl()->GetFrame();
  auto* plugin_element =
      To<HTMLObjectElement>(main_frame->GetDocument()->body()->firstChild());
  EXPECT_TRUE(plugin_element->OwnedPlugin());

  plugin_element->style()->setCSSText(main_frame->DomWindow(), "display: none",
                                      ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(plugin_element->OwnedPlugin());
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(plugin_element->OwnedPlugin());
  EXPECT_EQ("done", frame_client.Message());
  web_view_helper_.Reset();  // Remove dependency on locally scoped client.
}

// Check that first input delay is correctly reported to the document.
TEST_F(WebViewTest, FirstInputDelayReported) {
  WebViewImpl* web_view = web_view_helper_.Initialize();
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(),
                                     "<html><body></body></html>", base_url);

  LocalFrame* main_frame = web_view->MainFrameImpl()->GetFrame();
  ASSERT_NE(nullptr, main_frame);

  Document* document = main_frame->GetDocument();
  ASSERT_NE(nullptr, document);

  base::TimeTicks start_time = test_task_runner_->NowTicks();
  test_task_runner_->FastForwardBy(base::Milliseconds(70));

  InteractiveDetector* interactive_detector =
      GetTestInteractiveDetector(*document);

  EXPECT_FALSE(interactive_detector->GetFirstInputDelay().has_value());

  WebKeyboardEvent key_event1(WebInputEvent::Type::kRawKeyDown,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests());
  key_event1.dom_key = ui::DomKey::FromCharacter(' ');
  key_event1.windows_key_code = VKEY_SPACE;
  key_event1.SetTimeStamp(test_task_runner_->NowTicks());
  test_task_runner_->FastForwardBy(base::Milliseconds(50));
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event1, ui::LatencyInfo()));

  EXPECT_TRUE(interactive_detector->GetFirstInputDelay().has_value());
  EXPECT_NEAR(50,
              (*interactive_detector->GetFirstInputDelay()).InMillisecondsF(),
              0.01);
  EXPECT_EQ(70, (*interactive_detector->GetFirstInputTimestamp() - start_time)
                    .InMillisecondsF());

  // Sending a second event won't change the FirstInputDelay.
  WebKeyboardEvent key_event2(WebInputEvent::Type::kRawKeyDown,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests());
  key_event2.dom_key = ui::DomKey::FromCharacter(' ');
  key_event2.windows_key_code = VKEY_SPACE;
  test_task_runner_->FastForwardBy(base::Milliseconds(60));
  key_event2.SetTimeStamp(test_task_runner_->NowTicks());
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event2, ui::LatencyInfo()));

  EXPECT_NEAR(50,
              (*interactive_detector->GetFirstInputDelay()).InMillisecondsF(),
              0.01);
  EXPECT_EQ(70, (*interactive_detector->GetFirstInputTimestamp() - start_time)
                    .InMillisecondsF());
}

TEST_F(WebViewTest, InputDelayReported) {
  test_task_runner_->FastForwardBy(base::Milliseconds(50));

  WebViewImpl* web_view = web_view_helper_.Initialize();

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(),
                                     "<html><body></body></html>", base_url,
                                     test_task_runner_->GetMockTickClock());

  LocalFrame* main_frame = web_view->MainFrameImpl()->GetFrame();
  ASSERT_NE(nullptr, main_frame);
  Document* document = main_frame->GetDocument();
  ASSERT_NE(nullptr, document);
  GetTestInteractiveDetector(*document);

  test_task_runner_->FastForwardBy(base::Milliseconds(70));

  base::HistogramTester histogram_tester;
  WebKeyboardEvent key_event1(WebInputEvent::Type::kRawKeyDown,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests());
  key_event1.dom_key = ui::DomKey::FromCharacter(' ');
  key_event1.windows_key_code = VKEY_SPACE;
  key_event1.SetTimeStamp(test_task_runner_->NowTicks());
  test_task_runner_->FastForwardBy(base::Milliseconds(50));
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event1, ui::LatencyInfo()));

  WebKeyboardEvent key_event2(WebInputEvent::Type::kRawKeyDown,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests());
  key_event2.dom_key = ui::DomKey::FromCharacter(' ');
  key_event2.windows_key_code = VKEY_SPACE;
  key_event2.SetTimeStamp(test_task_runner_->NowTicks());
  test_task_runner_->FastForwardBy(base::Milliseconds(50));
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event2, ui::LatencyInfo()));

  WebKeyboardEvent key_event3(WebInputEvent::Type::kRawKeyDown,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests());
  key_event3.dom_key = ui::DomKey::FromCharacter(' ');
  key_event3.windows_key_code = VKEY_SPACE;
  key_event3.SetTimeStamp(test_task_runner_->NowTicks());
  test_task_runner_->FastForwardBy(base::Milliseconds(70));
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event3, ui::LatencyInfo()));

  histogram_tester.ExpectTotalCount("PageLoad.InteractiveTiming.InputDelay3",
                                    3);
  histogram_tester.ExpectBucketCount("PageLoad.InteractiveTiming.InputDelay3",
                                     50, 2);
  histogram_tester.ExpectBucketCount("PageLoad.InteractiveTiming.InputDelay3",
                                     70, 1);

  histogram_tester.ExpectTotalCount(
      "PageLoad.InteractiveTiming.InputTimestamp3", 3);
  histogram_tester.ExpectBucketCount(
      "PageLoad.InteractiveTiming.InputTimestamp3", 70, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.InteractiveTiming.InputTimestamp3", 120, 1);
  histogram_tester.ExpectBucketCount(
      "PageLoad.InteractiveTiming.InputTimestamp3", 170, 1);
}

// TODO(npm): Improve this test to receive real input sequences and avoid hacks.
// Check that first input delay is correctly reported to the document when the
// first input is a pointer down event, and we receive a pointer up event.
TEST_F(WebViewTest, PointerDownUpFirstInputDelay) {
  WebViewImpl* web_view = web_view_helper_.Initialize();
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(),
                                     "<html><body></body></html>", base_url);
  // Add an event listener for pointerdown to ensure it is not optimized out
  // before reaching the EventDispatcher.
  WebLocalFrame* frame = web_view_helper_.LocalMainFrame();
  frame->ExecuteScript(
      WebScriptSource("addEventListener('pointerdown', function() {});"));

  LocalFrame* main_frame = web_view->MainFrameImpl()->GetFrame();
  ASSERT_NE(nullptr, main_frame);

  Document* document = main_frame->GetDocument();
  ASSERT_NE(nullptr, document);

  base::TimeTicks start_time = test_task_runner_->NowTicks();
  test_task_runner_->FastForwardBy(base::Milliseconds(70));

  InteractiveDetector* interactive_detector =
      GetTestInteractiveDetector(*document);

  WebPointerEvent pointer_down(
      WebInputEvent::Type::kPointerDown,
      WebPointerProperties(1, WebPointerProperties::PointerType::kTouch), 5, 5);
  pointer_down.SetTimeStamp(test_task_runner_->NowTicks());
  // Set this to the left button, needed for testing to behave properly.
  pointer_down.SetModifiers(WebInputEvent::kLeftButtonDown);
  pointer_down.button = WebPointerProperties::Button::kLeft;
  test_task_runner_->FastForwardBy(base::Milliseconds(50));
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(pointer_down, ui::LatencyInfo()));

  // We don't know if this pointer event will result in a scroll or not, so we
  // can't report its delay. We don't consider a scroll to be meaningful input.
  EXPECT_FALSE(interactive_detector->GetFirstInputDelay().has_value());

  // When we receive a pointer up, we report the delay of the pointer down.
  WebPointerEvent pointer_up(
      WebInputEvent::Type::kPointerUp,
      WebPointerProperties(1, WebPointerProperties::PointerType::kTouch), 5, 5);
  test_task_runner_->FastForwardBy(base::Milliseconds(60));
  pointer_up.SetTimeStamp(test_task_runner_->NowTicks());
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(pointer_up, ui::LatencyInfo()));

  EXPECT_NEAR(50,
              (*interactive_detector->GetFirstInputDelay()).InMillisecondsF(),
              0.01);
  EXPECT_EQ(70, (*interactive_detector->GetFirstInputTimestamp() - start_time)
                    .InMillisecondsF());
}

// We need a way for JS to advance the mock clock. Hook into console.log, so
// that logging advances the clock by |event_handling_delay| seconds.
class MockClockAdvancingWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  MockClockAdvancingWebFrameClient(
      scoped_refptr<base::TestMockTimeTaskRunner> task_runner,
      base::TimeDelta event_handling_delay)
      : task_runner_(std::move(task_runner)),
        event_handling_delay_(event_handling_delay) {}
  // WebLocalFrameClient overrides:
  void DidAddMessageToConsole(const WebConsoleMessage& message,
                              const WebString& source_name,
                              unsigned source_line,
                              const WebString& stack_trace) override {
    task_runner_->FastForwardBy(event_handling_delay_);
  }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::TimeDelta event_handling_delay_;
};

// Check that the input delay is correctly reported to the document.
TEST_F(WebViewTest, FirstInputDelayExcludesProcessingTime) {
  // Page load timing logic depends on the time not being zero.
  test_task_runner_->FastForwardBy(base::Milliseconds(1));
  MockClockAdvancingWebFrameClient frame_client(test_task_runner_,
                                                base::Milliseconds(6000));
  WebViewImpl* web_view = web_view_helper_.Initialize(&frame_client);
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(),
                                     "<html><body></body></html>", base_url,
                                     test_task_runner_->GetMockTickClock());

  LocalFrame* main_frame = web_view->MainFrameImpl()->GetFrame();
  ASSERT_NE(nullptr, main_frame);

  Document* document = main_frame->GetDocument();
  ASSERT_NE(nullptr, document);

  WebLocalFrame* frame = web_view_helper_.LocalMainFrame();
  // console.log will advance the mock clock.
  frame->ExecuteScript(
      WebScriptSource("document.addEventListener('keydown', "
                      "() => {console.log('advancing timer');})"));

  InteractiveDetector* interactive_detector =
      GetTestInteractiveDetector(*document);

  WebKeyboardEvent key_event(WebInputEvent::Type::kRawKeyDown,
                             WebInputEvent::kNoModifiers,
                             WebInputEvent::GetStaticTimeStampForTests());
  key_event.dom_key = ui::DomKey::FromCharacter(' ');
  key_event.windows_key_code = VKEY_SPACE;
  key_event.SetTimeStamp(test_task_runner_->NowTicks());

  test_task_runner_->FastForwardBy(base::Milliseconds(5000));

  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event, ui::LatencyInfo()));

  EXPECT_TRUE(interactive_detector->GetFirstInputDelay().has_value());
  base::TimeDelta first_input_delay =
      *interactive_detector->GetFirstInputDelay();
  EXPECT_EQ(5000, first_input_delay.InMillisecondsF());

  web_view_helper_.Reset();  // Remove dependency on locally scoped client.
}

TEST_F(WebViewTest, RootLayerAttachment) {
  WebView* web_view = web_view_helper_.InitializeAndLoad("about:blank");

  // Do a lifecycle update that includes compositing but not paint. Hit test
  // events are an example of a real case where this occurs
  // (see: WebViewTest::ClientTapHandling).
  web_view->MainFrameWidget()->UpdateLifecycle(WebLifecycleUpdate::kPrePaint,
                                               DocumentUpdateReason::kTest);

  // Layers (including the root layer) should not be attached until the paint
  // lifecycle phase.
  cc::LayerTreeHost* layer_tree_host = web_view_helper_.GetLayerTreeHost();
  EXPECT_FALSE(layer_tree_host->root_layer());

  // Do a full lifecycle update and ensure that the root layer has been added.
  web_view->MainFrameWidget()->UpdateLifecycle(WebLifecycleUpdate::kAll,
                                               DocumentUpdateReason::kTest);
  EXPECT_TRUE(layer_tree_host->root_layer());
}

TEST_F(WebViewTest, ForceDarkModeInvalidatesPaint) {
  WebViewImpl* web_view = web_view_helper_.Initialize();
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 500));
  UpdateAllLifecyclePhases();

  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();
  ASSERT_TRUE(document);
  web_view->GetSettings()->SetForceDarkModeEnabled(true);
  EXPECT_TRUE(document->GetLayoutView()->ShouldDoFullPaintInvalidation());
}

// Regression test for https://crbug.com/1012068
TEST_F(WebViewTest, LongPressImageAndThenLongTapImage) {
  RegisterMockedHttpURLLoad("long_press_image.html");

  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url_ + "long_press_image.html");
  web_view->SettingsImpl()->SetAlwaysShowContextMenuOnTouch(false);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebGestureEvent event(WebInputEvent::Type::kGestureLongPress,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests(),
                        WebGestureDevice::kTouchscreen);
  event.SetPositionInWidget(gfx::PointF(10, 10));

  EXPECT_EQ(WebInputEventResult::kHandledSystem,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(event, ui::LatencyInfo())));
  EXPECT_TRUE(
      web_view->GetPage()->GetContextMenuController().ContextMenuNodeForFrame(
          web_view->MainFrameImpl()->GetFrame()));

  WebGestureEvent tap_event(WebInputEvent::Type::kGestureLongTap,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests(),
                            WebGestureDevice::kTouchscreen);
  tap_event.SetPositionInWidget(gfx::PointF(10, 10));

  EXPECT_EQ(WebInputEventResult::kNotHandled,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(tap_event, ui::LatencyInfo())));
  EXPECT_TRUE(
      web_view->GetPage()->GetContextMenuController().ContextMenuNodeForFrame(
          web_view->MainFrameImpl()->GetFrame()));
}

// Regression test for http://crbug.com/41562
TEST_F(WebViewTest, UpdateTargetURLWithInvalidURL) {
  WebViewImpl* web_view = web_view_helper_.Initialize();
  const KURL invalid_kurl("http://");
  web_view->UpdateTargetURL(blink::WebURL(invalid_kurl),
                            /* fallback_url=*/blink::WebURL());
  EXPECT_EQ(invalid_kurl, web_view->target_url_);
}

// Regression test for https://crbug.com/1112987
TEST_F(WebViewTest, LongPressThenLongTapLinkInIframeStartsContextMenu) {
  RegisterMockedHttpURLLoad("long_press_link_in_iframe.html");

  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad(
      base_url_ + "long_press_link_in_iframe.html");
  web_view->SettingsImpl()->SetTouchDragDropEnabled(true);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases();
  RunPendingTasks();

  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  Document* document = frame->GetFrame()->GetDocument();
  Element* child_frame = document->getElementById(AtomicString("childframe"));
  DCHECK(child_frame);
  Document* child_document =
      To<HTMLIFrameElement>(child_frame)->contentDocument();
  Element* anchor = child_document->getElementById(AtomicString("anchorTag"));
  gfx::Point center =
      To<WebLocalFrameImpl>(
          web_view->MainFrame()->FirstChild()->ToWebLocalFrame())
          ->GetFrameView()
          ->FrameToScreen(anchor->GetLayoutObject()->AbsoluteBoundingBoxRect())
          .CenterPoint();

  WebGestureEvent longpress_event(WebInputEvent::Type::kGestureLongPress,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::GetStaticTimeStampForTests(),
                                  WebGestureDevice::kTouchscreen);
  longpress_event.SetPositionInWidget(gfx::PointF(center.x(), center.x()));
  EXPECT_EQ(WebInputEventResult::kHandledSystem,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(longpress_event, ui::LatencyInfo())));

  WebGestureEvent tap_event(WebInputEvent::Type::kGestureLongTap,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests(),
                            WebGestureDevice::kTouchscreen);
  tap_event.SetPositionInWidget(gfx::PointF(center.x(), center.x()));

  // If touch-drag-and-context-menu is enabled, we expect an ongoing drag
  // operation at the moment a tap is dispatched.  This changes the outcome of
  // the tap event-handler below to "suppressed".
  WebInputEventResult expected_tap_handling_result =
      RuntimeEnabledFeatures::TouchDragAndContextMenuEnabled()
          ? WebInputEventResult::kHandledSuppressed
          : WebInputEventResult::kNotHandled;
  EXPECT_EQ(expected_tap_handling_result,
            web_view->MainFrameWidget()->HandleInputEvent(
                WebCoalescedInputEvent(tap_event, ui::LatencyInfo())));
  EXPECT_EQ("anchor contextmenu",
            web_view->MainFrameImpl()->GetDocument().Title());
}

TEST_F(WebViewTest, SetHistoryLengthAndOffset) {
  WebViewImpl* web_view_impl = web_view_helper_.Initialize();

  // No history to merge; one committed page.
  web_view_impl->SetHistoryListFromNavigation(0, 1);
  EXPECT_EQ(1, web_view_impl->HistoryBackListCount() +
                   web_view_impl->HistoryForwardListCount() + 1);
  EXPECT_EQ(0, web_view_impl->HistoryBackListCount());

  // History of length 1 to merge; one committed page.
  web_view_impl->SetHistoryListFromNavigation(1, 2);
  EXPECT_EQ(2, web_view_impl->HistoryBackListCount() +
                   web_view_impl->HistoryForwardListCount() + 1);
  EXPECT_EQ(1, web_view_impl->HistoryBackListCount());
}

// PopupWidgetImpl should inherit emulation params from the parent.
TEST_F(WebViewTest, EmulatingPopupRect) {
  // Some platforms don't support PagePopups so just return.
  if (!RuntimeEnabledFeatures::PagePopupEnabled())
    return;
  WebViewImpl* web_view = web_view_helper_.Initialize();
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(),
                                     "<html><div id=\"container\">"
                                     "   <select id=\"select\">"
                                     "     <option>1</option>"
                                     "     <option>2</option>"
                                     "   </select></div>"
                                     "</html>",
                                     base_url);

  LocalFrame* frame = web_view->MainFrameImpl()->GetFrame();
  auto* select = To<HTMLSelectElement>(
      frame->GetDocument()->getElementById(AtomicString("select")));
  ASSERT_TRUE(select);

  // Real screen rect set to 800x600.
  gfx::Rect screen_rect(800, 600);
  // Real widget and window screen rects.
  gfx::Rect window_screen_rect(1, 2, 137, 139);
  gfx::Rect widget_screen_rect(5, 7, 57, 59);

  blink::VisualProperties visual_properties;
  visual_properties.screen_infos = display::ScreenInfos(display::ScreenInfo());
  visual_properties.new_size = gfx::Size(400, 300);
  visual_properties.visible_viewport_size = gfx::Size(400, 300);
  visual_properties.screen_infos.mutable_current().rect = gfx::Rect(800, 600);

  web_view->MainFrameWidget()->ApplyVisualProperties(visual_properties);

  // Verify screen rect will be set.
  EXPECT_EQ(gfx::Rect(web_view->MainFrameWidget()->GetScreenInfo().rect),
            screen_rect);

  auto* menu = MakeGarbageCollected<InternalPopupMenu>(
      MakeGarbageCollected<EmptyChromeClient>(), *select);
  {
    // Make a popup widget.
    WebPagePopup* popup = web_view->OpenPagePopup(menu);

    // Fake that the browser showed it.
    static_cast<WebPagePopupImpl*>(popup)->DidShowPopup();

    // Set its size.
    popup->SetScreenRects(widget_screen_rect, window_screen_rect);

    // The WindowScreenRect, WidgetScreenRect, and ScreenRect are all available
    // to the popup.
    EXPECT_EQ(window_screen_rect, gfx::Rect(popup->WindowRect()));
    EXPECT_EQ(widget_screen_rect, gfx::Rect(popup->ViewRect()));
    EXPECT_EQ(screen_rect, gfx::Rect(popup->GetScreenInfo().rect));

    static_cast<WebPagePopupImpl*>(popup)->ClosePopup();
  }

  // Enable device emulation on the parent widget.
  DeviceEmulationParams emulation_params;
  gfx::Rect emulated_widget_rect(150, 160, 980, 1200);
  // In mobile emulation the WindowScreenRect and ScreenRect are both set to
  // match the WidgetScreenRect, which we set here.
  emulation_params.screen_type = mojom::EmulatedScreenType::kMobile;
  emulation_params.view_size = emulated_widget_rect.size();
  emulation_params.view_position = emulated_widget_rect.origin();
  web_view->EnableDeviceEmulation(emulation_params);

  {
    // Make a popup again. It should inherit device emulation params.
    WebPagePopup* popup = web_view->OpenPagePopup(menu);

    // Fake that the browser showed it.
    static_cast<WebPagePopupImpl*>(popup)->DidShowPopup();

    // Set its size again.
    popup->SetScreenRects(widget_screen_rect, window_screen_rect);

    // This time, the position of the WidgetScreenRect and WindowScreenRect
    // should be affected by emulation params.
    // TODO(danakj): This means the popup sees the top level widget at the
    // emulated position *plus* the real position. Whereas the top level
    // widget will see itself at the emulation position. Why this inconsistency?
    int window_x = emulated_widget_rect.x() + window_screen_rect.x();
    int window_y = emulated_widget_rect.y() + window_screen_rect.y();
    EXPECT_EQ(window_x, popup->WindowRect().x());
    EXPECT_EQ(window_y, popup->WindowRect().y());

    int widget_x = emulated_widget_rect.x() + widget_screen_rect.x();
    int widget_y = emulated_widget_rect.y() + widget_screen_rect.y();
    EXPECT_EQ(widget_x, popup->ViewRect().x());
    EXPECT_EQ(widget_y, popup->ViewRect().y());

    // TODO(danakj): Why don't the sizes get changed by emulation? The comments
    // that used to be in this test suggest that the sizes used to change, and
    // we were testing for that. But now we only test for positions changing?
    EXPECT_EQ(window_screen_rect.width(), popup->WindowRect().width());
    EXPECT_EQ(window_screen_rect.height(), popup->WindowRect().height());
    EXPECT_EQ(widget_screen_rect.width(), popup->ViewRect().width());
    EXPECT_EQ(widget_screen_rect.height(), popup->ViewRect().height());
    EXPECT_EQ(emulated_widget_rect,
              gfx::Rect(web_view->MainFrameWidget()->ViewRect()));
    EXPECT_EQ(emulated_widget_rect,
              gfx::Rect(web_view->MainFrameWidget()->WindowRect()));

    // TODO(danakj): Why isn't the ScreenRect visible to the popup an emulated
    // value? The ScreenRect has been changed by emulation as demonstrated
    // below.
    EXPECT_EQ(gfx::Rect(800, 600), gfx::Rect(popup->GetScreenInfo().rect));
    EXPECT_EQ(emulated_widget_rect,
              gfx::Rect(web_view->MainFrameWidget()->GetScreenInfo().rect));

    static_cast<WebPagePopupImpl*>(popup)->ClosePopup();
  }
}

TEST_F(WebViewTest, HiddenButPaintingIsSentToObservers) {
  // kHiddenButPainting should be sent to observers from both the visible and
  // hidden states.
  WebViewImpl* web_view = web_view_helper_.Initialize();
  MockWebViewObserver observer(web_view);

  web_view->SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               /*is_initial_state=*/false);
  EXPECT_EQ(observer.page_visibility_and_clear(),
            mojom::blink::PageVisibilityState::kHidden);

  web_view->SetVisibilityState(
      mojom::blink::PageVisibilityState::kHiddenButPainting,
      /*is_initial_state=*/false);
  EXPECT_EQ(observer.page_visibility_and_clear(),
            mojom::blink::PageVisibilityState::kHiddenButPainting);

  web_view->SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                               /*is_initial_state=*/false);
  EXPECT_EQ(observer.page_visibility_and_clear(),
            mojom::blink::PageVisibilityState::kVisible);

  web_view->SetVisibilityState(
      mojom::blink::PageVisibilityState::kHiddenButPainting,
      /*is_initial_state=*/false);
  EXPECT_EQ(observer.page_visibility_and_clear(),
            mojom::blink::PageVisibilityState::kHiddenButPainting);

  web_view->RemoveObserver(&observer);
}

TEST_F(WebViewTest, HiddenButPaintingPageIsntThrottled) {
  // The PageScheduler should consider `kHiddenButPainting` to be visible so
  // that the page is not throttled.
  WebViewImpl* web_view = web_view_helper_.Initialize();
  auto* const page = web_view->GetPage();
  auto* const scheduler = page->GetPageScheduler();

  // `kHidden` should mark the page as hidden for the scheduler.
  web_view->SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               /*is_initial_state=*/false);
  EXPECT_FALSE(scheduler->IsPageVisible());

  // `kVisible` should mark the page as visible for the scheduler.
  web_view->SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                               /*is_initial_state=*/false);
  EXPECT_TRUE(scheduler->IsPageVisible());

  // `kHiddenButPainting` should also mark the page scheduler as visible.
  web_view->SetVisibilityState(
      mojom::blink::PageVisibilityState::kHiddenButPainting,
      /*is_initial_state=*/false);
  EXPECT_TRUE(scheduler->IsPageVisible());
}

TEST_F(WebViewTest, HiddenVisibilityTransitionsDontDispatchEvents) {
  // When switching between `kHidden` and `kHiddenButPainting`, there should not
  // be events sent about it.  See https://crbug.com/1493618 .
  WebViewImpl* web_view = web_view_helper_.Initialize();

  // Switch in the 'kVisible' state, before we start checking.
  web_view->SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                               /*is_initial_state=*/false);

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(
      web_view->MainFrameImpl(),
      "<input id=input></input>"
      "<div id=log></div>"
      "<script>"
      "  var count = 0;"
      "  document.onvisibilitychange = function() {"
      "    ++count;"
      "    document.getElementById('log').textContent ="
      "      document.visibilityState + ' ' + count;"
      "  }"
      "</script>",
      base_url);

  WebLocalFrameImpl* frame = web_view->MainFrameImpl();
  WebElement log_element = frame->GetDocument().GetElementById("log");

  // kVisible => kHidden should fire an event.
  web_view->SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               /*is_initial_state=*/false);
  EXPECT_EQ("hidden 1", log_element.TextContent());

  // kHidden => kHidden should not fire an event.
  web_view->SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               /*is_initial_state=*/false);
  EXPECT_EQ("hidden 1", log_element.TextContent());

  // kHidden => kHiddenButPainting should not fire an event.
  web_view->SetVisibilityState(
      mojom::blink::PageVisibilityState::kHiddenButPainting,
      /*is_initial_state=*/false);
  EXPECT_EQ("hidden 1", log_element.TextContent());

  // kHiddenButPainting => kHiddenButPainting should not fire an event.
  web_view->SetVisibilityState(
      mojom::blink::PageVisibilityState::kHiddenButPainting,
      /*is_initial_state=*/false);
  EXPECT_EQ("hidden 1", log_element.TextContent());

  // kHiddenButPainting => kHidden should not fire an event.
  web_view->SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                               /*is_initial_state=*/false);
  EXPECT_EQ("hidden 1", log_element.TextContent());

  // kHidden => kVisible should fire an event.
  web_view->SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                               /*is_initial_state=*/false);
  EXPECT_EQ("visible 2", log_element.TextContent());

  // kVisible => kHiddenButPainting should fire an event.
  web_view->SetVisibilityState(
      mojom::blink::PageVisibilityState::kHiddenButPainting,
      /*is_initial_state=*/false);
  EXPECT_EQ("hidden 3", log_element.TextContent());

  // kHiddenButPainting => kVisible should fire an event.
  web_view->SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                               /*is_initial_state=*/false);
  EXPECT_EQ("visible 4", log_element.TextContent());

  // kVisible => kVisible should not fire an event.
  web_view->SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                               /*is_initial_state=*/false);
  EXPECT_EQ("visible 4", log_element.TextContent());
}

}  // namespace blink
