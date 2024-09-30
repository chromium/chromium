/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

// How ownership works
// -------------------
//
// Big oh represents a refcounted relationship: owner O--- ownee
//
// WebView (for the toplevel frame only)
//    O
//    |           WebFrame
//    |              O
//    |              |
//   Page O------- LocalFrame (main_frame_) O-------O LocalFrameView
//                   ||
//                   ||
//               FrameLoader
//
// FrameLoader and LocalFrame are formerly one object that was split apart
// because it got too big. They basically have the same lifetime, hence the
// double line.
//
// From the perspective of the embedder, WebFrame is simply an object that it
// allocates by calling WebFrame::create() and must be freed by calling close().
// Internally, WebFrame is actually refcounted and it holds a reference to its
// corresponding LocalFrame in blink.
//
// Oilpan: the middle objects + Page in the above diagram are Oilpan heap
// allocated, WebView and LocalFrameView are currently not. In terms of
// ownership and control, the relationships stays the same, but the references
// from the off-heap WebView to the on-heap Page is handled by a Persistent<>,
// not a scoped_refptr<>. Similarly, the mutual strong references between the
// on-heap LocalFrame and the off-heap LocalFrameView is through a RefPtr (from
// LocalFrame to LocalFrameView), and a Persistent refers to the LocalFrame in
// the other direction.
//
// From the embedder's point of view, the use of Oilpan brings no changes.
// close() must still be used to signal that the embedder is through with the
// WebFrame.  Calling it will bring about the release and finalization of the
// frame object, and everything underneath.
//
// How frames are destroyed
// ------------------------
//
// The main frame is never destroyed and is re-used. The FrameLoader is re-used
// and a reference to the main frame is kept by the Page.
//
// When frame content is replaced, all subframes are destroyed. This happens
// in Frame::detachChildren for each subframe in a pre-order depth-first
// traversal. Note that child node order may not match DOM node order!
// detachChildren() (virtually) calls Frame::detach(), which again calls
// LocalFrameClient::detached(). This triggers WebFrame to clear its reference
// to LocalFrame. LocalFrameClient::detached() also notifies the embedder via
// WebLocalFrameClient that the frame is detached. Most embedders will invoke
// close() on the WebFrame at this point, triggering its deletion unless
// something else is still retaining a reference.
//
// The client is expected to be set whenever the WebLocalFrameImpl is attached
// to the DOM.

#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numeric>
#include <utility>

#include "base/compiler_specific.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_params_builder.h"
#include "third_party/blink/public/common/frame/fenced_frame_sandbox_flags.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom-blink.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/media_player_action.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/tree_scope_type.mojom-blink.h"
#include "third_party/blink/public/mojom/lcp_critical_path_predictor/lcp_critical_path_predictor.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_isolated_world_info.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_content_capture_client.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_frame_owner_properties.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_manifest_manager.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_performance_metrics_for_nested_contexts.h"
#include "third_party/blink/public/web/web_performance_metrics_for_reporting.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_print_client.h"
#include "third_party/blink/public/web/web_print_page_description.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/public/web/web_print_preset_options.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/isolated_world_csp.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_utilities.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/add_event_listener_options_resolved.h"
#include "third_party/blink/renderer/core/dom/icon_url.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_in_page_coordinates.h"
#include "third_party/blink/renderer/core/editing/finder/text_finder.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/edit_context.h"
#include "third_party/blink/renderer/core/editing/ime/ime_text_span_vector_builder.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/plain_text_range.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/events/after_print_event.h"
#include "third_party/blink/renderer/core/events/before_print_event.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/execution_context/window_agent.h"
#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/find_in_page.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/intervention.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_client_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/pausable_script_executor.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_owner.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/smart_clip.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_remote_frame_impl.h"
#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/plugin_document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_conversion.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/lcp_critical_path_predictor/lcp_critical_path_predictor.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/history_item.h"
#include "third_party/blink/renderer/core/loader/web_associated_url_loader_impl.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/print_context.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/ignore_paint_timing_scope.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_factory.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "ui/gfx/geometry/size_conversions.h"

#if BUILDFLAG(IS_WIN)
#include "third_party/blink/public/web/win/web_font_family_names.h"
#include "third_party/blink/renderer/core/layout/layout_font_accessor_win.h"
#endif

namespace blink {

namespace {

int g_frame_count = 0;

class DummyFrameOwner final : public GarbageCollected<DummyFrameOwner>,
                              public FrameOwner {
 public:
  void Trace(Visitor* visitor) const override { FrameOwner::Trace(visitor); }

  // FrameOwner overrides:
  Frame* ContentFrame() const override { return nullptr; }
  void SetContentFrame(Frame&) override {}
  void ClearContentFrame() override {}
  const FramePolicy& GetFramePolicy() const override {
    DEFINE_STATIC_LOCAL(FramePolicy, frame_policy, ());
    return frame_policy;
  }
  void AddResourceTiming(mojom::blink::ResourceTimingInfoPtr) override {}
  void DispatchLoad() override {}
  void IntrinsicSizingInfoChanged() override {}
  void SetNeedsOcclusionTracking(bool) override {}
  AtomicString BrowsingContextContainerName() const override {
    return AtomicString();
  }
  mojom::blink::ScrollbarMode ScrollbarMode() const override {
    return mojom::blink::ScrollbarMode::kAuto;
  }
  int MarginWidth() const override { return -1; }
  int MarginHeight() const override { return -1; }
  bool AllowFullscreen() const override { return false; }
  bool AllowPaymentRequest() const override { return false; }
  bool IsDisplayNone() const override { return false; }
  mojom::blink::ColorScheme GetColorScheme() const override {
    return mojom::blink::ColorScheme::kLight;
  }
  mojom::blink::PreferredColorScheme GetPreferredColorScheme() const override {
    return mojom::blink::PreferredColorScheme::kLight;
  }
  bool ShouldLazyLoadChildren() const override { return false; }

 private:
  // Intentionally private to prevent redundant checks when the type is
  // already DummyFrameOwner.
  bool IsLocal() const override { return false; }
  bool IsRemote() const override { return false; }
};

}  // namespace

// Simple class to override some of PrintContext behavior. Some of the methods
// made virtual so that they can be overridden by ChromePluginPrintContext.
class ChromePrintContext : public PrintContext {
 public:
  explicit ChromePrintContext(LocalFrame* frame) : PrintContext(frame) {}
  ChromePrintContext(const ChromePrintContext&) = delete;
  ChromePrintContext& operator=(const ChromePrintContext&) = delete;

  ~ChromePrintContext() override = default;

  virtual WebPrintPageDescription GetPageDescription(uint32_t page_index) {
    return GetFrame()->GetDocument()->GetPageDescription(page_index);
  }

  void SpoolSinglePage(cc::PaintCanvas* canvas, wtf_size_t page_index) {
    // The page rect gets scaled and translated, so specify the entire
    // print content area here as the recording rect.
    PaintRecordBuilder builder;
    GraphicsContext& context = builder.Context();
    context.SetPrintingMetafile(canvas->GetPrintingMetafile());
    context.SetPrinting(true);
    context.BeginRecording();
    SpoolPage(context, page_index);
    canvas->drawPicture(context.EndRecording());
  }

  void SpoolPagesWithBoundariesForTesting(cc::PaintCanvas* canvas,
                                          const gfx::Size& spool_size_in_pixels,
                                          const WebVector<uint32_t>* pages) {
    gfx::Rect all_pages_rect(spool_size_in_pixels);

    PaintRecordBuilder builder;
    GraphicsContext& context = builder.Context();
    context.SetPrintingMetafile(canvas->GetPrintingMetafile());
    context.SetPrinting(true);
    context.BeginRecording();

    // Fill the whole background by white.
    context.FillRect(all_pages_rect, Color::kWhite, AutoDarkMode::Disabled());

    WebVector<uint32_t> all_pages;
    if (!pages) {
      all_pages.reserve(PageCount());
      all_pages.resize(PageCount());
      std::iota(all_pages.begin(), all_pages.end(), 0);
      pages = &all_pages;
    }

    int current_height = 0;
    for (uint32_t page_index : *pages) {
      if (page_index >= PageCount()) {
        break;
      }

      // Draw a line for a page boundary if this isn't the first page.
      if (page_index != pages->front()) {
        const gfx::Rect boundary_line_rect(0, current_height - 1,
                                           spool_size_in_pixels.width(), 1);
        context.FillRect(boundary_line_rect, Color(0, 0, 255),
                         AutoDarkMode::Disabled());
      }

      WebPrintPageDescription description =
          GetFrame()->GetDocument()->GetPageDescription(page_index);

      AffineTransform transform;
      transform.Translate(0, current_height);

      if (description.orientation == PageOrientation::kUpright) {
        current_height += description.size.height() + 1;
      } else {
        if (description.orientation == PageOrientation::kRotateRight) {
          transform.Translate(description.size.height(), 0);
          transform.Rotate(90);
        } else {
          DCHECK_EQ(description.orientation, PageOrientation::kRotateLeft);
          transform.Translate(0, description.size.width());
          transform.Rotate(-90);
        }
        current_height += description.size.width() + 1;
      }

      context.Save();
      context.ConcatCTM(transform);

      SpoolPage(context, page_index);

      context.Restore();
    }

    canvas->drawPicture(context.EndRecording());
  }

 protected:
  virtual void SpoolPage(GraphicsContext& context, wtf_size_t page_index) {
    DispatchEventsForPrintingOnAllFrames();
    if (!IsFrameValid()) {
      return;
    }

    auto* frame_view = GetFrame()->View();
    DCHECK(frame_view);
    frame_view->UpdateLifecyclePhasesForPrinting();

    if (!IsFrameValid() || page_index >= PageCount()) {
      // TODO(crbug.com/452672): The number of pages may change after layout for
      // pagination.
      return;
    }
    gfx::Rect page_rect = PageRect(page_index);

    // Cancel out the scroll offset used in screen mode.
    gfx::Vector2d offset = frame_view->LayoutViewport()->ScrollOffsetInt();
    context.Save();
    context.Translate(static_cast<float>(offset.x()),
                      static_cast<float>(offset.y()));

    const LayoutView* layout_view = frame_view->GetLayoutView();

    PaintRecordBuilder builder(context);

    frame_view->PrintPage(builder.Context(), page_index, CullRect(page_rect));

    auto property_tree_state =
        layout_view->FirstFragment().LocalBorderBoxProperties();
    OutputLinkedDestinations(builder.Context(), property_tree_state, page_rect);
    context.DrawRecord(builder.EndRecording(property_tree_state.Unalias()));
    context.Restore();
  }

 private:
  void DispatchEventsForPrintingOnAllFrames() {
    HeapVector<Member<Document>> documents;
    for (Frame* current_frame = GetFrame(); current_frame;
         current_frame = current_frame->Tree().TraverseNext(GetFrame())) {
      if (auto* current_local_frame = DynamicTo<LocalFrame>(current_frame))
        documents.push_back(current_local_frame->GetDocument());
    }

    for (auto& doc : documents)
      doc->DispatchEventsForPrinting();
  }
};

// Simple class to override some of PrintContext behavior. This is used when
// the frame hosts a plugin that supports custom printing. In this case, we
// want to delegate all printing related calls to the plugin.
class ChromePluginPrintContext final : public ChromePrintContext {
 public:
  ChromePluginPrintContext(LocalFrame* frame, WebPluginContainerImpl* plugin)
      : ChromePrintContext(frame), plugin_(plugin) {}

  ~ChromePluginPrintContext() override = default;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(plugin_);
    ChromePrintContext::Trace(visitor);
  }

  const gfx::Rect& PageRect(wtf_size_t) const = delete;

  WebPrintPageDescription GetPageDescription(uint32_t page_index) override {
    // Plug-ins aren't really able to provide any page description apart from
    // the "default" one. Yet, the printing code calls this function for
    // plug-ins, which isn't ideal, but something we have to cope with for now.
    return default_page_description_;
  }

  wtf_size_t PageCount() const override { return page_count_; }

  void BeginPrintMode(const WebPrintParams& print_params) override {
    default_page_description_ = print_params.default_page_description;
    page_count_ = plugin_->PrintBegin(print_params);
  }

  void EndPrintMode() override {
    plugin_->PrintEnd();
    // TODO(junov): The following should not be necessary because
    // the document's printing state does not need to be set when printing
    // via a plugin. The problem is that WebLocalFrameImpl::DispatchBeforePrint
    // modifies this state regardless of whether a plug-in is being used.
    // This code should be refactored so that the print_context_ is in scope
    // when  beforeprint/afterprint events are dispatched So that plug-in
    // behavior can be differentiated. Also, should beforeprint/afterprint
    // events even be dispatched when using a plug-in?
    if (IsFrameValid())
      GetFrame()->GetDocument()->SetPrinting(Document::kNotPrinting);
  }

 protected:
  void SpoolPage(GraphicsContext& context, wtf_size_t page_index) override {
    PaintRecordBuilder builder(context);
    plugin_->PrintPage(page_index, builder.Context());
    context.DrawRecord(builder.EndRecording());
  }

 private:
  // Set when printing.
  Member<WebPluginContainerImpl> plugin_;

  WebPrintPageDescription default_page_description_;

  wtf_size_t page_count_ = 0;
};

class PaintPreviewContext : public PrintContext {
 public:
  explicit PaintPreviewContext(LocalFrame* frame) : PrintContext(frame) {
    use_paginated_layout_ = false;
  }
  PaintPreviewContext(const PaintPreviewContext&) = delete;
  PaintPreviewContext& operator=(const PaintPreviewContext&) = delete;
  ~PaintPreviewContext() override = default;

  bool Capture(cc::PaintCanvas* canvas,
               const gfx::Rect& bounds,
               bool include_linked_destinations) {
    // This code is based on ChromePrintContext::SpoolSinglePage()/SpoolPage().
    // It differs in that it:
    //   1. Uses a different set of flags for painting and the graphics context.
    //   2. Paints a single "page" of `bounds` size without applying print
    //   modifications to the page.
    //   3. Does no scaling.
    if (!GetFrame()->GetDocument() ||
        !GetFrame()->GetDocument()->GetLayoutView())
      return false;
    GetFrame()->View()->UpdateLifecyclePhasesForPrinting();
    if (!GetFrame()->GetDocument() ||
        !GetFrame()->GetDocument()->GetLayoutView())
      return false;
    PaintRecordBuilder builder;
    builder.Context().SetPaintPreviewTracker(canvas->GetPaintPreviewTracker());

    LocalFrameView* frame_view = GetFrame()->View();
    DCHECK(frame_view);

    // This calls BeginRecording on |builder| with dimensions specified by the
    // CullRect.
    PaintFlags flags = PaintFlag::kOmitCompositingInfo;
    if (include_linked_destinations)
      flags |= PaintFlag::kAddUrlMetadata;

    frame_view->PaintOutsideOfLifecycle(builder.Context(), flags,
                                        CullRect(bounds));
    PropertyTreeStateOrAlias property_tree_state =
        frame_view->GetLayoutView()->FirstFragment().ContentsProperties();
    if (include_linked_destinations) {
      OutputLinkedDestinations(builder.Context(), property_tree_state, bounds);
    }
    canvas->drawPicture(builder.EndRecording(property_tree_state.Unalias()));
    return true;
  }
};

// Android WebView requires hit testing results on every touch event. This
// pushes the hit test result to the callback that is registered.
class TouchStartEventListener : public NativeEventListener {
 public:
  explicit TouchStartEventListener(
      base::RepeatingCallback<void(const blink::WebHitTestResult&)> callback)
      : callback_(std::move(callback)) {}

