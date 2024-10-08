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

#include <array>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <tuple>

#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/layers/picture_layer.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "cc/paint/paint_recorder.h"
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
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/common/page/launching_process_state.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/common/widget/device_emulation_params.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"
#include "third_party/blink/public/mojom/blob/data_element.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom-blink.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom-blink.h"
#include "third_party/blink/public/mojom/page_state/page_state.mojom-blink.h"
#include "third_party/blink/public/mojom/scroll/scrollbar_mode.mojom-blink.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom-blink.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/test/test_web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_navigation_timings.h"
#include "third_party/blink/public/web/web_navigation_type.h"
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
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/text_finder.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/spellcheck/idle_spell_check_controller.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
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
#include "third_party/blink/renderer/core/frame/web_remote_frame_impl.h"
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
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/messaging/blink_cloneable_message.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/drag_image.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scoped_page_pauser.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_test_suite.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/fake_local_frame_host.h"
#include "third_party/blink/renderer/core/testing/fake_remote_frame_host.h"
#include "third_party/blink/renderer/core/testing/fake_remote_main_frame_host.h"
#include "third_party/blink/renderer/core/testing/mock_clipboard_host.h"
#include "third_party/blink/renderer/core/testing/mock_policy_container_host.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/scoped_fake_plugin_registry.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/core/testing/wait_for_event.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/blob/testing/fake_blob.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_client.h"
#include "third_party/blink/renderer/platform/runtime_feature_state/runtime_feature_state_override_context.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/find_cc_layer.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "ui/base/ime/mojom/text_input_state.mojom-blink.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/gfx/geometry/transform.h"
#include "v8/include/v8.h"

using blink::mojom::SelectionMenuBehavior;
using blink::test::RunPendingTasks;
using blink::url_test_helpers::ToKURL;
using testing::_;
using testing::ElementsAre;
using testing::Mock;
using testing::Return;
using testing::UnorderedElementsAre;

namespace blink {

namespace {

const ScrollPaintPropertyNode* GetScrollNode(const LayoutObject& scroller) {
  if (auto* properties = scroller.FirstFragment().PaintProperties())
    return properties->Scroll();
  return nullptr;
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

// A helper function to execute the given `scripts` in the main world of the
// specified `frame`.
void ExecuteScriptsInMainWorld(
    WebLocalFrame* frame,
    base::span<const String> scripts,
    WebScriptExecutionCallback callback,
    mojom::blink::PromiseResultOption wait_for_promise =
        mojom::blink::PromiseResultOption::kAwait,
    mojom::blink::UserActivationOption user_gesture =
        mojom::blink::UserActivationOption::kDoNotActivate) {
  Vector<WebScriptSource> sources;
  for (auto script : scripts)
    sources.push_back(WebScriptSource(script));
  frame->RequestExecuteScript(
      DOMWrapperWorld::kMainWorldId, sources, user_gesture,
      mojom::blink::EvaluationTiming::kSynchronous,
      mojom::blink::LoadEventBlockingOption::kDoNotBlock, std::move(callback),
      BackForwardCacheAware::kAllow,
      mojom::blink::WantResultOption::kWantResult, wait_for_promise);
}

// Same as above, but for a single script.
void ExecuteScriptInMainWorld(
    WebLocalFrame* frame,
    String script_string,
    WebScriptExecutionCallback callback,
    mojom::blink::PromiseResultOption wait_for_promise =
        mojom::blink::PromiseResultOption::kAwait,
    mojom::blink::UserActivationOption user_gesture =
        mojom::blink::UserActivationOption::kDoNotActivate) {
  ExecuteScriptsInMainWorld(frame, base::span_from_ref(script_string),
                            std::move(callback), wait_for_promise,
                            user_gesture);
}

}  // namespace

const int kTouchPointPadding = 32;

const cc::OverscrollBehavior kOverscrollBehaviorAuto =
    cc::OverscrollBehavior(cc::OverscrollBehavior::Type::kAuto);

const cc::OverscrollBehavior kOverscrollBehaviorContain =
    cc::OverscrollBehavior(cc::OverscrollBehavior::Type::kContain);

const cc::OverscrollBehavior kOverscrollBehaviorNone =
    cc::OverscrollBehavior(cc::OverscrollBehavior::Type::kNone);

class WebFrameTest : public PageTestBase {
 protected:
  WebFrameTest()
      : base_url_("http://internal.test/"),
        not_base_url_("http://external.test/"),
        chrome_url_("chrome://test/") {
    // This is needed so that a chrome: URL's origin is computed correctly,
    // which is needed for Javascript URL security checks to work properly in
    // tests below.
    url::AddStandardScheme("chrome", url::SCHEME_WITH_HOST);
  }

  ~WebFrameTest() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  void DisableRendererSchedulerThrottling() {
    // Make sure that the RendererScheduler is foregrounded to avoid getting
    // throttled.
    if (kLaunchingProcessIsBackgrounded) {
      ThreadScheduler::Current()
          ->ToMainThreadScheduler()
          ->SetRendererBackgroundedForTesting(false);
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
    std::string full_string = base_url_ + file_name;
    KURL url = ToKURL(full_string);
    WebURLResponse response = WebURLResponse(url);
    response.SetMimeType("text/html");
    response.AddHttpHeaderField(
        report_only ? WebString("Content-Security-Policy-Report-Only")
                    : WebString("Content-Security-Policy"),
        WebString::FromUTF8(csp));
    RegisterMockedURLLoadWithCustomResponse(
        url, test::CoreTestDataPath(WebString::FromUTF8(file_name)), response);
  }

  void RegisterMockedHttpURLLoadWithMimeType(const std::string& file_name,
                                             const std::string& mime_type) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via the WebViewHelper instance in each test case.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), test::CoreTestDataPath(),
        WebString::FromUTF8(file_name), WebString::FromUTF8(mime_type));
  }

  static void ConfigureAndroid(WebSettings* settings) {
    frame_test_helpers::WebViewHelper::UpdateAndroidCompositingSettings(
        settings);
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
    web_view_helper->GetWebView()->MainFrameWidget()->SetFocus(true);
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
    Element* element =
        frame->GetDocument()->getElementById(AtomicString(testcase.c_str()));
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
    frame.GetDocument()->View()->UpdateAllLifecyclePhasesForTest();
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
      node_count += base::ranges::count_if(
          markers_in_node, [start_offset, end_offset, &node, &start_container,
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
      gfx::Rect& element_bounds,
      gfx::Rect& caret_bounds) {
    Element* element = helper.GetWebView()->FocusedElement();
    gfx::Rect caret_in_viewport, unused;
    helper.GetWebView()->MainFrameViewWidget()->CalculateSelectionBounds(
        caret_in_viewport, unused);
    caret_bounds =
        helper.GetWebView()->GetPage()->GetVisualViewport().ViewportToRootFrame(
            caret_in_viewport);
    element_bounds = element->GetDocument().View()->ConvertToRootFrame(
        ToPixelSnappedRect(element->Node::BoundingBox()));
  }

  std::string base_url_;
  std::string not_base_url_;
  std::string chrome_url_;

  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
  url::ScopedSchemeRegistryForTests scoped_registry_;
};

TEST_F(WebFrameTest, ContentText) {
  RegisterMockedHttpURLLoad("iframes_test.html");
  RegisterMockedHttpURLLoad("visible_iframe.html");
  RegisterMockedHttpURLLoad("invisible_iframe.html");
  RegisterMockedHttpURLLoad("zero_sized_iframe.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "iframes_test.html");

  // Now retrieve the frames text and test it only includes visible elements.
  std::string content = TestWebFrameContentDumper::DumpWebViewAsText(
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

  v8::HandleScope scope(web_view_helper.GetAgentGroupScheduler().Isolate());
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

class ScriptExecutionCallbackHelper final {
 public:
  // Returns true if the callback helper was ever invoked.
  bool DidComplete() const { return did_complete_; }

  WebScriptExecutionCallback Callback() {
    return WTF::BindOnce(&ScriptExecutionCallbackHelper::Completed,
                         WTF::Unretained(this));
  }

  // Returns true if any results (even if they were empty) were passed to the
  // callback helper. This is generally false if the execution context was
  // invalidated while running the script.
  bool HasAnyResults() const { return !!result_; }

  // Returns the single value returned from the execution.
  String SingleStringValue() const {
    if (!result_) {
      ADD_FAILURE() << "Expected a single result, but found nullopt";
      return String();
    }
    if (const std::string* str = result_->GetIfString())
      return String(*str);

    ADD_FAILURE() << "Type mismatch (not string)";
    return String();
  }
  bool SingleBoolValue() const {
    if (!result_) {
      ADD_FAILURE() << "Expected a single result, but found nullopt";
      return false;
    }
    if (std::optional<bool> b = result_->GetIfBool()) {
      return *b;
    }

    ADD_FAILURE() << "Type mismatch (not bool)";
    return false;
  }

 private:
  void Completed(std::optional<base::Value> value, base::TimeTicks start_time) {
    did_complete_ = true;
    result_ = std::move(value);
  }

 private:
  bool did_complete_ = false;
  std::optional<base::Value> result_;
};

TEST_F(WebFrameTest, RequestExecuteScript) {
  RegisterMockedHttpURLLoad("foo.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");

  v8::HandleScope scope(web_view_helper.GetAgentGroupScheduler().Isolate());
  ScriptExecutionCallbackHelper callback_helper;
  ExecuteScriptInMainWorld(web_view_helper.GetWebView()->MainFrameImpl(),
                           "'hello';", callback_helper.Callback());
  RunPendingTasks();
  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_EQ("hello", callback_helper.SingleStringValue());
}

TEST_F(WebFrameTest, SuspendedRequestExecuteScript) {
  RegisterMockedHttpURLLoad("foo.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");

  v8::HandleScope scope(web_view_helper.GetAgentGroupScheduler().Isolate());
  ScriptExecutionCallbackHelper callback_helper;

  // Suspend scheduled tasks so the script doesn't run.
  web_view_helper.GetWebView()->GetPage()->SetPaused(true);
  ExecuteScriptInMainWorld(web_view_helper.GetWebView()->MainFrameImpl(),
                           "'hello';", callback_helper.Callback());
  RunPendingTasks();
  EXPECT_FALSE(callback_helper.DidComplete());

  web_view_helper.Reset();
  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_FALSE(callback_helper.HasAnyResults());
}

TEST_F(WebFrameTest, ExecuteScriptWithError) {
  RegisterMockedHttpURLLoad("foo.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");

  v8::Isolate* isolate = web_view_helper.GetAgentGroupScheduler().Isolate();
  v8::HandleScope scope(isolate);
  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);
  ScriptExecutionCallbackHelper callback_helper;
  ExecuteScriptInMainWorld(web_view_helper.GetWebView()->MainFrameImpl(),
                           "foo = bar; 'hello';", callback_helper.Callback());
  RunPendingTasks();
  EXPECT_TRUE(callback_helper.DidComplete());
  // Even though an error is thrown here, it's swallowed by one of the
  // script runner classes, so the caller never sees it. Instead, the error
  // is represented by an empty V8Value (stringified to an empty string).
  EXPECT_FALSE(try_catch.HasCaught());
  EXPECT_FALSE(callback_helper.HasAnyResults());
}

TEST_F(WebFrameTest, ExecuteScriptWithPromiseWithoutWait) {
  RegisterMockedHttpURLLoad("foo.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");

  constexpr char kScript[] = R"(Promise.resolve('hello');)";

  v8::HandleScope scope(web_view_helper.GetAgentGroupScheduler().Isolate());
  ScriptExecutionCallbackHelper callback_helper;
  ExecuteScriptInMainWorld(web_view_helper.GetWebView()->MainFrameImpl(),
                           kScript, callback_helper.Callback(),
                           mojom::blink::PromiseResultOption::kDoNotWait);
  RunPendingTasks();
  // Since the caller specified the script shouldn't wait for the promise to
  // be resolved, the callback should have completed normally and the result
  // value should be the promise.
  // As `V8ValueConverterForTest` fails to convert the promise to `base::Value`,
  // the callback receives `std::nullopt`.
  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_FALSE(callback_helper.HasAnyResults());
}

TEST_F(WebFrameTest, ExecuteScriptWithPromiseFulfilled) {
  RegisterMockedHttpURLLoad("foo.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");

  constexpr char kScript[] = R"(Promise.resolve('hello');)";

  v8::HandleScope scope(web_view_helper.GetAgentGroupScheduler().Isolate());
  ScriptExecutionCallbackHelper callback_helper;
  ExecuteScriptInMainWorld(web_view_helper.GetWebView()->MainFrameImpl(),
                           kScript, callback_helper.Callback());
  RunPendingTasks();
  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_EQ("hello", callback_helper.SingleStringValue());
}

TEST_F(WebFrameTest, ExecuteScriptWithPromiseRejected) {
  RegisterMockedHttpURLLoad("foo.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");

  constexpr char kScript[] = R"(Promise.reject('hello');)";

  v8::HandleScope scope(web_view_helper.GetAgentGroupScheduler().Isolate());
  ScriptExecutionCallbackHelper callback_helper;
  ExecuteScriptInMainWorld(web_view_helper.GetWebView()->MainFrameImpl(),
                           kScript, callback_helper.Callback());
  RunPendingTasks();
  EXPECT_TRUE(callback_helper.DidComplete());
  // Promise rejection, similar to errors, are represented by `std::nullopt`
  // passed to the callback.
  EXPECT_FALSE(callback_helper.HasAnyResults());
}

TEST_F(WebFrameTest, ExecuteScriptWithFrameRemovalBeforePromiseResolves) {
  RegisterMockedHttpURLLoad("single_iframe.html");
  RegisterMockedHttpURLLoad("visible_iframe.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "single_iframe.html");

  v8::HandleScope scope(web_view_helper.GetAgentGroupScheduler().Isolate());

  constexpr char kScript[] = R"((new Promise((r) => {}));)";

  WebLocalFrame* iframe =
      web_view_helper.LocalMainFrame()->FirstChild()->ToWebLocalFrame();
  ScriptExecutionCallbackHelper callback_helper;
  ExecuteScriptInMainWorld(iframe, kScript, callback_helper.Callback());
  RunPendingTasks();
  EXPECT_FALSE(callback_helper.DidComplete());

  constexpr char kRemoveFrameScript[] =
      "var iframe = document.getElementsByTagName('iframe')[0]; "
      "document.body.removeChild(iframe);";
  web_view_helper.LocalMainFrame()->ExecuteScript(
      WebScriptSource(kRemoveFrameScript));
  RunPendingTasks();

  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_FALSE(callback_helper.HasAnyResults());
}

TEST_F(WebFrameTest, ExecuteScriptWithMultiplePromises) {
  RegisterMockedHttpURLLoad("foo.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");

  const String scripts[] = {
      "Promise.resolve('hello');",
      "Promise.resolve('world');",
  };

  v8::HandleScope scope(web_view_helper.GetAgentGroupScheduler().Isolate());
  ScriptExecutionCallbackHelper callback_helper;
  ExecuteScriptsInMainWorld(web_view_helper.GetWebView()->MainFrameImpl(),
                            scripts, callback_helper.Callback());
  RunPendingTasks();
  EXPECT_TRUE(callback_helper.DidComplete());
  // The result of the last script is returned.
  EXPECT_EQ("world", callback_helper.SingleStringValue());
}

TEST_F(WebFrameTest, ExecuteScriptWithMultiplePromisesWithDelayedSettlement) {
  RegisterMockedHttpURLLoad("foo.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");

  const String scripts[] = {
      "Promise.resolve('hello');",
      "(new Promise((r) => { window.resolveSecond = r; }));",
  };

  v8::HandleScope scope(web_view_helper.GetAgentGroupScheduler().Isolate());
  ScriptExecutionCallbackHelper callback_helper;
  ExecuteScriptsInMainWorld(web_view_helper.GetWebView()->MainFrameImpl(),
                            scripts, callback_helper.Callback());
  RunPendingTasks();
  EXPECT_FALSE(callback_helper.DidComplete());

  {
    ScriptExecutionCallbackHelper second_callback_helper;
    ExecuteScriptInMainWorld(web_view_helper.GetWebView()->MainFrameImpl(),
                             String("window.resolveSecond('world');"),
                             second_callback_helper.Callback());
    RunPendingTasks();
    EXPECT_TRUE(second_callback_helper.DidComplete());
    // `undefined` is mapped to `nullopt`.
    EXPECT_FALSE(second_callback_helper.HasAnyResults());
  }

  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_EQ("world", callback_helper.SingleStringValue());
}

TEST_F(WebFrameTest, ExecuteScriptWithMultipleSourcesWhereFirstIsPromise) {
  RegisterMockedHttpURLLoad("foo.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");

  const String scripts[] = {
      "Promise.resolve('hello');",
      "'world';",
  };

  v8::HandleScope scope(web_view_helper.GetAgentGroupScheduler().Isolate());
  ScriptExecutionCallbackHelper callback_helper;
  ExecuteScriptsInMainWorld(web_view_helper.GetWebView()->MainFrameImpl(),
                            scripts, callback_helper.Callback());
  RunPendingTasks();

  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_EQ("world", callback_helper.SingleStringValue());
}

TEST_F(WebFrameTest, ExecuteScriptWithMultipleSourcesWhereLastIsPromise) {
  RegisterMockedHttpURLLoad("foo.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");

  const String scripts[] = {
      "'hello';",
      "Promise.resolve('world');",
  };

  v8::HandleScope scope(web_view_helper.GetAgentGroupScheduler().Isolate());
  ScriptExecutionCallbackHelper callback_helper;
  ExecuteScriptsInMainWorld(web_view_helper.GetWebView()->MainFrameImpl(),
                            scripts, callback_helper.Callback());
  RunPendingTasks();

  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_EQ("world", callback_helper.SingleStringValue());
}

TEST_F(WebFrameTest, ExecuteScriptWithPromisesWhereOnlyFirstIsFulfilled) {
  RegisterMockedHttpURLLoad("foo.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");

  String scripts[] = {
      "Promise.resolve('hello');",
      "Promise.reject('world');",
  };

  v8::HandleScope scope(web_view_helper.GetAgentGroupScheduler().Isolate());
  ScriptExecutionCallbackHelper callback_helper;
  ExecuteScriptsInMainWorld(web_view_helper.GetWebView()->MainFrameImpl(),
                            scripts, callback_helper.Callback());
  RunPendingTasks();
  EXPECT_TRUE(callback_helper.DidComplete());
  // Promise rejection, similar to errors, are represented by `std::nullopt`
  // passed to the callback.
  EXPECT_FALSE(callback_helper.HasAnyResults());
}

TEST_F(WebFrameTest, ExecuteScriptWithPromisesWhereOnlyLastIsFulfilled) {
  RegisterMockedHttpURLLoad("foo.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");

  String scripts[] = {
      "Promise.reject('hello');",
      "Promise.resolve('world');",
  };

  v8::HandleScope scope(web_view_helper.GetAgentGroupScheduler().Isolate());
  ScriptExecutionCallbackHelper callback_helper;
  ExecuteScriptsInMainWorld(web_view_helper.GetWebView()->MainFrameImpl(),
                            scripts, callback_helper.Callback());
  RunPendingTasks();
  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_EQ("world", callback_helper.SingleStringValue());
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

  v8::Isolate* isolate = web_view_helper.GetAgentGroupScheduler().Isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context =
      web_view_helper.LocalMainFrame()->MainWorldScriptContext();
  ScriptExecutionCallbackHelper callback_helper;
  v8::Local<v8::Function> function =
      v8::Function::New(context, callback).ToLocalChecked();
  v8::Local<v8::Value> args[] = {v8::Undefined(isolate),
                                 V8String(isolate, "hello")};
  web_view_helper.GetWebView()
      ->MainFrame()
      ->ToWebLocalFrame()
      ->RequestExecuteV8Function(context, function, v8::Undefined(isolate),
                                 std::size(args), args,
                                 callback_helper.Callback());
  RunPendingTasks();
  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_EQ("hello", callback_helper.SingleStringValue());
}

TEST_F(WebFrameTest, RequestExecuteV8FunctionWhileSuspended) {
  DisableRendererSchedulerThrottling();
  RegisterMockedHttpURLLoad("foo.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");

  auto callback = [](const v8::FunctionCallbackInfo<v8::Value>& info) {
    info.GetReturnValue().Set(V8String(info.GetIsolate(), "hello"));
  };

  v8::HandleScope scope(web_view_helper.GetAgentGroupScheduler().Isolate());
  v8::Local<v8::Context> context =
      web_view_helper.LocalMainFrame()->MainWorldScriptContext();

  // Suspend scheduled tasks so the script doesn't run.
  WebLocalFrameImpl* main_frame = web_view_helper.LocalMainFrame();
  web_view_helper.GetWebView()->GetPage()->SetPaused(true);

  ScriptExecutionCallbackHelper callback_helper;
  v8::Local<v8::Function> function =
      v8::Function::New(context, callback).ToLocalChecked();
  main_frame->RequestExecuteV8Function(context, function,
                                       v8::Undefined(context->GetIsolate()), 0,
                                       nullptr, callback_helper.Callback());
  RunPendingTasks();
  EXPECT_FALSE(callback_helper.DidComplete());

  web_view_helper.GetWebView()->GetPage()->SetPaused(false);
  RunPendingTasks();
  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_EQ("hello", callback_helper.SingleStringValue());
}

TEST_F(WebFrameTest, RequestExecuteV8FunctionWhileSuspendedWithUserGesture) {
  DisableRendererSchedulerThrottling();
  RegisterMockedHttpURLLoad("foo.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");

  v8::HandleScope scope(web_view_helper.GetAgentGroupScheduler().Isolate());

  // Suspend scheduled tasks so the script doesn't run.
  web_view_helper.GetWebView()->GetPage()->SetPaused(true);
  LocalFrame::NotifyUserActivation(
      web_view_helper.LocalMainFrame()->GetFrame(),
      mojom::UserActivationNotificationType::kTest);
  ScriptExecutionCallbackHelper callback_helper;
  ExecuteScriptInMainWorld(web_view_helper.GetWebView()->MainFrameImpl(),
                           "navigator.userActivation.isActive;",
                           callback_helper.Callback());
  RunPendingTasks();
  EXPECT_FALSE(callback_helper.DidComplete());

  web_view_helper.GetWebView()->GetPage()->SetPaused(false);
  RunPendingTasks();
  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_TRUE(callback_helper.SingleBoolValue());
}

TEST_F(WebFrameTest, IframeScriptRemovesSelf) {
  RegisterMockedHttpURLLoad("single_iframe.html");
  RegisterMockedHttpURLLoad("visible_iframe.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "single_iframe.html");

  v8::HandleScope scope(web_view_helper.GetAgentGroupScheduler().Isolate());
  ScriptExecutionCallbackHelper callback_helper;
  ExecuteScriptInMainWorld(
      web_view_helper.GetWebView()
          ->MainFrame()
          ->FirstChild()
          ->ToWebLocalFrame(),
      "var iframe = window.top.document.getElementsByTagName('iframe')[0]; "
      "window.top.document.body.removeChild(iframe); 'hello';",
      callback_helper.Callback());
  RunPendingTasks();
  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_FALSE(callback_helper.HasAnyResults());
}

namespace {

class CapabilityDelegationMessageListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event* event) override {
    delegated_capability_ =
        static_cast<MessageEvent*>(event)->delegatedCapability();
  }

  bool DelegateCapability() {
    if (delegated_capability_ == mojom::blink::DelegatedCapability::kNone)
      return false;
    delegated_capability_ = mojom::blink::DelegatedCapability::kNone;
    return true;
  }

 private:
  mojom::blink::DelegatedCapability delegated_capability_ =
      mojom::blink::DelegatedCapability::kNone;
};

}  // namespace

TEST_F(WebFrameTest, CapabilityDelegationMessageEventTest) {
  RegisterMockedHttpURLLoad("single_iframe.html");
  RegisterMockedHttpURLLoad("visible_iframe.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "single_iframe.html");

  auto* main_frame =
      To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame());
  auto* child_frame = To<LocalFrame>(main_frame->FirstChild());
  DCHECK(main_frame);
  DCHECK(child_frame);

  auto* message_event_listener =
      MakeGarbageCollected<CapabilityDelegationMessageListener>();
  child_frame->GetDocument()->domWindow()->addEventListener(
      event_type_names::kMessage, message_event_listener);

  v8::HandleScope scope(web_view_helper.GetAgentGroupScheduler().Isolate());

  {
    String post_message_wo_request(
        "window.frames[0].postMessage('0', {targetOrigin: '/'});");
    String post_message_w_payment_request(
        "window.frames[0].postMessage("
        "'1', {targetOrigin: '/', delegate: 'payment'});");

    // The delegation info is not passed through a postMessage that is sent
    // without either user activation or the delegation option.
    {
      ScriptExecutionCallbackHelper callback_helper;
      ExecuteScriptInMainWorld(web_view_helper.GetWebView()->MainFrameImpl(),
                               post_message_wo_request,
                               callback_helper.Callback());
      RunPendingTasks();
      EXPECT_TRUE(callback_helper.DidComplete());
      EXPECT_FALSE(message_event_listener->DelegateCapability());
    }

    // The delegation info is not passed through a postMessage that is sent
    // without user activation but with the delegation option.
    {
      ScriptExecutionCallbackHelper callback_helper;
      ExecuteScriptInMainWorld(web_view_helper.GetWebView()->MainFrameImpl(),
                               post_message_w_payment_request,
                               callback_helper.Callback());
      RunPendingTasks();
      EXPECT_TRUE(callback_helper.DidComplete());
      EXPECT_FALSE(message_event_listener->DelegateCapability());
    }

    // The delegation info is not passed through a postMessage that is sent with
    // user activation but without the delegation option.
    {
      ScriptExecutionCallbackHelper callback_helper;
      ExecuteScriptInMainWorld(web_view_helper.GetWebView()->MainFrameImpl(),
                               post_message_wo_request,
                               callback_helper.Callback(),
                               blink::mojom::PromiseResultOption::kAwait,
                               blink::mojom::UserActivationOption::kActivate);
      RunPendingTasks();
      EXPECT_TRUE(callback_helper.DidComplete());
      EXPECT_FALSE(message_event_listener->DelegateCapability());
    }

    // The delegation info is passed through a postMessage that is sent with
    // both user activation and the delegation option.
    {
      ScriptExecutionCallbackHelper callback_helper;
      ExecuteScriptInMainWorld(web_view_helper.GetWebView()->MainFrameImpl(),
                               post_message_w_payment_request,
                               callback_helper.Callback(),
                               blink::mojom::PromiseResultOption::kAwait,
                               blink::mojom::UserActivationOption::kActivate);
      RunPendingTasks();
      EXPECT_TRUE(callback_helper.DidComplete());
      EXPECT_TRUE(message_event_listener->DelegateCapability());
    }
  }

  {
    String post_message_w_fullscreen_request(
        "window.frames[0].postMessage("
        "'1', {targetOrigin: '/', delegate: 'fullscreen'});");

    // The delegation info is passed through a postMessage that is sent with
    // both user activation and the delegation option for another known
    // capability.
    ScriptExecutionCallbackHelper callback_helper;
    ExecuteScriptInMainWorld(web_view_helper.GetWebView()->MainFrameImpl(),
                             post_message_w_fullscreen_request,
                             callback_helper.Callback(),
                             blink::mojom::PromiseResultOption::kAwait,
                             blink::mojom::UserActivationOption::kActivate);
    RunPendingTasks();
    EXPECT_TRUE(callback_helper.DidComplete());
    EXPECT_TRUE(message_event_listener->DelegateCapability());
  }

  {
    String post_message_w_display_capture_request(
        "window.frames[0].postMessage("
        "'1', {targetOrigin: '/', delegate: 'display-capture'});");

    // The delegation info is passed through a postMessage that is sent with
    // both user activation and the delegation option for another known
    // capability.
    ScriptExecutionCallbackHelper callback_helper;
    ExecuteScriptInMainWorld(web_view_helper.GetWebView()->MainFrameImpl(),
                             post_message_w_display_capture_request,
                             callback_helper.Callback(),
                             blink::mojom::PromiseResultOption::kAwait,
                             blink::mojom::UserActivationOption::kActivate);
    RunPendingTasks();
    EXPECT_TRUE(callback_helper.DidComplete());
    EXPECT_TRUE(message_event_listener->DelegateCapability());
  }

  {
    String post_message_w_unknown_request(
        "window.frames[0].postMessage("
        "'1', {targetOrigin: '/', delegate: 'foo'});");

    // The delegation info is not passed through a postMessage that is sent with
    // user activation and the delegation option for an unknown capability.
    ScriptExecutionCallbackHelper callback_helper;
    ExecuteScriptInMainWorld(web_view_helper.GetWebView()->MainFrameImpl(),
                             post_message_w_unknown_request,
                             callback_helper.Callback(),
                             blink::mojom::PromiseResultOption::kAwait,
                             blink::mojom::UserActivationOption::kActivate);
    RunPendingTasks();
    EXPECT_TRUE(callback_helper.DidComplete());
    EXPECT_FALSE(message_event_listener->DelegateCapability());
  }
}

TEST_F(WebFrameTest, FormWithNullFrame) {
  RegisterMockedHttpURLLoad("form.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "form.html");

  WebVector<WebFormElement> forms =
      web_view_helper.LocalMainFrame()->GetDocument().Forms();
  web_view_helper.Reset();

  EXPECT_EQ(forms.size(), 1u);

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
  std::string content = TestWebFrameContentDumper::DumpWebViewAsText(
                            web_view_helper.GetWebView(), 1024)
                            .Utf8();
  EXPECT_NE(std::string::npos, content.find("Clobbered"));
}

TEST_F(WebFrameTest, ChromePageNoJavascript) {
  RegisterMockedChromeURLLoad("history.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(chrome_url_ + "history.html");

  // Try to run JS against the chrome-style URL after prohibiting it.
#if DCHECK_IS_ON()
  // TODO(crbug.com/1329535): Remove if threaded preload scanner doesn't launch.
  // This is needed because the preload scanner creates a thread when loading a
  // page.
  WTF::SetIsBeforeThreadCreatedForTest();
#endif
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs("chrome");
  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                "javascript:document.body.appendChild(document."
                                "createTextNode('Clobbered'))");

  // Now retrieve the frame's text and ensure it wasn't modified by running
  // javascript.
  std::string content = TestWebFrameContentDumper::DumpWebViewAsText(
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

  std::string content = TestWebFrameContentDumper::DumpWebViewAsText(
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

  std::string content = TestWebFrameContentDumper::DumpWebViewAsText(
                            web_view_helper.GetWebView(), 1024)
                            .Utf8();
  EXPECT_EQ("http://internal.test/" + file_name, content);
}

class EvaluateOnLoadWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  EvaluateOnLoadWebFrameClient() = default;
  ~EvaluateOnLoadWebFrameClient() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void DidClearWindowObject() override {
    EXPECT_FALSE(executing_);
    was_executed_ = true;
    executing_ = true;
    v8::HandleScope handle_scope(Frame()->GetAgentGroupScheduler()->Isolate());
    Frame()->ExecuteScriptAndReturnValue(
        WebScriptSource(WebString("window.someProperty = 42;")));
    executing_ = false;
  }

  bool executing_ = false;
  bool was_executed_ = false;
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

  test::TaskEnvironment task_environment_;
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
              UnorderedElementsAre("div.initial_off", "div.initial_on"));

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
  EXPECT_THAT(MatchedSelectors(), UnorderedElementsAre("span", "span, p"));
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

  auto make_message = []() {
    BlinkTransferableMessage message;
    message.message = SerializedScriptValue::NullValue();
    message.sender_origin =
        SecurityOrigin::CreateFromString("https://origin.com");
    message.sender_agent_cluster_id = base::UnguessableToken::Create();
    return message;
  };

  // Send a message with the correct origin.
  scoped_refptr<SecurityOrigin> correct_origin =
      SecurityOrigin::Create(ToKURL(base_url_));
  frame->PostMessageEvent(std::nullopt, g_empty_string,
                          correct_origin->ToString(), make_message());

  // Send another message with incorrect origin.
  scoped_refptr<SecurityOrigin> incorrect_origin =
      SecurityOrigin::Create(ToKURL(chrome_url_));
  frame->PostMessageEvent(std::nullopt, g_empty_string,
                          incorrect_origin->ToString(), make_message());

  // Verify that only the first addition is in the body of the page.
  std::string content = TestWebFrameContentDumper::DumpWebViewAsText(
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

TEST_F(WebFrameTest, PostMessageEvent_CannotDeserialize) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank");

  auto* frame =
      To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame());
  LocalDOMWindow* window = frame->DomWindow();

  base::RunLoop run_loop;
  auto* wait = MakeGarbageCollected<WaitForEvent>();
  wait->AddEventListener(window, event_type_names::kMessage);
  wait->AddEventListener(window, event_type_names::kMessageerror);
  wait->AddCompletionClosure(run_loop.QuitClosure());

  scoped_refptr<SerializedScriptValue> message =
      SerializeString("message", ToScriptStateForMainWorld(frame));
  SerializedScriptValue::ScopedOverrideCanDeserializeInForTesting
      override_can_deserialize_in(base::BindLambdaForTesting(
          [&](const SerializedScriptValue& value,
              ExecutionContext* execution_context, bool can_deserialize) {
            EXPECT_EQ(&value, message.get());
            EXPECT_EQ(execution_context, window);
            EXPECT_TRUE(can_deserialize);
            return false;
          }));

  NonThrowableExceptionState exception_state;
  frame->DomWindow()->PostMessageForTesting(message, MessagePortArray(), "*",
                                            window, exception_state);
  EXPECT_FALSE(exception_state.HadException());

  run_loop.Run();
  EXPECT_EQ(wait->GetLastEvent()->type(), event_type_names::kMessageerror);
}

namespace {

// Helper function to set autosizing multipliers on a document.
bool SetTextAutosizingMultiplier(Document* document, float multiplier) {
  bool multiplier_set = false;
  for (LayoutObject* layout_object = document->GetLayoutView(); layout_object;
       layout_object = layout_object->NextInPreOrder()) {
    if (layout_object->Style()) {
      ComputedStyleBuilder builder(layout_object->StyleRef());
      builder.SetTextAutosizingMultiplier(multiplier);
      layout_object->SetStyle(builder.TakeStyle(),
                              LayoutObject::ApplyStyleChanges::kNo);
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
    frame_test_helpers::WebViewHelper* web_view_helper,
    const display::ScreenInfo& screen_info) {
  display::ScreenInfos screen_infos(screen_info);
  web_view_helper->GetWebView()->MainFrameViewWidget()->UpdateScreenInfo(
      screen_infos);
  web_view_helper->Resize(screen_info.rect.size());
}

void UpdateScreenInfoAndResizeView(
    frame_test_helpers::WebViewHelper* web_view_helper,
    int viewport_width,
    int viewport_height) {
  display::ScreenInfo screen_info =
      web_view_helper->GetMainFrameWidget()->GetOriginalScreenInfo();
  screen_info.rect = gfx::Rect(viewport_width, viewport_height);
  UpdateScreenInfoAndResizeView(web_view_helper, screen_info);
}

}  // namespace

TEST_F(WebFrameTest, ChangeInFixedLayoutResetsTextAutosizingMultipliers) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fixed_layout.html", nullptr,
                                    nullptr, ConfigureAndroid);

  Document* document =
      To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame())
          ->GetDocument();
  document->GetSettings()->SetTextAutosizingEnabled(true);
  EXPECT_TRUE(document->GetSettings()->GetTextAutosizingEnabled());
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

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + html_file, nullptr, nullptr,
                                    ConfigureAndroid);

  Document* document =
      To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame())
          ->GetDocument();
  document->GetSettings()->SetTextAutosizingEnabled(true);
  EXPECT_TRUE(document->GetSettings()->GetTextAutosizingEnabled());

  web_view_helper.Resize(gfx::Size(490, 800));

  // Multiplier: 980 / 490 = 2.0
  EXPECT_TRUE(CheckTextAutosizingMultiplier(document, 2.0));
}

TEST_F(WebFrameTest,
       VisualViewportSetSizeInvalidatesTextAutosizingMultipliers) {
  RegisterMockedHttpURLLoad("iframe_reload.html");
  RegisterMockedHttpURLLoad("visible_iframe.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "iframe_reload.html", nullptr,
                                    nullptr, ConfigureAndroid);

  auto* main_frame =
      To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame());
  Document* document = main_frame->GetDocument();
  LocalFrameView* frame_view = web_view_helper.LocalMainFrame()->GetFrameView();
  document->GetSettings()->SetTextAutosizingEnabled(true);
  EXPECT_TRUE(document->GetSettings()->GetTextAutosizingEnabled());
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

  frame_view->GetPage()->GetVisualViewport().SetSize(gfx::Size(200, 200));

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
  int viewport_width = 1280;
  int viewport_height = 0;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_EQ(viewport_width, web_view_helper.GetWebView()
                                ->MainFrameImpl()
                                ->GetFrameView()
                                ->GetLayoutSize()
                                .width());
  EXPECT_EQ(viewport_height, web_view_helper.GetWebView()
                                 ->MainFrameImpl()
                                 ->GetFrameView()
                                 ->GetLayoutSize()
                                 .height());
}

TEST_F(WebFrameTest, DeviceScaleFactorUsesDefaultWithoutViewportTag) {
  RegisterMockedHttpURLLoad("no_viewport_tag.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "no_viewport_tag.html", nullptr,
                                    nullptr, ConfigureAndroid);
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->SetDeviceScaleFactorForTesting(2.f);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  auto* frame =
      To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame());
  DCHECK(frame);
  EXPECT_EQ(2, frame->DevicePixelRatio());

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

