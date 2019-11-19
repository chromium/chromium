/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2008, 2009, 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/dom/document.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/input/scroll_snap_data.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom-blink.h"
#include "services/network/public/mojom/ip_address_space.mojom-blink.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/insecure_input/insecure_input_service.mojom-blink.h"
#include "third_party/blink/public/mojom/ukm/ukm.mojom-blink.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_prerendering_support.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/renderer/bindings/core/v8/html_script_element_or_svg_script_element.h"
#include "third_party/blink/renderer/bindings/core/v8/isolated_world_csp.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_element_creation_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v0_custom_element_constructor_builder.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_element_creation_options.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy.h"
#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/pending_animations.h"
#include "third_party/blink/renderer/core/animation/worklet_animation_controller.h"
#include "third_party/blink/renderer/core/aom/computed_accessible_node.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/cssom/computed_style_property_map.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/css/invalidation/style_invalidator.h"
#include "third_party/blink/renderer/core/css/media_query_matcher.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/font_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_stats.h"
#include "third_party/blink/renderer/core/css/selector_query.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/css/style_sheet_list.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/beforeunload_event_listener.h"
#include "third_party/blink/renderer/core/dom/cdata_section.h"
#include "third_party/blink/renderer/core/dom/comment.h"
#include "third_party/blink/renderer/core/dom/context_features.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/document_parser_timing.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_creation_options.h"
#include "third_party/blink/renderer/core/dom/element_data_cache.h"
#include "third_party/blink/renderer/core/dom/element_registration_options.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/live_node_list.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/node_child_removal_tracker.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_iterator.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/dom/node_rare_data.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/node_with_index.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/dom/scripted_animation_controller.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_recalc_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/dom/transform_source.h"
#include "third_party/blink/renderer/core/dom/tree_walker.h"
#include "third_party/blink/renderer/core/dom/visited_link_state.h"
#include "third_party/blink/renderer/core/dom/whitespace_attacher.h"
#include "third_party/blink/renderer/core/dom/xml_document.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/events/before_unload_event.h"
#include "third_party/blink/renderer/core/events/event_factory.h"
#include "third_party/blink/renderer/core/events/hash_change_event.h"
#include "third_party/blink/renderer/core/events/overscroll_event.h"
#include "third_party/blink/renderer/core/events/page_transition_event.h"
#include "third_party/blink/renderer/core/events/visual_viewport_resize_event.h"
#include "third_party/blink/renderer/core/events/visual_viewport_scroll_event.h"
#include "third_party/blink/renderer/core/execution_context/agent_metrics_collector.h"
#include "third_party/blink/renderer/core/execution_context/window_agent.h"
#include "third_party/blink/renderer/core/execution_context/window_agent_factory.h"
#include "third_party/blink/renderer/core/feature_policy/dom_document_policy.h"
#include "third_party/blink/renderer/core/feature_policy/feature_policy_parser.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/csp/navigation_initiator_impl.h"
#include "third_party/blink/renderer/core/frame/dom_timer.h"
#include "third_party/blink/renderer/core/frame/dom_visual_viewport.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/feature_policy_violation_report_body.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/history.h"
#include "third_party/blink/renderer/core/frame/intervention.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/performance_monitor.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/anchor_element_metrics.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_font_cache.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_descriptor.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_microtask_run_queue.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_registration_context.h"
#include "third_party/blink/renderer/core/html/document_all_name_collection.h"
#include "third_party/blink/renderer/core/html/document_name_collection.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_all_collection.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_base_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/html/html_title_element.h"
#include "third_party/blink/renderer/core/html/html_unknown_element.h"
#include "third_party/blink/renderer/core/html/imports/html_import_loader.h"
#include "third_party/blink/renderer/core/html/imports/html_imports_controller.h"
#include "third_party/blink/renderer/core/html/lazy_load_image_observer.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/parser/nesting_level_incrementer.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/html/plugin_document.h"
#include "third_party/blink/renderer/core/html/portal/document_portals.h"
#include "third_party/blink/renderer/core/html/portal/portal_contents.h"
#include "third_party/blink/renderer/core/html/window_name_collection.h"
#include "third_party/blink/renderer/core/html_element_factory.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/touch_list.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/hit_test_canvas_result.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache_host_for_frame.h"
#include "third_party/blink/renderer/core/loader/cookie_jar.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_fetch_context.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/http_refresh_scheduler.h"
#include "third_party/blink/renderer/core/loader/idleness_detector.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/loader/prerenderer_client.h"
#include "third_party/blink/renderer/core/loader/progress_tracker.h"
#include "third_party/blink/renderer/core/loader/text_resource_decoder_builder.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"
#include "third_party/blink/renderer/core/mathml_element_factory.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/event_with_hit_test_results.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/plugin_script_forbidden_scope.h"
#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/overscroll_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/scroll_state_callback.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/snap_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_anchor.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/page/spatial_navigation_controller.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/first_meaningful_paint_detector.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_controller.h"
#include "third_party/blink/renderer/core/script/script_runner.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"
#include "third_party/blink/renderer/core/svg/svg_script_element.h"
#include "third_party/blink/renderer/core/svg/svg_title_element.h"
#include "third_party/blink/renderer/core/svg/svg_unknown_element.h"
#include "third_party/blink/renderer/core/svg/svg_use_element.h"
#include "third_party/blink/renderer/core/svg_element_factory.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"
#include "third_party/blink/renderer/core/xml/parser/xml_document_parser.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/core/xmlns_names.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/document_resource_coordinator.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/loader/fetch/null_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/dummy_schedulers.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/weborigin/origin_access_entry.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"

#ifndef NDEBUG
using WeakDocumentSet = blink::HeapHashSet<blink::WeakMember<blink::Document>>;
static WeakDocumentSet& liveDocumentSet();
#endif

namespace blink {

namespace {

// Returns true if any of <object> ancestors don't start loading or are loading
// plugins/frames/images. If there are no <object> ancestors, this function
// returns false.
bool IsInIndeterminateObjectAncestor(const Element* element) {
  if (!element->isConnected())
    return false;
  for (; element; element = element->ParentOrShadowHostElement()) {
    if (const auto* object = DynamicTo<HTMLObjectElement>(element)) {
      if (!object->DidFinishLoading())
        return true;
    }
  }
  return false;
}

}  // namespace

class DocumentOutliveTimeReporter : public BlinkGCObserver {
 public:
  explicit DocumentOutliveTimeReporter(Document* document)
      : BlinkGCObserver(ThreadState::Current()), document_(document) {}

  ~DocumentOutliveTimeReporter() override {
    // As not all documents are destroyed before the process dies, this might
    // miss some long-lived documents or leaked documents.
    UMA_HISTOGRAM_EXACT_LINEAR(
        "Document.OutliveTimeAfterShutdown.DestroyedBeforeProcessDies",
        GetOutliveTimeCount() + 1, 101);
  }

  void OnCompleteSweepDone() override {
    enum GCCount {
      kGCCount5,
      kGCCount10,
      kGCCountMax,
    };

    // There are some cases that a document can live after shutting down because
    // the document can still be referenced (e.g. a document opened via
    // window.open can be referenced by the opener even after shutting down). To
    // avoid such cases as much as possible, outlive time count is started after
    // all DomWrapper of the document have disappeared.
    if (!gc_age_when_document_detached_) {
      if (document_->domWindow() &&
          DOMWrapperWorld::HasWrapperInAnyWorldInMainThread(
              document_->domWindow())) {
        return;
      }
      gc_age_when_document_detached_ = ThreadState::Current()->GcAge();
    }

    int outlive_time_count = GetOutliveTimeCount();
    if (outlive_time_count == 5 || outlive_time_count == 10) {
      const char* kUMAString = "Document.OutliveTimeAfterShutdown.GCCount";

      if (outlive_time_count == 5)
        UMA_HISTOGRAM_ENUMERATION(kUMAString, kGCCount5, kGCCountMax);
      else if (outlive_time_count == 10)
        UMA_HISTOGRAM_ENUMERATION(kUMAString, kGCCount10, kGCCountMax);
      else
        NOTREACHED();
    }

    if (outlive_time_count == 5 || outlive_time_count == 10 ||
        outlive_time_count == 20 || outlive_time_count == 50) {
      document_->RecordUkmOutliveTimeAfterShutdown(outlive_time_count);
    }
  }

 private:
  int GetOutliveTimeCount() const {
    if (!gc_age_when_document_detached_)
      return 0;
    return ThreadState::Current()->GcAge() - gc_age_when_document_detached_;
  }

  WeakPersistent<Document> document_;
  int gc_age_when_document_detached_ = 0;
};

static const unsigned kCMaxWriteRecursionDepth = 21;

// This amount of time must have elapsed before we will even consider scheduling
// a layout without a delay.
// FIXME: For faster machines this value can really be lowered to 200.  250 is
// adequate, but a little high for dual G5s. :)
static const base::TimeDelta kCLayoutScheduleThreshold =
    base::TimeDelta::FromMilliseconds(250);

// DOM Level 2 says (letters added):
//
// a) Name start characters must have one of the categories Ll, Lu, Lo, Lt, Nl.
// b) Name characters other than Name-start characters must have one of the
//    categories Mc, Me, Mn, Lm, or Nd.
// c) Characters in the compatibility area (i.e. with character code greater
//    than #xF900 and less than #xFFFE) are not allowed in XML names.
// d) Characters which have a font or compatibility decomposition (i.e. those
//    with a "compatibility formatting tag" in field 5 of the database -- marked
//    by field 5 beginning with a "<") are not allowed.
// e) The following characters are treated as name-start characters rather than
//    name characters, because the property file classifies them as Alphabetic:
//    [#x02BB-#x02C1], #x0559, #x06E5, #x06E6.
// f) Characters #x20DD-#x20E0 are excluded (in accordance with Unicode, section
//    5.14).
// g) Character #x00B7 is classified as an extender, because the property list
//    so identifies it.
// h) Character #x0387 is added as a name character, because #x00B7 is its
//    canonical equivalent.
// i) Characters ':' and '_' are allowed as name-start characters.
// j) Characters '-' and '.' are allowed as name characters.
//
// It also contains complete tables. If we decide it's better, we could include
// those instead of the following code.

static inline bool IsValidNameStart(UChar32 c) {
  // rule (e) above
  if ((c >= 0x02BB && c <= 0x02C1) || c == 0x559 || c == 0x6E5 || c == 0x6E6)
    return true;

  // rule (i) above
  if (c == ':' || c == '_')
    return true;

  // rules (a) and (f) above
  const uint32_t kNameStartMask =
      WTF::unicode::kLetter_Lowercase | WTF::unicode::kLetter_Uppercase |
      WTF::unicode::kLetter_Other | WTF::unicode::kLetter_Titlecase |
      WTF::unicode::kNumber_Letter;
  if (!(WTF::unicode::Category(c) & kNameStartMask))
    return false;

  // rule (c) above
  if (c >= 0xF900 && c < 0xFFFE)
    return false;

  // rule (d) above
  WTF::unicode::CharDecompositionType decomp_type =
      WTF::unicode::DecompositionType(c);
  if (decomp_type == WTF::unicode::kDecompositionFont ||
      decomp_type == WTF::unicode::kDecompositionCompat)
    return false;

  return true;
}

static inline bool IsValidNamePart(UChar32 c) {
  // rules (a), (e), and (i) above
  if (IsValidNameStart(c))
    return true;

  // rules (g) and (h) above
  if (c == 0x00B7 || c == 0x0387)
    return true;

  // rule (j) above
  if (c == '-' || c == '.')
    return true;

  // rules (b) and (f) above
  const uint32_t kOtherNamePartMask =
      WTF::unicode::kMark_NonSpacing | WTF::unicode::kMark_Enclosing |
      WTF::unicode::kMark_SpacingCombining | WTF::unicode::kLetter_Modifier |
      WTF::unicode::kNumber_DecimalDigit;
  if (!(WTF::unicode::Category(c) & kOtherNamePartMask))
    return false;

  // rule (c) above
  if (c >= 0xF900 && c < 0xFFFE)
    return false;

  // rule (d) above
  WTF::unicode::CharDecompositionType decomp_type =
      WTF::unicode::DecompositionType(c);
  if (decomp_type == WTF::unicode::kDecompositionFont ||
      decomp_type == WTF::unicode::kDecompositionCompat)
    return false;

  return true;
}

// Tests whether |name| is something the HTML parser would accept as a
// tag name.
template <typename CharType>
static inline bool IsValidElementNamePerHTMLParser(const CharType* characters,
                                                   unsigned length) {
  CharType c = characters[0] | 0x20;
  if (!('a' <= c && c <= 'z'))
    return false;

  for (unsigned i = 1; i < length; ++i) {
    c = characters[i];
    if (c == '\t' || c == '\n' || c == '\f' || c == '\r' || c == ' ' ||
        c == '/' || c == '>')
      return false;
  }

  return true;
}

static bool IsValidElementNamePerHTMLParser(const String& name) {
  unsigned length = name.length();
  if (!length)
    return false;

  if (name.Is8Bit()) {
    const LChar* characters = name.Characters8();
    return IsValidElementNamePerHTMLParser(characters, length);
  }
  const UChar* characters = name.Characters16();
  return IsValidElementNamePerHTMLParser(characters, length);
}

// Tests whether |name| is a valid name per DOM spec. Also checks
// whether the HTML parser would accept this element name and counts
// cases of mismatches.
static bool IsValidElementName(Document* document, const String& name) {
  bool is_valid_dom_name = Document::IsValidName(name);
  bool is_valid_html_name = IsValidElementNamePerHTMLParser(name);
  if (UNLIKELY(is_valid_html_name != is_valid_dom_name)) {
    // This is inaccurate because it will not report activity in
    // detached documents. However retrieving the frame from the
    // bindings is too slow.
    UseCounter::Count(document,
                      is_valid_dom_name
                          ? WebFeature::kElementNameDOMValidHTMLParserInvalid
                          : WebFeature::kElementNameDOMInvalidHTMLParserValid);
  }
  return is_valid_dom_name;
}

static bool AcceptsEditingFocus(const Element& element) {
  DCHECK(HasEditableStyle(element));

  return element.GetDocument().GetFrame() && RootEditableElement(element);
}

uint64_t Document::global_tree_version_ = 0;

static bool g_threaded_parsing_enabled_for_testing = true;

class Document::NetworkStateObserver final
    : public GarbageCollected<Document::NetworkStateObserver>,
      public NetworkStateNotifier::NetworkStateObserver,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(Document::NetworkStateObserver);

 public:
  explicit NetworkStateObserver(Document& document)
      : ContextLifecycleObserver(&document) {
    online_observer_handle_ = GetNetworkStateNotifier().AddOnLineObserver(
        this, GetExecutionContext()->GetTaskRunner(TaskType::kNetworking));
  }

  void OnLineStateChange(bool on_line) override {
    AtomicString event_name =
        on_line ? event_type_names::kOnline : event_type_names::kOffline;
    Document* document = To<Document>(GetExecutionContext());
    if (!document->domWindow())
      return;
    document->domWindow()->DispatchEvent(*Event::Create(event_name));
    probe::NetworkStateChanged(document->GetFrame(), on_line);
  }

  void ContextDestroyed(ExecutionContext* context) override {
    UnregisterAsObserver(context);
  }

  void UnregisterAsObserver(ExecutionContext* context) {
    DCHECK(context);
    online_observer_handle_ = nullptr;
  }

  void Trace(Visitor* visitor) override {
    ContextLifecycleObserver::Trace(visitor);
  }

 private:
  std::unique_ptr<NetworkStateNotifier::NetworkStateObserverHandle>
      online_observer_handle_;
};

// A helper class that allows the security context be initialized in the
// process of constructing the document.
class Document::SecurityContextInit : public FeaturePolicyParserDelegate {
  STACK_ALLOCATED();

 public:
  SecurityContextInit(const DocumentInit& initializer,
                      DocumentClassFlags document_classes) {
    // Content Security Policy can provide sandbox flags. In CSP
    // 'self' will be determined when the policy is bound. That occurs
    // once the document is constructed.
    InitializeContentSecurityPolicy(initializer, document_classes);

    // Sandbox flags can come from initializer, loader or CSP.
    InitializeSandboxFlags(initializer);

    // The origin can be opaque based on sandbox flags.
    InitializeOrigin(initializer);

    // Initialize origin trials, requires the post sandbox flags
    // security origin.
    InitializeOriginTrials(initializer);

    // Initialize feature policy, depends on origin trials.
    InitializeFeaturePolicy(initializer, document_classes);

    // Initialize the agent. Depends on security origin.
    InitializeAgent(initializer);
  }

  const scoped_refptr<SecurityOrigin>& GetSecurityOrigin() const {
    return security_origin_;
  }

  WebSandboxFlags GetSandboxFlags() { return sandbox_flags_; }

  ContentSecurityPolicy* GetCSP() const { return csp_; }

  std::unique_ptr<FeaturePolicy> TakeFeaturePolicy() {
    DCHECK(feature_policy_);
    return std::move(feature_policy_);
  }

  const Vector<String>& FeaturePolicyParseMessages() const {
    return feature_policy_parse_messages_;
  }
  const ParsedFeaturePolicy& ParsedHeader() const { return parsed_header_; }

  OriginTrialContext* GetOriginTrialContext() { return origin_trials_; }

  Agent* GetAgent() { return agent_; }

  void CountFeaturePolicyUsage(mojom::WebFeature feature) override {
    feature_count_.insert(feature);
  }

  bool FeaturePolicyFeatureObserved(
      mojom::FeaturePolicyFeature feature) override {
    if (parsed_feature_policies_.Contains(feature))
      return true;
    parsed_feature_policies_.insert(feature);
    return false;
  }

  bool FeatureEnabled(OriginTrialFeature feature) const override {
    return origin_trials_->IsFeatureEnabled(feature);
  }

  void ApplyPendingDataToDocument(Document& document) {
    for (auto feature : feature_count_)
      UseCounter::Count(document, feature);
    for (auto feature : parsed_feature_policies_)
      document.FeaturePolicyFeatureObserved(feature);
  }

  bool BindCSPImmediately() const { return bind_csp_immediately_; }

 private:
  void InitializeContentSecurityPolicy(const DocumentInit& initializer,
                                       DocumentClassFlags document_classes) {
    auto* frame = initializer.GetFrame();
    ContentSecurityPolicy* last_origin_document_csp =
        frame ? frame->Loader().GetLastOriginDocumentCSP() : nullptr;

    KURL url;
    if (initializer.ShouldSetURL())
      url = initializer.Url().IsEmpty() ? BlankURL() : initializer.Url();

    // Alias certain security properties from |owner_document|. Used for the
    // case of about:blank pages inheriting the security properties of their
    // requestor context.
    //
    // Note that this is currently somewhat broken; Blink always inherits from
    // the parent or opener, even though it should actually be inherited from
    // the request initiator.
    if (url.IsEmpty() && initializer.HasSecurityContext() &&
        !initializer.OriginToCommit() && initializer.OwnerDocument()) {
      last_origin_document_csp =
          initializer.OwnerDocument()->GetContentSecurityPolicy();
    }

    csp_ = initializer.GetContentSecurityPolicy();

    if (!csp_) {
      if (initializer.ImportsController()) {
        // If this document is an HTML import, grab a reference to its master
        // document's Content Security Policy. We don't bind the CSP's delegate
        // in 'InitSecurityPolicy' in this case, as we can't rebind the master
        // document's policy object: The Content Security Policy's delegate
        // needs to remain set to the master document.
        csp_ = initializer.ImportsController()
                   ->Master()
                   ->GetContentSecurityPolicy();
        return;
      }

      csp_ = MakeGarbageCollected<ContentSecurityPolicy>();
      bind_csp_immediately_ = true;
    }

    // We should inherit the navigation initiator CSP if the document is loaded
    // using a local-scheme url.
    //
    // Note: about:srcdoc inherits CSP from its parent, not from its initiator.
    // In this case, the initializer.GetContentSecurityPolicy() is used.
    if (last_origin_document_csp && !url.IsAboutSrcdocURL() &&
        (url.IsEmpty() || url.ProtocolIsAbout() || url.ProtocolIsData() ||
         url.ProtocolIs("blob") || url.ProtocolIs("filesystem"))) {
      csp_->CopyStateFrom(last_origin_document_csp);
    }

    if (document_classes & kPluginDocumentClass) {
      if (last_origin_document_csp) {
        csp_->CopyPluginTypesFrom(last_origin_document_csp);
        return;
      }

      // TODO(andypaicu): This should inherit the origin document's plugin types
      // but because this could be a OOPIF document it might not have access. In
      // this situation we fallback on using the parent/opener:
      if (frame) {
        Frame* inherit_from = frame->Tree().Parent()
                                  ? frame->Tree().Parent()
                                  : frame->Client()->Opener();
        if (inherit_from && frame != inherit_from) {
          csp_->CopyPluginTypesFrom(
              inherit_from->GetSecurityContext()->GetContentSecurityPolicy());
        }
      }
    }
  }

  void InitializeSandboxFlags(const DocumentInit& initializer) {
    sandbox_flags_ = initializer.GetSandboxFlags() | csp_->GetSandboxMask();
    auto* frame = initializer.GetFrame();
    if (frame && frame->Loader().GetDocumentLoader()->Archive()) {
      // The URL of a Document loaded from a MHTML archive is controlled by
      // the Content-Location header. This would allow UXSS, since
      // Content-Location can be arbitrarily controlled to control the
      // Document's URL and origin. Instead, force a Document loaded from a
      // MHTML archive to be sandboxed, providing exceptions only for creating
      // new windows.
      sandbox_flags_ |=
          (WebSandboxFlags::kAll &
           ~(WebSandboxFlags::kPopups |
             WebSandboxFlags::kPropagatesToAuxiliaryBrowsingContexts));
    }
  }

  void InitializeOrigin(const DocumentInit& initializer) {
    scoped_refptr<SecurityOrigin> document_origin =
        initializer.GetDocumentOrigin();
    if ((sandbox_flags_ & WebSandboxFlags::kOrigin) != WebSandboxFlags::kNone) {
      scoped_refptr<SecurityOrigin> sandboxed_origin =
          initializer.OriginToCommit()
              ? initializer.OriginToCommit()
              : document_origin->DeriveNewOpaqueOrigin();

      // If we're supposed to inherit our security origin from our
      // owner, but we're also sandboxed, the only things we inherit are
      // the origin's potential trustworthiness and the ability to
      // load local resources. The latter lets about:blank iframes in
      // file:// URL documents load images and other resources from
      // the file system.
      //
      // Note: Sandboxed about:srcdoc iframe without "allow-same-origin" aren't
      // allowed to load user's file, even if its parent can.
      if (initializer.OwnerDocument()) {
        if (document_origin->IsPotentiallyTrustworthy())
          sandboxed_origin->SetOpaqueOriginIsPotentiallyTrustworthy(true);
        if (document_origin->CanLoadLocalResources() &&
            !initializer.IsSrcdocDocument())
          sandboxed_origin->GrantLoadLocalResources();
      }
      security_origin_ = sandboxed_origin;
    } else {
      security_origin_ = document_origin;
    }

    // If we are a page popup in LayoutTests ensure we use the popup
    // owner's security origin so the tests can possibly access the
    // document via internals API.
    auto* frame = initializer.GetFrame();
    if (IsPagePopupRunningInWebTest(frame)) {
      security_origin_ = frame->PagePopupOwner()
                             ->GetDocument()
                             .GetSecurityOrigin()
                             ->IsolatedCopy();
    }

    if (initializer.HasSecurityContext()) {
      if (Settings* settings = initializer.GetSettings()) {
        if (!settings->GetWebSecurityEnabled()) {
          // Web security is turned off. We should let this document access
          // every other document. This is used primary by testing harnesses for
          // web sites.
          security_origin_->GrantUniversalAccess();
        } else if (security_origin_->IsLocal()) {
          if (settings->GetAllowUniversalAccessFromFileURLs()) {
            // Some clients want local URLs to have universal access, but that
            // setting is dangerous for other clients.
            security_origin_->GrantUniversalAccess();
          } else if (!settings->GetAllowFileAccessFromFileURLs()) {
            // Some clients do not want local URLs to have access to other local
            // URLs.
            security_origin_->BlockLocalAccessFromLocalOrigin();
          }
        }
      }
    }

    if (initializer.GrantLoadLocalResources())
      security_origin_->GrantLoadLocalResources();
  }

  void InitializeFeaturePolicy(const DocumentInit& initializer,
                               DocumentClassFlags document_classes) {
    auto* frame = initializer.GetFrame();
    // For a main frame, get inherited feature policy from the opener if any.
    const FeaturePolicy::FeatureState* opener_feature_state = nullptr;
    if (frame && frame->IsMainFrame() && !frame->OpenerFeatureState().empty()) {
      opener_feature_state = &frame->OpenerFeatureState();
    }

    parsed_header_ = FeaturePolicyParser::ParseHeader(
        initializer.FeaturePolicyHeader(), security_origin_,
        &feature_policy_parse_messages_, this);

    if (sandbox_flags_ != WebSandboxFlags::kNone &&
        RuntimeEnabledFeatures::FeaturePolicyForSandboxEnabled()) {
      // The sandbox flags might have come from CSP header or the browser; in
      // such cases the sandbox is not part of the container policy. They are
      // added to the header policy (which specifically makes sense in the case
      // of CSP sandbox).
      ApplySandboxFlagsToParsedFeaturePolicy(sandbox_flags_, parsed_header_);
    }

    ParsedFeaturePolicy container_policy;

    if (frame && frame->Owner()) {
      // TODO(chenleihu): Due to the data replication mechanism in
      // multi-process site-per-process environment, the container_policy
      // value in remote frame owner gets lazily updated only when the next
      // navigation is initiated on the remote frame. We need a more robust way
      // to enforce the validity of container_policy value.
      // https://crbug.com/972089
      if (frame->Owner()->IsRemote()) {
        container_policy = frame->Owner()->GetFramePolicy().container_policy;
      } else {
        container_policy = initializer.GetFramePolicy()
                               .value_or(FramePolicy())
                               .container_policy;
      }
    }

    // TODO(icelland): This is problematic querying sandbox flags before
    // feature policy is initialized.
    if (RuntimeEnabledFeatures::BlockingFocusWithoutUserActivationEnabled() &&
        frame && frame->Tree().Parent() &&
        (sandbox_flags_ & WebSandboxFlags::kNavigation) !=
            WebSandboxFlags::kNone) {
      // Enforcing the policy for sandbox frames (for context see
      // https://crbug.com/954349).
      DisallowFeatureIfNotPresent(
          mojom::FeaturePolicyFeature::kFocusWithoutUserActivation,
          container_policy);
    }

    const FeaturePolicy* parent_feature_policy = nullptr;
    if (frame && !frame->IsMainFrame()) {
      parent_feature_policy =
          frame->Tree().Parent()->GetSecurityContext()->GetFeaturePolicy();
    }

    // If we are a HTMLViewSourceDocument we use container, header or
    // inherited policies. https://crbug.com/898688
    if (document_classes & kViewSourceDocumentClass) {
      feature_policy_ = FeaturePolicy::CreateFromParentPolicy(
          nullptr, {}, security_origin_->ToUrlOrigin());
      return;
    }

    // Feature policy should either come from a parent in the case of an
    // embedded child frame, or from an opener if any when a new window is
    // created by an opener. A main frame without an opener would not have a
    // parent policy nor an opener feature state.
    DCHECK(!parent_feature_policy || !opener_feature_state);
    if (!opener_feature_state ||
        !RuntimeEnabledFeatures::FeaturePolicyForSandboxEnabled()) {
      feature_policy_ = FeaturePolicy::CreateFromParentPolicy(
          parent_feature_policy, container_policy,
          security_origin_->ToUrlOrigin());
    } else {
      DCHECK(!parent_feature_policy);
      feature_policy_ = FeaturePolicy::CreateWithOpenerPolicy(
          *opener_feature_state, security_origin_->ToUrlOrigin());
    }
    feature_policy_->SetHeaderPolicy(parsed_header_);
  }

  void InitializeOriginTrials(const DocumentInit& initializer) {
    origin_trials_ = MakeGarbageCollected<OriginTrialContext>();

    const String& header_value = initializer.OriginTrialsHeader();

    if (header_value.IsEmpty())
      return;
    std::unique_ptr<Vector<String>> tokens(
        OriginTrialContext::ParseHeaderValue(header_value));
    if (!tokens)
      return;
    origin_trials_->AddTokens(security_origin_.get(), true, *tokens);
  }

  void InitializeAgent(const DocumentInit& initializer) {
    auto* frame = initializer.GetFrame();

    // If we are a page popup in LayoutTests ensure we use the popup
    // owner's frame for looking up the Agent so the tests can possibly
    // access the document via internals API.
    if (IsPagePopupRunningInWebTest(frame)) {
      frame = frame->PagePopupOwner()->GetDocument().GetFrame();
    } else if (!frame) {
      if (Document* context_document = initializer.ContextDocument()) {
        frame = context_document->GetFrame();
      } else if (Document* owner_document = initializer.OwnerDocument()) {
        frame = owner_document->GetFrame();
      }
    }

    // If we are allowed to share our document with other windows then we need
    // to look at the window agent factory, otherwise we should create our own
    // window agent.
    if (frame && feature_policy_->IsFeatureEnabled(
                     mojom::FeaturePolicyFeature::kDocumentAccess)) {
      bool has_potential_universal_access_privilege = false;
      if (Settings* settings = frame->GetSettings()) {
        // TODO(keishi): Also check if AllowUniversalAccessFromFileURLs might
        // dynamically change.
        if (!settings->GetWebSecurityEnabled() ||
            settings->GetAllowUniversalAccessFromFileURLs())
          has_potential_universal_access_privilege = true;
      }
      agent_ = frame->window_agent_factory().GetAgentForOrigin(
          has_potential_universal_access_privilege,
          V8PerIsolateData::MainThreadIsolate(), security_origin_.get());
    } else {
      // ContextDocument is null only for Documents created in unit tests.
      // In that case, use a throw away WindowAgent.
      agent_ = MakeGarbageCollected<WindowAgent>(
          V8PerIsolateData::MainThreadIsolate());
    }

    // Derive possibly a new security origin that contains the cluster id.
    security_origin_ =
        security_origin_->GetOriginForAgentCluster(agent_->cluster_id());
  }

  bool IsPagePopupRunningInWebTest(LocalFrame* frame) {
    return frame && frame->GetPage()->GetChromeClient().IsPopup() &&
           WebTestSupport::IsRunningWebTest();
  }

  scoped_refptr<SecurityOrigin> security_origin_;
  WebSandboxFlags sandbox_flags_ = WebSandboxFlags::kNone;
  std::unique_ptr<FeaturePolicy> feature_policy_;
  Vector<String> feature_policy_parse_messages_;
  ParsedFeaturePolicy parsed_header_;
  Member<ContentSecurityPolicy> csp_;
  Member<OriginTrialContext> origin_trials_;
  Member<Agent> agent_;
  HashSet<mojom::FeaturePolicyFeature> parsed_feature_policies_;
  HashSet<mojom::WebFeature> feature_count_;
  bool bind_csp_immediately_ = false;
};

ExplicitlySetAttrElementsMap* Document::GetExplicitlySetAttrElementsMap(
    Element* element) {
  DCHECK(element);
  DCHECK(element->GetDocument() == this);
  auto add_result =
      element_explicitly_set_attr_elements_map_.insert(element, nullptr);
  if (add_result.is_new_entry) {
    add_result.stored_value->value =
        MakeGarbageCollected<ExplicitlySetAttrElementsMap>();
  }
  return add_result.stored_value->value;
}

Document* Document::Create(Document& document) {
  Document* new_document =
      MakeGarbageCollected<Document>(DocumentInit::Create()
                                         .WithContextDocument(&document)
                                         .WithURL(BlankURL())
                                         .WithOwnerDocument(&document));
  new_document->SetContextFeatures(document.GetContextFeatures());
  return new_document;
}

Document::Document() : Document(DocumentInit::Create()) {}

Document::Document(const DocumentInit& initializer,
                   DocumentClassFlags document_classes)
    : Document(initializer,
               SecurityContextInit(initializer, document_classes),
               document_classes) {}

Document::Document(const DocumentInit& initializer,
                   SecurityContextInit security_initializer,
                   DocumentClassFlags document_classes)
    : ContainerNode(nullptr, kCreateDocument),
      TreeScope(*this),
      SecurityContext(security_initializer.GetSecurityOrigin(),
                      security_initializer.GetSandboxFlags(),
                      security_initializer.TakeFeaturePolicy()),
      ExecutionContext(V8PerIsolateData::MainThreadIsolate(),
                       security_initializer.GetAgent(),
                       security_initializer.GetOriginTrialContext()),
      evaluate_media_queries_on_style_recalc_(false),
      pending_sheet_layout_(kNoLayoutWithPendingSheets),
      frame_(initializer.GetFrame()),
      // TODO(dcheng): Why does this need both a LocalFrame and LocalDOMWindow
      // pointer?
      dom_window_(frame_ ? frame_->DomWindow() : nullptr),
      imports_controller_(initializer.ImportsController()),
      context_document_(initializer.ContextDocument()),
      context_features_(ContextFeatures::DefaultSwitch()),
      http_refresh_scheduler_(MakeGarbageCollected<HttpRefreshScheduler>(this)),
      well_formed_(false),
      printing_(kNotPrinting),
      is_painting_preview_(false),
      compatibility_mode_(kNoQuirksMode),
      compatibility_mode_locked_(false),
      last_focus_type_(kWebFocusTypeNone),
      had_keyboard_event_(false),
      clear_focused_element_timer_(
          GetTaskRunner(TaskType::kInternalUserInteraction),
          this,
          &Document::ClearFocusedElementTimerFired),
      dom_tree_version_(++global_tree_version_),
      style_version_(0),
      listener_types_(0),
      mutation_observer_types_(0),
      visited_link_state_(MakeGarbageCollected<VisitedLinkState>(*this)),
      visually_ordered_(false),
      ready_state_(kComplete),
      parsing_state_(kFinishedParsing),
      contains_plugins_(false),
      ignore_destructive_write_count_(0),
      throw_on_dynamic_markup_insertion_count_(0),
      ignore_opens_during_unload_count_(0),
      markers_(MakeGarbageCollected<DocumentMarkerController>(*this)),
      css_target_(nullptr),
      was_discarded_(false),
      load_event_progress_(kLoadEventCompleted),
      is_freezing_in_progress_(false),
      script_runner_(MakeGarbageCollected<ScriptRunner>(this)),
      xml_version_("1.0"),
      xml_standalone_(kStandaloneUnspecified),
      has_xml_declaration_(0),
      design_mode_(false),
      is_running_exec_command_(false),
      has_annotated_regions_(false),
      annotated_regions_dirty_(false),
      document_classes_(document_classes),
      is_view_source_(false),
      saw_elements_in_known_namespaces_(false),
      is_srcdoc_document_(initializer.IsSrcdocDocument()),
      is_mobile_document_(false),
      layout_view_(nullptr),
      has_fullscreen_supplement_(false),
      load_event_delay_count_(0),
      // We already intentionally fire load event asynchronously and here we use
      // kDOMManipulation to ensure that we run onload() in order with other
      // callbacks (e.g. onloadstart()) per the spec.
      // See: https://html.spec.whatwg.org/#delay-the-load-event
      load_event_delay_timer_(GetTaskRunner(TaskType::kDOMManipulation),
                              this,
                              &Document::LoadEventDelayTimerFired),
      plugin_loading_timer_(GetTaskRunner(TaskType::kInternalLoading),
                            this,
                            &Document::PluginLoadingTimerFired),
      document_timing_(*this),
      write_recursion_is_too_deep_(false),
      write_recursion_depth_(0),
      current_frame_is_throttled_(false),
      registration_context_(initializer.RegistrationContext(this)),
      element_data_cache_clear_timer_(
          GetTaskRunner(TaskType::kInternalUserInteraction),
          this,
          &Document::ElementDataCacheClearTimerFired),
      timeline_(DocumentTimeline::Create(this)),
      pending_animations_(MakeGarbageCollected<PendingAnimations>(*this)),
      worklet_animation_controller_(
          MakeGarbageCollected<WorkletAnimationController>(this)),
      template_document_host_(nullptr),
      did_associate_form_controls_timer_(
          GetTaskRunner(TaskType::kInternalLoading),
          this,
          &Document::DidAssociateFormControlsTimerFired),
      timers_(GetTaskRunner(TaskType::kJavascriptTimer)),
      has_viewport_units_(false),
      parser_sync_policy_(kAllowAsynchronousParsing),
      node_count_(0),
      logged_field_edit_(false),
      secure_context_state_(SecureContextState::kUnknown),
      ukm_source_id_(ukm::UkmRecorder::GetNewSourceID()),
      needs_to_record_ukm_outlive_time_(false),
      viewport_data_(MakeGarbageCollected<ViewportData>(*this)),
      isolated_world_csp_map_(
          MakeGarbageCollected<
              HeapHashMap<int, Member<ContentSecurityPolicy>>>()) {
  security_initializer.ApplyPendingDataToDocument(*this);
  if (frame_) {
    DCHECK(frame_->GetPage());
    ProvideContextFeaturesToDocumentFrom(*this, *frame_->GetPage());
    fetcher_ = FrameFetchContext::CreateFetcherForCommittedDocument(
        *frame_->Loader().GetDocumentLoader(), *this);
    // TODO(dcheng): Why does this need to check that DOMWindow is non-null?
    CustomElementRegistry* registry =
        frame_->DomWindow() ? frame_->DomWindow()->MaybeCustomElements()
                            : nullptr;
    if (registry && registration_context_)
      registry->Entangle(registration_context_);
    cookie_jar_ = std::make_unique<CookieJar>(this);
  } else if (imports_controller_ &&
             !base::FeatureList::IsEnabled(
                 features::kHtmlImportsRequestInitiatorLock)) {
    fetcher_ = FrameFetchContext::CreateFetcherForImportedDocument(this);
  } else {
    // We disable fetches for frame-less Documents, including HTML-imported
    // Documents (if kHtmlImportsRequestInitiatorLock is enabled). Subresources
    // of HTML-imported Documents are fetched via the context document's
    // ResourceFetcher. See https://crbug.com/961614 for details.
    auto& properties =
        *MakeGarbageCollected<DetachableResourceFetcherProperties>(
            *MakeGarbageCollected<NullResourceFetcherProperties>());
    fetcher_ = MakeGarbageCollected<ResourceFetcher>(ResourceFetcherInit(
        properties, &FetchContext::NullInstance(),
        GetTaskRunner(TaskType::kNetworking), nullptr /* loader_factory */));

    if (imports_controller_) {
      // We don't expect the fetcher to be used, so count such unexpected use.
      fetcher_->SetShouldLogRequestAsInvalidInImportedDocument();
    }
  }
  DCHECK(fetcher_);

  root_scroller_controller_ =
      MakeGarbageCollected<RootScrollerController>(*this);

  // We depend on the url getting immediately set in subframes, but we
  // also depend on the url NOT getting immediately set in opened windows.
  // See fast/dom/early-frame-url.html
  // and fast/dom/location-new-window-no-crash.html, respectively.
  // FIXME: Can/should we unify this behavior?
  if (initializer.ShouldSetURL()) {
    SetURL(initializer.Url());
  } else {
    // Even if this document has no URL, we need to initialize base URL with
    // fallback base URL.
    UpdateBaseURL();
  }

  InitSecurityContext(initializer, security_initializer);
  FeaturePolicyInitialized(initializer, security_initializer);

  InitDNSPrefetch();

  InstanceCounters::IncrementCounter(InstanceCounters::kDocumentCounter);

  lifecycle_.AdvanceTo(DocumentLifecycle::kInactive);

  // Since CSSFontSelector requires Document::fetcher_ and StyleEngine owns
  // CSSFontSelector, need to initialize |style_engine_| after initializing
  // |fetcher_|.
  style_engine_ = MakeGarbageCollected<StyleEngine>(*this);

  // The parent's parser should be suspended together with all the other
  // objects, else this new Document would have a new ExecutionContext which
  // suspended state would not match the one from the parent, and could start
  // loading resources ignoring the defersLoading flag.
  DCHECK(!ParentDocument() || !ParentDocument()->IsContextPaused());

#ifndef NDEBUG
  liveDocumentSet().insert(this);
#endif

  if (frame_ && frame_->GetPage()->GetAgentMetricsCollector())
    frame_->GetPage()->GetAgentMetricsCollector()->DidAttachDocument(*this);
}