  void Invoke(ExecutionContext*, Event* event) override {
    auto* touch_event = DynamicTo<TouchEvent>(event);
    if (!touch_event)
      return;
    const auto* native_event = touch_event->NativeEvent();
    if (!native_event)
      return;

    DCHECK_EQ(WebInputEvent::Type::kTouchStart,
              native_event->Event().GetType());
    const auto& web_touch_event =
        static_cast<const WebTouchEvent&>(native_event->Event());

    if (web_touch_event.touches_length != 1u)
      return;

    LocalDOMWindow* dom_window = event->currentTarget()->ToLocalDOMWindow();
    CHECK(dom_window);

    WebGestureEvent tap_event(
        WebInputEvent::Type::kGestureTap, WebInputEvent::kNoModifiers,
        base::TimeTicks::Now(), WebGestureDevice::kTouchscreen);
    // GestureTap is only ever from a touchscreen.
    tap_event.SetPositionInWidget(
        web_touch_event.touches[0].PositionInWidget());
    tap_event.SetPositionInScreen(
        web_touch_event.touches[0].PositionInScreen());
    tap_event.SetFrameScale(web_touch_event.FrameScale());
    tap_event.SetFrameTranslate(web_touch_event.FrameTranslate());
    tap_event.data.tap.tap_count = 1;
    tap_event.data.tap.height = tap_event.data.tap.width =
        std::max(web_touch_event.touches[0].radius_x,
                 web_touch_event.touches[0].radius_y);

    HitTestResult result =
        dom_window->GetFrame()
            ->GetEventHandler()
            .HitTestResultForGestureEvent(
                tap_event, HitTestRequest::kReadOnly | HitTestRequest::kActive)
            .GetHitTestResult();

    result.SetToShadowHostIfInUAShadowRoot();

    callback_.Run(result);
  }

 private:
  base::RepeatingCallback<void(const blink::WebHitTestResult&)> callback_;
};

// WebFrame -------------------------------------------------------------------

static CreateWebFrameWidgetCallback* g_create_web_frame_widget = nullptr;

void InstallCreateWebFrameWidgetHook(
    CreateWebFrameWidgetCallback* create_widget) {
  // This DCHECK's aims to avoid unexpected replacement of the hook.
  DCHECK(!g_create_web_frame_widget || !create_widget);
  g_create_web_frame_widget = create_widget;
}

WebFrameWidget* WebLocalFrame::InitializeFrameWidget(
    CrossVariantMojoAssociatedRemote<mojom::blink::FrameWidgetHostInterfaceBase>
        mojo_frame_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
        mojo_frame_widget,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        mojo_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        mojo_widget,
    const viz::FrameSinkId& frame_sink_id,
    bool is_for_nested_main_frame,
    bool is_for_scalable_page,
    bool hidden) {
  CreateFrameWidgetInternal(
      base::PassKey<WebLocalFrame>(), std::move(mojo_frame_widget_host),
      std::move(mojo_frame_widget), std::move(mojo_widget_host),
      std::move(mojo_widget), frame_sink_id, is_for_nested_main_frame,
      is_for_scalable_page, hidden);
  return FrameWidget();
}

int WebFrame::InstanceCount() {
  return g_frame_count;
}

// static
WebFrame* WebFrame::FromFrameToken(const FrameToken& frame_token) {
  auto* frame = Frame::ResolveFrame(frame_token);
  return WebFrame::FromCoreFrame(frame);
}

// static
WebLocalFrame* WebLocalFrame::FromFrameToken(
    const LocalFrameToken& frame_token) {
  auto* frame = LocalFrame::FromFrameToken(frame_token);
  return WebLocalFrameImpl::FromFrame(frame);
}

WebLocalFrame* WebLocalFrame::FrameForCurrentContext() {
  v8::Isolate* isolate = v8::Isolate::TryGetCurrent();
  if (!isolate) [[unlikely]] {
    return nullptr;
  }
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (context.IsEmpty())
    return nullptr;
  return FrameForContext(context);
}

void WebLocalFrameImpl::NotifyUserActivation(
    mojom::blink::UserActivationNotificationType notification_type) {
  LocalFrame::NotifyUserActivation(GetFrame(), notification_type);
}

bool WebLocalFrameImpl::HasStickyUserActivation() {
  return GetFrame()->HasStickyUserActivation();
}

bool WebLocalFrameImpl::HasTransientUserActivation() {
  return LocalFrame::HasTransientUserActivation(GetFrame());
}

bool WebLocalFrameImpl::ConsumeTransientUserActivation(
    UserActivationUpdateSource update_source) {
  return LocalFrame::ConsumeTransientUserActivation(GetFrame(), update_source);
}

bool WebLocalFrameImpl::LastActivationWasRestricted() const {
  return GetFrame()->LastActivationWasRestricted();
}

#if BUILDFLAG(IS_WIN)
WebFontFamilyNames WebLocalFrameImpl::GetWebFontFamilyNames() const {
  FontFamilyNames font_family_names;
  GetFontsUsedByFrame(*GetFrame(), font_family_names);
  WebFontFamilyNames result;
  for (const String& font_family_name : font_family_names.font_names) {
    result.font_names.push_back(font_family_name);
  }
  return result;
}
#endif

WebLocalFrame* WebLocalFrame::FrameForContext(v8::Local<v8::Context> context) {
  return WebLocalFrameImpl::FromFrame(ToLocalFrameIfNotDetached(context));
}

bool WebLocalFrameImpl::IsWebLocalFrame() const {
  return true;
}

WebLocalFrame* WebLocalFrameImpl::ToWebLocalFrame() {
  return this;
}

const WebLocalFrame* WebLocalFrameImpl::ToWebLocalFrame() const {
  return this;
}

bool WebLocalFrameImpl::IsWebRemoteFrame() const {
  return false;
}

WebRemoteFrame* WebLocalFrameImpl::ToWebRemoteFrame() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

const WebRemoteFrame* WebLocalFrameImpl::ToWebRemoteFrame() const {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void WebLocalFrameImpl::Close() {
  WebLocalFrame::Close();

  if (frame_widget_) {
    frame_widget_->Close();
    frame_widget_ = nullptr;
  }

  client_ = nullptr;

  if (dev_tools_agent_)
    dev_tools_agent_.Clear();

  self_keep_alive_.Clear();

  if (print_context_)
    PrintEnd();
  print_client_.reset();
#if DCHECK_IS_ON()
  is_in_printing_ = false;
#endif
}

WebString WebLocalFrameImpl::AssignedName() const {
  return GetFrame()->Tree().GetName();
}

ui::AXTreeID WebLocalFrameImpl::GetAXTreeID() const {
  const std::optional<base::UnguessableToken>& embedding_token =
      GetEmbeddingToken();
  if (embedding_token && !embedding_token->is_empty())
    return ui::AXTreeID::FromToken(embedding_token.value());
  return ui::AXTreeIDUnknown();
}

void WebLocalFrameImpl::SetName(const WebString& name) {
  GetFrame()->Tree().SetName(name, FrameTree::kReplicate);
}

WebContentSettingsClient* WebLocalFrameImpl::GetContentSettingsClient() const {
  return content_settings_client_;
}

void WebLocalFrameImpl::SetContentSettingsClient(
    WebContentSettingsClient* client) {
  content_settings_client_ = client;
}

ScrollableArea* WebLocalFrameImpl::LayoutViewport() const {
  if (LocalFrameView* view = GetFrameView())
    return view->LayoutViewport();
  return nullptr;
}

bool WebLocalFrameImpl::IsFocused() const {
  if (!ViewImpl() || !ViewImpl()->GetPage())
    return false;

  return this ==
         WebFrame::FromCoreFrame(
             ViewImpl()->GetPage()->GetFocusController().FocusedFrame());
}

bool WebLocalFrameImpl::DispatchedPagehideAndStillHidden() const {
  // Dispatching pagehide is the first step in unloading, so we must have
  // already dispatched pagehide if unload had started.
  if (GetFrame() && GetFrame()->GetDocument() &&
      GetFrame()->GetDocument()->UnloadStarted()) {
    return true;
  }
  if (!ViewImpl() || !ViewImpl()->GetPage())
    return false;
  // We might have dispatched pagehide without unloading the document.
  return ViewImpl()->GetPage()->DispatchedPagehideAndStillHidden();
}

void WebLocalFrameImpl::CopyToFindPboard() {
#if BUILDFLAG(IS_MAC)
  if (HasSelection())
    GetFrame()->GetSystemClipboard()->CopyToFindPboard(SelectionAsText());
#endif
}

void WebLocalFrameImpl::CenterSelection() {
  if (HasSelection()) {
    GetFrame()->Selection().RevealSelection(ScrollAlignment::CenterAlways());
  }
}

gfx::PointF WebLocalFrameImpl::GetScrollOffset() const {
  if (ScrollableArea* scrollable_area = LayoutViewport())
    return scrollable_area->ScrollPosition();
  return gfx::PointF();
}

void WebLocalFrameImpl::SetScrollOffset(const gfx::PointF& offset) {
  if (ScrollableArea* scrollable_area = LayoutViewport()) {
    scrollable_area->SetScrollOffset(
        scrollable_area->ScrollPositionToOffset(offset),
        mojom::blink::ScrollType::kProgrammatic);
  }
}

gfx::Size WebLocalFrameImpl::DocumentSize() const {
  if (!GetFrameView() || !GetFrameView()->GetLayoutView())
    return gfx::Size();

  return ToPixelSnappedRect(GetFrameView()->GetLayoutView()->DocumentRect())
      .size();
}

bool WebLocalFrameImpl::HasVisibleContent() const {
  auto* layout_object = GetFrame()->OwnerLayoutObject();
  if (layout_object &&
      layout_object->StyleRef().UsedVisibility() != EVisibility::kVisible) {
    return false;
  }

  if (LocalFrameView* view = GetFrameView())
    return view->Width() > 0 && view->Height() > 0;
  return false;
}

gfx::Rect WebLocalFrameImpl::VisibleContentRect() const {
  if (LocalFrameView* view = GetFrameView())
    return view->LayoutViewport()->VisibleContentRect();
  return gfx::Rect();
}

WebView* WebLocalFrameImpl::View() const {
  return ViewImpl();
}

BrowserInterfaceBrokerProxy& WebLocalFrameImpl::GetBrowserInterfaceBroker() {
  return GetFrame()->GetBrowserInterfaceBroker();
}

WebDocument WebLocalFrameImpl::GetDocument() const {
  if (!GetFrame() || !GetFrame()->GetDocument())
    return WebDocument();
  return WebDocument(GetFrame()->GetDocument());
}

WebPerformanceMetricsForReporting
WebLocalFrameImpl::PerformanceMetricsForReporting() const {
  if (!GetFrame())
    return WebPerformanceMetricsForReporting();
  return WebPerformanceMetricsForReporting(
      DOMWindowPerformance::performance(*(GetFrame()->DomWindow())));
}

WebPerformanceMetricsForNestedContexts
WebLocalFrameImpl::PerformanceMetricsForNestedContexts() const {
  if (!GetFrame())
    return WebPerformanceMetricsForNestedContexts();
  return WebPerformanceMetricsForNestedContexts(
      DOMWindowPerformance::performance(*(GetFrame()->DomWindow())));
}

bool WebLocalFrameImpl::IsAdFrame() const {
  DCHECK(GetFrame());
  return GetFrame()->IsAdFrame();
}

bool WebLocalFrameImpl::IsAdScriptInStack() const {
  DCHECK(GetFrame());
  return GetFrame()->IsAdScriptInStack();
}

void WebLocalFrameImpl::SetAdEvidence(
    const blink::FrameAdEvidence& ad_evidence) {
  DCHECK(GetFrame());
  GetFrame()->SetAdEvidence(ad_evidence);
}

const std::optional<blink::FrameAdEvidence>& WebLocalFrameImpl::AdEvidence() {
  DCHECK(GetFrame());
  return GetFrame()->AdEvidence();
}

bool WebLocalFrameImpl::IsFrameCreatedByAdScript() {
  DCHECK(GetFrame());
  return GetFrame()->IsFrameCreatedByAdScript();
}

void WebLocalFrameImpl::ExecuteScript(const WebScriptSource& source) {
  DCHECK(GetFrame());
  ClassicScript::CreateUnspecifiedScript(source)->RunScript(
      GetFrame()->DomWindow());
}

void WebLocalFrameImpl::ExecuteScriptInIsolatedWorld(
    int32_t world_id,
    const WebScriptSource& source_in,
    BackForwardCacheAware back_forward_cache_aware) {
  DCHECK(GetFrame());
  CHECK_GT(world_id, DOMWrapperWorld::kMainWorldId);
  CHECK_LT(world_id, DOMWrapperWorld::kDOMWrapperWorldEmbedderWorldIdLimit);

  if (back_forward_cache_aware == BackForwardCacheAware::kPossiblyDisallow) {
    GetFrame()->GetFrameScheduler()->RegisterStickyFeature(
        SchedulingPolicy::Feature::kInjectedJavascript,
        {SchedulingPolicy::DisableBackForwardCache()});
  }

  // Note: An error event in an isolated world will never be dispatched to
  // a foreign world.
  v8::HandleScope handle_scope(ToIsolate(GetFrame()));
  ClassicScript::CreateUnspecifiedScript(source_in,
                                         SanitizeScriptErrors::kDoNotSanitize)
      ->RunScriptInIsolatedWorldAndReturnValue(GetFrame()->DomWindow(),
                                               world_id);
}

v8::Local<v8::Value>
WebLocalFrameImpl::ExecuteScriptInIsolatedWorldAndReturnValue(
    int32_t world_id,
    const WebScriptSource& source_in,
    BackForwardCacheAware back_forward_cache_aware) {
  DCHECK(GetFrame());
  CHECK_GT(world_id, DOMWrapperWorld::kMainWorldId);
  CHECK_LT(world_id, DOMWrapperWorld::kDOMWrapperWorldEmbedderWorldIdLimit);

  if (back_forward_cache_aware == BackForwardCacheAware::kPossiblyDisallow) {
    GetFrame()->GetFrameScheduler()->RegisterStickyFeature(
        SchedulingPolicy::Feature::kInjectedJavascript,
        {SchedulingPolicy::DisableBackForwardCache()});
  }

  // Note: An error event in an isolated world will never be dispatched to
  // a foreign world.
  return ClassicScript::CreateUnspecifiedScript(
             source_in, SanitizeScriptErrors::kDoNotSanitize)
      ->RunScriptInIsolatedWorldAndReturnValue(GetFrame()->DomWindow(),
                                               world_id)
      .GetSuccessValueOrEmpty();
}

void WebLocalFrameImpl::ClearIsolatedWorldCSPForTesting(int32_t world_id) {
  if (!GetFrame())
    return;
  if (world_id <= DOMWrapperWorld::kMainWorldId ||
      world_id >= DOMWrapperWorld::kDOMWrapperWorldEmbedderWorldIdLimit) {
    return;
  }

  GetFrame()->DomWindow()->ClearIsolatedWorldCSPForTesting(world_id);
}

void WebLocalFrameImpl::Alert(const WebString& message) {
  DCHECK(GetFrame());
  ScriptState* script_state = ToScriptStateForMainWorld(GetFrame());
  DCHECK(script_state);
  GetFrame()->DomWindow()->alert(script_state, message);
}

bool WebLocalFrameImpl::Confirm(const WebString& message) {
  DCHECK(GetFrame());
  ScriptState* script_state = ToScriptStateForMainWorld(GetFrame());
  DCHECK(script_state);
  return GetFrame()->DomWindow()->confirm(script_state, message);
}

WebString WebLocalFrameImpl::Prompt(const WebString& message,
                                    const WebString& default_value) {
  DCHECK(GetFrame());
  ScriptState* script_state = ToScriptStateForMainWorld(GetFrame());
  DCHECK(script_state);
  return GetFrame()->DomWindow()->prompt(script_state, message, default_value);
}

void WebLocalFrameImpl::GenerateInterventionReport(const WebString& message_id,
                                                   const WebString& message) {
  DCHECK(GetFrame());
  Intervention::GenerateReport(GetFrame(), message_id, message);
}

void WebLocalFrameImpl::CollectGarbageForTesting() {
  if (!GetFrame())
    return;
  if (!GetFrame()->GetSettings()->GetScriptEnabled())
    return;
  ThreadState::Current()->CollectAllGarbageForTesting();
}

v8::MaybeLocal<v8::Value> WebLocalFrameImpl::ExecuteMethodAndReturnValue(
    v8::Local<v8::Function> function,
    v8::Local<v8::Value> receiver,
    int argc,
    v8::Local<v8::Value> argv[]) {
  DCHECK(GetFrame());

  return GetFrame()
      ->DomWindow()
      ->GetScriptController()
      .EvaluateMethodInMainWorld(function, receiver, argc, argv);
}

v8::Local<v8::Value> WebLocalFrameImpl::ExecuteScriptAndReturnValue(
    const WebScriptSource& source) {
  DCHECK(GetFrame());
  return ClassicScript::CreateUnspecifiedScript(source)
      ->RunScriptAndReturnValue(GetFrame()->DomWindow())
      .GetSuccessValueOrEmpty();
}

void WebLocalFrameImpl::RequestExecuteV8Function(
    v8::Local<v8::Context> context,
    v8::Local<v8::Function> function,
    v8::Local<v8::Value> receiver,
    int argc,
    v8::Local<v8::Value> argv[],
    WebScriptExecutionCallback callback) {
  DCHECK(GetFrame());
  const auto want_result_option =
      callback ? mojom::blink::WantResultOption::kWantResult
               : mojom::blink::WantResultOption::kNoResult;
  PausableScriptExecutor::CreateAndRun(context, function, receiver, argc, argv,
                                       want_result_option, std::move(callback));
}

void WebLocalFrameImpl::RequestExecuteScript(
    int32_t world_id,
    base::span<const WebScriptSource> sources,
    mojom::blink::UserActivationOption user_gesture,
    mojom::blink::EvaluationTiming evaluation_timing,
    mojom::blink::LoadEventBlockingOption blocking_option,
    WebScriptExecutionCallback callback,
    BackForwardCacheAware back_forward_cache_aware,
    mojom::blink::WantResultOption want_result_option,
    mojom::blink::PromiseResultOption promise_behavior) {
  DCHECK(GetFrame());
  GetFrame()->RequestExecuteScript(
      world_id, sources, user_gesture, evaluation_timing, blocking_option,
      std::move(callback), back_forward_cache_aware, want_result_option,
      promise_behavior);
}

bool WebLocalFrameImpl::IsInspectorConnected() {
  return LocalRoot()->DevToolsAgentImpl(/*create_if_necessary=*/false);
}

v8::MaybeLocal<v8::Value> WebLocalFrameImpl::CallFunctionEvenIfScriptDisabled(
    v8::Local<v8::Function> function,
    v8::Local<v8::Value> receiver,
    int argc,
    v8::Local<v8::Value> argv[]) {
  DCHECK(GetFrame());
  return V8ScriptRunner::CallFunction(
      function, GetFrame()->DomWindow(), receiver, argc,
      static_cast<v8::Local<v8::Value>*>(argv), ToIsolate(GetFrame()));
}

v8::Local<v8::Context> WebLocalFrameImpl::MainWorldScriptContext() const {
  ScriptState* script_state = ToScriptStateForMainWorld(GetFrame());
  DCHECK(script_state);
  return script_state->GetContext();
}

int32_t WebLocalFrameImpl::GetScriptContextWorldId(
    v8::Local<v8::Context> script_context) const {
  DCHECK_EQ(this, FrameForContext(script_context));
  v8::Isolate* isolate = script_context->GetIsolate();
  return DOMWrapperWorld::World(isolate, script_context).GetWorldId();
}

v8::Local<v8::Context> WebLocalFrameImpl::GetScriptContextFromWorldId(
    v8::Isolate* isolate,
    int world_id) const {
  DOMWrapperWorld* world =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, world_id);
  return ToScriptState(GetFrame(), *world)->GetContext();
}