  int viewport_width = 640;
  int viewport_height = 480;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr, ConfigureAndroid);
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

  int viewport_width = 640;
  int viewport_height = 480;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr, ConfigureAndroid);
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

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-auto-initial-scale.html", nullptr, nullptr,
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

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-auto-initial-scale.html", nullptr, nullptr,
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

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "large-div.html", nullptr,
                                    nullptr, ConfigureAndroid);
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

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-auto-initial-scale.html", nullptr, nullptr,
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
                                .width());
  EXPECT_EQ(viewport_height, web_view_helper.GetWebView()
                                 ->MainFrameImpl()
                                 ->GetFrameView()
                                 ->Size()
                                 .height());
}

TEST_F(WebFrameTest, NoWideViewportIgnoresPageViewportWidthButAccountsScale) {
  RegisterMockedHttpURLLoad("viewport-wide-2x-initial-scale.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-wide-2x-initial-scale.html", nullptr, nullptr,
      ConfigureAndroid);
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
                                    .width());
  EXPECT_EQ(viewport_height / 2, web_view_helper.GetWebView()
                                     ->MainFrameImpl()
                                     ->GetFrameView()
                                     ->Size()
                                     .height());
}

TEST_F(WebFrameTest, WideViewportSetsTo980WithoutViewportTag) {
  RegisterMockedHttpURLLoad("no_viewport_tag.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "no_viewport_tag.html", nullptr,
                                    nullptr, ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(true);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_EQ(980, web_view_helper.GetWebView()
                     ->MainFrameImpl()
                     ->GetFrameView()
                     ->LayoutViewport()
                     ->ContentsSize()
                     .width());
  EXPECT_EQ(980.0 / viewport_width * viewport_height,
            web_view_helper.GetWebView()
                ->MainFrameImpl()
                ->GetFrameView()
                ->LayoutViewport()
                ->ContentsSize()
                .height());
}

TEST_F(WebFrameTest, WideViewportSetsTo980WithXhtmlMp) {
  RegisterMockedHttpURLLoad("viewport/viewport-legacy-xhtmlmp.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr, ConfigureAndroid);
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
                                .width());
  EXPECT_EQ(viewport_height, web_view_helper.GetWebView()
                                 ->MainFrameImpl()
                                 ->GetFrameView()
                                 ->Size()
                                 .height());
}

TEST_F(WebFrameTest, NoWideViewportAndHeightInMeta) {
  RegisterMockedHttpURLLoad("viewport-height-1000.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport-height-1000.html",
                                    nullptr, nullptr, ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_EQ(viewport_width, web_view_helper.GetWebView()
                                ->MainFrameImpl()
                                ->GetFrameView()
                                ->Size()
                                .width());
}

TEST_F(WebFrameTest, WideViewportSetsTo980WithAutoWidth) {
  RegisterMockedHttpURLLoad("viewport-2x-initial-scale.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-2x-initial-scale.html", nullptr, nullptr,
      ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(true);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_EQ(980, web_view_helper.GetWebView()
                     ->MainFrameImpl()
                     ->GetFrameView()
                     ->Size()
                     .width());
  EXPECT_EQ(980.0 / viewport_width * viewport_height,
            web_view_helper.GetWebView()
                ->MainFrameImpl()
                ->GetFrameView()
                ->Size()
                .height());
}

TEST_F(WebFrameTest, PageViewportInitialScaleOverridesLoadWithOverviewMode) {
  RegisterMockedHttpURLLoad("viewport-wide-2x-initial-scale.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-wide-2x-initial-scale.html", nullptr, nullptr,
      ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetLoadWithOverviewMode(false);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  // The page must be displayed at 200% zoom, as specified in its viewport meta
  // tag.
  EXPECT_EQ(2.0f, web_view_helper.GetWebView()->PageScaleFactor());
}

TEST_F(WebFrameTest, setInitialPageScaleFactorPermanently) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  float enforced_page_scale_factor = 2.0f;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fixed_layout.html", nullptr,
                                    nullptr, ConfigureAndroid);
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

  int viewport_width = 640;
  int viewport_height = 480;
  float enforced_page_scale_factor = 0.5f;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-auto-initial-scale.html", nullptr, nullptr,
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

  int viewport_width = 640;
  int viewport_height = 480;
  float enforced_page_scale_factor = 0.5f;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-wide-2x-initial-scale.html", nullptr, nullptr,
      ConfigureAndroid);
  web_view_helper.GetWebView()->SetInitialPageScaleOverride(
      enforced_page_scale_factor);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_EQ(enforced_page_scale_factor,
            web_view_helper.GetWebView()->PageScaleFactor());
}

TEST_F(WebFrameTest, SmallPermanentInitialPageScaleFactorIsClobbered) {
  const auto pages = std::to_array<const char*>(
      {// These pages trigger the clobbering condition. There must be a matching
       // item in "pageScaleFactors" array.
       "viewport-device-0.5x-initial-scale.html",
       "viewport-initial-scale-1.html",
       // These ones do not.
       "viewport-auto-initial-scale.html",
       "viewport-target-densitydpi-device-and-fixed-width.html"});
  const std::array<float, 2> page_scale_factors = {0.5f, 1.0f};
  for (size_t i = 0; i < std::size(pages); ++i)
    RegisterMockedHttpURLLoad(pages[i]);

  int viewport_width = 400;
  int viewport_height = 300;
  float enforced_page_scale_factor = 0.75f;

  for (size_t i = 0; i < std::size(pages); ++i) {
    for (int quirk_enabled = 0; quirk_enabled <= 1; ++quirk_enabled) {
      frame_test_helpers::WebViewHelper web_view_helper;
      web_view_helper.InitializeAndLoad(base_url_ + pages[i], nullptr, nullptr,
                                        ConfigureAndroid);
      web_view_helper.GetWebView()
          ->GetSettings()
          ->SetClobberUserAgentInitialScaleQuirk(quirk_enabled);
      web_view_helper.GetWebView()->SetInitialPageScaleOverride(
          enforced_page_scale_factor);
      web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

      float expected_page_scale_factor =
          quirk_enabled && i < std::size(page_scale_factors)
              ? page_scale_factors[i]
              : enforced_page_scale_factor;
      EXPECT_EQ(expected_page_scale_factor,
                web_view_helper.GetWebView()->PageScaleFactor());
    }
  }
}

TEST_F(WebFrameTest, PermanentInitialPageScaleFactorAffectsLayoutWidth) {
  int viewport_width = 640;
  int viewport_height = 480;
  float enforced_page_scale_factor = 0.5;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank", nullptr, nullptr,
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
                .width());
  EXPECT_EQ(enforced_page_scale_factor,
            web_view_helper.GetWebView()->PageScaleFactor());
}

TEST_F(WebFrameTest, DocumentElementClientHeightWorksWithWrapContentMode) {
  RegisterMockedHttpURLLoad("0-by-0.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "0-by-0.html", nullptr, nullptr,
                                    ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetForceZeroLayoutHeight(true);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  LocalFrame* frame = web_view_helper.LocalMainFrame()->GetFrame();
  Document* document = frame->GetDocument();
  EXPECT_EQ(viewport_height, document->documentElement()->clientHeight());
  EXPECT_EQ(viewport_width, document->documentElement()->clientWidth());
}

TEST_F(WebFrameTest, SetForceZeroLayoutHeightWorksWithWrapContentMode) {
  RegisterMockedHttpURLLoad("0-by-0.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "0-by-0.html", nullptr, nullptr,
                                    ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetForceZeroLayoutHeight(true);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  LocalFrameView* frame_view =
      web_view_helper.GetWebView()->MainFrameImpl()->GetFrameView();

  EXPECT_EQ(gfx::Size(), frame_view->GetLayoutSize());
  web_view_helper.Resize(gfx::Size(viewport_width, 0));
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  EXPECT_EQ(gfx::Size(viewport_width, 0), frame_view->GetLayoutSize());

  // The flag ForceZeroLayoutHeight will cause the following resize of viewport
  // height to be ignored by the outer viewport (the container layer of
  // LayerCompositor). The height of the visualViewport, however, is not
  // affected.
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  EXPECT_FALSE(frame_view->NeedsLayout());
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  EXPECT_EQ(gfx::Size(viewport_width, 0), frame_view->GetLayoutSize());

  LocalFrame* frame = web_view_helper.LocalMainFrame()->GetFrame();
  VisualViewport& visual_viewport = frame->GetPage()->GetVisualViewport();
  auto* scroll_node = visual_viewport.GetScrollTranslationNode()->ScrollNode();
  EXPECT_EQ(gfx::Rect(viewport_width, viewport_height),
            scroll_node->ContainerRect());
  EXPECT_EQ(gfx::Rect(viewport_width, viewport_height),
            scroll_node->ContentsRect());
}

TEST_F(WebFrameTest, SetForceZeroLayoutHeight) {
  RegisterMockedHttpURLLoad("200-by-300.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "200-by-300.html", nullptr,
                                    nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_LE(viewport_height, web_view_helper.GetWebView()
                                 ->MainFrameImpl()
                                 ->GetFrameView()
                                 ->GetLayoutSize()
                                 .height());
  web_view_helper.GetWebView()->GetSettings()->SetForceZeroLayoutHeight(true);
  EXPECT_TRUE(web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->NeedsLayout());

  EXPECT_EQ(0, web_view_helper.GetWebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->GetLayoutSize()
                   .height());

  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height * 2));
  EXPECT_FALSE(web_view_helper.GetWebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->NeedsLayout());
  EXPECT_EQ(0, web_view_helper.GetWebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->GetLayoutSize()
                   .height());

  web_view_helper.Resize(gfx::Size(viewport_width * 2, viewport_height));
  EXPECT_EQ(0, web_view_helper.GetWebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->GetLayoutSize()
                   .height());

  web_view_helper.GetWebView()->GetSettings()->SetForceZeroLayoutHeight(false);
  EXPECT_LE(viewport_height, web_view_helper.GetWebView()
                                 ->MainFrameImpl()
                                 ->GetFrameView()
                                 ->GetLayoutSize()
                                 .height());
}

TEST_F(WebFrameTest, ToggleViewportMetaOnOff) {
  RegisterMockedHttpURLLoad("viewport-device-width.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport-device-width.html",
                                    nullptr, nullptr);
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

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "button.html", nullptr, nullptr,
                                    ConfigureAndroid);
  // set view height to zero so that if the height of the view is not
  // successfully updated during later resizes touch events will fail
  // (as in not hit content included in the view)
  web_view_helper.Resize(gfx::Size(viewport_width, 0));

  web_view_helper.GetWebView()->GetSettings()->SetForceZeroLayoutHeight(true);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  gfx::PointF hit_point = gfx::PointF(30, 30);  // button size is 100x100

  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  Document* document = frame->GetFrame()->GetDocument();
  Element* element = document->getElementById(AtomicString("tap_button"));

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
  WebLocalFrameImpl* local_frame = helper.CreateLocalChild(
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
  frame_view->UpdateAllLifecyclePhasesForTest();
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
  WebLocalFrameImpl* local_frame = helper.CreateLocalChild(
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

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "200-by-300.html", nullptr,
                                    nullptr, ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetForceZeroLayoutHeight(true);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                base_url_ + "large-div.html");
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  EXPECT_EQ(0, web_view_helper.GetWebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->GetLayoutSize()
                   .height());
}

TEST_F(WebFrameTest, SetForceZeroLayoutHeightWithWideViewportQuirk) {
  RegisterMockedHttpURLLoad("200-by-300.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "200-by-300.html", nullptr,
                                    nullptr, ConfigureAndroid);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(true);
  web_view_helper.GetWebView()->GetSettings()->SetForceZeroLayoutHeight(true);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_EQ(0, web_view_helper.GetWebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->GetLayoutSize()
                   .height());
}

TEST_F(WebFrameTest, WideViewportQuirkClobbersHeight) {
  RegisterMockedHttpURLLoad("viewport-height-1000.html");

  int viewport_width = 600;
  int viewport_height = 800;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank", nullptr, nullptr,
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
                     .height());
  EXPECT_EQ(1, web_view_helper.GetWebView()->PageScaleFactor());
}

TEST_F(WebFrameTest, OverflowHiddenDisablesScrolling) {
  RegisterMockedHttpURLLoad("body-overflow-hidden.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr);
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

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr);
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

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr);
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

  int viewport_width = 640;
  int viewport_height = 480;
  float expected_page_scale_factor = 0.5f;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr, ConfigureAndroid);
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
                .width());
  EXPECT_EQ(expected_page_scale_factor,
            web_view_helper.GetWebView()->PageScaleFactor());

  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(true);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  EXPECT_EQ(viewport_width / expected_page_scale_factor,
            web_view_helper.GetWebView()
                ->MainFrameImpl()
                ->GetFrameView()
                ->GetLayoutSize()
                .width());
  EXPECT_EQ(expected_page_scale_factor,
            web_view_helper.GetWebView()->PageScaleFactor());
}

TEST_F(WebFrameTest, setPageScaleFactorDoesNotLayout) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  // Small viewport to ensure there are always scrollbars.
  int viewport_width = 64;
  int viewport_height = 48;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fixed_layout.html", nullptr,
                                    nullptr, ConfigureAndroid);
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

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fixed_layout.html", nullptr,
                                    nullptr, ConfigureAndroid);
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

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fixed_layout.html", nullptr,
                                    nullptr, ConfigureAndroid);
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

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr, ConfigureAndroid);
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

  // Small viewport to ensure there are always scrollbars.
  int viewport_width = 64;
  int viewport_height = 48;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "large-div.html", nullptr,
                                    nullptr, ConfigureAndroid);
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

  gfx::Size unscaled_size = view->Size();
  EXPECT_EQ(viewport_width, unscaled_size.width());
  EXPECT_EQ(viewport_height, unscaled_size.height());

  gfx::Size unscaled_size_minus_scrollbar = view->Size();
  EXPECT_EQ(viewport_width_minus_scrollbar,
            unscaled_size_minus_scrollbar.width());
  EXPECT_EQ(viewport_height_minus_scrollbar,
            unscaled_size_minus_scrollbar.height());

  gfx::Size frame_view_size = view->Size();
  EXPECT_EQ(viewport_width_minus_scrollbar, frame_view_size.width());
  EXPECT_EQ(viewport_height_minus_scrollbar, frame_view_size.height());
}

TEST_F(WebFrameTest, pageScaleFactorDoesNotApplyCssTransform) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fixed_layout.html", nullptr,
                                    nullptr, ConfigureAndroid);
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
                     .width());
}

TEST_F(WebFrameTest, targetDensityDpiHigh) {
  RegisterMockedHttpURLLoad("viewport-target-densitydpi-high.html");

  // high-dpi = 240
  float target_dpi = 240.0f;
  std::array<float, 3> device_scale_factors = {1.0f, 4.0f / 3.0f, 2.0f};
  int viewport_width = 640;
  int viewport_height = 480;

  for (size_t i = 0; i < std::size(device_scale_factors); ++i) {
    float device_scale_factor = device_scale_factors[i];
    float device_dpi = device_scale_factor * 160.0f;

    frame_test_helpers::WebViewHelper web_view_helper;
    web_view_helper.InitializeAndLoad(
        base_url_ + "viewport-target-densitydpi-high.html", nullptr, nullptr,
        ConfigureAndroid);
    web_view_helper.GetWebView()
        ->MainFrameWidget()
        ->SetDeviceScaleFactorForTesting(device_scale_factor);
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
                    .width(),
                1.0f);
    EXPECT_NEAR(viewport_height * density_dpi_scale_ratio,
                web_view_helper.GetWebView()
                    ->MainFrameImpl()
                    ->GetFrameView()
                    ->GetLayoutSize()
                    .height(),
                1.0f);
    EXPECT_NEAR(1.0f / density_dpi_scale_ratio,
                web_view_helper.GetWebView()->PageScaleFactor(), 0.01f);
  }
}

TEST_F(WebFrameTest, targetDensityDpiDevice) {
  RegisterMockedHttpURLLoad("viewport-target-densitydpi-device.html");

  std::array<float, 3> device_scale_factors = {1.0f, 4.0f / 3.0f, 2.0f};

  int viewport_width = 640;
  int viewport_height = 480;

  for (size_t i = 0; i < std::size(device_scale_factors); ++i) {
    frame_test_helpers::WebViewHelper web_view_helper;
    web_view_helper.InitializeAndLoad(
        base_url_ + "viewport-target-densitydpi-device.html", nullptr, nullptr,
        ConfigureAndroid);
    web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
    web_view_helper.GetWebView()
        ->MainFrameWidget()
        ->SetDeviceScaleFactorForTesting(device_scale_factors[i]);
    web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
        true);
    web_view_helper.GetWebView()
        ->GetSettings()
        ->SetSupportDeprecatedTargetDensityDPI(true);

    EXPECT_NEAR(viewport_width * device_scale_factors[i],
                web_view_helper.GetWebView()
                    ->MainFrameImpl()
                    ->GetFrameView()
                    ->GetLayoutSize()
                    .width(),
                1.0f);
    EXPECT_NEAR(viewport_height * device_scale_factors[i],
                web_view_helper.GetWebView()
                    ->MainFrameImpl()
                    ->GetFrameView()
                    ->GetLayoutSize()
                    .height(),
                1.0f);
    EXPECT_NEAR(1.0f, web_view_helper.GetWebView()->PageScaleFactor(), 0.01f);
    auto* frame =
        To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame());
    DCHECK(frame);
    EXPECT_EQ(device_scale_factors[i], frame->DevicePixelRatio());
  }
}

TEST_F(WebFrameTest, targetDensityDpiDeviceAndFixedWidth) {
  RegisterMockedHttpURLLoad(
      "viewport-target-densitydpi-device-and-fixed-width.html");

  std::array<float, 3> device_scale_factors = {1.0f, 4.0f / 3.0f, 2.0f};

  int viewport_width = 640;
  int viewport_height = 480;

  for (size_t i = 0; i < std::size(device_scale_factors); ++i) {
    frame_test_helpers::WebViewHelper web_view_helper;
    web_view_helper.InitializeAndLoad(
        base_url_ + "viewport-target-densitydpi-device-and-fixed-width.html",
        nullptr, nullptr, ConfigureAndroid);
    web_view_helper.GetWebView()
        ->MainFrameWidget()
        ->SetDeviceScaleFactorForTesting(device_scale_factors[i]);
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
                    .width(),
                1.0f);
    EXPECT_NEAR(viewport_height,
                web_view_helper.GetWebView()
                    ->MainFrameImpl()
                    ->GetFrameView()
                    ->GetLayoutSize()
                    .height(),
                1.0f);
    EXPECT_NEAR(1.0f, web_view_helper.GetWebView()->PageScaleFactor(), 0.01f);
  }
}

TEST_F(WebFrameTest, NoWideViewportAndScaleLessThanOne) {
  RegisterMockedHttpURLLoad("viewport-initial-scale-less-than-1.html");

  float device_scale_factor = 1.33f;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-initial-scale-less-than-1.html", nullptr, nullptr,
      ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->SetDeviceScaleFactorForTesting(device_scale_factor);
  web_view_helper.GetWebView()
      ->GetSettings()
      ->SetSupportDeprecatedTargetDensityDPI(true);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(false);

  EXPECT_NEAR(viewport_width * device_scale_factor,
              web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .width(),
              1.0f);
  EXPECT_NEAR(viewport_height * device_scale_factor,
              web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .height(),
              1.0f);

  EXPECT_NEAR(0.25f, web_view_helper.GetWebView()->PageScaleFactor(), 0.01f);
  auto* frame =
      To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame());
  DCHECK(frame);
  EXPECT_EQ(device_scale_factor, frame->DevicePixelRatio());
}

TEST_F(WebFrameTest, NoWideViewportAndScaleLessThanOneWithDeviceWidth) {
  RegisterMockedHttpURLLoad(
      "viewport-initial-scale-less-than-1-device-width.html");

  float device_scale_factor = 1.33f;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-initial-scale-less-than-1-device-width.html",
      nullptr, nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->SetDeviceScaleFactorForTesting(device_scale_factor);
  web_view_helper.GetWebView()
      ->GetSettings()
      ->SetSupportDeprecatedTargetDensityDPI(true);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(false);

  // We use 4.0f in EXPECT_NEAR to account for a rounding error.
  const float kPageZoom = 0.25f;
  EXPECT_NEAR(viewport_width * device_scale_factor / kPageZoom,
              web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .width(),
              4.0f);
  EXPECT_NEAR(viewport_height * device_scale_factor / kPageZoom,
              web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .height(),
              4.0f);

  EXPECT_NEAR(kPageZoom, web_view_helper.GetWebView()->PageScaleFactor(),
              0.01f);
  auto* frame =
      To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame());
  DCHECK(frame);
  EXPECT_EQ(device_scale_factor, frame->DevicePixelRatio());
}

TEST_F(WebFrameTest, NoWideViewportAndNoViewportWithInitialPageScaleOverride) {
  RegisterMockedHttpURLLoad("large-div.html");

  int viewport_width = 640;
  int viewport_height = 480;
  float enforced_page_scale_factor = 5.0f;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "large-div.html", nullptr,
                                    nullptr, ConfigureAndroid);
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
                  .width(),
              1.0f);
  EXPECT_NEAR(viewport_height / enforced_page_scale_factor,
              web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .height(),
              1.0f);
  EXPECT_NEAR(enforced_page_scale_factor,
              web_view_helper.GetWebView()->PageScaleFactor(), 0.01f);
}

TEST_F(WebFrameTest, NoUserScalableQuirkIgnoresViewportScale) {
  RegisterMockedHttpURLLoad("viewport-initial-scale-and-user-scalable-no.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-initial-scale-and-user-scalable-no.html", nullptr,
      nullptr, ConfigureAndroid);
  web_view_helper.GetWebView()
      ->GetSettings()
      ->SetViewportMetaNonUserScalableQuirk(true);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  EXPECT_NEAR(viewport_width,
              web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .width(),
              1.0f);
  EXPECT_NEAR(viewport_height,
              web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .height(),
              1.0f);
  EXPECT_NEAR(1.0f, web_view_helper.GetWebView()->PageScaleFactor(), 0.01f);
}

TEST_F(WebFrameTest,
       NoUserScalableQuirkIgnoresViewportScaleForNonWideViewport) {
  RegisterMockedHttpURLLoad("viewport-initial-scale-and-user-scalable-no.html");

  float device_scale_factor = 1.33f;
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-initial-scale-and-user-scalable-no.html", nullptr,
      nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->SetDeviceScaleFactorForTesting(device_scale_factor);
  web_view_helper.GetWebView()
      ->GetSettings()
      ->SetSupportDeprecatedTargetDensityDPI(true);
  web_view_helper.GetWebView()
      ->GetSettings()
      ->SetViewportMetaNonUserScalableQuirk(true);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(false);

  EXPECT_NEAR(viewport_width * device_scale_factor,
              web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .width(),
              1.0f);
  EXPECT_NEAR(viewport_height * device_scale_factor,
              web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .height(),
              1.0f);

  EXPECT_NEAR(2.0f, web_view_helper.GetWebView()->PageScaleFactor(), 0.01f);
  auto* frame =
      To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame());
  DCHECK(frame);
  EXPECT_EQ(device_scale_factor, frame->DevicePixelRatio());
}

TEST_F(WebFrameTest, NoUserScalableQuirkIgnoresViewportScaleForWideViewport) {
  RegisterMockedHttpURLLoad("viewport-2x-initial-scale-non-user-scalable.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-2x-initial-scale-non-user-scalable.html", nullptr,
      nullptr, ConfigureAndroid);
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
                  .width(),
              1.0f);
  EXPECT_NEAR(viewport_height,
              web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .height(),
              1.0f);
  EXPECT_NEAR(1.0f, web_view_helper.GetWebView()->PageScaleFactor(), 0.01f);
}

TEST_F(WebFrameTest, DesktopPageCanBeZoomedInWhenWideViewportIsTurnedOff) {
  RegisterMockedHttpURLLoad("no_viewport_tag.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "no_viewport_tag.html", nullptr,
                                    nullptr, ConfigureAndroid);
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
  void TestResizeYieldsCorrectScrollAndScale(
      const char* url,
      const float initial_page_scale_factor,
      const gfx::PointF& scroll_offset,
      const gfx::Size& viewport_size,
      const bool should_scale_relative_to_viewport_width) {
    RegisterMockedHttpURLLoad(url);

    const float aspect_ratio =
        static_cast<float>(viewport_size.width()) / viewport_size.height();

    frame_test_helpers::WebViewHelper web_view_helper;
    web_view_helper.InitializeAndLoad(base_url_ + url, nullptr, nullptr,
                                      ConfigureAndroid);
    web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.25f, 5);

    // Origin scrollOffsets preserved under resize.
    {
      web_view_helper.Resize(viewport_size);
      web_view_helper.GetWebView()->SetPageScaleFactor(
          initial_page_scale_factor);
      ASSERT_EQ(gfx::Size(viewport_size),
                web_view_helper.GetWebView()->MainFrameWidget()->Size());
      ASSERT_EQ(initial_page_scale_factor,
                web_view_helper.GetWebView()->PageScaleFactor());
      web_view_helper.Resize(
          gfx::Size(viewport_size.height(), viewport_size.width()));
      float expected_page_scale_factor =
          initial_page_scale_factor *
          (should_scale_relative_to_viewport_width ? 1 / aspect_ratio : 1);
      EXPECT_NEAR(expected_page_scale_factor,
                  web_view_helper.GetWebView()->PageScaleFactor(), 0.05f);
      EXPECT_EQ(gfx::PointF(),
                web_view_helper.LocalMainFrame()->GetScrollOffset());
    }

    // Resizing just the height should not affect pageScaleFactor or
    // scrollOffset.
    {
      web_view_helper.Resize(viewport_size);
      web_view_helper.GetWebView()->SetPageScaleFactor(
          initial_page_scale_factor);
      web_view_helper.LocalMainFrame()->SetScrollOffset(scroll_offset);
      UpdateAllLifecyclePhases(web_view_helper.GetWebView());
      const gfx::PointF expected_scroll_offset =
          web_view_helper.LocalMainFrame()->GetScrollOffset();
      web_view_helper.Resize(
          gfx::Size(viewport_size.width(), viewport_size.height() * 0.8f));
      EXPECT_EQ(initial_page_scale_factor,
                web_view_helper.GetWebView()->PageScaleFactor());
      EXPECT_EQ(expected_scroll_offset,
                web_view_helper.LocalMainFrame()->GetScrollOffset());
      web_view_helper.Resize(
          gfx::Size(viewport_size.width(), viewport_size.height() * 0.8f));
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
  const gfx::PointF scroll_offset(0, 50);
  const gfx::Size viewport_size(120, 160);
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
  const gfx::PointF scroll_offset(0, 0);
  const gfx::Size viewport_size(240, 320);
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
  const gfx::PointF scroll_offset(0, 200);
  const gfx::Size viewport_size(240, 320);
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
  const gfx::PointF scroll_offset(200, 400);
  const gfx::Size viewport_size(320, 240);
  const bool kShouldScaleRelativeToViewportWidth = true;

  TestResizeYieldsCorrectScrollAndScale(url, kInitialPageScaleFactor,
                                        scroll_offset, viewport_size,
                                        kShouldScaleRelativeToViewportWidth);
}

TEST_F(WebFrameTest, pageScaleFactorUpdatesScrollbars) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fixed_layout.html", nullptr,
                                    nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  LocalFrameView* view = web_view_helper.LocalMainFrame()->GetFrameView();
  ScrollableArea* scrollable_area = view->LayoutViewport();
  EXPECT_EQ(scrollable_area->ScrollSize(kHorizontalScrollbar),
            scrollable_area->ContentsSize().width() - view->Width());
  EXPECT_EQ(scrollable_area->ScrollSize(kVerticalScrollbar),
            scrollable_area->ContentsSize().height() - view->Height());

  web_view_helper.GetWebView()->SetPageScaleFactor(10);

  EXPECT_EQ(scrollable_area->ScrollSize(kHorizontalScrollbar),
            scrollable_area->ContentsSize().width() - view->Width());
  EXPECT_EQ(scrollable_area->ScrollSize(kVerticalScrollbar),
            scrollable_area->ContentsSize().height() - view->Height());
}

TEST_F(WebFrameTest, CanOverrideScaleLimits) {
  RegisterMockedHttpURLLoad("no_scale_for_you.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "no_scale_for_you.html",
                                    nullptr, nullptr, ConfigureAndroid);
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
#if BUILDFLAG(IS_ANDROID)
TEST_F(WebFrameTest, DISABLED_updateOverlayScrollbarLayers)
#else
TEST_F(WebFrameTest, updateOverlayScrollbarLayers)
#endif
{
  RegisterMockedHttpURLLoad("large-div.html");

  int view_width = 500;
  int view_height = 500;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();
  web_view_helper.GetWebView()
      ->GetPage()
      ->GetSettings()
      .SetPreferCompositingToLCDTextForTesting(true);

  web_view_helper.Resize(gfx::Size(view_width, view_height));
  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                base_url_ + "large-div.html");

  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  const cc::Layer* root_layer =
      web_view_helper.GetLayerTreeHost()->root_layer();
  EXPECT_EQ(1u, CcLayersByName(root_layer, "HorizontalScrollbar").size());
  EXPECT_EQ(1u, CcLayersByName(root_layer, "VerticalScrollbar").size());

  web_view_helper.Resize(gfx::Size(view_width * 10, view_height * 10));
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  EXPECT_EQ(0u, CcLayersByName(root_layer, "HorizontalScrollbar").size());
  EXPECT_EQ(0u, CcLayersByName(root_layer, "VerticalScrollbar").size());
}

void SetScaleAndScrollAndLayout(WebViewImpl* web_view,
                                const gfx::Point& scroll,
                                float scale) {
  web_view->SetPageScaleFactor(scale);
  web_view->MainFrameImpl()->SetScrollOffset(gfx::PointF(scroll));
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);
}

void SimulatePageScale(WebViewImpl* web_view_impl, float& scale) {
  float scale_delta =
      web_view_impl->FakePageScaleAnimationPageScaleForTesting() /
      web_view_impl->PageScaleFactor();
  web_view_impl->MainFrameWidget()->ApplyViewportChangesForTesting(
      {gfx::Vector2dF(), gfx::Vector2dF(), scale_delta, false, 0, 0,
       cc::BrowserControlsState::kBoth});
  scale = web_view_impl->PageScaleFactor();
}

