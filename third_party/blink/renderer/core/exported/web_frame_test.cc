/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/public/web/web_frame.h"

#include <initializer_list>
#include <limits>
#include <memory>

#include "base/bind_helpers.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/layers/picture_layer.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/scroll_node.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "skia/public/mojom/skcolor.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/common/page/launching_process_state.h"
#include "third_party/blink/public/common/widget/device_emulation_params.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"
#include "third_party/blink/public/mojom/blob/data_element.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame_owner_element_type.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom-blink.h"
#include "third_party/blink/public/mojom/page_state/page_state.mojom-blink.h"
#include "third_party/blink/public/mojom/scroll/scrollbar_mode.mojom-blink.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom-blink.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_context_menu_data.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_navigation_timings.h"
#include "third_party/blink/public/web/web_print_page_description.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_script_execution_callback.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_searchable_form_data.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_text_check_client.h"
#include "third_party/blink/public/web/web_text_checking_completion.h"
#include "third_party/blink/public/web/web_text_checking_result.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value_factory.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/v8_script_value_serializer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pointer_event_init.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/css/css_page_rule.h"
#include "third_party/blink/renderer/core/css/media_values.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/viewport_style_resolver.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/text_finder.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/spellcheck/idle_spell_check_controller.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/exported/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/find_in_page.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/frame/web_view_frame_widget.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/image_document.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/dev_tools_emulator.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/drag_image.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scoped_page_pauser.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_test_suite.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/fake_local_frame_host.h"
#include "third_party/blink/renderer/core/testing/fake_remote_frame_host.h"
#include "third_party/blink/renderer/core/testing/fake_remote_main_frame_host.h"
#include "third_party/blink/renderer/core/testing/mock_clipboard_host.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/scoped_fake_plugin_registry.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/blob/testing/fake_blob.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/testing/find_cc_layer.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "ui/base/ime/mojom/text_input_state.mojom-blink.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/gfx/transform.h"
#include "v8/include/v8.h"

using blink::url_test_helpers::ToKURL;
using blink::mojom::SelectionMenuBehavior;
using blink::test::RunPendingTasks;
using testing::ElementsAre;
using testing::Mock;
using testing::_;

namespace blink {

namespace {

const cc::ScrollNode* GetScrollNode(const cc::Layer* layer) {
  return layer->layer_tree_host()
      ->property_trees()
      ->scroll_tree.FindNodeFromElementId(layer->element_id());
}

std::string GetHTMLStringForReferrerPolicy(const std::string& meta_policy,
                                           const std::string& referrer_policy) {
  std::string meta_tag =
      meta_policy.empty()
          ? ""
          : base::StringPrintf("<meta name='referrer' content='%s'>",
                               meta_policy.c_str());
  std::string referrer_policy_attr =
      referrer_policy.empty()
          ? ""
          : base::StringPrintf("referrerpolicy='%s'", referrer_policy.c_str());
  return base::StringPrintf(
      "<!DOCTYPE html>"
      "%s"
      "<a id='dl' href='download_test' download='foo' %s>Click me</a>"
      "<script>"
      "(function () {"
      "  var evt = document.createEvent('MouseEvent');"
      "  evt.initMouseEvent('click', true, true);"
      "  document.getElementById('dl').dispatchEvent(evt);"
      "})();"
      "</script>",
      meta_tag.c_str(), referrer_policy_attr.c_str());
}
}  // namespace

const int kTouchPointPadding = 32;

const cc::OverscrollBehavior kOverscrollBehaviorAuto =
    cc::OverscrollBehavior(cc::OverscrollBehavior::Type::kAuto);

const cc::OverscrollBehavior kOverscrollBehaviorContain =
    cc::OverscrollBehavior(cc::OverscrollBehavior::Type::kContain);

const cc::OverscrollBehavior kOverscrollBehaviorNone =
    cc::OverscrollBehavior(cc::OverscrollBehavior::Type::kNone);

class WebFrameTest : public testing::Test {
 protected:
  WebFrameTest()
      : base_url_("http://internal.test/"),
        not_base_url_("http://external.test/"),
        chrome_url_("chrome://") {}

  ~WebFrameTest() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  void DisableRendererSchedulerThrottling() {
    // Make sure that the RendererScheduler is foregrounded to avoid getting
    // throttled.
    if (kLaunchingProcessIsBackgrounded) {
      ThreadScheduler::Current()
          ->GetWebMainThreadSchedulerForTest()
          ->SetRendererBackgrounded(false);
    }
  }

  void RegisterMockedHttpURLLoad(const std::string& file_name) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via the WebViewHelper instance in each test case.
    RegisterMockedURLLoadFromBase(base_url_, file_name);
  }

  void RegisterMockedChromeURLLoad(const std::string& file_name) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via the WebViewHelper instance in each test case.
    RegisterMockedURLLoadFromBase(chrome_url_, file_name);
  }

  void RegisterMockedURLLoadFromBase(const std::string& base_url,
                                     const std::string& file_name) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via the WebViewHelper instance in each test case.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url), test::CoreTestDataPath(),
        WebString::FromUTF8(file_name));
  }

  void RegisterMockedURLLoadWithCustomResponse(const WebURL& full_url,
                                               const WebString& file_path,
                                               WebURLResponse response) {
    url_test_helpers::RegisterMockedURLLoadWithCustomResponse(
        full_url, file_path, response);
  }

  void RegisterMockedHttpURLLoadWithCSP(const std::string& file_name,
                                        const std::string& csp,
                                        bool report_only = false) {
    WebURLResponse response;
    response.SetMimeType("text/html");
    response.AddHttpHeaderField(
        report_only ? WebString("Content-Security-Policy-Report-Only")
                    : WebString("Content-Security-Policy"),
        WebString::FromUTF8(csp));
    std::string full_string = base_url_ + file_name;
    RegisterMockedURLLoadWithCustomResponse(
        ToKURL(full_string),
        test::CoreTestDataPath(WebString::FromUTF8(file_name)), response);
  }

  void RegisterMockedHttpURLLoadWithMimeType(const std::string& file_name,
                                             const std::string& mime_type) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via the WebViewHelper instance in each test case.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), test::CoreTestDataPath(),
        WebString::FromUTF8(file_name), WebString::FromUTF8(mime_type));
  }

  static void ConfigureCompositingWebView(WebSettings* settings) {
    settings->SetPreferCompositingToLCDTextEnabled(true);
  }

  static void ConfigureAndroid(WebSettings* settings) {
    settings->SetViewportMetaEnabled(true);
    settings->SetViewportEnabled(true);
    settings->SetMainFrameResizesAreOrientationChanges(true);
    settings->SetShrinksViewportContentToFit(true);
    settings->SetViewportStyle(mojom::blink::ViewportStyle::kMobile);
  }

  static void ConfigureLoadsImagesAutomatically(WebSettings* settings) {
    settings->SetLoadsImagesAutomatically(true);
  }

  void InitializeTextSelectionWebView(
      const std::string& url,
      frame_test_helpers::WebViewHelper* web_view_helper) {
    web_view_helper->InitializeAndLoad(url);
    web_view_helper->GetWebView()->GetSettings()->SetDefaultFontSize(12);
    web_view_helper->Resize(gfx::Size(640, 480));
  }

  std::unique_ptr<DragImage> NodeImageTestSetup(
      frame_test_helpers::WebViewHelper* web_view_helper,
      const std::string& testcase) {
    RegisterMockedHttpURLLoad("nodeimage.html");
    web_view_helper->InitializeAndLoad(base_url_ + "nodeimage.html");
    web_view_helper->Resize(gfx::Size(640, 480));
    auto* frame =
        To<LocalFrame>(web_view_helper->GetWebView()->GetPage()->MainFrame());
    DCHECK(frame);
    Element* element = frame->GetDocument()->getElementById(testcase.c_str());
    return DataTransfer::NodeImage(*frame, *element);
  }

  void RemoveElementById(WebLocalFrameImpl* frame, const AtomicString& id) {
    Element* element = frame->GetFrame()->GetDocument()->getElementById(id);
    DCHECK(element);
    element->remove();
  }

  // Both sets the inner html and runs the document lifecycle.
  void InitializeWithHTML(LocalFrame& frame, const String& html_content) {
    frame.GetDocument()->body()->setInnerHTML(html_content);
    frame.GetDocument()->View()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  void SwapAndVerifyFirstChildConsistency(const char* const message,
                                          WebFrame* parent,
                                          WebFrame* new_child);
  void SwapAndVerifyMiddleChildConsistency(const char* const message,
                                           WebFrame* parent,
                                           WebFrame* new_child);
  void SwapAndVerifyLastChildConsistency(const char* const message,
                                         WebFrame* parent,
                                         WebFrame* new_child);
  void SwapAndVerifySubframeConsistency(const char* const message,
                                        WebFrame* parent,
                                        WebFrame* new_child);

  int NumMarkersInRange(const Document* document,
                        const EphemeralRange& range,
                        DocumentMarker::MarkerTypes marker_types) {
    Node* start_container = range.StartPosition().ComputeContainerNode();
    unsigned start_offset = static_cast<unsigned>(
        range.StartPosition().ComputeOffsetInContainerNode());

    Node* end_container = range.EndPosition().ComputeContainerNode();
    unsigned end_offset = static_cast<unsigned>(
        range.EndPosition().ComputeOffsetInContainerNode());

    int node_count = 0;
    for (Node& node : range.Nodes()) {
      const DocumentMarkerVector& markers_in_node =
          document->Markers().MarkersFor(To<Text>(node), marker_types);
      node_count += std::count_if(
          markers_in_node.begin(), markers_in_node.end(),
          [start_offset, end_offset, &node, &start_container,
           &end_container](const DocumentMarker* marker) {
            if (node == start_container && marker->EndOffset() <= start_offset)
              return false;
            if (node == end_container && marker->StartOffset() >= end_offset)
              return false;
            return true;
          });
    }

    return node_count;
  }

  void UpdateAllLifecyclePhases(WebViewImpl* web_view) {
    web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  static void GetElementAndCaretBoundsForFocusedEditableElement(
      frame_test_helpers::WebViewHelper& helper,
      IntRect& element_bounds,
      IntRect& caret_bounds) {
    Element* element = helper.GetWebView()->FocusedElement();
    gfx::Rect caret_in_viewport, unused;
    helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
        caret_in_viewport, unused);
    caret_bounds =
        helper.GetWebView()->GetPage()->GetVisualViewport().ViewportToRootFrame(
            IntRect(caret_in_viewport));
    element_bounds = element->GetDocument().View()->ConvertToRootFrame(
        PixelSnappedIntRect(element->Node::BoundingBox()));
  }

  std::string base_url_;
  std::string not_base_url_;
  std::string chrome_url_;
};

TEST_F(WebFrameTest, ContentText) {
  RegisterMockedHttpURLLoad("iframes_test.html");
  RegisterMockedHttpURLLoad("visible_iframe.html");
  RegisterMockedHttpURLLoad("invisible_iframe.html");
  RegisterMockedHttpURLLoad("zero_sized_iframe.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "iframes_test.html");

  // Now retrieve the frames text and test it only includes visible elements.
  std::string content = WebFrameContentDumper::DumpWebViewAsText(
                            web_view_helper.GetWebView(), 1024)
                            .Utf8();
  EXPECT_NE(std::string::npos, content.find(" visible paragraph"));
  EXPECT_NE(std::string::npos, content.find(" visible iframe"));
  EXPECT_EQ(std::string::npos, content.find(" invisible pararaph"));
  EXPECT_EQ(std::string::npos, content.find(" invisible iframe"));
  EXPECT_EQ(std::string::npos, content.find("iframe with zero size"));
}

TEST_F(WebFrameTest, FrameForEnteredContext) {
  RegisterMockedHttpURLLoad("iframes_test.html");
  RegisterMockedHttpURLLoad("visible_iframe.html");
  RegisterMockedHttpURLLoad("invisible_iframe.html");
  RegisterMockedHttpURLLoad("zero_sized_iframe.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "iframes_test.html");

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  EXPECT_EQ(web_view_helper.GetWebView()->MainFrame(),
            WebLocalFrame::FrameForContext(web_view_helper.GetWebView()
                                               ->MainFrameImpl()
                                               ->MainWorldScriptContext()));
  EXPECT_EQ(web_view_helper.GetWebView()->MainFrame()->FirstChild(),
            WebLocalFrame::FrameForContext(web_view_helper.GetWebView()
                                               ->MainFrame()
                                               ->FirstChild()
                                               ->ToWebLocalFrame()
                                               ->MainWorldScriptContext()));
}

class ScriptExecutionCallbackHelper : public WebScriptExecutionCallback {
 public:
  explicit ScriptExecutionCallbackHelper(v8::Local<v8::Context> context)
      : did_complete_(false), bool_value_(false), context_(context) {}
  ~ScriptExecutionCallbackHelper() override = default;

  bool DidComplete() const { return did_complete_; }
  const String& StringValue() const { return string_value_; }
  bool BoolValue() { return bool_value_; }

 private:
  // WebScriptExecutionCallback:
  void Completed(const WebVector<v8::Local<v8::Value>>& values) override {
    did_complete_ = true;
    if (!values.empty()) {
      if (values[0]->IsString()) {
        string_value_ =
            ToCoreString(values[0]->ToString(context_).ToLocalChecked());
      } else if (values[0]->IsBoolean()) {
        bool_value_ = values[0].As<v8::Boolean>()->Value();
      }
    }
  }

  bool did_complete_;
  String string_value_;
  bool bool_value_;
  v8::Local<v8::Context> context_;
};

TEST_F(WebFrameTest, RequestExecuteScript) {
  RegisterMockedHttpURLLoad("foo.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  ScriptExecutionCallbackHelper callback_helper(
      web_view_helper.LocalMainFrame()->MainWorldScriptContext());
  web_view_helper.GetWebView()
      ->MainFrameImpl()
      ->RequestExecuteScriptAndReturnValue(
          WebScriptSource(WebString("'hello';")), false, &callback_helper);
  RunPendingTasks();
  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_EQ("hello", callback_helper.StringValue());
}

TEST_F(WebFrameTest, SuspendedRequestExecuteScript) {
  RegisterMockedHttpURLLoad("foo.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  ScriptExecutionCallbackHelper callback_helper(
      web_view_helper.LocalMainFrame()->MainWorldScriptContext());

  // Suspend scheduled tasks so the script doesn't run.
  web_view_helper.GetWebView()->GetPage()->SetPaused(true);
  web_view_helper.GetWebView()
      ->MainFrameImpl()
      ->RequestExecuteScriptAndReturnValue(
          WebScriptSource(WebString("'hello';")), false, &callback_helper);
  RunPendingTasks();
  EXPECT_FALSE(callback_helper.DidComplete());

  web_view_helper.Reset();
  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_EQ(String(), callback_helper.StringValue());
}

TEST_F(WebFrameTest, RequestExecuteV8Function) {
  RegisterMockedHttpURLLoad("foo.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");

  auto callback = [](const v8::FunctionCallbackInfo<v8::Value>& info) {
    EXPECT_EQ(2, info.Length());
    EXPECT_TRUE(info[0]->IsUndefined());
    info.GetReturnValue().Set(info[1]);
  };

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context =
      web_view_helper.LocalMainFrame()->MainWorldScriptContext();
  ScriptExecutionCallbackHelper callback_helper(context);
  v8::Local<v8::Function> function =
      v8::Function::New(context, callback).ToLocalChecked();
  v8::Local<v8::Value> args[] = {v8::Undefined(isolate),
                                 V8String(isolate, "hello")};
  web_view_helper.GetWebView()
      ->MainFrame()
      ->ToWebLocalFrame()
      ->RequestExecuteV8Function(context, function, v8::Undefined(isolate),
                                 base::size(args), args, &callback_helper);
  RunPendingTasks();
  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_EQ("hello", callback_helper.StringValue());
}

TEST_F(WebFrameTest, RequestExecuteV8FunctionWhileSuspended) {
  DisableRendererSchedulerThrottling();
  RegisterMockedHttpURLLoad("foo.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");

  auto callback = [](const v8::FunctionCallbackInfo<v8::Value>& info) {
    info.GetReturnValue().Set(V8String(info.GetIsolate(), "hello"));
  };

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Context> context =
      web_view_helper.LocalMainFrame()->MainWorldScriptContext();

  // Suspend scheduled tasks so the script doesn't run.
  WebLocalFrameImpl* main_frame = web_view_helper.LocalMainFrame();
  web_view_helper.GetWebView()->GetPage()->SetPaused(true);

  ScriptExecutionCallbackHelper callback_helper(context);
  v8::Local<v8::Function> function =
      v8::Function::New(context, callback).ToLocalChecked();
  main_frame->RequestExecuteV8Function(context, function,
                                       v8::Undefined(context->GetIsolate()), 0,
                                       nullptr, &callback_helper);
  RunPendingTasks();
  EXPECT_FALSE(callback_helper.DidComplete());

  web_view_helper.GetWebView()->GetPage()->SetPaused(false);
  RunPendingTasks();
  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_EQ("hello", callback_helper.StringValue());
}

TEST_F(WebFrameTest, RequestExecuteV8FunctionWhileSuspendedWithUserGesture) {
  DisableRendererSchedulerThrottling();
  RegisterMockedHttpURLLoad("foo.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");

  v8::HandleScope scope(v8::Isolate::GetCurrent());

  // Suspend scheduled tasks so the script doesn't run.
  web_view_helper.GetWebView()->GetPage()->SetPaused(true);
  LocalFrame::NotifyUserActivation(
      web_view_helper.LocalMainFrame()->GetFrame(),
      mojom::UserActivationNotificationType::kTest);
  ScriptExecutionCallbackHelper callback_helper(
      web_view_helper.LocalMainFrame()->MainWorldScriptContext());
  WebScriptSource script_source("navigator.userActivation.isActive;");
  web_view_helper.GetWebView()
      ->MainFrameImpl()
      ->RequestExecuteScriptAndReturnValue(script_source, false,
                                           &callback_helper);
  RunPendingTasks();
  EXPECT_FALSE(callback_helper.DidComplete());

  web_view_helper.GetWebView()->GetPage()->SetPaused(false);
  RunPendingTasks();
  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_EQ(true, callback_helper.BoolValue());
}

TEST_F(WebFrameTest, IframeScriptRemovesSelf) {
  RegisterMockedHttpURLLoad("single_iframe.html");
  RegisterMockedHttpURLLoad("visible_iframe.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "single_iframe.html");

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  ScriptExecutionCallbackHelper callback_helper(
      web_view_helper.LocalMainFrame()->MainWorldScriptContext());
  web_view_helper.GetWebView()
      ->MainFrame()
      ->FirstChild()
      ->ToWebLocalFrame()
      ->RequestExecuteScriptAndReturnValue(
          WebScriptSource(WebString(
              "var iframe = "
              "window.top.document.getElementsByTagName('iframe')[0]; "
              "window.top.document.body.removeChild(iframe); 'hello';")),
          false, &callback_helper);
  RunPendingTasks();
  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_EQ(String(), callback_helper.StringValue());
}

TEST_F(WebFrameTest, FormWithNullFrame) {
  RegisterMockedHttpURLLoad("form.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "form.html");

  WebVector<WebFormElement> forms =
      web_view_helper.LocalMainFrame()->GetDocument().Forms();
  web_view_helper.Reset();

  EXPECT_EQ(forms.size(), 1U);

  // This test passes if this doesn't crash.
  WebSearchableFormData searchable_data_form(forms[0]);
}

TEST_F(WebFrameTest, ChromePageJavascript) {
  RegisterMockedChromeURLLoad("history.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(chrome_url_ + "history.html");

  // Try to run JS against the chrome-style URL.
  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                "javascript:document.body.appendChild(document."
                                "createTextNode('Clobbered'))");

  // Now retrieve the frame's text and ensure it was modified by running
  // javascript.
  std::string content = WebFrameContentDumper::DumpWebViewAsText(
                            web_view_helper.GetWebView(), 1024)
                            .Utf8();
  EXPECT_NE(std::string::npos, content.find("Clobbered"));
}

TEST_F(WebFrameTest, ChromePageNoJavascript) {
  RegisterMockedChromeURLLoad("history.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(chrome_url_ + "history.html");

  // Try to run JS against the chrome-style URL after prohibiting it.
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs("chrome");
  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                "javascript:document.body.appendChild(document."
                                "createTextNode('Clobbered'))");

  // Now retrieve the frame's text and ensure it wasn't modified by running
  // javascript.
  std::string content = WebFrameContentDumper::DumpWebViewAsText(
                            web_view_helper.GetWebView(), 1024)
                            .Utf8();
  EXPECT_EQ(std::string::npos, content.find("Clobbered"));
}

TEST_F(WebFrameTest, LocationSetHostWithMissingPort) {
  std::string file_name = "print-location-href.html";
  RegisterMockedHttpURLLoad(file_name);
  // TODO(crbug.com/751425): We should use the mock functionality
  // via the WebViewHelper instance in each test case.
  RegisterMockedURLLoadFromBase("http://internal.test:0/", file_name);

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + file_name);

  // Setting host to "hostname:" should be treated as "hostname:0".
  frame_test_helpers::LoadFrame(
      web_view_helper.GetWebView()->MainFrameImpl(),
      "javascript:location.host = 'internal.test:'; void 0;");

  frame_test_helpers::LoadFrame(
      web_view_helper.GetWebView()->MainFrameImpl(),
      "javascript:document.body.textContent = location.href; void 0;");

  std::string content = WebFrameContentDumper::DumpWebViewAsText(
                            web_view_helper.GetWebView(), 1024)
                            .Utf8();
  EXPECT_EQ("http://internal.test/" + file_name, content);
}

TEST_F(WebFrameTest, LocationSetEmptyPort) {
  std::string file_name = "print-location-href.html";
  RegisterMockedHttpURLLoad(file_name);
  // TODO(crbug.com/751425): We should use the mock functionality
  // via the WebViewHelper instance in each test case.
  RegisterMockedURLLoadFromBase("http://internal.test:0/", file_name);

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + file_name);

  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                "javascript:location.port = ''; void 0;");

  frame_test_helpers::LoadFrame(
      web_view_helper.GetWebView()->MainFrameImpl(),
      "javascript:document.body.textContent = location.href; void 0;");

  std::string content = WebFrameContentDumper::DumpWebViewAsText(
                            web_view_helper.GetWebView(), 1024)
                            .Utf8();
  EXPECT_EQ("http://internal.test/" + file_name, content);
}

class EvaluateOnLoadWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  EvaluateOnLoadWebFrameClient() : executing_(false), was_executed_(false) {}
  ~EvaluateOnLoadWebFrameClient() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void DidClearWindowObject() override {
    EXPECT_FALSE(executing_);
    was_executed_ = true;
    executing_ = true;
    v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
    Frame()->ExecuteScriptAndReturnValue(
        WebScriptSource(WebString("window.someProperty = 42;")));
    executing_ = false;
  }

  bool executing_;
  bool was_executed_;
};

TEST_F(WebFrameTest, DidClearWindowObjectIsNotRecursive) {
  EvaluateOnLoadWebFrameClient web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank", &web_frame_client);
  EXPECT_TRUE(web_frame_client.was_executed_);
}

class CSSCallbackWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  CSSCallbackWebFrameClient() : update_count_(0) {}
  ~CSSCallbackWebFrameClient() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void DidMatchCSS(
      const WebVector<WebString>& newly_matching_selectors,
      const WebVector<WebString>& stopped_matching_selectors) override;

  HashSet<String>& MatchedSelectors() {
    auto it = matched_selectors_.find(Frame());
    if (it != matched_selectors_.end())
      return it->value;

    auto add_result = matched_selectors_.insert(Frame(), HashSet<String>());
    return add_result.stored_value->value;
  }

  HashMap<WebLocalFrame*, HashSet<String>> matched_selectors_;
  int update_count_;
};

void CSSCallbackWebFrameClient::DidMatchCSS(
    const WebVector<WebString>& newly_matching_selectors,
    const WebVector<WebString>& stopped_matching_selectors) {
  ++update_count_;

  HashSet<String>& frame_selectors = MatchedSelectors();
  for (size_t i = 0; i < newly_matching_selectors.size(); ++i) {
    String selector = newly_matching_selectors[i];
    EXPECT_TRUE(frame_selectors.find(selector) == frame_selectors.end())
        << selector;
    frame_selectors.insert(selector);
  }
  for (size_t i = 0; i < stopped_matching_selectors.size(); ++i) {
    String selector = stopped_matching_selectors[i];
    EXPECT_TRUE(frame_selectors.find(selector) != frame_selectors.end())
        << selector;
    frame_selectors.erase(selector);
    EXPECT_TRUE(frame_selectors.find(selector) == frame_selectors.end())
        << selector;
  }
}

class WebFrameCSSCallbackTest : public testing::Test {
 protected:
  WebFrameCSSCallbackTest() {
    frame_ = helper_.InitializeAndLoad("about:blank", &client_)
                 ->MainFrame()
                 ->ToWebLocalFrame();
  }

  ~WebFrameCSSCallbackTest() override {
    EXPECT_EQ(1U, client_.matched_selectors_.size());
  }

  WebDocument Doc() const { return frame_->GetDocument(); }

  int UpdateCount() const { return client_.update_count_; }

  const HashSet<String>& MatchedSelectors() {
    auto it = client_.matched_selectors_.find(frame_);
    if (it != client_.matched_selectors_.end())
      return it->value;

    auto add_result =
        client_.matched_selectors_.insert(frame_, HashSet<String>());
    return add_result.stored_value->value;
  }

  void LoadHTML(const std::string& html) {
    frame_test_helpers::LoadHTMLString(frame_, html, ToKURL("about:blank"));
  }

  void ExecuteScript(const WebString& code) {
    frame_->ExecuteScript(WebScriptSource(code));
    frame_->View()->MainFrameWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
    RunPendingTasks();
  }

  CSSCallbackWebFrameClient client_;
  frame_test_helpers::WebViewHelper helper_;
  WebLocalFrame* frame_;
};

TEST_F(WebFrameCSSCallbackTest, AuthorStyleSheet) {
  LoadHTML(
      "<style>"
      // This stylesheet checks that the internal property and value can't be
      // set by a stylesheet, only WebDocument::watchCSSSelectors().
      "div.initial_on { -internal-callback: none; }"
      "div.initial_off { -internal-callback: -internal-presence; }"
      "</style>"
      "<div class=\"initial_on\"></div>"
      "<div class=\"initial_off\"></div>");

  Vector<WebString> selectors;
  selectors.push_back(WebString::FromUTF8("div.initial_on"));
  frame_->GetDocument().WatchCSSSelectors(WebVector<WebString>(selectors));
  frame_->View()->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);
  RunPendingTasks();
  EXPECT_EQ(1, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre("div.initial_on"));

  // Check that adding a watched selector calls back for already-present nodes.
  selectors.push_back(WebString::FromUTF8("div.initial_off"));
  Doc().WatchCSSSelectors(WebVector<WebString>(selectors));
  frame_->View()->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);
  RunPendingTasks();
  EXPECT_EQ(2, UpdateCount());
  EXPECT_THAT(MatchedSelectors(),
              ElementsAre("div.initial_off", "div.initial_on"));

  // Check that we can turn off callbacks for certain selectors.
  Doc().WatchCSSSelectors(WebVector<WebString>());
  frame_->View()->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);
  RunPendingTasks();
  EXPECT_EQ(3, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre());
}

TEST_F(WebFrameCSSCallbackTest, SharedComputedStyle) {
  // Check that adding an element calls back when it matches an existing rule.
  Vector<WebString> selectors;
  selectors.push_back(WebString::FromUTF8("span"));
  Doc().WatchCSSSelectors(WebVector<WebString>(selectors));

  ExecuteScript(
      "i1 = document.createElement('span');"
      "i1.id = 'first_span';"
      "document.body.appendChild(i1)");
  EXPECT_EQ(1, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span"));

  // Adding a second element that shares a ComputedStyle shouldn't call back.
  // We use <span>s to avoid default style rules that can set
  // ComputedStyle::unique().
  ExecuteScript(
      "i2 = document.createElement('span');"
      "i2.id = 'second_span';"
      "i1 = document.getElementById('first_span');"
      "i1.parentNode.insertBefore(i2, i1.nextSibling);");
  EXPECT_EQ(1, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span"));

  // Removing the first element shouldn't call back.
  ExecuteScript(
      "i1 = document.getElementById('first_span');"
      "i1.parentNode.removeChild(i1);");
  EXPECT_EQ(1, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span"));

  // But removing the second element *should* call back.
  ExecuteScript(
      "i2 = document.getElementById('second_span');"
      "i2.parentNode.removeChild(i2);");
  EXPECT_EQ(2, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre());
}

TEST_F(WebFrameCSSCallbackTest, CatchesAttributeChange) {
  LoadHTML("<span></span>");

  Vector<WebString> selectors;
  selectors.push_back(WebString::FromUTF8("span[attr=\"value\"]"));
  Doc().WatchCSSSelectors(WebVector<WebString>(selectors));
  RunPendingTasks();

  EXPECT_EQ(0, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre());

  ExecuteScript(
      "document.querySelector('span').setAttribute('attr', 'value');");
  EXPECT_EQ(1, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span[attr=\"value\"]"));
}

TEST_F(WebFrameCSSCallbackTest, DisplayNone) {
  LoadHTML("<div style='display:none'><span></span></div>");

  Vector<WebString> selectors;
  selectors.push_back(WebString::FromUTF8("span"));
  Doc().WatchCSSSelectors(WebVector<WebString>(selectors));
  RunPendingTasks();

  EXPECT_EQ(0, UpdateCount()) << "Don't match elements in display:none trees.";

  ExecuteScript(
      "d = document.querySelector('div');"
      "d.style.display = 'block';");
  EXPECT_EQ(1, UpdateCount()) << "Match elements when they become displayed.";
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span"));

  ExecuteScript(
      "d = document.querySelector('div');"
      "d.style.display = 'none';");
  EXPECT_EQ(2, UpdateCount())
      << "Unmatch elements when they become undisplayed.";
  EXPECT_THAT(MatchedSelectors(), ElementsAre());

  ExecuteScript(
      "s = document.querySelector('span');"
      "s.style.display = 'none';");
  EXPECT_EQ(2, UpdateCount())
      << "No effect from no-display'ing a span that's already undisplayed.";

  ExecuteScript(
      "d = document.querySelector('div');"
      "d.style.display = 'block';");
  EXPECT_EQ(2, UpdateCount())
      << "No effect from displaying a div whose span is display:none.";

  ExecuteScript(
      "s = document.querySelector('span');"
      "s.style.display = 'inline';");
  EXPECT_EQ(3, UpdateCount())
      << "Now the span is visible and produces a callback.";
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span"));

  ExecuteScript(
      "s = document.querySelector('span');"
      "s.style.display = 'none';");
  EXPECT_EQ(4, UpdateCount())
      << "Undisplaying the span directly should produce another callback.";
  EXPECT_THAT(MatchedSelectors(), ElementsAre());
}

TEST_F(WebFrameCSSCallbackTest, DisplayContents) {
  LoadHTML("<div style='display:contents'><span></span></div>");

  Vector<WebString> selectors(1u, WebString::FromUTF8("span"));
  Doc().WatchCSSSelectors(WebVector<WebString>(selectors));
  frame_->View()->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);
  RunPendingTasks();

  EXPECT_EQ(1, UpdateCount()) << "Match elements in display:contents trees.";
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span"));

  ExecuteScript(
      "s = document.querySelector('span');"
      "s.style.display = 'contents';");
  EXPECT_EQ(1, UpdateCount()) << "Match elements which are display:contents.";
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span"));

  ExecuteScript(
      "d = document.querySelector('div');"
      "d.style.display = 'block';");
  EXPECT_EQ(1, UpdateCount())
      << "Still match display:contents after parent becomes display:block.";
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span"));

  ExecuteScript(
      "d = document.querySelector('div');"
      "d.style.display = 'none';");
  EXPECT_EQ(2, UpdateCount())
      << "No longer matched when parent becomes display:none.";
  EXPECT_THAT(MatchedSelectors(), ElementsAre());
}

TEST_F(WebFrameCSSCallbackTest, Reparenting) {
  LoadHTML(
      "<div id='d1'><span></span></div>"
      "<div id='d2'></div>");

  Vector<WebString> selectors;
  selectors.push_back(WebString::FromUTF8("span"));
  Doc().WatchCSSSelectors(WebVector<WebString>(selectors));
  frame_->View()->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);
  RunPendingTasks();

  EXPECT_EQ(1, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span"));

  ExecuteScript(
      "s = document.querySelector('span');"
      "d2 = document.getElementById('d2');"
      "d2.appendChild(s);");
  EXPECT_EQ(1, UpdateCount()) << "Just moving an element that continues to "
                                 "match shouldn't send a spurious callback.";
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span"));
}

TEST_F(WebFrameCSSCallbackTest, MultiSelector) {
  LoadHTML("<span></span>");

  // Check that selector lists match as the whole list, not as each element
  // independently.
  Vector<WebString> selectors;
  selectors.push_back(WebString::FromUTF8("span"));
  selectors.push_back(WebString::FromUTF8("span,p"));
  Doc().WatchCSSSelectors(WebVector<WebString>(selectors));
  frame_->View()->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);
  RunPendingTasks();

  EXPECT_EQ(1, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span", "span, p"));
}

TEST_F(WebFrameCSSCallbackTest, InvalidSelector) {
  LoadHTML("<p><span></span></p>");

  // Build a list with one valid selector and one invalid.
  Vector<WebString> selectors;
  selectors.push_back(WebString::FromUTF8("span"));
  selectors.push_back(WebString::FromUTF8("["));       // Invalid.
  selectors.push_back(WebString::FromUTF8("p span"));  // Not compound.
  Doc().WatchCSSSelectors(WebVector<WebString>(selectors));
  frame_->View()->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);
  RunPendingTasks();

  EXPECT_EQ(1, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span"))
      << "An invalid selector shouldn't prevent other selectors from matching.";
}

TEST_F(WebFrameTest, PostMessageEvent) {
  RegisterMockedHttpURLLoad("postmessage_test.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "postmessage_test.html");

  auto* frame =
      To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame());

  scoped_refptr<SerializedScriptValue> data = SerializedScriptValue::Create();
  MessageEvent* message_event = MessageEvent::Create(
      /*ports=*/nullptr, std::move(data), "http://origin.com");

  // Send a message with the correct origin.
  scoped_refptr<SecurityOrigin> correct_origin =
      SecurityOrigin::Create(ToKURL(base_url_));
  frame->PostMessageEvent(
      base::nullopt, g_empty_string, correct_origin->ToString(),
      BlinkTransferableMessage::FromMessageEvent(message_event));

  // Send another message with incorrect origin.
  scoped_refptr<SecurityOrigin> incorrect_origin =
      SecurityOrigin::Create(ToKURL(chrome_url_));
  frame->PostMessageEvent(
      base::nullopt, g_empty_string, incorrect_origin->ToString(),
      BlinkTransferableMessage::FromMessageEvent(message_event));

  // Verify that only the first addition is in the body of the page.
  std::string content = WebFrameContentDumper::DumpWebViewAsText(
                            web_view_helper.GetWebView(), 1024)
                            .Utf8();
  EXPECT_NE(std::string::npos, content.find("Message 1."));
  EXPECT_EQ(std::string::npos, content.find("Message 2."));
}

namespace {

scoped_refptr<SerializedScriptValue> SerializeString(
    const StringView& message,
    ScriptState* script_state) {
  // This is inefficient, but avoids duplicating serialization logic for the
  // sake of this test.
  NonThrowableExceptionState exception_state;
  ScriptState::Scope scope(script_state);
  V8ScriptValueSerializer serializer(script_state);
  return serializer.Serialize(V8String(script_state->GetIsolate(), message),
                              exception_state);
}

}  // namespace

TEST_F(WebFrameTest, PostMessageThenDetach) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank");

  auto* frame =
      To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame());
  NonThrowableExceptionState exception_state;
  scoped_refptr<SerializedScriptValue> message =
      SerializeString("message", ToScriptStateForMainWorld(frame));
  MessagePortArray message_ports;
  frame->DomWindow()->PostMessageForTesting(
      message, message_ports, "*", frame->DomWindow(), exception_state);
  web_view_helper.Reset();
  EXPECT_FALSE(exception_state.HadException());

  // Success is not crashing.
  RunPendingTasks();
}

namespace {

class FixedLayoutTestWebWidgetClient
    : public frame_test_helpers::TestWebWidgetClient {
 public:
  FixedLayoutTestWebWidgetClient() = default;
  ~FixedLayoutTestWebWidgetClient() override = default;

  // frame_test_helpers::TestWebWidgetClient:
  ScreenInfo GetInitialScreenInfo() override { return screen_info_; }

  ScreenInfo screen_info_;
};

// Helper function to set autosizing multipliers on a document.
bool SetTextAutosizingMultiplier(Document* document, float multiplier) {
  bool multiplier_set = false;
  for (LayoutObject* layout_object = document->GetLayoutView(); layout_object;
       layout_object = layout_object->NextInPreOrder()) {
    if (layout_object->Style()) {
      scoped_refptr<ComputedStyle> modified_style =
          ComputedStyle::Clone(layout_object->StyleRef());
      modified_style->SetTextAutosizingMultiplier(multiplier);
      EXPECT_EQ(multiplier, modified_style->TextAutosizingMultiplier());
      layout_object->SetModifiedStyleOutsideStyleRecalc(
          std::move(modified_style), LayoutObject::ApplyStyleChanges::kNo);
      multiplier_set = true;
    }
  }
  return multiplier_set;
}

// Helper function to check autosizing multipliers on a document.
bool CheckTextAutosizingMultiplier(Document* document, float multiplier) {
  bool multiplier_checked = false;
  for (LayoutObject* layout_object = document->GetLayoutView(); layout_object;
       layout_object = layout_object->NextInPreOrder()) {
    if (layout_object->Style() && layout_object->IsText()) {
      EXPECT_EQ(multiplier, layout_object->Style()->TextAutosizingMultiplier());
      multiplier_checked = true;
    }
  }
  return multiplier_checked;
}

void UpdateScreenInfoAndResizeView(
    FixedLayoutTestWebWidgetClient* client,
    frame_test_helpers::WebViewHelper* web_view_helper,
    int viewport_width,
    int viewport_height) {
  client->screen_info_.rect = gfx::Rect(viewport_width, viewport_height);
  web_view_helper->GetWebView()->MainFrameViewWidget()->UpdateScreenInfo(
      client->screen_info_);
  web_view_helper->Resize(gfx::Size(viewport_width, viewport_height));
}

}  // namespace

class WebFixedLayoutFrameTest : public WebFrameTest {
 protected:
  FixedLayoutTestWebWidgetClient widget_client_;
};

TEST_F(WebFrameTest, ChangeInFixedLayoutResetsTextAutosizingMultipliers) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebWidgetClient client;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fixed_layout.html", nullptr,
                                    nullptr, &client, ConfigureAndroid);

  Document* document =
      To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame())
          ->GetDocument();
  document->GetSettings()->SetTextAutosizingEnabled(true);
  EXPECT_TRUE(document->GetSettings()->TextAutosizingEnabled());
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_TRUE(SetTextAutosizingMultiplier(document, 2));

  ViewportDescription description =
      document->GetViewportData().GetViewportDescription();
  // Choose a width that's not going match the viewport width of the loaded
  // document.
  description.min_width = Length::Fixed(100);
  description.max_width = Length::Fixed(100);
  web_view_helper.GetWebView()->UpdatePageDefinedViewportConstraints(
      description);

  EXPECT_TRUE(CheckTextAutosizingMultiplier(document, 1));
}

TEST_F(WebFrameTest, WorkingTextAutosizingMultipliers_VirtualViewport) {
  const std::string html_file = "fixed_layout.html";
  RegisterMockedHttpURLLoad(html_file);

  FixedLayoutTestWebWidgetClient client;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + html_file, nullptr, nullptr,
                                    &client, ConfigureAndroid);

  Document* document =
      To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame())
          ->GetDocument();
  document->GetSettings()->SetTextAutosizingEnabled(true);
  EXPECT_TRUE(document->GetSettings()->TextAutosizingEnabled());

  web_view_helper.Resize(gfx::Size(490, 800));

  // Multiplier: 980 / 490 = 2.0
  EXPECT_TRUE(CheckTextAutosizingMultiplier(document, 2.0));
}

TEST_F(WebFrameTest,
       VisualViewportSetSizeInvalidatesTextAutosizingMultipliers) {
  RegisterMockedHttpURLLoad("iframe_reload.html");
  RegisterMockedHttpURLLoad("visible_iframe.html");

  FixedLayoutTestWebWidgetClient client;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "iframe_reload.html", nullptr,
                                    nullptr, &client, ConfigureAndroid);

  auto* main_frame =
      To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame());
  Document* document = main_frame->GetDocument();
  LocalFrameView* frame_view = web_view_helper.LocalMainFrame()->GetFrameView();
  document->GetSettings()->SetTextAutosizingEnabled(true);
  EXPECT_TRUE(document->GetSettings()->TextAutosizingEnabled());
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  for (Frame* frame = main_frame; frame; frame = frame->Tree().TraverseNext()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;
    EXPECT_TRUE(SetTextAutosizingMultiplier(local_frame->GetDocument(), 2));
    for (LayoutObject* layout_object =
             local_frame->GetDocument()->GetLayoutView();
         layout_object; layout_object = layout_object->NextInPreOrder()) {
      if (layout_object->IsText())
        EXPECT_FALSE(layout_object->NeedsLayout());
    }
  }

  frame_view->GetPage()->GetVisualViewport().SetSize(IntSize(200, 200));

  for (Frame* frame = main_frame; frame; frame = frame->Tree().TraverseNext()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;
    for (LayoutObject* layout_object =
             local_frame->GetDocument()->GetLayoutView();
         !layout_object; layout_object = layout_object->NextInPreOrder()) {
      if (layout_object->IsText())
        EXPECT_TRUE(layout_object->NeedsLayout());
    }
  }
}

TEST_F(WebFrameTest, ZeroHeightPositiveWidthNotIgnored) {
  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 1280;
  int viewport_height = 0;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr, &client, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_EQ(viewport_width, web_view_helper.GetWebView()
                                ->MainFrameImpl()
                                ->GetFrameView()
                                ->GetLayoutSize()
                                .Width());
  EXPECT_EQ(viewport_height, web_view_helper.GetWebView()
                                 ->MainFrameImpl()
                                 ->GetFrameView()
                                 ->GetLayoutSize()
                                 .Height());
}

TEST_F(WebFrameTest, DeviceScaleFactorUsesDefaultWithoutViewportTag) {
  RegisterMockedHttpURLLoad("no_viewport_tag.html");

  int viewport_width = 640;
  int viewport_height = 480;

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 2;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "no_viewport_tag.html", nullptr,
                                    nullptr, &client, ConfigureAndroid);

  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_EQ(
      2,
      web_view_helper.GetWebView()->GetPage()->DeviceScaleFactorDeprecated());

  // Device scale factor should be independent of page scale.
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(1, 2);
  web_view_helper.GetWebView()->SetPageScaleFactor(0.5);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  EXPECT_EQ(1, web_view_helper.GetWebView()->PageScaleFactor());

  // Force the layout to happen before leaving the test.
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
}

TEST_F(WebFrameTest, FixedLayoutInitializeAtMinimumScale) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr, &client, ConfigureAndroid);
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.25f, 5);
  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                base_url_ + "fixed_layout.html");
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  int default_fixed_layout_width = 980;
  float minimum_page_scale_factor =
      viewport_width / (float)default_fixed_layout_width;
  EXPECT_EQ(minimum_page_scale_factor,
            web_view_helper.GetWebView()->PageScaleFactor());
  EXPECT_EQ(minimum_page_scale_factor,
            web_view_helper.GetWebView()->MinimumPageScaleFactor());

  // Assume the user has pinch zoomed to page scale factor 2.
  float user_pinch_page_scale_factor = 2;
  web_view_helper.GetWebView()->SetPageScaleFactor(
      user_pinch_page_scale_factor);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  // Make sure we don't reset to initial scale if the page continues to load.
  web_view_helper.GetWebView()->DidCommitLoad(false, false);
  web_view_helper.GetWebView()->DidChangeContentsSize();
  EXPECT_EQ(user_pinch_page_scale_factor,
            web_view_helper.GetWebView()->PageScaleFactor());

  // Make sure we don't reset to initial scale if the viewport size changes.
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height + 100));
  EXPECT_EQ(user_pinch_page_scale_factor,
            web_view_helper.GetWebView()->PageScaleFactor());
}

TEST_F(WebFrameTest, WideDocumentInitializeAtMinimumScale) {
  RegisterMockedHttpURLLoad("wide_document.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr, &client, ConfigureAndroid);
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.25f, 5);
  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                base_url_ + "wide_document.html");
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  int wide_document_width = 1500;
  float minimum_page_scale_factor = viewport_width / (float)wide_document_width;
  EXPECT_EQ(minimum_page_scale_factor,
            web_view_helper.GetWebView()->PageScaleFactor());
  EXPECT_EQ(minimum_page_scale_factor,
            web_view_helper.GetWebView()->MinimumPageScaleFactor());

  // Assume the user has pinch zoomed to page scale factor 2.
  float user_pinch_page_scale_factor = 2;
  web_view_helper.GetWebView()->SetPageScaleFactor(
      user_pinch_page_scale_factor);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  // Make sure we don't reset to initial scale if the page continues to load.
  web_view_helper.GetWebView()->DidCommitLoad(false, false);
  web_view_helper.GetWebView()->DidChangeContentsSize();
  EXPECT_EQ(user_pinch_page_scale_factor,
            web_view_helper.GetWebView()->PageScaleFactor());

  // Make sure we don't reset to initial scale if the viewport size changes.
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height + 100));
  EXPECT_EQ(user_pinch_page_scale_factor,
            web_view_helper.GetWebView()->PageScaleFactor());
}

TEST_F(WebFrameTest, DelayedViewportInitialScale) {
  RegisterMockedHttpURLLoad("viewport-auto-initial-scale.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-auto-initial-scale.html", nullptr, nullptr, &client,
      ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_EQ(0.25f, web_view_helper.GetWebView()->PageScaleFactor());

  ViewportData& viewport =
      To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame())
          ->GetDocument()
          ->GetViewportData();
  ViewportDescription description = viewport.GetViewportDescription();
  description.zoom = 2;
  viewport.SetViewportDescription(description);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  EXPECT_EQ(2, web_view_helper.GetWebView()->PageScaleFactor());
}

TEST_F(WebFrameTest, setLoadWithOverviewModeToFalse) {
  RegisterMockedHttpURLLoad("viewport-auto-initial-scale.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-auto-initial-scale.html", nullptr, nullptr, &client,
      ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetLoadWithOverviewMode(false);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  // The page must be displayed at 100% zoom.
  EXPECT_EQ(1.0f, web_view_helper.GetWebView()->PageScaleFactor());
}

TEST_F(WebFrameTest, SetLoadWithOverviewModeToFalseAndNoWideViewport) {
  RegisterMockedHttpURLLoad("large-div.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "large-div.html", nullptr,
                                    nullptr, &client, ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetLoadWithOverviewMode(false);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  // The page must be displayed at 100% zoom, despite that it hosts a wide div
  // element.
  EXPECT_EQ(1.0f, web_view_helper.GetWebView()->PageScaleFactor());
}

TEST_F(WebFrameTest, NoWideViewportIgnoresPageViewportWidth) {
  RegisterMockedHttpURLLoad("viewport-auto-initial-scale.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-auto-initial-scale.html", nullptr, nullptr, &client,
      ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  // The page sets viewport width to 3000, but with UseWideViewport == false is
  // must be ignored.
  EXPECT_EQ(viewport_width, web_view_helper.GetWebView()
                                ->MainFrameImpl()
                                ->GetFrameView()
                                ->Size()
                                .Width());
  EXPECT_EQ(viewport_height, web_view_helper.GetWebView()
                                 ->MainFrameImpl()
                                 ->GetFrameView()
                                 ->Size()
                                 .Height());
}

TEST_F(WebFrameTest, NoWideViewportIgnoresPageViewportWidthButAccountsScale) {
  RegisterMockedHttpURLLoad("viewport-wide-2x-initial-scale.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-wide-2x-initial-scale.html", nullptr, nullptr,
      &client, ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  // The page sets viewport width to 3000, but with UseWideViewport == false it
  // must be ignored while the initial scale specified by the page must be
  // accounted.
  EXPECT_EQ(viewport_width / 2, web_view_helper.GetWebView()
                                    ->MainFrameImpl()
                                    ->GetFrameView()
                                    ->Size()
                                    .Width());
  EXPECT_EQ(viewport_height / 2, web_view_helper.GetWebView()
                                     ->MainFrameImpl()
                                     ->GetFrameView()
                                     ->Size()
                                     .Height());
}

TEST_F(WebFrameTest, WideViewportSetsTo980WithoutViewportTag) {
  RegisterMockedHttpURLLoad("no_viewport_tag.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "no_viewport_tag.html", nullptr,
                                    nullptr, &client, ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(true);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_EQ(980, web_view_helper.GetWebView()
                     ->MainFrameImpl()
                     ->GetFrameView()
                     ->LayoutViewport()
                     ->ContentsSize()
                     .Width());
  EXPECT_EQ(980.0 / viewport_width * viewport_height,
            web_view_helper.GetWebView()
                ->MainFrameImpl()
                ->GetFrameView()
                ->LayoutViewport()
                ->ContentsSize()
                .Height());
}

TEST_F(WebFrameTest, WideViewportSetsTo980WithXhtmlMp) {
  RegisterMockedHttpURLLoad("viewport/viewport-legacy-xhtmlmp.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr, &client, ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(true);
  frame_test_helpers::LoadFrame(
      web_view_helper.GetWebView()->MainFrameImpl(),
      base_url_ + "viewport/viewport-legacy-xhtmlmp.html");

  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  EXPECT_EQ(viewport_width, web_view_helper.GetWebView()
                                ->MainFrameImpl()
                                ->GetFrameView()
                                ->Size()
                                .Width());
  EXPECT_EQ(viewport_height, web_view_helper.GetWebView()
                                 ->MainFrameImpl()
                                 ->GetFrameView()
                                 ->Size()
                                 .Height());
}

TEST_F(WebFrameTest, NoWideViewportAndHeightInMeta) {
  RegisterMockedHttpURLLoad("viewport-height-1000.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport-height-1000.html",
                                    nullptr, nullptr, &client,
                                    ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_EQ(viewport_width, web_view_helper.GetWebView()
                                ->MainFrameImpl()
                                ->GetFrameView()
                                ->Size()
                                .Width());
}

TEST_F(WebFrameTest, WideViewportSetsTo980WithAutoWidth) {
  RegisterMockedHttpURLLoad("viewport-2x-initial-scale.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-2x-initial-scale.html", nullptr, nullptr, &client,
      ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(true);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_EQ(980, web_view_helper.GetWebView()
                     ->MainFrameImpl()
                     ->GetFrameView()
                     ->Size()
                     .Width());
  EXPECT_EQ(980.0 / viewport_width * viewport_height,
            web_view_helper.GetWebView()
                ->MainFrameImpl()
                ->GetFrameView()
                ->Size()
                .Height());
}

TEST_F(WebFrameTest, PageViewportInitialScaleOverridesLoadWithOverviewMode) {
  RegisterMockedHttpURLLoad("viewport-wide-2x-initial-scale.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-wide-2x-initial-scale.html", nullptr, nullptr,
      &client, ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetLoadWithOverviewMode(false);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  // The page must be displayed at 200% zoom, as specified in its viewport meta
  // tag.
  EXPECT_EQ(2.0f, web_view_helper.GetWebView()->PageScaleFactor());
}

TEST_F(WebFrameTest, setInitialPageScaleFactorPermanently) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  float enforced_page_scale_factor = 2.0f;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fixed_layout.html", nullptr,
                                    nullptr, &client, ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetLoadWithOverviewMode(false);
  web_view_helper.GetWebView()->SetInitialPageScaleOverride(
      enforced_page_scale_factor);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  EXPECT_EQ(enforced_page_scale_factor,
            web_view_helper.GetWebView()->PageScaleFactor());

  int viewport_width = 640;
  int viewport_height = 480;
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_EQ(enforced_page_scale_factor,
            web_view_helper.GetWebView()->PageScaleFactor());

  web_view_helper.GetWebView()->SetInitialPageScaleOverride(-1);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  EXPECT_EQ(1.0, web_view_helper.GetWebView()->PageScaleFactor());
}

TEST_F(WebFrameTest,
       PermanentInitialPageScaleFactorOverridesLoadWithOverviewMode) {
  RegisterMockedHttpURLLoad("viewport-auto-initial-scale.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;
  float enforced_page_scale_factor = 0.5f;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-auto-initial-scale.html", nullptr, nullptr, &client,
      ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetLoadWithOverviewMode(false);
  web_view_helper.GetWebView()->SetInitialPageScaleOverride(
      enforced_page_scale_factor);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_EQ(enforced_page_scale_factor,
            web_view_helper.GetWebView()->PageScaleFactor());
}

TEST_F(WebFrameTest,
       PermanentInitialPageScaleFactorOverridesPageViewportInitialScale) {
  RegisterMockedHttpURLLoad("viewport-wide-2x-initial-scale.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;
  float enforced_page_scale_factor = 0.5f;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-wide-2x-initial-scale.html", nullptr, nullptr,
      &client, ConfigureAndroid);
  web_view_helper.GetWebView()->SetInitialPageScaleOverride(
      enforced_page_scale_factor);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_EQ(enforced_page_scale_factor,
            web_view_helper.GetWebView()->PageScaleFactor());
}

TEST_F(WebFrameTest, SmallPermanentInitialPageScaleFactorIsClobbered) {
  const char* pages[] = {
      // These pages trigger the clobbering condition. There must be a matching
      // item in "pageScaleFactors" array.
      "viewport-device-0.5x-initial-scale.html",
      "viewport-initial-scale-1.html",
      // These ones do not.
      "viewport-auto-initial-scale.html",
      "viewport-target-densitydpi-device-and-fixed-width.html"};
  float page_scale_factors[] = {0.5f, 1.0f};
  for (size_t i = 0; i < base::size(pages); ++i)
    RegisterMockedHttpURLLoad(pages[i]);

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 400;
  int viewport_height = 300;
  float enforced_page_scale_factor = 0.75f;

  for (size_t i = 0; i < base::size(pages); ++i) {
    for (int quirk_enabled = 0; quirk_enabled <= 1; ++quirk_enabled) {
      frame_test_helpers::WebViewHelper web_view_helper;
      web_view_helper.InitializeAndLoad(base_url_ + pages[i], nullptr, nullptr,
                                        &client, ConfigureAndroid);
      web_view_helper.GetWebView()
          ->GetSettings()
          ->SetClobberUserAgentInitialScaleQuirk(quirk_enabled);
      web_view_helper.GetWebView()->SetInitialPageScaleOverride(
          enforced_page_scale_factor);
      web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

      float expected_page_scale_factor =
          quirk_enabled && i < base::size(page_scale_factors)
              ? page_scale_factors[i]
              : enforced_page_scale_factor;
      EXPECT_EQ(expected_page_scale_factor,
                web_view_helper.GetWebView()->PageScaleFactor());
    }
  }
}

TEST_F(WebFrameTest, PermanentInitialPageScaleFactorAffectsLayoutWidth) {
  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;
  float enforced_page_scale_factor = 0.5;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank", nullptr, nullptr, &client,
                                    ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.GetWebView()->GetSettings()->SetLoadWithOverviewMode(false);
  web_view_helper.GetWebView()->SetInitialPageScaleOverride(
      enforced_page_scale_factor);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_EQ(viewport_width / enforced_page_scale_factor,
            web_view_helper.GetWebView()
                ->MainFrameImpl()
                ->GetFrameView()
                ->Size()
                .Width());
  EXPECT_EQ(enforced_page_scale_factor,
            web_view_helper.GetWebView()->PageScaleFactor());
}

TEST_F(WebFrameTest, DocumentElementClientHeightWorksWithWrapContentMode) {
  RegisterMockedHttpURLLoad("0-by-0.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "0-by-0.html", nullptr, nullptr,
                                    &client, ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetForceZeroLayoutHeight(true);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  LocalFrame* frame = web_view_helper.LocalMainFrame()->GetFrame();
  Document* document = frame->GetDocument();
  EXPECT_EQ(viewport_height, document->documentElement()->clientHeight());
  EXPECT_EQ(viewport_width, document->documentElement()->clientWidth());
}

TEST_F(WebFrameTest, SetForceZeroLayoutHeightWorksWithWrapContentMode) {
  RegisterMockedHttpURLLoad("0-by-0.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "0-by-0.html", nullptr, nullptr,
                                    &client, ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetForceZeroLayoutHeight(true);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  LocalFrameView* frame_view =
      web_view_helper.GetWebView()->MainFrameImpl()->GetFrameView();
  GraphicsLayer* scroll_container =
      frame_view->GetLayoutView()->Compositor()->RootGraphicsLayer();

  EXPECT_EQ(IntSize(), frame_view->GetLayoutSize());
  EXPECT_EQ(gfx::Size(), scroll_container->Size());

  web_view_helper.Resize(gfx::Size(viewport_width, 0));
  EXPECT_EQ(IntSize(viewport_width, 0), frame_view->GetLayoutSize());
  EXPECT_EQ(gfx::Size(viewport_width, 0), scroll_container->Size());

  // The flag ForceZeroLayoutHeight will cause the following resize of viewport
  // height to be ignored by the outer viewport (the container layer of
  // LayerCompositor). The height of the visualViewport, however, is not
  // affected.
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  EXPECT_FALSE(frame_view->NeedsLayout());
  EXPECT_EQ(IntSize(viewport_width, 0), frame_view->GetLayoutSize());
  EXPECT_EQ(gfx::Size(viewport_width, viewport_height),
            scroll_container->Size());

  LocalFrame* frame = web_view_helper.LocalMainFrame()->GetFrame();
  VisualViewport& visual_viewport = frame->GetPage()->GetVisualViewport();
  auto* scroll_node = visual_viewport.GetScrollTranslationNode()->ScrollNode();
  EXPECT_EQ(IntRect(0, 0, viewport_width, viewport_height),
            scroll_node->ContainerRect());
  EXPECT_EQ(IntSize(viewport_width, viewport_height),
            scroll_node->ContentsSize());
}

TEST_F(WebFrameTest, SetForceZeroLayoutHeight) {
  RegisterMockedHttpURLLoad("200-by-300.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "200-by-300.html", nullptr,
                                    nullptr, &client, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_LE(viewport_height, web_view_helper.GetWebView()
                                 ->MainFrameImpl()
                                 ->GetFrameView()
                                 ->GetLayoutSize()
                                 .Height());
  web_view_helper.GetWebView()->GetSettings()->SetForceZeroLayoutHeight(true);
  EXPECT_TRUE(web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->NeedsLayout());

  EXPECT_EQ(0, web_view_helper.GetWebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->GetLayoutSize()
                   .Height());

  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height * 2));
  EXPECT_FALSE(web_view_helper.GetWebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->NeedsLayout());
  EXPECT_EQ(0, web_view_helper.GetWebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->GetLayoutSize()
                   .Height());

  web_view_helper.Resize(gfx::Size(viewport_width * 2, viewport_height));
  EXPECT_EQ(0, web_view_helper.GetWebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->GetLayoutSize()
                   .Height());

  web_view_helper.GetWebView()->GetSettings()->SetForceZeroLayoutHeight(false);
  EXPECT_LE(viewport_height, web_view_helper.GetWebView()
                                 ->MainFrameImpl()
                                 ->GetFrameView()
                                 ->GetLayoutSize()
                                 .Height());
}

TEST_F(WebFrameTest, ToggleViewportMetaOnOff) {
  RegisterMockedHttpURLLoad("viewport-device-width.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport-device-width.html",
                                    nullptr, nullptr, &client);
  WebSettings* settings = web_view_helper.GetWebView()->GetSettings();
  settings->SetViewportMetaEnabled(false);
  settings->SetViewportEnabled(true);
  settings->SetMainFrameResizesAreOrientationChanges(true);
  settings->SetShrinksViewportContentToFit(true);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  ViewportData& viewport =
      To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame())
          ->GetDocument()
          ->GetViewportData();
  EXPECT_FALSE(viewport.GetViewportDescription().IsLegacyViewportType());

  settings->SetViewportMetaEnabled(true);
  EXPECT_TRUE(viewport.GetViewportDescription().IsLegacyViewportType());

  settings->SetViewportMetaEnabled(false);
  EXPECT_FALSE(viewport.GetViewportDescription().IsLegacyViewportType());
}

TEST_F(WebFrameTest,
       SetForceZeroLayoutHeightWorksWithRelayoutsWhenHeightChanged) {
  // this unit test is an attempt to target a real world case where an app could
  // 1. call resize(width, 0) and setForceZeroLayoutHeight(true)
  // 2. load content (hoping that the viewport height would increase
  // as more content is added)
  // 3. fail to register touch events aimed at the loaded content
  // because the layout is only updated if either width or height is changed
  RegisterMockedHttpURLLoad("button.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "button.html", nullptr, nullptr,
                                    &client, ConfigureAndroid);
  // set view height to zero so that if the height of the view is not
  // successfully updated during later resizes touch events will fail
  // (as in not hit content included in the view)
  web_view_helper.Resize(gfx::Size(viewport_width, 0));

  web_view_helper.GetWebView()->GetSettings()->SetForceZeroLayoutHeight(true);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  FloatPoint hit_point = FloatPoint(30, 30);  // button size is 100x100

  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  Document* document = frame->GetFrame()->GetDocument();
  Element* element = document->getElementById("tap_button");

  ASSERT_NE(nullptr, element);
  EXPECT_EQ(String("oldValue"), element->innerText());

  WebGestureEvent gesture_event(WebInputEvent::Type::kGestureTap,
                                WebInputEvent::kNoModifiers,
                                WebInputEvent::GetStaticTimeStampForTests(),
                                WebGestureDevice::kTouchscreen);
  gesture_event.SetFrameScale(1);
  gesture_event.SetPositionInWidget(hit_point);
  gesture_event.SetPositionInScreen(hit_point);
  web_view_helper.GetWebView()
      ->MainFrameImpl()
      ->GetFrame()
      ->GetEventHandler()
      .HandleGestureEvent(gesture_event);
  // when pressed, the button changes its own text to "updatedValue"
  EXPECT_EQ(String("updatedValue"), element->innerText());
}

TEST_F(WebFrameTest, FrameOwnerPropertiesMargin) {
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote();

  WebFrameOwnerProperties properties;
  properties.margin_width = 11;
  properties.margin_height = 22;
  WebLocalFrameImpl* local_frame = frame_test_helpers::CreateLocalChild(
      *helper.RemoteMainFrame(), "frameName", properties);

  RegisterMockedHttpURLLoad("frame_owner_properties.html");
  frame_test_helpers::LoadFrame(local_frame,
                                base_url_ + "frame_owner_properties.html");

  // Check if the LocalFrame has seen the marginwidth and marginheight
  // properties.
  Document* child_document = local_frame->GetFrame()->GetDocument();
  EXPECT_EQ(11, child_document->FirstBodyElement()->GetIntegralAttribute(
                    html_names::kMarginwidthAttr));
  EXPECT_EQ(22, child_document->FirstBodyElement()->GetIntegralAttribute(
                    html_names::kMarginheightAttr));

  LocalFrameView* frame_view = local_frame->GetFrameView();
  frame_view->Resize(800, 600);
  frame_view->SetNeedsLayout();
  frame_view->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  // Expect scrollbars to be enabled by default.
  EXPECT_NE(nullptr, frame_view->LayoutViewport()->HorizontalScrollbar());
  EXPECT_NE(nullptr, frame_view->LayoutViewport()->VerticalScrollbar());
}

TEST_F(WebFrameTest, FrameOwnerPropertiesScrolling) {
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote();

  WebFrameOwnerProperties properties;
  // Turn off scrolling in the subframe.
  properties.scrollbar_mode = mojom::blink::ScrollbarMode::kAlwaysOff;
  WebLocalFrameImpl* local_frame = frame_test_helpers::CreateLocalChild(
      *helper.RemoteMainFrame(), "frameName", properties);

  RegisterMockedHttpURLLoad("frame_owner_properties.html");
  frame_test_helpers::LoadFrame(local_frame,
                                base_url_ + "frame_owner_properties.html");

  Document* child_document = local_frame->GetFrame()->GetDocument();
  EXPECT_EQ(0, child_document->FirstBodyElement()->GetIntegralAttribute(
                   html_names::kMarginwidthAttr));
  EXPECT_EQ(0, child_document->FirstBodyElement()->GetIntegralAttribute(
                   html_names::kMarginheightAttr));

  LocalFrameView* frame_view = local_frame->GetFrameView();
  EXPECT_EQ(nullptr, frame_view->LayoutViewport()->HorizontalScrollbar());
  EXPECT_EQ(nullptr, frame_view->LayoutViewport()->VerticalScrollbar());
}

TEST_F(WebFrameTest, SetForceZeroLayoutHeightWorksAcrossNavigations) {
  RegisterMockedHttpURLLoad("200-by-300.html");
  RegisterMockedHttpURLLoad("large-div.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "200-by-300.html", nullptr,
                                    nullptr, &client, ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetForceZeroLayoutHeight(true);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                base_url_ + "large-div.html");
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  EXPECT_EQ(0, web_view_helper.GetWebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->GetLayoutSize()
                   .Height());
}

TEST_F(WebFrameTest, SetForceZeroLayoutHeightWithWideViewportQuirk) {
  RegisterMockedHttpURLLoad("200-by-300.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "200-by-300.html", nullptr,
                                    nullptr, &client, ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(true);
  web_view_helper.GetWebView()->GetSettings()->SetForceZeroLayoutHeight(true);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_EQ(0, web_view_helper.GetWebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->GetLayoutSize()
                   .Height());
}

TEST_F(WebFrameTest, WideViewportQuirkClobbersHeight) {
  RegisterMockedHttpURLLoad("viewport-height-1000.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 600;
  int viewport_height = 800;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank", nullptr, nullptr, &client,
                                    ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                base_url_ + "viewport-height-1000.html");
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_EQ(800, web_view_helper.GetWebView()
                     ->MainFrameImpl()
                     ->GetFrameView()
                     ->GetLayoutSize()
                     .Height());
  EXPECT_EQ(1, web_view_helper.GetWebView()->PageScaleFactor());
}

TEST_F(WebFrameTest, OverflowHiddenDisablesScrolling) {
  RegisterMockedHttpURLLoad("body-overflow-hidden.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr, &client);
  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                base_url_ + "body-overflow-hidden.html");
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  LocalFrameView* view = web_view_helper.LocalMainFrame()->GetFrameView();
  EXPECT_FALSE(view->LayoutViewport()->UserInputScrollable(kVerticalScrollbar));
  EXPECT_FALSE(
      view->LayoutViewport()->UserInputScrollable(kHorizontalScrollbar));
}

TEST_F(WebFrameTest, OverflowHiddenDisablesScrollingWithSetCanHaveScrollbars) {
  RegisterMockedHttpURLLoad("body-overflow-hidden-short.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr, &client);
  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                base_url_ + "body-overflow-hidden-short.html");
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  LocalFrameView* view = web_view_helper.LocalMainFrame()->GetFrameView();
  EXPECT_FALSE(view->LayoutViewport()->UserInputScrollable(kVerticalScrollbar));
  EXPECT_FALSE(
      view->LayoutViewport()->UserInputScrollable(kHorizontalScrollbar));

  web_view_helper.LocalMainFrame()->GetFrameView()->SetCanHaveScrollbars(true);
  EXPECT_FALSE(view->LayoutViewport()->UserInputScrollable(kVerticalScrollbar));
  EXPECT_FALSE(
      view->LayoutViewport()->UserInputScrollable(kHorizontalScrollbar));
}

TEST_F(WebFrameTest, IgnoreOverflowHiddenQuirk) {
  RegisterMockedHttpURLLoad("body-overflow-hidden.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr, &client);
  web_view_helper.GetWebView()
      ->GetSettings()
      ->SetIgnoreMainFrameOverflowHiddenQuirk(true);
  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                base_url_ + "body-overflow-hidden.html");
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  LocalFrameView* view = web_view_helper.LocalMainFrame()->GetFrameView();
  EXPECT_TRUE(view->LayoutViewport()->UserInputScrollable(kVerticalScrollbar));
}

TEST_F(WebFrameTest, NonZeroValuesNoQuirk) {
  RegisterMockedHttpURLLoad("viewport-nonzero-values.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;
  float expected_page_scale_factor = 0.5f;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr, &client, ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetViewportMetaZeroValuesQuirk(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                base_url_ + "viewport-nonzero-values.html");
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_EQ(viewport_width / expected_page_scale_factor,
            web_view_helper.GetWebView()
                ->MainFrameImpl()
                ->GetFrameView()
                ->GetLayoutSize()
                .Width());
  EXPECT_EQ(expected_page_scale_factor,
            web_view_helper.GetWebView()->PageScaleFactor());

  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(true);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  EXPECT_EQ(viewport_width / expected_page_scale_factor,
            web_view_helper.GetWebView()
                ->MainFrameImpl()
                ->GetFrameView()
                ->GetLayoutSize()
                .Width());
  EXPECT_EQ(expected_page_scale_factor,
            web_view_helper.GetWebView()->PageScaleFactor());
}

TEST_F(WebFrameTest, setPageScaleFactorDoesNotLayout) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  // Small viewport to ensure there are always scrollbars.
  int viewport_width = 64;
  int viewport_height = 48;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fixed_layout.html", nullptr,
                                    nullptr, &client, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  unsigned prev_layout_count =
      web_view_helper.LocalMainFrame()->GetFrameView()->LayoutCountForTesting();
  web_view_helper.GetWebView()->SetPageScaleFactor(3);
  EXPECT_FALSE(web_view_helper.GetWebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->NeedsLayout());
  EXPECT_EQ(prev_layout_count, web_view_helper.GetWebView()
                                   ->MainFrameImpl()
                                   ->GetFrameView()
                                   ->LayoutCountForTesting());
}

TEST_F(WebFrameTest, setPageScaleFactorWithOverlayScrollbarsDoesNotLayout) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fixed_layout.html", nullptr,
                                    nullptr, &client, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  unsigned prev_layout_count =
      web_view_helper.LocalMainFrame()->GetFrameView()->LayoutCountForTesting();
  web_view_helper.GetWebView()->SetPageScaleFactor(30);
  EXPECT_FALSE(web_view_helper.GetWebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->NeedsLayout());
  EXPECT_EQ(prev_layout_count, web_view_helper.GetWebView()
                                   ->MainFrameImpl()
                                   ->GetFrameView()
                                   ->LayoutCountForTesting());
}

TEST_F(WebFrameTest, pageScaleFactorWrittenToHistoryItem) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fixed_layout.html", nullptr,
                                    nullptr, &client, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  web_view_helper.GetWebView()->SetPageScaleFactor(3);
  EXPECT_EQ(3,
            To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame())
                ->Loader()
                .GetDocumentLoader()
                ->GetHistoryItem()
                ->GetViewState()
                ->page_scale_factor_);
}

TEST_F(WebFrameTest, initialScaleWrittenToHistoryItem) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr, &client, ConfigureAndroid);
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.25f, 5);
  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                base_url_ + "fixed_layout.html");
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  int default_fixed_layout_width = 980;
  float minimum_page_scale_factor =
      viewport_width / (float)default_fixed_layout_width;
  EXPECT_EQ(minimum_page_scale_factor,
            To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame())
                ->Loader()
                .GetDocumentLoader()
                ->GetHistoryItem()
                ->GetViewState()
                ->page_scale_factor_);
}

TEST_F(WebFrameTest, pageScaleFactorDoesntShrinkFrameView) {
  RegisterMockedHttpURLLoad("large-div.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  // Small viewport to ensure there are always scrollbars.
  int viewport_width = 64;
  int viewport_height = 48;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "large-div.html", nullptr,
                                    nullptr, &client, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  LocalFrameView* view = web_view_helper.LocalMainFrame()->GetFrameView();
  int viewport_width_minus_scrollbar = viewport_width;
  int viewport_height_minus_scrollbar = viewport_height;

  if (view->LayoutViewport()->VerticalScrollbar() &&
      !view->LayoutViewport()->VerticalScrollbar()->IsOverlayScrollbar())
    viewport_width_minus_scrollbar -= 15;

  if (view->LayoutViewport()->HorizontalScrollbar() &&
      !view->LayoutViewport()->HorizontalScrollbar()->IsOverlayScrollbar())
    viewport_height_minus_scrollbar -= 15;

  web_view_helper.GetWebView()->SetPageScaleFactor(2);

  IntSize unscaled_size = view->Size();
  EXPECT_EQ(viewport_width, unscaled_size.Width());
  EXPECT_EQ(viewport_height, unscaled_size.Height());

  IntSize unscaled_size_minus_scrollbar = view->Size();
  EXPECT_EQ(viewport_width_minus_scrollbar,
            unscaled_size_minus_scrollbar.Width());
  EXPECT_EQ(viewport_height_minus_scrollbar,
            unscaled_size_minus_scrollbar.Height());

  IntSize frame_view_size = view->Size();
  EXPECT_EQ(viewport_width_minus_scrollbar, frame_view_size.Width());
  EXPECT_EQ(viewport_height_minus_scrollbar, frame_view_size.Height());
}

TEST_F(WebFrameTest, pageScaleFactorDoesNotApplyCssTransform) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fixed_layout.html", nullptr,
                                    nullptr, &client, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  web_view_helper.GetWebView()->SetPageScaleFactor(2);

  EXPECT_EQ(980,
            To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame())
                ->ContentLayoutObject()
                ->DocumentRect()
                .Width());
  EXPECT_EQ(980, web_view_helper.GetWebView()
                     ->MainFrameImpl()
                     ->GetFrameView()
                     ->LayoutViewport()
                     ->ContentsSize()
                     .Width());
}

TEST_F(WebFrameTest, targetDensityDpiHigh) {
  RegisterMockedHttpURLLoad("viewport-target-densitydpi-high.html");

  FixedLayoutTestWebWidgetClient client;
  // high-dpi = 240
  float target_dpi = 240.0f;
  float device_scale_factors[] = {1.0f, 4.0f / 3.0f, 2.0f};
  int viewport_width = 640;
  int viewport_height = 480;

  for (size_t i = 0; i < base::size(device_scale_factors); ++i) {
    float device_scale_factor = device_scale_factors[i];
    float device_dpi = device_scale_factor * 160.0f;
    client.screen_info_.device_scale_factor = device_scale_factor;

    frame_test_helpers::WebViewHelper web_view_helper;
    web_view_helper.InitializeAndLoad(
        base_url_ + "viewport-target-densitydpi-high.html", nullptr, nullptr,
        &client, ConfigureAndroid);
    web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
        true);
    web_view_helper.GetWebView()
        ->GetSettings()
        ->SetSupportDeprecatedTargetDensityDPI(true);
    web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

    // We need to account for the fact that logical pixels are unconditionally
    // multiplied by deviceScaleFactor to produce physical pixels.
    float density_dpi_scale_ratio =
        device_scale_factor * target_dpi / device_dpi;
    EXPECT_NEAR(viewport_width * density_dpi_scale_ratio,
                web_view_helper.GetWebView()
                    ->MainFrameImpl()
                    ->GetFrameView()
                    ->GetLayoutSize()
                    .Width(),
                1.0f);
    EXPECT_NEAR(viewport_height * density_dpi_scale_ratio,
                web_view_helper.GetWebView()
                    ->MainFrameImpl()
                    ->GetFrameView()
                    ->GetLayoutSize()
                    .Height(),
                1.0f);
    EXPECT_NEAR(1.0f / density_dpi_scale_ratio,
                web_view_helper.GetWebView()->PageScaleFactor(), 0.01f);
  }
}

TEST_F(WebFrameTest, targetDensityDpiDevice) {
  RegisterMockedHttpURLLoad("viewport-target-densitydpi-device.html");

  float device_scale_factors[] = {1.0f, 4.0f / 3.0f, 2.0f};

  FixedLayoutTestWebWidgetClient client;
  int viewport_width = 640;
  int viewport_height = 480;

  for (size_t i = 0; i < base::size(device_scale_factors); ++i) {
    client.screen_info_.device_scale_factor = device_scale_factors[i];

    frame_test_helpers::WebViewHelper web_view_helper;
    web_view_helper.InitializeAndLoad(
        base_url_ + "viewport-target-densitydpi-device.html", nullptr, nullptr,
        &client, ConfigureAndroid);
    web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
        true);
    web_view_helper.GetWebView()
        ->GetSettings()
        ->SetSupportDeprecatedTargetDensityDPI(true);
    web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

    EXPECT_NEAR(viewport_width * client.screen_info_.device_scale_factor,
                web_view_helper.GetWebView()
                    ->MainFrameImpl()
                    ->GetFrameView()
                    ->GetLayoutSize()
                    .Width(),
                1.0f);
    EXPECT_NEAR(viewport_height * client.screen_info_.device_scale_factor,
                web_view_helper.GetWebView()
                    ->MainFrameImpl()
                    ->GetFrameView()
                    ->GetLayoutSize()
                    .Height(),
                1.0f);
    EXPECT_NEAR(1.0f / client.screen_info_.device_scale_factor,
                web_view_helper.GetWebView()->PageScaleFactor(), 0.01f);
  }
}

TEST_F(WebFrameTest, targetDensityDpiDeviceAndFixedWidth) {
  RegisterMockedHttpURLLoad(
      "viewport-target-densitydpi-device-and-fixed-width.html");

  float device_scale_factors[] = {1.0f, 4.0f / 3.0f, 2.0f};

  FixedLayoutTestWebWidgetClient client;
  int viewport_width = 640;
  int viewport_height = 480;

  for (size_t i = 0; i < base::size(device_scale_factors); ++i) {
    client.screen_info_.device_scale_factor = device_scale_factors[i];

    frame_test_helpers::WebViewHelper web_view_helper;
    web_view_helper.InitializeAndLoad(
        base_url_ + "viewport-target-densitydpi-device-and-fixed-width.html",
        nullptr, nullptr, &client, ConfigureAndroid);
    web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
        true);
    web_view_helper.GetWebView()
        ->GetSettings()
        ->SetSupportDeprecatedTargetDensityDPI(true);
    web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(true);
    web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

    EXPECT_NEAR(viewport_width,
                web_view_helper.GetWebView()
                    ->MainFrameImpl()
                    ->GetFrameView()
                    ->GetLayoutSize()
                    .Width(),
                1.0f);
    EXPECT_NEAR(viewport_height,
                web_view_helper.GetWebView()
                    ->MainFrameImpl()
                    ->GetFrameView()
                    ->GetLayoutSize()
                    .Height(),
                1.0f);
    EXPECT_NEAR(1.0f, web_view_helper.GetWebView()->PageScaleFactor(), 0.01f);
  }
}

TEST_F(WebFrameTest, NoWideViewportAndScaleLessThanOne) {
  RegisterMockedHttpURLLoad("viewport-initial-scale-less-than-1.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1.33f;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-initial-scale-less-than-1.html", nullptr, nullptr,
      &client, ConfigureAndroid);
  web_view_helper.GetWebView()
      ->GetSettings()
      ->SetSupportDeprecatedTargetDensityDPI(true);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_NEAR(viewport_width * client.screen_info_.device_scale_factor,
              web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .Width(),
              1.0f);
  EXPECT_NEAR(viewport_height * client.screen_info_.device_scale_factor,
              web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .Height(),
              1.0f);
  EXPECT_NEAR(1.0f / client.screen_info_.device_scale_factor,
              web_view_helper.GetWebView()->PageScaleFactor(), 0.01f);
}

TEST_F(WebFrameTest, NoWideViewportAndScaleLessThanOneWithDeviceWidth) {
  RegisterMockedHttpURLLoad(
      "viewport-initial-scale-less-than-1-device-width.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1.33f;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-initial-scale-less-than-1-device-width.html",
      nullptr, nullptr, &client, ConfigureAndroid);
  web_view_helper.GetWebView()
      ->GetSettings()
      ->SetSupportDeprecatedTargetDensityDPI(true);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  const float kPageZoom = 0.25f;
  EXPECT_NEAR(
      viewport_width * client.screen_info_.device_scale_factor / kPageZoom,
      web_view_helper.GetWebView()
          ->MainFrameImpl()
          ->GetFrameView()
          ->GetLayoutSize()
          .Width(),
      1.0f);
  EXPECT_NEAR(
      viewport_height * client.screen_info_.device_scale_factor / kPageZoom,
      web_view_helper.GetWebView()
          ->MainFrameImpl()
          ->GetFrameView()
          ->GetLayoutSize()
          .Height(),
      1.0f);
  EXPECT_NEAR(1.0f / client.screen_info_.device_scale_factor,
              web_view_helper.GetWebView()->PageScaleFactor(), 0.01f);
}

TEST_F(WebFrameTest, NoWideViewportAndNoViewportWithInitialPageScaleOverride) {
  RegisterMockedHttpURLLoad("large-div.html");

  FixedLayoutTestWebWidgetClient client;
  int viewport_width = 640;
  int viewport_height = 480;
  float enforced_page_scale_factor = 5.0f;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "large-div.html", nullptr,
                                    nullptr, &client, ConfigureAndroid);
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.25f, 5);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.GetWebView()->SetInitialPageScaleOverride(
      enforced_page_scale_factor);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_NEAR(viewport_width / enforced_page_scale_factor,
              web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .Width(),
              1.0f);
  EXPECT_NEAR(viewport_height / enforced_page_scale_factor,
              web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .Height(),
              1.0f);
  EXPECT_NEAR(enforced_page_scale_factor,
              web_view_helper.GetWebView()->PageScaleFactor(), 0.01f);
}

TEST_F(WebFrameTest, NoUserScalableQuirkIgnoresViewportScale) {
  RegisterMockedHttpURLLoad("viewport-initial-scale-and-user-scalable-no.html");

  FixedLayoutTestWebWidgetClient client;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-initial-scale-and-user-scalable-no.html", nullptr,
      nullptr, &client, ConfigureAndroid);
  web_view_helper.GetWebView()
      ->GetSettings()
      ->SetViewportMetaNonUserScalableQuirk(true);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_NEAR(viewport_width,
              web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .Width(),
              1.0f);
  EXPECT_NEAR(viewport_height,
              web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .Height(),
              1.0f);
  EXPECT_NEAR(1.0f, web_view_helper.GetWebView()->PageScaleFactor(), 0.01f);
}

TEST_F(WebFrameTest,
       NoUserScalableQuirkIgnoresViewportScaleForNonWideViewport) {
  RegisterMockedHttpURLLoad("viewport-initial-scale-and-user-scalable-no.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1.33f;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-initial-scale-and-user-scalable-no.html", nullptr,
      nullptr, &client, ConfigureAndroid);
  web_view_helper.GetWebView()
      ->GetSettings()
      ->SetSupportDeprecatedTargetDensityDPI(true);
  web_view_helper.GetWebView()
      ->GetSettings()
      ->SetViewportMetaNonUserScalableQuirk(true);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_NEAR(viewport_width * client.screen_info_.device_scale_factor,
              web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .Width(),
              1.0f);
  EXPECT_NEAR(viewport_height * client.screen_info_.device_scale_factor,
              web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .Height(),
              1.0f);
  EXPECT_NEAR(1.0f / client.screen_info_.device_scale_factor,
              web_view_helper.GetWebView()->PageScaleFactor(), 0.01f);
}

TEST_F(WebFrameTest, NoUserScalableQuirkIgnoresViewportScaleForWideViewport) {
  RegisterMockedHttpURLLoad("viewport-2x-initial-scale-non-user-scalable.html");

  FixedLayoutTestWebWidgetClient client;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-2x-initial-scale-non-user-scalable.html", nullptr,
      nullptr, &client, ConfigureAndroid);
  web_view_helper.GetWebView()
      ->GetSettings()
      ->SetViewportMetaNonUserScalableQuirk(true);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(true);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_NEAR(viewport_width,
              web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .Width(),
              1.0f);
  EXPECT_NEAR(viewport_height,
              web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .Height(),
              1.0f);
  EXPECT_NEAR(1.0f, web_view_helper.GetWebView()->PageScaleFactor(), 0.01f);
}

TEST_F(WebFrameTest, DesktopPageCanBeZoomedInWhenWideViewportIsTurnedOff) {
  RegisterMockedHttpURLLoad("no_viewport_tag.html");

  FixedLayoutTestWebWidgetClient client;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "no_viewport_tag.html", nullptr,
                                    nullptr, &client, ConfigureAndroid);
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.25f, 5);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_NEAR(1.0f, web_view_helper.GetWebView()->PageScaleFactor(), 0.01f);
  EXPECT_NEAR(1.0f, web_view_helper.GetWebView()->MinimumPageScaleFactor(),
              0.01f);
  EXPECT_NEAR(5.0f, web_view_helper.GetWebView()->MaximumPageScaleFactor(),
              0.01f);
}

class WebFrameResizeTest : public WebFrameTest {
 protected:
  static FloatSize ComputeRelativeOffset(const IntPoint& absolute_offset,
                                         const LayoutRect& rect) {
    FloatSize relative_offset =
        FloatPoint(absolute_offset) - FloatPoint(rect.Location());
    relative_offset.Scale(1.f / rect.Width(), 1.f / rect.Height());
    return relative_offset;
  }

  void TestResizeYieldsCorrectScrollAndScale(
      const char* url,
      const float initial_page_scale_factor,
      const WebSize scroll_offset,
      const WebSize viewport_size,
      const bool should_scale_relative_to_viewport_width) {
    RegisterMockedHttpURLLoad(url);

    const float aspect_ratio =
        static_cast<float>(viewport_size.width) / viewport_size.height;

    frame_test_helpers::WebViewHelper web_view_helper;
    web_view_helper.InitializeAndLoad(base_url_ + url, nullptr, nullptr,
                                      nullptr, ConfigureAndroid);
    web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.25f, 5);

    // Origin scrollOffsets preserved under resize.
    {
      web_view_helper.Resize(
          gfx::Size(viewport_size.width, viewport_size.height));
      web_view_helper.GetWebView()->SetPageScaleFactor(
          initial_page_scale_factor);
      ASSERT_EQ(gfx::Size(viewport_size),
                web_view_helper.GetWebView()->MainFrameWidget()->Size());
      ASSERT_EQ(initial_page_scale_factor,
                web_view_helper.GetWebView()->PageScaleFactor());
      web_view_helper.Resize(
          gfx::Size(viewport_size.height, viewport_size.width));
      float expected_page_scale_factor =
          initial_page_scale_factor *
          (should_scale_relative_to_viewport_width ? 1 / aspect_ratio : 1);
      EXPECT_NEAR(expected_page_scale_factor,
                  web_view_helper.GetWebView()->PageScaleFactor(), 0.05f);
      EXPECT_EQ(WebSize(), web_view_helper.LocalMainFrame()->GetScrollOffset());
    }

    // Resizing just the height should not affect pageScaleFactor or
    // scrollOffset.
    {
      web_view_helper.Resize(
          gfx::Size(viewport_size.width, viewport_size.height));
      web_view_helper.GetWebView()->SetPageScaleFactor(
          initial_page_scale_factor);
      web_view_helper.LocalMainFrame()->SetScrollOffset(scroll_offset);
      UpdateAllLifecyclePhases(web_view_helper.GetWebView());
      const WebSize expected_scroll_offset =
          web_view_helper.LocalMainFrame()->GetScrollOffset();
      web_view_helper.Resize(
          gfx::Size(viewport_size.width, viewport_size.height * 0.8f));
      EXPECT_EQ(initial_page_scale_factor,
                web_view_helper.GetWebView()->PageScaleFactor());
      EXPECT_EQ(expected_scroll_offset,
                web_view_helper.LocalMainFrame()->GetScrollOffset());
      web_view_helper.Resize(
          gfx::Size(viewport_size.width, viewport_size.height * 0.8f));
      EXPECT_EQ(initial_page_scale_factor,
                web_view_helper.GetWebView()->PageScaleFactor());
      EXPECT_EQ(expected_scroll_offset,
                web_view_helper.LocalMainFrame()->GetScrollOffset());
    }
  }
};

TEST_F(WebFrameResizeTest,
       ResizeYieldsCorrectScrollAndScaleForWidthEqualsDeviceWidth) {
  // With width=device-width, pageScaleFactor is preserved across resizes as
  // long as the content adjusts according to the device-width.
  const char* url = "resize_scroll_mobile.html";
  const float kInitialPageScaleFactor = 1;
  const WebSize scroll_offset(0, 50);
  const WebSize viewport_size(120, 160);
  const bool kShouldScaleRelativeToViewportWidth = true;

  TestResizeYieldsCorrectScrollAndScale(url, kInitialPageScaleFactor,
                                        scroll_offset, viewport_size,
                                        kShouldScaleRelativeToViewportWidth);
}

TEST_F(WebFrameResizeTest, ResizeYieldsCorrectScrollAndScaleForMinimumScale) {
  // This tests a scenario where minimum-scale is set to 1.0, but some element
  // on the page is slightly larger than the portrait width, so our "natural"
  // minimum-scale would be lower. In that case, we should stick to 1.0 scale
  // on rotation and not do anything strange.
  const char* url = "resize_scroll_minimum_scale.html";
  const float kInitialPageScaleFactor = 1;
  const WebSize scroll_offset(0, 0);
  const WebSize viewport_size(240, 320);
  const bool kShouldScaleRelativeToViewportWidth = false;

  TestResizeYieldsCorrectScrollAndScale(url, kInitialPageScaleFactor,
                                        scroll_offset, viewport_size,
                                        kShouldScaleRelativeToViewportWidth);
}

TEST_F(WebFrameResizeTest, ResizeYieldsCorrectScrollAndScaleForFixedWidth) {
  // With a fixed width, pageScaleFactor scales by the relative change in
  // viewport width.
  const char* url = "resize_scroll_fixed_width.html";
  const float kInitialPageScaleFactor = 2;
  const WebSize scroll_offset(0, 200);
  const WebSize viewport_size(240, 320);
  const bool kShouldScaleRelativeToViewportWidth = true;

  TestResizeYieldsCorrectScrollAndScale(url, kInitialPageScaleFactor,
                                        scroll_offset, viewport_size,
                                        kShouldScaleRelativeToViewportWidth);
}

TEST_F(WebFrameResizeTest, ResizeYieldsCorrectScrollAndScaleForFixedLayout) {
  // With a fixed layout, pageScaleFactor scales by the relative change in
  // viewport width.
  const char* url = "resize_scroll_fixed_layout.html";
  const float kInitialPageScaleFactor = 2;
  const WebSize scroll_offset(200, 400);
  const WebSize viewport_size(320, 240);
  const bool kShouldScaleRelativeToViewportWidth = true;

  TestResizeYieldsCorrectScrollAndScale(url, kInitialPageScaleFactor,
                                        scroll_offset, viewport_size,
                                        kShouldScaleRelativeToViewportWidth);
}

TEST_F(WebFrameTest, pageScaleFactorUpdatesScrollbars) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fixed_layout.html", nullptr,
                                    nullptr, &client, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  LocalFrameView* view = web_view_helper.LocalMainFrame()->GetFrameView();
  ScrollableArea* scrollable_area = view->LayoutViewport();
  EXPECT_EQ(scrollable_area->ScrollSize(kHorizontalScrollbar),
            scrollable_area->ContentsSize().Width() - view->Width());
  EXPECT_EQ(scrollable_area->ScrollSize(kVerticalScrollbar),
            scrollable_area->ContentsSize().Height() - view->Height());

  web_view_helper.GetWebView()->SetPageScaleFactor(10);

  EXPECT_EQ(scrollable_area->ScrollSize(kHorizontalScrollbar),
            scrollable_area->ContentsSize().Width() - view->Width());
  EXPECT_EQ(scrollable_area->ScrollSize(kVerticalScrollbar),
            scrollable_area->ContentsSize().Height() - view->Height());
}

TEST_F(WebFrameTest, CanOverrideScaleLimits) {
  RegisterMockedHttpURLLoad("no_scale_for_you.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "no_scale_for_you.html",
                                    nullptr, nullptr, &client,
                                    ConfigureAndroid);
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.25f, 5);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_EQ(2.0f, web_view_helper.GetWebView()->MinimumPageScaleFactor());
  EXPECT_EQ(2.0f, web_view_helper.GetWebView()->MaximumPageScaleFactor());

  web_view_helper.GetWebView()->SetIgnoreViewportTagScaleLimits(true);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  EXPECT_EQ(1.0f, web_view_helper.GetWebView()->MinimumPageScaleFactor());
  EXPECT_EQ(5.0f, web_view_helper.GetWebView()->MaximumPageScaleFactor());

  web_view_helper.GetWebView()->SetIgnoreViewportTagScaleLimits(false);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  EXPECT_EQ(2.0f, web_view_helper.GetWebView()->MinimumPageScaleFactor());
  EXPECT_EQ(2.0f, web_view_helper.GetWebView()->MaximumPageScaleFactor());
}

// Android doesn't have scrollbars on the main LocalFrameView
#if defined(OS_ANDROID)
TEST_F(WebFrameTest, DISABLED_updateOverlayScrollbarLayers)
#else
TEST_F(WebFrameTest, updateOverlayScrollbarLayers)
#endif
{
  RegisterMockedHttpURLLoad("large-div.html");

  int view_width = 500;
  int view_height = 500;

  FixedLayoutTestWebWidgetClient client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr, &client,
                             &ConfigureCompositingWebView);

  web_view_helper.Resize(gfx::Size(view_width, view_height));
  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                base_url_ + "large-div.html");

  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  LocalFrameView* view = web_view_helper.LocalMainFrame()->GetFrameView();
  EXPECT_TRUE(view->LayoutViewport()->LayerForHorizontalScrollbar());
  EXPECT_TRUE(view->LayoutViewport()->LayerForVerticalScrollbar());

  web_view_helper.Resize(gfx::Size(view_width * 10, view_height * 10));
  EXPECT_FALSE(view->LayoutViewport()->LayerForHorizontalScrollbar());
  EXPECT_FALSE(view->LayoutViewport()->LayerForVerticalScrollbar());
}

void SetScaleAndScrollAndLayout(WebViewImpl* web_view,
                                const gfx::Point& scroll,
                                float scale) {
  web_view->SetPageScaleFactor(scale);
  web_view->MainFrameImpl()->SetScrollOffset(WebSize(scroll.x(), scroll.y()));
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);
}

void SimulatePageScale(WebViewImpl* web_view_impl, float& scale) {
  float scale_delta =
      web_view_impl->FakePageScaleAnimationPageScaleForTesting() /
      web_view_impl->PageScaleFactor();
  web_view_impl->MainFrameWidget()->ApplyViewportChangesForTesting(
      {gfx::ScrollOffset(), gfx::Vector2dF(), scale_delta, false, 0, 0,
       cc::BrowserControlsState::kBoth});
  scale = web_view_impl->PageScaleFactor();
}

WebRect ComputeBlockBoundHelper(WebViewImpl* web_view_impl,
                                const gfx::Point& point,
                                bool ignore_clipping) {
  DCHECK(web_view_impl->MainFrameImpl());
  WebFrameWidgetBase* widget =
      web_view_impl->MainFrameImpl()->FrameWidgetImpl();
  DCHECK(widget);
  return widget->ComputeBlockBound(point, ignore_clipping);
}

void SimulateDoubleTap(WebViewImpl* web_view_impl,
                       gfx::Point& point,
                       float& scale) {
  web_view_impl->AnimateDoubleTapZoom(
      IntPoint(point), ComputeBlockBoundHelper(web_view_impl, point, false));
  EXPECT_TRUE(web_view_impl->FakeDoubleTapAnimationPendingForTesting());
  SimulatePageScale(web_view_impl, scale);
}

TEST_F(WebFrameTest, DivAutoZoomParamsTest) {
  RegisterMockedHttpURLLoad("get_scale_for_auto_zoom_into_div_test.html");

  const float kDeviceScaleFactor = 2.0f;
  int viewport_width = 640 / kDeviceScaleFactor;
  int viewport_height = 1280 / kDeviceScaleFactor;
  float double_tap_zoom_already_legible_ratio = 1.2f;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "get_scale_for_auto_zoom_into_div_test.html", nullptr,
      nullptr, nullptr, ConfigureAndroid);
  web_view_helper.GetWebView()->SetDeviceScaleFactor(kDeviceScaleFactor);
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.01f, 4);
  web_view_helper.GetWebView()->SetPageScaleFactor(0.5f);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  WebRect wide_div(200, 100, 400, 150);
  WebRect tall_div(200, 300, 400, 800);
  gfx::Point double_tap_point_wide(wide_div.x + 50, wide_div.y + 50);
  gfx::Point double_tap_point_tall(tall_div.x + 50, tall_div.y + 50);
  float scale;
  IntPoint scroll;

  float double_tap_zoom_already_legible_scale =
      web_view_helper.GetWebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;

  // Test double-tap zooming into wide div.
  WebRect wide_block_bound = ComputeBlockBoundHelper(
      web_view_helper.GetWebView(), double_tap_point_wide, false);
  web_view_helper.GetWebView()->ComputeScaleAndScrollForBlockRect(
      double_tap_point_wide, wide_block_bound, kTouchPointPadding,
      double_tap_zoom_already_legible_scale, scale, scroll);
  // The div should horizontally fill the screen (modulo margins), and
  // vertically centered (modulo integer rounding).
  EXPECT_NEAR(viewport_width / (float)wide_div.width, scale, 0.1);
  EXPECT_NEAR(wide_div.x, scroll.X(), 20);
  EXPECT_EQ(0, scroll.Y());

  SetScaleAndScrollAndLayout(web_view_helper.GetWebView(), scroll, scale);

  // Test zoom out back to minimum scale.
  wide_block_bound = ComputeBlockBoundHelper(web_view_helper.GetWebView(),
                                             double_tap_point_wide, false);
  web_view_helper.GetWebView()->ComputeScaleAndScrollForBlockRect(
      double_tap_point_wide, wide_block_bound, kTouchPointPadding,
      double_tap_zoom_already_legible_scale, scale, scroll);
  // FIXME: Looks like we are missing EXPECTs here.

  scale = web_view_helper.GetWebView()->MinimumPageScaleFactor();
  SetScaleAndScrollAndLayout(web_view_helper.GetWebView(), gfx::Point(), scale);

  // Test double-tap zooming into tall div.
  WebRect tall_block_bound = ComputeBlockBoundHelper(
      web_view_helper.GetWebView(), double_tap_point_tall, false);
  web_view_helper.GetWebView()->ComputeScaleAndScrollForBlockRect(
      double_tap_point_tall, tall_block_bound, kTouchPointPadding,
      double_tap_zoom_already_legible_scale, scale, scroll);
  // The div should start at the top left of the viewport.
  EXPECT_NEAR(viewport_width / (float)tall_div.width, scale, 0.1);
  EXPECT_NEAR(tall_div.x, scroll.X(), 20);
  EXPECT_NEAR(tall_div.y, scroll.Y(), 20);
}

TEST_F(WebFrameTest, DivAutoZoomWideDivTest) {
  RegisterMockedHttpURLLoad("get_wide_div_for_auto_zoom_test.html");

  const float kDeviceScaleFactor = 2.0f;
  int viewport_width = 640 / kDeviceScaleFactor;
  int viewport_height = 1280 / kDeviceScaleFactor;
  float double_tap_zoom_already_legible_ratio = 1.2f;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "get_wide_div_for_auto_zoom_test.html", nullptr, nullptr,
      nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  web_view_helper.GetWebView()->SetDeviceScaleFactor(kDeviceScaleFactor);
  web_view_helper.GetWebView()->SetPageScaleFactor(1.0f);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  web_view_helper.GetWebView()->EnableFakePageScaleAnimationForTesting(true);

  float double_tap_zoom_already_legible_scale =
      web_view_helper.GetWebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;

  WebRect div(0, 100, viewport_width, 150);
  gfx::Point point(div.x + 50, div.y + 50);
  float scale;
  SetScaleAndScrollAndLayout(
      web_view_helper.GetWebView(), gfx::Point(),
      (web_view_helper.GetWebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);

  SimulateDoubleTap(web_view_helper.GetWebView(), point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.GetWebView()->MinimumPageScaleFactor(),
                  scale);
}

TEST_F(WebFrameTest, DivAutoZoomVeryTallTest) {
  // When a block is taller than the viewport and a zoom targets a lower part
  // of it, then we should keep the target point onscreen instead of snapping
  // back up the top of the block.
  RegisterMockedHttpURLLoad("very_tall_div.html");

  const float kDeviceScaleFactor = 2.0f;
  int viewport_width = 640 / kDeviceScaleFactor;
  int viewport_height = 1280 / kDeviceScaleFactor;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "very_tall_div.html", nullptr,
                                    nullptr, nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  web_view_helper.GetWebView()->SetDeviceScaleFactor(kDeviceScaleFactor);
  web_view_helper.GetWebView()->SetPageScaleFactor(1.0f);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  WebRect div(200, 300, 400, 5000);
  gfx::Point point(div.x + 50, div.y + 3000);
  float scale;
  IntPoint scroll;

  WebRect block_bound =
      ComputeBlockBoundHelper(web_view_helper.GetWebView(), point, true);
  web_view_helper.GetWebView()->ComputeScaleAndScrollForBlockRect(
      point, block_bound, 0, 1.0f, scale, scroll);
  EXPECT_EQ(scale, 1.0f);
  EXPECT_EQ(scroll.Y(), 2660);
}

TEST_F(WebFrameTest, DivAutoZoomMultipleDivsTest) {
  RegisterMockedHttpURLLoad("get_multiple_divs_for_auto_zoom_test.html");

  const float kDeviceScaleFactor = 2.0f;
  int viewport_width = 640 / kDeviceScaleFactor;
  int viewport_height = 1280 / kDeviceScaleFactor;
  float double_tap_zoom_already_legible_ratio = 1.2f;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "get_multiple_divs_for_auto_zoom_test.html", nullptr, nullptr,
      nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.5f, 4);
  web_view_helper.GetWebView()->SetDeviceScaleFactor(kDeviceScaleFactor);
  web_view_helper.GetWebView()->SetPageScaleFactor(0.5f);
  web_view_helper.GetWebView()->SetMaximumLegibleScale(1.f);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  web_view_helper.GetWebView()->EnableFakePageScaleAnimationForTesting(true);

  WebRect top_div(200, 100, 200, 150);
  WebRect bottom_div(200, 300, 200, 150);
  gfx::Point top_point(top_div.x + 50, top_div.y + 50);
  gfx::Point bottom_point(bottom_div.x + 50, bottom_div.y + 50);
  float scale;
  SetScaleAndScrollAndLayout(
      web_view_helper.GetWebView(), gfx::Point(),
      (web_view_helper.GetWebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);

  // Test double tap on two different divs.  After first zoom, we should go back
  // to minimum page scale with a second double tap.
  SimulateDoubleTap(web_view_helper.GetWebView(), top_point, scale);
  EXPECT_FLOAT_EQ(1, scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), bottom_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.GetWebView()->MinimumPageScaleFactor(),
                  scale);

  // If the user pinch zooms after double tap, a second double tap should zoom
  // back to the div.
  SimulateDoubleTap(web_view_helper.GetWebView(), top_point, scale);
  EXPECT_FLOAT_EQ(1, scale);
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->ApplyViewportChangesForTesting({gfx::ScrollOffset(), gfx::Vector2dF(),
                                        0.6f, false, 0, 0,
                                        cc::BrowserControlsState::kBoth});
  SimulateDoubleTap(web_view_helper.GetWebView(), bottom_point, scale);
  EXPECT_FLOAT_EQ(1, scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), bottom_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.GetWebView()->MinimumPageScaleFactor(),
                  scale);

  // If we didn't yet get an auto-zoom update and a second double-tap arrives,
  // should go back to minimum scale.
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->ApplyViewportChangesForTesting({gfx::ScrollOffset(), gfx::Vector2dF(),
                                        1.1f, false, 0, 0,
                                        cc::BrowserControlsState::kBoth});

  WebRect block_bounds =
      ComputeBlockBoundHelper(web_view_helper.GetWebView(), top_point, false);
  web_view_helper.GetWebView()->AnimateDoubleTapZoom(IntPoint(top_point),
                                                     block_bounds);
  EXPECT_TRUE(
      web_view_helper.GetWebView()->FakeDoubleTapAnimationPendingForTesting());
  SimulateDoubleTap(web_view_helper.GetWebView(), bottom_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.GetWebView()->MinimumPageScaleFactor(),
                  scale);
}

TEST_F(WebFrameTest, DivAutoZoomScaleBoundsTest) {
  RegisterMockedHttpURLLoad("get_scale_bounds_check_for_auto_zoom_test.html");

  int viewport_width = 320;
  int viewport_height = 480;
  float double_tap_zoom_already_legible_ratio = 1.2f;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "get_scale_bounds_check_for_auto_zoom_test.html", nullptr,
      nullptr, nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  web_view_helper.GetWebView()->SetDeviceScaleFactor(1.5f);
  web_view_helper.GetWebView()->SetMaximumLegibleScale(1.f);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  web_view_helper.GetWebView()->EnableFakePageScaleAnimationForTesting(true);

  WebRect div(200, 100, 200, 150);
  gfx::Point double_tap_point(div.x + 50, div.y + 50);
  float scale;

  // Test double tap scale bounds.
  // minimumPageScale < doubleTapZoomAlreadyLegibleScale < 1
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.5f, 4);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  float double_tap_zoom_already_legible_scale =
      web_view_helper.GetWebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  SetScaleAndScrollAndLayout(
      web_view_helper.GetWebView(), gfx::Point(),
      (web_view_helper.GetWebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(1, scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.GetWebView()->MinimumPageScaleFactor(),
                  scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(1, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->ApplyViewportChangesForTesting({gfx::ScrollOffset(), gfx::Vector2dF(),
                                        1.1f, false, 0, 0,
                                        cc::BrowserControlsState::kBoth});
  // 1 < minimumPageScale < doubleTapZoomAlreadyLegibleScale
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(1.1f, 4);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  double_tap_zoom_already_legible_scale =
      web_view_helper.GetWebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  SetScaleAndScrollAndLayout(
      web_view_helper.GetWebView(), gfx::Point(),
      (web_view_helper.GetWebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.GetWebView()->MinimumPageScaleFactor(),
                  scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->ApplyViewportChangesForTesting({gfx::ScrollOffset(), gfx::Vector2dF(),
                                        1.1f, false, 0, 0,
                                        cc::BrowserControlsState::kBoth});
  // minimumPageScale < 1 < doubleTapZoomAlreadyLegibleScale
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.95f, 4);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  double_tap_zoom_already_legible_scale =
      web_view_helper.GetWebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  SetScaleAndScrollAndLayout(
      web_view_helper.GetWebView(), gfx::Point(),
      (web_view_helper.GetWebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.GetWebView()->MinimumPageScaleFactor(),
                  scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);
}

TEST_F(WebFrameTest, DivAutoZoomScaleLegibleScaleTest) {
  RegisterMockedHttpURLLoad("get_scale_bounds_check_for_auto_zoom_test.html");

  int viewport_width = 320;
  int viewport_height = 480;
  float double_tap_zoom_already_legible_ratio = 1.2f;
  float maximum_legible_scale_factor = 1.13f;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "get_scale_bounds_check_for_auto_zoom_test.html", nullptr,
      nullptr, nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  web_view_helper.GetWebView()->SetMaximumLegibleScale(
      maximum_legible_scale_factor);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  web_view_helper.GetWebView()->EnableFakePageScaleAnimationForTesting(true);
  web_view_helper.GetWebView()
      ->GetPage()
      ->GetSettings()
      .SetTextAutosizingEnabled(true);

  WebRect div(200, 100, 200, 150);
  gfx::Point double_tap_point(div.x + 50, div.y + 50);
  float scale;

  // Test double tap scale bounds.
  // minimumPageScale < doubleTapZoomAlreadyLegibleScale < 1 <
  //     maximumLegibleScaleFactor
  float legible_scale = maximum_legible_scale_factor;
  SetScaleAndScrollAndLayout(
      web_view_helper.GetWebView(), gfx::Point(),
      (web_view_helper.GetWebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  float double_tap_zoom_already_legible_scale =
      web_view_helper.GetWebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.5f, 4);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(legible_scale, scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.GetWebView()->MinimumPageScaleFactor(),
                  scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(legible_scale, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->ApplyViewportChangesForTesting({gfx::ScrollOffset(), gfx::Vector2dF(),
                                        1.1f, false, 0, 0,
                                        cc::BrowserControlsState::kBoth});
  // 1 < maximumLegibleScaleFactor < minimumPageScale <
  //     doubleTapZoomAlreadyLegibleScale
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(1.0f, 4);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  double_tap_zoom_already_legible_scale =
      web_view_helper.GetWebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  SetScaleAndScrollAndLayout(
      web_view_helper.GetWebView(), gfx::Point(),
      (web_view_helper.GetWebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.GetWebView()->MinimumPageScaleFactor(),
                  scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->ApplyViewportChangesForTesting({gfx::ScrollOffset(), gfx::Vector2dF(),
                                        1.1f, false, 0, 0,
                                        cc::BrowserControlsState::kBoth});
  // minimumPageScale < 1 < maximumLegibleScaleFactor <
  //     doubleTapZoomAlreadyLegibleScale
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.95f, 4);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  double_tap_zoom_already_legible_scale =
      web_view_helper.GetWebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  SetScaleAndScrollAndLayout(
      web_view_helper.GetWebView(), gfx::Point(),
      (web_view_helper.GetWebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.GetWebView()->MinimumPageScaleFactor(),
                  scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->ApplyViewportChangesForTesting({gfx::ScrollOffset(), gfx::Vector2dF(),
                                        1.1f, false, 0, 0,
                                        cc::BrowserControlsState::kBoth});
  // minimumPageScale < 1 < doubleTapZoomAlreadyLegibleScale <
  //     maximumLegibleScaleFactor
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.9f, 4);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  double_tap_zoom_already_legible_scale =
      web_view_helper.GetWebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  SetScaleAndScrollAndLayout(
      web_view_helper.GetWebView(), gfx::Point(),
      (web_view_helper.GetWebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(legible_scale, scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.GetWebView()->MinimumPageScaleFactor(),
                  scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(legible_scale, scale);
}

TEST_F(WebFrameTest, DivAutoZoomScaleFontScaleFactorTest) {
  RegisterMockedHttpURLLoad("get_scale_bounds_check_for_auto_zoom_test.html");

  int viewport_width = 320;
  int viewport_height = 480;
  float double_tap_zoom_already_legible_ratio = 1.2f;
  float accessibility_font_scale_factor = 1.13f;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "get_scale_bounds_check_for_auto_zoom_test.html", nullptr,
      nullptr, nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  web_view_helper.GetWebView()->SetMaximumLegibleScale(1.f);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  web_view_helper.GetWebView()->EnableFakePageScaleAnimationForTesting(true);
  web_view_helper.GetWebView()
      ->GetPage()
      ->GetSettings()
      .SetTextAutosizingEnabled(true);
  web_view_helper.GetWebView()
      ->GetPage()
      ->GetSettings()
      .SetAccessibilityFontScaleFactor(accessibility_font_scale_factor);

  WebRect div(200, 100, 200, 150);
  gfx::Point double_tap_point(div.x + 50, div.y + 50);
  float scale;

  // Test double tap scale bounds.
  // minimumPageScale < doubleTapZoomAlreadyLegibleScale < 1 <
  //     accessibilityFontScaleFactor
  float legible_scale = accessibility_font_scale_factor;
  SetScaleAndScrollAndLayout(
      web_view_helper.GetWebView(), gfx::Point(),
      (web_view_helper.GetWebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  float double_tap_zoom_already_legible_scale =
      web_view_helper.GetWebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.5f, 4);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(legible_scale, scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.GetWebView()->MinimumPageScaleFactor(),
                  scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(legible_scale, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->ApplyViewportChangesForTesting({gfx::ScrollOffset(), gfx::Vector2dF(),
                                        1.1f, false, 0, 0,
                                        cc::BrowserControlsState::kBoth});
  // 1 < accessibilityFontScaleFactor < minimumPageScale <
  //     doubleTapZoomAlreadyLegibleScale
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(1.0f, 4);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  double_tap_zoom_already_legible_scale =
      web_view_helper.GetWebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  SetScaleAndScrollAndLayout(
      web_view_helper.GetWebView(), gfx::Point(),
      (web_view_helper.GetWebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.GetWebView()->MinimumPageScaleFactor(),
                  scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->ApplyViewportChangesForTesting({gfx::ScrollOffset(), gfx::Vector2dF(),
                                        1.1f, false, 0, 0,
                                        cc::BrowserControlsState::kBoth});
  // minimumPageScale < 1 < accessibilityFontScaleFactor <
  //     doubleTapZoomAlreadyLegibleScale
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.95f, 4);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  double_tap_zoom_already_legible_scale =
      web_view_helper.GetWebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  SetScaleAndScrollAndLayout(
      web_view_helper.GetWebView(), gfx::Point(),
      (web_view_helper.GetWebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.GetWebView()->MinimumPageScaleFactor(),
                  scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->ApplyViewportChangesForTesting({gfx::ScrollOffset(), gfx::Vector2dF(),
                                        1.1f, false, 0, 0,
                                        cc::BrowserControlsState::kBoth});
  // minimumPageScale < 1 < doubleTapZoomAlreadyLegibleScale <
  //     accessibilityFontScaleFactor
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.9f, 4);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  double_tap_zoom_already_legible_scale =
      web_view_helper.GetWebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  SetScaleAndScrollAndLayout(
      web_view_helper.GetWebView(), gfx::Point(),
      (web_view_helper.GetWebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(legible_scale, scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.GetWebView()->MinimumPageScaleFactor(),
                  scale);
  SimulateDoubleTap(web_view_helper.GetWebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(legible_scale, scale);
}

TEST_F(WebFrameTest, BlockBoundTest) {
  RegisterMockedHttpURLLoad("block_bound.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "block_bound.html", nullptr,
                                    nullptr, nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(300, 300));

  IntRect rect_back = IntRect(0, 0, 200, 200);
  IntRect rect_left_top = IntRect(10, 10, 80, 80);
  IntRect rect_right_bottom = IntRect(110, 110, 80, 80);
  IntRect block_bound;

  block_bound = IntRect(ComputeBlockBoundHelper(web_view_helper.GetWebView(),
                                                gfx::Point(9, 9), true));
  EXPECT_EQ(rect_back, block_bound);

  block_bound = IntRect(ComputeBlockBoundHelper(web_view_helper.GetWebView(),
                                                gfx::Point(10, 10), true));
  EXPECT_EQ(rect_left_top, block_bound);

  block_bound = IntRect(ComputeBlockBoundHelper(web_view_helper.GetWebView(),
                                                gfx::Point(50, 50), true));
  EXPECT_EQ(rect_left_top, block_bound);

  block_bound = IntRect(ComputeBlockBoundHelper(web_view_helper.GetWebView(),
                                                gfx::Point(89, 89), true));
  EXPECT_EQ(rect_left_top, block_bound);

  block_bound = IntRect(ComputeBlockBoundHelper(web_view_helper.GetWebView(),
                                                gfx::Point(90, 90), true));
  EXPECT_EQ(rect_back, block_bound);

  block_bound = IntRect(ComputeBlockBoundHelper(web_view_helper.GetWebView(),
                                                gfx::Point(109, 109), true));
  EXPECT_EQ(rect_back, block_bound);

  block_bound = IntRect(ComputeBlockBoundHelper(web_view_helper.GetWebView(),
                                                gfx::Point(110, 110), true));
  EXPECT_EQ(rect_right_bottom, block_bound);
}

TEST_F(WebFrameTest, DontZoomInOnFocusedInTouchAction) {
  RegisterMockedHttpURLLoad("textbox_in_touch_action.html");

  int viewport_width = 600;
  int viewport_height = 1000;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "textbox_in_touch_action.html");
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.25f, 4);
  web_view_helper.GetWebView()->EnableFakePageScaleAnimationForTesting(true);
  web_view_helper.GetWebView()
      ->GetPage()
      ->GetSettings()
      .SetTextAutosizingEnabled(false);
  web_view_helper.GetWebView()
      ->GetSettings()
      ->SetAutoZoomFocusedNodeToLegibleScale(true);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  float initial_scale = web_view_helper.GetWebView()->PageScaleFactor();

  // Focus the first textbox that's in a touch-action: pan-x ancestor, this
  // shouldn't cause an autozoom since pan-x disables pinch-zoom.
  web_view_helper.GetWebView()->AdvanceFocus(false);
  web_view_helper.GetWebView()
      ->MainFrameImpl()
      ->FrameWidget()
      ->ScrollFocusedEditableElementIntoView();
  EXPECT_EQ(
      web_view_helper.GetWebView()->FakePageScaleAnimationPageScaleForTesting(),
      0);

  SetScaleAndScrollAndLayout(web_view_helper.GetWebView(), gfx::Point(),
                             initial_scale);
  ASSERT_EQ(initial_scale, web_view_helper.GetWebView()->PageScaleFactor());

  // Focus the second textbox that's in a touch-action: manipulation ancestor,
  // this should cause an autozoom since it allows pinch-zoom.
  web_view_helper.GetWebView()->AdvanceFocus(false);
  web_view_helper.GetWebView()
      ->MainFrameImpl()
      ->FrameWidget()
      ->ScrollFocusedEditableElementIntoView();
  EXPECT_GT(
      web_view_helper.GetWebView()->FakePageScaleAnimationPageScaleForTesting(),
      initial_scale);

  SetScaleAndScrollAndLayout(web_view_helper.GetWebView(), gfx::Point(),
                             initial_scale);
  ASSERT_EQ(initial_scale, web_view_helper.GetWebView()->PageScaleFactor());

  // Focus the third textbox that has a touch-action: pan-x ancestor, this
  // should cause an autozoom since it's seperated from the node with the
  // touch-action by an overflow:scroll element.
  web_view_helper.GetWebView()->AdvanceFocus(false);
  web_view_helper.GetWebView()
      ->MainFrameImpl()
      ->FrameWidget()
      ->ScrollFocusedEditableElementIntoView();
  EXPECT_GT(
      web_view_helper.GetWebView()->FakePageScaleAnimationPageScaleForTesting(),
      initial_scale);
}

TEST_F(WebFrameTest, DivScrollIntoEditableTest) {
  RegisterMockedHttpURLLoad("get_scale_for_zoom_into_editable_test.html");

  const bool kAutoZoomToLegibleScale = true;
  int viewport_width = 450;
  int viewport_height = 300;
  float left_box_ratio = 0.3f;
  int caret_padding = 10;
  float min_readable_caret_height = 16.0f;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "get_scale_for_zoom_into_editable_test.html");
  web_view_helper.GetWebView()
      ->GetPage()
      ->GetSettings()
      .SetTextAutosizingEnabled(false);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.25f, 4);

  web_view_helper.GetWebView()->EnableFakePageScaleAnimationForTesting(true);

  WebRect edit_box_with_text(200, 200, 250, 20);
  WebRect edit_box_with_no_text(200, 250, 250, 20);

  // Test scrolling the focused node
  // The edit box is shorter and narrower than the viewport when legible.
  web_view_helper.GetWebView()->AdvanceFocus(false);
  // Set the caret to the end of the input box.
  web_view_helper.GetWebView()
      ->MainFrameImpl()
      ->GetDocument()
      .GetElementById("EditBoxWithText")
      .To<WebInputElement>()
      .SetSelectionRange(1000, 1000);
  SetScaleAndScrollAndLayout(web_view_helper.GetWebView(), gfx::Point(), 1);
  gfx::Rect rect, caret;
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      caret, rect);

  // Set the page scale to be smaller than the minimal readable scale.
  float initial_scale = min_readable_caret_height / caret.height() * 0.5f;
  SetScaleAndScrollAndLayout(web_view_helper.GetWebView(), gfx::Point(),
                             initial_scale);

  float scale;
  IntPoint scroll;
  bool need_animation;
  IntRect element_bounds, caret_bounds;
  GetElementAndCaretBoundsForFocusedEditableElement(
      web_view_helper, element_bounds, caret_bounds);
  web_view_helper.GetWebView()->ComputeScaleAndScrollForEditableElementRects(
      element_bounds, caret_bounds, kAutoZoomToLegibleScale, scale, scroll,
      need_animation);
  EXPECT_TRUE(need_animation);
  // The edit box should be left aligned with a margin for possible label.
  int h_scroll = edit_box_with_text.x - left_box_ratio * viewport_width / scale;
  EXPECT_NEAR(h_scroll, scroll.X(), 2);
  int v_scroll = edit_box_with_text.y -
                 (viewport_height / scale - edit_box_with_text.height) / 2;
  EXPECT_NEAR(v_scroll, scroll.Y(), 2);
  EXPECT_NEAR(min_readable_caret_height / caret.height(), scale, 0.1);

  // The edit box is wider than the viewport when legible.
  viewport_width = 200;
  viewport_height = 150;
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  SetScaleAndScrollAndLayout(web_view_helper.GetWebView(), gfx::Point(),
                             initial_scale);
  GetElementAndCaretBoundsForFocusedEditableElement(
      web_view_helper, element_bounds, caret_bounds);
  web_view_helper.GetWebView()->ComputeScaleAndScrollForEditableElementRects(
      element_bounds, caret_bounds, kAutoZoomToLegibleScale, scale, scroll,
      need_animation);
  EXPECT_TRUE(need_animation);
  // The caret should be right aligned since the caret would be offscreen when
  // the edit box is left aligned.
  h_scroll = caret.x() + caret.width() + caret_padding - viewport_width / scale;
  EXPECT_NEAR(h_scroll, scroll.X(), 2);
  EXPECT_NEAR(min_readable_caret_height / caret.height(), scale, 0.1);

  SetScaleAndScrollAndLayout(web_view_helper.GetWebView(), gfx::Point(),
                             initial_scale);
  // Move focus to edit box with text.
  web_view_helper.GetWebView()->AdvanceFocus(false);
  GetElementAndCaretBoundsForFocusedEditableElement(
      web_view_helper, element_bounds, caret_bounds);
  web_view_helper.GetWebView()->ComputeScaleAndScrollForEditableElementRects(
      element_bounds, caret_bounds, kAutoZoomToLegibleScale, scale, scroll,
      need_animation);
  EXPECT_TRUE(need_animation);
  // The edit box should be left aligned.
  h_scroll = edit_box_with_no_text.x;
  EXPECT_NEAR(h_scroll, scroll.X(), 2);
  v_scroll = edit_box_with_no_text.y -
             (viewport_height / scale - edit_box_with_no_text.height) / 2;
  EXPECT_NEAR(v_scroll, scroll.Y(), 2);
  EXPECT_NEAR(min_readable_caret_height / caret.height(), scale, 0.1);

  // Move focus back to the first edit box.
  web_view_helper.GetWebView()->AdvanceFocus(true);
  // Zoom out slightly.
  const float within_tolerance_scale = scale * 0.9f;
  SetScaleAndScrollAndLayout(web_view_helper.GetWebView(), scroll,
                             within_tolerance_scale);
  // Move focus back to the second edit box.
  web_view_helper.GetWebView()->AdvanceFocus(false);
  GetElementAndCaretBoundsForFocusedEditableElement(
      web_view_helper, element_bounds, caret_bounds);
  web_view_helper.GetWebView()->ComputeScaleAndScrollForEditableElementRects(
      element_bounds, caret_bounds, kAutoZoomToLegibleScale, scale, scroll,
      need_animation);
  // The scale should not be adjusted as the zoomed out scale was sufficiently
  // close to the previously focused scale.
  EXPECT_FALSE(need_animation);
}

TEST_F(WebFrameTest, DivScrollIntoEditablePreservePageScaleTest) {
  RegisterMockedHttpURLLoad("get_scale_for_zoom_into_editable_test.html");

  const bool kAutoZoomToLegibleScale = true;
  const int kViewportWidth = 450;
  const int kViewportHeight = 300;
  const float kMinReadableCaretHeight = 16.0f;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "get_scale_for_zoom_into_editable_test.html");
  web_view_helper.GetWebView()
      ->GetPage()
      ->GetSettings()
      .SetTextAutosizingEnabled(false);
  web_view_helper.Resize(gfx::Size(kViewportWidth, kViewportHeight));
  web_view_helper.GetWebView()->EnableFakePageScaleAnimationForTesting(true);

  const WebRect edit_box_with_text(200, 200, 250, 20);

  web_view_helper.GetWebView()->AdvanceFocus(false);
  // Set the caret to the begining of the input box.
  web_view_helper.GetWebView()
      ->MainFrameImpl()
      ->GetDocument()
      .GetElementById("EditBoxWithText")
      .To<WebInputElement>()
      .SetSelectionRange(0, 0);
  SetScaleAndScrollAndLayout(web_view_helper.GetWebView(), gfx::Point(), 1);
  gfx::Rect rect, caret;
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      caret, rect);

  // Set the page scale to be twice as large as the minimal readable scale.
  float new_scale = kMinReadableCaretHeight / caret.height() * 2.0;
  SetScaleAndScrollAndLayout(web_view_helper.GetWebView(), gfx::Point(),
                             new_scale);

  float scale;
  IntPoint scroll;
  bool need_animation;
  IntRect element_bounds, caret_bounds;
  GetElementAndCaretBoundsForFocusedEditableElement(
      web_view_helper, element_bounds, caret_bounds);
  web_view_helper.GetWebView()->ComputeScaleAndScrollForEditableElementRects(
      element_bounds, caret_bounds, kAutoZoomToLegibleScale, scale, scroll,
      need_animation);
  EXPECT_TRUE(need_animation);
  // Edit box and caret should be left alinged
  int h_scroll = edit_box_with_text.x;
  EXPECT_NEAR(h_scroll, scroll.X(), 1);
  int v_scroll = edit_box_with_text.y -
                 (kViewportHeight / scale - edit_box_with_text.height) / 2;
  EXPECT_NEAR(v_scroll, scroll.Y(), 1);
  // Page scale have to be unchanged
  EXPECT_EQ(new_scale, scale);

  // Set page scale and scroll such that edit box will be under the screen
  new_scale = 3.0;
  h_scroll = 200;
  SetScaleAndScrollAndLayout(web_view_helper.GetWebView(),
                             gfx::Point(h_scroll, 0), new_scale);
  GetElementAndCaretBoundsForFocusedEditableElement(
      web_view_helper, element_bounds, caret_bounds);
  web_view_helper.GetWebView()->ComputeScaleAndScrollForEditableElementRects(
      element_bounds, caret_bounds, kAutoZoomToLegibleScale, scale, scroll,
      need_animation);
  EXPECT_TRUE(need_animation);
  // Horizontal scroll have to be the same
  EXPECT_NEAR(h_scroll, scroll.X(), 1);
  v_scroll = edit_box_with_text.y -
             (kViewportHeight / scale - edit_box_with_text.height) / 2;
  EXPECT_NEAR(v_scroll, scroll.Y(), 1);
  // Page scale have to be unchanged
  EXPECT_EQ(new_scale, scale);
}

// Tests the scroll into view functionality when
// autoZoomeFocusedNodeToLegibleScale set to false. i.e. The path non-Android
// platforms take.
TEST_F(WebFrameTest, DivScrollIntoEditableTestZoomToLegibleScaleDisabled) {
  RegisterMockedHttpURLLoad("get_scale_for_zoom_into_editable_test.html");

  const bool kAutoZoomToLegibleScale = false;
  int viewport_width = 100;
  int viewport_height = 100;
  float left_box_ratio = 0.3f;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "get_scale_for_zoom_into_editable_test.html");
  web_view_helper.GetWebView()
      ->GetPage()
      ->GetSettings()
      .SetTextAutosizingEnabled(false);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.25f, 4);

  web_view_helper.GetWebView()->EnableFakePageScaleAnimationForTesting(true);

  WebRect edit_box_with_no_text(200, 250, 250, 20);

  // Test scrolling the focused node
  // Since we're zoomed out, the caret is considered too small to be legible and
  // so we'd normally zoom in. Make sure we don't change scale since the
  // auto-zoom setting is off.

  // Focus the second empty textbox.
  web_view_helper.GetWebView()->AdvanceFocus(false);
  web_view_helper.GetWebView()->AdvanceFocus(false);

  // Set the page scale to be smaller than the minimal readable scale.
  float initial_scale = 0.25f;
  SetScaleAndScrollAndLayout(web_view_helper.GetWebView(), gfx::Point(),
                             initial_scale);

  float scale;
  IntPoint scroll;
  bool need_animation;
  IntRect element_bounds, caret_bounds;
  GetElementAndCaretBoundsForFocusedEditableElement(
      web_view_helper, element_bounds, caret_bounds);
  web_view_helper.GetWebView()->ComputeScaleAndScrollForEditableElementRects(
      element_bounds, caret_bounds, kAutoZoomToLegibleScale, scale, scroll,
      need_animation);

  // There should be no change in page scale.
  EXPECT_EQ(initial_scale, scale);
  // The edit box should be left aligned with a margin for possible label.
  EXPECT_TRUE(need_animation);
  int h_scroll =
      edit_box_with_no_text.x - left_box_ratio * viewport_width / scale;
  EXPECT_NEAR(h_scroll, scroll.X(), 2);
  int v_scroll = edit_box_with_no_text.y -
                 (viewport_height / scale - edit_box_with_no_text.height) / 2;
  EXPECT_NEAR(v_scroll, scroll.Y(), 2);

  SetScaleAndScrollAndLayout(web_view_helper.GetWebView(), scroll, scale);

  // Select the first textbox.
  web_view_helper.GetWebView()->AdvanceFocus(true);
  gfx::Rect rect, caret;
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      caret, rect);
  GetElementAndCaretBoundsForFocusedEditableElement(
      web_view_helper, element_bounds, caret_bounds);
  web_view_helper.GetWebView()->ComputeScaleAndScrollForEditableElementRects(
      element_bounds, caret_bounds, kAutoZoomToLegibleScale, scale, scroll,
      need_animation);

  // There should be no change at all since the textbox is fully visible
  // already.
  EXPECT_EQ(initial_scale, scale);
  EXPECT_FALSE(need_animation);
}

// Tests zoom into editable zoom and scroll correctly when zoom-for-dsf enabled.
TEST_F(WebFrameTest, DivScrollIntoEditableTestWithDeviceScaleFactor) {
  RegisterMockedHttpURLLoad("get_scale_for_zoom_into_editable_test.html");

  bool kAutoZoomToLegibleScale = true;
  const float kDeviceScaleFactor = 2.f;
  int viewport_width = 200 * kDeviceScaleFactor;
  int viewport_height = 150 * kDeviceScaleFactor;
  float min_readable_caret_height = 16.0f * kDeviceScaleFactor;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "get_scale_for_zoom_into_editable_test.html", nullptr,
      nullptr, nullptr, ConfigureAndroid);
  web_view_helper.GetWebView()
      ->GetPage()
      ->GetSettings()
      .SetTextAutosizingEnabled(false);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  web_view_helper.GetWebView()->SetZoomFactorForDeviceScaleFactor(
      kDeviceScaleFactor);
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.25f, 4);

  web_view_helper.GetWebView()->EnableFakePageScaleAnimationForTesting(true);

  WebRect edit_box_with_text(200 * kDeviceScaleFactor, 200 * kDeviceScaleFactor,
                             250 * kDeviceScaleFactor, 20 * kDeviceScaleFactor);
  web_view_helper.GetWebView()->AdvanceFocus(false);

  // Set the page scale to be smaller than the minimal readable scale.
  float initial_scale = 0.5f;
  SetScaleAndScrollAndLayout(web_view_helper.GetWebView(), gfx::Point(),
                             initial_scale);
  ASSERT_EQ(web_view_helper.GetWebView()->PageScaleFactor(), initial_scale);

  float scale;
  IntPoint scroll;
  bool need_animation;
  IntRect element_bounds, caret_bounds;
  GetElementAndCaretBoundsForFocusedEditableElement(
      web_view_helper, element_bounds, caret_bounds);
  web_view_helper.GetWebView()->ComputeScaleAndScrollForEditableElementRects(
      element_bounds, caret_bounds, kAutoZoomToLegibleScale, scale, scroll,
      need_animation);
  EXPECT_TRUE(need_animation);
  // The edit box wider than the viewport when legible should be left aligned.
  int h_scroll = edit_box_with_text.x;
  EXPECT_NEAR(h_scroll, scroll.X(), 2);
  int v_scroll = edit_box_with_text.y -
                 (viewport_height / scale - edit_box_with_text.height) / 2;
  EXPECT_NEAR(v_scroll, scroll.Y(), 2);
  EXPECT_NEAR(min_readable_caret_height / caret_bounds.Height(), scale, 0.1);
}

TEST_F(WebFrameTest, FirstRectForCharacterRangeWithPinchZoom) {
  RegisterMockedHttpURLLoad("textbox.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "textbox.html");
  web_view_helper.Resize(gfx::Size(640, 480));

  WebLocalFrame* main_frame = web_view_helper.LocalMainFrame();
  main_frame->ExecuteScript(WebScriptSource("selectRange();"));

  WebRect old_rect;
  main_frame->FirstRectForCharacterRange(0, 5, old_rect);

  gfx::PointF visual_offset(100, 130);
  float scale = 2;
  web_view_helper.GetWebView()->SetPageScaleFactor(scale);
  web_view_helper.GetWebView()->SetVisualViewportOffset(visual_offset);

  WebRect rect;
  main_frame->FirstRectForCharacterRange(0, 5, rect);

  EXPECT_EQ((old_rect.x - visual_offset.x()) * scale, rect.x);
  EXPECT_EQ((old_rect.y - visual_offset.y()) * scale, rect.y);
  EXPECT_EQ(old_rect.width * scale, rect.width);
  EXPECT_EQ(old_rect.height * scale, rect.height);
}
class TestReloadDoesntRedirectWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  TestReloadDoesntRedirectWebFrameClient() = default;
  ~TestReloadDoesntRedirectWebFrameClient() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void BeginNavigation(std::unique_ptr<WebNavigationInfo> info) override {
    EXPECT_FALSE(info->is_client_redirect);
    TestWebFrameClient::BeginNavigation(std::move(info));
  }
};

TEST_F(WebFrameTest, ReloadDoesntSetRedirect) {
  // Test for case in http://crbug.com/73104. Reloading a frame very quickly
  // would sometimes call BeginNavigation with isRedirect=true
  RegisterMockedHttpURLLoad("form.html");

  TestReloadDoesntRedirectWebFrameClient web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "form.html", &web_frame_client);

  web_view_helper.GetWebView()->MainFrameImpl()->StartReload(
      WebFrameLoadType::kReloadBypassingCache);
  // start another reload before request is delivered.
  frame_test_helpers::ReloadFrameBypassingCache(
      web_view_helper.GetWebView()->MainFrameImpl());
}

class ClearScrollStateOnCommitWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  ClearScrollStateOnCommitWebFrameClient() = default;
  ~ClearScrollStateOnCommitWebFrameClient() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void DidCommitNavigation(const WebHistoryItem&,
                           WebHistoryCommitType,
                           bool) override {
    Frame()->View()->ResetScrollAndScaleState();
  }
};

TEST_F(WebFrameTest, ReloadPreservesState) {
  const std::string url = "200-by-300.html";
  const float kPageScaleFactor = 1.1684f;
  const int kPageWidth = 120;
  const int kPageHeight = 100;

  RegisterMockedHttpURLLoad(url);

  ClearScrollStateOnCommitWebFrameClient client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + url, &client);
  web_view_helper.Resize(gfx::Size(kPageWidth, kPageHeight));
  web_view_helper.LocalMainFrame()->SetScrollOffset(
      WebSize(kPageWidth / 4, kPageHeight / 4));
  web_view_helper.GetWebView()->SetPageScaleFactor(kPageScaleFactor);

  // Reload the page and end up at the same url. State should not be propagated.
  web_view_helper.GetWebView()->MainFrameImpl()->StartReload(
      WebFrameLoadType::kReload);
  frame_test_helpers::PumpPendingRequestsForFrameToLoad(
      web_view_helper.LocalMainFrame());
  EXPECT_EQ(0, web_view_helper.LocalMainFrame()->GetScrollOffset().width);
  EXPECT_EQ(0, web_view_helper.LocalMainFrame()->GetScrollOffset().height);
  EXPECT_EQ(1.0f, web_view_helper.GetWebView()->PageScaleFactor());
}

TEST_F(WebFrameTest, ReloadWhileProvisional) {
  // Test that reloading while the previous load is still pending does not cause
  // the initial request to get lost.
  RegisterMockedHttpURLLoad("fixed_layout.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();
  WebURLRequest request(ToKURL(base_url_ + "fixed_layout.html"));
  web_view_helper.GetWebView()->MainFrameImpl()->StartNavigation(request);
  // start reload before first request is delivered.
  frame_test_helpers::ReloadFrameBypassingCache(
      web_view_helper.GetWebView()->MainFrameImpl());

  WebDocumentLoader* document_loader =
      web_view_helper.LocalMainFrame()->GetDocumentLoader();
  ASSERT_TRUE(document_loader);
  EXPECT_EQ(ToKURL(base_url_ + "fixed_layout.html"),
            KURL(document_loader->GetUrl()));
}

TEST_F(WebFrameTest, RedirectChainContainsInitialUrl) {
  const std::string first_url = "data:text/html,foo";

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(first_url);

  WebDocumentLoader* document_loader =
      web_view_helper.LocalMainFrame()->GetDocumentLoader();
  ASSERT_TRUE(document_loader);

  WebVector<WebURL> redirects;
  document_loader->RedirectChain(redirects);
  ASSERT_EQ(1U, redirects.size());
  EXPECT_EQ(ToKURL(first_url), KURL(redirects[0]));
}

TEST_F(WebFrameTest, IframeRedirect) {
  RegisterMockedHttpURLLoad("iframe_redirect.html");
  RegisterMockedHttpURLLoad("visible_iframe.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "iframe_redirect.html");
  // Pump pending requests one more time. The test page loads script that
  // navigates.
  frame_test_helpers::PumpPendingRequestsForFrameToLoad(
      web_view_helper.LocalMainFrame());

  WebFrame* iframe = web_view_helper.LocalMainFrame()->FindFrameByName(
      WebString::FromUTF8("ifr"));
  ASSERT_TRUE(iframe && iframe->IsWebLocalFrame());
  WebDocumentLoader* iframe_document_loader =
      iframe->ToWebLocalFrame()->GetDocumentLoader();
  ASSERT_TRUE(iframe_document_loader);
  WebVector<WebURL> redirects;
  iframe_document_loader->RedirectChain(redirects);
  ASSERT_EQ(2U, redirects.size());
  EXPECT_EQ(ToKURL("about:blank"), KURL(redirects[0]));
  EXPECT_EQ(ToKURL("http://internal.test/visible_iframe.html"),
            KURL(redirects[1]));
}

TEST_F(WebFrameTest, ClearFocusedNodeTest) {
  RegisterMockedHttpURLLoad("iframe_clear_focused_node_test.html");
  RegisterMockedHttpURLLoad("autofocus_input_field_iframe.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ +
                                    "iframe_clear_focused_node_test.html");

  // Clear the focused node.
  web_view_helper.GetWebView()->FocusedElement()->blur();

  // Now retrieve the FocusedNode and test it should be null.
  EXPECT_EQ(nullptr, web_view_helper.GetWebView()->FocusedElement());
}

class ChangedSelectionCounter : public frame_test_helpers::TestWebFrameClient {
 public:
  ChangedSelectionCounter() : call_count_(0) {}
  void DidChangeSelection(bool isSelectionEmpty) override { ++call_count_; }
  int Count() const { return call_count_; }
  void Reset() { call_count_ = 0; }

 private:
  int call_count_;
};

TEST_F(WebFrameTest, TabKeyCursorMoveTriggersOneSelectionChange) {
  ChangedSelectionCounter counter;
  frame_test_helpers::WebViewHelper web_view_helper;
  RegisterMockedHttpURLLoad("editable_elements.html");
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "editable_elements.html", &counter);

  WebKeyboardEvent tab_down(WebInputEvent::Type::kKeyDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests());
  WebKeyboardEvent tab_up(WebInputEvent::Type::kKeyUp,
                          WebInputEvent::kNoModifiers,
                          WebInputEvent::GetStaticTimeStampForTests());
  tab_down.dom_key = ui::DomKey::TAB;
  tab_up.dom_key = ui::DomKey::TAB;
  tab_down.windows_key_code = VKEY_TAB;
  tab_up.windows_key_code = VKEY_TAB;

  // Move to the next text-field: 1 cursor change.
  counter.Reset();
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(tab_down, ui::LatencyInfo()));
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(tab_up, ui::LatencyInfo()));
  EXPECT_EQ(1, counter.Count());

  // Move to another text-field: 1 cursor change.
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(tab_down, ui::LatencyInfo()));
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(tab_up, ui::LatencyInfo()));
  EXPECT_EQ(2, counter.Count());

  // Move to a number-field: 1 cursor change.
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(tab_down, ui::LatencyInfo()));
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(tab_up, ui::LatencyInfo()));
  EXPECT_EQ(3, counter.Count());

  // Move to an editable element: 1 cursor change.
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(tab_down, ui::LatencyInfo()));
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(tab_up, ui::LatencyInfo()));
  EXPECT_EQ(4, counter.Count());

  // Move to a non-editable element: 0 cursor changes.
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(tab_down, ui::LatencyInfo()));
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(tab_up, ui::LatencyInfo()));
  EXPECT_EQ(4, counter.Count());
}

// Implementation of WebLocalFrameClient that tracks the v8 contexts that are
// created and destroyed for verification.
class ContextLifetimeTestWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  struct Notification {
   public:
    Notification(WebLocalFrame* frame,
                 v8::Local<v8::Context> context,
                 int32_t world_id)
        : frame(frame),
          context(context->GetIsolate(), context),
          world_id(world_id) {}

    ~Notification() { context.Reset(); }

    bool Equals(Notification* other) {
      return other && frame == other->frame && context == other->context &&
             world_id == other->world_id;
    }

    WebLocalFrame* frame;
    v8::Persistent<v8::Context> context;
    int32_t world_id;
  };

  ContextLifetimeTestWebFrameClient(
      Vector<std::unique_ptr<Notification>>& create_notifications,
      Vector<std::unique_ptr<Notification>>& release_notifications)
      : create_notifications_(create_notifications),
        release_notifications_(release_notifications) {}
  ~ContextLifetimeTestWebFrameClient() override = default;

  void Reset() {
    create_notifications_.clear();
    release_notifications_.clear();
  }

  // WebLocalFrameClient:
  WebLocalFrame* CreateChildFrame(
      WebLocalFrame* parent,
      mojom::blink::TreeScopeType scope,
      const WebString& name,
      const WebString& fallback_name,
      const FramePolicy&,
      const WebFrameOwnerProperties&,
      mojom::blink::FrameOwnerElementType) override {
    return CreateLocalChild(*parent, scope,
                            std::make_unique<ContextLifetimeTestWebFrameClient>(
                                create_notifications_, release_notifications_));
  }

  void DidCreateScriptContext(v8::Local<v8::Context> context,
                              int32_t world_id) override {
    create_notifications_.push_back(
        std::make_unique<Notification>(Frame(), context, world_id));
  }

  void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                int32_t world_id) override {
    release_notifications_.push_back(
        std::make_unique<Notification>(Frame(), context, world_id));
  }

 private:
  Vector<std::unique_ptr<Notification>>& create_notifications_;
  Vector<std::unique_ptr<Notification>>& release_notifications_;
};

TEST_F(WebFrameTest, ContextNotificationsLoadUnload) {
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());

  RegisterMockedHttpURLLoad("context_notifications_test.html");
  RegisterMockedHttpURLLoad("context_notifications_test_frame.html");

  // Load a frame with an iframe, make sure we get the right create
  // notifications.
  Vector<std::unique_ptr<ContextLifetimeTestWebFrameClient::Notification>>
      create_notifications;
  Vector<std::unique_ptr<ContextLifetimeTestWebFrameClient::Notification>>
      release_notifications;
  ContextLifetimeTestWebFrameClient web_frame_client(create_notifications,
                                                     release_notifications);
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "context_notifications_test.html", &web_frame_client);

  WebLocalFrameImpl* main_frame = web_view_helper.LocalMainFrame();
  WebFrame* child_frame = main_frame->FirstChild();

  ASSERT_EQ(2u, create_notifications.size());
  EXPECT_EQ(0u, release_notifications.size());

  auto& first_create_notification = create_notifications[0];
  auto& second_create_notification = create_notifications[1];

  EXPECT_EQ(main_frame, first_create_notification->frame);
  EXPECT_EQ(main_frame->MainWorldScriptContext(),
            first_create_notification->context);
  EXPECT_EQ(0, first_create_notification->world_id);

  EXPECT_EQ(child_frame, second_create_notification->frame);
  EXPECT_EQ(child_frame->ToWebLocalFrame()->MainWorldScriptContext(),
            second_create_notification->context);
  EXPECT_EQ(0, second_create_notification->world_id);

  // Close the view. We should get two release notifications that are exactly
  // the same as the create ones, in reverse order.
  web_view_helper.Reset();

  ASSERT_EQ(2u, release_notifications.size());
  auto& first_release_notification = release_notifications[0];
  auto& second_release_notification = release_notifications[1];

  ASSERT_TRUE(
      first_create_notification->Equals(second_release_notification.get()));
  ASSERT_TRUE(
      second_create_notification->Equals(first_release_notification.get()));
}

TEST_F(WebFrameTest, ContextNotificationsReload) {
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());

  RegisterMockedHttpURLLoad("context_notifications_test.html");
  RegisterMockedHttpURLLoad("context_notifications_test_frame.html");

  Vector<std::unique_ptr<ContextLifetimeTestWebFrameClient::Notification>>
      create_notifications;
  Vector<std::unique_ptr<ContextLifetimeTestWebFrameClient::Notification>>
      release_notifications;
  ContextLifetimeTestWebFrameClient web_frame_client(create_notifications,
                                                     release_notifications);
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "context_notifications_test.html", &web_frame_client);

  // Refresh, we should get two release notifications and two more create
  // notifications.
  frame_test_helpers::ReloadFrame(
      web_view_helper.GetWebView()->MainFrameImpl());
  ASSERT_EQ(4u, create_notifications.size());
  ASSERT_EQ(2u, release_notifications.size());

  // The two release notifications we got should be exactly the same as the
  // first two create notifications.
  for (size_t i = 0; i < release_notifications.size(); ++i) {
    EXPECT_TRUE(release_notifications[i]->Equals(
        create_notifications[create_notifications.size() - 3 - i].get()));
  }

  // The last two create notifications should be for the current frames and
  // context.
  WebLocalFrameImpl* main_frame = web_view_helper.LocalMainFrame();
  WebFrame* child_frame = main_frame->FirstChild();
  auto& first_refresh_notification = create_notifications[2];
  auto& second_refresh_notification = create_notifications[3];

  EXPECT_EQ(main_frame, first_refresh_notification->frame);
  EXPECT_EQ(main_frame->MainWorldScriptContext(),
            first_refresh_notification->context);
  EXPECT_EQ(0, first_refresh_notification->world_id);

  EXPECT_EQ(child_frame, second_refresh_notification->frame);
  EXPECT_EQ(child_frame->ToWebLocalFrame()->MainWorldScriptContext(),
            second_refresh_notification->context);
  EXPECT_EQ(0, second_refresh_notification->world_id);
}

TEST_F(WebFrameTest, ContextNotificationsIsolatedWorlds) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);

  RegisterMockedHttpURLLoad("context_notifications_test.html");
  RegisterMockedHttpURLLoad("context_notifications_test_frame.html");

  Vector<std::unique_ptr<ContextLifetimeTestWebFrameClient::Notification>>
      create_notifications;
  Vector<std::unique_ptr<ContextLifetimeTestWebFrameClient::Notification>>
      release_notifications;
  ContextLifetimeTestWebFrameClient web_frame_client(create_notifications,
                                                     release_notifications);
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "context_notifications_test.html", &web_frame_client);

  // Add an isolated world.
  web_frame_client.Reset();

  int32_t isolated_world_id = 42;
  WebScriptSource script_source("hi!");
  web_view_helper.LocalMainFrame()->ExecuteScriptInIsolatedWorld(
      isolated_world_id, script_source);

  // We should now have a new create notification.
  ASSERT_EQ(1u, create_notifications.size());
  auto& notification = create_notifications[0];
  ASSERT_EQ(isolated_world_id, notification->world_id);
  ASSERT_EQ(web_view_helper.GetWebView()->MainFrame(), notification->frame);

  // We don't have an API to enumarate isolated worlds for a frame, but we can
  // at least assert that the context we got is *not* the main world's context.
  ASSERT_NE(web_view_helper.LocalMainFrame()->MainWorldScriptContext(),
            v8::Local<v8::Context>::New(isolate, notification->context));

  // Check that the context we got has the right isolated world id.
  ASSERT_EQ(isolated_world_id,
            web_view_helper.LocalMainFrame()->GetScriptContextWorldId(
                v8::Local<v8::Context>::New(isolate, notification->context)));

  web_view_helper.Reset();

  // We should have gotten three release notifications (one for each of the
  // frames, plus one for the isolated context).
  ASSERT_EQ(3u, release_notifications.size());

  // And one of them should be exactly the same as the create notification for
  // the isolated context.
  int match_count = 0;
  for (size_t i = 0; i < release_notifications.size(); ++i) {
    if (release_notifications[i]->Equals(create_notifications[0].get()))
      ++match_count;
  }
  EXPECT_EQ(1, match_count);
}

TEST_F(WebFrameTest, FindInPage) {
  RegisterMockedHttpURLLoad("find.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "find.html");
  ASSERT_TRUE(web_view_helper.LocalMainFrame());
  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  const int kFindIdentifier = 12345;
  auto options = mojom::blink::FindOptions::New();

  // Find in a <div> element.
  EXPECT_TRUE(frame->GetFindInPage()->FindInternal(
      kFindIdentifier, WebString::FromUTF8("bar1"), *options, false));
  frame->GetFindInPage()->StopFinding(
      blink::mojom::StopFindAction::kStopFindActionKeepSelection);
  WebRange range = frame->SelectionRange();
  EXPECT_EQ(5, range.StartOffset());
  EXPECT_EQ(9, range.EndOffset());
  EXPECT_TRUE(frame->GetDocument().FocusedElement().IsNull());

  // Find in an <input> value.
  EXPECT_TRUE(frame->GetFindInPage()->FindInternal(
      kFindIdentifier, WebString::FromUTF8("bar2"), *options, false));
  // Confirm stopFinding(WebLocalFrame::StopFindActionKeepSelection) sets the
  // selection on the found text.
  frame->GetFindInPage()->StopFinding(
      blink::mojom::StopFindAction::kStopFindActionKeepSelection);
  range = frame->SelectionRange();
  ASSERT_FALSE(range.IsNull());
  EXPECT_EQ(5, range.StartOffset());
  EXPECT_EQ(9, range.EndOffset());
  EXPECT_TRUE(frame->GetDocument().FocusedElement().HasHTMLTagName("input"));

  // Find in a <textarea> content.
  EXPECT_TRUE(frame->GetFindInPage()->FindInternal(
      kFindIdentifier, WebString::FromUTF8("bar3"), *options, false));
  // Confirm stopFinding(WebLocalFrame::StopFindActionKeepSelection) sets the
  // selection on the found text.
  frame->GetFindInPage()->StopFinding(
      blink::mojom::StopFindAction::kStopFindActionKeepSelection);
  range = frame->SelectionRange();
  ASSERT_FALSE(range.IsNull());
  EXPECT_EQ(5, range.StartOffset());
  EXPECT_EQ(9, range.EndOffset());
  EXPECT_TRUE(frame->GetDocument().FocusedElement().HasHTMLTagName("textarea"));

  // Find in a contentEditable element.
  EXPECT_TRUE(frame->GetFindInPage()->FindInternal(
      kFindIdentifier, WebString::FromUTF8("bar4"), *options, false));
  // Confirm stopFinding(WebLocalFrame::StopFindActionKeepSelection) sets the
  // selection on the found text.
  frame->GetFindInPage()->StopFinding(
      blink::mojom::StopFindAction::kStopFindActionKeepSelection);
  range = frame->SelectionRange();
  ASSERT_FALSE(range.IsNull());
  EXPECT_EQ(0, range.StartOffset());
  EXPECT_EQ(4, range.EndOffset());
  // "bar4" is surrounded by <span>, but the focusable node should be the parent
  // <div>.
  EXPECT_TRUE(frame->GetDocument().FocusedElement().HasHTMLTagName("div"));

  // Find in <select> content.
  EXPECT_FALSE(frame->GetFindInPage()->FindInternal(
      kFindIdentifier, WebString::FromUTF8("bar5"), *options, false));
  // If there are any matches, stopFinding will set the selection on the found
  // text.  However, we do not expect any matches, so check that the selection
  // is null.
  frame->GetFindInPage()->StopFinding(
      blink::mojom::StopFindAction::kStopFindActionKeepSelection);
  range = frame->SelectionRange();
  ASSERT_TRUE(range.IsNull());
}

TEST_F(WebFrameTest, GetContentAsPlainText) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank");
  // We set the size because it impacts line wrapping, which changes the
  // resulting text value.
  web_view_helper.Resize(gfx::Size(640, 480));
  WebLocalFrame* frame = web_view_helper.LocalMainFrame();

  // Generate a simple test case.
  const char kSimpleSource[] = "<div>Foo bar</div><div></div>baz";
  KURL test_url = ToKURL("about:blank");
  frame_test_helpers::LoadHTMLString(frame, kSimpleSource, test_url);

  // Make sure it comes out OK.
  const std::string expected("Foo bar\nbaz");
  WebString text = WebFrameContentDumper::DumpWebViewAsText(
      web_view_helper.GetWebView(), std::numeric_limits<size_t>::max());
  EXPECT_EQ(expected, text.Utf8());

  // Try reading the same one with clipping of the text.
  const int kLength = 5;
  text = WebFrameContentDumper::DumpWebViewAsText(web_view_helper.GetWebView(),
                                                  kLength);
  EXPECT_EQ(expected.substr(0, kLength), text.Utf8());

  // Now do a new test with a subframe.
  const char kOuterFrameSource[] = "Hello<iframe></iframe> world";
  frame_test_helpers::LoadHTMLString(frame, kOuterFrameSource, test_url);

  // Load something into the subframe.
  WebLocalFrame* subframe = frame->FirstChild()->ToWebLocalFrame();
  ASSERT_TRUE(subframe);
  frame_test_helpers::LoadHTMLString(subframe, "sub<p>text", test_url);

  text = WebFrameContentDumper::DumpWebViewAsText(
      web_view_helper.GetWebView(), std::numeric_limits<size_t>::max());
  EXPECT_EQ("Hello world\n\nsub\n\ntext", text.Utf8());

  // Get the frame text where the subframe separator falls on the boundary of
  // what we'll take. There used to be a crash in this case.
  text = WebFrameContentDumper::DumpWebViewAsText(web_view_helper.GetWebView(),
                                                  12);
  EXPECT_EQ("Hello world", text.Utf8());
}

TEST_F(WebFrameTest, GetFullHtmlOfPage) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank");
  WebLocalFrame* frame = web_view_helper.LocalMainFrame();

  // Generate a simple test case.
  const char kSimpleSource[] = "<p>Hello</p><p>World</p>";
  KURL test_url = ToKURL("about:blank");
  frame_test_helpers::LoadHTMLString(frame, kSimpleSource, test_url);

  WebString text = WebFrameContentDumper::DumpWebViewAsText(
      web_view_helper.GetWebView(), std::numeric_limits<size_t>::max());
  EXPECT_EQ("Hello\n\nWorld", text.Utf8());

  const std::string html = WebFrameContentDumper::DumpAsMarkup(frame).Utf8();

  // Load again with the output html.
  frame_test_helpers::LoadHTMLString(frame, html, test_url);

  EXPECT_EQ(html, WebFrameContentDumper::DumpAsMarkup(frame).Utf8());

  text = WebFrameContentDumper::DumpWebViewAsText(
      web_view_helper.GetWebView(), std::numeric_limits<size_t>::max());
  EXPECT_EQ("Hello\n\nWorld", text.Utf8());

  // Test selection check
  EXPECT_FALSE(frame->HasSelection());
  frame->ExecuteCommand(WebString::FromUTF8("SelectAll"));
  EXPECT_TRUE(frame->HasSelection());
  frame->ExecuteCommand(WebString::FromUTF8("Unselect"));
  EXPECT_FALSE(frame->HasSelection());
  WebString selection_html = frame->SelectionAsMarkup();
  EXPECT_TRUE(selection_html.IsEmpty());
}

class TestExecuteScriptDuringDidCreateScriptContext
    : public frame_test_helpers::TestWebFrameClient {
 public:
  TestExecuteScriptDuringDidCreateScriptContext() = default;
  ~TestExecuteScriptDuringDidCreateScriptContext() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void DidCreateScriptContext(v8::Local<v8::Context> context,
                              int32_t world_id) override {
    Frame()->ExecuteScript(WebScriptSource("window.history = 'replaced';"));
  }
};

TEST_F(WebFrameTest, ExecuteScriptDuringDidCreateScriptContext) {
  RegisterMockedHttpURLLoad("hello_world.html");

  TestExecuteScriptDuringDidCreateScriptContext web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "hello_world.html",
                                    &web_frame_client);

  frame_test_helpers::ReloadFrame(
      web_view_helper.GetWebView()->MainFrameImpl());
}

class TestFindInPageClient : public mojom::blink::FindInPageClient {
 public:
  TestFindInPageClient()
      : find_results_are_ready_(false), count_(-1), active_index_(-1) {}

  ~TestFindInPageClient() override = default;

  void SetFrame(WebLocalFrameImpl* frame) {
    frame->GetFindInPage()->SetClient(receiver_.BindNewPipeAndPassRemote());
  }

  void SetNumberOfMatches(
      int request_id,
      unsigned int current_number_of_matches,
      mojom::blink::FindMatchUpdateType final_update) final {
    count_ = current_number_of_matches;
    find_results_are_ready_ =
        (final_update == mojom::blink::FindMatchUpdateType::kFinalUpdate);
  }

  void SetActiveMatch(int request_id,
                      const gfx::Rect& active_match_rect,
                      int active_match_ordinal,
                      mojom::blink::FindMatchUpdateType final_update) final {
    active_index_ = active_match_ordinal;
    find_results_are_ready_ =
        (final_update == mojom::blink::FindMatchUpdateType::kFinalUpdate);
  }

  bool FindResultsAreReady() const { return find_results_are_ready_; }
  int Count() const { return count_; }
  int ActiveIndex() const { return active_index_; }

 private:
  bool find_results_are_ready_;
  int count_;
  int active_index_;
  mojo::Receiver<mojom::blink::FindInPageClient> receiver_{this};
};

TEST_F(WebFrameTest, FindInPageMatchRects) {
  RegisterMockedHttpURLLoad("find_in_page_frame.html");

  frame_test_helpers::TestWebFrameClient frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "find_in_page_frame.html",
                                    &frame_client);
  web_view_helper.Resize(gfx::Size(640, 480));
  web_view_helper.GetWebView()->SetMaximumLegibleScale(1.f);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  RunPendingTasks();

  // Note that the 'result 19' in the <select> element is not expected to
  // produce a match. Also, results 00 and 01 are in a different frame that is
  // not included in this test.
  const char kFindString[] = "result";
  const int kFindIdentifier = 12345;
  const int kNumResults = 17;

  auto options = mojom::blink::FindOptions::New();
  options->run_synchronously_for_testing = true;
  WebString search_text = WebString::FromUTF8(kFindString);
  WebLocalFrameImpl* main_frame = web_view_helper.LocalMainFrame();
  TestFindInPageClient find_in_page_client;
  find_in_page_client.SetFrame(main_frame);
  EXPECT_TRUE(main_frame->GetFindInPage()->FindInternal(
      kFindIdentifier, search_text, *options, false));

  main_frame->EnsureTextFinder().ResetMatchCount();

  for (WebLocalFrameImpl* frame = main_frame; frame;
       frame = To<WebLocalFrameImpl>(frame->TraverseNext())) {
    frame->EnsureTextFinder().StartScopingStringMatches(kFindIdentifier,
                                                        search_text, *options);
  }
  RunPendingTasks();
  EXPECT_TRUE(find_in_page_client.FindResultsAreReady());

  WebVector<gfx::RectF> web_match_rects =
      main_frame->EnsureTextFinder().FindMatchRects();
  ASSERT_EQ(static_cast<size_t>(kNumResults), web_match_rects.size());
  int rects_version = main_frame->GetFindInPage()->FindMatchMarkersVersion();

  for (int result_index = 0; result_index < kNumResults; ++result_index) {
    FloatRect result_rect =
        static_cast<FloatRect>(web_match_rects[result_index]);

    // Select the match by the center of its rect.
    EXPECT_EQ(main_frame->EnsureTextFinder().SelectNearestFindMatch(
                  result_rect.Center(), nullptr),
              result_index + 1);

    // Check that the find result ordering matches with our expectations.
    Range* result = main_frame->GetTextFinder()->ActiveMatch();
    ASSERT_TRUE(result);
    result->setEnd(result->endContainer(), result->endOffset() + 3);
    EXPECT_EQ(result->GetText(),
              String::Format("%s %02d", kFindString, result_index + 2));

    // Verify that the expected match rect also matches the currently active
    // match.  Compare the enclosing rects to prevent precision issues caused by
    // CSS transforms.
    gfx::RectF active_match =
        main_frame->GetFindInPage()->ActiveFindMatchRect();
    EXPECT_EQ(EnclosingIntRect(FloatRect(active_match)),
              EnclosingIntRect(result_rect));

    // The rects version should not have changed.
    EXPECT_EQ(main_frame->GetFindInPage()->FindMatchMarkersVersion(),
              rects_version);
  }

  // Resizing should update the rects version.
  web_view_helper.Resize(gfx::Size(800, 600));
  RunPendingTasks();
  EXPECT_TRUE(main_frame->GetFindInPage()->FindMatchMarkersVersion() !=
              rects_version);
}

TEST_F(WebFrameTest, FindInPageActiveIndex) {
  RegisterMockedHttpURLLoad("find_match_count.html");

  frame_test_helpers::TestWebFrameClient frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "find_match_count.html",
                                    &frame_client);
  web_view_helper.GetWebView()->MainFrameViewWidget()->Resize(
      gfx::Size(640, 480));
  RunPendingTasks();

  const char* kFindString = "a";
  const int kFindIdentifier = 7777;
  const int kActiveIndex = 1;

  auto options = mojom::blink::FindOptions::New();
  options->run_synchronously_for_testing = true;
  WebString search_text = WebString::FromUTF8(kFindString);
  WebLocalFrameImpl* main_frame = web_view_helper.LocalMainFrame();
  TestFindInPageClient find_in_page_client;
  find_in_page_client.SetFrame(main_frame);

  EXPECT_TRUE(main_frame->GetFindInPage()->FindInternal(
      kFindIdentifier, search_text, *options, false));
  main_frame->EnsureTextFinder().ResetMatchCount();

  for (WebLocalFrameImpl* frame = main_frame; frame;
       frame = To<WebLocalFrameImpl>(frame->TraverseNext())) {
    frame->EnsureTextFinder().StartScopingStringMatches(kFindIdentifier,
                                                        search_text, *options);
  }
  RunPendingTasks();

  EXPECT_TRUE(main_frame->GetFindInPage()->FindInternal(
      kFindIdentifier, search_text, *options, false));
  main_frame->GetFindInPage()->StopFinding(
      mojom::StopFindAction::kStopFindActionClearSelection);

  for (WebLocalFrameImpl* frame = main_frame; frame;
       frame = To<WebLocalFrameImpl>(frame->TraverseNext())) {
    frame->EnsureTextFinder().StartScopingStringMatches(kFindIdentifier,
                                                        search_text, *options);
  }

  RunPendingTasks();
  EXPECT_TRUE(find_in_page_client.FindResultsAreReady());
  EXPECT_EQ(kActiveIndex, find_in_page_client.ActiveIndex());

  const char* kFindStringNew = "e";
  WebString search_text_new = WebString::FromUTF8(kFindStringNew);

  EXPECT_TRUE(main_frame->GetFindInPage()->FindInternal(
      kFindIdentifier, search_text_new, *options, false));
  main_frame->EnsureTextFinder().ResetMatchCount();

  for (WebLocalFrameImpl* frame = main_frame; frame;
       frame = To<WebLocalFrameImpl>(frame->TraverseNext())) {
    frame->EnsureTextFinder().StartScopingStringMatches(
        kFindIdentifier, search_text_new, *options);
  }

  RunPendingTasks();
  EXPECT_TRUE(find_in_page_client.FindResultsAreReady());
  EXPECT_EQ(kActiveIndex, find_in_page_client.ActiveIndex());
}

TEST_F(WebFrameTest, FindOnDetachedFrame) {
  RegisterMockedHttpURLLoad("find_in_page.html");
  RegisterMockedHttpURLLoad("find_in_page_frame.html");

  frame_test_helpers::TestWebFrameClient frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "find_in_page.html",
                                    &frame_client);
  web_view_helper.Resize(gfx::Size(640, 480));
  RunPendingTasks();

  const char kFindString[] = "result";
  const int kFindIdentifier = 12345;

  auto options = mojom::blink::FindOptions::New();
  options->run_synchronously_for_testing = true;
  WebString search_text = WebString::FromUTF8(kFindString);
  WebLocalFrameImpl* main_frame = web_view_helper.LocalMainFrame();
  TestFindInPageClient main_find_in_page_client;
  main_find_in_page_client.SetFrame(main_frame);

  auto* second_frame = To<WebLocalFrameImpl>(main_frame->TraverseNext());

  // Detach the frame before finding.
  RemoveElementById(main_frame, "frame");

  EXPECT_TRUE(main_frame->GetFindInPage()->FindInternal(
      kFindIdentifier, search_text, *options, false));
  EXPECT_FALSE(second_frame->GetFindInPage()->FindInternal(
      kFindIdentifier, search_text, *options, false));

  RunPendingTasks();
  EXPECT_FALSE(main_find_in_page_client.FindResultsAreReady());

  main_frame->EnsureTextFinder().ResetMatchCount();

  for (WebLocalFrameImpl* frame = main_frame; frame;
       frame = To<WebLocalFrameImpl>(frame->TraverseNext())) {
    frame->EnsureTextFinder().StartScopingStringMatches(kFindIdentifier,
                                                        search_text, *options);
  }

  RunPendingTasks();
  EXPECT_TRUE(main_find_in_page_client.FindResultsAreReady());
}

TEST_F(WebFrameTest, FindDetachFrameBeforeScopeStrings) {
  RegisterMockedHttpURLLoad("find_in_page.html");
  RegisterMockedHttpURLLoad("find_in_page_frame.html");

  frame_test_helpers::TestWebFrameClient frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "find_in_page.html",
                                    &frame_client);
  web_view_helper.Resize(gfx::Size(640, 480));
  RunPendingTasks();

  const char kFindString[] = "result";
  const int kFindIdentifier = 12345;

  auto options = mojom::blink::FindOptions::New();
  options->run_synchronously_for_testing = true;
  WebString search_text = WebString::FromUTF8(kFindString);
  WebLocalFrameImpl* main_frame = web_view_helper.LocalMainFrame();
  TestFindInPageClient find_in_page_client;
  find_in_page_client.SetFrame(main_frame);

  for (WebLocalFrameImpl* frame = main_frame; frame;
       frame = To<WebLocalFrameImpl>(frame->TraverseNext())) {
    EXPECT_TRUE(frame->GetFindInPage()->FindInternal(
        kFindIdentifier, search_text, *options, false));
  }
  RunPendingTasks();
  EXPECT_FALSE(find_in_page_client.FindResultsAreReady());

  // Detach the frame between finding and scoping.
  RemoveElementById(main_frame, "frame");

  main_frame->EnsureTextFinder().ResetMatchCount();

  for (WebLocalFrameImpl* frame = main_frame; frame;
       frame = To<WebLocalFrameImpl>(frame->TraverseNext())) {
    frame->EnsureTextFinder().StartScopingStringMatches(kFindIdentifier,
                                                        search_text, *options);
  }

  RunPendingTasks();
  EXPECT_TRUE(find_in_page_client.FindResultsAreReady());
}

TEST_F(WebFrameTest, FindDetachFrameWhileScopingStrings) {
  RegisterMockedHttpURLLoad("find_in_page.html");
  RegisterMockedHttpURLLoad("find_in_page_frame.html");

  frame_test_helpers::TestWebFrameClient frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "find_in_page.html",
                                    &frame_client);
  web_view_helper.Resize(gfx::Size(640, 480));
  RunPendingTasks();

  const char kFindString[] = "result";
  const int kFindIdentifier = 12345;

  auto options = mojom::blink::FindOptions::New();
  options->run_synchronously_for_testing = true;
  WebString search_text = WebString::FromUTF8(kFindString);
  WebLocalFrameImpl* main_frame = web_view_helper.LocalMainFrame();
  TestFindInPageClient find_in_page_client;
  find_in_page_client.SetFrame(main_frame);

  for (WebLocalFrameImpl* frame = main_frame; frame;
       frame = To<WebLocalFrameImpl>(frame->TraverseNext())) {
    EXPECT_TRUE(frame->GetFindInPage()->FindInternal(
        kFindIdentifier, search_text, *options, false));
  }
  RunPendingTasks();
  EXPECT_FALSE(find_in_page_client.FindResultsAreReady());

  main_frame->EnsureTextFinder().ResetMatchCount();

  for (WebLocalFrameImpl* frame = main_frame; frame;
       frame = To<WebLocalFrameImpl>(frame->TraverseNext())) {
    frame->EnsureTextFinder().StartScopingStringMatches(kFindIdentifier,
                                                        search_text, *options);
  }

  // The first startScopingStringMatches will have reset the state. Detach
  // before it actually scopes.
  RemoveElementById(main_frame, "frame");

  for (WebLocalFrameImpl* frame = main_frame; frame;
       frame = To<WebLocalFrameImpl>(frame->TraverseNext())) {
    frame->EnsureTextFinder().StartScopingStringMatches(kFindIdentifier,
                                                        search_text, *options);
  }
  RunPendingTasks();
  EXPECT_TRUE(find_in_page_client.FindResultsAreReady());
}

TEST_F(WebFrameTest, ResetMatchCount) {
  RegisterMockedHttpURLLoad("find_in_generated_frame.html");

  frame_test_helpers::TestWebFrameClient frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "find_in_generated_frame.html",
                                    &frame_client);
  web_view_helper.Resize(gfx::Size(640, 480));
  RunPendingTasks();

  const char kFindString[] = "result";
  const int kFindIdentifier = 12345;

  auto options = mojom::blink::FindOptions::New();
  options->run_synchronously_for_testing = true;
  WebString search_text = WebString::FromUTF8(kFindString);
  WebLocalFrameImpl* main_frame = web_view_helper.LocalMainFrame();
  TestFindInPageClient find_in_page_client;
  find_in_page_client.SetFrame(main_frame);

  // Check that child frame exists.
  EXPECT_TRUE(!!main_frame->TraverseNext());

  for (WebLocalFrameImpl* frame = main_frame; frame;
       frame = To<WebLocalFrameImpl>(frame->TraverseNext())) {
    EXPECT_FALSE(frame->GetFindInPage()->FindInternal(
        kFindIdentifier, search_text, *options, false));
  }

  RunPendingTasks();
  EXPECT_FALSE(find_in_page_client.FindResultsAreReady());

  main_frame->EnsureTextFinder().ResetMatchCount();
}

TEST_F(WebFrameTest, SetTickmarks) {
  RegisterMockedHttpURLLoad("find.html");

  frame_test_helpers::TestWebFrameClient frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "find.html", &frame_client);
  web_view_helper.Resize(gfx::Size(640, 480));
  RunPendingTasks();

  const char kFindString[] = "foo";
  const int kFindIdentifier = 12345;

  auto options = mojom::blink::FindOptions::New();
  options->run_synchronously_for_testing = true;
  WebString search_text = WebString::FromUTF8(kFindString);
  WebLocalFrameImpl* main_frame = web_view_helper.LocalMainFrame();
  TestFindInPageClient find_in_page_client;
  find_in_page_client.SetFrame(main_frame);
  EXPECT_TRUE(main_frame->GetFindInPage()->FindInternal(
      kFindIdentifier, search_text, *options, false));

  main_frame->EnsureTextFinder().ResetMatchCount();
  main_frame->EnsureTextFinder().StartScopingStringMatches(
      kFindIdentifier, search_text, *options);

  RunPendingTasks();
  EXPECT_TRUE(find_in_page_client.FindResultsAreReady());

  const Vector<IntRect> kExpectedOverridingTickmarks = {
      IntRect(0, 0, 100, 100), IntRect(0, 20, 100, 100),
      IntRect(0, 30, 100, 100)};
  const Vector<IntRect> kResetTickmarks;

  {
    // Test SetTickmarks() with a null target WebElement.
    //
    // Get the tickmarks for the original find request. It should have 4
    // tickmarks, given the search performed above.
    LocalFrameView* frame_view =
        web_view_helper.LocalMainFrame()->GetFrameView();
    ScrollableArea* layout_viewport = frame_view->LayoutViewport();
    Vector<IntRect> original_tickmarks = layout_viewport->GetTickmarks();
    EXPECT_EQ(4u, original_tickmarks.size());

    // Override the tickmarks.
    main_frame->SetTickmarks(WebElement(), kExpectedOverridingTickmarks);

    // Check the tickmarks are overridden correctly.
    Vector<IntRect> overriding_tickmarks_actual =
        layout_viewport->GetTickmarks();
    EXPECT_EQ(kExpectedOverridingTickmarks, overriding_tickmarks_actual);

    // Reset the tickmark behavior.
    main_frame->SetTickmarks(WebElement(), kResetTickmarks);

    // Check that the original tickmarks are returned
    Vector<IntRect> original_tickmarks_after_reset =
        layout_viewport->GetTickmarks();
    EXPECT_EQ(original_tickmarks, original_tickmarks_after_reset);
  }

  {
    // Test SetTickmarks() with a non-null target WebElement.
    //
    // Use an element from within find.html for testing. It has no tickmarks.
    WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
    WebElement target = frame->GetDocument().GetElementById("textarea1");
    ASSERT_FALSE(target.IsNull());
    LayoutBox* box = target.ConstUnwrap<Element>()->GetLayoutBoxForScrolling();
    ASSERT_TRUE(box);
    ScrollableArea* scrollable_area = box->GetScrollableArea();
    ASSERT_TRUE(scrollable_area);
    Vector<IntRect> original_tickmarks = scrollable_area->GetTickmarks();
    EXPECT_EQ(0u, original_tickmarks.size());

    // Override the tickmarks.
    main_frame->SetTickmarks(target, kExpectedOverridingTickmarks);

    // Check the tickmarks are overridden correctly.
    Vector<IntRect> overriding_tickmarks_actual =
        scrollable_area->GetTickmarks();
    EXPECT_EQ(kExpectedOverridingTickmarks, overriding_tickmarks_actual);

    // Reset the tickmark behavior.
    main_frame->SetTickmarks(target, kResetTickmarks);

    // Check that the original tickmarks are returned
    Vector<IntRect> original_tickmarks_after_reset =
        scrollable_area->GetTickmarks();
    EXPECT_EQ(original_tickmarks, original_tickmarks_after_reset);
  }
}

TEST_F(WebFrameTest, FindInPageJavaScriptUpdatesDOM) {
  RegisterMockedHttpURLLoad("find.html");

  frame_test_helpers::TestWebFrameClient frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "find.html", &frame_client);
  web_view_helper.Resize(gfx::Size(640, 480));
  RunPendingTasks();

  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  TestFindInPageClient find_in_page_client;
  find_in_page_client.SetFrame(frame);

  const int kFindIdentifier = 12345;
  static const char* kFindString = "foo";
  WebString search_text = WebString::FromUTF8(kFindString);
  auto options = mojom::blink::FindOptions::New();
  options->run_synchronously_for_testing = true;
  bool active_now;

  frame->EnsureTextFinder().ResetMatchCount();
  frame->EnsureTextFinder().StartScopingStringMatches(kFindIdentifier,
                                                      search_text, *options);
  RunPendingTasks();
  EXPECT_TRUE(find_in_page_client.FindResultsAreReady());

  // Find in a <div> element.
  options->new_session = false;
  EXPECT_TRUE(frame->GetFindInPage()->FindInternal(
      kFindIdentifier, search_text, *options, false, &active_now));
  EXPECT_TRUE(active_now);

  // Insert new text, which contains occurence of |searchText|.
  frame->ExecuteScript(WebScriptSource(
      "var newTextNode = document.createTextNode('bar5 foo5');"
      "var textArea = document.getElementsByTagName('textarea')[0];"
      "document.body.insertBefore(newTextNode, textArea);"));

  // Find in a <input> element.
  EXPECT_TRUE(frame->GetFindInPage()->FindInternal(
      kFindIdentifier, search_text, *options, false, &active_now));
  EXPECT_TRUE(active_now);

  // Find in the inserted text node.
  EXPECT_TRUE(frame->GetFindInPage()->FindInternal(
      kFindIdentifier, search_text, *options, false, &active_now));
  frame->GetFindInPage()->StopFinding(
      blink::mojom::StopFindAction::kStopFindActionKeepSelection);
  WebRange range = frame->SelectionRange();
  EXPECT_EQ(5, range.StartOffset());
  EXPECT_EQ(8, range.EndOffset());
  EXPECT_TRUE(frame->GetDocument().FocusedElement().IsNull());
  EXPECT_FALSE(active_now);
}

TEST_F(WebFrameTest, FindInPageJavaScriptUpdatesDOMProperOrdinal) {
  const WebString search_pattern = WebString::FromUTF8("abc");
  // We have 2 occurrences of the pattern in our text.
  const char* html =
      "foo bar foo bar foo abc bar foo bar foo bar foo bar foo bar foo bar foo "
      "bar foo bar foo bar foo bar foo bar foo bar foo bar foo bar foo bar foo "
      "bar foo bar foo abc bar <div id='new_text'></div>";

  frame_test_helpers::TestWebFrameClient frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(&frame_client);

  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  frame_test_helpers::LoadHTMLString(frame, html,
                                     url_test_helpers::ToKURL(base_url_));
  web_view_helper.Resize(gfx::Size(640, 480));
  web_view_helper.GetWebView()->MainFrameWidget()->SetFocus(true);
  RunPendingTasks();

  TestFindInPageClient find_in_page_client;
  find_in_page_client.SetFrame(frame);
  const int kFindIdentifier = 12345;

  auto options = mojom::blink::FindOptions::New();
  options->run_synchronously_for_testing = true;
  options->new_session = true;
  options->forward = true;
  // The first search that will start the scoping process.
  frame->GetFindInPage()->Find(kFindIdentifier, search_pattern,
                               options->Clone());
  EXPECT_FALSE(find_in_page_client.FindResultsAreReady());
  RunPendingTasks();

  EXPECT_EQ(2, find_in_page_client.Count());
  EXPECT_EQ(1, find_in_page_client.ActiveIndex());

  options->new_session = false;
  // The second search will jump to the next match without any scoping.
  frame->GetFindInPage()->Find(kFindIdentifier, search_pattern,
                               options->Clone());
  // Run pending tasks to make sure IncreaseMatchCount calls passes.
  RunPendingTasks();
  EXPECT_EQ(2, find_in_page_client.Count());
  EXPECT_EQ(2, find_in_page_client.ActiveIndex());
  EXPECT_FALSE(frame->EnsureTextFinder().ScopingInProgress());

  // Insert new text, which contains occurence of |searchText|.
  frame->ExecuteScript(
      WebScriptSource("var textDiv = document.getElementById('new_text');"
                      "textDiv.innerHTML = 'foo abc';"));

  // The third search will find a new match and initiate a new scoping.
  frame->GetFindInPage()->Find(kFindIdentifier, search_pattern,
                               options->Clone());
  RunPendingTasks();

  EXPECT_EQ(3, find_in_page_client.Count());
  EXPECT_EQ(3, find_in_page_client.ActiveIndex());
}

TEST_F(WebFrameTest, FindInPageStopFindActionKeepSelectionInAnotherDocument) {
  RegisterMockedHttpURLLoad("find.html");
  RegisterMockedHttpURLLoad("hello_world.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "find.html");
  ASSERT_TRUE(web_view_helper.LocalMainFrame());
  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  const int kFindIdentifier = 12345;
  auto options = mojom::blink::FindOptions::New();

  // Set active match
  ASSERT_TRUE(frame->GetFindInPage()->FindInternal(
      kFindIdentifier, WebString::FromUTF8("foo"), *options, false));
  // Move to another page.
  frame_test_helpers::LoadFrame(frame, base_url_ + "hello_world.html");

  // Stop Find-In-Page. |TextFinder::active_match_| still hold a |Range| in
  // "find.html".
  frame->GetFindInPage()->StopFinding(
      blink::mojom::StopFindAction::kStopFindActionKeepSelection);

  // Pass if not crash. See http://crbug.com/719880 for details.
}

TEST_F(WebFrameTest, FindInPageForcedRedoOfFindInPage) {
  const WebString search_pattern = WebString::FromUTF8("bar");
  const char* html = "foo bar foo foo bar";
  frame_test_helpers::TestWebFrameClient frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(&frame_client);

  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  frame_test_helpers::LoadHTMLString(frame, html,
                                     url_test_helpers::ToKURL(base_url_));
  web_view_helper.Resize(gfx::Size(640, 480));
  web_view_helper.GetWebView()->MainFrameWidget()->SetFocus(true);
  RunPendingTasks();

  TestFindInPageClient find_in_page_client;
  find_in_page_client.SetFrame(frame);
  const int kFindIdentifier = 12345;

  auto options = mojom::blink::FindOptions::New();
  options->run_synchronously_for_testing = true;
  options->new_session = true;
  options->forward = true;
  // First run.
  frame->GetFindInPage()->Find(kFindIdentifier, search_pattern,
                               options->Clone());
  RunPendingTasks();
  EXPECT_EQ(2, find_in_page_client.Count());
  EXPECT_EQ(1, find_in_page_client.ActiveIndex());

  options->force = true;
  frame->GetFindInPage()->Find(kFindIdentifier, search_pattern,
                               options->Clone());
  RunPendingTasks();
  EXPECT_EQ(2, find_in_page_client.Count());
  EXPECT_EQ(1, find_in_page_client.ActiveIndex());

  options->new_session = false;
  options->force = false;

  frame->GetFindInPage()->Find(kFindIdentifier, search_pattern,
                               options->Clone());
  RunPendingTasks();
  EXPECT_EQ(2, find_in_page_client.Count());
  EXPECT_EQ(2, find_in_page_client.ActiveIndex());

  options->new_session = true;
  options->force = true;

  frame->GetFindInPage()->Find(kFindIdentifier, search_pattern,
                               options->Clone());
  RunPendingTasks();
  EXPECT_EQ(2, find_in_page_client.Count());
  EXPECT_EQ(2, find_in_page_client.ActiveIndex());
}

static gfx::Point BottomRightMinusOne(const gfx::Rect& rect) {
  // FIXME: If we don't subtract 1 from the x- and y-coordinates of the
  // selection bounds, selectRange() will select the *next* element. That's
  // strictly correct, as hit-testing checks the pixel to the lower-right of
  // the input coordinate, but it's a wart on the API.
  if (rect.width() > 0) {
    return gfx::Point(rect.x() + rect.width() - 1,
                      rect.y() + rect.height() - 1);
  }
  return gfx::Point(rect.x() + rect.width(), rect.y() + rect.height() - 1);
}

static gfx::Rect ElementBounds(WebLocalFrame* frame, const WebString& id) {
  return gfx::Rect(frame->GetDocument().GetElementById(id).BoundsInViewport());
}

static std::string SelectionAsString(WebFrame* frame) {
  return frame->ToWebLocalFrame()->SelectionAsText().Utf8();
}

TEST_F(WebFrameTest, SelectRange) {
  WebLocalFrame* frame;
  gfx::Rect start_rect;
  gfx::Rect end_rect;

  RegisterMockedHttpURLLoad("select_range_basic.html");
  RegisterMockedHttpURLLoad("select_range_scroll.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "select_range_basic.html",
                                 &web_view_helper);
  frame = web_view_helper.LocalMainFrame();
  EXPECT_EQ("Some test text for testing.", SelectionAsString(frame));
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      start_rect, end_rect);
  frame->ExecuteCommand(WebString::FromUTF8("Unselect"));
  EXPECT_EQ("", SelectionAsString(frame));
  frame->SelectRange(start_rect.origin(), BottomRightMinusOne(end_rect));
  // On some devices, the above bottomRightMinusOne() causes the ending '.' not
  // selected.
  std::string selection_string = SelectionAsString(frame);
  EXPECT_TRUE(selection_string == "Some test text for testing." ||
              selection_string == "Some test text for testing");

  InitializeTextSelectionWebView(base_url_ + "select_range_scroll.html",
                                 &web_view_helper);
  frame = web_view_helper.LocalMainFrame();
  EXPECT_EQ("Some offscreen test text for testing.", SelectionAsString(frame));
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      start_rect, end_rect);
  frame->ExecuteCommand(WebString::FromUTF8("Unselect"));
  EXPECT_EQ("", SelectionAsString(frame));
  frame->SelectRange(start_rect.origin(), BottomRightMinusOne(end_rect));
  // On some devices, the above bottomRightMinusOne() causes the ending '.' not
  // selected.
  selection_string = SelectionAsString(frame);
  EXPECT_TRUE(selection_string == "Some offscreen test text for testing." ||
              selection_string == "Some offscreen test text for testing");
}

TEST_F(WebFrameTest, SelectRangeDefaultHandleVisibility) {
  RegisterMockedHttpURLLoad("select_range_basic.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "select_range_basic.html",
                                 &web_view_helper);

  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  frame->SelectRange(WebRange(0, 5), WebLocalFrame::kHideSelectionHandle,
                     SelectionMenuBehavior::kHide);
  EXPECT_FALSE(frame->SelectionRange().IsNull());

  EXPECT_FALSE(frame->GetFrame()->Selection().IsHandleVisible())
      << "By default selection handles should not be visible";
}

TEST_F(WebFrameTest, SelectRangeHideHandle) {
  RegisterMockedHttpURLLoad("select_range_basic.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "select_range_basic.html",
                                 &web_view_helper);

  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  frame->SelectRange(WebRange(0, 5), WebLocalFrame::kHideSelectionHandle,
                     SelectionMenuBehavior::kHide);

  EXPECT_FALSE(frame->GetFrame()->Selection().IsHandleVisible())
      << "Selection handle should not be visible with kHideSelectionHandle";
}

TEST_F(WebFrameTest, SelectRangeShowHandle) {
  RegisterMockedHttpURLLoad("select_range_basic.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "select_range_basic.html",
                                 &web_view_helper);

  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  frame->SelectRange(WebRange(0, 5), WebLocalFrame::kShowSelectionHandle,
                     SelectionMenuBehavior::kHide);

  EXPECT_TRUE(frame->GetFrame()->Selection().IsHandleVisible())
      << "Selection handle should be visible with kShowSelectionHandle";
}

TEST_F(WebFrameTest, SelectRangePreserveHandleVisibility) {
  RegisterMockedHttpURLLoad("select_range_basic.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "select_range_basic.html",
                                 &web_view_helper);

  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  frame->SelectRange(WebRange(0, 5), WebLocalFrame::kHideSelectionHandle,
                     SelectionMenuBehavior::kHide);
  frame->SelectRange(WebRange(0, 6), WebLocalFrame::kPreserveHandleVisibility,
                     SelectionMenuBehavior::kHide);

  EXPECT_FALSE(frame->GetFrame()->Selection().IsHandleVisible())
      << "kPreserveHandleVisibility should keep handles invisible";

  frame->SelectRange(WebRange(0, 5), WebLocalFrame::kShowSelectionHandle,
                     SelectionMenuBehavior::kHide);
  frame->SelectRange(WebRange(0, 6), WebLocalFrame::kPreserveHandleVisibility,
                     SelectionMenuBehavior::kHide);

  EXPECT_TRUE(frame->GetFrame()->Selection().IsHandleVisible())
      << "kPreserveHandleVisibility should keep handles visible";
}

TEST_F(WebFrameTest, SelectRangeInIframe) {
  WebFrame* frame;
  gfx::Rect start_rect;
  gfx::Rect end_rect;

  RegisterMockedHttpURLLoad("select_range_iframe.html");
  RegisterMockedHttpURLLoad("select_range_basic.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "select_range_iframe.html",
                                 &web_view_helper);
  frame = web_view_helper.GetWebView()->MainFrame();
  WebLocalFrame* subframe = frame->FirstChild()->ToWebLocalFrame();
  EXPECT_EQ("Some test text for testing.", SelectionAsString(subframe));
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      start_rect, end_rect);
  subframe->ExecuteCommand(WebString::FromUTF8("Unselect"));
  EXPECT_EQ("", SelectionAsString(subframe));
  subframe->SelectRange(start_rect.origin(), BottomRightMinusOne(end_rect));
  // On some devices, the above bottomRightMinusOne() causes the ending '.' not
  // selected.
  std::string selection_string = SelectionAsString(subframe);
  EXPECT_TRUE(selection_string == "Some test text for testing." ||
              selection_string == "Some test text for testing");
}

TEST_F(WebFrameTest, SelectRangeDivContentEditable) {
  WebLocalFrame* frame;
  gfx::Rect start_rect;
  gfx::Rect end_rect;

  RegisterMockedHttpURLLoad("select_range_div_editable.html");

  // Select the middle of an editable element, then try to extend the selection
  // to the top of the document.  The selection range should be clipped to the
  // bounds of the editable element.
  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "select_range_div_editable.html",
                                 &web_view_helper);
  frame = web_view_helper.LocalMainFrame();
  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      start_rect, end_rect);

  frame->SelectRange(BottomRightMinusOne(end_rect), gfx::Point());
  EXPECT_EQ("16-char header. This text is initially selected.",
            SelectionAsString(frame));

  // As above, but extending the selection to the bottom of the document.
  InitializeTextSelectionWebView(base_url_ + "select_range_div_editable.html",
                                 &web_view_helper);
  frame = web_view_helper.LocalMainFrame();

  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      start_rect, end_rect);
  frame->SelectRange(start_rect.origin(), BottomRightMinusOne(end_rect));
  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      start_rect, end_rect);

  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      start_rect, end_rect);
  frame->SelectRange(start_rect.origin(), gfx::Point(640, 480));
  EXPECT_EQ("This text is initially selected. 16-char footer.",
            SelectionAsString(frame));
}

// positionForPoint returns the wrong values for contenteditable spans. See
// http://crbug.com/238334.
TEST_F(WebFrameTest, DISABLED_SelectRangeSpanContentEditable) {
  WebLocalFrame* frame;
  gfx::Rect start_rect;
  gfx::Rect end_rect;

  RegisterMockedHttpURLLoad("select_range_span_editable.html");

  // Select the middle of an editable element, then try to extend the selection
  // to the top of the document.
  // The selection range should be clipped to the bounds of the editable
  // element.
  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "select_range_span_editable.html",
                                 &web_view_helper);
  frame = web_view_helper.LocalMainFrame();
  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      start_rect, end_rect);

  frame->SelectRange(BottomRightMinusOne(end_rect), gfx::Point());
  EXPECT_EQ("16-char header. This text is initially selected.",
            SelectionAsString(frame));

  // As above, but extending the selection to the bottom of the document.
  InitializeTextSelectionWebView(base_url_ + "select_range_span_editable.html",
                                 &web_view_helper);
  frame = web_view_helper.LocalMainFrame();

  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      start_rect, end_rect);
  frame->SelectRange(start_rect.origin(), BottomRightMinusOne(end_rect));
  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      start_rect, end_rect);

  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      start_rect, end_rect);
  frame->SelectRange(start_rect.origin(), gfx::Point(640, 480));
  EXPECT_EQ("This text is initially selected. 16-char footer.",
            SelectionAsString(frame));
}

TEST_F(WebFrameTest, SelectRangeCanMoveSelectionStart) {
  RegisterMockedHttpURLLoad("text_selection.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "text_selection.html",
                                 &web_view_helper);
  WebLocalFrame* frame = web_view_helper.LocalMainFrame();

  // Select second span. We can move the start to include the first span.
  frame->ExecuteScript(WebScriptSource("selectElement('header_2');"));
  EXPECT_EQ("Header 2.", SelectionAsString(frame));
  frame->SelectRange(BottomRightMinusOne(ElementBounds(frame, "header_2")),
                     ElementBounds(frame, "header_1").origin());
  EXPECT_EQ("Header 1. Header 2.", SelectionAsString(frame));

  // We can move the start and end together.
  frame->ExecuteScript(WebScriptSource("selectElement('header_1');"));
  EXPECT_EQ("Header 1.", SelectionAsString(frame));
  frame->SelectRange(BottomRightMinusOne(ElementBounds(frame, "header_1")),
                     BottomRightMinusOne(ElementBounds(frame, "header_1")));
  EXPECT_EQ("", SelectionAsString(frame));
  // Selection is a caret, not empty.
  EXPECT_FALSE(frame->SelectionRange().IsNull());

  // We can move the start across the end.
  frame->ExecuteScript(WebScriptSource("selectElement('header_1');"));
  EXPECT_EQ("Header 1.", SelectionAsString(frame));
  frame->SelectRange(BottomRightMinusOne(ElementBounds(frame, "header_1")),
                     BottomRightMinusOne(ElementBounds(frame, "header_2")));
  EXPECT_EQ(" Header 2.", SelectionAsString(frame));

  // Can't extend the selection part-way into an editable element.
  frame->ExecuteScript(WebScriptSource("selectElement('footer_2');"));
  EXPECT_EQ("Footer 2.", SelectionAsString(frame));
  frame->SelectRange(BottomRightMinusOne(ElementBounds(frame, "footer_2")),
                     ElementBounds(frame, "editable_2").origin());
  EXPECT_EQ(" [ Footer 1. Footer 2.", SelectionAsString(frame));

  // Can extend the selection completely across editable elements.
  frame->ExecuteScript(WebScriptSource("selectElement('footer_2');"));
  EXPECT_EQ("Footer 2.", SelectionAsString(frame));
  frame->SelectRange(BottomRightMinusOne(ElementBounds(frame, "footer_2")),
                     ElementBounds(frame, "header_2").origin());
  EXPECT_EQ("Header 2. ] [ Editable 1. Editable 2. ] [ Footer 1. Footer 2.",
            SelectionAsString(frame));

  // If the selection is editable text, we can't extend it into non-editable
  // text.
  frame->ExecuteScript(WebScriptSource("selectElement('editable_2');"));
  EXPECT_EQ("Editable 2.", SelectionAsString(frame));
  frame->SelectRange(BottomRightMinusOne(ElementBounds(frame, "editable_2")),
                     ElementBounds(frame, "header_2").origin());
  EXPECT_EQ("[ Editable 1. Editable 2.", SelectionAsString(frame));
}

TEST_F(WebFrameTest, SelectRangeCanMoveSelectionEnd) {
  RegisterMockedHttpURLLoad("text_selection.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "text_selection.html",
                                 &web_view_helper);
  WebLocalFrame* frame = web_view_helper.LocalMainFrame();

  // Select first span. We can move the end to include the second span.
  frame->ExecuteScript(WebScriptSource("selectElement('header_1');"));
  EXPECT_EQ("Header 1.", SelectionAsString(frame));
  frame->SelectRange(ElementBounds(frame, "header_1").origin(),
                     BottomRightMinusOne(ElementBounds(frame, "header_2")));
  EXPECT_EQ("Header 1. Header 2.", SelectionAsString(frame));

  // We can move the start and end together.
  frame->ExecuteScript(WebScriptSource("selectElement('header_2');"));
  EXPECT_EQ("Header 2.", SelectionAsString(frame));
  frame->SelectRange(ElementBounds(frame, "header_2").origin(),
                     ElementBounds(frame, "header_2").origin());
  EXPECT_EQ("", SelectionAsString(frame));
  // Selection is a caret, not empty.
  EXPECT_FALSE(frame->SelectionRange().IsNull());

  // We can move the end across the start.
  frame->ExecuteScript(WebScriptSource("selectElement('header_2');"));
  EXPECT_EQ("Header 2.", SelectionAsString(frame));
  frame->SelectRange(ElementBounds(frame, "header_2").origin(),
                     ElementBounds(frame, "header_1").origin());
  EXPECT_EQ("Header 1. ", SelectionAsString(frame));

  // Can't extend the selection part-way into an editable element.
  frame->ExecuteScript(WebScriptSource("selectElement('header_1');"));
  EXPECT_EQ("Header 1.", SelectionAsString(frame));
  frame->SelectRange(ElementBounds(frame, "header_1").origin(),
                     BottomRightMinusOne(ElementBounds(frame, "editable_1")));
  EXPECT_EQ("Header 1. Header 2. ] ", SelectionAsString(frame));

  // Can extend the selection completely across editable elements.
  frame->ExecuteScript(WebScriptSource("selectElement('header_1');"));
  EXPECT_EQ("Header 1.", SelectionAsString(frame));
  frame->SelectRange(ElementBounds(frame, "header_1").origin(),
                     BottomRightMinusOne(ElementBounds(frame, "footer_1")));
  EXPECT_EQ("Header 1. Header 2. ] [ Editable 1. Editable 2. ] [ Footer 1.",
            SelectionAsString(frame));

  // If the selection is editable text, we can't extend it into non-editable
  // text.
  frame->ExecuteScript(WebScriptSource("selectElement('editable_1');"));
  EXPECT_EQ("Editable 1.", SelectionAsString(frame));
  frame->SelectRange(ElementBounds(frame, "editable_1").origin(),
                     BottomRightMinusOne(ElementBounds(frame, "footer_1")));
  EXPECT_EQ("Editable 1. Editable 2. ]", SelectionAsString(frame));
}

TEST_F(WebFrameTest, MoveRangeSelectionExtent) {
  WebLocalFrameImpl* frame;
  gfx::Rect start_rect;
  gfx::Rect end_rect;

  RegisterMockedHttpURLLoad("move_range_selection_extent.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "move_range_selection_extent.html",
                                 &web_view_helper);
  frame = web_view_helper.LocalMainFrame();
  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      start_rect, end_rect);

  frame->MoveRangeSelectionExtent(gfx::Point(640, 480));
  EXPECT_EQ("This text is initially selected. 16-char footer.",
            SelectionAsString(frame));

  frame->MoveRangeSelectionExtent(gfx::Point());
  EXPECT_EQ("16-char header. ", SelectionAsString(frame));

  // Reset with swapped base and extent.
  frame->SelectRange(end_rect.origin(), BottomRightMinusOne(start_rect));
  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));

  frame->MoveRangeSelectionExtent(gfx::Point(640, 480));
  EXPECT_EQ(" 16-char footer.", SelectionAsString(frame));

  frame->MoveRangeSelectionExtent(gfx::Point());
  EXPECT_EQ("16-char header. This text is initially selected.",
            SelectionAsString(frame));

  frame->ExecuteCommand(WebString::FromUTF8("Unselect"));
  EXPECT_EQ("", SelectionAsString(frame));
}

TEST_F(WebFrameTest, MoveRangeSelectionExtentCannotCollapse) {
  WebLocalFrameImpl* frame;
  gfx::Rect start_rect;
  gfx::Rect end_rect;

  RegisterMockedHttpURLLoad("move_range_selection_extent.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "move_range_selection_extent.html",
                                 &web_view_helper);
  frame = web_view_helper.LocalMainFrame();
  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      start_rect, end_rect);

  frame->MoveRangeSelectionExtent(BottomRightMinusOne(start_rect));
  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));

  // Reset with swapped base and extent.
  frame->SelectRange(end_rect.origin(), BottomRightMinusOne(start_rect));
  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));

  frame->MoveRangeSelectionExtent(BottomRightMinusOne(end_rect));
  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));
}

TEST_F(WebFrameTest, MoveRangeSelectionExtentScollsInputField) {
  WebLocalFrameImpl* frame;
  gfx::Rect start_rect;
  gfx::Rect end_rect;

  RegisterMockedHttpURLLoad("move_range_selection_extent_input_field.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(
      base_url_ + "move_range_selection_extent_input_field.html",
      &web_view_helper);
  frame = web_view_helper.LocalMainFrame();
  EXPECT_EQ("Length", SelectionAsString(frame));
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      start_rect, end_rect);

  EXPECT_EQ(0, frame->GetFrame()
                   ->Selection()
                   .ComputeVisibleSelectionInDOMTree()
                   .RootEditableElement()
                   ->scrollLeft());
  frame->MoveRangeSelectionExtent(gfx::Point(end_rect.x() + 500, end_rect.y()));
  EXPECT_GE(frame->GetFrame()
                ->Selection()
                .ComputeVisibleSelectionInDOMTree()
                .RootEditableElement()
                ->scrollLeft(),
            1);
  EXPECT_EQ("Lengthy text goes here.", SelectionAsString(frame));
}

TEST_F(WebFrameTest, SmartClipData) {
  static const char kExpectedClipText[] = "\nPrice 10,000,000won";
  static const char kExpectedClipHtml[] =
      "<div id=\"div4\" style=\"padding: 10px; margin: 10px; border: 2px solid "
      "skyblue; float: left; width: 190px; height: 30px; color: rgb(0, 0, 0); "
      "font-family: myahem; font-size: 8px; font-style: normal; "
      "font-variant-ligatures: normal; font-variant-caps: normal; font-weight: "
      "400; letter-spacing: normal; orphans: 2; text-align: start; "
      "text-indent: 0px; text-transform: none; white-space: normal; widows: 2; "
      "word-spacing: 0px; -webkit-text-stroke-width: 0px; "
      "text-decoration-thickness: initial; text-decoration-style: initial; "
      "text-decoration-color: initial;\">Air conditioner</div><div id=\"div5\" "
      "style=\"padding: 10px; margin: 10px; border: 2px solid skyblue; float: "
      "left; width: 190px; height: 30px; color: rgb(0, 0, 0); font-family: "
      "myahem; font-size: 8px; font-style: normal; font-variant-ligatures: "
      "normal; font-variant-caps: normal; font-weight: 400; letter-spacing: "
      "normal; orphans: 2; text-align: start; text-indent: 0px; "
      "text-transform: none; white-space: normal; widows: 2; word-spacing: "
      "0px; -webkit-text-stroke-width: 0px; text-decoration-thickness: "
      "initial; text-decoration-style: initial; text-decoration-color: "
      "initial;\">Price 10,000,000won</div>";
  WebString clip_text;
  WebString clip_html;
  WebRect clip_rect;
  RegisterMockedHttpURLLoad("Ahem.ttf");
  RegisterMockedHttpURLLoad("smartclip.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "smartclip.html");
  WebLocalFrame* frame = web_view_helper.LocalMainFrame();
  web_view_helper.Resize(gfx::Size(500, 500));
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  WebRect crop_rect(300, 125, 152, 50);
  frame->ExtractSmartClipData(crop_rect, clip_text, clip_html, clip_rect);
  EXPECT_EQ(kExpectedClipText, clip_text);
  EXPECT_EQ(kExpectedClipHtml, clip_html);
}

TEST_F(WebFrameTest, SmartClipDataWithPinchZoom) {
  static const char kExpectedClipText[] = "\nPrice 10,000,000won";
  static const char kExpectedClipHtml[] =
      "<div id=\"div4\" style=\"padding: 10px; margin: 10px; border: 2px solid "
      "skyblue; float: left; width: 190px; height: 30px; color: rgb(0, 0, 0); "
      "font-family: myahem; font-size: 8px; font-style: normal; "
      "font-variant-ligatures: normal; font-variant-caps: normal; font-weight: "
      "400; letter-spacing: normal; orphans: 2; text-align: start; "
      "text-indent: 0px; text-transform: none; white-space: normal; widows: 2; "
      "word-spacing: 0px; -webkit-text-stroke-width: 0px; "
      "text-decoration-thickness: initial; text-decoration-style: initial; "
      "text-decoration-color: initial;\">Air conditioner</div><div id=\"div5\" "
      "style=\"padding: 10px; margin: 10px; border: 2px solid skyblue; float: "
      "left; width: 190px; height: 30px; color: rgb(0, 0, 0); font-family: "
      "myahem; font-size: 8px; font-style: normal; font-variant-ligatures: "
      "normal; font-variant-caps: normal; font-weight: 400; letter-spacing: "
      "normal; orphans: 2; text-align: start; text-indent: 0px; "
      "text-transform: none; white-space: normal; widows: 2; word-spacing: "
      "0px; -webkit-text-stroke-width: 0px; text-decoration-thickness: "
      "initial; text-decoration-style: initial; text-decoration-color: "
      "initial;\">Price 10,000,000won</div>";
  WebString clip_text;
  WebString clip_html;
  WebRect clip_rect;
  RegisterMockedHttpURLLoad("Ahem.ttf");
  RegisterMockedHttpURLLoad("smartclip.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "smartclip.html");
  WebLocalFrame* frame = web_view_helper.LocalMainFrame();
  web_view_helper.Resize(gfx::Size(500, 500));
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  web_view_helper.GetWebView()->SetPageScaleFactor(1.5);
  web_view_helper.GetWebView()->SetVisualViewportOffset(gfx::PointF(167, 100));
  WebRect crop_rect(200, 38, 228, 75);
  frame->ExtractSmartClipData(crop_rect, clip_text, clip_html, clip_rect);
  EXPECT_EQ(kExpectedClipText, clip_text);
  EXPECT_EQ(kExpectedClipHtml, clip_html);
}

TEST_F(WebFrameTest, SmartClipReturnsEmptyStringsWhenUserSelectIsNone) {
  WebString clip_text;
  WebString clip_html;
  WebRect clip_rect;
  RegisterMockedHttpURLLoad("Ahem.ttf");
  RegisterMockedHttpURLLoad("smartclip_user_select_none.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ +
                                    "smartclip_user_select_none.html");
  WebLocalFrame* frame = web_view_helper.LocalMainFrame();
  web_view_helper.Resize(gfx::Size(500, 500));
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  WebRect crop_rect(0, 0, 100, 100);
  frame->ExtractSmartClipData(crop_rect, clip_text, clip_html, clip_rect);
  EXPECT_STREQ("", clip_text.Utf8().c_str());
  EXPECT_STREQ("", clip_html.Utf8().c_str());
}

TEST_F(WebFrameTest, SmartClipDoesNotCrashPositionReversed) {
  WebString clip_text;
  WebString clip_html;
  WebRect clip_rect;
  RegisterMockedHttpURLLoad("Ahem.ttf");
  RegisterMockedHttpURLLoad("smartclip_reversed_positions.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ +
                                    "smartclip_reversed_positions.html");
  WebLocalFrame* frame = web_view_helper.LocalMainFrame();
  web_view_helper.Resize(gfx::Size(500, 500));
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  // Left upper corner of the rect will be end position in the DOM hierarchy.
  WebRect crop_rect(30, 110, 400, 250);
  // This should not still crash. See crbug.com/589082 for more details.
  frame->ExtractSmartClipData(crop_rect, clip_text, clip_html, clip_rect);
}

static int ComputeOffset(LayoutObject* layout_object, int x, int y) {
  return layout_object->PositionForPoint(PhysicalOffset(x, y))
      .GetPosition()
      .ComputeOffsetInContainerNode();
}

// positionForPoint returns the wrong values for contenteditable spans. See
// http://crbug.com/238334.
TEST_F(WebFrameTest, DISABLED_PositionForPointTest) {
  RegisterMockedHttpURLLoad("select_range_span_editable.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "select_range_span_editable.html",
                                 &web_view_helper);
  WebLocalFrameImpl* main_frame = web_view_helper.LocalMainFrame();
  LayoutObject* layout_object = main_frame->GetFrame()
                                    ->Selection()
                                    .ComputeVisibleSelectionInDOMTree()
                                    .RootEditableElement()
                                    ->GetLayoutObject();
  EXPECT_EQ(0, ComputeOffset(layout_object, -1, -1));
  EXPECT_EQ(64, ComputeOffset(layout_object, 1000, 1000));

  RegisterMockedHttpURLLoad("select_range_div_editable.html");
  InitializeTextSelectionWebView(base_url_ + "select_range_div_editable.html",
                                 &web_view_helper);
  main_frame = web_view_helper.LocalMainFrame();
  layout_object = main_frame->GetFrame()
                      ->Selection()
                      .ComputeVisibleSelectionInDOMTree()
                      .RootEditableElement()
                      ->GetLayoutObject();
  EXPECT_EQ(0, ComputeOffset(layout_object, -1, -1));
  EXPECT_EQ(64, ComputeOffset(layout_object, 1000, 1000));
}

#if !defined(OS_MAC) && !defined(OS_LINUX) && !defined(OS_CHROMEOS)
TEST_F(WebFrameTest, SelectRangeStaysHorizontallyAlignedWhenMoved) {
  RegisterMockedHttpURLLoad("move_caret.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "move_caret.html",
                                 &web_view_helper);
  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();

  gfx::Rect initial_start_rect;
  gfx::Rect initial_end_rect;
  gfx::Rect start_rect;
  gfx::Rect end_rect;

  frame->ExecuteScript(WebScriptSource("selectRange();"));
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      initial_start_rect, initial_end_rect);
  gfx::Point moved_start(initial_start_rect.origin());

  moved_start.Offset(0, 40);
  frame->SelectRange(moved_start, BottomRightMinusOne(initial_end_rect));
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      start_rect, end_rect);
  EXPECT_EQ(start_rect, initial_start_rect);
  EXPECT_EQ(end_rect, initial_end_rect);

  moved_start.Offset(0, -80);
  frame->SelectRange(moved_start, BottomRightMinusOne(initial_end_rect));
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      start_rect, end_rect);
  EXPECT_EQ(start_rect, initial_start_rect);
  EXPECT_EQ(end_rect, initial_end_rect);

  gfx::Point moved_end(BottomRightMinusOne(initial_end_rect));

  moved_end.Offset(0, 40);
  frame->SelectRange(initial_start_rect.origin(), moved_end);
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      start_rect, end_rect);
  EXPECT_EQ(start_rect, initial_start_rect);
  EXPECT_EQ(end_rect, initial_end_rect);

  moved_end.Offset(0, -80);
  frame->SelectRange(initial_start_rect.origin(), moved_end);
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      start_rect, end_rect);
  EXPECT_EQ(start_rect, initial_start_rect);
  EXPECT_EQ(end_rect, initial_end_rect);
}

TEST_F(WebFrameTest, MoveCaretStaysHorizontallyAlignedWhenMoved) {
  WebLocalFrameImpl* frame;
  RegisterMockedHttpURLLoad("move_caret.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "move_caret.html",
                                 &web_view_helper);
  frame = (WebLocalFrameImpl*)web_view_helper.GetWebView()->MainFrame();

  gfx::Rect initial_start_rect;
  gfx::Rect initial_end_rect;
  gfx::Rect start_rect;
  gfx::Rect end_rect;

  frame->ExecuteScript(WebScriptSource("selectCaret();"));
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      initial_start_rect, initial_end_rect);
  gfx::Point move_to(initial_start_rect.origin());

  move_to.Offset(0, 40);
  frame->MoveCaretSelection(move_to);
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      start_rect, end_rect);
  EXPECT_EQ(start_rect, initial_start_rect);
  EXPECT_EQ(end_rect, initial_end_rect);

  move_to.Offset(0, -80);
  frame->MoveCaretSelection(move_to);
  web_view_helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
      start_rect, end_rect);
  EXPECT_EQ(start_rect, initial_start_rect);
  EXPECT_EQ(end_rect, initial_end_rect);
}
#endif

class CompositedSelectionBoundsTest
    : public WebFrameTest,
      private ScopedCompositedSelectionUpdateForTest {
 protected:
  CompositedSelectionBoundsTest()
      : ScopedCompositedSelectionUpdateForTest(true) {
    RegisterMockedHttpURLLoad("Ahem.ttf");

    web_view_helper_.Initialize(nullptr, nullptr, &web_widget_client_);
    web_view_helper_.GetWebView()->GetSettings()->SetDefaultFontSize(12);
    web_view_helper_.GetWebView()->SetDefaultPageScaleLimits(1, 1);
    web_view_helper_.Resize(gfx::Size(640, 480));
  }

  void RunTestWithNoSelection(const char* test_file) {
    RegisterMockedHttpURLLoad(test_file);
    web_view_helper_.GetWebView()->MainFrameWidget()->SetFocus(true);
    frame_test_helpers::LoadFrame(
        web_view_helper_.GetWebView()->MainFrameImpl(), base_url_ + test_file);
    UpdateAllLifecyclePhases(web_view_helper_.GetWebView());

    cc::LayerTreeHost* layer_tree_host = web_widget_client_.layer_tree_host();
    const cc::LayerSelection& selection = layer_tree_host->selection();

    ASSERT_EQ(selection, cc::LayerSelection());
    ASSERT_EQ(selection.start, cc::LayerSelectionBound());
    ASSERT_EQ(selection.end, cc::LayerSelectionBound());
  }

  void RunTest(const char* test_file) {
    RegisterMockedHttpURLLoad(test_file);
    web_view_helper_.GetWebView()->MainFrameWidget()->SetFocus(true);
    frame_test_helpers::LoadFrame(
        web_view_helper_.GetWebView()->MainFrameImpl(), base_url_ + test_file);
    UpdateAllLifecyclePhases(web_view_helper_.GetWebView());

    v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
    v8::Local<v8::Value> result =
        web_view_helper_.GetWebView()
            ->MainFrameImpl()
            ->ExecuteScriptAndReturnValue(WebScriptSource("expectedResult"));
    ASSERT_FALSE(result.IsEmpty() || (*result)->IsUndefined());

    ASSERT_TRUE((*result)->IsArray());
    v8::Array& expected_result = *v8::Array::Cast(*result);
    ASSERT_GE(expected_result.Length(), 10u);

    v8::Local<v8::Context> context =
        v8::Isolate::GetCurrent()->GetCurrentContext();

    const int start_edge_start_in_layer_x = expected_result.Get(context, 1)
                                                .ToLocalChecked()
                                                .As<v8::Int32>()
                                                ->Value();
    const int start_edge_start_in_layer_y = expected_result.Get(context, 2)
                                                .ToLocalChecked()
                                                .As<v8::Int32>()
                                                ->Value();
    const int start_edge_end_in_layer_x = expected_result.Get(context, 3)
                                              .ToLocalChecked()
                                              .As<v8::Int32>()
                                              ->Value();
    const int start_edge_end_in_layer_y = expected_result.Get(context, 4)
                                              .ToLocalChecked()
                                              .As<v8::Int32>()
                                              ->Value();

    const int end_edge_start_in_layer_x = expected_result.Get(context, 6)
                                              .ToLocalChecked()
                                              .As<v8::Int32>()
                                              ->Value();
    const int end_edge_start_in_layer_y = expected_result.Get(context, 7)
                                              .ToLocalChecked()
                                              .As<v8::Int32>()
                                              ->Value();
    const int end_edge_end_in_layer_x = expected_result.Get(context, 8)
                                            .ToLocalChecked()
                                            .As<v8::Int32>()
                                            ->Value();
    const int end_edge_end_in_layer_y = expected_result.Get(context, 9)
                                            .ToLocalChecked()
                                            .As<v8::Int32>()
                                            ->Value();

    FloatPoint hit_point;

    if (expected_result.Length() >= 17) {
      hit_point = FloatPoint(expected_result.Get(context, 15)
                                 .ToLocalChecked()
                                 .As<v8::Int32>()
                                 ->Value(),
                             expected_result.Get(context, 16)
                                 .ToLocalChecked()
                                 .As<v8::Int32>()
                                 ->Value());
    } else {
      hit_point =
          FloatPoint((start_edge_start_in_layer_x + start_edge_end_in_layer_x +
                      end_edge_start_in_layer_x + end_edge_end_in_layer_x) /
                         4,
                     (start_edge_start_in_layer_y + start_edge_end_in_layer_y +
                      end_edge_start_in_layer_y + end_edge_end_in_layer_y) /
                             4 +
                         3);
    }

    WebGestureEvent gesture_event(WebInputEvent::Type::kGestureTap,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::GetStaticTimeStampForTests(),
                                  WebGestureDevice::kTouchscreen);
    gesture_event.SetFrameScale(1);
    gesture_event.SetPositionInWidget(hit_point);
    gesture_event.SetPositionInScreen(hit_point);

    web_view_helper_.GetWebView()
        ->MainFrameImpl()
        ->GetFrame()
        ->GetEventHandler()
        .HandleGestureEvent(gesture_event);

    cc::LayerTreeHost* layer_tree_host = web_widget_client_.layer_tree_host();
    const cc::LayerSelection& selection = layer_tree_host->selection();

    ASSERT_NE(selection, cc::LayerSelection());
    ASSERT_NE(selection.start, cc::LayerSelectionBound());
    ASSERT_NE(selection.end, cc::LayerSelectionBound());

    blink::Node* layer_owner_node_for_start = V8Node::ToImplWithTypeCheck(
        v8::Isolate::GetCurrent(),
        expected_result.Get(context, 0).ToLocalChecked());
    ASSERT_TRUE(layer_owner_node_for_start);
    EXPECT_EQ(GetExpectedLayerForSelection(layer_owner_node_for_start)
                  ->CcLayer()
                  .id(),
              selection.start.layer_id);

    EXPECT_EQ(start_edge_start_in_layer_x, selection.start.edge_start.x());
    EXPECT_EQ(start_edge_start_in_layer_y, selection.start.edge_start.y());
    EXPECT_EQ(start_edge_end_in_layer_x, selection.start.edge_end.x());

    blink::Node* layer_owner_node_for_end = V8Node::ToImplWithTypeCheck(
        v8::Isolate::GetCurrent(),
        expected_result.Get(context, 5).ToLocalChecked());

    ASSERT_TRUE(layer_owner_node_for_end);
    EXPECT_EQ(
        GetExpectedLayerForSelection(layer_owner_node_for_end)->CcLayer().id(),
        selection.end.layer_id);

    EXPECT_EQ(end_edge_start_in_layer_x, selection.end.edge_start.x());
    EXPECT_EQ(end_edge_start_in_layer_y, selection.end.edge_start.y());
    EXPECT_EQ(end_edge_end_in_layer_x, selection.end.edge_end.x());

    // Platform differences can introduce small stylistic deviations in
    // y-axis positioning, the details of which aren't relevant to
    // selection behavior. However, such deviations from the expected value
    // should be consistent for the corresponding y coordinates.
    int y_bottom_epsilon = 0;
    if (expected_result.Length() == 13) {
      y_bottom_epsilon = expected_result.Get(context, 12)
                             .ToLocalChecked()
                             .As<v8::Int32>()
                             ->Value();
    }

    int y_bottom_deviation =
        start_edge_end_in_layer_y - selection.start.edge_end.y();
    EXPECT_GE(y_bottom_epsilon, std::abs(y_bottom_deviation));
    EXPECT_EQ(y_bottom_deviation,
              end_edge_end_in_layer_y - selection.end.edge_end.y());

    if (expected_result.Length() >= 15) {
      bool start_hidden = expected_result.Get(context, 13)
                              .ToLocalChecked()
                              .As<v8::Boolean>()
                              ->Value();
      bool end_hidden = expected_result.Get(context, 14)
                            .ToLocalChecked()
                            .As<v8::Boolean>()
                            ->Value();

      EXPECT_EQ(start_hidden, selection.start.hidden);
      EXPECT_EQ(end_hidden, selection.end.hidden);
    }
  }

  void RunTestWithMultipleFiles(
      const char* test_file,
      std::initializer_list<const char*> auxiliary_files) {
    for (const char* auxiliary_file : auxiliary_files) {
      RegisterMockedHttpURLLoad(auxiliary_file);
    }

    RunTest(test_file);
  }

  GraphicsLayer* GetExpectedLayerForSelection(blink::Node* node) const {
    CompositedLayerMapping* clm = node->GetLayoutObject()
                                      ->EnclosingLayer()
                                      ->EnclosingLayerForPaintInvalidation()
                                      ->GetCompositedLayerMapping();

    // If the Node is a scroller, the selection will be relative to its
    // scrolling contents layer.
    return clm->ScrollingContentsLayer() ? clm->ScrollingContentsLayer()
                                         : clm->MainGraphicsLayer();
  }

  frame_test_helpers::TestWebWidgetClient web_widget_client_;
  frame_test_helpers::WebViewHelper web_view_helper_;
};

TEST_F(CompositedSelectionBoundsTest, None) {
  RunTestWithNoSelection("composited_selection_bounds_none.html");
}
TEST_F(CompositedSelectionBoundsTest, NoneReadonlyCaret) {
  RunTestWithNoSelection(
      "composited_selection_bounds_none_readonly_caret.html");
}
TEST_F(CompositedSelectionBoundsTest, DetachedFrame) {
  RunTestWithNoSelection("composited_selection_bounds_detached_frame.html");
}

TEST_F(CompositedSelectionBoundsTest, Basic) {
  RunTest("composited_selection_bounds_basic.html");
}
TEST_F(CompositedSelectionBoundsTest, Transformed) {
  RunTest("composited_selection_bounds_transformed.html");
}
TEST_F(CompositedSelectionBoundsTest, VerticalRightToLeft) {
  RunTest("composited_selection_bounds_vertical_rl.html");
}
TEST_F(CompositedSelectionBoundsTest, VerticalLeftToRight) {
  RunTest("composited_selection_bounds_vertical_lr.html");
}
TEST_F(CompositedSelectionBoundsTest, SplitLayer) {
  RunTest("composited_selection_bounds_split_layer.html");
}
TEST_F(CompositedSelectionBoundsTest, Iframe) {
  RunTestWithMultipleFiles("composited_selection_bounds_iframe.html",
                           {"composited_selection_bounds_basic.html"});
}
TEST_F(CompositedSelectionBoundsTest, Editable) {
  RunTest("composited_selection_bounds_editable.html");
}
TEST_F(CompositedSelectionBoundsTest, EditableDiv) {
  RunTest("composited_selection_bounds_editable_div.html");
}
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#if !defined(OS_ANDROID)
TEST_F(CompositedSelectionBoundsTest, Input) {
  RunTest("composited_selection_bounds_input.html");
}
TEST_F(CompositedSelectionBoundsTest, InputScrolled) {
  RunTest("composited_selection_bounds_input_scrolled.html");
}
#endif
#endif

class TestWillInsertBodyWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  TestWillInsertBodyWebFrameClient() : did_load_(false) {}
  ~TestWillInsertBodyWebFrameClient() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void DidCommitNavigation(const WebHistoryItem&,
                           WebHistoryCommitType,
                           bool) override {
    did_load_ = true;
  }

  bool did_load_;
};

TEST_F(WebFrameTest, HTMLDocument) {
  RegisterMockedHttpURLLoad("clipped-body.html");

  TestWillInsertBodyWebFrameClient web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "clipped-body.html",
                                    &web_frame_client);

  EXPECT_TRUE(web_frame_client.did_load_);
}

TEST_F(WebFrameTest, EmptyDocument) {
  RegisterMockedHttpURLLoad("frameserializer/svg/green_rectangle.svg");

  TestWillInsertBodyWebFrameClient web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(&web_frame_client);

  EXPECT_FALSE(web_frame_client.did_load_);
}

TEST_F(WebFrameTest, MoveCaretSelectionTowardsWindowPointWithNoSelection) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank");
  WebFrame* frame = web_view_helper.GetWebView()->MainFrame();

  // This test passes if this doesn't crash.
  frame->ToWebLocalFrame()->MoveCaretSelection(gfx::Point());
}

class TextCheckClient : public WebTextCheckClient {
 public:
  TextCheckClient() : number_of_times_checked_(0) {}
  ~TextCheckClient() override = default;

  // WebTextCheckClient:
  bool IsSpellCheckingEnabled() const override { return true; }
  void RequestCheckingOfText(
      const WebString&,
      std::unique_ptr<WebTextCheckingCompletion> completion) override {
    ++number_of_times_checked_;
    Vector<WebTextCheckingResult> results;
    const int kMisspellingStartOffset = 1;
    const int kMisspellingLength = 8;
    results.push_back(WebTextCheckingResult(
        kWebTextDecorationTypeSpelling, kMisspellingStartOffset,
        kMisspellingLength, WebVector<WebString>()));
    completion->DidFinishCheckingText(results);
  }

  int NumberOfTimesChecked() const { return number_of_times_checked_; }

 private:
  int number_of_times_checked_;
};

TEST_F(WebFrameTest, ReplaceMisspelledRange) {
  RegisterMockedHttpURLLoad("spell.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "spell.html", &web_view_helper);

  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  TextCheckClient textcheck;
  frame->SetTextCheckClient(&textcheck);

  Document* document = frame->GetFrame()->GetDocument();
  Element* element = document->getElementById("data");

  web_view_helper.GetWebView()->GetSettings()->SetEditingBehavior(
      mojom::EditingBehavior::kEditingWindowsBehavior);

  element->focus();
  NonThrowableExceptionState exception_state;
  document->execCommand("InsertText", false, "_wellcome_.", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  document->GetFrame()
      ->GetSpellChecker()
      .GetIdleSpellCheckController()
      .ForceInvocationForTesting();

  const int kAllTextBeginOffset = 0;
  const int kAllTextLength = 11;
  frame->SelectRange(WebRange(kAllTextBeginOffset, kAllTextLength),
                     WebLocalFrame::kHideSelectionHandle,
                     SelectionMenuBehavior::kHide);
  EphemeralRange selection_range = frame->GetFrame()
                                       ->Selection()
                                       .ComputeVisibleSelectionInDOMTree()
                                       .ToNormalizedEphemeralRange();

  EXPECT_EQ(1, textcheck.NumberOfTimesChecked());
  EXPECT_EQ(1, NumMarkersInRange(document, selection_range,
                                 DocumentMarker::MarkerTypes::Spelling()));

  frame->ReplaceMisspelledRange("welcome");
  EXPECT_EQ("_welcome_.", WebFrameContentDumper::DumpWebViewAsText(
                              web_view_helper.GetWebView(),
                              std::numeric_limits<size_t>::max())
                              .Utf8());
}

TEST_F(WebFrameTest, RemoveSpellingMarkers) {
  RegisterMockedHttpURLLoad("spell.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "spell.html", &web_view_helper);

  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  TextCheckClient textcheck;
  frame->SetTextCheckClient(&textcheck);

  Document* document = frame->GetFrame()->GetDocument();
  Element* element = document->getElementById("data");

  web_view_helper.GetWebView()->GetSettings()->SetEditingBehavior(
      mojom::EditingBehavior::kEditingWindowsBehavior);

  element->focus();
  NonThrowableExceptionState exception_state;
  document->execCommand("InsertText", false, "_wellcome_.", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  document->GetFrame()
      ->GetSpellChecker()
      .GetIdleSpellCheckController()
      .ForceInvocationForTesting();

  frame->RemoveSpellingMarkers();

  const int kAllTextBeginOffset = 0;
  const int kAllTextLength = 11;
  frame->SelectRange(WebRange(kAllTextBeginOffset, kAllTextLength),
                     WebLocalFrame::kHideSelectionHandle,
                     SelectionMenuBehavior::kHide);
  EphemeralRange selection_range = frame->GetFrame()
                                       ->Selection()
                                       .ComputeVisibleSelectionInDOMTree()
                                       .ToNormalizedEphemeralRange();

  EXPECT_EQ(0, NumMarkersInRange(document, selection_range,
                                 DocumentMarker::MarkerTypes::Spelling()));
}

static void GetSpellingMarkerOffsets(WebVector<unsigned>* offsets,
                                     const Document& document) {
  Vector<unsigned> result;
  const DocumentMarkerVector& document_markers = document.Markers().Markers();
  for (size_t i = 0; i < document_markers.size(); ++i)
    result.push_back(document_markers[i]->StartOffset());
  offsets->Assign(result);
}

TEST_F(WebFrameTest, RemoveSpellingMarkersUnderWords) {
  RegisterMockedHttpURLLoad("spell.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "spell.html", &web_view_helper);

  WebLocalFrameImpl* web_frame = web_view_helper.LocalMainFrame();
  TextCheckClient textcheck;
  web_frame->SetTextCheckClient(&textcheck);

  LocalFrame* frame = web_frame->GetFrame();
  Document* document = frame->GetDocument();
  Element* element = document->getElementById("data");

  web_view_helper.GetWebView()->GetSettings()->SetEditingBehavior(
      mojom::EditingBehavior::kEditingWindowsBehavior);

  element->focus();
  NonThrowableExceptionState exception_state;
  document->execCommand("InsertText", false, " wellcome ", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  frame->GetSpellChecker()
      .GetIdleSpellCheckController()
      .ForceInvocationForTesting();

  WebVector<unsigned> offsets1;
  GetSpellingMarkerOffsets(&offsets1, *frame->GetDocument());
  EXPECT_EQ(1U, offsets1.size());

  Vector<String> words;
  words.push_back("wellcome");
  frame->RemoveSpellingMarkersUnderWords(words);

  WebVector<unsigned> offsets2;
  GetSpellingMarkerOffsets(&offsets2, *frame->GetDocument());
  EXPECT_EQ(0U, offsets2.size());
}

class StubbornTextCheckClient : public WebTextCheckClient {
 public:
  StubbornTextCheckClient() : completion_(nullptr) {}
  ~StubbornTextCheckClient() override = default;

  // WebTextCheckClient:
  bool IsSpellCheckingEnabled() const override { return true; }
  void RequestCheckingOfText(
      const WebString&,
      std::unique_ptr<WebTextCheckingCompletion> completion) override {
    completion_ = std::move(completion);
  }

  void KickNoResults() { Kick(-1, -1, kWebTextDecorationTypeSpelling); }

  void Kick() { Kick(1, 8, kWebTextDecorationTypeSpelling); }

  void KickGrammar() { Kick(1, 8, kWebTextDecorationTypeGrammar); }

 private:
  void Kick(int misspelling_start_offset,
            int misspelling_length,
            WebTextDecorationType type) {
    if (!completion_)
      return;
    Vector<WebTextCheckingResult> results;
    if (misspelling_start_offset >= 0 && misspelling_length > 0) {
      results.push_back(WebTextCheckingResult(type, misspelling_start_offset,
                                              misspelling_length));
    }
    completion_->DidFinishCheckingText(results);
    completion_.reset();
  }

  std::unique_ptr<WebTextCheckingCompletion> completion_;
};

TEST_F(WebFrameTest, SlowSpellcheckMarkerPosition) {
  RegisterMockedHttpURLLoad("spell.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "spell.html", &web_view_helper);

  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  StubbornTextCheckClient textcheck;
  frame->SetTextCheckClient(&textcheck);

  Document* document = frame->GetFrame()->GetDocument();
  Element* element = document->getElementById("data");

  web_view_helper.GetWebView()->GetSettings()->SetEditingBehavior(
      mojom::EditingBehavior::kEditingWindowsBehavior);

  element->focus();
  NonThrowableExceptionState exception_state;
  document->execCommand("InsertText", false, "wellcome ", exception_state);
  EXPECT_FALSE(exception_state.HadException());
  document->execCommand("InsertText", false, "he", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  document->GetFrame()
      ->GetSpellChecker()
      .GetIdleSpellCheckController()
      .ForceInvocationForTesting();

  textcheck.Kick();

  WebVector<unsigned> offsets;
  GetSpellingMarkerOffsets(&offsets, *frame->GetFrame()->GetDocument());
  EXPECT_EQ(0U, offsets.size());
}

TEST_F(WebFrameTest, SpellcheckResultErasesMarkers) {
  RegisterMockedHttpURLLoad("spell.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "spell.html", &web_view_helper);

  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  StubbornTextCheckClient textcheck;
  frame->SetTextCheckClient(&textcheck);

  Document* document = frame->GetFrame()->GetDocument();
  Element* element = document->getElementById("data");

  web_view_helper.GetWebView()->GetSettings()->SetEditingBehavior(
      mojom::EditingBehavior::kEditingWindowsBehavior);

  element->focus();
  NonThrowableExceptionState exception_state;
  document->execCommand("InsertText", false, "welcome ", exception_state);

  document->GetFrame()
      ->GetSpellChecker()
      .GetIdleSpellCheckController()
      .ForceInvocationForTesting();

  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  EXPECT_FALSE(exception_state.HadException());
  auto range = EphemeralRange::RangeOfContents(*element);
  document->Markers().AddSpellingMarker(range);
  document->Markers().AddGrammarMarker(range);
  EXPECT_EQ(2U, document->Markers().Markers().size());

  textcheck.KickNoResults();
  EXPECT_EQ(0U, document->Markers().Markers().size());
}

TEST_F(WebFrameTest, SpellcheckResultsSavedInDocument) {
  RegisterMockedHttpURLLoad("spell.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "spell.html", &web_view_helper);

  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  StubbornTextCheckClient textcheck;
  frame->SetTextCheckClient(&textcheck);

  Document* document = frame->GetFrame()->GetDocument();
  Element* element = document->getElementById("data");

  web_view_helper.GetWebView()->GetSettings()->SetEditingBehavior(
      mojom::EditingBehavior::kEditingWindowsBehavior);

  element->focus();
  NonThrowableExceptionState exception_state;
  document->execCommand("InsertText", false, "wellcome ", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  document->GetFrame()
      ->GetSpellChecker()
      .GetIdleSpellCheckController()
      .ForceInvocationForTesting();

  textcheck.Kick();
  ASSERT_EQ(1U, document->Markers().Markers().size());
  ASSERT_NE(static_cast<DocumentMarker*>(nullptr),
            document->Markers().Markers()[0]);
  EXPECT_EQ(DocumentMarker::kSpelling,
            document->Markers().Markers()[0]->GetType());

  document->execCommand("InsertText", false, "wellcome ", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  document->GetFrame()
      ->GetSpellChecker()
      .GetIdleSpellCheckController()
      .ForceInvocationForTesting();

  textcheck.KickGrammar();
  ASSERT_EQ(1U, document->Markers().Markers().size());
  ASSERT_NE(static_cast<DocumentMarker*>(nullptr),
            document->Markers().Markers()[0]);
  EXPECT_EQ(DocumentMarker::kGrammar,
            document->Markers().Markers()[0]->GetType());
}

class TestAccessInitialDocumentLocalFrameHost : public FakeLocalFrameHost {
 public:
  TestAccessInitialDocumentLocalFrameHost() = default;
  ~TestAccessInitialDocumentLocalFrameHost() override = default;

  // FakeLocalFrameHost:
  void DidAccessInitialDocument() override { ++did_access_initial_document_; }

  // !!!!!!!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!!!!!!
  // If the actual counts in the tests below increase, this could be an
  // indicator of a bug that causes DidAccessInitialDocument() to always be
  // invoked, regardless of whether or not the initial document is accessed.
  // Please do not simply increment the expected counts in the below tests
  // without understanding what's causing the increased count.
  int did_access_initial_document_ = 0;
};

TEST_F(WebFrameTest, DidAccessInitialDocumentBody) {
  TestAccessInitialDocumentLocalFrameHost frame_host;
  frame_test_helpers::TestWebFrameClient web_frame_client;
  frame_host.Init(web_frame_client.GetRemoteNavigationAssociatedInterfaces());
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(&web_frame_client);
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_document_);

  // Create another window that will try to access it.
  frame_test_helpers::WebViewHelper new_web_view_helper;
  WebViewImpl* new_view = new_web_view_helper.InitializeWithOpener(
      web_view_helper.GetWebView()->MainFrame());
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_document_);

  // Access the initial document by modifying the body.
  new_view->MainFrameImpl()->ExecuteScript(
      WebScriptSource("window.opener.document.body.innerHTML += 'Modified';"));
  RunPendingTasks();
  EXPECT_EQ(1, frame_host.did_access_initial_document_);

  web_view_helper.Reset();
}

TEST_F(WebFrameTest, DidAccessInitialDocumentOpen) {
  TestAccessInitialDocumentLocalFrameHost frame_host;
  frame_test_helpers::TestWebFrameClient web_frame_client;
  frame_host.Init(web_frame_client.GetRemoteNavigationAssociatedInterfaces());
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(&web_frame_client);
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_document_);

  // Create another window that will try to access it.
  frame_test_helpers::WebViewHelper new_web_view_helper;
  WebViewImpl* new_view = new_web_view_helper.InitializeWithOpener(
      web_view_helper.GetWebView()->MainFrame());
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_document_);

  // Access the initial document by calling document.open(), which allows
  // arbitrary modification of the initial document.
  new_view->MainFrameImpl()->ExecuteScript(
      WebScriptSource("window.opener.document.open();"));
  RunPendingTasks();
  EXPECT_EQ(1, frame_host.did_access_initial_document_);

  web_view_helper.Reset();
}

TEST_F(WebFrameTest, DidAccessInitialDocumentNavigator) {
  TestAccessInitialDocumentLocalFrameHost frame_host;
  frame_test_helpers::TestWebFrameClient web_frame_client;
  frame_host.Init(web_frame_client.GetRemoteNavigationAssociatedInterfaces());
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(&web_frame_client);
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_document_);

  // Create another window that will try to access it.
  frame_test_helpers::WebViewHelper new_web_view_helper;
  WebViewImpl* new_view = new_web_view_helper.InitializeWithOpener(
      web_view_helper.GetWebView()->MainFrame());
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_document_);

  // Access the initial document to get to the navigator object.
  new_view->MainFrameImpl()->ExecuteScript(
      WebScriptSource("console.log(window.opener.navigator);"));
  RunPendingTasks();
  EXPECT_EQ(1, frame_host.did_access_initial_document_);

  web_view_helper.Reset();
}

TEST_F(WebFrameTest, DidAccessInitialDocumentViaJavascriptUrl) {
  TestAccessInitialDocumentLocalFrameHost frame_host;
  frame_test_helpers::TestWebFrameClient web_frame_client;
  frame_host.Init(web_frame_client.GetRemoteNavigationAssociatedInterfaces());
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(&web_frame_client);
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_document_);

  // Access the initial document from a javascript: URL.
  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                "javascript:document.body.appendChild(document."
                                "createTextNode('Modified'))");
  EXPECT_EQ(1, frame_host.did_access_initial_document_);

  web_view_helper.Reset();
}

TEST_F(WebFrameTest, DidAccessInitialDocumentBodyBeforeModalDialog) {
  TestAccessInitialDocumentLocalFrameHost frame_host;
  frame_test_helpers::TestWebFrameClient web_frame_client;
  frame_host.Init(web_frame_client.GetRemoteNavigationAssociatedInterfaces());
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(&web_frame_client);
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_document_);

  // Create another window that will try to access it.
  frame_test_helpers::WebViewHelper new_web_view_helper;
  WebViewImpl* new_view = new_web_view_helper.InitializeWithOpener(
      web_view_helper.GetWebView()->MainFrame());
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_document_);

  // Access the initial document by modifying the body.
  new_view->MainFrameImpl()->ExecuteScript(
      WebScriptSource("window.opener.document.body.innerHTML += 'Modified';"));
  RunPendingTasks();
  EXPECT_EQ(1, frame_host.did_access_initial_document_);

  // Run a modal dialog, which used to run a nested run loop and require
  // a special case for notifying about the access.
  new_view->MainFrameImpl()->ExecuteScript(
      WebScriptSource("window.opener.confirm('Modal');"));
  RunPendingTasks();
  EXPECT_EQ(1, frame_host.did_access_initial_document_);

  // Ensure that we don't notify again later.
  RunPendingTasks();
  EXPECT_EQ(1, frame_host.did_access_initial_document_);

  web_view_helper.Reset();
}

TEST_F(WebFrameTest, DidWriteToInitialDocumentBeforeModalDialog) {
  TestAccessInitialDocumentLocalFrameHost frame_host;
  frame_test_helpers::TestWebFrameClient web_frame_client;
  frame_host.Init(web_frame_client.GetRemoteNavigationAssociatedInterfaces());
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(&web_frame_client);
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_document_);

  // Create another window that will try to access it.
  frame_test_helpers::WebViewHelper new_web_view_helper;
  WebViewImpl* new_view = new_web_view_helper.InitializeWithOpener(
      web_view_helper.GetWebView()->MainFrame());
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_document_);

  // Access the initial document with document.write, which moves us past the
  // initial empty document state of the state machine.
  new_view->MainFrameImpl()->ExecuteScript(
      WebScriptSource("window.opener.document.write('Modified'); "
                      "window.opener.document.close();"));
  RunPendingTasks();
  EXPECT_EQ(1, frame_host.did_access_initial_document_);

  // Run a modal dialog, which used to run a nested run loop and require
  // a special case for notifying about the access.
  new_view->MainFrameImpl()->ExecuteScript(
      WebScriptSource("window.opener.confirm('Modal');"));
  RunPendingTasks();
  EXPECT_EQ(1, frame_host.did_access_initial_document_);

  // Ensure that we don't notify again later.
  RunPendingTasks();
  EXPECT_EQ(1, frame_host.did_access_initial_document_);

  web_view_helper.Reset();
}

class TestScrolledFrameClient : public frame_test_helpers::TestWebFrameClient {
 public:
  TestScrolledFrameClient() { Reset(); }
  ~TestScrolledFrameClient() override = default;

  void Reset() { did_scroll_frame_ = false; }
  bool WasFrameScrolled() const { return did_scroll_frame_; }

  // WebLocalFrameClient:
  void DidChangeScrollOffset() override {
    if (Frame()->Parent())
      return;
    EXPECT_FALSE(did_scroll_frame_);
    LocalFrameView* view = To<WebLocalFrameImpl>(Frame())->GetFrameView();
    // LocalFrameView can be scrolled in
    // LocalFrameView::SetFixedVisibleContentRect which is called from
    // LocalFrame::CreateView (before the frame is associated with the the
    // view).
    if (view)
      did_scroll_frame_ = true;
  }

 private:
  bool did_scroll_frame_;
};

TEST_F(WebFrameTest, CompositorScrollIsUserScrollLongPage) {
  RegisterMockedHttpURLLoad("long_scroll.html");
  TestScrolledFrameClient client;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "long_scroll.html", &client);
  web_view_helper.Resize(gfx::Size(1000, 1000));

  WebLocalFrameImpl* frame_impl = web_view_helper.LocalMainFrame();
  DocumentLoader::InitialScrollState& initial_scroll_state =
      frame_impl->GetFrame()
          ->Loader()
          .GetDocumentLoader()
          ->GetInitialScrollState();

  EXPECT_FALSE(client.WasFrameScrolled());
  EXPECT_FALSE(initial_scroll_state.was_scrolled_by_user);

  auto* scrollable_area = frame_impl->GetFrameView()->LayoutViewport();

  // Do a compositor scroll, verify that this is counted as a user scroll.
  scrollable_area->DidScroll(FloatPoint(0, 1));
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->ApplyViewportChangesForTesting({gfx::ScrollOffset(), gfx::Vector2dF(),
                                        1.7f, false, 0, 0,
                                        cc::BrowserControlsState::kBoth});
  EXPECT_TRUE(client.WasFrameScrolled());
  EXPECT_TRUE(initial_scroll_state.was_scrolled_by_user);

  client.Reset();
  initial_scroll_state.was_scrolled_by_user = false;

  // The page scale 1.0f and scroll.
  scrollable_area->DidScroll(FloatPoint(0, 2));
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->ApplyViewportChangesForTesting({gfx::ScrollOffset(), gfx::Vector2dF(),
                                        1.0f, false, 0, 0,
                                        cc::BrowserControlsState::kBoth});
  EXPECT_TRUE(client.WasFrameScrolled());
  EXPECT_TRUE(initial_scroll_state.was_scrolled_by_user);
  client.Reset();
  initial_scroll_state.was_scrolled_by_user = false;

  // No scroll event if there is no scroll delta.
  scrollable_area->DidScroll(FloatPoint(0, 2));
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->ApplyViewportChangesForTesting({gfx::ScrollOffset(), gfx::Vector2dF(),
                                        1.0f, false, 0, 0,
                                        cc::BrowserControlsState::kBoth});
  EXPECT_FALSE(client.WasFrameScrolled());
  EXPECT_FALSE(initial_scroll_state.was_scrolled_by_user);
  client.Reset();

  // Non zero page scale and scroll.
  scrollable_area->DidScroll(FloatPoint(9, 15));
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->ApplyViewportChangesForTesting({gfx::ScrollOffset(), gfx::Vector2dF(),
                                        0.6f, false, 0, 0,
                                        cc::BrowserControlsState::kBoth});
  EXPECT_TRUE(client.WasFrameScrolled());
  EXPECT_TRUE(initial_scroll_state.was_scrolled_by_user);
  client.Reset();
  initial_scroll_state.was_scrolled_by_user = false;

  // Programmatic scroll.
  frame_impl->ExecuteScript(WebScriptSource("window.scrollTo(0, 20);"));
  EXPECT_TRUE(client.WasFrameScrolled());
  EXPECT_FALSE(initial_scroll_state.was_scrolled_by_user);
  client.Reset();

  // Programmatic scroll to same offset. No scroll event should be generated.
  frame_impl->ExecuteScript(WebScriptSource("window.scrollTo(0, 20);"));
  EXPECT_FALSE(client.WasFrameScrolled());
  EXPECT_FALSE(initial_scroll_state.was_scrolled_by_user);
  client.Reset();
}

TEST_F(WebFrameTest, SiteForCookiesForRedirect) {
  String file_path = test::CoreTestDataPath("first_party.html");

  WebURL test_url(ToKURL("http://internal.test/first_party_redirect.html"));
  char redirect[] = "http://internal.test/first_party.html";
  WebURL redirect_url(ToKURL(redirect));
  WebURLResponse redirect_response;
  redirect_response.SetMimeType("text/html");
  redirect_response.SetHttpStatusCode(302);
  redirect_response.SetHttpHeaderField("Location", redirect);
  RegisterMockedURLLoadWithCustomResponse(test_url, file_path,
                                          redirect_response);

  WebURLResponse final_response;
  final_response.SetMimeType("text/html");
  RegisterMockedURLLoadWithCustomResponse(redirect_url, file_path,
                                          final_response);

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "first_party_redirect.html");
  EXPECT_TRUE(
      web_view_helper.GetWebView()
          ->MainFrameImpl()
          ->GetDocument()
          .SiteForCookies()
          .IsEquivalent(net::SiteForCookies::FromUrl(ToKURL(redirect))));
}

class TestNewWindowWebViewClient
    : public frame_test_helpers::TestWebViewClient {
 public:
  TestNewWindowWebViewClient() = default;
  ~TestNewWindowWebViewClient() override = default;

  // frame_test_helpers::TestWebFrameClient:
  WebView* CreateView(WebLocalFrame*,
                      const WebURLRequest&,
                      const WebWindowFeatures&,
                      const WebString&,
                      WebNavigationPolicy,
                      network::mojom::blink::WebSandboxFlags,
                      const FeaturePolicyFeatureState&,
                      const SessionStorageNamespaceId&,
                      bool& consumed_user_gesture) override {
    EXPECT_TRUE(false);
    return nullptr;
  }
};

class TestNewWindowWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  TestNewWindowWebFrameClient() : begin_navigation_call_count_(0) {}
  ~TestNewWindowWebFrameClient() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void BeginNavigation(std::unique_ptr<WebNavigationInfo> info) override {
    if (ignore_navigations_) {
      begin_navigation_call_count_++;
      return;
    }
    TestWebFrameClient::BeginNavigation(std::move(info));
  }

  int BeginNavigationCallCount() const { return begin_navigation_call_count_; }
  void IgnoreNavigations() { ignore_navigations_ = true; }

 private:
  bool ignore_navigations_ = false;
  int begin_navigation_call_count_;
};

TEST_F(WebFrameTest, ModifiedClickNewWindow) {
  // This test checks that ctrl+click does not just open a new window,
  // but instead goes to client to decide the navigation policy.
  RegisterMockedHttpURLLoad("ctrl_click.html");
  RegisterMockedHttpURLLoad("hello_world.html");
  TestNewWindowWebViewClient web_view_client;
  TestNewWindowWebFrameClient web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "ctrl_click.html",
                                    &web_frame_client, &web_view_client);

  auto* frame =
      To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame());
  LocalDOMWindow* window = frame->DomWindow();
  KURL destination = ToKURL(base_url_ + "hello_world.html");

  // ctrl+click event
  MouseEventInit* mouse_initializer = MouseEventInit::Create();
  mouse_initializer->setView(window);
  mouse_initializer->setButton(1);
  mouse_initializer->setCtrlKey(true);

  Event* event =
      MouseEvent::Create(nullptr, event_type_names::kClick, mouse_initializer);
  FrameLoadRequest frame_request(window, ResourceRequest(destination));
  frame_request.SetNavigationPolicy(NavigationPolicyFromEvent(event));
  frame_request.SetTriggeringEventInfo(TriggeringEventInfo::kFromTrustedEvent);
  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  web_frame_client.IgnoreNavigations();
  frame->Loader().StartNavigation(frame_request, WebFrameLoadType::kStandard);
  frame_test_helpers::PumpPendingRequestsForFrameToLoad(
      web_view_helper.LocalMainFrame());

  // BeginNavigation should be called for the ctrl+click.
  EXPECT_EQ(1, web_frame_client.BeginNavigationCallCount());
}

class TestBeginNavigationCacheModeClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  TestBeginNavigationCacheModeClient()
      : cache_mode_(mojom::FetchCacheMode::kDefault) {}
  ~TestBeginNavigationCacheModeClient() override = default;

  mojom::FetchCacheMode GetCacheMode() const { return cache_mode_; }

  void BeginNavigation(std::unique_ptr<WebNavigationInfo> info) override {
    cache_mode_ = info->url_request.GetCacheMode();
    TestWebFrameClient::BeginNavigation(std::move(info));
  }

 private:
  mojom::FetchCacheMode cache_mode_;
};

TEST_F(WebFrameTest, BackToReload) {
  RegisterMockedHttpURLLoad("fragment_middle_click.html");
  TestBeginNavigationCacheModeClient client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fragment_middle_click.html",
                                    &client);
  WebLocalFrame* frame = web_view_helper.LocalMainFrame();
  const FrameLoader& main_frame_loader =
      web_view_helper.LocalMainFrame()->GetFrame()->Loader();
  Persistent<HistoryItem> first_item =
      main_frame_loader.GetDocumentLoader()->GetHistoryItem();
  EXPECT_TRUE(first_item);

  RegisterMockedHttpURLLoad("white-1x1.png");
  frame_test_helpers::LoadFrame(frame, base_url_ + "white-1x1.png");
  EXPECT_NE(first_item.Get(),
            main_frame_loader.GetDocumentLoader()->GetHistoryItem());

  frame_test_helpers::LoadHistoryItem(frame, WebHistoryItem(first_item.Get()),
                                      mojom::FetchCacheMode::kDefault);
  EXPECT_EQ(first_item.Get(),
            main_frame_loader.GetDocumentLoader()->GetHistoryItem());

  frame_test_helpers::ReloadFrame(frame);
  EXPECT_EQ(mojom::FetchCacheMode::kValidateCache, client.GetCacheMode());
}

TEST_F(WebFrameTest, ReloadPost) {
  RegisterMockedHttpURLLoad("reload_post.html");
  TestBeginNavigationCacheModeClient client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "reload_post.html", &client);
  WebLocalFrame* frame = web_view_helper.LocalMainFrame();

  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                "javascript:document.forms[0].submit()");
  // Pump requests one more time after the javascript URL has executed to
  // trigger the actual POST load request.
  frame_test_helpers::PumpPendingRequestsForFrameToLoad(
      web_view_helper.LocalMainFrame());
  EXPECT_EQ(WebString::FromUTF8("POST"),
            frame->GetDocumentLoader()->HttpMethod());

  frame_test_helpers::ReloadFrame(frame);
  EXPECT_EQ(mojom::FetchCacheMode::kValidateCache, client.GetCacheMode());
  EXPECT_EQ(kWebNavigationTypeFormResubmitted,
            frame->GetDocumentLoader()->GetNavigationType());
}

class TestCachePolicyWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  TestCachePolicyWebFrameClient()
      : cache_mode_(mojom::FetchCacheMode::kDefault),
        begin_navigation_call_count_(0) {}
  ~TestCachePolicyWebFrameClient() override = default;

  mojom::FetchCacheMode GetCacheMode() const { return cache_mode_; }
  int BeginNavigationCallCount() const { return begin_navigation_call_count_; }
  TestCachePolicyWebFrameClient& ChildClient(size_t i) {
    return *child_clients_[i].get();
  }
  size_t ChildFrameCreationCount() const { return child_clients_.size(); }

  // frame_test_helpers::TestWebFrameClient:
  WebLocalFrame* CreateChildFrame(
      WebLocalFrame* parent,
      mojom::blink::TreeScopeType scope,
      const WebString&,
      const WebString&,
      const FramePolicy&,
      const WebFrameOwnerProperties& frame_owner_properties,
      mojom::blink::FrameOwnerElementType) override {
    auto child = std::make_unique<TestCachePolicyWebFrameClient>();
    auto* child_ptr = child.get();
    child_clients_.push_back(std::move(child));
    return CreateLocalChild(*parent, scope, child_ptr);
  }
  void BeginNavigation(std::unique_ptr<WebNavigationInfo> info) override {
    cache_mode_ = info->url_request.GetCacheMode();
    begin_navigation_call_count_++;
    TestWebFrameClient::BeginNavigation(std::move(info));
  }

 private:
  mojom::FetchCacheMode cache_mode_;
  Vector<std::unique_ptr<TestCachePolicyWebFrameClient>> child_clients_;
  int begin_navigation_call_count_;
};

TEST_F(WebFrameTest, ReloadIframe) {
  RegisterMockedHttpURLLoad("iframe_reload.html");
  RegisterMockedHttpURLLoad("visible_iframe.html");

  TestCachePolicyWebFrameClient main_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "iframe_reload.html",
                                    &main_frame_client);
  WebLocalFrameImpl* main_frame = web_view_helper.LocalMainFrame();

  ASSERT_EQ(1U, main_frame_client.ChildFrameCreationCount());
  TestCachePolicyWebFrameClient* child_client =
      &main_frame_client.ChildClient(0);
  auto* child_frame = To<WebLocalFrameImpl>(main_frame->FirstChild());
  EXPECT_EQ(child_client, child_frame->Client());
  EXPECT_EQ(1u, main_frame->GetFrame()->Tree().ScopedChildCount());
  EXPECT_EQ(1, child_client->BeginNavigationCallCount());
  EXPECT_EQ(mojom::FetchCacheMode::kDefault, child_client->GetCacheMode());

  frame_test_helpers::ReloadFrame(main_frame);

  // A new child WebLocalFrame should have been created with a new client.
  ASSERT_EQ(2U, main_frame_client.ChildFrameCreationCount());
  TestCachePolicyWebFrameClient* new_child_client =
      &main_frame_client.ChildClient(1);
  auto* new_child_frame = To<WebLocalFrameImpl>(main_frame->FirstChild());
  EXPECT_EQ(new_child_client, new_child_frame->Client());
  ASSERT_NE(child_client, new_child_client);
  ASSERT_NE(child_frame, new_child_frame);
  // But there should still only be one subframe.
  EXPECT_EQ(1u, main_frame->GetFrame()->Tree().ScopedChildCount());

  EXPECT_EQ(1, new_child_client->BeginNavigationCallCount());
  // Sub-frames should not be forcibly revalidated.
  // TODO(toyoshim): Will consider to revalidate main resources in sub-frames
  // on reloads. Or will do only for bypassingCache.
  EXPECT_EQ(mojom::FetchCacheMode::kDefault, new_child_client->GetCacheMode());
}

class TestSameDocumentWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  TestSameDocumentWebFrameClient() : frame_load_type_reload_seen_(false) {}
  ~TestSameDocumentWebFrameClient() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void BeginNavigation(std::unique_ptr<WebNavigationInfo> info) override {
    if (info->frame_load_type == WebFrameLoadType::kReload)
      frame_load_type_reload_seen_ = true;
    TestWebFrameClient::BeginNavigation(std::move(info));
  }

  bool FrameLoadTypeReloadSeen() const { return frame_load_type_reload_seen_; }

 private:
  bool frame_load_type_reload_seen_;
};

TEST_F(WebFrameTest, NavigateToSame) {
  RegisterMockedHttpURLLoad("navigate_to_same.html");
  TestSameDocumentWebFrameClient client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "navigate_to_same.html",
                                    &client);
  EXPECT_FALSE(client.FrameLoadTypeReloadSeen());

  auto* local_frame =
      To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame());
  FrameLoadRequest frame_request(
      nullptr, ResourceRequest(local_frame->GetDocument()->Url()));
  local_frame->Loader().StartNavigation(frame_request);
  frame_test_helpers::PumpPendingRequestsForFrameToLoad(
      web_view_helper.LocalMainFrame());

  EXPECT_TRUE(client.FrameLoadTypeReloadSeen());
}

class TestMainFrameIntersectionChanged
    : public frame_test_helpers::TestWebFrameClient {
 public:
  TestMainFrameIntersectionChanged() = default;
  ~TestMainFrameIntersectionChanged() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void OnMainFrameIntersectionChanged(
      const WebRect& intersection_rect) override {
    main_frame_intersection_ = intersection_rect;
  }

  WebRect MainFrameIntersection() const { return main_frame_intersection_; }

 private:
  WebRect main_frame_intersection_;
};

TEST_F(WebFrameTest, MainFrameIntersectionChanged) {
  TestMainFrameIntersectionChanged client;
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote();

  WebLocalFrameImpl* local_frame = frame_test_helpers::CreateLocalChild(
      *helper.RemoteMainFrame(), "frameName", WebFrameOwnerProperties(),
      nullptr, &client);

  WebFrameWidget* widget = local_frame->FrameWidget();
  ASSERT_TRUE(widget);

  gfx::Rect viewport_intersection(0, 11, 200, 89);
  gfx::Rect mainframe_intersection(0, 0, 200, 140);
  blink::mojom::FrameOcclusionState occlusion_state =
      blink::mojom::FrameOcclusionState::kUnknown;
  gfx::Transform transform;
  transform.Translate(100, 100);

  auto intersection_state = blink::mojom::blink::ViewportIntersectionState(
      viewport_intersection, mainframe_intersection, gfx::Rect(),
      occlusion_state, gfx::Size(), gfx::Point(), transform);
  static_cast<WebFrameWidgetBase*>(widget)->SetRemoteViewportIntersection(
      intersection_state);
  EXPECT_EQ(client.MainFrameIntersection(), blink::WebRect(100, 100, 200, 140));
}

class TestSameDocumentWithImageWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  TestSameDocumentWithImageWebFrameClient() : num_of_image_requests_(0) {}
  ~TestSameDocumentWithImageWebFrameClient() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void WillSendRequest(WebURLRequest& request,
                       ForRedirect for_redirect) override {
    if (request.GetRequestContext() ==
        mojom::blink::RequestContextType::IMAGE) {
      num_of_image_requests_++;
      EXPECT_EQ(mojom::FetchCacheMode::kDefault, request.GetCacheMode());
    }
  }

  int NumOfImageRequests() const { return num_of_image_requests_; }

 private:
  int num_of_image_requests_;
};

TEST_F(WebFrameTest, NavigateToSameNoConditionalRequestForSubresource) {
  RegisterMockedHttpURLLoad("foo_with_image.html");
  RegisterMockedHttpURLLoad("white-1x1.png");
  TestSameDocumentWithImageWebFrameClient client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo_with_image.html", &client,
                                    nullptr, nullptr,
                                    &ConfigureLoadsImagesAutomatically);

  WebCache::Clear();
  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                base_url_ + "foo_with_image.html");

  // 2 images are requested, and each triggers 2 willSendRequest() calls,
  // once for preloading and once for the real request.
  EXPECT_EQ(client.NumOfImageRequests(), 4);
}

TEST_F(WebFrameTest, WebNodeImageContents) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank");
  WebLocalFrame* frame = web_view_helper.LocalMainFrame();

  static const char kBluePNG[] =
      "<img "
      "src=\"data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAoAAAAKCAYAAACNMs+"
      "9AAAAGElEQVQYV2NkYPj/n4EIwDiqEF8oUT94AFIQE/cCn90IAAAAAElFTkSuQmCC\">";

  // Load up the image and test that we can extract the contents.
  KURL test_url = ToKURL("about:blank");
  frame_test_helpers::LoadHTMLString(frame, kBluePNG, test_url);

  WebNode node = frame->GetDocument().Body().FirstChild();
  EXPECT_TRUE(node.IsElementNode());
  WebElement element = node.To<WebElement>();
  SkBitmap image = element.ImageContents();
  ASSERT_FALSE(image.isNull());
  EXPECT_EQ(image.width(), 10);
  EXPECT_EQ(image.height(), 10);
  EXPECT_EQ(image.getColor(0, 0), SK_ColorBLUE);
}

TEST_F(WebFrameTest, WebNodeImageContentsWithOrientation) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank");
  WebLocalFrame* frame = web_view_helper.LocalMainFrame();

  // 4x8 jpg with orientation = 6 ( 90 degree CW rotation ).
  // w - white, b - blue.
  //   raw      =>       oriented
  // w w w w          b b b b w w w w
  // w w w w          b b b b w w w w
  // w w w w          b b b b w w w w
  // w w w w          b b b b w w w w
  // b b b b
  // b b b b
  // b b b b
  // b b b b
  static const char kBlueJPGWithOrientation[] =
      "<img "
      "src=\"data:image/"
      "jpeg;base64,/9j/4AAQSkZJRgABAQEAYABgAAD/4QBiRXhpZgAATU0AKgAAAAgABQESAAM"
      "AAAABAAYAAAEaAAUAAAABAAAASgEbAAUAAAABAAAAUgEoAAMAAAABAAIAAAITAAMAAAABAA"
      "EAAAAAAAAAAABgAAAAAQAAAGAAAAAB/9sAQwACAQECAQECAgICAgICAgMFAwMDAwMGBAQDB"
      "QcGBwcHBgcHCAkLCQgICggHBwoNCgoLDAwMDAcJDg8NDA4LDAwM/9sAQwECAgIDAwMGAwMG"
      "DAgHCAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAw"
      "M/8AAEQgACAAEAwEiAAIRAQMRAf/EAB8AAAEFAQEBAQEBAAAAAAAAAAABAgMEBQYHCAkKC/"
      "/EALUQAAIBAwMCBAMFBQQEAAABfQECAwAEEQUSITFBBhNRYQcicRQygZGhCCNCscEVUtHwJ"
      "DNicoIJChYXGBkaJSYnKCkqNDU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3"
      "eHl6g4SFhoeIiYqSk5SVlpeYmZqio6Slpqeoqaqys7S1tre4ubrCw8TFxsfIycrS09TV1tf"
      "Y2drh4uPk5ebn6Onq8fLz9PX29/j5+v/EAB8BAAMBAQEBAQEBAQEAAAAAAAABAgMEBQYHCA"
      "kKC//EALURAAIBAgQEAwQHBQQEAAECdwABAgMRBAUhMQYSQVEHYXETIjKBCBRCkaGxwQkjM"
      "1LwFWJy0QoWJDThJfEXGBkaJicoKSo1Njc4OTpDREVGR0hJSlNUVVZXWFlaY2RlZmdoaWpz"
      "dHV2d3h5eoKDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytL"
      "T1NXW19jZ2uLj5OXm5+jp6vLz9PX29/j5+v/aAAwDAQACEQMRAD8A7j/iMz/6tv8A/Mgf/e"
    "2iiiv9ff8AiVzwx/6Fn/lbEf8Ay0+A/tvG/wA/4L/I/9k=\">";

  // Load up the image and test that we can extract the contents.
  KURL test_url = ToKURL("about:blank");
  frame_test_helpers::LoadHTMLString(frame, kBlueJPGWithOrientation, test_url);

  WebNode node = frame->GetDocument().Body().FirstChild();
  EXPECT_TRUE(node.IsElementNode());
  WebElement element = node.To<WebElement>();

  SkBitmap image_with_orientation = element.ImageContents();
  ASSERT_FALSE(image_with_orientation.isNull());
  EXPECT_EQ(image_with_orientation.width(), 8);
  EXPECT_EQ(image_with_orientation.height(), 4);
  // Should be almost blue.
  SkColor oriented_color = image_with_orientation.getColor(0, 0);
  EXPECT_NEAR(SkColorGetR(oriented_color), SkColorGetR(SK_ColorBLUE), 5);
  EXPECT_NEAR(SkColorGetG(oriented_color), SkColorGetG(SK_ColorBLUE), 5);
  EXPECT_NEAR(SkColorGetB(oriented_color), SkColorGetB(SK_ColorBLUE), 5);
  EXPECT_NEAR(SkColorGetA(oriented_color), SkColorGetA(SK_ColorBLUE), 5);
}

class TestStartStopCallbackWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  TestStartStopCallbackWebFrameClient()
      : start_loading_count_(0), stop_loading_count_(0) {}
  ~TestStartStopCallbackWebFrameClient() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void DidStartLoading() override {
    TestWebFrameClient::DidStartLoading();
    start_loading_count_++;
  }
  void DidStopLoading() override {
    TestWebFrameClient::DidStopLoading();
    stop_loading_count_++;
  }

  int StartLoadingCount() const { return start_loading_count_; }
  int StopLoadingCount() const { return stop_loading_count_; }

 private:
  int start_loading_count_;
  int stop_loading_count_;
};

TEST_F(WebFrameTest, PushStateStartsAndStops) {
  RegisterMockedHttpURLLoad("push_state.html");
  TestStartStopCallbackWebFrameClient client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "push_state.html", &client);

  // Wait for push state navigation to complete.
  frame_test_helpers::PumpPendingRequestsForFrameToLoad(
      web_view_helper.LocalMainFrame());
  EXPECT_EQ(client.StartLoadingCount(), 2);
  EXPECT_EQ(client.StopLoadingCount(), 2);
}

TEST_F(WebFrameTest, IPAddressSpace) {
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view =
      web_view_helper.InitializeAndLoad("data:text/html,ip_address_space");

  network::mojom::IPAddressSpace values[] = {
      network::mojom::IPAddressSpace::kUnknown,
      network::mojom::IPAddressSpace::kLocal,
      network::mojom::IPAddressSpace::kPrivate,
      network::mojom::IPAddressSpace::kPublic};

  for (auto value : values) {
    auto params = std::make_unique<WebNavigationParams>();
    params->url = url_test_helpers::ToKURL("about:blank");
    params->navigation_timings.navigation_start = base::TimeTicks::Now();
    params->navigation_timings.fetch_start = base::TimeTicks::Now();
    params->is_browser_initiated = true;
    params->ip_address_space = value;
    web_view_helper.LocalMainFrame()->CommitNavigation(std::move(params),
                                                       nullptr);
    frame_test_helpers::PumpPendingRequestsForFrameToLoad(
        web_view_helper.LocalMainFrame());

    ExecutionContext* context =
        web_view->MainFrameImpl()->GetFrame()->DomWindow();
    EXPECT_EQ(value, context->AddressSpace());
  }
}

class TestDidNavigateCommitTypeWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  TestDidNavigateCommitTypeWebFrameClient()
      : last_commit_type_(kWebHistoryInertCommit) {}
  ~TestDidNavigateCommitTypeWebFrameClient() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void DidFinishSameDocumentNavigation(const WebHistoryItem&,
                                       WebHistoryCommitType type,
                                       bool content_initiated) override {
    last_commit_type_ = type;
  }

  WebHistoryCommitType LastCommitType() const { return last_commit_type_; }

 private:
  WebHistoryCommitType last_commit_type_;
};

TEST_F(WebFrameTest, SameDocumentHistoryNavigationCommitType) {
  RegisterMockedHttpURLLoad("push_state.html");
  TestDidNavigateCommitTypeWebFrameClient client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl =
      web_view_helper.InitializeAndLoad(base_url_ + "push_state.html", &client);
  auto* local_frame = To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  Persistent<HistoryItem> item =
      local_frame->Loader().GetDocumentLoader()->GetHistoryItem();
  RunPendingTasks();

  local_frame->Loader().GetDocumentLoader()->CommitSameDocumentNavigation(
      item->Url(), WebFrameLoadType::kBackForward, item.Get(),
      ClientRedirectPolicy::kNotClientRedirect, nullptr, /* origin_document */
      false,                                             /* has_event */
      nullptr /* extra_data */);
  EXPECT_EQ(kWebBackForwardCommit, client.LastCommitType());
}

// Tests that the first navigation in an initially blank subframe will result in
// a history entry being replaced and not a new one being added.
TEST_F(WebFrameTest, FirstBlankSubframeNavigation) {
  RegisterMockedHttpURLLoad("history.html");
  RegisterMockedHttpURLLoad("find.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank");

  WebLocalFrame* frame = web_view_helper.LocalMainFrame();

  frame->ExecuteScript(WebScriptSource(WebString::FromUTF8(
      "document.body.appendChild(document.createElement('iframe'))")));

  auto* iframe = To<WebLocalFrameImpl>(frame->FirstChild());

  std::string url1 = base_url_ + "history.html";
  frame_test_helpers::LoadFrame(iframe, url1);
  EXPECT_EQ(url1, iframe->GetDocument().Url().GetString().Utf8());
  EXPECT_TRUE(iframe->GetDocumentLoader()->ReplacesCurrentHistoryItem());

  std::string url2 = base_url_ + "find.html";
  frame_test_helpers::LoadFrame(iframe, url2);
  EXPECT_EQ(url2, iframe->GetDocument().Url().GetString().Utf8());
  EXPECT_FALSE(iframe->GetDocumentLoader()->ReplacesCurrentHistoryItem());
}

// Tests that a navigation in a frame with a non-blank initial URL will create
// a new history item, unlike the case above.
TEST_F(WebFrameTest, FirstNonBlankSubframeNavigation) {
  RegisterMockedHttpURLLoad("history.html");
  RegisterMockedHttpURLLoad("find.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank");

  WebLocalFrame* frame = web_view_helper.LocalMainFrame();

  std::string url1 = base_url_ + "history.html";
  std::string load_frame_js =
      "javascript:var f = document.createElement('iframe'); "
      "f.src = '";
  load_frame_js += url1 + "';" + "document.body.appendChild(f)";
  frame_test_helpers::LoadFrame(frame, load_frame_js);

  WebLocalFrame* iframe = frame->FirstChild()->ToWebLocalFrame();
  EXPECT_EQ(url1, iframe->GetDocument().Url().GetString().Utf8());

  std::string url2 = base_url_ + "find.html";
  frame_test_helpers::LoadFrame(iframe, url2);
  EXPECT_EQ(url2, iframe->GetDocument().Url().GetString().Utf8());
  EXPECT_FALSE(iframe->GetDocumentLoader()->ReplacesCurrentHistoryItem());
}

// Test verifies that layout will change a layer's scrollable attibutes
TEST_F(WebFrameTest, overflowHiddenRewrite) {
  RegisterMockedHttpURLLoad("non-scrollable.html");
  FixedLayoutTestWebWidgetClient client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr, &client,
                             &ConfigureCompositingWebView);

  web_view_helper.Resize(gfx::Size(100, 100));
  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                base_url_ + "non-scrollable.html");

  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  auto* frame_view = web_view_helper.LocalMainFrame()->GetFrameView();
  auto* cc_scroll_layer = frame_view->GetScrollableArea()->LayerForScrolling();
  ASSERT_TRUE(cc_scroll_layer);

  // Verify that the cc::Layer is not scrollable initially.
  auto* scroll_node = GetScrollNode(cc_scroll_layer);
  ASSERT_FALSE(scroll_node->user_scrollable_horizontal);
  ASSERT_FALSE(scroll_node->user_scrollable_vertical);

  // Call javascript to make the layer scrollable, and verify it.
  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  frame->ExecuteScript(WebScriptSource("allowScroll();"));
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  cc_scroll_layer = frame_view->GetScrollableArea()->LayerForScrolling();
  scroll_node = GetScrollNode(cc_scroll_layer);
  ASSERT_TRUE(scroll_node->user_scrollable_horizontal);
  ASSERT_TRUE(scroll_node->user_scrollable_vertical);
}

// Test that currentHistoryItem reflects the current page, not the provisional
// load.
TEST_F(WebFrameTest, CurrentHistoryItem) {
  RegisterMockedHttpURLLoad("fixed_layout.html");
  std::string url = base_url_ + "fixed_layout.html";

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();
  WebLocalFrame* frame = web_view_helper.GetWebView()->MainFrameImpl();
  const FrameLoader& main_frame_loader =
      web_view_helper.LocalMainFrame()->GetFrame()->Loader();
  WebURLRequest request(ToKURL(url));

  // Before navigation, there is no history item.
  EXPECT_FALSE(main_frame_loader.GetDocumentLoader()->GetHistoryItem());

  frame->StartNavigation(request);
  frame_test_helpers::PumpPendingRequestsForFrameToLoad(
      web_view_helper.LocalMainFrame());

  // After navigation, there is.
  HistoryItem* item = main_frame_loader.GetDocumentLoader()->GetHistoryItem();
  ASSERT_TRUE(item);
  EXPECT_EQ(WTF::String(url.data()), item->UrlString());
}

class FailCreateChildFrame : public frame_test_helpers::TestWebFrameClient {
 public:
  FailCreateChildFrame() : call_count_(0) {}
  ~FailCreateChildFrame() override = default;

  // frame_test_helpers::TestWebFrameClient:
  WebLocalFrame* CreateChildFrame(
      WebLocalFrame* parent,
      mojom::blink::TreeScopeType scope,
      const WebString& name,
      const WebString& fallback_name,
      const FramePolicy&,
      const WebFrameOwnerProperties& frame_owner_properties,
      mojom::blink::FrameOwnerElementType) override {
    ++call_count_;
    return nullptr;
  }

  int CallCount() const { return call_count_; }

 private:
  int call_count_;
};

// Test that we don't crash if WebLocalFrameClient::createChildFrame() fails.
TEST_F(WebFrameTest, CreateChildFrameFailure) {
  RegisterMockedHttpURLLoad("create_child_frame_fail.html");
  FailCreateChildFrame client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "create_child_frame_fail.html",
                                    &client);

  EXPECT_EQ(1, client.CallCount());
}

TEST_F(WebFrameTest, fixedPositionInFixedViewport) {
  RegisterMockedHttpURLLoad("fixed-position-in-fixed-viewport.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "fixed-position-in-fixed-viewport.html", nullptr, nullptr,
      nullptr, ConfigureAndroid);

  WebViewImpl* web_view = web_view_helper.GetWebView();
  web_view_helper.Resize(gfx::Size(100, 100));

  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();
  Element* bottom_fixed = document->getElementById("bottom-fixed");
  Element* top_bottom_fixed = document->getElementById("top-bottom-fixed");
  Element* right_fixed = document->getElementById("right-fixed");
  Element* left_right_fixed = document->getElementById("left-right-fixed");

  // The layout viewport will hit the min-scale limit of 0.25, so it'll be
  // 400x800.
  web_view_helper.Resize(gfx::Size(100, 200));
  EXPECT_EQ(800, bottom_fixed->OffsetTop() + bottom_fixed->OffsetHeight());
  EXPECT_EQ(800, top_bottom_fixed->OffsetHeight());

  // Now the layout viewport hits the content width limit of 500px so it'll be
  // 500x500.
  web_view_helper.Resize(gfx::Size(200, 200));
  EXPECT_EQ(500, right_fixed->OffsetLeft() + right_fixed->OffsetWidth());
  EXPECT_EQ(500, left_right_fixed->OffsetWidth());
}

TEST_F(WebFrameTest, FrameViewMoveWithSetFrameRect) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank");
  web_view_helper.Resize(gfx::Size(200, 200));
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  LocalFrameView* frame_view = web_view_helper.LocalMainFrame()->GetFrameView();
  EXPECT_EQ(IntRect(0, 0, 200, 200), frame_view->FrameRect());
  frame_view->SetFrameRect(IntRect(100, 100, 200, 200));
  EXPECT_EQ(IntRect(100, 100, 200, 200), frame_view->FrameRect());
}

TEST_F(WebFrameTest, FrameViewScrollAccountsForBrowserControls) {
  FixedLayoutTestWebWidgetClient client;
  RegisterMockedHttpURLLoad("long_scroll.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "long_scroll.html", nullptr,
                                    nullptr, &client, ConfigureAndroid);

  WebViewImpl* web_view = web_view_helper.GetWebView();
  LocalFrameView* frame_view = web_view_helper.LocalMainFrame()->GetFrameView();

  float browser_controls_height = 40;
  web_view->ResizeWithBrowserControls(gfx::Size(100, 100),
                                      browser_controls_height, 0, false);
  web_view->SetPageScaleFactor(2.0f);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  web_view->MainFrameImpl()->SetScrollOffset(WebSize(0, 2000));
  EXPECT_EQ(ScrollOffset(0, 1900),
            frame_view->LayoutViewport()->GetScrollOffset());

  // Simulate the browser controls showing by 20px, thus shrinking the viewport
  // and allowing it to scroll an additional 20px.
  web_view->MainFrameWidget()->ApplyViewportChangesForTesting(
      {gfx::ScrollOffset(), gfx::Vector2dF(), 1.0f, false,
       20.0f / browser_controls_height, 0, cc::BrowserControlsState::kBoth});
  EXPECT_EQ(ScrollOffset(0, 1920),
            frame_view->LayoutViewport()->MaximumScrollOffset());

  // Show more, make sure the scroll actually gets clamped.
  web_view->MainFrameWidget()->ApplyViewportChangesForTesting(
      {gfx::ScrollOffset(), gfx::Vector2dF(), 1.0f, false,
       20.0f / browser_controls_height, 0, cc::BrowserControlsState::kBoth});
  web_view->MainFrameImpl()->SetScrollOffset(WebSize(0, 2000));
  EXPECT_EQ(ScrollOffset(0, 1940),
            frame_view->LayoutViewport()->GetScrollOffset());

  // Hide until there's 10px showing.
  web_view->MainFrameWidget()->ApplyViewportChangesForTesting(
      {gfx::ScrollOffset(), gfx::Vector2dF(), 1.0f, false,
       -30.0f / browser_controls_height, 0, cc::BrowserControlsState::kBoth});
  EXPECT_EQ(ScrollOffset(0, 1910),
            frame_view->LayoutViewport()->MaximumScrollOffset());

  // Simulate a LayoutEmbeddedContent::resize. The frame is resized to
  // accommodate the browser controls and Blink's view of the browser controls
  // matches that of the CC
  web_view->MainFrameWidget()->ApplyViewportChangesForTesting(
      {gfx::ScrollOffset(), gfx::Vector2dF(), 1.0f, false,
       30.0f / browser_controls_height, 0, cc::BrowserControlsState::kBoth});
  web_view->ResizeWithBrowserControls(gfx::Size(100, 60), 40.0f, 0, true);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  EXPECT_EQ(ScrollOffset(0, 1940),
            frame_view->LayoutViewport()->MaximumScrollOffset());

  // Now simulate hiding.
  web_view->MainFrameWidget()->ApplyViewportChangesForTesting(
      {gfx::ScrollOffset(), gfx::Vector2dF(), 1.0f, false,
       -10.0f / browser_controls_height, 0, cc::BrowserControlsState::kBoth});
  EXPECT_EQ(ScrollOffset(0, 1930),
            frame_view->LayoutViewport()->MaximumScrollOffset());

  // Reset to original state: 100px widget height, browser controls fully
  // hidden.
  web_view->MainFrameWidget()->ApplyViewportChangesForTesting(
      {gfx::ScrollOffset(), gfx::Vector2dF(), 1.0f, false,
       -30.0f / browser_controls_height, 0, cc::BrowserControlsState::kBoth});
  web_view->ResizeWithBrowserControls(gfx::Size(100, 100),
                                      browser_controls_height, 0, false);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  EXPECT_EQ(ScrollOffset(0, 1900),
            frame_view->LayoutViewport()->MaximumScrollOffset());

  // Show the browser controls by just 1px, since we're zoomed in to 2X, that
  // should allow an extra 0.5px of scrolling in the visual viewport. Make
  // sure we're not losing any pixels when applying the adjustment on the
  // main frame.
  web_view->MainFrameWidget()->ApplyViewportChangesForTesting(
      {gfx::ScrollOffset(), gfx::Vector2dF(), 1.0f, false,
       1.0f / browser_controls_height, 0, cc::BrowserControlsState::kBoth});
  EXPECT_EQ(ScrollOffset(0, 1901),
            frame_view->LayoutViewport()->MaximumScrollOffset());

  web_view->MainFrameWidget()->ApplyViewportChangesForTesting(
      {gfx::ScrollOffset(), gfx::Vector2dF(), 1.0f, false,
       2.0f / browser_controls_height, 0, cc::BrowserControlsState::kBoth});
  EXPECT_EQ(ScrollOffset(0, 1903),
            frame_view->LayoutViewport()->MaximumScrollOffset());
}

TEST_F(WebFrameTest, MaximumScrollPositionCanBeNegative) {
  RegisterMockedHttpURLLoad("rtl-overview-mode.html");

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "rtl-overview-mode.html",
                                    nullptr, nullptr, &client,
                                    ConfigureAndroid);
  web_view_helper.GetWebView()->SetInitialPageScaleOverride(-1);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetLoadWithOverviewMode(true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(true);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  LocalFrameView* frame_view = web_view_helper.LocalMainFrame()->GetFrameView();
  ScrollableArea* layout_viewport = frame_view->LayoutViewport();
  EXPECT_LT(layout_viewport->MaximumScrollOffset().Width(), 0);
}

TEST_F(WebFrameTest, FullscreenLayerSize) {
  FixedLayoutTestWebWidgetClient client;
  RegisterMockedHttpURLLoad("fullscreen_div.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  int viewport_width = 640;
  int viewport_height = 480;
  client.screen_info_.rect = gfx::Rect(viewport_width, viewport_height);
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "fullscreen_div.html", nullptr, nullptr, &client,
      ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  UpdateAllLifecyclePhases(web_view_impl);

  LocalFrame* frame = web_view_impl->MainFrameImpl()->GetFrame();
  Document* document = frame->GetDocument();
  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  Element* div_fullscreen = document->getElementById("div1");
  Fullscreen::RequestFullscreen(*div_fullscreen);
  EXPECT_EQ(nullptr, Fullscreen::FullscreenElementFrom(*document));
  web_view_impl->DidEnterFullscreen();
  EXPECT_EQ(div_fullscreen, Fullscreen::FullscreenElementFrom(*document));
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_EQ(div_fullscreen, Fullscreen::FullscreenElementFrom(*document));

  // Verify that the element is sized to the viewport.
  LayoutBox* fullscreen_layout_object =
      ToLayoutBox(div_fullscreen->GetLayoutObject());
  EXPECT_EQ(viewport_width, fullscreen_layout_object->LogicalWidth().ToInt());
  EXPECT_EQ(viewport_height, fullscreen_layout_object->LogicalHeight().ToInt());

  // Verify it's updated after a device rotation.
  UpdateScreenInfoAndResizeView(&client, &web_view_helper, viewport_height,
                                viewport_width);
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_EQ(viewport_height, fullscreen_layout_object->LogicalWidth().ToInt());
  EXPECT_EQ(viewport_width, fullscreen_layout_object->LogicalHeight().ToInt());
}

TEST_F(WebFrameTest, FullscreenLayerNonScrollable) {
  FixedLayoutTestWebWidgetClient client;
  RegisterMockedHttpURLLoad("fullscreen_div.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  int viewport_width = 640;
  int viewport_height = 480;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "fullscreen_div.html", nullptr, nullptr, &client,
      ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  UpdateAllLifecyclePhases(web_view_impl);

  LocalFrame* frame = web_view_impl->MainFrameImpl()->GetFrame();
  Document* document = frame->GetDocument();
  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  Element* div_fullscreen = document->getElementById("div1");
  Fullscreen::RequestFullscreen(*div_fullscreen);
  EXPECT_EQ(nullptr, Fullscreen::FullscreenElementFrom(*document));
  web_view_impl->DidEnterFullscreen();
  EXPECT_EQ(div_fullscreen, Fullscreen::FullscreenElementFrom(*document));
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_EQ(div_fullscreen, Fullscreen::FullscreenElementFrom(*document));

  // Verify that the viewports are nonscrollable.
  LocalFrameView* frame_view = web_view_helper.LocalMainFrame()->GetFrameView();
  cc::Layer* layout_viewport_scroll_layer =
      frame_view->GetScrollableArea()->LayerForScrolling();
  cc::Layer* visual_viewport_scroll_layer =
      frame_view->GetPage()->GetVisualViewport().LayerForScrolling();

  auto* layout_viewport_scroll_node =
      GetScrollNode(layout_viewport_scroll_layer);
  ASSERT_FALSE(layout_viewport_scroll_node->user_scrollable_horizontal);
  ASSERT_FALSE(layout_viewport_scroll_node->user_scrollable_vertical);
  auto* visual_viewport_scroll_node =
      GetScrollNode(visual_viewport_scroll_layer);
  ASSERT_FALSE(visual_viewport_scroll_node->user_scrollable_horizontal);
  ASSERT_FALSE(visual_viewport_scroll_node->user_scrollable_vertical);

  // Verify that the viewports are scrollable upon exiting fullscreen.
  EXPECT_EQ(div_fullscreen, Fullscreen::FullscreenElementFrom(*document));
  web_view_impl->DidExitFullscreen();
  EXPECT_EQ(nullptr, Fullscreen::FullscreenElementFrom(*document));
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_EQ(nullptr, Fullscreen::FullscreenElementFrom(*document));
  layout_viewport_scroll_layer =
      frame_view->GetScrollableArea()->LayerForScrolling();
  visual_viewport_scroll_layer =
      frame_view->GetPage()->GetVisualViewport().LayerForScrolling();
  layout_viewport_scroll_node = GetScrollNode(layout_viewport_scroll_layer);
  ASSERT_TRUE(layout_viewport_scroll_node->user_scrollable_horizontal);
  ASSERT_TRUE(layout_viewport_scroll_node->user_scrollable_vertical);
  visual_viewport_scroll_node = GetScrollNode(visual_viewport_scroll_layer);
  ASSERT_TRUE(visual_viewport_scroll_node->user_scrollable_horizontal);
  ASSERT_TRUE(visual_viewport_scroll_node->user_scrollable_vertical);
}

TEST_F(WebFrameTest, FullscreenMainFrame) {
  FixedLayoutTestWebWidgetClient client;
  RegisterMockedHttpURLLoad("fullscreen_div.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  int viewport_width = 640;
  int viewport_height = 480;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "fullscreen_div.html", nullptr, nullptr, &client,
      ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  UpdateAllLifecyclePhases(web_view_impl);

  cc::Layer* cc_scroll_layer = web_view_impl->MainFrameImpl()
                                   ->GetFrame()
                                   ->View()
                                   ->LayoutViewport()
                                   ->LayerForScrolling();
  auto* scroll_node = GetScrollNode(cc_scroll_layer);
  ASSERT_TRUE(scroll_node->scrollable);
  ASSERT_TRUE(scroll_node->user_scrollable_horizontal);
  ASSERT_TRUE(scroll_node->user_scrollable_vertical);

  LocalFrame* frame = web_view_impl->MainFrameImpl()->GetFrame();
  Document* document = frame->GetDocument();
  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*document->documentElement());
  EXPECT_EQ(nullptr, Fullscreen::FullscreenElementFrom(*document));
  web_view_impl->DidEnterFullscreen();
  EXPECT_EQ(document->documentElement(),
            Fullscreen::FullscreenElementFrom(*document));

  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_EQ(document->documentElement(),
            Fullscreen::FullscreenElementFrom(*document));

  // Verify that the main frame is still scrollable.
  cc_scroll_layer = web_view_impl->MainFrameImpl()
                        ->GetFrame()
                        ->View()
                        ->LayoutViewport()
                        ->LayerForScrolling();
  scroll_node = GetScrollNode(cc_scroll_layer);
  ASSERT_TRUE(scroll_node->scrollable);
  ASSERT_TRUE(scroll_node->user_scrollable_horizontal);
  ASSERT_TRUE(scroll_node->user_scrollable_vertical);

  // Verify the main frame still behaves correctly after a resize.
  web_view_helper.Resize(gfx::Size(viewport_height, viewport_width));
  scroll_node = GetScrollNode(cc_scroll_layer);
  ASSERT_TRUE(scroll_node->scrollable);
  ASSERT_TRUE(scroll_node->user_scrollable_horizontal);
  ASSERT_TRUE(scroll_node->user_scrollable_vertical);
}

TEST_F(WebFrameTest, FullscreenSubframe) {
  FixedLayoutTestWebWidgetClient client;
  RegisterMockedHttpURLLoad("fullscreen_iframe.html");
  RegisterMockedHttpURLLoad("fullscreen_div.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "fullscreen_iframe.html", nullptr, nullptr, &client,
      ConfigureAndroid);
  int viewport_width = 640;
  int viewport_height = 480;
  UpdateScreenInfoAndResizeView(&client, &web_view_helper, viewport_width,
                                viewport_height);
  UpdateAllLifecyclePhases(web_view_impl);

  LocalFrame* frame =
      To<WebLocalFrameImpl>(
          web_view_helper.GetWebView()->MainFrame()->FirstChild())
          ->GetFrame();
  Document* document = frame->GetDocument();
  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  Element* div_fullscreen = document->getElementById("div1");
  Fullscreen::RequestFullscreen(*div_fullscreen);
  web_view_impl->DidEnterFullscreen();
  UpdateAllLifecyclePhases(web_view_impl);

  // Verify that the element is sized to the viewport.
  LayoutBox* fullscreen_layout_object =
      ToLayoutBox(div_fullscreen->GetLayoutObject());
  EXPECT_EQ(viewport_width, fullscreen_layout_object->LogicalWidth().ToInt());
  EXPECT_EQ(viewport_height, fullscreen_layout_object->LogicalHeight().ToInt());

  // Verify it's updated after a device rotation.
  UpdateScreenInfoAndResizeView(&client, &web_view_helper, viewport_height,
                                viewport_width);
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_EQ(viewport_height, fullscreen_layout_object->LogicalWidth().ToInt());
  EXPECT_EQ(viewport_width, fullscreen_layout_object->LogicalHeight().ToInt());
}

// Tests entering nested fullscreen and then exiting via the same code path
// that's used when the browser process exits fullscreen.
TEST_F(WebFrameTest, FullscreenNestedExit) {
  RegisterMockedHttpURLLoad("fullscreen_iframe.html");
  RegisterMockedHttpURLLoad("fullscreen_div.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl =
      web_view_helper.InitializeAndLoad(base_url_ + "fullscreen_iframe.html");

  UpdateAllLifecyclePhases(web_view_impl);

  Document* top_doc = web_view_impl->MainFrameImpl()->GetFrame()->GetDocument();
  Element* top_body = top_doc->body();

  auto* iframe = To<HTMLIFrameElement>(top_doc->QuerySelector("iframe"));
  Document* iframe_doc = iframe->contentDocument();
  Element* iframe_body = iframe_doc->body();

  LocalFrame::NotifyUserActivation(
      top_doc->GetFrame(), mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*top_body);

  web_view_impl->DidEnterFullscreen();
  UpdateAllLifecyclePhases(web_view_impl);

  LocalFrame::NotifyUserActivation(
      iframe_doc->GetFrame(), mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*iframe_body);

  web_view_impl->DidEnterFullscreen();
  Microtask::PerformCheckpoint(V8PerIsolateData::MainThreadIsolate());
  UpdateAllLifecyclePhases(web_view_impl);

  // We are now in nested fullscreen, with both documents having a non-empty
  // fullscreen element stack.
  EXPECT_EQ(iframe, Fullscreen::FullscreenElementFrom(*top_doc));
  EXPECT_EQ(iframe_body, Fullscreen::FullscreenElementFrom(*iframe_doc));

  web_view_impl->DidExitFullscreen();
  UpdateAllLifecyclePhases(web_view_impl);

  // We should now have fully exited fullscreen.
  EXPECT_EQ(nullptr, Fullscreen::FullscreenElementFrom(*top_doc));
  EXPECT_EQ(nullptr, Fullscreen::FullscreenElementFrom(*iframe_doc));
}

TEST_F(WebFrameTest, FullscreenWithTinyViewport) {
  FixedLayoutTestWebWidgetClient client;
  RegisterMockedHttpURLLoad("viewport-tiny.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-tiny.html", nullptr, nullptr, &client,
      ConfigureAndroid);
  int viewport_width = 384;
  int viewport_height = 640;
  UpdateScreenInfoAndResizeView(&client, &web_view_helper, viewport_width,
                                viewport_height);
  UpdateAllLifecyclePhases(web_view_impl);

  auto* layout_view = web_view_helper.GetWebView()
                          ->MainFrameImpl()
                          ->GetFrameView()
                          ->GetLayoutView();
  EXPECT_EQ(320, layout_view->LogicalWidth().Floor());
  EXPECT_EQ(533, layout_view->LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(1.2, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(1.2, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(5.0, web_view_impl->MaximumPageScaleFactor());

  LocalFrame* frame = web_view_impl->MainFrameImpl()->GetFrame();
  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*frame->GetDocument()->documentElement());
  web_view_impl->DidEnterFullscreen();
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_EQ(384, layout_view->LogicalWidth().Floor());
  EXPECT_EQ(640, layout_view->LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MaximumPageScaleFactor());

  web_view_impl->DidExitFullscreen();
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_EQ(320, layout_view->LogicalWidth().Floor());
  EXPECT_EQ(533, layout_view->LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(1.2, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(1.2, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(5.0, web_view_impl->MaximumPageScaleFactor());
}

TEST_F(WebFrameTest, FullscreenResizeWithTinyViewport) {
  FixedLayoutTestWebWidgetClient client;
  RegisterMockedHttpURLLoad("viewport-tiny.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-tiny.html", nullptr, nullptr, &client,
      ConfigureAndroid);
  int viewport_width = 384;
  int viewport_height = 640;
  UpdateScreenInfoAndResizeView(&client, &web_view_helper, viewport_width,
                                viewport_height);
  UpdateAllLifecyclePhases(web_view_impl);

  auto* layout_view = web_view_helper.GetWebView()
                          ->MainFrameImpl()
                          ->GetFrameView()
                          ->GetLayoutView();
  LocalFrame* frame = web_view_impl->MainFrameImpl()->GetFrame();
  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*frame->GetDocument()->documentElement());
  web_view_impl->DidEnterFullscreen();
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_EQ(384, layout_view->LogicalWidth().Floor());
  EXPECT_EQ(640, layout_view->LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MaximumPageScaleFactor());

  viewport_width = 640;
  viewport_height = 384;
  UpdateScreenInfoAndResizeView(&client, &web_view_helper, viewport_width,
                                viewport_height);
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_EQ(640, layout_view->LogicalWidth().Floor());
  EXPECT_EQ(384, layout_view->LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MaximumPageScaleFactor());

  web_view_impl->DidExitFullscreen();
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_EQ(320, layout_view->LogicalWidth().Floor());
  EXPECT_EQ(192, layout_view->LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(2, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(2, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(5.0, web_view_impl->MaximumPageScaleFactor());
}

TEST_F(WebFrameTest, FullscreenRestoreScaleFactorUponExiting) {
  // The purpose of this test is to more precisely simulate the sequence of
  // resize and switching fullscreen state operations on WebView, with the
  // interference from Android status bars like a real device does.
  // This verifies we handle the transition and restore states correctly.
  WebSize screen_size_minus_status_bars_minus_url_bar(598, 303);
  WebSize screen_size_minus_status_bars(598, 359);
  WebSize screen_size(640, 384);

  FixedLayoutTestWebWidgetClient client;
  RegisterMockedHttpURLLoad("fullscreen_restore_scale_factor.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "fullscreen_restore_scale_factor.html", nullptr, nullptr,
      &client, &ConfigureAndroid);
  UpdateScreenInfoAndResizeView(
      &client, &web_view_helper,
      screen_size_minus_status_bars_minus_url_bar.width,
      screen_size_minus_status_bars_minus_url_bar.height);
  auto* layout_view = web_view_helper.GetWebView()
                          ->MainFrameImpl()
                          ->GetFrameView()
                          ->GetLayoutView();
  EXPECT_EQ(screen_size_minus_status_bars_minus_url_bar.width,
            layout_view->LogicalWidth().Floor());
  EXPECT_EQ(screen_size_minus_status_bars_minus_url_bar.height,
            layout_view->LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(5.0, web_view_impl->MaximumPageScaleFactor());

  {
    LocalFrame* frame = web_view_impl->MainFrameImpl()->GetFrame();
    LocalFrame::NotifyUserActivation(
        frame, mojom::UserActivationNotificationType::kTest);
    Fullscreen::RequestFullscreen(*frame->GetDocument()->body());
  }

  web_view_impl->DidEnterFullscreen();
  UpdateAllLifecyclePhases(web_view_impl);
  UpdateScreenInfoAndResizeView(&client, &web_view_helper,
                                screen_size_minus_status_bars.width,
                                screen_size_minus_status_bars.height);
  UpdateScreenInfoAndResizeView(&client, &web_view_helper, screen_size.width,
                                screen_size.height);
  EXPECT_EQ(screen_size.width, layout_view->LogicalWidth().Floor());
  EXPECT_EQ(screen_size.height, layout_view->LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MaximumPageScaleFactor());

  web_view_impl->DidExitFullscreen();
  UpdateAllLifecyclePhases(web_view_impl);
  UpdateScreenInfoAndResizeView(&client, &web_view_helper,
                                screen_size_minus_status_bars.width,
                                screen_size_minus_status_bars.height);
  UpdateScreenInfoAndResizeView(
      &client, &web_view_helper,
      screen_size_minus_status_bars_minus_url_bar.width,
      screen_size_minus_status_bars_minus_url_bar.height);
  EXPECT_EQ(screen_size_minus_status_bars_minus_url_bar.width,
            layout_view->LogicalWidth().Floor());
  EXPECT_EQ(screen_size_minus_status_bars_minus_url_bar.height,
            layout_view->LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(5.0, web_view_impl->MaximumPageScaleFactor());
}

// Tests that leaving fullscreen by navigating to a new page resets the
// fullscreen page scale constraints.
TEST_F(WebFrameTest, ClearFullscreenConstraintsOnNavigation) {
  RegisterMockedHttpURLLoad("viewport-tiny.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  int viewport_width = 100;
  int viewport_height = 200;

  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-tiny.html", nullptr, nullptr, nullptr,
      ConfigureAndroid);

  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  UpdateAllLifecyclePhases(web_view_impl);

  // viewport-tiny.html specifies a 320px layout width.
  auto* layout_view =
      web_view_impl->MainFrameImpl()->GetFrameView()->GetLayoutView();
  EXPECT_EQ(320, layout_view->LogicalWidth().Floor());
  EXPECT_EQ(640, layout_view->LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(0.3125, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(0.3125, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(5.0, web_view_impl->MaximumPageScaleFactor());

  LocalFrame* frame = web_view_impl->MainFrameImpl()->GetFrame();
  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*frame->GetDocument()->documentElement());
  web_view_impl->DidEnterFullscreen();
  UpdateAllLifecyclePhases(web_view_impl);

  // Entering fullscreen causes layout size and page scale limits to be
  // overridden.
  EXPECT_EQ(100, layout_view->LogicalWidth().Floor());
  EXPECT_EQ(200, layout_view->LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MaximumPageScaleFactor());

  const char kSource[] = "<meta name=\"viewport\" content=\"width=200\">";

  // Load a new page before exiting fullscreen.
  KURL test_url = ToKURL("about:blank");
  WebLocalFrame* web_frame = web_view_helper.LocalMainFrame();
  frame_test_helpers::LoadHTMLString(web_frame, kSource, test_url);
  web_view_impl->DidExitFullscreen();
  UpdateAllLifecyclePhases(web_view_impl);

  // Make sure the new page's layout size and scale factor limits aren't
  // overridden.
  layout_view = web_view_impl->MainFrameImpl()->GetFrameView()->GetLayoutView();
  EXPECT_EQ(200, layout_view->LogicalWidth().Floor());
  EXPECT_EQ(400, layout_view->LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(0.5, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(5.0, web_view_impl->MaximumPageScaleFactor());
}

TEST_F(WebFrameTest, OverlayFullscreenVideo) {
  ScopedForceOverlayFullscreenVideoForTest force_overlay_fullscreen_video(true);
  RegisterMockedHttpURLLoad("fullscreen_video.html");
  frame_test_helpers::TestWebWidgetClient web_widget_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl =
      web_view_helper.InitializeAndLoad(base_url_ + "fullscreen_video.html",
                                        nullptr, nullptr, &web_widget_client);

  // Ensure that the local frame view has a paint artifact compositor. It's
  // created lazily, and doing so after entering fullscreen would undo the
  // overlay video layer modification.
  UpdateAllLifecyclePhases(web_view_impl);

  const cc::LayerTreeHost* layer_tree_host =
      web_widget_client.layer_tree_host();

  LocalFrame* frame = web_view_impl->MainFrameImpl()->GetFrame();
  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  auto* video =
      To<HTMLVideoElement>(frame->GetDocument()->getElementById("video"));
  EXPECT_TRUE(video->UsesOverlayFullscreenVideo());
  EXPECT_FALSE(video->IsFullscreen());
  EXPECT_EQ(SkColorGetA(layer_tree_host->background_color()), SK_AlphaOPAQUE);

  const cc::Layer* root_layer = layer_tree_host->root_layer();
  EXPECT_EQ(1u, CcLayersByName(root_layer, "Scrolling Contents Layer").size());
  EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "other").size());
  // The video is not composited when it's not in full screen.
  EXPECT_EQ(0u, CcLayersByDOMElementId(root_layer, "video").size());

  video->webkitEnterFullscreen();
  web_view_impl->DidEnterFullscreen();
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_TRUE(video->IsFullscreen());
  EXPECT_EQ(SkColorGetA(layer_tree_host->background_color()),
            SK_AlphaTRANSPARENT);

  root_layer = layer_tree_host->root_layer();
  EXPECT_EQ(0u, CcLayersByName(root_layer, "Scrolling Contents Layer").size());
  EXPECT_EQ(0u, CcLayersByDOMElementId(root_layer, "other").size());
  EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "video").size());

  web_view_impl->DidExitFullscreen();
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_FALSE(video->IsFullscreen());
  EXPECT_EQ(SkColorGetA(layer_tree_host->background_color()), SK_AlphaOPAQUE);

  root_layer = layer_tree_host->root_layer();
  EXPECT_EQ(1u, CcLayersByName(root_layer, "Scrolling Contents Layer").size());
  EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "other").size());
  // The video is not composited when it's not in full screen.
  EXPECT_EQ(0u, CcLayersByDOMElementId(root_layer, "video").size());
}

TEST_F(WebFrameTest, OverlayFullscreenVideoInIframe) {
  ScopedForceOverlayFullscreenVideoForTest force_overlay_fullscreen_video(true);
  RegisterMockedHttpURLLoad("fullscreen_video_in_iframe.html");
  RegisterMockedHttpURLLoad("fullscreen_video.html");
  frame_test_helpers::TestWebWidgetClient web_widget_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "fullscreen_video_in_iframe.html", nullptr, nullptr,
      &web_widget_client);

  const cc::LayerTreeHost* layer_tree_host =
      web_widget_client.layer_tree_host();
  LocalFrame* iframe =
      To<WebLocalFrameImpl>(
          web_view_helper.GetWebView()->MainFrame()->FirstChild())
          ->GetFrame();
  LocalFrame::NotifyUserActivation(
      iframe, mojom::UserActivationNotificationType::kTest);
  auto* video =
      To<HTMLVideoElement>(iframe->GetDocument()->getElementById("video"));
  EXPECT_TRUE(video->UsesOverlayFullscreenVideo());
  EXPECT_FALSE(video->IsFullscreen());
  EXPECT_EQ(SkColorGetA(layer_tree_host->background_color()), SK_AlphaOPAQUE);

  video->webkitEnterFullscreen();
  web_view_impl->DidEnterFullscreen();
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_TRUE(video->IsFullscreen());
  EXPECT_EQ(SkColorGetA(layer_tree_host->background_color()),
            SK_AlphaTRANSPARENT);

  web_view_impl->DidExitFullscreen();
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_FALSE(video->IsFullscreen());
  EXPECT_EQ(SkColorGetA(layer_tree_host->background_color()), SK_AlphaOPAQUE);
}

TEST_F(WebFrameTest, WebXrImmersiveOverlay) {
  RegisterMockedHttpURLLoad("webxr_overlay.html");
  frame_test_helpers::TestWebWidgetClient web_widget_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "webxr_overlay.html", nullptr, nullptr, &web_widget_client);
  web_view_helper.Resize(gfx::Size(640, 480));

  // Ensure that the local frame view has a paint artifact compositor. It's
  // created lazily, and doing so after entering fullscreen would undo the
  // overlay layer modification.
  UpdateAllLifecyclePhases(web_view_impl);

  const cc::LayerTreeHost* layer_tree_host =
      web_widget_client.layer_tree_host();

  LocalFrame* frame = web_view_impl->MainFrameImpl()->GetFrame();
  Document* document = frame->GetDocument();

  Element* overlay = document->getElementById("overlay");
  EXPECT_FALSE(Fullscreen::IsFullscreenElement(*overlay));
  EXPECT_EQ(SkColorGetA(layer_tree_host->background_color()), SK_AlphaOPAQUE);

  // It's not legal to switch the fullscreen element while in immersive-ar mode,
  // so set the fullscreen element first before activating that. This requires
  // user activation.
  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*overlay);
  EXPECT_FALSE(document->IsXrOverlay());
  document->SetIsXrOverlay(true, overlay);
  EXPECT_TRUE(document->IsXrOverlay());

  const cc::Layer* root_layer = layer_tree_host->root_layer();
  EXPECT_EQ(1u, CcLayersByName(root_layer, "Scrolling Contents Layer").size());
  EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "other").size());
  // The overlay is not composited when it's not in full screen.
  EXPECT_EQ(0u, CcLayersByDOMElementId(root_layer, "overlay").size());
  EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "inner").size());

  web_view_impl->DidEnterFullscreen();
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_TRUE(Fullscreen::IsFullscreenElement(*overlay));
  EXPECT_EQ(SkColorGetA(layer_tree_host->background_color()),
            SK_AlphaTRANSPARENT);

  GraphicsLayer* inner_layer =
      ToLayoutBoxModelObject(
          frame->GetDocument()->getElementById("inner")->GetLayoutObject())
          ->Layer()
          ->GetCompositedLayerMapping()
          ->MainGraphicsLayer();
  EXPECT_TRUE(inner_layer);

  root_layer = layer_tree_host->root_layer();
  EXPECT_EQ(0u, CcLayersByName(root_layer, "Scrolling Contents Layer").size());
  EXPECT_EQ(0u, CcLayersByDOMElementId(root_layer, "other").size());
  EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "overlay").size());
  EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "inner").size());

  web_view_impl->DidExitFullscreen();
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_FALSE(Fullscreen::IsFullscreenElement(*overlay));
  EXPECT_EQ(SkColorGetA(layer_tree_host->background_color()), SK_AlphaOPAQUE);
  document->SetIsXrOverlay(false, overlay);

  root_layer = layer_tree_host->root_layer();
  EXPECT_EQ(1u, CcLayersByName(root_layer, "Scrolling Contents Layer").size());
  EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "other").size());
  // The overlay is not composited when it's not in full screen.
  EXPECT_EQ(0u, CcLayersByDOMElementId(root_layer, "overlay").size());
  EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "inner").size());
}

TEST_F(WebFrameTest, LayoutBlockPercentHeightDescendants) {
  RegisterMockedHttpURLLoad("percent-height-descendants.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ +
                                    "percent-height-descendants.html");

  WebViewImpl* web_view = web_view_helper.GetWebView();
  web_view_helper.Resize(gfx::Size(800, 800));
  UpdateAllLifecyclePhases(web_view);

  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();
  LayoutBlock* container =
      To<LayoutBlock>(document->getElementById("container")->GetLayoutObject());
  LayoutBox* percent_height_in_anonymous =
      ToLayoutBox(document->getElementById("percent-height-in-anonymous")
                      ->GetLayoutObject());
  LayoutBox* percent_height_direct_child =
      ToLayoutBox(document->getElementById("percent-height-direct-child")
                      ->GetLayoutObject());

  EXPECT_TRUE(
      container->HasPercentHeightDescendant(percent_height_in_anonymous));
  EXPECT_TRUE(
      container->HasPercentHeightDescendant(percent_height_direct_child));

  ASSERT_TRUE(container->PercentHeightDescendants());
  ASSERT_TRUE(container->HasPercentHeightDescendants());
  EXPECT_EQ(2U, container->PercentHeightDescendants()->size());
  EXPECT_TRUE(container->PercentHeightDescendants()->Contains(
      percent_height_in_anonymous));
  EXPECT_TRUE(container->PercentHeightDescendants()->Contains(
      percent_height_direct_child));

  LayoutBlock* anonymous_block = percent_height_in_anonymous->ContainingBlock();
  EXPECT_TRUE(anonymous_block->IsAnonymous());
  EXPECT_FALSE(anonymous_block->HasPercentHeightDescendants());
}

TEST_F(WebFrameTest, HasVisibleContentOnVisibleFrames) {
  RegisterMockedHttpURLLoad("visible_frames.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl =
      web_view_helper.InitializeAndLoad(base_url_ + "visible_frames.html");
  for (WebFrame* frame = web_view_impl->MainFrameImpl()->TraverseNext(); frame;
       frame = frame->TraverseNext()) {
    EXPECT_TRUE(frame->ToWebLocalFrame()->HasVisibleContent());
  }
}

TEST_F(WebFrameTest, HasVisibleContentOnHiddenFrames) {
  RegisterMockedHttpURLLoad("hidden_frames.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl =
      web_view_helper.InitializeAndLoad(base_url_ + "hidden_frames.html");
  for (WebFrame* frame = web_view_impl->MainFrameImpl()->TraverseNext(); frame;
       frame = frame->TraverseNext()) {
    EXPECT_FALSE(frame->ToWebLocalFrame()->HasVisibleContent());
  }
}

static Resource* FetchManifest(Document* document, const KURL& url) {
  FetchParameters fetch_parameters =
      FetchParameters::CreateForTest(ResourceRequest(url));
  fetch_parameters.SetRequestContext(
      mojom::blink::RequestContextType::MANIFEST);

  return RawResource::FetchSynchronously(fetch_parameters, document->Fetcher());
}

TEST_F(WebFrameTest, ManifestFetch) {
  RegisterMockedHttpURLLoad("foo.html");
  RegisterMockedHttpURLLoad("link-manifest-fetch.json");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");
  Document* document =
      web_view_helper.LocalMainFrame()->GetFrame()->GetDocument();

  Resource* resource =
      FetchManifest(document, ToKURL(base_url_ + "link-manifest-fetch.json"));

  EXPECT_TRUE(resource->IsLoaded());
}

TEST_F(WebFrameTest, ManifestCSPFetchAllow) {
  // TODO(crbug.com/751425): We should use the mock functionality
  // via the WebViewHelper instance in each test case.
  RegisterMockedURLLoadFromBase(not_base_url_, "link-manifest-fetch.json");
  RegisterMockedHttpURLLoadWithCSP("foo.html", "manifest-src *");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");
  Document* document =
      web_view_helper.LocalMainFrame()->GetFrame()->GetDocument();

  Resource* resource = FetchManifest(
      document, ToKURL(not_base_url_ + "link-manifest-fetch.json"));

  EXPECT_TRUE(resource->IsLoaded());
}

TEST_F(WebFrameTest, ManifestCSPFetchSelf) {
  // TODO(crbug.com/751425): We should use the mock functionality
  // via the WebViewHelper instance in each test case.
  RegisterMockedURLLoadFromBase(not_base_url_, "link-manifest-fetch.json");
  RegisterMockedHttpURLLoadWithCSP("foo.html", "manifest-src 'self'");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");
  Document* document =
      web_view_helper.LocalMainFrame()->GetFrame()->GetDocument();

  Resource* resource = FetchManifest(
      document, ToKURL(not_base_url_ + "link-manifest-fetch.json"));

  // Fetching resource wasn't allowed.
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->ErrorOccurred());
  EXPECT_TRUE(resource->GetResourceError().IsAccessCheck());
}

TEST_F(WebFrameTest, ManifestCSPFetchSelfReportOnly) {
  // TODO(crbug.com/751425): We should use the mock functionality
  // via the WebViewHelper instance in each test case.
  RegisterMockedURLLoadFromBase(not_base_url_, "link-manifest-fetch.json");
  RegisterMockedHttpURLLoadWithCSP("foo.html", "manifest-src 'self'",
                                   /* report only */ true);

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");
  Document* document =
      web_view_helper.LocalMainFrame()->GetFrame()->GetDocument();

  Resource* resource = FetchManifest(
      document, ToKURL(not_base_url_ + "link-manifest-fetch.json"));

  EXPECT_TRUE(resource->IsLoaded());
}

TEST_F(WebFrameTest, ReloadBypassingCache) {
  // Check that a reload bypassing cache on a frame will result in the cache
  // policy of the request being set to ReloadBypassingCache.
  RegisterMockedHttpURLLoad("foo.html");
  TestBeginNavigationCacheModeClient client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html", &client);
  WebLocalFrame* frame = web_view_helper.LocalMainFrame();
  frame_test_helpers::ReloadFrameBypassingCache(frame);
  EXPECT_EQ(mojom::FetchCacheMode::kBypassCache, client.GetCacheMode());
}

static void NodeImageTestValidation(const IntSize& reference_bitmap_size,
                                    DragImage* drag_image) {
  // Prepare the reference bitmap.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(reference_bitmap_size.Width(),
                        reference_bitmap_size.Height());
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(SK_ColorGREEN);

  EXPECT_EQ(reference_bitmap_size.Width(), drag_image->Size().Width());
  EXPECT_EQ(reference_bitmap_size.Height(), drag_image->Size().Height());
  const SkBitmap& drag_bitmap = drag_image->Bitmap();
  EXPECT_EQ(0, memcmp(bitmap.getPixels(), drag_bitmap.getPixels(),
                      bitmap.computeByteSize()));
}

TEST_F(WebFrameTest, NodeImageTestCSSTransformDescendant) {
  frame_test_helpers::WebViewHelper web_view_helper;
  std::unique_ptr<DragImage> drag_image = NodeImageTestSetup(
      &web_view_helper, std::string("case-css-3dtransform-descendant"));
  EXPECT_TRUE(drag_image);

  NodeImageTestValidation(IntSize(40, 40), drag_image.get());
}

TEST_F(WebFrameTest, NodeImageTestCSSTransform) {
  frame_test_helpers::WebViewHelper web_view_helper;
  std::unique_ptr<DragImage> drag_image =
      NodeImageTestSetup(&web_view_helper, std::string("case-css-transform"));
  EXPECT_TRUE(drag_image);

  NodeImageTestValidation(IntSize(40, 40), drag_image.get());
}

TEST_F(WebFrameTest, NodeImageTestCSS3DTransform) {
  frame_test_helpers::WebViewHelper web_view_helper;
  std::unique_ptr<DragImage> drag_image =
      NodeImageTestSetup(&web_view_helper, std::string("case-css-3dtransform"));
  EXPECT_TRUE(drag_image);

  NodeImageTestValidation(IntSize(40, 40), drag_image.get());
}

TEST_F(WebFrameTest, NodeImageTestInlineBlock) {
  frame_test_helpers::WebViewHelper web_view_helper;
  std::unique_ptr<DragImage> drag_image =
      NodeImageTestSetup(&web_view_helper, std::string("case-inlineblock"));
  EXPECT_TRUE(drag_image);

  NodeImageTestValidation(IntSize(40, 40), drag_image.get());
}

TEST_F(WebFrameTest, NodeImageTestFloatLeft) {
  frame_test_helpers::WebViewHelper web_view_helper;
  std::unique_ptr<DragImage> drag_image = NodeImageTestSetup(
      &web_view_helper, std::string("case-float-left-overflow-hidden"));
  EXPECT_TRUE(drag_image);

  NodeImageTestValidation(IntSize(40, 40), drag_image.get());
}

// Crashes on Android: http://crbug.com/403804
#if defined(OS_ANDROID)
TEST_F(WebFrameTest, DISABLED_PrintingBasic)
#else
TEST_F(WebFrameTest, PrintingBasic)
#endif
{
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("data:text/html,Hello, world.");

  WebLocalFrame* frame = web_view_helper.LocalMainFrame();

  WebPrintParams print_params;
  print_params.print_content_area.width = 500;
  print_params.print_content_area.height = 500;

  uint32_t page_count = frame->PrintBegin(print_params, WebNode());
  EXPECT_EQ(1u, page_count);
  frame->PrintEnd();
}

class ThemeColorTestLocalFrameHost : public FakeLocalFrameHost {
 public:
  ThemeColorTestLocalFrameHost() = default;
  ~ThemeColorTestLocalFrameHost() override = default;

  void Reset() { did_notify_ = false; }

  bool DidNotify() const { return did_notify_; }

 private:
  // FakeLocalFrameHost:
  void DidChangeThemeColor(
      const base::Optional<::SkColor>& theme_color) override {
    did_notify_ = true;
  }

  bool did_notify_ = false;
};

TEST_F(WebFrameTest, ThemeColor) {
  RegisterMockedHttpURLLoad("theme_color_test.html");
  ThemeColorTestLocalFrameHost host;
  frame_test_helpers::TestWebFrameClient client;
  host.Init(client.GetRemoteNavigationAssociatedInterfaces());
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "theme_color_test.html",
                                    &client);
  EXPECT_TRUE(host.DidNotify());
  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  EXPECT_EQ(Color(0, 0, 255), frame->GetDocument().ThemeColor());
  // Change color by rgb.
  host.Reset();
  frame->ExecuteScript(
      WebScriptSource("document.getElementById('tc1').setAttribute('content', "
                      "'rgb(0, 0, 0)');"));
  RunPendingTasks();
  EXPECT_TRUE(host.DidNotify());
  EXPECT_EQ(Color::kBlack, frame->GetDocument().ThemeColor());
  // Change color by hsl.
  host.Reset();
  frame->ExecuteScript(
      WebScriptSource("document.getElementById('tc1').setAttribute('content', "
                      "'hsl(240,100%, 50%)');"));
  RunPendingTasks();
  EXPECT_TRUE(host.DidNotify());
  EXPECT_EQ(Color(0, 0, 255), frame->GetDocument().ThemeColor());
  // Change of second theme-color meta tag will not change frame's theme
  // color.
  host.Reset();
  frame->ExecuteScript(WebScriptSource(
      "document.getElementById('tc2').setAttribute('content', '#00FF00');"));
  RunPendingTasks();
  EXPECT_TRUE(host.DidNotify());
  EXPECT_EQ(Color(0, 0, 255), frame->GetDocument().ThemeColor());
  // Remove the first theme-color meta tag to apply the second.
  host.Reset();
  frame->ExecuteScript(
      WebScriptSource("document.getElementById('tc1').remove();"));
  RunPendingTasks();
  EXPECT_TRUE(host.DidNotify());
  EXPECT_EQ(Color(0, 255, 0), frame->GetDocument().ThemeColor());
  // Remove the name attribute of the remaining meta.
  host.Reset();
  frame->ExecuteScript(WebScriptSource(
      "document.getElementById('tc2').removeAttribute('name');"));
  RunPendingTasks();
  EXPECT_TRUE(host.DidNotify());
  EXPECT_EQ(base::nullopt, frame->GetDocument().ThemeColor());
}

// Make sure that an embedder-triggered detach with a remote frame parent
// doesn't leave behind dangling pointers.
TEST_F(WebFrameTest, EmbedderTriggeredDetachWithRemoteMainFrame) {
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote();
  WebLocalFrame* child_frame =
      frame_test_helpers::CreateLocalChild(*helper.RemoteMainFrame());

  // Purposely keep the LocalFrame alive so it's the last thing to be destroyed.
  Persistent<Frame> child_core_frame = WebFrame::ToCoreFrame(*child_frame);
  helper.Reset();
  child_core_frame.Clear();
}

class WebFrameSwapTestClient : public frame_test_helpers::TestWebFrameClient {
 public:
  explicit WebFrameSwapTestClient(WebFrameSwapTestClient* parent = nullptr) {
    local_frame_host_ =
        std::make_unique<TestLocalFrameHostForFrameOwnerPropertiesChanges>(
            parent);
    local_frame_host_->Init(GetRemoteNavigationAssociatedInterfaces());
  }

  WebLocalFrame* CreateChildFrame(
      WebLocalFrame* parent,
      mojom::blink::TreeScopeType scope,
      const WebString& name,
      const WebString& fallback_name,
      const FramePolicy&,
      const WebFrameOwnerProperties&,
      mojom::blink::FrameOwnerElementType) override {
    return CreateLocalChild(*parent, scope,
                            std::make_unique<WebFrameSwapTestClient>(this));
  }

  void DidChangeFrameOwnerProperties(
      mojom::blink::FrameOwnerPropertiesPtr properties) {
    did_propagate_display_none_ |= properties->is_display_none;
  }

  bool DidPropagateDisplayNoneProperty() const {
    return did_propagate_display_none_;
  }

 private:
  class TestLocalFrameHostForFrameOwnerPropertiesChanges
      : public FakeLocalFrameHost {
   public:
    explicit TestLocalFrameHostForFrameOwnerPropertiesChanges(
        WebFrameSwapTestClient* parent)
        : parent_(parent) {}
    ~TestLocalFrameHostForFrameOwnerPropertiesChanges() override = default;

    // FakeLocalFrameHost:
    void DidChangeFrameOwnerProperties(
        const base::UnguessableToken& child_frame_token,
        mojom::blink::FrameOwnerPropertiesPtr properties) override {
      if (parent_)
        parent_->DidChangeFrameOwnerProperties(std::move(properties));
    }

    bool did_propagate_display_none_ = false;
    WebFrameSwapTestClient* parent_ = nullptr;
  };

  std::unique_ptr<TestLocalFrameHostForFrameOwnerPropertiesChanges>
      local_frame_host_;
  bool did_propagate_display_none_ = false;
};

class WebFrameSwapTest : public WebFrameTest {
 protected:
  WebFrameSwapTest() {
    RegisterMockedHttpURLLoad("frame-a-b-c.html");
    RegisterMockedHttpURLLoad("subframe-a.html");
    RegisterMockedHttpURLLoad("subframe-b.html");
    RegisterMockedHttpURLLoad("subframe-c.html");
    RegisterMockedHttpURLLoad("subframe-hello.html");

    web_view_helper_.InitializeAndLoad(base_url_ + "frame-a-b-c.html",
                                       &main_frame_client_);
  }

  void Reset() { web_view_helper_.Reset(); }
  WebLocalFrame* MainFrame() const { return web_view_helper_.LocalMainFrame(); }
  WebViewImpl* WebView() const { return web_view_helper_.GetWebView(); }

 private:
  frame_test_helpers::WebViewHelper web_view_helper_;
  WebFrameSwapTestClient main_frame_client_;
};

TEST_F(WebFrameSwapTest, SwapMainFrame) {
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  MainFrame()->Swap(remote_frame);

  WebLocalFrame* local_frame =
      frame_test_helpers::CreateProvisional(*remote_frame);
  remote_frame->Swap(local_frame);

  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  frame_test_helpers::LoadFrame(local_frame, base_url_ + "subframe-hello.html");

  std::string content =
      WebFrameContentDumper::DumpWebViewAsText(WebView(), 1024).Utf8();
  EXPECT_EQ("hello", content);
}

TEST_F(WebFrameSwapTest, ValidateSizeOnRemoteToLocalMainFrameSwap) {
  gfx::Size size(111, 222);

  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  MainFrame()->Swap(remote_frame);

  remote_frame->View()->Resize(size);

  WebLocalFrame* local_frame =
      frame_test_helpers::CreateProvisional(*remote_frame);
  remote_frame->Swap(local_frame);

  // Verify that the size that was set with a remote main frame is correct
  // after swapping to a local frame.
  Page* page = static_cast<WebViewImpl*>(local_frame->View())
                   ->GetPage()
                   ->MainFrame()
                   ->GetPage();
  EXPECT_EQ(size.width(), page->GetVisualViewport().Size().Width());
  EXPECT_EQ(size.height(), page->GetVisualViewport().Size().Height());
}

// Verify that size changes to browser controls while the main frame is remote
// are preserved when the main frame swaps to a local frame.  See
// https://crbug.com/769321.
TEST_F(WebFrameSwapTest,
       ValidateBrowserControlsSizeOnRemoteToLocalMainFrameSwap) {
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  MainFrame()->Swap(remote_frame);

  // Create a provisional main frame frame but don't swap it in yet.
  WebLocalFrame* local_frame =
      frame_test_helpers::CreateProvisional(*remote_frame);

  WebViewImpl* web_view = static_cast<WebViewImpl*>(local_frame->View());
  EXPECT_TRUE(web_view->MainFrame() &&
              web_view->MainFrame()->IsWebRemoteFrame());

  // Resize the browser controls.
  float top_browser_controls_height = 40;
  float bottom_browser_controls_height = 60;
  web_view->ResizeWithBrowserControls(gfx::Size(100, 100),
                                      top_browser_controls_height,
                                      bottom_browser_controls_height, false);

  // Swap the provisional frame in and verify that the browser controls size is
  // correct.
  remote_frame->Swap(local_frame);
  Page* page = static_cast<WebViewImpl*>(local_frame->View())
                   ->GetPage()
                   ->MainFrame()
                   ->GetPage();
  EXPECT_EQ(top_browser_controls_height,
            page->GetBrowserControls().TopHeight());
  EXPECT_EQ(bottom_browser_controls_height,
            page->GetBrowserControls().BottomHeight());
}

namespace {

class SwapMainFrameWhenTitleChangesWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  SwapMainFrameWhenTitleChangesWebFrameClient() = default;
  ~SwapMainFrameWhenTitleChangesWebFrameClient() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void DidReceiveTitle(const WebString& title) override {
    if (title.IsEmpty())
      return;

    if (!Frame()->Parent())
      Frame()->Swap(frame_test_helpers::CreateRemote());
  }
};

}  // namespace

TEST_F(WebFrameTest, SwapMainFrameWhileLoading) {
  SwapMainFrameWhenTitleChangesWebFrameClient frame_client;

  frame_test_helpers::WebViewHelper web_view_helper;
  RegisterMockedHttpURLLoad("frame-a-b-c.html");
  RegisterMockedHttpURLLoad("subframe-a.html");
  RegisterMockedHttpURLLoad("subframe-b.html");
  RegisterMockedHttpURLLoad("subframe-c.html");
  RegisterMockedHttpURLLoad("subframe-hello.html");

  web_view_helper.InitializeAndLoad(base_url_ + "frame-a-b-c.html",
                                    &frame_client);
}

void WebFrameTest::SwapAndVerifyFirstChildConsistency(const char* const message,
                                                      WebFrame* parent,
                                                      WebFrame* new_child) {
  SCOPED_TRACE(message);
  parent->FirstChild()->Swap(new_child);

  EXPECT_EQ(new_child, parent->FirstChild());
  EXPECT_EQ(new_child->Parent(), parent);
  EXPECT_EQ(new_child,
            parent->LastChild()->PreviousSibling()->PreviousSibling());
  EXPECT_EQ(new_child->NextSibling(), parent->LastChild()->PreviousSibling());
}

TEST_F(WebFrameSwapTest, SwapFirstChild) {
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  SwapAndVerifyFirstChildConsistency("local->remote", MainFrame(),
                                     remote_frame);

  WebLocalFrame* local_frame =
      frame_test_helpers::CreateProvisional(*remote_frame);
  SwapAndVerifyFirstChildConsistency("remote->local", MainFrame(), local_frame);

  // FIXME: This almost certainly fires more load events on the iframe element
  // than it should.
  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  frame_test_helpers::LoadFrame(local_frame, base_url_ + "subframe-hello.html");
  std::string content =
      WebFrameContentDumper::DumpWebViewAsText(WebView(), 1024).Utf8();
  EXPECT_EQ("  \n\nhello\n\nb \n\na\n\nc", content);
}

TEST_F(WebFrameSwapTest, DoNotPropagateDisplayNonePropertyOnSwap) {
  WebFrameSwapTestClient* main_frame_client =
      static_cast<WebFrameSwapTestClient*>(MainFrame()->Client());
  EXPECT_FALSE(main_frame_client->DidPropagateDisplayNoneProperty());

  WebLocalFrame* child_frame = MainFrame()->FirstChild()->ToWebLocalFrame();
  frame_test_helpers::LoadFrame(child_frame, "subframe-hello.html");
  EXPECT_FALSE(main_frame_client->DidPropagateDisplayNoneProperty());

  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  child_frame->Swap(remote_frame);
  EXPECT_FALSE(main_frame_client->DidPropagateDisplayNoneProperty());

  WebLocalFrame* local_frame =
      frame_test_helpers::CreateProvisional(*remote_frame);
  remote_frame->Swap(local_frame);
  EXPECT_FALSE(main_frame_client->DidPropagateDisplayNoneProperty());
  Reset();
}

void WebFrameTest::SwapAndVerifyMiddleChildConsistency(
    const char* const message,
    WebFrame* parent,
    WebFrame* new_child) {
  SCOPED_TRACE(message);
  parent->FirstChild()->NextSibling()->Swap(new_child);

  Frame* parent_frame = WebFrame::ToCoreFrame(*parent);
  Frame* new_child_frame = WebFrame::ToCoreFrame(*new_child);

  EXPECT_EQ(new_child_frame, parent_frame->FirstChild()->NextSibling());
  EXPECT_EQ(new_child_frame, parent_frame->LastChild()->PreviousSibling());
  EXPECT_EQ(new_child_frame->Parent(), parent_frame);
  EXPECT_EQ(new_child_frame, parent_frame->FirstChild()->NextSibling());
  EXPECT_EQ(new_child_frame->PreviousSibling(), parent_frame->FirstChild());
  EXPECT_EQ(new_child_frame, parent_frame->LastChild()->PreviousSibling());
  EXPECT_EQ(new_child_frame->NextSibling(), parent_frame->LastChild());
}

TEST_F(WebFrameSwapTest, SwapMiddleChild) {
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  SwapAndVerifyMiddleChildConsistency("local->remote", MainFrame(),
                                      remote_frame);

  WebLocalFrame* local_frame =
      frame_test_helpers::CreateProvisional(*remote_frame);
  SwapAndVerifyMiddleChildConsistency("remote->local", MainFrame(),
                                      local_frame);

  // FIXME: This almost certainly fires more load events on the iframe element
  // than it should.
  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  frame_test_helpers::LoadFrame(local_frame, base_url_ + "subframe-hello.html");
  std::string content =
      WebFrameContentDumper::DumpWebViewAsText(WebView(), 1024).Utf8();
  EXPECT_EQ("  \n\na\n\nhello\n\nc", content);
}

void WebFrameTest::SwapAndVerifyLastChildConsistency(const char* const message,
                                                     WebFrame* parent,
                                                     WebFrame* new_child) {
  SCOPED_TRACE(message);
  parent->LastChild()->Swap(new_child);

  EXPECT_EQ(new_child, parent->LastChild());
  EXPECT_EQ(new_child->Parent(), parent);
  EXPECT_EQ(new_child, parent->LastChild()->PreviousSibling()->NextSibling());
  EXPECT_EQ(new_child, parent->FirstChild()->NextSibling()->NextSibling());
  EXPECT_EQ(new_child->PreviousSibling(), parent->FirstChild()->NextSibling());
}

TEST_F(WebFrameSwapTest, SwapLastChild) {
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  SwapAndVerifyLastChildConsistency("local->remote", MainFrame(), remote_frame);

  WebLocalFrame* local_frame =
      frame_test_helpers::CreateProvisional(*remote_frame);
  SwapAndVerifyLastChildConsistency("remote->local", MainFrame(), local_frame);

  // FIXME: This almost certainly fires more load events on the iframe element
  // than it should.
  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  frame_test_helpers::LoadFrame(local_frame, base_url_ + "subframe-hello.html");
  std::string content =
      WebFrameContentDumper::DumpWebViewAsText(WebView(), 1024).Utf8();
  EXPECT_EQ("  \n\na\n\nb \n\na\n\nhello", content);
}

TEST_F(WebFrameSwapTest, DetachProvisionalFrame) {
  WebRemoteFrameImpl* remote_frame = frame_test_helpers::CreateRemote();
  SwapAndVerifyMiddleChildConsistency("local->remote", MainFrame(),
                                      remote_frame);

  WebLocalFrameImpl* provisional_frame =
      frame_test_helpers::CreateProvisional(*remote_frame);

  // The provisional frame should have a local frame owner.
  FrameOwner* owner = provisional_frame->GetFrame()->Owner();
  ASSERT_TRUE(owner->IsLocal());

  // But the owner should point to |remoteFrame|, since the new frame is still
  // provisional.
  EXPECT_EQ(remote_frame->GetFrame(), owner->ContentFrame());

  // After detaching the provisional frame, the frame owner should still point
  // at |remoteFrame|.
  provisional_frame->Detach();

  // The owner should not be affected by detaching the provisional frame, so it
  // should still point to |remoteFrame|.
  EXPECT_EQ(remote_frame->GetFrame(), owner->ContentFrame());
}

void WebFrameTest::SwapAndVerifySubframeConsistency(const char* const message,
                                                    WebFrame* old_frame,
                                                    WebFrame* new_frame) {
  SCOPED_TRACE(message);

  EXPECT_TRUE(old_frame->FirstChild());
  old_frame->Swap(new_frame);

  EXPECT_FALSE(new_frame->FirstChild());
  EXPECT_FALSE(new_frame->LastChild());
}

TEST_F(WebFrameSwapTest, EventsOnDisconnectedSubDocumentSkipped) {
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  WebFrame* target_frame = MainFrame()->FirstChild()->NextSibling();
  EXPECT_TRUE(target_frame);
  SwapAndVerifySubframeConsistency("local->remote", target_frame, remote_frame);
  remote_frame->SetReplicatedOrigin(
      WebSecurityOrigin(SecurityOrigin::CreateUniqueOpaque()), false);

  WebLocalFrameImpl* local_child = frame_test_helpers::CreateLocalChild(
      *remote_frame, "local-inside-remote");

  LocalFrame* main_frame = WebView()->MainFrameImpl()->GetFrame();
  Document* child_document = local_child->GetFrame()->GetDocument();
  EventHandlerRegistry& event_registry =
      local_child->GetFrame()->GetEventHandlerRegistry();

  // Add the non-connected, but local, child document as having an event.
  event_registry.DidAddEventHandler(
      *child_document, EventHandlerRegistry::kTouchStartOrMoveEventBlocking);
  // Passes if this does not crash or DCHECK.
  main_frame->View()->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
}

TEST_F(WebFrameSwapTest, EventsOnDisconnectedElementSkipped) {
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  WebFrame* target_frame = MainFrame()->FirstChild()->NextSibling();
  EXPECT_TRUE(target_frame);
  SwapAndVerifySubframeConsistency("local->remote", target_frame, remote_frame);
  remote_frame->SetReplicatedOrigin(
      WebSecurityOrigin(SecurityOrigin::CreateUniqueOpaque()), false);

  WebLocalFrameImpl* local_child = frame_test_helpers::CreateLocalChild(
      *remote_frame, "local-inside-remote");

  LocalFrame* main_frame = WebView()->MainFrameImpl()->GetFrame();

  // Layout ensures that elements in the local_child frame get LayoutObjects
  // attached, but doesn't paint, because the child frame needs to not have
  // been composited for the purpose of this test.
  local_child->GetFrameView()->UpdateLayout();
  Document* child_document = local_child->GetFrame()->GetDocument();
  EventHandlerRegistry& event_registry =
      local_child->GetFrame()->GetEventHandlerRegistry();

  // Add the non-connected body element as having an event.
  event_registry.DidAddEventHandler(
      *child_document->body(),
      EventHandlerRegistry::kTouchStartOrMoveEventBlocking);
  // Passes if this does not crash or DCHECK.
  main_frame->View()->UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
}

TEST_F(WebFrameSwapTest, SwapParentShouldDetachChildren) {
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  WebFrame* target_frame = MainFrame()->FirstChild()->NextSibling();
  EXPECT_TRUE(target_frame);
  SwapAndVerifySubframeConsistency("local->remote", target_frame, remote_frame);

  target_frame = MainFrame()->FirstChild()->NextSibling();
  EXPECT_TRUE(target_frame);

  // Create child frames in the target frame before testing the swap.
  frame_test_helpers::CreateRemoteChild(*remote_frame);

  WebLocalFrame* local_frame =
      frame_test_helpers::CreateProvisional(*remote_frame);
  SwapAndVerifySubframeConsistency("remote->local", target_frame, local_frame);

  // FIXME: This almost certainly fires more load events on the iframe element
  // than it should.
  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  frame_test_helpers::LoadFrame(local_frame, base_url_ + "subframe-hello.html");
  std::string content =
      WebFrameContentDumper::DumpWebViewAsText(WebView(), 1024).Utf8();
  EXPECT_EQ("  \n\na\n\nhello\n\nc", content);
}

TEST_F(WebFrameSwapTest, SwapPreservesGlobalContext) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Value> window_top =
      MainFrame()->ExecuteScriptAndReturnValue(WebScriptSource("window"));
  ASSERT_TRUE(window_top->IsObject());
  v8::Local<v8::Value> original_window =
      MainFrame()->ExecuteScriptAndReturnValue(
          WebScriptSource("document.querySelector('#frame2').contentWindow;"));
  ASSERT_TRUE(original_window->IsObject());

  // Make sure window reference stays the same when swapping to a remote frame.
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  WebFrame* target_frame = MainFrame()->FirstChild()->NextSibling();
  target_frame->Swap(remote_frame);
  v8::Local<v8::Value> remote_window = MainFrame()->ExecuteScriptAndReturnValue(
      WebScriptSource("document.querySelector('#frame2').contentWindow;"));
  EXPECT_TRUE(original_window->StrictEquals(remote_window));
  // Check that its view is consistent with the world.
  v8::Local<v8::Value> remote_window_top =
      MainFrame()->ExecuteScriptAndReturnValue(WebScriptSource(
          "document.querySelector('#frame2').contentWindow.top;"));
  EXPECT_TRUE(window_top->StrictEquals(remote_window_top));

  // Now check that remote -> local works too, since it goes through a different
  // code path.
  WebLocalFrame* local_frame =
      frame_test_helpers::CreateProvisional(*remote_frame);
  remote_frame->Swap(local_frame);
  v8::Local<v8::Value> local_window = MainFrame()->ExecuteScriptAndReturnValue(
      WebScriptSource("document.querySelector('#frame2').contentWindow;"));
  EXPECT_TRUE(original_window->StrictEquals(local_window));
  v8::Local<v8::Value> local_window_top =
      MainFrame()->ExecuteScriptAndReturnValue(WebScriptSource(
          "document.querySelector('#frame2').contentWindow.top;"));
  EXPECT_TRUE(window_top->StrictEquals(local_window_top));
}

TEST_F(WebFrameSwapTest, SetTimeoutAfterSwap) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope scope(isolate);
  MainFrame()->ExecuteScript(
      WebScriptSource("savedSetTimeout = window[0].setTimeout"));

  // Swap the frame to a remote frame.
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  WebFrame* target_frame = MainFrame()->FirstChild();
  target_frame->Swap(remote_frame);
  remote_frame->SetReplicatedOrigin(
      WebSecurityOrigin(SecurityOrigin::CreateUniqueOpaque()), false);

  // Invoking setTimeout should throw a security error.
  {
    v8::Local<v8::Value> exception = MainFrame()->ExecuteScriptAndReturnValue(
        WebScriptSource("try {\n"
                        "  savedSetTimeout.call(window[0], () => {}, 0);\n"
                        "} catch (e) { e; }"));
    ASSERT_TRUE(!exception.IsEmpty());
    EXPECT_EQ(
        "SecurityError: Blocked a frame with origin \"http://internal.test\" "
        "from accessing a cross-origin frame.",
        ToCoreString(exception
                         ->ToString(ToScriptStateForMainWorld(
                                        WebView()->MainFrameImpl()->GetFrame())
                                        ->GetContext())
                         .ToLocalChecked()));
  }
}

TEST_F(WebFrameSwapTest, SwapInitializesGlobal) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  v8::Local<v8::Value> window_top =
      MainFrame()->ExecuteScriptAndReturnValue(WebScriptSource("window"));
  ASSERT_TRUE(window_top->IsObject());

  v8::Local<v8::Value> last_child = MainFrame()->ExecuteScriptAndReturnValue(
      WebScriptSource("saved = window[2]"));
  ASSERT_TRUE(last_child->IsObject());

  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  MainFrame()->LastChild()->Swap(remote_frame);
  v8::Local<v8::Value> remote_window_top =
      MainFrame()->ExecuteScriptAndReturnValue(WebScriptSource("saved.top"));
  EXPECT_TRUE(remote_window_top->IsObject());
  EXPECT_TRUE(window_top->StrictEquals(remote_window_top));

  WebLocalFrame* local_frame =
      frame_test_helpers::CreateProvisional(*remote_frame);
  remote_frame->Swap(local_frame);
  v8::Local<v8::Value> local_window_top =
      MainFrame()->ExecuteScriptAndReturnValue(WebScriptSource("saved.top"));
  EXPECT_TRUE(local_window_top->IsObject());
  EXPECT_TRUE(window_top->StrictEquals(local_window_top));
}

TEST_F(WebFrameSwapTest, RemoteFramesAreIndexable) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  MainFrame()->LastChild()->Swap(remote_frame);
  v8::Local<v8::Value> remote_window =
      MainFrame()->ExecuteScriptAndReturnValue(WebScriptSource("window[2]"));
  EXPECT_TRUE(remote_window->IsObject());
  v8::Local<v8::Value> window_length = MainFrame()->ExecuteScriptAndReturnValue(
      WebScriptSource("window.length"));
  ASSERT_TRUE(window_length->IsInt32());
  EXPECT_EQ(3, window_length.As<v8::Int32>()->Value());
}

TEST_F(WebFrameSwapTest, RemoteFrameLengthAccess) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  MainFrame()->LastChild()->Swap(remote_frame);
  v8::Local<v8::Value> remote_window_length =
      MainFrame()->ExecuteScriptAndReturnValue(
          WebScriptSource("window[2].length"));
  ASSERT_TRUE(remote_window_length->IsInt32());
  EXPECT_EQ(0, remote_window_length.As<v8::Int32>()->Value());
}

TEST_F(WebFrameSwapTest, RemoteWindowNamedAccess) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  // TODO(dcheng): Once OOPIF unit test infrastructure is in place, test that
  // named window access on a remote window works. For now, just test that
  // accessing a named property doesn't crash.
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  MainFrame()->LastChild()->Swap(remote_frame);
  remote_frame->SetReplicatedOrigin(
      WebSecurityOrigin(SecurityOrigin::CreateUniqueOpaque()), false);
  v8::Local<v8::Value> remote_window_property =
      MainFrame()->ExecuteScriptAndReturnValue(
          WebScriptSource("window[2].foo"));
  EXPECT_TRUE(remote_window_property.IsEmpty());
}

TEST_F(WebFrameSwapTest, RemoteWindowToString) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope scope(isolate);

  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  MainFrame()->LastChild()->Swap(remote_frame);
  v8::Local<v8::Value> to_string_result =
      MainFrame()->ExecuteScriptAndReturnValue(
          WebScriptSource("Object.prototype.toString.call(window[2])"));
  ASSERT_FALSE(to_string_result.IsEmpty());
  EXPECT_STREQ("[object Object]",
               *v8::String::Utf8Value(isolate, to_string_result));
}

// TODO(alexmos, dcheng): This test and some other OOPIF tests use
// very little of the test fixture support in WebFrameSwapTest.  We should
// clean these tests up.
TEST_F(WebFrameSwapTest, FramesOfRemoteParentAreIndexable) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  WebRemoteFrame* remote_parent_frame = frame_test_helpers::CreateRemote();
  MainFrame()->Swap(remote_parent_frame);
  remote_parent_frame->SetReplicatedOrigin(
      WebSecurityOrigin(SecurityOrigin::CreateUniqueOpaque()), false);

  WebLocalFrame* child_frame =
      frame_test_helpers::CreateLocalChild(*remote_parent_frame);
  frame_test_helpers::LoadFrame(child_frame, base_url_ + "subframe-hello.html");

  v8::Local<v8::Value> window =
      child_frame->ExecuteScriptAndReturnValue(WebScriptSource("window"));
  v8::Local<v8::Value> child_of_remote_parent =
      child_frame->ExecuteScriptAndReturnValue(
          WebScriptSource("parent.frames[0]"));
  EXPECT_TRUE(child_of_remote_parent->IsObject());
  EXPECT_TRUE(window->StrictEquals(child_of_remote_parent));

  v8::Local<v8::Value> window_length = child_frame->ExecuteScriptAndReturnValue(
      WebScriptSource("parent.frames.length"));
  ASSERT_TRUE(window_length->IsInt32());
  EXPECT_EQ(1, window_length.As<v8::Int32>()->Value());
}

// Check that frames with a remote parent don't crash while accessing
// window.frameElement.
TEST_F(WebFrameSwapTest, FrameElementInFramesWithRemoteParent) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  WebRemoteFrame* remote_parent_frame = frame_test_helpers::CreateRemote();
  MainFrame()->Swap(remote_parent_frame);
  remote_parent_frame->SetReplicatedOrigin(
      WebSecurityOrigin(SecurityOrigin::CreateUniqueOpaque()), false);

  WebLocalFrame* child_frame =
      frame_test_helpers::CreateLocalChild(*remote_parent_frame);
  frame_test_helpers::LoadFrame(child_frame, base_url_ + "subframe-hello.html");

  v8::Local<v8::Value> frame_element = child_frame->ExecuteScriptAndReturnValue(
      WebScriptSource("window.frameElement"));
  // frameElement should be null if cross-origin.
  ASSERT_FALSE(frame_element.IsEmpty());
  EXPECT_TRUE(frame_element->IsNull());
}

class RemoteToLocalSwapWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  explicit RemoteToLocalSwapWebFrameClient(WebRemoteFrame* remote_frame)
      : history_commit_type_(kWebHistoryInertCommit),
        remote_frame_(remote_frame) {}
  ~RemoteToLocalSwapWebFrameClient() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void DidCommitNavigation(const WebHistoryItem&,
                           WebHistoryCommitType history_commit_type,
                           bool) override {
    history_commit_type_ = history_commit_type;
    remote_frame_->Swap(Frame());
  }

  WebHistoryCommitType HistoryCommitType() const {
    return history_commit_type_;
  }

  WebHistoryCommitType history_commit_type_;
  WebRemoteFrame* remote_frame_;
};

// The commit type should be Initial if we are swapping a RemoteFrame to a
// LocalFrame as it is first being created.  This happens when another frame
// exists in the same process, such that we create the RemoteFrame before the
// first navigation occurs.
TEST_F(WebFrameSwapTest, HistoryCommitTypeAfterNewRemoteToLocalSwap) {
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  WebFrame* target_frame = MainFrame()->FirstChild();
  ASSERT_TRUE(target_frame);
  target_frame->Swap(remote_frame);
  ASSERT_TRUE(MainFrame()->FirstChild());
  ASSERT_EQ(MainFrame()->FirstChild(), remote_frame);

  RemoteToLocalSwapWebFrameClient client(remote_frame);
  WebLocalFrame* local_frame =
      frame_test_helpers::CreateProvisional(*remote_frame, &client);
  frame_test_helpers::LoadFrame(local_frame, base_url_ + "subframe-hello.html");
  EXPECT_EQ(kWebHistoryInertCommit, client.HistoryCommitType());

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  Reset();
}

// The commit type should be Standard if we are swapping a RemoteFrame to a
// LocalFrame after commits have already happened in the frame.  The browser
// process will inform us via setCommittedFirstRealLoad.
TEST_F(WebFrameSwapTest, HistoryCommitTypeAfterExistingRemoteToLocalSwap) {
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  WebFrame* target_frame = MainFrame()->FirstChild();
  ASSERT_TRUE(target_frame);
  target_frame->Swap(remote_frame);
  ASSERT_TRUE(MainFrame()->FirstChild());
  ASSERT_EQ(MainFrame()->FirstChild(), remote_frame);

  RemoteToLocalSwapWebFrameClient client(remote_frame);
  WebLocalFrameImpl* local_frame =
      frame_test_helpers::CreateProvisional(*remote_frame, &client);
  local_frame->SetCommittedFirstRealLoad();
  frame_test_helpers::LoadFrame(local_frame, base_url_ + "subframe-hello.html");
  EXPECT_EQ(kWebStandardCommit, client.HistoryCommitType());

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  Reset();
}

class RemoteNavigationClient
    : public frame_test_helpers::TestWebRemoteFrameClient {
 public:
  RemoteNavigationClient() = default;
  ~RemoteNavigationClient() override = default;

  // frame_test_helpers::TestWebRemoteFrameClient:
  void Navigate(const WebURLRequest& request,
                blink::WebLocalFrame* initiator_frame,
                bool should_replace_current_entry,
                bool is_opener_navigation,
                bool initiator_frame_has_download_sandbox_flag,
                bool blocking_downloads_in_sandbox_enabled,
                bool initiator_frame_is_ad,
                CrossVariantMojoRemote<mojom::blink::BlobURLTokenInterfaceBase>,
                const base::Optional<WebImpression>& impression) override {
    last_request_.CopyFrom(request);
  }

  const WebURLRequest& LastRequest() const { return last_request_; }

 private:
  WebURLRequest last_request_;
};

TEST_F(WebFrameSwapTest, NavigateRemoteFrameViaLocation) {
  RemoteNavigationClient client;
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote(&client);
  WebFrame* target_frame = MainFrame()->FirstChild();
  ASSERT_TRUE(target_frame);
  target_frame->Swap(remote_frame);
  ASSERT_TRUE(MainFrame()->FirstChild());
  ASSERT_EQ(MainFrame()->FirstChild(), remote_frame);

  remote_frame->SetReplicatedOrigin(
      WebSecurityOrigin::CreateFromString("http://127.0.0.1"), false);
  MainFrame()->ExecuteScript(
      WebScriptSource("document.getElementsByTagName('iframe')[0]."
                      "contentWindow.location = 'data:text/html,hi'"));
  ASSERT_FALSE(client.LastRequest().IsNull());
  EXPECT_EQ(WebURL(ToKURL("data:text/html,hi")), client.LastRequest().Url());

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  Reset();
}

TEST_F(WebFrameSwapTest, WindowOpenOnRemoteFrame) {
  RemoteNavigationClient remote_client;
  WebRemoteFrame* remote_frame =
      frame_test_helpers::CreateRemote(&remote_client);
  MainFrame()->FirstChild()->Swap(remote_frame);
  remote_frame->SetReplicatedOrigin(
      WebSecurityOrigin(SecurityOrigin::CreateUniqueOpaque()), false);

  ASSERT_TRUE(MainFrame()->FirstChild()->IsWebRemoteFrame());
  LocalDOMWindow* main_window =
      To<WebLocalFrameImpl>(MainFrame())->GetFrame()->DomWindow();

  String destination = "data:text/html:destination";
  NonThrowableExceptionState exception_state;
  ScriptState* script_state =
      ToScriptStateForMainWorld(main_window->GetFrame());
  ScriptState::Scope entered_context_scope(script_state);
  v8::Context::BackupIncumbentScope incumbent_context_scope(
      script_state->GetContext());
  main_window->open(script_state->GetIsolate(), destination, "frame1", "",
                    exception_state);
  ASSERT_FALSE(remote_client.LastRequest().IsNull());
  EXPECT_EQ(remote_client.LastRequest().Url(), WebURL(KURL(destination)));

  // Pointing a named frame to an empty URL should just return a reference to
  // the frame's window without navigating it.
  DOMWindow* result = main_window->open(script_state->GetIsolate(), "",
                                        "frame1", "", exception_state);
  EXPECT_EQ(remote_client.LastRequest().Url(), WebURL(KURL(destination)));
  EXPECT_EQ(result, WebFrame::ToCoreFrame(*remote_frame)->DomWindow());

  Reset();
}

// blink::mojom::RemoteMainFrameHost instance that intecepts CloseWindowSoon()
// mojo calls and provides a getter to know if it was ever called.
class TestRemoteMainFrameHostForWindowClose : public FakeRemoteMainFrameHost {
 public:
  TestRemoteMainFrameHostForWindowClose() = default;
  ~TestRemoteMainFrameHostForWindowClose() override = default;

  // FakeRemoteMainFrameHost:
  void RouteCloseEvent() override { remote_window_closed_ = true; }

  bool remote_window_closed() const { return remote_window_closed_; }

 private:
  bool remote_window_closed_ = false;
};

class RemoteWindowCloseTest : public WebFrameTest {
 public:
  RemoteWindowCloseTest() {
    remote_main_frame_host_.Init(
        remote_frame_client_.GetRemoteAssociatedInterfaces());
  }

  ~RemoteWindowCloseTest() override = default;

  frame_test_helpers::TestWebRemoteFrameClient* remote_frame_client() {
    return &remote_frame_client_;
  }

  bool Closed() const { return remote_main_frame_host_.remote_window_closed(); }

 private:
  TestRemoteMainFrameHostForWindowClose remote_main_frame_host_;
  frame_test_helpers::TestWebRemoteFrameClient remote_frame_client_;
};

TEST_F(RemoteWindowCloseTest, WindowOpenRemoteClose) {
  frame_test_helpers::WebViewHelper main_web_view;
  main_web_view.Initialize();

  // Create a remote window that will be closed later in the test.
  frame_test_helpers::WebViewHelper popup;
  popup.InitializeRemote(remote_frame_client(), nullptr, nullptr);
  popup.GetWebView()->DidAttachRemoteMainFrame();

  LocalFrame* local_frame = main_web_view.LocalMainFrame()->GetFrame();
  RemoteFrame* remote_frame = popup.RemoteMainFrame()->GetFrame();

  remote_frame->SetOpenerDoNotNotify(local_frame);

  // Attempt to close the window, which should fail as it isn't opened
  // by a script.
  ScriptState* local_script_state = ToScriptStateForMainWorld(local_frame);
  ScriptState::Scope entered_context_scope(local_script_state);
  v8::Context::BackupIncumbentScope incumbent_context_scope(
      local_script_state->GetContext());
  remote_frame->DomWindow()->close(local_script_state->GetIsolate());
  EXPECT_FALSE(Closed());

  // Marking it as opened by a script should now allow it to be closed.
  remote_frame->GetPage()->SetOpenedByDOM();
  remote_frame->DomWindow()->close(local_script_state->GetIsolate());

  // The request to close the remote window is not immediately sent to make sure
  // that the JS finishes executing, so we need to wait for pending tasks first.
  RunPendingTasks();
  EXPECT_TRUE(Closed());
}

TEST_F(WebFrameTest, NavigateRemoteToLocalWithOpener) {
  frame_test_helpers::WebViewHelper main_web_view;
  main_web_view.Initialize();
  WebLocalFrame* main_frame = main_web_view.LocalMainFrame();

  // Create a popup with a remote frame and set its opener to the main frame.
  frame_test_helpers::WebViewHelper popup_helper;
  popup_helper.InitializeRemoteWithOpener(
      main_frame, nullptr, SecurityOrigin::CreateFromString("http://foo.com"));
  WebRemoteFrame* popup_remote_frame = popup_helper.RemoteMainFrame();
  EXPECT_FALSE(main_frame->GetSecurityOrigin().CanAccess(
      popup_remote_frame->GetSecurityOrigin()));

  // Do a remote-to-local swap in the popup.
  WebLocalFrame* popup_local_frame =
      frame_test_helpers::CreateProvisional(*popup_remote_frame);
  popup_remote_frame->Swap(popup_local_frame);

  // The initial document created during the remote-to-local swap should have
  // inherited its opener's SecurityOrigin.
  EXPECT_TRUE(main_frame->GetSecurityOrigin().CanAccess(
      popup_helper.LocalMainFrame()->GetSecurityOrigin()));
}

TEST_F(WebFrameTest, SwapWithOpenerCycle) {
  // First, create a remote main frame with itself as the opener.
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote();
  WebRemoteFrame* remote_frame = helper.RemoteMainFrame();
  WebFrame::ToCoreFrame(*helper.RemoteMainFrame())
      ->SetOpenerDoNotNotify(WebFrame::ToCoreFrame(*remote_frame));

  // Now swap in a local frame. It shouldn't crash.
  WebLocalFrame* local_frame =
      frame_test_helpers::CreateProvisional(*remote_frame);
  remote_frame->Swap(local_frame);

  // And the opener cycle should still be preserved.
  EXPECT_EQ(local_frame, local_frame->Opener());
}

class CommitTypeWebFrameClient : public frame_test_helpers::TestWebFrameClient {
 public:
  CommitTypeWebFrameClient() : history_commit_type_(kWebHistoryInertCommit) {}
  ~CommitTypeWebFrameClient() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void DidCommitNavigation(const WebHistoryItem&,
                           WebHistoryCommitType history_commit_type,
                           bool) override {
    history_commit_type_ = history_commit_type;
  }

  WebHistoryCommitType HistoryCommitType() const {
    return history_commit_type_;
  }

 private:
  WebHistoryCommitType history_commit_type_;
};

TEST_F(WebFrameTest, RemoteFrameInitialCommitType) {
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote(nullptr, SecurityOrigin::CreateFromString(
                                       WebString::FromUTF8(base_url_)));

  // If an iframe has a remote main frame, ensure the inital commit is correctly
  // identified as kWebHistoryInertCommit.
  CommitTypeWebFrameClient child_frame_client;
  WebLocalFrame* child_frame = frame_test_helpers::CreateLocalChild(
      *helper.RemoteMainFrame(), "frameName", WebFrameOwnerProperties(),
      nullptr, &child_frame_client);
  RegisterMockedHttpURLLoad("foo.html");
  frame_test_helpers::LoadFrame(child_frame, base_url_ + "foo.html");
  EXPECT_EQ(kWebHistoryInertCommit, child_frame_client.HistoryCommitType());

  helper.Reset();
}

TEST_F(WebFrameTest, DetachRemoteFrame) {
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote();
  WebRemoteFrame* child_frame =
      frame_test_helpers::CreateRemoteChild(*helper.RemoteMainFrame());
  child_frame->Detach();
}

class TestConsoleMessageWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  TestConsoleMessageWebFrameClient() = default;
  ~TestConsoleMessageWebFrameClient() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void DidAddMessageToConsole(const WebConsoleMessage& message,
                              const WebString& source_name,
                              unsigned source_line,
                              const WebString& stack_trace) override {
    messages.push_back(message);
  }

  Vector<WebConsoleMessage> messages;
};

TEST_F(WebFrameTest, CrossDomainAccessErrorsUseCallingWindow) {
  RegisterMockedHttpURLLoad("hidden_frames.html");
  RegisterMockedChromeURLLoad("hello_world.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  TestConsoleMessageWebFrameClient web_frame_client;
  web_view_helper.InitializeAndLoad(base_url_ + "hidden_frames.html",
                                    &web_frame_client);

  // Create another window with a cross-origin page, and point its opener to
  // first window.
  frame_test_helpers::WebViewHelper popup_web_view_helper;
  TestConsoleMessageWebFrameClient popup_web_frame_client;
  WebViewImpl* popup_view = popup_web_view_helper.InitializeAndLoad(
      chrome_url_ + "hello_world.html", &popup_web_frame_client);
  WebFrame::ToCoreFrame(*popup_view->MainFrame())
      ->SetOpenerDoNotNotify(
          WebFrame::ToCoreFrame(*web_view_helper.GetWebView()->MainFrame()));

  // Attempt a blocked navigation of an opener's subframe, and ensure that
  // the error shows up on the popup (calling) window's console, rather than
  // the target window.
  popup_view->MainFrameImpl()->ExecuteScript(WebScriptSource(
      "try { opener.frames[1].location.href='data:text/html,foo'; } catch (e) "
      "{}"));
  EXPECT_TRUE(web_frame_client.messages.IsEmpty());
  ASSERT_EQ(1u, popup_web_frame_client.messages.size());
  EXPECT_TRUE(std::string::npos !=
              popup_web_frame_client.messages[0].text.Utf8().find(
                  "Unsafe JavaScript attempt to initiate navigation"));

  // Try setting a cross-origin iframe element's source to a javascript: URL,
  // and check that this error is also printed on the calling window.
  popup_view->MainFrameImpl()->ExecuteScript(
      WebScriptSource("opener.document.querySelectorAll('iframe')[1].src='"
                      "javascript:alert()'"));
  EXPECT_TRUE(web_frame_client.messages.IsEmpty());
  ASSERT_EQ(2u, popup_web_frame_client.messages.size());
  EXPECT_TRUE(
      std::string::npos !=
      popup_web_frame_client.messages[1].text.Utf8().find("Blocked a frame"));

  // Manually reset to break WebViewHelpers' dependencies on the stack
  // allocated WebLocalFrameClients.
  web_view_helper.Reset();
  popup_web_view_helper.Reset();
}

TEST_F(WebFrameTest, ResizeInvalidatesDeviceMediaQueries) {
  RegisterMockedHttpURLLoad("device_media_queries.html");
  FixedLayoutTestWebWidgetClient client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "device_media_queries.html",
                                    nullptr, nullptr, &client,
                                    ConfigureAndroid);
  auto* frame =
      To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame());
  Element* element = frame->GetDocument()->getElementById("test");
  ASSERT_TRUE(element);

  client.screen_info_.available_rect = gfx::Rect(700, 500);
  UpdateScreenInfoAndResizeView(&client, &web_view_helper,
                                client.screen_info_.available_rect.width(),
                                client.screen_info_.available_rect.height());
  EXPECT_EQ(300, element->OffsetWidth());
  EXPECT_EQ(300, element->OffsetHeight());

  client.screen_info_.available_rect = gfx::Rect(710, 500);
  UpdateScreenInfoAndResizeView(&client, &web_view_helper,
                                client.screen_info_.available_rect.width(),
                                client.screen_info_.available_rect.height());
  EXPECT_EQ(400, element->OffsetWidth());
  EXPECT_EQ(300, element->OffsetHeight());

  client.screen_info_.available_rect = gfx::Rect(690, 500);
  UpdateScreenInfoAndResizeView(&client, &web_view_helper,
                                client.screen_info_.available_rect.width(),
                                client.screen_info_.available_rect.height());
  EXPECT_EQ(200, element->OffsetWidth());
  EXPECT_EQ(300, element->OffsetHeight());

  client.screen_info_.available_rect = gfx::Rect(700, 510);
  UpdateScreenInfoAndResizeView(&client, &web_view_helper,
                                client.screen_info_.available_rect.width(),
                                client.screen_info_.available_rect.height());
  EXPECT_EQ(300, element->OffsetWidth());
  EXPECT_EQ(400, element->OffsetHeight());

  client.screen_info_.available_rect = gfx::Rect(700, 490);
  UpdateScreenInfoAndResizeView(&client, &web_view_helper,
                                client.screen_info_.available_rect.width(),
                                client.screen_info_.available_rect.height());
  EXPECT_EQ(300, element->OffsetWidth());
  EXPECT_EQ(200, element->OffsetHeight());

  client.screen_info_.available_rect = gfx::Rect(690, 510);
  UpdateScreenInfoAndResizeView(&client, &web_view_helper,
                                client.screen_info_.available_rect.width(),
                                client.screen_info_.available_rect.height());
  EXPECT_EQ(200, element->OffsetWidth());
  EXPECT_EQ(400, element->OffsetHeight());
}

class DeviceEmulationTest : public WebFrameTest {
 protected:
  DeviceEmulationTest() {
    RegisterMockedHttpURLLoad("device_emulation.html");
    client_.screen_info_.device_scale_factor = 1;
    web_view_helper_.InitializeAndLoad(base_url_ + "device_emulation.html",
                                       nullptr, nullptr, &client_);
  }

  void TestResize(const gfx::Size& size, const String& expected_size) {
    client_.screen_info_.available_rect = gfx::Rect(size);
    UpdateScreenInfoAndResizeView(&client_, &web_view_helper_,
                                  client_.screen_info_.available_rect.width(),
                                  client_.screen_info_.available_rect.height());
    EXPECT_EQ(expected_size, DumpSize("test"));
  }

  String DumpSize(const String& id) {
    String code = "dumpSize('" + id + "')";
    v8::HandleScope scope(v8::Isolate::GetCurrent());
    ScriptExecutionCallbackHelper callback_helper(
        web_view_helper_.LocalMainFrame()->MainWorldScriptContext());
    web_view_helper_.GetWebView()
        ->MainFrameImpl()
        ->RequestExecuteScriptAndReturnValue(WebScriptSource(WebString(code)),
                                             false, &callback_helper);
    RunPendingTasks();
    EXPECT_TRUE(callback_helper.DidComplete());
    return callback_helper.StringValue();
  }

  FixedLayoutTestWebWidgetClient client_;
  frame_test_helpers::WebViewHelper web_view_helper_;
};

TEST_F(DeviceEmulationTest, DeviceSizeInvalidatedOnResize) {
  DeviceEmulationParams params;
  params.screen_type = mojom::EmulatedScreenType::kMobile;
  web_view_helper_.GetWebView()->EnableDeviceEmulation(params);

  TestResize(gfx::Size(700, 500), "300x300");
  TestResize(gfx::Size(710, 500), "400x300");
  TestResize(gfx::Size(690, 500), "200x300");
  TestResize(gfx::Size(700, 510), "300x400");
  TestResize(gfx::Size(700, 490), "300x200");
  TestResize(gfx::Size(710, 510), "400x400");
  TestResize(gfx::Size(690, 490), "200x200");
  TestResize(gfx::Size(800, 600), "400x400");

  web_view_helper_.GetWebView()->DisableDeviceEmulation();
}

TEST_F(DeviceEmulationTest, PointerAndHoverTypes) {
  web_view_helper_.GetWebView()
      ->GetDevToolsEmulator()
      ->SetTouchEventEmulationEnabled(true, 1);
  EXPECT_EQ("20x20", DumpSize("pointer"));
  web_view_helper_.GetWebView()
      ->GetDevToolsEmulator()
      ->SetTouchEventEmulationEnabled(false, 1);
}

TEST_F(WebFrameTest, CreateLocalChildWithPreviousSibling) {
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote();
  WebRemoteFrame* parent = helper.RemoteMainFrame();

  WebLocalFrame* second_frame(
      frame_test_helpers::CreateLocalChild(*parent, "name2"));
  WebLocalFrame* fourth_frame(frame_test_helpers::CreateLocalChild(
      *parent, "name4", WebFrameOwnerProperties(), second_frame));
  WebLocalFrame* third_frame(frame_test_helpers::CreateLocalChild(
      *parent, "name3", WebFrameOwnerProperties(), second_frame));
  WebLocalFrame* first_frame(
      frame_test_helpers::CreateLocalChild(*parent, "name1"));

  EXPECT_EQ(first_frame, parent->FirstChild());
  EXPECT_EQ(nullptr, first_frame->PreviousSibling());
  EXPECT_EQ(second_frame, first_frame->NextSibling());

  EXPECT_EQ(first_frame, second_frame->PreviousSibling());
  EXPECT_EQ(third_frame, second_frame->NextSibling());

  EXPECT_EQ(second_frame, third_frame->PreviousSibling());
  EXPECT_EQ(fourth_frame, third_frame->NextSibling());

  EXPECT_EQ(third_frame, fourth_frame->PreviousSibling());
  EXPECT_EQ(nullptr, fourth_frame->NextSibling());
  EXPECT_EQ(fourth_frame, parent->LastChild());

  EXPECT_EQ(parent, first_frame->Parent());
  EXPECT_EQ(parent, second_frame->Parent());
  EXPECT_EQ(parent, third_frame->Parent());
  EXPECT_EQ(parent, fourth_frame->Parent());
}

TEST_F(WebFrameTest, SendBeaconFromChildWithRemoteMainFrame) {
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote();

  WebLocalFrame* local_frame =
      frame_test_helpers::CreateLocalChild(*helper.RemoteMainFrame());

  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  RegisterMockedHttpURLLoad("send_beacon.html");
  RegisterMockedHttpURLLoad("reload_post.html");  // url param to sendBeacon()
  frame_test_helpers::LoadFrame(local_frame, base_url_ + "send_beacon.html");
  // Wait for the post.
  frame_test_helpers::PumpPendingRequestsForFrameToLoad(local_frame);
}

TEST_F(WebFrameTest, SiteForCookiesFromChildWithRemoteMainFrame) {
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote(nullptr,
                          SecurityOrigin::Create(ToKURL(not_base_url_)));

  WebLocalFrame* local_frame =
      frame_test_helpers::CreateLocalChild(*helper.RemoteMainFrame());

  RegisterMockedHttpURLLoad("foo.html");
  frame_test_helpers::LoadFrame(local_frame, base_url_ + "foo.html");
  EXPECT_TRUE(local_frame->GetDocument().SiteForCookies().IsNull());

  SchemeRegistry::RegisterURLSchemeAsFirstPartyWhenTopLevel("http");
  EXPECT_TRUE(net::SiteForCookies::FromUrl(ToKURL(not_base_url_))
                  .IsEquivalent(local_frame->GetDocument().SiteForCookies()));
  SchemeRegistry::RemoveURLSchemeAsFirstPartyWhenTopLevel("http");
}

// See https://crbug.com/525285.
TEST_F(WebFrameTest, RemoteToLocalSwapOnMainFrameInitializesCoreFrame) {
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote();

  frame_test_helpers::CreateLocalChild(*helper.RemoteMainFrame());

  // Do a remote-to-local swap of the top frame.
  WebLocalFrame* local_root =
      frame_test_helpers::CreateProvisional(*helper.RemoteMainFrame());
  helper.RemoteMainFrame()->Swap(local_root);

  // Load a page with a child frame in the new root to make sure this doesn't
  // crash when the child frame invokes setCoreFrame.
  RegisterMockedHttpURLLoad("single_iframe.html");
  RegisterMockedHttpURLLoad("visible_iframe.html");
  frame_test_helpers::LoadFrame(local_root, base_url_ + "single_iframe.html");
}

// See https://crbug.com/628942.
TEST_F(WebFrameTest, PausedPageLoadWithRemoteMainFrame) {
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote();
  WebRemoteFrameImpl* remote_root = helper.RemoteMainFrame();

  // Check that ScopedPagePauser properly triggers deferred loading for
  // the current Page.
  Page* page = remote_root->GetFrame()->GetPage();
  EXPECT_FALSE(page->Paused());
  {
    ScopedPagePauser pauser;
    EXPECT_TRUE(page->Paused());
  }
  EXPECT_FALSE(page->Paused());

  // Repeat this for a page with a local child frame, and ensure that the
  // child frame's loads are also suspended.
  WebLocalFrameImpl* web_local_child =
      frame_test_helpers::CreateLocalChild(*remote_root);
  RegisterMockedHttpURLLoad("foo.html");
  frame_test_helpers::LoadFrame(web_local_child, base_url_ + "foo.html");
  LocalFrame* local_child = web_local_child->GetFrame();
  EXPECT_FALSE(page->Paused());
  EXPECT_FALSE(
      local_child->GetDocument()->Fetcher()->GetProperties().IsPaused());
  {
    ScopedPagePauser pauser;
    EXPECT_TRUE(page->Paused());
    EXPECT_TRUE(
        local_child->GetDocument()->Fetcher()->GetProperties().IsPaused());
  }
  EXPECT_FALSE(page->Paused());
  EXPECT_FALSE(
      local_child->GetDocument()->Fetcher()->GetProperties().IsPaused());
}

class OverscrollWidgetInputHandlerHost
    : public frame_test_helpers::TestWidgetInputHandlerHost {
 public:
  MOCK_METHOD5(DidOverscroll,
               void(const gfx::Vector2dF&,
                    const gfx::Vector2dF&,
                    const gfx::PointF&,
                    const gfx::Vector2dF&,
                    cc::OverscrollBehavior));

  void DidOverscroll(mojom::blink::DidOverscrollParamsPtr params) override {
    DidOverscroll(params->latest_overscroll_delta,
                  params->accumulated_overscroll,
                  params->causal_event_viewport_point,
                  params->current_fling_velocity, params->overscroll_behavior);
  }
};

class OverscrollWebWidgetClient
    : public frame_test_helpers::TestWebWidgetClient {
 public:
  OverscrollWebWidgetClient() = default;

  frame_test_helpers::TestWidgetInputHandlerHost* GetInputHandlerHost()
      override {
    return &input_handler_host_;
  }

  OverscrollWidgetInputHandlerHost& GetOverscrollWidgetInputHandlerHost() {
    return input_handler_host_;
  }

 private:
  OverscrollWidgetInputHandlerHost input_handler_host_;
};

class WebFrameOverscrollTest
    : public WebFrameTest,
      public testing::WithParamInterface<WebGestureDevice> {
 public:
  WebFrameOverscrollTest() {}

 protected:
  WebCoalescedInputEvent GenerateEvent(WebInputEvent::Type type,
                                       float delta_x = 0.0,
                                       float delta_y = 0.0) {
    WebGestureEvent event(type, WebInputEvent::kNoModifiers,
                          WebInputEvent::GetStaticTimeStampForTests(),
                          GetParam());
    // TODO(wjmaclean): Make sure that touchpad device is only ever used for
    // gesture scrolling event types.
    event.SetPositionInWidget(gfx::PointF(100, 100));
    if (type == WebInputEvent::Type::kGestureScrollUpdate) {
      event.data.scroll_update.delta_x = delta_x;
      event.data.scroll_update.delta_y = delta_y;
    } else if (type == WebInputEvent::Type::kGestureScrollBegin) {
      event.data.scroll_begin.delta_x_hint = delta_x;
      event.data.scroll_begin.delta_y_hint = delta_y;
    }
    return WebCoalescedInputEvent(event, ui::LatencyInfo());
  }

  void ScrollBegin(frame_test_helpers::WebViewHelper* web_view_helper,
                   float delta_x_hint,
                   float delta_y_hint) {
    web_view_helper->GetWebView()->MainFrameWidget()->HandleInputEvent(
        GenerateEvent(WebInputEvent::Type::kGestureScrollBegin, delta_x_hint,
                      delta_y_hint));
  }

  void ScrollUpdate(frame_test_helpers::WebViewHelper* web_view_helper,
                    float delta_x,
                    float delta_y) {
    web_view_helper->GetWebView()->MainFrameWidget()->HandleInputEvent(
        GenerateEvent(WebInputEvent::Type::kGestureScrollUpdate, delta_x,
                      delta_y));
  }

  void ScrollEnd(frame_test_helpers::WebViewHelper* web_view_helper) {
    web_view_helper->GetWebView()->MainFrameWidget()->HandleInputEvent(
        GenerateEvent(WebInputEvent::Type::kGestureScrollEnd));
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         WebFrameOverscrollTest,
                         testing::Values(WebGestureDevice::kTouchpad,
                                         WebGestureDevice::kTouchscreen));

TEST_P(WebFrameOverscrollTest,
       AccumulatedRootOverscrollAndUnsedDeltaValuesOnOverscroll) {
  OverscrollWebWidgetClient client;
  RegisterMockedHttpURLLoad("overscroll/overscroll.html");
  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "overscroll/overscroll.html",
                                    nullptr, nullptr, &client,
                                    ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(200, 200));

  // Calculation of accumulatedRootOverscroll and unusedDelta on multiple
  // scrollUpdate.
  ScrollBegin(&web_view_helper, -300, -316);
  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(gfx::Vector2dF(8, 16), gfx::Vector2dF(8, 16),
                            gfx::PointF(100, 100), gfx::Vector2dF(),
                            kOverscrollBehaviorAuto));
  ScrollUpdate(&web_view_helper, -308, -316);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(gfx::Vector2dF(0, 13), gfx::Vector2dF(8, 29),
                            gfx::PointF(100, 100), gfx::Vector2dF(),
                            kOverscrollBehaviorAuto));
  ScrollUpdate(&web_view_helper, 0, -13);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(gfx::Vector2dF(20, 13), gfx::Vector2dF(28, 42),
                            gfx::PointF(100, 100), gfx::Vector2dF(),
                            kOverscrollBehaviorAuto));
  ScrollUpdate(&web_view_helper, -20, -13);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  // Overscroll is not reported.
  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(_, _, _, _, _))
      .Times(0);
  ScrollUpdate(&web_view_helper, 0, 1);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(_, _, _, _, _))
      .Times(0);
  ScrollUpdate(&web_view_helper, 1, 0);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  // Overscroll is reported.
  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(gfx::Vector2dF(0, -701), gfx::Vector2dF(0, -701),
                            gfx::PointF(100, 100), gfx::Vector2dF(),
                            kOverscrollBehaviorAuto));
  ScrollUpdate(&web_view_helper, 0, 1000);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  // Overscroll is not reported.
  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(_, _, _, _, _))
      .Times(0);
  ScrollEnd(&web_view_helper);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());
}

TEST_P(WebFrameOverscrollTest,
       AccumulatedOverscrollAndUnusedDeltaValuesOnDifferentAxesOverscroll) {
  OverscrollWebWidgetClient client;
  RegisterMockedHttpURLLoad("overscroll/div-overscroll.html");
  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(
      base_url_ + "overscroll/div-overscroll.html", nullptr, nullptr, &client,
      ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(200, 200));

  ScrollBegin(&web_view_helper, 0, -316);

  // Scroll the Div to the end.
  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(_, _, _, _, _))
      .Times(0);
  ScrollUpdate(&web_view_helper, 0, -316);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  ScrollEnd(&web_view_helper);
  ScrollBegin(&web_view_helper, 0, -100);

  // Now On Scrolling DIV, scroll is bubbled and root layer is over-scrolled.
  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(gfx::Vector2dF(0, 100), gfx::Vector2dF(0, 100),
                            gfx::PointF(100, 100), gfx::Vector2dF(),
                            kOverscrollBehaviorAuto));
  ScrollUpdate(&web_view_helper, 0, -100);
  ScrollUpdate(&web_view_helper, 0, -100);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  // TODO(bokan): This has never worked but by the accident that this test was
  // being run in a WebView without a size. This test should be fixed along with
  // the bug, crbug.com/589320.
  // Page scrolls vertically, but over-scrolls horizontally.
  // EXPECT_CALL(client, didOverscroll(gfx::Vector2dF(-100, 0),
  // gfx::Vector2dF(-100, 0), gfx::PointF(100, 100), gfx::Vector2dF()));
  // ScrollUpdate(&webViewHelper, 100, 50);
  // Mock::VerifyAndClearExpectations(&client);

  // Scrolling up, Overscroll is not reported.
  // EXPECT_CALL(client, didOverscroll(_, _, _, _)).Times(0);
  // ScrollUpdate(&webViewHelper, 0, -50);
  // Mock::VerifyAndClearExpectations(&client);

  // Page scrolls horizontally, but over-scrolls vertically.
  // EXPECT_CALL(client, didOverscroll(gfx::Vector2dF(0, 100), gfx::Vector2dF(0,
  // 100), gfx::PointF(100, 100), gfx::Vector2dF()));
  // ScrollUpdate(&webViewHelper, -100, -100);
  // Mock::VerifyAndClearExpectations(&client);
}

TEST_P(WebFrameOverscrollTest, RootLayerOverscrolledOnInnerDivOverScroll) {
  OverscrollWebWidgetClient client;
  RegisterMockedHttpURLLoad("overscroll/div-overscroll.html");
  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(
      base_url_ + "overscroll/div-overscroll.html", nullptr, nullptr, &client,
      ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(200, 200));

  ScrollBegin(&web_view_helper, 0, -316);

  // Scroll the Div to the end.
  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(_, _, _, _, _))
      .Times(0);
  ScrollUpdate(&web_view_helper, 0, -316);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  ScrollEnd(&web_view_helper);
  ScrollBegin(&web_view_helper, 0, -150);

  // Now On Scrolling DIV, scroll is bubbled and root layer is over-scrolled.
  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(gfx::Vector2dF(0, 50), gfx::Vector2dF(0, 50),
                            gfx::PointF(100, 100), gfx::Vector2dF(),
                            kOverscrollBehaviorAuto));
  ScrollUpdate(&web_view_helper, 0, -150);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());
}

TEST_P(WebFrameOverscrollTest, RootLayerOverscrolledOnInnerIFrameOverScroll) {
  OverscrollWebWidgetClient client;
  RegisterMockedHttpURLLoad("overscroll/iframe-overscroll.html");
  RegisterMockedHttpURLLoad("overscroll/scrollable-iframe.html");
  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(
      base_url_ + "overscroll/iframe-overscroll.html", nullptr, nullptr,
      &client, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(200, 200));

  ScrollBegin(&web_view_helper, 0, -320);
  // Scroll the IFrame to the end.
  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(_, _, _, _, _))
      .Times(0);

  // This scroll will fully scroll the iframe but will be consumed before being
  // counted as overscroll.
  ScrollUpdate(&web_view_helper, 0, -320);

  // This scroll will again target the iframe but wont bubble further up. Make
  // sure that the unused scroll isn't handled as overscroll.
  ScrollUpdate(&web_view_helper, 0, -50);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  ScrollEnd(&web_view_helper);
  ScrollBegin(&web_view_helper, 0, -150);

  // Now On Scrolling IFrame, scroll is bubbled and root layer is over-scrolled.
  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(gfx::Vector2dF(0, 50), gfx::Vector2dF(0, 50),
                            gfx::PointF(100, 100), gfx::Vector2dF(),
                            kOverscrollBehaviorAuto));
  ScrollUpdate(&web_view_helper, 0, -150);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  ScrollEnd(&web_view_helper);
}

TEST_P(WebFrameOverscrollTest, ScaledPageRootLayerOverscrolled) {
  OverscrollWebWidgetClient client;
  RegisterMockedHttpURLLoad("overscroll/overscroll.html");
  frame_test_helpers::WebViewHelper web_view_helper;

  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "overscroll/overscroll.html", nullptr, nullptr, &client,
      ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(200, 200));
  web_view_impl->SetPageScaleFactor(3.0);

  // Calculation of accumulatedRootOverscroll and unusedDelta on scaled page.
  // The point is (99, 99) because we clamp in the division by 3 to 33 so when
  // we go back to viewport coordinates it becomes (99, 99).
  ScrollBegin(&web_view_helper, 0, 30);
  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(gfx::Vector2dF(0, -30), gfx::Vector2dF(0, -30),
                            gfx::PointF(99, 99), gfx::Vector2dF(),
                            kOverscrollBehaviorAuto));
  ScrollUpdate(&web_view_helper, 0, 30);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(gfx::Vector2dF(0, -30), gfx::Vector2dF(0, -60),
                            gfx::PointF(99, 99), gfx::Vector2dF(),
                            kOverscrollBehaviorAuto));
  ScrollUpdate(&web_view_helper, 0, 30);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(gfx::Vector2dF(-30, -30), gfx::Vector2dF(-30, -90),
                            gfx::PointF(99, 99), gfx::Vector2dF(),
                            kOverscrollBehaviorAuto));
  ScrollUpdate(&web_view_helper, 30, 30);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(gfx::Vector2dF(-30, 0), gfx::Vector2dF(-60, -90),
                            gfx::PointF(99, 99), gfx::Vector2dF(),
                            kOverscrollBehaviorAuto));
  ScrollUpdate(&web_view_helper, 30, 0);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  // Overscroll is not reported.
  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(_, _, _, _, _))
      .Times(0);
  ScrollEnd(&web_view_helper);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());
}

TEST_P(WebFrameOverscrollTest, NoOverscrollForSmallvalues) {
  OverscrollWebWidgetClient client;
  RegisterMockedHttpURLLoad("overscroll/overscroll.html");
  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "overscroll/overscroll.html",
                                    nullptr, nullptr, &client,
                                    ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(200, 200));

  ScrollBegin(&web_view_helper, 10, 10);
  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(gfx::Vector2dF(-10, -10), gfx::Vector2dF(-10, -10),
                            gfx::PointF(100, 100), gfx::Vector2dF(),
                            kOverscrollBehaviorAuto));
  ScrollUpdate(&web_view_helper, 10, 10);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(gfx::Vector2dF(0, -0.10),
                            gfx::Vector2dF(-10, -10.10), gfx::PointF(100, 100),
                            gfx::Vector2dF(), kOverscrollBehaviorAuto));
  ScrollUpdate(&web_view_helper, 0, 0.10);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  EXPECT_CALL(
      client.GetOverscrollWidgetInputHandlerHost(),
      DidOverscroll(gfx::Vector2dF(-0.10, 0), gfx::Vector2dF(-10.10, -10.10),
                    gfx::PointF(100, 100), gfx::Vector2dF(),
                    kOverscrollBehaviorAuto));
  ScrollUpdate(&web_view_helper, 0.10, 0);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  // For residual values overscrollDelta should be reset and DidOverscroll
  // shouldn't be called.
  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(_, _, _, _, _))
      .Times(0);
  ScrollUpdate(&web_view_helper, 0, 0.09);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(_, _, _, _, _))
      .Times(0);
  ScrollUpdate(&web_view_helper, 0.09, 0.09);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(_, _, _, _, _))
      .Times(0);
  ScrollUpdate(&web_view_helper, 0.09, 0);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(_, _, _, _, _))
      .Times(0);
  ScrollUpdate(&web_view_helper, 0, -0.09);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(_, _, _, _, _))
      .Times(0);
  ScrollUpdate(&web_view_helper, -0.09, -0.09);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(_, _, _, _, _))
      .Times(0);
  ScrollUpdate(&web_view_helper, -0.09, 0);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());

  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(_, _, _, _, _))
      .Times(0);
  ScrollEnd(&web_view_helper);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());
}

TEST_P(WebFrameOverscrollTest, OverscrollBehaviorGoesToCompositor) {
  OverscrollWebWidgetClient client;
  RegisterMockedHttpURLLoad("overscroll/overscroll.html");
  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "overscroll/overscroll.html",
                                    nullptr, nullptr, &client,
                                    ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(200, 200));

  WebLocalFrame* mainFrame =
      web_view_helper.GetWebView()->MainFrame()->ToWebLocalFrame();
  EXPECT_EQ(web_view_helper.GetLayerTreeHost()->overscroll_behavior(),
            kOverscrollBehaviorAuto);
  mainFrame->ExecuteScript(
      WebScriptSource(WebString("document.body.style="
                                "'overscroll-behavior: auto;'")));
  ScrollBegin(&web_view_helper, 100, 116);
  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(gfx::Vector2dF(-100, -100),
                            gfx::Vector2dF(-100, -100), gfx::PointF(100, 100),
                            gfx::Vector2dF(), kOverscrollBehaviorAuto));
  ScrollUpdate(&web_view_helper, 100, 100);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());
  EXPECT_EQ(web_view_helper.GetLayerTreeHost()->overscroll_behavior(),
            kOverscrollBehaviorAuto);

  mainFrame->ExecuteScript(
      WebScriptSource(WebString("document.body.style="
                                "'overscroll-behavior: contain;'")));
  ScrollBegin(&web_view_helper, 100, 116);
  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(gfx::Vector2dF(-100, -100),
                            gfx::Vector2dF(-200, -200), gfx::PointF(100, 100),
                            gfx::Vector2dF(), kOverscrollBehaviorContain));
  ScrollUpdate(&web_view_helper, 100, 100);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());
  EXPECT_EQ(web_view_helper.GetLayerTreeHost()->overscroll_behavior(),
            kOverscrollBehaviorContain);

  mainFrame->ExecuteScript(
      WebScriptSource(WebString("document.body.style="
                                "'overscroll-behavior: none;'")));
  ScrollBegin(&web_view_helper, 100, 116);
  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(gfx::Vector2dF(-100, -100),
                            gfx::Vector2dF(-300, -300), gfx::PointF(100, 100),
                            gfx::Vector2dF(), kOverscrollBehaviorNone));
  ScrollUpdate(&web_view_helper, 100, 100);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());
  EXPECT_EQ(web_view_helper.GetLayerTreeHost()->overscroll_behavior(),
            kOverscrollBehaviorNone);
}

TEST_P(WebFrameOverscrollTest, OnlyMainFrameOverscrollBehaviorHasEffect) {
  OverscrollWebWidgetClient client;
  RegisterMockedHttpURLLoad("overscroll/iframe-overscroll.html");
  RegisterMockedHttpURLLoad("overscroll/scrollable-iframe.html");
  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(
      base_url_ + "overscroll/iframe-overscroll.html", nullptr, nullptr,
      &client, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(200, 200));

  WebLocalFrame* mainFrame =
      web_view_helper.GetWebView()->MainFrame()->ToWebLocalFrame();
  mainFrame->ExecuteScript(
      WebScriptSource(WebString("document.body.style="
                                "'overscroll-behavior: auto;'")));
  WebLocalFrame* subframe = web_view_helper.GetWebView()
                                ->MainFrame()
                                ->FirstChild()
                                ->ToWebLocalFrame();
  subframe->ExecuteScript(
      WebScriptSource(WebString("document.body.style="
                                "'overscroll-behavior: none;'")));

  ScrollBegin(&web_view_helper, 100, 116);
  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(gfx::Vector2dF(-100, -100),
                            gfx::Vector2dF(-100, -100), gfx::PointF(100, 100),
                            gfx::Vector2dF(), kOverscrollBehaviorAuto));
  ScrollUpdate(&web_view_helper, 100, 100);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());
  EXPECT_EQ(web_view_helper.GetLayerTreeHost()->overscroll_behavior(),
            kOverscrollBehaviorAuto);

  mainFrame->ExecuteScript(
      WebScriptSource(WebString("document.body.style="
                                "'overscroll-behavior: contain;'")));
  EXPECT_CALL(client.GetOverscrollWidgetInputHandlerHost(),
              DidOverscroll(gfx::Vector2dF(-100, -100),
                            gfx::Vector2dF(-200, -200), gfx::PointF(100, 100),
                            gfx::Vector2dF(), kOverscrollBehaviorContain));
  ScrollUpdate(&web_view_helper, 100, 100);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(
      &client.GetOverscrollWidgetInputHandlerHost());
  EXPECT_EQ(web_view_helper.GetLayerTreeHost()->overscroll_behavior(),
            kOverscrollBehaviorContain);
}

TEST_F(WebFrameTest, OrientationFrameDetach) {
  ScopedOrientationEventForTest orientation_event(true);
  RegisterMockedHttpURLLoad("orientation-frame-detach.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "orientation-frame-detach.html");
  web_view_impl->MainFrameImpl()->SendOrientationChangeEvent();
}

TEST_F(WebFrameTest, MaxFrames) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeRemote();
  Page* page = web_view_helper.GetWebView()->GetPage();

  WebLocalFrameImpl* frame =
      frame_test_helpers::CreateLocalChild(*web_view_helper.RemoteMainFrame());
  while (page->SubframeCount() < Page::MaxNumberOfFrames()) {
    frame_test_helpers::CreateRemoteChild(*web_view_helper.RemoteMainFrame());
  }
  auto* iframe = MakeGarbageCollected<HTMLIFrameElement>(
      *frame->GetFrame()->GetDocument());
  iframe->setAttribute(html_names::kSrcAttr, "");
  frame->GetFrame()->GetDocument()->body()->appendChild(iframe);
  EXPECT_FALSE(iframe->ContentFrame());
}

class TestViewportIntersection : public FakeRemoteFrameHost {
 public:
  TestViewportIntersection() = default;
  ~TestViewportIntersection() override = default;

  const mojom::blink::ViewportIntersectionStatePtr& GetIntersectionState()
      const {
    return intersection_state_;
  }

  // FakeRemoteFrameHost:
  void UpdateViewportIntersection(
      mojom::blink::ViewportIntersectionStatePtr intersection_state) override {
    intersection_state_ = std::move(intersection_state);
  }

 private:
  mojom::blink::ViewportIntersectionStatePtr intersection_state_;
};

TEST_F(WebFrameTest, RotatedIframeViewportIntersection) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();
  WebViewImpl* web_view = web_view_helper.GetWebView();
  web_view->Resize(gfx::Size(800, 600));
  InitializeWithHTML(*web_view->MainFrameImpl()->GetFrame(), R"HTML(
<!DOCTYPE html>
<style>
  iframe {
    position: absolute;
    top: 200px;
    left: 200px;
    transform: rotate(45deg);
  }
</style>
<iframe></iframe>
  )HTML");
  frame_test_helpers::TestWebRemoteFrameClient remote_frame_client;
  TestViewportIntersection remote_frame_host;
  remote_frame_host.Init(remote_frame_client.GetRemoteAssociatedInterfaces());
  WebRemoteFrameImpl* remote_frame =
      frame_test_helpers::CreateRemote(&remote_frame_client);
  web_view_helper.LocalMainFrame()->FirstChild()->Swap(remote_frame);
  web_view->MainFrameImpl()->GetFrame()->View()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);
  web_view->MainFrameImpl()->GetFrame()->View()->RunPostLifecycleSteps();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(!remote_frame_host.GetIntersectionState()
                   ->viewport_intersection.IsEmpty());
  EXPECT_TRUE(IntRect(IntPoint(), remote_frame->GetFrame()->View()->Size())
                  .Contains(IntRect(remote_frame_host.GetIntersectionState()
                                        ->viewport_intersection)));
  ASSERT_TRUE(!remote_frame_host.GetIntersectionState()
                   ->main_frame_intersection.IsEmpty());
  EXPECT_TRUE(IntRect(IntPoint(), remote_frame->GetFrame()->View()->Size())
                  .Contains(IntRect(remote_frame_host.GetIntersectionState()
                                        ->main_frame_intersection)));
  remote_frame->Detach();
}

TEST_F(WebFrameTest, ImageDocumentLoadResponseEnd) {
  // Loading an image resource directly generates an ImageDocument with
  // the document loader feeding image data into the resource of a generated
  // img tag. We expect the load finish time to be the same for the document
  // and the image resource.

  RegisterMockedHttpURLLoadWithMimeType("white-1x1.png", "image/png");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "white-1x1.png");
  WebViewImpl* web_view = web_view_helper.GetWebView();
  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();

  EXPECT_TRUE(document);
  EXPECT_TRUE(IsA<ImageDocument>(document));

  auto* img_document = To<ImageDocument>(document);
  ImageResourceContent* image_content = img_document->CachedImage();

  EXPECT_TRUE(image_content);
  EXPECT_NE(base::TimeTicks(), image_content->LoadResponseEnd());

  DocumentLoader* loader = document->Loader();

  EXPECT_TRUE(loader);
  EXPECT_EQ(loader->GetTiming().ResponseEnd(),
            image_content->LoadResponseEnd());
}

TEST_F(WebFrameTest, CopyImageDocument) {
  // After loading an image document, we should be able to copy it directly.

  RegisterMockedHttpURLLoadWithMimeType("white-1x1.png", "image/png");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "white-1x1.png");
  WebViewImpl* web_view = web_view_helper.GetWebView();
  WebLocalFrameImpl* web_frame = web_view->MainFrameImpl();
  Document* document = web_frame->GetFrame()->GetDocument();

  ASSERT_TRUE(document);
  EXPECT_TRUE(IsA<ImageDocument>(document));

  // Setup a mock clipboard host.
  PageTestBase::MockClipboardHostProvider mock_clipboard_host_provider(
      web_frame->GetFrame()->GetBrowserInterfaceBroker());

  SystemClipboard* system_clipboard =
      document->GetFrame()->GetSystemClipboard();
  ASSERT_TRUE(system_clipboard);

  EXPECT_TRUE(system_clipboard->ReadAvailableTypes().IsEmpty());

  bool result = web_frame->ExecuteCommand("Copy");
  test::RunPendingTasks();

  EXPECT_TRUE(result);

  Vector<String> types = system_clipboard->ReadAvailableTypes();
  EXPECT_EQ(2u, types.size());
  EXPECT_EQ("text/html", types[0]);
  EXPECT_EQ("image/png", types[1]);

  // Clear clipboard data
  system_clipboard->WritePlainText("");
  system_clipboard->CommitWrite();
}

TEST_F(WebFrameTest, CopyTextInImageDocument) {
  // If Javascript inserts other contents into an image document, we should be
  // able to copy those contents, not just the image itself.

  RegisterMockedHttpURLLoadWithMimeType("white-1x1.png", "image/png");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "white-1x1.png");
  WebViewImpl* web_view = web_view_helper.GetWebView();
  WebLocalFrameImpl* web_frame = web_view->MainFrameImpl();
  Document* document = web_frame->GetFrame()->GetDocument();

  ASSERT_TRUE(document);
  EXPECT_TRUE(IsA<ImageDocument>(document));

  Node* text = document->createTextNode("copy me");
  document->body()->appendChild(text);
  document->GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder().SelectAllChildren(*text).Build(),
      SetSelectionOptions());

  // Setup a mock clipboard host.
  PageTestBase::MockClipboardHostProvider mock_clipboard_host_provider(
      web_frame->GetFrame()->GetBrowserInterfaceBroker());

  SystemClipboard* system_clipboard =
      document->GetFrame()->GetSystemClipboard();
  ASSERT_TRUE(system_clipboard);

  EXPECT_TRUE(system_clipboard->ReadAvailableTypes().IsEmpty());

  bool result = web_frame->ExecuteCommand("Copy");
  test::RunPendingTasks();

  EXPECT_TRUE(result);

  Vector<String> types = system_clipboard->ReadAvailableTypes();
  EXPECT_EQ(2u, types.size());
  EXPECT_EQ("text/plain", types[0]);
  EXPECT_EQ("text/html", types[1]);

  // Clear clipboard data
  system_clipboard->WritePlainText("");
  system_clipboard->CommitWrite();
}

class TestRemoteFrameHostForVisibility : public FakeRemoteFrameHost {
 public:
  TestRemoteFrameHostForVisibility() = default;
  ~TestRemoteFrameHostForVisibility() override = default;

  // FakeRemoteFrameHost:
  void VisibilityChanged(blink::mojom::FrameVisibility visibility) override {
    visibility_ = visibility;
  }

  blink::mojom::FrameVisibility visibility() const { return visibility_; }

 private:
  blink::mojom::FrameVisibility visibility_ =
      blink::mojom::FrameVisibility::kRenderedInViewport;
};

class WebRemoteFrameVisibilityChangeTest : public WebFrameTest {
 public:
  WebRemoteFrameVisibilityChangeTest() {
    RegisterMockedHttpURLLoad("visible_iframe.html");
    RegisterMockedHttpURLLoad("single_iframe.html");
    frame_ =
        web_view_helper_.InitializeAndLoad(base_url_ + "single_iframe.html")
            ->MainFrameImpl();
    web_view_helper_.Resize(gfx::Size(640, 480));
    remote_frame_host_.Init(
        remote_frame_client_.GetRemoteAssociatedInterfaces());
    web_remote_frame_ = frame_test_helpers::CreateRemote(&remote_frame_client_);
  }

  ~WebRemoteFrameVisibilityChangeTest() override = default;

  void ExecuteScriptOnMainFrame(const WebScriptSource& script) {
    MainFrame()->ExecuteScript(script);
    web_view_helper_.GetWebView()
        ->MainFrameViewWidget()
        ->SynchronouslyCompositeForTesting(base::TimeTicks::Now());
    RunPendingTasks();
  }

  void SwapLocalFrameToRemoteFrame() {
    MainFrame()->LastChild()->Swap(RemoteFrame());
  }

  WebLocalFrame* MainFrame() { return frame_; }
  WebRemoteFrameImpl* RemoteFrame() { return web_remote_frame_; }
  TestRemoteFrameHostForVisibility* RemoteFrameHost() {
    return &remote_frame_host_;
  }

 private:
  TestRemoteFrameHostForVisibility remote_frame_host_;
  frame_test_helpers::TestWebRemoteFrameClient remote_frame_client_;
  frame_test_helpers::WebViewHelper web_view_helper_;
  WebLocalFrame* frame_;
  Persistent<WebRemoteFrameImpl> web_remote_frame_;
};

TEST_F(WebRemoteFrameVisibilityChangeTest, FrameVisibilityChange) {
  SwapLocalFrameToRemoteFrame();
  ExecuteScriptOnMainFrame(WebScriptSource(
      "document.querySelector('iframe').style.display = 'none';"));
  EXPECT_EQ(blink::mojom::FrameVisibility::kNotRendered,
            RemoteFrameHost()->visibility());

  ExecuteScriptOnMainFrame(WebScriptSource(
      "document.querySelector('iframe').style.display = 'block';"));
  EXPECT_EQ(blink::mojom::FrameVisibility::kRenderedInViewport,
            RemoteFrameHost()->visibility());

  ExecuteScriptOnMainFrame(WebScriptSource(
      "var padding = document.createElement('div');"
      "padding.style = 'width: 400px; height: 800px;';"
      "document.body.insertBefore(padding, document.body.firstChild);"));
  EXPECT_EQ(blink::mojom::FrameVisibility::kRenderedOutOfViewport,
            RemoteFrameHost()->visibility());

  ExecuteScriptOnMainFrame(
      WebScriptSource("document.scrollingElement.scrollTop = 800;"));
  EXPECT_EQ(blink::mojom::FrameVisibility::kRenderedInViewport,
            RemoteFrameHost()->visibility());
}

TEST_F(WebRemoteFrameVisibilityChangeTest, ParentVisibilityChange) {
  SwapLocalFrameToRemoteFrame();
  ExecuteScriptOnMainFrame(
      WebScriptSource("document.querySelector('iframe').parentElement.style."
                      "display = 'none';"));
  EXPECT_EQ(blink::mojom::FrameVisibility::kNotRendered,
            RemoteFrameHost()->visibility());
}

class TestLocalFrameHostForVisibility : public FakeLocalFrameHost {
 public:
  TestLocalFrameHostForVisibility() = default;
  ~TestLocalFrameHostForVisibility() override = default;

  // FakeLocalFrameHost:
  void VisibilityChanged(blink::mojom::FrameVisibility visibility) override {
    visibility_ = visibility;
  }

  blink::mojom::FrameVisibility visibility() const { return visibility_; }

 private:
  blink::mojom::FrameVisibility visibility_ =
      blink::mojom::FrameVisibility::kRenderedInViewport;
};

class WebLocalFrameVisibilityChangeTest
    : public WebFrameTest,
      public frame_test_helpers::TestWebFrameClient {
 public:
  WebLocalFrameVisibilityChangeTest() {
    RegisterMockedHttpURLLoad("visible_iframe.html");
    RegisterMockedHttpURLLoad("single_iframe.html");
    child_host_.Init(child_client_.GetRemoteNavigationAssociatedInterfaces());
    frame_ = web_view_helper_
                 .InitializeAndLoad(base_url_ + "single_iframe.html", this)
                 ->MainFrameImpl();
    web_view_helper_.Resize(gfx::Size(640, 480));
  }

  ~WebLocalFrameVisibilityChangeTest() override = default;

  void ExecuteScriptOnMainFrame(const WebScriptSource& script) {
    MainFrame()->ExecuteScript(script);
    web_view_helper_.GetWebView()
        ->MainFrameViewWidget()
        ->SynchronouslyCompositeForTesting(base::TimeTicks::Now());
    RunPendingTasks();
  }

  WebLocalFrame* MainFrame() { return frame_; }

  // frame_test_helpers::TestWebFrameClient:
  WebLocalFrame* CreateChildFrame(
      WebLocalFrame* parent,
      mojom::blink::TreeScopeType scope,
      const WebString& name,
      const WebString& fallback_name,
      const FramePolicy&,
      const WebFrameOwnerProperties&,
      mojom::blink::FrameOwnerElementType) override {
    return CreateLocalChild(*parent, scope, &child_client_);
  }

  TestLocalFrameHostForVisibility& ChildHost() { return child_host_; }

 private:
  TestLocalFrameHostForVisibility child_host_;
  frame_test_helpers::TestWebFrameClient child_client_;
  frame_test_helpers::WebViewHelper web_view_helper_;
  WebLocalFrame* frame_;
};

TEST_F(WebLocalFrameVisibilityChangeTest, FrameVisibilityChange) {
  ExecuteScriptOnMainFrame(WebScriptSource(
      "document.querySelector('iframe').style.display = 'none';"));
  EXPECT_EQ(blink::mojom::FrameVisibility::kNotRendered,
            ChildHost().visibility());

  ExecuteScriptOnMainFrame(WebScriptSource(
      "document.querySelector('iframe').style.display = 'block';"));
  EXPECT_EQ(blink::mojom::FrameVisibility::kRenderedInViewport,
            ChildHost().visibility());

  ExecuteScriptOnMainFrame(WebScriptSource(
      "var padding = document.createElement('div');"
      "padding.style = 'width: 400px; height: 800px;';"
      "document.body.insertBefore(padding, document.body.firstChild);"));
  EXPECT_EQ(blink::mojom::FrameVisibility::kRenderedOutOfViewport,
            ChildHost().visibility());

  ExecuteScriptOnMainFrame(
      WebScriptSource("document.scrollingElement.scrollTop = 800;"));
  EXPECT_EQ(blink::mojom::FrameVisibility::kRenderedInViewport,
            ChildHost().visibility());
}

TEST_F(WebLocalFrameVisibilityChangeTest, ParentVisibilityChange) {
  ExecuteScriptOnMainFrame(
      WebScriptSource("document.querySelector('iframe').parentElement.style."
                      "display = 'none';"));
  EXPECT_EQ(blink::mojom::FrameVisibility::kNotRendered,
            ChildHost().visibility());
}

static void EnableGlobalReuseForUnownedMainFrames(WebSettings* settings) {
  settings->SetShouldReuseGlobalForUnownedMainFrame(true);
}

// A main frame with no opener should have a unique security origin. Thus, the
// global should never be reused on the initial navigation.
TEST(WebFrameGlobalReuseTest, MainFrameWithNoOpener) {
  frame_test_helpers::WebViewHelper helper;
  helper.Initialize();

  WebLocalFrame* main_frame = helper.LocalMainFrame();
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  main_frame->ExecuteScript(WebScriptSource("hello = 'world';"));
  frame_test_helpers::LoadFrame(main_frame, "data:text/html,new page");
  v8::Local<v8::Value> result =
      main_frame->ExecuteScriptAndReturnValue(WebScriptSource("hello"));
  EXPECT_TRUE(result.IsEmpty());
}

// Child frames should never reuse the global on a cross-origin navigation, even
// if the setting is enabled. It's not safe to since the parent could have
// injected script before the initial navigation.
TEST(WebFrameGlobalReuseTest, ChildFrame) {
  frame_test_helpers::WebViewHelper helper;
  helper.Initialize(nullptr, nullptr, nullptr,
                    EnableGlobalReuseForUnownedMainFrames);

  WebLocalFrame* main_frame = helper.LocalMainFrame();
  frame_test_helpers::LoadFrame(main_frame, "data:text/html,<iframe></iframe>");

  WebLocalFrame* child_frame = main_frame->FirstChild()->ToWebLocalFrame();
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  child_frame->ExecuteScript(WebScriptSource("hello = 'world';"));
  frame_test_helpers::LoadFrame(child_frame, "data:text/html,new page");
  v8::Local<v8::Value> result =
      child_frame->ExecuteScriptAndReturnValue(WebScriptSource("hello"));
  EXPECT_TRUE(result.IsEmpty());
}

// A main frame with an opener should never reuse the global on a cross-origin
// navigation, even if the setting is enabled. It's not safe to since the opener
// could have injected script.
TEST(WebFrameGlobalReuseTest, MainFrameWithOpener) {
  frame_test_helpers::WebViewHelper opener_helper;
  opener_helper.Initialize();
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeWithOpener(opener_helper.GetWebView()->MainFrame(), nullptr,
                              nullptr, nullptr,
                              EnableGlobalReuseForUnownedMainFrames);

  WebLocalFrame* main_frame = helper.LocalMainFrame();
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  main_frame->ExecuteScript(WebScriptSource("hello = 'world';"));
  frame_test_helpers::LoadFrame(main_frame, "data:text/html,new page");
  v8::Local<v8::Value> result =
      main_frame->ExecuteScriptAndReturnValue(WebScriptSource("hello"));
  EXPECT_TRUE(result.IsEmpty());
}

// A main frame that is unrelated to any other frame /can/ reuse the global if
// the setting is enabled. In this case, it's impossible for any other frames to
// have touched the global. Only the embedder could have injected script, and
// the embedder enabling this setting is a signal that the injected script needs
// to persist on the first navigation away from the initial empty document.
TEST(WebFrameGlobalReuseTest, ReuseForMainFrameIfEnabled) {
  frame_test_helpers::WebViewHelper helper;
  helper.Initialize(nullptr, nullptr, nullptr,
                    EnableGlobalReuseForUnownedMainFrames);

  WebLocalFrame* main_frame = helper.LocalMainFrame();
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  main_frame->ExecuteScript(WebScriptSource("hello = 'world';"));
  frame_test_helpers::LoadFrame(main_frame, "data:text/html,new page");
  v8::Local<v8::Value> result =
      main_frame->ExecuteScriptAndReturnValue(WebScriptSource("hello"));
  ASSERT_TRUE(result->IsString());
  EXPECT_EQ("world",
            ToCoreString(result->ToString(main_frame->MainWorldScriptContext())
                             .ToLocalChecked()));
}

// This class intercepts the registration of Blob instances.
//
// Given that the content of the Blob is known (data URL)
// it gets the data from the DataElement's BytesProvider, and creates
// FakeBlob's accordingly.
class BlobRegistryForSaveImageFromDataURL : public mojom::blink::BlobRegistry {
 public:
  void Register(mojo::PendingReceiver<mojom::blink::Blob> blob,
                const String& uuid,
                const String& content_type,
                const String& content_disposition,
                Vector<mojom::blink::DataElementPtr> elements,
                RegisterCallback callback) override {
    DCHECK_EQ(elements.size(), 1u);
    DCHECK(elements[0]->is_bytes());

    auto& element0 = elements[0];
    const auto& bytes = element0->get_bytes();
    auto length = bytes->length;
    String body(reinterpret_cast<const char*>(bytes->embedded_data->data()),
                static_cast<uint32_t>(length));
    mojo::MakeSelfOwnedReceiver(std::make_unique<FakeBlob>(uuid, body),
                                std::move(blob));
    std::move(callback).Run();
  }

  void RegisterFromStream(
      const String& content_type,
      const String& content_disposition,
      uint64_t expected_length,
      mojo::ScopedDataPipeConsumerHandle,
      mojo::PendingAssociatedRemote<mojom::blink::ProgressClient>,
      RegisterFromStreamCallback) override {
    NOTREACHED();
  }

  void GetBlobFromUUID(mojo::PendingReceiver<mojom::blink::Blob>,
                       const String& uuid,
                       GetBlobFromUUIDCallback) override {
    NOTREACHED();
  }

  void URLStoreForOrigin(
      const scoped_refptr<const SecurityOrigin>&,
      mojo::PendingAssociatedReceiver<mojom::blink::BlobURLStore>) override {
    NOTREACHED();
  }
};

// blink::mojom::LocalFrameHost instance that intecepts DownloadURL() mojo
// calls and reads the blob data URL sent by the renderer accordingly.
class TestLocalFrameHostForSaveImageFromDataURL : public FakeLocalFrameHost {
 public:
  TestLocalFrameHostForSaveImageFromDataURL()
      : blob_registry_receiver_(
            &blob_registry_,
            blob_registry_remote_.BindNewPipeAndPassReceiver()) {
    BlobDataHandle::SetBlobRegistryForTesting(blob_registry_remote_.get());
  }
  ~TestLocalFrameHostForSaveImageFromDataURL() override {
    BlobDataHandle::SetBlobRegistryForTesting(nullptr);
  }

  // FakeLocalFrameHost:
  void DownloadURL(mojom::blink::DownloadURLParamsPtr params) override {
    mojo::Remote<mojom::blink::Blob> blob(std::move(params->data_url_blob));
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    auto result =
        mojo::CreateDataPipe(nullptr, &producer_handle, &consumer_handle);
    DCHECK(result == MOJO_RESULT_OK);

    blob->ReadAll(std::move(producer_handle), mojo::NullRemote());

    DataPipeDrainerClient client(&data_url_);
    auto data_pipe_drainer = std::make_unique<mojo::DataPipeDrainer>(
        &client, std::move(consumer_handle));
    client.Run();
  }

  const String& Result() const { return data_url_; }
  void Reset() { data_url_ = String(); }

 private:
  // Helper class to copy a blob to a string.
  class DataPipeDrainerClient : public mojo::DataPipeDrainer::Client {
   public:
    explicit DataPipeDrainerClient(String* output)
        : run_loop_(base::RunLoop::Type::kNestableTasksAllowed),
          output_(output) {}
    void Run() { run_loop_.Run(); }

    void OnDataAvailable(const void* data, size_t num_bytes) override {
      *output_ = String(reinterpret_cast<const char*>(data), num_bytes);
    }
    void OnDataComplete() override { run_loop_.Quit(); }

   private:
    base::RunLoop run_loop_;
    String* output_;
  };

  BlobRegistryForSaveImageFromDataURL blob_registry_;
  mojo::Remote<mojom::blink::BlobRegistry> blob_registry_remote_;
  mojo::Receiver<mojom::blink::BlobRegistry> blob_registry_receiver_;

  // Data URL retrieved from the blob.
  String data_url_;
};

TEST_F(WebFrameTest, SaveImageAt) {
  std::string url = base_url_ + "image-with-data-url.html";
  // TODO(crbug.com/751425): We should use the mock functionality
  // via the WebViewHelper instance in each test case.
  RegisterMockedURLLoadFromBase(base_url_, "image-with-data-url.html");
  url_test_helpers::RegisterMockedURLLoad(
      ToKURL("http://test"), test::CoreTestDataPath("white-1x1.png"));

  TestLocalFrameHostForSaveImageFromDataURL frame_host;
  frame_test_helpers::TestWebFrameClient web_frame_client;
  frame_host.Init(web_frame_client.GetRemoteNavigationAssociatedInterfaces());
  frame_test_helpers::WebViewHelper web_view_helper;
  RunPendingTasks();

  WebViewImpl* web_view =
      web_view_helper.InitializeAndLoad(url, &web_frame_client);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  UpdateAllLifecyclePhases(web_view);

  LocalFrame* local_frame = To<LocalFrame>(web_view->GetPage()->MainFrame());

  frame_host.Reset();
  local_frame->SaveImageAt(gfx::Point(1, 1));
  // Note that in this test does not use RunPendingTasks() since
  // TestLocalFrameHostForSaveImageFromDataURL trigger its own loops, so nesting
  // must be allowed.
  base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();

  EXPECT_EQ(
      String::FromUTF8("data:image/gif;base64"
                       ",R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs="),
      frame_host.Result());

  frame_host.Reset();

  local_frame->SaveImageAt(gfx::Point(1, 2));
  base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
  EXPECT_EQ(String(), frame_host.Result());

  web_view->SetPageScaleFactor(4);
  web_view->SetVisualViewportOffset(gfx::PointF(1, 1));

  frame_host.Reset();
  local_frame->SaveImageAt(gfx::Point(3, 3));
  base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
  EXPECT_EQ(
      String::FromUTF8("data:image/gif;base64"
                       ",R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs="),
      frame_host.Result());

  // Explicitly reset to break dependency on locally scoped client.
  web_view_helper.Reset();
}

TEST_F(WebFrameTest, SaveImageWithImageMap) {
  std::string url = base_url_ + "image-map.html";
  // TODO(crbug.com/751425): We should use the mock functionality
  // via the WebViewHelper instance in each test case.
  RegisterMockedURLLoadFromBase(base_url_, "image-map.html");

  TestLocalFrameHostForSaveImageFromDataURL frame_host;
  frame_test_helpers::WebViewHelper helper;
  frame_test_helpers::TestWebFrameClient client;
  frame_host.Init(client.GetRemoteNavigationAssociatedInterfaces());
  WebViewImpl* web_view = helper.InitializeAndLoad(url, &client);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  RunPendingTasks();

  LocalFrame* local_frame = To<LocalFrame>(web_view->GetPage()->MainFrame());

  frame_host.Reset();
  local_frame->SaveImageAt(gfx::Point(25, 25));
  base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
  EXPECT_EQ(
      String::FromUTF8("data:image/gif;base64"
                       ",R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs="),
      frame_host.Result());

  frame_host.Reset();
  local_frame->SaveImageAt(gfx::Point(75, 25));
  base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
  EXPECT_EQ(
      String::FromUTF8("data:image/gif;base64"
                       ",R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs="),
      frame_host.Result());

  frame_host.Reset();
  local_frame->SaveImageAt(gfx::Point(125, 25));
  base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
  EXPECT_EQ(String(), frame_host.Result());

  // Explicitly reset to break dependency on locally scoped client.
  helper.Reset();
}

TEST_F(WebFrameTest, CopyImageWithImageMap) {
  std::string url = base_url_ + "image-map.html";
  // TODO(crbug.com/751425): We should use the mock functionality
  // via the WebViewHelper instance in each test case.
  RegisterMockedURLLoadFromBase(base_url_, "image-map.html");

  TestLocalFrameHostForSaveImageFromDataURL frame_host;
  frame_test_helpers::WebViewHelper helper;
  frame_test_helpers::TestWebFrameClient client;
  frame_host.Init(client.GetRemoteNavigationAssociatedInterfaces());
  WebViewImpl* web_view = helper.InitializeAndLoad(url, &client);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  RunPendingTasks();

  frame_host.Reset();
  LocalFrame* local_frame = To<LocalFrame>(web_view->GetPage()->MainFrame());
  local_frame->SaveImageAt(gfx::Point(25, 25));
  base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
  EXPECT_EQ(
      String::FromUTF8("data:image/gif;base64"
                       ",R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs="),
      frame_host.Result());

  frame_host.Reset();
  local_frame->SaveImageAt(gfx::Point(75, 25));
  base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
  EXPECT_EQ(
      String::FromUTF8("data:image/gif;base64"
                       ",R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs="),
      frame_host.Result());

  frame_host.Reset();
  local_frame->SaveImageAt(gfx::Point(125, 25));
  base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
  EXPECT_EQ(String(), frame_host.Result());
  // Explicitly reset to break dependency on locally scoped client.
  helper.Reset();
}

TEST_F(WebFrameTest, LoadJavascriptURLInNewFrame) {
  frame_test_helpers::WebViewHelper helper;
  helper.Initialize();

  std::string redirect_url = base_url_ + "foo.html";
  KURL javascript_url = ToKURL("javascript:location='" + redirect_url + "'");
  url_test_helpers::RegisterMockedURLLoad(ToKURL(redirect_url),
                                          test::CoreTestDataPath("foo.html"));
  helper.LocalMainFrame()->LoadJavaScriptURL(javascript_url);
  RunPendingTasks();

  // The result of the JS url replaces the existing contents on the
  // Document, but the JS-triggered navigation should still occur.
  EXPECT_NE("", To<LocalFrame>(helper.GetWebView()->GetPage()->MainFrame())
                    ->GetDocument()
                    ->documentElement()
                    ->innerText());
  EXPECT_EQ(ToKURL(redirect_url),
            To<LocalFrame>(helper.GetWebView()->GetPage()->MainFrame())
                ->GetDocument()
                ->Url());
}

TEST_F(WebFrameTest, EmptyJavascriptFrameUrl) {
  std::string url = "data:text/html,<iframe src=\"javascript:''\"></iframe>";
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeAndLoad(url);
  RunPendingTasks();

  LocalFrame* child = To<LocalFrame>(
      helper.GetWebView()->GetPage()->MainFrame()->Tree().FirstChild());
  EXPECT_EQ(BlankURL(), child->GetDocument()->Url());
  EXPECT_EQ(BlankURL(), child->Loader().GetDocumentLoader()->Url());
}

class TestResourcePriorityWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  class ExpectedRequest {
   public:
    ExpectedRequest(const KURL& url, WebURLRequest::Priority priority)
        : url(url), priority(priority), seen(false) {}

    KURL url;
    WebURLRequest::Priority priority;
    bool seen;
  };

  TestResourcePriorityWebFrameClient() = default;
  ~TestResourcePriorityWebFrameClient() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void WillSendRequest(WebURLRequest& request,
                       ForRedirect for_redirect) override {
    ExpectedRequest* expected_request = expected_requests_.at(request.Url());
    DCHECK(expected_request);
    EXPECT_EQ(expected_request->priority, request.GetPriority());
    expected_request->seen = true;
  }

  void AddExpectedRequest(const KURL& url, WebURLRequest::Priority priority) {
    expected_requests_.insert(url,
                              std::make_unique<ExpectedRequest>(url, priority));
  }

  void VerifyAllRequests() {
    for (const auto& request : expected_requests_)
      EXPECT_TRUE(request.value->seen);
  }

 private:
  HashMap<KURL, std::unique_ptr<ExpectedRequest>> expected_requests_;
};

TEST_F(WebFrameTest, ChangeResourcePriority) {
  TestResourcePriorityWebFrameClient client;
  RegisterMockedHttpURLLoad("promote_img_in_viewport_priority.html");
  RegisterMockedHttpURLLoad("image_slow.pl");
  RegisterMockedHttpURLLoad("image_slow_out_of_viewport.pl");
  client.AddExpectedRequest(ToKURL("http://internal.test/image_slow.pl"),
                            WebURLRequest::Priority::kLow);
  client.AddExpectedRequest(
      ToKURL("http://internal.test/image_slow_out_of_viewport.pl"),
      WebURLRequest::Priority::kLow);

  frame_test_helpers::WebViewHelper helper;
  helper.Initialize(&client);
  helper.Resize(gfx::Size(640, 480));
  frame_test_helpers::LoadFrame(
      helper.GetWebView()->MainFrameImpl(),
      base_url_ + "promote_img_in_viewport_priority.html");

  // Ensure the image in the viewport got promoted after the request was sent.
  Resource* image = To<WebLocalFrameImpl>(helper.GetWebView()->MainFrame())
                        ->GetFrame()
                        ->GetDocument()
                        ->Fetcher()
                        ->AllResources()
                        .at(ToKURL("http://internal.test/image_slow.pl"));
  DCHECK(image);
  EXPECT_EQ(ResourceLoadPriority::kHigh,
            image->GetResourceRequest().Priority());

  client.VerifyAllRequests();
}

TEST_F(WebFrameTest, ScriptPriority) {
  TestResourcePriorityWebFrameClient client;
  RegisterMockedHttpURLLoad("script_priority.html");
  RegisterMockedHttpURLLoad("priorities/defer.js");
  RegisterMockedHttpURLLoad("priorities/async.js");
  RegisterMockedHttpURLLoad("priorities/head.js");
  RegisterMockedHttpURLLoad("priorities/document-write.js");
  RegisterMockedHttpURLLoad("priorities/injected.js");
  RegisterMockedHttpURLLoad("priorities/injected-async.js");
  RegisterMockedHttpURLLoad("priorities/body.js");
  client.AddExpectedRequest(ToKURL("http://internal.test/priorities/defer.js"),
                            WebURLRequest::Priority::kLow);
  client.AddExpectedRequest(ToKURL("http://internal.test/priorities/async.js"),
                            WebURLRequest::Priority::kLow);
  client.AddExpectedRequest(ToKURL("http://internal.test/priorities/head.js"),
                            WebURLRequest::Priority::kHigh);
  client.AddExpectedRequest(
      ToKURL("http://internal.test/priorities/document-write.js"),
      WebURLRequest::Priority::kHigh);
  client.AddExpectedRequest(
      ToKURL("http://internal.test/priorities/injected.js"),
      WebURLRequest::Priority::kLow);
  client.AddExpectedRequest(
      ToKURL("http://internal.test/priorities/injected-async.js"),
      WebURLRequest::Priority::kLow);
  client.AddExpectedRequest(ToKURL("http://internal.test/priorities/body.js"),
                            WebURLRequest::Priority::kHigh);

  frame_test_helpers::WebViewHelper helper;
  helper.InitializeAndLoad(base_url_ + "script_priority.html", &client);
  client.VerifyAllRequests();
}

class MultipleDataChunkDelegate : public WebURLLoaderTestDelegate {
 public:
  MultipleDataChunkDelegate() = default;
  ~MultipleDataChunkDelegate() override = default;

  // WebURLLoaderTestDelegate:
  void DidReceiveData(WebURLLoaderClient* original_client,
                      const char* data,
                      int data_length) override {
    EXPECT_GT(data_length, 16);
    original_client->DidReceiveData(data, 16);
    // This didReceiveData call shouldn't crash due to a failed assertion.
    original_client->DidReceiveData(data + 16, data_length - 16);
  }
};

TEST_F(WebFrameTest, ImageDocumentDecodeError) {
  std::string url = base_url_ + "not_an_image.ico";
  url_test_helpers::RegisterMockedURLLoad(
      ToKURL(url), test::CoreTestDataPath("not_an_image.ico"), "image/x-icon");
  MultipleDataChunkDelegate delegate;
  url_test_helpers::SetLoaderDelegate(&delegate);
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeAndLoad(url);
  url_test_helpers::SetLoaderDelegate(nullptr);

  Document* document =
      To<LocalFrame>(helper.GetWebView()->GetPage()->MainFrame())
          ->GetDocument();
  EXPECT_TRUE(IsA<ImageDocument>(document));
  EXPECT_EQ(ResourceStatus::kDecodeError,
            To<ImageDocument>(document)->CachedImage()->GetContentStatus());
}

// Ensure that the root layer -- whose size is ordinarily derived from the
// content size -- maintains a minimum height matching the viewport in cases
// where the content is smaller.
TEST_F(WebFrameTest, RootLayerMinimumHeight) {
  constexpr int kViewportWidth = 320;
  constexpr int kViewportHeight = 640;
  constexpr int kBrowserControlsHeight = 100;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr, nullptr, ConfigureAndroid);
  WebViewImpl* web_view = web_view_helper.GetWebView();
  web_view->ResizeWithBrowserControls(
      gfx::Size(kViewportWidth, kViewportHeight - kBrowserControlsHeight),
      kBrowserControlsHeight, 0, true);

  InitializeWithHTML(
      *web_view->MainFrameImpl()->GetFrame(),
      "<!DOCTYPE html>"
      "<meta name='viewport' content='width=device-width, initial-scale=1'>"
      "<style>"
      "  html, body {width:100%;height:540px;margin:0px}"
      "  #elem {"
      "    overflow: scroll;"
      "    width: 100px;"
      "    height: 10px;"
      "    position: fixed;"
      "    left: 0px;"
      "    bottom: 0px;"
      "  }"
      "</style>"
      "<div id='elem'></div>");
  UpdateAllLifecyclePhases(web_view);

  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();
  LocalFrameView* frame_view = web_view->MainFrameImpl()->GetFrameView();
  PaintLayerCompositor* compositor = frame_view->GetLayoutView()->Compositor();

  EXPECT_EQ(kViewportHeight - kBrowserControlsHeight,
            compositor->RootLayer()->BoundingBoxForCompositing().Height());

  document->View()->SetTracksRasterInvalidations(true);

  web_view->ResizeWithBrowserControls(
      gfx::Size(kViewportWidth, kViewportHeight), kBrowserControlsHeight, 0,
      false);

  EXPECT_EQ(kViewportHeight,
            compositor->RootLayer()->BoundingBoxForCompositing().Height());
  EXPECT_EQ(kViewportHeight, compositor->RootGraphicsLayer()->Size().height());
  EXPECT_EQ(kViewportHeight, compositor->RootGraphicsLayer()->Size().height());

  const RasterInvalidationTracking* invalidation_tracking =
      document->GetLayoutView()
          ->Layer()
          ->GetCompositedLayerMapping()
          ->MainGraphicsLayer()
          ->GetRasterInvalidationTracking();
  ASSERT_TRUE(invalidation_tracking);
  const auto& raster_invalidations = invalidation_tracking->Invalidations();

  // We don't issue raster invalidation, because the content paints into the
  // scrolling contents layer whose size hasn't changed.
  EXPECT_TRUE(raster_invalidations.IsEmpty());

  document->View()->SetTracksRasterInvalidations(false);
}

// Load a page with display:none set and try to scroll it. It shouldn't crash
// due to lack of layoutObject. crbug.com/653327.
TEST_F(WebFrameTest, ScrollBeforeLayoutDoesntCrash) {
  RegisterMockedHttpURLLoad("display-none.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "display-none.html");
  WebViewImpl* web_view = web_view_helper.GetWebView();
  web_view_helper.Resize(gfx::Size(640, 480));

  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();
  document->documentElement()->SetLayoutObject(nullptr);

  WebGestureEvent begin_event(
      WebInputEvent::Type::kGestureScrollBegin, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(), WebGestureDevice::kTouchpad);
  WebGestureEvent update_event(
      WebInputEvent::Type::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(), WebGestureDevice::kTouchpad);
  WebGestureEvent end_event(
      WebInputEvent::Type::kGestureScrollEnd, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(), WebGestureDevice::kTouchpad);

  // Try GestureScrollEnd and GestureScrollUpdate first to make sure that not
  // seeing a Begin first doesn't break anything. (This currently happens).
  web_view_helper.GetWebView()->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(end_event, ui::LatencyInfo()));
  web_view_helper.GetWebView()->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(update_event, ui::LatencyInfo()));

  // Try a full Begin/Update/End cycle.
  web_view_helper.GetWebView()->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(begin_event, ui::LatencyInfo()));
  web_view_helper.GetWebView()->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(update_event, ui::LatencyInfo()));
  web_view_helper.GetWebView()->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(end_event, ui::LatencyInfo()));
}

TEST_F(WebFrameTest, MouseOverDifferntNodeClearsTooltip) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();
  web_view_helper.Resize(gfx::Size(200, 200));
  WebViewImpl* web_view = web_view_helper.GetWebView();

  InitializeWithHTML(
      *web_view->MainFrameImpl()->GetFrame(),
      "<head>"
      "  <style type='text/css'>"
      "   div"
      "    {"
      "      width: 200px;"
      "      height: 100px;"
      "      background-color: #eeeeff;"
      "    }"
      "    div:hover"
      "    {"
      "      background-color: #ddddff;"
      "    }"
      "  </style>"
      "</head>"
      "<body>"
      "  <div id='div1' title='Title Attribute Value'>Hover HERE</div>"
      "  <div id='div2' title='Title Attribute Value'>Then HERE</div>"
      "  <br><br><br>"
      "</body>");
  UpdateAllLifecyclePhases(web_view);

  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();
  Element* div1_tag = document->getElementById("div1");

  HitTestResult hit_test_result = web_view->CoreHitTestResultAt(
      gfx::PointF(div1_tag->OffsetLeft() + 5, div1_tag->OffsetTop() + 5));

  EXPECT_TRUE(hit_test_result.InnerElement());

  // Mouse over link. Mouse cursor should be hand.
  WebMouseEvent mouse_move_over_link_event(
      WebInputEvent::Type::kMouseMove,
      gfx::PointF(div1_tag->OffsetLeft() + 5, div1_tag->OffsetTop() + 5),
      gfx::PointF(div1_tag->OffsetLeft() + 5, div1_tag->OffsetTop() + 5),
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      base::TimeTicks::Now());
  mouse_move_over_link_event.SetFrameScale(1);
  document->GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_over_link_event, Vector<WebMouseEvent>(),
      Vector<WebMouseEvent>());

  EXPECT_EQ(
      document->HoverElement(),
      document->GetFrame()->GetChromeClient().LastSetTooltipNodeForTesting());
  EXPECT_EQ(
      div1_tag,
      document->GetFrame()->GetChromeClient().LastSetTooltipNodeForTesting());

  Element* div2_tag = document->getElementById("div2");

  WebMouseEvent mouse_move_event(
      WebInputEvent::Type::kMouseMove,
      gfx::PointF(div2_tag->OffsetLeft() + 5, div2_tag->OffsetTop() + 5),
      gfx::PointF(div2_tag->OffsetLeft() + 5, div2_tag->OffsetTop() + 5),
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      base::TimeTicks::Now());
  mouse_move_event.SetFrameScale(1);
  document->GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());

  EXPECT_EQ(
      document->HoverElement(),
      document->GetFrame()->GetChromeClient().LastSetTooltipNodeForTesting());
  EXPECT_EQ(
      div2_tag,
      document->GetFrame()->GetChromeClient().LastSetTooltipNodeForTesting());
}

class WebFrameSimTest : public SimTest {
 public:
  void UseAndroidSettings() {
    WebView().GetPage()->GetSettings().SetViewportMetaEnabled(true);
    WebView().GetPage()->GetSettings().SetViewportEnabled(true);
    WebView().GetPage()->GetSettings().SetMainFrameResizesAreOrientationChanges(
        true);
    WebView().GetPage()->GetSettings().SetViewportStyle(
        mojom::blink::ViewportStyle::kMobile);
    WebView().GetSettings()->SetAutoZoomFocusedNodeToLegibleScale(true);
    WebView().GetSettings()->SetShrinksViewportContentToFit(true);
    WebView().SetDefaultPageScaleLimits(0.25f, 5);
  }
};

TEST_F(WebFrameSimTest, HitTestWithIgnoreClippingAtNegativeOffset) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  WebView().GetPage()->GetSettings().SetTextAutosizingEnabled(false);

  SimRequest r("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  r.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        body, html {
          width: 100%;
          height: 1000px;
          margin: 0;
        }
        #top {
          position: absolute;
          top: 500px;
          height: 100px;
          width: 100%;

        }
        #bottom {
          position: absolute;
          top: 600px;
          width: 100%;
          height: 500px;
        }
      </style>
      <div id="top"></div>
      <div id="bottom"></div>
  )HTML");

  Compositor().BeginFrame();

  auto* frame_view = To<LocalFrame>(WebView().GetPage()->MainFrame())->View();

  frame_view->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 600), mojom::blink::ScrollType::kProgrammatic);
  Compositor().BeginFrame();

  HitTestRequest request = HitTestRequest::kMove | HitTestRequest::kReadOnly |
                           HitTestRequest::kActive |
                           HitTestRequest::kIgnoreClipping;
  HitTestLocation location(
      frame_view->ConvertFromRootFrame(PhysicalOffset(100, -50)));
  HitTestResult result(request, location);
  frame_view->GetLayoutView()->HitTest(location, result);

  EXPECT_EQ(GetDocument().getElementById("top"), result.InnerNode());
}

TEST_F(WebFrameSimTest, TickmarksDocumentRelative) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  WebView().GetPage()->GetSettings().SetTextAutosizingEnabled(false);

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        body, html {
          width: 4000px;
          height: 4000px;
          margin: 0;
        }
        div {
          position: absolute;
          left: 800px;
          top: 2000px;
        }
      </style>
      <div>test</div>
  )HTML");

  Compositor().BeginFrame();

  auto* frame = To<WebLocalFrameImpl>(WebView().MainFrame());
  auto* frame_view = To<LocalFrame>(WebView().GetPage()->MainFrame())->View();

  frame_view->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(3000, 1000), mojom::blink::ScrollType::kProgrammatic);
  auto options = mojom::blink::FindOptions::New();
  options->run_synchronously_for_testing = true;
  WebString search_text = WebString::FromUTF8("test");
  const int kFindIdentifier = 12345;
  EXPECT_TRUE(frame->GetFindInPage()->FindInternal(kFindIdentifier, search_text,
                                                   *options, false));

  frame->EnsureTextFinder().ResetMatchCount();
  frame->EnsureTextFinder().StartScopingStringMatches(kFindIdentifier,
                                                      search_text, *options);

  // Get the tickmarks for the original find request.
  Vector<IntRect> original_tickmarks =
      frame_view->LayoutViewport()->GetTickmarks();
  EXPECT_EQ(1u, original_tickmarks.size());

  EXPECT_EQ(IntPoint(800, 2000), original_tickmarks[0].Location());
}

TEST_F(WebFrameSimTest, FindInPageSelectNextMatch) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  WebView().GetPage()->GetSettings().SetTextAutosizingEnabled(false);

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        body, html {
          width: 4000px;
          height: 4000px;
          margin: 0;
        }
        #box1 {
          position: absolute;
          left: 800px;
          top: 2000px;
        }

        #box2 {
          position: absolute;
          left: 1000px;
          top: 3000px;
        }
      </style>
      <div id="box1">test</div>
      <div id="box2">test</div>
  )HTML");

  Compositor().BeginFrame();

  auto* frame = To<WebLocalFrameImpl>(WebView().MainFrame());
  auto* local_frame = To<LocalFrame>(WebView().GetPage()->MainFrame());
  auto* frame_view = local_frame->View();

  Element* box1 = GetDocument().getElementById("box1");
  Element* box2 = GetDocument().getElementById("box2");

  IntRect box1_rect = box1->GetLayoutObject()->AbsoluteBoundingBoxRect();
  IntRect box2_rect = box2->GetLayoutObject()->AbsoluteBoundingBoxRect();

  frame_view->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(3000, 1000), mojom::blink::ScrollType::kProgrammatic);
  auto options = mojom::blink::FindOptions::New();
  options->run_synchronously_for_testing = true;
  WebString search_text = WebString::FromUTF8("test");
  const int kFindIdentifier = 12345;
  EXPECT_TRUE(frame->GetFindInPage()->FindInternal(kFindIdentifier, search_text,
                                                   *options, false));

  frame->EnsureTextFinder().ResetMatchCount();
  frame->EnsureTextFinder().StartScopingStringMatches(kFindIdentifier,
                                                      search_text, *options);

  WebVector<gfx::RectF> web_match_rects =
      frame->EnsureTextFinder().FindMatchRects();
  ASSERT_EQ(2ul, web_match_rects.size());

  FloatRect result_rect = static_cast<FloatRect>(web_match_rects[0]);
  frame->EnsureTextFinder().SelectNearestFindMatch(result_rect.Center(),
                                                   nullptr);

  EXPECT_TRUE(frame_view->GetScrollableArea()->VisibleContentRect().Contains(
      box1_rect));
  result_rect = static_cast<FloatRect>(web_match_rects[1]);
  frame->EnsureTextFinder().SelectNearestFindMatch(result_rect.Center(),
                                                   nullptr);

  EXPECT_TRUE(
      frame_view->GetScrollableArea()->VisibleContentRect().Contains(box2_rect))
      << "Box [" << box2_rect.ToString() << "] is not visible in viewport ["
      << frame_view->GetScrollableArea()->VisibleContentRect().ToString()
      << "]";
}

// Test bubbling a document (End key) scroll from an inner iframe. This test
// passes if it does not crash. https://crbug.com/904247.
TEST_F(WebFrameSimTest, ScrollToEndBubblingCrash) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  WebView().GetPage()->GetSettings().SetScrollAnimatorEnabled(false);

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        body, html {
          width: 100%;
          height: 100%;
          margin: 0;
        }
        #frame {
          width: 100%;
          height: 100%;
          border: 0;
        }
      </style>
      <iframe id="frame" srcdoc="
          <!DOCTYPE html>
          <style>html {height: 300%;}</style>
      "></iframe>
  )HTML");

  Compositor().BeginFrame();
  RunPendingTasks();

  // Focus the iframe.
  WebView().AdvanceFocus(false);

  WebKeyboardEvent key_event(WebInputEvent::Type::kRawKeyDown,
                             WebInputEvent::kNoModifiers,
                             WebInputEvent::GetStaticTimeStampForTests());
  key_event.windows_key_code = VKEY_END;

  // Scroll the iframe to the end.
  key_event.SetType(WebInputEvent::Type::kRawKeyDown);
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event, ui::LatencyInfo()));
  key_event.SetType(WebInputEvent::Type::kKeyUp);
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event, ui::LatencyInfo()));

  Compositor().BeginFrame();

  // End key should now bubble from the iframe up to the main viewport.
  key_event.SetType(WebInputEvent::Type::kRawKeyDown);
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event, ui::LatencyInfo()));
  key_event.SetType(WebInputEvent::Type::kKeyUp);
  WebView().MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(key_event, ui::LatencyInfo()));
}

TEST_F(WebFrameSimTest, TestScrollFocusedEditableElementIntoView) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  WebView().SetDefaultPageScaleLimits(1.f, 4);
  WebView().EnableFakePageScaleAnimationForTesting(true);
  WebView().GetPage()->GetSettings().SetTextAutosizingEnabled(false);
  WebView().GetPage()->GetSettings().SetViewportEnabled(false);
  WebView().GetSettings()->SetAutoZoomFocusedNodeToLegibleScale(true);

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        ::-webkit-scrollbar {
          width: 0px;
          height: 0px;
        }
        body {
          margin: 0px;
        }
        input {
          border: 0;
          padding: 0;
          position: absolute;
          left: 200px;
          top: 600px;
          width: 100px;
          height: 20px;
        }
        #content {
          background: silver;
          width: 500px;
          height: 600px;
        }
      </style>
      <div id="content">a</div>
      <input type="text">
  )HTML");

  Compositor().BeginFrame();

  WebView().AdvanceFocus(false);

  auto* frame = To<LocalFrame>(WebView().GetPage()->MainFrame());
  LocalFrameView* frame_view = frame->View();
  IntRect inputRect(200, 600, 100, 20);

  frame_view->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 0), mojom::blink::ScrollType::kProgrammatic);

  ASSERT_EQ(FloatPoint(),
            frame_view->GetScrollableArea()->VisibleContentRect().Location());

  WebView()
      .MainFrameImpl()
      ->FrameWidget()
      ->ScrollFocusedEditableElementIntoView();

  EXPECT_EQ(1, WebView().FakePageScaleAnimationPageScaleForTesting());

  frame_view->LayoutViewport()->SetScrollOffset(
      ToFloatSize(FloatPoint(
          WebView().FakePageScaleAnimationTargetPositionForTesting())),
      mojom::blink::ScrollType::kProgrammatic);

  EXPECT_TRUE(frame_view->GetScrollableArea()->VisibleContentRect().Contains(
      inputRect));

  // Reset the testing getters.
  WebView().EnableFakePageScaleAnimationForTesting(true);

  // This input is already in view, this shouldn't cause a scroll.
  WebView()
      .MainFrameImpl()
      ->FrameWidget()
      ->ScrollFocusedEditableElementIntoView();

  EXPECT_EQ(0, WebView().FakePageScaleAnimationPageScaleForTesting());
  EXPECT_EQ(IntPoint(),
            WebView().FakePageScaleAnimationTargetPositionForTesting());

  // Now resize the visual viewport so that the input box is no longer in view
  // (e.g. a keyboard is overlaid).
  WebView().ResizeVisualViewport(gfx::Size(200, 100));
  ASSERT_FALSE(frame_view->GetScrollableArea()->VisibleContentRect().Contains(
      inputRect));

  WebView()
      .MainFrameImpl()
      ->FrameWidget()
      ->ScrollFocusedEditableElementIntoView();
  frame_view->GetScrollableArea()->SetScrollOffset(
      ToFloatSize(FloatPoint(
          WebView().FakePageScaleAnimationTargetPositionForTesting())),
      mojom::blink::ScrollType::kProgrammatic);

  EXPECT_TRUE(frame_view->GetScrollableArea()->VisibleContentRect().Contains(
      inputRect));
  EXPECT_EQ(1, WebView().FakePageScaleAnimationPageScaleForTesting());
}

// Ensures scrolling a focused editable text into view that's located in the
// root scroller works by scrolling the root scroller.
TEST_F(WebFrameSimTest, TestScrollFocusedEditableInRootScroller) {
  ScopedImplicitRootScrollerForTest implicit_root_scroller(true);

  WebView().MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  WebView().SetDefaultPageScaleLimits(1.f, 4);
  WebView().EnableFakePageScaleAnimationForTesting(true);
  WebView().GetPage()->GetSettings().SetTextAutosizingEnabled(false);
  WebView().GetPage()->GetSettings().SetViewportEnabled(false);
  WebView().GetSettings()->SetAutoZoomFocusedNodeToLegibleScale(true);

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        ::-webkit-scrollbar {
          width: 0px;
          height: 0px;
        }
        body,html {
          width: 100%;
          height: 100%;
          margin: 0px;
        }
        input {
          border: 0;
          padding: 0;
          margin-left: 200px;
          margin-top: 700px;
          width: 100px;
          height: 20px;
        }
        #scroller {
          background: silver;
          width: 100%;
          height: 100%;
          overflow: auto;
        }
      </style>
      <div id="scroller" tabindex="-1">
        <input type="text">
      </div>
  )HTML");

  Compositor().BeginFrame();

  TopDocumentRootScrollerController& rs_controller =
      GetDocument().GetPage()->GlobalRootScrollerController();

  Element* scroller = GetDocument().getElementById("scroller");
  ASSERT_EQ(scroller, rs_controller.GlobalRootScroller());

  auto* frame = To<LocalFrame>(WebView().GetPage()->MainFrame());
  VisualViewport& visual_viewport = frame->GetPage()->GetVisualViewport();

  WebView().AdvanceFocus(false);

  rs_controller.RootScrollerArea()->SetScrollOffset(
      ScrollOffset(0, 300), mojom::blink::ScrollType::kProgrammatic);

  LocalFrameView* frame_view = frame->View();
  IntRect inputRect(200, 700, 100, 20);
  ASSERT_EQ(1, visual_viewport.Scale());
  ASSERT_EQ(FloatPoint(0, 300),
            frame_view->GetScrollableArea()->VisibleContentRect().Location());
  ASSERT_FALSE(frame_view->GetScrollableArea()->VisibleContentRect().Contains(
      inputRect));

  WebView()
      .MainFrameImpl()
      ->FrameWidget()
      ->ScrollFocusedEditableElementIntoView();

  EXPECT_EQ(1, WebView().FakePageScaleAnimationPageScaleForTesting());

  ScrollOffset target_offset = ToFloatSize(
      FloatPoint(WebView().FakePageScaleAnimationTargetPositionForTesting()));

  rs_controller.RootScrollerArea()->SetScrollOffset(
      target_offset, mojom::blink::ScrollType::kProgrammatic);

  EXPECT_TRUE(frame_view->GetScrollableArea()->VisibleContentRect().Contains(
      inputRect));
}

TEST_F(WebFrameSimTest, ScrollFocusedIntoViewClipped) {
  // The Android On-Screen Keyboard (OSK) resizes the Widget Blink is hosted
  // in. When the keyboard is shown, we scroll and zoom in on the currently
  // focused editable element. However, the scroll and zoom is a smoothly
  // animated "PageScaleAnimation" that's performed in CC only on the viewport
  // layers. There are some situations in which the widget resize causes the
  // focued input to be hidden by clipping parents that aren't the main frame.
  // In these cases, there's no way to scroll just the viewport to make the
  // input visible, we need to also scroll those clip/scroller elements  This
  // test ensures we do so. https://crbug.com/270018.
  UseAndroidSettings();
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 600));
  WebView().EnableFakePageScaleAnimationForTesting(true);
  WebView().GetPage()->GetSettings().SetTextAutosizingEnabled(false);

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
          margin: 0px;
          width: 100%;
          height: 100%;
        }
        input {
          padding: 0;
          position: relative;
          top: 1400px;
          width: 100px;
          height: 20px;
        }
        #clip {
          width: 100%;
          height: 100%;
          overflow: hidden;
        }
        #container {
          width: 980px;
          height: 1470px;
        }
      </style>
      <div id="clip">
        <div id="container">
          <input type="text" id="target">
        </div>
      </div>
  )HTML");

  Compositor().BeginFrame();
  WebView().AdvanceFocus(false);

  auto* frame = To<LocalFrame>(WebView().GetPage()->MainFrame());
  LocalFrameView* frame_view = frame->View();
  VisualViewport& visual_viewport = frame->GetPage()->GetVisualViewport();

  ASSERT_EQ(FloatPoint(),
            frame_view->GetScrollableArea()->VisibleContentRect().Location());

  // Simulate the keyboard being shown and resizing the widget. Cause a scroll
  // into view after.
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 300));

  float scale_before = visual_viewport.Scale();
  WebView()
      .MainFrameImpl()
      ->FrameWidget()
      ->ScrollFocusedEditableElementIntoView();

  Element* input = GetDocument().getElementById("target");
  IntRect input_rect(input->getBoundingClientRect()->top(),
                     input->getBoundingClientRect()->left(),
                     input->getBoundingClientRect()->width(),
                     input->getBoundingClientRect()->height());

  IntRect visible_content_rect(IntPoint(), frame_view->Size());
  EXPECT_TRUE(visible_content_rect.Contains(input_rect))
      << "Layout viewport [" << visible_content_rect.ToString()
      << "] does not contain input rect [" << input_rect.ToString()
      << "] after scroll into view.";

  EXPECT_TRUE(visual_viewport.VisibleRect().Contains(input_rect))
      << "Visual viewport [" << visual_viewport.VisibleRect().ToString()
      << "] does not contain input rect [" << input_rect.ToString()
      << "] after scroll into view.";

  // Make sure we also zoomed in on the input.
  EXPECT_GT(WebView().FakePageScaleAnimationPageScaleForTesting(),
            scale_before);

  // Additional gut-check that we actually scrolled the non-user-scrollable
  // clip element to make sure the input is in view.
  Element* clip = GetDocument().getElementById("clip");
  EXPECT_GT(clip->scrollTop(), 0);
}

//  This test ensures that we scroll to the correct scale when the focused
//  element has a selection rather than a carret.
TEST_F(WebFrameSimTest, ScrollFocusedSelectionIntoView) {
  UseAndroidSettings();
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 600));
  WebView().EnableFakePageScaleAnimationForTesting(true);
  WebView().GetPage()->GetSettings().SetTextAutosizingEnabled(false);

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
          margin: 0px;
          width: 100%;
          height: 100%;
        }
        input {
          padding: 0;
          width: 100px;
          height: 20px;
        }
      </style>
      <input type="text" id="target" value="test">
  )HTML");

  Compositor().BeginFrame();
  WebView().AdvanceFocus(false);

  auto* input = To<HTMLInputElement>(GetDocument().getElementById("target"));
  input->select();

  // Simulate the keyboard being shown and resizing the widget. Cause a scroll
  // into view after.
  ASSERT_EQ(WebView().FakePageScaleAnimationPageScaleForTesting(), 0.f);
  WebFrameWidget* widget = WebView().MainFrameImpl()->FrameWidgetImpl();
  widget->ScrollFocusedEditableElementIntoView();

  // Make sure zoomed in but only up to a legible scale. The bounds are
  // arbitrary and fuzzy since we don't specifically care to constrain the
  // amount of zooming (that should be tested elsewhere), we just care that it
  // zooms but not off to infinity.
  EXPECT_GT(WebView().FakePageScaleAnimationPageScaleForTesting(), .75f);
  EXPECT_LT(WebView().FakePageScaleAnimationPageScaleForTesting(), 2.f);
}

TEST_F(WebFrameSimTest, DoubleTapZoomWhileScrolled) {
  UseAndroidSettings();
  WebView().MainFrameViewWidget()->Resize(gfx::Size(490, 500));
  WebView().EnableFakePageScaleAnimationForTesting(true);
  WebView().GetSettings()->SetTextAutosizingEnabled(false);
  WebView().SetDefaultPageScaleLimits(0.5f, 4);

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        ::-webkit-scrollbar {
          width: 0px;
          height: 0px;
        }
        body {
          margin: 0px;
          width: 10000px;
          height: 10000px;
        }
        #target {
          position: absolute;
          left: 2000px;
          top: 3000px;
          width: 100px;
          height: 100px;
          background-color: blue;
        }
      </style>
      <div id="target"></div>
  )HTML");

  Compositor().BeginFrame();

  auto* frame = To<LocalFrame>(WebView().GetPage()->MainFrame());
  LocalFrameView* frame_view = frame->View();
  VisualViewport& visual_viewport = frame->GetPage()->GetVisualViewport();
  IntRect target_rect_in_document(2000, 3000, 100, 100);

  ASSERT_EQ(0.5f, visual_viewport.Scale());

  // Center the target in the screen.
  frame_view->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(2000 - 440, 3000 - 450),
      mojom::blink::ScrollType::kProgrammatic);
  Element* target = GetDocument().QuerySelector("#target");
  DOMRect* rect = target->getBoundingClientRect();
  ASSERT_EQ(440, rect->left());
  ASSERT_EQ(450, rect->top());

  // Double-tap on the target. Expect that we zoom in and the target is
  // contained in the visual viewport.
  {
    gfx::Point point(445, 455);
    WebRect block_bounds = ComputeBlockBoundHelper(&WebView(), point, false);
    WebView().AnimateDoubleTapZoom(IntPoint(point), block_bounds);
    EXPECT_TRUE(WebView().FakeDoubleTapAnimationPendingForTesting());
    ScrollOffset new_offset = ToScrollOffset(
        FloatPoint(WebView().FakePageScaleAnimationTargetPositionForTesting()));
    float new_scale = WebView().FakePageScaleAnimationPageScaleForTesting();
    visual_viewport.SetScale(new_scale);
    frame_view->GetScrollableArea()->SetScrollOffset(
        new_offset, mojom::blink::ScrollType::kProgrammatic);

    EXPECT_FLOAT_EQ(1, visual_viewport.Scale());
    EXPECT_TRUE(frame_view->GetScrollableArea()->VisibleContentRect().Contains(
        target_rect_in_document));
  }

  // Reset the testing getters.
  WebView().EnableFakePageScaleAnimationForTesting(true);

  // Double-tap on the target again. We should zoom out and the target should
  // remain on screen.
  {
    gfx::Point point(445, 455);
    WebRect block_bounds = ComputeBlockBoundHelper(&WebView(), point, false);
    WebView().AnimateDoubleTapZoom(IntPoint(point), block_bounds);
    EXPECT_TRUE(WebView().FakeDoubleTapAnimationPendingForTesting());
    IntPoint target_offset(
        WebView().FakePageScaleAnimationTargetPositionForTesting());
    float new_scale = WebView().FakePageScaleAnimationPageScaleForTesting();

    EXPECT_FLOAT_EQ(0.5f, new_scale);
    EXPECT_TRUE(target_rect_in_document.Contains(target_offset));
  }
}

TEST_F(WebFrameSimTest, ChangeBackgroundColor) {
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");
  main_resource.Complete("<!DOCTYPE html><body></body>");

  Element* body = GetDocument().QuerySelector("body");
  EXPECT_TRUE(!!body);

  Compositor().BeginFrame();
  // White is the default background of a web page.
  EXPECT_EQ(SK_ColorWHITE, Compositor().background_color());

  // Setting the background of the body to red will cause the background
  // color of the WebView to switch to red.
  body->SetInlineStyleProperty(CSSPropertyID::kBackgroundColor, "red");
  Compositor().BeginFrame();
  EXPECT_EQ(SK_ColorRED, Compositor().background_color());
}

// Ensure we don't crash if we try to scroll into view the focused editable
// element which doesn't have a LayoutObject.
TEST_F(WebFrameSimTest, ScrollFocusedEditableIntoViewNoLayoutObject) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(500, 600));
  WebView().GetPage()->GetSettings().SetTextAutosizingEnabled(false);

  SimRequest r("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  r.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        input {
          position: absolute;
          top: 1000px;
          left: 800px;
        }

        @media (max-height: 500px) {
          input {
            display: none;
          }
        }
      </style>
      <input id="target" type="text"></input>
  )HTML");

  Compositor().BeginFrame();

  Element* input = GetDocument().getElementById("target");
  input->focus();

  ScrollableArea* area = GetDocument().View()->LayoutViewport();
  area->SetScrollOffset(ScrollOffset(0, 0),
                        mojom::blink::ScrollType::kProgrammatic);

  ASSERT_TRUE(input->GetLayoutObject());
  ASSERT_EQ(input, WebView().FocusedElement());
  ASSERT_EQ(ScrollOffset(0, 0), area->GetScrollOffset());

  // The resize should cause the focused element to lose its LayoutObject. If
  // this resize came from the Android on-screen keyboard, this would be
  // followed by a ScrollFocusedEditableElementIntoView. Ensure we don't crash.
  WebView().MainFrameViewWidget()->Resize(gfx::Size(500, 300));

  ASSERT_FALSE(input->GetLayoutObject());
  ASSERT_EQ(input, WebView().FocusedElement());

  WebFrameWidget* widget = WebView().MainFrameImpl()->FrameWidgetImpl();
  widget->ScrollFocusedEditableElementIntoView();
  Compositor().BeginFrame();

  // Shouldn't cause any scrolling either.
  EXPECT_EQ(ScrollOffset(0, 0), area->GetScrollOffset());
}

TEST_F(WebFrameSimTest, DisplayNoneIFrameHasNoLayoutObjects) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/frame.html", "text/html");

  LoadURL("https://example.com/test.html");
  main_resource.Complete(
      "<!DOCTYPE html>"
      "<iframe src=frame.html style='display: none'></iframe>");
  frame_resource.Complete(
      "<!DOCTYPE html>"
      "<html><body>This is a visible iframe.</body></html>");

  Element* element = GetDocument().QuerySelector("iframe");
  auto* frame_owner_element = To<HTMLFrameOwnerElement>(element);
  Document* iframe_doc = frame_owner_element->contentDocument();
  EXPECT_FALSE(iframe_doc->documentElement()->GetLayoutObject());

  // Changing the display from 'none' -> 'block' should cause layout objects to
  // appear.
  element->SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kBlock);
  Compositor().BeginFrame();
  EXPECT_TRUE(iframe_doc->documentElement()->GetLayoutObject());

  // Changing the display from 'block' -> 'none' should cause layout objects to
  // disappear.
  element->SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kNone);

  Compositor().BeginFrame();
  EXPECT_FALSE(iframe_doc->documentElement()->GetLayoutObject());
}

// Although it is not spec compliant, many websites intentionally call
// Window.print() on display:none iframes. https://crbug.com/819327.
TEST_F(WebFrameSimTest, DisplayNoneIFramePrints) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/frame.html", "text/html");

  LoadURL("https://example.com/test.html");
  main_resource.Complete(
      "<!DOCTYPE html>"
      "<iframe src=frame.html style='display: none'></iframe>");
  frame_resource.Complete(
      "<!DOCTYPE html>"
      "<html><body>This is a visible iframe.</body></html>");

  Element* element = GetDocument().QuerySelector("iframe");
  auto* frame_owner_element = To<HTMLFrameOwnerElement>(element);
  Document* iframe_doc = frame_owner_element->contentDocument();
  EXPECT_FALSE(iframe_doc->documentElement()->GetLayoutObject());

  FloatSize page_size(400, 400);
  float maximum_shrink_ratio = 1.0;
  iframe_doc->GetFrame()->StartPrinting(page_size, page_size,
                                        maximum_shrink_ratio);
  EXPECT_TRUE(iframe_doc->documentElement()->GetLayoutObject());

  iframe_doc->GetFrame()->EndPrinting();
  EXPECT_FALSE(iframe_doc->documentElement()->GetLayoutObject());
}

TEST_F(WebFrameSimTest, NormalIFrameHasLayoutObjects) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/frame.html", "text/html");

  LoadURL("https://example.com/test.html");
  main_resource.Complete(
      "<!DOCTYPE html>"
      "<iframe src=frame.html style='display: block'></iframe>");
  frame_resource.Complete(
      "<!DOCTYPE html>"
      "<html><body>This is a visible iframe.</body></html>");

  Element* element = GetDocument().QuerySelector("iframe");
  auto* frame_owner_element = To<HTMLFrameOwnerElement>(element);
  Document* iframe_doc = frame_owner_element->contentDocument();
  EXPECT_TRUE(iframe_doc->documentElement()->GetLayoutObject());

  // Changing the display from 'block' -> 'none' should cause layout objects to
  // disappear.
  element->SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kNone);
  Compositor().BeginFrame();
  EXPECT_FALSE(iframe_doc->documentElement()->GetLayoutObject());
}

TEST_F(WebFrameSimTest, RtlInitialScrollOffsetWithViewport) {
  UseAndroidSettings();

  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 400));
  WebView().SetDefaultPageScaleLimits(0.25f, 2);

  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <meta name='viewport' content='width=device-width, minimum-scale=1'>
    <body dir='rtl'>
    <div style='width: 3000px; height: 20px'></div>
  )HTML");

  Compositor().BeginFrame();
  ScrollableArea* area = GetDocument().View()->LayoutViewport();
  ASSERT_EQ(ScrollOffset(0, 0), area->GetScrollOffset());
}

TEST_F(WebFrameSimTest, LayoutViewportExceedsLayoutOverflow) {
  UseAndroidSettings();

  WebView().ResizeWithBrowserControls(gfx::Size(400, 540), 60, 0, true);
  WebView().SetDefaultPageScaleLimits(0.25f, 2);

  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <meta name='viewport' content='width=device-width, minimum-scale=1'>
    <body style='margin: 0; height: 95vh'>
  )HTML");

  Compositor().BeginFrame();
  ScrollableArea* area = GetDocument().View()->LayoutViewport();
  ASSERT_EQ(540, area->VisibleHeight());
  ASSERT_EQ(IntSize(400, 570), area->ContentsSize());

  // Hide browser controls, growing layout viewport without affecting ICB.
  WebView().ResizeWithBrowserControls(gfx::Size(400, 600), 60, 0, false);
  Compositor().BeginFrame();

  // ContentsSize() should grow to accommodate new visible size.
  ASSERT_EQ(600, area->VisibleHeight());
  ASSERT_EQ(IntSize(400, 600), area->ContentsSize());
}

TEST_F(WebFrameSimTest, LayoutViewLocalVisualRect) {
  UseAndroidSettings();

  WebView().MainFrameViewWidget()->Resize(gfx::Size(600, 400));
  WebView().SetDefaultPageScaleLimits(0.5f, 2);

  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <meta name='viewport' content='width=device-width, minimum-scale=0.5'>
    <body style='margin: 0; width: 1800px; height: 1200px'></div>
  )HTML");

  Compositor().BeginFrame();
  ASSERT_EQ(PhysicalRect(0, 0, 1200, 800),
            GetDocument().GetLayoutView()->LocalVisualRect());
}

TEST_F(WebFrameSimTest, NamedLookupIgnoresEmptyNames) {
  SimRequest main_resource("https://example.com/main.html", "text/html");
  LoadURL("https://example.com/main.html");
  main_resource.Complete(R"HTML(
    <body>
    <iframe name="" src="data:text/html,"></iframe>
    </body>)HTML");

  EXPECT_EQ(nullptr, MainFrame().GetFrame()->Tree().ScopedChild(""));
  EXPECT_EQ(nullptr,
            MainFrame().GetFrame()->Tree().ScopedChild(AtomicString()));
  EXPECT_EQ(nullptr, MainFrame().GetFrame()->Tree().ScopedChild(g_empty_atom));
}

TEST_F(WebFrameTest, NoLoadingCompletionCallbacksInDetach) {
  class LoadingObserverFrameClient
      : public frame_test_helpers::TestWebFrameClient {
   public:
    LoadingObserverFrameClient() = default;
    ~LoadingObserverFrameClient() override = default;

    // frame_test_helpers::TestWebFrameClient:
    void FrameDetached() override {
      did_call_frame_detached_ = true;
      TestWebFrameClient::FrameDetached();
    }

    void DidStopLoading() override {
      // TODO(dcheng): Investigate not calling this as well during frame detach.
      did_call_did_stop_loading_ = true;
      TestWebFrameClient::DidStopLoading();
    }

    void DidFinishDocumentLoad() override {
      // TODO(dcheng): Investigate not calling this as well during frame detach.
      did_call_did_finish_document_load_ = true;
    }

    void DidHandleOnloadEvents() override {
      // TODO(dcheng): Investigate not calling this as well during frame detach.
      did_call_did_handle_onload_events_ = true;
    }

    void DidFinishLoad() override {
      EXPECT_TRUE(false) << "didFinishLoad() should not have been called.";
    }

    bool DidCallFrameDetached() const { return did_call_frame_detached_; }
    bool DidCallDidStopLoading() const { return did_call_did_stop_loading_; }
    bool DidCallDidFinishDocumentLoad() const {
      return did_call_did_finish_document_load_;
    }
    bool DidCallDidHandleOnloadEvents() const {
      return did_call_did_handle_onload_events_;
    }

   private:
    bool did_call_frame_detached_ = false;
    bool did_call_did_stop_loading_ = false;
    bool did_call_did_finish_document_load_ = false;
    bool did_call_did_handle_onload_events_ = false;
  };

  class MainFrameClient : public frame_test_helpers::TestWebFrameClient {
   public:
    MainFrameClient() = default;
    ~MainFrameClient() override = default;

    // frame_test_helpers::TestWebFrameClient:
    WebLocalFrame* CreateChildFrame(
        WebLocalFrame* parent,
        mojom::blink::TreeScopeType scope,
        const WebString& name,
        const WebString& fallback_name,
        const FramePolicy&,
        const WebFrameOwnerProperties&,
        mojom::blink::FrameOwnerElementType) override {
      return CreateLocalChild(*parent, scope, &child_client_);
    }

    LoadingObserverFrameClient& ChildClient() { return child_client_; }

   private:
    LoadingObserverFrameClient child_client_;
  };

  RegisterMockedHttpURLLoad("single_iframe.html");
  url_test_helpers::RegisterMockedURLLoad(
      ToKURL(base_url_ + "visible_iframe.html"),
      test::CoreTestDataPath("frame_with_frame.html"));
  RegisterMockedHttpURLLoad("parent_detaching_frame.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  MainFrameClient main_frame_client;
  web_view_helper.InitializeAndLoad(base_url_ + "single_iframe.html",
                                    &main_frame_client);

  EXPECT_TRUE(main_frame_client.ChildClient().DidCallFrameDetached());
  EXPECT_TRUE(main_frame_client.ChildClient().DidCallDidStopLoading());
  EXPECT_TRUE(main_frame_client.ChildClient().DidCallDidFinishDocumentLoad());
  EXPECT_TRUE(main_frame_client.ChildClient().DidCallDidHandleOnloadEvents());

  web_view_helper.Reset();
}

TEST_F(WebFrameTest, ClearClosedOpener) {
  frame_test_helpers::WebViewHelper opener_helper;
  opener_helper.Initialize();
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeWithOpener(opener_helper.GetWebView()->MainFrame());

  opener_helper.Reset();
  EXPECT_EQ(nullptr, helper.LocalMainFrame()->Opener());
}

class ShowVirtualKeyboardObserverWidgetClient
    : public frame_test_helpers::TestWebWidgetClient {
 public:
  ShowVirtualKeyboardObserverWidgetClient() = default;
  ~ShowVirtualKeyboardObserverWidgetClient() override = default;

  // frame_test_helpers::TestWebWidgetClient:
  void TextInputStateChanged(
      ui::mojom::blink::TextInputStatePtr state) override {
    did_show_virtual_keyboard_ |= state->show_ime_if_needed;
  }

  bool DidShowVirtualKeyboard() const { return did_show_virtual_keyboard_; }

 private:
  bool did_show_virtual_keyboard_ = false;
};

TEST_F(WebFrameTest, ShowVirtualKeyboardOnElementFocus) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeRemote();

  ShowVirtualKeyboardObserverWidgetClient web_widget_client;
  WebLocalFrameImpl* local_frame = frame_test_helpers::CreateLocalChild(
      *web_view_helper.RemoteMainFrame(), "child", WebFrameOwnerProperties(),
      nullptr, nullptr, &web_widget_client);

  RegisterMockedHttpURLLoad("input_field_default.html");
  frame_test_helpers::LoadFrame(local_frame,
                                base_url_ + "input_field_default.html");

  // Simulate an input element focus leading to Element::focus() call with a
  // user gesture.
  LocalFrame::NotifyUserActivation(
      local_frame->GetFrame(), mojom::UserActivationNotificationType::kTest);
  local_frame->ExecuteScript(
      WebScriptSource("window.focus();"
                      "document.querySelector('input').focus();"));

  RunPendingTasks();
  // Verify that the right WebWidgetClient has been notified.
#if BUILDFLAG(IS_ASH)
  EXPECT_FALSE(web_widget_client.DidShowVirtualKeyboard());
#else
  EXPECT_TRUE(web_widget_client.DidShowVirtualKeyboard());
#endif
  web_view_helper.Reset();
}

class ContextMenuWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  ContextMenuWebFrameClient() = default;
  ~ContextMenuWebFrameClient() override = default;

  // WebLocalFrameClient:
  void ShowContextMenu(const WebContextMenuData& data,
                       const base::Optional<gfx::Point>&) override {
    menu_data_ = data;
  }

  WebContextMenuData GetMenuData() { return menu_data_; }

 private:
  WebContextMenuData menu_data_;
  DISALLOW_COPY_AND_ASSIGN(ContextMenuWebFrameClient);
};

bool TestSelectAll(const std::string& html) {
  ContextMenuWebFrameClient frame;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize(&frame);
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(), html,
                                     ToKURL("about:blank"));
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);
  RunPendingTasks();
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  RunPendingTasks();

  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests());

  mouse_event.button = WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(8, 8);
  mouse_event.click_count = 1;
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_event, ui::LatencyInfo()));
  RunPendingTasks();
  web_view_helper.Reset();
  return frame.GetMenuData().edit_flags &
         ContextMenuDataEditFlags::kCanSelectAll;
}

TEST_F(WebFrameTest, ContextMenuDataSelectAll) {
  EXPECT_FALSE(TestSelectAll("<textarea></textarea>"));
  EXPECT_TRUE(TestSelectAll("<textarea>nonempty</textarea>"));
  EXPECT_FALSE(TestSelectAll("<input>"));
  EXPECT_TRUE(TestSelectAll("<input value='nonempty'>"));
  EXPECT_FALSE(TestSelectAll("<div contenteditable></div>"));
  EXPECT_TRUE(TestSelectAll("<div contenteditable>nonempty</div>"));
  EXPECT_TRUE(TestSelectAll("<div contenteditable>\n</div>"));
}

TEST_F(WebFrameTest, ContextMenuDataSelectedText) {
  ContextMenuWebFrameClient frame;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize(&frame);
  const std::string& html = "<input value=' '>";
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(), html,
                                     ToKURL("about:blank"));
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases(web_view);
  RunPendingTasks();
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  RunPendingTasks();

  web_view->MainFrameImpl()->ExecuteCommand(WebString::FromUTF8("SelectAll"));

  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests());

  mouse_event.button = WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(8, 8);
  mouse_event.click_count = 1;
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_event, ui::LatencyInfo()));
  RunPendingTasks();
  web_view_helper.Reset();
  EXPECT_EQ(frame.GetMenuData().selected_text, " ");
}

TEST_F(WebFrameTest, ContextMenuDataPasswordSelectedText) {
  ContextMenuWebFrameClient frame;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize(&frame);
  const std::string& html = "<input type='password' value='password'>";
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(), html,
                                     ToKURL("about:blank"));
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases(web_view);
  RunPendingTasks();
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  RunPendingTasks();

  web_view->MainFrameImpl()->ExecuteCommand(WebString::FromUTF8("SelectAll"));

  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests());

  mouse_event.button = WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(8, 8);
  mouse_event.click_count = 1;
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_event, ui::LatencyInfo()));

  RunPendingTasks();
  web_view_helper.Reset();
  EXPECT_EQ(frame.GetMenuData().input_field_type,
            blink::ContextMenuDataInputFieldType::kPassword);
  EXPECT_FALSE(frame.GetMenuData().selected_text.IsEmpty());
}

TEST_F(WebFrameTest, ContextMenuDataNonLocatedMenu) {
  ContextMenuWebFrameClient frame;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize(&frame);
  const std::string& html =
      "<div style='font-size: 1000%; line-height: 0.7em'>Select me<br/>"
      "Next line</div>";
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(), html,
                                     ToKURL("about:blank"));
  web_view->MainFrameViewWidget()->Resize(gfx::Size(500, 300));
  UpdateAllLifecyclePhases(web_view);
  RunPendingTasks();
  web_view->MainFrameImpl()->GetFrame()->SetInitialFocus(false);
  RunPendingTasks();

  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests());

  mouse_event.button = WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(0, 0);
  mouse_event.click_count = 2;
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_event, ui::LatencyInfo()));

  web_view->MainFrameImpl()->LocalRootFrameWidget()->ShowContextMenu(
      ui::mojom::MenuSourceType::TOUCH,
      web_view->MainFrameImpl()->GetPositionInViewportForTesting());

  RunPendingTasks();
  web_view_helper.Reset();
  EXPECT_EQ(frame.GetMenuData().source_type, kMenuSourceTouch);
  EXPECT_FALSE(frame.GetMenuData().selected_text.IsEmpty());
}

TEST_F(WebFrameTest, LocalFrameWithRemoteParentIsTransparent) {
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote();

  WebLocalFrameImpl* local_frame =
      frame_test_helpers::CreateLocalChild(*helper.RemoteMainFrame());
  frame_test_helpers::LoadFrame(local_frame, "data:text/html,some page");

  // Local frame with remote parent should have transparent baseBackgroundColor.
  Color color = local_frame->GetFrameView()->BaseBackgroundColor();
  EXPECT_EQ(Color::kTransparent, color);
}

class TestFallbackWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  TestFallbackWebFrameClient() : child_client_(nullptr) {}
  ~TestFallbackWebFrameClient() override = default;

  void SetChildWebFrameClient(TestFallbackWebFrameClient* client) {
    child_client_ = client;
  }

  // frame_test_helpers::TestWebFrameClient:
  WebLocalFrame* CreateChildFrame(
      WebLocalFrame* parent,
      mojom::blink::TreeScopeType scope,
      const WebString&,
      const WebString&,
      const FramePolicy&,
      const WebFrameOwnerProperties& frameOwnerProperties,
      mojom::blink::FrameOwnerElementType) override {
    DCHECK(child_client_);
    return CreateLocalChild(*parent, scope, child_client_);
  }
  void BeginNavigation(std::unique_ptr<WebNavigationInfo> info) override {
    if (child_client_ || KURL(info->url_request.Url()) == BlankURL()) {
      TestWebFrameClient::BeginNavigation(std::move(info));
      return;
    }
    Frame()->WillStartNavigation(*info);
  }

 private:
  TestFallbackWebFrameClient* child_client_;
};

TEST_F(WebFrameTest, FallbackForNonexistentProvisionalNavigation) {
  RegisterMockedHttpURLLoad("fallback.html");
  TestFallbackWebFrameClient main_client;
  TestFallbackWebFrameClient child_client;
  main_client.SetChildWebFrameClient(&child_client);

  frame_test_helpers::WebViewHelper web_view_helper_;
  web_view_helper_.Initialize(&main_client);

  WebLocalFrameImpl* main_frame = web_view_helper_.LocalMainFrame();
  WebURLRequest request(ToKURL(base_url_ + "fallback.html"));
  main_frame->StartNavigation(request);

  // Because the child frame will have placeholder document loader, the main
  // frame will not finish loading, so
  // frame_test_helpers::PumpPendingRequestsForFrameToLoad doesn't work here.
  url_test_helpers::ServeAsynchronousRequests();

  // Overwrite the client-handled child frame navigation with about:blank.
  WebLocalFrame* child = main_frame->FirstChild()->ToWebLocalFrame();
  frame_test_helpers::LoadFrameDontWait(child, BlankURL());

  // Failing the original child frame navigation and trying to render fallback
  // content shouldn't crash. It should return NoLoadInProgress. This is so the
  // caller won't attempt to replace the correctly empty frame with an error
  // page.
  EXPECT_EQ(WebNavigationControl::NoLoadInProgress,
            To<WebLocalFrameImpl>(child)->MaybeRenderFallbackContent(
                WebURLError(ResourceError::Failure(request.Url()))));
}

TEST_F(WebFrameTest, AltTextOnAboutBlankPage) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank");
  web_view_helper.Resize(gfx::Size(640, 480));
  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();

  const char kSource[] =
      "<img id='foo' src='foo' alt='foo alt' width='200' height='200'>";
  frame_test_helpers::LoadHTMLString(frame, kSource, ToKURL("about:blank"));
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  RunPendingTasks();

  // Check LayoutText with alt text "foo alt"
  LayoutObject* layout_object = frame->GetFrame()
                                    ->GetDocument()
                                    ->getElementById("foo")
                                    ->GetLayoutObject()
                                    ->SlowFirstChild();
  String text = "";
  for (LayoutObject* obj = layout_object; obj; obj = obj->NextInPreOrder()) {
    if (obj->IsText()) {
      LayoutText* layout_text = ToLayoutText(obj);
      text = layout_text->GetText();
      break;
    }
  }
  EXPECT_EQ("foo alt", text.Utf8());
}

TEST_F(WebFrameTest, NavigatorPluginsClearedWhenPluginsDisabled) {
  ScopedFakePluginRegistry fake_plugins;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Value> result =
      web_view_helper.LocalMainFrame()->ExecuteScriptAndReturnValue(
          WebScriptSource("navigator.plugins.length"));
  EXPECT_NE(0, result->Int32Value(context).ToChecked());
  web_view_helper.GetWebView()->GetPage()->GetSettings().SetPluginsEnabled(
      false);
  result = web_view_helper.LocalMainFrame()->ExecuteScriptAndReturnValue(
      WebScriptSource("navigator.plugins.length"));
  EXPECT_EQ(0, result->Int32Value(context).ToChecked());
}

TEST_F(WebFrameTest, RecordSameDocumentNavigationToHistogram) {
  const char* histogramName =
      "RendererScheduler.UpdateForSameDocumentNavigationCount";
  frame_test_helpers::WebViewHelper web_view_helper;
  HistogramTester tester;
  web_view_helper.InitializeAndLoad("about:blank");
  auto* frame =
      To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame());

  DocumentLoader& document_loader = *web_view_helper.GetWebView()
                                         ->MainFrameImpl()
                                         ->GetFrame()
                                         ->GetDocument()
                                         ->Loader();
  scoped_refptr<SerializedScriptValue> message =
      SerializeString("message", ToScriptStateForMainWorld(frame));
  tester.ExpectTotalCount(histogramName, 0);
  document_loader.UpdateForSameDocumentNavigation(
      ToKURL("about:blank"), kSameDocumentNavigationHistoryApi, message,
      mojom::blink::ScrollRestorationType::kAuto,
      WebFrameLoadType::kReplaceCurrentItem, frame->GetDocument());
  // The bucket index corresponds to the definition of
  // |SinglePageAppNavigationType|.
  tester.ExpectBucketCount(histogramName,
                           kSPANavTypeHistoryPushStateOrReplaceState, 1);
  document_loader.UpdateForSameDocumentNavigation(
      ToKURL("about:blank"), kSameDocumentNavigationDefault, message,
      mojom::blink::ScrollRestorationType::kManual,
      WebFrameLoadType::kBackForward, frame->GetDocument());
  tester.ExpectBucketCount(histogramName,
                           kSPANavTypeSameDocumentBackwardOrForward, 1);
  document_loader.UpdateForSameDocumentNavigation(
      ToKURL("about:blank"), kSameDocumentNavigationDefault, message,
      mojom::blink::ScrollRestorationType::kManual,
      WebFrameLoadType::kReplaceCurrentItem, frame->GetDocument());
  tester.ExpectBucketCount(histogramName, kSPANavTypeOtherFragmentNavigation,
                           1);
  // kSameDocumentNavigationHistoryApi and WebFrameLoadType::kBackForward is an
  // illegal combination, which has been caught by DCHECK in
  // UpdateForSameDocumentNavigation().

  tester.ExpectTotalCount(histogramName, 3);
}

static void TestFramePrinting(WebLocalFrameImpl* frame) {
  WebPrintParams print_params;
  WebSize page_size(500, 500);
  print_params.print_content_area.width = page_size.width;
  print_params.print_content_area.height = page_size.height;
  EXPECT_EQ(1u, frame->PrintBegin(print_params, WebNode()));
  PaintRecorder recorder;
  frame->PrintPagesForTesting(recorder.beginRecording(IntRect()), page_size,
                              page_size);
  frame->PrintEnd();
}

TEST_F(WebFrameTest, PrintDetachedIframe) {
  RegisterMockedHttpURLLoad("print-detached-iframe.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "print-detached-iframe.html");
  TestFramePrinting(
      To<WebLocalFrameImpl>(web_view_helper.LocalMainFrame()->FirstChild()));
}

TEST_F(WebFrameTest, PrintIframeUnderDetached) {
  RegisterMockedHttpURLLoad("print-detached-iframe.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "print-detached-iframe.html");
  TestFramePrinting(To<WebLocalFrameImpl>(
      web_view_helper.LocalMainFrame()->FirstChild()->FirstChild()));
}

namespace {

struct TextRunDOMNodeIdInfo {
  int glyph_len;
  DOMNodeId dom_node_id;
};

// Given a PaintRecord and a starting DOMNodeId, recursively iterate over all of
// the (nested) paint ops, and populate |text_runs| with the number of glyphs
// and the DOMNodeId of each text run.
void RecursiveCollectTextRunDOMNodeIds(
    sk_sp<const PaintRecord> paint_record,
    DOMNodeId dom_node_id,
    std::vector<TextRunDOMNodeIdInfo>* text_runs) {
  for (cc::PaintOpBuffer::Iterator it(paint_record.get()); it; ++it) {
    if ((*it)->GetType() == cc::PaintOpType::DrawRecord) {
      cc::DrawRecordOp* draw_record_op = static_cast<cc::DrawRecordOp*>(*it);
      RecursiveCollectTextRunDOMNodeIds(draw_record_op->record, dom_node_id,
                                        text_runs);
    } else if ((*it)->GetType() == cc::PaintOpType::SetNodeId) {
      cc::SetNodeIdOp* set_node_id_op = static_cast<cc::SetNodeIdOp*>(*it);
      dom_node_id = set_node_id_op->node_id;
    } else if ((*it)->GetType() == cc::PaintOpType::DrawTextBlob) {
      cc::DrawTextBlobOp* draw_text_op = static_cast<cc::DrawTextBlobOp*>(*it);
      SkTextBlob::Iter iter(*draw_text_op->blob);
      SkTextBlob::Iter::Run run;
      while (iter.next(&run)) {
        TextRunDOMNodeIdInfo text_run_info;
        text_run_info.glyph_len = run.fGlyphCount;
        text_run_info.dom_node_id = dom_node_id;
        text_runs->push_back(text_run_info);
      }
    }
  }
}

}  // namespace

TEST_F(WebFrameTest, FirstLetterHasDOMNodeIdWhenPrinting) {
  // When printing, every DrawText painting op needs to have an associated
  // DOM Node ID. This test ensures that when the first-letter style is used,
  // the drawing op for the first letter is correctly associated with the same
  // DOM Node ID as the following text.

  // Load a web page with two elements containing the text
  // "Hello" and "World", where "World" has a first-letter style.
  RegisterMockedHttpURLLoad("first-letter.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "first-letter.html");

  // Print the page and capture the PaintRecord.
  WebPrintParams print_params;
  WebSize page_size(500, 500);
  print_params.print_content_area.width = page_size.width;
  print_params.print_content_area.height = page_size.height;
  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  EXPECT_EQ(1u, frame->PrintBegin(print_params, WebNode()));
  PaintRecorder recorder;
  frame->PrintPagesForTesting(recorder.beginRecording(IntRect()), page_size,
                              page_size);
  frame->PrintEnd();
  sk_sp<PaintRecord> paint_record = recorder.finishRecordingAsPicture();

  // Unpack the paint record and collect info about the text runs.
  std::vector<TextRunDOMNodeIdInfo> text_runs;
  RecursiveCollectTextRunDOMNodeIds(paint_record, 0, &text_runs);

  // The first text run should be "Hello".
  ASSERT_EQ(3U, text_runs.size());
  EXPECT_EQ(5, text_runs[0].glyph_len);
  EXPECT_NE(kInvalidDOMNodeId, text_runs[0].dom_node_id);

  // The second text run should be "W", the first letter of "World".
  EXPECT_EQ(1, text_runs[1].glyph_len);
  EXPECT_NE(kInvalidDOMNodeId, text_runs[1].dom_node_id);

  // The last text run should be "orld", the rest of "World".
  EXPECT_EQ(4, text_runs[2].glyph_len);
  EXPECT_NE(kInvalidDOMNodeId, text_runs[2].dom_node_id);

  // The second and third text runs should have the same DOM Node ID.
  EXPECT_EQ(text_runs[1].dom_node_id, text_runs[2].dom_node_id);
}

TEST_F(WebFrameTest, RightClickActivatesForExecuteCommand) {
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad("about:blank");
  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();

  // Setup a mock clipboard host.
  PageTestBase::MockClipboardHostProvider mock_clipboard_host_provider(
      frame->GetFrame()->GetBrowserInterfaceBroker());

  EXPECT_FALSE(frame->GetFrame()->HasStickyUserActivation());
  frame->ExecuteScript(WebScriptSource(WebString("document.execCommand('copy');")));
  EXPECT_FALSE(frame->GetFrame()->HasStickyUserActivation());

  // Right-click to activate the page.
  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = WebMouseEvent::Button::kRight;
  mouse_event.click_count = 1;
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_event, ui::LatencyInfo()));
  RunPendingTasks();

  frame->ExecuteCommand(WebString::FromUTF8("Paste"));
  EXPECT_TRUE(frame->GetFrame()->HasStickyUserActivation());
}

TEST_F(WebFrameTest, GetCanonicalUrlForSharingNone) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank");
  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  EXPECT_TRUE(frame->GetDocument().CanonicalUrlForSharing().IsNull());
}

TEST_F(WebFrameTest, GetCanonicalUrlForSharingNotInHead) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();
  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  frame_test_helpers::LoadHTMLString(
      frame, R"(
    <body>
      <link rel="canonical" href="https://example.com/canonical.html">
    </body>)", ToKURL("https://example.com/test_page.html"));
  EXPECT_TRUE(frame->GetDocument().CanonicalUrlForSharing().IsNull());
}

TEST_F(WebFrameTest, GetCanonicalUrlForSharing) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();
  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  frame_test_helpers::LoadHTMLString(
      frame, R"(
    <head>
      <link rel="canonical" href="https://example.com/canonical.html">
    </head>)", ToKURL("https://example.com/test_page.html"));
  EXPECT_EQ(WebURL(ToKURL("https://example.com/canonical.html")),
            frame->GetDocument().CanonicalUrlForSharing());
}

TEST_F(WebFrameTest, GetCanonicalUrlForSharingMultiple) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();
  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  frame_test_helpers::LoadHTMLString(
      frame, R"(
    <head>
      <link rel="canonical" href="https://example.com/canonical1.html">
      <link rel="canonical" href="https://example.com/canonical2.html">
    </head>)", ToKURL("https://example.com/test_page.html"));
  EXPECT_EQ(WebURL(ToKURL("https://example.com/canonical1.html")),
            frame->GetDocument().CanonicalUrlForSharing());
}

TEST_F(WebFrameTest, NavigationTimingInfo) {
  RegisterMockedHttpURLLoad("foo.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");
  ResourceTimingInfo* navigation_timing_info = web_view_helper.LocalMainFrame()
                                                   ->GetFrame()
                                                   ->Loader()
                                                   .GetDocumentLoader()
                                                   ->GetNavigationTimingInfo();
  EXPECT_EQ(navigation_timing_info->TransferSize(), static_cast<uint64_t>(34));
}

TEST_F(WebFrameSimTest, EnterFullscreenResetScrollAndScaleState) {
  UseAndroidSettings();
  WebView().MainFrameViewWidget()->Resize(gfx::Size(490, 500));
  WebView().EnableFakePageScaleAnimationForTesting(true);
  WebView().GetSettings()->SetTextAutosizingEnabled(false);
  WebView().SetDefaultPageScaleLimits(0.5f, 4);

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        body {
          margin: 0px;
          width: 10000px;
          height: 10000px;
        }
      </style>
  )HTML");

  Compositor().BeginFrame();

  // Make the page scale and scroll with the given parameters.
  EXPECT_EQ(0.5f, WebView().PageScaleFactor());
  WebView().SetPageScaleFactor(2.0f);
  WebView().MainFrameImpl()->SetScrollOffset(WebSize(94, 111));
  WebView().SetVisualViewportOffset(gfx::PointF(12, 20));
  EXPECT_EQ(2.0f, WebView().PageScaleFactor());
  EXPECT_EQ(94, WebView().MainFrameImpl()->GetScrollOffset().width);
  EXPECT_EQ(111, WebView().MainFrameImpl()->GetScrollOffset().height);
  EXPECT_EQ(12, WebView().VisualViewportOffset().x());
  EXPECT_EQ(20, WebView().VisualViewportOffset().y());

  auto* frame = To<LocalFrame>(WebView().GetPage()->MainFrame());
  Element* element = frame->GetDocument()->body();
  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  Fullscreen::RequestFullscreen(*element);
  WebView().DidEnterFullscreen();

  // Page scale factor must be 1.0 during fullscreen for elements to be sized
  // properly.
  EXPECT_EQ(1.0f, WebView().PageScaleFactor());

  // Confirm that exiting fullscreen restores back to default values.
  WebView().DidExitFullscreen();
  WebView().MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  EXPECT_EQ(0.5f, WebView().PageScaleFactor());
  EXPECT_EQ(94, WebView().MainFrameImpl()->GetScrollOffset().width);
  EXPECT_EQ(111, WebView().MainFrameImpl()->GetScrollOffset().height);
  EXPECT_EQ(0, WebView().VisualViewportOffset().x());
  EXPECT_EQ(0, WebView().VisualViewportOffset().y());
}

TEST_F(WebFrameSimTest, GetPageSizeType) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(500, 500));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        @page {}
      </style>
  )HTML");

  Compositor().BeginFrame();
  RunPendingTasks();

  const struct {
    const char* size;
    PageSizeType page_size_type;
  } test_cases[] = {
      {"auto", PageSizeType::kAuto},
      {"portrait", PageSizeType::kPortrait},
      {"landscape", PageSizeType::kLandscape},
      {"a4", PageSizeType::kFixed},
      {"letter", PageSizeType::kFixed},
      {"a4 portrait", PageSizeType::kFixed},
      {"letter landscape", PageSizeType::kFixed},
      {"10in", PageSizeType::kFixed},
      {"10in 12cm", PageSizeType::kFixed},
  };

  auto* main_frame = WebView().MainFrame()->ToWebLocalFrame();
  auto* doc = To<LocalFrame>(WebView().GetPage()->MainFrame())->GetDocument();
  auto* sheet = To<CSSStyleSheet>(doc->StyleSheets().item(0));
  CSSStyleDeclaration* style_decl =
      To<CSSPageRule>(sheet->cssRules(ASSERT_NO_EXCEPTION)->item(0))->style();

  // Initially empty @page rule.
  EXPECT_EQ(PageSizeType::kAuto, main_frame->GetPageSizeType(1));

  for (const auto& test : test_cases) {
    style_decl->setProperty(doc->GetExecutionContext(), "size", test.size, "",
                            ASSERT_NO_EXCEPTION);
    EXPECT_EQ(test.page_size_type, main_frame->GetPageSizeType(1));
  }
}

TEST_F(WebFrameSimTest, PageOrientation) {
  ScopedNamedPagesForTest named_pages_enabler(true);
  gfx::Size page_size(500, 500);
  WebView().MainFrameWidget()->Resize(page_size);

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        @page upright { page-orientation: upright; }
        @page clockwise { page-orientation: rotate-right; }
        @page counter-clockwise { page-orientation: rotate-left; }
        div { height: 10px; }
      </style>
      <!-- First page: -->
      <div style="page:upright;"></div>
      <!-- Second page: -->
      <div style="page:counter-clockwise;"></div>
      <!-- Third page: -->
      <div style="page:clockwise;"></div>
      <div style="page:clockwise;"></div>
      <!-- Fourth page: -->
      <div></div>
  )HTML");

  Compositor().BeginFrame();
  RunPendingTasks();

  auto* frame = WebView().MainFrame()->ToWebLocalFrame();
  WebPrintParams print_params;
  print_params.print_content_area.width = page_size.width();
  print_params.print_content_area.height = page_size.height();
  EXPECT_EQ(4u, frame->PrintBegin(print_params, WebNode()));

  WebPrintPageDescription description;

  frame->GetPageDescription(0, &description);
  EXPECT_EQ(description.orientation, PageOrientation::kUpright);

  frame->GetPageDescription(1, &description);
  EXPECT_EQ(description.orientation, PageOrientation::kRotateLeft);

  frame->GetPageDescription(2, &description);
  EXPECT_EQ(description.orientation, PageOrientation::kRotateRight);

  frame->GetPageDescription(3, &description);
  EXPECT_EQ(description.orientation, PageOrientation::kUpright);

  frame->PrintEnd();
}

TEST_F(WebFrameSimTest, MainFrameTransformOffsetPixelSnapped) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <iframe id="iframe" style="position:absolute;top:7px;left:13.5px;border:none"></iframe>
  )HTML");
  frame_test_helpers::TestWebRemoteFrameClient remote_frame_client;
  TestViewportIntersection remote_frame_host;
  remote_frame_host.Init(remote_frame_client.GetRemoteAssociatedInterfaces());
  WebRemoteFrame* remote_frame =
      frame_test_helpers::CreateRemote(&remote_frame_client);
  MainFrame().FirstChild()->Swap(remote_frame);
  Compositor().BeginFrame();
  RunPendingTasks();
  EXPECT_TRUE(remote_frame_host.GetIntersectionState()
                  ->main_frame_transform.IsIdentityOrIntegerTranslation());
  EXPECT_EQ(remote_frame_host.GetIntersectionState()
                ->main_frame_transform.matrix()
                .get(0, 3),
            14.f);
  EXPECT_EQ(remote_frame_host.GetIntersectionState()
                ->main_frame_transform.matrix()
                .get(1, 3),
            7.f);
  MainFrame().FirstChild()->Detach();
}

TEST_F(WebFrameTest, MediaQueriesInLocalFrameInsideRemote) {
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote();

  FixedLayoutTestWebWidgetClient client;
  client.screen_info_.is_monochrome = false;
  client.screen_info_.depth_per_component = 8;

  WebLocalFrameImpl* local_frame = frame_test_helpers::CreateLocalChild(
      *helper.RemoteMainFrame(), WebString(), WebFrameOwnerProperties(),
      nullptr, nullptr, &client);

  ASSERT_TRUE(local_frame->GetFrame());
  MediaValues* media_values =
      MediaValues::CreateDynamicIfFrameExists(local_frame->GetFrame());
  ASSERT_TRUE(media_values);
  EXPECT_EQ(0, media_values->MonochromeBitsPerComponent());
  EXPECT_EQ(8, media_values->ColorBitsPerComponent());
  // Need to explicitly reset helper to make sure local_frame is not deleted
  // first.
  helper.Reset();
}

TEST_F(WebFrameTest, RemoteViewportAndMainframeIntersections) {
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote();
  WebLocalFrameImpl* local_frame = frame_test_helpers::CreateLocalChild(
      *helper.RemoteMainFrame(), "frameName");
  frame_test_helpers::LoadHTMLString(local_frame, R"HTML(
      <!DOCTYPE html>
      <style>
      #target {
        position: absolute;
        top: 10px;
        left: 20px;
        width: 200px;
        height: 100px;
      }
      </style>
      <div id=target></div>
      )HTML",
                                     ToKURL("about:blank"));

  Element* target =
      local_frame->GetFrame()->GetDocument()->getElementById("target");
  ASSERT_TRUE(target);
  ASSERT_TRUE(target->GetLayoutObject());

  // Simulate the local child frame being positioned at (7, -11) in the parent's
  // viewport, indicating that the top 11px of the child's content is clipped
  // by the parent. Let the local child frame be at (7, 40) in the parent
  // element.
  WebFrameWidget* widget = local_frame->FrameWidget();
  ASSERT_TRUE(widget);
  gfx::Transform viewport_transform;
  viewport_transform.Translate(7, -11);
  gfx::Rect viewport_intersection(0, 11, 200, 89);
  gfx::Rect mainframe_intersection(0, 0, 200, 140);
  blink::mojom::FrameOcclusionState occlusion_state =
      blink::mojom::FrameOcclusionState::kUnknown;

  static_cast<WebFrameWidgetBase*>(widget)->SetRemoteViewportIntersection(
      {viewport_intersection, mainframe_intersection, viewport_intersection,
       occlusion_state, gfx::Size(), gfx::Point(), viewport_transform});

  // The viewport intersection should be applied by the layout geometry mapping
  // code when these flags are used.
  int viewport_intersection_flags =
      kTraverseDocumentBoundaries | kApplyRemoteMainFrameTransform;

  // Expectation is: (target location) + (viewport offset) = (20, 10) + (7, -11)
  PhysicalOffset offset = target->GetLayoutObject()->LocalToAbsolutePoint(
      PhysicalOffset(), viewport_intersection_flags);
  EXPECT_EQ(PhysicalOffset(27, -1), offset);

  PhysicalRect rect(0, 0, 25, 35);
  local_frame->GetFrame()
      ->GetDocument()
      ->GetLayoutView()
      ->MapToVisualRectInAncestorSpace(
          nullptr, rect, viewport_intersection_flags, kDefaultVisualRectFlags);
  EXPECT_EQ(PhysicalRect(7, 0, 25, 24), rect);

  // Without the main frame overflow clip the rect should not be clipped and the
  // coordinates returned are the rects coordinates in the viewport space.
  PhysicalRect mainframe_rect(0, 0, 25, 35);
  local_frame->GetFrame()
      ->GetDocument()
      ->GetLayoutView()
      ->MapToVisualRectInAncestorSpace(nullptr, mainframe_rect,
                                       viewport_intersection_flags,
                                       kDontApplyMainFrameOverflowClip);
  EXPECT_EQ(PhysicalRect(7, -11, 25, 35), mainframe_rect);
}

class TestUpdateFaviconURLLocalFrameHost : public FakeLocalFrameHost {
 public:
  TestUpdateFaviconURLLocalFrameHost() = default;
  ~TestUpdateFaviconURLLocalFrameHost() override = default;

  // FakeLocalFrameHost:
  void UpdateFaviconURL(
      WTF::Vector<blink::mojom::blink::FaviconURLPtr> favicon_urls) override {
    did_notify_ = true;
  }

  bool did_notify_ = false;
};

// Ensure the render view sends favicon url update events correctly.
TEST_F(WebFrameTest, FaviconURLUpdateEvent) {
  TestUpdateFaviconURLLocalFrameHost frame_host;
  frame_test_helpers::TestWebFrameClient web_frame_client;
  frame_host.Init(web_frame_client.GetRemoteNavigationAssociatedInterfaces());
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(&web_frame_client);
  RunPendingTasks();

  WebViewImpl* web_view = web_view_helper.GetWebView();
  LocalFrame* frame = web_view->MainFrameImpl()->GetFrame();

  // An event should be sent when a favicon url exists.
  frame->GetDocument()->documentElement()->setInnerHTML(
      "<html>"
      "<head>"
      "<link rel='icon' href='http://www.google.com/favicon.ico'>"
      "</head>"
      "</html>");
  RunPendingTasks();

  EXPECT_TRUE(frame_host.did_notify_);

  frame_host.did_notify_ = false;

  // An event should not be sent if no favicon url exists. This is an assumption
  // made by some of Chrome's favicon handling.
  frame->GetDocument()->documentElement()->setInnerHTML(
      "<html>"
      "<head>"
      "</head>"
      "</html>");
  RunPendingTasks();

  EXPECT_FALSE(frame_host.did_notify_);
  web_view_helper.Reset();
}

class TestFocusedElementChangedLocalFrameHost : public FakeLocalFrameHost {
 public:
  TestFocusedElementChangedLocalFrameHost() = default;
  ~TestFocusedElementChangedLocalFrameHost() override = default;

  // FakeLocalFrameHost:
  void FocusedElementChanged(bool is_editable_element,
                             const gfx::Rect& bounds_in_frame_widget,
                             blink::mojom::FocusType focus_type) override {
    did_notify_ = true;
  }

  bool did_notify_ = false;
};

TEST_F(WebFrameTest, FocusElementCallsFocusedElementChanged) {
  TestFocusedElementChangedLocalFrameHost frame_host;
  frame_test_helpers::TestWebFrameClient web_frame_client;
  frame_host.Init(web_frame_client.GetRemoteNavigationAssociatedInterfaces());
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(&web_frame_client);
  RunPendingTasks();
  auto* main_frame = web_view_helper.GetWebView()->MainFrameImpl();

  main_frame->GetFrame()->GetDocument()->documentElement()->setInnerHTML(
      "<input id='test1' value='hello1'></input>"
      "<input id='test2' value='hello2'></input>");
  RunPendingTasks();

  EXPECT_FALSE(frame_host.did_notify_);

  main_frame->ExecuteScript(
      WebScriptSource(WebString("document.getElementById('test1').focus();")));
  RunPendingTasks();
  EXPECT_TRUE(frame_host.did_notify_);
  frame_host.did_notify_ = false;

  main_frame->ExecuteScript(
      WebScriptSource(WebString("document.getElementById('test2').focus();")));
  RunPendingTasks();
  EXPECT_TRUE(frame_host.did_notify_);
  frame_host.did_notify_ = false;

  main_frame->ExecuteScript(
      WebScriptSource(WebString("document.getElementById('test2').blur();")));
  RunPendingTasks();
  EXPECT_TRUE(frame_host.did_notify_);
}

// Tests that form.submit() cancels any navigations already sent to the browser
// process.
TEST_F(WebFrameTest, FormSubmitCancelsNavigation) {
  frame_test_helpers::TestWebFrameClient web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(&web_frame_client);
  RegisterMockedHttpURLLoad("foo.html");
  RegisterMockedHttpURLLoad("bar.html");
  auto* main_frame = web_view_helper.GetWebView()->MainFrameImpl();
  auto* local_frame = main_frame->GetFrame();
  auto* window = local_frame->DomWindow();

  window->document()->documentElement()->setInnerHTML(
      "<form id=formid action='http://internal.test/bar.html'></form>");
  ASSERT_FALSE(local_frame->Loader().HasProvisionalNavigation());

  FrameLoadRequest request(window,
                           ResourceRequest("http://internal.test/foo.html"));
  local_frame->Navigate(request, WebFrameLoadType::kStandard);
  ASSERT_TRUE(local_frame->Loader().HasProvisionalNavigation());

  main_frame->ExecuteScript(WebScriptSource(WebString("formid.submit()")));
  EXPECT_FALSE(local_frame->Loader().HasProvisionalNavigation());

  RunPendingTasks();
}

class TestLocalFrameHostForAnchorWithDownloadAttr : public FakeLocalFrameHost {
 public:
  TestLocalFrameHostForAnchorWithDownloadAttr() = default;
  ~TestLocalFrameHostForAnchorWithDownloadAttr() override = default;

  // FakeLocalFrameHost:
  void DownloadURL(mojom::blink::DownloadURLParamsPtr params) override {
    referrer_ = params->referrer ? params->referrer->url : KURL();
    referrer_policy_ = params->referrer
                           ? params->referrer->policy
                           : ReferrerUtils::MojoReferrerPolicyResolveDefault(
                                 network::mojom::ReferrerPolicy::kDefault);
  }

  KURL referrer_;
  network::mojom::ReferrerPolicy referrer_policy_;
};

TEST_F(WebFrameTest, DownloadReferrerPolicy) {
  TestLocalFrameHostForAnchorWithDownloadAttr frame_host;
  frame_test_helpers::TestWebFrameClient web_frame_client;
  frame_host.Init(web_frame_client.GetRemoteNavigationAssociatedInterfaces());
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(&web_frame_client);

  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  KURL test_url = ToKURL("http://www.test.com/foo/index.html");
  // 1.<meta name='referrer' content='no-referrer'>
  frame_test_helpers::LoadHTMLString(
      frame, GetHTMLStringForReferrerPolicy("no-referrer", std::string()),
      test_url);
  EXPECT_TRUE(frame_host.referrer_.IsEmpty());
  EXPECT_EQ(frame_host.referrer_policy_,
            network::mojom::ReferrerPolicy::kNever);

  // 2.<meta name='referrer' content='origin'>
  frame_test_helpers::LoadHTMLString(
      frame, GetHTMLStringForReferrerPolicy("origin", std::string()), test_url);
  EXPECT_EQ(frame_host.referrer_, ToKURL("http://www.test.com/"));
  EXPECT_EQ(frame_host.referrer_policy_,
            network::mojom::ReferrerPolicy::kOrigin);

  // 3.Without any declared referrer-policy attribute
  frame_test_helpers::LoadHTMLString(
      frame, GetHTMLStringForReferrerPolicy(std::string(), std::string()),
      test_url);
  EXPECT_EQ(frame_host.referrer_, test_url);
  EXPECT_EQ(frame_host.referrer_policy_,
            ReferrerUtils::MojoReferrerPolicyResolveDefault(
                network::mojom::ReferrerPolicy::kDefault));

  // 4.referrerpolicy='origin'
  frame_test_helpers::LoadHTMLString(
      frame, GetHTMLStringForReferrerPolicy(std::string(), "origin"), test_url);
  EXPECT_EQ(frame_host.referrer_, ToKURL("http://www.test.com/"));
  EXPECT_EQ(frame_host.referrer_policy_,
            network::mojom::ReferrerPolicy::kOrigin);

  // 5.referrerpolicy='same-origin'
  frame_test_helpers::LoadHTMLString(
      frame, GetHTMLStringForReferrerPolicy(std::string(), "same-origin"),
      test_url);
  EXPECT_EQ(frame_host.referrer_, test_url);
  EXPECT_EQ(frame_host.referrer_policy_,
            network::mojom::ReferrerPolicy::kSameOrigin);

  // 6.referrerpolicy='no-referrer'
  frame_test_helpers::LoadHTMLString(
      frame, GetHTMLStringForReferrerPolicy(std::string(), "no-referrer"),
      test_url);
  EXPECT_TRUE(frame_host.referrer_.IsEmpty());
  EXPECT_EQ(frame_host.referrer_policy_,
            network::mojom::ReferrerPolicy::kNever);
  web_view_helper.Reset();
}

}  // namespace blink
