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

#include "base/auto_reset.h"
#include "base/optional.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom-shared.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/net/ip_address_space.mojom-blink.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/modules/insecure_input/insecure_input_service.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/ukm.mojom-blink.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_prerendering_support.h"
#include "third_party/blink/renderer/bindings/core/v8/html_script_element_or_svg_script_element.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/usv_string_or_trusted_url.h"
#include "third_party/blink/renderer/bindings/core/v8/v0_custom_element_constructor_builder.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_element_creation_options.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy.h"
#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/pending_animations.h"
#include "third_party/blink/renderer/core/animation/worklet_animation_controller.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_style_sheet_init.h"
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
#include "third_party/blink/renderer/core/dom/attr.h"
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
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder.h"
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
#include "third_party/blink/renderer/core/dom/nth_index_cache.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/dom/scripted_animation_controller.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
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
#include "third_party/blink/renderer/core/events/page_transition_event.h"
#include "third_party/blink/renderer/core/events/visual_viewport_resize_event.h"
#include "third_party/blink/renderer/core/events/visual_viewport_scroll_event.h"
#include "third_party/blink/renderer/core/feature_policy/document_policy.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
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
#include "third_party/blink/renderer/core/frame/use_counter.h"
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
#include "third_party/blink/renderer/core/loader/appcache/application_cache_host.h"
#include "third_party/blink/renderer/core/loader/cookie_jar.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_fetch_context.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/idleness_detector.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/loader/navigation_scheduler.h"
#include "third_party/blink/renderer/core/loader/prerenderer_client.h"
#include "third_party/blink/renderer/core/loader/text_resource_decoder_builder.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/event_with_hit_test_results.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/overscroll_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/scroll_state_callback.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/snap_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/first_meaningful_paint_detector.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
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
#include "third_party/blink/renderer/core/trustedtypes/trusted_url.h"
#include "third_party/blink/renderer/core/workers/shared_worker_repository_client.h"
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
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/date_components.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/plugins/plugin_script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/weborigin/origin_access_entry.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

#ifndef NDEBUG
using WeakDocumentSet = blink::HeapHashSet<blink::WeakMember<blink::Document>>;
static WeakDocumentSet& liveDocumentSet();
#endif

namespace blink {

using namespace HTMLNames;

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
static const int kCLayoutScheduleThreshold = 250;

// After a document has been committed for this time, it can create a history
// entry even if the user hasn't interacted with the document.
static const int kElapsedTimeForHistoryEntryWithoutUserGestureMS = 5000;

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
      WTF::Unicode::kLetter_Lowercase | WTF::Unicode::kLetter_Uppercase |
      WTF::Unicode::kLetter_Other | WTF::Unicode::kLetter_Titlecase |
      WTF::Unicode::kNumber_Letter;
  if (!(WTF::Unicode::Category(c) & kNameStartMask))
    return false;

  // rule (c) above
  if (c >= 0xF900 && c < 0xFFFE)
    return false;

  // rule (d) above
  WTF::Unicode::CharDecompositionType decomp_type =
      WTF::Unicode::DecompositionType(c);
  if (decomp_type == WTF::Unicode::kDecompositionFont ||
      decomp_type == WTF::Unicode::kDecompositionCompat)
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
      WTF::Unicode::kMark_NonSpacing | WTF::Unicode::kMark_Enclosing |
      WTF::Unicode::kMark_SpacingCombining | WTF::Unicode::kLetter_Modifier |
      WTF::Unicode::kNumber_DecimalDigit;
  if (!(WTF::Unicode::Category(c) & kOtherNamePartMask))
    return false;

  // rule (c) above
  if (c >= 0xF900 && c < 0xFFFE)
    return false;

  // rule (d) above
  WTF::Unicode::CharDecompositionType decomp_type =
      WTF::Unicode::DecompositionType(c);
  if (decomp_type == WTF::Unicode::kDecompositionFont ||
      decomp_type == WTF::Unicode::kDecompositionCompat)
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

// This doesn't work with non-Document ExecutionContext.
static void RunAutofocusTask(ExecutionContext* context) {
  // Document lifecycle check is done in Element::focus()
  if (!context)
    return;

  Document* document = To<Document>(context);
  if (Element* element = document->AutofocusElement()) {
    document->SetAutofocusElement(nullptr);
    element->focus();
  }
}

static void RecordLoadReasonToHistogram(WouldLoadReason reason) {
  // TODO(dcheng): Make EnumerationHistogram work with scoped enums.
  DEFINE_STATIC_LOCAL(EnumerationHistogram, unseen_frame_histogram,
                      ("Navigation.DeferredDocumentLoading.StatesV4",
                       static_cast<int>(WouldLoadReason::kCount)));
  unseen_frame_histogram.Count(static_cast<int>(reason));
}

class Document::NetworkStateObserver final
    : public GarbageCollectedFinalized<Document::NetworkStateObserver>,
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
        on_line ? EventTypeNames::online : EventTypeNames::offline;
    Document* document = To<Document>(GetExecutionContext());
    if (!document->domWindow())
      return;
    document->domWindow()->DispatchEvent(*Event::Create(event_name));
    probe::networkStateChanged(document->GetFrame(), on_line);
  }

  void ContextDestroyed(ExecutionContext* context) override {
    UnregisterAsObserver(context);
  }

  void UnregisterAsObserver(ExecutionContext* context) {
    DCHECK(context);
    online_observer_handle_ = nullptr;
  }

  void Trace(blink::Visitor* visitor) override {
    ContextLifecycleObserver::Trace(visitor);
  }

 private:
  std::unique_ptr<NetworkStateNotifier::NetworkStateObserverHandle>
      online_observer_handle_;
};

Document* Document::CreateForTest() {
  return new Document(DocumentInit::Create());
}

Document* Document::Create(Document& document) {
  Document* new_document = new Document(
      DocumentInit::Create().WithContextDocument(&document).WithURL(
          BlankURL()));
  new_document->SetSecurityOrigin(document.GetMutableSecurityOrigin());
  new_document->SetContextFeatures(document.GetContextFeatures());
  return new_document;
}

Document::Document(const DocumentInit& initializer,
                   DocumentClassFlags document_classes)
    : ContainerNode(nullptr, kCreateDocument),
      TreeScope(*this),
      has_nodes_with_placeholder_style_(false),
      evaluate_media_queries_on_style_recalc_(false),
      pending_sheet_layout_(kNoLayoutWithPendingSheets),
      frame_(initializer.GetFrame()),
      // TODO(dcheng): Why does this need both a LocalFrame and LocalDOMWindow
      // pointer?
      dom_window_(frame_ ? frame_->DomWindow() : nullptr),
      imports_controller_(initializer.ImportsController()),
      context_document_(initializer.ContextDocument()),
      context_features_(ContextFeatures::DefaultSwitch()),
      well_formed_(false),
      printing_(kNotPrinting),
      compatibility_mode_(kNoQuirksMode),
      compatibility_mode_locked_(false),
      has_autofocused_(false),
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
      visited_link_state_(VisitedLinkState::Create(*this)),
      visually_ordered_(false),
      ready_state_(kComplete),
      parsing_state_(kFinishedParsing),
      contains_validity_style_rules_(false),
      contains_plugins_(false),
      ignore_destructive_write_count_(0),
      throw_on_dynamic_markup_insertion_count_(0),
      ignore_opens_during_unload_count_(0),
      markers_(new DocumentMarkerController(*this)),
      update_focus_appearance_timer_(
          GetTaskRunner(TaskType::kInternalUserInteraction),
          this,
          &Document::UpdateFocusAppearanceTimerFired),
      css_target_(nullptr),
      was_discarded_(false),
      load_event_progress_(kLoadEventCompleted),
      is_freezing_in_progress_(false),
      start_time_(CurrentTime()),
      script_runner_(ScriptRunner::Create(this)),
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
      is_srcdoc_document_(initializer.ShouldTreatURLAsSrcdocDocument()),
      is_mobile_document_(false),
      layout_view_(nullptr),
      has_fullscreen_supplement_(false),
      load_event_delay_count_(0),
      load_event_delay_timer_(GetTaskRunner(TaskType::kNetworking),
                              this,
                              &Document::LoadEventDelayTimerFired),
      plugin_loading_timer_(GetTaskRunner(TaskType::kInternalLoading),
                            this,
                            &Document::PluginLoadingTimerFired),
      document_timing_(*this),
      write_recursion_is_too_deep_(false),
      write_recursion_depth_(0),
      registration_context_(initializer.RegistrationContext(this)),
      element_data_cache_clear_timer_(
          GetTaskRunner(TaskType::kInternalUserInteraction),
          this,
          &Document::ElementDataCacheClearTimerFired),
      timeline_(DocumentTimeline::Create(this)),
      pending_animations_(new PendingAnimations(*this)),
      worklet_animation_controller_(new WorkletAnimationController(this)),
      template_document_host_(nullptr),
      did_associate_form_controls_timer_(
          GetTaskRunner(TaskType::kInternalLoading),
          this,
          &Document::DidAssociateFormControlsTimerFired),
      timers_(GetTaskRunner(TaskType::kJavascriptTimer)),
      has_viewport_units_(false),
      parser_sync_policy_(kAllowAsynchronousParsing),
      node_count_(0),
      would_load_reason_(WouldLoadReason::kInvalid),
      password_count_(0),
      logged_field_edit_(false),
      secure_context_state_(SecureContextState::kUnknown),
      ukm_source_id_(ukm::UkmRecorder::GetNewSourceID()),
#if DCHECK_IS_ON()
      slot_assignment_recalc_forbidden_recursion_depth_(0),
#endif
      needs_to_record_ukm_outlive_time_(false),
      viewport_data_(new ViewportData(*this)),
      agent_cluster_id_(base::UnguessableToken::Create()),
      parsed_feature_policies_(
          static_cast<int>(mojom::FeaturePolicyFeature::kMaxValue) + 1) {
  if (frame_) {
    DCHECK(frame_->GetPage());
    ProvideContextFeaturesToDocumentFrom(*this, *frame_->GetPage());

    fetcher_ = frame_->Loader().GetDocumentLoader()->Fetcher();
    FrameFetchContext::ProvideDocumentToContext(fetcher_->Context(), this);

    // TODO(dcheng): Why does this need to check that DOMWindow is non-null?
    CustomElementRegistry* registry =
        frame_->DomWindow() ? frame_->DomWindow()->MaybeCustomElements()
                            : nullptr;
    if (registry && registration_context_)
      registry->Entangle(registration_context_);
  } else if (imports_controller_) {
    fetcher_ = FrameFetchContext::CreateFetcherFromDocument(this);
  } else {
    fetcher_ = ResourceFetcher::Create(nullptr);
  }
  DCHECK(fetcher_);

  root_scroller_controller_ = RootScrollerController::Create(*this);

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

  InitSecurityContext(initializer);
  if (frame_)
    frame_->Client()->DidSetFramePolicyHeaders(GetSandboxFlags(), {});

  InitDNSPrefetch();

  InstanceCounters::IncrementCounter(InstanceCounters::kDocumentCounter);

  lifecycle_.AdvanceTo(DocumentLifecycle::kInactive);

  // Since CSSFontSelector requires Document::fetcher_ and StyleEngine owns
  // CSSFontSelector, need to initialize style_engine_ after initializing
  // fetcher_.
  style_engine_ = StyleEngine::Create(*this);

  // The parent's parser should be suspended together with all the other
  // objects, else this new Document would have a new ExecutionContext which
  // suspended state would not match the one from the parent, and could start
  // loading resources ignoring the defersLoading flag.
  DCHECK(!ParentDocument() || !ParentDocument()->IsContextPaused());

#ifndef NDEBUG
  liveDocumentSet().insert(this);
#endif
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
  if (anchor_node->GetTreeScope() == tree_scope)
    return Range::Create(tree_scope.GetDocument(), position, position);
  Node* const shadow_host = tree_scope.AncestorInThisScope(anchor_node);
  return Range::Create(tree_scope.GetDocument(),
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
    media_query_matcher_ = MediaQueryMatcher::Create(*this);
  return *media_query_matcher_;
}

void Document::MediaQueryAffectingValueChanged() {
  GetStyleEngine().MediaQueryAffectingValueChanged();
  if (NeedsLayoutTreeUpdate())
    evaluate_media_queries_on_style_recalc_ = true;
  else
    EvaluateMediaQueryList();
  probe::mediaQueryResultChanged(this);
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
    implementation_ = DOMImplementation::Create(*this);
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
  if (qname.NamespaceURI() == HTMLNames::xhtmlNamespaceURI) {
    // https://html.spec.whatwg.org/multipage/dom.html#elements-in-the-dom:element-interface
    element = HTMLElementFactory::Create(qname.LocalName(), *this, flags);
    if (!element) {
      // 6. If name is a valid custom element name, then return
      // HTMLElement.
      // 7. Return HTMLUnknownElement.
      if (CustomElement::IsValidName(qname.LocalName()))
        element = HTMLElement::Create(qname, *this);
      else
        element = HTMLUnknownElement::Create(qname, *this);
    }
    saw_elements_in_known_namespaces_ = true;
  } else if (qname.NamespaceURI() == svg_names::kNamespaceURI) {
    element = SVGElementFactory::Create(qname.LocalName(), *this, flags);
    if (!element)
      element = SVGUnknownElement::Create(qname, *this);
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
          QualifiedName(g_null_atom, local_name, HTMLNames::xhtmlNamespaceURI),
          CreateElementFlags::ByCreateElement());
    }
    if (auto* element = HTMLElementFactory::Create(
            local_name, *this, CreateElementFlags::ByCreateElement()))
      return element;
    QualifiedName q_name(g_null_atom, local_name, HTMLNames::xhtmlNamespaceURI);
    if (RegistrationContext() && V0CustomElement::IsValidName(local_name))
      return RegistrationContext()->CreateCustomTagElement(*this, q_name);
    return HTMLUnknownElement::Create(q_name, *this);
  }
  return Element::Create(QualifiedName(g_null_atom, name, g_null_atom), this);
}

AtomicString GetTypeExtension(Document* document,
                              const StringOrDictionary& string_or_options,
                              ExceptionState& exception_state) {
  if (string_or_options.IsNull())
    return AtomicString();

  if (string_or_options.IsString()) {
    UseCounter::Count(document,
                      WebFeature::kDocumentCreateElement2ndArgStringHandling);
    return AtomicString(string_or_options.GetAsString());
  }

  if (string_or_options.IsDictionary()) {
    Dictionary dict = string_or_options.GetAsDictionary();
    v8::Local<v8::Value> value;
    if (dict.HasProperty("is", exception_state) && dict.Get("is", value)) {
      return ToCoreAtomicString(v8::Local<v8::String>::Cast(value));
    }
  }

  return AtomicString();
}

// https://dom.spec.whatwg.org/#dom-document-createelement
Element* Document::CreateElementForBinding(
    const AtomicString& local_name,
    const StringOrDictionary& string_or_options,
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
                           ? HTMLNames::xhtmlNamespaceURI
                           : g_null_atom);

  bool is_v1 = string_or_options.IsDictionary() || !RegistrationContext();
  bool create_v1_builtin = string_or_options.IsDictionary();
  bool should_create_builtin =
      create_v1_builtin || string_or_options.IsString();

  // 3.
  const AtomicString& is =
      GetTypeExtension(this, string_or_options, exception_state);

  // 5. Let element be the result of creating an element given ...
  Element* element =
      CreateElement(q_name,
                    is_v1 ? CreateElementFlags::ByCreateElementV1()
                          : CreateElementFlags::ByCreateElementV0(),
                    should_create_builtin ? is : g_null_atom);

  // 8. If 'is' is non-null, set 'is' attribute
  if (!is_v1 && !is.IsEmpty())
    element->setAttribute(HTMLNames::isAttr, is);

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
Element* Document::createElementNS(const AtomicString& namespace_uri,
                                   const AtomicString& qualified_name,
                                   const StringOrDictionary& string_or_options,
                                   ExceptionState& exception_state) {
  if (string_or_options.IsNull())
    return createElementNS(namespace_uri, qualified_name, exception_state);

  // 1. Validate and extract
  QualifiedName q_name(
      CreateQualifiedName(namespace_uri, qualified_name, exception_state));
  if (q_name == QualifiedName::Null())
    return nullptr;

  bool is_v1 = string_or_options.IsDictionary() || !RegistrationContext();
  bool create_v1_builtin = string_or_options.IsDictionary();
  bool should_create_builtin =
      create_v1_builtin || string_or_options.IsString();

  // 2.
  const AtomicString& is =
      GetTypeExtension(this, string_or_options, exception_state);

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
    element->setAttribute(HTMLNames::isAttr, is);

  return element;
}