gfx::Rect ComputeBlockBoundHelper(WebViewImpl* web_view_impl,
                                  const gfx::Point& point,
                                  bool ignore_clipping) {
  DCHECK(web_view_impl->MainFrameImpl());
  WebFrameWidgetImpl* widget =
      web_view_impl->MainFrameImpl()->FrameWidgetImpl();
  DCHECK(widget);
  return widget->ComputeBlockBound(point, ignore_clipping);
}

void SimulateDoubleTap(WebViewImpl* web_view_impl,
                       gfx::Point& point,
                       float& scale) {
  web_view_impl->AnimateDoubleTapZoom(
      point, ComputeBlockBoundHelper(web_view_impl, point, false));
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
      nullptr, ConfigureAndroid);
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.01f, 4);
  web_view_helper.GetWebView()->SetPageScaleFactor(0.5f);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));

  gfx::Rect wide_div(200, 100, 400, 150);
  gfx::Rect tall_div(200, 300, 400, 800);
  gfx::Point double_tap_point_wide(wide_div.x() + 50, wide_div.y() + 50);
  gfx::Point double_tap_point_tall(tall_div.x() + 50, tall_div.y() + 50);
  float scale;
  gfx::Point scroll;

  float double_tap_zoom_already_legible_scale =
      web_view_helper.GetWebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;

  // Test double-tap zooming into wide div.
  gfx::Rect wide_block_bound = ComputeBlockBoundHelper(
      web_view_helper.GetWebView(), double_tap_point_wide, false);
  web_view_helper.GetWebView()->ComputeScaleAndScrollForBlockRect(
      double_tap_point_wide, wide_block_bound, kTouchPointPadding,
      double_tap_zoom_already_legible_scale, scale, scroll);
  // The div should horizontally fill the screen (modulo margins), and
  // vertically centered (modulo integer rounding).
  EXPECT_NEAR(viewport_width / (float)wide_div.width(), scale, 0.1);
  EXPECT_NEAR(wide_div.x(), scroll.x(), 20);
  EXPECT_EQ(0, scroll.y());

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
  gfx::Rect tall_block_bound = ComputeBlockBoundHelper(
      web_view_helper.GetWebView(), double_tap_point_tall, false);
  web_view_helper.GetWebView()->ComputeScaleAndScrollForBlockRect(
      double_tap_point_tall, tall_block_bound, kTouchPointPadding,
      double_tap_zoom_already_legible_scale, scale, scroll);
  // The div should start at the top left of the viewport.
  EXPECT_NEAR(viewport_width / (float)tall_div.width(), scale, 0.1);
  EXPECT_NEAR(tall_div.x(), scroll.x(), 20);
  EXPECT_NEAR(tall_div.y(), scroll.y(), 20);
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
      ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  web_view_helper.GetWebView()->SetZoomFactorForDeviceScaleFactor(
      kDeviceScaleFactor);
  web_view_helper.GetWebView()->SetPageScaleFactor(1.0f);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  web_view_helper.GetWebView()->EnableFakePageScaleAnimationForTesting(true);

  float double_tap_zoom_already_legible_scale =
      web_view_helper.GetWebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;

  gfx::Rect div(0, 100, viewport_width, 150);
  gfx::Point point(div.x() + 50, div.y() + 50);
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
                                    nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  web_view_helper.GetWebView()->SetZoomFactorForDeviceScaleFactor(
      kDeviceScaleFactor);
  web_view_helper.GetWebView()->SetPageScaleFactor(1.0f);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  gfx::Rect div(200, 300, 400, 5000);
  gfx::Point point(div.x() + 50, div.y() + 3000);
  float scale;
  gfx::Point scroll;

  gfx::Rect block_bound =
      ComputeBlockBoundHelper(web_view_helper.GetWebView(), point, true);
  web_view_helper.GetWebView()->ComputeScaleAndScrollForBlockRect(
      point, block_bound, 0, 1.0f, scale, scroll);
  EXPECT_EQ(scale, 1.0f);
  EXPECT_EQ(scroll.y(), 2660);
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
      ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.5f, 4);
  web_view_helper.GetWebView()->SetPageScaleFactor(0.5f);
  web_view_helper.GetWebView()->SetMaximumLegibleScale(1.f);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  web_view_helper.GetWebView()->EnableFakePageScaleAnimationForTesting(true);

  gfx::Rect top_div(200, 100, 200, 150);
  gfx::Rect bottom_div(200, 300, 200, 150);
  gfx::Point top_point(top_div.x() + 50, top_div.y() + 50);
  gfx::Point bottom_point(bottom_div.x() + 50, bottom_div.y() + 50);
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
      ->ApplyViewportChangesForTesting({gfx::Vector2dF(), gfx::Vector2dF(),
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
      ->ApplyViewportChangesForTesting({gfx::Vector2dF(), gfx::Vector2dF(),
                                        1.1f, false, 0, 0,
                                        cc::BrowserControlsState::kBoth});

  gfx::Rect block_bounds =
      ComputeBlockBoundHelper(web_view_helper.GetWebView(), top_point, false);
  web_view_helper.GetWebView()->AnimateDoubleTapZoom(top_point, block_bounds);
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
      nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  web_view_helper.GetWebView()->SetMaximumLegibleScale(1.f);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  web_view_helper.GetWebView()->EnableFakePageScaleAnimationForTesting(true);

  gfx::Rect div(200, 100, 200, 150);
  gfx::Point double_tap_point(div.x() + 50, div.y() + 50);
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
      ->ApplyViewportChangesForTesting({gfx::Vector2dF(), gfx::Vector2dF(),
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
      ->ApplyViewportChangesForTesting({gfx::Vector2dF(), gfx::Vector2dF(),
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
      nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  web_view_helper.GetWebView()->SetMaximumLegibleScale(
      maximum_legible_scale_factor);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  web_view_helper.GetWebView()->EnableFakePageScaleAnimationForTesting(true);
  web_view_helper.GetWebView()
      ->GetPage()
      ->GetSettings()
      .SetTextAutosizingEnabled(true);

  gfx::Rect div(200, 100, 200, 150);
  gfx::Point double_tap_point(div.x() + 50, div.y() + 50);
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
      ->ApplyViewportChangesForTesting({gfx::Vector2dF(), gfx::Vector2dF(),
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
      ->ApplyViewportChangesForTesting({gfx::Vector2dF(), gfx::Vector2dF(),
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
      ->ApplyViewportChangesForTesting({gfx::Vector2dF(), gfx::Vector2dF(),
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
      nullptr, ConfigureAndroid);
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

  gfx::Rect div(200, 100, 200, 150);
  gfx::Point double_tap_point(div.x() + 50, div.y() + 50);
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
      ->ApplyViewportChangesForTesting({gfx::Vector2dF(), gfx::Vector2dF(),
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
      ->ApplyViewportChangesForTesting({gfx::Vector2dF(), gfx::Vector2dF(),
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
      ->ApplyViewportChangesForTesting({gfx::Vector2dF(), gfx::Vector2dF(),
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
                                    nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(300, 300));

  gfx::Rect rect_back(0, 0, 200, 200);
  gfx::Rect rect_left_top(10, 10, 80, 80);
  gfx::Rect rect_right_bottom(110, 110, 80, 80);
  gfx::Rect block_bound;

  block_bound = ComputeBlockBoundHelper(web_view_helper.GetWebView(),
                                        gfx::Point(9, 9), true);
  EXPECT_EQ(rect_back, block_bound);

  block_bound = ComputeBlockBoundHelper(web_view_helper.GetWebView(),
                                        gfx::Point(10, 10), true);
  EXPECT_EQ(rect_left_top, block_bound);

  block_bound = ComputeBlockBoundHelper(web_view_helper.GetWebView(),
                                        gfx::Point(50, 50), true);
  EXPECT_EQ(rect_left_top, block_bound);

  block_bound = ComputeBlockBoundHelper(web_view_helper.GetWebView(),
                                        gfx::Point(89, 89), true);
  EXPECT_EQ(rect_left_top, block_bound);

  block_bound = ComputeBlockBoundHelper(web_view_helper.GetWebView(),
                                        gfx::Point(90, 90), true);
  EXPECT_EQ(rect_back, block_bound);

  block_bound = ComputeBlockBoundHelper(web_view_helper.GetWebView(),
                                        gfx::Point(109, 109), true);
  EXPECT_EQ(rect_back, block_bound);

  block_bound = ComputeBlockBoundHelper(web_view_helper.GetWebView(),
                                        gfx::Point(110, 110), true);
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
      ->SetAutoZoomFocusedEditableToLegibleScale(true);
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
  RegisterMockedHttpURLLoad("Ahem.ttf");
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

  gfx::Rect edit_box_with_text(200, 200, 250, 20);
  gfx::Rect edit_box_with_no_text(200, 250, 250, 20);

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
  gfx::Point scroll;
  bool need_animation;
  gfx::Rect element_bounds, caret_bounds;
  GetElementAndCaretBoundsForFocusedEditableElement(
      web_view_helper, element_bounds, caret_bounds);
  web_view_helper.GetWebView()->ComputeScaleAndScrollForEditableElementRects(
      element_bounds, caret_bounds, kAutoZoomToLegibleScale, scale, scroll,
      need_animation);
  EXPECT_TRUE(need_animation);
  // The edit box should be left aligned with a margin for possible label.
  int h_scroll =
      edit_box_with_text.x() - left_box_ratio * viewport_width / scale;
  EXPECT_NEAR(h_scroll, scroll.x(), 2);
  int v_scroll = edit_box_with_text.y() -
                 (viewport_height / scale - edit_box_with_text.height()) / 2;
  EXPECT_NEAR(v_scroll, scroll.y(), 2);
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
  EXPECT_NEAR(h_scroll, scroll.x(), 2);
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
  h_scroll = edit_box_with_no_text.x();
  EXPECT_NEAR(h_scroll, scroll.x(), 2);
  v_scroll = edit_box_with_no_text.y() -
             (viewport_height / scale - edit_box_with_no_text.height()) / 2;
  EXPECT_NEAR(v_scroll, scroll.y(), 2);
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

  const gfx::Rect edit_box_with_text(200, 200, 250, 20);

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
  gfx::Point scroll;
  bool need_animation;
  gfx::Rect element_bounds, caret_bounds;
  GetElementAndCaretBoundsForFocusedEditableElement(
      web_view_helper, element_bounds, caret_bounds);
  web_view_helper.GetWebView()->ComputeScaleAndScrollForEditableElementRects(
      element_bounds, caret_bounds, kAutoZoomToLegibleScale, scale, scroll,
      need_animation);
  EXPECT_TRUE(need_animation);
  // Edit box and caret should be left alinged
  int h_scroll = edit_box_with_text.x();
  EXPECT_NEAR(h_scroll, scroll.x(), 1);
  int v_scroll = edit_box_with_text.y() -
                 (kViewportHeight / scale - edit_box_with_text.height()) / 2;
  EXPECT_NEAR(v_scroll, scroll.y(), 1);
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
  EXPECT_NEAR(h_scroll, scroll.x(), 1);
  v_scroll = edit_box_with_text.y() -
             (kViewportHeight / scale - edit_box_with_text.height()) / 2;
  EXPECT_NEAR(v_scroll, scroll.y(), 1);
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

  gfx::Rect edit_box_with_no_text(200, 250, 250, 20);

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
  gfx::Point scroll;
  bool need_animation;
  gfx::Rect element_bounds, caret_bounds;
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
      edit_box_with_no_text.x() - left_box_ratio * viewport_width / scale;
  EXPECT_NEAR(h_scroll, scroll.x(), 2);
  int v_scroll = edit_box_with_no_text.y() -
                 (viewport_height / scale - edit_box_with_no_text.height()) / 2;
  EXPECT_NEAR(v_scroll, scroll.y(), 2);

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
      nullptr, ConfigureAndroid);
  web_view_helper.GetWebView()
      ->GetPage()
      ->GetSettings()
      .SetTextAutosizingEnabled(false);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  web_view_helper.GetWebView()->SetZoomFactorForDeviceScaleFactor(
      kDeviceScaleFactor);
  web_view_helper.GetWebView()->SetDefaultPageScaleLimits(0.25f, 4);

  web_view_helper.GetWebView()->EnableFakePageScaleAnimationForTesting(true);

  gfx::Rect edit_box_with_text(
      200 * kDeviceScaleFactor, 200 * kDeviceScaleFactor,
      250 * kDeviceScaleFactor, 20 * kDeviceScaleFactor);
  web_view_helper.GetWebView()->AdvanceFocus(false);

  // Set the page scale to be smaller than the minimal readable scale.
  float initial_scale = 0.5f;
  SetScaleAndScrollAndLayout(web_view_helper.GetWebView(), gfx::Point(),
                             initial_scale);
  ASSERT_EQ(web_view_helper.GetWebView()->PageScaleFactor(), initial_scale);

  float scale;
  gfx::Point scroll;
  bool need_animation;
  gfx::Rect element_bounds, caret_bounds;
  GetElementAndCaretBoundsForFocusedEditableElement(
      web_view_helper, element_bounds, caret_bounds);
  web_view_helper.GetWebView()->ComputeScaleAndScrollForEditableElementRects(
      element_bounds, caret_bounds, kAutoZoomToLegibleScale, scale, scroll,
      need_animation);
  EXPECT_TRUE(need_animation);
  // The edit box wider than the viewport when legible should be left aligned.
  int h_scroll = edit_box_with_text.x();
  EXPECT_NEAR(h_scroll, scroll.x(), 2);
  int v_scroll = edit_box_with_text.y() -
                 (viewport_height / scale - edit_box_with_text.height()) / 2;
  EXPECT_NEAR(v_scroll, scroll.y(), 2);
  EXPECT_NEAR(min_readable_caret_height / caret_bounds.height(), scale, 0.1);
}

TEST_F(WebFrameTest, FirstRectForCharacterRangeWithPinchZoom) {
  RegisterMockedHttpURLLoad("textbox.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "textbox.html");
  web_view_helper.Resize(gfx::Size(640, 480));

  WebLocalFrame* main_frame = web_view_helper.LocalMainFrame();
  main_frame->ExecuteScript(WebScriptSource("selectRange();"));

  gfx::Rect old_rect;
  main_frame->FirstRectForCharacterRange(0, 5, old_rect);

  gfx::PointF visual_offset(100, 130);
  float scale = 2;
  web_view_helper.GetWebView()->SetPageScaleFactor(scale);
  web_view_helper.GetWebView()->SetVisualViewportOffset(visual_offset);

  gfx::Rect rect;
  main_frame->FirstRectForCharacterRange(0, 5, rect);

  EXPECT_EQ((old_rect.x() - visual_offset.x()) * scale, rect.x());
  EXPECT_EQ((old_rect.y() - visual_offset.y()) * scale, rect.y());
  EXPECT_EQ(old_rect.width() * scale, rect.width());
  EXPECT_EQ(old_rect.height() * scale, rect.height());
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
  void DidCommitNavigation(
      WebHistoryCommitType commit_type,
      bool should_reset_browser_interface_broker,
      const ParsedPermissionsPolicy& permissions_policy_header,
      const DocumentPolicyFeatureState& document_policy_header) override {
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
      gfx::PointF(kPageWidth / 4, kPageHeight / 4));
  web_view_helper.GetWebView()->SetPageScaleFactor(kPageScaleFactor);

  // Reload the page and end up at the same url. State should not be propagated.
  web_view_helper.GetWebView()->MainFrameImpl()->StartReload(
      WebFrameLoadType::kReload);
  frame_test_helpers::PumpPendingRequestsForFrameToLoad(
      web_view_helper.LocalMainFrame());
  EXPECT_EQ(gfx::PointF(), web_view_helper.LocalMainFrame()->GetScrollOffset());
  EXPECT_EQ(1.0f, web_view_helper.GetWebView()->PageScaleFactor());
}

TEST_F(WebFrameTest, ReloadWhileProvisional) {
  // Test that reloading while the previous load is still pending does not cause
  // the initial request to get lost.
  RegisterMockedHttpURLLoad("fixed_layout.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();
  WebLocalFrameImpl* main_frame = web_view_helper.LocalMainFrame();
  FrameLoadRequest frame_load_request(
      nullptr, ResourceRequest(ToKURL(base_url_ + "fixed_layout.html")));
  main_frame->GetFrame()->Loader().StartNavigation(frame_load_request);
  // start reload before first request is delivered.
  frame_test_helpers::ReloadFrameBypassingCache(
      web_view_helper.GetWebView()->MainFrameImpl());

  WebDocumentLoader* document_loader =
      web_view_helper.LocalMainFrame()->GetDocumentLoader();
  ASSERT_TRUE(document_loader);
  EXPECT_EQ(ToKURL(base_url_ + "fixed_layout.html"),
            KURL(document_loader->GetUrl()));
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
  void DidChangeSelection(bool isSelectionEmpty,
                          blink::SyncCondition force_sync) override {
    ++call_count_;
  }
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
      mojom::blink::TreeScopeType scope,
      const WebString& name,
      const WebString& fallback_name,
      const FramePolicy&,
      const WebFrameOwnerProperties&,
      FrameOwnerElementType,
      WebPolicyContainerBindParams policy_container_bind_params,
      ukm::SourceId document_ukm_source_id,
      FinishChildFrameCreationFn finish_creation) override {
    return CreateLocalChild(*Frame(), scope,
                            std::make_unique<ContextLifetimeTestWebFrameClient>(
                                create_notifications_, release_notifications_),
                            std::move(policy_container_bind_params),
                            finish_creation);
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
  v8::HandleScope handle_scope(
      web_view_helper.GetAgentGroupScheduler().Isolate());
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
  RegisterMockedHttpURLLoad("context_notifications_test.html");
  RegisterMockedHttpURLLoad("context_notifications_test_frame.html");

  Vector<std::unique_ptr<ContextLifetimeTestWebFrameClient::Notification>>
      create_notifications;
  Vector<std::unique_ptr<ContextLifetimeTestWebFrameClient::Notification>>
      release_notifications;
  ContextLifetimeTestWebFrameClient web_frame_client(create_notifications,
                                                     release_notifications);
  frame_test_helpers::WebViewHelper web_view_helper;
  v8::HandleScope handle_scope(
      web_view_helper.GetAgentGroupScheduler().Isolate());
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
  for (wtf_size_t i = 0; i < release_notifications.size(); ++i) {
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
  RegisterMockedHttpURLLoad("context_notifications_test.html");
  RegisterMockedHttpURLLoad("context_notifications_test_frame.html");

  Vector<std::unique_ptr<ContextLifetimeTestWebFrameClient::Notification>>
      create_notifications;
  Vector<std::unique_ptr<ContextLifetimeTestWebFrameClient::Notification>>
      release_notifications;
  ContextLifetimeTestWebFrameClient web_frame_client(create_notifications,
                                                     release_notifications);
  frame_test_helpers::WebViewHelper web_view_helper;
  v8::Isolate* isolate = web_view_helper.GetAgentGroupScheduler().Isolate();
  v8::HandleScope handle_scope(isolate);
  web_view_helper.InitializeAndLoad(
      base_url_ + "context_notifications_test.html", &web_frame_client);

  // Add an isolated world.
  web_frame_client.Reset();

  int32_t isolated_world_id = 42;
  WebScriptSource script_source("hi!");
  web_view_helper.LocalMainFrame()->ExecuteScriptInIsolatedWorld(
      isolated_world_id, script_source, BackForwardCacheAware::kAllow);

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
  for (wtf_size_t i = 0; i < release_notifications.size(); ++i) {
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
  WebString text = TestWebFrameContentDumper::DumpWebViewAsText(
      web_view_helper.GetWebView(), std::numeric_limits<size_t>::max());
  EXPECT_EQ(expected, text.Utf8());

  // Try reading the same one with clipping of the text.
  const int kLength = 5;
  text = TestWebFrameContentDumper::DumpWebViewAsText(
      web_view_helper.GetWebView(), kLength);
  EXPECT_EQ(expected.substr(0, kLength), text.Utf8());

  // Now do a new test with a subframe.
  const char kOuterFrameSource[] = "Hello<iframe></iframe> world";
  frame_test_helpers::LoadHTMLString(frame, kOuterFrameSource, test_url);

  // Load something into the subframe.
  WebLocalFrame* subframe = frame->FirstChild()->ToWebLocalFrame();
  ASSERT_TRUE(subframe);
  frame_test_helpers::LoadHTMLString(subframe, "sub<p>text", test_url);

  text = TestWebFrameContentDumper::DumpWebViewAsText(
      web_view_helper.GetWebView(), std::numeric_limits<size_t>::max());
  EXPECT_EQ("Hello world\n\nsub\n\ntext", text.Utf8());

  // Get the frame text where the subframe separator falls on the boundary of
  // what we'll take. There used to be a crash in this case.
  text = TestWebFrameContentDumper::DumpWebViewAsText(
      web_view_helper.GetWebView(), 12);
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

  WebString text = TestWebFrameContentDumper::DumpWebViewAsText(
      web_view_helper.GetWebView(), std::numeric_limits<size_t>::max());
  EXPECT_EQ("Hello\n\nWorld", text.Utf8());

  const std::string html =
      TestWebFrameContentDumper::DumpAsMarkup(frame).Utf8();

  // Load again with the output html.
  frame_test_helpers::LoadHTMLString(frame, html, test_url);

  EXPECT_EQ(html, TestWebFrameContentDumper::DumpAsMarkup(frame).Utf8());

  text = TestWebFrameContentDumper::DumpWebViewAsText(
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

#if BUILDFLAG(IS_ANDROID)
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
    const gfx::RectF& result_rect = web_match_rects[result_index];

    // Select the match by the center of its rect.
    EXPECT_EQ(main_frame->EnsureTextFinder().SelectNearestFindMatch(
                  result_rect.CenterPoint(), nullptr),
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
    EXPECT_EQ(gfx::ToEnclosingRect(active_match),
              gfx::ToEnclosingRect(result_rect));

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
#endif  // BUILDFLAG(IS_ANDROID)

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
  RemoveElementById(main_frame, AtomicString("frame"));

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
  RemoveElementById(main_frame, AtomicString("frame"));

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
  RemoveElementById(main_frame, AtomicString("frame"));

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

  const Vector<gfx::Rect> kExpectedOverridingTickmarks = {
      gfx::Rect(0, 0, 100, 100), gfx::Rect(0, 20, 100, 100),
      gfx::Rect(0, 30, 100, 100)};
  const Vector<gfx::Rect> kExpectedOverridingTickmarksIntRect = {
      kExpectedOverridingTickmarks[0], kExpectedOverridingTickmarks[1],
      kExpectedOverridingTickmarks[2]};
  const Vector<gfx::Rect> kResetTickmarks;

  {
    // Test SetTickmarks() with a null target WebElement.
    //
    // Get the tickmarks for the original find request. It should have 4
    // tickmarks, given the search performed above.
    LocalFrameView* frame_view =
        web_view_helper.LocalMainFrame()->GetFrameView();
    ScrollableArea* layout_viewport = frame_view->LayoutViewport();
    Vector<gfx::Rect> original_tickmarks = layout_viewport->GetTickmarks();
    EXPECT_EQ(4u, original_tickmarks.size());

    // Override the tickmarks.
    main_frame->SetTickmarks(WebElement(), kExpectedOverridingTickmarks);

    // Check the tickmarks are overridden correctly.
    Vector<gfx::Rect> overriding_tickmarks_actual =
        layout_viewport->GetTickmarks();
    EXPECT_EQ(kExpectedOverridingTickmarksIntRect, overriding_tickmarks_actual);

    // Reset the tickmark behavior.
    main_frame->SetTickmarks(WebElement(), kResetTickmarks);

    // Check that the original tickmarks are returned
    Vector<gfx::Rect> original_tickmarks_after_reset =
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
    Vector<gfx::Rect> original_tickmarks = scrollable_area->GetTickmarks();
    EXPECT_EQ(0u, original_tickmarks.size());

    // Override the tickmarks.
    main_frame->SetTickmarks(target, kExpectedOverridingTickmarks);

    // Check the tickmarks are overridden correctly.
    Vector<gfx::Rect> overriding_tickmarks_actual =
        scrollable_area->GetTickmarks();
    EXPECT_EQ(kExpectedOverridingTickmarksIntRect, overriding_tickmarks_actual);

    // Reset the tickmark behavior.
    main_frame->SetTickmarks(target, kResetTickmarks);

    // Check that the original tickmarks are returned
    Vector<gfx::Rect> original_tickmarks_after_reset =
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
  return gfx::Rect(frame->GetDocument().GetElementById(id).BoundsInWidget());
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
                     SelectionMenuBehavior::kHide,
                     WebLocalFrame::kSelectionSetFocus);
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
                     SelectionMenuBehavior::kHide,
                     WebLocalFrame::kSelectionSetFocus);

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
                     SelectionMenuBehavior::kHide,
                     WebLocalFrame::kSelectionSetFocus);

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
                     SelectionMenuBehavior::kHide,
                     WebLocalFrame::kSelectionSetFocus);
  frame->SelectRange(WebRange(0, 6), WebLocalFrame::kPreserveHandleVisibility,
                     SelectionMenuBehavior::kHide,
                     WebLocalFrame::kSelectionSetFocus);

  EXPECT_FALSE(frame->GetFrame()->Selection().IsHandleVisible())
      << "kPreserveHandleVisibility should keep handles invisible";

  frame->SelectRange(WebRange(0, 5), WebLocalFrame::kShowSelectionHandle,
                     SelectionMenuBehavior::kHide,
                     WebLocalFrame::kSelectionSetFocus);
  frame->SelectRange(WebRange(0, 6), WebLocalFrame::kPreserveHandleVisibility,
                     SelectionMenuBehavior::kHide,
                     WebLocalFrame::kSelectionSetFocus);

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
      "text-indent: 0px; text-transform: none; widows: 2; "
      "word-spacing: 0px; -webkit-text-stroke-width: 0px; white-space: normal; "
      "text-decoration-thickness: initial; text-decoration-style: initial; "
      "text-decoration-color: initial;\">Air conditioner</div><div id=\"div5\" "
      "style=\"padding: 10px; margin: 10px; border: 2px solid skyblue; float: "
      "left; width: 190px; height: 30px; color: rgb(0, 0, 0); font-family: "
      "myahem; font-size: 8px; font-style: normal; font-variant-ligatures: "
      "normal; font-variant-caps: normal; font-weight: 400; letter-spacing: "
      "normal; orphans: 2; text-align: start; text-indent: 0px; "
      "text-transform: none; widows: 2; word-spacing: 0px; "
      "-webkit-text-stroke-width: 0px; white-space: normal; "
      "text-decoration-thickness: "
      "initial; text-decoration-style: initial; text-decoration-color: "
      "initial;\">Price 10,000,000won</div>";
  String clip_text;
  String clip_html;
  gfx::Rect clip_rect;
  RegisterMockedHttpURLLoad("Ahem.ttf");
  RegisterMockedHttpURLLoad("smartclip.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "smartclip.html");
  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  web_view_helper.Resize(gfx::Size(500, 500));
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  gfx::Rect crop_rect(300, 125, 152, 50);
  frame->GetFrame()->ExtractSmartClipDataInternal(crop_rect, clip_text,
                                                  clip_html, clip_rect);
  EXPECT_EQ(String(kExpectedClipText), clip_text);
  EXPECT_EQ(String(kExpectedClipHtml), clip_html);
}

TEST_F(WebFrameTest, SmartClipDataWithPinchZoom) {
  static const char kExpectedClipText[] = "\nPrice 10,000,000won";
  static const char kExpectedClipHtml[] =
      "<div id=\"div4\" style=\"padding: 10px; margin: 10px; border: 2px solid "
      "skyblue; float: left; width: 190px; height: 30px; color: rgb(0, 0, 0); "
      "font-family: myahem; font-size: 8px; font-style: normal; "
      "font-variant-ligatures: normal; font-variant-caps: normal; font-weight: "
      "400; letter-spacing: normal; orphans: 2; text-align: start; "
      "text-indent: 0px; text-transform: none; widows: 2; "
      "word-spacing: 0px; -webkit-text-stroke-width: 0px; white-space: normal; "
      "text-decoration-thickness: initial; text-decoration-style: initial; "
      "text-decoration-color: initial;\">Air conditioner</div><div id=\"div5\" "
      "style=\"padding: 10px; margin: 10px; border: 2px solid skyblue; float: "
      "left; width: 190px; height: 30px; color: rgb(0, 0, 0); font-family: "
      "myahem; font-size: 8px; font-style: normal; font-variant-ligatures: "
      "normal; font-variant-caps: normal; font-weight: 400; letter-spacing: "
      "normal; orphans: 2; text-align: start; text-indent: 0px; "
      "text-transform: none; widows: 2; word-spacing: 0px; "
      "-webkit-text-stroke-width: 0px; white-space: normal; "
      "text-decoration-thickness: "
      "initial; text-decoration-style: initial; text-decoration-color: "
      "initial;\">Price 10,000,000won</div>";
  String clip_text;
  String clip_html;
  gfx::Rect clip_rect;
  RegisterMockedHttpURLLoad("Ahem.ttf");
  RegisterMockedHttpURLLoad("smartclip.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "smartclip.html");
  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  web_view_helper.Resize(gfx::Size(500, 500));
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  web_view_helper.GetWebView()->SetPageScaleFactor(1.5);
  web_view_helper.GetWebView()->SetVisualViewportOffset(gfx::PointF(167, 100));
  gfx::Rect crop_rect(200, 38, 228, 75);
  frame->GetFrame()->ExtractSmartClipDataInternal(crop_rect, clip_text,
                                                  clip_html, clip_rect);
  EXPECT_EQ(String(kExpectedClipText), clip_text);
  EXPECT_EQ(String(kExpectedClipHtml), clip_html);
}

TEST_F(WebFrameTest, SmartClipReturnsEmptyStringsWhenUserSelectIsNone) {
  String clip_text;
  String clip_html;
  gfx::Rect clip_rect;
  RegisterMockedHttpURLLoad("Ahem.ttf");
  RegisterMockedHttpURLLoad("smartclip_user_select_none.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ +
                                    "smartclip_user_select_none.html");
  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  web_view_helper.Resize(gfx::Size(500, 500));
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  gfx::Rect crop_rect(0, 0, 100, 100);
  frame->GetFrame()->ExtractSmartClipDataInternal(crop_rect, clip_text,
                                                  clip_html, clip_rect);
  EXPECT_STREQ("", clip_text.Utf8().c_str());
  EXPECT_STREQ("", clip_html.Utf8().c_str());
}

TEST_F(WebFrameTest, SmartClipDoesNotCrashPositionReversed) {
  String clip_text;
  String clip_html;
  gfx::Rect clip_rect;
  RegisterMockedHttpURLLoad("Ahem.ttf");
  RegisterMockedHttpURLLoad("smartclip_reversed_positions.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ +
                                    "smartclip_reversed_positions.html");
  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  web_view_helper.Resize(gfx::Size(500, 500));
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  // Left upper corner of the rect will be end position in the DOM hierarchy.
  gfx::Rect crop_rect(30, 110, 400, 250);
  // This should not still crash. See crbug.com/589082 for more details.
  frame->GetFrame()->ExtractSmartClipDataInternal(crop_rect, clip_text,
                                                  clip_html, clip_rect);
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

#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/1090246): Fix these tests on Fuchsia and re-enable.
// TODO(crbug.com/1317375): Build these tests on all platforms.
#define MAYBE_SelectRangeStaysHorizontallyAlignedWhenMoved \
  DISABLED_SelectRangeStaysHorizontallyAlignedWhenMoved
#define MAYBE_MoveCaretStaysHorizontallyAlignedWhenMoved \
  DISABLED_MoveCaretStaysHorizontallyAlignedWhenMoved
#else
#define MAYBE_SelectRangeStaysHorizontallyAlignedWhenMoved \
  SelectRangeStaysHorizontallyAlignedWhenMoved
#define MAYBE_MoveCaretStaysHorizontallyAlignedWhenMoved \
  MoveCaretStaysHorizontallyAlignedWhenMoved
#endif
TEST_F(WebFrameTest, MAYBE_SelectRangeStaysHorizontallyAlignedWhenMoved) {
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

TEST_F(WebFrameTest, MAYBE_MoveCaretStaysHorizontallyAlignedWhenMoved) {
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

class CompositedSelectionBoundsTest
    : public WebFrameTest,
      private ScopedCompositedSelectionUpdateForTest {
 protected:
  CompositedSelectionBoundsTest()
      : ScopedCompositedSelectionUpdateForTest(true) {
    RegisterMockedHttpURLLoad("Ahem.ttf");

    web_view_helper_.Initialize(nullptr, nullptr);
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

    cc::LayerTreeHost* layer_tree_host = web_view_helper_.GetLayerTreeHost();
    const cc::LayerSelection& selection = layer_tree_host->selection();

    ASSERT_EQ(selection, cc::LayerSelection());
    ASSERT_EQ(selection.start, cc::LayerSelectionBound());
    ASSERT_EQ(selection.end, cc::LayerSelectionBound());
  }

  void RunTest(const char* test_file, bool selection_is_caret = false) {
    RegisterMockedHttpURLLoad(test_file);
    web_view_helper_.GetWebView()->MainFrameWidget()->SetFocus(true);
    frame_test_helpers::LoadFrame(
        web_view_helper_.GetWebView()->MainFrameImpl(), base_url_ + test_file);

    UpdateAllLifecyclePhases(web_view_helper_.GetWebView());

    v8::Isolate* isolate = web_view_helper_.GetAgentGroupScheduler().Isolate();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Value> result =
        web_view_helper_.GetWebView()
            ->MainFrameImpl()
            ->ExecuteScriptAndReturnValue(WebScriptSource("expectedResult"));
    ASSERT_FALSE(result.IsEmpty() || (*result)->IsUndefined());

    ASSERT_TRUE((*result)->IsArray());
    v8::Array& expected_result = *v8::Array::Cast(*result);
    ASSERT_GE(expected_result.Length(), 10u);

    v8::Local<v8::Context> context =
        expected_result.GetCreationContext(isolate).ToLocalChecked();
    v8::Context::Scope v8_context_scope(context);

    int start_edge_start_in_layer_x = expected_result.Get(context, 1)
                                          .ToLocalChecked()
                                          .As<v8::Int32>()
                                          ->Value();
    int start_edge_start_in_layer_y = expected_result.Get(context, 2)
                                          .ToLocalChecked()
                                          .As<v8::Int32>()
                                          ->Value();
    int start_edge_end_in_layer_x = expected_result.Get(context, 3)
                                        .ToLocalChecked()
                                        .As<v8::Int32>()
                                        ->Value();
    int start_edge_end_in_layer_y = expected_result.Get(context, 4)
                                        .ToLocalChecked()
                                        .As<v8::Int32>()
                                        ->Value();

    int end_edge_start_in_layer_x = expected_result.Get(context, 6)
                                        .ToLocalChecked()
                                        .As<v8::Int32>()
                                        ->Value();
    int end_edge_start_in_layer_y = expected_result.Get(context, 7)
                                        .ToLocalChecked()
                                        .As<v8::Int32>()
                                        ->Value();
    int end_edge_end_in_layer_x = expected_result.Get(context, 8)
                                      .ToLocalChecked()
                                      .As<v8::Int32>()
                                      ->Value();
    int end_edge_end_in_layer_y = expected_result.Get(context, 9)
                                      .ToLocalChecked()
                                      .As<v8::Int32>()
                                      ->Value();

    gfx::PointF hit_point;

    if (expected_result.Length() >= 17) {
      hit_point = gfx::PointF(expected_result.Get(context, 15)
                                  .ToLocalChecked()
                                  .As<v8::Int32>()
                                  ->Value(),
                              expected_result.Get(context, 16)
                                  .ToLocalChecked()
                                  .As<v8::Int32>()
                                  ->Value());
    } else {
      hit_point =
          gfx::PointF((start_edge_start_in_layer_x + start_edge_end_in_layer_x +
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

    UpdateAllLifecyclePhases(web_view_helper_.GetWebView());

    cc::LayerTreeHost* layer_tree_host = web_view_helper_.GetLayerTreeHost();
    const cc::LayerSelection& selection = layer_tree_host->selection();

    ASSERT_NE(selection, cc::LayerSelection());
    ASSERT_NE(selection.start, cc::LayerSelectionBound());
    ASSERT_NE(selection.end, cc::LayerSelectionBound());

    blink::Node* layer_owner_node_for_start =
        V8Node::ToWrappable(web_view_helper_.GetAgentGroupScheduler().Isolate(),
                            expected_result.Get(context, 0).ToLocalChecked());
    // Hidden selection does not always have a layer (might be hidden due to not
    // having been painted.
    ASSERT_TRUE(layer_owner_node_for_start || selection.start.hidden);
    int start_layer_id = 0;
    if (layer_owner_node_for_start) {
      start_layer_id = LayerIdFromNode(layer_tree_host->root_layer(),
                                       layer_owner_node_for_start);
    }
    if (selection_is_caret) {
      // The selection data are recorded on the caret layer which is the next
      // layer for the current test cases.
      start_layer_id++;
      EXPECT_EQ("Caret",
                layer_tree_host->LayerById(start_layer_id)->DebugName());
      // The locations are relative to the caret layer.
      start_edge_end_in_layer_x -= start_edge_start_in_layer_x;
      start_edge_end_in_layer_y -= start_edge_start_in_layer_y;
      start_edge_start_in_layer_x = 0;
      start_edge_start_in_layer_y = 0;
    }
    EXPECT_EQ(start_layer_id, selection.start.layer_id);

    EXPECT_NEAR(start_edge_start_in_layer_x, selection.start.edge_start.x(), 1);
    EXPECT_NEAR(start_edge_start_in_layer_y, selection.start.edge_start.y(), 1);
    EXPECT_NEAR(start_edge_end_in_layer_x, selection.start.edge_end.x(), 1);

    blink::Node* layer_owner_node_for_end =
        V8Node::ToWrappable(web_view_helper_.GetAgentGroupScheduler().Isolate(),
                            expected_result.Get(context, 5).ToLocalChecked());
    // Hidden selection does not always have a layer (might be hidden due to not
    // having been painted.
    ASSERT_TRUE(layer_owner_node_for_end || selection.end.hidden);
    int end_layer_id = 0;
    if (layer_owner_node_for_end) {
      end_layer_id = LayerIdFromNode(layer_tree_host->root_layer(),
                                     layer_owner_node_for_end);
    }

    if (selection_is_caret) {
      // The selection data are recorded on the caret layer which is the next
      // layer for the current test cases.
      end_layer_id++;
      EXPECT_EQ(start_layer_id, end_layer_id);
      // The locations are relative to the caret layer.
      end_edge_end_in_layer_x -= end_edge_start_in_layer_x;
      end_edge_end_in_layer_y -= end_edge_start_in_layer_y;
      end_edge_start_in_layer_x = 0;
      end_edge_start_in_layer_y = 0;
    }
    EXPECT_EQ(end_layer_id, selection.end.layer_id);

    EXPECT_NEAR(end_edge_start_in_layer_x, selection.end.edge_start.x(), 1);
    EXPECT_NEAR(end_edge_start_in_layer_y, selection.end.edge_start.y(), 1);
    EXPECT_NEAR(end_edge_end_in_layer_x, selection.end.edge_end.x(), 1);

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

  void RunTestWithCaret(const char* test_file) {
    RunTest(test_file, /*selection_is_caret*/ true);
  }

  static int LayerIdFromNode(const cc::Layer* root_layer, blink::Node* node) {
    Vector<const cc::Layer*> layers;
    if (node->IsDocumentNode()) {
      layers = CcLayersByName(root_layer,
                              "Scrolling background of LayoutView #document");
    } else {
      DCHECK(node->IsElementNode());
      layers = CcLayersByDOMElementId(root_layer,
                                      To<Element>(node)->GetIdAttribute());
    }

    EXPECT_EQ(layers.size(), 1u);
    return layers[0]->id();
  }

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
TEST_F(CompositedSelectionBoundsTest, BasicRTL) {
  RunTest("composited_selection_bounds_basic_rtl.html");
}
TEST_F(CompositedSelectionBoundsTest, VerticalRightToLeftRTL) {
  RunTest("composited_selection_bounds_vertical_rl_rtl.html");
}
TEST_F(CompositedSelectionBoundsTest, VerticalLeftToRightRTL) {
  RunTest("composited_selection_bounds_vertical_lr_rtl.html");
}
TEST_F(CompositedSelectionBoundsTest, SplitLayer) {
  RunTest("composited_selection_bounds_split_layer.html");
}
TEST_F(CompositedSelectionBoundsTest, Iframe) {
  RunTestWithMultipleFiles("composited_selection_bounds_iframe.html",
                           {"composited_selection_bounds_basic.html"});
}
TEST_F(CompositedSelectionBoundsTest, Editable) {
  web_view_helper_.GetWebView()->GetSettings()->SetDefaultFontSize(16);
  RunTestWithCaret("composited_selection_bounds_editable.html");
}
TEST_F(CompositedSelectionBoundsTest, EditableDiv) {
  RunTestWithCaret("composited_selection_bounds_editable_div.html");
}
TEST_F(CompositedSelectionBoundsTest, SVGBasic) {
  RunTest("composited_selection_bounds_svg_basic.html");
}
TEST_F(CompositedSelectionBoundsTest, SVGTextWithFragments) {
  RunTest("composited_selection_bounds_svg_text_with_fragments.html");
}
TEST_F(CompositedSelectionBoundsTest, LargeSelectionScroll) {
  RunTest("composited_selection_bounds_large_selection_scroll.html");
}
TEST_F(CompositedSelectionBoundsTest, LargeSelectionNoScroll) {
  RunTest("composited_selection_bounds_large_selection_noscroll.html");
}
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#if !BUILDFLAG(IS_ANDROID)
TEST_F(CompositedSelectionBoundsTest, Input) {
  web_view_helper_.GetWebView()->GetSettings()->SetDefaultFontSize(16);
  RunTest("composited_selection_bounds_input.html");
}
TEST_F(CompositedSelectionBoundsTest, InputScrolled) {
  web_view_helper_.GetWebView()->GetSettings()->SetDefaultFontSize(16);
  RunTest("composited_selection_bounds_input_scrolled.html");
}
#endif
#endif

class CompositedSelectionBoundsTestWithImage
    : public CompositedSelectionBoundsTest {
 public:
  CompositedSelectionBoundsTestWithImage() : CompositedSelectionBoundsTest() {
    RegisterMockedHttpURLLoad("notifications/120x120.png");
  }
};

TEST_F(CompositedSelectionBoundsTestWithImage, Replaced) {
  RunTest("composited_selection_bounds_replaced.html");
}

TEST_F(CompositedSelectionBoundsTestWithImage, ReplacedRTL) {
  RunTest("composited_selection_bounds_replaced_rtl.html");
}

TEST_F(CompositedSelectionBoundsTestWithImage, ReplacedVerticalLR) {
  RunTest("composited_selection_bounds_replaced_vertical_lr.html");
}

class TestWillInsertBodyWebFrameClient final
    : public frame_test_helpers::TestWebFrameClient {
 public:
  TestWillInsertBodyWebFrameClient() = default;
  ~TestWillInsertBodyWebFrameClient() override = default;

  bool did_load() const { return did_load_; }

  // frame_test_helpers::TestWebFrameClient:
  void DidCommitNavigation(
      WebHistoryCommitType commit_type,
      bool should_reset_browser_interface_broker,
      const ParsedPermissionsPolicy& permissions_policy_header,
      const DocumentPolicyFeatureState& document_policy_header) final {
    did_load_ = true;
  }

 private:
  bool did_load_ = false;
};

TEST_F(WebFrameTest, HTMLDocument) {
  RegisterMockedHttpURLLoad("clipped-body.html");

  TestWillInsertBodyWebFrameClient web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "clipped-body.html",
                                    &web_frame_client);

  EXPECT_TRUE(web_frame_client.did_load());
}

TEST_F(WebFrameTest, EmptyDocument) {
  RegisterMockedHttpURLLoad("frameserializer/svg/green_rectangle.svg");

  TestWillInsertBodyWebFrameClient web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(&web_frame_client);

  EXPECT_FALSE(web_frame_client.did_load());
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
  Element* element = document->getElementById(AtomicString("data"));

  web_view_helper.GetWebView()->GetSettings()->SetEditingBehavior(
      mojom::EditingBehavior::kEditingWindowsBehavior);

  element->Focus();
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
                     SelectionMenuBehavior::kHide,
                     WebLocalFrame::kSelectionSetFocus);
  EphemeralRange selection_range = frame->GetFrame()
                                       ->Selection()
                                       .ComputeVisibleSelectionInDOMTree()
                                       .ToNormalizedEphemeralRange();

  EXPECT_EQ(1, textcheck.NumberOfTimesChecked());
  EXPECT_EQ(1, NumMarkersInRange(document, selection_range,
                                 DocumentMarker::MarkerTypes::Spelling()));

  frame->ReplaceMisspelledRange("welcome");
  EXPECT_EQ("_welcome_.", TestWebFrameContentDumper::DumpWebViewAsText(
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
  Element* element = document->getElementById(AtomicString("data"));

  web_view_helper.GetWebView()->GetSettings()->SetEditingBehavior(
      mojom::EditingBehavior::kEditingWindowsBehavior);

  element->Focus();
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
                     SelectionMenuBehavior::kHide,
                     WebLocalFrame::kSelectionSetFocus);
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
  for (wtf_size_t i = 0; i < document_markers.size(); ++i)
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
  Element* element = document->getElementById(AtomicString("data"));

  web_view_helper.GetWebView()->GetSettings()->SetEditingBehavior(
      mojom::EditingBehavior::kEditingWindowsBehavior);

  element->Focus();
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
  Element* element = document->getElementById(AtomicString("data"));

  web_view_helper.GetWebView()->GetSettings()->SetEditingBehavior(
      mojom::EditingBehavior::kEditingWindowsBehavior);

  element->Focus();
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
  Element* element = document->getElementById(AtomicString("data"));

  web_view_helper.GetWebView()->GetSettings()->SetEditingBehavior(
      mojom::EditingBehavior::kEditingWindowsBehavior);

  element->Focus();
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
  Element* element = document->getElementById(AtomicString("data"));

  web_view_helper.GetWebView()->GetSettings()->SetEditingBehavior(
      mojom::EditingBehavior::kEditingWindowsBehavior);

  element->Focus();
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

class TestAccessInitialDocumentLocalFrameHost
    : public mojom::blink::LocalMainFrameHost {
 public:
  TestAccessInitialDocumentLocalFrameHost() = default;
  ~TestAccessInitialDocumentLocalFrameHost() override = default;

  void Init(blink::AssociatedInterfaceProvider* provider) {
    provider->OverrideBinderForTesting(
        mojom::blink::LocalMainFrameHost::Name_,
        WTF::BindRepeating(
            &TestAccessInitialDocumentLocalFrameHost::BindFrameHostReceiver,
            WTF::Unretained(this)));
  }

  // LocalMainFrameHost:
  void ScaleFactorChanged(float scale) override {}
  void ContentsPreferredSizeChanged(const ::gfx::Size& pref_size) override {}
  void TextAutosizerPageInfoChanged(
      ::blink::mojom::blink::TextAutosizerPageInfoPtr page_info) override {}
  void FocusPage() override {}
  void TakeFocus(bool reverse) override {}
  void UpdateTargetURL(const ::blink::KURL& url,
                       UpdateTargetURLCallback callback) override {
    std::move(callback).Run();
  }
  void RequestClose() override {}
  void ShowCreatedWindow(const ::blink::LocalFrameToken& opener_frame_token,
                         ::ui::mojom::blink::WindowOpenDisposition disposition,
                         const mojom::blink::WindowFeaturesPtr window_features,
                         bool opened_by_user_gesture,
                         ShowCreatedWindowCallback callback) override {
    std::move(callback).Run();
  }
  void SetWindowRect(const ::gfx::Rect& bounds,
                     SetWindowRectCallback callback) override {
    std::move(callback).Run();
  }
  void Minimize() override {}
  void Maximize() override {}
  void Restore() override {}
  void SetResizable(bool resizable) override {}
  void DidFirstVisuallyNonEmptyPaint() override {}
  void DidAccessInitialMainDocument() override {
    ++did_access_initial_main_document_;
  }
  void DraggableRegionsChanged(
      Vector<mojom::blink::DraggableRegionPtr> regions) override {}

  // !!!!!!!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!!!!!!
  // If the actual counts in the tests below increase, this could be an
  // indicator of a bug that causes DidAccessInitialMainDocument() to always be
  // invoked, regardless of whether or not the initial document is accessed.
  // Please do not simply increment the expected counts in the below tests
  // without understanding what's causing the increased count.
  int did_access_initial_main_document_ = 0;

 private:
  void BindFrameHostReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<mojom::blink::LocalMainFrameHost>(
            std::move(handle)));
  }
  mojo::AssociatedReceiver<mojom::blink::LocalMainFrameHost> receiver_{this};
};

TEST_F(WebFrameTest, DidAccessInitialMainDocumentBody) {
  TestAccessInitialDocumentLocalFrameHost frame_host;
  frame_test_helpers::TestWebFrameClient web_frame_client;
  frame_host.Init(web_frame_client.GetRemoteNavigationAssociatedInterfaces());
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(&web_frame_client);
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_main_document_);

  // Create another window that will try to access it.
  frame_test_helpers::WebViewHelper new_web_view_helper;
  WebViewImpl* new_view = new_web_view_helper.InitializeWithOpener(
      web_view_helper.GetWebView()->MainFrame());
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_main_document_);

  // Access the initial document by modifying the body.
  new_view->MainFrameImpl()->ExecuteScript(
      WebScriptSource("window.opener.document.body.innerHTML += 'Modified';"));
  RunPendingTasks();
  EXPECT_EQ(1, frame_host.did_access_initial_main_document_);

  web_view_helper.Reset();
}

TEST_F(WebFrameTest, DidAccessInitialMainDocumentOpen) {
  TestAccessInitialDocumentLocalFrameHost frame_host;
  frame_test_helpers::TestWebFrameClient web_frame_client;
  frame_host.Init(web_frame_client.GetRemoteNavigationAssociatedInterfaces());
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(&web_frame_client);
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_main_document_);

  // Create another window that will try to access it.
  frame_test_helpers::WebViewHelper new_web_view_helper;
  WebViewImpl* new_view = new_web_view_helper.InitializeWithOpener(
      web_view_helper.GetWebView()->MainFrame());
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_main_document_);

  // Access the initial document by calling document.open(), which allows
  // arbitrary modification of the initial document.
  new_view->MainFrameImpl()->ExecuteScript(
      WebScriptSource("window.opener.document.open();"));
  RunPendingTasks();
  EXPECT_EQ(1, frame_host.did_access_initial_main_document_);

  web_view_helper.Reset();
}

TEST_F(WebFrameTest, DidAccessInitialMainDocumentNavigator) {
  TestAccessInitialDocumentLocalFrameHost frame_host;
  frame_test_helpers::TestWebFrameClient web_frame_client;
  frame_host.Init(web_frame_client.GetRemoteNavigationAssociatedInterfaces());
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(&web_frame_client);
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_main_document_);

  // Create another window that will try to access it.
  frame_test_helpers::WebViewHelper new_web_view_helper;
  WebViewImpl* new_view = new_web_view_helper.InitializeWithOpener(
      web_view_helper.GetWebView()->MainFrame());
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_main_document_);

  // Access the initial document to get to the navigator object.
  new_view->MainFrameImpl()->ExecuteScript(
      WebScriptSource("console.log(window.opener.navigator);"));
  RunPendingTasks();
  EXPECT_EQ(1, frame_host.did_access_initial_main_document_);

  web_view_helper.Reset();
}

TEST_F(WebFrameTest, DidAccessInitialMainDocumentViaJavascriptUrl) {
  TestAccessInitialDocumentLocalFrameHost frame_host;
  frame_test_helpers::TestWebFrameClient web_frame_client;
  frame_host.Init(web_frame_client.GetRemoteNavigationAssociatedInterfaces());
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(&web_frame_client);
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_main_document_);

  // Access the initial document from a javascript: URL.
  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                "javascript:document.body.appendChild(document."
                                "createTextNode('Modified'))");
  EXPECT_EQ(1, frame_host.did_access_initial_main_document_);

  web_view_helper.Reset();
}

TEST_F(WebFrameTest, DidAccessInitialMainDocumentBodyBeforeModalDialog) {
  TestAccessInitialDocumentLocalFrameHost frame_host;
  frame_test_helpers::TestWebFrameClient web_frame_client;
  frame_host.Init(web_frame_client.GetRemoteNavigationAssociatedInterfaces());
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(&web_frame_client);
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_main_document_);

  // Create another window that will try to access it.
  frame_test_helpers::WebViewHelper new_web_view_helper;
  WebViewImpl* new_view = new_web_view_helper.InitializeWithOpener(
      web_view_helper.GetWebView()->MainFrame());
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_main_document_);

  // Access the initial document by modifying the body.
  new_view->MainFrameImpl()->ExecuteScript(
      WebScriptSource("window.opener.document.body.innerHTML += 'Modified';"));
  RunPendingTasks();
  EXPECT_EQ(1, frame_host.did_access_initial_main_document_);

  // Run a modal dialog, which used to run a nested run loop and require
  // a special case for notifying about the access.
  new_view->MainFrameImpl()->ExecuteScript(
      WebScriptSource("window.opener.confirm('Modal');"));
  RunPendingTasks();
  EXPECT_EQ(1, frame_host.did_access_initial_main_document_);

  // Ensure that we don't notify again later.
  RunPendingTasks();
  EXPECT_EQ(1, frame_host.did_access_initial_main_document_);

  web_view_helper.Reset();
}

TEST_F(WebFrameTest, DidWriteToInitialMainDocumentBeforeModalDialog) {
  TestAccessInitialDocumentLocalFrameHost frame_host;
  frame_test_helpers::TestWebFrameClient web_frame_client;
  frame_host.Init(web_frame_client.GetRemoteNavigationAssociatedInterfaces());
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(&web_frame_client);
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_main_document_);

  // Create another window that will try to access it.
  frame_test_helpers::WebViewHelper new_web_view_helper;
  WebViewImpl* new_view = new_web_view_helper.InitializeWithOpener(
      web_view_helper.GetWebView()->MainFrame());
  RunPendingTasks();
  EXPECT_EQ(0, frame_host.did_access_initial_main_document_);

  // Access the initial document with document.write, which moves us past the
  // initial empty document state of the state machine.
  new_view->MainFrameImpl()->ExecuteScript(
      WebScriptSource("window.opener.document.write('Modified'); "
                      "window.opener.document.close();"));
  RunPendingTasks();
  EXPECT_EQ(1, frame_host.did_access_initial_main_document_);

  // Run a modal dialog, which used to run a nested run loop and require
  // a special case for notifying about the access.
  new_view->MainFrameImpl()->ExecuteScript(
      WebScriptSource("window.opener.confirm('Modal');"));
  RunPendingTasks();
  EXPECT_EQ(1, frame_host.did_access_initial_main_document_);

  // Ensure that we don't notify again later.
  RunPendingTasks();
  EXPECT_EQ(1, frame_host.did_access_initial_main_document_);

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
  scrollable_area->DidCompositorScroll(gfx::PointF(0, 1));
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->ApplyViewportChangesForTesting({gfx::Vector2dF(), gfx::Vector2dF(),
                                        1.7f, false, 0, 0,
                                        cc::BrowserControlsState::kBoth});
  EXPECT_TRUE(client.WasFrameScrolled());
  EXPECT_TRUE(initial_scroll_state.was_scrolled_by_user);

  client.Reset();
  initial_scroll_state.was_scrolled_by_user = false;

  // The page scale 1.0f and scroll.
  scrollable_area->DidCompositorScroll(gfx::PointF(0, 2));
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->ApplyViewportChangesForTesting({gfx::Vector2dF(), gfx::Vector2dF(),
                                        1.0f, false, 0, 0,
                                        cc::BrowserControlsState::kBoth});
  EXPECT_TRUE(client.WasFrameScrolled());
  EXPECT_TRUE(initial_scroll_state.was_scrolled_by_user);
  client.Reset();
  initial_scroll_state.was_scrolled_by_user = false;

  // No scroll event if there is no scroll delta.
  scrollable_area->DidCompositorScroll(gfx::PointF(0, 2));
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->ApplyViewportChangesForTesting({gfx::Vector2dF(), gfx::Vector2dF(),
                                        1.0f, false, 0, 0,
                                        cc::BrowserControlsState::kBoth});
  EXPECT_FALSE(client.WasFrameScrolled());
  EXPECT_FALSE(initial_scroll_state.was_scrolled_by_user);
  client.Reset();

  // Non zero page scale and scroll.
  scrollable_area->DidCompositorScroll(gfx::PointF(9, 15));
  web_view_helper.GetWebView()
      ->MainFrameWidget()
      ->ApplyViewportChangesForTesting({gfx::Vector2dF(), gfx::Vector2dF(),
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
  EXPECT_TRUE(web_view_helper.GetWebView()
                  ->MainFrameImpl()
                  ->GetDocument()
                  .SiteForCookies()
                  .IsEquivalent(net::SiteForCookies::FromUrl(GURL(redirect))));
}

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

  WebView* CreateNewWindow(
      const WebURLRequest&,
      const WebWindowFeatures&,
      const WebString&,
      WebNavigationPolicy,
      network::mojom::blink::WebSandboxFlags,
      const SessionStorageNamespaceId&,
      bool& consumed_user_gesture,
      const std::optional<Impression>&,
      const std::optional<WebPictureInPictureWindowOptions>&,
      const WebURL&) override {
    EXPECT_TRUE(false);
    return nullptr;
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
  TestNewWindowWebFrameClient web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "ctrl_click.html",
                                    &web_frame_client);

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
  frame_request.SetTriggeringEventInfo(
      mojom::blink::TriggeringEventInfo::kFromTrustedEvent);
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
  auto* main_frame = web_view_helper.GetWebView()->MainFrameImpl();

  frame_test_helpers::LoadFrame(main_frame,
                                "javascript:document.forms[0].submit()");
  // Pump requests one more time after the javascript URL has executed to
  // trigger the actual POST load request.
  frame_test_helpers::PumpPendingRequestsForFrameToLoad(
      web_view_helper.LocalMainFrame());
  EXPECT_EQ(WebString::FromUTF8("POST"),
            frame->GetDocumentLoader()->HttpMethod());

  frame_test_helpers::ReloadFrame(frame);
  EXPECT_EQ(mojom::FetchCacheMode::kValidateCache, client.GetCacheMode());
  EXPECT_EQ(kWebNavigationTypeFormResubmittedReload,
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
  TestCachePolicyWebFrameClient& ChildClient(wtf_size_t i) {
    return *child_clients_[i].get();
  }
  wtf_size_t ChildFrameCreationCount() const { return child_clients_.size(); }

  // frame_test_helpers::TestWebFrameClient:
  WebLocalFrame* CreateChildFrame(
      mojom::blink::TreeScopeType scope,
      const WebString&,
      const WebString&,
      const FramePolicy&,
      const WebFrameOwnerProperties& frame_owner_properties,
      FrameOwnerElementType,
      WebPolicyContainerBindParams policy_container_bind_params,
      ukm::SourceId document_ukm_source_id,
      FinishChildFrameCreationFn finish_creation) override {
    auto child = std::make_unique<TestCachePolicyWebFrameClient>();
    auto* child_ptr = child.get();
    child_clients_.push_back(std::move(child));
    return CreateLocalChild(*Frame(), scope, child_ptr,
                            std::move(policy_container_bind_params),
                            finish_creation);
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

class TestMainFrameIntersectionChanged
    : public frame_test_helpers::TestWebFrameClient {
 public:
  TestMainFrameIntersectionChanged() = default;
  ~TestMainFrameIntersectionChanged() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void OnMainFrameIntersectionChanged(
      const gfx::Rect& intersection_rect) override {
    main_frame_intersection_ = intersection_rect;
  }

  gfx::Rect MainFrameIntersection() const { return main_frame_intersection_; }

 private:
  gfx::Rect main_frame_intersection_;
};

TEST_F(WebFrameTest, MainFrameIntersectionChanged) {
  TestMainFrameIntersectionChanged client;
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote();

  WebLocalFrameImpl* local_frame =
      helper.CreateLocalChild(*helper.RemoteMainFrame(), "frameName",
                              WebFrameOwnerProperties(), nullptr, &client);

  WebFrameWidget* widget = local_frame->FrameWidget();
  ASSERT_TRUE(widget);

  gfx::Rect viewport_intersection(0, 11, 200, 89);
  gfx::Rect mainframe_intersection(0, 0, 200, 140);
  blink::mojom::FrameOcclusionState occlusion_state =
      blink::mojom::FrameOcclusionState::kUnknown;
  gfx::Transform transform;
  transform.Translate(100, 100);

  auto intersection_state = blink::mojom::blink::ViewportIntersectionState::New(
      viewport_intersection, mainframe_intersection, gfx::Rect(),
      occlusion_state, gfx::Size(), gfx::Point(), transform);
  static_cast<WebFrameWidgetImpl*>(widget)->ApplyViewportIntersectionForTesting(
      std::move(intersection_state));
  EXPECT_EQ(client.MainFrameIntersection(), gfx::Rect(100, 100, 200, 140));
}

class TestSameDocumentWithImageWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  TestSameDocumentWithImageWebFrameClient() : num_of_image_requests_(0) {}
  ~TestSameDocumentWithImageWebFrameClient() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void FinalizeRequest(WebURLRequest& request) override {
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
  web_view_helper.Initialize(&client, nullptr,
                             &ConfigureLoadsImagesAutomatically);
  // TODO(crbug.com/1329535): Remove if threaded preload scanner doesn't launch.
  // Disable preload scanner so it doesn't make any requests.
  web_view_helper.LocalMainFrame()
      ->GetFrame()
      ->GetDocument()
      ->GetSettings()
      ->SetDoHtmlPreloadScanning(false);
  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                base_url_ + "foo_with_image.html");
  EXPECT_EQ(client.NumOfImageRequests(), 1);

  WebCache::Clear();
  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                base_url_ + "foo_with_image.html");
  EXPECT_EQ(client.NumOfImageRequests(), 2);
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

TEST_F(WebFrameTest,
       CommitSynchronousNavigationForAboutBlankAndCheckStorageKeyNonce) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("data:text/html,<iframe></iframe>");

  StorageKey storage_key = StorageKey::CreateWithNonce(
      url::Origin(), base::UnguessableToken::Create());

  auto* child_frame =
      To<WebLocalFrameImpl>(web_view_helper.LocalMainFrame()->FirstChild());
  child_frame->GetFrame()->DomWindow()->SetStorageKey(storage_key);

  auto params = std::make_unique<WebNavigationParams>();
  params->url = url_test_helpers::ToKURL("about:blank");
  params->navigation_timings.navigation_start = base::TimeTicks::Now();
  params->navigation_timings.fetch_start = base::TimeTicks::Now();
  params->is_browser_initiated = true;
  MockPolicyContainerHost mock_policy_container_host;
  params->policy_container = std::make_unique<WebPolicyContainer>(
      WebPolicyContainerPolicies(),
      mock_policy_container_host.BindNewEndpointAndPassDedicatedRemote());
  params->is_synchronous_commit_for_bug_778318 = true;

  child_frame->CommitNavigation(std::move(params), nullptr);
  frame_test_helpers::PumpPendingRequestsForFrameToLoad(child_frame);

  // The synchronous commit for bug 778318 should not change the storage key.
  EXPECT_EQ(storage_key.nonce(),
            child_frame->GetFrame()->DomWindow()->GetStorageKey().GetNonce());
}

class TestDidNavigateCommitTypeWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  TestDidNavigateCommitTypeWebFrameClient()
      : last_commit_type_(kWebHistoryInertCommit) {}
  ~TestDidNavigateCommitTypeWebFrameClient() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void DidFinishSameDocumentNavigation(
      WebHistoryCommitType type,
      bool is_synchronously_committed,
      mojom::blink::SameDocumentNavigationType,
      bool is_client_redirect,
      const std::optional<blink::SameDocNavigationScreenshotDestinationToken>&
          screenshot_destination) override {
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
      ClientRedirectPolicy::kNotClientRedirect,
      false /* has_transient_user_activation */, /*initiator_origin=*/nullptr,
      /*is_synchronously_committed=*/false, /*source_element=*/nullptr,
      mojom::blink::TriggeringEventInfo::kNotFromEvent,
      /*is_browser_initiated=*/true,
      /*has_ua_visual_transition,=*/false,
      /*soft_navigation_heuristics_task_id=*/std::nullopt);
  EXPECT_EQ(kWebBackForwardCommit, client.LastCommitType());
}

TEST_F(WebFrameTest, SameDocumentHistoryNavigationPropagatesSequenceNumber) {
  RegisterMockedHttpURLLoad("push_state_empty.html");
  frame_test_helpers::TestWebFrameClient client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "push_state_empty.html", &client);
  auto* local_frame = To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  Persistent<HistoryItem> item =
      local_frame->Loader().GetDocumentLoader()->GetHistoryItem();
  RunPendingTasks();

  local_frame->Loader().GetDocumentLoader()->CommitSameDocumentNavigation(
      item->Url(), WebFrameLoadType::kBackForward, item.Get(),
      ClientRedirectPolicy::kNotClientRedirect,
      false /* has_transient_user_activation */, /*initiator_origin=*/nullptr,
      /*is_synchronously_committed=*/false, /*source_element=*/nullptr,
      mojom::blink::TriggeringEventInfo::kNotFromEvent,
      /*is_browser_initiated=*/true,
      /*has_ua_visual_transition,=*/false,
      /*soft_navigation_heuristics_task_id=*/std::nullopt);

  EXPECT_EQ(item->ItemSequenceNumber(),
            web_view_helper.GetLayerTreeHost()
                ->primary_main_frame_item_sequence_number_for_testing());
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
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();
  web_view_helper.GetWebView()
      ->GetPage()
      ->GetSettings()
      .SetPreferCompositingToLCDTextForTesting(true);

  web_view_helper.Resize(gfx::Size(100, 100));
  frame_test_helpers::LoadFrame(web_view_helper.GetWebView()->MainFrameImpl(),
                                base_url_ + "non-scrollable.html");

  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  auto* layout_view =
      web_view_helper.LocalMainFrame()->GetFrameView()->GetLayoutView();
  // Verify that the cc::Layer is not scrollable initially.
  auto* scroll_node = GetScrollNode(*layout_view);
  ASSERT_TRUE(scroll_node);
  ASSERT_FALSE(scroll_node->UserScrollableHorizontal());
  ASSERT_FALSE(scroll_node->UserScrollableVertical());

  // Call javascript to make the layer scrollable, and verify it.
  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  frame->ExecuteScript(WebScriptSource("allowScroll();"));
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  scroll_node = GetScrollNode(*layout_view);
  ASSERT_TRUE(scroll_node);
  ASSERT_TRUE(scroll_node->UserScrollableHorizontal());
  ASSERT_TRUE(scroll_node->UserScrollableVertical());
}

// Test that currentHistoryItem reflects the current page, not the provisional
// load.
TEST_F(WebFrameTest, CurrentHistoryItem) {
  RegisterMockedHttpURLLoad("fixed_layout.html");
  std::string url = base_url_ + "fixed_layout.html";

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();
  FrameLoader& main_frame_loader =
      web_view_helper.LocalMainFrame()->GetFrame()->Loader();

  // Before navigation, there is no history item.
  EXPECT_FALSE(main_frame_loader.GetDocumentLoader()->GetHistoryItem());

  FrameLoadRequest frame_load_request(nullptr, ResourceRequest(ToKURL(url)));
  main_frame_loader.StartNavigation(frame_load_request);
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
      mojom::blink::TreeScopeType scope,
      const WebString& name,
      const WebString& fallback_name,
      const FramePolicy&,
      const WebFrameOwnerProperties& frame_owner_properties,
      FrameOwnerElementType,
      WebPolicyContainerBindParams policy_container_bind_params,
      ukm::SourceId document_ukm_source_id,
      FinishChildFrameCreationFn finish_creation) override {
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
      ConfigureAndroid);

  WebViewImpl* web_view = web_view_helper.GetWebView();
  web_view_helper.Resize(gfx::Size(100, 100));

  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();
  Element* bottom_fixed =
      document->getElementById(AtomicString("bottom-fixed"));
  Element* top_bottom_fixed =
      document->getElementById(AtomicString("top-bottom-fixed"));
  Element* right_fixed = document->getElementById(AtomicString("right-fixed"));
  Element* left_right_fixed =
      document->getElementById(AtomicString("left-right-fixed"));

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
  EXPECT_EQ(gfx::Rect(0, 0, 200, 200), frame_view->FrameRect());
  frame_view->SetFrameRect(gfx::Rect(100, 100, 200, 200));
  EXPECT_EQ(gfx::Rect(100, 100, 200, 200), frame_view->FrameRect());
}

TEST_F(WebFrameTest, FrameViewScrollAccountsForBrowserControls) {
  RegisterMockedHttpURLLoad("long_scroll.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "long_scroll.html", nullptr,
                                    nullptr, ConfigureAndroid);

  WebViewImpl* web_view = web_view_helper.GetWebView();
  LocalFrameView* frame_view = web_view_helper.LocalMainFrame()->GetFrameView();

  float browser_controls_height = 40;
  web_view->ResizeWithBrowserControls(gfx::Size(100, 100),
                                      browser_controls_height, 0, false);
  web_view->SetPageScaleFactor(2.0f);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  web_view->MainFrameImpl()->SetScrollOffset(gfx::PointF(0, 2000));
  EXPECT_EQ(ScrollOffset(0, 1900),
            frame_view->LayoutViewport()->GetScrollOffset());

  // Simulate the browser controls showing by 20px, thus shrinking the viewport
  // and allowing it to scroll an additional 20px.
  web_view->MainFrameWidget()->ApplyViewportChangesForTesting(
      {gfx::Vector2dF(), gfx::Vector2dF(), 1.0f, false,
       20.0f / browser_controls_height, 0, cc::BrowserControlsState::kBoth});
  EXPECT_EQ(ScrollOffset(0, 1920),
            frame_view->LayoutViewport()->MaximumScrollOffset());

  // Show more, make sure the scroll actually gets clamped.
  web_view->MainFrameWidget()->ApplyViewportChangesForTesting(
      {gfx::Vector2dF(), gfx::Vector2dF(), 1.0f, false,
       20.0f / browser_controls_height, 0, cc::BrowserControlsState::kBoth});
  web_view->MainFrameImpl()->SetScrollOffset(gfx::PointF(0, 2000));
  EXPECT_EQ(ScrollOffset(0, 1940),
            frame_view->LayoutViewport()->GetScrollOffset());

  // Hide until there's 10px showing.
  web_view->MainFrameWidget()->ApplyViewportChangesForTesting(
      {gfx::Vector2dF(), gfx::Vector2dF(), 1.0f, false,
       -30.0f / browser_controls_height, 0, cc::BrowserControlsState::kBoth});
  EXPECT_EQ(ScrollOffset(0, 1910),
            frame_view->LayoutViewport()->MaximumScrollOffset());

  // Simulate a LayoutEmbeddedContent::resize. The frame is resized to
  // accommodate the browser controls and Blink's view of the browser controls
  // matches that of the CC
  web_view->MainFrameWidget()->ApplyViewportChangesForTesting(
      {gfx::Vector2dF(), gfx::Vector2dF(), 1.0f, false,
       30.0f / browser_controls_height, 0, cc::BrowserControlsState::kBoth});
  web_view->ResizeWithBrowserControls(gfx::Size(100, 60), 40.0f, 0, true);
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());
  EXPECT_EQ(ScrollOffset(0, 1940),
            frame_view->LayoutViewport()->MaximumScrollOffset());

  // Now simulate hiding.
  web_view->MainFrameWidget()->ApplyViewportChangesForTesting(
      {gfx::Vector2dF(), gfx::Vector2dF(), 1.0f, false,
       -10.0f / browser_controls_height, 0, cc::BrowserControlsState::kBoth});
  EXPECT_EQ(ScrollOffset(0, 1930),
            frame_view->LayoutViewport()->MaximumScrollOffset());

  // Reset to original state: 100px widget height, browser controls fully
  // hidden.
  web_view->MainFrameWidget()->ApplyViewportChangesForTesting(
      {gfx::Vector2dF(), gfx::Vector2dF(), 1.0f, false,
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
      {gfx::Vector2dF(), gfx::Vector2dF(), 1.0f, false,
       1.0f / browser_controls_height, 0, cc::BrowserControlsState::kBoth});
  EXPECT_EQ(ScrollOffset(0, 1901),
            frame_view->LayoutViewport()->MaximumScrollOffset());

  web_view->MainFrameWidget()->ApplyViewportChangesForTesting(
      {gfx::Vector2dF(), gfx::Vector2dF(), 1.0f, false,
       2.0f / browser_controls_height, 0, cc::BrowserControlsState::kBoth});
  EXPECT_EQ(ScrollOffset(0, 1903),
            frame_view->LayoutViewport()->MaximumScrollOffset());
}

TEST_F(WebFrameTest, MaximumScrollPositionCanBeNegative) {
  RegisterMockedHttpURLLoad("rtl-overview-mode.html");

  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "rtl-overview-mode.html",
                                    nullptr, nullptr, ConfigureAndroid);
  web_view_helper.GetWebView()->SetInitialPageScaleOverride(-1);
  web_view_helper.GetWebView()->GetSettings()->SetWideViewportQuirkEnabled(
      true);
  web_view_helper.GetWebView()->GetSettings()->SetLoadWithOverviewMode(true);
  web_view_helper.GetWebView()->GetSettings()->SetUseWideViewport(true);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  UpdateAllLifecyclePhases(web_view_helper.GetWebView());

  LocalFrameView* frame_view = web_view_helper.LocalMainFrame()->GetFrameView();
  ScrollableArea* layout_viewport = frame_view->LayoutViewport();
  EXPECT_LT(layout_viewport->MaximumScrollOffset().x(), 0);
}

TEST_F(WebFrameTest, FullscreenLayerSize) {
  RegisterMockedHttpURLLoad("fullscreen_div.html");
  int viewport_width = 640;
  int viewport_height = 480;

  frame_test_helpers::WebViewHelper web_view_helper;
  //  client.screen_info_.rect = gfx::Rect(viewport_width, viewport_height);
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "fullscreen_div.html", nullptr, nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  UpdateAllLifecyclePhases(web_view_impl);

  LocalFrame* frame = web_view_impl->MainFrameImpl()->GetFrame();
  Document* document = frame->GetDocument();
  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  Element* div_fullscreen = document->getElementById(AtomicString("div1"));
  Fullscreen::RequestFullscreen(*div_fullscreen);
  EXPECT_EQ(nullptr, Fullscreen::FullscreenElementFrom(*document));
  web_view_impl->DidEnterFullscreen();
  EXPECT_EQ(div_fullscreen, Fullscreen::FullscreenElementFrom(*document));
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_EQ(div_fullscreen, Fullscreen::FullscreenElementFrom(*document));

  // Verify that the element is sized to the viewport.
  auto* fullscreen_layout_object =
      To<LayoutBox>(div_fullscreen->GetLayoutObject());
  EXPECT_EQ(viewport_width, fullscreen_layout_object->LogicalWidth().ToInt());
  EXPECT_EQ(viewport_height, fullscreen_layout_object->LogicalHeight().ToInt());

  // Verify it's updated after a device rotation.
  UpdateScreenInfoAndResizeView(&web_view_helper, viewport_height,
                                viewport_width);
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_EQ(viewport_height, fullscreen_layout_object->LogicalWidth().ToInt());
  EXPECT_EQ(viewport_width, fullscreen_layout_object->LogicalHeight().ToInt());
}

TEST_F(WebFrameTest, FullscreenLayerNonScrollable) {
  RegisterMockedHttpURLLoad("fullscreen_div.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  int viewport_width = 640;
  int viewport_height = 480;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "fullscreen_div.html", nullptr, nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  UpdateAllLifecyclePhases(web_view_impl);

  LocalFrame* frame = web_view_impl->MainFrameImpl()->GetFrame();
  Document* document = frame->GetDocument();
  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  Element* div_fullscreen = document->getElementById(AtomicString("div1"));
  Fullscreen::RequestFullscreen(*div_fullscreen);
  EXPECT_EQ(nullptr, Fullscreen::FullscreenElementFrom(*document));
  web_view_impl->DidEnterFullscreen();
  EXPECT_EQ(div_fullscreen, Fullscreen::FullscreenElementFrom(*document));
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_EQ(div_fullscreen, Fullscreen::FullscreenElementFrom(*document));

  // Verify that the viewports are nonscrollable.
  auto* frame_view = web_view_helper.LocalMainFrame()->GetFrameView();
  auto* layout_viewport_scroll_node =
      GetScrollNode(*frame_view->GetLayoutView());
  ASSERT_FALSE(layout_viewport_scroll_node->UserScrollableHorizontal());
  ASSERT_FALSE(layout_viewport_scroll_node->UserScrollableVertical());
  auto* visual_viewport_scroll_node =
      frame_view->GetPage()->GetVisualViewport().GetScrollNode();
  ASSERT_FALSE(visual_viewport_scroll_node->UserScrollableHorizontal());
  ASSERT_FALSE(visual_viewport_scroll_node->UserScrollableVertical());

  // Verify that the viewports are scrollable upon exiting fullscreen.
  EXPECT_EQ(div_fullscreen, Fullscreen::FullscreenElementFrom(*document));
  web_view_impl->DidExitFullscreen();
  EXPECT_EQ(nullptr, Fullscreen::FullscreenElementFrom(*document));
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_EQ(nullptr, Fullscreen::FullscreenElementFrom(*document));
  layout_viewport_scroll_node = GetScrollNode(*frame_view->GetLayoutView());
  ASSERT_TRUE(layout_viewport_scroll_node->UserScrollableHorizontal());
  ASSERT_TRUE(layout_viewport_scroll_node->UserScrollableVertical());
  visual_viewport_scroll_node =
      frame_view->GetPage()->GetVisualViewport().GetScrollNode();
  ASSERT_TRUE(visual_viewport_scroll_node->UserScrollableHorizontal());
  ASSERT_TRUE(visual_viewport_scroll_node->UserScrollableVertical());
}

TEST_F(WebFrameTest, FullscreenMainFrame) {
  RegisterMockedHttpURLLoad("fullscreen_div.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  int viewport_width = 640;
  int viewport_height = 480;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "fullscreen_div.html", nullptr, nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(viewport_width, viewport_height));
  UpdateAllLifecyclePhases(web_view_impl);

  auto* layout_view =
      web_view_impl->MainFrameImpl()->GetFrame()->View()->GetLayoutView();
  auto* scroll_node = GetScrollNode(*layout_view);
  ASSERT_TRUE(scroll_node->UserScrollableHorizontal());
  ASSERT_TRUE(scroll_node->UserScrollableVertical());

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
  scroll_node = GetScrollNode(*layout_view);
  ASSERT_TRUE(scroll_node->UserScrollableHorizontal());
  ASSERT_TRUE(scroll_node->UserScrollableVertical());

  // Verify the main frame still behaves correctly after a resize.
  web_view_helper.Resize(gfx::Size(viewport_height, viewport_width));
  scroll_node = GetScrollNode(*layout_view);
  ASSERT_TRUE(scroll_node->UserScrollableHorizontal());
  ASSERT_TRUE(scroll_node->UserScrollableVertical());
}

TEST_F(WebFrameTest, FullscreenSubframe) {
  RegisterMockedHttpURLLoad("fullscreen_iframe.html");
  RegisterMockedHttpURLLoad("fullscreen_div.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "fullscreen_iframe.html", nullptr, nullptr, ConfigureAndroid);
  int viewport_width = 640;
  int viewport_height = 480;
  UpdateScreenInfoAndResizeView(&web_view_helper, viewport_width,
                                viewport_height);
  UpdateAllLifecyclePhases(web_view_impl);

  LocalFrame* frame =
      To<WebLocalFrameImpl>(
          web_view_helper.GetWebView()->MainFrame()->FirstChild())
          ->GetFrame();
  Document* document = frame->GetDocument();
  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  Element* div_fullscreen = document->getElementById(AtomicString("div1"));
  Fullscreen::RequestFullscreen(*div_fullscreen);
  web_view_impl->DidEnterFullscreen();
  UpdateAllLifecyclePhases(web_view_impl);

  // Verify that the element is sized to the viewport.
  auto* fullscreen_layout_object =
      To<LayoutBox>(div_fullscreen->GetLayoutObject());
  EXPECT_EQ(viewport_width, fullscreen_layout_object->LogicalWidth().ToInt());
  EXPECT_EQ(viewport_height, fullscreen_layout_object->LogicalHeight().ToInt());

  // Verify it's updated after a device rotation.
  UpdateScreenInfoAndResizeView(&web_view_helper, viewport_height,
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

  auto* iframe =
      To<HTMLIFrameElement>(top_doc->QuerySelector(AtomicString("iframe")));
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
  top_doc->GetAgent().event_loop()->PerformMicrotaskCheckpoint();
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
  RegisterMockedHttpURLLoad("viewport-tiny.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-tiny.html", nullptr, nullptr, ConfigureAndroid);
  int viewport_width = 384;
  int viewport_height = 640;
  UpdateScreenInfoAndResizeView(&web_view_helper, viewport_width,
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
  RegisterMockedHttpURLLoad("viewport-tiny.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-tiny.html", nullptr, nullptr, ConfigureAndroid);
  int viewport_width = 384;
  int viewport_height = 640;
  UpdateScreenInfoAndResizeView(&web_view_helper, viewport_width,
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
  UpdateScreenInfoAndResizeView(&web_view_helper, viewport_width,
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
  gfx::Size screen_size_minus_status_bars_minus_url_bar(598, 303);
  gfx::Size screen_size_minus_status_bars(598, 359);
  gfx::Size screen_size(640, 384);

  RegisterMockedHttpURLLoad("fullscreen_restore_scale_factor.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "fullscreen_restore_scale_factor.html", nullptr, nullptr,
      &ConfigureAndroid);
  UpdateScreenInfoAndResizeView(
      &web_view_helper, screen_size_minus_status_bars_minus_url_bar.width(),
      screen_size_minus_status_bars_minus_url_bar.height());
  auto* layout_view = web_view_helper.GetWebView()
                          ->MainFrameImpl()
                          ->GetFrameView()
                          ->GetLayoutView();
  EXPECT_EQ(screen_size_minus_status_bars_minus_url_bar.width(),
            layout_view->LogicalWidth().Floor());
  EXPECT_EQ(screen_size_minus_status_bars_minus_url_bar.height(),
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
  UpdateScreenInfoAndResizeView(&web_view_helper,
                                screen_size_minus_status_bars.width(),
                                screen_size_minus_status_bars.height());
  UpdateScreenInfoAndResizeView(&web_view_helper, screen_size.width(),
                                screen_size.height());
  EXPECT_EQ(screen_size.width(), layout_view->LogicalWidth().Floor());
  EXPECT_EQ(screen_size.height(), layout_view->LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MaximumPageScaleFactor());

  web_view_impl->DidExitFullscreen();
  UpdateAllLifecyclePhases(web_view_impl);
  UpdateScreenInfoAndResizeView(&web_view_helper,
                                screen_size_minus_status_bars.width(),
                                screen_size_minus_status_bars.height());
  UpdateScreenInfoAndResizeView(
      &web_view_helper, screen_size_minus_status_bars_minus_url_bar.width(),
      screen_size_minus_status_bars_minus_url_bar.height());
  EXPECT_EQ(screen_size_minus_status_bars_minus_url_bar.width(),
            layout_view->LogicalWidth().Floor());
  EXPECT_EQ(screen_size_minus_status_bars_minus_url_bar.height(),
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
      base_url_ + "viewport-tiny.html", nullptr, nullptr, ConfigureAndroid);

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

TEST_F(WebFrameTest, WebXrImmersiveOverlay) {
  RegisterMockedHttpURLLoad("webxr_overlay.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "webxr_overlay.html", nullptr, nullptr);
  web_view_helper.Resize(gfx::Size(640, 480));

  // Ensure that the local frame view has a paint artifact compositor. It's
  // created lazily, and doing so after entering fullscreen would undo the
  // overlay layer modification.
  UpdateAllLifecyclePhases(web_view_impl);

  const cc::LayerTreeHost* layer_tree_host = web_view_helper.GetLayerTreeHost();

  LocalFrame* frame = web_view_impl->MainFrameImpl()->GetFrame();
  Document* document = frame->GetDocument();

  Element* overlay = document->getElementById(AtomicString("overlay"));
  EXPECT_FALSE(Fullscreen::IsFullscreenElement(*overlay));
  EXPECT_TRUE(layer_tree_host->background_color().isOpaque());

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
  EXPECT_EQ(1u, CcLayersByName(root_layer,
                               "Scrolling background of LayoutView #document")
                    .size());
  EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "other").size());
  // The overlay is not composited when it's not in full screen.
  EXPECT_EQ(0u, CcLayersByDOMElementId(root_layer, "overlay").size());
  EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "inner").size());

  web_view_impl->DidEnterFullscreen();
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_TRUE(Fullscreen::IsFullscreenElement(*overlay));
  EXPECT_TRUE(!layer_tree_host->background_color().isOpaque());

  root_layer = layer_tree_host->root_layer();
  EXPECT_EQ(0u, CcLayersByName(root_layer,
                               "Scrolling background of LayoutView #document")
                    .size());
  EXPECT_EQ(0u, CcLayersByDOMElementId(root_layer, "other").size());
  EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "overlay").size());
  EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "inner").size());

  web_view_impl->DidExitFullscreen();
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_FALSE(Fullscreen::IsFullscreenElement(*overlay));
  EXPECT_TRUE(layer_tree_host->background_color().isOpaque());
  document->SetIsXrOverlay(false, overlay);

  root_layer = layer_tree_host->root_layer();
  EXPECT_EQ(1u, CcLayersByName(root_layer,
                               "Scrolling background of LayoutView #document")
                    .size());
  EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "other").size());
  // The overlay is not composited when it's not in full screen.
  EXPECT_EQ(0u, CcLayersByDOMElementId(root_layer, "overlay").size());
  EXPECT_EQ(1u, CcLayersByDOMElementId(root_layer, "inner").size());
}

TEST_F(WebFrameTest, FullscreenFrameSet) {
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      "data:text/html,<frameset id=frameset></frameset>", nullptr, nullptr);
  web_view_helper.Resize(gfx::Size(640, 480));
  UpdateAllLifecyclePhases(web_view_impl);

  LocalFrame* frame = web_view_impl->MainFrameImpl()->GetFrame();
  Document* document = frame->GetDocument();
  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  Element* frameset = document->getElementById(AtomicString("frameset"));
  Fullscreen::RequestFullscreen(*frameset);
  EXPECT_EQ(nullptr, Fullscreen::FullscreenElementFrom(*document));
  web_view_impl->DidEnterFullscreen();
  EXPECT_EQ(frameset, Fullscreen::FullscreenElementFrom(*document));
  UpdateAllLifecyclePhases(web_view_impl);
  EXPECT_EQ(frameset, Fullscreen::FullscreenElementFrom(*document));

  // Verify that the element is in the top layer, attached to the LayoutView.
  EXPECT_TRUE(frameset->IsInTopLayer());
  auto* fullscreen_layout_object = To<LayoutBox>(frameset->GetLayoutObject());
  ASSERT_TRUE(fullscreen_layout_object);
  EXPECT_EQ(fullscreen_layout_object->Parent(), document->GetLayoutView());
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

static void NodeImageTestValidation(const gfx::Size& reference_bitmap_size,
                                    DragImage* drag_image) {
  // Prepare the reference bitmap.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(reference_bitmap_size.width(),
                        reference_bitmap_size.height());
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawColor(SK_ColorGREEN);

  EXPECT_EQ(reference_bitmap_size.width(), drag_image->Size().width());
  EXPECT_EQ(reference_bitmap_size.height(), drag_image->Size().height());
  const SkBitmap& drag_bitmap = drag_image->Bitmap();
  EXPECT_EQ(0, memcmp(bitmap.getPixels(), drag_bitmap.getPixels(),
                      bitmap.computeByteSize()));
}

TEST_F(WebFrameTest, NodeImageTestCSSTransformDescendant) {
  frame_test_helpers::WebViewHelper web_view_helper;
  std::unique_ptr<DragImage> drag_image = NodeImageTestSetup(
      &web_view_helper, std::string("case-css-3dtransform-descendant"));
  EXPECT_TRUE(drag_image);

  NodeImageTestValidation(gfx::Size(40, 40), drag_image.get());
}

TEST_F(WebFrameTest, NodeImageTestCSSTransform) {
  frame_test_helpers::WebViewHelper web_view_helper;
  std::unique_ptr<DragImage> drag_image =
      NodeImageTestSetup(&web_view_helper, std::string("case-css-transform"));
  EXPECT_TRUE(drag_image);

  NodeImageTestValidation(gfx::Size(40, 40), drag_image.get());
}

TEST_F(WebFrameTest, NodeImageTestCSS3DTransform) {
  frame_test_helpers::WebViewHelper web_view_helper;
  std::unique_ptr<DragImage> drag_image =
      NodeImageTestSetup(&web_view_helper, std::string("case-css-3dtransform"));
  EXPECT_TRUE(drag_image);

  NodeImageTestValidation(gfx::Size(40, 40), drag_image.get());
}

TEST_F(WebFrameTest, NodeImageTestInlineBlock) {
  frame_test_helpers::WebViewHelper web_view_helper;
  std::unique_ptr<DragImage> drag_image =
      NodeImageTestSetup(&web_view_helper, std::string("case-inlineblock"));
  EXPECT_TRUE(drag_image);

  NodeImageTestValidation(gfx::Size(40, 40), drag_image.get());
}

TEST_F(WebFrameTest, NodeImageTestFloatLeft) {
  frame_test_helpers::WebViewHelper web_view_helper;
  std::unique_ptr<DragImage> drag_image = NodeImageTestSetup(
      &web_view_helper, std::string("case-float-left-overflow-hidden"));
  EXPECT_TRUE(drag_image);

  NodeImageTestValidation(gfx::Size(40, 40), drag_image.get());
}

// Crashes on Android: http://crbug.com/403804
#if BUILDFLAG(IS_ANDROID)
TEST_F(WebFrameTest, DISABLED_PrintingBasic)
#else
TEST_F(WebFrameTest, PrintingBasic)
#endif
{
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("data:text/html,Hello, world.");

  WebLocalFrame* frame = web_view_helper.LocalMainFrame();

  WebPrintParams print_params((gfx::SizeF(500, 500)));

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
  void DidChangeThemeColor(std::optional<::SkColor> theme_color) override {
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
  EXPECT_EQ(SK_ColorBLUE, frame->GetDocument().ThemeColor());
  // Change color by rgb.
  host.Reset();
  frame->ExecuteScript(
      WebScriptSource("document.getElementById('tc1').setAttribute('content', "
                      "'rgb(0, 0, 0)');"));
  RunPendingTasks();
  EXPECT_TRUE(host.DidNotify());
  EXPECT_EQ(SK_ColorBLACK, frame->GetDocument().ThemeColor());
  // Change color by hsl.
  host.Reset();
  frame->ExecuteScript(
      WebScriptSource("document.getElementById('tc1').setAttribute('content', "
                      "'hsl(240,100%, 50%)');"));
  RunPendingTasks();
  EXPECT_TRUE(host.DidNotify());
  EXPECT_EQ(SK_ColorBLUE, frame->GetDocument().ThemeColor());
  // Change of second theme-color meta tag will not change frame's theme
  // color.
  host.Reset();
  frame->ExecuteScript(WebScriptSource(
      "document.getElementById('tc2').setAttribute('content', '#00FF00');"));
  RunPendingTasks();
  EXPECT_TRUE(host.DidNotify());
  EXPECT_EQ(SK_ColorBLUE, frame->GetDocument().ThemeColor());
  // Remove the first theme-color meta tag to apply the second.
  host.Reset();
  frame->ExecuteScript(
      WebScriptSource("document.getElementById('tc1').remove();"));
  RunPendingTasks();
  EXPECT_TRUE(host.DidNotify());
  EXPECT_EQ(SK_ColorGREEN, frame->GetDocument().ThemeColor());
  // Remove the name attribute of the remaining meta.
  host.Reset();
  frame->ExecuteScript(WebScriptSource(
      "document.getElementById('tc2').removeAttribute('name');"));
  RunPendingTasks();
  EXPECT_TRUE(host.DidNotify());
  EXPECT_EQ(std::nullopt, frame->GetDocument().ThemeColor());
}

// Make sure that an embedder-triggered detach with a remote frame parent
// doesn't leave behind dangling pointers.
TEST_F(WebFrameTest, EmbedderTriggeredDetachWithRemoteMainFrame) {
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote();
  WebLocalFrame* child_frame =
      helper.CreateLocalChild(*helper.RemoteMainFrame());

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
      mojom::blink::TreeScopeType scope,
      const WebString& name,
      const WebString& fallback_name,
      const FramePolicy&,
      const WebFrameOwnerProperties&,
      FrameOwnerElementType,
      WebPolicyContainerBindParams policy_container_bind_params,
      ukm::SourceId document_ukm_source_id,
      FinishChildFrameCreationFn finish_creation) override {
    return CreateLocalChild(
        *Frame(), scope, std::make_unique<WebFrameSwapTestClient>(this),
        std::move(policy_container_bind_params), finish_creation);
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
        const blink::FrameToken& child_frame_token,
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
    RegisterMockedHttpURLLoad("named-frame-a-b-c.html");
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
  WebFrameSwapTestClient main_frame_client_;

 protected:
  // This must be destroyed before `main_frame_client_`; when the WebViewHelper
  // is deleted, it destroys child views that were created, but the list of
  // child views is maintained on `main_frame_client_`.
  frame_test_helpers::WebViewHelper web_view_helper_;
};

TEST_F(WebFrameSwapTest, SwapMainFrame) {
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(MainFrame(), remote_frame);

  WebLocalFrame* local_frame =
      web_view_helper_.CreateProvisional(*remote_frame);

  // Committing a navigation in `local_frame` should swap it back in.
  frame_test_helpers::LoadFrame(local_frame, base_url_ + "subframe-hello.html");

  std::string content =
      TestWebFrameContentDumper::DumpWebViewAsText(WebView(), 1024).Utf8();
  EXPECT_EQ("hello", content);
}

class DidClearWindowObjectCounter
    : public frame_test_helpers::TestWebFrameClient {
 public:
  void DidClearWindowObject() override { ++count_; }

  int Count() const { return count_; }

 private:
  int count_ = 0;
};

TEST_F(WebFrameSwapTest, SwapMainFrameLocalToLocal) {
  // Start with a WebView with a local main frame.
  Frame* original_page_main_frame = WebFrame::ToCoreFrame(*MainFrame());
  EXPECT_EQ(original_page_main_frame,
            web_view_helper_.GetWebView()->GetPage()->MainFrame());

  // Set up a new WebView with a placeholder remote frame and a provisional
  // local frame, to do a local swap with the previous WebView.
  frame_test_helpers::WebViewHelper new_view_helper;
  new_view_helper.InitializePlaceholderRemote();
  DidClearWindowObjectCounter counter_client;
  WebLocalFrameImpl* provisional_frame = new_view_helper.CreateProvisional(
      *new_view_helper.RemoteMainFrame(), &counter_client);
  new_view_helper.GetWebView()->GetPage()->SetPreviousMainFrameForLocalSwap(
      DynamicTo<LocalFrame>(original_page_main_frame));
  EXPECT_NE(web_view_helper_.GetWebView(), new_view_helper.GetWebView());
  EXPECT_EQ(WebFrame::ToCoreFrame(*new_view_helper.RemoteMainFrame()),
            new_view_helper.GetWebView()->GetPage()->MainFrame());

  // Perform a cross-Page main frame swap. This should unload the previous
  // Page's main LocalFrame, replacing it with a placeholder RemoteFrame. After
  // that, the placeholder RemoteFrame in the new Page will be swapped out, and
  // the new main LocalFrame will be swapped in.
  auto params = std::make_unique<WebNavigationParams>();
  params->url = url_test_helpers::ToKURL("about:blank");
  // `CommitNavigation` will swap in the local frame to replace the remote
  // frame.
  provisional_frame->CommitNavigation(std::move(params), nullptr);

  // Android WebView's Java Object bridge is sensitive to exactly when and how
  // often `DidClearWindowObject()` is called. Though this is a fairly indirect
  // signal, it's still better than no signal at all.
  EXPECT_EQ(1, counter_client.Count());

  // Make sure the WindowProxy itself is not initialized, since the original
  // frame never ran script and was never scripted.
  LocalFrame* const frame = new_view_helper.LocalMainFrame()->GetFrame();
  v8::Isolate* const isolate = ToIsolate(frame);
  // Technically not needed for this test, but if something is broken, it fails
  // more gracefully with a HandleScope.
  v8::HandleScope scope(ToIsolate(frame));
  v8::Local<v8::Context> context =
      ToV8ContextMaybeEmpty(frame, DOMWrapperWorld::MainWorld(isolate));
  EXPECT_TRUE(context.IsEmpty());

  // The new WebView's main frame is now set to a new main LocalFrame.
  EXPECT_EQ(WebFrame::ToCoreFrame(*provisional_frame),
            new_view_helper.GetWebView()->GetPage()->MainFrame());

  // The old WebView's main frame is now a placeholder RemoteFrame that is not
  // detached.
  EXPECT_NE(original_page_main_frame,
            web_view_helper_.GetWebView()->GetPage()->MainFrame());
  EXPECT_TRUE(original_page_main_frame->IsDetached());
  EXPECT_TRUE(
      web_view_helper_.GetWebView()->GetPage()->MainFrame()->IsRemoteFrame());

  new_view_helper.Reset();
}

TEST_F(WebFrameSwapTest, DetachProvisionalLocalFrameAndPlaceholderRemoteFrame) {
  // Start with a WebView with a local main frame.
  Frame* original_page_main_frame = WebFrame::ToCoreFrame(*MainFrame());
  EXPECT_EQ(original_page_main_frame,
            web_view_helper_.GetWebView()->GetPage()->MainFrame());

  // Set up a new WebView with a placeholder remote frame and a provisional
  // local frame, that is set to do local swap with the previous WebView.
  frame_test_helpers::WebViewHelper new_view_helper;
  new_view_helper.InitializePlaceholderRemote();
  WebRemoteFrameImpl* remote_frame = new_view_helper.RemoteMainFrame();
  WebLocalFrameImpl* provisional_local_frame =
      new_view_helper.CreateProvisional(*remote_frame);
  new_view_helper.GetWebView()->GetPage()->SetPreviousMainFrameForLocalSwap(
      DynamicTo<LocalFrame>(original_page_main_frame));
  EXPECT_NE(web_view_helper_.GetWebView(), new_view_helper.GetWebView());
  EXPECT_EQ(WebFrame::ToCoreFrame(*remote_frame),
            new_view_helper.GetWebView()->GetPage()->MainFrame());

  // Detach the new WebView's provisional local main frame before any swapping
  // happens.
  provisional_local_frame->Detach();
  // The detachment should not affect the placeholder RemoteFrame, nor the
  // previous page.
  EXPECT_FALSE(
      WebFrame::ToCoreFrame(*new_view_helper.RemoteMainFrame())->IsDetached());
  EXPECT_FALSE(
      WebFrame::ToCoreFrame(*web_view_helper_.LocalMainFrame())->IsDetached());

  // Make sure that shutting down the new WebView does not affect the previous
  // WebView.
  new_view_helper.Reset();
  // The detachment should not affect the previous page too.
  EXPECT_FALSE(
      WebFrame::ToCoreFrame(*web_view_helper_.LocalMainFrame())->IsDetached());
}

TEST_F(WebFrameSwapTest, SwapMainFrameWithPageScaleReset) {
  WebView()->SetDefaultPageScaleLimits(1, 2);
  WebView()->SetPageScaleFactor(1.25);
  EXPECT_EQ(1.25, WebView()->PageScaleFactor());

  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(MainFrame(), remote_frame);

  mojo::AssociatedRemote<mojom::blink::RemoteMainFrameHost> main_frame_host;
  std::ignore = main_frame_host.BindNewEndpointAndPassDedicatedReceiver();
  WebView()->DidAttachRemoteMainFrame(
      main_frame_host.Unbind(),
      mojo::AssociatedRemote<mojom::blink::RemoteMainFrame>()
          .BindNewEndpointAndPassDedicatedReceiver());

  EXPECT_EQ(1.0, WebView()->PageScaleFactor());
}

TEST_F(WebFrameSwapTest, ValidateSizeOnRemoteToLocalMainFrameSwap) {
  gfx::Size size(111, 222);

  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(MainFrame(), remote_frame);

  To<WebViewImpl>(remote_frame->View())->Resize(size);

  WebLocalFrame* local_frame =
      web_view_helper_.CreateProvisional(*remote_frame);
  remote_frame->Swap(local_frame);

  // Verify that the size that was set with a remote main frame is correct
  // after swapping to a local frame.
  Page* page =
      To<WebViewImpl>(local_frame->View())->GetPage()->MainFrame()->GetPage();
  EXPECT_EQ(size.width(), page->GetVisualViewport().Size().width());
  EXPECT_EQ(size.height(), page->GetVisualViewport().Size().height());
}

// Verify that size changes to browser controls while the main frame is remote
// are preserved when the main frame swaps to a local frame.  See
// https://crbug.com/769321.
TEST_F(WebFrameSwapTest,
       ValidateBrowserControlsSizeOnRemoteToLocalMainFrameSwap) {
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(MainFrame(), remote_frame);

  // Create a provisional main frame frame but don't swap it in yet.
  WebLocalFrame* local_frame =
      web_view_helper_.CreateProvisional(*remote_frame);

  WebViewImpl* web_view = To<WebViewImpl>(local_frame->View());
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
  Page* page =
      To<WebViewImpl>(local_frame->View())->GetPage()->MainFrame()->GetPage();
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

    if (!Frame()->Parent()) {
      frame_test_helpers::SwapRemoteFrame(Frame(),
                                          frame_test_helpers::CreateRemote());
    }
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

TEST_F(WebFrameTest, SwapChildAddFrameInUnload) {
  frame_test_helpers::WebViewHelper web_view_helper;

  // This sets up a main frame with one child frame. When the document in the
  // child frame is unloaded (e.g. due to the `Frame::Swap()` call below), the
  // unload handler will insert a new <iframe> into the main frame's document.
  RegisterMockedHttpURLLoad("add-frame-in-unload-main.html");
  RegisterMockedHttpURLLoad("add-frame-in-unload-subframe.html");
  web_view_helper.InitializeAndLoad(base_url_ +
                                    "add-frame-in-unload-main.html");

  WebLocalFrame* new_frame = web_view_helper.CreateProvisional(
      *web_view_helper.LocalMainFrame()->FirstChild());

  // This triggers the unload handler in the child frame's Document, mutating
  // the frame tree during the `Frame::Swap()` call.
  web_view_helper.LocalMainFrame()->FirstChild()->Swap(new_frame);

  // TODO(dcheng): This is currently required to trigger a crash when the bug is
  // not fixed. Removing a frame from the frame tree will fail one of the
  // consistency checks in `Frame::RemoveChild()` if the frame tree is
  // corrupted.  This should be replaced with a test helper that comprehensively
  // validates that a frame tree is not corrupted: this helper could also be
  // used to simplify the various SwapAndVerify* helpers below.
  web_view_helper.LocalMainFrame()->ExecuteScript(
      WebScriptSource("document.querySelector('iframe').remove()"));
}

void WebFrameTest::SwapAndVerifyFirstChildConsistency(const char* const message,
                                                      WebFrame* parent,
                                                      WebFrame* new_child) {
  SCOPED_TRACE(message);
  if (new_child->IsWebLocalFrame()) {
    parent->FirstChild()->Swap(new_child->ToWebLocalFrame());
  } else {
    frame_test_helpers::SwapRemoteFrame(parent->FirstChild(),
                                        new_child->ToWebRemoteFrame());
  }

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
      web_view_helper_.CreateProvisional(*remote_frame);
  SwapAndVerifyFirstChildConsistency("remote->local", MainFrame(), local_frame);

  // FIXME: This almost certainly fires more load events on the iframe element
  // than it should.
  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  frame_test_helpers::LoadFrame(local_frame, base_url_ + "subframe-hello.html");
  std::string content =
      TestWebFrameContentDumper::DumpWebViewAsText(WebView(), 1024).Utf8();
  EXPECT_EQ("  \n\nhello\n\nb \n\na\n\nc", content);
}

// Asserts that the `Settings::SetHighlightAds` is properly applied to a
// `LocalFrame` even if `Settings::SetHighlightAds` is fired when the
// `LocalFrame` is still provisional. See crbug/1312107. While the bug is first
// observed on fenced frames, the underlying issue lies in the timing of the
// `Settings::SetHighlightAds` call with respect to the navigation progress of
// the frame.
TEST_F(WebFrameSwapTest, AdHighlightEarlyApply) {
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  SwapAndVerifyFirstChildConsistency("local->remote", MainFrame(),
                                     remote_frame);

  // Create the provisional frame and set its ad evidence.
  WebLocalFrameImpl* local_frame =
      web_view_helper_.CreateProvisional(*remote_frame);
  // Value of `parent_is_ad` does not matter.
  blink::FrameAdEvidence ad_evidence(/*parent_is_ad=*/false);
  ad_evidence.set_created_by_ad_script(
      mojom::FrameCreationStackEvidence::kCreatedByAdScript);
  ad_evidence.set_is_complete();
  local_frame->SetAdEvidence(ad_evidence);

  // Toggle the settings for provisional local frame.
  local_frame->View()->GetSettings()->SetHighlightAds(true);

  // Assert that the local frame does not have any overlay color since it is not
  // in the frame tree yet.
  ASSERT_EQ(local_frame->GetFrame()->GetFrameOverlayColorForTesting(),
            std::nullopt);

  WebDocument doc_before_navigation = local_frame->GetDocument();

  auto params = std::make_unique<WebNavigationParams>();
  params->url = url_test_helpers::ToKURL("about:blank");
  // `CommitNavigation` will swap in the local frame to replace the remote
  // frame.
  local_frame->CommitNavigation(std::move(params), nullptr);

  ASSERT_FALSE(local_frame->IsProvisional());
  ASSERT_NE(doc_before_navigation, local_frame->GetDocument());
  ASSERT_EQ(local_frame->GetFrame()->GetFrameOverlayColorForTesting(),
            SkColorSetARGB(128, 255, 0, 0));
}

// TODO(crbug.com/1314493): This test is flaky with the TimedHTMLParserBudget
// feature enabled.
TEST_F(WebFrameSwapTest, DISABLED_DoNotPropagateDisplayNonePropertyOnSwap) {
  WebFrameSwapTestClient* main_frame_client =
      static_cast<WebFrameSwapTestClient*>(MainFrame()->Client());
  EXPECT_FALSE(main_frame_client->DidPropagateDisplayNoneProperty());

  WebLocalFrame* child_frame = MainFrame()->FirstChild()->ToWebLocalFrame();
  frame_test_helpers::LoadFrame(child_frame, "subframe-hello.html");
  EXPECT_FALSE(main_frame_client->DidPropagateDisplayNoneProperty());

  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(child_frame, remote_frame);
  EXPECT_FALSE(main_frame_client->DidPropagateDisplayNoneProperty());

  WebLocalFrame* local_frame =
      web_view_helper_.CreateProvisional(*remote_frame);
  remote_frame->Swap(local_frame);
  EXPECT_FALSE(main_frame_client->DidPropagateDisplayNoneProperty());
  Reset();
}

void WebFrameTest::SwapAndVerifyMiddleChildConsistency(
    const char* const message,
    WebFrame* parent,
    WebFrame* new_child) {
  SCOPED_TRACE(message);

  if (new_child->IsWebLocalFrame()) {
    parent->FirstChild()->NextSibling()->Swap(new_child->ToWebLocalFrame());
  } else {
    frame_test_helpers::SwapRemoteFrame(parent->FirstChild()->NextSibling(),
                                        new_child->ToWebRemoteFrame());
  }

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
      web_view_helper_.CreateProvisional(*remote_frame);
  SwapAndVerifyMiddleChildConsistency("remote->local", MainFrame(),
                                      local_frame);

  // FIXME: This almost certainly fires more load events on the iframe element
  // than it should.
  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  frame_test_helpers::LoadFrame(local_frame, base_url_ + "subframe-hello.html");
  std::string content =
      TestWebFrameContentDumper::DumpWebViewAsText(WebView(), 1024).Utf8();
  EXPECT_EQ("  \n\na\n\nhello\n\nc", content);
}

void WebFrameTest::SwapAndVerifyLastChildConsistency(const char* const message,
                                                     WebFrame* parent,
                                                     WebFrame* new_child) {
  SCOPED_TRACE(message);
  if (new_child->IsWebLocalFrame()) {
    parent->LastChild()->Swap(new_child->ToWebLocalFrame());
  } else {
    frame_test_helpers::SwapRemoteFrame(parent->LastChild(),
                                        new_child->ToWebRemoteFrame());
  }

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
      web_view_helper_.CreateProvisional(*remote_frame);
  SwapAndVerifyLastChildConsistency("remote->local", MainFrame(), local_frame);

  // FIXME: This almost certainly fires more load events on the iframe element
  // than it should.
  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  frame_test_helpers::LoadFrame(local_frame, base_url_ + "subframe-hello.html");
  std::string content =
      TestWebFrameContentDumper::DumpWebViewAsText(WebView(), 1024).Utf8();
  EXPECT_EQ("  \n\na\n\nb \n\na\n\nhello", content);
}

TEST_F(WebFrameSwapTest, DetachProvisionalFrame) {
  WebRemoteFrameImpl* remote_frame = frame_test_helpers::CreateRemote();
  SwapAndVerifyMiddleChildConsistency("local->remote", MainFrame(),
                                      remote_frame);

  WebLocalFrameImpl* provisional_frame =
      web_view_helper_.CreateProvisional(*remote_frame);

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

  if (new_frame->IsWebLocalFrame()) {
    old_frame->Swap(new_frame->ToWebLocalFrame());
  } else {
    frame_test_helpers::SwapRemoteFrame(old_frame,
                                        new_frame->ToWebRemoteFrame());
  }

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

  WebLocalFrameImpl* local_child =
      web_view_helper_.CreateLocalChild(*remote_frame, "local-inside-remote");

  LocalFrame* main_frame = WebView()->MainFrameImpl()->GetFrame();
  Document* child_document = local_child->GetFrame()->GetDocument();
  EventHandlerRegistry& event_registry =
      local_child->GetFrame()->GetEventHandlerRegistry();

  // Add the non-connected, but local, child document as having an event.
  event_registry.DidAddEventHandler(
      *child_document, EventHandlerRegistry::kTouchStartOrMoveEventBlocking);
  // Passes if this does not crash or DCHECK.
  main_frame->View()->UpdateAllLifecyclePhasesForTest();
}

TEST_F(WebFrameSwapTest, EventsOnDisconnectedElementSkipped) {
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  WebFrame* target_frame = MainFrame()->FirstChild()->NextSibling();
  EXPECT_TRUE(target_frame);
  SwapAndVerifySubframeConsistency("local->remote", target_frame, remote_frame);
  remote_frame->SetReplicatedOrigin(
      WebSecurityOrigin(SecurityOrigin::CreateUniqueOpaque()), false);

  WebLocalFrameImpl* local_child =
      web_view_helper_.CreateLocalChild(*remote_frame, "local-inside-remote");

  LocalFrame* main_frame = WebView()->MainFrameImpl()->GetFrame();

  // Layout ensures that elements in the local_child frame get LayoutObjects
  // attached, but doesn't paint, because the child frame needs to not have
  // been composited for the purpose of this test.
  local_child->GetFrameView()->UpdateStyleAndLayout();
  Document* child_document = local_child->GetFrame()->GetDocument();
  EventHandlerRegistry& event_registry =
      local_child->GetFrame()->GetEventHandlerRegistry();

  // Add the non-connected body element as having an event.
  event_registry.DidAddEventHandler(
      *child_document->body(),
      EventHandlerRegistry::kTouchStartOrMoveEventBlocking);
  // Passes if this does not crash or DCHECK.
  main_frame->View()->UpdateAllLifecyclePhasesForTest();
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
      web_view_helper_.CreateProvisional(*remote_frame);
  SwapAndVerifySubframeConsistency("remote->local", target_frame, local_frame);

  // FIXME: This almost certainly fires more load events on the iframe element
  // than it should.
  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  frame_test_helpers::LoadFrame(local_frame, base_url_ + "subframe-hello.html");
  std::string content =
      TestWebFrameContentDumper::DumpWebViewAsText(WebView(), 1024).Utf8();
  EXPECT_EQ("  \n\na\n\nhello\n\nc", content);
}

TEST_F(WebFrameSwapTest, SwapPreservesGlobalContext) {
  v8::HandleScope scope(web_view_helper_.GetAgentGroupScheduler().Isolate());
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
  frame_test_helpers::SwapRemoteFrame(target_frame, remote_frame);
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
      web_view_helper_.CreateProvisional(*remote_frame);
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
  v8::Isolate* isolate = web_view_helper_.GetAgentGroupScheduler().Isolate();
  v8::HandleScope scope(isolate);
  MainFrame()->ExecuteScript(
      WebScriptSource("savedSetTimeout = window[0].setTimeout"));

  // Swap the frame to a remote frame.
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  WebFrame* target_frame = MainFrame()->FirstChild();
  frame_test_helpers::SwapRemoteFrame(target_frame, remote_frame);
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
        ToCoreString(isolate,
                     exception
                         ->ToString(ToScriptStateForMainWorld(
                                        WebView()->MainFrameImpl()->GetFrame())
                                        ->GetContext())
                         .ToLocalChecked()));
  }
}

TEST_F(WebFrameSwapTest, SwapInitializesGlobal) {
  v8::HandleScope scope(web_view_helper_.GetAgentGroupScheduler().Isolate());

  v8::Local<v8::Value> window_top =
      MainFrame()->ExecuteScriptAndReturnValue(WebScriptSource("window"));
  ASSERT_TRUE(window_top->IsObject());

  v8::Local<v8::Value> last_child = MainFrame()->ExecuteScriptAndReturnValue(
      WebScriptSource("saved = window[2]"));
  ASSERT_TRUE(last_child->IsObject());

  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(MainFrame()->LastChild(), remote_frame);
  v8::Local<v8::Value> remote_window_top =
      MainFrame()->ExecuteScriptAndReturnValue(WebScriptSource("saved.top"));
  EXPECT_TRUE(remote_window_top->IsObject());
  EXPECT_TRUE(window_top->StrictEquals(remote_window_top));

  WebLocalFrame* local_frame =
      web_view_helper_.CreateProvisional(*remote_frame);
  // Committing a navigation in a provisional frame will swap it in.
  frame_test_helpers::LoadFrame(local_frame, "data:text/html,");
  v8::Local<v8::Value> local_window_top =
      MainFrame()->ExecuteScriptAndReturnValue(WebScriptSource("saved.top"));
  EXPECT_TRUE(local_window_top->IsObject());
  EXPECT_TRUE(window_top->StrictEquals(local_window_top));
  local_frame->ExecuteScriptAndReturnValue(WebScriptSource("42"));
}

TEST_F(WebFrameSwapTest, RemoteFramesAreIndexable) {
  v8::HandleScope scope(web_view_helper_.GetAgentGroupScheduler().Isolate());

  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(MainFrame()->LastChild(), remote_frame);
  v8::Local<v8::Value> remote_window =
      MainFrame()->ExecuteScriptAndReturnValue(WebScriptSource("window[2]"));
  EXPECT_TRUE(remote_window->IsObject());
  v8::Local<v8::Value> window_length = MainFrame()->ExecuteScriptAndReturnValue(
      WebScriptSource("window.length"));
  ASSERT_TRUE(window_length->IsInt32());
  EXPECT_EQ(3, window_length.As<v8::Int32>()->Value());
}

TEST_F(WebFrameSwapTest, RemoteFrameLengthAccess) {
  v8::HandleScope scope(web_view_helper_.GetAgentGroupScheduler().Isolate());

  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(MainFrame()->LastChild(), remote_frame);
  v8::Local<v8::Value> remote_window_length =
      MainFrame()->ExecuteScriptAndReturnValue(
          WebScriptSource("window[2].length"));
  ASSERT_TRUE(remote_window_length->IsInt32());
  EXPECT_EQ(0, remote_window_length.As<v8::Int32>()->Value());
}

TEST_F(WebFrameSwapTest, RemoteWindowNamedAccess) {
  v8::HandleScope scope(web_view_helper_.GetAgentGroupScheduler().Isolate());

  // TODO(dcheng): Once OOPIF unit test infrastructure is in place, test that
  // named window access on a remote window works. For now, just test that
  // accessing a named property doesn't crash.
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(MainFrame()->LastChild(), remote_frame);
  remote_frame->SetReplicatedOrigin(
      WebSecurityOrigin(SecurityOrigin::CreateUniqueOpaque()), false);
  v8::Local<v8::Value> remote_window_property =
      MainFrame()->ExecuteScriptAndReturnValue(
          WebScriptSource("window[2].foo"));
  EXPECT_TRUE(remote_window_property.IsEmpty());
}

TEST_F(WebFrameSwapTest, RemoteWindowToString) {
  v8::Isolate* isolate = web_view_helper_.GetAgentGroupScheduler().Isolate();
  v8::HandleScope scope(isolate);

  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(MainFrame()->LastChild(), remote_frame);
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
  v8::HandleScope scope(web_view_helper_.GetAgentGroupScheduler().Isolate());

  WebRemoteFrame* remote_parent_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(MainFrame(), remote_parent_frame);
  remote_parent_frame->SetReplicatedOrigin(
      WebSecurityOrigin(SecurityOrigin::CreateUniqueOpaque()), false);

  WebLocalFrame* child_frame =
      web_view_helper_.CreateLocalChild(*remote_parent_frame);
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
  v8::HandleScope scope(web_view_helper_.GetAgentGroupScheduler().Isolate());

  WebRemoteFrame* remote_parent_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(MainFrame(), remote_parent_frame);
  remote_parent_frame->SetReplicatedOrigin(
      WebSecurityOrigin(SecurityOrigin::CreateUniqueOpaque()), false);

  WebLocalFrame* child_frame =
      web_view_helper_.CreateLocalChild(*remote_parent_frame);
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
  ~RemoteToLocalSwapWebFrameClient() override = default;

  // frame_test_helpers::TestWebFrameClient:
  void DidCommitNavigation(
      WebHistoryCommitType history_commit_type,
      bool should_reset_browser_interface_broker,
      const ParsedPermissionsPolicy& permissions_policy_header,
      const DocumentPolicyFeatureState& document_policy_header) override {
    history_commit_type_ = history_commit_type;
  }

  WebHistoryCommitType HistoryCommitType() const {
    return *history_commit_type_;
  }

  std::optional<WebHistoryCommitType> history_commit_type_;
};

// The commit type should be Standard if we are swapping a RemoteFrame to a
// LocalFrame after commits have already happened in the frame.  The browser
// process will inform us via setCommittedFirstRealLoad.
TEST_F(WebFrameSwapTest, HistoryCommitTypeAfterExistingRemoteToLocalSwap) {
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  WebFrame* target_frame = MainFrame()->FirstChild();
  ASSERT_TRUE(target_frame);
  frame_test_helpers::SwapRemoteFrame(target_frame, remote_frame);
  ASSERT_TRUE(MainFrame()->FirstChild());
  ASSERT_EQ(MainFrame()->FirstChild(), remote_frame);

  RemoteToLocalSwapWebFrameClient client;
  WebLocalFrameImpl* local_frame =
      web_view_helper_.CreateProvisional(*remote_frame, &client);
  local_frame->SetIsNotOnInitialEmptyDocument();
  frame_test_helpers::LoadFrame(local_frame, base_url_ + "subframe-hello.html");
  EXPECT_EQ(kWebStandardCommit, client.HistoryCommitType());

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  Reset();
}

class RemoteFrameHostInterceptor : public FakeRemoteFrameHost {
 public:
  RemoteFrameHostInterceptor() = default;
  ~RemoteFrameHostInterceptor() override = default;

  // FakeRemoteFrameHost:
  void OpenURL(mojom::blink::OpenURLParamsPtr params) override {
    intercepted_params_ = std::move(params);
  }

  const mojom::blink::OpenURLParamsPtr& GetInterceptedParams() {
    return intercepted_params_;
  }

 private:
  mojom::blink::OpenURLParamsPtr intercepted_params_;
};

TEST_F(WebFrameSwapTest, NavigateRemoteFrameViaLocation) {
  RemoteFrameHostInterceptor interceptor;
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  WebFrame* target_frame = MainFrame()->FirstChild();
  ASSERT_TRUE(target_frame);
  frame_test_helpers::SwapRemoteFrame(target_frame, remote_frame,
                                      interceptor.BindNewAssociatedRemote());
  ASSERT_TRUE(MainFrame()->FirstChild());
  ASSERT_EQ(MainFrame()->FirstChild(), remote_frame);

  remote_frame->SetReplicatedOrigin(
      WebSecurityOrigin::CreateFromString("http://127.0.0.1"), false);
  MainFrame()->ExecuteScript(
      WebScriptSource("document.getElementsByTagName('iframe')[0]."
                      "contentWindow.location = 'data:text/html,hi'"));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(interceptor.GetInterceptedParams());
  EXPECT_EQ(ToKURL("data:text/html,hi"),
            KURL(interceptor.GetInterceptedParams()->url));

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  Reset();
}

TEST_F(WebFrameSwapTest, WindowOpenOnRemoteFrame) {
  // This test needs explicitly named iframes due to the open() call below.
  frame_test_helpers::LoadFrame(MainFrame(),
                                base_url_ + "named-frame-a-b-c.html");

  RemoteFrameHostInterceptor interceptor;
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(MainFrame()->FirstChild(), remote_frame,
                                      interceptor.BindNewAssociatedRemote());
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
  main_window->open(script_state->GetIsolate(), destination,
                    AtomicString("frame1"), "", exception_state);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(interceptor.GetInterceptedParams());
  EXPECT_EQ(KURL(interceptor.GetInterceptedParams()->url), KURL(destination));

  // Pointing a named frame to an empty URL should just return a reference to
  // the frame's window without navigating it.
  DOMWindow* result =
      main_window->open(script_state->GetIsolate(), "", AtomicString("frame1"),
                        "", exception_state);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(interceptor.GetInterceptedParams());
  EXPECT_EQ(KURL(interceptor.GetInterceptedParams()->url), KURL(destination));
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
  RemoteWindowCloseTest() = default;
  ~RemoteWindowCloseTest() override = default;

  bool Closed() const { return remote_main_frame_host_.remote_window_closed(); }

  TestRemoteMainFrameHostForWindowClose* remote_main_frame_host() {
    return &remote_main_frame_host_;
  }

 private:
  TestRemoteMainFrameHostForWindowClose remote_main_frame_host_;
};

TEST_F(RemoteWindowCloseTest, WindowOpenRemoteClose) {
  frame_test_helpers::WebViewHelper main_web_view;
  main_web_view.Initialize();

  // Create a remote window that will be closed later in the test.
  frame_test_helpers::WebViewHelper popup;
  popup.InitializeRemote(nullptr, nullptr);
  popup.GetWebView()->DidAttachRemoteMainFrame(
      remote_main_frame_host()->BindNewAssociatedRemote(),
      mojo::AssociatedRemote<mojom::blink::RemoteMainFrame>()
          .BindNewEndpointAndPassDedicatedReceiver());

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

// Tests that calling window.close() when detaching document as a result of
// closing the WebView shouldn't crash. This is a regression test for
// https://crbug.com/5058796.
TEST_F(WebFrameTest, WindowCloseOnDetach) {
  // Open a page that calls window.close() from its pagehide handler.
  RegisterMockedHttpURLLoad("close-on-pagehide.html");
  frame_test_helpers::WebViewHelper main_web_view;
  main_web_view.InitializeAndLoad(base_url_ + "close-on-pagehide.html");

  // Mark the Page as opened by DOM so that window.close() will work.
  LocalFrame* local_frame = main_web_view.LocalMainFrame()->GetFrame();
  local_frame->GetPage()->SetOpenedByDOM();

  // Reset the WebView, which will detach the document, triggering the pagehide
  // handler, eventually calling window.close().
  main_web_view.Reset();

  // window.close() should synchronously mark the page as closed.
  EXPECT_TRUE(local_frame->DomWindow()->closed());

  // We used to still post a task to close the WebView even after the WebView is
  // reset, causing a crash when the task runs. Now we won't post the task, and
  // the crash should not happen. Verify that we won't crash if we run pending
  // tasks.
  RunPendingTasks();
}

TEST_F(WebFrameTest, NavigateRemoteToLocalWithOpener) {
  frame_test_helpers::WebViewHelper main_web_view;
  main_web_view.Initialize();
  WebLocalFrame* main_frame = main_web_view.LocalMainFrame();

  // Create a popup with a remote frame and set its opener to the main frame.
  frame_test_helpers::WebViewHelper popup_helper;
  popup_helper.InitializeRemoteWithOpener(
      main_frame, SecurityOrigin::CreateFromString("http://foo.com"));
  WebRemoteFrame* popup_remote_frame = popup_helper.RemoteMainFrame();
  EXPECT_FALSE(main_frame->GetSecurityOrigin().CanAccess(
      popup_remote_frame->GetSecurityOrigin()));

  // Do a remote-to-local swap in the popup.
  WebLocalFrame* popup_local_frame =
      popup_helper.CreateProvisional(*popup_remote_frame);
  popup_remote_frame->Swap(popup_local_frame);

  // The initial document created in a provisional frame should not be
  // scriptable by any other frame.
  EXPECT_FALSE(main_frame->GetSecurityOrigin().CanAccess(
      popup_helper.LocalMainFrame()->GetSecurityOrigin()));
  EXPECT_TRUE(popup_helper.LocalMainFrame()->GetSecurityOrigin().IsOpaque());
}

TEST_F(WebFrameTest, SwapWithOpenerCycle) {
  // First, create a remote main frame with itself as the opener.
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote();
  WebRemoteFrame* remote_frame = helper.RemoteMainFrame();
  WebFrame::ToCoreFrame(*helper.RemoteMainFrame())
      ->SetOpenerDoNotNotify(WebFrame::ToCoreFrame(*remote_frame));

  // Now swap in a local frame. It shouldn't crash.
  WebLocalFrame* local_frame = helper.CreateProvisional(*remote_frame);
  remote_frame->Swap(local_frame);

  // And the opener cycle should still be preserved.
  EXPECT_EQ(local_frame, local_frame->Opener());
}

class CommitTypeWebFrameClient final
    : public frame_test_helpers::TestWebFrameClient {
 public:
  CommitTypeWebFrameClient() = default;
  ~CommitTypeWebFrameClient() override = default;

  WebHistoryCommitType HistoryCommitType() const {
    return history_commit_type_;
  }

  // frame_test_helpers::TestWebFrameClient:
  void DidCommitNavigation(
      WebHistoryCommitType history_commit_type,
      bool should_reset_browser_interface_broker,
      const ParsedPermissionsPolicy& permissions_policy_header,
      const DocumentPolicyFeatureState& document_policy_header) final {
    history_commit_type_ = history_commit_type;
  }

 private:
  WebHistoryCommitType history_commit_type_ = kWebHistoryInertCommit;
};

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
  EXPECT_TRUE(web_frame_client.messages.empty());
  ASSERT_EQ(1u, popup_web_frame_client.messages.size());
  EXPECT_TRUE(std::string::npos !=
              popup_web_frame_client.messages[0].text.Utf8().find(
                  "Unsafe attempt to initiate navigation"));

  // Try setting a cross-origin iframe element's source to a javascript: URL,
  // and check that this error is also printed on the calling window.
  popup_view->MainFrameImpl()->ExecuteScript(
      WebScriptSource("opener.document.querySelectorAll('iframe')[1].src='"
                      "javascript:alert()'"));
  EXPECT_TRUE(web_frame_client.messages.empty());
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
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "device_media_queries.html",
                                    nullptr, nullptr, ConfigureAndroid);
  auto* frame =
      To<LocalFrame>(web_view_helper.GetWebView()->GetPage()->MainFrame());
  Element* element = frame->GetDocument()->getElementById(AtomicString("test"));
  ASSERT_TRUE(element);

  display::ScreenInfo screen_info =
      web_view_helper.GetMainFrameWidget()->GetOriginalScreenInfo();
  screen_info.rect = screen_info.available_rect = gfx::Rect(700, 500);
  UpdateScreenInfoAndResizeView(&web_view_helper, screen_info);
  EXPECT_EQ(300, element->OffsetWidth());
  EXPECT_EQ(300, element->OffsetHeight());

  screen_info.rect = screen_info.available_rect = gfx::Rect(710, 500);
  UpdateScreenInfoAndResizeView(&web_view_helper, screen_info);
  EXPECT_EQ(400, element->OffsetWidth());
  EXPECT_EQ(300, element->OffsetHeight());

  screen_info.rect = screen_info.available_rect = gfx::Rect(690, 500);
  UpdateScreenInfoAndResizeView(&web_view_helper, screen_info);
  EXPECT_EQ(200, element->OffsetWidth());
  EXPECT_EQ(300, element->OffsetHeight());

  screen_info.rect = screen_info.available_rect = gfx::Rect(700, 510);
  UpdateScreenInfoAndResizeView(&web_view_helper, screen_info);
  EXPECT_EQ(300, element->OffsetWidth());
  EXPECT_EQ(400, element->OffsetHeight());

  screen_info.rect = screen_info.available_rect = gfx::Rect(700, 490);
  UpdateScreenInfoAndResizeView(&web_view_helper, screen_info);
  EXPECT_EQ(300, element->OffsetWidth());
  EXPECT_EQ(200, element->OffsetHeight());

  screen_info.rect = screen_info.available_rect = gfx::Rect(690, 510);
  UpdateScreenInfoAndResizeView(&web_view_helper, screen_info);
  EXPECT_EQ(200, element->OffsetWidth());
  EXPECT_EQ(400, element->OffsetHeight());
}

class DeviceEmulationTest : public WebFrameTest {
 protected:
  DeviceEmulationTest() {
    RegisterMockedHttpURLLoad("device_emulation.html");
    web_view_helper_.InitializeAndLoad(base_url_ + "device_emulation.html",
                                       nullptr, nullptr);
  }

  void TestResize(const gfx::Size& size, const String& expected_size) {
    display::ScreenInfo screen_info =
        web_view_helper_.GetMainFrameWidget()->GetOriginalScreenInfo();
    screen_info.rect = screen_info.available_rect = gfx::Rect(size);
    UpdateScreenInfoAndResizeView(&web_view_helper_, screen_info);
    EXPECT_EQ(expected_size, DumpSize("test"));
  }

  String DumpSize(const String& id) {
    String code = "dumpSize('" + id + "')";
    v8::HandleScope scope(web_view_helper_.GetAgentGroupScheduler().Isolate());
    ScriptExecutionCallbackHelper callback_helper;
    ExecuteScriptInMainWorld(web_view_helper_.GetWebView()->MainFrameImpl(),
                             code, callback_helper.Callback());
    RunPendingTasks();
    EXPECT_TRUE(callback_helper.DidComplete());
    return callback_helper.SingleStringValue();
  }

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

  WebLocalFrame* second_frame(helper.CreateLocalChild(*parent, "name2"));
  WebLocalFrame* fourth_frame(helper.CreateLocalChild(
      *parent, "name4", WebFrameOwnerProperties(), second_frame));
  WebLocalFrame* third_frame(helper.CreateLocalChild(
      *parent, "name3", WebFrameOwnerProperties(), second_frame));
  WebLocalFrame* first_frame(helper.CreateLocalChild(*parent, "name1"));

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
      helper.CreateLocalChild(*helper.RemoteMainFrame());

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
  helper.InitializeRemote(SecurityOrigin::Create(ToKURL(not_base_url_)));

  WebLocalFrame* local_frame =
      helper.CreateLocalChild(*helper.RemoteMainFrame());

  RegisterMockedHttpURLLoad("foo.html");
  frame_test_helpers::LoadFrame(local_frame, base_url_ + "foo.html");
  EXPECT_TRUE(local_frame->GetDocument().SiteForCookies().IsNull());

#if DCHECK_IS_ON()
  // TODO(crbug.com/1329535): Remove if threaded preload scanner doesn't launch.
  // This is needed because the preload scanner creates a thread when loading a
  // page.
  WTF::SetIsBeforeThreadCreatedForTest();
#endif
  SchemeRegistry::RegisterURLSchemeAsFirstPartyWhenTopLevel("http");
  EXPECT_TRUE(net::SiteForCookies::FromUrl(GURL(not_base_url_))
                  .IsEquivalent(local_frame->GetDocument().SiteForCookies()));
  SchemeRegistry::RemoveURLSchemeAsFirstPartyWhenTopLevel("http");
}

// See https://crbug.com/525285.
TEST_F(WebFrameTest, RemoteToLocalSwapOnMainFrameInitializesCoreFrame) {
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote();

  helper.CreateLocalChild(*helper.RemoteMainFrame());

  // Do a remote-to-local swap of the top frame.
  WebLocalFrame* local_root =
      helper.CreateProvisional(*helper.RemoteMainFrame());
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
  WebLocalFrameImpl* web_local_child = helper.CreateLocalChild(*remote_root);
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

class WebFrameOverscrollTest
    : public WebFrameTest,
      public testing::WithParamInterface<WebGestureDevice> {
 public:
  WebFrameOverscrollTest() {}

 protected:
  WebGestureEvent GenerateEvent(WebInputEvent::Type type,
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
    return event;
  }

  void ScrollBegin(frame_test_helpers::WebViewHelper* web_view_helper,
                   float delta_x_hint,
                   float delta_y_hint) {
    web_view_helper->GetMainFrameWidget()->DispatchThroughCcInputHandler(
        GenerateEvent(WebInputEvent::Type::kGestureScrollBegin, delta_x_hint,
                      delta_y_hint));
  }

  void ScrollUpdate(frame_test_helpers::WebViewHelper* web_view_helper,
                    float delta_x,
                    float delta_y) {
    web_view_helper->GetMainFrameWidget()->DispatchThroughCcInputHandler(
        GenerateEvent(WebInputEvent::Type::kGestureScrollUpdate, delta_x,
                      delta_y));
  }

  void ScrollEnd(frame_test_helpers::WebViewHelper* web_view_helper) {
    web_view_helper->GetMainFrameWidget()->DispatchThroughCcInputHandler(
        GenerateEvent(WebInputEvent::Type::kGestureScrollEnd));
  }

  void ExpectOverscrollParams(
      const mojom::blink::DidOverscrollParamsPtr& params,
      gfx::Vector2dF expected_accumulated_overscroll,
      gfx::Vector2dF expected_latest_overscroll_delta,
      gfx::Vector2dF expected_current_fling_velocity,
      gfx::PointF expected_causal_event_viewport_point,
      cc::OverscrollBehavior expected_overscroll_behavior) {
    // Rounding errors are sometimes too big for DidOverscrollParams::Equals.
    const float kAbsError = 0.001;

    EXPECT_VECTOR2DF_NEAR(expected_accumulated_overscroll,
                          params->accumulated_overscroll, kAbsError);
    EXPECT_VECTOR2DF_NEAR(expected_latest_overscroll_delta,
                          params->latest_overscroll_delta, kAbsError);
    EXPECT_VECTOR2DF_NEAR(expected_current_fling_velocity,
                          params->current_fling_velocity, kAbsError);
    EXPECT_POINTF_NEAR(expected_causal_event_viewport_point,
                       params->causal_event_viewport_point, kAbsError);
    EXPECT_EQ(expected_overscroll_behavior, params->overscroll_behavior);
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         WebFrameOverscrollTest,
                         testing::Values(WebGestureDevice::kTouchpad,
                                         WebGestureDevice::kTouchscreen));

TEST_P(WebFrameOverscrollTest,
       AccumulatedRootOverscrollAndUnsedDeltaValuesOnOverscroll) {
  RegisterMockedHttpURLLoad("overscroll/overscroll.html");
  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "overscroll/overscroll.html",
                                    nullptr, nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(200, 200));

  auto* widget = web_view_helper.GetMainFrameWidget();
  auto* layer_tree_host = web_view_helper.GetLayerTreeHost();
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());

  // Calculation of accumulatedRootOverscroll and unusedDelta on multiple
  // scrollUpdate.
  ScrollBegin(&web_view_helper, -300, -316);
  ScrollUpdate(&web_view_helper, -308, -316);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  ExpectOverscrollParams(widget->last_overscroll(), gfx::Vector2dF(8, 16),
                         gfx::Vector2dF(8, 16), gfx::Vector2dF(),
                         gfx::PointF(100, 100), kOverscrollBehaviorAuto);

  ScrollUpdate(&web_view_helper, 0, -13);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  ExpectOverscrollParams(widget->last_overscroll(), gfx::Vector2dF(8, 29),
                         gfx::Vector2dF(0, 13), gfx::Vector2dF(),
                         gfx::PointF(100, 100), kOverscrollBehaviorAuto);

  ScrollUpdate(&web_view_helper, -20, -13);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  ExpectOverscrollParams(widget->last_overscroll(), gfx::Vector2dF(28, 42),
                         gfx::Vector2dF(20, 13), gfx::Vector2dF(),
                         gfx::PointF(100, 100), kOverscrollBehaviorAuto);

  // Overscroll is not reported.
  ScrollUpdate(&web_view_helper, 0, 1);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  EXPECT_TRUE(widget->last_overscroll().is_null());

  ScrollUpdate(&web_view_helper, 1, 0);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  EXPECT_TRUE(widget->last_overscroll().is_null());

  // Overscroll is reported.
  ScrollUpdate(&web_view_helper, 0, 1000);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  ExpectOverscrollParams(widget->last_overscroll(), gfx::Vector2dF(0, -701),
                         gfx::Vector2dF(0, -701), gfx::Vector2dF(),
                         gfx::PointF(100, 100), kOverscrollBehaviorAuto);

  // Overscroll is not reported.
  ScrollEnd(&web_view_helper);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  EXPECT_TRUE(widget->last_overscroll().is_null());
}

TEST_P(WebFrameOverscrollTest,
       AccumulatedOverscrollAndUnusedDeltaValuesOnDifferentAxesOverscroll) {
  RegisterMockedHttpURLLoad("overscroll/div-overscroll.html");
  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(
      base_url_ + "overscroll/div-overscroll.html", nullptr, nullptr,
      ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(200, 200));

  auto* widget = web_view_helper.GetMainFrameWidget();
  auto* layer_tree_host = web_view_helper.GetLayerTreeHost();
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());

  ScrollBegin(&web_view_helper, 0, -316);

  // Scroll the Div to the end.
  ScrollUpdate(&web_view_helper, 0, -316);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  EXPECT_TRUE(widget->last_overscroll().is_null());

  ScrollEnd(&web_view_helper);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());

  ScrollBegin(&web_view_helper, 0, -100);

  // Now On Scrolling DIV, scroll is bubbled and root layer is over-scrolled.
  ScrollUpdate(&web_view_helper, 0, -100);
  ScrollUpdate(&web_view_helper, 0, -100);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  ExpectOverscrollParams(widget->last_overscroll(), gfx::Vector2dF(0, 100),
                         gfx::Vector2dF(0, 100), gfx::Vector2dF(),
                         gfx::PointF(100, 100), kOverscrollBehaviorAuto);

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
  RegisterMockedHttpURLLoad("overscroll/div-overscroll.html");
  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(
      base_url_ + "overscroll/div-overscroll.html", nullptr, nullptr,
      ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(200, 200));

  auto* widget = web_view_helper.GetMainFrameWidget();
  auto* layer_tree_host = web_view_helper.GetLayerTreeHost();
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());

  ScrollBegin(&web_view_helper, 0, -316);

  // Scroll the Div to the end.
  ScrollUpdate(&web_view_helper, 0, -316);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  EXPECT_TRUE(widget->last_overscroll().is_null());

  ScrollEnd(&web_view_helper);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());

  ScrollBegin(&web_view_helper, 0, -150);

  // Now On Scrolling DIV, scroll is bubbled and root layer is over-scrolled.
  ScrollUpdate(&web_view_helper, 0, -150);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  ExpectOverscrollParams(widget->last_overscroll(), gfx::Vector2dF(0, 50),
                         gfx::Vector2dF(0, 50), gfx::Vector2dF(),
                         gfx::PointF(100, 100), kOverscrollBehaviorAuto);
}

TEST_P(WebFrameOverscrollTest, RootLayerOverscrolledOnInnerIFrameOverScroll) {
  RegisterMockedHttpURLLoad("overscroll/iframe-overscroll.html");
  RegisterMockedHttpURLLoad("overscroll/scrollable-iframe.html");
  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(
      base_url_ + "overscroll/iframe-overscroll.html", nullptr, nullptr,
      ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(200, 200));

  auto* widget = web_view_helper.GetMainFrameWidget();
  auto* layer_tree_host = web_view_helper.GetLayerTreeHost();
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());

  ScrollBegin(&web_view_helper, 0, -320);
  // Scroll the IFrame to the end.
  // This scroll will fully scroll the iframe but will be consumed before being
  // counted as overscroll.
  ScrollUpdate(&web_view_helper, 0, -320);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  EXPECT_TRUE(widget->last_overscroll().is_null());

  // This scroll will again target the iframe but wont bubble further up. Make
  // sure that the unused scroll isn't handled as overscroll.
  ScrollUpdate(&web_view_helper, 0, -50);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  EXPECT_TRUE(widget->last_overscroll().is_null());

  ScrollEnd(&web_view_helper);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());

  ScrollBegin(&web_view_helper, 0, -150);

  // Now On Scrolling IFrame, scroll is bubbled and root layer is over-scrolled.
  ScrollUpdate(&web_view_helper, 0, -150);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  ExpectOverscrollParams(widget->last_overscroll(), gfx::Vector2dF(0, 50),
                         gfx::Vector2dF(0, 50), gfx::Vector2dF(),
                         gfx::PointF(100, 100), kOverscrollBehaviorAuto);

  ScrollEnd(&web_view_helper);
}

TEST_P(WebFrameOverscrollTest, ScaledPageRootLayerOverscrolled) {
  RegisterMockedHttpURLLoad("overscroll/overscroll.html");
  frame_test_helpers::WebViewHelper web_view_helper;

  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "overscroll/overscroll.html", nullptr, nullptr,
      ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(200, 200));
  web_view_impl->SetPageScaleFactor(3.0);

  auto* widget = web_view_helper.GetMainFrameWidget();
  auto* layer_tree_host = web_view_helper.GetLayerTreeHost();
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());

  // Calculation of accumulatedRootOverscroll and unusedDelta on scaled page.
  // The point is (100, 100) because that is the position GenerateEvent uses.
  ScrollBegin(&web_view_helper, 0, 30);
  ScrollUpdate(&web_view_helper, 0, 30);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  ExpectOverscrollParams(widget->last_overscroll(), gfx::Vector2dF(0, -30),
                         gfx::Vector2dF(0, -30), gfx::Vector2dF(),
                         gfx::PointF(100, 100), kOverscrollBehaviorAuto);

  ScrollUpdate(&web_view_helper, 0, 30);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  ExpectOverscrollParams(widget->last_overscroll(), gfx::Vector2dF(0, -60),
                         gfx::Vector2dF(0, -30), gfx::Vector2dF(),
                         gfx::PointF(100, 100), kOverscrollBehaviorAuto);

  ScrollUpdate(&web_view_helper, 30, 30);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  ExpectOverscrollParams(widget->last_overscroll(), gfx::Vector2dF(-30, -90),
                         gfx::Vector2dF(-30, -30), gfx::Vector2dF(),
                         gfx::PointF(100, 100), kOverscrollBehaviorAuto);

  ScrollUpdate(&web_view_helper, 30, 0);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  ExpectOverscrollParams(widget->last_overscroll(), gfx::Vector2dF(-60, -90),
                         gfx::Vector2dF(-30, 0), gfx::Vector2dF(),
                         gfx::PointF(100, 100), kOverscrollBehaviorAuto);

  // Overscroll is not reported.
  ScrollEnd(&web_view_helper);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  EXPECT_TRUE(widget->last_overscroll().is_null());
}

TEST_P(WebFrameOverscrollTest, NoOverscrollForSmallvalues) {
  RegisterMockedHttpURLLoad("overscroll/overscroll.html");
  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "overscroll/overscroll.html",
                                    nullptr, nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(200, 200));

  auto* widget = web_view_helper.GetMainFrameWidget();
  auto* layer_tree_host = web_view_helper.GetLayerTreeHost();
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());

  ScrollBegin(&web_view_helper, 10, 10);
  ScrollUpdate(&web_view_helper, 10, 10);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  ExpectOverscrollParams(widget->last_overscroll(), gfx::Vector2dF(-10, -10),
                         gfx::Vector2dF(-10, -10), gfx::Vector2dF(),
                         gfx::PointF(100, 100), kOverscrollBehaviorAuto);

  ScrollUpdate(&web_view_helper, 0, 0.10);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  ExpectOverscrollParams(widget->last_overscroll(), gfx::Vector2dF(-10, -10.10),
                         gfx::Vector2dF(0, -0.10), gfx::Vector2dF(),
                         gfx::PointF(100, 100), kOverscrollBehaviorAuto);

  ScrollUpdate(&web_view_helper, 0.10, 0);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  ExpectOverscrollParams(widget->last_overscroll(),
                         gfx::Vector2dF(-10.10, -10.10),
                         gfx::Vector2dF(-0.10, 0), gfx::Vector2dF(),
                         gfx::PointF(100, 100), kOverscrollBehaviorAuto);

  // For residual values overscrollDelta should be reset and DidOverscroll
  // shouldn't be called.
  ScrollUpdate(&web_view_helper, 0, 0.09);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  EXPECT_TRUE(widget->last_overscroll().is_null());

  ScrollUpdate(&web_view_helper, 0.09, 0.09);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  EXPECT_TRUE(widget->last_overscroll().is_null());

  ScrollUpdate(&web_view_helper, 0.09, 0);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  EXPECT_TRUE(widget->last_overscroll().is_null());

  ScrollUpdate(&web_view_helper, 0, -0.09);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  EXPECT_TRUE(widget->last_overscroll().is_null());

  ScrollUpdate(&web_view_helper, -0.09, -0.09);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  EXPECT_TRUE(widget->last_overscroll().is_null());

  ScrollUpdate(&web_view_helper, -0.09, 0);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  EXPECT_TRUE(widget->last_overscroll().is_null());

  ScrollEnd(&web_view_helper);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  EXPECT_TRUE(widget->last_overscroll().is_null());
}