Document::~Document() {
  DCHECK(!GetLayoutView());
  DCHECK(!ParentTreeScope());
  // If a top document with a cache, verify that it was comprehensively
  // cleared during detach.
  DCHECK(!ax_object_cache_);

  InstanceCounters::DecrementCounter(InstanceCounters::kDocumentCounter);
}

Range* Document::CreateRangeAdjustedToTreeScope(const TreeScope& tree_scope,
                                                const Position& position) {
  DCHECK(position.IsNotNull());
  // Note: Since |Position::ComputeContainerNode()| returns |nullptr| if
  // |position| is |BeforeAnchor| or |AfterAnchor|.
  Node* const anchor_node = position.AnchorNode();
  if (anchor_node->GetTreeScope() == tree_scope) {
    return MakeGarbageCollected<Range>(tree_scope.GetDocument(), position,
                                       position);
  }
  Node* const shadow_host = tree_scope.AncestorInThisScope(anchor_node);
  return MakeGarbageCollected<Range>(tree_scope.GetDocument(),
                                     Position::BeforeNode(*shadow_host),
                                     Position::BeforeNode(*shadow_host));
}

SelectorQueryCache& Document::GetSelectorQueryCache() {
  if (!selector_query_cache_)
    selector_query_cache_ = std::make_unique<SelectorQueryCache>();
  return *selector_query_cache_;
}

MediaQueryMatcher& Document::GetMediaQueryMatcher() {
  if (!media_query_matcher_)
    media_query_matcher_ = MakeGarbageCollected<MediaQueryMatcher>(*this);
  return *media_query_matcher_;
}

void Document::MediaQueryAffectingValueChanged() {
  GetStyleEngine().MediaQueryAffectingValueChanged();
  if (NeedsLayoutTreeUpdate())
    evaluate_media_queries_on_style_recalc_ = true;
  else
    EvaluateMediaQueryList();
  probe::MediaQueryResultChanged(this);
}

void Document::SetCompatibilityMode(CompatibilityMode mode) {
  if (compatibility_mode_locked_ || mode == compatibility_mode_)
    return;

  if (compatibility_mode_ == kQuirksMode)
    UseCounter::Count(*this, WebFeature::kQuirksModeDocument);
  else if (compatibility_mode_ == kLimitedQuirksMode)
    UseCounter::Count(*this, WebFeature::kLimitedQuirksModeDocument);

  compatibility_mode_ = mode;
  GetSelectorQueryCache().Invalidate();
}

String Document::compatMode() const {
  return InQuirksMode() ? "BackCompat" : "CSS1Compat";
}

void Document::SetDoctype(DocumentType* doc_type) {
  // This should never be called more than once.
  DCHECK(!doc_type_ || !doc_type);
  doc_type_ = doc_type;
  if (doc_type_) {
    AdoptIfNeeded(*doc_type_);
    if (doc_type_->publicId().StartsWithIgnoringASCIICase(
            "-//wapforum//dtd xhtml mobile 1.")) {
      is_mobile_document_ = true;
      style_engine_->ViewportRulesChanged();
    }
  }
}

DOMImplementation& Document::implementation() {
  if (!implementation_)
    implementation_ = MakeGarbageCollected<DOMImplementation>(*this);
  return *implementation_;
}

Location* Document::location() const {
  if (!GetFrame())
    return nullptr;

  return domWindow()->location();
}

bool Document::ShouldInstallV8Extensions() const {
  return frame_->Client()->AllowScriptExtensions();
}

ContentSecurityPolicy* Document::GetContentSecurityPolicyForWorld() {
  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> v8_context = isolate->GetCurrentContext();

  // This can be called before we enter v8, hence the context might be empty,
  // which implies we are not in an isolated world.
  if (v8_context.IsEmpty())
    return GetContentSecurityPolicy();

  DOMWrapperWorld& world = DOMWrapperWorld::Current(isolate);
  if (!world.IsIsolatedWorld())
    return GetContentSecurityPolicy();

  int32_t world_id = world.GetWorldId();
  auto it = isolated_world_csp_map_->find(world_id);
  if (it != isolated_world_csp_map_->end())
    return it->value;

  ContentSecurityPolicy* policy =
      IsolatedWorldCSP::Get().CreateIsolatedWorldCSP(*this, world_id);
  if (!policy)
    return GetContentSecurityPolicy();

  isolated_world_csp_map_->insert(world_id, policy);
  return policy;
}

void Document::ChildrenChanged(const ChildrenChange& change) {
  ContainerNode::ChildrenChanged(change);
  document_element_ = ElementTraversal::FirstWithin(*this);

  // For non-HTML documents the willInsertBody notification won't happen
  // so we resume as soon as we have a document element. Even for XHTML
  // documents there may never be a <body> (since the parser won't always
  // insert one), so we resume here too. That does mean XHTML documents make
  // frames when there's only a <head>, but such documents are pretty rare.
  if (document_element_ && !IsHTMLDocument())
    BeginLifecycleUpdatesIfRenderingReady();
}

void Document::setRootScroller(Element* new_scroller, ExceptionState&) {
  root_scroller_controller_->Set(new_scroller);
}

Element* Document::rootScroller() const {
  return root_scroller_controller_->Get();
}

bool Document::IsInMainFrame() const {
  return GetFrame() && GetFrame()->IsMainFrame();
}

AtomicString Document::ConvertLocalName(const AtomicString& name) {
  return IsHTMLDocument() ? name.LowerASCII() : name;
}

// Just creates an element with specified qualified name without any
// custom element processing.
// This is a common code for step 5.2 and 7.2 of "create an element"
// <https://dom.spec.whatwg.org/#concept-create-element>
// Functions other than this one should not use HTMLElementFactory and
// SVGElementFactory because they don't support prefixes correctly.
Element* Document::CreateRawElement(const QualifiedName& qname,
                                    CreateElementFlags flags) {
  Element* element = nullptr;
  if (qname.NamespaceURI() == html_names::xhtmlNamespaceURI) {
    // https://html.spec.whatwg.org/C/#elements-in-the-dom:element-interface
    element = HTMLElementFactory::Create(qname.LocalName(), *this, flags);
    if (!element) {
      // 6. If name is a valid custom element name, then return
      // HTMLElement.
      // 7. Return HTMLUnknownElement.
      if (CustomElement::IsValidName(qname.LocalName()))
        element = MakeGarbageCollected<HTMLElement>(qname, *this);
      else
        element = MakeGarbageCollected<HTMLUnknownElement>(qname, *this);
    }
    saw_elements_in_known_namespaces_ = true;
  } else if (qname.NamespaceURI() == svg_names::kNamespaceURI) {
    element = SVGElementFactory::Create(qname.LocalName(), *this, flags);
    if (!element)
      element = MakeGarbageCollected<SVGUnknownElement>(qname, *this);
    saw_elements_in_known_namespaces_ = true;
  } else if (RuntimeEnabledFeatures::MathMLCoreEnabled() &&
             qname.NamespaceURI() == mathml_names::kNamespaceURI) {
    element = MathMLElementFactory::Create(qname.LocalName(), *this, flags);
    // TODO(crbug.com/1021837): Determine if we need to introduce a
    // MathMLUnknownClass.
    if (!element)
      element = MakeGarbageCollected<MathMLElement>(qname, *this);
    saw_elements_in_known_namespaces_ = true;
  } else {
    element = Element::Create(qname, this);
  }

  if (element->prefix() != qname.Prefix())
    element->SetTagNameForCreateElementNS(qname);
  DCHECK(qname == element->TagQName());

  return element;
}

// https://dom.spec.whatwg.org/#dom-document-createelement
Element* Document::CreateElementForBinding(const AtomicString& name,
                                           ExceptionState& exception_state) {
  if (!IsValidElementName(this, name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        "The tag name provided ('" + name + "') is not a valid name.");
    return nullptr;
  }

  if (IsXHTMLDocument() || IsHTMLDocument()) {
    // 2. If the context object is an HTML document, let localName be
    // converted to ASCII lowercase.
    AtomicString local_name = ConvertLocalName(name);
    if (CustomElement::ShouldCreateCustomElement(local_name)) {
      return CustomElement::CreateCustomElement(
          *this,
          QualifiedName(g_null_atom, local_name, html_names::xhtmlNamespaceURI),
          CreateElementFlags::ByCreateElement());
    }
    if (auto* element = HTMLElementFactory::Create(
            local_name, *this, CreateElementFlags::ByCreateElement()))
      return element;
    QualifiedName q_name(g_null_atom, local_name,
                         html_names::xhtmlNamespaceURI);
    if (RegistrationContext() && V0CustomElement::IsValidName(local_name))
      return RegistrationContext()->CreateCustomTagElement(*this, q_name);
    return MakeGarbageCollected<HTMLUnknownElement>(q_name, *this);
  }
  return Element::Create(QualifiedName(g_null_atom, name, g_null_atom), this);
}

AtomicString GetTypeExtension(
    Document* document,
    const StringOrElementCreationOptions& string_or_options) {
  if (string_or_options.IsNull())
    return AtomicString();

  if (string_or_options.IsString()) {
    UseCounter::Count(document,
                      WebFeature::kDocumentCreateElement2ndArgStringHandling);
    return AtomicString(string_or_options.GetAsString());
  }

  if (string_or_options.IsElementCreationOptions()) {
    const ElementCreationOptions& options =
        *string_or_options.GetAsElementCreationOptions();
    if (options.hasIs())
      return AtomicString(options.is());
  }

  return AtomicString();
}

// https://dom.spec.whatwg.org/#dom-document-createelement
Element* Document::CreateElementForBinding(
    const AtomicString& local_name,
    const StringOrElementCreationOptions& string_or_options,
    ExceptionState& exception_state) {
  if (string_or_options.IsNull()) {
    return CreateElementForBinding(local_name, exception_state);
  }

  // 1. If localName does not match Name production, throw InvalidCharacterError
  if (!IsValidElementName(this, local_name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        "The tag name provided ('" + local_name + "') is not a valid name.");
    return nullptr;
  }

  // 2. localName converted to ASCII lowercase
  const AtomicString& converted_local_name = ConvertLocalName(local_name);
  QualifiedName q_name(g_null_atom, converted_local_name,
                       IsXHTMLDocument() || IsHTMLDocument()
                           ? html_names::xhtmlNamespaceURI
                           : g_null_atom);

  bool is_v1 =
      string_or_options.IsElementCreationOptions() || !RegistrationContext();
  // V0 is only allowed with the flag.
  DCHECK(is_v1 || RuntimeEnabledFeatures::CustomElementsV0Enabled(this));
  bool create_v1_builtin = string_or_options.IsElementCreationOptions();
  bool should_create_builtin =
      create_v1_builtin || string_or_options.IsString();

  // 3.
  const AtomicString& is = GetTypeExtension(this, string_or_options);

  // 5. Let element be the result of creating an element given ...
  Element* element =
      CreateElement(q_name,
                    is_v1 ? CreateElementFlags::ByCreateElementV1()
                          : CreateElementFlags::ByCreateElementV0(),
                    should_create_builtin ? is : g_null_atom);

  // 8. If 'is' is non-null, set 'is' attribute
  if (!is_v1 && !is.IsEmpty())
    element->setAttribute(html_names::kIsAttr, is);

  return element;
}

static inline QualifiedName CreateQualifiedName(
    const AtomicString& namespace_uri,
    const AtomicString& qualified_name,
    ExceptionState& exception_state) {
  AtomicString prefix, local_name;
  if (!Document::ParseQualifiedName(qualified_name, prefix, local_name,
                                    exception_state))
    return QualifiedName::Null();

  QualifiedName q_name(prefix, local_name, namespace_uri);
  if (!Document::HasValidNamespaceForElements(q_name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNamespaceError,
        "The namespace URI provided ('" + namespace_uri +
            "') is not valid for the qualified name provided ('" +
            qualified_name + "').");
    return QualifiedName::Null();
  }

  return q_name;
}

Element* Document::createElementNS(const AtomicString& namespace_uri,
                                   const AtomicString& qualified_name,
                                   ExceptionState& exception_state) {
  QualifiedName q_name(
      CreateQualifiedName(namespace_uri, qualified_name, exception_state));
  if (q_name == QualifiedName::Null())
    return nullptr;

  CreateElementFlags flags = CreateElementFlags::ByCreateElement();
  if (CustomElement::ShouldCreateCustomElement(q_name))
    return CustomElement::CreateCustomElement(*this, q_name, flags);
  if (RegistrationContext() && V0CustomElement::IsValidName(q_name.LocalName()))
    return RegistrationContext()->CreateCustomTagElement(*this, q_name);
  return CreateRawElement(q_name, flags);
}

// https://dom.spec.whatwg.org/#internal-createelementns-steps
Element* Document::createElementNS(
    const AtomicString& namespace_uri,
    const AtomicString& qualified_name,
    const StringOrElementCreationOptions& string_or_options,
    ExceptionState& exception_state) {
  if (string_or_options.IsNull())
    return createElementNS(namespace_uri, qualified_name, exception_state);

  // 1. Validate and extract
  QualifiedName q_name(
      CreateQualifiedName(namespace_uri, qualified_name, exception_state));
  if (q_name == QualifiedName::Null())
    return nullptr;

  bool is_v1 =
      string_or_options.IsElementCreationOptions() || !RegistrationContext();
  // V0 is only allowed with the flag.
  DCHECK(is_v1 || RuntimeEnabledFeatures::CustomElementsV0Enabled(this));
  bool create_v1_builtin = string_or_options.IsElementCreationOptions();
  bool should_create_builtin =
      create_v1_builtin || string_or_options.IsString();

  // 2.
  const AtomicString& is = GetTypeExtension(this, string_or_options);

  if (!IsValidElementName(this, qualified_name)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidCharacterError,
                                      "The tag name provided ('" +
                                          qualified_name +
                                          "') is not a valid name.");
    return nullptr;
  }

  // 3. Let element be the result of creating an element
  Element* element =
      CreateElement(q_name,
                    is_v1 ? CreateElementFlags::ByCreateElementV1()
                          : CreateElementFlags::ByCreateElementV0(),
                    should_create_builtin ? is : g_null_atom);

  // 4. If 'is' is non-null, set 'is' attribute
  if (!is_v1 && !is.IsEmpty())
    element->setAttribute(html_names::kIsAttr, is);

  return element;
}

// Entry point of "create an element".
// https://dom.spec.whatwg.org/#concept-create-element
Element* Document::CreateElement(const QualifiedName& q_name,
                                 const CreateElementFlags flags,
                                 const AtomicString& is) {
  CustomElementDefinition* definition = nullptr;
  if (flags.IsCustomElementsV1() &&
      q_name.NamespaceURI() == html_names::xhtmlNamespaceURI) {
    const CustomElementDescriptor desc(is.IsNull() ? q_name.LocalName() : is,
                                       q_name.LocalName());
    if (CustomElementRegistry* registry = CustomElement::Registry(*this))
      definition = registry->DefinitionFor(desc);
  }

  if (definition)
    return definition->CreateElement(*this, q_name, flags);

  return CustomElement::CreateUncustomizedOrUndefinedElement(*this, q_name,
                                                             flags, is);
}

ScriptValue Document::registerElement(ScriptState* script_state,
                                      const AtomicString& name,
                                      const ElementRegistrationOptions* options,
                                      ExceptionState& exception_state) {
  if (!RegistrationContext()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "No element registration context is available.");
    return ScriptValue();
  }

  // Polymer V1 uses Custom Elements V0. <dom-module> is defined in its base
  // library and is a strong signal that this is a Polymer V1.
  // This counter is used to research how much users are affected once Custom
  // Element V0 is deprecated.
  if (name == "dom-module")
    UseCounter::Count(*this, WebFeature::kPolymerV1Detected);

  V0CustomElementConstructorBuilder constructor_builder(script_state, options);
  RegistrationContext()->RegisterElement(this, &constructor_builder, name,
                                         exception_state);
  if (exception_state.HadException())
    return ScriptValue();
  return constructor_builder.BindingsReturnValue();
}

V0CustomElementRegistrationContext* Document::RegistrationContext() const {
  if (RuntimeEnabledFeatures::CustomElementsV0Enabled(this))
    return registration_context_.Get();
  return nullptr;
}

V0CustomElementMicrotaskRunQueue* Document::CustomElementMicrotaskRunQueue() {
  if (!custom_element_microtask_run_queue_) {
    custom_element_microtask_run_queue_ =
        MakeGarbageCollected<V0CustomElementMicrotaskRunQueue>();
  }
  return custom_element_microtask_run_queue_.Get();
}

void Document::ClearImportsController() {
  fetcher_->ClearContext();
  imports_controller_ = nullptr;
}

HTMLImportsController* Document::EnsureImportsController() {
  if (!imports_controller_) {
    DCHECK(frame_);
    imports_controller_ = MakeGarbageCollected<HTMLImportsController>(*this);
  }

  return imports_controller_;
}

HTMLImportLoader* Document::ImportLoader() const {
  if (!imports_controller_)
    return nullptr;
  return imports_controller_->LoaderFor(*this);
}

bool Document::IsHTMLImport() const {
  return imports_controller_ && imports_controller_->Master() != this;
}

Document& Document::MasterDocument() const {
  if (!imports_controller_)
    return *const_cast<Document*>(this);

  Document* master = imports_controller_->Master();
  DCHECK(master);
  return *master;
}

bool Document::HaveImportsLoaded() const {
  if (!imports_controller_)
    return true;
  return !imports_controller_->ShouldBlockScriptExecution(*this);
}

LocalDOMWindow* Document::ExecutingWindow() const {
  if (LocalDOMWindow* owning_window = domWindow())
    return owning_window;
  if (HTMLImportsController* import = ImportsController())
    return import->Master()->domWindow();
  return nullptr;
}

LocalFrame* Document::ExecutingFrame() {
  LocalDOMWindow* window = ExecutingWindow();
  if (!window)
    return nullptr;
  return window->GetFrame();
}

DocumentFragment* Document::createDocumentFragment() {
  return DocumentFragment::Create(*this);
}

Text* Document::createTextNode(const String& data) {
  return Text::Create(*this, data);
}

Comment* Document::createComment(const String& data) {
  return Comment::Create(*this, data);
}

CDATASection* Document::createCDATASection(const String& data,
                                           ExceptionState& exception_state) {
  if (IsHTMLDocument()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "This operation is not supported for HTML documents.");
    return nullptr;
  }
  if (data.Contains("]]>")) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidCharacterError,
                                      "String cannot contain ']]>' since that "
                                      "is the end delimiter of a CData "
                                      "section.");
    return nullptr;
  }
  return CDATASection::Create(*this, data);
}

ProcessingInstruction* Document::createProcessingInstruction(
    const String& target,
    const String& data,
    ExceptionState& exception_state) {
  if (!IsValidName(target)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        "The target provided ('" + target + "') is not a valid name.");
    return nullptr;
  }
  if (data.Contains("?>")) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        "The data provided ('" + data + "') contains '?>'.");
    return nullptr;
  }
  if (IsHTMLDocument()) {
    UseCounter::Count(*this,
                      WebFeature::kHTMLDocumentCreateProcessingInstruction);
  }
  return ProcessingInstruction::Create(*this, target, data);
}

Text* Document::CreateEditingTextNode(const String& text) {
  return Text::CreateEditingText(*this, text);
}

Node* Document::importNode(Node* imported_node,
                           bool deep,
                           ExceptionState& exception_state) {
  // https://dom.spec.whatwg.org/#dom-document-importnode

  // 1. If node is a document or shadow root, then throw a "NotSupportedError"
  // DOMException.
  if (imported_node->IsDocumentNode()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The node provided is a document, which may not be imported.");
    return nullptr;
  }
  if (imported_node->IsShadowRoot()) {
    // ShadowRoot nodes should not be explicitly importable.  Either they are
    // imported along with their host node, or created implicitly.
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The node provided is a shadow root, which may not be imported.");
    return nullptr;
  }

  // 2. Return a clone of node, with context object and the clone children flag
  // set if deep is true.
  return imported_node->Clone(
      *this, deep ? CloneChildrenFlag::kClone : CloneChildrenFlag::kSkip);
}

Node* Document::adoptNode(Node* source, ExceptionState& exception_state) {
  EventQueueScope scope;

  switch (source->getNodeType()) {
    case kDocumentNode:
      exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                        "The node provided is of type '" +
                                            source->nodeName() +
                                            "', which may not be adopted.");
      return nullptr;
    case kAttributeNode: {
      auto* attr = To<Attr>(source);
      if (Element* owner_element = attr->ownerElement())
        owner_element->removeAttributeNode(attr, exception_state);
      break;
    }
    default:
      if (source->IsShadowRoot()) {
        // ShadowRoot cannot disconnect itself from the host node.
        exception_state.ThrowDOMException(
            DOMExceptionCode::kHierarchyRequestError,
            "The node provided is a shadow root, which may not be adopted.");
        return nullptr;
      }

      if (auto* frame_owner_element =
              DynamicTo<HTMLFrameOwnerElement>(source)) {
        if (GetFrame() && GetFrame()->Tree().IsDescendantOf(
                              frame_owner_element->ContentFrame())) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kHierarchyRequestError,
              "The node provided is a frame which contains this document.");
          return nullptr;
        }
      }
      if (source->parentNode()) {
        source->parentNode()->RemoveChild(source, exception_state);
        if (exception_state.HadException())
          return nullptr;
        // The above removeChild() can execute arbitrary JavaScript code.
        if (source->parentNode()) {
          AddConsoleMessage(ConsoleMessage::Create(
              mojom::ConsoleMessageSource::kJavaScript,
              mojom::ConsoleMessageLevel::kWarning,
              ExceptionMessages::FailedToExecute("adoptNode", "Document",
                                                 "Unable to remove the "
                                                 "specified node from the "
                                                 "original parent.")));
          return nullptr;
        }
      }
  }

  AdoptIfNeeded(*source);

  return source;
}

bool Document::HasValidNamespaceForElements(const QualifiedName& q_name) {
  // These checks are from DOM Core Level 2, createElementNS
  // http://www.w3.org/TR/DOM-Level-2-Core/core.html#ID-DocCrElNS
  // createElementNS(null, "html:div")
  if (!q_name.Prefix().IsEmpty() && q_name.NamespaceURI().IsNull())
    return false;
  // createElementNS("http://www.example.com", "xml:lang")
  if (q_name.Prefix() == g_xml_atom &&
      q_name.NamespaceURI() != xml_names::kNamespaceURI)
    return false;

  // Required by DOM Level 3 Core and unspecified by DOM Level 2 Core:
  // http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core.html#ID-DocCrElNS
  // createElementNS("http://www.w3.org/2000/xmlns/", "foo:bar"),
  // createElementNS(null, "xmlns:bar"), createElementNS(null, "xmlns")
  if (q_name.Prefix() == g_xmlns_atom ||
      (q_name.Prefix().IsEmpty() && q_name.LocalName() == g_xmlns_atom))
    return q_name.NamespaceURI() == xmlns_names::kNamespaceURI;
  return q_name.NamespaceURI() != xmlns_names::kNamespaceURI;
}

bool Document::HasValidNamespaceForAttributes(const QualifiedName& q_name) {
  return HasValidNamespaceForElements(q_name);
}

String Document::readyState() const {
  DEFINE_STATIC_LOCAL(const String, loading, ("loading"));
  DEFINE_STATIC_LOCAL(const String, interactive, ("interactive"));
  DEFINE_STATIC_LOCAL(const String, complete, ("complete"));

  switch (ready_state_) {
    case kLoading:
      return loading;
    case kInteractive:
      return interactive;
    case kComplete:
      return complete;
  }

  NOTREACHED();
  return String();
}

void Document::SetReadyState(DocumentReadyState ready_state) {
  if (ready_state == ready_state_)
    return;

  switch (ready_state) {
    case kLoading:
      if (document_timing_.DomLoading().is_null()) {
        document_timing_.MarkDomLoading();
      }
      break;
    case kInteractive:
      if (document_timing_.DomInteractive().is_null())
        document_timing_.MarkDomInteractive();
      break;
    case kComplete:
      if (document_timing_.DomComplete().is_null())
        document_timing_.MarkDomComplete();
      break;
  }

  ready_state_ = ready_state;
  DispatchEvent(*Event::Create(event_type_names::kReadystatechange));
}

bool Document::IsLoadCompleted() const {
  return ready_state_ == kComplete;
}

AtomicString Document::EncodingName() const {
  // TextEncoding::name() returns a char*, no need to allocate a new
  // String for it each time.
  // FIXME: We should fix TextEncoding to speak AtomicString anyway.
  return AtomicString(Encoding().GetName());
}

void Document::SetContentLanguage(const AtomicString& language) {
  if (content_language_ == language)
    return;
  content_language_ = language;

  // Document's style depends on the content language.
  GetStyleEngine().MarkViewportStyleDirty();
  GetStyleEngine().MarkAllElementsForStyleRecalc(
      StyleChangeReasonForTracing::Create(style_change_reason::kLanguage));
}

void Document::setXMLVersion(const String& version,
                             ExceptionState& exception_state) {
  if (!XMLDocumentParser::SupportsXMLVersion(version)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "This document does not support the XML version '" + version + "'.");
    return;
  }

  xml_version_ = version;
}

void Document::setXMLStandalone(bool standalone,
                                ExceptionState& exception_state) {
  xml_standalone_ = standalone ? kStandalone : kNotStandalone;
}

void Document::SetContent(const String& content) {
  // Only set the content of the document if it is ready to be set. This method
  // could be called at any time.
  if (ScriptableDocumentParser* parser = GetScriptableDocumentParser()) {
    if (parser->IsParsing() && parser->IsExecutingScript())
      return;
  }
  if (ignore_opens_during_unload_count_)
    return;

  open();
  parser_->Append(content);
  close();
}

String Document::SuggestedMIMEType() const {
  if (IsXMLDocument()) {
    if (IsXHTMLDocument())
      return "application/xhtml+xml";
    if (IsSVGDocument())
      return "image/svg+xml";
    return "application/xml";
  }
  if (xmlStandalone())
    return "text/xml";
  if (IsHTMLDocument())
    return "text/html";

  if (DocumentLoader* document_loader = Loader())
    return document_loader->MimeType();
  return String();
}

void Document::SetMimeType(const AtomicString& mime_type) {
  mime_type_ = mime_type;
}

AtomicString Document::contentType() const {
  if (!mime_type_.IsEmpty())
    return mime_type_;

  if (DocumentLoader* document_loader = Loader())
    return document_loader->MimeType();

  String mime_type = SuggestedMIMEType();
  if (!mime_type.IsEmpty())
    return AtomicString(mime_type);

  return AtomicString("application/xml");
}

Element* Document::ElementFromPoint(double x, double y) const {
  if (!GetLayoutView())
    return nullptr;

  return TreeScope::ElementFromPoint(x, y);
}

HeapVector<Member<Element>> Document::ElementsFromPoint(double x,
                                                        double y) const {
  if (!GetLayoutView())
    return HeapVector<Member<Element>>();
  return TreeScope::ElementsFromPoint(x, y);
}

Range* Document::caretRangeFromPoint(int x, int y) {
  if (!GetLayoutView())
    return nullptr;

  HitTestResult result = HitTestInDocument(this, x, y);
  PositionWithAffinity position_with_affinity = result.GetPosition();
  if (position_with_affinity.IsNull())
    return nullptr;

  Position range_compliant_position =
      position_with_affinity.GetPosition().ParentAnchoredEquivalent();
  return CreateRangeAdjustedToTreeScope(*this, range_compliant_position);
}

Element* Document::scrollingElement() {
  if (RuntimeEnabledFeatures::ScrollTopLeftInteropEnabled() && InQuirksMode())
    UpdateStyleAndLayoutTree();
  return ScrollingElementNoLayout();
}

Element* Document::ScrollingElementNoLayout() {
  if (RuntimeEnabledFeatures::ScrollTopLeftInteropEnabled()) {
    if (InQuirksMode()) {
      DCHECK(!IsActive() ||
             lifecycle_.GetState() >= DocumentLifecycle::kStyleClean);
      HTMLBodyElement* body = FirstBodyElement();
      if (body && body->GetLayoutObject() &&
          body->GetLayoutObject()->HasOverflowClip())
        return nullptr;

      return body;
    }

    return documentElement();
  }

  return body();
}

/*
 * Performs three operations:
 *  1. Convert control characters to spaces
 *  2. Trim leading and trailing spaces
 *  3. Collapse internal whitespace.
 */
template <typename CharacterType>
static inline String CanonicalizedTitle(Document* document,
                                        const String& title) {
  unsigned length = title.length();
  unsigned builder_index = 0;
  const CharacterType* characters = title.GetCharacters<CharacterType>();

  StringBuffer<CharacterType> buffer(length);

  // Replace control characters with spaces and collapse whitespace.
  bool pending_whitespace = false;
  for (unsigned i = 0; i < length; ++i) {
    UChar32 c = characters[i];
    if ((c <= WTF::unicode::kSpaceCharacter &&
         c != WTF::unicode::kLineTabulationCharacter) ||
        c == WTF::unicode::kDeleteCharacter) {
      if (builder_index != 0)
        pending_whitespace = true;
    } else {
      if (pending_whitespace) {
        buffer[builder_index++] = ' ';
        pending_whitespace = false;
      }
      buffer[builder_index++] = c;
    }
  }
  buffer.Shrink(builder_index);

  return String::Adopt(buffer);
}

void Document::UpdateTitle(const String& title) {
  if (raw_title_ == title)
    return;

  raw_title_ = title;

  String old_title = title_;
  if (raw_title_.IsEmpty())
    title_ = String();
  else if (raw_title_.Is8Bit())
    title_ = CanonicalizedTitle<LChar>(this, raw_title_);
  else
    title_ = CanonicalizedTitle<UChar>(this, raw_title_);

  if (!frame_ || old_title == title_)
    return;
  DispatchDidReceiveTitle();

  if (AXObjectCache* cache = ExistingAXObjectCache())
    cache->DocumentTitleChanged();
}

void Document::DispatchDidReceiveTitle() {
  frame_->Client()->DispatchDidReceiveTitle(title_);
}

void Document::setTitle(const String& title) {
  // Title set by JavaScript -- overrides any title elements.
  Element* element = documentElement();
  if (IsSVGSVGElement(element)) {
    if (!title_element_) {
      title_element_ = MakeGarbageCollected<SVGTitleElement>(*this);
      element->InsertBefore(title_element_.Get(), element->firstChild());
    }
    if (auto* svg_title = ToSVGTitleElementOrNull(title_element_.Get()))
      svg_title->SetText(title);
  } else if (element && element->IsHTMLElement()) {
    if (!title_element_) {
      HTMLElement* head_element = head();
      if (!head_element)
        return;
      title_element_ = MakeGarbageCollected<HTMLTitleElement>(*this);
      head_element->AppendChild(title_element_.Get());
    }
    if (auto* html_title = DynamicTo<HTMLTitleElement>(title_element_.Get()))
      html_title->setText(title);
  }
}

void Document::SetTitleElement(Element* title_element) {
  // If the root element is an svg element in the SVG namespace, then let value
  // be the child text content of the first title element in the SVG namespace
  // that is a child of the root element.
  if (IsSVGSVGElement(documentElement())) {
    title_element_ = Traversal<SVGTitleElement>::FirstChild(*documentElement());
  } else {
    if (title_element_ && title_element_ != title_element)
      title_element_ = Traversal<HTMLTitleElement>::FirstWithin(*this);
    else
      title_element_ = title_element;

    // If the root element isn't an svg element in the SVG namespace and the
    // title element is in the SVG namespace, it is ignored.
    if (IsSVGTitleElement(title_element_)) {
      title_element_ = nullptr;
      return;
    }
  }

  if (auto* html_title = DynamicTo<HTMLTitleElement>(title_element_.Get()))
    UpdateTitle(html_title->text());
  else if (auto* svg_title = ToSVGTitleElementOrNull(title_element_.Get()))
    UpdateTitle(svg_title->textContent());
}

void Document::RemoveTitle(Element* title_element) {
  if (title_element_ != title_element)
    return;

  title_element_ = nullptr;

  // Update title based on first title element in the document, if one exists.
  if (IsHTMLDocument() || IsXHTMLDocument()) {
    if (HTMLTitleElement* title =
            Traversal<HTMLTitleElement>::FirstWithin(*this))
      SetTitleElement(title);
  } else if (IsSVGDocument()) {
    if (SVGTitleElement* title = Traversal<SVGTitleElement>::FirstWithin(*this))
      SetTitleElement(title);
  }

  if (!title_element_)
    UpdateTitle(String());
}

const AtomicString& Document::dir() {
  Element* root_element = documentElement();
  if (auto* html = DynamicTo<HTMLHtmlElement>(root_element))
    return html->dir();
  return g_null_atom;
}

void Document::setDir(const AtomicString& value) {
  Element* root_element = documentElement();
  if (auto* html = DynamicTo<HTMLHtmlElement>(root_element))
    html->setDir(value);
}

bool Document::IsPageVisible() const {
  // The visibility of the document is inherited from the visibility of the
  // page. If there is no page associated with the document, we will assume
  // that the page is hidden, as specified by the spec:
  // https://w3c.github.io/page-visibility/#hidden-attribute
  if (!frame_ || !frame_->GetPage())
    return false;
  // While visibilitychange is being dispatched during unloading it is
  // expected that the visibility is hidden regardless of the page's
  // visibility.
  if (load_event_progress_ >= kUnloadVisibilityChangeInProgress)
    return false;
  return frame_->GetPage()->IsPageVisible();
}

bool Document::IsPrefetchOnly() const {
  if (!frame_ || !frame_->GetPage())
    return false;

  PrerendererClient* prerenderer_client =
      PrerendererClient::From(frame_->GetPage());
  return prerenderer_client && prerenderer_client->IsPrefetchOnly();
}

String Document::visibilityState() const {
  return PageHiddenStateString(hidden());
}

bool Document::hidden() const {
  return !IsPageVisible();
}

bool Document::wasDiscarded() const {
  return was_discarded_;
}

void Document::SetWasDiscarded(bool was_discarded) {
  was_discarded_ = was_discarded;
}

void Document::DidChangeVisibilityState() {
  DispatchEvent(*Event::CreateBubble(event_type_names::kVisibilitychange));
  // Also send out the deprecated version until it can be removed.
  DispatchEvent(
      *Event::CreateBubble(event_type_names::kWebkitvisibilitychange));

  if (IsPageVisible())
    Timeline().SetAllCompositorPending();

  if (hidden() && canvas_font_cache_)
    canvas_font_cache_->PruneAll();

  InteractiveDetector* interactive_detector = InteractiveDetector::From(*this);
  if (interactive_detector) {
    interactive_detector->OnPageHiddenChanged(hidden());
  }
}

String Document::nodeName() const {
  return "#document";
}

Node::NodeType Document::getNodeType() const {
  return kDocumentNode;
}

FormController& Document::GetFormController() {
  if (!form_controller_) {
    form_controller_ = MakeGarbageCollected<FormController>(*this);
    HistoryItem* history_item = Loader() ? Loader()->GetHistoryItem() : nullptr;
    if (history_item)
      history_item->SetDocumentState(form_controller_->ControlStates());
  }
  return *form_controller_;
}

DocumentState* Document::GetDocumentState() const {
  if (!form_controller_)
    return nullptr;
  return form_controller_->ControlStates();
}

void Document::SetStateForNewControls(const Vector<String>& state_vector) {
  if (!state_vector.size() && !form_controller_)
    return;
  GetFormController().SetStateForNewControls(state_vector);
}

LocalFrameView* Document::View() const {
  return frame_ ? frame_->View() : nullptr;
}

Page* Document::GetPage() const {
  return frame_ ? frame_->GetPage() : nullptr;
}

LocalFrame* Document::GetFrameOfMasterDocument() const {
  if (frame_)
    return frame_;
  if (imports_controller_)
    return imports_controller_->Master()->GetFrame();
  return nullptr;
}

Settings* Document::GetSettings() const {
  return frame_ ? frame_->GetSettings() : nullptr;
}

Range* Document::createRange() {
  return Range::Create(*this);
}

NodeIterator* Document::createNodeIterator(Node* root,
                                           unsigned what_to_show,
                                           V8NodeFilter* filter) {
  DCHECK(root);
  return MakeGarbageCollected<NodeIterator>(root, what_to_show, filter);
}