v8::Local<v8::Object> WebLocalFrameImpl::GlobalProxy(
    v8::Isolate* isolate) const {
  return MainWorldScriptContext()->Global();
}

bool WebFrame::ScriptCanAccess(v8::Isolate* isolate, WebFrame* target) {
  return BindingSecurity::ShouldAllowAccessTo(
      CurrentDOMWindow(isolate), ToCoreFrame(*target)->DomWindow());
}

void WebLocalFrameImpl::StartReload(WebFrameLoadType frame_load_type) {
  // TODO(clamy): Remove this function once RenderFrame calls StartNavigation
  // for all requests.
  DCHECK(GetFrame());
  DCHECK(IsReloadLoadType(frame_load_type));
  TRACE_EVENT1("navigation", "WebLocalFrameImpl::StartReload", "load_type",
               static_cast<int>(frame_load_type));

  ResourceRequest request =
      GetFrame()->Loader().ResourceRequestForReload(frame_load_type);
  if (request.IsNull())
    return;
  if (GetTextFinder())
    GetTextFinder()->ClearActiveFindMatch();

  FrameLoadRequest frame_load_request(GetFrame()->DomWindow(), request);
  GetFrame()->Loader().StartNavigation(frame_load_request, frame_load_type);
}

void WebLocalFrameImpl::ReloadImage(const WebNode& web_node) {
  Node* node = web_node;  // Use implicit WebNode->Node* cast.
  HitTestResult hit_test_result;
  hit_test_result.SetInnerNode(node);
  hit_test_result.SetToShadowHostIfInUAShadowRoot();
  node = hit_test_result.InnerNodeOrImageMapImage();
  if (auto* image_element = DynamicTo<HTMLImageElement>(*node))
    image_element->ForceReload();
}

void WebLocalFrameImpl::ClearActiveFindMatchForTesting() {
  DCHECK(GetFrame());
  if (GetTextFinder())
    GetTextFinder()->ClearActiveFindMatch();
}

WebDocumentLoader* WebLocalFrameImpl::GetDocumentLoader() const {
  DCHECK(GetFrame());
  return GetFrame()->Loader().GetDocumentLoader();
}

void WebLocalFrameImpl::EnableViewSourceMode(bool enable) {
  if (GetFrame())
    GetFrame()->SetInViewSourceMode(enable);
}

bool WebLocalFrameImpl::IsViewSourceModeEnabled() const {
  if (!GetFrame())
    return false;
  return GetFrame()->InViewSourceMode();
}

void WebLocalFrameImpl::SetReferrerForRequest(WebURLRequest& request,
                                              const WebURL& referrer_url) {
  String referrer = referrer_url.IsEmpty()
                        ? GetFrame()->DomWindow()->OutgoingReferrer()
                        : String(referrer_url.GetString());
  ResourceRequest& resource_request = request.ToMutableResourceRequest();
  resource_request.SetReferrerPolicy(
      GetFrame()->DomWindow()->GetReferrerPolicy());
  resource_request.SetReferrerString(referrer);
}

std::unique_ptr<WebAssociatedURLLoader>
WebLocalFrameImpl::CreateAssociatedURLLoader(
    const WebAssociatedURLLoaderOptions& options) {
  return std::make_unique<WebAssociatedURLLoaderImpl>(GetFrame()->DomWindow(),
                                                      options);
}

void WebLocalFrameImpl::DeprecatedStopLoading() {
  if (!GetFrame())
    return;
  // FIXME: Figure out what we should really do here. It seems like a bug
  // that FrameLoader::stopLoading doesn't call stopAllLoaders.
  GetFrame()->Loader().StopAllLoaders(/*abort_client=*/true);
}

void WebLocalFrameImpl::ReplaceSelection(const WebString& text) {
  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kSelection);

  GetFrame()->GetEditor().ReplaceSelection(text);
}

void WebLocalFrameImpl::UnmarkText() {
  GetFrame()->GetInputMethodController().CancelComposition();
}

bool WebLocalFrameImpl::HasMarkedText() const {
  return GetFrame()->GetInputMethodController().HasComposition();
}

WebRange WebLocalFrameImpl::MarkedRange() const {
  return GetFrame()->GetInputMethodController().CompositionEphemeralRange();
}

bool WebLocalFrameImpl::FirstRectForCharacterRange(
    uint32_t location,
    uint32_t length,
    gfx::Rect& rect_in_viewport) const {
  if ((location + length < location) && (location + length))
    length = 0;

  if (EditContext* edit_context =
          GetFrame()->GetInputMethodController().GetActiveEditContext()) {
    return edit_context->FirstRectForCharacterRange(location, length,
                                                    rect_in_viewport);
  }

  Element* editable =
      GetFrame()->Selection().RootEditableElementOrDocumentElement();
  if (!editable)
    return false;

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  see http://crbug.com/590369 for more details.
  editable->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  const EphemeralRange range =
      PlainTextRange(location, location + length).CreateRange(*editable);
  if (range.IsNull())
    return false;
  rect_in_viewport =
      GetFrame()->View()->FrameToViewport(FirstRectForRange(range));
  return true;
}

bool WebLocalFrameImpl::ExecuteCommand(const WebString& name) {
  DCHECK(GetFrame());

  if (name.length() <= 2)
    return false;

  // Since we don't have NSControl, we will convert the format of command
  // string and call the function on Editor directly.
  String command = name;

  // Make sure the first letter is upper case.
  command.replace(0, 1, command.Substring(0, 1).UpperASCII());

  // Remove the trailing ':' if existing.
  if (command[command.length() - 1] == UChar(':'))
    command = command.Substring(0, command.length() - 1);

  Node* plugin_lookup_context_node = nullptr;
  if (WebPluginContainerImpl::SupportsCommand(name))
    plugin_lookup_context_node = ContextMenuNodeInner();

  WebPluginContainerImpl* plugin_container =
      GetFrame()->GetWebPluginContainer(plugin_lookup_context_node);
  if (plugin_container && plugin_container->ExecuteEditCommand(name))
    return true;

  return GetFrame()->GetEditor().ExecuteCommand(command);
}

bool WebLocalFrameImpl::ExecuteCommand(const WebString& name,
                                       const WebString& value) {
  DCHECK(GetFrame());

  WebPluginContainerImpl* plugin_container =
      GetFrame()->GetWebPluginContainer();
  if (plugin_container && plugin_container->ExecuteEditCommand(name, value))
    return true;

  return GetFrame()->GetEditor().ExecuteCommand(name, value);
}

bool WebLocalFrameImpl::IsCommandEnabled(const WebString& name) const {
  DCHECK(GetFrame());
  return GetFrame()->GetEditor().IsCommandEnabled(name);
}

bool WebLocalFrameImpl::SelectionTextDirection(
    base::i18n::TextDirection& start,
    base::i18n::TextDirection& end) const {
  FrameSelection& selection = frame_->Selection();
  if (!selection.IsAvailable()) {
    // plugins/mouse-capture-inside-shadow.html reaches here
    return false;
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame_->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kSelection);

  if (selection.ComputeVisibleSelectionInDOMTree()
          .ToNormalizedEphemeralRange()
          .IsNull())
    return false;
  start = ToBaseTextDirection(PrimaryDirectionOf(
      *selection.ComputeVisibleSelectionInDOMTree().Start().AnchorNode()));
  end = ToBaseTextDirection(PrimaryDirectionOf(
      *selection.ComputeVisibleSelectionInDOMTree().End().AnchorNode()));
  return true;
}

bool WebLocalFrameImpl::IsSelectionAnchorFirst() const {
  FrameSelection& selection = frame_->Selection();
  if (!selection.IsAvailable()) {
    // plugins/mouse-capture-inside-shadow.html reaches here
    return false;
  }

  return selection.GetSelectionInDOMTree().IsAnchorFirst();
}

void WebLocalFrameImpl::SetTextDirectionForTesting(
    base::i18n::TextDirection direction) {
  frame_->SetTextDirection(direction);
}

void WebLocalFrameImpl::ReplaceMisspelledRange(const WebString& text) {
  // If this caret selection has two or more markers, this function replace the
  // range covered by the first marker with the specified word as Microsoft Word
  // does.
  if (GetFrame()->GetWebPluginContainer())
    return;

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  see http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kSpellCheck);

  GetFrame()->GetSpellChecker().ReplaceMisspelledRange(text);
}

void WebLocalFrameImpl::RemoveSpellingMarkers() {
  GetFrame()->GetSpellChecker().RemoveSpellingMarkers();
}

void WebLocalFrameImpl::RemoveSpellingMarkersUnderWords(
    const WebVector<WebString>& words) {
  Vector<String> converted_words;
  converted_words.AppendSpan(base::span(words));
  GetFrame()->RemoveSpellingMarkersUnderWords(converted_words);
}

bool WebLocalFrameImpl::HasSelection() const {
  DCHECK(GetFrame());
  WebPluginContainerImpl* plugin_container =
      GetFrame()->GetWebPluginContainer();
  if (plugin_container)
    return plugin_container->Plugin()->HasSelection();

  // TODO(editing-dev): The use of UpdateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kSelection);
  return GetFrame()->Selection().ComputeVisibleSelectionInDOMTree().IsRange();
}

WebRange WebLocalFrameImpl::SelectionRange() const {
  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kSelection);

  return GetFrame()
      ->Selection()
      .ComputeVisibleSelectionInDOMTree()
      .ToNormalizedEphemeralRange();
}

WebString WebLocalFrameImpl::SelectionAsText() const {
  DCHECK(GetFrame());
  WebPluginContainerImpl* plugin_container =
      GetFrame()->GetWebPluginContainer();
  if (plugin_container)
    return plugin_container->Plugin()->SelectionAsText();

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kSelection);

  String text;
  if (EditContext* edit_context =
          GetFrame()->GetInputMethodController().GetActiveEditContext()) {
    text = edit_context->text().Substring(
        edit_context->selectionStart(),
        edit_context->selectionEnd() - edit_context->selectionStart());
  } else {
    text = GetFrame()->Selection().SelectedText(
        TextIteratorBehavior::EmitsObjectReplacementCharacterBehavior());
  }
#if BUILDFLAG(IS_WIN)
  ReplaceNewlinesWithWindowsStyleNewlines(text);
#endif
  ReplaceNBSPWithSpace(text);
  return text;
}

WebString WebLocalFrameImpl::SelectionAsMarkup() const {
  WebPluginContainerImpl* plugin_container =
      GetFrame()->GetWebPluginContainer();
  if (plugin_container)
    return plugin_container->Plugin()->SelectionAsMarkup();

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // Selection normalization and markup generation require clean layout.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kSelection);

  return GetFrame()->Selection().SelectedHTMLForClipboard();
}

void WebLocalFrameImpl::TextSelectionChanged(const WebString& selection_text,
                                             uint32_t offset,
                                             const gfx::Range& range) {
  GetFrame()->TextSelectionChanged(selection_text, offset, range);
}