TEST_P(WebFrameOverscrollTest, OverscrollBehaviorGoesToCompositor) {
  RegisterMockedHttpURLLoad("overscroll/overscroll.html");
  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "overscroll/overscroll.html",
                                    nullptr, nullptr, ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(200, 200));

  auto* widget = web_view_helper.GetMainFrameWidget();
  auto* layer_tree_host = web_view_helper.GetLayerTreeHost();

  WebLocalFrame* mainFrame =
      web_view_helper.GetWebView()->MainFrame()->ToWebLocalFrame();
  EXPECT_EQ(web_view_helper.GetLayerTreeHost()->overscroll_behavior(),
            kOverscrollBehaviorAuto);
  mainFrame->ExecuteScript(
      WebScriptSource(WebString("document.body.style="
                                "'overscroll-behavior: auto;'")));
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());

  ScrollBegin(&web_view_helper, 100, 116);
  ScrollUpdate(&web_view_helper, 100, 100);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  ExpectOverscrollParams(widget->last_overscroll(), gfx::Vector2dF(-100, -100),
                         gfx::Vector2dF(-100, -100), gfx::Vector2dF(),
                         gfx::PointF(100, 100), kOverscrollBehaviorAuto);
  EXPECT_EQ(web_view_helper.GetLayerTreeHost()->overscroll_behavior(),
            kOverscrollBehaviorAuto);

  mainFrame->ExecuteScript(
      WebScriptSource(WebString("document.body.style="
                                "'overscroll-behavior: contain;'")));
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());

  ScrollUpdate(&web_view_helper, 100, 100);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  ExpectOverscrollParams(widget->last_overscroll(), gfx::Vector2dF(-200, -200),
                         gfx::Vector2dF(-100, -100), gfx::Vector2dF(),
                         gfx::PointF(100, 100), kOverscrollBehaviorContain);
  EXPECT_EQ(web_view_helper.GetLayerTreeHost()->overscroll_behavior(),
            kOverscrollBehaviorContain);

  mainFrame->ExecuteScript(
      WebScriptSource(WebString("document.body.style="
                                "'overscroll-behavior: none;'")));
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());

  ScrollUpdate(&web_view_helper, 100, 100);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  ExpectOverscrollParams(widget->last_overscroll(), gfx::Vector2dF(-300, -300),
                         gfx::Vector2dF(-100, -100), gfx::Vector2dF(),
                         gfx::PointF(100, 100), kOverscrollBehaviorNone);
  EXPECT_EQ(web_view_helper.GetLayerTreeHost()->overscroll_behavior(),
            kOverscrollBehaviorNone);
}