TreeWalker* Document::createTreeWalker(Node* root,
                                       unsigned what_to_show,
                                       V8NodeFilter* filter) {
  DCHECK(root);
  return MakeGarbageCollected<TreeWalker>(root, what_to_show, filter);
}

bool Document::NeedsLayoutTreeUpdate() const {
  if (!IsActive() || !View())
    return false;
  if (NeedsFullLayoutTreeUpdate())
    return true;
  if (style_engine_->NeedsStyleRecalc())
    return true;
  if (style_engine_->NeedsStyleInvalidation())
    return true;
  if (GetLayoutView() && GetLayoutView()->WasNotifiedOfSubtreeChange())
    return true;
  if (style_engine_->NeedsLayoutTreeRebuild()) {
    // TODO(futhark): there a couple of places where call back into the top
    // frame while recursively doing a lifecycle update. One of them are for the
    // RootScrollerController. These should probably be post layout tasks and
    // make this test unnecessary since the layout tree rebuild dirtiness is
    // internal to StyleEngine::UpdateStyleAndLayoutTree().
    DCHECK(InStyleRecalc());
    return true;
  }
  return false;
}

bool Document::NeedsFullLayoutTreeUpdate() const {
  // This method returns true if we cannot decide which specific elements need
  // to have its style or layout tree updated on the next lifecycle update. If
  // this method returns false, we typically use that to walk up the ancestor
  // chain to decide if we can let getComputedStyle() use the current
  // ComputedStyle without doing the lifecycle update (implemented in
  // Document::NeedsLayoutTreeUpdateForNodeIncludingDisplayLocked()).
  if (!IsActive() || !View())
    return false;
  if (style_engine_->NeedsFullStyleUpdate())
    return true;
  if (!use_elements_needing_update_.IsEmpty())
    return true;
  // We have scheduled an invalidation set on the document node which means any
  // element may need a style recalc.
  if (NeedsStyleInvalidation())
    return true;
  if (IsSlotAssignmentOrLegacyDistributionDirty())
    return true;
  if (DocumentAnimations::NeedsAnimationTimingUpdate(*this))
    return true;
  return false;
}

bool Document::ShouldScheduleLayoutTreeUpdate() const {
  if (!IsActive())
    return false;
  if (InStyleRecalc())
    return false;
  // InPreLayout will recalc style itself. There's no reason to schedule another
  // recalc.
  if (lifecycle_.GetState() == DocumentLifecycle::kInPreLayout)
    return false;
  if (!ShouldScheduleLayout())
    return false;
  return true;
}

void Document::ScheduleLayoutTreeUpdate() {
  DCHECK(!HasPendingVisualUpdate());
  DCHECK(ShouldScheduleLayoutTreeUpdate());
  DCHECK(NeedsLayoutTreeUpdate());

  if (!View()->CanThrottleRendering())
    GetPage()->Animator().ScheduleVisualUpdate(GetFrame());
  lifecycle_.EnsureStateAtMost(DocumentLifecycle::kVisualUpdatePending);

  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "ScheduleStyleRecalculation", TRACE_EVENT_SCOPE_THREAD,
                       "data",
                       inspector_recalculate_styles_event::Data(GetFrame()));
  ++style_version_;
}

bool Document::HasPendingForcedStyleRecalc() const {
  return HasPendingVisualUpdate() && !InStyleRecalc() &&
         GetStyleChangeType() == kSubtreeStyleChange;
}

void Document::UpdateStyleInvalidationIfNeeded() {
  DCHECK(IsActive());
  ScriptForbiddenScope forbid_script;

  if (!GetStyleEngine().NeedsStyleInvalidation())
    return;
  TRACE_EVENT0("blink", "Document::updateStyleInvalidationIfNeeded");
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER_HIGHRES("Style.InvalidationTime");
  GetStyleEngine().InvalidateStyle();
}

void Document::SetupFontBuilder(ComputedStyle& document_style) {
  FontBuilder font_builder(this);
  CSSFontSelector* selector = GetStyleEngine().GetFontSelector();
  font_builder.CreateFontForDocument(selector, document_style);
}

#define PROPAGATE_FROM(source, getter, setter, initial) \
  PROPAGATE_VALUE(source ? source->getter() : initial, getter, setter);

#define PROPAGATE_VALUE(value, getter, setter)     \
  if ((new_viewport_style->getter()) != (value)) { \
    new_viewport_style->setter(value);             \
    changed = true;                                \
  }

bool PropagateScrollSnapStyleToViewport(
    Document& document,
    const ComputedStyle* document_element_style,
    scoped_refptr<ComputedStyle> new_viewport_style) {
  bool changed = false;
  // We only propagate the properties related to snap container since viewport
  // defining element cannot be a snap area.
  PROPAGATE_FROM(document_element_style, GetScrollSnapType, SetScrollSnapType,
                 cc::ScrollSnapType());
  PROPAGATE_FROM(document_element_style, ScrollPaddingTop, SetScrollPaddingTop,
                 Length());
  PROPAGATE_FROM(document_element_style, ScrollPaddingRight,
                 SetScrollPaddingRight, Length());
  PROPAGATE_FROM(document_element_style, ScrollPaddingBottom,
                 SetScrollPaddingBottom, Length());
  PROPAGATE_FROM(document_element_style, ScrollPaddingLeft,
                 SetScrollPaddingLeft, Length());

  if (changed) {
    document.GetSnapCoordinator().SnapContainerDidChange(
        *document.GetLayoutView());
  }

  return changed;
}

void Document::PropagateStyleToViewport() {
  DCHECK(InStyleRecalc());
  HTMLElement* body = this->body();
  Element* document_element = this->documentElement();

  const ComputedStyle* document_element_style =
      document_element && documentElement()->GetLayoutObject()
          ? documentElement()->GetComputedStyle()
          : nullptr;
  const ComputedStyle* body_style =
      body && body->GetLayoutObject() ? body->GetComputedStyle() : nullptr;

  const ComputedStyle& viewport_style = GetLayoutView()->StyleRef();
  scoped_refptr<ComputedStyle> new_viewport_style =
      ComputedStyle::Clone(viewport_style);
  bool changed = false;
  bool update_scrollbar_style = false;

  // Writing mode and direction
  {
    const ComputedStyle* direction_style =
        body_style ? body_style : document_element_style;
    PROPAGATE_FROM(direction_style, GetWritingMode, SetWritingMode,
                   WritingMode::kHorizontalTb);
    PROPAGATE_FROM(direction_style, Direction, SetDirection,
                   TextDirection::kLtr);
  }

  // Background
  {
    const ComputedStyle* background_style = document_element_style;
    // http://www.w3.org/TR/css3-background/#body-background
    // <html> root element with no background steals background from its first
    // <body> child.
    // Also see LayoutBoxModelObject::BackgroundTransfersToView()
    if (body_style && IsA<HTMLHtmlElement>(documentElement()) &&
        IsA<HTMLBodyElement>(body) && !background_style->HasBackground()) {
      background_style = body_style;
    }

    Color background_color = Color::kTransparent;
    FillLayer background_layers(EFillLayerType::kBackground, true);
    EImageRendering image_rendering = EImageRendering::kAuto;

    if (background_style) {
      background_color = background_style->VisitedDependentColor(
          GetCSSPropertyBackgroundColor());
      background_layers = background_style->BackgroundLayers();
      for (auto* current_layer = &background_layers; current_layer;
           current_layer = current_layer->Next()) {
        // http://www.w3.org/TR/css3-background/#root-background
        // The root element background always have painting area of the whole
        // canvas.
        current_layer->SetClip(EFillBox::kBorder);

        // The root element doesn't scroll. It always propagates its layout
        // overflow to the viewport. Positioning background against either box
        // is equivalent to positioning against the scrolled box of the
        // viewport.
        if (current_layer->Attachment() == EFillAttachment::kScroll)
          current_layer->SetAttachment(EFillAttachment::kLocal);
      }
      image_rendering = background_style->ImageRendering();
    }

    if (viewport_style.VisitedDependentColor(GetCSSPropertyBackgroundColor()) !=
            background_color ||
        viewport_style.BackgroundLayers() != background_layers ||
        viewport_style.ImageRendering() != image_rendering) {
      changed = true;
      new_viewport_style->SetBackgroundColor(background_color);
      new_viewport_style->AccessBackgroundLayers() = background_layers;
      new_viewport_style->SetImageRendering(image_rendering);
    }
  }

  // Overflow
  {
    const ComputedStyle* overflow_style = nullptr;
    if (Element* viewport_element = ViewportDefiningElement()) {
      if (viewport_element == body) {
        overflow_style = body_style;
      } else {
        DCHECK_EQ(viewport_element, documentElement());
        overflow_style = document_element_style;

        // The body element has its own scrolling box, independent from the
        // viewport.  This is a bit of a weird edge case in the CSS spec that we
        // might want to try to eliminate some day (eg. for ScrollTopLeftInterop
        // - see http://crbug.com/157855).
        if (body_style && !body_style->IsOverflowVisible()) {
          UseCounter::Count(*this,
                            WebFeature::kBodyScrollsInAdditionToViewport);
        }
      }
    }

    // TODO(954423, 952711): overscroll-behavior (and most likely
    // overflow-anchor) should be propagated from the document element and not
    // the viewport defining element.
    PROPAGATE_FROM(overflow_style, OverscrollBehaviorX, SetOverscrollBehaviorX,
                   EOverscrollBehavior::kAuto);
    PROPAGATE_FROM(overflow_style, OverscrollBehaviorY, SetOverscrollBehaviorY,
                   EOverscrollBehavior::kAuto);

    // Counts any time scroll snapping and scroll padding break if we change its
    // viewport propagation logic. Scroll snapping only breaks if body has
    // non-none snap type that is different from the document one.
    // TODO(952711): Remove once propagation logic change is complete.
    if (document_element_style && body_style) {
      bool snap_type_is_different =
          !body_style->GetScrollSnapType().is_none &&
          (body_style->GetScrollSnapType() !=
           document_element_style->GetScrollSnapType());
      bool scroll_padding_is_different =
          body_style->ScrollPaddingTop() !=
              document_element_style->ScrollPaddingTop() ||
          body_style->ScrollPaddingBottom() !=
              document_element_style->ScrollPaddingBottom() ||
          body_style->ScrollPaddingLeft() !=
              document_element_style->ScrollPaddingLeft() ||
          body_style->ScrollPaddingRight() !=
              document_element_style->ScrollPaddingRight();

      if (snap_type_is_different) {
        UseCounter::Count(*this, WebFeature::kScrollSnapOnViewportBreaks);
      }
      if (scroll_padding_is_different) {
        UseCounter::Count(*this, WebFeature::kScrollPaddingOnViewportBreaks);
      }
    }

    EOverflow overflow_x = EOverflow::kAuto;
    EOverflow overflow_y = EOverflow::kAuto;
    EOverflowAnchor overflow_anchor = EOverflowAnchor::kAuto;

    if (overflow_style) {
      overflow_x = overflow_style->OverflowX();
      overflow_y = overflow_style->OverflowY();
      overflow_anchor = overflow_style->OverflowAnchor();
      // Visible overflow on the viewport is meaningless, and the spec says to
      // treat it as 'auto':
      if (overflow_x == EOverflow::kVisible)
        overflow_x = EOverflow::kAuto;
      if (overflow_y == EOverflow::kVisible)
        overflow_y = EOverflow::kAuto;
      if (overflow_anchor == EOverflowAnchor::kVisible)
        overflow_anchor = EOverflowAnchor::kAuto;

      if (IsInMainFrame()) {
        using OverscrollBehaviorType =
            cc::OverscrollBehavior::OverscrollBehaviorType;
        GetPage()->GetChromeClient().SetOverscrollBehavior(
            *GetFrame(),
            cc::OverscrollBehavior(static_cast<OverscrollBehaviorType>(
                                       overflow_style->OverscrollBehaviorX()),
                                   static_cast<OverscrollBehaviorType>(
                                       overflow_style->OverscrollBehaviorY())));
      }

      if (overflow_style->HasPseudoElementStyle(kPseudoIdScrollbar))
        update_scrollbar_style = true;
    }

    PROPAGATE_VALUE(overflow_x, OverflowX, SetOverflowX)
    PROPAGATE_VALUE(overflow_y, OverflowY, SetOverflowY)
    PROPAGATE_VALUE(overflow_anchor, OverflowAnchor, SetOverflowAnchor);
  }

  // Misc
  {
    PROPAGATE_FROM(document_element_style, GetEffectiveTouchAction,
                   SetEffectiveTouchAction, TouchAction::kTouchActionAuto);
    PROPAGATE_FROM(document_element_style, GetScrollBehavior, SetScrollBehavior,
                   kScrollBehaviorAuto);
    PROPAGATE_FROM(document_element_style, DarkColorScheme, SetDarkColorScheme,
                   false);
  }

  changed |= PropagateScrollSnapStyleToViewport(*this, document_element_style,
                                                new_viewport_style);

  if (changed) {
    new_viewport_style->UpdateFontOrientation();
    GetLayoutView()->SetStyle(new_viewport_style);
    SetupFontBuilder(*new_viewport_style);
  }

  if (changed || update_scrollbar_style) {
    if (PaintLayerScrollableArea* scrollable_area =
            GetLayoutView()->GetScrollableArea()) {
      if (scrollable_area->HorizontalScrollbar() &&
          scrollable_area->HorizontalScrollbar()->IsCustomScrollbar())
        scrollable_area->HorizontalScrollbar()->StyleChanged();
      if (scrollable_area->VerticalScrollbar() &&
          scrollable_area->VerticalScrollbar()->IsCustomScrollbar())
        scrollable_area->VerticalScrollbar()->StyleChanged();
    }
  }
}
#undef PROPAGATE_VALUE
#undef PROPAGATE_FROM

#if DCHECK_IS_ON()
static void AssertLayoutTreeUpdated(Node& root) {
  Node* node = &root;
  while (node) {
    auto* element = DynamicTo<Element>(node);
    if (element &&
        RuntimeEnabledFeatures::DisplayLockingEnabled(
            root.GetExecutionContext()) &&
        element->StyleRecalcBlockedByDisplayLock(
            DisplayLockLifecycleTarget::kChildren)) {
      node = FlatTreeTraversal::NextSkippingChildren(*node);
      continue;
    }

    DCHECK(!node->NeedsStyleRecalc());
    DCHECK(!node->ChildNeedsStyleRecalc());
    DCHECK(!node->NeedsReattachLayoutTree());
    DCHECK(!node->ChildNeedsReattachLayoutTree());
    DCHECK(!node->ChildNeedsDistributionRecalc());
    DCHECK(!node->NeedsStyleInvalidation());
    DCHECK(!node->ChildNeedsStyleInvalidation());
    DCHECK(!node->GetForceReattachLayoutTree());
    // Make sure there is no node which has a LayoutObject, but doesn't have a
    // parent in a flat tree. If there is such a node, we forgot to detach the
    // node. DocumentNode is only an exception.
    DCHECK((node->IsDocumentNode() || !node->GetLayoutObject() ||
            FlatTreeTraversal::Parent(*node)))
        << *node;

    node = FlatTreeTraversal::Next(*node);
  }
}
#endif

void Document::UpdateStyleAndLayoutTree() {
  DCHECK(IsMainThread());
  if (Lifecycle().LifecyclePostponed())
    return;

  HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
  ScriptForbiddenScope forbid_script;

  if (HTMLFrameOwnerElement* owner = LocalOwner()) {
    owner->GetDocument().UpdateStyleAndLayoutTree();
  }

  if (!View() || !IsActive())
    return;

  if (View()->ShouldThrottleRendering())
    return;

  // RecalcSlotAssignments should be done before checking
  // NeedsLayoutTreeUpdate().
  GetSlotAssignmentEngine().RecalcSlotAssignments();

  // We can call FlatTreeTraversal::AssertFlatTreeNodeDataUpdated just after
  // calling RecalcSlotAssignments(), however, it would be better to call it at
  // least after InStyleRecalc() check below in order to avoid superfluous
  // check, which would be the cause of web tests timeout when dcheck is on.

  SlotAssignmentRecalcForbiddenScope forbid_slot_recalc(*this);

  if (!NeedsLayoutTreeUpdate()) {
    if (Lifecycle().GetState() < DocumentLifecycle::kStyleClean) {
      // needsLayoutTreeUpdate may change to false without any actual layout
      // tree update.  For example, needsAnimationTimingUpdate may change to
      // false when time elapses.  Advance lifecycle to StyleClean because style
      // is actually clean now.
      Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
      Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);
    }
    return;
  }

  if (InStyleRecalc())
    return;

#if DCHECK_IS_ON()
  int assigned_nodes_in_slot_count = 0;
  int nodes_which_have_assigned_slot_count = 0;
  FlatTreeTraversal::AssertFlatTreeNodeDataUpdated(
      *this, assigned_nodes_in_slot_count,
      nodes_which_have_assigned_slot_count);
  DCHECK_EQ(assigned_nodes_in_slot_count, nodes_which_have_assigned_slot_count);
#endif

  // Entering here from inside layout, paint etc. would be catastrophic since
  // recalcStyle can tear down the layout tree or (unfortunately) run
  // script. Kill the whole layoutObject if someone managed to get into here in
  // states not allowing tree mutations.
  CHECK(Lifecycle().StateAllowsTreeMutations());

  TRACE_EVENT_BEGIN1("blink,devtools.timeline", "UpdateLayoutTree", "beginData",
                     inspector_recalculate_styles_event::Data(GetFrame()));

  unsigned start_element_count = GetStyleEngine().StyleForElementCount();

  probe::RecalculateStyle recalculate_style_scope(this);

  DocumentAnimations::UpdateAnimationTimingIfNeeded(*this);
  EvaluateMediaQueryListIfNeeded();
  UpdateUseShadowTreesIfNeeded();

  UpdateDistributionForLegacyDistributedNodes();

  UpdateActiveStyle();
  UpdateStyleInvalidationIfNeeded();
  UpdateStyle();

  NotifyLayoutTreeOfSubtreeChanges();

  if (focused_element_ && !focused_element_->IsFocusable())
    ClearFocusedElementSoon();
  GetLayoutView()->ClearHitTestCache();

  DCHECK(!DocumentAnimations::NeedsAnimationTimingUpdate(*this));

  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_element_count;

  TRACE_EVENT_END1("blink,devtools.timeline", "UpdateLayoutTree",
                   "elementCount", element_count);

#if DCHECK_IS_ON()
  AssertLayoutTreeUpdated(*this);
#endif
}

void Document::UpdateActiveStyle() {
  DCHECK(IsActive());
  DCHECK(IsMainThread());
  TRACE_EVENT0("blink", "Document::updateActiveStyle");
  GetStyleEngine().UpdateActiveStyle();
}

void Document::UpdateStyle() {
  DCHECK(!View()->ShouldThrottleRendering());
  TRACE_EVENT_BEGIN0("blink,blink_style", "Document::updateStyle");
  RUNTIME_CALL_TIMER_SCOPE(V8PerIsolateData::MainThreadIsolate(),
                           RuntimeCallStats::CounterId::kUpdateStyle);

  unsigned initial_element_count = GetStyleEngine().StyleForElementCount();

  lifecycle_.AdvanceTo(DocumentLifecycle::kInStyleRecalc);

  // SetNeedsStyleRecalc should only happen on Element and Text nodes.
  DCHECK(!NeedsStyleRecalc());

  StyleResolver& resolver = EnsureStyleResolver();
  bool should_record_stats;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("blink,blink_style", &should_record_stats);
  GetStyleEngine().SetStatsEnabled(should_record_stats);

  GetStyleEngine().UpdateStyleAndLayoutTree();

  ClearChildNeedsStyleRecalc();

  PropagateStyleToViewport();

  View()->UpdateCountersAfterStyleChange();
  GetLayoutView()->RecalcLayoutOverflow();

  DCHECK(!NeedsStyleRecalc());
  DCHECK(!ChildNeedsStyleRecalc());
  DCHECK(!NeedsReattachLayoutTree());
  DCHECK(!ChildNeedsReattachLayoutTree());
  DCHECK(InStyleRecalc());
  DCHECK_EQ(GetStyleResolver(), &resolver);
  lifecycle_.AdvanceTo(DocumentLifecycle::kStyleClean);
  if (should_record_stats) {
    TRACE_EVENT_END2(
        "blink,blink_style", "Document::updateStyle", "resolverAccessCount",
        GetStyleEngine().StyleForElementCount() - initial_element_count,
        "counters", GetStyleEngine().Stats()->ToTracedValue());
  } else {
    TRACE_EVENT_END1(
        "blink,blink_style", "Document::updateStyle", "resolverAccessCount",
        GetStyleEngine().StyleForElementCount() - initial_element_count);
  }
}

void Document::NotifyLayoutTreeOfSubtreeChanges() {
  if (!GetLayoutView()->WasNotifiedOfSubtreeChange())
    return;

  lifecycle_.AdvanceTo(DocumentLifecycle::kInLayoutSubtreeChange);

  GetLayoutView()->HandleSubtreeModifications();
  DCHECK(!GetLayoutView()->WasNotifiedOfSubtreeChange());

  lifecycle_.AdvanceTo(DocumentLifecycle::kLayoutSubtreeChangeClean);
}

bool Document::NeedsLayoutTreeUpdateForNode(const Node& node,
                                            bool ignore_adjacent_style) const {
  // TODO(rakina): Switch some callers that may need to call
  // NeedsLayoutTreeUpdateForNodeIncludingDisplayLocked instead of this.
  if (DisplayLockUtilities::NearestLockedExclusiveAncestor(node)) {
    // |node| is in a locked-subtree, so we don't need to update it.
    return false;
  }
  return NeedsLayoutTreeUpdateForNodeIncludingDisplayLocked(
      node, ignore_adjacent_style);
}

bool Document::NeedsLayoutTreeUpdateForNodeIncludingDisplayLocked(
    const Node& node,
    bool ignore_adjacent_style) const {
  if (!node.CanParticipateInFlatTree())
    return false;
  if (locked_display_lock_count_ == 0 && !NeedsLayoutTreeUpdate())
    return false;
  if (!node.isConnected())
    return false;

  if (NeedsFullLayoutTreeUpdate() || node.NeedsStyleRecalc() ||
      node.NeedsStyleInvalidation())
    return true;
  for (const ContainerNode* ancestor = LayoutTreeBuilderTraversal::Parent(node);
       ancestor; ancestor = LayoutTreeBuilderTraversal::Parent(*ancestor)) {
    if (ShadowRoot* root = ancestor->GetShadowRoot()) {
      if (root->NeedsStyleRecalc() || root->NeedsStyleInvalidation() ||
          root->NeedsAdjacentStyleRecalc()) {
        return true;
      }
    }
    if (ancestor->NeedsStyleRecalc() || ancestor->NeedsStyleInvalidation() ||
        (ancestor->NeedsAdjacentStyleRecalc() && !ignore_adjacent_style)) {
      return true;
    }
    auto* element = DynamicTo<Element>(ancestor);
    if (!element)
      continue;
    if (auto* context = element->GetDisplayLockContext()) {
      // Even if the ancestor is style-clean, we might've previously
      // blocked a style traversal going to the ancestor or its descendants.
      if (context->StyleTraversalWasBlocked()) {
        DCHECK(context->IsLocked());
        return true;
      }
    }
  }
  return false;
}

void Document::UpdateStyleAndLayoutTreeForNode(const Node* node) {
  DCHECK(node);
  if (!node->InActiveDocument()) {
    // If |node| is not in the active document, we can't update its style or
    // layout tree.
    DCHECK_EQ(node->ownerDocument(), this);
    return;
  }
  DCHECK(!InStyleRecalc())
      << "UpdateStyleAndLayoutTreeForNode called from within style recalc";
  if (!NeedsLayoutTreeUpdateForNodeIncludingDisplayLocked(*node))
    return;

  DisplayLockUtilities::ScopedChainForcedUpdate scoped_update_forced(node);
  UpdateStyleAndLayoutTree();
}

void Document::UpdateStyleAndLayoutTreeForSubtree(const Node* node) {
  DCHECK(node);
  if (!node->InActiveDocument()) {
    DCHECK_EQ(node->ownerDocument(), this);
    return;
  }
  DCHECK(!InStyleRecalc())
      << "UpdateStyleAndLayoutTreeForSubtree called from within style recalc";

  if (NeedsLayoutTreeUpdateForNodeIncludingDisplayLocked(*node) ||
      node->ChildNeedsStyleRecalc() || node->ChildNeedsStyleInvalidation()) {
    DisplayLockUtilities::ScopedChainForcedUpdate scoped_update_forced(node);
    UpdateStyleAndLayoutTree();
  }
}

void Document::UpdateStyleAndLayoutForNode(const Node* node) {
  DCHECK(node);
  if (!node->InActiveDocument())
    return;

  DisplayLockUtilities::ScopedChainForcedUpdate scoped_update_forced(node);
  UpdateStyleAndLayout();
}

void Document::UpdateStyleAndLayout(ForcedLayoutStatus status) {
  DCHECK(IsMainThread());
  LocalFrameView* frame_view = View();

  if (status == IsForcedLayout && frame_view)
    frame_view->WillStartForcedLayout();

  HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
  ScriptForbiddenScope forbid_script;

  DCHECK(!frame_view || !frame_view->IsInPerformLayout())
      << "View layout should not be re-entrant";

  if (HTMLFrameOwnerElement* owner = LocalOwner())
    owner->GetDocument().UpdateStyleAndLayout();

  UpdateStyleAndLayoutTree();

  if (!IsActive())
    return;

  if (frame_view && frame_view->NeedsLayout())
    frame_view->UpdateLayout();

  if (Lifecycle().GetState() < DocumentLifecycle::kLayoutClean)
    Lifecycle().AdvanceTo(DocumentLifecycle::kLayoutClean);

  if (LocalFrameView* frame_view_anchored = View())
    frame_view_anchored->PerformScrollAnchoringAdjustments();

  if (status == IsForcedLayout && frame_view)
    frame_view->DidFinishForcedLayout();

  if (update_focus_appearance_after_layout_)
    UpdateFocusAppearance();
}

void Document::LayoutUpdated() {
  DCHECK(GetFrame());
  DCHECK(View());

  // If we're restoring a scroll position from history, that takes precedence
  // over scrolling to the anchor in the URL.
  View()->InvokeFragmentAnchor();
  GetFrame()->Loader().RestoreScrollPositionAndViewState();

  // Plugins can run script inside layout which can detach the page.
  // TODO(dcheng): Does it make sense to do any of this work if detached?
  if (GetFrame()) {
    if (GetFrame()->IsMainFrame())
      GetFrame()->GetPage()->GetChromeClient().MainFrameLayoutUpdated();

    // We do attach here, during lifecycle update, because until then we
    // don't have a good place that has access to its local root's FrameWidget.
    // TODO(dcheng): If we create FrameWidget before Frame then we could move
    // this to Document::Initialize().
    if (Platform::Current()->IsThreadedAnimationEnabled() &&
        GetSettings()->GetAcceleratedCompositingEnabled()) {
      GetPage()->GetChromeClient().AttachCompositorAnimationTimeline(
          Timeline().CompositorTimeline(), GetFrame());
    }
  }

  Markers().InvalidateRectsForAllTextMatchMarkers();

  // TODO(esprehn): This doesn't really make sense, why not track the first
  // beginFrame? This will catch the first layout in a page that does lots
  // of layout thrashing even though that layout might not be followed by
  // a paint for many seconds.
  if (HaveRenderBlockingResourcesLoaded()) {
    if (document_timing_.FirstLayout().is_null())
      document_timing_.MarkFirstLayout();
  }
}

void Document::ClearFocusedElementSoon() {
  if (!clear_focused_element_timer_.IsActive())
    clear_focused_element_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void Document::ClearFocusedElementTimerFired(TimerBase*) {
  UpdateStyleAndLayoutTree();

  if (focused_element_ && !focused_element_->IsFocusable())
    focused_element_->blur();
}

scoped_refptr<const ComputedStyle> Document::StyleForPage(int page_index) {
  UpdateDistributionForUnknownReasons();
  return EnsureStyleResolver().StyleForPage(page_index);
}

void Document::EnsurePaintLocationDataValidForNode(const Node* node) {
  DCHECK(node);
  if (!node->InActiveDocument())
    return;

  DisplayLockUtilities::ScopedChainForcedUpdate scoped_update_forced(node);

  // For all nodes we must have up-to-date style and have performed layout to do
  // any location-based calculation.
  UpdateStyleAndLayout();

  // The location of elements that are position: sticky is not known until
  // compositing inputs are cleaned. Therefore, for any elements that are either
  // sticky or are in a sticky sub-tree (e.g. are affected by a sticky element),
  // we need to also clean compositing inputs.
  if (View() && node->GetLayoutObject() &&
      node->GetLayoutObject()->StyleRef().SubtreeIsSticky()) {
    bool success = false;
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      // In CAP, compositing inputs are cleaned as part of PrePaint.
      success = View()->UpdateAllLifecyclePhasesExceptPaint();
    } else {
      success = View()->UpdateLifecycleToCompositingInputsClean();
    }
    // The lifecycle update should always succeed, because forced lifecycles
    // from script are never throttled.
    DCHECK(success);
  }
}

bool Document::IsPageBoxVisible(int page_index) {
  return StyleForPage(page_index)->Visibility() !=
         EVisibility::kHidden;  // display property doesn't apply to @page.
}

void Document::PageSizeAndMarginsInPixels(int page_index,
                                          DoubleSize& page_size,
                                          int& margin_top,
                                          int& margin_right,
                                          int& margin_bottom,
                                          int& margin_left) {
  scoped_refptr<const ComputedStyle> style = StyleForPage(page_index);

  double width = page_size.Width();
  double height = page_size.Height();
  switch (style->PageSizeType()) {
    case EPageSizeType::kAuto:
      break;
    case EPageSizeType::kLandscape:
      if (width < height)
        std::swap(width, height);
      break;
    case EPageSizeType::kPortrait:
      if (width > height)
        std::swap(width, height);
      break;
    case EPageSizeType::kResolved: {
      FloatSize size = style->PageSize();
      width = size.Width();
      height = size.Height();
      break;
    }
    default:
      NOTREACHED();
  }
  page_size = DoubleSize(width, height);

  // The percentage is calculated with respect to the width even for margin top
  // and bottom.
  // http://www.w3.org/TR/CSS2/box.html#margin-properties
  margin_top = style->MarginTop().IsAuto()
                   ? margin_top
                   : IntValueForLength(style->MarginTop(), width);
  margin_right = style->MarginRight().IsAuto()
                     ? margin_right
                     : IntValueForLength(style->MarginRight(), width);
  margin_bottom = style->MarginBottom().IsAuto()
                      ? margin_bottom
                      : IntValueForLength(style->MarginBottom(), width);
  margin_left = style->MarginLeft().IsAuto()
                    ? margin_left
                    : IntValueForLength(style->MarginLeft(), width);
}

void Document::SetIsViewSource(bool is_view_source) {
  is_view_source_ = is_view_source;
  if (!is_view_source_)
    return;
}

void Document::SetIsImmersiveArOverlay(bool val) {
  if (!documentElement())
    return;

  if (val != is_immersive_ar_overlay_) {
    DCHECK(RuntimeEnabledFeatures::WebXRARDOMOverlayEnabled(this));
    is_immersive_ar_overlay_ = val;

    // If the property has changed, apply the pseudo-style change to the root
    // element. This will cascade further UA stylesheet changes such as setting
    // the fullscreened element and its backdrop transparent.
    documentElement()->PseudoStateChanged(
        CSSSelector::kPseudoXrImmersiveDomOverlay);

    // Ensure that the graphics layer tree gets fully rebuilt on changes,
    // similar to HTMLVideoElement::DidEnterFullscreen(). This may not be
    // strictly necessary if the compositing changes are based on visibility
    // settings, but helps ensure consistency in case it's changed to
    // detaching layers or re-rooting the graphics layer tree.
    GetLayoutView()->Compositor()->SetNeedsCompositingUpdate(
        kCompositingUpdateRebuildTree);
  }
}

void Document::ScheduleUseShadowTreeUpdate(SVGUseElement& element) {
  use_elements_needing_update_.insert(&element);
  ScheduleLayoutTreeUpdateIfNeeded();
}

void Document::UnscheduleUseShadowTreeUpdate(SVGUseElement& element) {
  use_elements_needing_update_.erase(&element);
}

void Document::UpdateUseShadowTreesIfNeeded() {
  if (use_elements_needing_update_.IsEmpty())
    return;

  ScriptForbiddenScope forbid_script;

  HeapHashSet<Member<SVGUseElement>> elements;
  use_elements_needing_update_.swap(elements);
  for (SVGUseElement* element : elements)
    element->BuildPendingResource();

  DCHECK(use_elements_needing_update_.IsEmpty());
}

StyleResolver* Document::GetStyleResolver() const {
  return style_engine_->Resolver();
}

StyleResolver& Document::EnsureStyleResolver() const {
  return style_engine_->EnsureResolver();
}

void Document::Initialize() {
  DCHECK_EQ(lifecycle_.GetState(), DocumentLifecycle::kInactive);
  DCHECK(!ax_object_cache_ || this != &AXObjectCacheOwner());

  layout_view_ = new LayoutView(this);
  SetLayoutObject(layout_view_);

  layout_view_->SetStyle(StyleResolver::StyleForViewport(*this));

  AttachContext context;
  AttachLayoutTree(context);

  // The TextAutosizer can't update layout view info while the Document is
  // detached, so update now in case anything changed.
  if (TextAutosizer* autosizer = GetTextAutosizer())
    autosizer->UpdatePageInfo();

  frame_->DidAttachDocument();
  lifecycle_.AdvanceTo(DocumentLifecycle::kStyleClean);

  if (View())
    View()->DidAttachDocument();

  // Observer(s) should not be initialized until the document is initialized /
  // attached to a frame. Otherwise ContextLifecycleObserver::contextDestroyed
  // wouldn't be fired.
  network_state_observer_ = MakeGarbageCollected<NetworkStateObserver>(*this);

  // Check for frame_ so we only attach execution contexts with its own
  // scheduler.
  if (frame_)
    GetAgent()->AttachExecutionContext(this);
}

void Document::Shutdown() {
  TRACE_EVENT0("blink", "Document::shutdown");
  CHECK(!frame_ || frame_->Tree().ChildCount() == 0);
  if (!IsActive())
    return;

  // An active Document must have an associated frame.
  CHECK(frame_);

  // Frame navigation can cause a new Document to be attached. Don't allow that,
  // since that will cause a situation where LocalFrame still has a Document
  // attached after this finishes!  Normally, it shouldn't actually be possible
  // to trigger navigation here.  However, plugins (see below) can cause lots of
  // crazy things to happen, since plugin detach involves nested run loops.
  FrameNavigationDisabler navigation_disabler(*frame_);
  // Defer plugin dispose to avoid plugins trying to run script inside
  // ScriptForbiddenScope, which will crash the renderer after
  // https://crrev.com/200984
  // TODO(dcheng): This is a temporary workaround, Document::Shutdown() should
  // not be running script at all.
  HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
  // Don't allow script to run in the middle of DetachLayoutTree() because a
  // detaching Document is not in a consistent state.
  ScriptForbiddenScope forbid_script;

  lifecycle_.AdvanceTo(DocumentLifecycle::kStopping);

  // Do not add code before this without a documented reason. A postcondition of
  // Shutdown() is that |frame_| must not have an attached Document. Allowing
  // script execution when the Document is shutting down can make it easy to
  // accidentally violate this condition, and the ordering of the scopers above
  // is subtle due to legacy interactions with plugins.

  if (num_canvases_ > 0)
    UMA_HISTOGRAM_COUNTS_100("Blink.Canvas.NumCanvasesPerPage", num_canvases_);

  GetViewportData().Shutdown();

  View()->Dispose();
  // TODO(crbug.com/729196): Trace why LocalFrameView::DetachFromLayout crashes.
  CHECK(!View()->IsAttached());

  // If the EmbeddedContentView of the document's frame owner doesn't match
  // view() then LocalFrameView::Dispose() didn't clear the owner's
  // EmbeddedContentView. If we don't clear it here, it may be clobbered later
  // in LocalFrame::CreateView(). See also https://crbug.com/673170 and the
  // comment in LocalFrameView::Dispose().
  HTMLFrameOwnerElement* owner_element = frame_->DeprecatedLocalOwner();

  // In the case of a provisional frame, skip clearing the EmbeddedContentView.
  // A provisional frame is not fully attached to the DOM yet and clearing the
  // EmbeddedContentView here could clear a not-yet-swapped-out frame
  // (https://crbug.com/807772).
  if (owner_element && !frame_->IsProvisional())
    owner_element->SetEmbeddedContentView(nullptr);

  markers_->PrepareForDestruction();

  GetPage()->DocumentDetached(this);

  probe::DocumentDetached(this);

  // FIXME: consider using ContextLifecycleStateObserver.
  if (scripted_animation_controller_)
    scripted_animation_controller_->ClearDocumentPointer();
  scripted_animation_controller_.Clear();

  scripted_idle_task_controller_.Clear();

  if (SvgExtensions())
    AccessSVGExtensions().PauseAnimations();

  CancelPendingJavaScriptUrls();
  http_refresh_scheduler_->Cancel();

  if (Platform::Current()->IsThreadedAnimationEnabled() &&
      GetSettings()->GetAcceleratedCompositingEnabled()) {
    GetPage()->GetChromeClient().DetachCompositorAnimationTimeline(
        Timeline().CompositorTimeline(), GetFrame());
  }

  if (frame_->IsLocalRoot())
    GetPage()->GetChromeClient().AttachRootLayer(nullptr, frame_.Get());
  layout_view_->CleanUpCompositor();

  if (RegistrationContext())
    RegistrationContext()->DocumentWasDetached();

  MutationObserver::CleanSlotChangeList(*this);

  hover_element_ = nullptr;
  active_element_ = nullptr;
  autofocus_candidates_.clear();

  if (focused_element_.Get()) {
    Element* old_focused_element = focused_element_;
    focused_element_ = nullptr;
    NotifyFocusedElementChanged(old_focused_element, nullptr);
  }
  sequential_focus_navigation_starting_point_ = nullptr;

  if (this == &AXObjectCacheOwner()) {
    ax_contexts_.clear();
    ClearAXObjectCache();
  }
  computed_node_mapping_.clear();

  layout_view_ = nullptr;
  DetachLayoutTree();
  // TODO(crbug.com/729196): Trace why LocalFrameView::DetachFromLayout crashes.
  CHECK(!View()->IsAttached());

  if (this != &AXObjectCacheOwner()) {
    if (AXObjectCache* cache = ExistingAXObjectCache()) {
      // Documents that are not a root document use the AXObjectCache in
      // their root document. Node::removedFrom is called after the
      // document has been detached so it can't find the root document.
      // We do the removals here instead.
      for (Node& node : NodeTraversal::DescendantsOf(*this)) {
        cache->Remove(&node);
      }
    }
  }

  GetStyleEngine().DidDetach();

  frame_->GetEventHandlerRegistry().DocumentDetached(*this);

  // Signal destruction to mutation observers.
  DocumentShutdownNotifier::NotifyContextDestroyed();
  SynchronousMutationNotifier::NotifyContextDestroyed();

  cookie_jar_ = nullptr;  // Not accessible after navigated away.
  fetcher_->ClearContext();
  // If this document is the master for an HTMLImportsController, sever that
  // relationship. This ensures that we don't leave import loads in flight,
  // thinking they should have access to a valid frame when they don't.
  if (imports_controller_) {
    imports_controller_->Dispose();
    ClearImportsController();
  }

  if (media_query_matcher_)
    media_query_matcher_->DocumentDetached();

  lifecycle_.AdvanceTo(DocumentLifecycle::kStopped);
  // TODO(crbug.com/729196): Trace why LocalFrameView::DetachFromLayout crashes.
  CHECK(!View()->IsAttached());

  // Check for frame_ so we only detach execution contexts with its own
  // scheduler.
  // TODO(bokan): Can this happen? |frame_| is dereferenced above and CHECKed
  // at top.
  if (frame_)
    GetAgent()->DetachExecutionContext(this);

  // TODO(haraken): Call contextDestroyed() before we start any disruptive
  // operations.
  // TODO(haraken): Currently we call notifyContextDestroyed() only in
  // Document::detachLayoutTree(), which means that we don't call
  // notifyContextDestroyed() for a document that doesn't get detached.
  // If such a document has any observer, the observer won't get
  // a contextDestroyed() notification. This can happen for a document
  // created by DOMImplementation::createDocument().
  ExecutionContext::NotifyContextDestroyed();
  // TODO(crbug.com/729196): Trace why LocalFrameView::DetachFromLayout crashes.
  CHECK(!View()->IsAttached());

  needs_to_record_ukm_outlive_time_ = IsInMainFrame();
  if (needs_to_record_ukm_outlive_time_) {
    // Ensure |ukm_recorder_| and |ukm_source_id_|.
    UkmRecorder();
  }

  mime_handler_view_before_unload_event_listener_ = nullptr;

  resource_coordinator_.reset();

  // This is required, as our LocalFrame might delete itself as soon as it
  // detaches us. However, this violates Node::detachLayoutTree() semantics, as
  // it's never possible to re-attach. Eventually Document::detachLayoutTree()
  // should be renamed, or this setting of the frame to 0 could be made
  // explicit in each of the callers of Document::detachLayoutTree().
  frame_ = nullptr;

  document_outlive_time_reporter_ =
      std::make_unique<DocumentOutliveTimeReporter>(this);
}