bool WebLocalFrameImpl::SelectAroundCaret(
    mojom::blink::SelectionGranularity granularity,
    bool should_show_handle,
    bool should_show_context_menu) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::selectAroundCaret");

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  see http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kSelection);
  // TODO(1275801): Add mapping between the enums once it becomes possible to
  // do so.
  blink::TextGranularity text_granularity;
  switch (granularity) {
    case mojom::blink::SelectionGranularity::kWord:
      text_granularity = blink::TextGranularity::kWord;
      break;
    case mojom::blink::SelectionGranularity::kSentence:
      text_granularity = blink::TextGranularity::kSentence;
      break;
  }
  return GetFrame()->Selection().SelectAroundCaret(
      text_granularity,
      should_show_handle ? HandleVisibility::kVisible
                         : HandleVisibility::kNotVisible,
      should_show_context_menu ? ContextMenuVisibility ::kVisible
                               : ContextMenuVisibility ::kNotVisible);
}

EphemeralRange WebLocalFrameImpl::GetWordSelectionRangeAroundCaret() const {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::getWordSelectionRangeAroundCaret");
  return GetFrame()->Selection().GetWordSelectionRangeAroundCaret();
}

void WebLocalFrameImpl::SelectRange(const gfx::Point& base_in_viewport,
                                    const gfx::Point& extent_in_viewport) {
  MoveRangeSelection(base_in_viewport, extent_in_viewport);
}

void WebLocalFrameImpl::SelectRange(
    const WebRange& web_range,
    HandleVisibilityBehavior handle_visibility_behavior,
    blink::mojom::SelectionMenuBehavior selection_menu_behavior,
    SelectionSetFocusBehavior selection_set_focus_behavior) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::selectRange");

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  see http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kSelection);

  const EphemeralRange& range = web_range.CreateEphemeralRange(GetFrame());
  if (range.IsNull())
    return;

  FrameSelection& selection = GetFrame()->Selection();
  const bool show_handles =
      handle_visibility_behavior == kShowSelectionHandle ||
      (handle_visibility_behavior == kPreserveHandleVisibility &&
       selection.IsHandleVisible());
  using blink::mojom::SelectionMenuBehavior;
  const bool selection_not_set_focus =
      selection_set_focus_behavior == kSelectionDoNotSetFocus;
  selection.SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(range)
          .SetAffinity(TextAffinity::kDefault)
          .Build(),
      SetSelectionOptions::Builder()
          .SetShouldShowHandle(show_handles)
          .SetShouldShrinkNextTap(selection_menu_behavior ==
                                  SelectionMenuBehavior::kShow)
          .SetDoNotSetFocus(selection_not_set_focus)
          .Build());

  if (selection_menu_behavior == SelectionMenuBehavior::kShow) {
    ContextMenuAllowedScope scope;
    GetFrame()->GetEventHandler().ShowNonLocatedContextMenu(
        nullptr, kMenuSourceAdjustSelection);
  }
}

WebString WebLocalFrameImpl::RangeAsText(const WebRange& web_range) {
  if (EditContext* edit_context =
          GetFrame()->GetInputMethodController().GetActiveEditContext()) {
    return edit_context->text().Substring(web_range.StartOffset(),
                                          web_range.length());
  } else {
    // TODO(editing-dev): The use of UpdateStyleAndLayout
    // needs to be audited.  see http://crbug.com/590369 for more details.
    GetFrame()->GetDocument()->UpdateStyleAndLayout(
        DocumentUpdateReason::kEditing);

    DocumentLifecycle::DisallowTransitionScope disallow_transition(
        GetFrame()->GetDocument()->Lifecycle());

    return PlainText(
        web_range.CreateEphemeralRange(GetFrame()),
        TextIteratorBehavior::EmitsObjectReplacementCharacterBehavior());
  }
}

void WebLocalFrameImpl::MoveRangeSelectionExtent(const gfx::Point& point) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::moveRangeSelectionExtent");

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kSelection);

  GetFrame()->Selection().MoveRangeSelectionExtent(
      GetFrame()->View()->ViewportToFrame(point));
}

void WebLocalFrameImpl::MoveRangeSelection(
    const gfx::Point& base_in_viewport,
    const gfx::Point& extent_in_viewport,
    WebFrame::TextGranularity granularity) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::moveRangeSelection");

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kSelection);

  blink::TextGranularity blink_granularity = blink::TextGranularity::kCharacter;
  if (granularity == WebFrame::kWordGranularity)
    blink_granularity = blink::TextGranularity::kWord;
  GetFrame()->Selection().MoveRangeSelection(
      GetFrame()->View()->ViewportToFrame(base_in_viewport),
      GetFrame()->View()->ViewportToFrame(extent_in_viewport),
      blink_granularity);
}

void WebLocalFrameImpl::MoveCaretSelection(
    const gfx::Point& point_in_viewport) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::moveCaretSelection");

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  see http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kSelection);
  const gfx::Point point_in_contents =
      GetFrame()->View()->ViewportToFrame(point_in_viewport);
  GetFrame()->Selection().MoveCaretSelection(point_in_contents);
}

bool WebLocalFrameImpl::SetEditableSelectionOffsets(int start, int end) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::setEditableSelectionOffsets");
  if (EditContext* edit_context =
          GetFrame()->GetInputMethodController().GetActiveEditContext()) {
    edit_context->SetSelection(start, end, /*dispatch_text_update_event=*/true);
    return true;
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kSelection);

  return GetFrame()->GetInputMethodController().SetEditableSelectionOffsets(
      PlainTextRange(start, end));
}

bool WebLocalFrameImpl::AddImeTextSpansToExistingText(
    const WebVector<ui::ImeTextSpan>& ime_text_spans,
    unsigned text_start,
    unsigned text_end) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::AddImeTextSpansToExistingText");

  if (!GetFrame()->GetEditor().CanEdit())
    return false;

  InputMethodController& input_method_controller =
      GetFrame()->GetInputMethodController();

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kEditing);

  input_method_controller.AddImeTextSpansToExistingText(
      ImeTextSpanVectorBuilder::Build(ime_text_spans), text_start, text_end);

  return true;
}
bool WebLocalFrameImpl::ClearImeTextSpansByType(ui::ImeTextSpan::Type type,
                                                unsigned text_start,
                                                unsigned text_end) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::ClearImeTextSpansByType");

  if (!GetFrame()->GetEditor().CanEdit())
    return false;

  InputMethodController& input_method_controller =
      GetFrame()->GetInputMethodController();

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kEditing);

  input_method_controller.ClearImeTextSpansByType(ConvertUiTypeToType(type),
                                                  text_start, text_end);

  return true;
}

bool WebLocalFrameImpl::SetCompositionFromExistingText(
    int composition_start,
    int composition_end,
    const WebVector<ui::ImeTextSpan>& ime_text_spans) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::setCompositionFromExistingText");
  if (EditContext* edit_context =
          GetFrame()->GetInputMethodController().GetActiveEditContext()) {
    return edit_context->SetCompositionFromExistingText(
        composition_start, composition_end, ime_text_spans);
  }

  if (!GetFrame()->GetEditor().CanEdit())
    return false;

  InputMethodController& input_method_controller =
      GetFrame()->GetInputMethodController();

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kEditing);

  input_method_controller.SetCompositionFromExistingText(
      ImeTextSpanVectorBuilder::Build(ime_text_spans), composition_start,
      composition_end);

  return true;
}

void WebLocalFrameImpl::ExtendSelectionAndDelete(int before, int after) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::extendSelectionAndDelete");
  if (EditContext* edit_context =
          GetFrame()->GetInputMethodController().GetActiveEditContext()) {
    edit_context->ExtendSelectionAndDelete(before, after);
    return;
  }

  if (WebPlugin* plugin = FocusedPluginIfInputMethodSupported()) {
    plugin->ExtendSelectionAndDelete(before, after);
    return;
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kSelection);

  GetFrame()->GetInputMethodController().ExtendSelectionAndDelete(before,
                                                                  after);
}

void WebLocalFrameImpl::ExtendSelectionAndReplace(
    int before,
    int after,
    const WebString& replacement_text) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::extendSelectionAndReplace");

  // EditContext and WebPlugin do not support atomic replacement.
  if (EditContext* edit_context =
          GetFrame()->GetInputMethodController().GetActiveEditContext()) {
    edit_context->ExtendSelectionAndDelete(before, after);
    edit_context->CommitText(replacement_text, std::vector<ui::ImeTextSpan>(),
                             blink::WebRange(), 0);
    return;
  }

  if (WebPlugin* plugin = FocusedPluginIfInputMethodSupported()) {
    plugin->ExtendSelectionAndDelete(before, after);
    plugin->CommitText(replacement_text, std::vector<ui::ImeTextSpan>(),
                       blink::WebRange(), 0);
    return;
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kSelection);

  GetFrame()->GetInputMethodController().ExtendSelectionAndReplace(
      before, after, replacement_text);
}

void WebLocalFrameImpl::DeleteSurroundingText(int before, int after) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::deleteSurroundingText");

  if (EditContext* edit_context =
          GetFrame()->GetInputMethodController().GetActiveEditContext()) {
    edit_context->DeleteSurroundingText(before, after);
    return;
  }

  if (WebPlugin* plugin = FocusedPluginIfInputMethodSupported()) {
    plugin->DeleteSurroundingText(before, after);
    return;
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kEditing);

  GetFrame()->GetInputMethodController().DeleteSurroundingText(before, after);
}

void WebLocalFrameImpl::DeleteSurroundingTextInCodePoints(int before,
                                                          int after) {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::deleteSurroundingTextInCodePoints");
  if (WebPlugin* plugin = FocusedPluginIfInputMethodSupported()) {
    plugin->DeleteSurroundingTextInCodePoints(before, after);
    return;
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame()->GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kEditing);

  GetFrame()->GetInputMethodController().DeleteSurroundingTextInCodePoints(
      before, after);
}

WebPlugin* WebLocalFrameImpl::FocusedPluginIfInputMethodSupported() {
  WebPluginContainerImpl* container = GetFrame()->GetWebPluginContainer();
  if (container && container->SupportsInputMethod())
    return container->Plugin();
  return nullptr;
}

void WebLocalFrameImpl::DispatchBeforePrintEvent(
    base::WeakPtr<WebPrintClient> print_client) {
#if DCHECK_IS_ON()
  DCHECK(!is_in_printing_) << "DispatchAfterPrintEvent() should have been "
                              "called after the previous "
                              "DispatchBeforePrintEvent() call.";
  is_in_printing_ = true;
#endif

  print_client_ = print_client;

  // Disable BackForwardCache when printing API is used for now. When the page
  // navigates with BackForwardCache, we currently do not close the printing
  // popup properly.
  GetFrame()->GetFrameScheduler()->RegisterStickyFeature(
      blink::SchedulingPolicy::Feature::kPrinting,
      {blink::SchedulingPolicy::DisableBackForwardCache()});

  GetFrame()->GetDocument()->SetPrinting(Document::kBeforePrinting);
  DispatchPrintEventRecursively(event_type_names::kBeforeprint);
  // In case the printing or print preview aborts for any reason, it is
  // important not to leave the document in the kBeforePrinting state.
  // See: crbug.com/1309595
  if (GetFrame())
    GetFrame()->GetDocument()->SetPrinting(Document::kNotPrinting);
}

void WebLocalFrameImpl::DispatchAfterPrintEvent() {
#if DCHECK_IS_ON()
  DCHECK(is_in_printing_) << "DispatchBeforePrintEvent() should be called "
                             "before DispatchAfterPrintEvent().";
  is_in_printing_ = false;
#endif

  print_client_.reset();

  if (View())
    DispatchPrintEventRecursively(event_type_names::kAfterprint);
}

void WebLocalFrameImpl::DispatchPrintEventRecursively(
    const AtomicString& event_type) {
  DCHECK(event_type == event_type_names::kBeforeprint ||
         event_type == event_type_names::kAfterprint);

  HeapVector<Member<Frame>> frames;
  for (Frame* frame = frame_; frame; frame = frame->Tree().TraverseNext(frame_))
    frames.push_back(frame);

  for (auto& frame : frames) {
    if (frame->IsRemoteFrame()) {
      // TODO(tkent): Support remote frames. crbug.com/455764.
      continue;
    }
    if (!frame->Tree().IsDescendantOf(frame_))
      continue;
    Event* event =
        event_type == event_type_names::kBeforeprint
            ? static_cast<Event*>(MakeGarbageCollected<BeforePrintEvent>())
            : static_cast<Event*>(MakeGarbageCollected<AfterPrintEvent>());
    To<LocalFrame>(frame.Get())->DomWindow()->DispatchEvent(*event);
  }
}

WebPluginContainerImpl* WebLocalFrameImpl::GetPluginToPrintHelper(
    const WebNode& constrain_to_node) {
  if (constrain_to_node.IsNull()) {
    // If this is a plugin document, check if the plugin supports its own
    // printing. If it does, we will delegate all printing to that.
    return GetFrame()->GetWebPluginContainer();
  }

  // We only support printing plugin nodes for now.
  return To<WebPluginContainerImpl>(constrain_to_node.PluginContainer());
}

WebPlugin* WebLocalFrameImpl::GetPluginToPrint(
    const WebNode& constrain_to_node) {
  WebPluginContainerImpl* plugin_container =
      GetPluginToPrintHelper(constrain_to_node);
  return plugin_container ? plugin_container->Plugin() : nullptr;
}

bool WebLocalFrameImpl::WillPrintSoon() {
  return GetFrame()->GetDocument()->WillPrintSoon();
}

uint32_t WebLocalFrameImpl::PrintBegin(const WebPrintParams& print_params,
                                       const WebNode& constrain_to_node) {
  WebPluginContainerImpl* plugin_container =
      GetPluginToPrintHelper(constrain_to_node);
  if (plugin_container && plugin_container->SupportsPaginatedPrint()) {
    print_context_ = MakeGarbageCollected<ChromePluginPrintContext>(
        GetFrame(), plugin_container);
  } else {
    print_context_ = MakeGarbageCollected<ChromePrintContext>(GetFrame());
  }

  print_context_->BeginPrintMode(print_params);

  return print_context_->PageCount();
}

void WebLocalFrameImpl::PrintPage(uint32_t page_index,
                                  cc::PaintCanvas* canvas) {
  DCHECK(print_context_);
  DCHECK(GetFrame());
  DCHECK(GetFrame()->GetDocument());

  print_context_->SpoolSinglePage(canvas, page_index);
}

void WebLocalFrameImpl::PrintEnd() {
  DCHECK(print_context_);
  print_context_->EndPrintMode();
  print_context_.Clear();
}

bool WebLocalFrameImpl::GetPrintPresetOptionsForPlugin(
    const WebNode& node,
    WebPrintPresetOptions* preset_options) {
  WebPluginContainerImpl* plugin_container = GetPluginToPrintHelper(node);
  if (!plugin_container || !plugin_container->SupportsPaginatedPrint())
    return false;

  return plugin_container->GetPrintPresetOptionsFromDocument(preset_options);
}