TEST_P(WebFrameOverscrollTest, SubframeOverscrollBehaviorPreventsChaining) {
  RegisterMockedHttpURLLoad("overscroll/iframe-overscroll.html");
  RegisterMockedHttpURLLoad("overscroll/scrollable-iframe.html");
  frame_test_helpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(
      base_url_ + "overscroll/iframe-overscroll.html", nullptr, nullptr,
      ConfigureAndroid);
  web_view_helper.Resize(gfx::Size(200, 200));

  auto* widget = web_view_helper.GetMainFrameWidget();
  auto* layer_tree_host = web_view_helper.GetLayerTreeHost();

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
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());

  ScrollBegin(&web_view_helper, 100, 116);
  ScrollUpdate(&web_view_helper, 100, 100);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  EXPECT_TRUE(widget->last_overscroll().is_null());
  EXPECT_EQ(web_view_helper.GetLayerTreeHost()->overscroll_behavior(),
            kOverscrollBehaviorAuto);

  subframe->ExecuteScript(
      WebScriptSource(WebString("document.body.style="
                                "'overscroll-behavior: contain;'")));
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());

  ScrollUpdate(&web_view_helper, 100, 100);
  layer_tree_host->CompositeForTest(base::TimeTicks::Now(), false,
                                    base::OnceClosure());
  EXPECT_TRUE(widget->last_overscroll().is_null());
  EXPECT_EQ(web_view_helper.GetLayerTreeHost()->overscroll_behavior(),
            kOverscrollBehaviorAuto);
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
      web_view_helper.CreateLocalChild(*web_view_helper.RemoteMainFrame());
  while (page->SubframeCount() < Page::MaxNumberOfFrames()) {
    frame_test_helpers::CreateRemoteChild(*web_view_helper.RemoteMainFrame());
  }
  auto* iframe = MakeGarbageCollected<HTMLIFrameElement>(
      *frame->GetFrame()->GetDocument());
  iframe->setAttribute(html_names::kSrcAttr, g_empty_atom);
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
      mojom::blink::ViewportIntersectionStatePtr intersection_state,
      const std::optional<FrameVisualProperties>& visual_properties) override {
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
  TestViewportIntersection remote_frame_host;
  WebRemoteFrameImpl* remote_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(
      web_view_helper.LocalMainFrame()->FirstChild(), remote_frame,
      remote_frame_host.BindNewAssociatedRemote());
  web_view->MainFrameImpl()
      ->GetFrame()
      ->View()
      ->UpdateAllLifecyclePhasesForTest();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(!remote_frame_host.GetIntersectionState()
                   ->viewport_intersection.IsEmpty());
  EXPECT_TRUE(
      gfx::Rect(remote_frame->GetFrame()->View()->Size())
          .Contains(
              remote_frame_host.GetIntersectionState()->viewport_intersection));
  ASSERT_TRUE(!remote_frame_host.GetIntersectionState()
                   ->main_frame_intersection.IsEmpty());
  EXPECT_TRUE(gfx::Rect(remote_frame->GetFrame()->View()->Size())
                  .Contains(remote_frame_host.GetIntersectionState()
                                ->main_frame_intersection));
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

  EXPECT_TRUE(system_clipboard->ReadAvailableTypes().empty());

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

  EXPECT_TRUE(system_clipboard->ReadAvailableTypes().empty());

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

class SelectionMockWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  MOCK_METHOD(void, DidChangeSelection, (bool, blink::SyncCondition));
};