void Document::RemoveAllEventListeners() {
  ContainerNode::RemoveAllEventListeners();

  if (LocalDOMWindow* dom_window = domWindow())
    dom_window->RemoveAllEventListeners();
}

Document& Document::AXObjectCacheOwner() const {
  // Every document has its own axObjectCache if accessibility is enabled,
  // except for page popups, which share the axObjectCache of their owner.
  Document* doc = const_cast<Document*>(this);
  if (doc->GetFrame() && doc->GetFrame()->PagePopupOwner()) {
    DCHECK(!doc->ax_object_cache_);
    return doc->GetFrame()
        ->PagePopupOwner()
        ->GetDocument()
        .AXObjectCacheOwner();
  }
  return *doc;
}

void Document::AddAXContext(AXContext* context) {
  // The only case when |&cache_owner| is not |this| is when this is a
  // pop-up. We want pop-ups to share the AXObjectCache of their parent
  // document. However, there's no valid reason to explicitly create an
  // AXContext for a pop-up document, so check to make sure we're not
  // trying to do that here.
  DCHECK_EQ(&AXObjectCacheOwner(), this);

  // If the document has already been detached, do not make a new AXObjectCache.
  if (!GetLayoutView())
    return;

  ax_contexts_.push_back(context);
  if (ax_contexts_.size() != 1)
    return;

  if (!ax_object_cache_)
    ax_object_cache_ = AXObjectCache::Create(*this);
}

void Document::RemoveAXContext(AXContext* context) {
  auto** iter =
      std::find_if(ax_contexts_.begin(), ax_contexts_.end(),
                   [&context](const auto& item) { return item == context; });
  if (iter != ax_contexts_.end())
    ax_contexts_.erase(iter);
  if (ax_contexts_.size() == 0)
    ClearAXObjectCache();
}

void Document::ClearAXObjectCache() {
  DCHECK_EQ(&AXObjectCacheOwner(), this);

  // Clear the cache member variable before calling delete because attempts
  // are made to access it during destruction.
  if (ax_object_cache_)
    ax_object_cache_->Dispose();
  ax_object_cache_.Clear();

  // If there's at least one AXContext in scope and there's still a LayoutView
  // around, recreate an empty AXObjectCache.
  //
  // TODO(dmazzoni): right now ClearAXObjectCache() is being used as a way
  // to invalidate / reset the AXObjectCache while keeping it around. We
  // should rewrite that as a method on AXObjectCache rather than destroying
  // and recreating it here.
  if (ax_contexts_.size() > 0 && GetLayoutView())
    ax_object_cache_ = AXObjectCache::Create(*this);
}

AXObjectCache* Document::ExistingAXObjectCache() const {
  auto& cache_owner = AXObjectCacheOwner();

  // If the LayoutView is gone then we are in the process of destruction.
  // This method will be called before frame_ = nullptr.
  if (!cache_owner.GetLayoutView())
    return nullptr;

  return cache_owner.ax_object_cache_.Get();
}

CanvasFontCache* Document::GetCanvasFontCache() {
  if (!canvas_font_cache_)
    canvas_font_cache_ = MakeGarbageCollected<CanvasFontCache>(*this);

  return canvas_font_cache_.Get();
}

DocumentParser* Document::CreateParser() {
  if (IsHTMLDocument()) {
    return MakeGarbageCollected<HTMLDocumentParser>(ToHTMLDocument(*this),
                                                    parser_sync_policy_);
  }
  // FIXME: this should probably pass the frame instead
  return MakeGarbageCollected<XMLDocumentParser>(*this, View());
}

bool Document::IsFrameSet() const {
  if (!IsHTMLDocument())
    return false;
  return IsA<HTMLFrameSetElement>(body());
}

ScriptableDocumentParser* Document::GetScriptableDocumentParser() const {
  return Parser() ? Parser()->AsScriptableDocumentParser() : nullptr;
}

void Document::DisplayNoneChangedForFrame() {
  if (!documentElement())
    return;
  // LayoutView()::CanHaveChildren(), hence the existence of style and
  // layout tree, depends on the owner being display:none or not. Trigger
  // detaching or attaching the style/layout-tree as a result of that
  // changing.
  documentElement()->SetNeedsStyleRecalc(
      kLocalStyleChange,
      StyleChangeReasonForTracing::Create(style_change_reason::kFrame));
}

void Document::SetPrinting(PrintingState state) {
  bool was_printing = Printing();
  printing_ = state;
  bool is_printing = Printing();

  if ((was_printing != is_printing) && documentElement() && GetFrame() &&
      !GetFrame()->IsMainFrame() && GetFrame()->Owner() &&
      GetFrame()->Owner()->IsDisplayNone()) {
    // In non-printing mode we do not generate style or layout objects for
    // display:none iframes, yet we do when printing (see
    // LayoutView::CanHaveChildren). Trigger a style recalc on the root element
    // to create a layout tree for printing.
    DisplayNoneChangedForFrame();
  }
}

void Document::SetIsPaintingPreview(bool is_painting_preview) {
  is_painting_preview_ = is_painting_preview;
}

// https://html.spec.whatwg.org/C/dynamic-markup-insertion.html#document-open-steps
void Document::open(Document* entered_document,
                    ExceptionState& exception_state) {
  if (ImportLoader()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Imported document doesn't support open().");
    return;
  }

  // If |document| is an XML document, then throw an "InvalidStateError"
  // DOMException exception.
  if (!IsHTMLDocument()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Only HTML documents support open().");
    return;
  }

  // If |document|'s throw-on-dynamic-markup-insertion counter is greater than
  // 0, then throw an "InvalidStateError" DOMException.
  if (throw_on_dynamic_markup_insertion_count_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Custom Element constructor should not use open().");
    return;
  }

  if (!AllowedToUseDynamicMarkUpInsertion("open", exception_state))
    return;

  // If |document|'s origin is not same origin to the origin of the responsible
  // document specified by the entry settings object, then throw a
  // "SecurityError" DOMException.
  if (entered_document && !GetSecurityOrigin()->IsSameSchemeHostPort(
                              entered_document->GetSecurityOrigin())) {
    exception_state.ThrowSecurityError(
        "Can only call open() on same-origin documents.");
    return;
  }

  // If |document| has an active parser whose script nesting level is greater
  // than 0, then return |document|.
  if (ScriptableDocumentParser* parser = GetScriptableDocumentParser()) {
    if (parser->IsParsing() && parser->IsExecutingScript())
      return;
  }

  // Similarly, if |document|'s ignore-opens-during-unload counter is greater
  // than 0, then return |document|.
  if (ignore_opens_during_unload_count_)
    return;

  if (ignore_opens_and_writes_for_abort_)
    return;

  // Change |document|'s URL to the URL of the responsible document specified
  // by the entry settings object.
  if (entered_document && this != entered_document) {
    auto* csp = MakeGarbageCollected<ContentSecurityPolicy>();
    csp->CopyStateFrom(entered_document->GetContentSecurityPolicy());
    // We inherit the sandbox flags of the entered document, so mask on
    // the ones contained in the CSP.
    sandbox_flags_ |= csp->GetSandboxMask();
    InitContentSecurityPolicy(csp);
    // Clear the hash fragment from the inherited URL to prevent a
    // scroll-into-view for any document.open()'d frame.
    KURL new_url = entered_document->Url();
    new_url.SetFragmentIdentifier(String());
    SetURL(new_url);
    if (Loader())
      Loader()->UpdateUrlForDocumentOpen(new_url);

    SetSecurityOrigin(entered_document->GetMutableSecurityOrigin());
    SetReferrerPolicy(entered_document->GetReferrerPolicy());
    SetCookieURL(entered_document->CookieURL());
  }

  open();
}

// https://html.spec.whatwg.org/C/dynamic-markup-insertion.html#document-open-steps
void Document::open() {
  DCHECK(!ImportLoader());
  DCHECK(!ignore_opens_during_unload_count_);
  if (ScriptableDocumentParser* parser = GetScriptableDocumentParser())
    DCHECK(!parser->IsParsing() || !parser->IsExecutingScript());

  // If |document| has a browsing context and there is an existing attempt to
  // navigate |document|'s browsing context, then stop document loading given
  // |document|.
  //
  // As noted in the spec and https://github.com/whatwg/html/issues/3975, we
  // want to treat ongoing navigation and queued navigation the same way.
  // However, we don't want to consider navigations scheduled too much into the
  // future through Refresh headers or a <meta> refresh pragma to be a current
  // navigation. Thus, we cut it off with
  // IsHttpRefreshScheduledWithin(base::TimeDelta()).
  //
  // This also prevents window.open(url) -- eg window.open("about:blank") --
  // from blowing away results from a subsequent window.document.open /
  // window.document.write call.
  if (frame_ && (frame_->Loader().HasProvisionalNavigation() ||
                 IsHttpRefreshScheduledWithin(base::TimeDelta()))) {
    frame_->Loader().StopAllLoaders();
    // Navigations handled by the client should also be cancelled.
    if (frame_ && frame_->Client())
      frame_->Client()->AbortClientNavigation();
  }
  CancelPendingJavaScriptUrls();

  // For each shadow-including inclusive descendant |node| of |document|, erase
  // all event listeners and handlers given |node|.
  //
  // Erase all event listeners and handlers given |window|.
  //
  // NB: Document::RemoveAllEventListeners() (called by
  // RemoveAllEventListenersRecursively()) erases event listeners from the
  // Window object as well.
  RemoveAllEventListenersRecursively();

  ResetTreeScope();
  if (frame_)
    frame_->Selection().Clear();

  // Create a new HTML parser and associate it with |document|.
  //
  // Set the current document readiness of |document| to "loading".
  ImplicitOpen(kForceSynchronousParsing);

  // This is a script-created parser.
  if (ScriptableDocumentParser* parser = GetScriptableDocumentParser())
    parser->SetWasCreatedByScript(true);

  if (frame_)
    frame_->Loader().DidExplicitOpen();
}

void Document::DetachParser() {
  if (!parser_)
    return;
  parser_->Detach();
  parser_.Clear();
  DocumentParserTiming::From(*this).MarkParserDetached();
}

void Document::CancelParsing() {
  // There appears to be an unspecced assumption that a document.open()
  // or document.write() immediately after a navigation start won't cancel
  // the navigation. Firefox avoids cancelling the navigation by ignoring an
  // open() or write() after an active parser is aborted. See
  // https://github.com/whatwg/html/issues/4723 for discussion about
  // standardizing this behavior.
  if (parser_ && parser_->IsParsing())
    ignore_opens_and_writes_for_abort_ = true;
  DetachParser();
  SetParsingState(kFinishedParsing);
  SetReadyState(kComplete);
  if (!LoadEventFinished())
    load_event_progress_ = kLoadEventCompleted;
  CancelPendingJavaScriptUrls();
  http_refresh_scheduler_->Cancel();
}

DocumentParser* Document::OpenForNavigation(
    ParserSynchronizationPolicy parser_sync_policy,
    const AtomicString& mime_type,
    const AtomicString& encoding) {
  DocumentParser* parser = ImplicitOpen(parser_sync_policy);
  if (parser->NeedsDecoder())
    parser->SetDecoder(BuildTextResourceDecoderFor(this, mime_type, encoding));
  return parser;
}

DocumentParser* Document::ImplicitOpen(
    ParserSynchronizationPolicy parser_sync_policy) {
  RemoveChildren();
  DCHECK(!focused_element_);

  SetCompatibilityMode(kNoQuirksMode);

  if (!ThreadedParsingEnabledForTesting()) {
    parser_sync_policy = kForceSynchronousParsing;
  } else if (parser_sync_policy == kAllowAsynchronousParsing &&
             IsPrefetchOnly()) {
    // Prefetch must be synchronous.
    parser_sync_policy = kForceSynchronousParsing;
  }

  DetachParser();
  parser_sync_policy_ = parser_sync_policy;
  parser_ = CreateParser();
  DocumentParserTiming::From(*this).MarkParserStart();
  SetParsingState(kParsing);
  SetReadyState(kLoading);
  if (load_event_progress_ != kLoadEventInProgress &&
      PageDismissalEventBeingDispatched() == kNoDismissal) {
    load_event_progress_ = kLoadEventNotRun;
  }

  return parser_;
}

HTMLElement* Document::body() const {
  if (!IsA<HTMLHtmlElement>(documentElement()))
    return nullptr;

  for (HTMLElement* child =
           Traversal<HTMLElement>::FirstChild(*documentElement());
       child; child = Traversal<HTMLElement>::NextSibling(*child)) {
    if (IsA<HTMLFrameSetElement>(*child) || IsA<HTMLBodyElement>(*child))
      return child;
  }

  return nullptr;
}

HTMLBodyElement* Document::FirstBodyElement() const {
  if (!IsA<HTMLHtmlElement>(documentElement()))
    return nullptr;

  for (HTMLElement* child =
           Traversal<HTMLElement>::FirstChild(*documentElement());
       child; child = Traversal<HTMLElement>::NextSibling(*child)) {
    if (auto* body = DynamicTo<HTMLBodyElement>(*child))
      return body;
  }

  return nullptr;
}

void Document::setBody(HTMLElement* prp_new_body,
                       ExceptionState& exception_state) {
  HTMLElement* new_body = prp_new_body;

  if (!new_body) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        ExceptionMessages::ArgumentNullOrIncorrectType(1, "HTMLElement"));
    return;
  }
  if (!documentElement()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kHierarchyRequestError,
                                      "No document element exists.");
    return;
  }

  if (!IsA<HTMLBodyElement>(*new_body) &&
      !IsA<HTMLFrameSetElement>(*new_body)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "The new body element is of type '" + new_body->tagName() +
            "'. It must be either a 'BODY' or 'FRAMESET' element.");
    return;
  }

  HTMLElement* old_body = body();
  if (old_body == new_body)
    return;

  if (old_body)
    documentElement()->ReplaceChild(new_body, old_body, exception_state);
  else
    documentElement()->AppendChild(new_body, exception_state);
}

void Document::WillInsertBody() {
  if (Loader())
    fetcher_->LoosenLoadThrottlingPolicy();

  // If we get to the <body> try to resume commits since we should have content
  // to paint now.
  // TODO(esprehn): Is this really optimal? We might start producing frames
  // for very little content, should we wait for some heuristic like
  // isVisuallyNonEmpty() ?
  BeginLifecycleUpdatesIfRenderingReady();
}

HTMLHeadElement* Document::head() const {
  Node* de = documentElement();
  if (!de)
    return nullptr;

  return Traversal<HTMLHeadElement>::FirstChild(*de);
}

Element* Document::ViewportDefiningElement() const {
  // If a BODY element sets non-visible overflow, it is to be propagated to the
  // viewport, as long as the following conditions are all met:
  // (1) The root element is HTML.
  // (2) It is the primary BODY element (we only assert for this, expecting
  //     callers to behave).
  // (3) The root element has visible overflow.
  // Otherwise it's the root element's properties that are to be propagated.
  Element* root_element = documentElement();
  Element* body_element = body();
  if (!root_element)
    return nullptr;
  const ComputedStyle* root_style = root_element->GetComputedStyle();
  if (!root_style || root_style->IsEnsuredInDisplayNone())
    return nullptr;
  if (body_element && root_style->IsOverflowVisible() &&
      IsA<HTMLHtmlElement>(root_element))
    return body_element;
  return root_element;
}

Document* Document::open(v8::Isolate* isolate,
                         const AtomicString& type,
                         const AtomicString& replace,
                         ExceptionState& exception_state) {
  if (replace == "replace") {
    CountUse(WebFeature::kDocumentOpenTwoArgsWithReplace);
  }
  open(EnteredDOMWindow(isolate)->document(), exception_state);
  return this;
}

DOMWindow* Document::open(v8::Isolate* isolate,
                          const String& url_string,
                          const AtomicString& name,
                          const AtomicString& features,
                          ExceptionState& exception_state) {
  if (!domWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return nullptr;
  }

  return domWindow()->open(isolate, url_string, name, features,
                           exception_state);
}

// https://html.spec.whatwg.org/C/dynamic-markup-insertion.html#dom-document-close
void Document::close(ExceptionState& exception_state) {
  if (ImportLoader()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Imported document doesn't support close().");
    return;
  }

  // If the Document object is an XML document, then throw an
  // "InvalidStateError" DOMException.
  if (!IsHTMLDocument()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Only HTML documents support close().");
    return;
  }

  // If the Document object's throw-on-dynamic-markup-insertion counter is
  // greater than zero, then throw an "InvalidStateError" DOMException.
  if (throw_on_dynamic_markup_insertion_count_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Custom Element constructor should not use close().");
    return;
  }

  if (!AllowedToUseDynamicMarkUpInsertion("close", exception_state))
    return;

  close();
}

// https://html.spec.whatwg.org/C/dynamic-markup-insertion.html#dom-document-close
void Document::close() {
  // If there is no script-created parser associated with the document, then
  // return.
  if (!GetScriptableDocumentParser() ||
      !GetScriptableDocumentParser()->WasCreatedByScript() ||
      !GetScriptableDocumentParser()->IsParsing())
    return;

  // Insert an explicit "EOF" character at the end of the parser's input
  // stream.
  parser_->Finish();

  // TODO(timothygu): We should follow the specification more closely.
  if (!parser_ || !parser_->IsParsing())
    SetReadyState(kComplete);
  CheckCompleted();
}

void Document::ImplicitClose() {
  DCHECK(!InStyleRecalc());

  load_event_progress_ = kLoadEventInProgress;

  // We have to clear the parser, in case someone document.write()s from the
  // onLoad event handler, as in Radar 3206524.
  DetachParser();

  // JS running below could remove the frame or destroy the LayoutView so we
  // call those two functions repeatedly and don't save them on the stack.

  // To align the HTML load event and the SVGLoad event for the outermost <svg>
  // element, fire it from here, instead of doing it from
  // SVGElement::finishedParsingChildren.
  if (SvgExtensions())
    AccessSVGExtensions().DispatchSVGLoadEventToOutermostSVGElements();

  if (domWindow())
    domWindow()->DocumentWasClosed();

  if (GetFrame()) {
    GetFrame()->Client()->DispatchDidHandleOnloadEvents();
    Loader()->GetApplicationCacheHost()->StopDeferringEvents();
  }

  if (!GetFrame()) {
    load_event_progress_ = kLoadEventCompleted;
    return;
  }

  if (GetFrame()->Loader().HasProvisionalNavigation() &&
      start_time_.Elapsed() < kCLayoutScheduleThreshold) {
    // Just bail out. Before or during the onload we were shifted to another
    // page.  The old i-Bench suite does this. When this happens don't bother
    // painting or laying out.
    load_event_progress_ = kLoadEventCompleted;
    return;
  }

  // We used to force a synchronous display and flush here.  This really isn't
  // necessary and can in fact be actively harmful if pages are loading at a
  // rate of > 60fps
  // (if your platform is syncing flushes and limiting them to 60fps).
  if (!LocalOwner() || (LocalOwner()->GetLayoutObject() &&
                        !LocalOwner()->GetLayoutObject()->NeedsLayout())) {
    UpdateStyleAndLayoutTree();

    // Always do a layout after loading if needed.
    if (View() && GetLayoutView() &&
        (!GetLayoutView()->FirstChild() || GetLayoutView()->NeedsLayout()))
      View()->UpdateLayout();
  }

  load_event_progress_ = kLoadEventCompleted;

  if (GetFrame() && GetLayoutView()) {
    if (AXObjectCache* cache = ExistingAXObjectCache()) {
      if (this == &AXObjectCacheOwner())
        cache->HandleLoadComplete(this);
      else
        cache->HandleLayoutComplete(this);
    }
  }

  if (SvgExtensions())
    AccessSVGExtensions().StartAnimations();
}

static bool AllDescendantsAreComplete(Document* document) {
  Frame* frame = document->GetFrame();
  if (!frame)
    return true;
  for (Frame* child = frame->Tree().FirstChild(); child;
       child = child->Tree().TraverseNext(frame)) {
    if (child->IsLoading())
      return false;
  }
  for (PortalContents* portal : DocumentPortals::From(*document).GetPortals()) {
    auto* frame = portal->GetFrame();
    if (frame && frame->IsLoading())
      return false;
  }
  return true;
}

bool Document::ShouldComplete() {
  return parsing_state_ == kFinishedParsing && HaveImportsLoaded() &&
         !fetcher_->BlockingRequestCount() && !IsDelayingLoadEvent() &&
         !javascript_url_task_handle_.IsActive() &&
         load_event_progress_ != kLoadEventInProgress &&
         AllDescendantsAreComplete(this) && !Fetcher()->IsInRequestResource();
}

void Document::Abort() {
  CancelParsing();
  CheckCompletedInternal();
}

void Document::CheckCompleted() {
  if (CheckCompletedInternal()) {
    frame_->Loader().DidFinishNavigation(
        FrameLoader::NavigationFinishState::kSuccess);
  }
}

bool Document::CheckCompletedInternal() {
  if (!ShouldComplete())
    return false;

  if (frame_ && !UnloadStarted()) {
    frame_->Client()->RunScriptsAtDocumentIdle();

    // Injected scripts may have disconnected this frame.
    if (!frame_)
      return false;

    // Check again, because runScriptsAtDocumentIdle() may have delayed the load
    // event.
    if (!ShouldComplete())
      return false;
  }

  // OK, completed. Fire load completion events as needed.
  SetReadyState(kComplete);
  if (LoadEventStillNeeded())
    ImplicitClose();

  // The readystatechanged or load event may have disconnected this frame.
  if (!frame_ || !frame_->IsAttached())
    return false;
  http_refresh_scheduler_->MaybeStartTimer();
  View()->HandleLoadCompleted();
  // The document itself is complete, but if a child frame was restarted due to
  // an event, this document is still considered to be in progress.
  if (!AllDescendantsAreComplete(this))
    return false;

  // No need to repeat if we've already notified this load as finished.
  if (!Loader()->SentDidFinishLoad()) {
    if (frame_->IsMainFrame())
      GetViewportData().GetViewportDescription().ReportMobilePageStats(frame_);
    Loader()->SetSentDidFinishLoad();
    frame_->Client()->DispatchDidFinishLoad();
    if (!frame_)
      return false;

    // Send the source ID of the document to the browser.
    if (frame_->Client()->GetRemoteNavigationAssociatedInterfaces()) {
      mojo::AssociatedRemote<mojom::blink::UkmSourceIdFrameHost> ukm_binding;
      frame_->Client()->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
          &ukm_binding);
      DCHECK(ukm_binding.is_bound());
      ukm_binding->SetDocumentSourceId(ukm_source_id_);
    }

    frame_->GetFrameScheduler()->RegisterStickyFeature(
        SchedulingPolicy::Feature::kDocumentLoaded,
        {SchedulingPolicy::RecordMetricsForBackForwardCache()});

    AnchorElementMetrics::NotifyOnLoad(*this);

    // If this is a document associated with a resource loading hints based
    // preview, then record the resource loading hints UKM now that the load is
    // finished.
    PreviewsResourceLoadingHints* hints =
        Loader()->GetPreviewsResourceLoadingHints();
    if (hints) {
      hints->RecordUKM(UkmRecorder());
    }
  }

  return true;
}

bool Document::DispatchBeforeUnloadEvent(ChromeClient* chrome_client,
                                         bool is_reload,
                                         bool& did_allow_navigation) {
  if (!dom_window_)
    return true;

  if (!body())
    return true;

  if (ProcessingBeforeUnload())
    return false;

  auto& before_unload_event = *MakeGarbageCollected<BeforeUnloadEvent>();
  before_unload_event.initEvent(event_type_names::kBeforeunload, false, true);
  load_event_progress_ = kBeforeUnloadEventInProgress;
  const base::TimeTicks beforeunload_event_start = base::TimeTicks::Now();
  dom_window_->DispatchEvent(before_unload_event, this);
  const base::TimeTicks beforeunload_event_end = base::TimeTicks::Now();
  load_event_progress_ = kBeforeUnloadEventCompleted;
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, beforeunload_histogram,
      ("DocumentEventTiming.BeforeUnloadDuration", 0, 10000000, 50));
  beforeunload_histogram.CountMicroseconds(beforeunload_event_end -
                                           beforeunload_event_start);
  if (!before_unload_event.defaultPrevented())
    DefaultEventHandler(before_unload_event);

  enum BeforeUnloadDialogHistogramEnum {
    kNoDialogNoText,
    kNoDialogNoUserGesture,
    kNoDialogMultipleConfirmationForNavigation,
    kShowDialog,
    kNoDialogAutoCancelTrue,
    kDialogEnumMax
  };
  DEFINE_STATIC_LOCAL(EnumerationHistogram, beforeunload_dialog_histogram,
                      ("Document.BeforeUnloadDialog", kDialogEnumMax));
  if (before_unload_event.returnValue().IsNull()) {
    beforeunload_dialog_histogram.Count(kNoDialogNoText);
  }
  if (!GetFrame() || before_unload_event.returnValue().IsNull())
    return true;

  if (!GetFrame()->HasBeenActivated()) {
    beforeunload_dialog_histogram.Count(kNoDialogNoUserGesture);
    String message =
        "Blocked attempt to show a 'beforeunload' confirmation panel for a "
        "frame that never had a user gesture since its load. "
        "https://www.chromestatus.com/feature/5082396709879808";
    Intervention::GenerateReport(frame_, "BeforeUnloadNoGesture", message);
    return true;
  }

  if (did_allow_navigation) {
    beforeunload_dialog_histogram.Count(
        kNoDialogMultipleConfirmationForNavigation);
    String message =
        "Blocked attempt to show multiple 'beforeunload' confirmation panels "
        "for a single navigation.";
    Intervention::GenerateReport(frame_, "BeforeUnloadMultiple", message);
    return true;
  }

  // If |chrome_client| is null simply indicate that the navigation should
  // not proceed.
  if (!chrome_client) {
    beforeunload_dialog_histogram.Count(kNoDialogAutoCancelTrue);
    did_allow_navigation = false;
    return false;
  }

  String text = before_unload_event.returnValue();
  beforeunload_dialog_histogram.Count(
      BeforeUnloadDialogHistogramEnum::kShowDialog);
  const base::TimeTicks beforeunload_confirmpanel_start =
      base::TimeTicks::Now();
  did_allow_navigation =
      chrome_client->OpenBeforeUnloadConfirmPanel(text, frame_, is_reload);
  const base::TimeTicks beforeunload_confirmpanel_end = base::TimeTicks::Now();
  if (did_allow_navigation) {
    // Only record when a navigation occurs, since we want to understand
    // the impact of the before unload dialog on overall input to navigation.
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "DocumentEventTiming.BeforeUnloadDialogDuration.ByNavigation",
        beforeunload_confirmpanel_end - beforeunload_confirmpanel_start);
    return true;
  }

  return false;
}

void Document::DispatchUnloadEvents(
    SecurityOrigin* committing_origin,
    base::Optional<Document::UnloadEventTiming>* unload_timing) {
  PluginScriptForbiddenScope forbid_plugin_destructor_scripting;
  if (parser_)
    parser_->StopParsing();

  if (load_event_progress_ == kLoadEventNotRun)
    return;

  if (load_event_progress_ <= kUnloadEventInProgress) {
    Element* current_focused_element = FocusedElement();
    if (auto* input = ToHTMLInputElementOrNull(current_focused_element))
      input->EndEditing();
    if (load_event_progress_ < kPageHideInProgress) {
      load_event_progress_ = kPageHideInProgress;
      if (LocalDOMWindow* window = domWindow()) {
        const base::TimeTicks pagehide_event_start = base::TimeTicks::Now();
        window->DispatchEvent(
            *PageTransitionEvent::Create(event_type_names::kPagehide, false),
            this);
        const base::TimeTicks pagehide_event_end = base::TimeTicks::Now();
        DEFINE_STATIC_LOCAL(
            CustomCountHistogram, pagehide_histogram,
            ("DocumentEventTiming.PageHideDuration", 0, 10000000, 50));
        pagehide_histogram.CountMicroseconds(pagehide_event_end -
                                             pagehide_event_start);
      }
      if (!frame_)
        return;

      // This must be queried before |load_event_progress_| is changed to
      // kUnloadVisibilityChangeInProgress because that would change the result.
      bool page_visible = IsPageVisible();
      load_event_progress_ = kUnloadVisibilityChangeInProgress;
      if (page_visible) {
        // Dispatch visibilitychange event, but don't bother doing
        // other notifications as we're about to be unloaded.
        const base::TimeTicks pagevisibility_hidden_event_start =
            base::TimeTicks::Now();
        DispatchEvent(
            *Event::CreateBubble(event_type_names::kVisibilitychange));
        const base::TimeTicks pagevisibility_hidden_event_end =
            base::TimeTicks::Now();
        DEFINE_STATIC_LOCAL(CustomCountHistogram, pagevisibility_histogram,
                            ("DocumentEventTiming.PageVibilityHiddenDuration",
                             0, 10000000, 50));
        pagevisibility_histogram.CountMicroseconds(
            pagevisibility_hidden_event_end -
            pagevisibility_hidden_event_start);
        DispatchEvent(
            *Event::CreateBubble(event_type_names::kWebkitvisibilitychange));
      }
      if (!frame_)
        return;

      frame_->Loader().SaveScrollAnchor();

      load_event_progress_ = kUnloadEventInProgress;
      Event& unload_event = *Event::Create(event_type_names::kUnload);
      const base::TimeTicks unload_event_start = base::TimeTicks::Now();
      frame_->DomWindow()->DispatchEvent(unload_event, this);
      const base::TimeTicks unload_event_end = base::TimeTicks::Now();

      if (unload_timing) {
        // Record unload event timing when navigating cross-document.
        DEFINE_STATIC_LOCAL(
            CustomCountHistogram, unload_histogram,
            ("DocumentEventTiming.UnloadDuration", 0, 10000000, 50));
        unload_histogram.CountMicroseconds(unload_event_end -
                                           unload_event_start);

        // Fill in the unload timing if the new document origin has access to
        // them.
        if (committing_origin->CanRequest(Url())) {
          auto& timing = unload_timing->emplace();
          timing.unload_event_start = unload_event_start;
          timing.unload_event_end = unload_event_end;
        }
      }
    }
    load_event_progress_ = kUnloadEventHandled;
  }
}

void Document::DispatchFreezeEvent() {
  const base::TimeTicks freeze_event_start = base::TimeTicks::Now();
  SetFreezingInProgress(true);
  DispatchEvent(*Event::Create(event_type_names::kFreeze));
  SetFreezingInProgress(false);
  const base::TimeTicks freeze_event_end = base::TimeTicks::Now();
  DEFINE_STATIC_LOCAL(CustomCountHistogram, freeze_histogram,
                      ("DocumentEventTiming.FreezeDuration", 0, 10000000, 50));
  freeze_histogram.CountMicroseconds(freeze_event_end - freeze_event_start);
  UseCounter::Count(*this, WebFeature::kPageLifeCycleFreeze);
}

Document::PageDismissalType Document::PageDismissalEventBeingDispatched()
    const {
  switch (load_event_progress_) {
    case kBeforeUnloadEventInProgress:
      return kBeforeUnloadDismissal;
    case kPageHideInProgress:
      return kPageHideDismissal;
    case kUnloadVisibilityChangeInProgress:
      return kUnloadVisibilityChangeDismissal;
    case kUnloadEventInProgress:
      return kUnloadDismissal;

    case kLoadEventNotRun:
    case kLoadEventInProgress:
    case kLoadEventCompleted:
    case kBeforeUnloadEventCompleted:
    case kUnloadEventHandled:
      return kNoDismissal;
  }
  NOTREACHED();
  return kNoDismissal;
}

void Document::SetParsingState(ParsingState parsing_state) {
  ParsingState previous_state = parsing_state_;
  parsing_state_ = parsing_state;

  if (Parsing() && !element_data_cache_)
    element_data_cache_ = MakeGarbageCollected<ElementDataCache>();
  if (previous_state != kFinishedParsing &&
      parsing_state_ == kFinishedParsing) {
    if (form_controller_ && form_controller_->HasControlStates())
      form_controller_->ScheduleRestore();
  }
}

bool Document::ShouldScheduleLayout() const {
  // This function will only be called when LocalFrameView thinks a layout is
  // needed. This enforces a couple extra rules.
  //
  //    (a) Only schedule a layout once the stylesheets are loaded.
  //    (b) Only schedule layout once we have a body element.
  if (!IsActive())
    return false;

  if (HaveRenderBlockingResourcesLoaded() && body())
    return true;

  if (documentElement() && !IsA<HTMLHtmlElement>(documentElement()))
    return true;

  return false;
}

void Document::write(const String& text,
                     Document* entered_document,
                     ExceptionState& exception_state) {
  if (ImportLoader()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Imported document doesn't support write().");
    return;
  }

  if (!IsHTMLDocument()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Only HTML documents support write().");
    return;
  }

  if (throw_on_dynamic_markup_insertion_count_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Custom Element constructor should not use write().");
    return;
  }

  if (entered_document && !GetSecurityOrigin()->IsSameSchemeHostPort(
                              entered_document->GetSecurityOrigin())) {
    exception_state.ThrowSecurityError(
        "Can only call write() on same-origin documents.");
    return;
  }

  if (ignore_opens_and_writes_for_abort_)
    return;

  NestingLevelIncrementer nesting_level_incrementer(write_recursion_depth_);

  write_recursion_is_too_deep_ =
      (write_recursion_depth_ > 1) && write_recursion_is_too_deep_;
  write_recursion_is_too_deep_ =
      (write_recursion_depth_ > kCMaxWriteRecursionDepth) ||
      write_recursion_is_too_deep_;

  if (write_recursion_is_too_deep_)
    return;

  bool has_insertion_point = parser_ && parser_->HasInsertionPoint();

  if (!has_insertion_point) {
    if (ignore_destructive_write_count_) {
      AddConsoleMessage(ConsoleMessage::Create(
          mojom::ConsoleMessageSource::kJavaScript,
          mojom::ConsoleMessageLevel::kWarning,
          ExceptionMessages::FailedToExecute(
              "write", "Document",
              "It isn't possible to write into a document "
              "from an asynchronously-loaded external "
              "script unless it is explicitly opened.")));
      return;
    }
    if (ignore_opens_during_unload_count_)
      return;

    open(entered_document, ASSERT_NO_EXCEPTION);
  }

  DCHECK(parser_);
  PerformanceMonitor::ReportGenericViolation(
      this, PerformanceMonitor::kDiscouragedAPIUse,
      "Avoid using document.write(). "
      "https://developers.google.com/web/updates/2016/08/"
      "removing-document-write",
      base::TimeDelta(), nullptr);
  probe::BreakableLocation(this, "Document.write");
  parser_->insert(text);
}

void Document::writeln(const String& text,
                       Document* entered_document,
                       ExceptionState& exception_state) {
  write(text, entered_document, exception_state);
  if (exception_state.HadException())
    return;
  write("\n", entered_document);
}

void Document::write(v8::Isolate* isolate,
                     const Vector<String>& text,
                     ExceptionState& exception_state) {
  if (!AllowedToUseDynamicMarkUpInsertion("write", exception_state))
    return;

  StringBuilder builder;
  for (const String& string : text)
    builder.Append(string);
  String string =
      GetStringFromTrustedHTML(builder.ToString(), this, exception_state);
  if (exception_state.HadException())
    return;

  write(string, EnteredDOMWindow(isolate)->document(), exception_state);
}