// Entry point of "create an element".
// https://dom.spec.whatwg.org/#concept-create-element
Element* Document::CreateElement(const QualifiedName& q_name,
                                 const CreateElementFlags flags,
                                 const AtomicString& is) {
  CustomElementDefinition* definition = nullptr;
  if (flags.IsCustomElementsV1() &&
      q_name.NamespaceURI() == HTMLNames::xhtmlNamespaceURI) {
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

ScriptPromise Document::createCSSStyleSheet(ScriptState* script_state,
                                            const String& text,
                                            ExceptionState& exception_state) {
  return Document::createCSSStyleSheet(script_state, text, CSSStyleSheetInit(),
                                       exception_state);
}

ScriptPromise Document::createCSSStyleSheet(ScriptState* script_state,
                                            const String& text,
                                            const CSSStyleSheetInit& options,
                                            ExceptionState& exception_state) {
  // Even though this function returns a Promise, it actually does all the work
  // at once here because CSS parsing is done synchronously on the main thread.
  // TODO(rakina): Find a way to improve this.
  CSSStyleSheet* sheet = CSSStyleSheet::Create(*this, options, exception_state);
  sheet->SetText(text, true /* allow_import_rules */, exception_state);
  sheet->SetAssociatedDocument(this);
  return ScriptPromise::Cast(script_state,
                             ScriptValue::From(script_state, sheet));
}

CSSStyleSheet* Document::createCSSStyleSheetSync(
    ScriptState* script_state,
    const String& text,
    ExceptionState& exception_state) {
  return Document::createCSSStyleSheetSync(
      script_state, text, CSSStyleSheetInit(), exception_state);
}

CSSStyleSheet* Document::createCSSStyleSheetSync(
    ScriptState* script_state,
    const String& text,
    const CSSStyleSheetInit& options,
    ExceptionState& exception_state) {
  CSSStyleSheet* sheet = CSSStyleSheet::Create(*this, options, exception_state);
  sheet->SetText(text, false /* allow_import_rules */, exception_state);
  if (exception_state.HadException())
    return nullptr;
  sheet->SetAssociatedDocument(this);
  return sheet;
}

CSSStyleSheet* Document::createEmptyCSSStyleSheet(
    ScriptState* script_state,
    const CSSStyleSheetInit& options,
    ExceptionState& exception_state) {
  CSSStyleSheet* sheet = CSSStyleSheet::Create(*this, options, exception_state);
  sheet->SetAssociatedDocument(this);
  return sheet;
}

CSSStyleSheet* Document::createEmptyCSSStyleSheet(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return Document::createEmptyCSSStyleSheet(script_state, CSSStyleSheetInit(),
                                            exception_state);
}

ScriptValue Document::registerElement(ScriptState* script_state,
                                      const AtomicString& name,
                                      const ElementRegistrationOptions& options,
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
  return constructor_builder.BindingsReturnValue();
}

V0CustomElementMicrotaskRunQueue* Document::CustomElementMicrotaskRunQueue() {
  if (!custom_element_microtask_run_queue_)
    custom_element_microtask_run_queue_ =
        V0CustomElementMicrotaskRunQueue::Create();
  return custom_element_microtask_run_queue_.Get();
}

void Document::ClearImportsController() {
  if (!Loader())
    fetcher_->ClearContext();
  imports_controller_ = nullptr;
}

HTMLImportsController* Document::EnsureImportsController() {
  if (!imports_controller_) {
    DCHECK(frame_);
    imports_controller_ = HTMLImportsController::Create(*this);
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
      Attr* attr = ToAttr(source);
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

      if (source->IsFrameOwnerElement()) {
        HTMLFrameOwnerElement* frame_owner_element =
            ToHTMLFrameOwnerElement(source);
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
              kJSMessageSource, kWarningMessageLevel,
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
  DispatchEvent(*Event::Create(EventTypeNames::readystatechange));
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
  SetNeedsStyleRecalc(kSubtreeStyleChange, StyleChangeReasonForTracing::Create(
                                               style_change_reason::kLanguage));
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
      DCHECK(lifecycle_.GetState() >= DocumentLifecycle::kStyleClean);
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
    if ((c <= WTF::Unicode::kSpaceCharacter &&
         c != WTF::Unicode::kLineTabulationCharacter) ||
        c == WTF::Unicode::kDeleteCharacter) {
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
  if (!title_element_) {
    if (IsHTMLDocument() || IsXHTMLDocument()) {
      HTMLElement* head_element = head();
      if (!head_element)
        return;
      title_element_ = HTMLTitleElement::Create(*this);
      head_element->AppendChild(title_element_.Get());
    } else if (IsSVGDocument()) {
      Element* element = documentElement();
      if (!IsSVGSVGElement(element))
        return;
      title_element_ = SVGTitleElement::Create(*this);
      element->InsertBefore(title_element_.Get(), element->firstChild());
    }
  } else {
    if (!IsHTMLDocument() && !IsXHTMLDocument() && !IsSVGDocument())
      title_element_ = nullptr;
  }

  if (auto* html_title = ToHTMLTitleElementOrNull(title_element_))
    html_title->setText(title);
  else if (auto* svg_title = ToSVGTitleElementOrNull(title_element_))
    svg_title->SetText(title);
  else
    UpdateTitle(title);
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

  if (auto* html_title = ToHTMLTitleElementOrNull(title_element_))
    UpdateTitle(html_title->text());
  else if (auto* svg_title = ToSVGTitleElementOrNull(title_element_))
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
  if (auto* html = ToHTMLHtmlElementOrNull(root_element))
    return html->dir();
  return g_null_atom;
}

void Document::setDir(const AtomicString& value) {
  Element* root_element = documentElement();
  if (auto* html = ToHTMLHtmlElementOrNull(root_element))
    html->setDir(value);
}

mojom::PageVisibilityState Document::GetPageVisibilityState() const {
  // The visibility of the document is inherited from the visibility of the
  // page. If there is no page associated with the document, we will assume
  // that the page is hidden, as specified by the spec:
  // https://w3c.github.io/page-visibility/#hidden-attribute
  if (!frame_ || !frame_->GetPage())
    return mojom::PageVisibilityState::kHidden;
  // While visibilitychange is being dispatched during unloading it is
  // expected that the visibility is hidden regardless of the page's
  // visibility.
  if (load_event_progress_ >= kUnloadVisibilityChangeInProgress)
    return mojom::PageVisibilityState::kHidden;
  return frame_->GetPage()->VisibilityState();
}

bool Document::IsPrefetchOnly() const {
  if (!frame_ || !frame_->GetPage())
    return false;

  PrerendererClient* prerenderer_client =
      PrerendererClient::From(frame_->GetPage());
  return prerenderer_client && prerenderer_client->IsPrefetchOnly();
}

String Document::visibilityState() const {
  return PageVisibilityStateString(GetPageVisibilityState());
}

bool Document::hidden() const {
  return GetPageVisibilityState() != mojom::PageVisibilityState::kVisible;
}

bool Document::wasDiscarded() const {
  return was_discarded_;
}

void Document::SetWasDiscarded(bool was_discarded) {
  was_discarded_ = was_discarded;
}

void Document::DidChangeVisibilityState() {
  DispatchEvent(*Event::CreateBubble(EventTypeNames::visibilitychange));
  // Also send out the deprecated version until it can be removed.
  DispatchEvent(*Event::CreateBubble(EventTypeNames::webkitvisibilitychange));

  if (GetPageVisibilityState() == mojom::PageVisibilityState::kVisible)
    Timeline().SetAllCompositorPending();

  if (hidden() && canvas_font_cache_)
    canvas_font_cache_->PruneAll();

  InteractiveDetector* interactive_detector = InteractiveDetector::From(*this);
  if (interactive_detector) {
    interactive_detector->OnPageVisibilityChanged(GetPageVisibilityState());
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
    form_controller_ = FormController::Create();
    HistoryItem* history_item = Loader() ? Loader()->GetHistoryItem() : nullptr;
    if (history_item)
      history_item->SetDocumentState(form_controller_->FormElementsState());
  }
  return *form_controller_;
}

DocumentState* Document::FormElementsState() const {
  if (!form_controller_)
    return nullptr;
  return form_controller_->FormElementsState();
}

void Document::SetStateForNewFormElements(const Vector<String>& state_vector) {
  if (!state_vector.size() && !form_controller_)
    return;
  GetFormController().SetStateForNewFormElements(state_vector);
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
  return NodeIterator::Create(root, what_to_show, filter);
}

TreeWalker* Document::createTreeWalker(Node* root,
                                       unsigned what_to_show,
                                       V8NodeFilter* filter) {
  DCHECK(root);
  return TreeWalker::Create(root, what_to_show, filter);
}

bool Document::NeedsLayoutTreeUpdate() const {
  if (!IsActive() || !View())
    return false;
  if (NeedsFullLayoutTreeUpdate())
    return true;
  if (ChildNeedsStyleRecalc())
    return true;
  if (ChildNeedsStyleInvalidation())
    return true;
  if (ChildNeedsReattachLayoutTree()) {
    DCHECK(InStyleRecalc());
    return true;
  }
  if (GetLayoutView() && GetLayoutView()->WasNotifiedOfSubtreeChange())
    return true;
  return false;
}

bool Document::NeedsFullLayoutTreeUpdate() const {
  if (!IsActive() || !View())
    return false;
  if (style_engine_->NeedsActiveStyleUpdate())
    return true;
  if (style_engine_->NeedsWhitespaceReattachment())
    return true;
  if (!use_elements_needing_update_.IsEmpty())
    return true;
  if (NeedsStyleRecalc())
    return true;
  if (NeedsStyleInvalidation())
    return true;
  // FIXME: The childNeedsDistributionRecalc bit means either self or children,
  // we should fix that.
  if (ChildNeedsDistributionRecalc())
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
                       InspectorRecalculateStylesEvent::Data(GetFrame()));
  ++style_version_;
}

bool Document::HasPendingForcedStyleRecalc() const {
  return HasPendingVisualUpdate() && !InStyleRecalc() &&
         GetStyleChangeType() >= kSubtreeStyleChange;
}

void Document::UpdateStyleInvalidationIfNeeded() {
  DCHECK(IsActive());
  ScriptForbiddenScope forbid_script;

  if (!ChildNeedsStyleInvalidation() && !NeedsStyleInvalidation())
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

void Document::PropagateStyleToViewport() {
  DCHECK(InStyleRecalc());
  if (!documentElement())
    return;

  HTMLElement* body = this->body();

  const ComputedStyle* body_style =
      body ? body->EnsureComputedStyle() : nullptr;
  const ComputedStyle* document_element_style =
      documentElement()->EnsureComputedStyle();

  TouchAction effective_touch_action =
      document_element_style->GetEffectiveTouchAction();
  WritingMode root_writing_mode = document_element_style->GetWritingMode();
  TextDirection root_direction = document_element_style->Direction();
  if (body_style) {
    root_writing_mode = body_style->GetWritingMode();
    root_direction = body_style->Direction();
  }

  const ComputedStyle* background_style = document_element_style;
  // http://www.w3.org/TR/css3-background/#body-background
  // <html> root element with no background steals background from its first
  // <body> child.
  // Also see LayoutBoxModelObject::BackgroundTransfersToView()
  if (IsHTMLHtmlElement(documentElement()) &&
      document_element_style->Display() != EDisplay::kNone &&
      IsHTMLBodyElement(body) && !background_style->HasBackground()) {
    background_style = body_style;
  }

  Color background_color = Color::kTransparent;
  FillLayer background_layers(EFillLayerType::kBackground, true);
  EImageRendering image_rendering = EImageRendering::kAuto;

  if (background_style->Display() != EDisplay::kNone) {
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
      // overflow to the viewport. Positioning background against either box is
      // equivalent to positioning against the scrolled box of the viewport.
      if (current_layer->Attachment() == EFillAttachment::kScroll)
        current_layer->SetAttachment(EFillAttachment::kLocal);
    }
    image_rendering = background_style->ImageRendering();
  }

  const ComputedStyle* overflow_style = nullptr;
  Element* viewport_element = ViewportDefiningElement();
  DCHECK(viewport_element);
  if (viewport_element == body) {
    overflow_style = body_style;
  } else {
    DCHECK_EQ(viewport_element, documentElement());
    overflow_style = document_element_style;

    // The body element has its own scrolling box, independent from the
    // viewport.  This is a bit of a weird edge case in the CSS spec that we
    // might want to try to eliminate some day (eg. for ScrollTopLeftInterop -
    // see http://crbug.com/157855).
    if (body_style && !body_style->IsOverflowVisible())
      UseCounter::Count(*this, WebFeature::kBodyScrollsInAdditionToViewport);
  }
  DCHECK(overflow_style);

  EOverflowAnchor overflow_anchor = overflow_style->OverflowAnchor();
  EOverflow overflow_x = overflow_style->OverflowX();
  EOverflow overflow_y = overflow_style->OverflowY();
  // Visible overflow on the viewport is meaningless, and the spec says to
  // treat it as 'auto':
  if (overflow_x == EOverflow::kVisible)
    overflow_x = EOverflow::kAuto;
  if (overflow_y == EOverflow::kVisible)
    overflow_y = EOverflow::kAuto;
  if (overflow_anchor == EOverflowAnchor::kVisible)
    overflow_anchor = EOverflowAnchor::kAuto;
  // Column-gap is (ab)used by the current paged overflow implementation (in
  // lack of other ways to specify gaps between pages), so we have to
  // propagate it too.
  GapLength column_gap = overflow_style->ColumnGap();

  ScrollSnapType snap_type = overflow_style->GetScrollSnapType();
  ScrollBehavior scroll_behavior = document_element_style->GetScrollBehavior();

  EOverscrollBehavior overscroll_behavior_x =
      overflow_style->OverscrollBehaviorX();
  EOverscrollBehavior overscroll_behavior_y =
      overflow_style->OverscrollBehaviorY();
  using OverscrollBehaviorType = cc::OverscrollBehavior::OverscrollBehaviorType;
  if (IsInMainFrame()) {
    GetPage()->GetOverscrollController().SetOverscrollBehavior(
        cc::OverscrollBehavior(
            static_cast<OverscrollBehaviorType>(overscroll_behavior_x),
            static_cast<OverscrollBehaviorType>(overscroll_behavior_y)));
  }

  Length scroll_padding_top = overflow_style->ScrollPaddingTop();
  Length scroll_padding_right = overflow_style->ScrollPaddingRight();
  Length scroll_padding_bottom = overflow_style->ScrollPaddingBottom();
  Length scroll_padding_left = overflow_style->ScrollPaddingLeft();

  const ComputedStyle& viewport_style = GetLayoutView()->StyleRef();
  if (viewport_style.GetWritingMode() != root_writing_mode ||
      viewport_style.Direction() != root_direction ||
      viewport_style.VisitedDependentColor(GetCSSPropertyBackgroundColor()) !=
          background_color ||
      viewport_style.BackgroundLayers() != background_layers ||
      viewport_style.ImageRendering() != image_rendering ||
      viewport_style.OverflowAnchor() != overflow_anchor ||
      viewport_style.OverflowX() != overflow_x ||
      viewport_style.OverflowY() != overflow_y ||
      viewport_style.ColumnGap() != column_gap ||
      viewport_style.GetScrollSnapType() != snap_type ||
      viewport_style.GetScrollBehavior() != scroll_behavior ||
      viewport_style.OverscrollBehaviorX() != overscroll_behavior_x ||
      viewport_style.OverscrollBehaviorY() != overscroll_behavior_y ||
      viewport_style.ScrollPaddingTop() != scroll_padding_top ||
      viewport_style.ScrollPaddingRight() != scroll_padding_right ||
      viewport_style.ScrollPaddingBottom() != scroll_padding_bottom ||
      viewport_style.ScrollPaddingLeft() != scroll_padding_left ||
      viewport_style.GetEffectiveTouchAction() != effective_touch_action) {
    scoped_refptr<ComputedStyle> new_style =
        ComputedStyle::Clone(viewport_style);
    new_style->SetWritingMode(root_writing_mode);
    new_style->UpdateFontOrientation();
    new_style->SetDirection(root_direction);
    new_style->SetBackgroundColor(background_color);
    new_style->AccessBackgroundLayers() = background_layers;
    new_style->SetImageRendering(image_rendering);
    new_style->SetOverflowAnchor(overflow_anchor);
    new_style->SetOverflowX(overflow_x);
    new_style->SetOverflowY(overflow_y);
    new_style->SetColumnGap(column_gap);
    new_style->SetScrollSnapType(snap_type);
    new_style->SetScrollBehavior(scroll_behavior);
    new_style->SetOverscrollBehaviorX(overscroll_behavior_x);
    new_style->SetOverscrollBehaviorY(overscroll_behavior_y);
    new_style->SetScrollPaddingTop(scroll_padding_top);
    new_style->SetScrollPaddingRight(scroll_padding_right);
    new_style->SetScrollPaddingBottom(scroll_padding_bottom);
    new_style->SetScrollPaddingLeft(scroll_padding_left);
    new_style->SetEffectiveTouchAction(effective_touch_action);
    GetLayoutView()->SetStyle(new_style);
    SetupFontBuilder(*new_style);

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

#if DCHECK_IS_ON()
static void AssertLayoutTreeUpdated(Node& root) {
  for (Node& node : NodeTraversal::InclusiveDescendantsOf(root)) {
    DCHECK(!node.NeedsStyleRecalc());
    DCHECK(!node.ChildNeedsStyleRecalc());
    DCHECK(!node.NeedsReattachLayoutTree());
    DCHECK(!node.ChildNeedsReattachLayoutTree());
    DCHECK(!node.ChildNeedsDistributionRecalc());
    DCHECK(!node.NeedsStyleInvalidation());
    DCHECK(!node.ChildNeedsStyleInvalidation());
    // Make sure there is no node which has a LayoutObject, but doesn't have a
    // parent in a flat tree. If there is such a node, we forgot to detach the
    // node. DocumentNode is only an exception.
    DCHECK((node.IsDocumentNode() || !node.GetLayoutObject() ||
            FlatTreeTraversal::Parent(node)))
        << node;

    if (ShadowRoot* shadow_root = node.GetShadowRoot())
      AssertLayoutTreeUpdated(*shadow_root);
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

#if DCHECK_IS_ON()
  NestingLevelIncrementer slot_assignment_recalc_forbidden_scope(
      slot_assignment_recalc_forbidden_recursion_depth_);
#endif

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

  // Entering here from inside layout, paint etc. would be catastrophic since
  // recalcStyle can tear down the layout tree or (unfortunately) run
  // script. Kill the whole layoutObject if someone managed to get into here in
  // states not allowing tree mutations.
  CHECK(Lifecycle().StateAllowsTreeMutations());

  TRACE_EVENT_BEGIN1("blink,devtools.timeline", "UpdateLayoutTree", "beginData",
                     InspectorRecalculateStylesEvent::Data(GetFrame()));

  unsigned start_element_count = GetStyleEngine().StyleForElementCount();

  probe::RecalculateStyle recalculate_style_scope(this);

  DocumentAnimations::UpdateAnimationTimingIfNeeded(*this);
  EvaluateMediaQueryListIfNeeded();
  UpdateUseShadowTreesIfNeeded();

  UpdateDistributionForLegacyDistributedNodes();

  UpdateActiveStyle();
  UpdateStyleInvalidationIfNeeded();

  // FIXME: We should update style on our ancestor chain before proceeding
  // however doing so currently causes several tests to crash, as
  // LocalFrame::setDocument calls Document::attach before setting the
  // LocalDOMWindow on the LocalFrame, or the SecurityOrigin on the
  // document. The attach, in turn resolves style (here) and then when we
  // resolve style on the parent chain, we may end up re-attaching our
  // containing iframe, which when asked HTMLFrameElementBase::isURLAllowed hits
  // a null-dereference due to security code always assuming the document has a
  // SecurityOrigin.

  UpdateStyle();

  NotifyLayoutTreeOfSubtreeChanges();

  // As a result of the style recalculation, the currently hovered element might
  // have been detached (for example, by setting display:none in the :hover
  // style), schedule another mouseMove event to check if any other elements
  // ended up under the mouse pointer due to re-layout.
  if (HoverElement() && !HoverElement()->GetLayoutObject() && GetFrame()) {
    GetFrame()->GetEventHandler().MayUpdateHoverWhenContentUnderMouseChanged(
        MouseEventManager::UpdateHoverReason::kLayoutOrStyleChanged);
  }

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

  StyleRecalcChange change = kNoChange;
  if (GetStyleChangeType() >= kSubtreeStyleChange)
    change = kForce;

  NthIndexCache nth_index_cache(*this);

  // TODO(futhark@chromium.org): Cannot access the EnsureStyleResolver() before
  // calling StyleForViewport() below because apparently the StyleResolver's
  // constructor has side effects. We should fix it. See
  // printing/setPrinting.html, printing/width-overflow.html though they only
  // fail on mac when accessing the resolver by what appears to be a viewport
  // size difference.

  if (change == kForce) {
    has_nodes_with_placeholder_style_ = false;
    scoped_refptr<ComputedStyle> viewport_style =
        StyleResolver::StyleForViewport(*this);
    StyleRecalcChange local_change = ComputedStyle::StylePropagationDiff(
        viewport_style.get(), GetLayoutView()->Style());
    if (local_change != kNoChange)
      GetLayoutView()->SetStyle(std::move(viewport_style));
  }

  ClearNeedsStyleRecalc();
  ClearNeedsReattachLayoutTree();

  StyleResolver& resolver = EnsureStyleResolver();

  bool should_record_stats;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("blink,blink_style", &should_record_stats);
  GetStyleEngine().SetStatsEnabled(should_record_stats);

  if (Element* document_element = documentElement()) {
    if (document_element->ShouldCallRecalcStyle(change)) {
      TRACE_EVENT0("blink,blink_style", "Document::recalcStyle");
      SCOPED_BLINK_UMA_HISTOGRAM_TIMER_HIGHRES("Style.RecalcTime");
      Element* viewport_defining = ViewportDefiningElement();
      GetStyleEngine().RecalcStyle(change);
      if (viewport_defining != ViewportDefiningElement())
        ViewportDefiningElementDidChange();
    }
    GetStyleEngine().MarkForWhitespaceReattachment();
    if (document_element->NeedsReattachLayoutTree() ||
        document_element->ChildNeedsReattachLayoutTree()) {
      TRACE_EVENT0("blink,blink_style", "Document::rebuildLayoutTree");
      SCOPED_BLINK_UMA_HISTOGRAM_TIMER_HIGHRES("Style.RebuildLayoutTreeTime");
      ReattachLegacyLayoutObjectList legacy_layout_objects(*this);
      GetStyleEngine().RebuildLayoutTree();
      legacy_layout_objects.ForceLegacyLayoutIfNeeded();
    }
  }
  GetStyleEngine().ClearWhitespaceReattachSet();
  ClearChildNeedsStyleRecalc();
  ClearChildNeedsReattachLayoutTree();

  PropagateStyleToViewport();
  View()->UpdateCountersAfterStyleChange();
  GetLayoutView()->RecalcOverflow();

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

void Document::ViewportDefiningElementDidChange() {
  HTMLBodyElement* body = FirstBodyElement();
  if (!body)
    return;
  LayoutObject* layout_object = body->GetLayoutObject();
  if (layout_object && layout_object->IsLayoutBlock()) {
    // When the overflow style for documentElement changes to or from visible,
    // it changes whether the body element's box should have scrollable overflow
    // on its own box or propagated to the viewport. If the body style did not
    // need a recalc, this will not be updated as its done as part of setting
    // ComputedStyle on the LayoutObject. Force a SetStyle for body when the
    // ViewportDefiningElement changes in order to trigger an update of
    // HasOverflowClip() and the PaintLayer in StyleDidChange().
    layout_object->SetStyle(ComputedStyle::Clone(*layout_object->Style()));
    // CompositingReason::kClipsCompositingDescendants depends on the root
    // element having a clip-related style. Since style update due to changes of
    // viewport-defining element don't end up as a StyleDifference, we need a
    // special dirty bit for this situation.
    if (layout_object->HasLayer()) {
      ToLayoutBoxModelObject(layout_object)
          ->Layer()
          ->SetNeeedsCompositingReasonsUpdate();
    }
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

bool Document::NeedsLayoutTreeUpdateForNode(const Node& node) const {
  if (!node.CanParticipateInFlatTree())
    return false;
  if (!NeedsLayoutTreeUpdate())
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
        ancestor->NeedsAdjacentStyleRecalc()) {
      return true;
    }
  }
  return false;
}

void Document::UpdateStyleAndLayoutTreeForNode(const Node* node) {
  DCHECK(node);
  if (!NeedsLayoutTreeUpdateForNode(*node))
    return;
  UpdateStyleAndLayoutTree();
}

void Document::UpdateStyleAndLayoutIgnorePendingStylesheetsForNode(Node* node) {
  DCHECK(node);
  if (!node->InActiveDocument())
    return;
  UpdateStyleAndLayoutIgnorePendingStylesheets();
}

void Document::UpdateStyleAndLayout() {
  DCHECK(IsMainThread());

  HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;

  LocalFrameView* frame_view = View();
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

  if (AXObjectCache* cache = ExistingAXObjectCache())
    cache->ProcessUpdatesAfterLayout(*this);

  if (LocalFrameView* frame_view_anchored = View())
    frame_view_anchored->PerformScrollAnchoringAdjustments();
}

void Document::LayoutUpdated() {
  DCHECK(GetFrame());
  DCHECK(View());

  // If we're restoring a scroll position from history, that takes precedence
  // over scrolling to the anchor in the URL.
  View()->ScrollAndFocusFragmentAnchor();

  // Script run in the call above may detach the document.
  if (GetFrame() && View()) {
    GetFrame()->Loader().RestoreScrollPositionAndViewState();

    // The focus call above can execute JS which can dirty layout. Ensure
    // layout is clean since this is called from UpdateLayout.
    if (View()->NeedsLayout())
      View()->UpdateLayout();
  }

  // Plugins can run script inside layout which can detach the page.
  // TODO(dcheng): Does it make sense to do any of this work if detached?
  if (GetFrame() && GetFrame()->IsMainFrame())
    GetFrame()->GetPage()->GetChromeClient().MainFrameLayoutUpdated();

  Markers().InvalidateRectsForAllTextMatchMarkers();

  // The layout system may perform layouts with pending stylesheets. When
  // recording first layout time, we ignore these layouts, since painting is
  // suppressed for them. We're interested in tracking the time of the
  // first real or 'paintable' layout.
  // TODO(esprehn): This doesn't really make sense, why not track the first
  // beginFrame? This will catch the first layout in a page that does lots
  // of layout thrashing even though that layout might not be followed by
  // a paint for many seconds.
  if (IsRenderingReady() && body() && HaveRenderBlockingResourcesLoaded()) {
    if (document_timing_.FirstLayout().is_null())
      document_timing_.MarkFirstLayout();
  }
}

void Document::ClearFocusedElementSoon() {
  if (!clear_focused_element_timer_.IsActive())
    clear_focused_element_timer_.StartOneShot(TimeDelta(), FROM_HERE);
}

void Document::ClearFocusedElementTimerFired(TimerBase*) {
  UpdateStyleAndLayoutTree();

  if (focused_element_ && !focused_element_->IsFocusable())
    focused_element_->blur();
}

// FIXME: This is a bad idea and needs to be removed eventually.
// Other browsers load stylesheets before they continue parsing the web page.
// Since we don't, we can run JavaScript code that needs answers before the
// stylesheets are loaded. Doing a layout ignoring the pending stylesheets
// lets us get reasonable answers. The long term solution to this problem is
// to instead suspend JavaScript execution.
void Document::UpdateStyleAndLayoutTreeIgnorePendingStylesheets() {
  if (Lifecycle().LifecyclePostponed())
    return;
  // See comment for equivalent CHECK in Document::UpdateStyleAndLayoutTree.
  // Updating style and layout can dirty state that must remain clean during
  // lifecycle updates.
  CHECK(Lifecycle().StateAllowsTreeMutations());
  StyleEngine::IgnoringPendingStylesheet ignoring(GetStyleEngine());

  if (!HaveRenderBlockingResourcesLoaded()) {
    // FIXME: We are willing to attempt to suppress painting with outdated style
    // info only once.  Our assumption is that it would be dangerous to try to
    // stop it a second time, after page content has already been loaded and
    // displayed with accurate style information. (Our suppression involves
    // blanking the whole page at the moment. If it were more refined, we might
    // be able to do something better.) It's worth noting though that this
    // entire method is a hack, since what we really want to do is suspend JS
    // instead of doing a layout with inaccurate information.
    HTMLElement* body_element = body();
    if (body_element && !body_element->GetLayoutObject() &&
        pending_sheet_layout_ == kNoLayoutWithPendingSheets) {
      pending_sheet_layout_ = kDidLayoutWithPendingSheets;
      GetStyleEngine().MarkAllTreeScopesDirty();
    }
    if (has_nodes_with_placeholder_style_) {
      // If new nodes have been added or style recalc has been done with style
      // sheets still pending, some nodes may not have had their real style
      // calculated yet.  Normally this gets cleaned when style sheets arrive
      // but here we need up-to-date style immediately.
      SetNeedsStyleRecalc(kSubtreeStyleChange,
                          StyleChangeReasonForTracing::Create(
                              style_change_reason::kCleanupPlaceholderStyles));
    }
  }
  UpdateStyleAndLayoutTree();
}

void Document::UpdateStyleAndLayoutIgnorePendingStylesheets(
    Document::RunPostLayoutTasks run_post_layout_tasks) {
  LocalFrameView* local_view = View();
  if (local_view)
    local_view->WillStartForcedLayout();
  UpdateStyleAndLayoutTreeIgnorePendingStylesheets();
  UpdateStyleAndLayout();

  if (local_view) {
    if (run_post_layout_tasks == kRunPostLayoutTasksSynchronously)
      local_view->FlushAnyPendingPostLayoutTasks();

    local_view->DidFinishForcedLayout();
  }
}

scoped_refptr<ComputedStyle>
Document::StyleForElementIgnoringPendingStylesheets(Element* element) {
  DCHECK_EQ(element->GetDocument(), this);
  StyleEngine::IgnoringPendingStylesheet ignoring(GetStyleEngine());
  if (!element->CanParticipateInFlatTree())
    return EnsureStyleResolver().StyleForElement(element, nullptr);

  ContainerNode* parent = LayoutTreeBuilderTraversal::Parent(*element);
  const ComputedStyle* parent_style =
      parent ? parent->EnsureComputedStyle() : nullptr;

  ContainerNode* layout_parent =
      parent ? LayoutTreeBuilderTraversal::LayoutParent(*element) : nullptr;
  const ComputedStyle* layout_parent_style =
      layout_parent ? layout_parent->EnsureComputedStyle() : parent_style;

  return EnsureStyleResolver().StyleForElement(element, parent_style,
                                               layout_parent_style);
}

scoped_refptr<ComputedStyle> Document::StyleForPage(int page_index) {
  UpdateDistributionForUnknownReasons();
  return EnsureStyleResolver().StyleForPage(page_index);
}

void Document::EnsurePaintLocationDataValidForNode(const Node* node) {
  DCHECK(node);
  if (!node->InActiveDocument())
    return;

  // For all nodes we must have up-to-date style and have performed layout to do
  // any location-based calculation.
  UpdateStyleAndLayoutIgnorePendingStylesheets();

  // The location of elements that are position: sticky is not known until
  // compositing inputs are cleaned. Therefore, for any elements that are either
  // sticky or are in a sticky sub-tree (e.g. are affected by a sticky element),
  // we need to also clean compositing inputs.
  if (View() && node->GetLayoutObject() &&
      node->GetLayoutObject()->StyleRef().SubtreeIsSticky()) {
    bool success = false;
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      // In SPv2, compositing inputs are cleaned as part of PrePaint.
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
  scoped_refptr<ComputedStyle> style = StyleForPage(page_index);

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

void Document::ScheduleUseShadowTreeUpdate(SVGUseElement& element) {
  use_elements_needing_update_.insert(&element);
  ScheduleLayoutTreeUpdateIfNeeded();
}

void Document::UnscheduleUseShadowTreeUpdate(SVGUseElement& element) {
  use_elements_needing_update_.erase(&element);
}

void Document::UpdateUseShadowTreesIfNeeded() {
  ScriptForbiddenScope forbid_script;

  if (use_elements_needing_update_.IsEmpty())
    return;

  HeapHashSet<Member<SVGUseElement>> elements;
  use_elements_needing_update_.swap(elements);
  for (SVGUseElement* element : elements)
    element->BuildPendingResource();
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

  layout_view_->SetIsInWindow(true);
  layout_view_->SetStyle(StyleResolver::StyleForViewport(*this));
  layout_view_->Compositor()->SetNeedsCompositingUpdate(
      kCompositingUpdateAfterCompositingInputChange);

  {
    ReattachLegacyLayoutObjectList legacy_layout_objects(*this);
    AttachContext context;
    ContainerNode::AttachLayoutTree(context);
    legacy_layout_objects.ForceLegacyLayoutIfNeeded();
  }

  // The TextAutosizer can't update layout view info while the Document is
  // detached, so update now in case anything changed.
  if (TextAutosizer* autosizer = GetTextAutosizer())
    autosizer->UpdatePageInfo();

  frame_->DocumentAttached();
  lifecycle_.AdvanceTo(DocumentLifecycle::kStyleClean);

  if (View())
    View()->DidAttachDocument();

  // Observer(s) should not be initialized until the document is initialized /
  // attached to a frame. Otherwise ContextLifecycleObserver::contextDestroyed
  // wouldn't be fired.
  network_state_observer_ = new NetworkStateObserver(*this);
}

void Document::Shutdown() {
  if (num_canvases_ > 0)
    UMA_HISTOGRAM_COUNTS_100("Blink.Canvas.NumCanvasesPerPage", num_canvases_);
  TRACE_EVENT0("blink", "Document::shutdown");
  CHECK(!frame_ || frame_->Tree().ChildCount() == 0);
  if (!IsActive())
    return;

  GetViewportData().Shutdown();

  // Frame navigation can cause a new Document to be attached. Don't allow that,
  // since that will cause a situation where LocalFrame still has a Document
  // attached after this finishes!  Normally, it shouldn't actually be possible
  // to trigger navigation here.  However, plugins (see below) can cause lots of
  // crazy things to happen, since plugin detach involves nested run loops.
  FrameNavigationDisabler navigation_disabler(*frame_);
  // Defer plugin dispose to avoid plugins trying to run script inside
  // ScriptForbiddenScope, which will crash the renderer after
  // https://crrev.com/200984
  HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
  // Don't allow script to run in the middle of detachLayoutTree() because a
  // detaching Document is not in a consistent state.
  ScriptForbiddenScope forbid_script;

  lifecycle_.AdvanceTo(DocumentLifecycle::kStopping);
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

  if (GetPage())
    GetPage()->DocumentDetached(this);
  probe::documentDetached(this);

  if (frame_->Client()->GetSharedWorkerRepositoryClient())
    frame_->Client()->GetSharedWorkerRepositoryClient()->DocumentDetached(this);

  // FIXME: consider using PausableObject.
  if (scripted_animation_controller_)
    scripted_animation_controller_->ClearDocumentPointer();
  scripted_animation_controller_.Clear();

  scripted_idle_task_controller_.Clear();

  if (SvgExtensions())
    AccessSVGExtensions().PauseAnimations();

  if (layout_view_)
    layout_view_->SetIsInWindow(false);

  if (RegistrationContext())
    RegistrationContext()->DocumentWasDetached();

  MutationObserver::CleanSlotChangeList(*this);

  hover_element_ = nullptr;
  active_element_ = nullptr;
  autofocus_element_ = nullptr;

  if (focused_element_.Get()) {
    Element* old_focused_element = focused_element_;
    focused_element_ = nullptr;
    if (GetPage())
      GetPage()->GetChromeClient().FocusedNodeChanged(old_focused_element,
                                                      nullptr);
  }
  sequential_focus_navigation_starting_point_ = nullptr;

  if (this == &AXObjectCacheOwner()) {
    ax_contexts_.clear();
    ClearAXObjectCache();
  }

  layout_view_ = nullptr;
  ContainerNode::DetachLayoutTree();
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

  // If this Document is associated with a live DocumentLoader, the
  // DocumentLoader will take care of clearing the FetchContext. Deferring
  // to the DocumentLoader when possible also prevents prematurely clearing
  // the context in the case where multiple Documents end up associated with
  // a single DocumentLoader (e.g., navigating to a javascript: url).
  if (!Loader())
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
    canvas_font_cache_ = CanvasFontCache::Create(*this);

  return canvas_font_cache_.Get();
}

DocumentParser* Document::CreateParser() {
  if (IsHTMLDocument())
    return HTMLDocumentParser::Create(ToHTMLDocument(*this),
                                      parser_sync_policy_);
  // FIXME: this should probably pass the frame instead
  return XMLDocumentParser::Create(*this, View());
}

bool Document::IsFrameSet() const {
  if (!IsHTMLDocument())
    return false;
  return IsHTMLFrameSetElement(body());
}

ScriptableDocumentParser* Document::GetScriptableDocumentParser() const {
  return Parser() ? Parser()->AsScriptableDocumentParser() : nullptr;
}

void Document::SetPrinting(PrintingState state) {
  bool was_printing = Printing();
  printing_ = state;
  bool is_printing = Printing();

  // Changing the state of Printing() can change whether layout objects are
  // created for iframes. As such, we need to do a full reattach. See
  // LayoutView::CanHaveChildren.
  // https://crbug.com/819327.
  if ((was_printing != is_printing) && documentElement() && GetFrame() &&
      !GetFrame()->IsMainFrame() && GetFrame()->Owner() &&
      GetFrame()->Owner()->IsDisplayNone()) {
    // LazyReattachIfAttached() is not idempotent. HTMLObjectElements will lose
    // their contents, which must be asynchronously regenerated. As such, we
    // avoid calling this method unless we think that this is a display-none
    // iframe and calling this is necessary.
    // This still leaves the edge case of a display: none iframe with an
    // HTMLObjectElement that doesn't print properly. https://crbug.com/838760.
    documentElement()->LazyReattachIfAttached();
  }
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

  // Change |document|'s URL to the URL of the responsible document specified
  // by the entry settings object.
  if (entered_document && this != entered_document) {
    // Clear the hash fragment from the inherited URL to prevent a
    // scroll-into-view for any document.open()'d frame.
    KURL new_url = entered_document->Url();
    new_url.SetFragmentIdentifier(String());
    SetURL(new_url);

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
  // navigation. Thus, we cut it off with IsNavigationScheduledWithin(0).
  //
  // This also prevents window.open(url) -- eg window.open("about:blank") --
  // from blowing away results from a subsequent window.document.open /
  // window.document.write call.
  if (frame_ &&
      (frame_->Loader().HasProvisionalNavigation() ||
       frame_->GetNavigationScheduler().IsNavigationScheduledWithin(0))) {
    frame_->Loader().StopAllLoaders();
    // Navigations handled by the client should also be cancelled.
    if (frame_ && frame_->Client())
      frame_->Client()->AbortClientNavigation();
  }

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
  DetachParser();
  SetParsingState(kFinishedParsing);
  SetReadyState(kComplete);
  SuppressLoadEvent();
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
  if (!documentElement() || !IsHTMLHtmlElement(documentElement()))
    return nullptr;

  for (HTMLElement* child =
           Traversal<HTMLElement>::FirstChild(*documentElement());
       child; child = Traversal<HTMLElement>::NextSibling(*child)) {
    if (IsHTMLFrameSetElement(*child) || IsHTMLBodyElement(*child))
      return child;
  }

  return nullptr;
}

HTMLBodyElement* Document::FirstBodyElement() const {
  if (!documentElement() || !IsHTMLHtmlElement(documentElement()))
    return nullptr;

  for (HTMLElement* child =
           Traversal<HTMLElement>::FirstChild(*documentElement());
       child; child = Traversal<HTMLElement>::NextSibling(*child)) {
    if (auto* body = ToHTMLBodyElementOrNull(*child))
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

  if (!IsHTMLBodyElement(*new_body) && !IsHTMLFrameSetElement(*new_body)) {
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
  if (auto* loader = Loader())
    loader->Fetcher()->LoosenLoadThrottlingPolicy();

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
  if (!root_style)
    return nullptr;
  if (body_element && root_style->IsOverflowVisible() &&
      IsHTMLHtmlElement(*root_element))
    return body_element;
  return root_element;
}

Document* Document::open(LocalDOMWindow* entered_window,
                         const AtomicString& type,
                         const AtomicString& replace,
                         ExceptionState& exception_state) {
  if (replace == "replace") {
    UseCounter::Count(Loader(), WebFeature::kDocumentOpenTwoArgsWithReplace);
  }
  open(entered_window->document(), exception_state);
  return this;
}

DOMWindow* Document::open(LocalDOMWindow* current_window,
                          LocalDOMWindow* entered_window,
                          const USVStringOrTrustedURL& stringOrUrl,
                          const AtomicString& name,
                          const AtomicString& features,
                          ExceptionState& exception_state) {
  if (!domWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The document has no window associated.");
    return nullptr;
  }
  AtomicString frame_name = name.IsEmpty() ? "_blank" : name;

  return domWindow()->open(stringOrUrl, frame_name, features, current_window,
                           entered_window, exception_state);
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
  DCHECK(parser_);

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

  // Make sure both the initial layout and reflow happen after the onload
  // fires. This will improve onload scores, and other browsers do it.
  // If they wanna cheat, we can too. -dwh

  if (GetFrame()->GetNavigationScheduler().LocationChangePending() &&
      ElapsedTime() < kCLayoutScheduleThreshold) {
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

  if (View())
    View()->ScrollAndFocusFragmentAnchor();

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

static bool AllDescendantsAreComplete(Frame* frame) {
  if (!frame)
    return true;
  for (Frame* child = frame->Tree().FirstChild(); child;
       child = child->Tree().TraverseNext(frame)) {
    if (child->IsLoading())
      return false;
  }
  return true;
}

bool Document::ShouldComplete() {
  return parsing_state_ == kFinishedParsing && HaveImportsLoaded() &&
         !fetcher_->BlockingRequestCount() && !IsDelayingLoadEvent() &&
         load_event_progress_ != kLoadEventInProgress &&
         AllDescendantsAreComplete(frame_);
}

void Document::Abort() {
  CancelParsing();
  CheckCompletedInternal();
}

void Document::CheckCompleted() {
  if (CheckCompletedInternal())
    frame_->Loader().DidFinishNavigation();
}

bool Document::CheckCompletedInternal() {
  if (!ShouldComplete())
    return false;

  if (frame_) {
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
  if (frame_->GetSettings()->GetSavePreviousDocumentResources() ==
      SavePreviousDocumentResources::kUntilOnLoad) {
    fetcher_->ClearResourcesFromPreviousFetcher();
  }
  frame_->GetNavigationScheduler().StartTimer();
  View()->HandleLoadCompleted();
  // The document itself is complete, but if a child frame was restarted due to
  // an event, this document is still considered to be in progress.
  if (!AllDescendantsAreComplete(frame_))
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
      mojom::blink::UkmSourceIdFrameHostAssociatedPtr ukm_binding;
      frame_->Client()->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
          &ukm_binding);
      DCHECK(ukm_binding.is_bound());
      ukm_binding->SetDocumentSourceId(ukm_source_id_);
    }

    AnchorElementMetrics::MaybeReportViewportMetricsOnLoad(*this);
  }

  return true;
}

bool Document::DispatchBeforeUnloadEvent(ChromeClient& chrome_client,
                                         bool is_reload,
                                         bool auto_cancel,
                                         bool& did_allow_navigation) {
  if (!dom_window_)
    return true;

  if (!body())
    return true;

  if (ProcessingBeforeUnload())
    return false;

  BeforeUnloadEvent& before_unload_event = *BeforeUnloadEvent::Create();
  before_unload_event.initEvent(EventTypeNames::beforeunload, false, true);
  load_event_progress_ = kBeforeUnloadEventInProgress;
  const TimeTicks beforeunload_event_start = CurrentTimeTicks();
  dom_window_->DispatchEvent(before_unload_event, this);
  const TimeTicks beforeunload_event_end = CurrentTimeTicks();
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

  // If |auto_cancel| is set then do not ask the |chrome_client| to display a
  // modal dialog. Simply indicate that the navigation should not proceed.
  if (auto_cancel) {
    beforeunload_dialog_histogram.Count(kNoDialogAutoCancelTrue);
    did_allow_navigation = false;
    return false;
  }

  String text = before_unload_event.returnValue();
  beforeunload_dialog_histogram.Count(
      BeforeUnloadDialogHistogramEnum::kShowDialog);
  const TimeTicks beforeunload_confirmpanel_start = CurrentTimeTicks();
  did_allow_navigation =
      chrome_client.OpenBeforeUnloadConfirmPanel(text, frame_, is_reload);
  const TimeTicks beforeunload_confirmpanel_end = CurrentTimeTicks();
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

void Document::DispatchUnloadEvents() {
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
        const TimeTicks pagehide_event_start = CurrentTimeTicks();
        window->DispatchEvent(
            *PageTransitionEvent::Create(EventTypeNames::pagehide, false),
            this);
        const TimeTicks pagehide_event_end = CurrentTimeTicks();
        DEFINE_STATIC_LOCAL(
            CustomCountHistogram, pagehide_histogram,
            ("DocumentEventTiming.PageHideDuration", 0, 10000000, 50));
        pagehide_histogram.CountMicroseconds(pagehide_event_end -
                                             pagehide_event_start);
      }
      if (!frame_)
        return;

      mojom::PageVisibilityState visibility_state = GetPageVisibilityState();
      load_event_progress_ = kUnloadVisibilityChangeInProgress;
      if (visibility_state != mojom::PageVisibilityState::kHidden) {
        // Dispatch visibilitychange event, but don't bother doing
        // other notifications as we're about to be unloaded.
        const TimeTicks pagevisibility_hidden_event_start = CurrentTimeTicks();
        DispatchEvent(*Event::CreateBubble(EventTypeNames::visibilitychange));
        const TimeTicks pagevisibility_hidden_event_end = CurrentTimeTicks();
        DEFINE_STATIC_LOCAL(CustomCountHistogram, pagevisibility_histogram,
                            ("DocumentEventTiming.PageVibilityHiddenDuration",
                             0, 10000000, 50));
        pagevisibility_histogram.CountMicroseconds(
            pagevisibility_hidden_event_end -
            pagevisibility_hidden_event_start);
        DispatchEvent(
            *Event::CreateBubble(EventTypeNames::webkitvisibilitychange));
      }
      if (!frame_)
        return;

      frame_->Loader().SaveScrollAnchor();

      DocumentLoader* document_loader =
          frame_->Loader().GetProvisionalDocumentLoader();
      load_event_progress_ = kUnloadEventInProgress;
      Event& unload_event = *Event::Create(EventTypeNames::unload);
      if (document_loader &&
          document_loader->GetTiming().UnloadEventStart().is_null() &&
          document_loader->GetTiming().UnloadEventEnd().is_null()) {
        DocumentLoadTiming& timing = document_loader->GetTiming();
        DCHECK(!timing.NavigationStart().is_null());
        const TimeTicks unload_event_start = CurrentTimeTicks();
        timing.MarkUnloadEventStart(unload_event_start);
        frame_->DomWindow()->DispatchEvent(unload_event, this);
        const TimeTicks unload_event_end = CurrentTimeTicks();
        DEFINE_STATIC_LOCAL(
            CustomCountHistogram, unload_histogram,
            ("DocumentEventTiming.UnloadDuration", 0, 10000000, 50));
        unload_histogram.CountMicroseconds(unload_event_end -
                                           unload_event_start);
        timing.MarkUnloadEventEnd(unload_event_end);
      } else {
        frame_->DomWindow()->DispatchEvent(unload_event, frame_->GetDocument());
      }
    }
    load_event_progress_ = kUnloadEventHandled;
  }

  if (!frame_)
    return;

  // Don't remove event listeners from a transitional empty document (see
  // https://bugs.webkit.org/show_bug.cgi?id=28716 for more information).
  bool keep_event_listeners =
      frame_->Loader().GetProvisionalDocumentLoader() &&
      frame_->ShouldReuseDefaultView(
          frame_->Loader().GetProvisionalDocumentLoader()->Url(),
          frame_->Loader()
              .GetProvisionalDocumentLoader()
              ->GetContentSecurityPolicy());
  if (!keep_event_listeners)
    RemoveAllEventListenersRecursively();
}

void Document::DispatchFreezeEvent() {
  DCHECK(RuntimeEnabledFeatures::PageLifecycleEnabled());
  const TimeTicks freeze_event_start = CurrentTimeTicks();
  SetFreezingInProgress(true);
  DispatchEvent(*Event::Create(EventTypeNames::freeze));
  SetFreezingInProgress(false);
  const TimeTicks freeze_event_end = CurrentTimeTicks();
  DEFINE_STATIC_LOCAL(CustomCountHistogram, freeze_histogram,
                      ("DocumentEventTiming.FreezeDuration", 0, 10000000, 50));
  freeze_histogram.CountMicroseconds(freeze_event_end - freeze_event_start);
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
  parsing_state_ = parsing_state;

  if (Parsing() && !element_data_cache_)
    element_data_cache_ = ElementDataCache::Create();
}

bool Document::ShouldScheduleLayout() const {
  // This function will only be called when LocalFrameView thinks a layout is
  // needed. This enforces a couple extra rules.
  //
  //    (a) Only schedule a layout once the stylesheets are loaded.
  //    (b) Only schedule layout once we have a body element.
  if (!IsActive())
    return false;

  if (IsRenderingReady() && body())
    return true;

  if (documentElement() && !IsHTMLHtmlElement(*documentElement()))
    return true;

  return false;
}

int Document::ElapsedTime() const {
  return static_cast<int>((CurrentTime() - start_time_) * 1000);
}

bool Document::CanCreateHistoryEntry() const {
  if (!frame_ || frame_->HasBeenActivated())
    return true;
  if (ElapsedTime() >= kElapsedTimeForHistoryEntryWithoutUserGestureMS)
    return true;
  UseCounter::Count(*this, WebFeature::kSuppressHistoryEntryWithoutUserGesture);
  // TODO(japhet): This flag controls an intervention to require a user gesture
  // or a long time on page in order for a content-initiated navigation to add
  // an entry to the back/forward list. Removing the flag and making this the
  // default will require updating a couple hundred tests that currently depend
  // on creating history entries without user gestures. I'm waiting to update
  // the tests until the feature is proven to minimize churn.
  // https://bugs.chromium.org/p/chromium/issues/detail?id=638198
  if (!GetSettings() || !GetSettings()->GetHistoryEntryRequiresUserGesture())
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
          kJSMessageSource, kWarningMessageLevel,
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
      "Avoid using document.write().", base::TimeDelta(), nullptr);
  probe::breakableLocation(this, "Document.write");
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

void Document::write(LocalDOMWindow* calling_window,
                     const Vector<String>& text,
                     ExceptionState& exception_state) {
  DCHECK(calling_window);

  if (GetSecurityContext().RequireTrustedTypes()) {
    DCHECK(RuntimeEnabledFeatures::TrustedDOMTypesEnabled());
    exception_state.ThrowTypeError(
        "This document can only write `TrustedHTML` objects.");
    return;
  }

  if (!AllowedToUseDynamicMarkUpInsertion("write", exception_state))
    return;

  StringBuilder builder;
  for (const String& string : text)
    builder.Append(string);
  write(builder.ToString(), calling_window->document(), exception_state);
}

void Document::writeln(LocalDOMWindow* calling_window,
                       const Vector<String>& text,
                       ExceptionState& exception_state) {
  DCHECK(calling_window);

  if (GetSecurityContext().RequireTrustedTypes()) {
    DCHECK(RuntimeEnabledFeatures::TrustedDOMTypesEnabled());
    exception_state.ThrowTypeError(
        "This document can only write `TrustedHTML` objects.");
    return;
  }

  if (!AllowedToUseDynamicMarkUpInsertion("writeln", exception_state))
    return;

  StringBuilder builder;
  for (const String& string : text)
    builder.Append(string);
  writeln(builder.ToString(), calling_window->document(), exception_state);
}

void Document::write(LocalDOMWindow* calling_window,
                     TrustedHTML* text,
                     ExceptionState& exception_state) {
  DCHECK(calling_window);
  DCHECK(RuntimeEnabledFeatures::TrustedDOMTypesEnabled());
  write(text->toString(), calling_window->document(), exception_state);
}

void Document::writeln(LocalDOMWindow* calling_window,
                       TrustedHTML* text,
                       ExceptionState& exception_state) {
  DCHECK(calling_window);
  DCHECK(RuntimeEnabledFeatures::TrustedDOMTypesEnabled());
  writeln(text->toString(), calling_window->document(), exception_state);
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
  const KURL& new_url = url.IsEmpty() ? BlankURL() : url;
  if (new_url == url_)
    return;

  url_ = new_url;
  access_entry_from_url_ = nullptr;
  UpdateBaseURL();
  GetContextFeatures().UrlDidChange(this);

  // TODO(crbug/795354): Move handling of URL recording out of the renderer.
  // URL must only be recorded from the main frame.
  if (ukm_recorder_ && IsInMainFrame())
    ukm_recorder_->UpdateSourceURL(ukm_source_id_, url_);
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
  // from the Document interface otherwise (which we store, preparsed, in url_).
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
      const AtomicString& value = base->FastGetAttribute(hrefAttr);
      if (!value.IsNull())
        href = &value;
    }
    if (!target) {
      const AtomicString& value = base->FastGetAttribute(targetAttr);
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
          kSecurityMessageSource, kErrorMessageLevel,
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
  if (!ImportLoader())
    StyleResolverMayHaveChanged();
  DidLoadAllScriptBlockingResources();
}

void Document::DidAddPendingStylesheetInBody() {
  if (ScriptableDocumentParser* parser = GetScriptableDocumentParser())
    parser->DidAddPendingStylesheetInBody();
}

void Document::DidRemoveAllPendingStylesheet() {
  StyleResolverMayHaveChanged();

  // Only imports on master documents can trigger rendering.
  if (HTMLImportLoader* import = ImportLoader())
    import->DidRemoveAllPendingStylesheet();
  if (!HaveImportsLoaded())
    return;
  DidLoadAllScriptBlockingResources();
}

void Document::DidRemoveAllPendingBodyStylesheets() {
  if (ScriptableDocumentParser* parser = GetScriptableDocumentParser())
    parser->DidLoadAllBodyStylesheets();
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

  double delay;
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
    AddConsoleMessage(ConsoleMessage::Create(kSecurityMessageSource,
                                             kErrorMessageLevel, message));
    return;
  }

  if (http_refresh_type == kHttpRefreshFromMetaTag &&
      IsSandboxed(kSandboxAutomaticFeatures)) {
    String message =
        "Refused to execute the redirect specified via '<meta "
        "http-equiv='refresh' content='...'>'. The document is sandboxed, and "
        "the 'allow-scripts' keyword is not set.";
    AddConsoleMessage(ConsoleMessage::Create(kSecurityMessageSource,
                                             kErrorMessageLevel, message));
    return;
  }
  frame_->GetNavigationScheduler().ScheduleRedirect(delay, refresh_url,
                                                    http_refresh_type);
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
      frame = ToLocalFrame(frame->Tree().Parent());
      // Srcdoc documents cannot be top-level documents, by definition,
      // because they need to be contained in iframes with the srcdoc.
      DCHECK(frame);
    }
    referrer_document = frame->GetDocument();
  }

  // Step: 3.1.4: "Let referrerSource be document's URL."
  return referrer_document->url_.StrippedForUseAsReferrer();
}

ReferrerPolicy Document::GetReferrerPolicy() const {
  ReferrerPolicy policy = ExecutionContext::GetReferrerPolicy();
  // For srcdoc documents without their own policy, walk up the frame
  // tree to find the document that is either not a srcdoc or doesn't
  // have its own policy. This algorithm is defined in
  // https://html.spec.whatwg.org/multipage/window-object.html#set-up-a-window-environment-settings-object.
  if (!frame_ || policy != kReferrerPolicyDefault || !IsSrcdocDocument()) {
    return policy;
  }
  LocalFrame* frame = ToLocalFrame(frame_->Tree().Parent());
  DCHECK(frame);
  return frame->GetDocument()->GetReferrerPolicy();
}

MouseEventWithHitTestResults Document::PerformMouseEventHitTest(
    const HitTestRequest& request,
    const LayoutPoint& document_point,
    const WebMouseEvent& event) {
  DCHECK(!GetLayoutView() || GetLayoutView()->IsLayoutView());

  // LayoutView::hitTest causes a layout, and we don't want to hit that until
  // the first layout because until then, there is nothing shown on the screen -
  // the user can't have intentionally clicked on something belonging to this
  // page.  Furthermore, mousemove events before the first layout should not
  // lead to a premature layout() happening, which could show a flash of white.
  // See also the similar code in EventHandler::hitTestResultAtPoint.
  if (!GetLayoutView() || !View() || !View()->DidFirstLayout()) {
    HitTestLocation location((LayoutPoint()));
    return MouseEventWithHitTestResults(event, location,
                                        HitTestResult(request, location));
  }

  HitTestLocation location(document_point);
  HitTestResult result(request, location);
  GetLayoutView()->HitTest(location, result);

  if (!request.ReadOnly())
    UpdateHoverActiveState(request, result.InnerElement());

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
  if (new_child.IsDocumentFragment()) {
    for (Node& child :
         NodeTraversal::ChildrenOf(ToDocumentFragment(new_child))) {
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
  DocumentInit init = DocumentInit::Create()
                          .WithContextDocument(ContextDocument())
                          .WithURL(Url());
  if (IsXMLDocument()) {
    if (IsXHTMLDocument())
      return XMLDocument::CreateXHTML(
          init.WithRegistrationContext(RegistrationContext()));
    return XMLDocument::Create(init);
  }
  return Create(init);
}

void Document::CloneDataFromDocument(const Document& other) {
  SetCompatibilityMode(other.GetCompatibilityMode());
  SetEncodingData(other.encoding_data_);
  SetContextFeatures(other.GetContextFeatures());
  SetSecurityOrigin(other.GetSecurityOrigin()->IsolatedCopy());
  SetMimeType(other.contentType());
}

StyleSheetList& Document::StyleSheets() {
  if (!style_sheet_list_)
    style_sheet_list_ = StyleSheetList::Create(this);
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

void Document::StyleResolverMayHaveChanged() {
  if (HasNodesWithPlaceholderStyle()) {
    SetNeedsStyleRecalc(kSubtreeStyleChange,
                        StyleChangeReasonForTracing::Create(
                            style_change_reason::kCleanupPlaceholderStyles));
  }

  if (DidLayoutWithPendingStylesheets() &&
      HaveRenderBlockingResourcesLoaded()) {
    // We need to manually repaint because we avoid doing all repaints in layout
    // or style recalc while sheets are still loading to avoid FOUC.
    pending_sheet_layout_ = kIgnoreLayoutWithPendingSheets;

    DCHECK(GetLayoutView() || ImportsController());
    if (GetLayoutView())
      GetLayoutView()->InvalidatePaintForViewAndCompositedLayers();
  }
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

void Document::RemoveFocusedElementOfSubtree(Node* node,
                                             bool among_children_only) {
  if (!focused_element_)
    return;

  // We can't be focused if we're not in the document.
  if (!node->isConnected())
    return;
  bool contains =
      node->IsShadowIncludingInclusiveAncestorOf(focused_element_.Get());
  if (contains && (focused_element_ != node || !among_children_only))
    ClearFocusedElement();
}

static Element* SkipDisplayNoneAncestors(Element* element) {
  for (; element; element = FlatTreeTraversal::ParentElement(*element)) {
    if (element->GetLayoutObject() || element->HasDisplayContentsStyle())
      return element;
  }
  return nullptr;
}

void Document::HoveredElementDetached(Element& element) {
  if (!hover_element_)
    return;
  if (element != hover_element_)
    return;

  // While in detaching, we shouldn't use FlatTreeTraversal if slot assignemnt
  // is dirty because it might triger assignement recalc. hover_element_ will be
  // updated after recalc assignment is calculated (and re-layout is done).
  if (IsSlotAssignmentOrLegacyDistributionDirty()) {
    hover_element_ = nullptr;
  } else {
    hover_element_ = SkipDisplayNoneAncestors(&element);
  }

  // If the mouse cursor is not visible, do not clear existing
  // hover effects on the ancestors of |element| and do not invoke
  // new hover effects on any other element.
  if (!GetPage()->IsCursorVisible())
    return;

  if (GetFrame())
    GetFrame()->GetEventHandler().ScheduleHoverStateUpdate();
}

void Document::ActiveChainNodeDetached(Element& element) {
  if (element == active_element_)
    active_element_ = SkipDisplayNoneAncestors(&element);
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

  // Make sure newFocusedNode is actually in this document
  if (new_focused_element && (new_focused_element->GetDocument() != this))
    return true;

  if (NodeChildRemovalTracker::IsBeingRemoved(new_focused_element))
    return true;

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
      old_focused_element->DispatchFocusOutEvent(EventTypeNames::focusout,
                                                 new_focused_element,
                                                 params.source_capabilities);
      // 'DOMFocusOut' is a DOM level 2 name for compatibility.
      // FIXME: We should remove firing DOMFocusOutEvent event when we are sure
      // no content depends on it, probably when <rdar://problem/8503958> is
      // resolved.
      old_focused_element->DispatchFocusOutEvent(EventTypeNames::DOMFocusOut,
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
      focus_change_blocked = true;
      goto SetFocusedElementDone;
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
      focus_change_blocked = true;
      goto SetFocusedElementDone;
    }
    CancelFocusAppearanceUpdate();
    EnsurePaintLocationDataValidForNode(focused_element_);
    // UpdateStyleAndLayout can call SetFocusedElement (through
    // ScrollAndFocusFragmentAnchor called in Document::LayoutUpdated) and clear
    // focused_element_.
    if (focused_element_ != new_focused_element) {
      focus_change_blocked = true;
      goto SetFocusedElementDone;
    }
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
        focus_change_blocked = true;
        goto SetFocusedElementDone;
      }
      // DOM level 3 bubbling focus event.
      focused_element_->DispatchFocusInEvent(EventTypeNames::focusin,
                                             old_focused_element, params.type,
                                             params.source_capabilities);

      if (focused_element_ != new_focused_element) {
        // handler shifted focus
        focus_change_blocked = true;
        goto SetFocusedElementDone;
      }

      // For DOM level 2 compatibility.
      // FIXME: We should remove firing DOMFocusInEvent event when we are sure
      // no content depends on it, probably when <rdar://problem/8503958> is m.
      focused_element_->DispatchFocusInEvent(EventTypeNames::DOMFocusIn,
                                             old_focused_element, params.type,
                                             params.source_capabilities);

      if (focused_element_ != new_focused_element) {
        // handler shifted focus
        focus_change_blocked = true;
        goto SetFocusedElementDone;
      }
    }
  }

  if (!focus_change_blocked && focused_element_) {
    // Create the AXObject cache in a focus change because Chromium relies on
    // it.
    if (AXObjectCache* cache = ExistingAXObjectCache()) {
      cache->HandleFocusedUIElementChanged(old_focused_element,
                                           new_focused_element);
    }
  }

  if (!focus_change_blocked && GetPage()) {
    GetPage()->GetChromeClient().FocusedNodeChanged(old_focused_element,
                                                    focused_element_.Get());
  }

SetFocusedElementDone:
  UpdateStyleAndLayoutTree();
  if (LocalFrame* frame = GetFrame())
    frame->Selection().DidChangeFocus();
  return !focus_change_blocked;
}

void Document::ClearFocusedElement() {
  SetFocusedElement(nullptr, FocusParams(SelectionBehaviorOnFocus::kNone,
                                         kWebFocusTypeNone, nullptr));
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
    if (node->IsElementNode())
      return ToElement(node);
    if (Element* neighbor_element = type == kWebFocusTypeForward
                                        ? ElementTraversal::Previous(*node)
                                        : ElementTraversal::Next(*node))
      return neighbor_element;
    return node->ParentOrShadowHostElement();
  }

  // Range::selectNodeContents didn't select contents because the element had
  // no children.
  if (sequential_focus_navigation_starting_point_->startContainer()
          ->IsElementNode() &&
      !sequential_focus_navigation_starting_point_->startContainer()
           ->hasChildren() &&
      sequential_focus_navigation_starting_point_->startOffset() == 0)
    return ToElement(
        sequential_focus_navigation_starting_point_->startContainer());

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
      Node* previous = next_node;
      do {
        previous = FlatTreeTraversal::Previous(*previous);
      } while (previous && !previous->IsElementNode());
      return ToElement(previous);
    }
    if (next_node->IsElementNode())
      return ToElement(next_node);
    Node* next = next_node;
    do {
      next = FlatTreeTraversal::Next(*next);
    } while (next && !next->IsElementNode());
    return ToElement(next);
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

static void LiveNodeListBaseWriteBarrier(void* parent,
                                         const LiveNodeListBase* list) {
  if (IsHTMLCollectionType(list->GetType())) {
    ScriptWrappableMarkingVisitor::WriteBarrier(
        static_cast<const HTMLCollection*>(list));
  } else {
    ScriptWrappableMarkingVisitor::WriteBarrier(
        static_cast<const LiveNodeList*>(list));
  }
}

void Document::RegisterNodeList(const LiveNodeListBase* list) {
  node_lists_.Add(list, list->InvalidationType());
  LiveNodeListBaseWriteBarrier(this, list);
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
  LiveNodeListBaseWriteBarrier(this, list);
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
                            ? Event::CreateBubble(EventTypeNames::scroll)
                            : Event::Create(EventTypeNames::scroll);
  scroll_event->SetTarget(target);
  EnsureScriptedAnimationController().EnqueuePerFrameEvent(scroll_event);
}

void Document::EnqueueResizeEvent() {
  Event* event = Event::Create(EventTypeNames::resize);
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
        Url().Protocol(), Url().Host(),
        network::cors::OriginAccessEntry::kAllowRegisterableDomains);
  }
  return *access_entry_from_url_;
}

void Document::SendSensitiveInputVisibility() {
  if (sensitive_input_visibility_task_.IsActive())
    return;

  sensitive_input_visibility_task_ = PostCancellableTask(
      *GetTaskRunner(TaskType::kInternalLoading), FROM_HERE,
      WTF::Bind(&Document::SendSensitiveInputVisibilityInternal,
                WrapWeakPersistent(this)));
}

void Document::SendSensitiveInputVisibilityInternal() {
  if (!GetFrame())
    return;

  mojom::blink::InsecureInputServicePtr insecure_input_service_ptr;
  GetFrame()->GetInterfaceProvider().GetInterface(
      mojo::MakeRequest(&insecure_input_service_ptr));
  if (password_count_ > 0) {
    insecure_input_service_ptr->PasswordFieldVisibleInInsecureContext();
    return;
  }
  insecure_input_service_ptr->AllPasswordFieldsInInsecureContextInvisible();
}

void Document::SendDidEditFieldInInsecureContext() {
  if (!GetFrame())
    return;

  mojom::blink::InsecureInputServicePtr insecure_input_service_ptr;
  GetFrame()->GetInterfaceProvider().GetInterface(
      mojo::MakeRequest(&insecure_input_service_ptr));

  insecure_input_service_ptr->DidEditFieldInInsecureContext();
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
          !OriginTrials::TouchEventFeatureDetectionEnabled(execution_context))
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
  if (event_type == EventTypeNames::DOMSubtreeModified) {
    UseCounter::Count(*this, WebFeature::kDOMSubtreeModifiedEvent);
    AddMutationEventListenerTypeIfEnabled(kDOMSubtreeModifiedListener);
  } else if (event_type == EventTypeNames::DOMNodeInserted) {
    UseCounter::Count(*this, WebFeature::kDOMNodeInsertedEvent);
    AddMutationEventListenerTypeIfEnabled(kDOMNodeInsertedListener);
  } else if (event_type == EventTypeNames::DOMNodeRemoved) {
    UseCounter::Count(*this, WebFeature::kDOMNodeRemovedEvent);
    AddMutationEventListenerTypeIfEnabled(kDOMNodeRemovedListener);
  } else if (event_type == EventTypeNames::DOMNodeRemovedFromDocument) {
    UseCounter::Count(*this, WebFeature::kDOMNodeRemovedFromDocumentEvent);
    AddMutationEventListenerTypeIfEnabled(kDOMNodeRemovedFromDocumentListener);
  } else if (event_type == EventTypeNames::DOMNodeInsertedIntoDocument) {
    UseCounter::Count(*this, WebFeature::kDOMNodeInsertedIntoDocumentEvent);
    AddMutationEventListenerTypeIfEnabled(kDOMNodeInsertedIntoDocumentListener);
  } else if (event_type == EventTypeNames::DOMCharacterDataModified) {
    UseCounter::Count(*this, WebFeature::kDOMCharacterDataModifiedEvent);
    AddMutationEventListenerTypeIfEnabled(kDOMCharacterDataModifiedListener);
  } else if (event_type == EventTypeNames::webkitAnimationStart ||
             event_type == EventTypeNames::animationstart) {
    AddListenerType(kAnimationStartListener);
  } else if (event_type == EventTypeNames::webkitAnimationEnd ||
             event_type == EventTypeNames::animationend) {
    AddListenerType(kAnimationEndListener);
  } else if (event_type == EventTypeNames::webkitAnimationIteration ||
             event_type == EventTypeNames::animationiteration) {
    AddListenerType(kAnimationIterationListener);
    if (View()) {
      // Need to re-evaluate time-to-effect-change for any running animations.
      View()->ScheduleAnimation();
    }
  } else if (event_type == EventTypeNames::webkitTransitionEnd ||
             event_type == EventTypeNames::transitionend) {
    AddListenerType(kTransitionEndListener);
  } else if (event_type == EventTypeNames::scroll) {
    AddListenerType(kScrollListener);
  } else if (event_type == EventTypeNames::load) {
    if (Node* node = event_target.ToNode()) {
      if (IsHTMLStyleElement(*node)) {
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

  if (RuntimeEnabledFeatures::DisplayNoneIFrameCreatesNoLayoutObjectEnabled()) {
    if (documentElement()) {
      if (is_display_none != owner->IsDisplayNone())
        documentElement()->LazyReattachIfAttached();
    }
  }

  // body() may become null as a result of modification event listeners, so we
  // check before each call.
  if (margin_width != owner->MarginWidth()) {
    if (auto* body_element = body()) {
      body_element->SetIntegralAttribute(marginwidthAttr, margin_width);
    }
  }
  if (margin_height != owner->MarginHeight()) {
    if (auto* body_element = body()) {
      body_element->SetIntegralAttribute(marginheightAttr, margin_height);
    }
  }
  if (scrolling_mode != owner->ScrollingMode() && View()) {
    View()->SetNeedsLayout();
  }
}

bool Document::IsInInvisibleSubframe() const {
  if (!LocalOwner())
    return false;  // this is a local root element

  // TODO(bokan): This looks like it doesn't work in OOPIF.
  DCHECK(GetFrame());
  return !GetFrame()->OwnerLayoutObject();
}

String Document::cookie(ExceptionState& exception_state) const {
  if (GetSettings() && !GetSettings()->GetCookieEnabled())
    return String();

  UseCounter::Count(*this, WebFeature::kCookieGet);

  // FIXME: The HTML5 DOM spec states that this attribute can raise an
  // InvalidStateError exception on getting if the Document has no
  // browsing context.

  if (!GetSecurityOrigin()->CanAccessCookies()) {
    if (IsSandboxed(kSandboxOrigin))
      exception_state.ThrowSecurityError(
          "The document is sandboxed and lacks the 'allow-same-origin' flag.");
    else if (Url().ProtocolIs("data"))
      exception_state.ThrowSecurityError(
          "Cookies are disabled inside 'data:' URLs.");
    else
      exception_state.ThrowSecurityError("Access is denied for this document.");
    return String();
  } else if (GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(*this, WebFeature::kFileAccessedCookies);
  }

  KURL cookie_url = CookieURL();
  if (cookie_url.IsEmpty())
    return String();

  return Cookies(this, cookie_url);
}

void Document::setCookie(const String& value, ExceptionState& exception_state) {
  if (GetSettings() && !GetSettings()->GetCookieEnabled())
    return;

  UseCounter::Count(*this, WebFeature::kCookieSet);

  // FIXME: The HTML5 DOM spec states that this attribute can raise an
  // InvalidStateError exception on setting if the Document has no
  // browsing context.

  if (!GetSecurityOrigin()->CanAccessCookies()) {
    if (IsSandboxed(kSandboxOrigin))
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

  KURL cookie_url = CookieURL();
  if (cookie_url.IsEmpty())
    return;

  SetCookies(this, cookie_url, value);
}

const AtomicString& Document::referrer() const {
  if (Loader())
    return Loader()->GetRequest().HttpReferrer();
  return g_null_atom;
}

String Document::domain() const {
  return GetSecurityOrigin()->Domain();
}

void Document::setDomain(const String& raw_domain,
                         ExceptionState& exception_state) {
  UseCounter::Count(*this, WebFeature::kDocumentSetDomain);

  if (!frame_) {
    exception_state.ThrowSecurityError(
        "A browsing context is required to set a domain.");
    return;
  }

  if (IsSandboxed(kSandboxDocumentDomain)) {
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

  // TODO(mkwst): If we decide to ship this, change the IDL file to make the
  // value nullable (via `TreatNullAs=NullString`, for example). For the moment,
  // just rely on JavaScript's inherent nuttiness for implicit conversion to the
  // string "null". https://crbug.com/733150
  if (!RuntimeEnabledFeatures::NullableDocumentDomainEnabled() ||
      new_domain != "null") {
    OriginAccessEntry access_entry(
        GetSecurityOrigin()->Protocol(), new_domain,
        network::cors::OriginAccessEntry::kAllowSubdomains);
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
  }

  if (frame_) {
    UseCounter::Count(*this,
                      GetSecurityOrigin()->Port() == 0
                          ? WebFeature::kDocumentDomainSetWithDefaultPort
                          : WebFeature::kDocumentDomainSetWithNonDefaultPort);
    bool was_cross_domain = frame_->IsCrossOriginSubframe();
    GetMutableSecurityOrigin()->SetDomainFromDOM(new_domain);
    if (View() && (was_cross_domain != frame_->IsCrossOriginSubframe()))
      View()->CrossOriginStatusChanged();

    frame_->GetScriptController().UpdateSecurityOrigin(GetSecurityOrigin());
  }
}

// http://www.whatwg.org/specs/web-apps/current-work/#dom-document-lastmodified
String Document::lastModified() const {
  DateComponents date;
  bool found_date = false;
  if (frame_) {
    if (DocumentLoader* document_loader = Loader()) {
      const AtomicString& http_last_modified =
          document_loader->GetResponse().HttpHeaderField(
              HTTPNames::Last_Modified);
      if (!http_last_modified.IsEmpty()) {
        double date_value = ParseDate(http_last_modified);
        if (!std::isnan(date_value)) {
          date.SetMillisecondsSinceEpochForDateTime(
              ConvertToLocalTime(date_value));
          found_date = true;
        }
      }
    }
  }
  // FIXME: If this document came from the file system, the HTML5
  // specificiation tells us to read the last modification date from the file
  // system.
  if (!found_date)
    date.SetMillisecondsSinceEpochForDateTime(
        ConvertToLocalTime(CurrentTimeMS()));
  return String::Format("%02d/%02d/%04d %02d:%02d:%02d", date.Month() + 1,
                        date.MonthDay(), date.FullYear(), date.Hour(),
                        date.Minute(), date.Second());
}

const KURL Document::SiteForCookies() const {
  // TODO(mkwst): This doesn't properly handle HTML Import documents.

  // If this is an imported document, grab its master document's first-party:
  if (IsHTMLImport())
    return ImportsController()->Master()->SiteForCookies();

  if (!GetFrame())
    return NullURL();

  // TODO(mkwst): This doesn't correctly handle sandboxed documents; we want to
  // look at their URL, but we can't because we don't know what it is.
  Frame& top = GetFrame()->Tree().Top();
  KURL top_document_url;
  if (top.IsLocalFrame()) {
    top_document_url = ToLocalFrame(top).GetDocument()->Url();
  } else {
    const SecurityOrigin* origin =
        top.GetSecurityContext()->GetSecurityOrigin();
    // TODO(yhirano): Ideally |origin| should not be null here.
    if (origin)
      top_document_url = KURL(NullURL(), origin->ToString());
    else
      top_document_url = NullURL();
  }

  if (SchemeRegistry::ShouldTreatURLSchemeAsFirstPartyWhenTopLevel(
          top_document_url.Protocol()))
    return top_document_url;

  // We're intentionally using the URL of each document rather than the
  // document's SecurityOrigin. A sandboxed document has a unique opaque
  // origin, but that shouldn't affect first-/third-party status for cookies
  // and site data.
  base::Optional<OriginAccessEntry> remote_entry;
  if (!top.IsLocalFrame()) {
    remote_entry.emplace(
        top_document_url.Protocol(), top_document_url.Host(),
        network::cors::OriginAccessEntry::kAllowRegisterableDomains);
  }
  const OriginAccessEntry& access_entry =
      remote_entry ? *remote_entry
                   : ToLocalFrame(top).GetDocument()->AccessEntryFromURL();

  const Frame* current_frame = GetFrame();
  while (current_frame) {
    // Skip over srcdoc documents, as they are always same-origin with their
    // closest non-srcdoc parent.
    while (current_frame->IsLocalFrame() &&
           ToLocalFrame(current_frame)->GetDocument()->IsSrcdocDocument())
      current_frame = current_frame->Tree().Parent();
    DCHECK(current_frame);

    // We use 'matchesDomain' here, as it turns out that some folks embed HTTPS
    // login forms into HTTP pages; we should allow this kind of upgrade.
    if (access_entry.MatchesDomain(
            *current_frame->GetSecurityContext()->GetSecurityOrigin()) ==
        network::cors::OriginAccessEntry::kDoesNotMatchOrigin) {
      return NullURL();
    }

    current_frame = current_frame->Tree().Parent();
  }

  return top_document_url;
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
    U16_NEXT(characters, i, length, c)
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
    CString original_bytes = title_element_->textContent().Latin1();
    std::unique_ptr<TextCodec> codec = NewTextCodec(new_data.Encoding());
    String correctly_decoded_title =
        codec->Decode(original_bytes.data(), original_bytes.length(),
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
    SetNeedsStyleRecalc(kSubtreeStyleChange,
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
  // https://html.spec.whatwg.org/multipage/browsers.html#origin
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
  StyleChangeType type = RuntimeEnabledFeatures::LayoutNGEnabled()
                             ? kNeedsReattachStyleChange
                             : kSubtreeStyleChange;
  SetNeedsStyleRecalc(type, StyleChangeReasonForTracing::Create(
                                style_change_reason::kDesignMode));
}

Document* Document::ParentDocument() const {
  if (!frame_)
    return nullptr;
  Frame* parent = frame_->Tree().Parent();
  if (!parent || !parent->IsLocalFrame())
    return nullptr;
  return ToLocalFrame(parent)->GetDocument();
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
  return createAttributeNS(g_null_atom, ConvertLocalName(name), exception_state,
                           true);
}

Attr* Document::createAttributeNS(const AtomicString& namespace_uri,
                                  const AtomicString& qualified_name,
                                  ExceptionState& exception_state,
                                  bool should_ignore_namespace_checks) {
  AtomicString prefix, local_name;
  if (!ParseQualifiedName(qualified_name, prefix, local_name, exception_state))
    return nullptr;

  QualifiedName q_name(prefix, local_name, namespace_uri);

  if (!should_ignore_namespace_checks &&
      !HasValidNamespaceForAttributes(q_name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNamespaceError,
        "The namespace URI provided ('" + namespace_uri +
            "') is not valid for the qualified name provided ('" +
            qualified_name + "').");
    return nullptr;
  }

  return Attr::Create(*this, q_name, g_empty_atom);
}

const SVGDocumentExtensions* Document::SvgExtensions() {
  return svg_extensions_.Get();
}

SVGDocumentExtensions& Document::AccessSVGExtensions() {
  if (!svg_extensions_)
    svg_extensions_ = new SVGDocumentExtensions(this);
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

void Document::FinishedParsing() {
  DCHECK(!GetScriptableDocumentParser() || !parser_->IsParsing());
  DCHECK(!GetScriptableDocumentParser() || ready_state_ != kLoading);
  SetParsingState(kInDOMContentLoaded);
  DocumentParserTiming::From(*this).MarkParserStop();

  // FIXME: DOMContentLoaded is dispatched synchronously, but this should be
  // dispatched in a queued task, see https://crbug.com/425790
  if (document_timing_.DomContentLoadedEventStart().is_null())
    document_timing_.MarkDomContentLoadedEventStart();
  DispatchEvent(*Event::CreateBubble(EventTypeNames::DOMContentLoaded));
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
                         InspectorMarkLoadEvent::Data(frame));
    probe::domContentLoadedEventFired(frame);
    frame->GetIdlenessDetector()->DomContentLoadedEventFired();
  }

  // Schedule dropping of the ElementDataCache. We keep it alive for a while
  // after parsing finishes so that dynamically inserted content can also
  // benefit from sharing optimizations.  Note that we don't refresh the timer
  // on cache access since that could lead to huge caches being kept alive
  // indefinitely by something innocuous like JS setting .innerHTML repeatedly
  // on a timer.
  element_data_cache_clear_timer_.StartOneShot(TimeDelta::FromSeconds(10),
                                               FROM_HERE);

  // Parser should have picked up all preloads by now
  fetcher_->ClearPreloads(ResourceFetcher::kClearSpeculativeMarkupPreloads);
  if (!frame_ || frame_->GetSettings()->GetSavePreviousDocumentResources() ==
                     SavePreviousDocumentResources::kUntilOnDOMContentLoaded) {
    fetcher_->ClearResourcesFromPreviousFetcher();
  }

  if (IsPrefetchOnly())
    WebPrerenderingSupport::Current()->PrefetchFinished();
}

void Document::ElementDataCacheClearTimerFired(TimerBase*) {
  element_data_cache_.Clear();
}

void Document::BeginLifecycleUpdatesIfRenderingReady() {
  if (!IsActive())
    return;
  if (!IsRenderingReady())
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

Color Document::ThemeColor() const {
  auto* root_element = documentElement();
  if (!root_element)
    return Color();
  for (HTMLMetaElement& meta_element :
       Traversal<HTMLMetaElement>::DescendantsOf(*root_element)) {
    Color color = Color::kTransparent;
    if (DeprecatedEqualIgnoringCase(meta_element.GetName(), "theme-color") &&
        CSSParser::ParseColor(
            color, meta_element.Content().GetString().StripWhiteSpace(), true))
      return color;
  }
  return Color();
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

void Document::ApplyFeaturePolicyFromHeader(
    const String& feature_policy_header) {
  if (!feature_policy_header.IsEmpty())
    UseCounter::Count(*this, WebFeature::kFeaturePolicyHeader);
  Vector<String> messages;
  const ParsedFeaturePolicy& declared_policy = ParseFeaturePolicyHeader(
      feature_policy_header, GetSecurityOrigin(), &messages);
  for (auto& message : messages) {
    AddConsoleMessage(
        ConsoleMessage::Create(kSecurityMessageSource, kErrorMessageLevel,
                               "Error with Feature-Policy header: " + message));
  }
  ApplyFeaturePolicy(declared_policy);
  if (frame_) {
    frame_->Client()->DidSetFramePolicyHeaders(GetSandboxFlags(),
                                               declared_policy);
  }
}

void Document::ApplyFeaturePolicy(const ParsedFeaturePolicy& declared_policy) {
  FeaturePolicy* parent_feature_policy = nullptr;
  ParsedFeaturePolicy container_policy;

  // If this frame is not the main frame, then get the appropriate parent policy
  // and container policy to construct the policy for this frame.
  if (frame_) {
    if (!frame_->IsMainFrame()) {
      parent_feature_policy =
          frame_->Tree().Parent()->GetSecurityContext()->GetFeaturePolicy();
    }
    if (frame_->Owner())
      container_policy = frame_->Owner()->ContainerPolicy();
  }

  InitializeFeaturePolicy(declared_policy, container_policy,
                          parent_feature_policy);

  // At this point, the document will not have been installed in the frame's
  // LocalDOMWindow, so we cannot call frame_->IsFeatureEnabled. This calls
  // SecurityContext::IsFeatureEnabled instead, which cannot report, but we
  // don't need reporting here in any case.
  is_vertical_scroll_enforced_ =
      RuntimeEnabledFeatures::ExperimentalProductivityFeaturesEnabled() &&
      !GetFeaturePolicy()->IsFeatureEnabled(
          mojom::FeaturePolicyFeature::kVerticalScroll);
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

  ukm_recorder_ =
      ukm::MojoUkmRecorder::Create(Platform::Current()->GetConnector());

  // TODO(crbug/795354): Move handling of URL recording out of the renderer.
  // URL must only be recorded from the main frame.
  if (IsInMainFrame())
    ukm_recorder_->UpdateSourceURL(ukm_source_id_, url_);
  return ukm_recorder_.get();
}

int64_t Document::UkmSourceID() const {
  return ukm_source_id_;
}

void Document::InitSecurityContext(const DocumentInit& initializer) {
  DCHECK(!GetSecurityOrigin());

  if (!initializer.HasSecurityContext()) {
    // No source for a security context.
    // This can occur via document.implementation.createDocument().
    cookie_url_ = KURL(g_empty_string);
    SetSecurityOrigin(SecurityOrigin::CreateUniqueOpaque());
    InitContentSecurityPolicy();
    ApplyFeaturePolicy({});
    return;
  }

  SandboxFlags sandbox_flags = initializer.GetSandboxFlags();
  if (fetcher_->Archive()) {
    // The URL of a Document loaded from a MHTML archive is controlled by the
    // Content-Location header. This would allow UXSS, since Content-Location
    // can be arbitrarily controlled to control the Document's URL and origin.
    // Instead, force a Document loaded from a MHTML archive to be sandboxed,
    // providing exceptions only for creating new windows.
    sandbox_flags |=
        kSandboxAll &
        ~(kSandboxPopups | kSandboxPropagatesToAuxiliaryBrowsingContexts);
  }
  // In the common case, create the security context from the currently
  // loading URL with a fresh content security policy.
  EnforceSandboxFlags(sandbox_flags);
  SetInsecureRequestPolicy(initializer.GetInsecureRequestPolicy());
  if (initializer.InsecureNavigationsToUpgrade()) {
    for (auto to_upgrade : *initializer.InsecureNavigationsToUpgrade())
      AddInsecureNavigationUpgrade(to_upgrade);
  }

  const ContentSecurityPolicy* policy_to_inherit = nullptr;

  if (IsSandboxed(kSandboxOrigin)) {
    cookie_url_ = url_;
    scoped_refptr<SecurityOrigin> security_origin =
        SecurityOrigin::CreateUniqueOpaque();
    // If we're supposed to inherit our security origin from our
    // owner, but we're also sandboxed, the only things we inherit are
    // the origin's potential trustworthiness and the ability to
    // load local resources. The latter lets about:blank iframes in
    // file:// URL documents load images and other resources from
    // the file system.
    Document* owner = initializer.OwnerDocument();
    if (owner) {
      if (owner->GetSecurityOrigin()->IsPotentiallyTrustworthy())
        security_origin->SetOpaqueOriginIsPotentiallyTrustworthy(true);
      if (owner->GetSecurityOrigin()->CanLoadLocalResources())
        security_origin->GrantLoadLocalResources();
      policy_to_inherit = owner->GetContentSecurityPolicy();
    }
    SetSecurityOrigin(std::move(security_origin));
  } else if (Document* owner = initializer.OwnerDocument()) {
    cookie_url_ = owner->CookieURL();
    // We alias the SecurityOrigins to match Firefox, see Bug 15313
    // https://bugs.webkit.org/show_bug.cgi?id=15313
    SetSecurityOrigin(owner->GetMutableSecurityOrigin());
    policy_to_inherit = owner->GetContentSecurityPolicy();
  } else {
    cookie_url_ = url_;
    SetSecurityOrigin(SecurityOrigin::Create(url_));
  }

  // Set the address space before setting up CSP, as the latter may override
  // the former via the 'treat-as-public-address' directive (see
  // https://wicg.github.io/cors-rfc1918/#csp).
  if (initializer.IsHostedInReservedIPRange()) {
    SetAddressSpace(GetSecurityOrigin()->IsLocalhost()
                        ? mojom::IPAddressSpace::kLocal
                        : mojom::IPAddressSpace::kPrivate);
  } else if (GetSecurityOrigin()->IsLocal()) {
    // "Local" security origins (like 'file://...') are treated as having
    // a local address space.
    //
    // TODO(mkwst): It's not entirely clear that this is a good idea.
    SetAddressSpace(mojom::IPAddressSpace::kLocal);
  } else {
    SetAddressSpace(mojom::IPAddressSpace::kPublic);
  }

  if (ImportsController()) {
    // If this document is an HTML import, grab a reference to it's master
    // document's Content Security Policy. We don't call
    // 'initContentSecurityPolicy' in this case, as we can't rebind the master
    // document's policy object: its ExecutionContext needs to remain tied to
    // the master document.
    SetContentSecurityPolicy(
        ImportsController()->Master()->GetContentSecurityPolicy());
  } else {
    InitContentSecurityPolicy(nullptr, policy_to_inherit,
                              initializer.PreviousDocumentCSP());
  }

  if (Settings* settings = initializer.GetSettings()) {
    if (!settings->GetWebSecurityEnabled()) {
      // Web security is turned off. We should let this document access every
      // other document. This is used primary by testing harnesses for web
      // sites.
      GetMutableSecurityOrigin()->GrantUniversalAccess();
    } else if (GetSecurityOrigin()->IsLocal()) {
      if (settings->GetAllowUniversalAccessFromFileURLs()) {
        // Some clients want local URLs to have universal access, but that
        // setting is dangerous for other clients.
        GetMutableSecurityOrigin()->GrantUniversalAccess();
      } else if (!settings->GetAllowFileAccessFromFileURLs()) {
        // Some clients do not want local URLs to have access to other local
        // URLs.
        GetMutableSecurityOrigin()->BlockLocalAccessFromLocalOrigin();
      }
    }
  }

  if (GetSecurityOrigin()->IsOpaque() &&
      SecurityOrigin::Create(url_)->IsPotentiallyTrustworthy())
    GetMutableSecurityOrigin()->SetOpaqueOriginIsPotentiallyTrustworthy(true);

  ApplyFeaturePolicy({});

  InitSecureContextState();
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

// the first parameter specifies a policy to use as the document csp meaning
// the document will take ownership of the policy
// the second parameter specifies a policy to inherit meaning the document
// will attempt to copy over the policy
void Document::InitContentSecurityPolicy(
    ContentSecurityPolicy* csp,
    const ContentSecurityPolicy* policy_to_inherit,
    const ContentSecurityPolicy* previous_document_csp) {
  SetContentSecurityPolicy(csp ? csp : ContentSecurityPolicy::Create());

  GetContentSecurityPolicy()->BindToExecutionContext(this);

  // We inherit the parent/opener's CSP for documents with "local" schemes:
  // 'about', 'blob', 'data', and 'filesystem'. We also inherit CSP for
  // documents with empty/invalid URLs because we treat those URLs as
  // 'about:blank' in Blink.
  //
  // https://w3c.github.io/webappsec-csp/#initialize-document-csp
  //
  // TODO(dcheng): This is similar enough to work we're doing in
  // 'DocumentLoader::ensureWriter' that it might make sense to combine them.
  if (policy_to_inherit) {
    GetContentSecurityPolicy()->CopyStateFrom(policy_to_inherit);
  } else {
    if (frame_) {
      Frame* inherit_from = frame_->Tree().Parent()
                                ? frame_->Tree().Parent()
                                : frame_->Client()->Opener();
      if (inherit_from && frame_ != inherit_from) {
        DCHECK(inherit_from->GetSecurityContext() &&
               inherit_from->GetSecurityContext()->GetContentSecurityPolicy());
        policy_to_inherit =
            inherit_from->GetSecurityContext()->GetContentSecurityPolicy();
      }
    }

    // If we don't have an opener or parent, inherit from the previous
    // document CSP.
    if (!policy_to_inherit)
      policy_to_inherit = previous_document_csp;

    // We should inherit the relevant CSP if the document is loaded using
    // a local-scheme url.
    if (policy_to_inherit &&
        (url_.IsEmpty() || url_.ProtocolIsAbout() || url_.ProtocolIsData() ||
         url_.ProtocolIs("blob") || url_.ProtocolIs("filesystem")))
      GetContentSecurityPolicy()->CopyStateFrom(policy_to_inherit);
  }
  // Plugin documents inherit their parent/opener's 'plugin-types' directive
  // regardless of URL.
  if (policy_to_inherit && IsPluginDocument())
    GetContentSecurityPolicy()->CopyPluginTypesFrom(policy_to_inherit);
}

bool Document::IsSecureTransitionTo(const KURL& url) const {
  scoped_refptr<const SecurityOrigin> other = SecurityOrigin::Create(url);
  return GetSecurityOrigin()->CanAccess(other.get());
}

bool Document::CanExecuteScripts(ReasonForCallingCanExecuteScripts reason) {
  DCHECK(GetFrame())
      << "you are querying canExecuteScripts on a non contextDocument.";

  // Normally, scripts are not allowed in sandboxed contexts that disallow them.
  // However, there is an exception for cases when the script should bypass the
  // main world's CSP (such as for privileged isolated worlds). See
  // https://crbug.com/811528.
  if (IsSandboxed(kSandboxScripts) &&
      !GetFrame()->GetScriptController().ShouldBypassMainWorldCSP()) {
    // FIXME: This message should be moved off the console once a solution to
    // https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
    if (reason == kAboutToExecuteScript) {
      AddConsoleMessage(ConsoleMessage::Create(
          kSecurityMessageSource, kErrorMessageLevel,
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

bool Document::IsRenderingReady() const {
  return style_engine_->IgnoringPendingStylesheets() ||
         HaveRenderBlockingResourcesLoaded();
}

bool Document::AllowInlineEventHandler(Node* node,
                                       EventListener* listener,
                                       const String& context_url,
                                       const WTF::OrdinalNumber& context_line) {
  Element* element = node && node->IsElementNode() ? ToElement(node) : nullptr;
  if (!ContentSecurityPolicy::ShouldBypassMainWorld(this) &&
      !GetContentSecurityPolicy()->AllowInlineEventHandler(
          element, listener->Code(), context_url, context_line))
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

void Document::EnforceSandboxFlags(SandboxFlags mask) {
  scoped_refptr<const SecurityOrigin> stand_in_origin = GetSecurityOrigin();
  bool is_potentially_trustworthy =
      stand_in_origin && stand_in_origin->IsPotentiallyTrustworthy();
  ApplySandboxFlags(mask, is_potentially_trustworthy);
}

void Document::UpdateSecurityOrigin(scoped_refptr<SecurityOrigin> origin) {
  SetSecurityOrigin(std::move(origin));
  DidUpdateSecurityOrigin();
}

void Document::DidUpdateSecurityOrigin() {
  if (!frame_)
    return;
  frame_->GetScriptController().UpdateSecurityOrigin(GetSecurityOrigin());
}

bool Document::IsContextThread() const {
  return IsMainThread();
}

void Document::UpdateFocusAppearanceLater() {
  if (!update_focus_appearance_timer_.IsActive())
    update_focus_appearance_timer_.StartOneShot(TimeDelta(), FROM_HERE);
}

void Document::CancelFocusAppearanceUpdate() {
  update_focus_appearance_timer_.Stop();
}

void Document::UpdateFocusAppearanceTimerFired(TimerBase*) {
  Element* element = FocusedElement();
  if (!element)
    return;
  UpdateStyleAndLayout();
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
  if (!intersection_observer_controller_)
    intersection_observer_controller_ =
        IntersectionObserverController::Create(this);
  return *intersection_observer_controller_;
}

ResizeObserverController& Document::EnsureResizeObserverController() {
  if (!resize_observer_controller_)
    resize_observer_controller_ = new ResizeObserverController();
  return *resize_observer_controller_;
}

static void RunAddConsoleMessageTask(MessageSource source,
                                     MessageLevel level,
                                     const String& message,
                                     ExecutionContext* context) {
  ConsoleMessage* console_message =
      ConsoleMessage::Create(source, level, message);
  context->AddConsoleMessage(console_message);
}

void Document::AddConsoleMessage(ConsoleMessage* console_message) {
  if (!IsContextThread()) {
    PostCrossThreadTask(
        *GetTaskRunner(TaskType::kInternalInspector), FROM_HERE,
        CrossThreadBind(&RunAddConsoleMessageTask, console_message->Source(),
                        console_message->Level(), console_message->Message(),
                        WrapCrossThreadPersistent(this)));
    return;
  }

  if (!frame_)
    return;

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
        SourceLocation::Create(Url().GetString(), line_number, 0, nullptr));
    console_message->SetNodes(frame_, std::move(nodes));
  }

  frame_->Console().AddMessage(console_message);
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
    if (auto* dialog = ToHTMLDialogElementOrNull(*it))
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

void Document::SuppressLoadEvent() {
  if (!LoadEventFinished())
    load_event_progress_ = kLoadEventCompleted;
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
    load_event_delay_timer_.StartOneShot(TimeDelta(), FROM_HERE);
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
    plugin_loading_timer_.StartOneShot(TimeDelta(), FROM_HERE);
}

void Document::PluginLoadingTimerFired(TimerBase*) {
  UpdateStyleAndLayout();
}

ScriptedAnimationController& Document::EnsureScriptedAnimationController() {
  if (!scripted_animation_controller_) {
    scripted_animation_controller_ = ScriptedAnimationController::Create(this);
    // We need to make sure that we don't start up the animation controller on a
    // background tab, for example.
    if (!GetPage())
      scripted_animation_controller_->Pause();
  }
  return *scripted_animation_controller_;
}

int Document::RequestAnimationFrame(
    FrameRequestCallbackCollection::FrameCallback* callback) {
  return EnsureScriptedAnimationController().RegisterCallback(callback);
}

void Document::CancelAnimationFrame(int id) {
  if (!scripted_animation_controller_)
    return;
  scripted_animation_controller_->CancelCallback(id);
}

void Document::ServiceScriptedAnimations(
    base::TimeTicks monotonic_animation_start_time) {
  if (!scripted_animation_controller_)
    return;
  scripted_animation_controller_->ServiceScriptedAnimations(
      monotonic_animation_start_time);
}

ScriptedIdleTaskController& Document::EnsureScriptedIdleTaskController() {
  if (!scripted_idle_task_controller_)
    scripted_idle_task_controller_ = ScriptedIdleTaskController::Create(this);
  return *scripted_idle_task_controller_;
}

int Document::RequestIdleCallback(
    ScriptedIdleTaskController::IdleTask* idle_task,
    const IdleRequestOptions& options) {
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

SnapCoordinator* Document::GetSnapCoordinator() {
  if (RuntimeEnabledFeatures::CSSScrollSnapPointsEnabled() &&
      !snap_coordinator_)
    snap_coordinator_ = SnapCoordinator::Create();

  return snap_coordinator_.Get();
}

void Document::SetContextFeatures(ContextFeatures& features) {
  context_features_ = &features;
}

// TODO(mustaq) |request| parameter maybe a misuse of HitTestRequest in
// updateHoverActiveState() since the function doesn't bother with hit-testing.
void Document::UpdateHoverActiveState(const HitTestRequest& request,
                                      Element* inner_element) {
  DCHECK(!request.ReadOnly());

  if (request.Active() && frame_)
    frame_->GetEventHandler().NotifyElementActivated();

  Element* inner_element_in_document = inner_element;

  while (inner_element_in_document &&
         inner_element_in_document->GetDocument() != this) {
    inner_element_in_document->GetDocument().UpdateHoverActiveState(
        request, inner_element_in_document);
    inner_element_in_document =
        inner_element_in_document->GetDocument().LocalOwner();
  }

  UpdateDistributionForFlatTreeTraversal();

  UpdateActiveState(request, inner_element_in_document);
  UpdateHoverState(request, inner_element_in_document);
}

void Document::UpdateActiveState(const HitTestRequest& request,
                                 Element* inner_element_in_document) {
  Element* old_active_element = GetActiveElement();
  if (old_active_element && !request.Active()) {
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
        !new_active_element->IsDisabledFormControl() && request.Active() &&
        !request.TouchMove()) {
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

  // If the mouse is down and if this is a mouse move event, we want to restrict
  // changes in :active to only apply to elements that are in the :active
  // chain that we froze at the time the mouse went down.
  bool must_be_in_active_chain = request.Active() && request.Move();

  Element* new_element = SkipDisplayNoneAncestors(inner_element_in_document);

  // Now set the active state for our new object up to the root.
  for (Element* curr = new_element; curr;
       curr = FlatTreeTraversal::ParentElement(*curr)) {
    if (!must_be_in_active_chain || curr->InActiveChain())
      curr->SetActive(true);
  }
}

void Document::UpdateHoverState(const HitTestRequest& request,
                                Element* inner_element_in_document) {
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
    if (ancestor && ancestor->IsElementNode())
      ancestor_element = ToElement(ancestor);
  }

  HeapVector<Member<Element>, 32> elements_to_remove_from_chain;
  HeapVector<Member<Element>, 32> elements_to_add_to_hover_chain;

  // The old hover path only needs to be cleared up to (and not including) the
  // common ancestor;
  //
  // FIXME(ecobos@igalia.com): oldHoverElement may be disconnected from the
  // tree already.
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
  if (RuntimeEnabledFeatures::CSSInBodyDoesNotBlockPaintEnabled()) {
    return HaveImportsLoaded() &&
           style_engine_->HaveRenderBlockingStylesheetsLoaded();
  }
  return HaveImportsLoaded() &&
         style_engine_->HaveScriptBlockingStylesheetsLoaded();
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

Document& Document::EnsureTemplateDocument() {
  if (IsTemplateDocument())
    return *this;

  if (template_document_)
    return *template_document_;

  if (IsHTMLDocument()) {
    template_document_ =
        HTMLDocument::Create(DocumentInit::Create()
                                 .WithContextDocument(ContextDocument())
                                 .WithURL(BlankURL())
                                 .WithNewRegistrationContext());
  } else {
    template_document_ =
        Document::Create(DocumentInit::Create().WithURL(BlankURL()));
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
        TimeDelta::FromMilliseconds(300), FROM_HERE);
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
    text_autosizer_ = TextAutosizer::Create(this);
  return text_autosizer_.Get();
}

void Document::SetAutofocusElement(Element* element) {
  if (!element) {
    autofocus_element_ = nullptr;
    return;
  }
  if (has_autofocused_)
    return;
  has_autofocused_ = true;
  DCHECK(!autofocus_element_);
  autofocus_element_ = element;
  GetTaskRunner(TaskType::kUserInteraction)
      ->PostTask(FROM_HERE,
                 WTF::Bind(&RunAutofocusTask, WrapWeakPersistent(this)));
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
  return BodyAttributeValue(bgcolorAttr);
}

void Document::setBgColor(const AtomicString& value) {
  if (!IsFrameSet())
    SetBodyAttribute(bgcolorAttr, value);
}

const AtomicString& Document::fgColor() const {
  return BodyAttributeValue(textAttr);
}

void Document::setFgColor(const AtomicString& value) {
  if (!IsFrameSet())
    SetBodyAttribute(textAttr, value);
}

const AtomicString& Document::alinkColor() const {
  return BodyAttributeValue(alinkAttr);
}

void Document::setAlinkColor(const AtomicString& value) {
  if (!IsFrameSet())
    SetBodyAttribute(alinkAttr, value);
}

const AtomicString& Document::linkColor() const {
  return BodyAttributeValue(linkAttr);
}

void Document::setLinkColor(const AtomicString& value) {
  if (!IsFrameSet())
    SetBodyAttribute(linkAttr, value);
}

const AtomicString& Document::vlinkColor() const {
  return BodyAttributeValue(vlinkAttr);
}

void Document::setVlinkColor(const AtomicString& value) {
  if (!IsFrameSet())
    SetBodyAttribute(vlinkAttr, value);
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
  if (GetSandboxFlags() != kSandboxNone) {
    UseCounter::Count(
        *this, is_secure
                   ? WebFeature::kSecureContextCheckForSandboxedOriginPassed
                   : WebFeature::kSecureContextCheckForSandboxedOriginFailed);
  }
  UseCounter::Count(*this, is_secure ? WebFeature::kSecureContextCheckPassed
                                     : WebFeature::kSecureContextCheckFailed);
  return is_secure;
}

void Document::DidEnforceInsecureRequestPolicy() {
  if (!GetFrame())
    return;
  GetFrame()->Client()->DidEnforceInsecureRequestPolicy(
      GetInsecureRequestPolicy());
}

void Document::DidEnforceInsecureNavigationsSet() {
  if (!GetFrame())
    return;
  GetFrame()->Client()->DidEnforceInsecureNavigationsSet(
      SecurityContext::SerializeInsecureNavigationSet(
          *InsecureNavigationsToUpgrade()));
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

  // For V0 -> V1 upgrade, we need style recalculation for the whole document.
  if (shadow_cascade_order_ == ShadowCascadeOrder::kShadowCascadeV0 &&
      order == ShadowCascadeOrder::kShadowCascadeV1) {
    SetNeedsStyleRecalc(
        kSubtreeStyleChange,
        StyleChangeReasonForTracing::Create(style_change_reason::kShadow));
    UseCounter::Count(*this, WebFeature::kMixedShadowRootV0AndV1);
  }

  if (order > shadow_cascade_order_)
    shadow_cascade_order_ = order;
}

PropertyRegistry* Document::GetPropertyRegistry() {
  // TODO(timloh): When the flag is removed, return a reference instead.
  if (!property_registry_ && RuntimeEnabledFeatures::CSSVariables2Enabled())
    property_registry_ = PropertyRegistry::Create();
  return property_registry_;
}

const PropertyRegistry* Document::GetPropertyRegistry() const {
  return const_cast<Document*>(this)->GetPropertyRegistry();
}

void Document::IncrementPasswordCount() {
  ++password_count_;
  if (IsSecureContext() || password_count_ != 1) {
    // The browser process only cares about passwords on pages where the
    // top-level URL is not secure. Secure contexts must have a top-level
    // URL that is secure, so there is no need to send notifications for
    // password fields in secure contexts.
    //
    // Also, only send a message on the first visible password field; the
    // browser process doesn't care about the presence of additional
    // password fields beyond that.
    return;
  }
  SendSensitiveInputVisibility();
}

void Document::DecrementPasswordCount() {
  DCHECK_GT(password_count_, 0u);
  --password_count_;
  if (IsSecureContext() || password_count_ > 0)
    return;
  SendSensitiveInputVisibility();
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

FrameOrWorkerScheduler* Document::GetScheduler() {
  DCHECK(IsMainThread());

  if (ContextDocument() && ContextDocument()->GetFrame())
    return ContextDocument()->GetFrame()->GetFrameScheduler();
  // In most cases, ContextDocument() will get us to a relevant Frame. In some
  // cases, though, there isn't a good candidate (most commonly when either the
  // passed-in document or ContextDocument() used to be attached to a Frame but
  // has since been detached).
  return nullptr;
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
  return Platform::Current()->CurrentThread()->GetTaskRunner();
}

Policy* Document::policy() {
  if (!policy_)
    policy_ = new DocumentPolicy(this);
  return policy_.Get();
}

const AtomicString& Document::RequiredCSP() {
  return Loader() ? Loader()->RequiredCSP() : g_null_atom;
}

StylePropertyMapReadOnly* Document::ComputedStyleMap(Element* element) {
  ElementComputedStyleMap::AddResult add_result =
      element_computed_style_map_.insert(element, nullptr);
  if (add_result.is_new_entry)
    add_result.stored_value->value = ComputedStylePropertyMap::Create(element);
  return add_result.stored_value->value;
}

void Document::AddComputedStyleMapItem(
    Element* element,
    StylePropertyMapReadOnly* computed_style) {
  element_computed_style_map_.insert(element, computed_style);
}

StylePropertyMapReadOnly* Document::RemoveComputedStyleMapItem(
    Element* element) {
  StylePropertyMapReadOnly* computed_style =
      element_computed_style_map_.at(element);
  element_computed_style_map_.erase(element);
  return computed_style;
}

void Document::Trace(blink::Visitor* visitor) {
  visitor->Trace(imports_controller_);
  visitor->Trace(doc_type_);
  visitor->Trace(implementation_);
  visitor->Trace(autofocus_element_);
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
  visitor->Trace(lazy_load_image_observer_);
  Supplementable<Document>::Trace(visitor);
  TreeScope::Trace(visitor);
  ContainerNode::Trace(visitor);
  ExecutionContext::Trace(visitor);
  SecurityContext::Trace(visitor);
  DocumentShutdownNotifier::Trace(visitor);
  SynchronousMutationNotifier::Trace(visitor);
}

void Document::RecordDeferredLoadReason(WouldLoadReason reason) {
  DCHECK(would_load_reason_ == WouldLoadReason::kInvalid ||
         reason != WouldLoadReason::kCreated);
  DCHECK(reason != WouldLoadReason::kInvalid);
  DCHECK(GetFrame());
  DCHECK(GetFrame()->IsCrossOriginSubframe());
  if (reason <= would_load_reason_ ||
      !GetFrame()->Loader().StateMachine()->CommittedFirstRealDocumentLoad())
    return;
  for (int i = static_cast<int>(would_load_reason_) + 1;
       i <= static_cast<int>(reason); ++i)
    RecordLoadReasonToHistogram(static_cast<WouldLoadReason>(i));
  would_load_reason_ = reason;
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
    if (child->IsLocalFrame()) {
      if (ToLocalFrame(child)->IsAdSubframe()) {
        ToLocalFrame(child)->Navigate(
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
    slot_assignment_engine_ = SlotAssignmentEngine::Create();
  return *slot_assignment_engine_;
}

bool Document::IsSlotAssignmentOrLegacyDistributionDirty() {
  if (ChildNeedsDistributionRecalc())
    return true;
  if (GetSlotAssignmentEngine().HasPendingSlotAssignmentRecalc()) {
    return true;
  }
  return false;
}

bool Document::IsLazyLoadPolicyEnforced() const {
  return RuntimeEnabledFeatures::ExperimentalProductivityFeaturesEnabled() &&
         !GetFeaturePolicy()->IsFeatureEnabled(
             mojom::FeaturePolicyFeature::kLazyLoad);
}

LazyLoadImageObserver& Document::EnsureLazyLoadImageObserver() {
  if (!lazy_load_image_observer_)
    lazy_load_image_observer_ = new LazyLoadImageObserver();
  return *lazy_load_image_observer_;
}

void Document::ReportFeaturePolicyViolation(mojom::FeaturePolicyFeature feature,
                                            const String& message) const {
  if (!RuntimeEnabledFeatures::FeaturePolicyReportingEnabled())
    return;
  LocalFrame* frame = GetFrame();
  if (!frame)
    return;
  const String& feature_name = GetNameForFeature(feature);
  FeaturePolicyViolationReportBody* body = new FeaturePolicyViolationReportBody(
      feature_name, "Feature policy violation", SourceLocation::Capture());
  Report* report = new Report("feature-policy", Url().GetString(), body);
  ReportingContext::From(this)->QueueReport(report);

  bool is_null;
  int line_number = body->lineNumber(is_null);
  line_number = is_null ? 0 : line_number;
  int column_number = body->columnNumber(is_null);
  column_number = is_null ? 0 : column_number;

  // Send the feature policy violation report to the Reporting API.
  frame->GetReportingService()->QueueFeaturePolicyViolationReport(
      Url(), feature_name, "Feature policy violation", body->sourceFile(),
      line_number, column_number);
  frame->Console().AddMessage(ConsoleMessage::Create(
      kViolationMessageSource, kErrorMessageLevel,
      (message.IsEmpty() ? ("Feature policy violation: " + feature_name +
                            " is not allowed in this document.")
                         : message)));
}

void Document::IncrementNumberOfCanvases() {
  num_canvases_++;
}

void Document::SendViolationReport(
    mojom::blink::CSPViolationParamsPtr violation_params) {
  std::unique_ptr<SourceLocation> source_location = SourceLocation::Create(
      violation_params->source_location->url,
      violation_params->source_location->line_number,
      violation_params->source_location->column_number, nullptr);

  Vector<String> report_endpoints;
  for (const WebString& end_point : violation_params->report_endpoints)
    report_endpoints.push_back(end_point);

  AddConsoleMessage(ConsoleMessage::Create(kSecurityMessageSource,
                                           kErrorMessageLevel,
                                           violation_params->console_message));
  GetContentSecurityPolicy()->ReportViolation(
      violation_params->directive,
      ContentSecurityPolicy::GetDirectiveType(
          violation_params->effective_directive),
      violation_params->console_message, KURL(violation_params->blocked_url),
      report_endpoints, violation_params->use_reporting_api,
      violation_params->header,
      static_cast<ContentSecurityPolicyHeaderType>(
          violation_params->disposition),
      ContentSecurityPolicy::ViolationType::kURLViolation,
      std::move(source_location), nullptr /* LocalFrame */,
      violation_params->after_redirect ? RedirectStatus::kFollowedRedirect
                                       : RedirectStatus::kNoRedirect,
      nullptr /* Element */);
}

template class CORE_TEMPLATE_EXPORT Supplement<Document>;

}  // namespace blink

#ifndef NDEBUG
static WeakDocumentSet& liveDocumentSet() {
  DEFINE_STATIC_LOCAL(blink::Persistent<WeakDocumentSet>, set,
                      (new WeakDocumentSet));
  return *set;
}

void showLiveDocumentInstances() {
  WeakDocumentSet& set = liveDocumentSet();
  fprintf(stderr, "There are %u documents currently alive:\n", set.size());
  for (blink::Document* document : set)
    fprintf(stderr, "- Document %p URL: %s\n", document,
            document->Url().GetString().Utf8().data());
}
#endif