bool WebLocalFrameImpl::CapturePaintPreview(const gfx::Rect& bounds,
                                            cc::PaintCanvas* canvas,
                                            bool include_linked_destinations,
                                            bool skip_accelerated_content) {
  bool success = false;
  {
    // Ignore paint timing while capturing a paint preview as it can change LCP
    // see crbug.com/1323073.
    IgnorePaintTimingScope scope;
    IgnorePaintTimingScope::IncrementIgnoreDepth();

    Document::PaintPreviewScope paint_preview(
        *GetFrame()->GetDocument(),
        skip_accelerated_content
            ? Document::kPaintingPreviewSkipAcceleratedContent
            : Document::kPaintingPreview);
    GetFrame()->StartPaintPreview();
    PaintPreviewContext* paint_preview_context =
        MakeGarbageCollected<PaintPreviewContext>(GetFrame());
    success = paint_preview_context->Capture(canvas, bounds,
                                             include_linked_destinations);
    GetFrame()->EndPaintPreview();
  }
  return success;
}

WebPrintPageDescription WebLocalFrameImpl::GetPageDescription(
    uint32_t page_index) {
  if (page_index >= print_context_->PageCount()) {
    // TODO(crbug.com/452672): The number of pages may change after layout for
    // pagination. Very bad, but let's avoid crashing. The GetPageDescription()
    // API has no way of reporting failure, and the API user should be able to
    // trust that the numbers of pages reported when generating print layout
    // anyway. Due to Blink bugs, this isn't always the case, though. Get the
    // description of the first page.
    page_index = 0;
  }
  return print_context_->GetPageDescription(page_index);
}

gfx::Size WebLocalFrameImpl::SpoolSizeInPixelsForTesting(
    const WebVector<uint32_t>& pages) {
  int spool_width = 0;
  int spool_height = 0;

  for (uint32_t page_index : pages) {
    // Make room for the 1px tall page separator.
    if (page_index != pages.front())
      spool_height++;

    WebPrintPageDescription description =
        GetFrame()->GetDocument()->GetPageDescription(page_index);
    gfx::Size page_size = gfx::ToCeiledSize(description.size);
    if (description.orientation == PageOrientation::kUpright) {
      spool_width = std::max(spool_width, page_size.width());
      spool_height += page_size.height();
    } else {
      spool_height += page_size.width();
      spool_width = std::max(spool_width, page_size.height());
    }
  }
  return gfx::Size(spool_width, spool_height);
}

gfx::Size WebLocalFrameImpl::SpoolSizeInPixelsForTesting(uint32_t page_count) {
  WebVector<uint32_t> pages(page_count);
  std::iota(pages.begin(), pages.end(), 0);
  return SpoolSizeInPixelsForTesting(pages);
}

void WebLocalFrameImpl::PrintPagesForTesting(
    cc::PaintCanvas* canvas,
    const gfx::Size& spool_size_in_pixels,
    const WebVector<uint32_t>* pages) {
  DCHECK(print_context_);

  print_context_->SpoolPagesWithBoundariesForTesting(
      canvas, spool_size_in_pixels, pages);
}

gfx::Rect WebLocalFrameImpl::GetSelectionBoundsRectForTesting() const {
  DCHECK(GetFrame());  // Not valid after the Frame is detached.
  GetFrame()->View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kSelection);
  return HasSelection() ? ToPixelSnappedRect(
                              GetFrame()->Selection().AbsoluteUnclippedBounds())
                        : gfx::Rect();
}

gfx::Point WebLocalFrameImpl::GetPositionInViewportForTesting() const {
  DCHECK(GetFrame());  // Not valid after the Frame is detached.
  LocalFrameView* view = GetFrameView();
  return view->ConvertToRootFrame(gfx::Point());
}

// WebLocalFrameImpl public --------------------------------------------------

WebLocalFrame* WebLocalFrame::CreateMainFrame(
    WebView* web_view,
    WebLocalFrameClient* client,
    InterfaceRegistry* interface_registry,
    CrossVariantMojoRemote<mojom::BrowserInterfaceBrokerInterfaceBase>
        interface_broker,
    const LocalFrameToken& frame_token,
    const DocumentToken& document_token,
    std::unique_ptr<WebPolicyContainer> policy_container,
    WebFrame* opener,
    const WebString& name,
    network::mojom::blink::WebSandboxFlags sandbox_flags,
    const WebURL& creator_base_url) {
  return WebLocalFrameImpl::CreateMainFrame(
      web_view, client, interface_registry, std::move(interface_broker),
      frame_token, opener, name, sandbox_flags, document_token,
      std::move(policy_container), creator_base_url);
}

WebLocalFrame* WebLocalFrame::CreateProvisional(
    WebLocalFrameClient* client,
    InterfaceRegistry* interface_registry,
    CrossVariantMojoRemote<mojom::BrowserInterfaceBrokerInterfaceBase>
        interface_broker,
    const LocalFrameToken& frame_token,
    WebFrame* previous_frame,
    const FramePolicy& frame_policy,
    const WebString& name,
    WebView* web_view) {
  return WebLocalFrameImpl::CreateProvisional(
      client, interface_registry, std::move(interface_broker), frame_token,
      previous_frame, frame_policy, name, web_view);
}

WebLocalFrameImpl* WebLocalFrameImpl::CreateMainFrame(
    WebView* web_view,
    WebLocalFrameClient* client,
    InterfaceRegistry* interface_registry,
    mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker> interface_broker,
    const LocalFrameToken& frame_token,
    WebFrame* opener,
    const WebString& name,
    network::mojom::blink::WebSandboxFlags sandbox_flags,
    const DocumentToken& document_token,
    std::unique_ptr<WebPolicyContainer> policy_container,
    const WebURL& creator_base_url) {
  auto* frame = MakeGarbageCollected<WebLocalFrameImpl>(
      base::PassKey<WebLocalFrameImpl>(),
      mojom::blink::TreeScopeType::kDocument, client, interface_registry,
      frame_token);
  Page& page = *To<WebViewImpl>(web_view)->GetPage();
  DCHECK(!page.MainFrame());

  // TODO(https://crbug.com/1355751): From the browser process, plumb the
  // correct StorageKey for window in main frame. This is not an issue here,
  // because the FrameLoader is able to recover a correct StorageKey from the
  // origin of the document only.
  StorageKey storage_key;

  frame->InitializeCoreFrame(
      page, nullptr, nullptr, nullptr, FrameInsertType::kInsertInConstructor,
      name, opener ? &ToCoreFrame(*opener)->window_agent_factory() : nullptr,
      opener, document_token, std::move(interface_broker),
      std::move(policy_container), storage_key, creator_base_url,
      sandbox_flags);
  return frame;
}

WebLocalFrameImpl* WebLocalFrameImpl::CreateProvisional(
    WebLocalFrameClient* client,
    blink::InterfaceRegistry* interface_registry,
    mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker> interface_broker,
    const LocalFrameToken& frame_token,
    WebFrame* previous_web_frame,
    const FramePolicy& frame_policy,
    const WebString& name,
    WebView* web_view) {
  DCHECK(client);
  Frame* previous_frame = ToCoreFrame(*previous_web_frame);
  DCHECK(name.IsEmpty() || name.Equals(previous_frame->Tree().GetName()));
  auto* web_frame = MakeGarbageCollected<WebLocalFrameImpl>(
      base::PassKey<WebLocalFrameImpl>(),
      previous_web_frame->GetTreeScopeType(), client, interface_registry,
      frame_token);
  network::mojom::blink::WebSandboxFlags sandbox_flags =
      network::mojom::blink::WebSandboxFlags::kNone;
  PermissionsPolicyFeatureState feature_state;
  if (!previous_frame->Owner() || previous_frame->IsFencedFrameRoot()) {
    // Provisional main frames need to force sandbox flags.  This is necessary
    // to inherit sandbox flags when a sandboxed frame does a window.open()
    // which triggers a cross-process navigation.
    // Fenced frames also need to force special initial sandbox flags that are
    // passed via frame_policy.
    sandbox_flags = frame_policy.sandbox_flags;
  }

  // Note: this *always* temporarily sets a frame owner, even for main frames!
  // When a core Frame is created with no owner, it attempts to set itself as
  // the main frame of the Page. However, this is a provisional frame, and may
  // disappear, so Page::m_mainFrame can't be updated just yet.
  // Note 2: Becuase the dummy owner is still the owner when the initial empty
  // document is created, the initial empty document will not inherit the
  // correct sandbox flags. However, since the provisional frame is inivisible
  // to the rest of the page, the initial document is also invisible and
  // unscriptable. Once the provisional frame gets properly attached and is
  // observable, it will have the real FrameOwner, and any subsequent real
  // documents will correctly inherit sandbox flags from the owner.
  //
  // Note: this intentionally initializes the initial document of the
  // provisional frame with a random DocumentToken rather than plumbing it
  // through from //content. The fact that provisional frames have an initial
  // document is a weird implementation detail and this is an attempt to
  // minimize its visibility/usefulness.
  Page* page_for_provisional_frame = To<WebViewImpl>(web_view)->GetPage();
  web_frame->InitializeCoreFrame(
      *page_for_provisional_frame, MakeGarbageCollected<DummyFrameOwner>(),
      previous_web_frame->Parent(), nullptr, FrameInsertType::kInsertLater,
      name, &ToCoreFrame(*previous_web_frame)->window_agent_factory(),
      previous_web_frame->Opener(), DocumentToken(),
      std::move(interface_broker),
      /*policy_container=*/nullptr, StorageKey(),
      /*creator_base_url=*/KURL(), sandbox_flags);

  LocalFrame* new_frame = web_frame->GetFrame();

  if (previous_frame->GetPage() != page_for_provisional_frame) {
    // The previous frame's Page is different from the new frame's page. This
    // can only be true when creating a provisional LocalFrame that will do a
    // local main frame swap when its navigation commits. To be able to do the
    // swap, the provisional frame must have a pointer to the previous Page's
    // local main frame, and also be set as the provisional frame of the
    // placeholder RemoteFrame of the new Page.
    // Note that the new provisional frame is not set as the provisional frame
    // of the previous Page's main frame, to avoid triggering the deletion of
    // the new Page's provisional frame if/when the previous Page's main frame
    // gets deleted. With that, the new Page's provisional main frame's deletion
    // can only be triggered by deleting the new Page (when its WebView gets
    // deleted).
    CHECK(!previous_web_frame->Parent());
    CHECK(previous_web_frame->IsWebLocalFrame());
    CHECK(page_for_provisional_frame->MainFrame()->IsRemoteFrame());
    CHECK(!DynamicTo<RemoteFrame>(page_for_provisional_frame->MainFrame())
               ->IsRemoteFrameHostRemoteBound());
    page_for_provisional_frame->SetPreviousMainFrameForLocalSwap(
        DynamicTo<LocalFrame>(ToCoreFrame(*previous_web_frame)));
    page_for_provisional_frame->MainFrame()->SetProvisionalFrame(new_frame);
  } else {
    // This is a normal provisional frame, which will either replace a
    // RemoteFrame or a non-main-frame LocalFrame. This makes it possible to
    // find the provisional owner frame (the previous frame) when swapping in
    // the new frame. This also ensures that detaching the previous frame also
    // disposes of the provisional frame.
    previous_frame->SetProvisionalFrame(new_frame);
  }

  new_frame->SetOwner(previous_frame->Owner());
  if (auto* remote_frame_owner =
          DynamicTo<RemoteFrameOwner>(new_frame->Owner())) {
    remote_frame_owner->SetFramePolicy(frame_policy);
  }

  return web_frame;
}

WebLocalFrameImpl* WebLocalFrameImpl::CreateLocalChild(
    mojom::blink::TreeScopeType scope,
    WebLocalFrameClient* client,
    blink::InterfaceRegistry* interface_registry,
    const LocalFrameToken& frame_token) {
  auto* frame = MakeGarbageCollected<WebLocalFrameImpl>(
      base::PassKey<WebLocalFrameImpl>(), scope, client, interface_registry,
      frame_token);
  return frame;
}

WebLocalFrameImpl::WebLocalFrameImpl(
    base::PassKey<WebLocalFrameImpl>,
    mojom::blink::TreeScopeType scope,
    WebLocalFrameClient* client,
    blink::InterfaceRegistry* interface_registry,
    const LocalFrameToken& frame_token)
    : WebNavigationControl(scope, frame_token),
      client_(client),
      local_frame_client_(MakeGarbageCollected<LocalFrameClientImpl>(this)),
      autofill_client_(nullptr),
      find_in_page_(
          MakeGarbageCollected<FindInPage>(*this, interface_registry)),
      interface_registry_(interface_registry),
      input_method_controller_(*this),
      spell_check_panel_host_client_(nullptr),
      not_restored_reasons_(
          mojom::BackForwardCacheNotRestoredReasonsPtr(nullptr)) {
  CHECK(client_);
  g_frame_count++;
  client_->BindToFrame(this);
}

WebLocalFrameImpl::WebLocalFrameImpl(base::PassKey<WebRemoteFrameImpl>,
                                     mojom::blink::TreeScopeType scope,
                                     WebLocalFrameClient* client,
                                     InterfaceRegistry* interface_registry,
                                     const LocalFrameToken& frame_token)
    : WebLocalFrameImpl(base::PassKey<WebLocalFrameImpl>(),
                        scope,
                        client,
                        interface_registry,
                        frame_token) {}

WebLocalFrameImpl::~WebLocalFrameImpl() {
  // The widget for the frame, if any, must have already been closed.
  DCHECK(!frame_widget_);
  g_frame_count--;
}

void WebLocalFrameImpl::Trace(Visitor* visitor) const {
  visitor->Trace(local_frame_client_);
  visitor->Trace(find_in_page_);
  visitor->Trace(frame_);
  visitor->Trace(dev_tools_agent_);
  visitor->Trace(frame_widget_);
  visitor->Trace(print_context_);
  visitor->Trace(input_method_controller_);
  visitor->Trace(current_history_item_);
}

void WebLocalFrameImpl::SetCoreFrame(LocalFrame* frame) {
  frame_ = frame;
}

void WebLocalFrameImpl::InitializeCoreFrame(
    Page& page,
    FrameOwner* owner,
    WebFrame* parent,
    WebFrame* previous_sibling,
    FrameInsertType insert_type,
    const AtomicString& name,
    WindowAgentFactory* window_agent_factory,
    WebFrame* opener,
    const DocumentToken& document_token,
    mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker> interface_broker,
    std::unique_ptr<blink::WebPolicyContainer> policy_container,
    const StorageKey& storage_key,
    const KURL& creator_base_url,
    network::mojom::blink::WebSandboxFlags sandbox_flags) {
  InitializeCoreFrameInternal(
      page, owner, parent, previous_sibling, insert_type, name,
      window_agent_factory, opener, document_token, std::move(interface_broker),
      PolicyContainer::CreateFromWebPolicyContainer(
          std::move(policy_container)),
      storage_key, ukm::kInvalidSourceId, creator_base_url, sandbox_flags);
}