void Document::writeln(v8::Isolate* isolate,
                       const Vector<String>& text,
                       ExceptionState& exception_state) {
  if (!AllowedToUseDynamicMarkUpInsertion("writeln", exception_state))
    return;

  StringBuilder builder;
  for (const String& string : text)
    builder.Append(string);
  String string =
      GetStringFromTrustedHTML(builder.ToString(), this, exception_state);
  if (exception_state.HadException())
    return;

  writeln(string, EnteredDOMWindow(isolate)->document(), exception_state);
}

bool Document::IsTrustedTypesEnabledForDoc() const {
  return ExecutionContext::RequireTrustedTypes();
}

void Document::write(v8::Isolate* isolate,
                     TrustedHTML* text,
                     ExceptionState& exception_state) {
  DCHECK(RuntimeEnabledFeatures::TrustedDOMTypesEnabled(this));
  write(text->toString(), EnteredDOMWindow(isolate)->document(),
        exception_state);
}

void Document::writeln(v8::Isolate* isolate,
                       TrustedHTML* text,
                       ExceptionState& exception_state) {
  DCHECK(RuntimeEnabledFeatures::TrustedDOMTypesEnabled(this));
  writeln(text->toString(), EnteredDOMWindow(isolate)->document(),
          exception_state);
}

DOMTimerCoordinator* Document::Timers() {
  return &timers_;
}

EventTarget* Document::ErrorEventTarget() {
  return domWindow();
}

void Document::ExceptionThrown(ErrorEvent* event) {
  MainThreadDebugger::Instance()->ExceptionThrown(this, event);
}

KURL Document::urlForBinding() const {
  if (!Url().IsNull()) {
    return Url();
  }
  return BlankURL();
}

void Document::SetURL(const KURL& url) {
  KURL new_url = url.IsEmpty() ? BlankURL() : url;
  if (new_url == url_)
    return;

  // Count non-targetText occurrences of :~: in the url fragment to make sure
  // the delimiter is web-compatible. This can be removed once the feature
  // ships.
  wtf_size_t delim_pos = new_url.FragmentIdentifier().Find(":~:");
  if (delim_pos != kNotFound) {
    const wtf_size_t one_past_delim = delim_pos + 3;
    if (new_url.FragmentIdentifier().Find(kTextFragmentIdentifierPrefix,
                                          one_past_delim) != one_past_delim) {
      // We can't use count here because the DocumentLoader hasn't yet been
      // created. It'll be use counted with other delimiters in
      // FragmentAnchor::TryCreate.
      use_count_fragment_directive_ = true;
    }
  }

  // If text fragment identifiers are enabled, we strip the fragment directive
  // from the URL fragment.
  // E.g. "#id:~:text=a" --> "#id"
  if (RuntimeEnabledFeatures::TextFragmentIdentifiersEnabled(this)) {
    String fragment = new_url.FragmentIdentifier();
    wtf_size_t start_pos = fragment.Find(kFragmentDirectivePrefix);
    if (start_pos != kNotFound) {
      fragment_directive_ =
          fragment.Substring(start_pos + kFragmentDirectivePrefixStringLength);

      if (start_pos == 0)
        new_url.RemoveFragmentIdentifier();
      else
        new_url.SetFragmentIdentifier(fragment.Substring(0, start_pos));
    }
  }

  url_ = new_url;
  access_entry_from_url_ = nullptr;
  UpdateBaseURL();
  GetContextFeatures().UrlDidChange(this);

  // TODO(crbug/795354): Move handling of URL recording out of the renderer.
  // URL must only be recorded from the main frame.
  if (ukm_recorder_ && IsInMainFrame())
    ukm_recorder_->UpdateSourceURL(ukm_source_id_, url_);

  if (frame_) {
    if (FrameScheduler* frame_scheduler = frame_->GetFrameScheduler())
      frame_scheduler->TraceUrlChange(url_.GetString());
  }
}

KURL Document::ValidBaseElementURL() const {
  if (base_element_url_.IsValid())
    return base_element_url_;

  return KURL();
}

void Document::UpdateBaseURL() {
  KURL old_base_url = base_url_;
  // DOM 3 Core: When the Document supports the feature "HTML" [DOM Level 2
  // HTML], the base URI is computed using first the value of the href attribute
  // of the HTML BASE element if any, and the value of the documentURI attribute
  // from the Document interface otherwise (which we store, preparsed, in
  // |url_|).
  if (!base_element_url_.IsEmpty())
    base_url_ = base_element_url_;
  else if (!base_url_override_.IsEmpty())
    base_url_ = base_url_override_;
  else
    base_url_ = FallbackBaseURL();

  GetSelectorQueryCache().Invalidate();

  if (!base_url_.IsValid())
    base_url_ = KURL();

  if (elem_sheet_) {
    // Element sheet is silly. It never contains anything.
    DCHECK(!elem_sheet_->Contents()->RuleCount());
    elem_sheet_ = CSSStyleSheet::CreateInline(*this, base_url_);
  }

  if (!EqualIgnoringFragmentIdentifier(old_base_url, base_url_)) {
    // Base URL change changes any relative visited links.
    // FIXME: There are other URLs in the tree that would need to be
    // re-evaluated on dynamic base URL change. Style should be invalidated too.
    for (HTMLAnchorElement& anchor :
         Traversal<HTMLAnchorElement>::StartsAfter(*this))
      anchor.InvalidateCachedVisitedLinkHash();
  }
}

KURL Document::FallbackBaseURL() const {
  if (IsSrcdocDocument()) {
    // TODO(tkent): Referring to ParentDocument() is not correct.  See
    // crbug.com/751329.
    if (Document* parent = ParentDocument())
      return parent->BaseURL();
  } else if (urlForBinding().IsAboutBlankURL()) {
    if (context_document_)
      return context_document_->BaseURL();
    // TODO(tkent): Referring to ParentDocument() is not correct.  See
    // crbug.com/751329.
    if (Document* parent = ParentDocument())
      return parent->BaseURL();
  }
  return urlForBinding();
}

const KURL& Document::BaseURL() const {
  if (!base_url_.IsNull())
    return base_url_;
  return BlankURL();
}

void Document::SetBaseURLOverride(const KURL& url) {
  base_url_override_ = url;
  UpdateBaseURL();
}

void Document::ProcessBaseElement() {
  UseCounter::Count(*this, WebFeature::kBaseElement);

  // Find the first href attribute in a base element and the first target
  // attribute in a base element.
  const AtomicString* href = nullptr;
  const AtomicString* target = nullptr;
  for (HTMLBaseElement* base = Traversal<HTMLBaseElement>::FirstWithin(*this);
       base && (!href || !target);
       base = Traversal<HTMLBaseElement>::Next(*base)) {
    if (!href) {
      const AtomicString& value = base->FastGetAttribute(html_names::kHrefAttr);
      if (!value.IsNull())
        href = &value;
    }
    if (!target) {
      const AtomicString& value =
          base->FastGetAttribute(html_names::kTargetAttr);
      if (!value.IsNull())
        target = &value;
    }
    if (GetContentSecurityPolicy()->IsActive()) {
      UseCounter::Count(*this,
                        WebFeature::kContentSecurityPolicyWithBaseElement);
    }
  }

  // FIXME: Since this doesn't share code with completeURL it may not handle
  // encodings correctly.
  KURL base_element_url;
  if (href) {
    String stripped_href = StripLeadingAndTrailingHTMLSpaces(*href);
    if (!stripped_href.IsEmpty())
      base_element_url = KURL(FallbackBaseURL(), stripped_href);
  }

  if (!base_element_url.IsEmpty()) {
    if (base_element_url.ProtocolIsData() ||
        base_element_url.ProtocolIsJavaScript()) {
      UseCounter::Count(*this, WebFeature::kBaseWithDataHref);
      AddConsoleMessage(ConsoleMessage::Create(
          mojom::ConsoleMessageSource::kSecurity,
          mojom::ConsoleMessageLevel::kError,
          "'" + base_element_url.Protocol() +
              "' URLs may not be used as base URLs for a document."));
    }
    if (!GetSecurityOrigin()->CanRequest(base_element_url))
      UseCounter::Count(*this, WebFeature::kBaseWithCrossOriginHref);
  }

  if (base_element_url != base_element_url_ &&
      !base_element_url.ProtocolIsData() &&
      !base_element_url.ProtocolIsJavaScript() &&
      GetContentSecurityPolicy()->AllowBaseURI(base_element_url)) {
    base_element_url_ = base_element_url;
    UpdateBaseURL();
  }

  if (target) {
    if (target->Contains('\n') || target->Contains('\r'))
      UseCounter::Count(*this, WebFeature::kBaseWithNewlinesInTarget);
    if (target->Contains('<'))
      UseCounter::Count(*this, WebFeature::kBaseWithOpenBracketInTarget);
    base_target_ = *target;
  } else {
    base_target_ = g_null_atom;
  }
}

String Document::UserAgent() const {
  return GetFrame() ? GetFrame()->Loader().UserAgent() : String();
}

void Document::DisableEval(const String& error_message) {
  if (!GetFrame())
    return;

  GetFrame()->GetScriptController().DisableEval(error_message);
}

void Document::DidLoadAllImports() {
  if (!HaveScriptBlockingStylesheetsLoaded())
    return;
  DidLoadAllScriptBlockingResources();
}

void Document::DidAddPendingParserBlockingStylesheet() {
  if (ScriptableDocumentParser* parser = GetScriptableDocumentParser())
    parser->DidAddPendingParserBlockingStylesheet();
}

void Document::DidRemoveAllPendingStylesheets() {
  // Only imports on master documents can trigger rendering.
  if (HTMLImportLoader* import = ImportLoader())
    import->DidRemoveAllPendingStylesheets();
  if (!HaveImportsLoaded())
    return;
  DidLoadAllScriptBlockingResources();
}

void Document::DidLoadAllPendingParserBlockingStylesheets() {
  if (ScriptableDocumentParser* parser = GetScriptableDocumentParser())
    parser->DidLoadAllPendingParserBlockingStylesheets();
}

void Document::DidLoadAllScriptBlockingResources() {
  // Use wrapWeakPersistent because the task should not keep this Document alive
  // just for executing scripts.
  execute_scripts_waiting_for_resources_task_handle_ = PostCancellableTask(
      *GetTaskRunner(TaskType::kNetworking), FROM_HERE,
      WTF::Bind(&Document::ExecuteScriptsWaitingForResources,
                WrapWeakPersistent(this)));

  if (IsHTMLDocument() && body()) {
    // For HTML if we have no more stylesheets to load and we're past the body
    // tag, we should have something to paint so resume.
    BeginLifecycleUpdatesIfRenderingReady();
  } else if (!IsHTMLDocument() && documentElement()) {
    // For non-HTML there is no body so resume as soon as the sheets are loaded.
    BeginLifecycleUpdatesIfRenderingReady();
  }
}

void Document::ExecuteScriptsWaitingForResources() {
  if (!IsScriptExecutionReady())
    return;
  if (ScriptableDocumentParser* parser = GetScriptableDocumentParser())
    parser->ExecuteScriptsWaitingForResources();
}

CSSStyleSheet& Document::ElementSheet() {
  if (!elem_sheet_)
    elem_sheet_ = CSSStyleSheet::CreateInline(*this, base_url_);
  return *elem_sheet_;
}

void Document::MaybeHandleHttpRefresh(const String& content,
                                      HttpRefreshType http_refresh_type) {
  if (is_view_source_ || !frame_)
    return;

  base::TimeDelta delay;
  String refresh_url_string;
  if (!ParseHTTPRefresh(content,
                        http_refresh_type == kHttpRefreshFromMetaTag
                            ? IsHTMLSpace<UChar>
                            : nullptr,
                        delay, refresh_url_string))
    return;
  KURL refresh_url =
      refresh_url_string.IsEmpty() ? Url() : CompleteURL(refresh_url_string);

  if (refresh_url.ProtocolIsJavaScript()) {
    String message =
        "Refused to refresh " + url_.ElidedString() + " to a javascript: URL";
    AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kSecurity,
                               mojom::ConsoleMessageLevel::kError, message));
    return;
  }

  if (http_refresh_type == kHttpRefreshFromMetaTag &&
      IsSandboxed(WebSandboxFlags::kAutomaticFeatures)) {
    String message =
        "Refused to execute the redirect specified via '<meta "
        "http-equiv='refresh' content='...'>'. The document is sandboxed, and "
        "the 'allow-scripts' keyword is not set.";
    AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kSecurity,
                               mojom::ConsoleMessageLevel::kError, message));
    return;
  }
  if (http_refresh_type == kHttpRefreshFromHeader) {
    UseCounter::Count(this, WebFeature::kRefreshHeader);
  }
  http_refresh_scheduler_->Schedule(delay, refresh_url, http_refresh_type);
}

bool Document::IsHttpRefreshScheduledWithin(base::TimeDelta interval) {
  return http_refresh_scheduler_->IsScheduledWithin(interval);
}

// https://w3c.github.io/webappsec-referrer-policy/#determine-requests-referrer
String Document::OutgoingReferrer() const {
  // Step 3.1: "If environment's global object is a Window object, then"

  // Step 3.1.1: "Let document be the associated Document of environment's
  // global object."
  const Document* referrer_document = this;

  // Step 3.1.2: "If document's origin is an opaque origin, return no referrer."
  if (GetSecurityOrigin()->IsOpaque())
    return String();

  // Step 3.1.3: "While document is an iframe srcdoc document, let document be
  // document's browsing context's browsing context container's node document."
  if (LocalFrame* frame = frame_) {
    while (frame->GetDocument()->IsSrcdocDocument()) {
      // Srcdoc documents must be local within the containing frame.
      frame = To<LocalFrame>(frame->Tree().Parent());
      // Srcdoc documents cannot be top-level documents, by definition,
      // because they need to be contained in iframes with the srcdoc.
      DCHECK(frame);
    }
    referrer_document = frame->GetDocument();
  }

  // Step: 3.1.4: "Let referrerSource be document's URL."
  return referrer_document->url_.StrippedForUseAsReferrer();
}

network::mojom::ReferrerPolicy Document::GetReferrerPolicy() const {
  network::mojom::ReferrerPolicy policy = ExecutionContext::GetReferrerPolicy();
  // For srcdoc documents without their own policy, walk up the frame
  // tree to find the document that is either not a srcdoc or doesn't
  // have its own policy. This algorithm is defined in
  // https://html.spec.whatwg.org/C/#set-up-a-window-environment-settings-object.
  if (!frame_ || policy != network::mojom::ReferrerPolicy::kDefault ||
      !IsSrcdocDocument()) {
    return policy;
  }
  LocalFrame* frame = To<LocalFrame>(frame_->Tree().Parent());
  return frame->GetDocument()->GetReferrerPolicy();
}

MouseEventWithHitTestResults Document::PerformMouseEventHitTest(
    const HitTestRequest& request,
    const PhysicalOffset& document_point,
    const WebMouseEvent& event) {
  DCHECK(!GetLayoutView() || GetLayoutView()->IsLayoutView());

  // LayoutView::hitTest causes a layout, and we don't want to hit that until
  // the first layout because until then, there is nothing shown on the screen -
  // the user can't have intentionally clicked on something belonging to this
  // page.  Furthermore, mousemove events before the first layout should not
  // lead to a premature layout() happening, which could show a flash of white.
  // See also the similar code in EventHandler::hitTestResultAtPoint.
  if (!GetLayoutView() || !View() || !View()->DidFirstLayout()) {
    HitTestLocation location((PhysicalOffset()));
    return MouseEventWithHitTestResults(event, location,
                                        HitTestResult(request, location));
  }

  HitTestLocation location(document_point);
  HitTestResult result(request, location);
  GetLayoutView()->HitTest(location, result);

  if (!request.ReadOnly()) {
    UpdateHoverActiveState(request.Active(), !request.Move(),
                           result.InnerElement());
  }

  if (auto* canvas = ToHTMLCanvasElementOrNull(result.InnerNode())) {
    HitTestCanvasResult* hit_test_canvas_result =
        canvas->GetControlAndIdIfHitRegionExists(
            result.PointInInnerNodeFrame());
    if (hit_test_canvas_result->GetControl()) {
      result.SetInnerNode(hit_test_canvas_result->GetControl());
    }
    result.SetCanvasRegionId(hit_test_canvas_result->GetId());
  }

  return MouseEventWithHitTestResults(event, location, result);
}

// DOM Section 1.1.1
bool Document::ChildTypeAllowed(NodeType type) const {
  switch (type) {
    case kAttributeNode:
    case kCdataSectionNode:
    case kDocumentFragmentNode:
    case kDocumentNode:
    case kTextNode:
      return false;
    case kCommentNode:
    case kProcessingInstructionNode:
      return true;
    case kDocumentTypeNode:
    case kElementNode:
      // Documents may contain no more than one of each of these.
      // (One Element and one DocumentType.)
      for (Node& c : NodeTraversal::ChildrenOf(*this)) {
        if (c.getNodeType() == type)
          return false;
      }
      return true;
  }
  return false;
}

// This is an implementation of step 6 of
// https://dom.spec.whatwg.org/#concept-node-ensure-pre-insertion-validity
// and https://dom.spec.whatwg.org/#concept-node-replace .
//
// 6. If parent is a document, and any of the statements below, switched on
// node, are true, throw a HierarchyRequestError.
//  -> DocumentFragment node
//     If node has more than one element child or has a Text node child.
//     Otherwise, if node has one element child and either parent has an element
//     child, child is a doctype, or child is not null and a doctype is
//     following child.
//  -> element
//     parent has an element child, child is a doctype, or child is not null and
//     a doctype is following child.
//  -> doctype
//     parent has a doctype child, child is non-null and an element is preceding
//     child, or child is null and parent has an element child.
//
// 6. If parent is a document, and any of the statements below, switched on
// node, are true, throw a HierarchyRequestError.
//  -> DocumentFragment node
//     If node has more than one element child or has a Text node child.
//     Otherwise, if node has one element child and either parent has an element
//     child that is not child or a doctype is following child.
//  -> element
//     parent has an element child that is not child or a doctype is following
//     child.
//  -> doctype
//     parent has a doctype child that is not child, or an element is preceding
//     child.
bool Document::CanAcceptChild(const Node& new_child,
                              const Node* next,
                              const Node* old_child,
                              ExceptionState& exception_state) const {
  DCHECK(!(next && old_child));
  if (old_child && old_child->getNodeType() == new_child.getNodeType())
    return true;

  int num_doctypes = 0;
  int num_elements = 0;
  bool has_doctype_after_reference_node = false;
  bool has_element_after_reference_node = false;

  // First, check how many doctypes and elements we have, not counting
  // the child we're about to remove.
  bool saw_reference_node = false;
  for (Node& child : NodeTraversal::ChildrenOf(*this)) {
    if (old_child && *old_child == child) {
      saw_reference_node = true;
      continue;
    }
    if (&child == next)
      saw_reference_node = true;

    switch (child.getNodeType()) {
      case kDocumentTypeNode:
        num_doctypes++;
        has_doctype_after_reference_node = saw_reference_node;
        break;
      case kElementNode:
        num_elements++;
        has_element_after_reference_node = saw_reference_node;
        break;
      default:
        break;
    }
  }

  // Then, see how many doctypes and elements might be added by the new child.
  if (auto* new_child_fragment = DynamicTo<DocumentFragment>(new_child)) {
    for (Node& child : NodeTraversal::ChildrenOf(*new_child_fragment)) {
      switch (child.getNodeType()) {
        case kAttributeNode:
        case kCdataSectionNode:
        case kDocumentFragmentNode:
        case kDocumentNode:
        case kTextNode:
          exception_state.ThrowDOMException(
              DOMExceptionCode::kHierarchyRequestError,
              "Nodes of type '" + new_child.nodeName() +
                  "' may not be inserted inside nodes of type '#document'.");
          return false;
        case kCommentNode:
        case kProcessingInstructionNode:
          break;
        case kDocumentTypeNode:
          num_doctypes++;
          break;
        case kElementNode:
          num_elements++;
          if (has_doctype_after_reference_node) {
            exception_state.ThrowDOMException(
                DOMExceptionCode::kHierarchyRequestError,
                "Can't insert an element before a doctype.");
            return false;
          }
          break;
      }
    }
  } else {
    switch (new_child.getNodeType()) {
      case kAttributeNode:
      case kCdataSectionNode:
      case kDocumentFragmentNode:
      case kDocumentNode:
      case kTextNode:
        exception_state.ThrowDOMException(
            DOMExceptionCode::kHierarchyRequestError,
            "Nodes of type '" + new_child.nodeName() +
                "' may not be inserted inside nodes of type '#document'.");
        return false;
      case kCommentNode:
      case kProcessingInstructionNode:
        return true;
      case kDocumentTypeNode:
        num_doctypes++;
        if (num_elements > 0 && !has_element_after_reference_node) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kHierarchyRequestError,
              "Can't insert a doctype before the root element.");
          return false;
        }
        break;
      case kElementNode:
        num_elements++;
        if (has_doctype_after_reference_node) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kHierarchyRequestError,
              "Can't insert an element before a doctype.");
          return false;
        }
        break;
    }
  }

  if (num_elements > 1 || num_doctypes > 1) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        String::Format("Only one %s on document allowed.",
                       num_elements > 1 ? "element" : "doctype"));
    return false;
  }

  return true;
}

Node* Document::Clone(Document& factory, CloneChildrenFlag flag) const {
  DCHECK_EQ(this, &factory)
      << "Document::Clone() doesn't support importNode mode.";
  Document* clone = CloneDocumentWithoutChildren();
  clone->CloneDataFromDocument(*this);
  if (flag == CloneChildrenFlag::kClone)
    clone->CloneChildNodesFrom(*this);
  return clone;
}

Document* Document::CloneDocumentWithoutChildren() const {
  DocumentInit init =
      DocumentInit::Create()
          .WithContextDocument(ContextDocument())
          .WithURL(Url())
          .WithOriginToCommit(GetSecurityOrigin()->IsolatedCopy());
  if (IsXMLDocument()) {
    if (IsXHTMLDocument())
      return XMLDocument::CreateXHTML(
          init.WithRegistrationContext(RegistrationContext()));
    return MakeGarbageCollected<XMLDocument>(init);
  }
  return MakeGarbageCollected<Document>(init);
}

void Document::CloneDataFromDocument(const Document& other) {
  SetCompatibilityMode(other.GetCompatibilityMode());
  SetEncodingData(other.encoding_data_);
  SetContextFeatures(other.GetContextFeatures());
  SetMimeType(other.contentType());
}

StyleSheetList& Document::StyleSheets() {
  if (!style_sheet_list_)
    style_sheet_list_ = MakeGarbageCollected<StyleSheetList>(this);
  return *style_sheet_list_;
}

void Document::EvaluateMediaQueryListIfNeeded() {
  if (!evaluate_media_queries_on_style_recalc_)
    return;
  EvaluateMediaQueryList();
  evaluate_media_queries_on_style_recalc_ = false;
}

void Document::EvaluateMediaQueryList() {
  if (media_query_matcher_)
    media_query_matcher_->MediaFeaturesChanged();
}

void Document::SetResizedForViewportUnits() {
  if (media_query_matcher_)
    media_query_matcher_->ViewportChanged();
  if (!HasViewportUnits())
    return;
  EnsureStyleResolver().SetResizedForViewportUnits();
  SetNeedsStyleRecalcForViewportUnits();
}

void Document::ClearResizedForViewportUnits() {
  EnsureStyleResolver().ClearResizedForViewportUnits();
}

void Document::SetHoverElement(Element* new_hover_element) {
  hover_element_ = new_hover_element;
}

void Document::SetActiveElement(Element* new_active_element) {
  if (!new_active_element) {
    active_element_.Clear();
    return;
  }

  active_element_ = new_active_element;
}

void Document::RemoveFocusedElementOfSubtree(Node& node,
                                             bool among_children_only) {
  if (!focused_element_)
    return;

  // We can't be focused if we're not in the document.
  if (!node.isConnected())
    return;
  bool contains = node.IsShadowIncludingInclusiveAncestorOf(*focused_element_);
  if (contains && (focused_element_ != &node || !among_children_only))
    ClearFocusedElement();
}

static Element* SkipDisplayNoneAncestors(Element* element) {
  for (; element; element = FlatTreeTraversal::ParentElement(*element)) {
    if (element->GetLayoutObject() || element->HasDisplayContentsStyle())
      return element;
  }
  return nullptr;
}

static Element* SkipDisplayNoneAncestorsOrReturnNullIfFlatTreeIsDirty(
    Element& element) {
  if (element.GetDocument().IsSlotAssignmentOrLegacyDistributionDirty()) {
    // We shouldn't use FlatTreeTraversal during detach if slot assignment is
    // dirty because it might trigger assignment recalc. The hover and active
    // elements are then set to null. The hover element is updated on the next
    // lifecycle update instead.
    //
    // TODO(crbug.com/939769): The active element is not updated on the next
    // lifecycle update, and is generally not correctly updated on re-slotting.
    return nullptr;
  }
  return SkipDisplayNoneAncestors(&element);
}

void Document::HoveredElementDetached(Element& element) {
  if (!hover_element_)
    return;
  if (element != hover_element_)
    return;
  hover_element_ =
      SkipDisplayNoneAncestorsOrReturnNullIfFlatTreeIsDirty(element);

  // If the mouse cursor is not visible, do not clear existing
  // hover effects on the ancestors of |element| and do not invoke
  // new hover effects on any other element.
  if (!GetPage()->IsCursorVisible())
    return;

  if (GetFrame())
    GetFrame()->GetEventHandler().ScheduleHoverStateUpdate();
}

void Document::ActiveChainNodeDetached(Element& element) {
  if (active_element_ && element == active_element_) {
    active_element_ =
        SkipDisplayNoneAncestorsOrReturnNullIfFlatTreeIsDirty(element);
  }
}

const Vector<AnnotatedRegionValue>& Document::AnnotatedRegions() const {
  return annotated_regions_;
}

void Document::SetAnnotatedRegions(
    const Vector<AnnotatedRegionValue>& regions) {
  annotated_regions_ = regions;
  SetAnnotatedRegionsDirty(false);
}

void Document::SetLastFocusType(WebFocusType last_focus_type) {
  last_focus_type_ = last_focus_type;
}

bool Document::SetFocusedElement(Element* new_focused_element,
                                 const FocusParams& params) {
  DCHECK(!lifecycle_.InDetach());

  clear_focused_element_timer_.Stop();

  // Make sure new_focused_element is actually in this document.
  if (new_focused_element) {
    if (new_focused_element->GetDocument() != this)
      return true;

    if (NodeChildRemovalTracker::IsBeingRemoved(*new_focused_element))
      return true;
  }

  if (focused_element_ == new_focused_element)
    return true;

  bool focus_change_blocked = false;
  Element* old_focused_element = focused_element_;
  focused_element_ = nullptr;

  UpdateDistributionForFlatTreeTraversal();
  Node* ancestor = (old_focused_element && old_focused_element->isConnected() &&
                    new_focused_element)
                       ? FlatTreeTraversal::CommonAncestor(*old_focused_element,
                                                           *new_focused_element)
                       : nullptr;

  // Remove focus from the existing focus node (if any)
  if (old_focused_element) {
    old_focused_element->SetFocused(false, params.type);
    old_focused_element->SetHasFocusWithinUpToAncestor(false, ancestor);

    // Dispatch the blur event and let the node do any other blur related
    // activities (important for text fields)
    // If page lost focus, blur event will have already been dispatched
    if (GetPage() && (GetPage()->GetFocusController().IsFocused())) {
      old_focused_element->DispatchBlurEvent(new_focused_element, params.type,
                                             params.source_capabilities);
      if (focused_element_) {
        // handler shifted focus
        focus_change_blocked = true;
        new_focused_element = nullptr;
      }

      // 'focusout' is a DOM level 3 name for the bubbling blur event.
      old_focused_element->DispatchFocusOutEvent(event_type_names::kFocusout,
                                                 new_focused_element,
                                                 params.source_capabilities);
      // 'DOMFocusOut' is a DOM level 2 name for compatibility.
      // FIXME: We should remove firing DOMFocusOutEvent event when we are sure
      // no content depends on it, probably when <rdar://problem/8503958> is
      // resolved.
      old_focused_element->DispatchFocusOutEvent(event_type_names::kDOMFocusOut,
                                                 new_focused_element,
                                                 params.source_capabilities);

      if (focused_element_) {
        // handler shifted focus
        focus_change_blocked = true;
        new_focused_element = nullptr;
      }
    }
  }

  if (new_focused_element)
    UpdateStyleAndLayoutTreeForNode(new_focused_element);
  if (new_focused_element && new_focused_element->IsFocusable()) {
    if (IsRootEditableElement(*new_focused_element) &&
        !AcceptsEditingFocus(*new_focused_element)) {
      // delegate blocks focus change
      UpdateStyleAndLayoutTree();
      if (LocalFrame* frame = GetFrame())
        frame->Selection().DidChangeFocus();
      return false;
    }
    // Set focus on the new node
    focused_element_ = new_focused_element;
    SetSequentialFocusNavigationStartingPoint(focused_element_.Get());

    // Keep track of last focus from user interaction, ignoring focus from code.
    if (params.type != kWebFocusTypeNone)
      last_focus_type_ = params.type;

    focused_element_->SetFocused(true, params.type);
    focused_element_->SetHasFocusWithinUpToAncestor(true, ancestor);

    // Element::setFocused for frames can dispatch events.
    if (focused_element_ != new_focused_element) {
      UpdateStyleAndLayoutTree();
      if (LocalFrame* frame = GetFrame())
        frame->Selection().DidChangeFocus();
      return false;
    }
    CancelFocusAppearanceUpdate();
    EnsurePaintLocationDataValidForNode(focused_element_);
    focused_element_->UpdateFocusAppearanceWithOptions(
        params.selection_behavior, params.options);

    // Dispatch the focus event and let the node do any other focus related
    // activities (important for text fields)
    // If page lost focus, event will be dispatched on page focus, don't
    // duplicate
    if (GetPage() && (GetPage()->GetFocusController().IsFocused())) {
      focused_element_->DispatchFocusEvent(old_focused_element, params.type,
                                           params.source_capabilities);

      if (focused_element_ != new_focused_element) {
        // handler shifted focus
        UpdateStyleAndLayoutTree();
        if (LocalFrame* frame = GetFrame())
          frame->Selection().DidChangeFocus();
        return false;
      }
      // DOM level 3 bubbling focus event.
      focused_element_->DispatchFocusInEvent(event_type_names::kFocusin,
                                             old_focused_element, params.type,
                                             params.source_capabilities);

      if (focused_element_ != new_focused_element) {
        // handler shifted focus
        UpdateStyleAndLayoutTree();
        if (LocalFrame* frame = GetFrame())
          frame->Selection().DidChangeFocus();
        return false;
      }

      // For DOM level 2 compatibility.
      // FIXME: We should remove firing DOMFocusInEvent event when we are sure
      // no content depends on it, probably when <rdar://problem/8503958> is m.
      focused_element_->DispatchFocusInEvent(event_type_names::kDOMFocusIn,
                                             old_focused_element, params.type,
                                             params.source_capabilities);

      if (focused_element_ != new_focused_element) {
        // handler shifted focus
        UpdateStyleAndLayoutTree();
        if (LocalFrame* frame = GetFrame())
          frame->Selection().DidChangeFocus();
        return false;
      }
    }
  }

  if (!focus_change_blocked)
    NotifyFocusedElementChanged(old_focused_element, focused_element_.Get());

  UpdateStyleAndLayoutTree();
  if (LocalFrame* frame = GetFrame())
    frame->Selection().DidChangeFocus();
  return !focus_change_blocked;
}

void Document::ClearFocusedElement() {
  SetFocusedElement(nullptr, FocusParams(SelectionBehaviorOnFocus::kNone,
                                         kWebFocusTypeNone, nullptr));
}

void Document::NotifyFocusedElementChanged(Element* old_focused_element,
                                           Element* new_focused_element) {
  if (new_focused_element) {
    if (AXObjectCache* cache = ExistingAXObjectCache()) {
      cache->HandleFocusedUIElementChanged(old_focused_element,
                                           new_focused_element);
    }
  }

  if (GetPage()) {
    GetPage()->GetChromeClient().FocusedElementChanged(old_focused_element,
                                                       new_focused_element);
    if (GetSettings()->GetSpatialNavigationEnabled())
      GetPage()->GetSpatialNavigationController().FocusedNodeChanged(this);
  }
}

void Document::SetSequentialFocusNavigationStartingPoint(Node* node) {
  if (!frame_)
    return;
  if (!node) {
    sequential_focus_navigation_starting_point_ = nullptr;
    return;
  }
  DCHECK_EQ(node->GetDocument(), this);
  if (!sequential_focus_navigation_starting_point_)
    sequential_focus_navigation_starting_point_ = Range::Create(*this);
  sequential_focus_navigation_starting_point_->selectNodeContents(
      node, ASSERT_NO_EXCEPTION);
}

Element* Document::SequentialFocusNavigationStartingPoint(
    WebFocusType type) const {
  if (focused_element_)
    return focused_element_.Get();
  if (!sequential_focus_navigation_starting_point_)
    return nullptr;
  DCHECK(sequential_focus_navigation_starting_point_->IsConnected());
  if (!sequential_focus_navigation_starting_point_->collapsed()) {
    Node* node = sequential_focus_navigation_starting_point_->startContainer();
    DCHECK_EQ(node,
              sequential_focus_navigation_starting_point_->endContainer());
    if (auto* element = DynamicTo<Element>(node))
      return element;
    if (Element* neighbor_element = type == kWebFocusTypeForward
                                        ? ElementTraversal::Previous(*node)
                                        : ElementTraversal::Next(*node))
      return neighbor_element;
    return node->ParentOrShadowHostElement();
  }

  // Range::selectNodeContents didn't select contents because the element had
  // no children.
  auto* element = DynamicTo<Element>(
      sequential_focus_navigation_starting_point_->startContainer());
  if (element && !element->hasChildren() &&
      sequential_focus_navigation_starting_point_->startOffset() == 0)
    return element;

  // A node selected by Range::selectNodeContents was removed from the
  // document tree.
  if (Node* next_node =
          sequential_focus_navigation_starting_point_->FirstNode()) {
    if (next_node->IsShadowRoot())
      return next_node->OwnerShadowHost();
    // TODO(tkent): Using FlatTreeTraversal is inconsistent with
    // FocusController. Ideally we should find backward/forward focusable
    // elements before the starting point is disconnected. crbug.com/606582
    if (type == kWebFocusTypeForward) {
      Node* previous = FlatTreeTraversal::Previous(*next_node);
      for (; previous; previous = FlatTreeTraversal::Previous(*previous)) {
        if (auto* element = DynamicTo<Element>(previous))
          return element;
      }
    }
    for (Node* next = next_node; next; next = FlatTreeTraversal::Next(*next)) {
      if (auto* element = DynamicTo<Element>(next))
        return element;
    }
  }
  return nullptr;
}

void Document::SetCSSTarget(Element* new_target) {
  if (css_target_)
    css_target_->PseudoStateChanged(CSSSelector::kPseudoTarget);
  css_target_ = new_target;
  if (css_target_)
    css_target_->PseudoStateChanged(CSSSelector::kPseudoTarget);
}

void Document::RegisterNodeList(const LiveNodeListBase* list) {
  node_lists_.Add(list, list->InvalidationType());
  if (list->IsRootedAtTreeScope())
    lists_invalidated_at_document_.insert(list);
}

void Document::UnregisterNodeList(const LiveNodeListBase* list) {
  node_lists_.Remove(list, list->InvalidationType());
  if (list->IsRootedAtTreeScope()) {
    DCHECK(lists_invalidated_at_document_.Contains(list));
    lists_invalidated_at_document_.erase(list);
  }
}

void Document::RegisterNodeListWithIdNameCache(const LiveNodeListBase* list) {
  node_lists_.Add(list, kInvalidateOnIdNameAttrChange);
}

void Document::UnregisterNodeListWithIdNameCache(const LiveNodeListBase* list) {
  node_lists_.Remove(list, kInvalidateOnIdNameAttrChange);
}

void Document::AttachNodeIterator(NodeIterator* ni) {
  node_iterators_.insert(ni);
}

void Document::DetachNodeIterator(NodeIterator* ni) {
  // The node iterator can be detached without having been attached if its root
  // node didn't have a document when the iterator was created, but has it now.
  node_iterators_.erase(ni);
}

void Document::MoveNodeIteratorsToNewDocument(Node& node,
                                              Document& new_document) {
  HeapHashSet<WeakMember<NodeIterator>> node_iterators_list = node_iterators_;
  for (NodeIterator* ni : node_iterators_list) {
    if (ni->root() == node) {
      DetachNodeIterator(ni);
      new_document.AttachNodeIterator(ni);
    }
  }
}

void Document::DidMoveTreeToNewDocument(const Node& root) {
  DCHECK_NE(root.GetDocument(), this);
  if (!ranges_.IsEmpty()) {
    AttachedRangeSet ranges = ranges_;
    for (Range* range : ranges)
      range->UpdateOwnerDocumentIfNeeded();
  }
  NotifyMoveTreeToNewDocument(root);
}

void Document::NodeChildrenWillBeRemoved(ContainerNode& container) {
  EventDispatchForbiddenScope assert_no_event_dispatch;
  for (Range* range : ranges_) {
    range->NodeChildrenWillBeRemoved(container);
    if (range == sequential_focus_navigation_starting_point_)
      range->FixupRemovedChildrenAcrossShadowBoundary(container);
  }

  for (NodeIterator* ni : node_iterators_) {
    for (Node& n : NodeTraversal::ChildrenOf(container))
      ni->NodeWillBeRemoved(n);
  }

  NotifyNodeChildrenWillBeRemoved(container);

  if (ContainsV1ShadowTree()) {
    for (Node& n : NodeTraversal::ChildrenOf(container))
      n.CheckSlotChangeBeforeRemoved();
  }
}