TEST_F(WebFrameTest, ImeSelectionCommitDoesNotChangeClipboard) {
  using blink::ImeTextSpan;
  using ui::mojom::ImeTextSpanThickness;
  using ui::mojom::ImeTextSpanUnderlineStyle;

  RegisterMockedHttpURLLoad("foo.html");
  SelectionMockWebFrameClient web_frame_client;

  frame_test_helpers::WebViewHelper web_view_helper;
  WebLocalFrameImpl* web_frame =
      web_view_helper
          .InitializeAndLoad(base_url_ + "foo.html", &web_frame_client)
          ->MainFrameImpl();
  WebViewImpl* web_view = web_view_helper.GetWebView();
  WebFrameWidget* widget = web_view->MainFrameImpl()->FrameWidgetImpl();
  EXPECT_CALL(web_frame_client, DidChangeSelection(true, _))
      .WillRepeatedly(Return());  // Happens due to edit change.
  EXPECT_CALL(web_frame_client, DidChangeSelection(false, _))
      .WillRepeatedly(testing::Invoke(
          [widget] { EXPECT_FALSE(widget->HandlingInputEvent()); }));

  Document* document = web_frame->GetFrame()->GetDocument();

  document->write("<div id='sample' contenteditable>hello world</div>");
  document->getElementById(AtomicString("sample"))->Focus();

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 5, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  InputMethodController& controller =
      web_frame->GetFrame()->GetInputMethodController();
  controller.SetCompositionFromExistingText(ime_text_spans, 0, 5);

  // Even though the commit came as part of a user interaction,
  // the internal selection to replace the composition (done as
  // part of the commit) should _not_ be marked as such, or it would
  // change the X11 clipboard (crbug.com/1213325).
  // The actual test for this is in the EXPECT_CALL above.
  //
  // Since the selection-to-clipboard logic isn't hooked up in
  // TestWebFrameClient, we cannot check that the actual clipboard
  // values don't change, but must be slightly more indirect
  // in our testing, and thus, we check for HandlingInputEvent()
  // instead (which, in the actual code, suppresses the clipboard logic).
  widget->SetHandlingInputEvent(true);
  controller.CommitText(String("replaced"), ime_text_spans, 0);
  widget->SetHandlingInputEvent(false);
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
    web_remote_frame_ = frame_test_helpers::CreateRemote();
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
    frame_test_helpers::SwapRemoteFrame(
        MainFrame()->LastChild(), RemoteFrame(),
        remote_frame_host_.BindNewAssociatedRemote());
  }

  WebLocalFrame* MainFrame() { return frame_; }
  WebRemoteFrameImpl* RemoteFrame() { return web_remote_frame_; }
  TestRemoteFrameHostForVisibility* RemoteFrameHost() {
    return &remote_frame_host_;
  }

 private:
  TestRemoteFrameHostForVisibility remote_frame_host_;
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
      mojom::blink::TreeScopeType scope,
      const WebString& name,
      const WebString& fallback_name,
      const FramePolicy&,
      const WebFrameOwnerProperties&,
      FrameOwnerElementType,
      WebPolicyContainerBindParams policy_container_bind_params,
      ukm::SourceId document_ukm_source_id,
      FinishChildFrameCreationFn finish_creation) override {
    return CreateLocalChild(*Frame(), scope, &child_client_,
                            std::move(policy_container_bind_params),
                            finish_creation);
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
  test::TaskEnvironment task_environment;
  frame_test_helpers::WebViewHelper helper;
  helper.Initialize();

  WebLocalFrame* main_frame = helper.LocalMainFrame();
  v8::HandleScope scope(helper.GetAgentGroupScheduler().Isolate());
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
  test::TaskEnvironment task_environment;
  frame_test_helpers::WebViewHelper helper;
  helper.Initialize(nullptr, nullptr, EnableGlobalReuseForUnownedMainFrames);

  WebLocalFrame* main_frame = helper.LocalMainFrame();
  frame_test_helpers::LoadFrame(main_frame, "data:text/html,<iframe></iframe>");

  WebLocalFrame* child_frame = main_frame->FirstChild()->ToWebLocalFrame();
  v8::HandleScope scope(helper.GetAgentGroupScheduler().Isolate());
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
  test::TaskEnvironment task_environment;
  frame_test_helpers::WebViewHelper opener_helper;
  opener_helper.Initialize();
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeWithOpener(opener_helper.GetWebView()->MainFrame(), nullptr,
                              nullptr, EnableGlobalReuseForUnownedMainFrames);

  WebLocalFrame* main_frame = helper.LocalMainFrame();
  v8::HandleScope scope(helper.GetAgentGroupScheduler().Isolate());
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
  test::TaskEnvironment task_environment;
  frame_test_helpers::WebViewHelper helper;
  helper.Initialize(nullptr, nullptr, EnableGlobalReuseForUnownedMainFrames);

  WebLocalFrame* main_frame = helper.LocalMainFrame();
  v8::Isolate* isolate = helper.GetAgentGroupScheduler().Isolate();
  v8::HandleScope scope(isolate);
  main_frame->ExecuteScript(WebScriptSource("hello = 'world';"));
  frame_test_helpers::LoadFrame(main_frame, "data:text/html,new page");
  v8::Local<v8::Value> result =
      main_frame->ExecuteScriptAndReturnValue(WebScriptSource("hello"));
  ASSERT_TRUE(result->IsString());
  EXPECT_EQ("world",
            ToCoreString(isolate,
                         result->ToString(main_frame->MainWorldScriptContext())
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
    NOTREACHED_IN_MIGRATION();
  }

  void GetBlobFromUUID(mojo::PendingReceiver<mojom::blink::Blob>,
                       const String& uuid,
                       GetBlobFromUUIDCallback) override {
    NOTREACHED_IN_MIGRATION();
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
        mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle);
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

    void OnDataAvailable(base::span<const uint8_t> data) override {
      std::string_view chars = base::as_string_view(data);
      *output_ = String(chars.data(), chars.size());
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
  helper.LocalMainFrame()->GetFrame()->LoadJavaScriptURL(javascript_url);
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

TEST_F(WebFrameTest, DiscardFrame) {
  DisableRendererSchedulerThrottling();
  RegisterMockedHttpURLLoad("foo.html");

  frame_test_helpers::WebViewHelper helper;
  helper.InitializeAndLoad(base_url_ + "foo.html");

  EXPECT_NE("", To<LocalFrame>(helper.GetWebView()->GetPage()->MainFrame())
                    ->GetDocument()
                    ->documentElement()
                    ->innerText());

  helper.LocalMainFrame()->GetFrame()->Discard();
  RunPendingTasks();

  // Discarding should replace the contents of the document.
  EXPECT_EQ("", To<LocalFrame>(helper.GetWebView()->GetPage()->MainFrame())
                    ->GetDocument()
                    ->documentElement()
                    ->innerText());
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
  void FinalizeRequest(WebURLRequest& request) override {
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

// TODO(crbug.com/1314493): This test is flaky with the TimedHTMLParserBudget
// feature enabled.
TEST_F(WebFrameTest, DISABLED_ChangeResourcePriority) {
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

class MultipleDataChunkDelegate : public URLLoaderTestDelegate {
 public:
  MultipleDataChunkDelegate() = default;
  ~MultipleDataChunkDelegate() override = default;

  // URLLoaderTestDelegate:
  void DidReceiveData(URLLoaderClient* original_client,
                      base::span<const char> data) override {
    EXPECT_GT(data.size(), 16u);
    original_client->DidReceiveDataForTesting(data.subspan(0, 16));
    // This didReceiveData call shouldn't crash due to a failed assertion.
    original_client->DidReceiveDataForTesting(data.subspan(16));
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

// Ensure that the root LayoutView maintains a minimum height matching the
// viewport in cases where the content is smaller.
TEST_F(WebFrameTest, RootLayerMinimumHeight) {
  constexpr int kViewportWidth = 320;
  constexpr int kViewportHeight = 640;
  constexpr int kBrowserControlsHeight = 100;

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(nullptr, nullptr, ConfigureAndroid);
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
  const auto* layout_view = frame_view->GetLayoutView();
  EXPECT_EQ(kViewportHeight - kBrowserControlsHeight,
            layout_view->ViewRect().Height());
  EXPECT_EQ(kViewportHeight - kBrowserControlsHeight,
            layout_view->BackgroundRect().Height());

  document->View()->SetTracksRasterInvalidations(true);

  web_view->ResizeWithBrowserControls(
      gfx::Size(kViewportWidth, kViewportHeight), kBrowserControlsHeight, 0,
      false);
  UpdateAllLifecyclePhases(web_view);

  EXPECT_EQ(kViewportHeight, layout_view->ViewRect().Height());
  EXPECT_EQ(kViewportHeight, layout_view->BackgroundRect().Height());
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
  auto* widget = web_view_helper.GetMainFrameWidget();
  widget->DispatchThroughCcInputHandler(end_event);
  widget->DispatchThroughCcInputHandler(update_event);
  web_view_helper.GetLayerTreeHost()->CompositeForTest(
      base::TimeTicks::Now(), false, base::OnceClosure());

  // Try a full Begin/Update/End cycle.
  widget->DispatchThroughCcInputHandler(begin_event);
  widget->DispatchThroughCcInputHandler(update_event);
  widget->DispatchThroughCcInputHandler(end_event);
  web_view_helper.GetLayerTreeHost()->CompositeForTest(
      base::TimeTicks::Now(), false, base::OnceClosure());
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
  Element* div1_tag = document->getElementById(AtomicString("div1"));

  HitTestResult hit_test_result =
      web_view->MainFrameViewWidget()->CoreHitTestResultAt(
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

  Element* div2_tag = document->getElementById(AtomicString("div2"));

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
    WebView().GetSettings()->SetAutoZoomFocusedEditableToLegibleScale(true);
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

  EXPECT_EQ(GetDocument().getElementById(AtomicString("top")),
            result.InnerNode());
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
  Vector<gfx::Rect> original_tickmarks =
      frame_view->LayoutViewport()->GetTickmarks();
  EXPECT_EQ(1u, original_tickmarks.size());

  EXPECT_EQ(gfx::Point(800, 2000), original_tickmarks[0].origin());
}

#if BUILDFLAG(IS_ANDROID)
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

  Element* box1 = GetDocument().getElementById(AtomicString("box1"));
  Element* box2 = GetDocument().getElementById(AtomicString("box2"));

  gfx::Rect box1_rect = box1->GetLayoutObject()->AbsoluteBoundingBoxRect();
  gfx::Rect box2_rect = box2->GetLayoutObject()->AbsoluteBoundingBoxRect();

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

  gfx::RectF result_rect = web_match_rects[0];
  frame->EnsureTextFinder().SelectNearestFindMatch(result_rect.CenterPoint(),
                                                   nullptr);

  EXPECT_TRUE(frame_view->GetScrollableArea()->VisibleContentRect().Contains(
      box1_rect));
  result_rect = web_match_rects[1];
  frame->EnsureTextFinder().SelectNearestFindMatch(result_rect.CenterPoint(),
                                                   nullptr);

  EXPECT_TRUE(
      frame_view->GetScrollableArea()->VisibleContentRect().Contains(box2_rect))
      << "Box [" << box2_rect.ToString() << "] is not visible in viewport ["
      << frame_view->GetScrollableArea()->VisibleContentRect().ToString()
      << "]";
}
#endif  // BUILDFLAG(IS_ANDROID)

// Check that removing an element whilst focusing it does not cause a null
// pointer deference. This test passes if it does not crash.
// https://crbug.com/1184546
TEST_F(WebFrameSimTest, FocusOnBlurRemoveBubblingCrash) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <script>
      window.onload = function (){
        document.getElementById('id0').onblur=function() {
          var o=document.getElementById('id6');
          var n=document.createElement(undefined);
          o.parentNode.replaceChild(n,o);
        };
        var o=document.getElementById('id7');
        o.focus();
      }
      </script>
      <body id='id0'>
      <strong id='id6'>
      <iframe id='id7'src=''></iframe>
      <textarea id='id35' autofocus='false'>
  )HTML");

  Compositor().BeginFrame();
  RunPendingTasks();
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
  WebView().GetSettings()->SetAutoZoomFocusedEditableToLegibleScale(true);

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
  gfx::Rect inputRect(200, 600, 100, 20);

  frame_view->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 0), mojom::blink::ScrollType::kProgrammatic);

  ASSERT_EQ(gfx::Point(),
            frame_view->GetScrollableArea()->VisibleContentRect().origin());

  WebView()
      .MainFrameImpl()
      ->FrameWidget()
      ->ScrollFocusedEditableElementIntoView();

  EXPECT_EQ(1, WebView().FakePageScaleAnimationPageScaleForTesting());

  frame_view->LayoutViewport()->SetScrollOffset(
      ScrollOffset(WebView()
                       .FakePageScaleAnimationTargetPositionForTesting()
                       .OffsetFromOrigin()),
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
  EXPECT_EQ(gfx::Point(),
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
      ScrollOffset(WebView()
                       .FakePageScaleAnimationTargetPositionForTesting()
                       .OffsetFromOrigin()),
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
  WebView().GetSettings()->SetAutoZoomFocusedEditableToLegibleScale(true);

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

  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  ASSERT_EQ(scroller, rs_controller.GlobalRootScroller());

  auto* frame = To<LocalFrame>(WebView().GetPage()->MainFrame());
  VisualViewport& visual_viewport = frame->GetPage()->GetVisualViewport();

  WebView().AdvanceFocus(false);

  rs_controller.RootScrollerArea()->SetScrollOffset(
      ScrollOffset(0, 300), mojom::blink::ScrollType::kProgrammatic);

  LocalFrameView* frame_view = frame->View();
  gfx::Rect inputRect(200, 700, 100, 20);
  ASSERT_EQ(1, visual_viewport.Scale());
  ASSERT_EQ(gfx::Point(0, 300),
            frame_view->GetScrollableArea()->VisibleContentRect().origin());
  ASSERT_FALSE(frame_view->GetScrollableArea()->VisibleContentRect().Contains(
      inputRect));

  WebView()
      .MainFrameImpl()
      ->FrameWidget()
      ->ScrollFocusedEditableElementIntoView();

  EXPECT_EQ(1, WebView().FakePageScaleAnimationPageScaleForTesting());

  ScrollOffset target_offset(
      WebView()
          .FakePageScaleAnimationTargetPositionForTesting()
          .OffsetFromOrigin());

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

  ASSERT_EQ(gfx::Point(),
            frame_view->GetScrollableArea()->VisibleContentRect().origin());

  // Simulate the keyboard being shown and resizing the widget. Cause a scroll
  // into view after.
  WebView().MainFrameViewWidget()->Resize(gfx::Size(400, 300));

  float scale_before = visual_viewport.Scale();
  WebView()
      .MainFrameImpl()
      ->FrameWidget()
      ->ScrollFocusedEditableElementIntoView();

  Element* input = GetDocument().getElementById(AtomicString("target"));
  gfx::Rect input_rect(input->GetBoundingClientRect()->top(),
                       input->GetBoundingClientRect()->left(),
                       input->GetBoundingClientRect()->width(),
                       input->GetBoundingClientRect()->height());

  gfx::Rect visible_content_rect(frame_view->Size());
  EXPECT_TRUE(visible_content_rect.Contains(input_rect))
      << "Layout viewport [" << visible_content_rect.ToString()
      << "] does not contain input rect [" << input_rect.ToString()
      << "] after scroll into view.";

  EXPECT_TRUE(visual_viewport.VisibleRect().Contains(gfx::RectF(input_rect)))
      << "Visual viewport [" << visual_viewport.VisibleRect().ToString()
      << "] does not contain input rect [" << input_rect.ToString()
      << "] after scroll into view.";

  // Make sure we also zoomed in on the input.
  EXPECT_GT(WebView().FakePageScaleAnimationPageScaleForTesting(),
            scale_before);

  // Additional gut-check that we actually scrolled the non-user-scrollable
  // clip element to make sure the input is in view.
  Element* clip = GetDocument().getElementById(AtomicString("clip"));
  EXPECT_GT(clip->scrollTop(), 0);
}

// This test ensures that we scroll to the correct scale when the focused
// element has a selection rather than a caret.
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

  auto* input = To<HTMLInputElement>(
      GetDocument().getElementById(AtomicString("target")));
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
  gfx::Rect target_rect_in_document(2000, 3000, 100, 100);

  ASSERT_EQ(0.5f, visual_viewport.Scale());

  // Center the target in the screen.
  frame_view->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(2000 - 440, 3000 - 450),
      mojom::blink::ScrollType::kProgrammatic);
  Element* target = GetDocument().QuerySelector(AtomicString("#target"));
  DOMRect* rect = target->GetBoundingClientRect();
  ASSERT_EQ(440, rect->left());
  ASSERT_EQ(450, rect->top());

  // Double-tap on the target. Expect that we zoom in and the target is
  // contained in the visual viewport.
  {
    gfx::Point point(445, 455);
    gfx::Rect block_bounds = ComputeBlockBoundHelper(&WebView(), point, false);
    WebView().AnimateDoubleTapZoom(point, block_bounds);
    EXPECT_TRUE(WebView().FakeDoubleTapAnimationPendingForTesting());
    ScrollOffset new_offset(
        WebView()
            .FakePageScaleAnimationTargetPositionForTesting()
            .OffsetFromOrigin());
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
    gfx::Rect block_bounds = ComputeBlockBoundHelper(&WebView(), point, false);
    WebView().AnimateDoubleTapZoom(point, block_bounds);
    EXPECT_TRUE(WebView().FakeDoubleTapAnimationPendingForTesting());
    gfx::Point target_offset(
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

  Element* body = GetDocument().QuerySelector(AtomicString("body"));
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

  Element* input = GetDocument().getElementById(AtomicString("target"));
  input->Focus();

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

TEST_F(WebFrameSimTest, ScrollEditContextIntoView) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(500, 600));
  WebView().GetPage()->GetSettings().SetTextAutosizingEnabled(false);
  WebView().SetZoomFactorForDeviceScaleFactor(2.0f);

  SimRequest r("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  r.Complete(R"HTML(
      <!DOCTYPE html>
      <div id="target" style='width:2000px;height:2000px'></div>
      <script>
        const editContext = new EditContext();
        const target = document.getElementById('target');
        target.editContext = editContext;
        target.focus();
        let controlBounds = new DOMRect(500, 850, 1, 20);
        editContext.updateControlBounds(controlBounds);
      </script>
  )HTML");

  WebView().EnableFakePageScaleAnimationForTesting(true);

  WebView()
      .MainFrameImpl()
      ->FrameWidgetImpl()
      ->ScrollFocusedEditableElementIntoView();

  // scrollOffset.x = controlBound.x * zoom - left padding = 500 * 2 - 150 = 850
  // scrollOffset.y = controlBound.y * zoom - (viewport.height -
  // controlBound.height * 2)/2
  //                = 850 * 2 - (600 - 20 * 2) / 2 = 1420
  EXPECT_EQ(gfx::Point(850, 1420),
            WebView().FakePageScaleAnimationTargetPositionForTesting());
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

  Element* element = GetDocument().QuerySelector(AtomicString("iframe"));
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

  Element* element = GetDocument().QuerySelector(AtomicString("iframe"));
  auto* frame_owner_element = To<HTMLFrameOwnerElement>(element);
  Document* iframe_doc = frame_owner_element->contentDocument();
  EXPECT_FALSE(iframe_doc->documentElement()->GetLayoutObject());

  gfx::SizeF page_size(400, 400);
  float maximum_shrink_ratio = 1.0;
  iframe_doc->GetFrame()->StartPrinting(WebPrintParams(page_size),
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

  Element* element = GetDocument().QuerySelector(AtomicString("iframe"));
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
  ASSERT_EQ(gfx::Size(400, 570), area->ContentsSize());

  // Hide browser controls, growing layout viewport without affecting ICB.
  WebView().ResizeWithBrowserControls(gfx::Size(400, 600), 60, 0, false);
  Compositor().BeginFrame();

  // ContentsSize() should grow to accommodate new visible size.
  ASSERT_EQ(600, area->VisibleHeight());
  ASSERT_EQ(gfx::Size(400, 600), area->ContentsSize());
}

TEST_F(WebFrameSimTest, NamedLookupIgnoresEmptyNames) {
  SimRequest main_resource("https://example.com/main.html", "text/html");
  LoadURL("https://example.com/main.html");
  main_resource.Complete(R"HTML(
    <body>
    <iframe name="" src="data:text/html,"></iframe>
    </body>)HTML");

  EXPECT_EQ(nullptr, MainFrame().GetFrame()->Tree().ScopedChild(g_empty_atom));
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

    void DidDispatchDOMContentLoadedEvent() override {
      // TODO(dcheng): Investigate not calling this as well during frame detach.
      did_call_did_dispatch_dom_content_loaded_event_ = true;
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
    bool DidCallDidDispatchDOMContentLoadedEvent() const {
      return did_call_did_dispatch_dom_content_loaded_event_;
    }
    bool DidCallDidHandleOnloadEvents() const {
      return did_call_did_handle_onload_events_;
    }

   private:
    bool did_call_frame_detached_ = false;
    bool did_call_did_stop_loading_ = false;
    bool did_call_did_dispatch_dom_content_loaded_event_ = false;
    bool did_call_did_handle_onload_events_ = false;
  };

  class MainFrameClient : public frame_test_helpers::TestWebFrameClient {
   public:
    MainFrameClient() = default;
    ~MainFrameClient() override = default;

    // frame_test_helpers::TestWebFrameClient:
    WebLocalFrame* CreateChildFrame(
        mojom::blink::TreeScopeType scope,
        const WebString& name,
        const WebString& fallback_name,
        const FramePolicy&,
        const WebFrameOwnerProperties&,
        FrameOwnerElementType,
        WebPolicyContainerBindParams policy_container_bind_params,
        ukm::SourceId document_ukm_source_id,
        FinishChildFrameCreationFn finish_creation) override {
      return CreateLocalChild(*Frame(), scope, &child_client_,
                              std::move(policy_container_bind_params),
                              finish_creation);
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
  EXPECT_TRUE(main_frame_client.ChildClient()
                  .DidCallDidDispatchDOMContentLoadedEvent());
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

TEST_F(WebFrameTest, ShowVirtualKeyboardOnElementFocus) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeRemote();

  WebLocalFrameImpl* local_frame = web_view_helper.CreateLocalChild(
      *web_view_helper.RemoteMainFrame(), "child", WebFrameOwnerProperties(),
      nullptr, nullptr);

  frame_test_helpers::TestWebFrameWidgetHost& widget_host =
      static_cast<frame_test_helpers::TestWebFrameWidget*>(
          local_frame->FrameWidgetImpl())
          ->WidgetHost();

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
  // Verify that the right WidgetHost has been notified.
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(0u, widget_host.VirtualKeyboardRequestCount());
#else
  EXPECT_LT(0u, widget_host.VirtualKeyboardRequestCount());
#endif
  web_view_helper.Reset();
}

class ContextMenuWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  ContextMenuWebFrameClient() = default;
  ContextMenuWebFrameClient(const ContextMenuWebFrameClient&) = delete;
  ContextMenuWebFrameClient& operator=(const ContextMenuWebFrameClient&) =
      delete;
  ~ContextMenuWebFrameClient() override = default;

  // WebLocalFrameClient:
  void UpdateContextMenuDataForTesting(
      const ContextMenuData& data,
      const std::optional<gfx::Point>&) override {
    menu_data_ = data;
  }

  ContextMenuData GetMenuData() { return menu_data_; }

 private:
  ContextMenuData menu_data_;
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
  EXPECT_FALSE(TestSelectAll("<div contenteditable>\n</div>"));
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
  EXPECT_EQ(frame.GetMenuData().form_control_type,
            blink::mojom::FormControlType::kInputPassword);
  EXPECT_FALSE(frame.GetMenuData().selected_text.empty());
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
  EXPECT_FALSE(frame.GetMenuData().selected_text.empty());
}

TEST_F(WebFrameTest, LocalFrameWithRemoteParentIsTransparent) {
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote();

  WebLocalFrameImpl* local_frame =
      helper.CreateLocalChild(*helper.RemoteMainFrame());
  frame_test_helpers::LoadFrame(local_frame, "data:text/html,some page");

  // Local frame with remote parent should have transparent baseBackgroundColor.
  Color color = local_frame->GetFrameView()->BaseBackgroundColor();
  EXPECT_EQ(Color::kTransparent, color);
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
                                    ->getElementById(AtomicString("foo"))
                                    ->GetLayoutObject()
                                    ->SlowFirstChild();
  String text = "";
  for (LayoutObject* obj = layout_object; obj; obj = obj->NextInPreOrder()) {
    if (obj->IsText()) {
      text = To<LayoutText>(obj)->TransformedText();
      break;
    }
  }
  EXPECT_EQ("foo alt", text.Utf8());
}

static void TestFramePrinting(WebLocalFrameImpl* frame) {
  gfx::Size page_size(500, 500);
  WebPrintParams print_params((gfx::SizeF(page_size)));
  EXPECT_EQ(1u, frame->PrintBegin(print_params, WebNode()));
  cc::PaintRecorder recorder;
  frame->PrintPagesForTesting(recorder.beginRecording(), page_size);
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
    const PaintRecord& paint_record,
    DOMNodeId dom_node_id,
    std::vector<TextRunDOMNodeIdInfo>* text_runs) {
  for (const cc::PaintOp& op : paint_record) {
    if (op.GetType() == cc::PaintOpType::kDrawRecord) {
      const auto& draw_record_op = static_cast<const cc::DrawRecordOp&>(op);
      RecursiveCollectTextRunDOMNodeIds(draw_record_op.record, dom_node_id,
                                        text_runs);
    } else if (op.GetType() == cc::PaintOpType::kSetNodeId) {
      const auto& set_node_id_op = static_cast<const cc::SetNodeIdOp&>(op);
      dom_node_id = set_node_id_op.node_id;
    } else if (op.GetType() == cc::PaintOpType::kDrawTextBlob) {
      const auto& draw_text_op = static_cast<const cc::DrawTextBlobOp&>(op);
      SkTextBlob::Iter iter(*draw_text_op.blob);
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

std::vector<TextRunDOMNodeIdInfo> GetPrintedTextRunDOMNodeIds(
    WebLocalFrame* frame,
    const WebVector<uint32_t>* pages = nullptr) {
  gfx::Size page_size(500, 500);
  WebPrintParams print_params((gfx::SizeF(page_size)));

  frame->PrintBegin(print_params, WebNode());
  cc::PaintRecorder recorder;
  frame->PrintPagesForTesting(recorder.beginRecording(), page_size, pages);
  frame->PrintEnd();

  cc::PaintRecord paint_record = recorder.finishRecordingAsPicture();
  std::vector<TextRunDOMNodeIdInfo> text_runs;
  RecursiveCollectTextRunDOMNodeIds(paint_record, 0, &text_runs);

  return text_runs;
}

}  // namespace

TEST_F(WebFrameTest, PrintSomePages) {
  RegisterMockedHttpURLLoad("print-pages.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "print-pages.html");

  WebVector<uint32_t> pages;
  pages.push_back(1);
  pages.push_back(4);
  pages.push_back(8);
  std::vector<TextRunDOMNodeIdInfo> text_runs =
      GetPrintedTextRunDOMNodeIds(web_view_helper.LocalMainFrame(), &pages);

  ASSERT_EQ(3u, text_runs.size());
  EXPECT_EQ(2, text_runs[0].glyph_len);  // Page 2
  EXPECT_EQ(5, text_runs[1].glyph_len);  // Page 5
  EXPECT_EQ(9, text_runs[2].glyph_len);  // Page 9
}

TEST_F(WebFrameTest, PrintAllPages) {
  RegisterMockedHttpURLLoad("print-pages.html");
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "print-pages.html");

  std::vector<TextRunDOMNodeIdInfo> text_runs =
      GetPrintedTextRunDOMNodeIds(web_view_helper.LocalMainFrame());
  EXPECT_EQ(10u, text_runs.size());
}

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

  std::vector<TextRunDOMNodeIdInfo> text_runs =
      GetPrintedTextRunDOMNodeIds(web_view_helper.LocalMainFrame());

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
  frame->ExecuteScript(
      WebScriptSource(WebString("document.execCommand('copy');")));
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
  WebView().MainFrameImpl()->SetScrollOffset(gfx::PointF(94, 111));
  WebView().SetVisualViewportOffset(gfx::PointF(12, 20));
  EXPECT_EQ(2.0f, WebView().PageScaleFactor());
  EXPECT_EQ(94, WebView().MainFrameImpl()->GetScrollOffset().x());
  EXPECT_EQ(111, WebView().MainFrameImpl()->GetScrollOffset().y());
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
  EXPECT_EQ(94, WebView().MainFrameImpl()->GetScrollOffset().x());
  EXPECT_EQ(111, WebView().MainFrameImpl()->GetScrollOffset().y());
  EXPECT_EQ(0, WebView().VisualViewportOffset().x());
  EXPECT_EQ(0, WebView().VisualViewportOffset().y());
}

TEST_F(WebFrameSimTest, PageSizeType) {
  gfx::Size page_size(500, 500);
  WebView().MainFrameViewWidget()->Resize(page_size);

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

  auto* frame = WebView().MainFrame()->ToWebLocalFrame();
  WebPrintParams print_params((gfx::SizeF(page_size)));
  frame->PrintBegin(print_params, WebNode());
  // Initially empty @page rule.
  EXPECT_EQ(PageSizeType::kAuto,
            main_frame->GetPageDescription(0).page_size_type);
  frame->PrintEnd();

  for (const auto& test : test_cases) {
    style_decl->setProperty(doc->GetExecutionContext(), "size", test.size, "",
                            ASSERT_NO_EXCEPTION);
    frame->PrintBegin(print_params, WebNode());
    EXPECT_EQ(test.page_size_type,
              main_frame->GetPageDescription(0).page_size_type);
    frame->PrintEnd();
  }
}

TEST_F(WebFrameSimTest, PageOrientation) {
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
  WebPrintParams print_params((gfx::SizeF(page_size)));
  EXPECT_EQ(4u, frame->PrintBegin(print_params, WebNode()));

  WebPrintPageDescription description = frame->GetPageDescription(0);
  EXPECT_EQ(description.orientation, PageOrientation::kUpright);

  description = frame->GetPageDescription(1);
  EXPECT_EQ(description.orientation, PageOrientation::kRotateLeft);

  description = frame->GetPageDescription(2);
  EXPECT_EQ(description.orientation, PageOrientation::kRotateRight);

  description = frame->GetPageDescription(3);
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
  TestViewportIntersection remote_frame_host;
  WebRemoteFrame* remote_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(
      MainFrame().FirstChild(), remote_frame,
      remote_frame_host.BindNewAssociatedRemote());
  Compositor().BeginFrame();
  RunPendingTasks();
  EXPECT_TRUE(remote_frame_host.GetIntersectionState()
                  ->main_frame_transform.IsIdentityOrIntegerTranslation());
  EXPECT_EQ(gfx::Vector2dF(14.f, 7.f),
            remote_frame_host.GetIntersectionState()
                ->main_frame_transform.To2dTranslation());
  MainFrame().FirstChild()->Detach();
}

TEST_F(WebFrameTest, MediaQueriesInLocalFrameInsideRemote) {
  frame_test_helpers::WebViewHelper helper;
  helper.InitializeRemote();

  WebLocalFrameImpl* local_frame =
      helper.CreateLocalChild(*helper.RemoteMainFrame(), WebString(),
                              WebFrameOwnerProperties(), nullptr, nullptr);

  frame_test_helpers::TestWebFrameWidget* local_frame_widget =
      static_cast<frame_test_helpers::TestWebFrameWidget*>(
          local_frame->FrameWidgetImpl());
  display::ScreenInfos screen_infos(
      local_frame_widget->GetOriginalScreenInfo());
  screen_infos.mutable_current().is_monochrome = false;
  screen_infos.mutable_current().depth_per_component = 8;
  local_frame_widget->UpdateScreenInfo(screen_infos);

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
  WebLocalFrameImpl* local_frame =
      helper.CreateLocalChild(*helper.RemoteMainFrame(), "frameName");
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

  Element* target = local_frame->GetFrame()->GetDocument()->getElementById(
      AtomicString("target"));
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

  static_cast<WebFrameWidgetImpl*>(widget)->ApplyViewportIntersectionForTesting(
      blink::mojom::blink::ViewportIntersectionState::New(
          viewport_intersection, mainframe_intersection, viewport_intersection,
          occlusion_state, gfx::Size(), gfx::Point(), viewport_transform));

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
      ->MapToVisualRectInAncestorSpace(nullptr, rect);
  EXPECT_EQ(PhysicalRect(7, 0, 25, 24), rect);

  // Without the main frame overflow clip the rect should not be clipped and the
  // coordinates returned are the rects coordinates in the viewport space.
  PhysicalRect mainframe_rect(0, 0, 25, 35);
  local_frame->GetFrame()
      ->GetDocument()
      ->GetLayoutView()
      ->MapToVisualRectInAncestorSpace(nullptr, mainframe_rect,
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
                             bool is_richly_editable_element,
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

  {
    // 1.<meta name='referrer' content='no-referrer'>
    MockPolicyContainerHost policy_container_host;
    frame->GetFrame()->DomWindow()->SetPolicyContainer(
        std::make_unique<PolicyContainer>(
            policy_container_host.BindNewEndpointAndPassDedicatedRemote(),
            mojom::blink::PolicyContainerPolicies::New()));
    EXPECT_CALL(policy_container_host,
                SetReferrerPolicy(network::mojom::ReferrerPolicy::kNever));
    frame_test_helpers::LoadHTMLString(
        frame, GetHTMLStringForReferrerPolicy("no-referrer", std::string()),
        test_url);
    EXPECT_TRUE(frame_host.referrer_.IsEmpty());
    EXPECT_EQ(frame_host.referrer_policy_,
              network::mojom::ReferrerPolicy::kNever);
    policy_container_host.FlushForTesting();
  }

  {
    // 2.<meta name='referrer' content='origin'>
    MockPolicyContainerHost policy_container_host;
    frame->GetFrame()->DomWindow()->SetPolicyContainer(
        std::make_unique<PolicyContainer>(
            policy_container_host.BindNewEndpointAndPassDedicatedRemote(),
            mojom::blink::PolicyContainerPolicies::New()));
    EXPECT_CALL(policy_container_host,
                SetReferrerPolicy(network::mojom::ReferrerPolicy::kOrigin));
    frame_test_helpers::LoadHTMLString(
        frame, GetHTMLStringForReferrerPolicy("origin", std::string()),
        test_url);
    EXPECT_EQ(frame_host.referrer_, ToKURL("http://www.test.com/"));
    EXPECT_EQ(frame_host.referrer_policy_,
              network::mojom::ReferrerPolicy::kOrigin);
    policy_container_host.FlushForTesting();
  }

  {
    // 3.Without any declared referrer-policy attribute
    MockPolicyContainerHost policy_container_host;
    frame->GetFrame()->DomWindow()->SetPolicyContainer(
        std::make_unique<PolicyContainer>(
            policy_container_host.BindNewEndpointAndPassDedicatedRemote(),
            mojom::blink::PolicyContainerPolicies::New()));
    EXPECT_CALL(policy_container_host, SetReferrerPolicy(_)).Times(0);
    frame_test_helpers::LoadHTMLString(
        frame, GetHTMLStringForReferrerPolicy(std::string(), std::string()),
        test_url);
    EXPECT_EQ(frame_host.referrer_, test_url);
    EXPECT_EQ(frame_host.referrer_policy_,
              ReferrerUtils::MojoReferrerPolicyResolveDefault(
                  network::mojom::ReferrerPolicy::kDefault));
    policy_container_host.FlushForTesting();
  }

  {
    // 4.referrerpolicy='origin'
    MockPolicyContainerHost policy_container_host;
    frame->GetFrame()->DomWindow()->SetPolicyContainer(
        std::make_unique<PolicyContainer>(
            policy_container_host.BindNewEndpointAndPassDedicatedRemote(),
            mojom::blink::PolicyContainerPolicies::New()));
    EXPECT_CALL(policy_container_host, SetReferrerPolicy(_)).Times(0);
    frame_test_helpers::LoadHTMLString(
        frame, GetHTMLStringForReferrerPolicy(std::string(), "origin"),
        test_url);
    EXPECT_EQ(frame_host.referrer_, ToKURL("http://www.test.com/"));
    EXPECT_EQ(frame_host.referrer_policy_,
              network::mojom::ReferrerPolicy::kOrigin);
    policy_container_host.FlushForTesting();
  }

  {
    // 5.referrerpolicy='same-origin'
    MockPolicyContainerHost policy_container_host;
    frame->GetFrame()->DomWindow()->SetPolicyContainer(
        std::make_unique<PolicyContainer>(
            policy_container_host.BindNewEndpointAndPassDedicatedRemote(),
            mojom::blink::PolicyContainerPolicies::New()));
    EXPECT_CALL(policy_container_host, SetReferrerPolicy(_)).Times(0);
    frame_test_helpers::LoadHTMLString(
        frame, GetHTMLStringForReferrerPolicy(std::string(), "same-origin"),
        test_url);
    EXPECT_EQ(frame_host.referrer_, test_url);
    EXPECT_EQ(frame_host.referrer_policy_,
              network::mojom::ReferrerPolicy::kSameOrigin);
    policy_container_host.FlushForTesting();
  }

  {
    // 6.referrerpolicy='no-referrer'
    MockPolicyContainerHost policy_container_host;
    frame->GetFrame()->DomWindow()->SetPolicyContainer(
        std::make_unique<PolicyContainer>(
            policy_container_host.BindNewEndpointAndPassDedicatedRemote(),
            mojom::blink::PolicyContainerPolicies::New()));
    EXPECT_CALL(policy_container_host, SetReferrerPolicy(_)).Times(0);
    frame_test_helpers::LoadHTMLString(
        frame, GetHTMLStringForReferrerPolicy(std::string(), "no-referrer"),
        test_url);
    EXPECT_TRUE(frame_host.referrer_.IsEmpty());
    EXPECT_EQ(frame_host.referrer_policy_,
              network::mojom::ReferrerPolicy::kNever);
    policy_container_host.FlushForTesting();
  }

  web_view_helper.Reset();
}

TEST_F(WebFrameTest, RemoteFrameCompositingScaleFactor) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();

  WebViewImpl* web_view = web_view_helper.GetWebView();
  web_view->Resize(gfx::Size(800, 800));
  InitializeWithHTML(*web_view->MainFrameImpl()->GetFrame(), R"HTML(
      <!DOCTYPE html>
      <style>
        iframe {
          width: 1600px;
          height: 1200px;
          transform-origin: top left;
          transform: scale(0.5);
          border: none;
        }
      </style>
      <iframe></iframe>
  )HTML");

  WebRemoteFrameImpl* remote_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(
      web_view_helper.LocalMainFrame()->FirstChild(), remote_frame);
  remote_frame->SetReplicatedOrigin(
      WebSecurityOrigin(SecurityOrigin::CreateUniqueOpaque()), false);

  // Call directly into frame view since we need to RunPostLifecycleSteps() too.
  web_view->MainFrameImpl()
      ->GetFrame()
      ->View()
      ->UpdateAllLifecyclePhasesForTest();
  RunPendingTasks();

  // The compositing scale factor tells the OOPIF compositor to raster at a
  // lower scale since the frame is scaled down in the parent webview.
  EXPECT_EQ(remote_frame->GetCompositingRect(), gfx::Rect(0, 0, 1600, 1200));
  EXPECT_EQ(remote_frame->GetFrame()->View()->GetCompositingScaleFactor(),
            0.5f);
}

TEST_F(WebFrameTest, RotatedRemoteFrameCompositingScaleFactor) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();

  WebViewImpl* web_view = web_view_helper.GetWebView();
  web_view->Resize(gfx::Size(800, 800));
  InitializeWithHTML(*web_view->MainFrameImpl()->GetFrame(), R"HTML(
      <!DOCTYPE html>
      <style>
        iframe {
          width: 1600px;
          height: 1200px;
          transform-origin: top left;
          transform: scale(0.5) rotate(45deg);
          border: none;
        }
      </style>
      <iframe></iframe>
  )HTML");

  WebRemoteFrameImpl* remote_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(
      web_view_helper.LocalMainFrame()->FirstChild(), remote_frame);
  remote_frame->SetReplicatedOrigin(
      WebSecurityOrigin(SecurityOrigin::CreateUniqueOpaque()), false);

  // Call directly into frame view since we need to RunPostLifecycleSteps() too.
  web_view->MainFrameImpl()
      ->GetFrame()
      ->View()
      ->UpdateAllLifecyclePhasesForTest();
  RunPendingTasks();

  // The compositing scale factor tells the OOPIF compositor to raster at a
  // lower scale since the frame is scaled down in the parent webview.
  EXPECT_EQ(remote_frame->GetCompositingRect(), gfx::Rect(0, 0, 1600, 1200));
  EXPECT_EQ(remote_frame->GetFrame()->View()->GetCompositingScaleFactor(),
            0.5f);
}

TEST_F(WebFrameTest, ZeroScaleRemoteFrameCompositingScaleFactor) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();

  WebViewImpl* web_view = web_view_helper.GetWebView();
  web_view->Resize(gfx::Size(800, 800));
  InitializeWithHTML(*web_view->MainFrameImpl()->GetFrame(), R"HTML(
      <!DOCTYPE html>
      <style>
        iframe {
          width: 1600px;
          height: 1200px;
          transform-origin: top left;
          transform: scale(0);
          border: none;
        }
      </style>
      <iframe></iframe>
  )HTML");

  WebRemoteFrameImpl* remote_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(
      web_view_helper.LocalMainFrame()->FirstChild(), remote_frame);
  remote_frame->SetReplicatedOrigin(
      WebSecurityOrigin(SecurityOrigin::CreateUniqueOpaque()), false);

  // Call directly into frame view since we need to RunPostLifecycleSteps() too.
  web_view->MainFrameImpl()
      ->GetFrame()
      ->View()
      ->UpdateAllLifecyclePhasesForTest();
  RunPendingTasks();

  // The compositing scale factor tells the OOPIF compositor to raster at a
  // reasonable minimum scale even though the iframe's transform scale is zero.
  EXPECT_EQ(remote_frame->GetFrame()->View()->GetCompositingScaleFactor(),
            0.25f);
}

TEST_F(WebFrameTest, LargeScaleRemoteFrameCompositingScaleFactor) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();

  WebViewImpl* web_view = web_view_helper.GetWebView();
  web_view->Resize(gfx::Size(800, 800));
  InitializeWithHTML(*web_view->MainFrameImpl()->GetFrame(), R"HTML(
      <!DOCTYPE html>
      <style>
        iframe {
          width: 1600px;
          height: 1200px;
          transform-origin: top left;
          transform: scale(10.0);
          border: none;
        }
      </style>
      <iframe></iframe>
  )HTML");

  WebRemoteFrameImpl* remote_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(
      web_view_helper.LocalMainFrame()->FirstChild(), remote_frame);
  remote_frame->SetReplicatedOrigin(
      WebSecurityOrigin(SecurityOrigin::CreateUniqueOpaque()), false);

  // Call directly into frame view since we need to RunPostLifecycleSteps() too.
  web_view->MainFrameImpl()
      ->GetFrame()
      ->View()
      ->UpdateAllLifecyclePhasesForTest();
  RunPendingTasks();

  // The compositing scale factor is at most 5.0 irrespective of iframe scale.
  EXPECT_EQ(remote_frame->GetFrame()->View()->GetCompositingScaleFactor(),
            5.0f);
}

TEST_F(WebFrameTest, VerticalRLScrollOffset) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();

  WebViewImpl* web_view = web_view_helper.GetWebView();
  web_view->Resize(gfx::Size(800, 800));
  auto* frame = web_view->MainFrameImpl()->GetFrame();
  InitializeWithHTML(*frame, R"HTML(
    <!DOCTYPE html>
    <style>body { margin: 0; }</style>
    <div style="width: 2000px; height: 2000px"></div>
  )HTML");

  frame->GetDocument()->documentElement()->setAttribute(
      html_names::kStyleAttr, AtomicString("writing-mode: vertical-rl"));
  frame->View()->UpdateAllLifecyclePhasesForTest();

  auto* web_main_frame = web_view_helper.LocalMainFrame();
  EXPECT_EQ(gfx::PointF(1200, 0), web_main_frame->GetScrollOffset());
  web_main_frame->SetScrollOffset(gfx::PointF(-100, 100));
  EXPECT_EQ(gfx::PointF(0, 100), web_main_frame->GetScrollOffset());
}

TEST_F(WebFrameTest, FrameOwnerColorScheme) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      "data:text/html,<frameset><frame id=frame></frame></frameset>");

  WebViewImpl* web_view = web_view_helper.GetWebView();

  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();
  HTMLFrameOwnerElement* frame = To<HTMLFrameOwnerElement>(
      document->getElementById(AtomicString("frame")));
  EXPECT_EQ(frame->GetColorScheme(), mojom::blink::ColorScheme::kLight);
  EXPECT_EQ(frame->contentDocument()->GetStyleEngine().GetOwnerColorScheme(),
            mojom::blink::ColorScheme::kLight);

  frame->SetInlineStyleProperty(CSSPropertyID::kColorScheme, "dark");
  EXPECT_EQ(frame->GetColorScheme(), mojom::blink::ColorScheme::kLight);

  UpdateAllLifecyclePhases(web_view);
  EXPECT_EQ(frame->GetColorScheme(), mojom::blink::ColorScheme::kDark);
  EXPECT_EQ(frame->contentDocument()->GetStyleEngine().GetOwnerColorScheme(),
            mojom::blink::ColorScheme::kDark);
}

TEST_F(WebFrameSimTest, RenderBlockingPromotesResource) {
  SimRequest main_request("https://example.com/", "text/html");
  SimSubresourceRequest script_request("https://example.com/script.js",
                                       "text/javascript");

  LoadURL("https://example.com/");
  main_request.Write(R"HTML(
    <!doctype html>
    <script defer fetchpriority="low" src="script.js"></script>
  )HTML");

  Resource* script = GetDocument().Fetcher()->AllResources().at(
      ToKURL("https://example.com/script.js"));

  // Script is fetched at the low priority due to `fetchpriority="low"`.
  ASSERT_TRUE(script);
  EXPECT_EQ(ResourceLoadPriority::kLow,
            script->GetResourceRequest().Priority());

  main_request.Complete(R"HTML(
    <script defer fetchpriority="low" blocking="render" src="script.js"></script>
  )HTML");

  // `blocking=render` promotes the priority to high.
  EXPECT_EQ(ResourceLoadPriority::kHigh,
            script->GetResourceRequest().Priority());

  script_request.Complete();
}

// Verify that modified_runtime_features is correctly set in the
// RuntimeFeatureStateOverrideContext when a navigation is committed.
TEST_F(WebFrameSimTest, SetModifiedFeaturesInOverrideContext) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();

  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();

  auto params = std::make_unique<WebNavigationParams>();
  // The url isn't important, just pick something.
  params->url = url_test_helpers::ToKURL("http://www.example.com");

  // Create a modified features value map and give it a value that we can check.
  auto modified_features =
      base::flat_map<::blink::mojom::RuntimeFeature, bool>();
  modified_features[blink::mojom::RuntimeFeature::kTestFeature] = true;
  params->modified_runtime_features = modified_features;

  // Commit the navigation
  frame->CommitNavigation(std::move(params), nullptr);

  // Get the override context and compare the override values map with the
  // modified features map.
  RuntimeFeatureStateOverrideContext* override_context =
      frame->GetFrame()->DomWindow()->GetRuntimeFeatureStateOverrideContext();
  EXPECT_EQ(override_context->GetOverrideValuesForTesting(), modified_features);

  // Do the same thing for a value of "false"
  params = std::make_unique<WebNavigationParams>();
  params->url = url_test_helpers::ToKURL("http://www.example2.com");
  modified_features = base::flat_map<::blink::mojom::RuntimeFeature, bool>();
  modified_features[blink::mojom::RuntimeFeature::kTestFeature] = false;
  params->modified_runtime_features = modified_features;
  frame->CommitNavigation(std::move(params), nullptr);
  override_context =
      frame->GetFrame()->DomWindow()->GetRuntimeFeatureStateOverrideContext();
  EXPECT_EQ(override_context->GetOverrideValuesForTesting(), modified_features);
}

TEST_F(WebFrameTest, IframeMoveBeforeConnectedSubframeCount) {
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();

  WebViewImpl* web_view = web_view_helper.GetWebView();
  web_view->Resize(gfx::Size(800, 800));
  auto* frame = web_view->MainFrameImpl()->GetFrame();
  InitializeWithHTML(*frame, R"HTML(
    <!DOCTYPE html>
    <body>
      <div id=oldParent><iframe></iframe></div>
      <div id=newParent></div>
    </body>
  )HTML");

  frame->View()->UpdateAllLifecyclePhasesForTest();

  Element* body = frame->GetDocument()->body();
  Element* iframe = frame->GetDocument()->QuerySelector(AtomicString("iframe"));
  Element* old_parent =
      frame->GetDocument()->getElementById(AtomicString("oldParent"));
  Element* new_parent =
      frame->GetDocument()->getElementById(AtomicString("newParent"));

  EXPECT_EQ(body->ConnectedSubframeCount(), 1u);
  EXPECT_EQ(old_parent->ConnectedSubframeCount(), 1u);
  EXPECT_EQ(new_parent->ConnectedSubframeCount(), 0u);

  new_parent->moveBefore(iframe, nullptr, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(body->ConnectedSubframeCount(), 1u);
  EXPECT_EQ(old_parent->ConnectedSubframeCount(), 0u);
  EXPECT_EQ(new_parent->ConnectedSubframeCount(), 1u);
}

}  // namespace blink