void WebLocalFrameImpl::InitializeCoreFrameInternal(
    Page& page,
    FrameOwner* owner,
    WebFrame* parent,
    WebFrame* previous_sibling,
    FrameInsertType insert_type,
    const AtomicString& name,
    WindowAgentFactory* window_agent_factory,
    WebFrame* opener,
    const DocumentToken& document_token,
    mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker> interface_broker,
    std::unique_ptr<PolicyContainer> policy_container,
    const StorageKey& storage_key,
    ukm::SourceId document_ukm_source_id,
    const KURL& creator_base_url,
    network::mojom::blink::WebSandboxFlags sandbox_flags) {
  Frame* parent_frame = parent ? ToCoreFrame(*parent) : nullptr;
  Frame* previous_sibling_frame =
      previous_sibling ? ToCoreFrame(*previous_sibling) : nullptr;
  SetCoreFrame(MakeGarbageCollected<LocalFrame>(
      local_frame_client_.Get(), page, owner, parent_frame,
      previous_sibling_frame, insert_type, GetLocalFrameToken(),
      window_agent_factory, interface_registry_, std::move(interface_broker)));
  frame_->Tree().SetName(name);

  // See sandbox inheritance: content/browser/renderer_host/sandbox_flags.md
  //
  // New documents are either:
  // 1. The initial empty document:
  //   a. In a new iframe.
  //   b. In a new fencedframe.
  //   c. In a new popup.
  // 2. A document replacing the previous, one via a navigation.
  //
  // 1.b. will get the special sandbox flags. See:
  // https://docs.google.com/document/d/1RO4NkQk_XaEE7vuysM9LJilZYsoOhydfh93sOvrPQxU/edit
  // For 1.c., this is used to define sandbox flags for
  // the initial empty document in a new popup.
  if (frame_->IsMainFrame()) {
    DCHECK(!frame_->IsInFencedFrameTree() ||
           ((sandbox_flags & blink::kFencedFrameForcedSandboxFlags) ==
            blink::kFencedFrameForcedSandboxFlags))
        << "An MPArch fencedframe must be configured with its forced sandbox "
        << "flags:" << sandbox_flags;
    frame_->SetOpenerSandboxFlags(sandbox_flags);
  }

  Frame* opener_frame = opener ? ToCoreFrame(*opener) : nullptr;

  // We must call init() after frame_ is assigned because it is referenced
  // during init().
  frame_->Init(opener_frame, document_token, std::move(policy_container),
               storage_key, document_ukm_source_id, creator_base_url);

  if (!owner) {
    // This trace event is needed to detect the main frame of the
    // renderer in telemetry metrics. See crbug.com/692112#c11.
    TRACE_EVENT_INSTANT1("loading", "markAsMainFrame", TRACE_EVENT_SCOPE_THREAD,
                         "frame", GetFrameIdForTracing(frame_));
  }
}

LocalFrame* WebLocalFrameImpl::CreateChildFrame(
    const AtomicString& name,
    HTMLFrameOwnerElement* owner_element) {
  DCHECK(client_);
  TRACE_EVENT0("blink", "WebLocalFrameImpl::createChildframe");
  mojom::blink::TreeScopeType scope =
      GetFrame()->GetDocument() == owner_element->GetTreeScope()
          ? mojom::blink::TreeScopeType::kDocument
          : mojom::blink::TreeScopeType::kShadow;
  WebFrameOwnerProperties owner_properties(
      owner_element->BrowsingContextContainerName(),
      owner_element->ScrollbarMode(), owner_element->MarginWidth(),
      owner_element->MarginHeight(), owner_element->AllowFullscreen(),
      owner_element->AllowPaymentRequest(), owner_element->IsDisplayNone(),
      owner_element->GetColorScheme(),
      owner_element->GetPreferredColorScheme());

  mojo::PendingAssociatedRemote<mojom::blink::PolicyContainerHost>
      policy_container_remote;
  mojo::PendingAssociatedReceiver<mojom::blink::PolicyContainerHost>
      policy_container_receiver =
          policy_container_remote.InitWithNewEndpointAndPassReceiver();

  FramePolicy frame_policy = owner_element->GetFramePolicy();

  // The initial empty document's policy container is inherited from its parent.
  mojom::blink::PolicyContainerPoliciesPtr policy_container_data =
      GetFrame()->DomWindow()->GetPolicyContainer()->GetPolicies().Clone();

  // The frame sandbox flags and the initial empty document's sandbox flags
  // are restricted by the parent document's sandbox flags and the iframe's
  // sandbox attribute. It is the union of:
  //  - The parent's sandbox flags which are contained in
  //    policy_container_data and were cloned from the parent's document policy
  //    container above.
  //  - The iframe's sandbox attribute which is contained in frame_policy, from
  //    the owner element's frame policy.
  policy_container_data->sandbox_flags |= frame_policy.sandbox_flags;
  frame_policy.sandbox_flags = policy_container_data->sandbox_flags;

  // No URL is associated with this frame, but we can still assign UKM events to
  // this identifier.
  ukm::SourceId document_ukm_source_id = ukm::NoURLSourceId();

  auto complete_initialization =
      [this, owner_element, &policy_container_remote, &policy_container_data,
       &name, document_ukm_source_id](
          WebLocalFrame* new_child_frame, const DocumentToken& document_token,
          CrossVariantMojoRemote<mojom::BrowserInterfaceBrokerInterfaceBase>
              interface_broker) {
        // The initial empty document's credentialless bit is the union of:
        // - its parent's credentialless bit.
        // - its frame's credentialless attribute.
        policy_container_data->is_credentialless |=
            owner_element->Credentialless();

        std::unique_ptr<PolicyContainer> policy_container =
            std::make_unique<PolicyContainer>(
                std::move(policy_container_remote),
                std::move(policy_container_data));

        KURL creator_base_url(owner_element->GetDocument().BaseURL());
        To<WebLocalFrameImpl>(new_child_frame)
            ->InitializeCoreFrameInternal(
                *GetFrame()->GetPage(), owner_element, this, LastChild(),
                FrameInsertType::kInsertInConstructor, name,
                &GetFrame()->window_agent_factory(), nullptr, document_token,
                std::move(interface_broker), std::move(policy_container),
                GetFrame()->DomWindow()->GetStorageKey(),
                document_ukm_source_id, creator_base_url);
      };

  // FIXME: Using subResourceAttributeName as fallback is not a perfect
  // solution. subResourceAttributeName returns just one attribute name. The
  // element might not have the attribute, and there might be other attributes
  // which can identify the element.
  WebLocalFrameImpl* webframe_child =
      To<WebLocalFrameImpl>(client_->CreateChildFrame(
          scope, name,
          owner_element->getAttribute(
              owner_element->SubResourceAttributeName()),
          std::move(frame_policy), owner_properties, owner_element->OwnerType(),
          WebPolicyContainerBindParams{std::move(policy_container_receiver)},
          document_ukm_source_id, complete_initialization));
  if (!webframe_child)
    return nullptr;

  DCHECK(webframe_child->Parent());
  // If the lambda to complete initialization is not called, this will fail.
  DCHECK(webframe_child->GetFrame());
  return webframe_child->GetFrame();
}

RemoteFrame* WebLocalFrameImpl::CreateFencedFrame(
    HTMLFencedFrameElement* fenced_frame,
    mojo::PendingAssociatedReceiver<mojom::blink::FencedFrameOwnerHost>
        receiver) {
  mojom::blink::FrameReplicationStatePtr initial_replicated_state =
      mojom::blink::FrameReplicationState::New();
  initial_replicated_state->origin = SecurityOrigin::CreateUniqueOpaque();
  RemoteFrameToken frame_token;
  base::UnguessableToken devtools_frame_token =
      base::UnguessableToken::Create();
  auto remote_frame_interfaces =
      mojom::blink::RemoteFrameInterfacesFromRenderer::New();
  mojo::PendingAssociatedRemote<mojom::blink::RemoteFrameHost>
      remote_frame_host = remote_frame_interfaces->frame_host_receiver
                              .InitWithNewEndpointAndPassRemote();
  mojo::PendingAssociatedReceiver<mojom::blink::RemoteFrame>
      remote_frame_receiver =
          remote_frame_interfaces->frame.InitWithNewEndpointAndPassReceiver();

  GetFrame()->GetLocalFrameHostRemote().CreateFencedFrame(
      std::move(receiver), std::move(remote_frame_interfaces), frame_token,
      devtools_frame_token);

  DCHECK(initial_replicated_state->origin->IsOpaque());

  WebRemoteFrameImpl* remote_frame = WebRemoteFrameImpl::CreateForFencedFrame(
      mojom::blink::TreeScopeType::kDocument, frame_token, devtools_frame_token,
      fenced_frame, std::move(remote_frame_host),
      std::move(remote_frame_receiver), std::move(initial_replicated_state));

  client_->DidCreateFencedFrame(frame_token);
  return remote_frame->GetFrame();
}

void WebLocalFrameImpl::DidChangeContentsSize(const gfx::Size& size) {
  if (GetTextFinder() && GetTextFinder()->TotalMatchCount() > 0)
    GetTextFinder()->IncreaseMarkerVersion();
}

bool WebLocalFrameImpl::HasDevToolsOverlays() const {
  return dev_tools_agent_ && dev_tools_agent_->HasOverlays();
}

void WebLocalFrameImpl::UpdateDevToolsOverlaysPrePaint() {
  if (dev_tools_agent_)
    dev_tools_agent_->UpdateOverlaysPrePaint();
}

void WebLocalFrameImpl::PaintDevToolsOverlays(GraphicsContext& context) {
  if (dev_tools_agent_)
    dev_tools_agent_->PaintOverlays(context);
}

void WebLocalFrameImpl::CreateFrameView() {
  TRACE_EVENT0("blink", "WebLocalFrameImpl::createFrameView");

  DCHECK(GetFrame());  // If frame() doesn't exist, we probably didn't init
                       // properly.

  WebViewImpl* web_view = ViewImpl();

  // Check if we're shutting down.
  if (!web_view->GetPage())
    return;

  bool is_main_frame = !Parent();
  // TODO(dcheng): Can this be better abstracted away? It's pretty ugly that
  // only local roots are special-cased here.
  gfx::Size initial_size = (is_main_frame || !frame_widget_)
                               ? web_view->MainFrameSize()
                               : frame_widget_->Size();
  Color base_background_color = web_view->BaseBackgroundColor();
  if (!is_main_frame && Parent()->IsWebRemoteFrame())
    base_background_color = Color::kTransparent;

  GetFrame()->CreateView(initial_size, base_background_color);
  if (web_view->ShouldAutoResize() && GetFrame()->IsLocalRoot()) {
    GetFrame()->View()->EnableAutoSizeMode(web_view->MinAutoSize(),
                                           web_view->MaxAutoSize());
  }

  if (frame_widget_)
    frame_widget_->DidCreateLocalRootView();
}

WebLocalFrameImpl* WebLocalFrameImpl::FromFrame(LocalFrame* frame) {
  if (!frame)
    return nullptr;
  return FromFrame(*frame);
}

std::string WebLocalFrameImpl::GetNullFrameReasonForBug1139104(
    LocalFrame* frame) {
  LocalFrameClient* client = frame->Client();
  if (!client)
    return "WebLocalFrameImpl::client";
  if (!client->IsLocalFrameClientImpl())
    return "WebLocalFrameImpl::client-not-local";
  WebLocalFrame* web_frame = client->GetWebFrame();
  if (!web_frame)
    return "WebLocalFrameImpl::web_frame";
  return "not-null";
}

WebLocalFrameImpl* WebLocalFrameImpl::FromFrame(LocalFrame& frame) {
  LocalFrameClient* client = frame.Client();
  if (!client || !client->IsLocalFrameClientImpl())
    return nullptr;
  return To<WebLocalFrameImpl>(client->GetWebFrame());
}

WebViewImpl* WebLocalFrameImpl::ViewImpl() const {
  if (!GetFrame())
    return nullptr;
  return GetFrame()->GetPage()->GetChromeClient().GetWebView();
}

bool WebLocalFrameImpl::ShouldWarmUpCompositorOnPrerenderFromThisPoint(
    features::Prerender2WarmUpCompositorTriggerPoint trigger_point) {
  static const bool is_warm_up_compositor_enabled =
      base::FeatureList::IsEnabled(::features::kWarmUpCompositor);
  if (!is_warm_up_compositor_enabled) {
    return false;
  }

  if (!GetFrame()->IsOutermostMainFrame()) {
    return false;
  }

  if (!GetFrame()->GetPage() || !GetFrame()->GetPage()->IsPrerendering() ||
      !GetFrame()->GetPage()->ShouldWarmUpCompositorOnPrerender()) {
    return false;
  }

  static const bool is_prerender2_warm_up_compositor_enabled =
      base::FeatureList::IsEnabled(features::kPrerender2WarmUpCompositor);
  // TODO(crbug.com/41496019): Seek the best point to start warm-up.
  static const auto prerender2_warm_up_compositor_trigger_point =
      features::kPrerender2WarmUpCompositorTriggerPoint.Get();
  if (!is_prerender2_warm_up_compositor_enabled ||
      prerender2_warm_up_compositor_trigger_point != trigger_point) {
    return false;
  }

  return true;
}

void WebLocalFrameImpl::DidCommitLoad() {
  if (frame_widget_ &&
      ShouldWarmUpCompositorOnPrerenderFromThisPoint(
          features::Prerender2WarmUpCompositorTriggerPoint::kDidCommitLoad)) {
    frame_widget_->WarmUpCompositor();
  }
}

void WebLocalFrameImpl::DidDispatchDOMContentLoadedEvent() {
  if (frame_widget_ && ShouldWarmUpCompositorOnPrerenderFromThisPoint(
                           features::Prerender2WarmUpCompositorTriggerPoint::
                               kDidDispatchDOMContentLoadedEvent)) {
    frame_widget_->WarmUpCompositor();
  }
}

void WebLocalFrameImpl::DidFailLoad(const ResourceError& error,
                                    WebHistoryCommitType web_commit_type) {
  if (WebPluginContainerImpl* plugin = GetFrame()->GetWebPluginContainer())
    plugin->DidFailLoading(error);
  WebDocumentLoader* document_loader = GetDocumentLoader();
  DCHECK(document_loader);
  GetFrame()->GetLocalFrameHostRemote().DidFailLoadWithError(
      document_loader->GetUrl(), error.ErrorCode());
}

void WebLocalFrameImpl::DidFinish() {
  if (!Client())
    return;

  if (frame_widget_ &&
      ShouldWarmUpCompositorOnPrerenderFromThisPoint(
          features::Prerender2WarmUpCompositorTriggerPoint::kDidFinishLoad)) {
    frame_widget_->WarmUpCompositor();
  }

  if (WebPluginContainerImpl* plugin = GetFrame()->GetWebPluginContainer())
    plugin->DidFinishLoading();

  Client()->DidFinishLoad();
}

void WebLocalFrameImpl::DidFinishLoadForPrinting() {
  Client()->DidFinishLoadForPrinting();
}

HitTestResult WebLocalFrameImpl::HitTestResultForVisualViewportPos(
    const gfx::Point& pos_in_viewport) {
  gfx::Point root_frame_point(
      GetFrame()->GetPage()->GetVisualViewport().ViewportToRootFrame(
          pos_in_viewport));
  HitTestLocation location(
      GetFrame()->View()->ConvertFromRootFrame(root_frame_point));
  HitTestResult result = GetFrame()->GetEventHandler().HitTestResultAtLocation(
      location, HitTestRequest::kReadOnly | HitTestRequest::kActive);
  result.SetToShadowHostIfInUAShadowRoot();
  return result;
}

void WebLocalFrameImpl::SetAutofillClient(WebAutofillClient* autofill_client) {
  autofill_client_ = autofill_client;
}

WebAutofillClient* WebLocalFrameImpl::AutofillClient() {
  return autofill_client_;
}

void WebLocalFrameImpl::SetContentCaptureClient(
    WebContentCaptureClient* content_capture_client) {
  content_capture_client_ = content_capture_client;
}

WebContentCaptureClient* WebLocalFrameImpl::ContentCaptureClient() const {
  return content_capture_client_;
}

bool WebLocalFrameImpl::IsProvisional() const {
  return frame_->IsProvisional();
}

WebLocalFrameImpl* WebLocalFrameImpl::LocalRoot() {
  DCHECK(GetFrame());
  auto* result = FromFrame(GetFrame()->LocalFrameRoot());
  DCHECK(result);
  return result;
}