void Document::NodeWillBeRemoved(Node& n) {
  for (NodeIterator* ni : node_iterators_)
    ni->NodeWillBeRemoved(n);

  for (Range* range : ranges_) {
    range->NodeWillBeRemoved(n);
    if (range == sequential_focus_navigation_starting_point_)
      range->FixupRemovedNodeAcrossShadowBoundary(n);
  }

  NotifyNodeWillBeRemoved(n);

  if (ContainsV1ShadowTree())
    n.CheckSlotChangeBeforeRemoved();

  if (n.InActiveDocument())
    GetStyleEngine().NodeWillBeRemoved(n);
}

void Document::DidInsertText(const CharacterData& text,
                             unsigned offset,
                             unsigned length) {
  for (Range* range : ranges_)
    range->DidInsertText(text, offset, length);
}

void Document::DidRemoveText(const CharacterData& text,
                             unsigned offset,
                             unsigned length) {
  for (Range* range : ranges_)
    range->DidRemoveText(text, offset, length);
}

void Document::DidMergeTextNodes(const Text& merged_node,
                                 const Text& node_to_be_removed,
                                 unsigned old_length) {
  NodeWithIndex node_to_be_removed_with_index(
      const_cast<Text&>(node_to_be_removed));
  if (!ranges_.IsEmpty()) {
    for (Range* range : ranges_)
      range->DidMergeTextNodes(node_to_be_removed_with_index, old_length);
  }

  NotifyMergeTextNodes(merged_node, node_to_be_removed_with_index, old_length);

  // FIXME: This should update markers for spelling and grammar checking.
}

void Document::DidSplitTextNode(const Text& old_node) {
  for (Range* range : ranges_)
    range->DidSplitTextNode(old_node);

  NotifySplitTextNode(old_node);

  // FIXME: This should update markers for spelling and grammar checking.
}

void Document::SetWindowAttributeEventListener(const AtomicString& event_type,
                                               EventListener* listener) {
  LocalDOMWindow* dom_window = domWindow();
  if (!dom_window)
    return;
  dom_window->SetAttributeEventListener(event_type, listener);
}

EventListener* Document::GetWindowAttributeEventListener(
    const AtomicString& event_type) {
  LocalDOMWindow* dom_window = domWindow();
  if (!dom_window)
    return nullptr;
  return dom_window->GetAttributeEventListener(event_type);
}

void Document::EnqueueAnimationFrameTask(base::OnceClosure task) {
  EnsureScriptedAnimationController().EnqueueTask(std::move(task));
}

void Document::EnqueueAnimationFrameEvent(Event* event) {
  EnsureScriptedAnimationController().EnqueueEvent(event);
}

void Document::EnqueueUniqueAnimationFrameEvent(Event* event) {
  EnsureScriptedAnimationController().EnqueuePerFrameEvent(event);
}

void Document::EnqueueScrollEventForNode(Node* target) {
  // Per the W3C CSSOM View Module only scroll events fired at the document
  // should bubble.
  Event* scroll_event = target->IsDocumentNode()
                            ? Event::CreateBubble(event_type_names::kScroll)
                            : Event::Create(event_type_names::kScroll);
  scroll_event->SetTarget(target);
  EnsureScriptedAnimationController().EnqueuePerFrameEvent(scroll_event);
}

void Document::EnqueueScrollEndEventForNode(Node* target) {
  // Mimic bubbling behavior of scroll event for consistency.
  Event* scroll_end_event =
      target->IsDocumentNode()
          ? Event::CreateBubble(event_type_names::kScrollend)
          : Event::Create(event_type_names::kScrollend);
  scroll_end_event->SetTarget(target);
  EnsureScriptedAnimationController().EnqueuePerFrameEvent(scroll_end_event);
}

void Document::EnqueueOverscrollEventForNode(Node* target,
                                             double delta_x,
                                             double delta_y) {
  // Mimic bubbling behavior of scroll event for consistency.
  bool bubbles = target->IsDocumentNode();
  Event* overscroll_event = OverscrollEvent::Create(
      event_type_names::kOverscroll, bubbles, delta_x, delta_y);
  overscroll_event->SetTarget(target);
  EnsureScriptedAnimationController().EnqueuePerFrameEvent(overscroll_event);
}

void Document::EnqueueResizeEvent() {
  Event* event = Event::Create(event_type_names::kResize);
  event->SetTarget(domWindow());
  EnsureScriptedAnimationController().EnqueuePerFrameEvent(event);
}

void Document::EnqueueMediaQueryChangeListeners(
    HeapVector<Member<MediaQueryListListener>>& listeners) {
  EnsureScriptedAnimationController().EnqueueMediaQueryChangeListeners(
      listeners);
}

void Document::EnqueueVisualViewportScrollEvent() {
  VisualViewportScrollEvent* event = VisualViewportScrollEvent::Create();
  event->SetTarget(domWindow()->visualViewport());
  EnsureScriptedAnimationController().EnqueuePerFrameEvent(event);
}

void Document::EnqueueVisualViewportResizeEvent() {
  VisualViewportResizeEvent* event = VisualViewportResizeEvent::Create();
  event->SetTarget(domWindow()->visualViewport());
  EnsureScriptedAnimationController().EnqueuePerFrameEvent(event);
}

void Document::DispatchEventsForPrinting() {
  if (!scripted_animation_controller_)
    return;
  scripted_animation_controller_->DispatchEventsAndCallbacksForPrinting();
}

Document::EventFactorySet& Document::EventFactories() {
  DEFINE_STATIC_LOCAL(EventFactorySet, event_factory, ());
  return event_factory;
}

const OriginAccessEntry& Document::AccessEntryFromURL() {
  if (!access_entry_from_url_) {
    access_entry_from_url_ = std::make_unique<OriginAccessEntry>(
        Url(), network::mojom::CorsDomainMatchMode::kAllowRegistrableDomains);
  }
  return *access_entry_from_url_;
}

void Document::SendDidEditFieldInInsecureContext() {
  if (!GetFrame())
    return;

  mojo::Remote<mojom::blink::InsecureInputService> insecure_input_service;
  GetFrame()->GetBrowserInterfaceBroker().GetInterface(
      insecure_input_service.BindNewPipeAndPassReceiver());

  insecure_input_service->DidEditFieldInInsecureContext();
}

void Document::RegisterEventFactory(
    std::unique_ptr<EventFactoryBase> event_factory) {
  DCHECK(!EventFactories().Contains(event_factory.get()));
  EventFactories().insert(std::move(event_factory));
}

Event* Document::createEvent(ScriptState* script_state,
                             const String& event_type,
                             ExceptionState& exception_state) {
  Event* event = nullptr;
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  for (const auto& factory : EventFactories()) {
    event = factory->Create(execution_context, event_type);
    if (event) {
      // createEvent for TouchEvent should throw DOM exception if touch event
      // feature detection is not enabled. See crbug.com/392584#c22
      if (DeprecatedEqualIgnoringCase(event_type, "TouchEvent") &&
          !RuntimeEnabledFeatures::TouchEventFeatureDetectionEnabled(
              execution_context))
        break;
      return event;
    }
  }
  exception_state.ThrowDOMException(
      DOMExceptionCode::kNotSupportedError,
      "The provided event type ('" + event_type + "') is invalid.");
  return nullptr;
}

void Document::AddMutationEventListenerTypeIfEnabled(
    ListenerType listener_type) {
  if (ContextFeatures::MutationEventsEnabled(this))
    AddListenerType(listener_type);
}

void Document::AddListenerTypeIfNeeded(const AtomicString& event_type,
                                       EventTarget& event_target) {
  if (event_type == event_type_names::kDOMSubtreeModified) {
    UseCounter::Count(*this, WebFeature::kDOMSubtreeModifiedEvent);
    AddMutationEventListenerTypeIfEnabled(kDOMSubtreeModifiedListener);
  } else if (event_type == event_type_names::kDOMNodeInserted) {
    UseCounter::Count(*this, WebFeature::kDOMNodeInsertedEvent);
    AddMutationEventListenerTypeIfEnabled(kDOMNodeInsertedListener);
  } else if (event_type == event_type_names::kDOMNodeRemoved) {
    UseCounter::Count(*this, WebFeature::kDOMNodeRemovedEvent);
    AddMutationEventListenerTypeIfEnabled(kDOMNodeRemovedListener);
  } else if (event_type == event_type_names::kDOMNodeRemovedFromDocument) {
    UseCounter::Count(*this, WebFeature::kDOMNodeRemovedFromDocumentEvent);
    AddMutationEventListenerTypeIfEnabled(kDOMNodeRemovedFromDocumentListener);
  } else if (event_type == event_type_names::kDOMNodeInsertedIntoDocument) {
    UseCounter::Count(*this, WebFeature::kDOMNodeInsertedIntoDocumentEvent);
    AddMutationEventListenerTypeIfEnabled(kDOMNodeInsertedIntoDocumentListener);
  } else if (event_type == event_type_names::kDOMCharacterDataModified) {
    UseCounter::Count(*this, WebFeature::kDOMCharacterDataModifiedEvent);
    AddMutationEventListenerTypeIfEnabled(kDOMCharacterDataModifiedListener);
  } else if (event_type == event_type_names::kWebkitAnimationStart ||
             event_type == event_type_names::kAnimationstart) {
    AddListenerType(kAnimationStartListener);
  } else if (event_type == event_type_names::kWebkitAnimationEnd ||
             event_type == event_type_names::kAnimationend) {
    AddListenerType(kAnimationEndListener);
  } else if (event_type == event_type_names::kWebkitAnimationIteration ||
             event_type == event_type_names::kAnimationiteration) {
    AddListenerType(kAnimationIterationListener);
    if (View()) {
      // Need to re-evaluate time-to-effect-change for any running animations.
      View()->ScheduleAnimation();
    }
  } else if (event_type == event_type_names::kTransitioncancel) {
    AddListenerType(kTransitionCancelListener);
  } else if (event_type == event_type_names::kTransitionrun) {
    AddListenerType(kTransitionRunListener);
  } else if (event_type == event_type_names::kTransitionstart) {
    AddListenerType(kTransitionStartListener);
  } else if (event_type == event_type_names::kWebkitTransitionEnd ||
             event_type == event_type_names::kTransitionend) {
    AddListenerType(kTransitionEndListener);
  } else if (event_type == event_type_names::kScroll) {
    AddListenerType(kScrollListener);
  } else if (event_type == event_type_names::kLoad) {
    if (Node* node = event_target.ToNode()) {
      if (IsA<HTMLStyleElement>(*node)) {
        AddListenerType(kLoadListenerAtCapturePhaseOrAtStyleElement);
        return;
      }
    }
    if (event_target.HasCapturingEventListeners(event_type))
      AddListenerType(kLoadListenerAtCapturePhaseOrAtStyleElement);
  }
}

HTMLFrameOwnerElement* Document::LocalOwner() const {
  if (!GetFrame())
    return nullptr;
  // FIXME: This probably breaks the attempts to layout after a load is finished
  // in implicitClose(), and probably tons of other things...
  return GetFrame()->DeprecatedLocalOwner();
}

void Document::WillChangeFrameOwnerProperties(int margin_width,
                                              int margin_height,
                                              ScrollbarMode scrolling_mode,
                                              bool is_display_none) {
  DCHECK(GetFrame() && GetFrame()->Owner());
  FrameOwner* owner = GetFrame()->Owner();

  if (is_display_none != owner->IsDisplayNone())
    DisplayNoneChangedForFrame();

  // body() may become null as a result of modification event listeners, so we
  // check before each call.
  if (margin_width != owner->MarginWidth()) {
    if (auto* body_element = body()) {
      body_element->SetIntegralAttribute(html_names::kMarginwidthAttr,
                                         margin_width);
    }
  }
  if (margin_height != owner->MarginHeight()) {
    if (auto* body_element = body()) {
      body_element->SetIntegralAttribute(html_names::kMarginheightAttr,
                                         margin_height);
    }
  }
  if (scrolling_mode != owner->ScrollingMode() && View()) {
    View()->SetCanHaveScrollbars(scrolling_mode != ScrollbarMode::kAlwaysOff);
    View()->SetNeedsLayout();
  }
}

String Document::cookie(ExceptionState& exception_state) const {
  if (GetSettings() && !GetSettings()->GetCookieEnabled())
    return String();

  CountUse(WebFeature::kCookieGet);

  if (!GetSecurityOrigin()->CanAccessCookies()) {
    if (IsSandboxed(WebSandboxFlags::kOrigin))
      exception_state.ThrowSecurityError(
          "The document is sandboxed and lacks the 'allow-same-origin' flag.");
    else if (Url().ProtocolIs("data"))
      exception_state.ThrowSecurityError(
          "Cookies are disabled inside 'data:' URLs.");
    else
      exception_state.ThrowSecurityError("Access is denied for this document.");
    return String();
  } else if (GetSecurityOrigin()->IsLocal()) {
    CountUse(WebFeature::kFileAccessedCookies);
  }

  if (!cookie_jar_)
    return String();

  return cookie_jar_->Cookies();
}

void Document::setCookie(const String& value, ExceptionState& exception_state) {
  if (GetSettings() && !GetSettings()->GetCookieEnabled())
    return;

  UseCounter::Count(*this, WebFeature::kCookieSet);

  if (!GetSecurityOrigin()->CanAccessCookies()) {
    if (IsSandboxed(WebSandboxFlags::kOrigin))
      exception_state.ThrowSecurityError(
          "The document is sandboxed and lacks the 'allow-same-origin' flag.");
    else if (Url().ProtocolIs("data"))
      exception_state.ThrowSecurityError(
          "Cookies are disabled inside 'data:' URLs.");
    else
      exception_state.ThrowSecurityError("Access is denied for this document.");
    return;
  } else if (GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(*this, WebFeature::kFileAccessedCookies);
  }

  if (!cookie_jar_)
    return;

  cookie_jar_->SetCookie(value);
}

bool Document::CookiesEnabled() const {
  // Compatible behavior in contexts that don't have cookie access.
  if (!GetSecurityOrigin()->CanAccessCookies())
    return true;

  if (!cookie_jar_)
    return false;

  return cookie_jar_->CookiesEnabled();
}

const AtomicString& Document::referrer() const {
  if (Loader())
    return Loader()->GetReferrer().referrer;
  return g_null_atom;
}

String Document::domain() const {
  return GetSecurityOrigin()->Domain();
}

void Document::setDomain(const String& raw_domain,
                         ExceptionState& exception_state) {
  UseCounter::Count(*this, WebFeature::kDocumentSetDomain);

  const String feature_policy_error =
      "Setting `document.domain` is disabled by Feature Policy.";
  if (!IsFeatureEnabled(mojom::FeaturePolicyFeature::kDocumentDomain,
                        ReportOptions::kReportOnFailure,
                        feature_policy_error)) {
    exception_state.ThrowSecurityError(feature_policy_error);
    return;
  }

  if (!frame_) {
    exception_state.ThrowSecurityError(
        "A browsing context is required to set a domain.");
    return;
  }

  if (IsSandboxed(WebSandboxFlags::kDocumentDomain)) {
    exception_state.ThrowSecurityError(
        "Assignment is forbidden for sandboxed iframes.");
    return;
  }

  if (SchemeRegistry::IsDomainRelaxationForbiddenForURLScheme(
          GetSecurityOrigin()->Protocol())) {
    exception_state.ThrowSecurityError("Assignment is forbidden for the '" +
                                       GetSecurityOrigin()->Protocol() +
                                       "' scheme.");
    return;
  }

  bool success = false;
  String new_domain = SecurityOrigin::CanonicalizeHost(raw_domain, &success);
  if (!success) {
    exception_state.ThrowSecurityError("'" + raw_domain +
                                       "' could not be parsed properly.");
    return;
  }

  if (new_domain.IsEmpty()) {
    exception_state.ThrowSecurityError("'" + new_domain +
                                       "' is an empty domain.");
    return;
  }

  scoped_refptr<SecurityOrigin> new_origin =
      GetSecurityOrigin()->IsolatedCopy();
  new_origin->SetDomainFromDOM(new_domain);
  OriginAccessEntry access_entry(
      *new_origin, network::mojom::CorsDomainMatchMode::kAllowSubdomains);
  network::cors::OriginAccessEntry::MatchResult result =
      access_entry.MatchesOrigin(*GetSecurityOrigin());
  if (result == network::cors::OriginAccessEntry::kDoesNotMatchOrigin) {
    exception_state.ThrowSecurityError(
        "'" + new_domain + "' is not a suffix of '" + domain() + "'.");
    return;
  }

  if (result ==
      network::cors::OriginAccessEntry::kMatchesOriginButIsPublicSuffix) {
    exception_state.ThrowSecurityError("'" + new_domain +
                                       "' is a top-level domain.");
    return;
  }

  if (frame_) {
    UseCounter::Count(*this,
                      GetSecurityOrigin()->Port() == 0
                          ? WebFeature::kDocumentDomainSetWithDefaultPort
                          : WebFeature::kDocumentDomainSetWithNonDefaultPort);
    bool was_cross_domain = frame_->IsCrossOriginSubframe();
    GetMutableSecurityOrigin()->SetDomainFromDOM(new_domain);
    bool is_cross_domain = frame_->IsCrossOriginSubframe();
    if (FrameScheduler* frame_scheduler = frame_->GetFrameScheduler())
      frame_scheduler->SetCrossOrigin(is_cross_domain);
    if (View() && (was_cross_domain != is_cross_domain))
      View()->CrossOriginStatusChanged();

    frame_->GetScriptController().UpdateSecurityOrigin(GetSecurityOrigin());
  }
}

// https://html.spec.whatwg.org/C#dom-document-lastmodified
String Document::lastModified() const {
  base::Time::Exploded exploded;
  bool found_date = false;
  AtomicString http_last_modified = override_last_modified_;
  if (http_last_modified.IsEmpty() && frame_) {
    if (DocumentLoader* document_loader = Loader()) {
      http_last_modified = document_loader->GetResponse().HttpHeaderField(
          http_names::kLastModified);
    }
  }
  if (!http_last_modified.IsEmpty()) {
    base::Optional<base::Time> date_value = ParseDate(http_last_modified);
    if (date_value) {
      date_value.value().LocalExplode(&exploded);
      found_date = true;
    }
  }
  // FIXME: If this document came from the file system, the HTML5
  // specificiation tells us to read the last modification date from the file
  // system.
  if (!found_date) {
    base::Time::Now().LocalExplode(&exploded);
  }
  return String::Format("%02d/%02d/%04d %02d:%02d:%02d", exploded.month,
                        exploded.day_of_month, exploded.year, exploded.hour,
                        exploded.minute, exploded.second);
}

void Document::SetFindInPageRoot(Element* find_in_page_root) {
  DCHECK(RuntimeEnabledFeatures::InvisibleDOMEnabled());
  DCHECK(!find_in_page_root || !find_in_page_root_);
  find_in_page_root_ = find_in_page_root;
}

scoped_refptr<const SecurityOrigin> Document::TopFrameOrigin() const {
  if (!GetFrame())
    return scoped_refptr<const SecurityOrigin>();

  return GetFrame()->Tree().Top().GetSecurityContext()->GetSecurityOrigin();
}

const KURL Document::SiteForCookies() const {
  // TODO(mkwst): This doesn't properly handle HTML Import documents.

  // If this is an imported document, grab its master document's first-party:
  if (IsHTMLImport())
    return ImportsController()->Master()->SiteForCookies();

  if (!GetFrame())
    return NullURL();

  Frame& top = GetFrame()->Tree().Top();
  const SecurityOrigin* origin = top.GetSecurityContext()->GetSecurityOrigin();
  // TODO(yhirano): Ideally |origin| should not be null here.
  if (!origin)
    return NullURL();

  if (SchemeRegistry::ShouldTreatURLSchemeAsFirstPartyWhenTopLevel(
          origin->Protocol())) {
    return origin->IsOpaque() ? NullURL() : KURL(origin->ToString());
  }

  OriginAccessEntry access_entry(
      *origin, network::mojom::CorsDomainMatchMode::kAllowRegistrableDomains);

  const Frame* current_frame = GetFrame();
  while (current_frame) {
    // We use 'matchesDomain' here, as it turns out that some folks embed HTTPS
    // login forms into HTTP pages; we should allow this kind of upgrade.
    if (access_entry.MatchesDomain(
            *current_frame->GetSecurityContext()->GetSecurityOrigin()) ==
        network::cors::OriginAccessEntry::kDoesNotMatchOrigin) {
      return NullURL();
    }

    current_frame = current_frame->Tree().Parent();
  }

  return origin->IsOpaque() ? NullURL() : KURL(origin->ToString());
}

ScriptPromise Document::hasStorageAccess(ScriptState* script_state) const {
  const bool has_access =
      TopFrameOrigin() &&
      GetSecurityOrigin()->IsSameSchemeHostPort(TopFrameOrigin().get());
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  // TODO (http://crbug.com/989663)
  // Hookup actual logic to Resolve/Reject this request properly.
  ScriptPromise promise = resolver->Promise();
  resolver->Resolve(has_access);
  return promise;
}

ScriptPromise Document::requestStorageAccess(ScriptState* script_state) const {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  // TODO (http://crbug.com/989663)
  // Hookup actual logic to Resolve/Reject this request properly.
  ScriptPromise promise = resolver->Promise();
  resolver->Reject();
  return promise;
}

static bool IsValidNameNonASCII(const LChar* characters, unsigned length) {
  if (!IsValidNameStart(characters[0]))
    return false;

  for (unsigned i = 1; i < length; ++i) {
    if (!IsValidNamePart(characters[i]))
      return false;
  }

  return true;
}

static bool IsValidNameNonASCII(const UChar* characters, unsigned length) {
  for (unsigned i = 0; i < length;) {
    bool first = i == 0;
    UChar32 c;
    U16_NEXT(characters, i, length, c);  // Increments i.
    if (first ? !IsValidNameStart(c) : !IsValidNamePart(c))
      return false;
  }

  return true;
}

template <typename CharType>
static inline bool IsValidNameASCII(const CharType* characters,
                                    unsigned length) {
  CharType c = characters[0];
  if (!(IsASCIIAlpha(c) || c == ':' || c == '_'))
    return false;

  for (unsigned i = 1; i < length; ++i) {
    c = characters[i];
    if (!(IsASCIIAlphanumeric(c) || c == ':' || c == '_' || c == '-' ||
          c == '.'))
      return false;
  }

  return true;
}

bool Document::IsValidName(const String& name) {
  unsigned length = name.length();
  if (!length)
    return false;

  if (name.Is8Bit()) {
    const LChar* characters = name.Characters8();

    if (IsValidNameASCII(characters, length))
      return true;

    return IsValidNameNonASCII(characters, length);
  }

  const UChar* characters = name.Characters16();

  if (IsValidNameASCII(characters, length))
    return true;

  return IsValidNameNonASCII(characters, length);
}

enum QualifiedNameStatus {
  kQNValid,
  kQNMultipleColons,
  kQNInvalidStartChar,
  kQNInvalidChar,
  kQNEmptyPrefix,
  kQNEmptyLocalName
};

struct ParseQualifiedNameResult {
  QualifiedNameStatus status;
  UChar32 character;
  ParseQualifiedNameResult() = default;
  explicit ParseQualifiedNameResult(QualifiedNameStatus status)
      : status(status) {}
  ParseQualifiedNameResult(QualifiedNameStatus status, UChar32 character)
      : status(status), character(character) {}
};

template <typename CharType>
static ParseQualifiedNameResult ParseQualifiedNameInternal(
    const AtomicString& qualified_name,
    const CharType* characters,
    unsigned length,
    AtomicString& prefix,
    AtomicString& local_name) {
  bool name_start = true;
  bool saw_colon = false;
  unsigned colon_pos = 0;

  for (unsigned i = 0; i < length;) {
    UChar32 c;
    U16_NEXT(characters, i, length, c);
    if (c == ':') {
      if (saw_colon)
        return ParseQualifiedNameResult(kQNMultipleColons);
      name_start = true;
      saw_colon = true;
      colon_pos = i - 1;
    } else if (name_start) {
      if (!IsValidNameStart(c))
        return ParseQualifiedNameResult(kQNInvalidStartChar, c);
      name_start = false;
    } else {
      if (!IsValidNamePart(c))
        return ParseQualifiedNameResult(kQNInvalidChar, c);
    }
  }

  if (!saw_colon) {
    prefix = g_null_atom;
    local_name = qualified_name;
  } else {
    prefix = AtomicString(characters, colon_pos);
    if (prefix.IsEmpty())
      return ParseQualifiedNameResult(kQNEmptyPrefix);
    int prefix_start = colon_pos + 1;
    local_name = AtomicString(characters + prefix_start, length - prefix_start);
  }

  if (local_name.IsEmpty())
    return ParseQualifiedNameResult(kQNEmptyLocalName);

  return ParseQualifiedNameResult(kQNValid);
}

bool Document::ParseQualifiedName(const AtomicString& qualified_name,
                                  AtomicString& prefix,
                                  AtomicString& local_name,
                                  ExceptionState& exception_state) {
  unsigned length = qualified_name.length();

  if (!length) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidCharacterError,
                                      "The qualified name provided is empty.");
    return false;
  }

  ParseQualifiedNameResult return_value;
  if (qualified_name.Is8Bit())
    return_value =
        ParseQualifiedNameInternal(qualified_name, qualified_name.Characters8(),
                                   length, prefix, local_name);
  else
    return_value = ParseQualifiedNameInternal(qualified_name,
                                              qualified_name.Characters16(),
                                              length, prefix, local_name);
  if (return_value.status == kQNValid)
    return true;

  StringBuilder message;
  message.Append("The qualified name provided ('");
  message.Append(qualified_name);
  message.Append("') ");

  if (return_value.status == kQNMultipleColons) {
    message.Append("contains multiple colons.");
  } else if (return_value.status == kQNInvalidStartChar) {
    message.Append("contains the invalid name-start character '");
    message.Append(return_value.character);
    message.Append("'.");
  } else if (return_value.status == kQNInvalidChar) {
    message.Append("contains the invalid character '");
    message.Append(return_value.character);
    message.Append("'.");
  } else if (return_value.status == kQNEmptyPrefix) {
    message.Append("has an empty namespace prefix.");
  } else {
    DCHECK_EQ(return_value.status, kQNEmptyLocalName);
    message.Append("has an empty local name.");
  }

  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidCharacterError,
                                    message.ToString());
  return false;
}

void Document::SetEncodingData(const DocumentEncodingData& new_data) {
  // It's possible for the encoding of the document to change while we're
  // decoding data. That can only occur while we're processing the <head>
  // portion of the document. There isn't much user-visible content in the
  // <head>, but there is the <title> element. This function detects that
  // situation and re-decodes the document's title so that the user doesn't see
  // an incorrectly decoded title in the title bar.
  if (title_element_ && Encoding() != new_data.Encoding() &&
      !ElementTraversal::FirstWithin(*title_element_) &&
      Encoding() == Latin1Encoding() &&
      title_element_->textContent().ContainsOnlyLatin1OrEmpty()) {
    std::string original_bytes = title_element_->textContent().Latin1();
    std::unique_ptr<TextCodec> codec = NewTextCodec(new_data.Encoding());
    String correctly_decoded_title =
        codec->Decode(original_bytes.c_str(),
                      static_cast<wtf_size_t>(original_bytes.length()),
                      WTF::FlushBehavior::kDataEOF);
    title_element_->setTextContent(correctly_decoded_title);
  }

  DCHECK(new_data.Encoding().IsValid());
  encoding_data_ = new_data;

  // FIXME: Should be removed as part of
  // https://code.google.com/p/chromium/issues/detail?id=319643
  bool should_use_visual_ordering =
      encoding_data_.Encoding().UsesVisualOrdering();
  if (should_use_visual_ordering != visually_ordered_) {
    visually_ordered_ = should_use_visual_ordering;
    GetStyleEngine().MarkViewportStyleDirty();
    GetStyleEngine().MarkAllElementsForStyleRecalc(
        StyleChangeReasonForTracing::Create(
            style_change_reason::kVisuallyOrdered));
  }
}

KURL Document::CompleteURL(const String& url) const {
  return CompleteURLWithOverride(url, base_url_);
}

KURL Document::CompleteURLWithOverride(const String& url,
                                       const KURL& base_url_override) const {
  DCHECK(base_url_override.IsEmpty() || base_url_override.IsValid());

  // Always return a null URL when passed a null string.
  // FIXME: Should we change the KURL constructor to have this behavior?
  // See also [CSS]StyleSheet::completeURL(const String&)
  if (url.IsNull())
    return KURL();
  if (!Encoding().IsValid())
    return KURL(base_url_override, url);
  return KURL(base_url_override, url, Encoding());
}

// static
bool Document::ShouldInheritSecurityOriginFromOwner(const KURL& url) {
  // https://html.spec.whatwg.org/C/#origin
  //
  // If a Document is the initial "about:blank" document The origin and
  // effective script origin of the Document are those it was assigned when its
  // browsing context was created.
  //
  // Note: We generalize this to all "blank" URLs and invalid URLs because we
  // treat all of these URLs as about:blank.
  return url.IsEmpty() || url.ProtocolIsAbout();
}

KURL Document::OpenSearchDescriptionURL() {
  static const char kOpenSearchMIMEType[] =
      "application/opensearchdescription+xml";
  static const char kOpenSearchRelation[] = "search";

  // FIXME: Why do only top-level frames have openSearchDescriptionURLs?
  if (!GetFrame() || GetFrame()->Tree().Parent())
    return KURL();

  // FIXME: Why do we need to wait for load completion?
  if (!LoadEventFinished())
    return KURL();

  if (!head())
    return KURL();

  for (HTMLLinkElement* link_element =
           Traversal<HTMLLinkElement>::FirstChild(*head());
       link_element;
       link_element = Traversal<HTMLLinkElement>::NextSibling(*link_element)) {
    if (!DeprecatedEqualIgnoringCase(link_element->GetType(),
                                     kOpenSearchMIMEType) ||
        !DeprecatedEqualIgnoringCase(link_element->Rel(), kOpenSearchRelation))
      continue;
    if (link_element->Href().IsEmpty())
      continue;

    // Count usage; perhaps we can lock this to secure contexts.
    WebFeature osd_disposition;
    scoped_refptr<const SecurityOrigin> target =
        SecurityOrigin::Create(link_element->Href());
    if (IsSecureContext()) {
      osd_disposition = target->IsPotentiallyTrustworthy()
                            ? WebFeature::kOpenSearchSecureOriginSecureTarget
                            : WebFeature::kOpenSearchSecureOriginInsecureTarget;
    } else {
      osd_disposition =
          target->IsPotentiallyTrustworthy()
              ? WebFeature::kOpenSearchInsecureOriginSecureTarget
              : WebFeature::kOpenSearchInsecureOriginInsecureTarget;
    }
    UseCounter::Count(*this, osd_disposition);

    return link_element->Href();
  }

  return KURL();
}

void Document::currentScriptForBinding(
    HTMLScriptElementOrSVGScriptElement& script_element) const {
  if (!current_script_stack_.IsEmpty()) {
    if (ScriptElementBase* script_element_base = current_script_stack_.back())
      script_element_base->SetScriptElementForBinding(script_element);
  }
}

void Document::PushCurrentScript(ScriptElementBase* new_current_script) {
  current_script_stack_.push_back(new_current_script);
}

void Document::PopCurrentScript(ScriptElementBase* script) {
  DCHECK(!current_script_stack_.IsEmpty());
  DCHECK_EQ(current_script_stack_.back(), script);
  current_script_stack_.pop_back();
}

void Document::SetTransformSource(std::unique_ptr<TransformSource> source) {
  transform_source_ = std::move(source);
}

String Document::designMode() const {
  return InDesignMode() ? "on" : "off";
}

void Document::setDesignMode(const String& value) {
  bool new_value = design_mode_;
  if (DeprecatedEqualIgnoringCase(value, "on")) {
    new_value = true;
    UseCounter::Count(*this, WebFeature::kDocumentDesignModeEnabeld);
  } else if (DeprecatedEqualIgnoringCase(value, "off")) {
    new_value = false;
  }
  if (new_value == design_mode_)
    return;
  design_mode_ = new_value;
  GetStyleEngine().MarkViewportStyleDirty();
  GetStyleEngine().MarkAllElementsForStyleRecalc(
      StyleChangeReasonForTracing::Create(style_change_reason::kDesignMode));
}

Document* Document::ParentDocument() const {
  if (!frame_)
    return nullptr;
  auto* parent_local_frame = DynamicTo<LocalFrame>(frame_->Tree().Parent());
  if (!parent_local_frame)
    return nullptr;
  return parent_local_frame->GetDocument();
}

Document& Document::TopDocument() const {
  // FIXME: Not clear what topDocument() should do in the OOPI case--should it
  // return the topmost available Document, or something else?
  Document* doc = const_cast<Document*>(this);
  for (HTMLFrameOwnerElement* element = doc->LocalOwner(); element;
       element = doc->LocalOwner())
    doc = &element->GetDocument();

  DCHECK(doc);
  return *doc;
}

Document* Document::ContextDocument() const {
  if (context_document_)
    return context_document_;
  if (frame_)
    return const_cast<Document*>(this);
  return nullptr;
}

Attr* Document::createAttribute(const AtomicString& name,
                                ExceptionState& exception_state) {
  if (!IsValidName(name)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidCharacterError,
                                      "The localName provided ('" + name +
                                          "') contains an invalid character.");
    return nullptr;
  }
  return MakeGarbageCollected<Attr>(
      *this, QualifiedName(g_null_atom, ConvertLocalName(name), g_null_atom),
      g_empty_atom);
}

Attr* Document::createAttributeNS(const AtomicString& namespace_uri,
                                  const AtomicString& qualified_name,
                                  ExceptionState& exception_state) {
  AtomicString prefix, local_name;
  if (!ParseQualifiedName(qualified_name, prefix, local_name, exception_state))
    return nullptr;

  QualifiedName q_name(prefix, local_name, namespace_uri);

  if (!HasValidNamespaceForAttributes(q_name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNamespaceError,
        "The namespace URI provided ('" + namespace_uri +
            "') is not valid for the qualified name provided ('" +
            qualified_name + "').");
    return nullptr;
  }

  return MakeGarbageCollected<Attr>(*this, q_name, g_empty_atom);
}

const SVGDocumentExtensions* Document::SvgExtensions() {
  return svg_extensions_.Get();
}

SVGDocumentExtensions& Document::AccessSVGExtensions() {
  if (!svg_extensions_)
    svg_extensions_ = MakeGarbageCollected<SVGDocumentExtensions>(this);
  return *svg_extensions_;
}

bool Document::HasSVGRootNode() const {
  return IsSVGSVGElement(documentElement());
}

HTMLCollection* Document::images() {
  return EnsureCachedCollection<HTMLCollection>(kDocImages);
}

HTMLCollection* Document::applets() {
  return EnsureCachedCollection<HTMLCollection>(kDocApplets);
}

HTMLCollection* Document::embeds() {
  return EnsureCachedCollection<HTMLCollection>(kDocEmbeds);
}

HTMLCollection* Document::scripts() {
  return EnsureCachedCollection<HTMLCollection>(kDocScripts);
}

HTMLCollection* Document::links() {
  return EnsureCachedCollection<HTMLCollection>(kDocLinks);
}

HTMLCollection* Document::forms() {
  return EnsureCachedCollection<HTMLCollection>(kDocForms);
}

HTMLCollection* Document::anchors() {
  return EnsureCachedCollection<HTMLCollection>(kDocAnchors);
}

HTMLAllCollection* Document::all() {
  return EnsureCachedCollection<HTMLAllCollection>(kDocAll);
}

HTMLCollection* Document::WindowNamedItems(const AtomicString& name) {
  return EnsureCachedCollection<WindowNameCollection>(kWindowNamedItems, name);
}

DocumentNameCollection* Document::DocumentNamedItems(const AtomicString& name) {
  return EnsureCachedCollection<DocumentNameCollection>(kDocumentNamedItems,
                                                        name);
}

HTMLCollection* Document::DocumentAllNamedItems(const AtomicString& name) {
  return EnsureCachedCollection<DocumentAllNameCollection>(
      kDocumentAllNamedItems, name);
}

LocalDOMWindow* Document::defaultView() const {
  // The HTML spec requires to return null if the document is detached from the
  // DOM.  However, |dom_window_| is not cleared on the detachment.  So, we need
  // to check |frame_| to tell whether the document is attached or not.
  return frame_ ? dom_window_ : nullptr;
}

namespace {

using performance_manager::mojom::InterventionPolicy;

// A helper function to set the origin trial freeze policy of a document.
void SetOriginTrialFreezePolicy(
    DocumentResourceCoordinator* document_resource_coordinator,
    ExecutionContext* context) {
  // An explicit opt-out overrides an explicit opt-in if both are present.
  InterventionPolicy policy = InterventionPolicy::kDefault;
  if (RuntimeEnabledFeatures::PageFreezeOptOutEnabled(context)) {
    policy = InterventionPolicy::kOptOut;
    UseCounter::Count(context, WebFeature::kPageFreezeOptOut);
  } else if (RuntimeEnabledFeatures::PageFreezeOptInEnabled(context)) {
    policy = InterventionPolicy::kOptIn;
    UseCounter::Count(context, WebFeature::kPageFreezeOptIn);
  }

  document_resource_coordinator->SetOriginTrialFreezePolicy(policy);
}

}  // namespace

void Document::FinishedParsing() {
  DCHECK(!GetScriptableDocumentParser() || !parser_->IsParsing());
  DCHECK(!GetScriptableDocumentParser() || ready_state_ != kLoading);
  SetParsingState(kInDOMContentLoaded);
  DocumentParserTiming::From(*this).MarkParserStop();

  // FIXME: DOMContentLoaded is dispatched synchronously, but this should be
  // dispatched in a queued task, see https://crbug.com/425790
  if (document_timing_.DomContentLoadedEventStart().is_null())
    document_timing_.MarkDomContentLoadedEventStart();
  DispatchEvent(*Event::CreateBubble(event_type_names::kDOMContentLoaded));
  if (document_timing_.DomContentLoadedEventEnd().is_null())
    document_timing_.MarkDomContentLoadedEventEnd();
  SetParsingState(kFinishedParsing);

  // Ensure Custom Element callbacks are drained before DOMContentLoaded.
  // FIXME: Remove this ad-hoc checkpoint when DOMContentLoaded is dispatched in
  // a queued task, which will do a checkpoint anyway. https://crbug.com/425790
  Microtask::PerformCheckpoint(V8PerIsolateData::MainThreadIsolate());

  ScriptableDocumentParser* parser = GetScriptableDocumentParser();
  well_formed_ = parser && parser->WellFormed();

  if (LocalFrame* frame = GetFrame()) {
    // Guarantee at least one call to the client specifying a title. (If
    // |title_| is not empty, then the title has already been dispatched.)
    if (title_.IsEmpty())
      DispatchDidReceiveTitle();

    // Don't update the layout tree if we haven't requested the main resource
    // yet to avoid adding extra latency. Note that the first layout tree update
    // can be expensive since it triggers the parsing of the default stylesheets
    // which are compiled-in.
    const bool main_resource_was_already_requested =
        frame->Loader().StateMachine()->CommittedFirstRealDocumentLoad();

    // FrameLoader::finishedParsing() might end up calling
    // Document::implicitClose() if all resource loads are
    // complete. HTMLObjectElements can start loading their resources from post
    // attach callbacks triggered by recalcStyle().  This means if we parse out
    // an <object> tag and then reach the end of the document without updating
    // styles, we might not have yet started the resource load and might fire
    // the window load event too early.  To avoid this we force the styles to be
    // up to date before calling FrameLoader::finishedParsing().  See
    // https://bugs.webkit.org/show_bug.cgi?id=36864 starting around comment 35.
    if (main_resource_was_already_requested)
      UpdateStyleAndLayoutTree();

    BeginLifecycleUpdatesIfRenderingReady();

    frame->Loader().FinishedParsing();

    TRACE_EVENT_INSTANT1("devtools.timeline", "MarkDOMContent",
                         TRACE_EVENT_SCOPE_THREAD, "data",
                         inspector_mark_load_event::Data(frame));
    probe::DomContentLoadedEventFired(frame);
    frame->GetIdlenessDetector()->DomContentLoadedEventFired();

    // Forward origin trial freeze policy to the corresponding frame object in
    // the resource coordinator.
    if (auto* document_resource_coordinator = GetResourceCoordinator())
      SetOriginTrialFreezePolicy(document_resource_coordinator, this);
  }

  // Schedule dropping of the ElementDataCache. We keep it alive for a while
  // after parsing finishes so that dynamically inserted content can also
  // benefit from sharing optimizations.  Note that we don't refresh the timer
  // on cache access since that could lead to huge caches being kept alive
  // indefinitely by something innocuous like JS setting .innerHTML repeatedly
  // on a timer.
  element_data_cache_clear_timer_.StartOneShot(base::TimeDelta::FromSeconds(10),
                                               FROM_HERE);

  // Parser should have picked up all preloads by now
  fetcher_->ClearPreloads(ResourceFetcher::kClearSpeculativeMarkupPreloads);

  if (IsPrefetchOnly())
    WebPrerenderingSupport::Current()->PrefetchFinished();
}

void Document::ElementDataCacheClearTimerFired(TimerBase*) {
  element_data_cache_.Clear();
}

void Document::BeginLifecycleUpdatesIfRenderingReady() {
  if (!IsActive())
    return;
  if (!HaveRenderBlockingResourcesLoaded())
    return;
  View()->BeginLifecycleUpdates();
}

Vector<IconURL> Document::IconURLs(int icon_types_mask) {
  IconURL first_favicon;
  IconURL first_touch_icon;
  IconURL first_touch_precomposed_icon;
  Vector<IconURL> secondary_icons;

  using TraversalFunction = HTMLLinkElement* (*)(const Node&);
  TraversalFunction find_next_candidate =
      &Traversal<HTMLLinkElement>::NextSibling;

  HTMLLinkElement* first_element = nullptr;
  if (head()) {
    first_element = Traversal<HTMLLinkElement>::FirstChild(*head());
  } else if (IsSVGDocument() && IsSVGSVGElement(documentElement())) {
    first_element = Traversal<HTMLLinkElement>::FirstWithin(*documentElement());
    find_next_candidate = &Traversal<HTMLLinkElement>::Next;
  }

  // Start from the first child node so that icons seen later take precedence as
  // required by the spec.
  for (HTMLLinkElement* link_element = first_element; link_element;
       link_element = find_next_candidate(*link_element)) {
    if (!(link_element->GetIconType() & icon_types_mask))
      continue;
    if (link_element->Href().IsEmpty())
      continue;

    IconURL new_url(link_element->Href(), link_element->IconSizes(),
                    link_element->GetType(), link_element->GetIconType());
    if (link_element->GetIconType() == kFavicon) {
      if (first_favicon.icon_type_ != kInvalidIcon)
        secondary_icons.push_back(first_favicon);
      first_favicon = new_url;
    } else if (link_element->GetIconType() == kTouchIcon) {
      if (first_touch_icon.icon_type_ != kInvalidIcon)
        secondary_icons.push_back(first_touch_icon);
      first_touch_icon = new_url;
    } else if (link_element->GetIconType() == kTouchPrecomposedIcon) {
      if (first_touch_precomposed_icon.icon_type_ != kInvalidIcon)
        secondary_icons.push_back(first_touch_precomposed_icon);
      first_touch_precomposed_icon = new_url;
    } else {
      NOTREACHED();
    }
  }

  Vector<IconURL> icon_urls;
  if (first_favicon.icon_type_ != kInvalidIcon)
    icon_urls.push_back(first_favicon);
  else if (url_.ProtocolIsInHTTPFamily() && icon_types_mask & kFavicon)
    icon_urls.push_back(IconURL::DefaultFavicon(url_));

  if (first_touch_icon.icon_type_ != kInvalidIcon)
    icon_urls.push_back(first_touch_icon);
  if (first_touch_precomposed_icon.icon_type_ != kInvalidIcon)
    icon_urls.push_back(first_touch_precomposed_icon);
  for (int i = secondary_icons.size() - 1; i >= 0; --i)
    icon_urls.push_back(secondary_icons[i]);
  return icon_urls;
}

base::Optional<Color> Document::ThemeColor() const {
  auto* root_element = documentElement();
  if (!root_element)
    return base::nullopt;
  for (HTMLMetaElement& meta_element :
       Traversal<HTMLMetaElement>::DescendantsOf(*root_element)) {
    Color color;
    if (DeprecatedEqualIgnoringCase(meta_element.GetName(), "theme-color") &&
        CSSParser::ParseColor(
            color, meta_element.Content().GetString().StripWhiteSpace(), true))
      return color;
  }
  return base::nullopt;
}

void Document::ColorSchemeMetaChanged() {
  if (!RuntimeEnabledFeatures::MetaColorSchemeEnabled())
    return;

  auto* root_element = documentElement();
  if (!root_element)
    return;

  const CSSValue* color_scheme = nullptr;
  for (HTMLMetaElement& meta_element :
       Traversal<HTMLMetaElement>::DescendantsOf(*root_element)) {
    if (EqualIgnoringASCIICase(meta_element.GetName(), "color-scheme")) {
      if ((color_scheme = CSSParser::ParseSingleValue(
               CSSPropertyID::kColorScheme,
               meta_element.Content().GetString().StripWhiteSpace(),
               ElementSheet().Contents()->ParserContext()))) {
        break;
      }
    }
  }
  GetStyleEngine().SetColorSchemeFromMeta(color_scheme);
}

static HTMLLinkElement* GetLinkElement(const Document* doc,
                                       bool (*match_fn)(HTMLLinkElement&)) {
  HTMLHeadElement* head = doc->head();
  if (!head)
    return nullptr;

  // The first matching link element is used. Others are ignored.
  for (HTMLLinkElement& link_element :
       Traversal<HTMLLinkElement>::ChildrenOf(*head)) {
    if (match_fn(link_element))
      return &link_element;
  }
  return nullptr;
}

HTMLLinkElement* Document::LinkManifest() const {
  return GetLinkElement(this, [](HTMLLinkElement& link_element) {
    return link_element.RelAttribute().IsManifest();
  });
}

HTMLLinkElement* Document::LinkCanonical() const {
  return GetLinkElement(this, [](HTMLLinkElement& link_element) {
    return link_element.RelAttribute().IsCanonical();
  });
}

void Document::FeaturePolicyInitialized(
    const DocumentInit& document_initializer,
    const SecurityContextInit& security_initializer) {
  // Processing of the feature policy header is done before the SecurityContext
  // is initialized. This method just records the usage.
  if (!document_initializer.FeaturePolicyHeader().IsEmpty())
    UseCounter::Count(*this, WebFeature::kFeaturePolicyHeader);
  for (const auto& message :
       security_initializer.FeaturePolicyParseMessages()) {
    AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kSecurity,
                               mojom::ConsoleMessageLevel::kError,
                               "Error with Feature-Policy header: " + message));
  }
  if (frame_) {
    pending_parsed_headers_ = security_initializer.ParsedHeader();
  }

  // At this point, the document will not have been installed in the frame's
  // LocalDOMWindow, so we cannot call frame_->IsFeatureEnabled. This calls
  // SecurityContext::IsFeatureEnabled instead, which cannot report, but we
  // don't need reporting here in any case.
  is_vertical_scroll_enforced_ =
      frame_ && !frame_->IsMainFrame() &&
      RuntimeEnabledFeatures::ExperimentalProductivityFeaturesEnabled() &&
      !GetFeaturePolicy()->IsFeatureEnabled(
          mojom::FeaturePolicyFeature::kVerticalScroll);
}

const ParsedFeaturePolicy Document::GetOwnerContainerPolicy() const {
  // If this frame is not the main frame, then get the container policy from its
  // owner.
  if (frame_ && frame_->Owner())
    return frame_->Owner()->GetFramePolicy().container_policy;
  return ParsedFeaturePolicy();
}

const FeaturePolicy* Document::GetParentFeaturePolicy() const {
  // If this frame is not the main frame, then get the feature policy from its
  // parent.
  if (frame_ && !frame_->IsMainFrame())
    return frame_->Tree().Parent()->GetSecurityContext()->GetFeaturePolicy();
  return nullptr;
}

void Document::ApplyPendingFeaturePolicyHeaders() {
  if (frame_) {
    frame_->Client()->DidSetFramePolicyHeaders(GetSandboxFlags(),
                                               pending_parsed_headers_);
  }
  pending_parsed_headers_.clear();
}

void Document::ApplyReportOnlyFeaturePolicyFromHeader(
    const String& feature_policy_report_only_header) {
  if (feature_policy_report_only_header.IsEmpty())
    return;
  // TODO(iclelland): Remove this message when reporting is no longer part of an
  // origin trial.
  // Note that we do not return here. Instead, the header is parsed and the
  // report-only policy is stored, in case a valid Origin Trial token is added
  // later. In that case, any subsequent violations will be correctly reported.
  if (!RuntimeEnabledFeatures::FeaturePolicyReportingEnabled(this)) {
    AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kWarning,
        "Feature-Policy-Report-Only header will have no effect unless Feature "
        "Policy reporting is enabled with an Origin Trial. Sign up at "
        "https://developers.chrome.com/origintrials/"));
  }

  UseCounter::Count(*this, WebFeature::kFeaturePolicyReportOnlyHeader);
  Vector<String> messages;
  const ParsedFeaturePolicy& report_only_policy =
      FeaturePolicyParser::ParseHeader(feature_policy_report_only_header,
                                       GetSecurityOrigin(), &messages, this);
  for (auto& message : messages) {
    AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        "Error with Feature-Policy-Report-Only header: " + message));
  }

  AddReportOnlyFeaturePolicy(report_only_policy, GetOwnerContainerPolicy(),
                             GetParentFeaturePolicy());
}

bool Document::AllowedToUseDynamicMarkUpInsertion(
    const char* api_name,
    ExceptionState& exception_state) {
  if (!RuntimeEnabledFeatures::ExperimentalProductivityFeaturesEnabled()) {
    return true;
  }
  if (!frame_ || IsFeatureEnabled(mojom::FeaturePolicyFeature::kDocumentWrite,
                                  ReportOptions::kReportOnFailure)) {
    return true;
  }

  // TODO(ekaramad): Throwing an exception seems an ideal resolution to mishaps
  // in using the API against the policy. But this cannot be applied to cross-
  // origin as there are security risks involved. We should perhaps unload the
  // whole frame instead of throwing.
  exception_state.ThrowDOMException(
      DOMExceptionCode::kNotAllowedError,
      String::Format(
          "The use of method '%s' has been blocked by feature policy. The "
          "feature "
          "'document-write' is disabled in this document.",
          api_name));
  return false;
}

ukm::UkmRecorder* Document::UkmRecorder() {
  if (ukm_recorder_)
    return ukm_recorder_.get();

  mojo::PendingRemote<ukm::mojom::UkmRecorderInterface> recorder;
  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      recorder.InitWithNewPipeAndPassReceiver());
  ukm_recorder_ = std::make_unique<ukm::MojoUkmRecorder>(std::move(recorder));

  // TODO(crbug/795354): Move handling of URL recording out of the renderer.
  // URL must only be recorded from the main frame.
  if (IsInMainFrame())
    ukm_recorder_->UpdateSourceURL(ukm_source_id_, url_);
  return ukm_recorder_.get();
}

ukm::SourceId Document::UkmSourceID() const {
  return ukm_source_id_;
}

void Document::InitContentSecurityPolicy(ContentSecurityPolicy* csp) {
  SetContentSecurityPolicy(csp);
  GetContentSecurityPolicy()->BindToDelegate(
      GetContentSecurityPolicyDelegate());
}

void Document::InitSecurityContext(
    const DocumentInit& initializer,
    const SecurityContextInit& security_initializer) {
  DCHECK(GetSecurityOrigin());

  // If the CSP was provided by the DocumentLoader or is from ImportsController
  // it doesn't need to be bound right now. ImportsController takes a reference
  // to a master document's CSP which is already bound. Document construction
  // occurs in the DocumentLoader occurs before the frame reference is bound so
  // callbacks from binding the CSP delegate immediately would not get called
  // if it was bound immediately. eg. Callbacks back to browser or console
  // logging.
  if (security_initializer.BindCSPImmediately()) {
    InitContentSecurityPolicy(security_initializer.GetCSP());
  } else {
    SetContentSecurityPolicy(security_initializer.GetCSP());
  }
  if (!initializer.HasSecurityContext()) {
    // No source for a security context.
    // This can occur via document.implementation.createDocument().
    cookie_url_ = KURL(g_empty_string);
    return;
  }
  SetInsecureRequestPolicy(initializer.GetInsecureRequestPolicy());
  if (initializer.InsecureNavigationsToUpgrade()) {
    for (auto to_upgrade : *initializer.InsecureNavigationsToUpgrade())
      AddInsecureNavigationUpgrade(to_upgrade);
  }

  bool inherit_cookie_url_from_owner = initializer.IsSrcdocDocument() ||
                                       url_.IsAboutBlankURL() ||
                                       !initializer.OriginToCommit();

  cookie_url_ = inherit_cookie_url_from_owner && initializer.OwnerDocument()
                    ? initializer.OwnerDocument()->CookieURL()
                    : url_;

  SetAddressSpace(initializer.GetIPAddressSpace());

  if (GetSecurityOrigin()->IsOpaque() &&
      SecurityOrigin::Create(url_)->IsPotentiallyTrustworthy())
    GetMutableSecurityOrigin()->SetOpaqueOriginIsPotentiallyTrustworthy(true);

  InitSecureContextState();
}

void Document::SetSecurityOrigin(scoped_refptr<SecurityOrigin> origin) {
  // Enforce that we don't change access, we might change the reference (via
  // IsolatedCopy but we can't change the security policy).
  CHECK(origin);
  CHECK(GetSecurityOrigin()->CanAccess(origin.get()));
  SecurityContext::SetSecurityOrigin(origin);
}

void Document::BindContentSecurityPolicy() {
  DCHECK(!GetContentSecurityPolicy()->IsBound());
  GetContentSecurityPolicy()->BindToDelegate(
      GetContentSecurityPolicyDelegate());
}

void Document::InitSecureContextState() {
  DCHECK_EQ(secure_context_state_, SecureContextState::kUnknown);
  if (!GetSecurityOrigin()->IsPotentiallyTrustworthy()) {
    secure_context_state_ = SecureContextState::kNonSecure;
  } else if (SchemeRegistry::SchemeShouldBypassSecureContextCheck(
                 GetSecurityOrigin()->Protocol())) {
    secure_context_state_ = SecureContextState::kSecure;
  } else if (frame_) {
    Frame* parent = frame_->Tree().Parent();
    while (parent) {
      if (!parent->GetSecurityContext()
               ->GetSecurityOrigin()
               ->IsPotentiallyTrustworthy()) {
        secure_context_state_ = SecureContextState::kNonSecure;
        break;
      }
      parent = parent->Tree().Parent();
    }
    if (secure_context_state_ == SecureContextState::kUnknown)
      secure_context_state_ = SecureContextState::kSecure;
  } else {
    secure_context_state_ = SecureContextState::kNonSecure;
  }
  DCHECK_NE(secure_context_state_, SecureContextState::kUnknown);
}

bool Document::CanExecuteScripts(ReasonForCallingCanExecuteScripts reason) {
  DCHECK(GetFrame())
      << "you are querying canExecuteScripts on a non contextDocument.";

  // Normally, scripts are not allowed in sandboxed contexts that disallow them.
  // However, there is an exception for cases when the script should bypass the
  // main world's CSP (such as for privileged isolated worlds). See
  // https://crbug.com/811528.
  if (IsSandboxed(WebSandboxFlags::kScripts) &&
      !ContentSecurityPolicy::ShouldBypassMainWorld(this)) {
    // FIXME: This message should be moved off the console once a solution to
    // https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
    if (reason == kAboutToExecuteScript) {
      AddConsoleMessage(ConsoleMessage::Create(
          mojom::ConsoleMessageSource::kSecurity,
          mojom::ConsoleMessageLevel::kError,
          "Blocked script execution in '" + Url().ElidedString() +
              "' because the document's frame is sandboxed and the "
              "'allow-scripts' permission is not set."));
    }
    return false;
  }

  // No scripting on a detached frame.
  if (!GetFrame()->Client())
    return false;

  WebContentSettingsClient* settings_client =
      GetFrame()->GetContentSettingsClient();

  Settings* settings = GetFrame()->GetSettings();
  bool script_enabled = settings && settings->GetScriptEnabled();
  if (settings_client)
    script_enabled = settings_client->AllowScript(script_enabled);
  if (!script_enabled && reason == kAboutToExecuteScript && settings_client)
    settings_client->DidNotAllowScript();
  return script_enabled;
}

bool Document::AllowInlineEventHandler(Node* node,
                                       EventListener* listener,
                                       const String& context_url,
                                       const WTF::OrdinalNumber& context_line) {
  auto* element = DynamicTo<Element>(node);

  // https://html.spec.whatwg.org/multipage/webappapis.html#event-handler-content-attributes
  // Step 5.1. If the Should element's inline behavior be blocked by Content
  // Security Policy? algorithm returns "Blocked" when executed upon element,
  // "script attribute", and value, then return. [CSP] [spec text]
  if (!GetContentSecurityPolicyForWorld()->AllowInline(
          ContentSecurityPolicy::InlineType::kScriptAttribute, element,
          listener->ScriptBody(), String() /* nonce */, context_url,
          context_line))
    return false;

  // HTML says that inline script needs browsing context to create its execution
  // environment.
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/webappapis.html#event-handler-attributes
  // Also, if the listening node came from other document, which happens on
  // context-less event dispatching, we also need to ask the owner document of
  // the node.
  LocalFrame* frame = ExecutingFrame();
  if (!frame)
    return false;
  if (!ContextDocument()->CanExecuteScripts(kNotAboutToExecuteScript))
    return false;
  if (node && node->GetDocument() != this &&
      !node->GetDocument().AllowInlineEventHandler(node, listener, context_url,
                                                   context_line))
    return false;

  return true;
}

bool Document::IsContextThread() const {
  return IsMainThread();
}

void Document::UpdateFocusAppearanceAfterLayout() {
  update_focus_appearance_after_layout_ = true;
}

void Document::CancelFocusAppearanceUpdate() {
  update_focus_appearance_after_layout_ = false;
}

bool Document::WillUpdateFocusAppearance() const {
  return update_focus_appearance_after_layout_;
}

void Document::UpdateFocusAppearance() {
  update_focus_appearance_after_layout_ = false;
  Element* element = FocusedElement();
  if (!element)
    return;
  if (element->IsFocusable())
    element->UpdateFocusAppearance(SelectionBehaviorOnFocus::kRestore);
}

void Document::AttachRange(Range* range) {
  DCHECK(!ranges_.Contains(range));
  ranges_.insert(range);
}

void Document::DetachRange(Range* range) {
  // We don't DCHECK ranges_.contains(range) to allow us to call this
  // unconditionally to fix: https://bugs.webkit.org/show_bug.cgi?id=26044
  ranges_.erase(range);
}

void Document::InitDNSPrefetch() {
  Settings* settings = GetSettings();

  have_explicitly_disabled_dns_prefetch_ = false;
  is_dns_prefetch_enabled_ = settings && settings->GetDNSPrefetchingEnabled() &&
                             GetSecurityOrigin()->Protocol() == "http";

  // Inherit DNS prefetch opt-out from parent frame
  if (Document* parent = ParentDocument()) {
    if (!parent->IsDNSPrefetchEnabled())
      is_dns_prefetch_enabled_ = false;
  }
}

void Document::ParseDNSPrefetchControlHeader(
    const String& dns_prefetch_control) {
  if (DeprecatedEqualIgnoringCase(dns_prefetch_control, "on") &&
      !have_explicitly_disabled_dns_prefetch_) {
    is_dns_prefetch_enabled_ = true;
    return;
  }

  is_dns_prefetch_enabled_ = false;
  have_explicitly_disabled_dns_prefetch_ = true;
}

IntersectionObserverController* Document::GetIntersectionObserverController() {
  return intersection_observer_controller_;
}

IntersectionObserverController&
Document::EnsureIntersectionObserverController() {
  if (!intersection_observer_controller_) {
    intersection_observer_controller_ =
        MakeGarbageCollected<IntersectionObserverController>(this);
  }
  return *intersection_observer_controller_;
}

ResizeObserverController& Document::EnsureResizeObserverController() {
  if (!resize_observer_controller_) {
    resize_observer_controller_ =
        MakeGarbageCollected<ResizeObserverController>();
  }
  return *resize_observer_controller_;
}

static void RunAddConsoleMessageTask(mojom::ConsoleMessageSource source,
                                     mojom::ConsoleMessageLevel level,
                                     const String& message,
                                     ExecutionContext* context,
                                     bool discard_duplicates) {
  ConsoleMessage* console_message =
      ConsoleMessage::Create(source, level, message);
  context->AddConsoleMessage(console_message, discard_duplicates);
}

void Document::AddConsoleMessageImpl(ConsoleMessage* console_message,
                                     bool discard_duplicates) {
  if (!IsContextThread()) {
    PostCrossThreadTask(
        *GetTaskRunner(TaskType::kInternalInspector), FROM_HERE,
        CrossThreadBindOnce(
            &RunAddConsoleMessageTask, console_message->Source(),
            console_message->Level(), console_message->Message(),
            WrapCrossThreadPersistent(this), discard_duplicates));
    return;
  }

  if (!frame_) {
    if (imports_controller_) {
      imports_controller_->Master()->GetFrame()->Console().AddMessage(
          console_message);
    }
    return;
  }

  if (console_message->Location()->IsUnknown()) {
    // TODO(dgozman): capture correct location at call places instead.
    unsigned line_number = 0;
    if (!IsInDocumentWrite() && GetScriptableDocumentParser()) {
      ScriptableDocumentParser* parser = GetScriptableDocumentParser();
      if (parser->IsParsingAtLineNumber())
        line_number = parser->LineNumber().OneBasedInt();
    }
    Vector<DOMNodeId> nodes(console_message->Nodes());
    console_message = ConsoleMessage::Create(
        console_message->Source(), console_message->Level(),
        console_message->Message(),
        std::make_unique<SourceLocation>(Url().GetString(), line_number, 0,
                                         nullptr));
    console_message->SetNodes(frame_, std::move(nodes));
  }

  frame_->Console().AddMessage(console_message, discard_duplicates);
}

void Document::TasksWerePaused() {
  GetScriptRunner()->Suspend();

  if (parser_)
    parser_->PauseScheduledTasks();
  if (scripted_animation_controller_)
    scripted_animation_controller_->Pause();
}

void Document::TasksWereUnpaused() {
  GetScriptRunner()->Resume();

  if (parser_)
    parser_->UnpauseScheduledTasks();
  if (scripted_animation_controller_)
    scripted_animation_controller_->Unpause();

  MutationObserver::ResumeSuspendedObservers();
  if (dom_window_)
    DOMWindowPerformance::performance(*dom_window_)->ResumeSuspendedObservers();
}

bool Document::TasksNeedPause() {
  Page* page = GetPage();
  return page && page->Paused();
}

void Document::AddToTopLayer(Element* element, const Element* before) {
  if (element->IsInTopLayer())
    return;

  DCHECK(!top_layer_elements_.Contains(element));
  DCHECK(!before || top_layer_elements_.Contains(before));
  if (before) {
    wtf_size_t before_position = top_layer_elements_.Find(before);
    top_layer_elements_.insert(before_position, element);
  } else {
    top_layer_elements_.push_back(element);
  }
  element->SetIsInTopLayer(true);
}

void Document::RemoveFromTopLayer(Element* element) {
  if (!element->IsInTopLayer())
    return;
  wtf_size_t position = top_layer_elements_.Find(element);
  DCHECK_NE(position, kNotFound);
  top_layer_elements_.EraseAt(position);
  element->SetIsInTopLayer(false);
}

HTMLDialogElement* Document::ActiveModalDialog() const {
  for (auto it = top_layer_elements_.rbegin(); it != top_layer_elements_.rend();
       ++it) {
    if (auto* dialog = DynamicTo<HTMLDialogElement>(*it->Get()))
      return dialog;
  }

  return nullptr;
}

void Document::exitPointerLock() {
  if (!GetPage())
    return;
  if (Element* target = GetPage()->GetPointerLockController().GetElement()) {
    if (target->GetDocument() != this)
      return;
    GetPage()->GetPointerLockController().RequestPointerUnlock();
  }
}

Element* Document::PointerLockElement() const {
  if (!GetPage() || GetPage()->GetPointerLockController().LockPending())
    return nullptr;
  if (Element* element = GetPage()->GetPointerLockController().GetElement()) {
    if (element->GetDocument() == this)
      return element;
  }
  return nullptr;
}

void Document::DecrementLoadEventDelayCount() {
  DCHECK(load_event_delay_count_);
  --load_event_delay_count_;

  if (!load_event_delay_count_)
    CheckLoadEventSoon();
}

void Document::DecrementLoadEventDelayCountAndCheckLoadEvent() {
  DCHECK(load_event_delay_count_);
  --load_event_delay_count_;

  if (!load_event_delay_count_)
    CheckCompleted();
}

void Document::CheckLoadEventSoon() {
  if (GetFrame() && !load_event_delay_timer_.IsActive())
    load_event_delay_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

bool Document::IsDelayingLoadEvent() {
  // Always delay load events until after garbage collection.
  // This way we don't have to explicitly delay load events via
  // incrementLoadEventDelayCount and decrementLoadEventDelayCount in
  // Node destructors.
  if (ThreadState::Current()->SweepForbidden()) {
    if (!load_event_delay_count_)
      CheckLoadEventSoon();
    return true;
  }
  return load_event_delay_count_;
}

void Document::LoadEventDelayTimerFired(TimerBase*) {
  CheckCompleted();
}

void Document::LoadPluginsSoon() {
  // FIXME: Remove this timer once we don't need to compute layout to load
  // plugins.
  if (!plugin_loading_timer_.IsActive())
    plugin_loading_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void Document::PluginLoadingTimerFired(TimerBase*) {
  UpdateStyleAndLayout();
}

ScriptedAnimationController& Document::EnsureScriptedAnimationController() {
  if (!scripted_animation_controller_) {
    scripted_animation_controller_ =
        MakeGarbageCollected<ScriptedAnimationController>(this);
    // We need to make sure that we don't start up the animation controller on a
    // background tab, for example.
    if (!GetPage())
      scripted_animation_controller_->Pause();
  }
  return *scripted_animation_controller_;
}

int Document::RequestAnimationFrame(
    FrameRequestCallbackCollection::FrameCallback* callback) {
  return EnsureScriptedAnimationController().RegisterFrameCallback(callback);
}

void Document::CancelAnimationFrame(int id) {
  if (!scripted_animation_controller_)
    return;
  scripted_animation_controller_->CancelFrameCallback(id);
}

void Document::ServiceScriptedAnimations(
    base::TimeTicks monotonic_animation_start_time) {
  if (!scripted_animation_controller_)
    return;
  auto start_time = base::TimeTicks::Now();
  scripted_animation_controller_->ServiceScriptedAnimations(
      monotonic_animation_start_time);
  if (GetFrame()) {
    GetFrame()->GetFrameScheduler()->AddTaskTime(base::TimeTicks::Now() -
                                                 start_time);
  }
}

int Document::RequestPostAnimationFrame(
    FrameRequestCallbackCollection::FrameCallback* cb) {
  return EnsureScriptedAnimationController().RegisterPostFrameCallback(cb);
}

void Document::CancelPostAnimationFrame(int id) {
  if (scripted_animation_controller_)
    scripted_animation_controller_->CancelPostFrameCallback(id);
}

void Document::RunPostAnimationFrameCallbacks() {
  bool was_throttled = current_frame_is_throttled_;
  current_frame_is_throttled_ = false;
  if (was_throttled || !scripted_animation_controller_)
    return;
  scripted_animation_controller_->RunPostFrameCallbacks();
}

ScriptedIdleTaskController& Document::EnsureScriptedIdleTaskController() {
  if (!scripted_idle_task_controller_) {
    scripted_idle_task_controller_ = ScriptedIdleTaskController::Create(this);
    // We need to make sure that we don't start up the idle controller if we
    // don't have an attached frame and if execution context is destroyed.
    if (!frame_ || !frame_->IsAttached() ||
        ExecutionContext::IsContextDestroyed()) {
      scripted_idle_task_controller_->ContextLifecycleStateChanged(
          mojom::FrameLifecycleState::kFrozen);
    }
  }
  return *scripted_idle_task_controller_;
}

int Document::RequestIdleCallback(
    ScriptedIdleTaskController::IdleTask* idle_task,
    const IdleRequestOptions* options) {
  return EnsureScriptedIdleTaskController().RegisterCallback(idle_task,
                                                             options);
}

void Document::CancelIdleCallback(int id) {
  if (!scripted_idle_task_controller_)
    return;
  scripted_idle_task_controller_->CancelCallback(id);
}

DocumentLoader* Document::Loader() const {
  if (!frame_)
    return nullptr;

  // TODO(dcheng): remove this check. frame_ is guaranteed to be non-null only
  // if frame_->GetDocument() == this.
  if (frame_->GetDocument() != this)
    return nullptr;

  return frame_->Loader().GetDocumentLoader();
}

Node* EventTargetNodeForDocument(Document* doc) {
  if (!doc)
    return nullptr;
  Node* node = doc->FocusedElement();
  if (!node && doc->IsPluginDocument()) {
    PluginDocument* plugin_document = ToPluginDocument(doc);
    node = plugin_document->PluginNode();
  }
  if (!node && doc->IsHTMLDocument())
    node = doc->body();
  if (!node)
    node = doc->documentElement();
  return node;
}

void Document::AdjustFloatQuadsForScrollAndAbsoluteZoom(
    Vector<FloatQuad>& quads,
    const LayoutObject& layout_object) const {
  if (!View())
    return;

  for (wtf_size_t i = 0; i < quads.size(); ++i) {
    AdjustForAbsoluteZoom::AdjustFloatQuad(quads[i], layout_object);
  }
}

void Document::AdjustFloatRectForScrollAndAbsoluteZoom(
    FloatRect& rect,
    const LayoutObject& layout_object) const {
  if (!View())
    return;

  AdjustForAbsoluteZoom::AdjustFloatRect(rect, layout_object);
}

void Document::SetThreadedParsingEnabledForTesting(bool enabled) {
  g_threaded_parsing_enabled_for_testing = enabled;
}

bool Document::ThreadedParsingEnabledForTesting() {
  return g_threaded_parsing_enabled_for_testing;
}

SnapCoordinator& Document::GetSnapCoordinator() {
  if (!snap_coordinator_)
    snap_coordinator_ = MakeGarbageCollected<SnapCoordinator>();

  return *snap_coordinator_;
}

void Document::SetContextFeatures(ContextFeatures& features) {
  context_features_ = &features;
}

void Document::UpdateHoverActiveState(bool is_active,
                                      bool update_active_chain,
                                      Element* inner_element) {
  if (is_active && frame_)
    frame_->GetEventHandler().NotifyElementActivated();

  Element* inner_element_in_document = inner_element;

  while (inner_element_in_document &&
         inner_element_in_document->GetDocument() != this) {
    inner_element_in_document->GetDocument().UpdateHoverActiveState(
        is_active, update_active_chain, inner_element_in_document);
    inner_element_in_document =
        inner_element_in_document->GetDocument().LocalOwner();
  }

  UpdateDistributionForFlatTreeTraversal();

  UpdateActiveState(is_active, update_active_chain, inner_element_in_document);
  UpdateHoverState(inner_element_in_document);
}

void Document::UpdateActiveState(bool is_active,
                                 bool update_active_chain,
                                 Element* inner_element_in_document) {
  Element* old_active_element = GetActiveElement();
  if (old_active_element && !is_active) {
    // The oldActiveElement layoutObject is null, dropped on :active by setting
    // display: none, for instance. We still need to clear the ActiveChain as
    // the mouse is released.
    for (Element* element = old_active_element; element;
         element = FlatTreeTraversal::ParentElement(*element)) {
      element->SetActive(false);
      user_action_elements_.SetInActiveChain(element, false);
    }
    SetActiveElement(nullptr);
  } else {
    Element* new_active_element = inner_element_in_document;
    if (!old_active_element && new_active_element &&
        !new_active_element->IsDisabledFormControl() && is_active) {
      // We are setting the :active chain and freezing it. If future moves
      // happen, they will need to reference this chain.
      for (Element* element = new_active_element; element;
           element = FlatTreeTraversal::ParentElement(*element)) {
        user_action_elements_.SetInActiveChain(element, true);
      }
      SetActiveElement(new_active_element);
    }
  }

  // If the mouse has just been pressed, set :active on the chain. Those (and
  // only those) nodes should remain :active until the mouse is released.
  bool allow_active_changes = !old_active_element && GetActiveElement();
  if (!allow_active_changes)
    return;

  DCHECK(is_active);

  Element* new_element = SkipDisplayNoneAncestors(inner_element_in_document);

  // Now set the active state for our new object up to the root.  If the mouse
  // is down and if this is a mouse move event, we want to restrict changes in
  // :active to only apply to elements that are in the :active chain that we
  // froze at the time the mouse went down.
  for (Element* curr = new_element; curr;
       curr = FlatTreeTraversal::ParentElement(*curr)) {
    if (update_active_chain || curr->InActiveChain())
      curr->SetActive(true);
  }
}

void Document::UpdateHoverState(Element* inner_element_in_document) {
  Element* old_hover_element = HoverElement();

  // The passed in innerElement may not be a result of a hit test for the
  // current up-to-date flat/layout tree. That means the element may be
  // display:none at this point. Skip up the ancestor chain until we reach an
  // element with a layoutObject or a display:contents element.
  Element* new_hover_element =
      SkipDisplayNoneAncestors(inner_element_in_document);

  // Update our current hover element.
  SetHoverElement(new_hover_element);

  if (old_hover_element == new_hover_element)
    return;

  Node* ancestor_element = nullptr;
  if (old_hover_element && old_hover_element->isConnected() &&
      new_hover_element) {
    Node* ancestor = FlatTreeTraversal::CommonAncestor(*old_hover_element,
                                                       *new_hover_element);
    if (auto* element = DynamicTo<Element>(ancestor))
      ancestor_element = element;
  }

  HeapVector<Member<Element>, 32> elements_to_remove_from_chain;
  HeapVector<Member<Element>, 32> elements_to_add_to_hover_chain;

  // The old hover path only needs to be cleared up to (and not including) the
  // common ancestor;
  //
  // TODO(emilio): old_hover_element may be disconnected from the tree already.
  if (old_hover_element && old_hover_element->isConnected()) {
    for (Element* curr = old_hover_element; curr && curr != ancestor_element;
         curr = FlatTreeTraversal::ParentElement(*curr)) {
      elements_to_remove_from_chain.push_back(curr);
    }
  }

  // Now set the hover state for our new object up to the root.
  for (Element* curr = new_hover_element; curr;
       curr = FlatTreeTraversal::ParentElement(*curr)) {
    elements_to_add_to_hover_chain.push_back(curr);
  }

  for (Element* element : elements_to_remove_from_chain)
    element->SetHovered(false);

  bool saw_common_ancestor = false;
  for (Element* element : elements_to_add_to_hover_chain) {
    if (element == ancestor_element)
      saw_common_ancestor = true;
    if (!saw_common_ancestor || element == hover_element_)
      element->SetHovered(true);
  }
}

bool Document::HaveScriptBlockingStylesheetsLoaded() const {
  return style_engine_->HaveScriptBlockingStylesheetsLoaded();
}

bool Document::HaveRenderBlockingResourcesLoaded() const {
  return HaveImportsLoaded() &&
         style_engine_->HaveRenderBlockingStylesheetsLoaded();
}

Locale& Document::GetCachedLocale(const AtomicString& locale) {
  AtomicString locale_key = locale;
  if (locale.IsEmpty() ||
      !RuntimeEnabledFeatures::LangAttributeAwareFormControlUIEnabled())
    return Locale::DefaultLocale();
  LocaleIdentifierToLocaleMap::AddResult result =
      locale_cache_.insert(locale_key, nullptr);
  if (result.is_new_entry)
    result.stored_value->value = Locale::Create(locale_key);
  return *(result.stored_value->value);
}

AnimationClock& Document::GetAnimationClock() {
  DCHECK(GetPage());
  return GetPage()->Animator().Clock();
}

const AnimationClock& Document::GetAnimationClock() const {
  DCHECK(GetPage());
  return GetPage()->Animator().Clock();
}

Document& Document::EnsureTemplateDocument() {
  if (IsTemplateDocument())
    return *this;

  if (template_document_)
    return *template_document_;

  if (IsHTMLDocument()) {
    template_document_ = MakeGarbageCollected<HTMLDocument>(
        DocumentInit::Create()
            .WithContextDocument(ContextDocument())
            .WithURL(BlankURL())
            .WithNewRegistrationContext());
  } else {
    template_document_ = MakeGarbageCollected<Document>(
        DocumentInit::Create().WithURL(BlankURL()));
  }

  template_document_->template_document_host_ = this;  // balanced in dtor.

  return *template_document_.Get();
}

void Document::DidAssociateFormControl(Element* element) {
  if (!GetFrame() || !GetFrame()->GetPage() || !HasFinishedParsing())
    return;

  // We add a slight delay because this could be called rapidly.
  if (!did_associate_form_controls_timer_.IsActive()) {
    did_associate_form_controls_timer_.StartOneShot(
        base::TimeDelta::FromMilliseconds(300), FROM_HERE);
  }
}

void Document::DidAssociateFormControlsTimerFired(TimerBase* timer) {
  DCHECK_EQ(timer, &did_associate_form_controls_timer_);
  if (!GetFrame() || !GetFrame()->GetPage())
    return;

  GetFrame()->GetPage()->GetChromeClient().DidAssociateFormControlsAfterLoad(
      GetFrame());
}

float Document::DevicePixelRatio() const {
  return frame_ ? frame_->DevicePixelRatio() : 1.0;
}

TextAutosizer* Document::GetTextAutosizer() {
  if (!text_autosizer_)
    text_autosizer_ = MakeGarbageCollected<TextAutosizer>(this);
  return text_autosizer_.Get();
}

bool Document::SetPseudoStateForTesting(Element& element,
                                        const String& pseudo,
                                        bool matches) {
  DCHECK(WebTestSupport::IsRunningWebTest());
  auto& set = UserActionElements();
  if (pseudo == ":focus") {
    set.SetFocused(&element, matches);
    element.PseudoStateChanged(CSSSelector::kPseudoFocus);
  } else if (pseudo == ":focus-within") {
    set.SetHasFocusWithin(&element, matches);
    element.PseudoStateChanged(CSSSelector::kPseudoFocusWithin);
  } else if (pseudo == ":active") {
    set.SetActive(&element, matches);
    element.PseudoStateChanged(CSSSelector::kPseudoActive);
  } else if (pseudo == ":hover") {
    set.SetHovered(&element, matches);
    element.PseudoStateChanged(CSSSelector::kPseudoHover);
  } else {
    return false;
  }
  return true;
}

void Document::EnqueueAutofocusCandidate(Element& element) {
  // https://html.spec.whatwg.org/C#the-autofocus-attribute
  // 7. If topDocument's autofocus processed flag is false, then remove the
  // element from topDocument's autofocus candidates, and append the element
  // to topDocument's autofocus candidates.
  if (autofocus_processed_flag_)
    return;
  wtf_size_t index = autofocus_candidates_.Find(&element);
  if (index != WTF::kNotFound)
    autofocus_candidates_.EraseAt(index);
  autofocus_candidates_.push_back(element);
  // ScriptedAnimationController invokes FlushAutofocusCandidates().
  EnsureScriptedAnimationController();
}

bool Document::HasAutofocusCandidates() const {
  return autofocus_candidates_.size() > 0;
}

// https://html.spec.whatwg.org/C/#flush-autofocus-candidates
void Document::FlushAutofocusCandidates() {
  // 1. If topDocument's autofocus processed flag is true, then return.
  if (autofocus_processed_flag_)
    return;

  // 3. If candidates is empty, then return.
  if (autofocus_candidates_.IsEmpty())
    return;

  // 4. If topDocument's focused area is not topDocument itself, or
  //    topDocument's URL's fragment is not empty, then:
  //  1. Empty candidates.
  //  2. Set topDocument's autofocus processed flag to true.
  //  3. Return.
  if (AdjustedFocusedElement()) {
    autofocus_candidates_.clear();
    autofocus_processed_flag_ = true;
    AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kRendering,
                               mojom::ConsoleMessageLevel::kInfo,
                               "Autofocus processing was blocked because a "
                               "document already has a focused element."));
    return;
  }
  if (HasNonEmptyFragment()) {
    autofocus_candidates_.clear();
    autofocus_processed_flag_ = true;
    AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kRendering,
                               mojom::ConsoleMessageLevel::kInfo,
                               "Autofocus processing was blocked because a "
                               "document's URL has a fragment '#" +
                                   Url().FragmentIdentifier() + "'."));
    return;
  }

  // 5. While candidates is not empty:
  while (!autofocus_candidates_.IsEmpty()) {
    // 5.1. Let element be candidates[0].
    Element& element = *autofocus_candidates_[0];

    // 5.2. Let doc be element's node document.
    Document* doc = &element.GetDocument();

    // 5.3. If doc is not fully active, then remove element from candidates,
    // and continue.
    // 5.4. If doc's browsing context's top-level browsing context is not same
    // as topDocument's browsing context, then remove element from candidates,
    // and continue.
    if (&doc->TopDocument() != this) {
      autofocus_candidates_.EraseAt(0);
      continue;
    }

    // The element is in the fallback content of an OBJECT of which
    // fallback state is not fixed yet.
    // TODO(tkent): Standardize this behavior.
    if (IsInIndeterminateObjectAncestor(&element)) {
      return;
    }

    // 5.5. If doc's script-blocking style sheet counter is greater than 0,
    // then return.
    // TODO(tkent): Is this necessary? WPT spin-by-blocking-style-sheet.html
    // doesn't hit this condition, and FlushAutofocusCandidates() is not called
    // until the stylesheet is loaded.
    StyleEngine& engine = GetStyleEngine();
    if (engine.HasPendingScriptBlockingSheets() ||
        engine.HasPendingRenderBlockingSheets()) {
      return;
    }

    // 5.6. Remove element from candidates.
    autofocus_candidates_.EraseAt(0);

    // 5.7. Let inclusiveAncestorDocuments be a list consisting of doc, plus
    // the active documents of each of doc's browsing context's ancestor
    // browsing contexts.
    // 5.8. If URL's fragment of any Document in inclusiveAncestorDocuments
    // is not empty, then continue.
    if (doc != this) {
      for (HTMLFrameOwnerElement* frameOwner = doc->LocalOwner();
           !doc->HasNonEmptyFragment() && frameOwner;
           frameOwner = doc->LocalOwner()) {
        doc = &frameOwner->GetDocument();
      }
      if (doc->HasNonEmptyFragment()) {
        AddConsoleMessage(
            ConsoleMessage::Create(mojom::ConsoleMessageSource::kRendering,
                                   mojom::ConsoleMessageLevel::kInfo,
                                   "Autofocus processing was blocked because a "
                                   "document's URL has a fragment '#" +
                                       doc->Url().FragmentIdentifier() + "'."));
        continue;
      }
      DCHECK_EQ(doc, this);
    }

    // 9. Let target be element.
    Element* target = &element;

    // 10. If target is not a focusable area, then set target to the result of
    // getting the focusable area for target.
    element.GetDocument().UpdateStyleAndLayoutTree();
    if (!target->IsFocusable())
      target = target->GetFocusableArea();

    // 11. If target is not null, then:
    if (target) {
      // 11.1. Empty candidates.
      // 11.2. Set topDocument's autofocus processed flag to true.
      FinalizeAutofocus();
      // 11.3. Run the focusing steps for element.
      element.focus();
    } else {
      // TODO(tkent): Show a console message, and fix LocalNTP*Test.*
      // in browser_tests.
    }
  }
}