WebFrame* WebLocalFrameImpl::FindFrameByName(const WebString& name) {
  return WebFrame::FromCoreFrame(GetFrame()->Tree().FindFrameByName(name));
}

void WebLocalFrameImpl::SetEmbeddingToken(
    const base::UnguessableToken& embedding_token) {
  frame_->SetEmbeddingToken(embedding_token);
}

bool WebLocalFrameImpl::IsInFencedFrameTree() const {
  bool result = frame_->IsInFencedFrameTree();
  DCHECK(!result || blink::features::IsFencedFramesEnabled());
  return result;
}

const std::optional<base::UnguessableToken>&
WebLocalFrameImpl::GetEmbeddingToken() const {
  return frame_->GetEmbeddingToken();
}

void WebLocalFrameImpl::SendPings(const WebURL& destination_url) {
  DCHECK(GetFrame());
  if (Node* node = ContextMenuNodeInner()) {
    Element* anchor = node->EnclosingLinkEventParentOrSelf();
    // TODO(crbug.com/369219144): Should this be
    // DynamicTo<HTMLAnchorElementBase>?
    if (auto* html_anchor = DynamicTo<HTMLAnchorElement>(anchor))
      html_anchor->SendPings(destination_url);
  }
}

bool WebLocalFrameImpl::DispatchBeforeUnloadEvent(bool is_reload) {
  if (!GetFrame())
    return true;

  return GetFrame()->Loader().ShouldClose(is_reload);
}

void WebLocalFrameImpl::CommitNavigation(
    std::unique_ptr<WebNavigationParams> navigation_params,
    std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) {
  DCHECK(GetFrame());
  DCHECK(!navigation_params->url.ProtocolIs("javascript"));
  if (navigation_params->is_synchronous_commit_for_bug_778318) {
    DCHECK(WebDocumentLoader::WillLoadUrlAsEmpty(navigation_params->url));
    navigation_params->storage_key = GetFrame()->DomWindow()->GetStorageKey();
    navigation_params->document_ukm_source_id =
        GetFrame()->DomWindow()->UkmSourceID();

    // This corresponds to step 8 of
    // https://html.spec.whatwg.org/multipage/browsers.html#creating-a-new-browsing-context.
    // Most of these steps are handled in the caller
    // (RenderFrameImpl::SynchronouslyCommitAboutBlankForBug778318) but the
    // caller doesn't have access to the core frame (LocalFrame).
    // The actual agent is determined downstream, but here we need to request
    // whether an origin-keyed agent is needed. Since this case is only
    // for about:blank navigations this reduces to copying the agent flag from
    // the current document.
    navigation_params->origin_agent_cluster =
        GetFrame()->GetDocument()->GetAgent().IsOriginKeyedForInheritance();

    KURL url = navigation_params->url;
    if (navigation_params->is_synchronous_commit_for_bug_778318 &&
        // Explicitly check for about:blank or about:srcdoc to prevent things
        // like about:mumble propagating the base url.
        (url.IsAboutBlankURL() || url.IsAboutSrcdocURL())) {
      navigation_params->fallback_base_url =
          GetFrame()->GetDocument()->BaseURL();
    }
  }
  if (GetTextFinder())
    GetTextFinder()->ClearActiveFindMatch();
  GetFrame()->Loader().CommitNavigation(std::move(navigation_params),
                                        std::move(extra_data));
}

blink::mojom::CommitResult WebLocalFrameImpl::CommitSameDocumentNavigation(
    const WebURL& url,
    WebFrameLoadType web_frame_load_type,
    const WebHistoryItem& item,
    bool is_client_redirect,
    bool has_transient_user_activation,
    const WebSecurityOrigin& initiator_origin,
    bool is_browser_initiated,
    bool has_ua_visual_transition,
    std::optional<scheduler::TaskAttributionId>
        soft_navigation_heuristics_task_id) {
  DCHECK(GetFrame());
  DCHECK(!url.ProtocolIs("javascript"));

  HistoryItem* history_item = item;
  return GetFrame()->Loader().GetDocumentLoader()->CommitSameDocumentNavigation(
      url, web_frame_load_type, history_item,
      is_client_redirect ? ClientRedirectPolicy::kClientRedirect
                         : ClientRedirectPolicy::kNotClientRedirect,
      has_transient_user_activation, initiator_origin.Get(),
      /*is_synchronously_committed=*/false, /*source_element=*/nullptr,
      mojom::blink::TriggeringEventInfo::kNotFromEvent, is_browser_initiated,
      has_ua_visual_transition, soft_navigation_heuristics_task_id);
}

bool WebLocalFrameImpl::IsLoading() const {
  if (!GetFrame() || !GetFrame()->GetDocument())
    return false;
  return GetFrame()->GetDocument()->IsInitialEmptyDocument() ||
         GetFrame()->Loader().HasProvisionalNavigation() ||
         !GetFrame()->GetDocument()->LoadEventFinished();
}

bool WebLocalFrameImpl::IsNavigationScheduledWithin(
    base::TimeDelta interval) const {
  if (!GetFrame())
    return false;
  return GetFrame()->Loader().HasProvisionalNavigation() ||
         GetFrame()->GetDocument()->IsHttpRefreshScheduledWithin(interval);
}

void WebLocalFrameImpl::SetIsNotOnInitialEmptyDocument() {
  DCHECK(GetFrame());
  GetFrame()->GetDocument()->OverrideIsInitialEmptyDocument();
  GetFrame()->Loader().SetIsNotOnInitialEmptyDocument();
}

bool WebLocalFrameImpl::IsOnInitialEmptyDocument() {
  DCHECK(GetFrame());
  return GetFrame()->GetDocument()->IsInitialEmptyDocument();
}

void WebLocalFrameImpl::BlinkFeatureUsageReport(
    blink::mojom::WebFeature feature) {
  UseCounter::Count(GetFrame()->GetDocument(), feature);
}

void WebLocalFrameImpl::DidDropNavigation() {
  GetFrame()->Loader().DidDropNavigation();
}

void WebLocalFrameImpl::DownloadURL(
    const WebURLRequest& request,
    network::mojom::blink::RedirectMode cross_origin_redirect_behavior,
    CrossVariantMojoRemote<mojom::blink::BlobURLTokenInterfaceBase>
        blob_url_token) {
  GetFrame()->DownloadURL(request.ToResourceRequest(),
                          cross_origin_redirect_behavior,
                          std::move(blob_url_token));
}

void WebLocalFrameImpl::MaybeStartOutermostMainFrameNavigation(
    const WebVector<WebURL>& urls) const {
  Vector<KURL> kurls;
  std::move(urls.begin(), urls.end(), std::back_inserter(kurls));
  GetFrame()->MaybeStartOutermostMainFrameNavigation(std::move(kurls));
}

bool WebLocalFrameImpl::WillStartNavigation(const WebNavigationInfo& info) {
  DCHECK(!info.url_request.IsNull());
  DCHECK(!info.url_request.Url().ProtocolIs("javascript"));
  return GetFrame()->Loader().WillStartNavigation(info);
}

void WebLocalFrameImpl::SendOrientationChangeEvent() {
  // Speculative fix for https://crbug.com/1143380.
  // TODO(https://crbug.com/838348): It's a logic bug that this function is
  // being called when either the LocalFrame or LocalDOMWindow are null, but
  // there is a bug where the browser can inadvertently detach the main frame of
  // a WebView that is still active.
  if (!GetFrame() || !GetFrame()->DomWindow())
    return;

  // Screen Orientation API
  CoreInitializer::GetInstance().NotifyOrientationChanged(*GetFrame());

  // Legacy window.orientation API
  if (RuntimeEnabledFeatures::OrientationEventEnabled())
    GetFrame()->DomWindow()->SendOrientationChangeEvent();
}

WebNode WebLocalFrameImpl::ContextMenuNode() const {
  return ContextMenuNodeInner();
}

WebNode WebLocalFrameImpl::ContextMenuImageNode() const {
  return ContextMenuImageNodeInner();
}

void WebLocalFrameImpl::WillBeDetached() {
  if (frame_->IsMainFrame())
    ViewImpl()->DidDetachLocalMainFrame();
  if (dev_tools_agent_)
    dev_tools_agent_->WillBeDestroyed();
  if (find_in_page_)
    find_in_page_->Dispose();
  if (print_client_)
    print_client_->WillBeDestroyed();

  for (auto& observer : observers_)
    observer.WebLocalFrameDetached();
}

void WebLocalFrameImpl::WillDetachParent() {
  // Do not expect string scoping results from any frames that got detached
  // in the middle of the operation.
  if (GetTextFinder() && GetTextFinder()->ScopingInProgress()) {
    // There is a possibility that the frame being detached was the only
    // pending one. We need to make sure final replies can be sent.
    GetTextFinder()->FlushCurrentScoping();

    GetTextFinder()->CancelPendingScopingEffort();
  }
}

void WebLocalFrameImpl::CreateFrameWidgetInternal(
    base::PassKey<WebLocalFrame> pass_key,
    CrossVariantMojoAssociatedRemote<mojom::blink::FrameWidgetHostInterfaceBase>
        mojo_frame_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
        mojo_frame_widget,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        mojo_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        mojo_widget,
    const viz::FrameSinkId& frame_sink_id,
    bool is_for_nested_main_frame,
    bool is_for_scalable_page,
    bool hidden) {
  DCHECK(!frame_widget_);
  DCHECK(frame_->IsLocalRoot());
  bool is_for_child_local_root = Parent();

  // Check that if this is for a child local root |is_for_nested_main_frame|
  // is false.
  DCHECK(!is_for_child_local_root || !is_for_nested_main_frame);

  bool never_composited = ViewImpl()->widgets_never_composited();

  if (g_create_web_frame_widget) {
    // It is safe to cast to WebFrameWidgetImpl because the only concrete
    // subclass of WebFrameWidget that is allowed is WebFrameWidgetImpl. This
    // is enforced via a private constructor (and friend class) on
    // WebFrameWidget.
    frame_widget_ =
        static_cast<WebFrameWidgetImpl*>(g_create_web_frame_widget->Run(
            std::move(pass_key), std::move(mojo_frame_widget_host),
            std::move(mojo_frame_widget), std::move(mojo_widget_host),
            std::move(mojo_widget),
            Scheduler()->GetAgentGroupScheduler()->DefaultTaskRunner(),
            frame_sink_id, hidden, never_composited, is_for_child_local_root,
            is_for_nested_main_frame, is_for_scalable_page));
  } else {
    frame_widget_ = MakeGarbageCollected<WebFrameWidgetImpl>(
        std::move(pass_key), std::move(mojo_frame_widget_host),
        std::move(mojo_frame_widget), std::move(mojo_widget_host),
        std::move(mojo_widget),
        Scheduler()->GetAgentGroupScheduler()->DefaultTaskRunner(),
        frame_sink_id, hidden, never_composited, is_for_child_local_root,
        is_for_nested_main_frame, is_for_scalable_page);
  }
  frame_widget_->BindLocalRoot(*this);

  // If this is for a main frame grab the associated WebViewImpl and
  // assign this widget as the main frame widget.
  // Note: this can't DCHECK that the view's main frame points to
  // |this|, as provisional frames violate this precondition.
  if (!is_for_child_local_root) {
    DCHECK(ViewImpl());
    ViewImpl()->SetMainFrameViewWidget(frame_widget_);
  }
}

WebFrameWidget* WebLocalFrameImpl::FrameWidget() const {
  return frame_widget_.Get();
}

void WebLocalFrameImpl::CopyImageAtForTesting(
    const gfx::Point& pos_in_viewport) {
  GetFrame()->CopyImageAtViewportPoint(pos_in_viewport);
}

void WebLocalFrameImpl::ShowContextMenuFromExternal(
    const UntrustworthyContextMenuParams& params,
    CrossVariantMojoAssociatedRemote<
        mojom::blink::ContextMenuClientInterfaceBase> context_menu_client) {
  GetFrame()->GetLocalFrameHostRemote().ShowContextMenu(
      std::move(context_menu_client), params);
}

void WebLocalFrameImpl::ShowContextMenu(
    mojo::PendingAssociatedRemote<mojom::blink::ContextMenuClient> client,
    const blink::ContextMenuData& data,
    const std::optional<gfx::Point>& host_context_menu_location) {
  UntrustworthyContextMenuParams params =
      blink::ContextMenuParamsBuilder::Build(data);
  if (host_context_menu_location.has_value()) {
    // If the context menu request came from the browser, it came with a
    // position that was stored on blink::WebFrameWidgetImpl and is relative to
    // the WindowScreenRect.
    params.x = host_context_menu_location.value().x();
    params.y = host_context_menu_location.value().y();
  } else {
    // If the context menu request came from the renderer, the position in
    // |params| is real, but they come in blink viewport coordinates, which
    // include the device scale factor, but not emulation scale. Here we convert
    // them to DIP coordinates relative to the WindowScreenRect.
    // TODO(crbug.com/1093904): This essentially is a floor of the coordinates.
    // Determine if rounding is more appropriate.
    gfx::Rect position_in_dips =
        LocalRootFrameWidget()->BlinkSpaceToEnclosedDIPs(
            gfx::Rect(params.x, params.y, 0, 0));

    const float scale = LocalRootFrameWidget()->GetEmulatorScale();
    params.x = position_in_dips.x() * scale;
    params.y = position_in_dips.y() * scale;
  }

  // Serializing a GURL longer than kMaxURLChars will fail, so don't do
  // it.  We replace it with an empty GURL so the appropriate items are disabled
  // in the context menu.
  // TODO(jcivelli): http://crbug.com/45160 This prevents us from saving large
  //                 data encoded images.  We should have a way to save them.
  if (params.src_url.spec().size() > url::kMaxURLChars)
    params.src_url = GURL();

  params.selection_rect =
      LocalRootFrameWidget()->BlinkSpaceToEnclosedDIPs(data.selection_rect);

  if (!GetFrame())
    return;
  GetFrame()->GetLocalFrameHostRemote().ShowContextMenu(std::move(client),
                                                        params);

  if (Client())
    Client()->UpdateContextMenuDataForTesting(data, host_context_menu_location);
}

bool WebLocalFrameImpl::IsAllowedToDownload() const {
  if (!GetFrame())
    return true;

  return (GetFrame()->Loader().PendingEffectiveSandboxFlags() &
          network::mojom::blink::WebSandboxFlags::kDownloads) ==
         network::mojom::blink::WebSandboxFlags::kNone;
}

bool WebLocalFrameImpl::IsCrossOriginToOutermostMainFrame() const {
  return GetFrame()->IsCrossOriginToOutermostMainFrame();
}

void WebLocalFrameImpl::UsageCountChromeLoadTimes(const WebString& metric) {
  WebFeature feature = WebFeature::kChromeLoadTimesUnknown;
  if (metric == "requestTime") {
    feature = WebFeature::kChromeLoadTimesRequestTime;
  } else if (metric == "startLoadTime") {
    feature = WebFeature::kChromeLoadTimesStartLoadTime;
  } else if (metric == "commitLoadTime") {
    feature = WebFeature::kChromeLoadTimesCommitLoadTime;
  } else if (metric == "finishDocumentLoadTime") {
    feature = WebFeature::kChromeLoadTimesFinishDocumentLoadTime;
  } else if (metric == "finishLoadTime") {
    feature = WebFeature::kChromeLoadTimesFinishLoadTime;
  } else if (metric == "firstPaintTime") {
    feature = WebFeature::kChromeLoadTimesFirstPaintTime;
  } else if (metric == "firstPaintAfterLoadTime") {
    feature = WebFeature::kChromeLoadTimesFirstPaintAfterLoadTime;
  } else if (metric == "navigationType") {
    feature = WebFeature::kChromeLoadTimesNavigationType;
  } else if (metric == "wasFetchedViaSpdy") {
    feature = WebFeature::kChromeLoadTimesWasFetchedViaSpdy;
  } else if (metric == "wasNpnNegotiated") {
    feature = WebFeature::kChromeLoadTimesWasNpnNegotiated;
  } else if (metric == "npnNegotiatedProtocol") {
    feature = WebFeature::kChromeLoadTimesNpnNegotiatedProtocol;
  } else if (metric == "wasAlternateProtocolAvailable") {
    feature = WebFeature::kChromeLoadTimesWasAlternateProtocolAvailable;
  } else if (metric == "connectionInfo") {
    feature = WebFeature::kChromeLoadTimesConnectionInfo;
  }
  Deprecation::CountDeprecation(GetFrame()->DomWindow(), feature);
}

void WebLocalFrameImpl::UsageCountChromeCSI(const WebString& metric) {
  CHECK(GetFrame());
  WebFeature feature = WebFeature::kChromeCSIUnknown;
  if (metric == "onloadT") {
    feature = WebFeature::kChromeCSIOnloadT;
  } else if (metric == "pageT") {
    feature = WebFeature::kChromeCSIPageT;
  } else if (metric == "startE") {
    feature = WebFeature::kChromeCSIStartE;
  } else if (metric == "tran") {
    feature = WebFeature::kChromeCSITran;
  }
  GetFrame()->DomWindow()->CountUse(feature);
}

FrameScheduler* WebLocalFrameImpl::Scheduler() const {
  return GetFrame()->GetFrameScheduler();
}

scheduler::WebAgentGroupScheduler* WebLocalFrameImpl::GetAgentGroupScheduler()
    const {
  return &ViewImpl()->GetWebAgentGroupScheduler();
}

scoped_refptr<base::SingleThreadTaskRunner> WebLocalFrameImpl::GetTaskRunner(
    TaskType task_type) {
  return GetFrame()->GetTaskRunner(task_type);
}

WebInputMethodController* WebLocalFrameImpl::GetInputMethodController() {
  return &input_method_controller_;
}

bool WebLocalFrameImpl::ShouldSuppressKeyboardForFocusedElement() {
  if (!autofill_client_)
    return false;

  DCHECK(GetFrame()->GetDocument());
  auto* focused_form_control_element = DynamicTo<HTMLFormControlElement>(
      GetFrame()->GetDocument()->FocusedElement());
  return focused_form_control_element &&
         autofill_client_->ShouldSuppressKeyboard(focused_form_control_element);
}

void WebLocalFrameImpl::AddMessageToConsoleImpl(
    const WebConsoleMessage& message,
    bool discard_duplicates) {
  DCHECK(GetFrame());
  GetFrame()->GetDocument()->AddConsoleMessage(
      MakeGarbageCollected<ConsoleMessage>(message, GetFrame()),
      discard_duplicates);
}

// This is only triggered by test_runner.cc
void WebLocalFrameImpl::AddInspectorIssueImpl(
    mojom::blink::InspectorIssueCode code) {
  DCHECK(GetFrame());
  auto info = mojom::blink::InspectorIssueInfo::New(
      code, mojom::blink::InspectorIssueDetails::New());
  GetFrame()->AddInspectorIssue(
      AuditsIssue(ConvertInspectorIssueToProtocolFormat(
          InspectorIssue::Create(std::move(info)))));
}

void WebLocalFrameImpl::AddGenericIssueImpl(
    mojom::blink::GenericIssueErrorType error_type,
    int violating_node_id) {
  DCHECK(GetFrame());
  AuditsIssue::ReportGenericIssue(GetFrame(), error_type, violating_node_id);
}

void WebLocalFrameImpl::AddGenericIssueImpl(
    mojom::blink::GenericIssueErrorType error_type,
    int violating_node_id,
    const WebString& violating_node_attribute) {
  DCHECK(GetFrame());
  AuditsIssue::ReportGenericIssue(GetFrame(), error_type, violating_node_id,
                                  violating_node_attribute);
}

void WebLocalFrameImpl::SetTextCheckClient(
    WebTextCheckClient* text_check_client) {
  text_check_client_ = text_check_client;
}

void WebLocalFrameImpl::SetSpellCheckPanelHostClient(
    WebSpellCheckPanelHostClient* spell_check_panel_host_client) {
  spell_check_panel_host_client_ = spell_check_panel_host_client;
}

WebFrameWidgetImpl* WebLocalFrameImpl::LocalRootFrameWidget() {
  CHECK(LocalRoot());
  return LocalRoot()->FrameWidgetImpl();
}

Node* WebLocalFrameImpl::ContextMenuNodeInner() const {
  if (!ViewImpl() || !ViewImpl()->GetPage())
    return nullptr;
  return ViewImpl()
      ->GetPage()
      ->GetContextMenuController()
      .ContextMenuNodeForFrame(GetFrame());
}

Node* WebLocalFrameImpl::ContextMenuImageNodeInner() const {
  if (!ViewImpl() || !ViewImpl()->GetPage())
    return nullptr;
  return ViewImpl()
      ->GetPage()
      ->GetContextMenuController()
      .ContextMenuImageNodeForFrame(GetFrame());
}

void WebLocalFrameImpl::WaitForDebuggerWhenShown() {
  DCHECK(frame_->IsLocalRoot());
  DevToolsAgentImpl(/*create_if_necessary=*/true)->WaitForDebuggerWhenShown();
}

WebDevToolsAgentImpl* WebLocalFrameImpl::DevToolsAgentImpl(
    bool create_if_necessary) {
  if (!frame_->IsLocalRoot()) {
    return nullptr;
  }
  if (!dev_tools_agent_ && create_if_necessary) {
    dev_tools_agent_ = WebDevToolsAgentImpl::CreateForFrame(this);
  }
  return dev_tools_agent_.Get();
}

void WebLocalFrameImpl::WasHidden() {
  if (frame_)
    frame_->WasHidden();
}

void WebLocalFrameImpl::WasShown() {
  if (frame_)
    frame_->WasShown();
}

void WebLocalFrameImpl::SetAllowsCrossBrowsingInstanceFrameLookup() {
  DCHECK(GetFrame());

  // Allow the frame's security origin to access other SecurityOrigins
  // that match everything except the agent cluster check. This is needed
  // for embedders that hand out frame references outside of a browsing
  // instance, for example extensions and webview tag.
  auto* window = GetFrame()->DomWindow();
  window->GetMutableSecurityOrigin()->GrantCrossAgentClusterAccess();
}

WebHistoryItem WebLocalFrameImpl::GetCurrentHistoryItem() const {
  return WebHistoryItem(current_history_item_);
}

void WebLocalFrameImpl::SetLocalStorageArea(
    CrossVariantMojoRemote<mojom::StorageAreaInterfaceBase>
        local_storage_area) {
  CoreInitializer::GetInstance().SetLocalStorageArea(
      *GetFrame(), std::move(local_storage_area));
}

void WebLocalFrameImpl::SetSessionStorageArea(
    CrossVariantMojoRemote<mojom::StorageAreaInterfaceBase>
        session_storage_area) {
  CoreInitializer::GetInstance().SetSessionStorageArea(
      *GetFrame(), std::move(session_storage_area));
}

void WebLocalFrameImpl::SetNotRestoredReasons(
    const mojom::BackForwardCacheNotRestoredReasonsPtr& not_restored_reasons) {
  GetFrame()->SetNotRestoredReasons(
      ConvertNotRestoredReasons(not_restored_reasons));
}

const mojom::blink::BackForwardCacheNotRestoredReasonsPtr&
WebLocalFrameImpl::GetNotRestoredReasons() {
  return GetFrame()->GetNotRestoredReasons();
}

mojom::blink::BackForwardCacheNotRestoredReasonsPtr
WebLocalFrameImpl::ConvertNotRestoredReasons(
    const mojom::BackForwardCacheNotRestoredReasonsPtr& reasons_to_copy) {
  mojom::blink::BackForwardCacheNotRestoredReasonsPtr not_restored_reasons;
  if (!reasons_to_copy.is_null()) {
    not_restored_reasons =
        mojom::blink::BackForwardCacheNotRestoredReasons::New();
    if (reasons_to_copy->id) {
      not_restored_reasons->id = reasons_to_copy->id.value().c_str();
    }
    if (reasons_to_copy->name) {
      not_restored_reasons->name = reasons_to_copy->name.value().c_str();
    }
    if (reasons_to_copy->src) {
      not_restored_reasons->src = reasons_to_copy->src.value().c_str();
    }
    for (const auto& reason_to_copy : reasons_to_copy->reasons) {
      mojom::blink::BFCacheBlockingDetailedReasonPtr reason =
          mojom::blink::BFCacheBlockingDetailedReason::New();
      reason->name = WTF::String(reason_to_copy->name);
      if (reason_to_copy->source) {
        CHECK_GT(reason_to_copy->source->line_number, 0U);
        CHECK_GT(reason_to_copy->source->column_number, 0U);
        mojom::blink::ScriptSourceLocationPtr source_location =
            mojom::blink::ScriptSourceLocation::New(
                KURL(reason_to_copy->source->url),
                WTF::String(reason_to_copy->source->function_name),
                reason_to_copy->source->line_number,
                reason_to_copy->source->column_number);
        reason->source = std::move(source_location);
      }
      not_restored_reasons->reasons.push_back(std::move(reason));
    }
    if (reasons_to_copy->same_origin_details) {
      auto details = mojom::blink::SameOriginBfcacheNotRestoredDetails::New();
      details->url = KURL(reasons_to_copy->same_origin_details->url);
      for (const auto& child : reasons_to_copy->same_origin_details->children) {
        details->children.push_back(ConvertNotRestoredReasons(child));
      }
      not_restored_reasons->same_origin_details = std::move(details);
    }
  }
  return not_restored_reasons;
}

void WebLocalFrameImpl::SetLCPPHint(
    const mojom::LCPCriticalPathPredictorNavigationTimeHintPtr& hint) {
  LocalFrame* frame = GetFrame();
  if (!frame) {
    return;
  }

  LCPCriticalPathPredictor* lcpp = frame->GetLCPP();
  if (!lcpp) {
    return;
  }

  lcpp->Reset();

  if (!hint) {
    return;
  }

  lcpp->set_lcp_element_locators(hint->lcp_element_locators);

  HashSet<KURL> lcp_influencer_scripts;
  for (auto& url : hint->lcp_influencer_scripts) {
    lcp_influencer_scripts.insert(KURL(url));
  }
  lcpp->set_lcp_influencer_scripts(std::move(lcp_influencer_scripts));

  Vector<KURL> fetched_fonts;
  fetched_fonts.reserve(
      base::checked_cast<wtf_size_t>(hint->fetched_fonts.size()));
  for (const auto& url : hint->fetched_fonts) {
    fetched_fonts.emplace_back(url);
  }
  lcpp->set_fetched_fonts(std::move(fetched_fonts));

  Vector<url::Origin> preconnect_origins;
  preconnect_origins.reserve(
      base::checked_cast<wtf_size_t>(hint->preconnect_origins.size()));
  for (const auto& origin_url : hint->preconnect_origins) {
    preconnect_origins.emplace_back(url::Origin::Create(origin_url));
  }
  lcpp->set_preconnected_origins(preconnect_origins);

  Vector<KURL> unused_preloads;
  unused_preloads.reserve(
      base::checked_cast<wtf_size_t>(hint->unused_preloads.size()));
  for (const auto& url : hint->unused_preloads) {
    unused_preloads.emplace_back(url);
  }
  lcpp->set_unused_preloads(std::move(unused_preloads));
}

bool WebLocalFrameImpl::IsFeatureEnabled(
    const mojom::blink::PermissionsPolicyFeature& feature) const {
  return GetFrame()->DomWindow()->IsFeatureEnabled(feature);
}

void WebLocalFrameImpl::AddHitTestOnTouchStartCallback(
    base::RepeatingCallback<void(const blink::WebHitTestResult&)> callback) {
  TouchStartEventListener* touch_start_event_listener =
      MakeGarbageCollected<TouchStartEventListener>(std::move(callback));
  AddEventListenerOptionsResolved* options =
      MakeGarbageCollected<AddEventListenerOptionsResolved>();
  options->setPassive(true);
  options->SetPassiveSpecified(true);
  options->setCapture(true);
  GetFrame()->DomWindow()->addEventListener(
      event_type_names::kTouchstart, touch_start_event_listener, options);
}

void WebLocalFrameImpl::BlockParserForTesting() {
  // Avoid blocking for MHTML tests since MHTML archives are loaded
  // synchronously during commit. WebFrameTestProxy only has a chance to act at
  // DidCommit after that's happened.
  if (GetFrame()->Loader().GetDocumentLoader()->Archive()) {
    return;
  }
  GetFrame()->Loader().GetDocumentLoader()->BlockParser();
}

void WebLocalFrameImpl::ResumeParserForTesting() {
  if (GetFrame()->Loader().GetDocumentLoader()->Archive()) {
    return;
  }
  GetFrame()->Loader().GetDocumentLoader()->ResumeParser();
}

void WebLocalFrameImpl::FlushInputForTesting(base::OnceClosure done_callback) {
  frame_widget_->FlushInputForTesting(std::move(done_callback));
}

void WebLocalFrameImpl::SetTargetToCurrentHistoryItem(const WebString& target) {
  current_history_item_->SetTarget(target);
}

void WebLocalFrameImpl::UpdateCurrentHistoryItem() {
  current_history_item_ = WebHistoryItem(
      GetFrame()->Loader().GetDocumentLoader()->GetHistoryItem());
}

PageState WebLocalFrameImpl::CurrentHistoryItemToPageState() {
  return current_history_item_->ToPageState();
}

void WebLocalFrameImpl::ScrollFocusedEditableElementIntoView() {
  if (has_scrolled_focused_editable_node_into_rect_ && autofill_client_) {
    autofill_client_->DidCompleteFocusChangeInFrame();
    return;
  }

  WebFrameWidgetImpl* local_root_frame_widget = LocalRootFrameWidget();

  if (!local_root_frame_widget->ScrollFocusedEditableElementIntoView())
    return;

  has_scrolled_focused_editable_node_into_rect_ = true;
  if (!local_root_frame_widget->HasPendingPageScaleAnimation() &&
      autofill_client_) {
    autofill_client_->DidCompleteFocusChangeInFrame();
  }
}

void WebLocalFrameImpl::ResetHasScrolledFocusedEditableIntoView() {
  has_scrolled_focused_editable_node_into_rect_ = false;
}

void WebLocalFrameImpl::AddObserver(WebLocalFrameObserver* observer) {
  // Ensure that the frame is attached.
  DCHECK(GetFrame());
  observers_.AddObserver(observer);
}

void WebLocalFrameImpl::RemoveObserver(WebLocalFrameObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WebLocalFrameImpl::WillSendSubmitEvent(const WebFormElement& form) {
  for (auto& observer : observers_)
    observer.WillSendSubmitEvent(form);
}

bool WebLocalFrameImpl::AllowStorageAccessSyncAndNotify(
    WebContentSettingsClient::StorageType storage_type) {
  return GetFrame()->AllowStorageAccessSyncAndNotify(storage_type);
}

}  // namespace blink