void Document::FinalizeAutofocus() {
  autofocus_candidates_.clear();
  autofocus_processed_flag_ = true;
}

bool Document::HasNonEmptyFragment() const {
  return !Url().FragmentIdentifier().IsEmpty();
}

Element* Document::ActiveElement() const {
  if (Element* element = AdjustedFocusedElement())
    return element;
  return body();
}

bool Document::hasFocus() const {
  return GetPage() && GetPage()->GetFocusController().IsDocumentFocused(*this);
}

const AtomicString& Document::BodyAttributeValue(
    const QualifiedName& name) const {
  if (auto* bodyElement = body())
    return bodyElement->FastGetAttribute(name);
  return g_null_atom;
}

void Document::SetBodyAttribute(const QualifiedName& name,
                                const AtomicString& value) {
  if (auto* bodyElement = body()) {
    // FIXME: This check is apparently for benchmarks that set the same value
    // repeatedly.  It's not clear what benchmarks though, it's also not clear
    // why we don't avoid causing a style recalc when setting the same value to
    // a presentational attribute in the common case.
    if (bodyElement->FastGetAttribute(name) != value)
      bodyElement->setAttribute(name, value);
  }
}

const AtomicString& Document::bgColor() const {
  return BodyAttributeValue(html_names::kBgcolorAttr);
}

void Document::setBgColor(const AtomicString& value) {
  if (!IsFrameSet())
    SetBodyAttribute(html_names::kBgcolorAttr, value);
}

const AtomicString& Document::fgColor() const {
  return BodyAttributeValue(html_names::kTextAttr);
}

void Document::setFgColor(const AtomicString& value) {
  if (!IsFrameSet())
    SetBodyAttribute(html_names::kTextAttr, value);
}

const AtomicString& Document::alinkColor() const {
  return BodyAttributeValue(html_names::kAlinkAttr);
}

void Document::setAlinkColor(const AtomicString& value) {
  if (!IsFrameSet())
    SetBodyAttribute(html_names::kAlinkAttr, value);
}

const AtomicString& Document::linkColor() const {
  return BodyAttributeValue(html_names::kLinkAttr);
}

void Document::setLinkColor(const AtomicString& value) {
  if (!IsFrameSet())
    SetBodyAttribute(html_names::kLinkAttr, value);
}

const AtomicString& Document::vlinkColor() const {
  return BodyAttributeValue(html_names::kVlinkAttr);
}

void Document::setVlinkColor(const AtomicString& value) {
  if (!IsFrameSet())
    SetBodyAttribute(html_names::kVlinkAttr, value);
}

template <unsigned type>
bool ShouldInvalidateNodeListCachesForAttr(
    const LiveNodeListRegistry& node_lists,
    const QualifiedName& attr_name) {
  auto invalidation_type = static_cast<NodeListInvalidationType>(type);
  if (node_lists.ContainsInvalidationType(invalidation_type) &&
      LiveNodeListBase::ShouldInvalidateTypeOnAttributeChange(invalidation_type,
                                                              attr_name))
    return true;
  return ShouldInvalidateNodeListCachesForAttr<type + 1>(node_lists, attr_name);
}

template <>
bool ShouldInvalidateNodeListCachesForAttr<kNumNodeListInvalidationTypes>(
    const LiveNodeListRegistry&,
    const QualifiedName&) {
  return false;
}

bool Document::ShouldInvalidateNodeListCaches(
    const QualifiedName* attr_name) const {
  if (attr_name) {
    return ShouldInvalidateNodeListCachesForAttr<
        kDoNotInvalidateOnAttributeChanges + 1>(node_lists_, *attr_name);
  }

  // If the invalidation is not for an attribute, invalidation is needed if
  // there is any node list present (with any invalidation type).
  return !node_lists_.IsEmpty();
}

void Document::InvalidateNodeListCaches(const QualifiedName* attr_name) {
  for (const LiveNodeListBase* list : lists_invalidated_at_document_)
    list->InvalidateCacheForAttribute(attr_name);
}

void Document::PlatformColorsChanged() {
  if (!IsActive())
    return;

  GetStyleEngine().PlatformColorsChanged();
}

bool Document::IsSecureContext(String& error_message) const {
  if (!IsSecureContext()) {
    error_message = SecurityOrigin::IsPotentiallyTrustworthyErrorMessage();
    return false;
  }
  return true;
}

bool Document::IsSecureContext() const {
  bool is_secure = secure_context_state_ == SecureContextState::kSecure;
  if (GetSandboxFlags() != WebSandboxFlags::kNone) {
    CountUse(is_secure
                 ? WebFeature::kSecureContextCheckForSandboxedOriginPassed
                 : WebFeature::kSecureContextCheckForSandboxedOriginFailed);
  }
  CountUse(is_secure ? WebFeature::kSecureContextCheckPassed
                     : WebFeature::kSecureContextCheckFailed);
  return is_secure;
}

void Document::DidEnforceInsecureRequestPolicy() {
  if (!GetFrame())
    return;
  GetFrame()->GetLocalFrameHostRemote().EnforceInsecureRequestPolicy(
      GetInsecureRequestPolicy());
}

void Document::DidEnforceInsecureNavigationsSet() {
  if (!GetFrame())
    return;
  GetFrame()->GetLocalFrameHostRemote().EnforceInsecureNavigationsSet(
      SecurityContext::SerializeInsecureNavigationSet(
          InsecureNavigationsToUpgrade()));
}

void Document::CountDetachingNodeAccessInDOMNodeRemovedHandler() {
  auto state = GetInDOMNodeRemovedHandlerState();
  DCHECK_NE(state, InDOMNodeRemovedHandlerState::kNone);
  UseCounter::Count(
      *this,
      state == InDOMNodeRemovedHandlerState::kDOMNodeRemoved
          ? WebFeature::kDOMNodeRemovedEventHandlerAccessDetachingNode
          : WebFeature::
                kDOMNodeRemovedFromDocumentEventHandlerAccessDetachingNode);
}

void Document::SetShadowCascadeOrder(ShadowCascadeOrder order) {
  DCHECK_NE(order, ShadowCascadeOrder::kShadowCascadeNone);
  if (order == shadow_cascade_order_)
    return;

  if (order == ShadowCascadeOrder::kShadowCascadeV0) {
    may_contain_v0_shadow_ = true;
    if (shadow_cascade_order_ == ShadowCascadeOrder::kShadowCascadeV1) {
      // ::slotted() rules has to be moved to tree boundary rule sets.
      style_engine_->V0ShadowAddedOnV1Document();
      UseCounter::Count(*this, WebFeature::kMixedShadowRootV0AndV1);
    }
  }

  // For V0 -> V1 upgrade, we need style recalculation for all elements.
  if (shadow_cascade_order_ == ShadowCascadeOrder::kShadowCascadeV0 &&
      order == ShadowCascadeOrder::kShadowCascadeV1) {
    GetStyleEngine().MarkAllElementsForStyleRecalc(
        StyleChangeReasonForTracing::Create(style_change_reason::kShadow));
    UseCounter::Count(*this, WebFeature::kMixedShadowRootV0AndV1);
  }

  if (order > shadow_cascade_order_)
    shadow_cascade_order_ = order;
}

PropertyRegistry* Document::GetPropertyRegistry() {
  // TODO(timloh): When the flag is removed, return a reference instead.
  if (!property_registry_ && RuntimeEnabledFeatures::CSSVariables2Enabled())
    property_registry_ = MakeGarbageCollected<PropertyRegistry>();
  return property_registry_;
}

const PropertyRegistry* Document::GetPropertyRegistry() const {
  return const_cast<Document*>(this)->GetPropertyRegistry();
}

void Document::MaybeQueueSendDidEditFieldInInsecureContext() {
  if (logged_field_edit_ || sensitive_input_edited_task_.IsActive() ||
      IsSecureContext()) {
    // Send a message on the first edit; the browser process doesn't care
    // about the presence of additional edits.
    //
    // The browser process only cares about editing fields on pages where the
    // top-level URL is not secure. Secure contexts must have a top-level URL
    // that is secure, so there is no need to send notifications for editing
    // in secure contexts.
    return;
  }
  logged_field_edit_ = true;
  sensitive_input_edited_task_ = PostCancellableTask(
      *GetTaskRunner(TaskType::kUserInteraction), FROM_HERE,
      WTF::Bind(&Document::SendDidEditFieldInInsecureContext,
                WrapWeakPersistent(this)));
}

CoreProbeSink* Document::GetProbeSink() {
  LocalFrame* frame = GetFrame();
  if (!frame && TemplateDocumentHost())
    frame = TemplateDocumentHost()->GetFrame();
  return probe::ToCoreProbeSink(frame);
}

service_manager::InterfaceProvider* Document::GetInterfaceProvider() {
  if (!GetFrame())
    return nullptr;

  return &GetFrame()->GetInterfaceProvider();
}

BrowserInterfaceBrokerProxy& Document::GetBrowserInterfaceBroker() {
  if (!GetFrame())
    return GetEmptyBrowserInterfaceBroker();

  return GetFrame()->GetBrowserInterfaceBroker();
}

DocumentResourceCoordinator* Document::GetResourceCoordinator() {
  if (!resource_coordinator_ && GetFrame()) {
    resource_coordinator_ =
        DocumentResourceCoordinator::MaybeCreate(GetBrowserInterfaceBroker());
  }
  return resource_coordinator_.get();
}

FrameOrWorkerScheduler* Document::GetScheduler() {
  DCHECK(IsMainThread());

  if (ContextDocument() && ContextDocument()->GetFrame())
    return ContextDocument()->GetFrame()->GetFrameScheduler();
  // In most cases, ContextDocument() will get us to a relevant Frame. In some
  // cases, though, there isn't a good candidate (most commonly when either the
  // passed-in document or ContextDocument() used to be attached to a Frame but
  // has since been detached).
  if (!detached_scheduler_) {
    detached_scheduler_ = scheduler::CreateDummyFrameScheduler();
  }
  return detached_scheduler_.get();
}

scoped_refptr<base::SingleThreadTaskRunner> Document::GetTaskRunner(
    TaskType type) {
  DCHECK(IsMainThread());

  if (ContextDocument() && ContextDocument()->GetFrame())
    return ContextDocument()->GetFrame()->GetTaskRunner(type);
  // In most cases, ContextDocument() will get us to a relevant Frame. In some
  // cases, though, there isn't a good candidate (most commonly when either the
  // passed-in document or ContextDocument() used to be attached to a Frame but
  // has since been detached).
  return Thread::Current()->GetTaskRunner();
}

DOMFeaturePolicy* Document::featurePolicy() {
  if (!policy_)
    policy_ = MakeGarbageCollected<DOMDocumentPolicy>(this);
  return policy_.Get();
}

const AtomicString& Document::RequiredCSP() {
  if (!Loader())
    return g_null_atom;
  return frame_->Loader().RequiredCSP();
}

StylePropertyMapReadOnly* Document::ComputedStyleMap(Element* element) {
  ElementComputedStyleMap::AddResult add_result =
      element_computed_style_map_.insert(element, nullptr);
  if (add_result.is_new_entry) {
    add_result.stored_value->value =
        MakeGarbageCollected<ComputedStylePropertyMap>(element);
  }
  return add_result.stored_value->value;
}

void Document::AddComputedStyleMapItem(
    Element* element,
    StylePropertyMapReadOnly* computed_style) {
  element_computed_style_map_.insert(element, computed_style);
}

StylePropertyMapReadOnly* Document::RemoveComputedStyleMapItem(
    Element* element) {
  return element_computed_style_map_.Take(element);
}

void Document::Trace(Visitor* visitor) {
  visitor->Trace(imports_controller_);
  visitor->Trace(doc_type_);
  visitor->Trace(implementation_);
  visitor->Trace(autofocus_candidates_);
  visitor->Trace(focused_element_);
  visitor->Trace(sequential_focus_navigation_starting_point_);
  visitor->Trace(hover_element_);
  visitor->Trace(active_element_);
  visitor->Trace(document_element_);
  visitor->Trace(root_scroller_controller_);
  visitor->Trace(title_element_);
  visitor->Trace(ax_object_cache_);
  visitor->Trace(markers_);
  visitor->Trace(css_target_);
  visitor->Trace(current_script_stack_);
  visitor->Trace(script_runner_);
  visitor->Trace(lists_invalidated_at_document_);
  visitor->Trace(node_lists_);
  visitor->Trace(top_layer_elements_);
  visitor->Trace(elem_sheet_);
  visitor->Trace(node_iterators_);
  visitor->Trace(ranges_);
  visitor->Trace(style_engine_);
  visitor->Trace(form_controller_);
  visitor->Trace(visited_link_state_);
  visitor->Trace(element_computed_style_map_);
  visitor->Trace(frame_);
  visitor->Trace(dom_window_);
  visitor->Trace(fetcher_);
  visitor->Trace(parser_);
  visitor->Trace(context_features_);
  visitor->Trace(http_refresh_scheduler_);
  visitor->Trace(style_sheet_list_);
  visitor->Trace(document_timing_);
  visitor->Trace(media_query_matcher_);
  visitor->Trace(scripted_animation_controller_);
  visitor->Trace(scripted_idle_task_controller_);
  visitor->Trace(text_autosizer_);
  visitor->Trace(registration_context_);
  visitor->Trace(custom_element_microtask_run_queue_);
  visitor->Trace(element_data_cache_);
  visitor->Trace(use_elements_needing_update_);
  visitor->Trace(timers_);
  visitor->Trace(template_document_);
  visitor->Trace(template_document_host_);
  visitor->Trace(user_action_elements_);
  visitor->Trace(svg_extensions_);
  visitor->Trace(timeline_);
  visitor->Trace(pending_animations_);
  visitor->Trace(worklet_animation_controller_);
  visitor->Trace(context_document_);
  visitor->Trace(canvas_font_cache_);
  visitor->Trace(intersection_observer_controller_);
  visitor->Trace(snap_coordinator_);
  visitor->Trace(resize_observer_controller_);
  visitor->Trace(property_registry_);
  visitor->Trace(network_state_observer_);
  visitor->Trace(policy_);
  visitor->Trace(slot_assignment_engine_);
  visitor->Trace(viewport_data_);
  visitor->Trace(navigation_initiator_);
  visitor->Trace(lazy_load_image_observer_);
  visitor->Trace(isolated_world_csp_map_);
  visitor->Trace(find_in_page_root_);
  visitor->Trace(computed_node_mapping_);
  visitor->Trace(mime_handler_view_before_unload_event_listener_);
  visitor->Trace(element_explicitly_set_attr_elements_map_);
  visitor->Trace(display_lock_activation_observer_);
  Supplementable<Document>::Trace(visitor);
  TreeScope::Trace(visitor);
  ContainerNode::Trace(visitor);
  ExecutionContext::Trace(visitor);
  SecurityContext::Trace(visitor);
  DocumentShutdownNotifier::Trace(visitor);
  SynchronousMutationNotifier::Trace(visitor);
}

void Document::RecordUkmOutliveTimeAfterShutdown(int outlive_time_count) {
  if (!needs_to_record_ukm_outlive_time_)
    return;

  DCHECK(ukm_recorder_);
  DCHECK(ukm_source_id_ != ukm::kInvalidSourceId);

  ukm::builders::Document_OutliveTimeAfterShutdown(ukm_source_id_)
      .SetGCCount(outlive_time_count)
      .Record(ukm_recorder_.get());
}

bool Document::CurrentFrameHadRAF() const {
  return scripted_animation_controller_ &&
         scripted_animation_controller_->CurrentFrameHadRAF();
}

bool Document::NextFrameHasPendingRAF() const {
  return scripted_animation_controller_ &&
         scripted_animation_controller_->NextFrameHasPendingRAF();
}

void Document::NavigateLocalAdsFrames() {
  // This navigates all the frames detected as an advertisement to about:blank.
  DCHECK(frame_);
  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().TraverseNext(frame_)) {
    if (auto* child_local_frame = DynamicTo<LocalFrame>(child)) {
      if (child_local_frame->IsAdSubframe()) {
        child_local_frame->Navigate(
            FrameLoadRequest(this, ResourceRequest(BlankURL())),
            WebFrameLoadType::kStandard);
      }
    }
    // TODO(yuzus): Once AdsTracker for remote frames is implemented and OOPIF
    // is enabled on low-end devices, navigate remote ads as well.
  }
}

SlotAssignmentEngine& Document::GetSlotAssignmentEngine() {
  if (!slot_assignment_engine_)
    slot_assignment_engine_ = MakeGarbageCollected<SlotAssignmentEngine>();
  return *slot_assignment_engine_;
}

bool Document::IsSlotAssignmentOrLegacyDistributionDirty() const {
  if (ChildNeedsDistributionRecalc())
    return true;
  if (slot_assignment_engine_ &&
      slot_assignment_engine_->HasPendingSlotAssignmentRecalc()) {
    return true;
  }
  return false;
}

bool Document::IsLazyLoadPolicyEnforced() const {
  return RuntimeEnabledFeatures::ExperimentalProductivityFeaturesEnabled() &&
         !GetFeaturePolicy()->IsFeatureEnabled(
             mojom::FeaturePolicyFeature::kLazyLoad);
}

bool Document::IsFocusAllowed() const {
  if (!frame_ || frame_->IsMainFrame() ||
      LocalFrame::HasTransientUserActivation(frame_)) {
    // 'autofocus' runs Element::focus asynchronously at which point the
    // document might not have a frame (see https://crbug.com/960224).
    return true;
  }

  WebFeature uma_type;
  bool sandboxed = IsSandboxed(WebSandboxFlags::kNavigation);
  bool ad = frame_->IsAdSubframe();
  if (sandboxed) {
    uma_type = ad ? WebFeature::kFocusWithoutUserActivationSandboxedAdFrame
                  : WebFeature::kFocusWithoutUserActivationSandboxedNotAdFrame;
  } else {
    uma_type =
        ad ? WebFeature::kFocusWithoutUserActivationNotSandboxedAdFrame
           : WebFeature::kFocusWithoutUserActivationNotSandboxedNotAdFrame;
  }
  CountUse(uma_type);
  if (!RuntimeEnabledFeatures::BlockingFocusWithoutUserActivationEnabled())
    return true;
  return IsFeatureEnabled(
      mojom::FeaturePolicyFeature::kFocusWithoutUserActivation);
}

NavigationInitiatorImpl& Document::NavigationInitiator() {
  if (!navigation_initiator_) {
    navigation_initiator_ =
        MakeGarbageCollected<NavigationInitiatorImpl>(*this);
  }
  return *navigation_initiator_;
}

LazyLoadImageObserver& Document::EnsureLazyLoadImageObserver() {
  if (!lazy_load_image_observer_) {
    lazy_load_image_observer_ =
        MakeGarbageCollected<LazyLoadImageObserver>(*this);
  }
  return *lazy_load_image_observer_;
}

WindowAgent& Document::GetWindowAgent() {
  return *static_cast<WindowAgent*>(GetAgent());
}

void Document::CountPotentialFeaturePolicyViolation(
    mojom::FeaturePolicyFeature feature) const {
  wtf_size_t index = static_cast<wtf_size_t>(feature);
  if (potentially_violated_features_.size() == 0) {
    potentially_violated_features_.resize(
        static_cast<wtf_size_t>(mojom::FeaturePolicyFeature::kMaxValue) + 1);
  } else if (potentially_violated_features_[index]) {
    return;
  }
  potentially_violated_features_[index] = true;
  UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.FeaturePolicy.PotentialViolation",
                            feature);
}
void Document::ReportFeaturePolicyViolation(
    mojom::FeaturePolicyFeature feature,
    mojom::FeaturePolicyDisposition disposition,
    const String& message,
    const String& source_file) const {
  if (!RuntimeEnabledFeatures::FeaturePolicyReportingEnabled(this))
    return;
  LocalFrame* frame = GetFrame();
  if (!frame)
    return;

  // Construct the feature policy violation report.
  const String& feature_name = GetNameForFeature(feature);
  const String& disp_str =
      (disposition == mojom::FeaturePolicyDisposition::kReport ? "report"
                                                               : "enforce");

  FeaturePolicyViolationReportBody* body =
      source_file.IsEmpty()
          ? MakeGarbageCollected<FeaturePolicyViolationReportBody>(
                feature_name, "Feature policy violation", disp_str)
          : MakeGarbageCollected<FeaturePolicyViolationReportBody>(
                feature_name, "Feature policy violation", disp_str,
                source_file);

  Report* report = MakeGarbageCollected<Report>(
      ReportType::kFeaturePolicyViolation, Url().GetString(), body);

  // Send the feature policy violation report to any ReportingObservers.
  auto* reporting_context = ReportingContext::From(this);
  reporting_context->QueueReport(report);

  // TODO(iclelland): Report something different in report-only mode
  if (disposition == mojom::FeaturePolicyDisposition::kEnforce) {
    frame->Console().AddMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kViolation,
        mojom::ConsoleMessageLevel::kError,
        (message.IsEmpty() ? ("Feature policy violation: " + feature_name +
                              " is not allowed in this document.")
                           : message)));
  }
}

void Document::IncrementNumberOfCanvases() {
  num_canvases_++;
}

void Document::AddActivationBlockingDisplayLock() {
  ++activation_blocking_display_lock_count_;
}

void Document::RemoveActivationBlockingDisplayLock() {
  DCHECK_GT(activation_blocking_display_lock_count_, 0);
  --activation_blocking_display_lock_count_;
}

int Document::ActivationBlockingDisplayLockCount() const {
  return activation_blocking_display_lock_count_;
}

void Document::AddLockedDisplayLock() {
  ++locked_display_lock_count_;
}

void Document::RemoveLockedDisplayLock() {
  DCHECK_GT(locked_display_lock_count_, 0);
  --locked_display_lock_count_;
}

int Document::LockedDisplayLockCount() const {
  return locked_display_lock_count_;
}

void Document::RegisterDisplayLockActivationObservation(Element* element) {
  EnsureDisplayLockActivationObserver().observe(element);
}

void Document::UnregisterDisplayLockActivationObservation(Element* element) {
  EnsureDisplayLockActivationObserver().unobserve(element);
}

IntersectionObserver& Document::EnsureDisplayLockActivationObserver() {
  if (!display_lock_activation_observer_) {
    // Use kPostTaskToDeliver method, since a commit can dirty layout, and we
    // want to avoid dirtying layout during post-lifecycle steps.
    display_lock_activation_observer_ = IntersectionObserver::Create(
        {}, {std::numeric_limits<float>::min()}, this,
        WTF::BindRepeating(&Document::ProcessDisplayLockActivationObservation,
                           WrapWeakPersistent(this)),
        IntersectionObserver::kPostTaskToDeliver);
  }
  return *display_lock_activation_observer_;
}

void Document::ProcessDisplayLockActivationObservation(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  for (auto& entry : entries) {
    if (entry->isIntersecting()) {
      auto* context = entry->target()->GetDisplayLockContext();
      DCHECK(context);
      DCHECK(context->ShouldCommitForActivation(
          DisplayLockActivationReason::kViewportIntersection));
      context->CommitForActivationWithSignal(entry->target());
    }
  }
}

void Document::ExecuteJavaScriptUrls() {
  DCHECK(frame_);
  Vector<PendingJavascriptUrl> urls_to_execute;
  urls_to_execute.swap(pending_javascript_urls_);

  for (auto& url_to_execute : urls_to_execute) {
    frame_->GetScriptController().ExecuteJavaScriptURL(
        url_to_execute.url, url_to_execute.disposition);
    if (!frame_)
      break;
  }
  CheckCompleted();
}

void Document::ProcessJavaScriptUrl(
    const KURL& url,
    ContentSecurityPolicyDisposition disposition) {
  DCHECK(url.ProtocolIsJavaScript());
  if (frame_->Loader().StateMachine()->IsDisplayingInitialEmptyDocument())
    load_event_progress_ = kLoadEventNotRun;
  frame_->Loader().Progress().ProgressStarted();
  pending_javascript_urls_.push_back(PendingJavascriptUrl(url, disposition));
  if (!javascript_url_task_handle_.IsActive()) {
    javascript_url_task_handle_ = PostCancellableTask(
        *GetTaskRunner(TaskType::kNetworking), FROM_HERE,
        WTF::Bind(&Document::ExecuteJavaScriptUrls, WrapWeakPersistent(this)));
  }
}

void Document::CancelPendingJavaScriptUrls() {
  if (javascript_url_task_handle_.IsActive())
    javascript_url_task_handle_.Cancel();
  pending_javascript_urls_.clear();
}

bool Document::IsInWebAppScope() const {
  if (!GetSettings())
    return false;

  const String& web_app_scope = GetSettings()->GetWebAppScope();
  if (web_app_scope.IsNull() || web_app_scope.IsEmpty())
    return false;

  DCHECK_EQ(KURL(web_app_scope).GetString(), web_app_scope);
  return Url().GetString().StartsWith(web_app_scope);
}

void Document::ClearIsolatedWorldCSPForTesting(int32_t world_id) {
  isolated_world_csp_map_->erase(world_id);
}

bool Document::ChildrenCanHaveStyle() const {
  if (LayoutObject* view = GetLayoutView())
    return view->CanHaveChildren();
  return false;
}

ComputedAccessibleNode* Document::GetOrCreateComputedAccessibleNode(
    AXID ax_id,
    WebComputedAXTree* tree) {
  if (computed_node_mapping_.find(ax_id) == computed_node_mapping_.end()) {
    auto* node =
        MakeGarbageCollected<ComputedAccessibleNode>(ax_id, tree, this);
    computed_node_mapping_.insert(ax_id, node);
  }
  return computed_node_mapping_.at(ax_id);
}

void Document::SetShowBeforeUnloadDialog(bool show_dialog) {
  if (!mime_handler_view_before_unload_event_listener_) {
    if (!show_dialog)
      return;

    mime_handler_view_before_unload_event_listener_ =
        MakeGarbageCollected<BeforeUnloadEventListener>(this);
    domWindow()->addEventListener(
        event_type_names::kBeforeunload,
        mime_handler_view_before_unload_event_listener_, false);
  }
  mime_handler_view_before_unload_event_listener_->SetShowBeforeUnloadDialog(
      show_dialog);
}

const Document* DocumentForTrustedTypes(const Document* doc) {
  // The Trusted Type factory & friends are stored on the window. For
  // programmatically created docs (like createHTMLDocument) let's use the one
  // from the context document.
  DCHECK(doc);
  while (doc->ContextDocument() && !doc->ExecutingWindow())
    doc = doc->ContextDocument();
  return doc;
}

TrustedTypePolicyFactory* Document::GetTrustedTypes() const {
  const Document* doc = DocumentForTrustedTypes(this);
  return doc->ExecutingWindow() ? doc->ExecutingWindow()->trustedTypes()
                                : nullptr;
}

bool Document::RequireTrustedTypes() const {
  return DocumentForTrustedTypes(this)->ExecutionContext::RequireTrustedTypes();
}

void Document::ColorSchemeChanged() {
  GetStyleEngine().ColorSchemeChanged();
  MediaQueryAffectingValueChanged();
}

bool Document::InForcedColorsMode() const {
  return RuntimeEnabledFeatures::ForcedColorsEnabled() && Platform::Current() &&
         Platform::Current()->ThemeEngine() &&
         Platform::Current()->ThemeEngine()->GetForcedColors() !=
             ForcedColors::kNone;
}

void Document::CountUse(mojom::WebFeature feature) const {
  if (DocumentLoader* loader = Loader()) {
    loader->CountUse(feature);
  }
}

void Document::CountUse(mojom::WebFeature feature) {
  if (DocumentLoader* loader = Loader()) {
    loader->CountUse(feature);
  }
}

void Document::CountDeprecation(mojom::WebFeature feature) {
  // TODO(yoichio): We should remove these counters when v0 APIs are removed.
  // crbug.com/946875.
  if (const OriginTrialContext* origin_trial_context =
          GetOriginTrialContext()) {
    if (feature == WebFeature::kHTMLImports &&
        origin_trial_context->IsFeatureEnabled(
            OriginTrialFeature::kHTMLImports)) {
      CountUse(WebFeature::kHTMLImportsOnReverseOriginTrials);
    } else if (feature == WebFeature::kElementCreateShadowRoot &&
               origin_trial_context->IsFeatureEnabled(
                   OriginTrialFeature::kShadowDOMV0)) {
      CountUse(WebFeature::kElementCreateShadowRootOnReverseOriginTrials);
    } else if (feature == WebFeature::kDocumentRegisterElement &&
               origin_trial_context->IsFeatureEnabled(
                   OriginTrialFeature::kCustomElementsV0)) {
      CountUse(WebFeature::kDocumentRegisterElementOnReverseOriginTrials);
    }
  }
  Deprecation::CountDeprecation(Loader(), feature);
}

void Document::CountProperty(CSSPropertyID property) const {
  if (DocumentLoader* loader = Loader()) {
    loader->GetUseCounterHelper().Count(
        property, UseCounterHelper::CSSPropertyType::kDefault, GetFrame());
  }
}

void Document::CountAnimatedProperty(CSSPropertyID property) const {
  if (DocumentLoader* loader = Loader()) {
    loader->GetUseCounterHelper().Count(
        property, UseCounterHelper::CSSPropertyType::kAnimation, GetFrame());
  }
}

void Document::CountUseOnlyInCrossOriginIframe(
    mojom::WebFeature feature) const {
  LocalFrame* frame = GetFrame();
  if (frame && frame->IsCrossOriginSubframe())
    CountUse(feature);
}

bool Document::IsUseCounted(mojom::WebFeature feature) const {
  if (DocumentLoader* loader = Loader()) {
    return loader->GetUseCounterHelper().HasRecordedMeasurement(feature);
  }
  return false;
}

bool Document::IsPropertyCounted(CSSPropertyID property) const {
  if (DocumentLoader* loader = Loader()) {
    return loader->GetUseCounterHelper().IsCounted(
        property, UseCounterHelper::CSSPropertyType::kDefault);
  }
  return false;
}

bool Document::IsAnimatedPropertyCounted(CSSPropertyID property) const {
  if (DocumentLoader* loader = Loader()) {
    return loader->GetUseCounterHelper().IsCounted(
        property, UseCounterHelper::CSSPropertyType::kAnimation);
  }
  return false;
}

void Document::ClearUseCounterForTesting(mojom::WebFeature feature) {
  if (DocumentLoader* loader = Loader())
    loader->GetUseCounterHelper().ClearMeasurementForTesting(feature);
}

template class CORE_TEMPLATE_EXPORT Supplement<Document>;

}  // namespace blink
#ifndef NDEBUG
static WeakDocumentSet& liveDocumentSet() {
  DEFINE_STATIC_LOCAL(blink::Persistent<WeakDocumentSet>, set,
                      (blink::MakeGarbageCollected<WeakDocumentSet>()));
  return *set;
}

void showLiveDocumentInstances() {
  WeakDocumentSet& set = liveDocumentSet();
  fprintf(stderr, "There are %u documents currently alive:\n", set.size());
  for (blink::Document* document : set) {
    fprintf(stderr, "- Document %p URL: %s\n", document,
            document->Url().GetString().Utf8().c_str());
  }
}
#endif
