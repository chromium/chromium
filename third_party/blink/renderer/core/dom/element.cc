/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012, 2013 Apple Inc.
 * All rights reserved.
 *           (C) 2007 Eric Seidel (eric@webkit.org)
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

#include "third_party/blink/renderer/core/dom/element.h"

#include <algorithm>
#include <bitset>
#include <limits>
#include <memory>
#include <utility>

#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "cc/input/snap_selection_strategy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/public/web/web_autofill_state.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/frozen_array.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_aria_notification_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_check_visibility_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_get_animations_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_keyframe_animation_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pointer_lock_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_container.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_into_view_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_to_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_set_html_unsafe_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shadow_root_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_range.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_range_offset.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_boolean_scrollintoviewoptions.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_keyframeanimationoptions_unrestricteddouble.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_keyframeeffectoptions_unrestricteddouble.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_timelinerangeoffset.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_stringlegacynulltoemptystring_trustedhtml.h"
#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/effect_input.h"
#include "third_party/blink/renderer/core/animation/effect_model.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/animation/timing_input.h"
#include "third_party/blink/renderer/core/css/container_query_data.h"
#include "third_party/blink/renderer/core/css/container_query_evaluator.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_selector_watch.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/cssom/inline_style_property_map.h"
#include "third_party/blink/renderer/core/css/native_paint_image_generator.h"
#include "third_party/blink/renderer/core/css/out_of_flow_data.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"
#include "third_party/blink/renderer/core/css/post_style_update_scope.h"
#include "third_party/blink/renderer/core/css/property_set_css_style_declaration.h"
#include "third_party/blink/renderer/core/css/resolver/css_to_style_map.h"
#include "third_party/blink/renderer/core/css/resolver/selector_filter_parent_scope.h"
#include "third_party/blink/renderer/core/css/resolver/style_adjuster.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_stats.h"
#include "third_party/blink/renderer/core/css/selector_query.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_containment_scope.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/column_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/dataset_dom_string_map.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/dom/document_part_root.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/element_data_cache.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_vector.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_result.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/indexed_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/interest_invoker_target_data.h"
#include "third_party/blink/renderer/core/dom/invalidate_node_list_caches_scope.h"
#include "third_party/blink/renderer/core/dom/invoker_data.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder.h"
#include "third_party/blink/renderer/core/dom/mutation_observer_interest_group.h"
#include "third_party/blink/renderer/core/dom/mutation_record.h"
#include "third_party/blink/renderer/core/dom/named_node_map.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_cloning_data.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/overscroll_pseudo_element_data.h"
#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/dom/presentation_attribute_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_group_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/whitespace_attacher.h"
#include "third_party/blink/renderer/core/editing/commands/undo_stack.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/edit_context.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/focus_event.h"
#include "third_party/blink/renderer/core/events/gesture_event.h"
#include "third_party/blink/renderer/core/events/interest_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_list.h"
#include "third_party/blink/renderer/core/html/anchor_element_observer.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/display_ad_element_monitor.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_field_set_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_controls_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_options_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/html_menu_item_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/html_quote_element.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html/html_table_rows_collection.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/nesting_level_incrementer.h"
#include "third_party/blink/renderer/core/html/parser/html_element_stack.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_utils.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/intersection_observer/element_intersection_observer_data.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/forms/layout_fieldset.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/loader/render_blocking_resource_manager.h"
#include "third_party/blink/renderer/core/overscroll/overscroll_area_tracker.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/sync_scroll_attempt_heuristic.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observation.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_size.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer_api.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/style_interest_delay.h"
#include "third_party/blink/renderer/core/svg/svg_a_element.h"
#include "third_party/blink/renderer/core/svg/svg_animated_href.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_stop_element.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_use_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_pseudo_element_base.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_transition_element.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_activity_logger.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/region_capture_crop_id.h"
#include "third_party/blink/renderer/platform/restriction_target_id.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/bidi_paragraph.h"
#include "third_party/blink/renderer/platform/text/writing_mode_utils.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

namespace {

class DisplayLockStyleScope {
  STACK_ALLOCATED();

 public:
  explicit DisplayLockStyleScope(Element* element) : element_(element) {
    // Note that we don't store context as a member of this scope, since it may
    // get created as part of element self style recalc.
  }

  ~DisplayLockStyleScope() {
    if (auto* context = element_->GetDisplayLockContext()) {
      if (did_update_children_) {
        context->DidStyleChildren();
        if (auto* document_rules = DocumentSpeculationRules::FromIfExists(
                element_->GetDocument())) {
          document_rules->DidStyleChildren(element_);
        }
      }
    }
  }

  bool ShouldUpdateChildStyle() const {
    // We can't calculate this on construction time, because the element's lock
    // state may changes after self-style calculation ShouldStyle(children).
    auto* context = element_->GetDisplayLockContext();
    return !context || context->ShouldStyleChildren();
  }
  void DidUpdateChildStyle() { did_update_children_ = true; }
  // Returns true if the element was force unlocked due to missing requirements.
  StyleRecalcChange DidUpdateSelfStyle(StyleRecalcChange change) {
    if (auto* context = element_->GetDisplayLockContext()) {
      context->DidStyleSelf();
      // After we notified context that we styled self, it may cause an unlock /
      // modification to the blocked style change, so accumulate the change here
      // again. Note that if the context is locked we will restore it as the
      // blocked style change in RecalcStyle.
      return change.Combine(context->TakeBlockedStyleRecalcChange());
    }
    return change;
  }

  void NotifyChildStyleRecalcWasBlocked(const StyleRecalcChange& change) {
    DCHECK(!ShouldUpdateChildStyle());
    // The only way to be blocked here is if we have a display lock context.
    DCHECK(element_->GetDisplayLockContext());

    element_->GetDisplayLockContext()->NotifyChildStyleRecalcWasBlocked(change);
    if (auto* document_rules =
            DocumentSpeculationRules::FromIfExists(element_->GetDocument())) {
      document_rules->ChildStyleRecalcBlocked(element_);
    }
  }

  bool IsLockedContentVisibilityAuto() const {
    auto* context = element_->GetDisplayLockContext();
    return context && context->IsLocked() && context->IsAuto();
  }

 private:
  Element* element_;
  bool did_update_children_ = false;
};

ScrollDirectionPhysical GetPhysicalDirectionForCommand(
    CommandEventType command,
    const ComputedStyle& style) {
  if (command == CommandEventType::kPageUp) {
    return kScrollUp;
  } else if (command == CommandEventType::kPageDown) {
    return kScrollDown;
  } else if (command == CommandEventType::kPageLeft) {
    return kScrollLeft;
  } else if (command == CommandEventType::kPageRight) {
    return kScrollRight;
  } else {
    LogicalToPhysical<bool> mapping(
        style.GetWritingDirection(),
        command == CommandEventType::kPageInlineStart,
        command == CommandEventType::kPageInlineEnd,
        command == CommandEventType::kPageBlockStart,
        command == CommandEventType::kPageBlockEnd);

    if (mapping.Top()) {
      return kScrollUp;
    } else if (mapping.Bottom()) {
      return kScrollDown;
    } else if (mapping.Left()) {
      return kScrollLeft;
    } else if (mapping.Right()) {
      return kScrollRight;
    }
  }
  NOTREACHED();
}

bool IsRootEditableElementWithCounting(const Element& element) {
  bool is_editable = IsRootEditableElement(element);
  Document& doc = element.GetDocument();
  if (!doc.IsActive()) {
    return is_editable;
  }
  // -webkit-user-modify doesn't affect text control elements.
  if (element.IsTextControl()) {
    return is_editable;
  }
  const auto* style = element.GetComputedStyle();
  if (!style) {
    return is_editable;
  }
  auto user_modify = style->UsedUserModify();
  AtomicString ce_value =
      element.FastGetAttribute(html_names::kContenteditableAttr).LowerASCII();
  if (ce_value.IsNull() || ce_value == keywords::kFalse) {
    if (user_modify == EUserModify::kReadWritePlaintextOnly) {
      UseCounter::Count(doc, WebFeature::kPlainTextEditingEffective);
      UseCounter::Count(doc, WebFeature::kWebKitUserModifyPlainTextEffective);
      UseCounter::Count(doc, WebFeature::kWebKitUserModifyEffective);
    } else if (user_modify == EUserModify::kReadWrite) {
      UseCounter::Count(doc, WebFeature::kWebKitUserModifyReadWriteEffective);
      UseCounter::Count(doc, WebFeature::kWebKitUserModifyEffective);
    }
  } else if (ce_value.empty() || ce_value == keywords::kTrue) {
    if (user_modify == EUserModify::kReadWritePlaintextOnly) {
      UseCounter::Count(doc, WebFeature::kPlainTextEditingEffective);
      UseCounter::Count(doc, WebFeature::kWebKitUserModifyPlainTextEffective);
      UseCounter::Count(doc, WebFeature::kWebKitUserModifyEffective);
    } else if (user_modify == EUserModify::kReadOnly) {
      UseCounter::Count(doc, WebFeature::kWebKitUserModifyReadOnlyEffective);
      UseCounter::Count(doc, WebFeature::kWebKitUserModifyEffective);
    }
  } else if (ce_value == keywords::kPlaintextOnly) {
    UseCounter::Count(doc, WebFeature::kPlainTextEditingEffective);
    if (user_modify == EUserModify::kReadWrite) {
      UseCounter::Count(doc, WebFeature::kWebKitUserModifyReadWriteEffective);
      UseCounter::Count(doc, WebFeature::kWebKitUserModifyEffective);
    } else if (user_modify == EUserModify::kReadOnly) {
      UseCounter::Count(doc, WebFeature::kWebKitUserModifyReadOnlyEffective);
      UseCounter::Count(doc, WebFeature::kWebKitUserModifyEffective);
    }
  }
  return is_editable;
}

bool HasLeftwardDirection(const Element& element) {
  auto* style = element.GetComputedStyle();
  if (!style) {
    return false;
  }

  const auto writing_direction = style->GetWritingDirection();
  return writing_direction.InlineEnd() == PhysicalDirection::kLeft ||
         writing_direction.BlockEnd() == PhysicalDirection::kLeft;
}

bool HasUpwardDirection(const Element& element) {
  auto* style = element.GetComputedStyle();
  if (!style) {
    return false;
  }

  const auto writing_direction = style->GetWritingDirection();
  return writing_direction.InlineEnd() == PhysicalDirection::kUp ||
         writing_direction.BlockEnd() == PhysicalDirection::kUp;
}

// TODO(meredithl): Automatically generate this method once the IDL compiler has
// been refactored. See http://crbug.com/839389 for details.
bool IsElementReflectionAttribute(const QualifiedName& name) {
  if (name == html_names::kAriaActivedescendantAttr) {
    return true;
  }
  if (name == html_names::kAriaControlsAttr) {
    return true;
  }
  if (name == html_names::kAriaDescribedbyAttr) {
    return true;
  }
  if (name == html_names::kAriaDetailsAttr) {
    return true;
  }
  if (name == html_names::kAriaErrormessageAttr) {
    return true;
  }
  if (name == html_names::kAriaFlowtoAttr) {
    return true;
  }
  if (name == html_names::kAriaLabeledbyAttr) {
    return true;
  }
  if (name == html_names::kAriaLabelledbyAttr) {
    return true;
  }
  if (name == html_names::kAriaOwnsAttr) {
    return true;
  }
  if (name == html_names::kPopovertargetAttr) {
    return true;
  }
  if (name == html_names::kAnchorAttr) {
    return true;
  }
  if (name == html_names::kCommandforAttr) {
    return true;
  }
  if (name == html_names::kInterestforAttr) {
    return true;
  }
  if (name == html_names::kSelectedcontentelementAttr) {
    return true;
  }
  return false;
}

// Checks that the given element |candidate| is a descendant of
// |attribute_element|'s  shadow including ancestors.
bool ElementIsDescendantOfShadowIncludingAncestor(
    const Element& attribute_element,
    const Element& candidate) {
  auto* candidate_root = &candidate.TreeRoot();
  auto* element_root = &attribute_element.TreeRoot();
  while (true) {
    if (candidate_root == element_root) {
      return true;
    }
    if (!element_root->IsInShadowTree()) {
      return false;
    }
    element_root = &element_root->OwnerShadowHost()->TreeRoot();
  }
}

// The first algorithm in
// https://html.spec.whatwg.org/C/#the-autofocus-attribute
void EnqueueAutofocus(Element& element) {
  // When an element with the autofocus attribute specified is inserted into a
  // document, run the following steps:
  DCHECK(element.isConnected());
  if (!element.IsAutofocusable()) {
    return;
  }

  // 1. If the user has indicated (for example, by starting to type in a form
  // control) that they do not wish focus to be changed, then optionally return.

  // We don't implement this optional step. If other browsers have such
  // behavior, we should follow it or standardize it.

  // 2. Let target be the element's node document.
  Document& doc = element.GetDocument();
  LocalDOMWindow* window = doc.domWindow();

  // 3. If target's browsing context is null, then return.
  if (!window) {
    return;
  }

  // 4. If target's active sandboxing flag set has the sandboxed automatic
  // features browsing context flag, then return.
  if (window->IsSandboxed(
          network::mojom::blink::WebSandboxFlags::kAutomaticFeatures)) {
    window->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        StrCat({"Blocked autofocusing on a <", element.TagQName().ToString(),
                "> element because the element's frame is sandboxed and the "
                "'allow-scripts' permission is not set."})));
    return;
  }

  // 5. For each ancestorBC of target's browsing context's ancestor browsing
  // contexts: if ancestorBC's active document's origin is not same origin with
  // target's origin, then return.
  for (Frame* frame = doc.GetFrame(); frame; frame = frame->Parent()) {
    if (!frame->IsCrossOriginToOutermostMainFrame()) {
      continue;
    }
    window->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        StrCat({"Blocked autofocusing on a <", element.TagQName().ToString(),
                "> element in a cross-origin subframe."})));
    return;
  }

  // 6. Let topDocument be the active document of target's browsing context's
  // top-level browsing context.
  Document& top_document = element.GetDocument().TopDocument();

  top_document.EnqueueAutofocusCandidate(element);
}

bool WillUpdateSizeContainerDuringLayout(const LayoutObject& layout_object) {
  // When a size-container LayoutObject is marked as needs layout,
  // BlockNode::Layout() will resume style recalc with an up-to-date size in
  // StyleEngine::UpdateStyleAndLayoutTreeForSizeContainer().
  return layout_object.NeedsLayout() &&
         layout_object.IsEligibleForSizeContainment();
}

bool IsValidShadowHostName(const AtomicString& local_name) {
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, shadow_root_tags,
                      ({
                          html_names::kArticleTag.LocalName(),
                          html_names::kAsideTag.LocalName(),
                          html_names::kBlockquoteTag.LocalName(),
                          html_names::kBodyTag.LocalName(),
                          html_names::kDivTag.LocalName(),
                          html_names::kFooterTag.LocalName(),
                          html_names::kH1Tag.LocalName(),
                          html_names::kH2Tag.LocalName(),
                          html_names::kH3Tag.LocalName(),
                          html_names::kH4Tag.LocalName(),
                          html_names::kH5Tag.LocalName(),
                          html_names::kH6Tag.LocalName(),
                          html_names::kHeaderTag.LocalName(),
                          html_names::kNavTag.LocalName(),
                          html_names::kMainTag.LocalName(),
                          html_names::kPTag.LocalName(),
                          html_names::kSectionTag.LocalName(),
                          html_names::kSpanTag.LocalName(),
                      }));
  return shadow_root_tags.Contains(local_name);
}

const AtomicString& V8ShadowRootModeToString(V8ShadowRootMode::Enum mode) {
  if (mode == V8ShadowRootMode::Enum::kOpen) {
    return keywords::kOpen;
  }
  return keywords::kClosed;
}

}  // namespace

Element::Element(const QualifiedName& tag_name,
                 Document* document,
                 ConstructionType type)
    : ContainerNode(document, type),
      tag_name_(tag_name),
      computed_style_(nullptr) {}

Element* Element::GetAnimationTarget() {
  return this;
}

namespace {

V8UnionKeyframeEffectOptionsOrUnrestrictedDouble* CoerceEffectOptions(
    const V8UnionKeyframeAnimationOptionsOrUnrestrictedDouble* options) {
  switch (options->GetContentType()) {
    case V8UnionKeyframeAnimationOptionsOrUnrestrictedDouble::ContentType::
        kKeyframeAnimationOptions:
      return MakeGarbageCollected<
          V8UnionKeyframeEffectOptionsOrUnrestrictedDouble>(
          options->GetAsKeyframeAnimationOptions());
    case V8UnionKeyframeAnimationOptionsOrUnrestrictedDouble::ContentType::
        kUnrestrictedDouble:
      return MakeGarbageCollected<
          V8UnionKeyframeEffectOptionsOrUnrestrictedDouble>(
          options->GetAsUnrestrictedDouble());
  }
  NOTREACHED();
}

}  // namespace

// https://w3.org/TR/web-animations-1/#dom-animatable-animate
Animation* Element::animate(
    ScriptState* script_state,
    const ScriptValue& keyframes,
    const V8UnionKeyframeAnimationOptionsOrUnrestrictedDouble* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    return nullptr;
  }
  Element* element = GetAnimationTarget();
  if (!element->GetExecutionContext()) {
    return nullptr;
  }
  KeyframeEffect* effect =
      KeyframeEffect::Create(script_state, element, keyframes,
                             CoerceEffectOptions(options), exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  // Creation of the keyframe effect parses JavaScript, which could result
  // in destruction of the execution context. Recheck that it is still valid.
  if (!element->GetExecutionContext()) {
    return nullptr;
  }

  if (!options->IsKeyframeAnimationOptions()) {
    return element->GetDocument().Timeline().Play(effect, exception_state);
  }

  Animation* animation;
  const KeyframeAnimationOptions* options_dict =
      options->GetAsKeyframeAnimationOptions();
  if (!options_dict->hasTimeline()) {
    animation = element->GetDocument().Timeline().Play(effect, exception_state);
  } else if (AnimationTimeline* timeline = options_dict->timeline()) {
    animation = timeline->Play(effect, exception_state);
  } else {
    animation = Animation::Create(element->GetExecutionContext(), effect,
                                  nullptr, exception_state);
  }

  if (!animation) {
    return nullptr;
  }

  animation->setId(options_dict->id());

  // ViewTimeline options.
  if (options_dict->hasRangeStart()) {
    animation->SetRangeStartInternal(TimelineOffset::Create(
        element, options_dict->rangeStart(), 0, exception_state));
  }
  if (options_dict->hasRangeEnd()) {
    animation->SetRangeEndInternal(TimelineOffset::Create(
        element, options_dict->rangeEnd(), 100, exception_state));
  }
  return animation;
}

// https://w3.org/TR/web-animations-1/#dom-animatable-animate
Animation* Element::animate(ScriptState* script_state,
                            const ScriptValue& keyframes,
                            ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    return nullptr;
  }
  Element* element = GetAnimationTarget();
  if (!element->GetExecutionContext()) {
    return nullptr;
  }
  KeyframeEffect* effect =
      KeyframeEffect::Create(script_state, element, keyframes, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  // Creation of the keyframe effect parses JavaScript, which could result
  // in destruction of the execution context. Recheck that it is still valid.
  if (!element->GetExecutionContext()) {
    return nullptr;
  }

  return element->GetDocument().Timeline().Play(effect, exception_state);
}

// https://w3.org/TR/web-animations-1/#dom-animatable-getanimations
HeapVector<Member<Animation>> Element::getAnimations(
    GetAnimationsOptions* options) {
  bool use_subtree = options && options->subtree();
  return GetAnimationsInternal(
      GetAnimationsOptionsResolved{.use_subtree = use_subtree});
}

HeapVector<Member<Animation>> Element::GetAnimationsInternal(
    GetAnimationsOptionsResolved options) {
  Element* element = GetAnimationTarget();
  if (options.use_subtree) {
    element->GetDocument().UpdateStyleAndLayoutTreeForSubtree(
        element, DocumentUpdateReason::kWebAnimation);
  } else {
    element->GetDocument().UpdateStyleAndLayoutTreeForElement(
        element, DocumentUpdateReason::kWebAnimation);
  }

  HeapVector<Member<Animation>> animations;
  if (!options.use_subtree && !element->HasAnimations()) {
    return animations;
  }

  for (const auto& animation :
       element->GetDocument().GetDocumentAnimations().getAnimations(
           element->GetTreeScope())) {
    DCHECK(animation->effect());
    // TODO(gtsteel) make this use the idl properties
    Element* target = To<KeyframeEffect>(animation->effect())->EffectTarget();
    if (element == target ||
        (options.use_subtree && element->contains(target))) {
      // DocumentAnimations::getAnimations should only give us animations that
      // are either current or in effect.
      DCHECK(animation->effect()->IsCurrent() ||
             animation->effect()->IsInEffect());
      animations.push_back(animation);
    }
  }
  return animations;
}

bool Element::HasElementFlag(ElementFlags mask) const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->HasElementFlag(mask);
  }
  return false;
}

void Element::SetElementFlag(ElementFlags mask, bool value) {
  if (ElementRareDataVector* data = GetElementRareData()) {
    data->SetElementFlag(mask, value);
  } else if (value) {
    EnsureElementRareData().SetElementFlag(mask, value);
  }
}

void Element::ClearElementFlag(ElementFlags mask) {
  if (ElementRareDataVector* data = GetElementRareData()) {
    data->ClearElementFlag(mask);
  }
}

void Element::ClearTabIndexExplicitlyIfNeeded() {
  if (ElementRareDataVector* data = GetElementRareData()) {
    data->ClearTabIndexExplicitly();
  }
}

void Element::SetTabIndexExplicitly() {
  EnsureElementRareData().SetTabIndexExplicitly();
}

void Element::setTabIndex(int value) {
  SetIntegralAttribute(html_names::kTabindexAttr, value);
}

int Element::tabIndex() const {
  // https://html.spec.whatwg.org/C/#dom-tabindex
  // The tabIndex IDL attribute must reflect the value of the tabindex content
  // attribute. The default value is 0 if the element is an a, area, button,
  // frame, iframe, input, object, select, textarea, or SVG a element, or is a
  // summary element that is a summary for its parent details. The default value
  // is −1 otherwise.
  return GetIntegralAttribute(html_names::kTabindexAttr, DefaultTabIndex());
}

int Element::DefaultTabIndex() const {
  return -1;
}

bool Element::WasLastFocusFromUserGestureInternal() const {
  return GetElementRareData()->WasLastFocusFromUserGesture();
}

const HeapVector<Member<Node>> Element::ReadingFlowChildren() const {
  HeapVector<Member<Node>> children;
  const Element* layout_parent =
      HasDisplayContentsStyle()
          ? LayoutTreeBuilderTraversal::LayoutParentElement(*this)
          : this;
  if (!layout_parent || !layout_parent->IsReadingFlowContainer()) {
    return children;
  }
  // Add all reading flow items first, in the reading flow order.
  for (Node* reading_flow_item :
       layout_parent->GetLayoutBox()->ReadingFlowNodes()) {
    // reading_flow_item or its ancestor (display: contents) might not be a
    // direct child of this. Loop the parents and only add the node if its
    // FlatTreeTraversal::ParentElement is this element.
    while (reading_flow_item) {
      auto* parent = FlatTreeTraversal::ParentElement(*reading_flow_item);
      if (parent == this) {
        // TODO(dizhangg) this check is O(n^2)
        if (!children.Contains(reading_flow_item)) {
          children.push_back(reading_flow_item);
        }
        break;
      }
      reading_flow_item = parent;
    }
  }
  // Add all non-reading flow items at the end of the reading flow.
  // We use LayoutTreeBuilder traversal to make sure all pseudo-elements
  // (including scroll markers) are accounted for.
  for (Node* child = LayoutTreeBuilderTraversal::FirstChild(*this); child;
       child = LayoutTreeBuilderTraversal::NextSibling(*child)) {
    // TODO(dizhangg) this check is O(n^2)
    if (!children.Contains(child)) {
      children.push_back(child);
    }
  }
  // Now that we have a complete list of children, sort them by reading-order.
  auto reading_order = [](Node* node) {
    if (const Element* el = DynamicTo<Element>(node)) {
      if (const ComputedStyle* style = el->GetComputedStyle()) {
        return style->ReadingOrder();
      }
    }
    return 0;
  };
  std::stable_sort(children.begin(), children.end(),
                   [&reading_order](const auto& lhs, const auto& rhs) {
                     return reading_order(lhs) < reading_order(rhs);
                   });
  return children;
}

bool Element::IsFocusableStyle(UpdateBehavior update_behavior) const {
  // TODO(vmpstr): Note that this may be called by accessibility during layout
  // tree attachment, at which point we might not have cleared all of the dirty
  // bits to ensure that the layout tree doesn't need an update. This should be
  // fixable by deferring AX tree updates as a separate phase after layout tree
  // attachment has happened. At that point `InStyleRecalc()` portion of the
  // following DCHECK can be removed.

  // In order to check focusable style, we use the existence of LayoutObjects
  // as a proxy for determining whether the element would have a display mode
  // that restricts visibility (such as display: none). However, with
  // display-locking, it is possible that we deferred such LayoutObject
  // creation. We need to ensure to update style and layout tree to have
  // up-to-date information.
  //
  // Note also that there may be situations where focus / keyboard navigation
  // causes us to have dirty style, so we update StyleAndLayoutTreeForNode here.
  // If the style and layout tree are clean, then this should be a quick
  // operation. See crbug.com/1079385 for details.
  //
  // Also note that if this node is ignored due to a display lock for focus
  // activation reason, we simply return false to avoid updating style & layout
  // tree for this node.
  if (DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
          *this, DisplayLockActivationReason::kUserFocus)) {
    return false;
  }
  if (update_behavior == UpdateBehavior::kStyleAndLayout) {
    GetDocument().UpdateStyleAndLayoutTreeForElement(
        this, DocumentUpdateReason::kFocus, /*only_cv_auto=*/true);
    // Look for ancestors with DisplayLockContext again since the previous
    // update to style and layout may have created one.
    if (DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
            *this, DisplayLockActivationReason::kUserFocus)) {
      return false;
    }
  } else {
    DCHECK(!NeedsStyleRecalc()) << this;
  }
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      GetDocument().Lifecycle());

  DCHECK(
      !GetDocument().IsActive() || GetDocument().InStyleRecalc() ||
      !GetDocument().NeedsLayoutTreeUpdateForNodeIncludingDisplayLocked(*this))
      << *this;

  if (LayoutObject* layout_object = GetLayoutObject()) {
    return layout_object->StyleRef().IsFocusable();
  }

  if (HasDisplayContentsStyle() &&
      RuntimeEnabledFeatures::DisplayContentsFocusableEnabled()) {
    if (const ComputedStyle* style =
            ComputedStyle::NullifyEnsured(GetComputedStyle())) {
      return style->IsFocusable();
    }
  }

  // If a canvas represents embedded content, its descendants are not rendered.
  // But they are still allowed to be focusable as long as their style allows
  // focus, their canvas is rendered, and its style allows focus.
  if (IsCanvasOrInCanvasSubtree()) {
    const ComputedStyle* style = GetComputedStyle();
    if (!style || !style->IsFocusable()) {
      return false;
    }

    const HTMLCanvasElement* canvas = nullptr;
    for (const Element* element = this; element;
         element = element->ParentOrShadowHostElement()) {
      if ((canvas = DynamicTo<HTMLCanvasElement>(element))) {
        break;
      }
    }
    DCHECK(canvas);
    if (LayoutObject* layout_object = canvas->GetLayoutObject()) {
      return layout_object->IsCanvas() &&
             layout_object->StyleRef().IsFocusable();
    }
  }

  return false;
}

constexpr unsigned kMinHeadingOffset = 0u;
// 9 is the maximum heading level recommended by the ARIA
// specification. See https://w3c.github.io/aria/#aria-level
// See also AXNodeObject::HeadingLevel():
// https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/modules/accessibility/ax_node_object.cc;l=3225;drc=d99d45a2124f7075b201e6bf5db39fec8971d583
constexpr unsigned kMaxHeadingOffset = 9u;

void Element::setHeadingOffset(int value) {
  DCHECK(RuntimeEnabledFeatures::HeadingOffsetEnabled());
  SetUnsignedIntegralAttribute(html_names::kHeadingoffsetAttr, value,
                               kMinHeadingOffset);
}

int Element::headingOffset() const {
  DCHECK(RuntimeEnabledFeatures::HeadingOffsetEnabled());
  const AtomicString& headingoffset_value =
      FastGetAttribute(html_names::kHeadingoffsetAttr);
  unsigned value = 0;
  if (!ParseHTMLClampedNonNegativeInteger(
          headingoffset_value, kMinHeadingOffset, kMaxHeadingOffset, value)) {
    return kMinHeadingOffset;
  }
  return value;
}

void Element::setHeadingReset(bool value) {
  DCHECK(RuntimeEnabledFeatures::HeadingOffsetEnabled());
  SetBooleanAttribute(html_names::kHeadingresetAttr, value);
}

bool Element::headingReset() const {
  DCHECK(RuntimeEnabledFeatures::HeadingOffsetEnabled());
  // Modal dialogs must always reset the headingoffset, because they
  // effectively act like "mini windows", so an h1 at the root of the
  // dialog must be an h1.
  if (auto* dialog = DynamicTo<HTMLDialogElement>(this)) {
    return dialog->IsModal();
  }
  return FastHasAttribute(html_names::kHeadingresetAttr);
}

int Element::GetComputedHeadingOffset(int max_offset) {
  if (!RuntimeEnabledFeatures::HeadingOffsetEnabled()) {
    return 0;
  }

  int offset = this->headingOffset();
  if (headingReset()) {
    return std::min(max_offset, offset);
  }

  Node* ancestor = this;
  while (ancestor) {
    ancestor = FlatTreeTraversal::Parent(*ancestor);
    auto* element = DynamicTo<Element>(ancestor);
    if (!element || element->headingReset()) {
      // When encountering a headingreset, it's important to return the
      // existing accumulated offset, to allow for deeply nested trees having
      // children with headingoffsets. Returning to 0 would create a confusing
      // structure for authors. Consider a deeply nested structure like:
      //
      // <div headingreset>
      //   <h1>First Heading</h1>
      //   <div headingoffset=1>
      //     <h1>Second Heading</h1>
      //   </div>
      // </div>
      //
      // The "First Heading" should be an h1 (h1+reset), but importantly
      // the "Second Heading" should be an h2 (h1+offset=1+reset). As it walks
      // the tree, it will accumulate the `headingoffset=1`, then land on the
      // headingreset, where we hit this line of logic. Our choices are to
      // return either `0` or `1` (the accumulated offset). Returning `0` makes
      // "Second Heading" an h1, which is the incorrect heading for the ideal
      // document structure here. Returning the accumulated offset of 1 makes
      // it an h2 which is a better heading structure.
      // Ultimately, `headingreset` may be a poor choice of name, and perhaps
      // something more like `headingoffsetboundary` better describes the logic
      // of this attribute.
      return std::min(offset, max_offset);
    }
    offset += element->headingOffset();
    if (offset >= max_offset) {
      return max_offset;
    }
  }
  return std::min(offset, max_offset);
}

Node* Element::Clone(Document& factory,
                     NodeCloningData& data,
                     ContainerNode* append_to,
                     CustomElementRegistry* fallback_registry,
                     ExceptionState& append_exception_state) const {
  Element* copy;
  CustomElementRegistry* registry = nullptr;
  if (RuntimeEnabledFeatures::ScopedCustomElementRegistryEnabled()) {
    // 2-1. Let registry be node's custom element registry.
    // 2-2. If registry is null, then set registry to fallbackRegistry
    if (auto* node_registry = customElementRegistry()) {
      registry = node_registry;
    } else {
      registry = fallback_registry;
    }
    // 2-3. If registry is a global custom element registry, then set
    // registry to document's effective global custom element registry.
    if (registry && registry->IsGlobalRegistry()) {
      registry = factory.customElementRegistry();
    }
  }
  if (!data.Has(CloneOption::kIncludeDescendants)) {
    copy = &CloneWithoutChildren(data, registry, &factory);
    if (append_to) {
      append_to->AppendChild(copy, append_exception_state);
    }
  } else {
    copy = &CloneWithChildren(data, &factory, append_to, registry,
                              append_exception_state);
  }
  // 6. If node is a shadow host whose shadow root’s clonable is true:
  auto* shadow_root = GetShadowRoot();
  if (!shadow_root) {
    return copy;
  }
  if (shadow_root->clonable()) {
    if (shadow_root->GetMode() == ShadowRootMode::kOpen ||
        shadow_root->GetMode() == ShadowRootMode::kClosed) {
      CustomElementRegistry* shadow_root_registry = nullptr;
      if (RuntimeEnabledFeatures::ScopedCustomElementRegistryEnabled()) {
        // 6.2 Let shadowRootRegistry be node's shadow root's custom element
        // registry
        if (auto* node_registry = shadow_root->customElementRegistry()) {
          shadow_root_registry = node_registry;
        }
        // 6.3 If shadowRootRegistry is a global custom element registry, then
        // set shadowRootRegistry to document's effective global custom element
        // registry
        if (shadow_root_registry && shadow_root_registry->IsGlobalRegistry()) {
          shadow_root_registry = factory.customElementRegistry();
        }
      }
      // 6.4 Run attach a shadow root with copy, node's shadow root's mode,
      // true, node’s shadow root’s delegates focus, and node’s shadow root’s
      // slot assignment.
      // TODO(crbug.com/1523816): it seems like the `registry` parameter should
      // not always be nullptr.
      ShadowRoot& cloned_shadow_root = copy->AttachShadowRootInternal(
          shadow_root->GetMode(),
          shadow_root->delegatesFocus() ? FocusDelegation::kDelegateFocus
                                        : FocusDelegation::kNone,
          shadow_root->GetSlotAssignmentMode(), shadow_root_registry,
          shadow_root->serializable(),
          /*clonable*/ true, shadow_root->referenceTarget());

      // 6.5 Set copy’s shadow root’s declarative to node’s shadow root’s
      // declarative.
      cloned_shadow_root.SetIsDeclarativeShadowRoot(
          shadow_root->IsDeclarativeShadowRoot());

      // This step is not currently spec'd.
      cloned_shadow_root.SetAvailableToElementInternals(
          shadow_root->IsAvailableToElementInternals());

      // 6.6 If the clone children flag is set, then for each child child of
      // node’s shadow root, in tree order: append the result of cloning child
      // with document and the clone children flag set, to copy’s shadow root.
      NodeCloningData shadow_data{CloneOption::kIncludeDescendants};
      cloned_shadow_root.CloneChildNodesFrom(*shadow_root, shadow_data,
                                             /*fallback_registry*/ nullptr);
    }
  }
  return copy;
}

Element& Element::CloneWithChildren(
    NodeCloningData& data,
    Document* nullable_factory,
    ContainerNode* append_to,
    CustomElementRegistry* registry,
    ExceptionState& append_exception_state) const {
  InvalidateNodeListCachesScope deferred_invalidation_scope(GetDocument());
  Element& clone = CloneWithoutAttributesAndChildren(
      nullable_factory ? *nullable_factory : GetDocument(), registry);
  // This will catch HTML elements in the wrong namespace that are not correctly
  // copied.  This is a sanity check as HTML overloads some of the DOM methods.
  DCHECK_EQ(IsHTMLElement(), clone.IsHTMLElement());

  clone.CloneAttributesFrom(*this);
  clone.CloneNonAttributePropertiesFrom(*this, data);
  if (data.Has(CloneOption::kPreserveDOMPartsMinimalAPI) && HasNodePart()) {
    DCHECK(RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled());
    clone.SetHasNodePart();
  } else if (data.Has(CloneOption::kPreserveDOMParts)) {
    PartRoot::CloneParts(*this, clone, data);
  }

  // Append the clone to its parent first, before cloning children. If this is
  // done in the reverse order, each new child will receive treeDepth calls to
  // Node::InsertedInto().
  if (append_to) {
    append_to->AppendChild(&clone, append_exception_state);
  }
  clone.CloneChildNodesFrom(*this, data, registry);
  return clone;
}

Element& Element::CloneWithoutChildren() const {
  NodeCloningData data;
  return CloneWithoutChildren(data, /*registry*/ nullptr);
}

Element& Element::CloneWithoutChildren(NodeCloningData& data,
                                       CustomElementRegistry* registry,
                                       Document* nullable_factory) const {
  Element& clone = CloneWithoutAttributesAndChildren(
      nullable_factory ? *nullable_factory : GetDocument(), registry);
  // This will catch HTML elements in the wrong namespace that are not correctly
  // copied.  This is a sanity check as HTML overloads some of the DOM methods.
  DCHECK_EQ(IsHTMLElement(), clone.IsHTMLElement());

  clone.CloneAttributesFrom(*this);
  clone.CloneNonAttributePropertiesFrom(*this, data);
  if (data.Has(CloneOption::kPreserveDOMPartsMinimalAPI) && HasNodePart()) {
    DCHECK(RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled());
    clone.SetHasNodePart();
  } else if (data.Has(CloneOption::kPreserveDOMParts)) {
    PartRoot::CloneParts(*this, clone, data);
  }
  return clone;
}

Element& Element::CloneWithoutAttributesAndChildren(
    Document& factory,
    CustomElementRegistry* registry) const {
  return *factory.CreateElement(TagQName(), CreateElementFlags::ByCloneNode(),
                                IsValue(), registry);
}

Attr* Element::DetachAttribute(wtf_size_t index) {
  DCHECK(HasElementData());
  const Attribute& attribute = GetElementData()->Attributes().at(index);
  Attr* attr_node = AttrIfExists(attribute.GetName());
  if (attr_node) {
    DetachAttrNodeAtIndex(attr_node, index);
  } else {
    attr_node = MakeGarbageCollected<Attr>(GetDocument(), attribute.GetName(),
                                           attribute.Value());
    RemoveAttributeInternal(index, AttributeModificationReason::kDirectly);
  }
  return attr_node;
}

void Element::DetachAttrNodeAtIndex(Attr* attr, wtf_size_t index) {
  DCHECK(attr);
  DCHECK(HasElementData());

  const Attribute& attribute = GetElementData()->Attributes().at(index);
  DCHECK(attribute.GetName() == attr->GetQualifiedName());
  DetachAttrNodeFromElementWithValue(attr, attribute.Value());
  RemoveAttributeInternal(index, AttributeModificationReason::kDirectly);
}

void Element::removeAttribute(const QualifiedName& name) {
  wtf_size_t index = FindAttributeIndex(name);
  if (index == kNotFound) {
    return;
  }

  RemoveAttributeInternal(index, AttributeModificationReason::kDirectly);
}

void Element::SetBooleanAttribute(const QualifiedName& name, bool value) {
  if (value) {
    setAttribute(name, g_empty_atom);
  } else {
    removeAttribute(name);
  }
}

bool Element::HasAnyExplicitlySetAttrAssociatedElements() const {
  if (ElementRareDataVector* data = GetElementRareData()) {
    const ExplicitlySetAttrElementsMap* element_attribute_map =
        data->GetExplicitlySetElementsForAttr();
    return element_attribute_map && !element_attribute_map->map.empty();
  }
  return false;
}

bool Element::HasExplicitlySetAttrAssociatedElements(
    const QualifiedName& name) const {
  return GetExplicitlySetElementsForAttr(name);
}

GCedHeapLinkedHashSet<WeakMember<Element>>*
Element::GetExplicitlySetElementsForAttr(const QualifiedName& name) const {
  if (ElementRareDataVector* data = GetElementRareData()) {
    const ExplicitlySetAttrElementsMap* element_attribute_map =
        data->GetExplicitlySetElementsForAttr();
    if (!element_attribute_map) {
      return nullptr;
    }
    auto it = element_attribute_map->map.find(name);
    if (it == element_attribute_map->map.end()) {
      return nullptr;
    }
    const auto& elements = it->value;
    return elements->size() ? elements : nullptr;
  }
  return nullptr;
}

void Element::SynchronizeContentAttributeAndElementReference(
    const QualifiedName& name) {
  if (ElementRareDataVector* data = GetElementRareData()) {
    ExplicitlySetAttrElementsMap* element_attribute_map =
        data->GetExplicitlySetElementsForAttr();
    if (element_attribute_map) {
      element_attribute_map->map.erase(name);
    }
  }
}

void Element::SetElementAttribute(const QualifiedName& name, Element* element) {
  DCHECK(IsElementReflectionAttribute(name))
      << " Element attributes must be added to IsElementReflectionAttribute. "
         "name: "
      << name;
  if (!element && (!GetElementRareData() ||
                   !GetElementRareData()->GetExplicitlySetElementsForAttr())) {
    // Nothing to do for explicitly set attributes below,
    // so don't ensure the map; just remove the attribute.
    removeAttribute(name);
    return;
  }

  ExplicitlySetAttrElementsMap& explicitly_set_attr_elements_map =
      EnsureElementRareData().EnsureExplicitlySetElementsForAttr();

  // If the reflected element is explicitly null then we remove the content
  // attribute and the explicitly set attr-element.
  if (!element) {
    explicitly_set_attr_elements_map.map.erase(name);
    removeAttribute(name);
    return;
  }

  setAttribute(name, g_empty_atom);

  auto result = explicitly_set_attr_elements_map.map.insert(name, nullptr);
  if (result.is_new_entry) {
    result.stored_value->value =
        MakeGarbageCollected<GCedHeapLinkedHashSet<WeakMember<Element>>>();
  } else {
    result.stored_value->value->clear();
  }
  result.stored_value->value->insert(element);

  if (isConnected()) {
    if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache()) {
      cache->HandleAttributeChanged(name, this);
    }
  }
}

Element* Element::GetShadowReferenceTarget(const QualifiedName& name) const {
  if (!RuntimeEnabledFeatures::ShadowRootReferenceTargetEnabled(
          GetExecutionContext())) {
    return nullptr;
  }
  if (!RuntimeEnabledFeatures::ShadowRootReferenceTargetAriaOwnsEnabled(
          GetExecutionContext()) &&
      name == html_names::kAriaOwnsAttr) {
    return nullptr;
  }

  if (ShadowRoot* shadow_root = GetShadowRoot()) {
    if (Element* target = shadow_root->referenceTargetElement()) {
      if (Element* inner_target = target->GetShadowReferenceTarget(name)) {
        return inner_target;
      }
      return target;
    }
  }
  return nullptr;
}

Element* Element::GetShadowReferenceTargetOrSelf(const QualifiedName& name) {
  if (!RuntimeEnabledFeatures::ShadowRootReferenceTargetEnabled(
          GetExecutionContext())) {
    return this;
  }
  if (!RuntimeEnabledFeatures::ShadowRootReferenceTargetAriaOwnsEnabled(
          GetExecutionContext()) &&
      name == html_names::kAriaOwnsAttr) {
    return this;
  }
  ShadowRoot* shadow_root = GetShadowRoot();
  if (!shadow_root) {
    return this;
  }
  if (shadow_root->referenceTarget() == g_null_atom) {
    // If referenceTarget is not used, return this element.
    return this;
  }
  Element* target = shadow_root->referenceTargetElement();
  if (!target) {
    // If referenceTarget is specified but the ID is invalid, return nullptr.
    return nullptr;
  }
  return target->GetShadowReferenceTargetOrSelf(name);
}

Element* Element::getElementByIdIncludingDisconnected(
    const Element& element,
    const AtomicString& id) const {
  if (id.empty()) {
    return nullptr;
  }
  if (element.isConnected()) {
    return element.GetTreeScope().getElementById(id);
  }
  // https://html.spec.whatwg.org/#attr-associated-element
  // Attr associated element lookup does not depend on whether the element
  // is connected. However, the TreeOrderedMap that is used for
  // TreeScope::getElementById() only stores connected elements.
  Node& root = element.TreeRoot();
  for (Element& el : ElementTraversal::DescendantsOf(root)) {
    if (el.GetIdAttribute() == id) {
      return &el;
    }
  }
  return nullptr;
}

Element* Element::GetElementAttribute(const QualifiedName& name) const {
  GCedHeapLinkedHashSet<WeakMember<Element>>* element_attribute_vector =
      GetExplicitlySetElementsForAttr(name);
  Element* element = nullptr;

  if (element_attribute_vector) {
    DCHECK_EQ(element_attribute_vector->size(), 1u);
    element = *(element_attribute_vector->begin());
    DCHECK_NE(element, nullptr);

    // Only return the explicit element if it still exists within a valid scope.
    if (!ElementIsDescendantOfShadowIncludingAncestor(*this, *element)) {
      return nullptr;
    }
  } else {
    // Compute the attr-associated element from the content attribute if
    // present, id can be null.
    AtomicString id = getAttribute(name);
    if (id.IsNull()) {
      return nullptr;
    }

    element = getElementByIdIncludingDisconnected(*this, id);
  }

  // Don't return the element if it has an invalid reference target.
  if (RuntimeEnabledFeatures::ShadowRootReferenceTargetEnabled(
          GetExecutionContext()) &&
      element && !element->GetShadowReferenceTargetOrSelf(name)) {
    return nullptr;
  }

  return element;
}

Element* Element::GetElementAttributeResolvingReferenceTarget(
    const QualifiedName& name) const {
  if (Element* element = GetElementAttribute(name)) {
    return element->GetShadowReferenceTargetOrSelf(name);
  }

  return nullptr;
}

GCedHeapVector<Member<Element>>* Element::GetAttrAssociatedElements(
    const QualifiedName& name) const {
  // https://html.spec.whatwg.org/multipage/common-dom-interfaces.html#attr-associated-elements
  // 1. Let elements be an empty list.
  GCedHeapVector<Member<Element>>* result_elements =
      MakeGarbageCollected<GCedHeapVector<Member<Element>>>();
  GCedHeapLinkedHashSet<WeakMember<Element>>* explicitly_set_elements =
      GetExplicitlySetElementsForAttr(name);
  if (explicitly_set_elements) {
    // 3. If reflectedTarget's explicitly set attr-elements is not null:
    for (const auto& attr_element : *explicitly_set_elements) {
      // 3.1. If attrElement is not a descendant of any of element's
      // shadow-including ancestors, then continue.
      if (ElementIsDescendantOfShadowIncludingAncestor(*this, *attr_element)) {
        // 3.NEW. Resolve the referenceTarget of attr_element
        Element* reference_target =
            attr_element->GetShadowReferenceTargetOrSelf(name);

        // 3.2. Append attrElement to elements.
        if (reference_target) {
          result_elements->push_back(reference_target);
        }
      }
    }
  } else {
    // 4. Otherwise:
    // 4.1. Let contentAttributeValue be the result of running reflectedTarget's
    // get the content attribute.
    QualifiedName attr = name;

    // Account for labelled vs labeled spelling
    if (attr == html_names::kAriaLabelledbyAttr) {
      bool is_alternative_spelling =
          hasAttribute(html_names::kAriaLabeledbyAttr) &&
          !hasAttribute(html_names::kAriaLabelledbyAttr);
      attr = is_alternative_spelling ? html_names::kAriaLabeledbyAttr
                                     : html_names::kAriaLabelledbyAttr;
      if (is_alternative_spelling) {
        UseCounter::Count(GetDocument(),
                          WebFeature::kAriaLabeledByAlternativeSpelling);
      }
    }

    if (!hasAttribute(attr)) {
      // 4.2.  If contentAttributeValue is null, then return null.
      return nullptr;
    }

    String attribute_value = getAttribute(attr).GetString();

    // 4.3. Let tokens be contentAttributeValue, split on ASCII whitespace.
    Vector<String> tokens;
    attribute_value = attribute_value.SimplifyWhiteSpace();
    attribute_value.Split(' ', tokens);

    for (const auto& id : tokens) {
      // 4.3.1. Let candidate be the first element, in tree order, that meets
      // [certain criteria].
      Element* candidate =
          getElementByIdIncludingDisconnected(*this, AtomicString(id));
      if (candidate) {
        // 4.3.NEW. Resolve the referenceTarget of the candidate element
        candidate = candidate->GetShadowReferenceTargetOrSelf(attr);

        // 4.3.2. Append candidate to elements.
        if (candidate) {
          result_elements->push_back(candidate);
        }
      }
    }
  }
  // 5. Return elements.
  return result_elements;
}

FrozenArray<Element>* Element::GetElementArrayAttribute(
    const QualifiedName& name) {
  // https://html.spec.whatwg.org/multipage/common-dom-interfaces.html#reflecting-content-attributes-in-idl-attributes:element-3

  // 1. Let elements be this's attr-associated elements.
  GCedHeapVector<Member<Element>>* elements = GetAttrAssociatedElements(name);

  // Due to reference target it's possible that attr-associated elements could
  // be in non-ancestor shadow trees. We don't want to leak references into
  // those scopes, so retarget the elements.
  if (RuntimeEnabledFeatures::ShadowRootReferenceTargetEnabled(
          GetExecutionContext()) &&
      elements) {
    std::transform(elements->begin(), elements->end(), elements->begin(),
                   [this](Element* element) {
                     return &this->GetTreeScope().Retarget(*element);
                   });
  }

  CachedAttrAssociatedElementsMap* cached_attr_associated_elements_map =
      GetDocument().GetCachedAttrAssociatedElementsMap(this);
  DCHECK(cached_attr_associated_elements_map);

  if (!elements) {
    // 4. Set this's cached attr-associated elements to elementsAsFrozenArray.
    cached_attr_associated_elements_map->erase(name);
    // 5. Return elementsAsFrozenArray.
    return nullptr;
  }

  auto it = cached_attr_associated_elements_map->find(name);
  if (it != cached_attr_associated_elements_map->end()) {
    FrozenArray<Element>* cached_attr_associated_elements = it->value.Get();
    DCHECK(cached_attr_associated_elements);
    if (cached_attr_associated_elements->AsVector() == *elements) {
      // 2. If the contents of elements is equal to the contents of this's
      // cached attr-associated elements, then return this's cached
      // attr-associated elements.
      return cached_attr_associated_elements;
    }
  }

  // 3. Let elementsAsFrozenArray be elements, converted to a FrozenArray<T>?.
  FrozenArray<Element>* elements_as_frozen_array =
      MakeGarbageCollected<FrozenArray<Element>>(
          HeapVector<Member<Element>>(std::move(*elements)));

  // 4. Set this's cached attr-associated elements to elementsAsFrozenArray.
  cached_attr_associated_elements_map->Set(name, elements_as_frozen_array);

  // 5. Return elementsAsFrozenArray.
  return elements_as_frozen_array;
}

void Element::SetElementArrayAttribute(
    const QualifiedName& name,
    const GCedHeapVector<Member<Element>>* given_elements) {
  // https://html.spec.whatwg.org/multipage/common-dom-interfaces.html#reflecting-content-attributes-in-idl-attributes:element-3

  ExplicitlySetAttrElementsMap& element_attribute_map =
      EnsureElementRareData().EnsureExplicitlySetElementsForAttr();

  if (!given_elements) {
    // 1. If the given value is null:
    //   1. Set this's explicitly set attr-elements to null.
    element_attribute_map.map.erase(name);
    //   2. Run this's delete the content attribute.
    removeAttribute(name);
    return;
  }

  // 2. Run this's set the content attribute with the empty string.
  setAttribute(name, g_empty_atom);

  // 3. Let elements be an empty list.
  // 4. For each element in the given value: Append a weak reference to
  // element to elements.
  // 5. Set this's explicitly set attr-elements to elements.
  //
  // In practice, we're fetching elements from element_attribute_map, clearing
  // the previous value if necessary to get an empty list, and then populating
  // the list.
  auto it = element_attribute_map.map.find(name);
  GCedHeapLinkedHashSet<WeakMember<Element>>* stored_elements =
      it != element_attribute_map.map.end() ? it->value : nullptr;
  if (!stored_elements) {
    stored_elements =
        MakeGarbageCollected<GCedHeapLinkedHashSet<WeakMember<Element>>>();
    element_attribute_map.map.Set(name, stored_elements);
  } else {
    stored_elements->clear();
  }

  for (const auto& element : *given_elements) {
    stored_elements->insert(element);
  }

  // This |Set| call must occur after our call to |setAttribute| above.
  //
  // |setAttribute| will call through to |AttributeChanged| which calls
  // |SynchronizeContentAttributeAndElementReference| erasing the entry for
  // |name| from the map.
  element_attribute_map.map.Set(name, stored_elements);

  // |HandleAttributeChanged| must be called after updating the attribute map.
  if (isConnected()) {
    if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache()) {
      cache->HandleAttributeChanged(name, this);
    }
  }
}

FrozenArray<Element>* Element::ariaControlsElements() {
  return GetElementArrayAttribute(html_names::kAriaControlsAttr);
}
void Element::setAriaControlsElements(
    GCedHeapVector<Member<Element>>* given_elements) {
  SetElementArrayAttribute(html_names::kAriaControlsAttr, given_elements);
}

FrozenArray<Element>* Element::ariaDescribedByElements() {
  return GetElementArrayAttribute(html_names::kAriaDescribedbyAttr);
}
void Element::setAriaDescribedByElements(
    GCedHeapVector<Member<Element>>* given_elements) {
  SetElementArrayAttribute(html_names::kAriaDescribedbyAttr, given_elements);
}

FrozenArray<Element>* Element::ariaDetailsElements() {
  return GetElementArrayAttribute(html_names::kAriaDetailsAttr);
}
void Element::setAriaDetailsElements(
    GCedHeapVector<Member<Element>>* given_elements) {
  SetElementArrayAttribute(html_names::kAriaDetailsAttr, given_elements);
}

FrozenArray<Element>* Element::ariaErrorMessageElements() {
  return GetElementArrayAttribute(html_names::kAriaErrormessageAttr);
}
void Element::setAriaErrorMessageElements(
    GCedHeapVector<Member<Element>>* given_elements) {
  SetElementArrayAttribute(html_names::kAriaErrormessageAttr, given_elements);
}

FrozenArray<Element>* Element::ariaFlowToElements() {
  return GetElementArrayAttribute(html_names::kAriaFlowtoAttr);
}
void Element::setAriaFlowToElements(
    GCedHeapVector<Member<Element>>* given_elements) {
  SetElementArrayAttribute(html_names::kAriaFlowtoAttr, given_elements);
}

FrozenArray<Element>* Element::ariaLabelledByElements() {
  return GetElementArrayAttribute(html_names::kAriaLabelledbyAttr);
}
void Element::setAriaLabelledByElements(
    GCedHeapVector<Member<Element>>* given_elements) {
  SetElementArrayAttribute(html_names::kAriaLabelledbyAttr, given_elements);
}

FrozenArray<Element>* Element::ariaOwnsElements() {
  return GetElementArrayAttribute(html_names::kAriaOwnsAttr);
}
void Element::setAriaOwnsElements(
    GCedHeapVector<Member<Element>>* given_elements) {
  SetElementArrayAttribute(html_names::kAriaOwnsAttr, given_elements);
}

NamedNodeMap* Element::attributesForBindings() const {
  ElementRareDataVector& rare_data =
      const_cast<Element*>(this)->EnsureElementRareData();
  if (NamedNodeMap* attribute_map = rare_data.AttributeMap()) {
    return attribute_map;
  }

  rare_data.SetAttributeMap(
      MakeGarbageCollected<NamedNodeMap>(const_cast<Element*>(this)));
  return rare_data.AttributeMap();
}

AttributeNamesView Element::getAttributeNamesForBindings() const {
  return bindings::Transform<AttributeToNameTransform>(Attributes());
}

Vector<AtomicString> Element::getAttributeNames() const {
  Vector<AtomicString> result;
  auto view = getAttributeNamesForBindings();
  std::transform(view.begin(), view.end(), std::back_inserter(result),
                 [](const String& str) { return AtomicString(str); });
  return result;
}

Vector<QualifiedName> Element::getAttributeQualifiedNames() const {
  Vector<QualifiedName> result;
  auto attrs = Attributes();
  std::transform(attrs.begin(), attrs.end(), std::back_inserter(result),
                 [](const Attribute& attr) { return attr.GetName(); });
  return result;
}

inline ElementRareDataVector* Element::GetElementRareData() const {
  return static_cast<ElementRareDataVector*>(RareData());
}

inline ElementRareDataVector& Element::EnsureElementRareData() {
  return static_cast<ElementRareDataVector&>(EnsureRareData());
}

void Element::RemovePopoverData() {
  DCHECK(GetElementRareData());
  GetElementRareData()->RemovePopoverData();
}

PopoverData& Element::EnsurePopoverData() {
  return EnsureElementRareData().EnsurePopoverData();
}
PopoverData* Element::GetPopoverData() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetPopoverData();
  }
  return nullptr;
}

ContentData* Element::GetAltContentData() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetAltContentData();
  }
  return nullptr;
}

void Element::SetAltContentData(ContentData* content_data) {
  EnsureElementRareData().SetAltContentData(content_data);
}

InvokerData& Element::EnsureInvokerData() {
  return EnsureElementRareData().EnsureInvokerData();
}
InvokerData* Element::GetInvokerData() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetInvokerData();
  }
  return nullptr;
}

void Element::RemoveInterestInvokerTargetData() {
  DCHECK(GetElementRareData());
  GetElementRareData()->RemoveInterestInvokerTargetData();
}
InterestInvokerTargetData& Element::EnsureInterestInvokerTargetData() {
  return EnsureElementRareData().EnsureInterestInvokerTargetData();
}
InterestInvokerTargetData* Element::GetInterestInvokerTargetData() const {
  CHECK(RuntimeEnabledFeatures::HTMLInterestForAttributeEnabled());
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetInterestInvokerTargetData();
  }
  return nullptr;
}

bool Element::IsPopoverInTopLayer() {
  auto& document = GetDocument();
  DCHECK_EQ(IsInTopLayer(), document.TopLayerElements().Contains(this));
  if (!IsInTopLayer()) {
    return false;
  }
  auto* popover = DynamicTo<HTMLElement>(this);
  if (!popover || !popover->IsPopover()) {
    return false;
  }
  if (popover->popoverOpen()) {
    return true;
  }
  // This could be a popover that is transitioning out of the top layer.
  auto top_layer_reason = document.IsScheduledForTopLayerRemoval(this);
  return top_layer_reason.has_value() &&
         top_layer_reason.value() == Document::TopLayerReason::kPopover;
}

bool Element::IsDialogInTopLayer() {
  auto& document = GetDocument();
  DCHECK_EQ(IsInTopLayer(), document.TopLayerElements().Contains(this));
  if (!IsInTopLayer()) {
    return false;
  }
  auto* dialog = DynamicTo<HTMLDialogElement>(this);
  if (!dialog) {
    return false;
  }
  if (dialog->IsModal()) {
    DCHECK(document.TopLayerElements().Contains(dialog));
    return true;
  }
  // This could be a modal dialog that is transitioning out of the top layer.
  auto top_layer_reason = document.IsScheduledForTopLayerRemoval(this);
  return top_layer_reason &&
         top_layer_reason.value() == Document::TopLayerReason::kDialog;
}

// If this element is a triggering element for an *open* popover, in one of
// several ways, this returns the target popover. These forms of triggering
// are supported:
//   <button command=*-popover commandfor=foo>
//   <button popovertarget=foo>
//   <button interestfor=foo>
//   (JS) popover.showPopover({source: invoker});
// Note that only one of these mechanisms can be active at a time, and only
// an active invoker relationship will cause this function to return a popover.
// E.g. if there is `<button popovertarget=foo>` pointing to popover foo, but
// foo was opened with `foo.showPopover()` (no invoker specified), then this
// function will return nullptr.
HTMLElement* Element::GetOpenPopoverTarget() const {
  if (!IsFocusable() || !IsInTreeScope()) {
    return nullptr;
  }
  InvokerData* data = GetInvokerData();
  if (!data) {
    return nullptr;
  }
  HTMLElement* popover = data->GetInvokedPopover();
  if (!popover || !popover->popoverOpen()) {
    return nullptr;
  }
  CHECK_EQ(popover->GetPopoverData()->invoker(), this);
  return popover;
}

namespace {
bool ShouldContinueWithInterest(Element& invoker,
                                Element* target,
                                Element::InterestState new_state) {
  // Check pre-conditions. This function is called from posted tasks, so things
  // may have changed since invoker and target were passed.
  if (!target || !invoker.IsInTreeScope() ||
      !invoker.GetDocument().IsActive() ||
      invoker.InterestForElement() != target ||
      (new_state == Element::InterestState::kNoInterest &&
       target->SourceInterestInvoker() != invoker)) {
    return false;
  }
  return true;
}
}  // namespace

bool Element::InterestGained(Element* target) {
  CHECK(RuntimeEnabledFeatures::HTMLInterestForAttributeEnabled());

  if (!ShouldContinueWithInterest(*this, target,
                                  InterestState::kFullInterest)) {
    return false;
  }

  if (Element* existing_invoker = target->SourceInterestInvoker()) {
    // We're gaining interest, but the target already has an active interest
    // invoker. There are two cases:
    //  1. This is the same invoker. An example case is that the gain
    //     interest delay is short, but the lose interest delay is long, and
    //     we just de-hovered and then re-hovered the invoker. In this case,
    //     we can just cancel any interest lost event and move on.
    //  2. This is a different invoker. An example is that again, the lose
    //     interest delay is long, and we've hovered a different invoker for
    //     the same target. In this case, we need to immediately lose
    //     interest from the old invoker before gaining it via the new one.
    if (existing_invoker == this) {
      // Case 1.
      auto* invoker_data = GetInvokerData();
      CHECK(!invoker_data->HasInterestGainedTask());
      invoker_data->CancelInterestLostTask();
      return false;
    } else {
      // Case 2.
      if (!existing_invoker->InterestLost(target)) {
        return false;
      }
      // Event handlers might have changed things around, so re-check.
      if (!ShouldContinueWithInterest(*this, target,
                                      InterestState::kFullInterest)) {
        return false;
      }
    }
  }

  Event* interest_event = InterestEvent::Create(event_type_names::kInterest,
                                                this, Event::Cancelable::kYes);
  target->DispatchEvent(*interest_event);
  if (interest_event->defaultPrevented()) {
    return false;
  }

  // This is now the target's interest invoker
  CHECK(!target->SourceInterestInvoker());
  target->EnsureElementRareData()
      .EnsureInterestInvokerTargetData()
      .setInterestInvoker(this);
  ChangeInterestState(target, InterestState::kFullInterest);

  // If the target is a popover, invoke it.
  if (auto* popover = DynamicTo<HTMLElement>(target);
      popover && popover->PopoverType() != PopoverValueType::kNone) {
    if (popover->IsPopoverReady(PopoverTriggerAction::kShow,
                                /*exception_state=*/nullptr,
                                /*include_event_handler_text=*/true,
                                &GetDocument())) {
      popover->InvokePopover(*this);
    }
  }
  return true;
}

bool Element::InterestLost(Element* target,
                           InterestLostCancelable cancelable,
                           InterestLostPopoverBehavior behavior) {
  CHECK(RuntimeEnabledFeatures::HTMLInterestForAttributeEnabled());

  if (!ShouldContinueWithInterest(*this, target, InterestState::kNoInterest)) {
    return false;
  }

  Event* lose_interest_event =
      InterestEvent::Create(event_type_names::kLoseinterest, this,
                            cancelable == InterestLostCancelable::kCancelable
                                ? Event::Cancelable::kYes
                                : Event::Cancelable::kNo);
  target->DispatchEvent(*lose_interest_event);
  if (lose_interest_event->defaultPrevented()) {
    return false;
  }

  // If the target still thinks this invoker is its invoker, remove it.
  if (auto* targets_invoker = target->SourceInterestInvoker();
      targets_invoker && targets_invoker == this) {
    ChangeInterestState(target, InterestState::kNoInterest);
  }

  // If the target is a popover, hide it.
  if (behavior == InterestLostPopoverBehavior::kClosePopovers) {
    if (auto* popover = DynamicTo<HTMLElement>(target);
        popover && popover->PopoverType() != PopoverValueType::kNone) {
      if (popover->IsPopoverReady(PopoverTriggerAction::kHide,
                                  /*exception_state=*/nullptr,
                                  /*include_event_handler_text=*/true,
                                  &GetDocument())) {
        popover->HidePopoverInternal(
            /*invoker=*/this, HidePopoverFocusBehavior::kFocusPreviousElement,
            HidePopoverTransitionBehavior::kFireEventsAndWaitForTransitions,
            /*exception_state=*/nullptr);
      }
    }
  }
  return true;
}

void Element::HandlePointerEventsForInterestFor(
    const AtomicString& event_type) {
  if (!RuntimeEnabledFeatures::HTMLInterestForAttributeEnabled()) {
    return;
  }
  for (Element* element = this; element; element = element->parentElement()) {
    if (element->InterestForElement() || element->SourceInterestInvoker() ||
        element->GetInterestState() != InterestState::kNoInterest)
        [[unlikely]] {
      if (event_type == event_type_names::kPointerover) {
        element->HandleInterestForHoverOrFocus(InterestSource::kHover);
      } else {
        CHECK_EQ(event_type, event_type_names::kPointerout);
        element->HandleInterestForHoverOrFocus(InterestSource::kDeHover);
      }
    }
  }
}

void Element::DefaultEventHandler(Event& event) {
  if (RuntimeEnabledFeatures::HTMLInterestForAttributeEnabled() &&
      (InterestForElement() || SourceInterestInvoker() ||
       GetInterestState() != InterestState::kNoInterest)) [[unlikely]] {
    // Handle new `interestfor` activation via keyboard or long-press.
    String type = event.type();
    if (auto* focus_event = DynamicTo<FocusEvent>(event);
        focus_event &&
        (!focus_event->sourceCapabilities() ||
         !focus_event->sourceCapabilities()->firesTouchEvents())) {
      if (type == event_type_names::kFocusin) {
        HandleInterestForHoverOrFocus(InterestSource::kFocus);
      } else if (type == event_type_names::kFocusout) {
        HandleInterestForHoverOrFocus(InterestSource::kBlur);
      }
    }

    // For long presses on buttons, no context menu will be generated, because
    // the UA stylesheet adds `user-select:none` in this case. However, this
    // decision is made on the browser-side, async, which means we can't
    // explicitly check it here. So we just immediately show interest here for
    // all buttons that don't yet have interest.
    // TODO: should <area> elements be handled here also?
    if (auto* button = DynamicTo<HTMLButtonElement>(this);
        button && IsA<GestureEvent>(event) &&
        type == event_type_names::kGesturelongpress &&
        GetInterestState() == InterestState::kNoInterest) {
      // The pointer event manager will send a `pointerup` at the end of
      // this long-press, and (without intervention) that will immediately
      // light dismiss any target popover. To avoid this problem, set the
      // pointerdown target to the target popover, which will not match the
      // `null` target for that pointerup event.
      if (auto* target_popover = DynamicTo<HTMLElement>(InterestForElement());
          target_popover && target_popover->IsPopover()) {
        GetDocument().SetPopoverPointerdownTarget(target_popover);
      }
      // Delays don't apply to long-press, since the "long press" has a
      // built-in delay. Just show interest immediately in this case. This
      // follows the same path used by context-menu activations on link
      // elements.
      ShowInterestNow();
    }
  }
  ContainerNode::DefaultEventHandler(event);
}

Element* Element::anchorElement() const {
  if (!RuntimeEnabledFeatures::HTMLAnchorAttributeEnabled()) {
    return nullptr;
  }
  return GetElementAttributeResolvingReferenceTarget(html_names::kAnchorAttr);
}

// For JavaScript binding, return the anchor element without resolving the
// reference target, to avoid exposing shadow root content to JS.
Element* Element::anchorElementForBinding() const {
  CHECK(RuntimeEnabledFeatures::HTMLAnchorAttributeEnabled());
  return GetElementAttribute(html_names::kAnchorAttr);
}

void Element::setAnchorElementForBinding(Element* new_element) {
  CHECK(RuntimeEnabledFeatures::HTMLAnchorAttributeEnabled());
  SetElementAttribute(html_names::kAnchorAttr, new_element);
  EnsureAnchorElementObserver().Notify();
}

inline void Element::SynchronizeAttribute(const QualifiedName& name) const {
  if (!HasElementData()) {
    return;
  }
  if (name == html_names::kStyleAttr &&
      GetElementData()->style_attribute_is_dirty()) [[unlikely]] {
    DCHECK(IsStyledElement());
    SynchronizeStyleAttributeInternal();
    return;
  }
  if (GetElementData()->svg_attributes_are_dirty()) [[unlikely]] {
    // See comment in the AtomicString version of SynchronizeAttribute()
    // also.
    To<SVGElement>(this)->SynchronizeSVGAttribute(name);
  }
}

ElementAnimations* Element::GetElementAnimations() const {
  if (ElementRareDataVector* data = GetElementRareData()) {
    return data->GetElementAnimations();
  }
  return nullptr;
}

ElementAnimations& Element::EnsureElementAnimations() {
  ElementRareDataVector& rare_data = EnsureElementRareData();
  if (!rare_data.GetElementAnimations()) {
    rare_data.SetElementAnimations(MakeGarbageCollected<ElementAnimations>());
  }
  return *rare_data.GetElementAnimations();
}

bool Element::HasAnimations() const {
  if (ElementRareDataVector* data = GetElementRareData()) {
    const ElementAnimations* element_animations = data->GetElementAnimations();
    return element_animations && !element_animations->IsEmpty();
  }
  return false;
}

bool Element::hasAttribute(const QualifiedName& name) const {
  if (!HasElementData()) {
    return false;
  }
  SynchronizeAttribute(name);
  return GetElementData()->Attributes().Find(name);
}

bool Element::HasAttributeIgnoringNamespace(
    const AtomicString& local_name) const {
  if (!HasElementData()) {
    return false;
  }
  AtomicStringTable::WeakResult hint = WeakLowercaseIfNecessary(local_name);
  SynchronizeAttributeHinted(local_name, hint);
  if (hint.IsNull()) {
    return false;
  }
  for (const Attribute& attribute : GetElementData()->Attributes()) {
    if (hint == attribute.LocalName()) {
      return true;
    }
  }
  return false;
}

void Element::SynchronizeAllAttributes() const {
  if (!HasElementData()) {
    return;
  }
  // NOTE: AnyAttributeMatches in selector_checker.cc currently assumes that all
  // lazy attributes have a null namespace.  If that ever changes we'll need to
  // fix that code.
  if (GetElementData()->style_attribute_is_dirty()) {
    DCHECK(IsStyledElement());
    SynchronizeStyleAttributeInternal();
  }
  SynchronizeAllAttributesExceptStyle();
}

void Element::SynchronizeAllAttributesExceptStyle() const {
  if (!HasElementData()) {
    return;
  }
  if (GetElementData()->svg_attributes_are_dirty()) {
    To<SVGElement>(this)->SynchronizeAllSVGAttributes();
  }
}

const AtomicString& Element::getAttribute(const QualifiedName& name) const {
  if (!HasElementData()) {
    return g_null_atom;
  }
  SynchronizeAttribute(name);
  if (const Attribute* attribute = GetElementData()->Attributes().Find(name)) {
    return attribute->Value();
  }
  return g_null_atom;
}

AtomicString Element::LowercaseIfNecessary(AtomicString name) const {
  return IsHTMLElement() && IsA<HTMLDocument>(GetDocument())
             ? AtomicString::LowerASCII(std::move(name))
             : std::move(name);
}

const AtomicString& Element::nonce() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetNonce();
  }
  return g_null_atom;
}

void Element::setNonce(const AtomicString& nonce) {
  EnsureElementRareData().SetNonce(nonce);
}

namespace {

// TODO(https://crbug.com/41406914): Ad-hoc method until we hook up with scroll
// animation end.
ScriptPromise<IDLUndefined> CreateScrollResolvedPromise(
    ScriptState* script_state) {
  // Legacy binary tests pass a null `script_state`.
  if (!script_state ||
      !RuntimeEnabledFeatures::ProgrammaticScrollPromiseEnabled()) {
    return EmptyPromise();  // This is exposed to JS as `undefined`.
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  resolver->Resolve();
  return resolver->Promise();
}

}  // namespace

ScriptPromise<IDLUndefined> Element::scrollIntoView(ScriptState* script_state,
                                                    bool align_to_top) {
  auto* arg =
      MakeGarbageCollected<V8UnionBooleanOrScrollIntoViewOptions>(align_to_top);
  return scrollIntoView(script_state, arg);
}

ScriptPromise<IDLUndefined> Element::scrollIntoView(
    ScriptState* script_state,
    const V8UnionBooleanOrScrollIntoViewOptions* arg) {
  ScrollIntoViewOptions* options = nullptr;
  switch (arg->GetContentType()) {
    case V8UnionBooleanOrScrollIntoViewOptions::ContentType::kBoolean:
      options = ScrollIntoViewOptions::Create();
      options->setBlock(arg->GetAsBoolean()
                            ? V8ScrollLogicalPosition::Enum::kStart
                            : V8ScrollLogicalPosition::Enum::kEnd);
      options->setInlinePosition(V8ScrollLogicalPosition::Enum::kNearest);
      break;
    case V8UnionBooleanOrScrollIntoViewOptions::ContentType::
        kScrollIntoViewOptions:
      options = arg->GetAsScrollIntoViewOptions();
      break;
  }
  DCHECK(options);
  scrollIntoViewWithOptions(options);

  return CreateScrollResolvedPromise(script_state);
}

void Element::scrollIntoViewWithOptions(const ScrollIntoViewOptions* options) {
  ActivateDisplayLockIfNeeded(DisplayLockActivationReason::kScrollIntoView);
  GetDocument().EnsurePaintLocationDataValidForNode(
      this, DocumentUpdateReason::kJavaScript);

  if (!GetLayoutObject() || !GetDocument().GetPage()) {
    return;
  }

  mojom::blink::ScrollIntoViewParamsPtr params =
      scroll_into_view_util::CreateScrollIntoViewParams(*options,
                                                        *GetComputedStyle());
  Element* container = nullptr;
  if (options->hasContainer() &&
      options->container() == V8ScrollContainer::Enum::kNearest) {
    container = this;
    GetDocument().CountUse(WebFeature::kScrollIntoViewContainerNearest);
  }

  ScrollIntoViewNoVisualUpdate(std::move(params), container);
}

// TODO(crbug.com/385129957): This only searches up to the nearest scroll
// container. Ancestor scroll containers might also need to be notified.
void Element::NotifyScrollMarkerGroupOfTargetedScroll() {
  if (ScrollMarkerPseudoElement* marker = FindScrollMarkerForTargetedScroll()) {
    if (ScrollMarkerGroupPseudoElement* group = marker->ScrollMarkerGroup()) {
      group->PinSelectedMarker(marker);
    }
  }
}

ScrollMarkerPseudoElement* Element::FindScrollMarkerForTargetedScroll() {
  if (auto* this_marker = DynamicTo<ScrollMarkerPseudoElement>(this)) {
    // This itself is a scroll-marker.
    return this_marker;
  } else if (PseudoElement* scroll_marker =
                 GetPseudoElement(kPseudoIdScrollMarker)) {
    // This itself has a scroll-marker.
    return DynamicTo<ScrollMarkerPseudoElement>(scroll_marker);
  }

  LayoutObject* target_obj = GetLayoutObject();
  if (!target_obj) {
    return nullptr;
  }

  // Search for the scroll-marker before |this| in pre-order.
  ScrollMarkerPseudoElement* dom_marker = nullptr;
  for (LayoutObject* obj = target_obj->PreviousInPreOrder(); obj;
       obj = obj->PreviousInPreOrder()) {
    if (obj->IsScrollContainerWithScrollMarkerGroup()) {
      // Don't escape the target's nearest containing
      // scroll-marker-group-generating containing scroll container.
      break;
    }
    auto* obj_node = obj->GetNode();
    if (!obj_node) {
      continue;
    }
    if (ScrollMarkerPseudoElement* marker_node =
            DynamicTo<ScrollMarkerPseudoElement>(obj_node)) {
      dom_marker = marker_node;
      break;
    } else if (auto* obj_element = DynamicTo<Element>(obj_node)) {
      if (ScrollMarkerPseudoElement* previous_marker =
              DynamicTo<ScrollMarkerPseudoElement>(
                  obj_element->GetPseudoElement(kPseudoIdScrollMarker))) {
        dom_marker = previous_marker;
        break;
      }
    }
  }

  // We might have found a marker before |this| in DOM-order, but we need to do
  // one last check for whether the scroll target is within a
  // scroll-marker-generating ::column which might be preferable to the
  // already-found marker.
  const LayoutBox* containing_box = target_obj->ContainingScrollContainer();
  while (containing_box) {
    if (containing_box->IsScrollContainerWithScrollMarkerGroup()) {
      break;
    }
    containing_box = containing_box->ContainingScrollContainer();
  }

  if (!containing_box) {
    return dom_marker;
  }
  const Element* containing_element =
      DynamicTo<Element>(containing_box->GetNode());
  if (!containing_element) {
    return dom_marker;
  }
  const ColumnPseudoElementsVector* cols =
      containing_element->GetColumnPseudoElements();
  if (!cols) {
    return dom_marker;
  }
  const LayoutBox* target_box = GetLayoutBox();
  PhysicalRect scroll_target_rect = target_box->LocalToAncestorRect(
      target_box->PhysicalBorderBoxRect(), containing_box);
  ScrollableArea* current_scroll_area = containing_box->GetScrollableArea();
  if (current_scroll_area) {
    // Account for scroll translation.
    scroll_target_rect.Move(current_scroll_area->LocalToScrollOriginOffset());
  } else {
    NOTREACHED();
  }
  for (const ColumnPseudoElement* column_pseudo : *cols) {
    ScrollMarkerPseudoElement* column_marker =
        DynamicTo<ScrollMarkerPseudoElement>(
            column_pseudo->GetPseudoElement(kPseudoIdScrollMarker));
    const PhysicalRect& column_rect = column_pseudo->ColumnRect();
    if (column_marker && column_rect.Intersects(scroll_target_rect)) {
      if (!dom_marker) {
        // We didn't have a scroll-marker from the DOM search to begin, the
        // ::column::scroll-marker we found will have to do.
        return column_marker;
      }
      // If we already had a marker from the initial DOM search,
      // we should figure out whether |dom_marker| belongs to an element that
      // was flowed the scroll-marker-generating ::column, in which case
      // |dom_marker| is the preferred scroll marker. Otherwise the
      // scroll-marker belonging to the ::column is preferred.
      LayoutBox* dom_marker_box =
          dom_marker->UltimateOriginatingElement().GetLayoutBox();
      PhysicalRect dom_search_target_rect = dom_marker_box->LocalToAncestorRect(
          dom_marker_box->PhysicalBorderBoxRect(), containing_box);
      if (current_scroll_area) {
        dom_search_target_rect.Move(
            current_scroll_area->LocalToScrollOriginOffset());
      }
      return column_rect.Intersects(dom_search_target_rect) ? dom_marker
                                                            : column_marker;
    }
  }
  return dom_marker;
}

void Element::ScrollIntoViewNoVisualUpdate(
    mojom::blink::ScrollIntoViewParamsPtr params,
    const Element* container,
    bool include_self) {
  if (!GetLayoutObject() || !GetDocument().GetPage()) {
    return;
  }

  Element* originating_element = this;
  LayoutObject* target = nullptr;
  auto* pseudo_element = DynamicTo<PseudoElement>(this);
  if (pseudo_element) {
    originating_element = &pseudo_element->UltimateOriginatingElement();
    if (pseudo_element->parentNode()->IsColumnPseudoElement()) {
      // The originating element of a ::column is a multicol container. See if
      // it also is the scrollable container that is to be scrolled, or if it's
      // a descendant (in the latter case `target` will remain nullptr here).
      target = originating_element->GetLayoutBoxForScrolling();
    }
  }
  if (!target) {
    target = originating_element->GetLayoutObject();
  }

  if (DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
          *originating_element, DisplayLockActivationReason::kScrollIntoView)) {
    return;
  }

  NotifyScrollMarkerGroupOfTargetedScroll();

  PhysicalRect bounds = BoundingBoxForScrollIntoView();
  scroll_into_view_util::ScrollRectToVisible(
      *target, bounds, std::move(params),
      container ? container->GetLayoutObject() : nullptr,
      /* from_remote_frame = */ false,
      /* include_self = */ include_self ||
          !RuntimeEnabledFeatures::ScrollIntoViewSelfScrollFixEnabled());

  GetDocument().SetSequentialFocusNavigationStartingPoint(originating_element);
}

void Element::scrollIntoViewIfNeeded(bool center_if_needed) {
  GetDocument().EnsurePaintLocationDataValidForNode(
      this, DocumentUpdateReason::kJavaScript);

  if (!GetLayoutObject()) {
    return;
  }

  PhysicalRect bounds = BoundingBoxForScrollIntoView();
  if (center_if_needed) {
    scroll_into_view_util::ScrollRectToVisible(
        *GetLayoutObject(), bounds,
        scroll_into_view_util::CreateScrollIntoViewParams(
            ScrollAlignment::CenterIfNeeded(),
            ScrollAlignment::CenterIfNeeded()));
  } else {
    scroll_into_view_util::ScrollRectToVisible(
        *GetLayoutObject(), bounds,
        scroll_into_view_util::CreateScrollIntoViewParams(
            ScrollAlignment::ToEdgeIfNeeded(),
            ScrollAlignment::ToEdgeIfNeeded()));
  }
}

int Element::OffsetLeft() {
  GetDocument().EnsurePaintLocationDataValidForNode(
      this, DocumentUpdateReason::kJavaScript);
  if (const auto* layout_object = GetLayoutBoxModelObject()) {
    return AdjustForAbsoluteZoom::AdjustLayoutUnit(
               layout_object->OffsetLeft(OffsetParent()),
               layout_object->StyleRef())
        .Round();
  }
  return 0;
}

int Element::OffsetTop() {
  GetDocument().EnsurePaintLocationDataValidForNode(
      this, DocumentUpdateReason::kJavaScript);
  if (const auto* layout_object = GetLayoutBoxModelObject()) {
    return AdjustForAbsoluteZoom::AdjustLayoutUnit(
               layout_object->OffsetTop(OffsetParent()),
               layout_object->StyleRef())
        .Round();
  }
  return 0;
}

int Element::OffsetWidth() {
  GetDocument().EnsurePaintLocationDataValidForNode(
      this, DocumentUpdateReason::kJavaScript);
  if (const auto* layout_object = GetLayoutBoxModelObject()) {
    return AdjustForAbsoluteZoom::AdjustLayoutUnit(layout_object->OffsetWidth(),
                                                   layout_object->StyleRef())
        .Round();
  }
  return 0;
}

int Element::OffsetHeight() {
  GetDocument().EnsurePaintLocationDataValidForNode(
      this, DocumentUpdateReason::kJavaScript);
  if (const auto* layout_object = GetLayoutBoxModelObject()) {
    return AdjustForAbsoluteZoom::AdjustLayoutUnit(
               layout_object->OffsetHeight(), layout_object->StyleRef())
        .Round();
  }
  return 0;
}

Element* Element::OffsetParent() {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  LayoutObject* layout_object = GetLayoutObject();
  return layout_object ? layout_object->OffsetParent() : nullptr;
}

int Element::clientLeft() {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);
  return ClientLeftNoLayout();
}

int Element::clientTop() {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);
  return ClientTopNoLayout();
}

int Element::ClientLeftNoLayout() const {
  if (const auto* layout_object = GetLayoutBox()) {
    return AdjustForAbsoluteZoom::AdjustLayoutUnit(layout_object->ClientLeft(),
                                                   layout_object->StyleRef())
        .Round();
  }
  return 0;
}

int Element::ClientTopNoLayout() const {
  if (const auto* layout_object = GetLayoutBox()) {
    return AdjustForAbsoluteZoom::AdjustLayoutUnit(layout_object->ClientTop(),
                                                   layout_object->StyleRef())
        .Round();
  }
  return 0;
}

void Element::LastRememberedSizeChanged(ResizeObserverSize* size) {
  if (ShouldUpdateLastRememberedBlockSize()) {
    SetLastRememberedBlockSize(LayoutUnit(size->blockSize()));
  }
  if (ShouldUpdateLastRememberedInlineSize()) {
    SetLastRememberedInlineSize(LayoutUnit(size->inlineSize()));
  }
}

bool Element::ShouldUpdateLastRememberedBlockSize() const {
  const auto* style = GetComputedStyle();
  if (!style) {
    return false;
  }

  return style->IsHorizontalWritingMode()
             ? style->ContainIntrinsicHeight().HasAuto()
             : style->ContainIntrinsicWidth().HasAuto();
}

bool Element::ShouldUpdateLastRememberedInlineSize() const {
  const auto* style = GetComputedStyle();
  if (!style) {
    return false;
  }

  return style->IsHorizontalWritingMode()
             ? style->ContainIntrinsicWidth().HasAuto()
             : style->ContainIntrinsicHeight().HasAuto();
}

void Element::SetLastRememberedInlineSize(std::optional<LayoutUnit> size) {
  if (ElementRareDataVector* data = GetElementRareData()) {
    data->SetLastRememberedInlineSize(size);
  } else if (size) {
    EnsureElementRareData().SetLastRememberedInlineSize(size);
  }
}

void Element::SetLastRememberedBlockSize(std::optional<LayoutUnit> size) {
  if (ElementRareDataVector* data = GetElementRareData()) {
    data->SetLastRememberedBlockSize(size);
  } else if (size) {
    EnsureElementRareData().SetLastRememberedBlockSize(size);
  }
}

std::optional<LayoutUnit> Element::LastRememberedInlineSize() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->LastRememberedInlineSize();
  }
  return std::nullopt;
}

std::optional<LayoutUnit> Element::LastRememberedBlockSize() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->LastRememberedBlockSize();
  }
  return std::nullopt;
}

bool Element::IsViewportScrollElement() {
  auto& document = GetDocument();
  bool quirks_mode = document.InQuirksMode();
  return (!quirks_mode && document.documentElement() == this) ||
         (quirks_mode && IsHTMLElement() && document.body() == this);
}

int Element::clientWidth() {
  // When in strict mode, clientWidth for the document element should return the
  // width of the containing frame.
  // When in quirks mode, clientWidth for the body element should return the
  // width of the containing frame.
  if (IsViewportScrollElement()) {
    auto* layout_view = GetDocument().GetLayoutView();
    if (layout_view) {
      // TODO(crbug.com/740879): Use per-page overlay scrollbar settings.
      if (!ScrollbarThemeSettings::OverlayScrollbarsEnabled() ||
          !GetDocument().GetFrame()->IsLocalRoot()) {
        GetDocument().UpdateStyleAndLayoutForNode(
            this, DocumentUpdateReason::kJavaScript);
      }
      if (GetDocument().GetPage()->GetSettings().GetForceZeroLayoutHeight()) {
        // OverflowClipRect() may return infinite along a particular axis if
        // |layout_view| is not a scroll-container.
        DCHECK(layout_view->IsScrollContainer());
        return AdjustForAbsoluteZoom::AdjustLayoutUnit(
                   layout_view->OverflowClipRect(PhysicalOffset()).Width(),
                   layout_view->StyleRef())
            .Round();
      }
      return AdjustForAbsoluteZoom::AdjustInt(
          layout_view->GetLayoutSize().width(), layout_view->StyleRef());
    }
  }

  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  int result = 0;
  if (const auto* layout_object = GetLayoutBox()) {
    result = AdjustForAbsoluteZoom::AdjustLayoutUnit(
                 layout_object->ClientWidthWithTableSpecialBehavior(),
                 layout_object->StyleRef())
                 .Round();
  }
  return result;
}

int Element::clientHeight() {
  // When in strict mode, clientHeight for the document element should return
  // the height of the containing frame.
  // When in quirks mode, clientHeight for the body element should return the
  // height of the containing frame.
  if (IsViewportScrollElement()) {
    auto* layout_view = GetDocument().GetLayoutView();
    if (layout_view) {
      // TODO(crbug.com/740879): Use per-page overlay scrollbar settings.
      if (!ScrollbarThemeSettings::OverlayScrollbarsEnabled() ||
          !GetDocument().GetFrame()->IsLocalRoot()) {
        GetDocument().UpdateStyleAndLayoutForNode(
            this, DocumentUpdateReason::kJavaScript);
      }
      if (GetDocument().GetPage()->GetSettings().GetForceZeroLayoutHeight()) {
        // OverflowClipRect() may return infinite along a particular axis if
        // |layout_view| is not a scroll-container.
        DCHECK(layout_view->IsScrollContainer());
        return AdjustForAbsoluteZoom::AdjustLayoutUnit(
                   layout_view->OverflowClipRect(PhysicalOffset()).Height(),
                   layout_view->StyleRef())
            .Round();
      }
      return AdjustForAbsoluteZoom::AdjustInt(
          layout_view->GetLayoutSize().height(), layout_view->StyleRef());
    }
  }

  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  int result = 0;
  if (const auto* layout_object = GetLayoutBox()) {
    result = AdjustForAbsoluteZoom::AdjustLayoutUnit(
                 layout_object->ClientHeightWithTableSpecialBehavior(),
                 layout_object->StyleRef())
                 .Round();
  }
  return result;
}

double Element::currentCSSZoom() {
  GetDocument().UpdateStyleAndLayoutTreeForElement(
      this, DocumentUpdateReason::kComputedStyle);
  if (const auto* layout_object = GetLayoutObject()) {
    return layout_object->StyleRef().EffectiveZoom() /
           GetDocument().GetStyleEngine().GetStyleResolver().InitialZoom();
  }
  return 1.0;
}

double Element::scrollLeft() {
  if (!InActiveDocument()) {
    return 0;
  }

  // TODO(crbug.com/1499981): This should be removed once synchronized scrolling
  // impact is understood.
  SyncScrollAttemptHeuristic::DidAccessScrollOffset();

  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  if (GetDocument().ScrollingElementNoLayout() == this) {
    if (GetDocument().domWindow()) {
      return GetDocument().domWindow()->scrollX();
    }
    return 0;
  }

  LayoutBox* box = GetLayoutBoxForScrolling();
  if (!box) {
    return 0;
  }
  if (PaintLayerScrollableArea* scrollable_area = box->GetScrollableArea()) {
    DCHECK(GetLayoutBox());

    if (HasLeftwardDirection(*this)) {
      UseCounter::Count(
          GetDocument(),
          WebFeature::
              kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTop);
    }

    return AdjustForAbsoluteZoom::AdjustScroll(
        scrollable_area->GetWebExposedScrollOffset().x(), *GetLayoutBox());
  }

  return 0;
}

double Element::scrollTop() {
  if (!InActiveDocument()) {
    return 0;
  }

  // TODO(crbug.com/1499981): This should be removed once synchronized scrolling
  // impact is understood.
  SyncScrollAttemptHeuristic::DidAccessScrollOffset();

  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  if (GetDocument().ScrollingElementNoLayout() == this) {
    if (GetDocument().domWindow()) {
      return GetDocument().domWindow()->scrollY();
    }
    return 0;
  }

  // Don't disclose scroll position in preview state. See crbug.com/1261689.
  auto* select_element = DynamicTo<HTMLSelectElement>(this);
  if (select_element && !select_element->UsesMenuList() &&
      select_element->IsPreviewed()) {
    return 0;
  }

  LayoutBox* box = GetLayoutBoxForScrolling();
  if (!box) {
    return 0;
  }
  if (PaintLayerScrollableArea* scrollable_area = box->GetScrollableArea()) {
    DCHECK(GetLayoutBox());

    if (HasUpwardDirection(*this)) {
      UseCounter::Count(
          GetDocument(),
          WebFeature::
              kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTop);
    }

    return AdjustForAbsoluteZoom::AdjustScroll(
        scrollable_area->GetWebExposedScrollOffset().y(), *GetLayoutBox());
  }

  return 0;
}

void Element::setScrollLeft(double new_left) {
  if (!InActiveDocument()) {
    return;
  }

  // TODO(crbug.com/1499981): This should be removed once synchronized scrolling
  // impact is understood.
  SyncScrollAttemptHeuristic::DidSetScrollOffset();

  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  new_left = ScrollableArea::NormalizeNonFiniteScroll(new_left);

  if (GetDocument().ScrollingElementNoLayout() == this) {
    if (LocalDOMWindow* window = GetDocument().domWindow()) {
      ScrollToOptions* options = ScrollToOptions::Create();
      options->setLeft(new_left);
      window->scrollTo(nullptr, options);
    }
    return;
  }

  LayoutBox* box = GetLayoutBoxForScrolling();
  if (!box) {
    return;
  }
  if (PaintLayerScrollableArea* scrollable_area = box->GetScrollableArea()) {
    if (HasLeftwardDirection(*this)) {
      UseCounter::Count(
          GetDocument(),
          WebFeature::
              kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTop);
      if (new_left > 0) {
        UseCounter::Count(
            GetDocument(),
            WebFeature::
                kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTopSetPositive);
      }
    }

    ScrollOffset end_offset(new_left * box->Style()->EffectiveZoom(),
                            scrollable_area->GetScrollOffset().y());
    std::unique_ptr<cc::SnapSelectionStrategy> strategy =
        cc::SnapSelectionStrategy::CreateForEndPosition(
            scrollable_area->ScrollOffsetToPosition(end_offset), true, false);
    std::optional<gfx::PointF> snap_point =
        scrollable_area->GetSnapPositionAndSetTarget(*strategy);
    if (snap_point.has_value()) {
      end_offset = scrollable_area->ScrollPositionToOffset(snap_point.value());
    }
    scrollable_area->SetScrollOffset(end_offset,
                                     mojom::blink::ScrollType::kProgrammatic,
                                     cc::ScrollSourceType::kAbsoluteScroll,
                                     mojom::blink::ScrollBehavior::kAuto);
  }
}

void Element::setScrollTop(double new_top) {
  if (!InActiveDocument()) {
    return;
  }

  // TODO(crbug.com/1499981): This should be removed once synchronized scrolling
  // impact is understood.
  SyncScrollAttemptHeuristic::DidSetScrollOffset();

  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  new_top = ScrollableArea::NormalizeNonFiniteScroll(new_top);

  if (GetDocument().ScrollingElementNoLayout() == this) {
    if (LocalDOMWindow* window = GetDocument().domWindow()) {
      ScrollToOptions* options = ScrollToOptions::Create();
      options->setTop(new_top);
      window->scrollTo(nullptr, options);
    }
    return;
  }

  LayoutBox* box = GetLayoutBoxForScrolling();
  if (!box) {
    return;
  }
  if (PaintLayerScrollableArea* scrollable_area = box->GetScrollableArea()) {
    if (HasUpwardDirection(*this)) {
      UseCounter::Count(
          GetDocument(),
          WebFeature::
              kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTop);
      if (new_top > 0) {
        UseCounter::Count(
            GetDocument(),
            WebFeature::
                kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTopSetPositive);
      }
    }

    ScrollOffset end_offset(scrollable_area->GetScrollOffset().x(),
                            new_top * box->Style()->EffectiveZoom());
    std::unique_ptr<cc::SnapSelectionStrategy> strategy =
        cc::SnapSelectionStrategy::CreateForEndPosition(
            scrollable_area->ScrollOffsetToPosition(end_offset), false, true);
    std::optional<gfx::PointF> snap_point =
        scrollable_area->GetSnapPositionAndSetTarget(*strategy);
    if (snap_point.has_value()) {
      end_offset = scrollable_area->ScrollPositionToOffset(snap_point.value());
    }

    scrollable_area->SetScrollOffset(end_offset,
                                     mojom::blink::ScrollType::kProgrammatic,
                                     cc::ScrollSourceType::kAbsoluteScroll,
                                     mojom::blink::ScrollBehavior::kAuto);
  }
}

int Element::scrollWidth() {
  if (!InActiveDocument()) {
    return 0;
  }

  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  if (GetDocument().ScrollingElementNoLayout() == this) {
    if (GetDocument().View()) {
      return AdjustForAbsoluteZoom::AdjustInt(
          GetDocument().View()->LayoutViewport()->ContentsSize().width(),
          GetDocument().GetFrame()->LayoutZoomFactor());
    }
    return 0;
  }

  if (LayoutBox* box = GetLayoutBox()) {
    return AdjustForAbsoluteZoom::AdjustLayoutUnit(box->ScrollWidth(), *box)
        .Round();
  }
  return 0;
}

int Element::scrollHeight() {
  if (!InActiveDocument()) {
    return 0;
  }

  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  if (GetDocument().ScrollingElementNoLayout() == this) {
    if (GetDocument().View()) {
      return AdjustForAbsoluteZoom::AdjustInt(
          GetDocument().View()->LayoutViewport()->ContentsSize().height(),
          GetDocument().GetFrame()->LayoutZoomFactor());
    }
    return 0;
  }

  if (LayoutBox* box = GetLayoutBox()) {
    return AdjustForAbsoluteZoom::AdjustLayoutUnit(box->ScrollHeight(), *box)
        .Round();
  }
  return 0;
}

ScriptPromise<IDLUndefined> Element::scrollBy(ScriptState* script_state,
                                              double x,
                                              double y) {
  ScrollToOptions* scroll_to_options = ScrollToOptions::Create();
  scroll_to_options->setLeft(x);
  scroll_to_options->setTop(y);
  return scrollBy(script_state, scroll_to_options);
}

ScriptPromise<IDLUndefined> Element::scrollBy(
    ScriptState* script_state,
    const ScrollToOptions* scroll_to_options) {
  if (!InActiveDocument()) {
    return CreateScrollResolvedPromise(script_state);
  }

  // TODO(crbug.com/1499981): This should be removed once synchronized scrolling
  // impact is understood.
  SyncScrollAttemptHeuristic::DidSetScrollOffset();

  // FIXME: This should be removed once scroll updates are processed only after
  // the compositing update. See http://crbug.com/420741.
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  if (GetDocument().ScrollingElementNoLayout() == this) {
    ScrollFrameBy(scroll_to_options);
  } else {
    ScrollLayoutBoxBy(scroll_to_options);
  }

  return CreateScrollResolvedPromise(script_state);
}

ScriptPromise<IDLUndefined> Element::scrollTo(ScriptState* script_state,
                                              double x,
                                              double y) {
  ScrollToOptions* scroll_to_options = ScrollToOptions::Create();
  scroll_to_options->setLeft(x);
  scroll_to_options->setTop(y);
  return scrollTo(script_state, scroll_to_options);
}

ScriptPromise<IDLUndefined> Element::scrollTo(
    ScriptState* script_state,
    const ScrollToOptions* scroll_to_options) {
  if (!InActiveDocument()) {
    return CreateScrollResolvedPromise(script_state);
  }
  SetScrollOffset(scroll_to_options);
  return CreateScrollResolvedPromise(script_state);
}

bool Element::SetScrollOffset(const ScrollToOptions* scroll_to_options) {
  if (!InActiveDocument()) {
    return false;
  }

  // TODO(crbug.com/1499981): This should be removed once synchronized scrolling
  // impact is understood.
  SyncScrollAttemptHeuristic::DidSetScrollOffset();

  // FIXME: This should be removed once scroll updates are processed only after
  // the compositing update. See http://crbug.com/420741.
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  if (GetDocument().ScrollingElementNoLayout() == this) {
    return ScrollFrameTo(scroll_to_options);
  } else {
    return ScrollLayoutBoxTo(scroll_to_options);
  }
}

void Element::scrollIntoViewForTesting() {
  scrollIntoView(nullptr, /*align_to_top=*/true);
}

void Element::scrollIntoViewForTesting(
    const V8UnionBooleanOrScrollIntoViewOptions* arg) {
  scrollIntoView(nullptr, arg);
}

void Element::scrollByForTesting(double x, double y) {
  scrollBy(nullptr, x, y);
}

void Element::scrollToForTesting(double x, double y) {
  scrollTo(nullptr, x, y);
}

bool Element::ScrollLayoutBoxBy(const ScrollToOptions* scroll_to_options) {
  gfx::Vector2dF displacement;
  if (scroll_to_options->hasLeft()) {
    displacement.set_x(
        ScrollableArea::NormalizeNonFiniteScroll(scroll_to_options->left()));
  }
  if (scroll_to_options->hasTop()) {
    displacement.set_y(
        ScrollableArea::NormalizeNonFiniteScroll(scroll_to_options->top()));
  }

  mojom::blink::ScrollBehavior scroll_behavior =
      ScrollableArea::V8EnumToScrollBehavior(
          scroll_to_options->behavior().AsEnum());
  LayoutBox* box = GetLayoutBoxForScrolling();
  if (!box) {
    return false;
  }
  PaintLayerScrollableArea* scrollable_area = box->GetScrollableArea();
  if (!scrollable_area) {
    return false;
  }

  DCHECK(box);
  gfx::PointF current_position(scrollable_area->ScrollPosition().x(),
                               scrollable_area->ScrollPosition().y());
  displacement.Scale(box->Style()->EffectiveZoom());
  gfx::PointF new_position = current_position + displacement;

  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForDisplacement(
          current_position, displacement,
          RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled());
  new_position =
      scrollable_area->GetSnapPositionAndSetTarget(*strategy).value_or(
          new_position);
  return scrollable_area->ScrollToAbsolutePosition(
      new_position, scroll_behavior, mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kRelativeScroll);
}

bool Element::ScrollLayoutBoxTo(const ScrollToOptions* scroll_to_options) {
  mojom::blink::ScrollBehavior scroll_behavior =
      ScrollableArea::V8EnumToScrollBehavior(
          scroll_to_options->behavior().AsEnum());

  LayoutBox* box = GetLayoutBoxForScrolling();
  if (!box) {
    return false;
  }

  PaintLayerScrollableArea* scrollable_area = box->GetScrollableArea();
  if (!scrollable_area) {
    return false;
  }

  if (scroll_to_options->hasLeft() && HasLeftwardDirection(*this)) {
    UseCounter::Count(
        GetDocument(),
        WebFeature::
            kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTop);
    if (scroll_to_options->left() > 0) {
      UseCounter::Count(
          GetDocument(),
          WebFeature::
              kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTopSetPositive);
    }
  }
  if (scroll_to_options->hasTop() && HasUpwardDirection(*this)) {
    UseCounter::Count(
        GetDocument(),
        WebFeature::
            kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTop);
    if (scroll_to_options->top() > 0) {
      UseCounter::Count(
          GetDocument(),
          WebFeature::
              kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTopSetPositive);
    }
  }

  ScrollOffset new_offset = scrollable_area->GetScrollOffset();
  if (scroll_to_options->hasLeft()) {
    new_offset.set_x(
        ScrollableArea::NormalizeNonFiniteScroll(scroll_to_options->left()) *
        box->Style()->EffectiveZoom());
  }
  if (scroll_to_options->hasTop()) {
    new_offset.set_y(
        ScrollableArea::NormalizeNonFiniteScroll(scroll_to_options->top()) *
        box->Style()->EffectiveZoom());
  }

  new_offset = SnapScrollOffsetToPhysicalPixels(new_offset);
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForEndPosition(
          scrollable_area->ScrollOffsetToPosition(new_offset),
          scroll_to_options->hasLeft(), scroll_to_options->hasTop());
  std::optional<gfx::PointF> snap_point =
      scrollable_area->GetSnapPositionAndSetTarget(*strategy);
  if (snap_point.has_value()) {
    new_offset = scrollable_area->ScrollPositionToOffset(snap_point.value());
  }

  return scrollable_area->SetScrollOffset(
      new_offset, mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kAbsoluteScroll, scroll_behavior);
}

bool Element::ScrollFrameBy(const ScrollToOptions* scroll_to_options) {
  gfx::Vector2dF displacement;
  if (scroll_to_options->hasLeft()) {
    displacement.set_x(
        ScrollableArea::NormalizeNonFiniteScroll(scroll_to_options->left()));
  }
  if (scroll_to_options->hasTop()) {
    displacement.set_y(
        ScrollableArea::NormalizeNonFiniteScroll(scroll_to_options->top()));
  }

  mojom::blink::ScrollBehavior scroll_behavior =
      ScrollableArea::V8EnumToScrollBehavior(
          scroll_to_options->behavior().AsEnum());
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame || !frame->View() || !GetDocument().GetPage()) {
    return false;
  }

  ScrollableArea* viewport = frame->View()->LayoutViewport();
  if (!viewport) {
    return false;
  }

  displacement.Scale(frame->LayoutZoomFactor());
  gfx::PointF new_position = viewport->ScrollPosition() + displacement;
  gfx::PointF current_position = viewport->ScrollPosition();
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForDisplacement(
          current_position, displacement,
          RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled());
  new_position =
      viewport->GetSnapPositionAndSetTarget(*strategy).value_or(new_position);
  return viewport->SetScrollOffset(
      viewport->ScrollPositionToOffset(new_position),
      mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kRelativeScroll, scroll_behavior);
}

bool Element::ScrollFrameTo(const ScrollToOptions* scroll_to_options) {
  mojom::blink::ScrollBehavior scroll_behavior =
      ScrollableArea::V8EnumToScrollBehavior(
          scroll_to_options->behavior().AsEnum());
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame || !frame->View() || !GetDocument().GetPage()) {
    return false;
  }

  ScrollableArea* viewport = frame->View()->LayoutViewport();
  if (!viewport) {
    return false;
  }

  ScrollOffset new_offset = viewport->GetScrollOffset();
  if (scroll_to_options->hasLeft()) {
    new_offset.set_x(
        ScrollableArea::NormalizeNonFiniteScroll(scroll_to_options->left()) *
        frame->LayoutZoomFactor());
  }
  if (scroll_to_options->hasTop()) {
    new_offset.set_y(
        ScrollableArea::NormalizeNonFiniteScroll(scroll_to_options->top()) *
        frame->LayoutZoomFactor());
  }

  gfx::PointF new_position = viewport->ScrollOffsetToPosition(
      SnapScrollOffsetToPhysicalPixels(new_offset));
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForEndPosition(
          new_position, scroll_to_options->hasLeft(),
          scroll_to_options->hasTop());
  new_position =
      viewport->GetSnapPositionAndSetTarget(*strategy).value_or(new_position);
  new_offset = viewport->ScrollPositionToOffset(new_position);
  return viewport->SetScrollOffset(
      new_offset, mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kAbsoluteScroll, scroll_behavior);
}

bool Element::HandleScrollCommand(CommandEventType command) {
  DCHECK(RuntimeEnabledFeatures::HTMLCommandForScrollCommandsEnabled());

  if (!InActiveDocument()) {
    return false;
  }

  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  LayoutBox* box = GetLayoutBoxForScrolling();
  if (!box) {
    return false;
  }

  // TODO(457939344): Support root scroller.
  PaintLayerScrollableArea* scrollable_area = box->GetScrollableArea();
  if (!scrollable_area) {
    return false;
  }

  ScrollDirectionPhysical physical_direction =
      GetPhysicalDirectionForCommand(command, *box->Style());

  return scrollable_area->ScrollByPageWithSnap(physical_direction);
}

gfx::Rect Element::BoundsInWidget() const {
  GetDocument().EnsurePaintLocationDataValidForNode(
      this, DocumentUpdateReason::kUnknown);

  LocalFrameView* view = GetDocument().View();
  if (!view) {
    return gfx::Rect();
  }

  Vector<gfx::QuadF> quads;

  // TODO(pdr): Unify the quad/bounds code with Element::ClientQuads.

  // Foreign objects need to convert between SVG and HTML coordinate spaces and
  // cannot use LocalToAbsoluteQuad directly with ObjectBoundingBox which is
  // SVG coordinates and not HTML coordinates. Instead, use the AbsoluteQuads
  // codepath below.
  auto* svg_element = DynamicTo<SVGElement>(this);
  if (svg_element && GetLayoutObject() &&
      !GetLayoutObject()->IsSVGForeignObject()) {
    // Get the bounding rectangle from the SVG model.
    // TODO(pdr): This should include stroke.
    if (IsA<SVGGraphicsElement>(svg_element)) {
      quads.push_back(GetLayoutObject()->LocalToAbsoluteQuad(
          gfx::QuadF(GetLayoutObject()->ObjectBoundingBox())));
    }
  } else {
    // Get the bounding rectangle from the box model.
    if (GetLayoutBoxModelObject()) {
      GetLayoutBoxModelObject()->AbsoluteQuads(quads);
    }
  }

  if (quads.empty()) {
    return gfx::Rect();
  }

  gfx::RectF result;
  for (auto& quad : quads) {
    result.Union(quad.BoundingBox());
  }

  return view->FrameToViewport(gfx::ToEnclosingRect(result));
}

Vector<gfx::Rect> Element::OutlineRectsInWidget(
    DocumentUpdateReason reason) const {
  Vector<gfx::Rect> rects;

  LocalFrameView* view = GetDocument().View();
  if (!view) {
    return rects;
  }

  GetDocument().EnsurePaintLocationDataValidForNode(this, reason);

  LayoutBoxModelObject* layout_object = GetLayoutBoxModelObject();
  if (!layout_object) {
    return rects;
  }

  Vector<PhysicalRect> outline_rects = layout_object->OutlineRects(
      nullptr, PhysicalOffset(),
      layout_object->StyleRef().OutlineRectsShouldIncludeBlockInkOverflow());
  for (auto& r : outline_rects) {
    PhysicalRect physical_rect = layout_object->LocalToAbsoluteRect(r);
    gfx::Rect absolute_rect =
        view->FrameToViewport(ToPixelSnappedRect(physical_rect));
    rects.push_back(absolute_rect);
  }

  return rects;
}

gfx::Rect Element::VisibleBoundsInLocalRoot() const {
  if (!GetLayoutObject() || !GetDocument().GetPage() ||
      !GetDocument().GetFrame()) {
    return gfx::Rect();
  }

  // TODO(crbug.com/41417572): Flag-guard. Remove once this change lands safely
  // in stable.
  if (RuntimeEnabledFeatures::ClipElementVisibleBoundsInLocalRootEnabled()) {
    return VisibleBoundsRespectingClipsInLocalRoot();
  }

  // We don't use absoluteBoundingBoxRect() because it can return an gfx::Rect
  // larger the actual size by 1px. crbug.com/470503
  PhysicalRect rect(
      gfx::ToRoundedRect(GetLayoutObject()->AbsoluteBoundingBoxRectF()));
  PhysicalRect frame_clip_rect =
      GetDocument().View()->GetLayoutView()->ClippingRect(PhysicalOffset());
  rect.Intersect(frame_clip_rect);

  // MapToVisualRectInAncestorSpace, called with a null ancestor argument,
  // returns the root-frame-visible rect in the root frame's coordinate space.
  // TODO(bokan): When the root is remote this appears to be document space,
  // rather than frame.
  // MapToVisualRectInAncestorSpace applies ancestors' frame's clipping but does
  // not apply (overflow) element clipping.
  GetDocument().View()->GetLayoutView()->MapToVisualRectInAncestorSpace(nullptr,
                                                                        rect);

  rect = GetDocument()
             .GetFrame()
             ->LocalFrameRoot()
             .ContentLayoutObject()
             ->AbsoluteToLocalRect(rect, kTraverseDocumentBoundaries |
                                             kApplyRemoteMainFrameTransform);

  return ToPixelSnappedRect(rect);
}

gfx::Rect Element::VisibleBoundsRespectingClipsInLocalRoot() const {
  if (!GetLayoutObject()) {
    return gfx::Rect();
  }

  gfx::RectF rect_in_viewport =
      GetLayoutObject()->LocalBoundingBoxRectForAccessibility();

  GetLayoutObject()->MapToVisualRectInAncestorSpace(/*ancestor=*/nullptr,
                                                    rect_in_viewport);

  PhysicalRect rect_in_local_root =
      GetDocument()
          .GetFrame()
          ->LocalFrameRoot()
          .ContentLayoutObject()
          ->AbsoluteToLocalRect(
              PhysicalRect::EnclosingRect(rect_in_viewport),
              kTraverseDocumentBoundaries | kApplyRemoteMainFrameTransform);

  return ToPixelSnappedRect(rect_in_local_root);
}

void Element::ClientQuads(Vector<gfx::QuadF>& quads) const {
  LayoutObject* element_layout_object = GetLayoutObject();
  if (!element_layout_object) {
    return;
  }

  // Foreign objects need to convert between SVG and HTML coordinate spaces and
  // cannot use LocalToAbsoluteQuad directly with ObjectBoundingBox which is
  // SVG coordinates and not HTML coordinates. Instead, use the AbsoluteQuads
  // codepath below.
  const auto* svg_element = DynamicTo<SVGElement>(this);
  if (svg_element && !element_layout_object->IsSVGRoot() &&
      !element_layout_object->IsSVGForeignObject()) {
    // Get the bounding rectangle from the SVG model.
    // TODO(pdr): ObjectBoundingBox does not include stroke and the spec is not
    // clear (see: https://github.com/w3c/svgwg/issues/339, crbug.com/529734).
    // If stroke is desired, we can update this to use AbsoluteQuads, below.
    if (const auto* svg_graphics_element =
            DynamicTo<SVGGraphicsElement>(this)) {
      if (svg_graphics_element->IsNonRendered(element_layout_object)) {
        return;
      }

      quads.push_back(element_layout_object->LocalToAbsoluteQuad(
          gfx::QuadF(element_layout_object->ObjectBoundingBox())));
    }
    return;
  }

  // FIXME: Handle table/inline-table with a caption.
  if (element_layout_object->IsBoxModelObject() ||
      element_layout_object->IsBR()) {
    element_layout_object->AbsoluteQuads(quads);
  }
}

DOMRectList* Element::getClientRects() {
  Vector<gfx::RectF> rects = GetClientRectsNoAdjustment();
  if (rects.empty()) {
    return MakeGarbageCollected<DOMRectList>();
  }
  LayoutObject* element_layout_object = GetLayoutObject();
  DCHECK(element_layout_object);
  for (auto& rect : rects) {
    GetDocument().AdjustRectForScrollAndAbsoluteZoom(rect,
                                                     *element_layout_object);
  }
  return MakeGarbageCollected<DOMRectList>(rects);
}

Vector<gfx::RectF> Element::GetClientRectsNoAdjustment() {
  // TODO(crbug.com/1499981): This should be removed once synchronized scrolling
  // impact is understood.
  SyncScrollAttemptHeuristic::DidAccessScrollOffset();
  GetDocument().EnsurePaintLocationDataValidForNode(
      this, DocumentUpdateReason::kJavaScript);

  Vector<gfx::QuadF> quads;
  ClientQuads(quads);
  if (quads.empty()) {
    return {};
  }
  Vector<gfx::RectF> result;
  for (auto& quad : quads) {
    result.emplace_back(quad.BoundingBox());
  }
  return result;
}

gfx::RectF Element::GetBoundingClientRectNoLifecycleUpdateNoAdjustment() const {
  Vector<gfx::QuadF> quads;
  ClientQuads(quads);
  if (quads.empty()) {
    return gfx::RectF();
  }

  gfx::RectF result;
  for (auto& quad : quads) {
    result.Union(quad.BoundingBox());
  }
  return result;
}

gfx::RectF Element::GetBoundingClientRectNoLifecycleUpdate() const {
  gfx::RectF result = GetBoundingClientRectNoLifecycleUpdateNoAdjustment();
  if (result == gfx::RectF()) {
    return result;
  }

  LayoutObject* element_layout_object = GetLayoutObject();
  DCHECK(element_layout_object);
  GetDocument().AdjustRectForScrollAndAbsoluteZoom(result,
                                                   *element_layout_object);
  return result;
}

DOMRect* Element::GetBoundingClientRect() {
  GetDocument().EnsurePaintLocationDataValidForNode(
      this, DocumentUpdateReason::kJavaScript);
  return DOMRect::FromRectF(GetBoundingClientRectNoLifecycleUpdate());
}

DOMRect* Element::GetBoundingClientRectForBinding() {
  // TODO(crbug.com/1499981): This should be removed once synchronized scrolling
  // impact is understood.
  SyncScrollAttemptHeuristic::DidAccessScrollOffset();
  return GetBoundingClientRect();
}

const AtomicString& Element::computedRole() {
  Document& document = GetDocument();
  if (!document.IsActive() || !document.View()) {
    return g_null_atom;
  }
  AXContext ax_context(document, ui::kAXModeBasic);
  document.View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kJavaScript);
  return ax_context.GetAXObjectCache().ComputedRoleForNode(this);
}

const AtomicString& Element::ComputedRoleNoLifecycleUpdate() {
  Document& document = GetDocument();
  if (!document.IsActive() || !document.View()) {
    return g_null_atom;
  }
  // TODO(chrishtr) this should never happen. Possibly changes already in
  // InspectorOverlayAgent already make this unnecessary.
  if (document.Lifecycle().GetState() < DocumentLifecycle::kPrePaintClean) {
    DCHECK(false);
    return g_null_atom;
  }
  AXContext ax_context(document, ui::kAXModeBasic);
  // Allocating the AXContext needs to not change lifecycle states.
  DCHECK_GE(document.Lifecycle().GetState(), DocumentLifecycle::kPrePaintClean)
      << " State was: " << document.Lifecycle().GetState();
  return ax_context.GetAXObjectCache().ComputedRoleForNode(this);
}

String Element::computedName() {
  Document& document = GetDocument();
  if (!document.IsActive() || !document.View()) {
    return String();
  }
  AXContext ax_context(document, ui::kAXModeBasic);
  document.View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kJavaScript);
  return ax_context.GetAXObjectCache().ComputedNameForNode(this);
}

String Element::ComputedNameNoLifecycleUpdate() {
  Document& document = GetDocument();
  if (!document.IsActive() || !document.View()) {
    return String();
  }
  // TODO(chrishtr) this should never happen. Possibly changes already in
  // InspectorOverlayAgent already make this unnecessary.
  if (document.Lifecycle().GetState() < DocumentLifecycle::kPrePaintClean) {
    DCHECK(false);
    return g_null_atom;
  }
  AXContext ax_context(document, ui::kAXModeBasic);
  // Allocating the AXContext needs to not change lifecycle states.
  DCHECK_GE(document.Lifecycle().GetState(), DocumentLifecycle::kPrePaintClean)
      << " State was: " << document.Lifecycle().GetState();
  return ax_context.GetAXObjectCache().ComputedNameForNode(this);
}

void Element::ariaNotify(const String& announcement,
                         const AriaNotificationOptions* options) {
  DCHECK(RuntimeEnabledFeatures::AriaNotifyEnabled(GetExecutionContext()));

  if (auto* cache = GetDocument().ExistingAXObjectCache()) {
    cache->HandleAriaNotification(this, announcement, options);
  }
}

bool Element::toggleAttribute(const AtomicString& qualified_name,
                              ExceptionState& exception_state) {
  // https://dom.spec.whatwg.org/#dom-element-toggleattribute
  // 1. If qualifiedName does not match the Name production in XML, then throw
  // an "InvalidCharacterError" DOMException.
  bool is_valid =
      RuntimeEnabledFeatures::RelaxDOMValidNamesEnabled()
          ? Document::IsValidAttributeLocalNameNewSpec(qualified_name)
          : Document::IsValidName(qualified_name);
  if (!is_valid) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        StrCat({"'", qualified_name, "' is not a valid attribute name."}));
    return false;
  }
  // 2. If the context object is in the HTML namespace and its node document is
  // an HTML document, then set qualifiedName to qualifiedName in ASCII
  // lowercase.
  AtomicString lowercase_name = LowercaseIfNecessary(qualified_name);
  AtomicStringTable::WeakResult hint(lowercase_name.Impl());
  // 3. Let attribute be the first attribute in the context object’s attribute
  // list whose qualified name is qualifiedName, and null otherwise.
  SynchronizeAttributeHinted(lowercase_name, hint);
  auto [index, q_name] =
      LookupAttributeQNameHinted(std::move(lowercase_name), hint);
  // 4. If attribute is null, then
  if (index == kNotFound) {
    // 4. 1. If force is not given or is true, create an attribute whose local
    // name is qualified_name, value is the empty string, and node document is
    // the context object’s node document, then append this attribute to the
    // context object, and then return true.
    SetAttributeInternal(index, q_name, g_empty_atom,
                         AttributeModificationReason::kDirectly);
    return true;
  }
  // 5. Otherwise, if force is not given or is false, remove an attribute given
  // qualifiedName and the context object, and then return false.
  SetAttributeInternal(index, q_name, g_null_atom,
                       AttributeModificationReason::kDirectly);
  return false;
}

bool Element::toggleAttribute(const AtomicString& qualified_name,
                              bool force,
                              ExceptionState& exception_state) {
  // https://dom.spec.whatwg.org/#dom-element-toggleattribute
  // 1. If qualifiedName does not match the Name production in XML, then throw
  // an "InvalidCharacterError" DOMException.
  bool is_valid =
      RuntimeEnabledFeatures::RelaxDOMValidNamesEnabled()
          ? Document::IsValidAttributeLocalNameNewSpec(qualified_name)
          : Document::IsValidName(qualified_name);
  if (!is_valid) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        StrCat({"'", qualified_name, "' is not a valid attribute name."}));
    return false;
  }
  // 2. If the context object is in the HTML namespace and its node document is
  // an HTML document, then set qualifiedName to qualifiedName in ASCII
  // lowercase.
  AtomicString lowercase_name = LowercaseIfNecessary(qualified_name);
  AtomicStringTable::WeakResult hint(lowercase_name.Impl());
  // 3. Let attribute be the first attribute in the context object’s attribute
  // list whose qualified name is qualifiedName, and null otherwise.
  SynchronizeAttributeHinted(lowercase_name, hint);
  auto [index, q_name] =
      LookupAttributeQNameHinted(std::move(lowercase_name), hint);
  // 4. If attribute is null, then
  if (index == kNotFound) {
    // 4. 1. If force is not given or is true, create an attribute whose local
    // name is qualified_name, value is the empty string, and node document is
    // the context object’s node document, then append this attribute to the
    // context object, and then return true.
    if (force) {
      SetAttributeInternal(index, q_name, g_empty_atom,
                           AttributeModificationReason::kDirectly);
      return true;
    }
    // 4. 2. Return false.
    return false;
  }
  // 5. Otherwise, if force is not given or is false, remove an attribute given
  // qualifiedName and the context object, and then return false.
  if (!force) {
    SetAttributeInternal(index, q_name, g_null_atom,
                         AttributeModificationReason::kDirectly);
    return false;
  }
  // 6. Return true.
  return true;
}

const AtomicString& Element::getAttributeNS(
    const AtomicString& namespace_uri,
    const AtomicString& local_name) const {
  return getAttribute(QualifiedName(g_null_atom, local_name, namespace_uri));
}

const AttrNameToTrustedType& Element::GetCheckedAttributeTypes() const {
  DEFINE_STATIC_LOCAL(AttrNameToTrustedType, attribute_map, ({}));
  return attribute_map;
}

const std::tuple<SpecificTrustedType, const AtomicString, const AtomicString>
Element::GetTrustedTypeDataForAttribute(const QualifiedName& q_name,
                                        const char* legacy_sink_name) const {
  // https://w3c.github.io/trusted-types/dist/spec/#abstract-opdef-get-trusted-type-data-for-attribute

  // We implement both legacy and new behaviour, guarded by TrustedTypesHTML.
  //
  // Once TrustedTypesHTML is perma-enabled, the second branch can be removed,
  // as can the legacy_sink_name parameter. And the return type can become
  // AttrNameToTrustedType::value_type.
  if (RuntimeEnabledFeatures::TrustedTypesHTMLEnabled()) {
    // Step 1: Let data be null. (Nothing to do; we don't set data.)
    // Step 2: If [... conditions ...] and attribute is the name of an event
    //         handler content attribute: [...]
    if (q_name.NamespaceURI().IsNull() &&
        (namespaceURI() == html_names::xhtmlNamespaceURI ||
         namespaceURI() == svg_names::kNamespaceURI ||
         namespaceURI() == mathml_names::kNamespaceURI) &&
        IsTrustedTypesEventHandlerAttribute(q_name)) {
      return {SpecificTrustedType::kScript, trusted_types_names::kElement,
              q_name.LocalName()};
    }

    // Step 3: Find the row in the following table [...]
    // Since there's only one namespaced, TT-relevant attribute, we'll keep
    // namespaces out of the tables. Thus, we'll handle the one namespaced
    // attribute separately.
    if (!q_name.NamespaceURI().empty() &&
        !q_name.Matches(xlink_names::kHrefAttr)) {
      return {SpecificTrustedType::kNone, trusted_types_names::kElement,
              q_name.LocalName()};
    }
    const AttrNameToTrustedType* attribute_types = &GetCheckedAttributeTypes();
    AttrNameToTrustedType::const_iterator iter =
        attribute_types->find(q_name.LocalName());

    // Step 4: Return data. [data might be null.]
    if (iter == attribute_types->end()) {
      return {SpecificTrustedType::kNone, trusted_types_names::kElement,
              q_name.LocalName()};
    }
    return {iter->value.first, iter->value.second, q_name.LocalName()};
  } else {
    // Legacy behaviour; no longer spec compliant.

    // TODO(vogelheim): Once the TrustedTypesHTML flag is removed, this code
    // also gets removed and it should be easy to change the return type back
    // to a reference.
    AtomicString property_name(legacy_sink_name);
    if (!q_name.NamespaceURI().IsNull() &&
        !SVGAnimatedHref::IsKnownAttribute(q_name)) {
      return {SpecificTrustedType::kNone, trusted_types_names::kElement,
              property_name};
    }
    const AttrNameToTrustedType* attribute_types = &GetCheckedAttributeTypes();
    AttrNameToTrustedType::const_iterator iter =
        attribute_types->find(q_name.LocalName());
    if (iter != attribute_types->end()) {
      return {iter->value.first, trusted_types_names::kElement, property_name};
    }

    if (IsTrustedTypesEventHandlerAttribute(q_name)) {
      return {SpecificTrustedType::kScript, trusted_types_names::kElement,
              property_name};
    }

    return {SpecificTrustedType::kNone, trusted_types_names::kElement,
            property_name};
  }
}

String Element::TrustedTypesCheckForAttribute(
    const QualifiedName& q_name,
    const V8TrustedType* value,
    const char* legacy_sink_name,
    ExceptionState& exception_state) const {
  auto data = GetTrustedTypeDataForAttribute(q_name, legacy_sink_name);
  return TrustedTypesCheckFor(std::get<0>(data), value, GetExecutionContext(),
                              std::get<1>(data), std::get<2>(data),
                              exception_state);
}

String Element::TrustedTypesCheckForAttribute(
    const QualifiedName& q_name,
    const AtomicString& value,
    const char* legacy_sink_name,
    ExceptionState& exception_state) const {
  auto data = GetTrustedTypeDataForAttribute(q_name, legacy_sink_name);
  return TrustedTypesCheckFor(std::get<0>(data), value, GetExecutionContext(),
                              std::get<1>(data), std::get<2>(data),
                              exception_state);
}

String Element::TrustedTypesCheckForAttribute(
    const QualifiedName& q_name,
    String value,
    const char* legacy_sink_name,
    ExceptionState& exception_state) const {
  auto data = GetTrustedTypeDataForAttribute(q_name, legacy_sink_name);
  return TrustedTypesCheckFor(std::get<0>(data), std::move(value),
                              GetExecutionContext(), std::get<1>(data),
                              std::get<2>(data), exception_state);
}

void Element::ProcessElementRenderBlocking(const AtomicString& id_or_name) {
  if (!GetDocument().HasPendingExpectLinkElements() || !isConnected() ||
      !IsFinishedParsingChildren() || id_or_name.IsNull() ||
      id_or_name.empty()) {
    return;
  }
  DCHECK(GetDocument().GetRenderBlockingResourceManager());
  GetDocument().GetRenderBlockingResourceManager()->RemovePendingParsingElement(
      id_or_name, this);
}

DISABLE_CFI_PERF
void Element::AttributeChanged(const AttributeModificationParams& params) {
  ParseAttribute(params);

  GetDocument().IncDOMTreeVersion();
  GetDocument().NotifyAttributeChanged(*this, params.name, params.old_value,
                                       params.new_value);

  const QualifiedName& name = params.name;
  if (name == html_names::kIdAttr) {
    AtomicString lowercase_id;
    if (GetDocument().InQuirksMode() && !params.new_value.IsLowerASCII()) {
      lowercase_id = params.new_value.LowerASCII();
    }
    const AtomicString& new_id = lowercase_id ? lowercase_id : params.new_value;
    if (new_id != GetElementData()->IdForStyleResolution()) {
      AtomicString old_id = GetElementData()->SetIdForStyleResolution(new_id);
      GetDocument().GetStyleEngine().IdChangedForElement(old_id, new_id, *this);
    }

    ProcessElementRenderBlocking(new_id);

    if (GetDocument().HasPendingExpectLinkElements() &&
        IsFinishedParsingChildren()) {
      DCHECK(GetDocument().GetRenderBlockingResourceManager());
      GetDocument()
          .GetRenderBlockingResourceManager()
          ->RemovePendingParsingElement(GetIdAttribute(), this);
    }

    // If the id changes that may have been a target of overscroll command, we
    // need to notify that an overscroll-target pseudo class may have changed.
    const auto& overscroll_command_targets =
        GetDocument().OverscrollCommandTargets();
    auto invalidate_overscroll_target_state = [&](const AtomicString& idref) {
      OverscrollTargetStateChanged();
      // We also may have a new target with the same id. Note that this
      // invalidates all elements with this id, which should be a small set
      // typically.
      for (auto& element : GetDocument().GetAllElementsById(idref)) {
        element->OverscrollTargetStateChanged();
      }
    };

    if (!params.old_value.empty() &&
        overscroll_command_targets.Contains(params.old_value)) {
      invalidate_overscroll_target_state(params.old_value);
    }
    if (!params.new_value.empty() &&
        overscroll_command_targets.Contains(params.new_value)) {
      invalidate_overscroll_target_state(params.new_value);
    }

  } else if (name == html_names::kClassAttr) {
    if (params.old_value == params.new_value &&
        params.reason != AttributeModificationReason::kByMoveToNewDocument &&
        params.reason != AttributeModificationReason::kByCloning) {
      return;
    }
    ClassAttributeChanged(params.new_value);
    UpdateClassList(params.old_value, params.new_value);
  } else if (name == html_names::kNameAttr) {
    SetHasName(!params.new_value.IsNull());
  } else if (HasTagName(html_names::kATag) && name == html_names::kHrefAttr) {
    // <a> element is a potential scroll marker, set flag to check and update if
    // needed.
    GetDocument().SetNeedsScrollTargetGroupRelationsUpdate();
  } else if (name == html_names::kPartAttr) {
    part().DidUpdateAttributeValue(params.old_value, params.new_value);
    GetDocument().GetStyleEngine().PartChangedForElement(*this);
  } else if (name == html_names::kExportpartsAttr) {
    EnsureElementRareData().SetPartNamesMap(params.new_value);
    GetDocument().GetStyleEngine().ExportpartsChangedForElement(*this);
  } else if (name == html_names::kTabindexAttr) {
    int tabindex = 0;
    if (params.new_value.empty() ||
        !ParseHTMLInteger(params.new_value, tabindex)) {
      ClearTabIndexExplicitlyIfNeeded();
    } else {
      // We only set when value is in integer range.
      SetTabIndexExplicitly();
    }
    if (params.reason == AttributeModificationReason::kDirectly &&
        AdjustedFocusedElementInTreeScope() == this) {
      // The attribute change may cause supportsFocus() to return false
      // for the element which had focus.
      //
      // TODO(tkent): We should avoid updating style.  We'd like to check only
      // DOM-level focusability here.
      GetDocument().UpdateStyleAndLayoutTreeForElement(
          this, DocumentUpdateReason::kFocus);
      if (!IsFocusable() && !GetFocusableArea()) {
        blur();
      }
    }
  } else if (name == html_names::kSlotAttr) {
    if (params.old_value != params.new_value) {
      if (ShadowRoot* root = ShadowRootOfParent()) {
        root->DidChangeHostChildSlotName(params.old_value, params.new_value);
      }
    }
  } else if (name == html_names::kFocusgroupAttr) {
    // Only update the focusgroup flags when the node has been added to the
    // tree. This is because the computed focusgroup value will depend on the
    // focusgroup value of its closest ancestor node that is a focusgroup, if
    // any.
    if (parentNode()) {
      UpdateFocusgroup(params.new_value);
    }
  } else if (RuntimeEnabledFeatures::CSSOverscrollGesturesEnabled() &&
             name == html_names::kOverscrollcontainerAttr) {
    if (params.new_value.IsNull() || params.old_value.IsNull()) {
      // TODO(crbug.com/467968812): We can optimize this in some cases since a
      // container that disappears necessarily adds its elements to the
      // ancestor container. However, if the container appears, it's harder to
      // figure out which elements are contained by it without doing a subtree
      // recalc.
      SetNeedsStyleRecalc(kSubtreeStyleChange,
                          StyleChangeReasonForTracing::FromAttribute(name));
    }
  } else if (IsStyledElement()) {
    if (name == html_names::kStyleAttr) {
      if (params.old_value == params.new_value) {
        return;
      }
      StyleAttributeChanged(params.new_value, params.reason);
    } else if (IsPresentationAttribute(name)) {
      if (name == html_names::kAnchorAttr) {
        if (RuntimeEnabledFeatures::HTMLAnchorAttributeEnabled()) {
          EnsureAnchorElementObserver().Notify();
        }
      } else if (name == html_names::kHiddenAttr) {
        if (params.new_value == keywords::kUntilFound) {
          EnsureDisplayLockContext().SetIsHiddenUntilFoundElement(true);
        } else if (DisplayLockContext* context = GetDisplayLockContext()) {
          context->SetIsHiddenUntilFoundElement(false);
        }
      } else if (name == html_names::kInertAttr) {
        UseCounter::Count(GetDocument(), WebFeature::kInertAttribute);
      }
      // NOTE: We could test here if we have a shared ElementData
      // with presentation attribute style, and avoid re-dirtying
      // if so, but somehow, that benchmarks poorly, so we don't do it.
      // It's fairly rare that this would save a lot of time anyway.
      GetElementData()->SetPresentationAttributeStyleIsDirty(true);
      SetNeedsStyleRecalc(kLocalStyleChange,
                          StyleChangeReasonForTracing::FromAttribute(name));
    }
  }

  if (IsElementReflectionAttribute(name)) {
    SynchronizeContentAttributeAndElementReference(name);
    if (name == html_names::kInterestforAttr &&
        RuntimeEnabledFeatures::HTMLInterestForAttributeEnabled()) {
      UseCounter::Count(GetDocument(), WebFeature::kInterestFor);
      if (!params.old_value.IsNull()) {
        // We are changing the value of the `interestfor` attribute, so
        // ensure it doesn't have interest.
        ChangeInterestState(InterestForElement(), InterestState::kNoInterest);
      }
    }
  }

  InvalidateNodeListCachesInAncestors(&name, this, nullptr);

  if (isConnected()) {
    if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache()) {
      if (params.old_value != params.new_value) {
        cache->HandleAttributeChanged(name, this);
      }
    }
  }
}

bool Element::HasLegalLinkAttribute(const QualifiedName&) const {
  return false;
}

void Element::ClassAttributeChanged(const AtomicString& new_class_string) {
  DCHECK(HasElementData());
  // Note that this is a copy-by-value of the class names.
  const SpaceSplitString old_classes = GetElementData()->ClassNames();
  if (new_class_string.empty()) [[unlikely]] {
    GetDocument().GetStyleEngine().ClassChangedForElement(old_classes, *this);
    GetElementData()->ClearClass();
    return;
  }
  if (GetDocument().InQuirksMode()) [[unlikely]] {
    GetElementData()->SetClassFoldingCase(new_class_string);
  } else {
    GetElementData()->SetClass(new_class_string);
  }
  const SpaceSplitString& new_classes = GetElementData()->ClassNames();
  for (const AtomicString& class_name : new_classes) {
    attribute_or_class_bloom_ |= FilterForString(class_name);
  }
  UpdateSubtreeBloomFilterAfterInsert();
  GetDocument().GetStyleEngine().ClassChangedForElement(old_classes,
                                                        new_classes, *this);
}

void Element::UpdateSubtreeBloomFilterAfterInsert() {
  Element* to_update = this;
  Element* parent = parentElement();
  while (parent && (parent->attribute_or_class_bloom_ |
                    to_update->attribute_or_class_bloom_) !=
                       parent->attribute_or_class_bloom_) {
    parent->attribute_or_class_bloom_ |= to_update->attribute_or_class_bloom_;
    to_update = parent;
    parent = to_update->parentElement();
  }
}

void Element::UpdateSubtreeBloomFilterAfterChildRemoval() {
  Element* first_child = ElementTraversal::FirstChild(*this);
  if (first_child && first_child->nextSibling()) {
    // Two or more children left; don't do anything, and don't propagate.
    // Note that this may leave stale bits in the filter.
    return;
  }

  // Zero or one children left.
  TinyBloomFilter new_bloom_filter =
      first_child ? first_child->attribute_or_class_bloom_ : 0;
  new_bloom_filter |= RecomputeLocalBloomFilter();
  if (attribute_or_class_bloom_ == new_bloom_filter) {
    // No need to do anything, nor traverse upwards.
    return;
  }
  attribute_or_class_bloom_ = new_bloom_filter;

  // If the parent _also_ has a single child (us), update its
  // subtree Bloom filter as well, and so on. (It may never have
  // zero children, obviously.)
  for (Element *current = this, *parent = parentElement();
       parent && !current->previousElementSibling() &&
       !current->nextElementSibling();
       current = parent, parent = current->parentElement()) {
    new_bloom_filter |= parent->RecomputeLocalBloomFilter();
    if (parent->attribute_or_class_bloom_ == new_bloom_filter) {
      break;
    }
    parent->attribute_or_class_bloom_ = new_bloom_filter;
  }
}

Element::TinyBloomFilter Element::RecomputeLocalBloomFilter() const {
  TinyBloomFilter new_bloom_filter = 0;
  if (element_data_) {
    for (const AtomicString& class_name : element_data_->ClassNames()) {
      new_bloom_filter |= FilterForString(class_name);
    }
    for (const Attribute& attribute : element_data_->Attributes()) {
      new_bloom_filter |= FilterForAttribute(attribute.GetName());
    }
  }
  return new_bloom_filter;
}

bool Element::IsExcludedAttribute(
    const QualifiedName& qname,
    Element::AttributesToExcludeHashesFor attributes_to_exclude) {
  if (attributes_to_exclude == kExcludeStandardAttributesOnly) {
    return qname.LocalName() == html_names::kClassAttr.LocalName() ||
           qname.LocalName() == html_names::kIdAttr.LocalName() ||
           qname.LocalName() == html_names::kStyleAttr.LocalName();
  }

  DCHECK(attributes_to_exclude == kExcludeAllLazilySynchronizedAttributes ||
         attributes_to_exclude ==
             kExcludeLowercaseLazilySynchronizedAttributes);

  // Assume any known name needs synchronization.
  if (qname.IsDefinedName()) {
    return true;
  }
  const QualifiedName local_qname(qname.LocalName());
  if (local_qname.IsDefinedName()) {
    return true;
  }
  // HTML elements in an html doc use the lower case name.
  if (attributes_to_exclude == kExcludeLowercaseLazilySynchronizedAttributes ||
      qname.LocalName().IsLowerASCII()) {
    return false;
  }
  const QualifiedName lower_local_qname(qname.LocalName().LowerASCII());
  return lower_local_qname.IsDefinedName();
}

void Element::UpdateClassList(const AtomicString& old_class_string,
                              const AtomicString& new_class_string) {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    if (DOMTokenList* class_list = data->GetClassList()) {
      class_list->DidUpdateAttributeValue(old_class_string, new_class_string);
    }
  }
}

// Returns true if the given attribute is an event handler.
// We consider an event handler any attribute that begins with "on".
// It is a simple solution that has the advantage of not requiring any
// code or configuration change if a new event handler is defined.

static inline bool IsEventHandlerAttribute(const Attribute& attribute) {
  return attribute.GetName().NamespaceURI().IsNull() &&
         attribute.GetName().LocalName().StartsWith("on");
}

bool Element::AttributeValueIsJavaScriptURL(const Attribute& attribute) {
  return ProtocolIsJavaScript(
      StripLeadingAndTrailingHTMLSpaces(attribute.Value()));
}

bool Element::IsJavaScriptURLAttribute(const Attribute& attribute) const {
  return IsURLAttribute(attribute) && AttributeValueIsJavaScriptURL(attribute);
}

bool Element::IsScriptingAttribute(const Attribute& attribute) const {
  return IsEventHandlerAttribute(attribute) ||
         IsJavaScriptURLAttribute(attribute) ||
         IsHTMLContentAttribute(attribute) ||
         IsSVGAnimationAttributeSettingJavaScriptURL(attribute);
}

void Element::StripScriptingAttributes(
    Vector<Attribute, kAttributePrealloc>& attribute_vector) const {
  wtf_size_t destination = 0;
  for (wtf_size_t source = 0; source < attribute_vector.size(); ++source) {
    if (IsScriptingAttribute(attribute_vector[source])) {
      continue;
    }

    if (source != destination) {
      attribute_vector[destination] = attribute_vector[source];
    }

    ++destination;
  }
  attribute_vector.Shrink(destination);
}

void Element::ParserSetAttributes(
    const Vector<Attribute, kAttributePrealloc>& attribute_vector) {
  DCHECK(!isConnected());
  DCHECK(!parentNode());
  DCHECK(!element_data_);

  if (!attribute_vector.empty()) {
    if (ElementDataCache* cache = GetDocument().GetElementDataCache()) {
      element_data_ = cache->CachedShareableElementDataWithAttributes(
          localName().Impl(), attribute_vector);
    } else {
      element_data_ =
          ShareableElementData::CreateWithAttributes(attribute_vector);
    }

    DCHECK_EQ(nullptr, ElementTraversal::FirstChild(*this));

    // NOTE: AttributeChanged() will add back the class names (if any),
    // so it is safe to reset the filter here.
    attribute_or_class_bloom_ = 0;
    for (const Attribute& attribute : attribute_vector) {
      attribute_or_class_bloom_ |= FilterForAttribute(attribute.GetName());
    }
    UpdateSubtreeBloomFilterAfterInsert();
  }

  ParserDidSetAttributes();

  // Use attribute_vector instead of element_data_ because AttributeChanged
  // might modify element_data_.
  for (const auto& attribute : attribute_vector) {
    AttributeChanged(AttributeModificationParams(
        attribute.GetName(), g_null_atom, attribute.Value(),
        AttributeModificationReason::kByParser));
  }
}

bool Element::HasEquivalentAttributes(const Element& other) const {
  SynchronizeAllAttributes();
  other.SynchronizeAllAttributes();
  if (GetElementData() == other.GetElementData()) {
    return true;
  }
  if (HasElementData()) {
    return GetElementData()->IsEquivalent(other.GetElementData());
  }
  if (other.HasElementData()) {
    return other.GetElementData()->IsEquivalent(GetElementData());
  }
  return true;
}

String Element::nodeName() const {
  return tag_name_.ToString();
}

AtomicString Element::LocalNameForSelectorMatching() const {
  if (IsHTMLElement() || !IsA<HTMLDocument>(GetDocument())) {
    return localName();
  }
  return localName().LowerASCII();
}

const AtomicString& Element::LocateNamespacePrefix(
    const AtomicString& namespace_to_locate) const {
  if (!prefix().IsNull() && namespaceURI() == namespace_to_locate) {
    return prefix();
  }

  AttributeCollection attributes = Attributes();
  for (const Attribute& attr : attributes) {
    if (attr.Prefix() == g_xmlns_atom && attr.Value() == namespace_to_locate) {
      return attr.LocalName();
    }
  }

  if (Element* parent = parentElement()) {
    return parent->LocateNamespacePrefix(namespace_to_locate);
  }

  return g_null_atom;
}

const AtomicString Element::ImageSourceURL() const {
  return FastGetAttribute(html_names::kSrcAttr);
}

bool Element::LayoutObjectIsNeeded(const DisplayStyle& style) const {
  return style.Display() != EDisplay::kNone &&
         style.Display() != EDisplay::kContents;
}

bool Element::LayoutObjectIsNeeded(const ComputedStyle& style) const {
  return LayoutObjectIsNeeded(style.GetDisplayStyle());
}

LayoutObject* Element::CreateLayoutObject(const ComputedStyle& style) {
  return LayoutObject::CreateObject(this, style);
}

Node::InsertionNotificationRequest Element::InsertedInto(
    ContainerNode& insertion_point) {
  // need to do superclass processing first so isConnected() is true
  // by the time we reach updateId
  ContainerNode::InsertedInto(insertion_point);
  UpdateSubtreeBloomFilterAfterInsert();

  DCHECK(!GetElementRareData() || !GetElementRareData()->HasPseudoElements() ||
         GetDocument().StatePreservingAtomicMoveInProgress());

  RecomputeDirectionFromParent();

  auto* parent = ParentOrShadowHostElement();
  if (parent && parent->IsCanvasOrInCanvasSubtree()) {
    SetIsCanvasOrInCanvasSubtree(true);
  }

  if (AnchorElementObserver* observer = GetAnchorElementObserver()) {
    observer->Notify();
  }

  if (!insertion_point.IsInTreeScope()) {
    return kInsertionDone;
  }

  if (isConnected()) {
    if (ElementRareDataVector* rare_data = GetElementRareData()) {
      if (ElementIntersectionObserverData* observer_data =
              rare_data->IntersectionObserverData()) {
        observer_data->TrackWithController(
            GetDocument().EnsureIntersectionObserverController());
        if (!observer_data->IsEmpty()) {
          if (LocalFrameView* frame_view = GetDocument().View()) {
            frame_view->SetIntersectionObservationState(
                LocalFrameView::kRequired);
          }
        }
      }

      if (auto* context = rare_data->GetDisplayLockContext()) {
        context->ElementConnected();
      }

      if (DisplayAdElementMonitor* ad_monitor =
              rare_data->GetDisplayAdElementMonitor()) {
        ad_monitor->EnsureStarted();
      }
    }

    ProcessElementRenderBlocking(GetIdAttribute());
  }

  if (isConnected()) {
    EnqueueAutofocus(*this);

    if (GetCustomElementState() == CustomElementState::kCustom) {
      if (GetDocument().StatePreservingAtomicMoveInProgress()) {
        CustomElement::EnqueueConnectedMoveCallback(*this);
      } else {
        CustomElement::EnqueueConnectedCallback(*this);
      }
    } else if (GetCustomElementState() == CustomElementState::kUndefined) {
      CustomElement::TryToUpgrade(*this);
    }
  }

  TreeScope& scope = insertion_point.GetTreeScope();
  if (scope != GetTreeScope()) {
    return kInsertionDone;
  }

  const AtomicString& id_value = GetIdAttribute();
  if (!id_value.IsNull()) {
    UpdateId(scope, g_null_atom, id_value);
  }

  const AtomicString& name_value = GetNameAttribute();
  if (!name_value.IsNull()) {
    UpdateName(g_null_atom, name_value);
  }

  ExecutionContext* context = GetExecutionContext();
  if (RuntimeEnabledFeatures::FocusgroupEnabled(context)) {
    const AtomicString& focusgroup_value =
        FastGetAttribute(html_names::kFocusgroupAttr);
    if (!focusgroup_value.IsNull()) {
      UpdateFocusgroup(focusgroup_value);
    }

    // We parse the focusgroup attribute for the ShadowDOM elements before we
    // parse it for any of its root's ancestors, which might lead to an
    // incorrect focusgroup value. Re-run the algorithm for the ShadowDOM
    // elements when the ShadowRoot's parent gets inserted in the tree.
    if (GetShadowRoot()) {
      UpdateFocusgroupInShadowRootIfNeeded();
    }
  }

  EditContext* edit_context = editContext();
  if (edit_context && edit_context->GetExecutionContext() != context) {
    edit_context->SetExecutionContext(context);
  }

  if (GetDocument().StatePreservingAtomicMoveInProgress() &&
      Fullscreen::IsFullscreenElement(*this)) {
    // We don't actually need to cross frame boundaries, but we do need to mark
    // all our ancestors as containing a full screen element.
    SetContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(true);
  }

  if (!id_value.empty() &&
      GetDocument().OverscrollCommandTargets().Contains(id_value)) {
    for (auto& element : GetDocument().GetAllElementsById(id_value)) {
      element->OverscrollTargetStateChanged();
    }
  }

  return kInsertionDone;
}

void Element::MovedFrom(ContainerNode& old_parent) {
  Node::MovedFrom(old_parent);

  DCHECK(!GetDocument().StatePreservingAtomicMoveInProgress());

  // `old_parent` can be the document.
  if (!old_parent.IsElementNode()) {
    return;
  }

  Element* focused_element = GetDocument().FocusedElement();
  Element* new_parent_element = parentElement();
  Element* old_parent_element = &To<Element>(old_parent);
  if (focused_element && old_parent.HasFocusWithin() &&
      contains(focused_element) && old_parent != *new_parent_element) {
    Element* common_ancestor = To<Element>(NodeTraversal::CommonAncestor(
        *old_parent_element, *new_parent_element));

    // The "focus within" flag is set separately on each ancestor, and affects
    // the :focus-within CSS property. We set it to the right value here because
    // we skipped the step that sets it on removal/insertion.
    old_parent_element->SetHasFocusWithinUpToAncestor(
        false, common_ancestor, /*need_snap_container_search=*/false);
    new_parent_element->SetHasFocusWithinUpToAncestor(
        true, common_ancestor, /*need_snap_container_search=*/false);
  }
}

void Element::SetIsCanvasOrInCanvasSubtree(bool value) {
  if (value == IsCanvasOrInCanvasSubtree()) {
    return;
  }
  SetElementFlag(ElementFlags::kIsCanvasOrInCanvasSubtree, value);
  DidChangeIsCanvasOrInCanvasSubtree();
}

void Element::RemovedFrom(ContainerNode& insertion_point) {
  bool was_in_document = insertion_point.isConnected();
  if (Element* parent = DynamicTo<Element>(insertion_point)) {
    parent->UpdateSubtreeBloomFilterAfterChildRemoval();
  }

  if (!GetDocument().StatePreservingAtomicMoveInProgress()) {
    SetComputedStyle(nullptr);
  }

  if (Fullscreen::IsFullscreenElement(*this)) {
    SetContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(false);
    if (auto* insertion_point_element = DynamicTo<Element>(insertion_point)) {
      insertion_point_element->SetContainsFullScreenElement(false);
      insertion_point_element
          ->SetContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(
              false);
    }
  }
  Document& document = GetDocument();
  Page* page = document.GetPage();
  if (page) {
    page->GetPointerLockController().ElementRemoved(this);
  }

  document.UnobserveForIntrinsicSize(this);
  if (auto* local_frame_view = document.View();
      local_frame_view &&
      (LastRememberedInlineSize() || LastRememberedBlockSize())) {
    local_frame_view->NotifyElementWithRememberedSizeDisconnected(this);
  }

  SetSavedLayerScrollOffset(ScrollOffset());

  const AtomicString& id_value = GetIdAttribute();
  if (insertion_point.IsInTreeScope() && GetTreeScope() == document) {
    if (!id_value.IsNull()) {
      UpdateId(insertion_point.GetTreeScope(), id_value, g_null_atom);
    }

    const AtomicString& name_value = GetNameAttribute();
    if (!name_value.IsNull()) {
      UpdateName(name_value, g_null_atom);
    }
  }

  if (ElementRareDataVector* data = GetElementRareData()) {
    if (InvokerData* invoker_data = data->GetInvokerData()) [[unlikely]] {
      // The element being removed is an interest invoker - move it to the
      // no-interest state and cancel any pending interest tasks.
      if (invoker_data->GetInterestState() != InterestState::kNoInterest) {
        DCHECK(RuntimeEnabledFeatures::HTMLInterestForAttributeEnabled());
        ChangeInterestState(invoker_data->ActiveInterestTarget(),
                            InterestState::kNoInterest);
      }
    }
    if (InterestInvokerTargetData* target_data =
            data->GetInterestInvokerTargetData()) [[unlikely]] {
      DCHECK(RuntimeEnabledFeatures::HTMLInterestForAttributeEnabled());
      if (Element* invoker = target_data->interestInvoker();
          invoker &&
          invoker->GetInterestState() != InterestState::kNoInterest &&
          invoker->GetInvokerData()->ActiveInterestTarget() == this) {
        // The element being removed is the target of an active interest
        // invoker. Set that invoker to the no-interest state.
        invoker->ChangeInterestState(this, InterestState::kNoInterest);
      }
    }

    if (DisplayAdElementMonitor* ad_monitor =
            data->GetDisplayAdElementMonitor()) {
      ad_monitor->OnElementRemovedOrUntagged();
    }
  }

  ContainerNode::RemovedFrom(insertion_point);

  if (was_in_document &&
      GetCustomElementState() == CustomElementState::kCustom &&
      !GetDocument().StatePreservingAtomicMoveInProgress()) {
    CustomElement::EnqueueDisconnectedCallback(*this);
  }

  RecomputeDirectionFromParent();

  document.GetRootScrollerController().ElementRemoved(*this);

  if (IsInTopLayer() && !document.StatePreservingAtomicMoveInProgress()) {
    Fullscreen::ElementRemoved(*this);
    document.RemoveFromTopLayerImmediately(this);
  }

  SetIsCanvasOrInCanvasSubtree(false);

  if (ElementRareDataVector* data = GetElementRareData()) {
    data->ClearFocusgroupData();
    data->ClearRestyleFlags();

    if (!GetDocument().StatePreservingAtomicMoveInProgress()) {
      if (ElementAnimations* element_animations =
              data->GetElementAnimations()) {
        element_animations->CssAnimations().Cancel();
      }
    }

    NodeRareData* node_data = RareData();
    node_data->InvalidateAssociatedAnimationEffects();

    if (auto* context = data->GetDisplayLockContext()) {
      context->ElementDisconnected();
    }

    DCHECK(!data->HasPseudoElements() ||
           GetDocument().StatePreservingAtomicMoveInProgress());
  }

  if (auto* const frame = document.GetFrame()) {
    if (HasUndoStack()) [[unlikely]] {
      frame->GetEditor().GetUndoStack().ElementRemoved(this);
    }
    frame->GetEditor().ElementRemoved(this);
    frame->GetSpellChecker().ElementRemoved(this);
    frame->GetEventHandler().ElementRemoved(this);
  }

  if (AnchorElementObserver* observer = GetAnchorElementObserver()) {
    observer->Notify();
  }

  if (!id_value.empty() &&
      document.OverscrollCommandTargets().Contains(id_value)) {
    for (auto& element : document.GetAllElementsById(id_value)) {
      element->OverscrollTargetStateChanged();
    }
  }

  // Removing an element means that we should remove this overscroll area,
  // since we won't visit this node during style when we typically would do
  // this. There may be another element with the same ID that we discover
  // during the style walk, but that's OK since we will just add it back to
  // the overscroll area.
  // We do this outside of the OverscrollCommandTargets check since we could,
  // for instance, remove the element's id first and then remove it from the
  // DOM.
  if (auto* container = OverscrollContainer()) {
    container->OverscrollAreaTracker()->RemoveOverscroll(this);
  }

  // Remove all of the overscroll areas from this tracker.
  if (auto* tracker = OverscrollAreaTracker()) {
    tracker->RemoveAllOverscroll();
  }
}

void Element::AttachColumnPseudoElements(AttachContext& context) {
  if (const ColumnPseudoElementsVector* columns = GetColumnPseudoElements()) {
    for (ColumnPseudoElement* column : *columns) {
      column->AttachLayoutTree(context);
    }
  }
}

void Element::DetachColumnPseudoElements(bool performing_reattach) {
  if (const ColumnPseudoElementsVector* columns = GetColumnPseudoElements()) {
    for (ColumnPseudoElement* column : *columns) {
      column->DetachLayoutTree(performing_reattach);
    }
  }
}

void Element::AttachLayoutTree(AttachContext& context) {
  DCHECK(GetDocument().InStyleRecalc() ||
         GetDocument().GetStyleEngine().InScrollMarkersAttachment());

  StyleEngine& style_engine = GetDocument().GetStyleEngine();

  const ComputedStyle* style = GetComputedStyle();
  bool being_rendered =
      context.parent && style && !style->IsEnsuredInDisplayNone();

  bool skipped_container_descendants = SkippedContainerStyleRecalc();

  if (!being_rendered && !ChildNeedsReattachLayoutTree()) {
    // We may have skipped recalc for this Element if it's a query container for
    // size queries. This recalc must be resumed now, since we're not going to
    // create a LayoutObject for the Element after all.
    if (skipped_container_descendants) {
      style_engine.UpdateStyleForNonEligibleSizeContainer(*this);
      skipped_container_descendants = false;
    }
    // The above recalc may have marked some descendant for reattach, which
    // would set the child-needs flag.
    if (!ChildNeedsReattachLayoutTree()) {
      Node::AttachLayoutTree(context);
      return;
    }
  }

  AttachLayoutPrecedingPseudoElements(context);

  AttachContext children_context(context);
  LayoutObject* layout_object = nullptr;
  if (being_rendered) {
    LayoutTreeBuilderForElement builder(*this, context, style);
    builder.CreateLayoutObject();

    layout_object = GetLayoutObject();
    if (layout_object) {
      children_context.previous_in_flow = nullptr;
      children_context.parent = layout_object;
      children_context.next_sibling = nullptr;
      children_context.next_sibling_valid = true;
    } else if (style->Display() != EDisplay::kContents) {
      // The layout object creation was suppressed for other reasons than
      // being display:none or display:contents (E.g.
      // LayoutObject::CanHaveChildren() returning false). Make sure we don't
      // attempt to create LayoutObjects further down the subtree.
      children_context.parent = nullptr;
    }
    // For display:contents elements, we keep the previous_in_flow,
    // next_sibling, and parent, in the context for attaching children.
  } else {
    // We are a display:none element. Set the parent to nullptr to make sure
    // we never create any child layout boxes.
    children_context.parent = nullptr;
  }
  children_context.use_previous_in_flow = true;

  AttachLayoutSucceedingPseudoElements(context);

  if (skipped_container_descendants &&
      (!layout_object || !layout_object->IsEligibleForSizeContainment())) {
    style_engine.UpdateStyleForNonEligibleSizeContainer(*this);
    skipped_container_descendants = false;
  }

  bool skip_lock_descendants = ChildStyleRecalcBlockedByDisplayLock();
  if (skipped_container_descendants || skip_lock_descendants) {
    // Since we block style recalc on descendants of this node due to display
    // locking or container queries, none of its descendants should have the
    // NeedsReattachLayoutTree bit set.
    DCHECK(!ChildNeedsReattachLayoutTree());

    if (skip_lock_descendants) {
      // If an element is locked we shouldn't attach the layout tree for its
      // descendants. We should notify that we blocked a reattach so that we
      // will correctly attach the descendants when allowed.
      GetDisplayLockContext()->NotifyReattachLayoutTreeWasBlocked();
    }
    Node::AttachLayoutTree(context);
    if (layout_object && layout_object->AffectsWhitespaceSiblings()) {
      context.previous_in_flow = layout_object;
    }
    return;
  }

  if (!IsPseudoElement() && layout_object) {
    context.counters_context.EnterObject(*layout_object);
  }

  AttachColumnPseudoElements(children_context);
  AttachPrecedingPseudoElements(children_context);

  if (ShadowRoot* shadow_root = GetShadowRoot()) {
    // When a shadow root exists, it does the work of attaching the children.
    shadow_root->AttachLayoutTree(children_context);
    Node::AttachLayoutTree(context);
    ClearChildNeedsReattachLayoutTree();
  } else if (HTMLSlotElement* slot =
                 ToHTMLSlotElementIfSupportsAssignmentOrNull(this)) {
    slot->AttachLayoutTreeForSlotChildren(children_context);
    Node::AttachLayoutTree(context);
    ClearChildNeedsReattachLayoutTree();
  } else {
    ContainerNode::AttachLayoutTree(children_context);
  }

  AttachSucceedingPseudoElements(children_context);
  AttachTransitionPseudoElements(children_context);

  if (!IsPseudoElement() && layout_object) {
    context.counters_context.LeaveObject(*layout_object);
  }

  if (layout_object) {
    if (layout_object->AffectsWhitespaceSiblings()) {
      context.previous_in_flow = layout_object;
    }
    layout_object->HandleSubtreeModifications();
  } else {
    context.previous_in_flow = children_context.previous_in_flow;
  }
}

void Element::DetachLayoutTree(bool performing_reattach) {
  HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;

  // Pseudo-elements that may have child pseudo-elements (such as ::column) must
  // be cleared before clearing the rare data vector below.
  ClearColumnPseudoElements();

  if (ElementRareDataVector* data = GetElementRareData()) {
    if (!performing_reattach) {
      data->ClearPseudoElements();
      data->ClearContainerQueryData();
      data->ClearOutOfFlowData();
      data->RemoveScrollMarkerGroupData();
    } else if (data->GetOutOfFlowData()) {
      GetDocument()
          .GetStyleEngine()
          .MarkLastSuccessfulPositionFallbackDirtyForElement(*this);
    }

    if (ElementAnimations* element_animations = data->GetElementAnimations()) {
      if (!performing_reattach) {
        DocumentLifecycle::DetachScope will_detach(GetDocument().Lifecycle());
        element_animations->CssAnimations().Cancel();
        element_animations->SetAnimationStyleChange(false);
      }
      element_animations->RestartAnimationOnCompositor();
    }

    data->RemoveAnchorPositionScrollData();
  }

  DetachColumnPseudoElements(performing_reattach);
  DetachPrecedingPseudoElements(performing_reattach);

  auto* context = GetDisplayLockContext();

  // TODO(futhark): We need to traverse into IsUserActionElement() subtrees,
  // even if they are already display:none because we do not clear the
  // hovered/active bits as part of style recalc, but wait until the next time
  // we do a hit test. That means we could be doing a forced layout tree update
  // making a hovered subtree display:none and immediately remove the subtree
  // leaving stale hovered/active state on ancestors. See relevant issues:
  // https://crbug.com/967548
  // https://crbug.com/939769
  if (ChildNeedsReattachLayoutTree() || GetComputedStyle() ||
      (!performing_reattach && IsUserActionElement())) {
    if (ShadowRoot* shadow_root = GetShadowRoot()) {
      shadow_root->DetachLayoutTree(performing_reattach);
      Node::DetachLayoutTree(performing_reattach);
    } else {
      ContainerNode::DetachLayoutTree(performing_reattach);
    }
  } else {
    Node::DetachLayoutTree(performing_reattach);
  }

  DetachSucceedingPseudoElements(performing_reattach);
  DetachTransitionPseudoElements(performing_reattach);

  if (!performing_reattach) {
    UpdateCallbackSelectors(GetComputedStyle(), nullptr);
    NotifyIfMatchedDocumentRulesSelectorsChanged(GetComputedStyle(), nullptr);
    SetComputedStyle(nullptr);
  }

  if (!performing_reattach && IsUserActionElement()) {
    if (IsHovered()) {
      GetDocument().HoveredElementDetached(*this);
    }
    if (InActiveChain()) {
      GetDocument().ActiveChainNodeDetached(*this);
    }
    GetDocument().UserActionElements().DidDetach(*this);
  }

  if (context) {
    context->DetachLayoutTree();
  }
}

void Element::ReattachLayoutTreeChildren(base::PassKey<StyleEngine>) {
  DCHECK(NeedsReattachLayoutTree());
  DCHECK(ChildNeedsReattachLayoutTree());
  DCHECK(GetLayoutObject());

  constexpr bool performing_reattach = true;

  DetachPrecedingPseudoElements(performing_reattach);

  ShadowRoot* shadow_root = GetShadowRoot();

  if (shadow_root) {
    shadow_root->DetachLayoutTree(performing_reattach);
  } else {
    // Can not use ContainerNode::DetachLayoutTree() because that would also
    // call Node::DetachLayoutTree for this element.
    for (Node* child = firstChild(); child; child = child->nextSibling()) {
      child->DetachLayoutTree(performing_reattach);
    }
  }

  DetachSucceedingPseudoElements(performing_reattach);

  LayoutObject* layout_object = GetLayoutObject();
  AttachContext context;
  context.parent = layout_object;
  context.performing_reattach = performing_reattach;
  context.use_previous_in_flow = true;
  context.next_sibling_valid = true;

  if (!IsPseudoElement()) {
    DCHECK(layout_object);
    context.counters_context.EnterObject(*layout_object);
  }

  AttachPrecedingPseudoElements(context);

  if (shadow_root) {
    shadow_root->AttachLayoutTree(context);
  } else {
    // Can not use ContainerNode::DetachLayoutTree() because that would also
    // call Node::AttachLayoutTree for this element.
    for (Node* child = firstChild(); child; child = child->nextSibling()) {
      child->AttachLayoutTree(context);
    }
  }

  AttachSucceedingPseudoElements(context);

  if (!IsPseudoElement()) {
    DCHECK(layout_object);
    context.counters_context.LeaveObject(*layout_object);
  }

  ClearChildNeedsReattachLayoutTree();
  ClearNeedsReattachLayoutTree();
}

const ComputedStyle* Element::StyleForLayoutObject(
    const StyleRecalcContext& style_recalc_context) {
  DCHECK(GetDocument().InStyleRecalc());

  StyleRecalcContext new_style_recalc_context(style_recalc_context);

  if (ElementAnimations* element_animations = GetElementAnimations()) {
    // For multiple style recalc passes for the same element in the same
    // lifecycle, which can happen for container queries, we may end up having
    // pending updates from the previous pass. In that case the update from the
    // previous pass should be dropped as it will be re-added if necessary. It
    // may be that an update detected in the previous pass would no longer be
    // necessary if the animated property flipped back to the old style with no
    // change as the result.
    DCHECK(GetDocument().GetStyleEngine().InInterleavedStyleRecalc() ||
           PostStyleUpdateScope::InPendingPseudoUpdate() ||
           element_animations->CssAnimations().PendingUpdate().IsEmpty());
    element_animations->CssAnimations().ClearPendingUpdate();
  }

  new_style_recalc_context.old_style = PostStyleUpdateScope::GetOldStyle(*this);
  const ComputedStyle* style =
      HasCustomStyleCallbacks()
          ? CustomStyleForLayoutObject(new_style_recalc_context)
          : OriginalStyleForLayoutObject(new_style_recalc_context);
  if (!style) {
    DCHECK(IsPseudoElement());
    return nullptr;
  }
  if (style->IsStartingStyle()) {
    // @starting-style styles matched. We need to compute the style a second
    // time to compute the actual style and trigger transitions starting from
    // style with @starting-style applied.
    new_style_recalc_context.old_style =
        style->Display() == EDisplay::kNone ? nullptr : style;
    style = HasCustomStyleCallbacks()
                ? CustomStyleForLayoutObject(new_style_recalc_context)
                : OriginalStyleForLayoutObject(new_style_recalc_context);
    if (!style) {
      DCHECK(IsPseudoElement());
      return nullptr;
    }

    // This sets the flag on the element when starting style are detected. There
    // is no reliable way to detect whether starting styles no longer affect an
    // element, so this flag is "sticky".
    if (!AffectedByStartingStyles()) {
      SetAffectedByStartingStyles();
      probe::UpdateAffectedByStartingStylesFlag(this, /*override_flag=*/true);
    }
  }

  DisplayLockContext* context = GetDisplayLockContext();
  // The common case for most elements is that we don't have a context and have
  // the default (visible) content-visibility value.
  if (context || !style->IsContentVisibilityVisible()) [[unlikely]] {
    if (!context) {
      context = &EnsureDisplayLockContext();
    }
    context->SetRequestedState(style->ContentVisibility());
    context->SetHasScrollerWithScrollMarkerGroup(
        style_recalc_context
            .has_scroller_ancestor_with_scroll_marker_group_property);
    style = context->AdjustElementStyle(style);
  }

  if (style->DependsOnSizeContainerQueries() ||
      style->GetPositionTryFallbacks() || style->HasAnchorFunctions() ||
      style->HasAnimationTrigger()) {
    GetDocument().GetStyleEngine().SetStyleAffectedByLayout();
  }

  return style;
}

void Element::AdjustStyle(base::PassKey<StyleAdjuster>,
                          ComputedStyleBuilder& builder) {
  AdjustStyle(builder);
}

const ComputedStyle* Element::OriginalStyleForLayoutObject(
    const StyleRecalcContext& style_recalc_context) {
  return GetDocument().GetStyleResolver().ResolveStyle(this,
                                                       style_recalc_context);
}

void Element::RecalcStyleForTraversalRootAncestor() {
  if (!ChildNeedsReattachLayoutTree()) {
    UpdateFirstLetterPseudoElement(StyleUpdatePhase::kRecalc);
  }
  if (HasCustomStyleCallbacks()) {
    DidRecalcStyle({});
  }
}

bool Element::SkipStyleRecalcForContainer(
    const ComputedStyle& style,
    const StyleRecalcChange& child_change,
    const StyleRecalcContext& style_recalc_context) {
  if (!GetDocument().GetStyleEngine().SkipStyleRecalcAllowed()) {
    return false;
  }

  if (!child_change.TraversePseudoElements(*this)) {
    // If none of the children or pseudo-elements need to be traversed for style
    // recalc, there is no point in marking the subtree as skipped.
    DCHECK(!child_change.TraverseChildren(*this));
    return false;
  }

  if (!child_change.ReattachLayoutTree()) {
    LayoutObject* layout_object = GetLayoutObject();
    if (!layout_object ||
        !WillUpdateSizeContainerDuringLayout(*layout_object)) {
      return false;
    }
  }

  // Don't skip style recalc for form controls. The reason for skipping is a
  // baseline inconsistency issue laying out an input element with a placeholder
  // when interleaving layout and style recalc. This bigger cannon is to avoid
  // potential issues with other peculiarities inside form controls.
  if (IsFormControlElement()) {
    return false;
  }

  // If we are moving the ::backdrop element to the top layer while laying out
  // its originating element, it means we will add a layout-dirty box as a
  // preceding sibling of the originating element's box which means we will not
  // reach the box for ::backdrop during layout. Don't skip style recalc for
  // children of containers in the top layer for this reason.
  if (style.IsRenderedInTopLayer(*this)) {
    return false;
  }

  // We are both a size container and trying to compute interleaved styles
  // from out-of-flow layout. Our children should be the first opportunity to
  // skip recalc.
  //
  // Note that anchor_evaluator will be non-null only for the root element
  // of the interleaved style recalc.
  if (style_recalc_context.anchor_evaluator) {
    return false;
  }

  // ::scroll-marker-group and ::scroll-button() boxes are created outside their
  // originating element's box and cannot be skipped if the originating element
  // is a size container because the pseudo-element and its box need to be
  // created before layout.
  if (!style.ScrollMarkerGroupNone() ||
      CanGeneratePseudoElement(kPseudoIdScrollButton) ||
      HasSiblingBoxPseudoElements()) {
    return false;
  }

  // Store the child_change so that we can continue interleaved style layout
  // from where we left off.
  EnsureElementRareData().EnsureContainerQueryData().SkipStyleRecalc(
      child_change.ForceMarkReattachLayoutTree());

  GetDocument().GetStyleEngine().IncrementSkippedContainerRecalc();

  if (HasCustomStyleCallbacks()) {
    DidRecalcStyle(child_change);
  }

  // This needs to be cleared to satisfy the DCHECKed invariants in
  // Element::RebuildLayoutTree(). ChildNeedsStyleRecalc() is flipped back on
  // before resuming the style recalc when the container is laid out. The stored
  // child_change contains the correct flags to resume recalc of child nodes.
  ClearChildNeedsStyleRecalc();
  return true;
}

void Element::MarkNonSlottedHostChildrenForStyleRecalc() {
  // Mark non-slotted children of shadow hosts for style recalc for forced
  // subtree recalcs when they have ensured computed style outside the flat
  // tree. Elements outside the flat tree are not recomputed during the style
  // recalc step, but we need to make sure the ensured styles are dirtied so
  // that we know to clear out old styles from
  // StyleEngine::ClearEnsuredDescendantStyles() the next time we call
  // getComputedStyle() on any of the descendant elements.
  for (Node* child = firstChild(); child; child = child->nextSibling()) {
    if (child->NeedsStyleRecalc()) {
      continue;
    }
    if (auto* element = DynamicTo<Element>(child)) {
      if (auto* style = element->GetComputedStyle()) {
        if (style->IsEnsuredOutsideFlatTree()) {
          child->SetStyleChangeForNonSlotted();
        }
      }
    }
  }
}

const ComputedStyle* Element::ParentComputedStyle() const {
  Element* parent = LayoutTreeBuilderTraversal::ParentElement(*this);
  const bool is_rendered_as_sibling = IsBackdropPseudoElement() ||
                                      IsScrollButtonPseudoElement() ||
                                      IsScrollMarkerGroupPseudoElement();
  if (parent && (parent->ChildrenCanHaveStyle() || is_rendered_as_sibling)) {
    const ComputedStyle* parent_style = parent->GetComputedStyle();
    if (parent_style && !parent_style->IsEnsuredInDisplayNone()) {
      return parent_style;
    }
  }
  return nullptr;
}

// Recalculate the style for this element, and if that element notes
// that children must also be recalculated, call ourself recursively
// on any children (via RecalcDescendantStyles()), and/or update
// pseudo-elements.
void Element::RecalcStyle(const StyleRecalcChange change,
                          const StyleRecalcContext& style_recalc_context) {
  DCHECK(InActiveDocument());
  DCHECK(GetDocument().InStyleRecalc());
  DCHECK(!GetDocument().Lifecycle().InDetach());
  DCHECK(!GetForceReattachLayoutTree() || GetComputedStyle())
      << "No need to force a layout tree reattach if we had no computed style";
  DCHECK(LayoutTreeBuilderTraversal::ParentElement(*this) ||
         this == GetDocument().documentElement())
      << "No recalc for Elements outside flat tree";

  DisplayLockStyleScope display_lock_style_scope(this);
  if (HasCustomStyleCallbacks()) {
    WillRecalcStyle(change);
  }

  StyleScopeFrame style_scope_frame(
      *this, /* parent */ style_recalc_context.style_scope_frame);
  StyleRecalcContext local_style_recalc_context = style_recalc_context;
  local_style_recalc_context.style_scope_frame = &style_scope_frame;

  StyleRecalcChange child_change =
      IsPseudoElement() ? change.ForPseudoElement() : change.ForChildren(*this);
  if (change.ShouldRecalcStyleFor(*this)) {
    child_change = RecalcOwnStyle(change, local_style_recalc_context);
    if (GetStyleChangeType() == kSubtreeStyleChange) {
      child_change =
          child_change.EnsureAtLeast(StyleRecalcChange::kRecalcDescendants);
    }
    ClearNeedsStyleRecalc();
  } else if (GetForceReattachLayoutTree() ||
             (change.MarkReattachLayoutTree() && GetComputedStyle())) {
    SetNeedsReattachLayoutTree();
    child_change = child_change.ForceReattachLayoutTree();
    ClearNeedsStyleRecalc();
  }

  // We may need to update the internal CSSContainerValues of the
  // ContainerQueryEvaluator if e.g. the value of the 'rem' unit or container-
  // relative units changed. It are not guaranteed to reach RecalcOwnStyle for
  // the container, so this update happens here instead.
  if (ContainerQueryEvaluator* evaluator = GetContainerQueryEvaluator()) {
    evaluator->UpdateContainerValuesFromUnitChanges(child_change);
  }

  // We're done with self style, notify the display lock.
  child_change = display_lock_style_scope.DidUpdateSelfStyle(child_change);
  if (!display_lock_style_scope.ShouldUpdateChildStyle()) {
    display_lock_style_scope.NotifyChildStyleRecalcWasBlocked(child_change);
    if (HasCustomStyleCallbacks()) {
      DidRecalcStyle(child_change);
    }
    return;
  }

  const StyleRecalcContext child_recalc_context =
      StyleRecalcContext::FromParentContext(local_style_recalc_context, *this);

  if (ContainerQueryData* cq_data = GetContainerQueryData()) {
    // If we skipped the subtree during style recalc, retrieve the
    // StyleRecalcChange which was the current change for the skipped subtree
    // and combine it with the current child_change.
    if (cq_data->SkippedStyleRecalc()) {
      GetDocument().GetStyleEngine().DecrementSkippedContainerRecalc();
      child_change = cq_data->ClearAndReturnRecalcChangeForChildren().Combine(
          child_change);
    }
  }

  if (const ComputedStyle* style = GetComputedStyle()) {
    if (style->CanMatchSizeContainerQueries(*this)) {
      // IsSuppressed() means we are at the root of a container subtree called
      // from UpdateStyleAndLayoutTreeForSizeContainer(). If so, we can not skip
      // recalc again. Otherwise, we may skip recalc of the subtree if we can
      // guarantee that we will be able to resume during layout later.
      if (!change.IsSuppressed()) {
        if (SkipStyleRecalcForContainer(*style, child_change,
                                        style_recalc_context)) {
          return;
        }
      }
    }
  }

  if (LayoutObject* layout_object = GetLayoutObject()) {
    // If a layout subtree was synchronously detached on DOM or flat tree
    // changes, we need to revisit the element during layout tree rebuild for
    // two reasons:
    //
    // 1. SubtreeDidChange() needs to be called on list-item layout objects
    //    ancestors for markers (see SubtreeDidChange() implementation on list
    //    item layout objects).
    // 2. Whitespace siblings of removed subtrees may change to have their
    //    layout object added or removed as the need for rendering the
    //    whitespace may have changed.
    bool mark_ancestors = layout_object->WasNotifiedOfSubtreeChange();
    if (layout_object->WhitespaceChildrenMayChange()) {
      if (LayoutTreeBuilderTraversal::FirstChild(*this)) {
        mark_ancestors = true;
      } else {
        layout_object->SetWhitespaceChildrenMayChange(false);
      }
    }
    if (mark_ancestors) {
      MarkAncestorsWithChildNeedsReattachLayoutTree();
    }
  }
  StyleRecalcContext layout_sibling_recalc_context = child_recalc_context;
  if (!IsDocumentElement()) {
    // The ::scroll-marker-group/::scroll-button() box is a sibling of its
    // originating element, which means that it's laid out before or after its
    // originating element. That means the ::scroll-marker-group is not
    // contained by its parent, and size container queries will have layout
    // cycles if the originating element is an eligible size query container.
    // There is an exception when the originating element is the root element,
    // since these pseudo elements generate child boxes in that case.
    //
    // Note that the originating element can still be a query container for
    // style() and scroll-state() queries.
    //
    // Use the same start size query container candidate as the originating
    // element to allow querying container further up the ancestor chain.
    layout_sibling_recalc_context.size_container =
        local_style_recalc_context.size_container;
  }
  if (child_change.TraversePseudoElements(*this)) {
    UpdateBackdropPseudoElement(child_change, child_recalc_context);
    UpdatePseudoElement(kPseudoIdMarker, child_change, child_recalc_context);
    UpdatePseudoElement(kPseudoIdScrollMarkerGroupBefore, child_change,
                        layout_sibling_recalc_context);
    UpdatePseudoElement(kPseudoIdScrollButtonBlockStart, child_change,
                        layout_sibling_recalc_context);
    UpdatePseudoElement(kPseudoIdScrollButtonInlineStart, child_change,
                        layout_sibling_recalc_context);
    UpdatePseudoElement(kPseudoIdScrollButtonInlineEnd, child_change,
                        layout_sibling_recalc_context);
    UpdatePseudoElement(kPseudoIdScrollButtonBlockEnd, child_change,
                        layout_sibling_recalc_context);
    UpdatePseudoElement(kPseudoIdScrollMarker, child_change,
                        child_recalc_context);
    UpdateColumnPseudoElements(child_change, child_recalc_context);

    if (IsA<HTMLMenuItemElement>(this)) {
      UpdatePseudoElement(kPseudoIdCheckMark, child_change,
                          child_recalc_context);
    }

    if (DynamicTo<HTMLOptionElement>(this)) {
      UpdatePseudoElement(kPseudoIdCheckMark, child_change,
                          child_recalc_context);
    }

    UpdatePseudoElement(kPseudoIdBefore, child_change, child_recalc_context);
  }

  if (child_change.TraverseChildren(*this)) {
    if (ShadowRoot* root = GetShadowRoot()) {
      root->RecalcDescendantStyles(child_change, child_recalc_context, *this);
      if (child_change.RecalcDescendants()) {
        MarkNonSlottedHostChildrenForStyleRecalc();
      }
    } else if (auto* slot = ToHTMLSlotElementIfSupportsAssignmentOrNull(this)) {
      SelectorFilterParentScope filter_scope(
          this, SelectorFilterParentScope::ScopeType::kParent);
      slot->RecalcStyleForSlotChildren(child_change, child_recalc_context);
    } else {
      RecalcDescendantStyles(child_change, child_recalc_context, *this);
    }
  }

  if (child_change.TraversePseudoElements(*this)) {
    UpdatePseudoElement(kPseudoIdAfter, child_change, child_recalc_context);

    if (IsA<HTMLSelectElement>(this)) {
      UpdatePseudoElement(kPseudoIdPickerIcon, child_change,
                          child_recalc_context);
    }

    if (RuntimeEnabledFeatures::HTMLInterestForInterestHintPseudoEnabled(
            GetExecutionContext())) {
      UpdatePseudoElement(kPseudoIdInterestHint, child_change,
                          child_recalc_context);
    }

    UpdatePseudoElement(kPseudoIdScrollMarkerGroupAfter, child_change,
                        layout_sibling_recalc_context);

    // If we are re-attaching us or any of our descendants, we need to attach
    // the descendants before we know if this element generates a ::first-letter
    // and which element the ::first-letter inherits style from.
    //
    // If style recalc was suppressed for this element, it means it's a size
    // query container, and child_change.ReattachLayoutTree() comes from the
    // skipped style recalc. In that case we haven't updated the style, and we
    // will not update the ::first-letter style in the originating element's
    // AttachLayoutTree().
    if (child_change.ReattachLayoutTree() && !change.IsSuppressed()) {
      // Make sure we reach this element during reattachment. There are cases
      // where we compute and store the styles for a subtree but stop attaching
      // layout objects at an element that does not allow child boxes. Marking
      // dirty for re-attachment means we AttachLayoutTree() will still traverse
      // down to all elements with a ComputedStyle which clears the
      // NeedsStyleRecalc() flag.
      if (PseudoElement* first_letter =
              GetPseudoElement(kPseudoIdFirstLetter)) {
        first_letter->SetNeedsReattachLayoutTree();
      }
    } else if (!ChildNeedsReattachLayoutTree()) {
      UpdateFirstLetterPseudoElement(StyleUpdatePhase::kRecalc,
                                     child_recalc_context);
    }

    UpdateOverscrollPseudoElements(child_change, child_recalc_context);
    UpdateTransitionPseudoElements(child_change, child_recalc_context);
  }

  ClearChildNeedsStyleRecalc();
  // We've updated all the children that needs an update (might be 0).
  display_lock_style_scope.DidUpdateChildStyle();

  if (HasCustomStyleCallbacks()) {
    DidRecalcStyle(child_change);
  }
}

const ComputedStyle* Element::PropagateInheritedProperties() {
  if (IsPseudoElement()) {
    return nullptr;
  }
  if (NeedsStyleRecalc()) {
    return nullptr;
  }
  if (HasAnimations()) {
    return nullptr;
  }
  if (HasCustomStyleCallbacks()) {
    return nullptr;
  }
  const ComputedStyle* parent_style = ParentComputedStyle();
  DCHECK(parent_style);
  const ComputedStyle* style = GetComputedStyle();
  if (!style || style->Animations() || style->Transitions() ||
      style->HasVariableReference() || style->HasVariableDeclaration()) {
    return nullptr;
  }
  if (style->InsideLink() != EInsideLink::kNotInsideLink) {
    // We cannot do the inherited propagation optimization within links,
    // since -internal-visited-color is handled in CascadeExpansion
    // (which we do not run in that path), and we also have no tracking
    // of whether the property was inherited or not.
    return nullptr;
  }
  if (style->HasAppliedTextDecorations()) {
    // If we have text decorations, they can depend on currentColor,
    // and are normally updated by the StyleAdjuster. We can, however,
    // reach this path when color is modified, leading to the decoration
    // being the wrong color (see crbug.com/1330953). We could rerun
    // the right part of the StyleAdjuster here, but it's simpler just to
    // disable the optimization in such cases (especially as we have already
    // disabled it for links, which are the main causes of text decorations),
    // so we do that.
    return nullptr;
  }
  ComputedStyleBuilder builder(*style);
  builder.PropagateIndependentInheritedProperties(*parent_style);
  INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                independent_inherited_styles_propagated, 1);
  return builder.TakeStyle();
}

static bool NeedsContainerQueryEvaluator(
    const ContainerQueryEvaluator& evaluator,
    const ComputedStyle& new_style) {
  return evaluator.DependsOnStyle() ||
         new_style.IsContainerForSizeContainerQueries() ||
         new_style.IsContainerForScrollStateContainerQueries() ||
         new_style.IsContainerForAnchoredContainerQueries();
}

static const StyleRecalcChange ApplyComputedStyleDiff(
    const StyleRecalcChange change,
    ComputedStyle::Difference diff) {
  if (change.RecalcDescendants() ||
      diff < ComputedStyle::Difference::kPseudoElementStyle) {
    return change;
  }
  if (diff == ComputedStyle::Difference::kDescendantAffecting) {
    return change.EnsureAtLeast(StyleRecalcChange::kRecalcDescendants);
  }
  if (diff == ComputedStyle::Difference::kInherited) {
    return change.EnsureAtLeast(StyleRecalcChange::kRecalcChildren);
  }
  if (diff == ComputedStyle::Difference::kIndependentInherited) {
    return change.EnsureAtLeast(StyleRecalcChange::kIndependentInherit);
  }
  DCHECK(diff == ComputedStyle::Difference::kPseudoElementStyle);
  return change.EnsureAtLeast(StyleRecalcChange::kUpdatePseudoElements);
}

static bool LayoutViewCanHaveChildren(Element& element) {
  if (LayoutObject* view = element.GetDocument().GetLayoutView()) {
    return view->CanHaveChildren();
  }
  return false;
}

void Element::NotifyAXOfAttachedSubtree() {
  if (auto* ax_cache = GetDocument().ExistingAXObjectCache()) {
    ax_cache->SubtreeIsAttached(this);
  }
}

// This function performs two important tasks:
//
//  1. It computes the correct style for the element itself.
//  2. It figures out to what degree we need to propagate changes
//     to child elements (and returns that).
//
// #1 can happen in one out of two ways. The normal way is that ask the
// style resolver to compute the style from scratch (modulo some caching).
// The other one is an optimization for “independent inherited properties”;
// if this recalc is because the parent has changed only properties marked
// as “independent” (i.e., they do not affect other properties; “visibility”
// is an example of such a property), we can reuse our existing style and just
// re-propagate those properties.
//
// #2 happens by diffing the old and new styles. In the extreme example,
// if the two are identical, we don't need to invalidate child elements
// at all. But if they are different, they will usually be different to
// differing degrees; e.g. as noted above, if only independent properties
// changed, we can inform children of that for less work down the tree.
// Our own diff gets combined with the input StyleRecalcChange to produce a
// child recalc policy that's roughly the strictest of the two.
StyleRecalcChange Element::RecalcOwnStyle(
    const StyleRecalcChange change,
    const StyleRecalcContext& style_recalc_context) {
  DCHECK(GetDocument().InStyleRecalc());

  StyleRecalcContext new_style_recalc_context = style_recalc_context;
  if (change.RecalcChildren() || change.RecalcContainerQueryDependent(*this)) {
    if (NeedsStyleRecalc()) {
      if (ElementRareDataVector* data = GetElementRareData()) {
        // This element needs recalc because its parent changed inherited
        // properties or there was some style change in the ancestry which
        // needed a full subtree recalc. In that case we cannot use the
        // BaseComputedStyle optimization.
        if (ElementAnimations* element_animations =
                data->GetElementAnimations()) {
          element_animations->SetAnimationStyleChange(false);
        }
        // We can not apply the style incrementally if we're propagating
        // inherited changes from the parent, as incremental styling would not
        // include those changes. (Incremental styling is disabled by default.)
      }
    }
  } else {
    // We are not propagating inherited changes from the parent,
    // and (if other circumstances allow it;
    // see CanApplyInlineStyleIncrementally()), incremental style
    // may be used.
    new_style_recalc_context.can_use_incremental_style = true;
  }

  const ComputedStyle* new_style = nullptr;
  const ComputedStyle* old_style = GetComputedStyle();

  StyleRecalcChange child_change = change.ForChildren(*this);

  const ComputedStyle* parent_style = ParentComputedStyle();
  if (parent_style && old_style && change.IndependentInherit(*old_style)) {
    // When propagating inherited changes, we don't need to do a full style
    // recalc if the only changed properties are independent. In this case, we
    // can simply clone the old ComputedStyle and set these directly.
    new_style = PropagateInheritedProperties();
    if (new_style) {
      // If the child style is copied from the old one, we'll never
      // reach StyleBuilder::ApplyProperty(), hence we'll
      // never set the flag on the parent. this is completely analogous
      // to the code in StyleResolver::ApplyMatchedCache().
      if (new_style->HasExplicitInheritance()) {
        parent_style->SetChildHasExplicitInheritance();
      }
    }
  }
  if (!new_style && (parent_style || (GetDocument().documentElement() == this &&
                                      LayoutViewCanHaveChildren(*this)))) {
    // This is the normal flow through the function; calculates
    // the element's style more or less from scratch (typically
    // ending up calling StyleResolver::ResolveStyle()).
    new_style = StyleForLayoutObject(new_style_recalc_context);
  }
  bool base_is_display_none =
      !new_style ||
      new_style->GetBaseComputedStyleOrThis()->Display() == EDisplay::kNone;

  // Record a display: none subtree when it's under a content-visibility: auto
  // locked subtree.
  if (base_is_display_none &&
      style_recalc_context.has_content_visibility_auto_locked_ancestor) {
    UseCounter::Count(
        GetDocument(),
        WebFeature::kDisplayNoneComputedInContentVisibilityAutoLockedSubtree);
  }

  if (new_style) {
    if (!ShouldStoreComputedStyle(*new_style)) {
      new_style = nullptr;
      NotifyAXOfAttachedSubtree();
    } else {
      if (!old_style && !new_style->IsContentVisibilityVisible()) {
        NotifyAXOfAttachedSubtree();
      }
      if (new_style->IsContainerForSizeContainerQueries()) {
        new_style_recalc_context.size_container = this;
      }
      new_style = RecalcHighlightStyles(new_style_recalc_context, old_style,
                                        *new_style, parent_style);
    }
  }

  ComputedStyle::Difference diff =
      ComputedStyle::ComputeDifference(old_style, new_style);
  if (ViewTransitionUtils::GetTransition(*this)) {
    // Even if the computed style is an exact match, we must trigger pseudo-
    // element traversal to properly populate the pseudo-element's subtree.
    diff = std::max(diff, ComputedStyle::Difference::kPseudoElementStyle);
  }

  if (old_style && old_style->IsEnsuredInDisplayNone()) {
    // Make sure we traverse children for clearing ensured computed styles
    // further down the tree.
    child_change =
        child_change.EnsureAtLeast(StyleRecalcChange::kRecalcChildren);
    // If the existing style was ensured in a display:none subtree, set it to
    // null to make sure we don't mark for re-attachment if the new style is
    // null.
    old_style = nullptr;
  }

  // If we have an overscroll container, but it's the wrong one or we shouldn't
  // have one, remove this element from the overscroll container (which should
  // also clear OverscrollContainer() on `this`).
  if (OverscrollContainer() &&
      (!new_style || !new_style->IsInternalOverscrollPositionAuto() ||
       OverscrollContainer() != style_recalc_context.overscroll_container)) {
    auto* tracker = OverscrollContainer()->OverscrollAreaTracker();
    // We should've created a tracker when we set the OverscrollContainer on
    // `this`.
    CHECK(tracker);
    tracker->RemoveOverscroll(this);
  }
  // If we no longer an overscroll container, but need one, add this element to
  // the context overscroll container.
  if (!OverscrollContainer() && new_style &&
      new_style->IsInternalOverscrollPositionAuto()) {
    // Note that we don't do anything special if there is no overscroll
    // container.
    if (style_recalc_context.overscroll_container) {
      style_recalc_context.overscroll_container->EnsureOverscrollAreaTracker()
          .AddOverscroll(this);
    }
  }

  if (!new_style) {
    if (ElementRareDataVector* data = GetElementRareData()) {
      if (ElementAnimations* element_animations =
              data->GetElementAnimations()) {
        // The animation should only be canceled when the base style is
        // display:none. If new_style is otherwise set to display:none, then it
        // means an animation set display:none, and an animation shouldn't
        // cancel itself in this case.
        if (base_is_display_none) {
          element_animations->CssAnimations().Cancel();
        }
      }
      data->SetContainerQueryEvaluator(nullptr);
      data->ClearPseudoElements();
      data->RemoveScrollMarkerGroupData();
    }
  }
  SetComputedStyle(new_style);

  if ((!old_style && new_style && new_style->GetCounterDirectives()) ||
      (old_style && new_style &&
       !old_style->CounterDirectivesEqual(*new_style)) ||
      (old_style && old_style->GetCounterDirectives() && !new_style)) {
    GetDocument().GetStyleEngine().MarkCountersDirty();
  }

  if ((!new_style && old_style && old_style->ContainsStyle()) ||
      (old_style && new_style &&
       old_style->ContainsStyle() != new_style->ContainsStyle())) {
    GetDocument().GetStyleEngine().MarkCountersDirty();
  }

  if ((!old_style || old_style->ScrollTargetGroupNone()) && new_style &&
      !new_style->ScrollTargetGroupNone()) {
    GetDocument().AddScrollTargetGroup(&EnsureScrollTargetGroupData());
  }

  if (old_style && !old_style->ScrollTargetGroupNone() && new_style &&
      new_style->ScrollTargetGroupNone()) {
    RemoveScrollTargetGroupData();
  }

  bool old_style_has_scroll_marker_group =
      old_style && !old_style->ScrollMarkerGroupNone();
  bool new_style_has_scroll_marker_group =
      new_style && !new_style->ScrollMarkerGroupNone();
  // If the scroll-marker-group and there may be content-visibility: auto locked
  // locks, then we need to recurse. If there are non-locked content-visibility:
  // auto locks we'll reach those when the lock gets acquired again.
  if (old_style_has_scroll_marker_group != new_style_has_scroll_marker_group &&
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount() >
          0 &&
      GetDocument().GetDisplayLockDocumentState().HasActivatableLocks()) {
    child_change = child_change.ForceRecalcDescendantContentVisibility();
  }

  // Update style containment tree if the style containment of the element
  // has changed.
  // Don't update if the style containment tree has not been initialized.
  if (GetDocument().GetStyleEngine().GetStyleContainmentScopeTree() &&
      ((!new_style && old_style && old_style->ContainsStyle()) ||
       (old_style && new_style &&
        old_style->ContainsStyle() != new_style->ContainsStyle()))) {
    StyleContainmentScopeTree& tree =
        GetDocument().GetStyleEngine().EnsureStyleContainmentScopeTree();
    if (old_style && old_style->ContainsStyle()) {
      tree.DestroyScopeForElement(*this);
    }
    if (new_style && new_style->ContainsStyle()) {
      tree.CreateScopeForElement(*this);
    }
  }

  ProcessContainIntrinsicSizeChanges();

  if (!child_change.ReattachLayoutTree() &&
      (GetForceReattachLayoutTree() || NeedsReattachLayoutTree() ||
       ComputedStyle::NeedsReattachLayoutTree(*this, old_style, new_style))) {
    if (style_recalc_context.anchor_evaluator == nullptr) {
      child_change = child_change.ForceReattachLayoutTree();
    } else {
      // position-try-fallbacks should not have style changes that causes layout
      // tree changes. If they do, it is probably the ComputedStyle diff in
      // NeedsReattachLayoutTree() that incorrectly detects a diff without an
      // actual computed value change. If we ForceReattachLayoutTree() on the
      // child_change here, we could end up accessing a destroyed LayoutObject
      // in layout.
      //
      // Ideally, this should have been a NOTREACHED(), but if we have
      // StyleImage with ErrorOccurred(), or if the resource is not allowed to
      // be cached, the diffing of the content property will return true for
      // NeedsReattachLayoutTree() even if the computed value is the same.
      CHECK(!GetForceReattachLayoutTree());
      CHECK(!NeedsReattachLayoutTree());
    }
  }

  if (diff == ComputedStyle::Difference::kEqual) {
    INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                  styles_unchanged, 1);
    if (!new_style) {
      DCHECK(!old_style);
      return {};
    }
  } else {
    INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                  styles_changed, 1);
    probe::DidUpdateComputedStyle(this, old_style, new_style);
    if (this == GetDocument().documentElement()) {
      if (GetDocument().GetStyleEngine().UpdateRootFontRelativeUnits(
              old_style, new_style)) {
        // Trigger a full document recalc on root font units changes. We could
        // keep track of which elements depend on root font units like we do for
        // viewport styles, but we assume root font size changes are rare and
        // just recalculate everything.
        child_change =
            child_change.EnsureAtLeast(StyleRecalcChange::kRecalcDescendants);
      }

      if (new_style &&
          GetDocument().GetLayoutView()->SetScrollbarSizesForViewportUnits(
              new_style->UnconditionalScrollbarSize())) {
        GetDocument().GetStyleEngine().UpdateViewportSize();
        GetDocument()
            .GetStyleResolver()
            .InvalidateMatchedPropertiesCacheForViewportUnits();
        child_change =
            child_change.EnsureAtLeast(StyleRecalcChange::kRecalcDescendants);
      }
    }
    child_change = ApplyComputedStyleDiff(child_change, diff);
    UpdateCallbackSelectors(old_style, new_style);
    NotifyIfMatchedDocumentRulesSelectorsChanged(old_style, new_style);
  }

  // We do not allow locked content to resume recalc when computing styles for
  // anchored fallback positions during layout.
  //
  // An anchored element with content-visibility:hidden may have stored a
  // StyleRecalcChange for a blocked style update that forces a layout tree
  // re-attachment. Re-attaching the box of an element being laid out would
  // cause a crash. The stored StyleRecalcChange for the blocked style update
  // would still be stored in the DisplayLockContext and considered for the next
  // normal style recalc pass.
  if (style_recalc_context.anchor_evaluator == nullptr) {
    if (DisplayLockContext* context = GetDisplayLockContext()) {
      // Combine the change from the display lock context. If the context is
      // locked and is preventing child update, we'll store this style recalc
      // change again from Element::RecalcStyle.
      child_change =
          child_change.Combine(context->TakeBlockedStyleRecalcChange());
    }
  }

  if (new_style) {
    if (old_style && !child_change.RecalcChildren() &&
        old_style->HasChildDependentFlags()) {
      new_style->CopyChildDependentFlagsFrom(*old_style);
    }
    if (ContainerQueryEvaluator* evaluator = GetContainerQueryEvaluator()) {
      if (!NeedsContainerQueryEvaluator(*evaluator, *new_style)) {
        EnsureElementRareData()
            .EnsureContainerQueryData()
            .SetContainerQueryEvaluator(nullptr);
      } else if (old_style) {
        if (style_recalc_context.anchor_evaluator == nullptr) {
          // position-try-fallbacks are only applied for
          // UpdateStyleAndLayoutTreeForOutOfFlow during layout, so we need to
          // reset the anchored(fallback) state to no fallback for normal style
          // recalc.
          child_change = evaluator->ApplyAnchoredChanges(
              child_change, PositionTryFallback(),
              WritingDirectionMode(WritingMode::kHorizontalTb,
                                   TextDirection::kLtr));
        }
        child_change = evaluator->ApplyScrollStateAndStyleChanges(
            child_change, *old_style, *new_style,
            diff != ComputedStyle::Difference::kEqual);
      }
    }
    if (IsPseudoElement() && new_style->MayUseImplicitAnchor()) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kCSSPseudoElementUsesImplicitAnchor);
      if (RuntimeEnabledFeatures::OriginatingElementIsImplicitAnchorEnabled()) {
        parentElement()->SetMayBeImplicitAnchor();
      }
    }
  }

  if (child_change.ReattachLayoutTree()) {
    if (new_style || old_style) {
      SetNeedsReattachLayoutTree();
    }
    return child_change;
  }

  DCHECK(!NeedsReattachLayoutTree())
      << "If we need to reattach the layout tree we should have returned "
         "above. Updating and diffing the style of a LayoutObject which is "
         "about to be deleted is a waste.";

  if (LayoutObject* layout_object = GetLayoutObject()) {
    DCHECK(new_style);
    if (layout_object->IsText() &&
        IsA<LayoutTextCombine>(layout_object->Parent())) [[unlikely]] {
      // Adjust style for <br> and <wbr> in combined text.
      // See http://crbug.com/1228058
      ComputedStyleBuilder adjust_builder(*new_style);
      StyleAdjuster::AdjustStyleForCombinedText(adjust_builder);
      new_style = adjust_builder.TakeStyle();
    }
    // kEqual means that the computed style didn't change, but there are
    // additional flags in ComputedStyle which may have changed. For instance,
    // the AffectedBy* flags. We don't need to go through the visual
    // invalidation diffing in that case, but we replace the old ComputedStyle
    // object with the new one to ensure the mentioned flags are up to date.
    LayoutObject::ApplyStyleChanges apply_changes =
        diff == ComputedStyle::Difference::kEqual
            ? LayoutObject::ApplyStyleChanges::kNo
            : LayoutObject::ApplyStyleChanges::kYes;

    if (diff != ComputedStyle::Difference::kEqual && GetOutOfFlowData() &&
        (!new_style->HasOutOfFlowPosition() ||
         !base::ValuesEquivalent(old_style->GetPositionTryFallbacks().Get(),
                                 new_style->GetPositionTryFallbacks().Get()))) {
      // position-try-fallbacks or positioning changed, which both invalidate
      // last successful try option.
      GetDocument()
          .GetStyleEngine()
          .MarkLastSuccessfulPositionFallbackDirtyForElement(*this);
    }

    const ComputedStyle* layout_style = new_style;
    if (auto* pseudo_element = DynamicTo<PseudoElement>(this)) {
      if (const ComputedStyle* adjusted_style =
              pseudo_element->AdjustedLayoutStyle(
                  *layout_style, layout_object->Parent()->StyleRef())) {
        layout_style = adjusted_style;
      }
    } else if (auto* html_element = DynamicTo<HTMLHtmlElement>(this)) {
      if (this == GetDocument().documentElement()) {
        layout_style = html_element->LayoutStyleForElement(layout_style);
        // Always apply changes for html root, even if the ComputedStyle may be
        // the same, propagation changes picked up from body style, or
        // previously propagated styles from a removed body element, may still
        // change the LayoutObject's style.
        apply_changes = LayoutObject::ApplyStyleChanges::kYes;
      }
    }
    if (style_recalc_context.anchor_evaluator) {
      // If we're in an interleaved style recalc from out-of-flow,
      // and we're already in the middle of laying out the anchored element,
      // we should not mark that element for layout again. Note that the
      // anchor_evaluator is reset for children so that that elements
      // recalculated for anchored() queries will be invalidates as normal.
      apply_changes = LayoutObject::ApplyStyleChanges::kNo;
    } else if (new_style->HasAnchorFunctionsWithoutEvaluator() ||
               (IsFirstLetterPseudoElement() &&
                !layout_style->InitialLetter().IsNormal())) {
      // For regular (non-interleaved) recalcs that depend on anchor*()
      // functions, we need to invalidate layout even without a diff,
      // see ComputedStyle::HasAnchorFunctionsWithoutEvaluator.
      //
      // For ::first-letter pseudo elements with initial-letter, we may need to
      // compute new font styling for the initial letter text box if a new font
      // was available.
      apply_changes = LayoutObject::ApplyStyleChanges::kYes;
    }
    layout_object->SetStyle(layout_style, apply_changes);
  }

  return child_change;
}

void Element::ProcessContainIntrinsicSizeChanges() {
  // It is important that we early out, since ShouldUpdateLastRemembered*Size
  // functions only return meaningful results if we have computed style. If we
  // don't have style, we also avoid clearing the last remembered sizes.
  if (!GetComputedStyle()) {
    GetDocument().UnobserveForIntrinsicSize(this);
    return;
  }

  DisplayLockContext* context = GetDisplayLockContext();
  // The only case where we _don't_ record new sizes is if we're skipping
  // contents.
  bool allowed_to_record_new_intrinsic_sizes = !context || !context->IsLocked();

  // We should only record new sizes if we will update either the block or
  // inline direction. IOW, if we have contain-intrinsic-size: auto on at least
  // one of the directions.
  bool should_record_new_intrinsic_sizes = false;
  if (ShouldUpdateLastRememberedBlockSize()) {
    should_record_new_intrinsic_sizes = true;
  } else {
    SetLastRememberedBlockSize(std::nullopt);
  }

  if (ShouldUpdateLastRememberedInlineSize()) {
    should_record_new_intrinsic_sizes = true;
  } else {
    SetLastRememberedInlineSize(std::nullopt);
  }

  if (allowed_to_record_new_intrinsic_sizes &&
      should_record_new_intrinsic_sizes) {
    GetDocument().ObserveForIntrinsicSize(this);
  } else {
    GetDocument().UnobserveForIntrinsicSize(this);
  }
}

void Element::RebuildLayoutTree(WhitespaceAttacher& whitespace_attacher) {
  DCHECK(InActiveDocument());
  DCHECK(parentNode());

  if (NeedsReattachLayoutTree()) {
    AttachContext reattach_context;
    if (IsDocumentElement()) {
      reattach_context.counters_context.SetAttachmentRootIsDocumentElement();
    }
    reattach_context.parent =
        LayoutTreeBuilderTraversal::ParentLayoutObject(*this);
    ReattachLayoutTree(reattach_context);
    if (IsDocumentElement()) {
      GetDocument().GetStyleEngine().MarkCountersClean();
    }
    whitespace_attacher.DidReattachElement(this,
                                           reattach_context.previous_in_flow);
  } else if (NeedsRebuildChildLayoutTrees(whitespace_attacher) &&
             !ChildStyleRecalcBlockedByDisplayLock() &&
             !SkippedContainerStyleRecalc()) {
    // TODO(crbug.com/972752): Make the condition above a DCHECK instead when
    // style recalc and dirty bit propagation uses flat-tree traversal.
    // We create a local WhitespaceAttacher when rebuilding children of an
    // element with a LayoutObject since whitespace nodes do not rely on layout
    // objects further up the tree. Also, if this Element's layout object is an
    // out-of-flow box, in-flow children should not affect whitespace siblings
    // of the out-of-flow box. However, if this element is a display:contents
    // element. Continue using the passed in attacher as display:contents
    // children may affect whitespace nodes further up the tree as they may be
    // layout tree siblings.
    WhitespaceAttacher local_attacher;
    WhitespaceAttacher* child_attacher;
    RebuildPseudoElementLayoutTree(kPseudoIdScrollMarkerGroupAfter,
                                   local_attacher);
    LayoutObject* layout_object = GetLayoutObject();
    if (layout_object || !HasDisplayContentsStyle()) {
      whitespace_attacher.DidVisitElement(this);
      if (layout_object && layout_object->WhitespaceChildrenMayChange()) {
        layout_object->SetWhitespaceChildrenMayChange(false);
        local_attacher.SetReattachAllWhitespaceNodes();
      }
      child_attacher = &local_attacher;
    } else {
      child_attacher = &whitespace_attacher;
    }
    RebuildTransitionLayoutTree(*child_attacher);
    RebuildPseudoElementLayoutTree(kPseudoIdAfter, *child_attacher);
    RebuildPseudoElementLayoutTree(kPseudoIdPickerIcon, *child_attacher);
    RebuildPseudoElementLayoutTree(kPseudoIdInterestHint, *child_attacher);
    if (GetShadowRoot()) {
      RebuildShadowRootLayoutTree(*child_attacher);
    } else {
      RebuildChildrenLayoutTrees(*child_attacher);
    }
    RebuildPseudoElementLayoutTree(kPseudoIdCheckMark, *child_attacher);
    RebuildPseudoElementLayoutTree(kPseudoIdBefore, *child_attacher);
    RebuildPseudoElementLayoutTree(kPseudoIdMarker, *child_attacher);
    RebuildPseudoElementLayoutTree(kPseudoIdScrollButtonBlockEnd,
                                   local_attacher);
    RebuildPseudoElementLayoutTree(kPseudoIdScrollButtonInlineEnd,
                                   local_attacher);
    RebuildPseudoElementLayoutTree(kPseudoIdScrollButtonInlineStart,
                                   local_attacher);
    RebuildPseudoElementLayoutTree(kPseudoIdScrollButtonBlockStart,
                                   local_attacher);
    RebuildPseudoElementLayoutTree(kPseudoIdScrollMarkerGroupBefore,
                                   local_attacher);
    RebuildPseudoElementLayoutTree(kPseudoIdBackdrop, *child_attacher);
    RebuildFirstLetterLayoutTree();
    RebuildPseudoElementLayoutTree(kPseudoIdScrollMarker, *child_attacher);
    RebuildColumnLayoutTrees(*child_attacher);
    ClearChildNeedsReattachLayoutTree();
  }
  DCHECK(!NeedsStyleRecalc());
  DCHECK(!ChildNeedsStyleRecalc() || ChildStyleRecalcBlockedByDisplayLock());
  DCHECK(!NeedsReattachLayoutTree());
  DCHECK(!ChildNeedsReattachLayoutTree() ||
         ChildStyleRecalcBlockedByDisplayLock());
  HandleSubtreeModifications();
}

void Element::RebuildShadowRootLayoutTree(
    WhitespaceAttacher& whitespace_attacher) {
  DCHECK(IsShadowHost(this));
  ShadowRoot* root = GetShadowRoot();
  root->RebuildLayoutTree(whitespace_attacher);
}

void Element::RebuildPseudoElementLayoutTree(
    PseudoId pseudo_id,
    WhitespaceAttacher& whitespace_attacher) {
  if (PseudoElement* element = GetPseudoElement(pseudo_id)) {
    RebuildLayoutTreeForChild(element, whitespace_attacher);
  }
}

void Element::RebuildColumnLayoutTrees(
    WhitespaceAttacher& whitespace_attacher) {
  if (const ColumnPseudoElementsVector* columns = GetColumnPseudoElements()) {
    for (ColumnPseudoElement* column : *columns) {
      RebuildLayoutTreeForChild(column, whitespace_attacher);
    }
  }
}

void Element::RebuildFirstLetterLayoutTree() {
  // Need to create a ::first-letter element here for the following case:
  //
  // <style>#outer::first-letter {...}</style>
  // <div id=outer><div id=inner style="display:none">Text</div></div>
  // <script> outer.offsetTop; inner.style.display = "block" </script>
  //
  // The creation of FirstLetterPseudoElement relies on the layout tree of the
  // block contents. In this case, the ::first-letter element is not created
  // initially since the #inner div is not displayed. On RecalcStyle it's not
  // created since the layout tree is still not built, and AttachLayoutTree
  // for #inner will not update the ::first-letter of outer. However, we end
  // up here for #outer after AttachLayoutTree is called on #inner at which
  // point the layout sub-tree is available for deciding on creating the
  // ::first-letter.
  StyleEngine::AllowMarkForReattachFromRebuildLayoutTreeScope scope(
      GetDocument().GetStyleEngine());

  UpdateFirstLetterPseudoElement(StyleUpdatePhase::kRebuildLayoutTree);
  if (PseudoElement* element = GetPseudoElement(kPseudoIdFirstLetter)) {
    WhitespaceAttacher whitespace_attacher;
    if (element->NeedsRebuildLayoutTree(whitespace_attacher)) {
      element->RebuildLayoutTree(whitespace_attacher);
    }
  }
}

void Element::RebuildTransitionLayoutTree(
    WhitespaceAttacher& whitespace_attacher) {
  auto rebuild_pseudo_tree =
      [&whitespace_attacher](PseudoElement* pseudo_element) {
        pseudo_element->RebuildLayoutTree(whitespace_attacher);
      };
  ViewTransitionUtils::ForEachTransitionPseudo(
      *this, rebuild_pseudo_tree, ViewTransitionUtils::Filter::kDirectChildren);
}

void Element::AttachOverscrollPseudoElements(AttachContext& context) {
  const OverscrollAreaParentPseudoElementsVector* overscroll_area_parents =
      GetOverscrollAreaParentPseudoElements();
  if (!overscroll_area_parents) {
    return;
  }

  for (IndexedPseudoElement* pseudo_element : *overscroll_area_parents) {
    pseudo_element->AttachLayoutTree(context);
    CHECK(pseudo_element->GetLayoutObject());
  }
}

void Element::AttachTransitionPseudoElements(AttachContext& context) {
  // For a document transition, the LayoutObject for the ::view-transition
  // pseudo-element is wrapped by the anonymous LayoutViewTransitionRoot,
  // which represents the snapshot containing block.
  //
  // The LayoutViewTransitionRoot is a child of the LayoutView, and will be
  // injected by LayoutView::AddChild.
  // See LayoutTreeBuilderTraversal::ParentLayoutObject.
  AttachContext children_context(context);
  if (context.parent && context.parent->IsDocumentElement()) {
    children_context.parent = GetDocument().GetLayoutView();
  }

  auto attach_pseudo = [&](PseudoElement* pseudo_element) {
    pseudo_element->AttachLayoutTree(children_context);
  };
  ViewTransitionUtils::ForEachTransitionPseudo(
      *this, attach_pseudo, ViewTransitionUtils::Filter::kDirectChildren);
}

void Element::DetachTransitionPseudoElements(bool performing_reattach) {
  auto detach_pseudo = [&](PseudoElement* pseudo_element) {
    pseudo_element->DetachLayoutTree(performing_reattach);
  };
  ViewTransitionUtils::ForEachTransitionPseudo(
      *this, detach_pseudo, ViewTransitionUtils::Filter::kDirectChildren);
}

void Element::HandleSubtreeModifications() {
  if (auto* layout_object = GetLayoutObject()) {
    layout_object->HandleSubtreeModifications();
  }
}

void Element::UpdateCallbackSelectors(const ComputedStyle* old_style,
                                      const ComputedStyle* new_style) {
  Vector<String> empty_vector;
  const Vector<String>& old_callback_selectors =
      old_style ? old_style->CallbackSelectors() : empty_vector;
  const Vector<String>& new_callback_selectors =
      new_style ? new_style->CallbackSelectors() : empty_vector;
  if (old_callback_selectors.empty() && new_callback_selectors.empty()) {
    return;
  }
  if (old_callback_selectors != new_callback_selectors) {
    CSSSelectorWatch::From(GetDocument())
        .UpdateSelectorMatches(old_callback_selectors, new_callback_selectors);
  }
}

void Element::NotifyIfMatchedDocumentRulesSelectorsChanged(
    const ComputedStyle* old_style,
    const ComputedStyle* new_style) {
  if (!IsLink() ||
      !(HasTagName(html_names::kATag) || HasTagName(html_names::kAreaTag))) {
    return;
  }

  HTMLAnchorElementBase* link = To<HTMLAnchorElementBase>(this);
  auto* document_rules = DocumentSpeculationRules::FromIfExists(GetDocument());
  if (!document_rules) {
    return;
  }

  if (ComputedStyle::IsNullOrEnsured(old_style) !=
      ComputedStyle::IsNullOrEnsured(new_style)) {
    document_rules->LinkGainedOrLostComputedStyle(link);
    return;
  }

  auto get_selectors_from_computed_style = [](const ComputedStyle* style)
      -> const GCedHeapHashSet<WeakMember<StyleRule>>* {
    if (!style || !style->DocumentRulesSelectors()) {
      return nullptr;
    }
    return style->DocumentRulesSelectors().Get();
  };

  const GCedHeapHashSet<WeakMember<StyleRule>>* old_document_rules_selectors =
      get_selectors_from_computed_style(old_style);
  const GCedHeapHashSet<WeakMember<StyleRule>>* new_document_rules_selectors =
      get_selectors_from_computed_style(new_style);
  if ((!old_document_rules_selectors ||
       old_document_rules_selectors->empty()) &&
      (!new_document_rules_selectors ||
       new_document_rules_selectors->empty())) {
    return;
  }

  if (!old_document_rules_selectors || !new_document_rules_selectors ||
      (*old_document_rules_selectors != *new_document_rules_selectors)) {
    document_rules->LinkMatchedSelectorsUpdated(link);
  }
}

TextDirection Element::ParentDirectionality() const {
  Node* parent = parentNode();
  if (Element* parent_element = DynamicTo<Element>(parent)) {
    return parent_element->CachedDirectionality();
  }

  if (ShadowRoot* shadow_root = DynamicTo<ShadowRoot>(parent)) {
    return shadow_root->host().CachedDirectionality();
  }

  return TextDirection::kLtr;
}

void Element::RecomputeDirectionFromParent() {
  // This function recomputes the inherited direction if an element inherits
  // direction from a parent or shadow host.
  //
  // It should match the computation done in
  // Element::UpdateDirectionalityAndDescendant that applies an inherited
  // direction change to the descendants that need updating.
  if (GetDocument().HasDirAttribute() &&
      HTMLElement::ElementInheritsDirectionality(this)) {
    SetCachedDirectionality(ParentDirectionality());
  }
}

void Element::UpdateDirectionalityAndDescendant(TextDirection direction) {
  // This code applies a direction change to an element and to any elements
  // that inherit from it.  It should match the code in
  // Element::RecomputeDirectionFromParent that determines whether a single
  // element should inherit direction and recomputes it if it does.
  Element* element = this;
  do {
    if (element != this &&
        (!HTMLElement::ElementInheritsDirectionality(element) ||
         element->CachedDirectionality() == direction)) {
      element = ElementTraversal::NextSkippingChildren(*element, this);
      continue;
    }

    element->SetCachedDirectionality(direction);
    element->PseudoStateChanged(CSSSelector::kPseudoDir);

    if (ShadowRoot* shadow_root = element->GetShadowRoot()) {
      for (Node& child : ElementTraversal::ChildrenOf(*shadow_root)) {
        if (Element* child_element = DynamicTo<Element>(child)) {
          if (HTMLElement::ElementInheritsDirectionality(child_element) &&
              child_element->CachedDirectionality() != direction) {
            child_element->UpdateDirectionalityAndDescendant(direction);
          }
        }
      }

      // The directionality of a shadow host also affects the effect of
      // its slots on the auto directionality of an ancestor.
      if (shadow_root->HasSlotAssignment()) {
        for (HTMLSlotElement* slot : shadow_root->GetSlotAssignment().Slots()) {
          Element* slot_parent = slot->parentElement();
          if (slot_parent && slot_parent->SelfOrAncestorHasDirAutoAttribute() &&
              slot_parent->CachedDirectionality() != direction) {
            slot_parent->UpdateAncestorWithDirAuto(
                UpdateAncestorTraversal::IncludeSelf);
          }
        }
      }
    }
    element = ElementTraversal::Next(*element, this);
  } while (element);
}

// Because the self-or-ancestor has dir=auto state could come from either a
// node tree ancestor, a slot, or an input, we have a method to
// recalculate it (just for this element) based on all three sources.
bool Element::RecalcSelfOrAncestorHasDirAuto() {
  if (IsHTMLElement()) {
    AtomicString dir_attribute_value = FastGetAttribute(html_names::kDirAttr);
    if (HTMLElement::IsValidDirAttribute(dir_attribute_value)) {
      return EqualIgnoringASCIICase(dir_attribute_value, "auto");
    }
  }
  Node* parent = parentNode();
  if (parent && parent->SelfOrAncestorHasDirAutoAttribute()) {
    return true;
  }
  if (HTMLSlotElement* slot = AssignedSlot()) {
    if (slot->HasDirectionAuto()) {
      return true;
    }
  }
  if (ShadowRoot* shadow_root = DynamicTo<ShadowRoot>(parent)) {
    if (TextControlElement* text_element =
            HTMLElement::ElementIfAutoDirectionalityFormAssociatedOrNull(
                &shadow_root->host())) {
      if (text_element->HasDirectionAuto()) {
        return true;
      }
    }
  }
  return false;
}

void Element::UpdateDescendantHasDirAutoAttribute(bool has_dir_auto) {
  if (ToHTMLSlotElementIfSupportsAssignmentOrNull(this) ||
      HTMLElement::ElementIfAutoDirectionalityFormAssociatedOrNull(this)) {
    for (Node& node : FlatTreeTraversal::ChildrenOf(*this)) {
      if (Element* element = DynamicTo<Element>(node)) {
        if (!element->IsHTMLElement() ||
            !HTMLElement::IsValidDirAttribute(
                element->FastGetAttribute(html_names::kDirAttr))) {
          if (!has_dir_auto) {
            if (!element->SelfOrAncestorHasDirAutoAttribute() ||
                element->RecalcSelfOrAncestorHasDirAuto()) {
              continue;
            }
            element->ClearSelfOrAncestorHasDirAutoAttribute();
          } else {
            if (element->SelfOrAncestorHasDirAutoAttribute()) {
              continue;
            }
            element->SetSelfOrAncestorHasDirAutoAttribute();
          }
          element->UpdateDescendantHasDirAutoAttribute(has_dir_auto);
        }
      }
    }
  } else {
    Element* element = ElementTraversal::FirstChild(*this);
    while (element) {
      if (element->IsHTMLElement()) {
        AtomicString dir_attribute_value =
            element->FastGetAttribute(html_names::kDirAttr);
        if (HTMLElement::IsValidDirAttribute(dir_attribute_value)) {
          element = ElementTraversal::NextSkippingChildren(*element, this);
          continue;
        }
      }

      if (!has_dir_auto) {
        if (!element->SelfOrAncestorHasDirAutoAttribute() ||
            element->RecalcSelfOrAncestorHasDirAuto()) {
          element = ElementTraversal::NextSkippingChildren(*element, this);
          continue;
        }
        element->ClearSelfOrAncestorHasDirAutoAttribute();
      } else {
        if (element->SelfOrAncestorHasDirAutoAttribute()) {
          element = ElementTraversal::NextSkippingChildren(*element, this);
          continue;
        }
        element->SetSelfOrAncestorHasDirAutoAttribute();
      }
      element = ElementTraversal::Next(*element, this);
    }
  }
}

std::optional<TextDirection> Element::ResolveAutoDirectionality() const {
  if (const TextControlElement* text_element =
          HTMLElement::ElementIfAutoDirectionalityFormAssociatedOrNull(this)) {
    return BidiParagraph::BaseDirectionForStringOrLtr(text_element->Value());
  }

  auto include_in_traversal = [](Element* element) -> bool {
    // Skip bdi, script, style and textarea.
    if (element->HasTagName(html_names::kBdiTag) ||
        element->HasTagName(html_names::kScriptTag) ||
        element->HasTagName(html_names::kStyleTag) ||
        element->HasTagName(html_names::kTextareaTag) ||
        element->ShadowPseudoId() ==
            shadow_element_names::kPseudoInputPlaceholder) {
      return false;
    }

    // Skip elements with valid dir attribute
    if (element->IsHTMLElement()) {
      AtomicString dir_attribute_value =
          element->FastGetAttribute(html_names::kDirAttr);
      if (HTMLElement::IsValidDirAttribute(dir_attribute_value)) {
        return false;
      }
    }
    return true;
  };

  // https://html.spec.whatwg.org/multipage/dom.html#contained-text-auto-directionality
  auto contained_text_auto_directionality =
      [&include_in_traversal](
          const Element* subtree_root) -> std::optional<TextDirection> {
    Node* node = NodeTraversal::FirstChild(*subtree_root);
    while (node) {
      if (auto* element = DynamicTo<Element>(node)) {
        if (!include_in_traversal(element)) {
          node = NodeTraversal::NextSkippingChildren(*node, subtree_root);
          continue;
        }
      }

      if (auto* slot = ToHTMLSlotElementIfSupportsAssignmentOrNull(node)) {
        if (ShadowRoot* root = slot->ContainingShadowRoot()) {
          return root->host().CachedDirectionality();
        }
      }

      if (node->IsTextNode()) {
        if (const std::optional<TextDirection> text_direction =
                BidiParagraph::BaseDirectionForString(
                    node->textContent(true))) {
          return *text_direction;
        }
      }

      node = NodeTraversal::Next(*node, subtree_root);
    }
    return std::nullopt;
  };

  // Note that the one caller of this method is overridden by HTMLSlotElement
  // in order to defer doing this until it is safe to do so.
  if (const HTMLSlotElement* slot_this =
          ToHTMLSlotElementIfSupportsAssignmentOrNull(this)) {
    auto& assigned_nodes = slot_this->AssignedNodes();
    // Use the assigned nodes if there are any.  Otherwise, the <slot>
    // represents its content and we should fall back to the regular codepath.
    if (!assigned_nodes.empty()) {
      for (Node* slotted_node : assigned_nodes) {
        if (slotted_node->IsTextNode()) {
          if (const std::optional<TextDirection> text_direction =
                  BidiParagraph::BaseDirectionForString(
                      slotted_node->textContent(true))) {
            return *text_direction;
          }
        } else if (Element* slotted_element =
                       DynamicTo<Element>(slotted_node)) {
          if (include_in_traversal(slotted_element)) {
            std::optional<TextDirection> slotted_child_result =
                contained_text_auto_directionality(slotted_element);
            if (slotted_child_result) {
              return slotted_child_result;
            }
          }
        }
      }
      return std::nullopt;
    }
  }

  return contained_text_auto_directionality(this);
}

void Element::AdjustDirectionalityIfNeededAfterChildrenChanged(
    const ChildrenChange& change) {
  if (!SelfOrAncestorHasDirAutoAttribute()) {
    return;
  }

  if (change.type == ChildrenChangeType::kTextChanged) {
    CHECK(change.old_text);
    std::optional<TextDirection> old_text_direction =
        BidiParagraph::BaseDirectionForString(*change.old_text);
    auto* character_data = DynamicTo<CharacterData>(change.sibling_changed);
    DCHECK(character_data);
    std::optional<TextDirection> new_text_direction =
        BidiParagraph::BaseDirectionForString(character_data->data());
    if (old_text_direction == new_text_direction) {
      return;
    }
  } else if (change.IsChildInsertion()) {
    if (!ShouldAdjustDirectionalityForInsert(change)) {
      return;
    }
  }

  UpdateDescendantHasDirAutoAttribute(true /* has_dir_auto */);
  this->UpdateAncestorWithDirAuto(UpdateAncestorTraversal::IncludeSelf);
}

bool Element::ShouldAdjustDirectionalityForInsert(
    const ChildrenChange& change) const {
  if (change.type ==
      ChildrenChangeType::kFinishedBuildingDocumentFragmentTree) {
    for (Node& child : NodeTraversal::ChildrenOf(*this)) {
      if (!DoesChildTextNodesDirectionMatchThis(child)) {
        return true;
      }
    }
    return false;
  }
  return !DoesChildTextNodesDirectionMatchThis(*change.sibling_changed);
}

bool Element::DoesChildTextNodesDirectionMatchThis(const Node& node) const {
  if (node.IsTextNode()) {
    const std::optional<TextDirection> new_text_direction =
        BidiParagraph::BaseDirectionForString(node.textContent(true));
    if (!new_text_direction || *new_text_direction == CachedDirectionality()) {
      return true;
    }
  }
  return false;
}

void Element::UpdateAncestorWithDirAuto(UpdateAncestorTraversal traversal) {
  bool skip = traversal == UpdateAncestorTraversal::ExcludeSelf;

  for (Element* element_to_adjust = this; element_to_adjust;
       element_to_adjust = element_to_adjust->parentElement()) {
    if (!skip) {
      if (HTMLElement::ElementAffectsDirectionality(element_to_adjust)) {
        HTMLElement* html_element_to_adjust =
            To<HTMLElement>(element_to_adjust);
        if (html_element_to_adjust->HasDirectionAuto() &&
            html_element_to_adjust->CalculateAndAdjustAutoDirectionality()) {
          SetNeedsStyleRecalc(kLocalStyleChange,
                              StyleChangeReasonForTracing::Create(
                                  style_change_reason::kPseudoClass));
          element_to_adjust->PseudoStateChanged(CSSSelector::kPseudoDir);
        }
        return;
      }
      if (!element_to_adjust->SelfOrAncestorHasDirAutoAttribute()) {
        return;
      }
    }
    skip = false;
    // Directionality mostly operates on the node tree rather than the
    // flat tree.  However, a <slot>'s dir=auto is affected by its
    // assigned nodes.
    if (HTMLSlotElement* slot = element_to_adjust->AssignedSlot()) {
      if (slot->HasDirectionAuto() &&
          slot->CalculateAndAdjustAutoDirectionality()) {
        SetNeedsStyleRecalc(kLocalStyleChange,
                            StyleChangeReasonForTracing::Create(
                                style_change_reason::kPseudoClass));
        slot->PseudoStateChanged(CSSSelector::kPseudoDir);
      }
    }
    // And the values of many text form controls influence dir=auto on
    // the control.
    if (ShadowRoot* shadow_root =
            DynamicTo<ShadowRoot>(element_to_adjust->parentNode())) {
      if (TextControlElement* text_control =
              HTMLElement::ElementIfAutoDirectionalityFormAssociatedOrNull(
                  &shadow_root->host())) {
        if (text_control->HasDirectionAuto() &&
            text_control->CalculateAndAdjustAutoDirectionality()) {
          SetNeedsStyleRecalc(kLocalStyleChange,
                              StyleChangeReasonForTracing::Create(
                                  style_change_reason::kPseudoClass));
          text_control->PseudoStateChanged(CSSSelector::kPseudoDir);
        }
      }
    }
  }
}

ShadowRoot& Element::CreateAndAttachShadowRoot(ShadowRootMode type,
                                               SlotAssignmentMode mode) {
#if DCHECK_IS_ON()
  NestingLevelIncrementer slot_assignment_recalc_forbidden_scope(
      GetDocument().SlotAssignmentRecalcForbiddenRecursionDepth());
#endif
  HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
  EventDispatchForbiddenScope assert_no_event_dispatch;
  ScriptForbiddenScope forbid_script;

  DCHECK(!GetShadowRoot());

  auto* shadow_root =
      MakeGarbageCollected<ShadowRoot>(GetDocument(), type, mode);

  if (InActiveDocument()) {
    // We need to call child.RemovedFromFlatTree() before setting a shadow
    // root to the element because detach must use the original flat tree
    // structure before attachShadow happens. We cannot use
    // ParentSlotChanged() because we don't know at this point whether a
    // slot will be added and the child assigned to a slot on the next slot
    // assignment update.
    for (Node& child : NodeTraversal::ChildrenOf(*this)) {
      child.RemovedFromFlatTree();
    }
  }
  EnsureElementRareData().SetShadowRoot(*shadow_root);
  SetHasShadowRoot();
  shadow_root->SetShadowHostNode(this);
  shadow_root->SetParentTreeScope(GetTreeScope());
  shadow_root->InsertedInto(*this);

  probe::DidPushShadowRoot(this, shadow_root);

  return *shadow_root;
}

// TODO(crbug.com/465839474): LTO-inline this function to unify the
// fast and slow paths. (It cannot be easily inlined by putting it
// into element.h, due to the dependency on ElementRareDataVector.)
ShadowRoot* Element::GetShadowRootInternal() const {
  return GetElementRareData()->GetShadowRoot();
}

EditContext* Element::editContext() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetEditContext();
  }
  return nullptr;
}

void Element::setEditContext(EditContext* edit_context,
                             ExceptionState& exception_state) {
  CHECK(DynamicTo<HTMLElement>(this));

  // https://w3c.github.io/edit-context/#extensions-to-the-htmlelement-interface
  // 1. If this's local name is neither a valid shadow host name nor "canvas",
  // then throw a "NotSupportedError" DOMException.
  const AtomicString& local_name = localName();
  if (!(IsCustomElement() && CustomElement::IsValidName(local_name)) &&
      !IsValidShadowHostName(local_name) &&
      local_name != html_names::kCanvasTag) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "This element does not support EditContext");
    return;
  }

  if (edit_context && edit_context->attachedElements().size() > 0 &&
      edit_context->attachedElements()[0] != this) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "An EditContext can be only be associated with a single element");
    return;
  }

  // If an element is in focus when being attached to a new EditContext,
  // its old EditContext, if it has any, will get blurred,
  // and the new EditContext will automatically get focused.
  if (auto* old_edit_context = editContext()) {
    if (IsFocusedElementInDocument()) {
      old_edit_context->Blur();
    }

    old_edit_context->DetachElement(DynamicTo<HTMLElement>(this));
  }

  if (edit_context) {
    edit_context->AttachElement(DynamicTo<HTMLElement>(this));

    if (IsFocusedElementInDocument()) {
      edit_context->Focus();
    }
  }

  EnsureElementRareData().SetEditContext(edit_context);

  // EditContext affects the -webkit-user-modify CSS property of the element
  // (which is what Chromium uses internally to determine editability) so
  // we need to recalc styles. This is an inherited property, so we invalidate
  // the subtree rather than just the node itself.
  SetNeedsStyleRecalc(
      StyleChangeType::kSubtreeStyleChange,
      StyleChangeReasonForTracing::Create(style_change_reason::kEditContext));
}

struct Element::AffectedByPseudoStateChange {
  bool children_or_siblings{true};
  bool ancestors_or_siblings{true};

  AffectedByPseudoStateChange(CSSSelector::PseudoType pseudo_type,
                              Element& element) {
    switch (pseudo_type) {
      case CSSSelector::kPseudoFocus:
        children_or_siblings = element.ChildrenOrSiblingsAffectedByFocus();
        ancestors_or_siblings =
            element.AncestorsOrSiblingsAffectedByFocusInHas();
        break;
      case CSSSelector::kPseudoFocusVisible:
        children_or_siblings =
            element.ChildrenOrSiblingsAffectedByFocusVisible();
        ancestors_or_siblings =
            element.AncestorsOrSiblingsAffectedByFocusVisibleInHas();
        break;
      case CSSSelector::kPseudoFocusWithin:
        children_or_siblings =
            element.ChildrenOrSiblingsAffectedByFocusWithin();
        ancestors_or_siblings =
            element.AncestorsOrSiblingsAffectedByFocusInHas();
        break;
      case CSSSelector::kPseudoHover:
        children_or_siblings = element.ChildrenOrSiblingsAffectedByHover();
        ancestors_or_siblings =
            element.AncestorsOrSiblingsAffectedByHoverInHas();
        break;
      case CSSSelector::kPseudoActive:
        children_or_siblings = element.ChildrenOrSiblingsAffectedByActive();
        ancestors_or_siblings =
            element.AncestorsOrSiblingsAffectedByActiveInHas();
        break;
      default:
        // Activate :has() invalidation for all allowed pseudo-classes.
        //
        // IsPseudoClassValidWithinHasArgument() in css_selector_parser.cc
        // maintains the disallowed pseudo-classes inside :has().
        // If a :has() argument contains any of the disallowed pseudo,
        // CSSSelectorParser will drop the argument. If the argument is
        // dropped, RuleFeatureSet will not maintain the pseudo type for
        // :has() invalidation. So, StyleEngine will not do :has()
        // invalidation for the disallowed pseudo type changes even if
        // the Element::PseudoStateChanged() was called with the disallowed
        // pseudo type.
        break;
    }
  }

  AffectedByPseudoStateChange() : ancestors_or_siblings(true) {}  // For testing
};

void Element::PseudoStateChanged(CSSSelector::PseudoType pseudo) {
  PseudoStateChanged(pseudo, AffectedByPseudoStateChange(pseudo, *this));
}

void Element::PseudoStateChangedForTesting(CSSSelector::PseudoType pseudo) {
  PseudoStateChanged(pseudo, AffectedByPseudoStateChange());
}

void Element::PseudoStateChanged(
    CSSSelector::PseudoType pseudo,
    AffectedByPseudoStateChange&& affected_by_pseudo) {
  // We can't schedule invaliation sets from inside style recalc otherwise
  // we'd never process them.
  // TODO(esprehn): Make this an ASSERT and fix places that call into this
  // like HTMLSelectElement.
  Document& document = GetDocument();
  if (document.InStyleRecalc()) {
    return;
  }
  if (affected_by_pseudo.children_or_siblings ||
      affected_by_pseudo.ancestors_or_siblings) {
    document.GetStyleEngine().PseudoStateChangedForElement(
        pseudo, *this, affected_by_pseudo.children_or_siblings,
        affected_by_pseudo.ancestors_or_siblings);
  }
}

void Element::SetTargetedSnapAreaIdsForSnapContainers() {
  std::optional<cc::ElementId> targeted_area_id = std::nullopt;
  const LayoutBox* box = GetLayoutBox();
  while (box) {
    if (const ComputedStyle* style = box->Style()) {
      // If this is a snap area, associate it with the first snap area we
      // encountered, if any, since the previous snap container.
      if (box->IsScrollContainer() && !style->GetScrollSnapType().is_none) {
        if (auto* scrollable_area = box->GetScrollableArea()) {
          scrollable_area->SetTargetedSnapAreaId(targeted_area_id);
          GetDocument().View()->AddPendingSnapUpdate(scrollable_area);
        }
        targeted_area_id.reset();
      }
      // Only update |targeted_area_id| if we don't already have one so that we
      // prefer associating snap containers with their innermost snap targets.
      const auto& snap_align = style->GetScrollSnapAlign();
      if (!targeted_area_id &&
          (snap_align.alignment_block != cc::SnapAlignment::kNone ||
           snap_align.alignment_inline != cc::SnapAlignment::kNone)) {
        if (Node* node = box->GetNode()) {
          targeted_area_id =
              CompositorElementIdFromDOMNodeId(node->GetDomNodeId());
        }
        // Though not spec'd, we should prefer associating snap containers with
        // their innermost (in DOM hierarchy) snap areas.
        // This means we can skip any snap areas between this area and its snap
        // container.
        box = box->ContainingScrollContainer();
        continue;
      }
    }
    box = box->ContainingBlock();
  }
}

void Element::ClearTargetedSnapAreaIdsForSnapContainers() {
  const LayoutBox* box = GetLayoutBox();
  while (box) {
    if (const ComputedStyle* style = box->Style()) {
      if (box->IsScrollContainer() && !style->GetScrollSnapType().is_none) {
        if (auto* scrollable_area = box->GetScrollableArea()) {
          scrollable_area->SetTargetedSnapAreaId(std::nullopt);
        }
      }
    }
    box = box->ContainingBlock();
  }
}

GCedHeapVector<Member<Element>>* Element::ElementsFromAttributeOrInternals(
    const QualifiedName& attribute) const {
  GCedHeapVector<Member<Element>>* attr_associated_elements =
      GetAttrAssociatedElements(attribute);
  if (attr_associated_elements) {
    if (attr_associated_elements->empty()) {
      return nullptr;
    }
    return attr_associated_elements;
  }

  const ElementInternals* element_internals = GetElementInternals();
  if (!element_internals) {
    return nullptr;
  }

  const FrozenArray<Element>* element_internals_attr_elements =
      element_internals->GetElementArrayAttribute(attribute);

  if (!element_internals_attr_elements ||
      element_internals_attr_elements->empty()) {
    return nullptr;
  }

  return MakeGarbageCollected<GCedHeapVector<Member<Element>>>(
      element_internals_attr_elements->AsVector());
}

Element::HighlightRecalc Element::CalculateHighlightRecalc(
    const ComputedStyle* old_style,
    const ComputedStyle& new_style,
    const ComputedStyle* parent_style) const {
  if (!new_style.HasAnyHighlightPseudoElementStyles()) {
    return HighlightRecalc::kNone;
  }
  // If we are a root element (our parent is a Document or ShadowRoot), we can
  // skip highlight recalc if all of the following are true:
  // * there neither are nor were any non-UA highlight rules (regardless of
  //   whether or not they are non-universal), because then no inherited
  //   highlight properties have changed.
  // * the root’s effective zoom (‘zoom’ × page zoom × device scale factor) did
  //   not change, because then no units have changed size.
  // * the InitialData for custom properties has not changed, because then
  //   custom properties will still have the same initial values.
  // In that case, we only need to calculate highlight styles once, because our
  // UA styles only use type selectors, do not have custom properties, and we
  // never change them dynamically.
  DCHECK(IsInTreeScope());
  if (parentNode() == GetTreeScope().RootNode()) {
    if (new_style.HasNonUaHighlightPseudoStyles()) {
      return HighlightRecalc::kFull;
    }
    if (old_style) {
      if (old_style->HasNonUaHighlightPseudoStyles()) {
        return HighlightRecalc::kFull;
      }
      if (old_style->EffectiveZoom() != new_style.EffectiveZoom()) {
        return HighlightRecalc::kFull;
      }
      if (old_style->InitialData() != new_style.InitialData()) {
        return HighlightRecalc::kFull;
      }
      // Neither the new style nor the old style has any non-UA highlight rules,
      // so they will be equal. Let’s reuse the old styles for all highlights.
      return HighlightRecalc::kReuse;
    }
    return HighlightRecalc::kFull;
  }

  // If the parent matched any non-universal highlight rules, then we need
  // to recalc, in case there are universal highlight rules.
  bool parent_non_universal =
      parent_style != nullptr &&
      parent_style->HasNonUniversalHighlightPseudoStyles();

  // If we matched any non-universal highlight rules, then we need to recalc
  // and our children also need to recalc (see above).
  bool self_non_universal = new_style.HasNonUniversalHighlightPseudoStyles();

  if (parent_non_universal || self_non_universal) {
    return HighlightRecalc::kFull;
  }

  // If the parent has any relative units then we may need
  // recalc to capture sizes from the originating element. But note that
  // self will be recalculated regardless if self has its own non-universal
  // pseudo style.
  if (parent_style != nullptr &&
      parent_style->HighlightPseudoElementStylesDependOnRelativeUnits()) {
    return HighlightRecalc::kOriginatingDependent;
  }

  // If the parent style depends on custom properties we may need recalc
  // in the event the originating element has changed the values for those
  // properties.
  if (parent_style != nullptr &&
      parent_style->HighlightPseudoElementStylesHaveVariableReferences()) {
    return HighlightRecalc::kOriginatingDependent;
  }
  return HighlightRecalc::kNone;
}

bool Element::ShouldRecalcHighlightPseudoStyle(
    HighlightRecalc highlight_recalc,
    const ComputedStyle* highlight_parent,
    const ComputedStyle& originating_style,
    const Element* originating_container) const {
  if (highlight_recalc == HighlightRecalc::kFull) {
    return true;
  }
  DCHECK(highlight_recalc == HighlightRecalc::kOriginatingDependent);
  // If the highlight depends on variables and the variables on the
  // originating element have changed, we need to re-evaluate.
  if (highlight_parent && highlight_parent->HasVariableReference() &&
      (originating_style.InheritedVariables() !=
           highlight_parent->InheritedVariables() ||
       originating_style.NonInheritedVariables() !=
           highlight_parent->NonInheritedVariables())) {
    return true;
  }
  // Font relative units must be recomputed if the font size has changed.
  if (highlight_parent && highlight_parent->HasFontRelativeUnits() &&
      originating_style.SpecifiedFontSize() !=
          highlight_parent->SpecifiedFontSize()) {
    return true;
  }
  // If the originating element is a container for sizes, it means the
  // container has changed from that of the parent highlight, so we need
  // to re-evaluate container units.
  if (highlight_parent && highlight_parent->HasContainerRelativeValue() &&
      originating_container == this &&
      originating_style.CanMatchSizeContainerQueries(*this)) {
    return true;
  }
  // If there are logical direction relative units and the writing mode is
  // different from that of the parent, we need to re-evaluate the units.
  if (highlight_parent &&
      highlight_parent->HasLogicalDirectionRelativeUnits() &&
      originating_style.IsHorizontalWritingMode() !=
          highlight_parent->IsHorizontalWritingMode()) {
    return true;
  }
  // We do not need to return true for viewport unit dependencies because the
  // parent, if there is one, will have the same viewport dimensions. If the
  // parent otherwise has different units we must have already decided to do
  // a recalc.
  return false;
}

void Element::RecalcCustomHighlightPseudoStyle(
    const StyleRecalcContext& style_recalc_context,
    HighlightRecalc highlight_recalc,
    ComputedStyleBuilder& builder,
    const StyleHighlightData* parent_highlights,
    const ComputedStyle& originating_style) {
  const HashSet<AtomicString>* highlight_names =
      originating_style.CustomHighlightNames();
  if (!highlight_names) {
    return;
  }

  StyleHighlightData& highlights = builder.AccessHighlightData();
  for (const auto& highlight_name : *highlight_names) {
    const ComputedStyle* highlight_parent =
        parent_highlights ? parent_highlights->CustomHighlight(highlight_name)
                          : nullptr;
    if (ShouldRecalcHighlightPseudoStyle(highlight_recalc, highlight_parent,
                                         originating_style,
                                         style_recalc_context.size_container)) {
      const ComputedStyle* highlight_style = StyleForHighlightPseudoElement(
          style_recalc_context, highlight_parent, originating_style,
          kPseudoIdHighlight, highlight_name);
      if (highlight_style) {
        highlights.SetCustomHighlight(highlight_name, highlight_style);
      }
    }
  }
}

const ComputedStyle* Element::RecalcHighlightStyles(
    const StyleRecalcContext& style_recalc_context,
    const ComputedStyle* old_style,
    const ComputedStyle& new_style,
    const ComputedStyle* parent_style) {
  HighlightRecalc highlight_recalc =
      CalculateHighlightRecalc(old_style, new_style, parent_style);
  if (highlight_recalc == HighlightRecalc::kNone) {
    return &new_style;
  }

  ComputedStyleBuilder builder(new_style);

  if (highlight_recalc == HighlightRecalc::kReuse) {
    DCHECK(old_style);
    builder.SetHighlightData(old_style->HighlightData());
    return builder.TakeStyle();
  }

  const StyleHighlightData* parent_highlights =
      parent_style ? &parent_style->HighlightData() : nullptr;

  if (new_style.HasPseudoElementStyle(kPseudoIdSelection)) {
    const ComputedStyle* highlight_parent =
        parent_highlights ? parent_highlights->Selection() : nullptr;
    if (ShouldRecalcHighlightPseudoStyle(highlight_recalc, highlight_parent,
                                         new_style,
                                         style_recalc_context.size_container)) {
      builder.AccessHighlightData().SetSelection(
          StyleForHighlightPseudoElement(style_recalc_context, highlight_parent,
                                         new_style, kPseudoIdSelection));
    }
  }

  if (RuntimeEnabledFeatures::SearchTextHighlightPseudoEnabled() &&
      new_style.HasPseudoElementStyle(kPseudoIdSearchText)) {
    const ComputedStyle* highlight_parent_current =
        parent_highlights ? parent_highlights->SearchTextCurrent() : nullptr;
    if (ShouldRecalcHighlightPseudoStyle(highlight_recalc,
                                         highlight_parent_current, new_style,
                                         style_recalc_context.size_container)) {
      builder.AccessHighlightData().SetSearchTextCurrent(
          StyleForSearchTextPseudoElement(style_recalc_context,
                                          highlight_parent_current, new_style,
                                          StyleRequest::kCurrent));
    }
    const ComputedStyle* highlight_parent_not_current =
        parent_highlights ? parent_highlights->SearchTextNotCurrent() : nullptr;
    if (ShouldRecalcHighlightPseudoStyle(
            highlight_recalc, highlight_parent_not_current, new_style,
            style_recalc_context.size_container)) {
      builder.AccessHighlightData().SetSearchTextNotCurrent(
          StyleForSearchTextPseudoElement(
              style_recalc_context, highlight_parent_not_current, new_style,
              StyleRequest::kNotCurrent));
    }
  }

  if (new_style.HasPseudoElementStyle(kPseudoIdTargetText)) {
    const ComputedStyle* highlight_parent =
        parent_highlights ? parent_highlights->TargetText() : nullptr;
    if (ShouldRecalcHighlightPseudoStyle(highlight_recalc, highlight_parent,
                                         new_style,
                                         style_recalc_context.size_container)) {
      builder.AccessHighlightData().SetTargetText(
          StyleForHighlightPseudoElement(style_recalc_context, highlight_parent,
                                         new_style, kPseudoIdTargetText));
    }
  }

  if (new_style.HasPseudoElementStyle(kPseudoIdSpellingError)) {
    const ComputedStyle* highlight_parent =
        parent_highlights ? parent_highlights->SpellingError() : nullptr;
    if (ShouldRecalcHighlightPseudoStyle(highlight_recalc, highlight_parent,
                                         new_style,
                                         style_recalc_context.size_container)) {
      builder.AccessHighlightData().SetSpellingError(
          StyleForHighlightPseudoElement(style_recalc_context, highlight_parent,
                                         new_style, kPseudoIdSpellingError));
    }
  }

  if (new_style.HasPseudoElementStyle(kPseudoIdGrammarError)) {
    const ComputedStyle* highlight_parent =
        parent_highlights ? parent_highlights->GrammarError() : nullptr;
    if (ShouldRecalcHighlightPseudoStyle(highlight_recalc, highlight_parent,
                                         new_style,
                                         style_recalc_context.size_container)) {
      builder.AccessHighlightData().SetGrammarError(
          StyleForHighlightPseudoElement(style_recalc_context, highlight_parent,
                                         new_style, kPseudoIdGrammarError));
    }
  }

  if (new_style.HasPseudoElementStyle(kPseudoIdHighlight)) {
    RecalcCustomHighlightPseudoStyle(style_recalc_context, highlight_recalc,
                                     builder, parent_highlights, new_style);
  }

  return builder.TakeStyle();
}

void Element::SetAnimationStyleChange(bool animation_style_change) {
  if (animation_style_change && GetDocument().InStyleRecalc()) {
    return;
  }

  if (ElementRareDataVector* data = GetElementRareData()) {
    if (ElementAnimations* element_animations = data->GetElementAnimations()) {
      element_animations->SetAnimationStyleChange(animation_style_change);
    }
  }
}

void Element::SetNeedsAnimationStyleRecalc() {
  if (GetDocument().InStyleRecalc()) {
    return;
  }
  if (GetDocument().GetStyleEngine().InApplyAnimationUpdate()) {
    return;
  }
  if (GetStyleChangeType() != kNoStyleChange) {
    return;
  }

  SetNeedsStyleRecalc(kLocalStyleChange, StyleChangeReasonForTracing::Create(
                                             style_change_reason::kAnimation));

  // Setting this flag to 'true' only makes sense if there's an existing style,
  // otherwise there is no previous style to use as the basis for the new one.
  if (NeedsStyleRecalc() && GetComputedStyle() &&
      !GetComputedStyle()->IsEnsuredInDisplayNone()) {
    SetAnimationStyleChange(true);
  }
}

void Element::SetNeedsCompositingUpdate() {
  if (!GetDocument().IsActive()) {
    return;
  }
  LayoutBoxModelObject* layout_object = GetLayoutBoxModelObject();
  if (!layout_object) {
    return;
  }

  auto* painting_layer = layout_object->PaintingLayer();
  // Repaint because the foreign layer may have changed.
  painting_layer->SetNeedsRepaint();

  // Changes to AdditionalCompositingReasons can change direct compositing
  // reasons which affect paint properties.
  if (layout_object->CanHaveAdditionalCompositingReasons()) {
    layout_object->SetNeedsPaintPropertyUpdate();
  }
}

void Element::SetRegionCaptureCropId(
    std::unique_ptr<RegionCaptureCropId> crop_id) {
  ElementRareDataVector& rare_data = EnsureElementRareData();
  CHECK(!rare_data.GetRegionCaptureCropId());

  // Propagate efficient form through the rendering pipeline.
  rare_data.SetRegionCaptureCropId(std::move(crop_id));

  // If a LayoutObject does not yet exist, this full paint invalidation
  // will occur automatically after it is created.
  if (LayoutObject* layout_object = GetLayoutObject()) {
    // The SubCaptureTarget ID needs to be propagated to the paint system.
    layout_object->SetShouldDoFullPaintInvalidation();
  }
}

const RegionCaptureCropId* Element::GetRegionCaptureCropId() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetRegionCaptureCropId();
  }
  return nullptr;
}

void Element::SetRestrictionTargetId(std::unique_ptr<RestrictionTargetId> id) {
  CHECK(RuntimeEnabledFeatures::ElementCaptureEnabled(GetExecutionContext()));

  ElementRareDataVector& rare_data = EnsureElementRareData();
  CHECK(!rare_data.GetRestrictionTargetId());

  // Propagate efficient form through the rendering pipeline.
  // This has the intended side effect of forcing the element
  // into its own stacking context during rendering.
  rare_data.SetRestrictionTargetId(std::move(id));

  // If a LayoutObject does not yet exist, this full paint invalidation
  // will occur automatically after it is created.
  if (LayoutObject* layout_object = GetLayoutObject()) {
    // The paint properties need to updated, even though the style hasn't
    // changed.
    layout_object->SetNeedsPaintPropertyUpdate();

    // The SubCaptureTarget ID needs to be propagated to the paint system.
    layout_object->SetShouldDoFullPaintInvalidation();
  }
}

const RestrictionTargetId* Element::GetRestrictionTargetId() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetRestrictionTargetId();
  }
  return nullptr;
}

void Element::SetIsEligibleForElementCapture(bool value) {
  CHECK(GetRestrictionTargetId());

  const bool has_checked =
      HasElementFlag(ElementFlags::kHasCheckedElementCaptureEligibility);
  if (!has_checked) {
    SetElementFlag(ElementFlags::kHasCheckedElementCaptureEligibility, true);
  }

  if (has_checked) {
    const bool old_value =
        HasElementFlag(ElementFlags::kIsEligibleForElementCapture);

    if (value != old_value) {
      AddConsoleMessage(
          mojom::blink::ConsoleMessageSource::kRendering,
          mojom::blink::ConsoleMessageLevel::kInfo,
          String::Format("restrictTo(): Element %s restriction eligibility. "
                         "For eligibility conditions, see "
                         "https://screen-share.github.io/element-capture/"
                         "#elements-eligible-for-restriction",
                         value ? "gained" : "lost"));
    }
  } else {
    // We want to issue a different log message if the element is not eligible
    // when first painted.
    if (!value) {
      AddConsoleMessage(
          mojom::blink::ConsoleMessageSource::kRendering,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "restrictTo(): Element is not eligible for restriction. For "
          "eligibility conditions, see "
          "https://screen-share.github.io/element-capture/"
          "#elements-eligible-for-restriction");
    }
  }

  return SetElementFlag(ElementFlags::kIsEligibleForElementCapture, value);
}

void Element::SetCustomElementDefinition(CustomElementDefinition* definition) {
  DCHECK(definition);
  DCHECK(!GetCustomElementDefinition());
  EnsureElementRareData().SetCustomElementDefinition(definition);
  SetCustomElementState(CustomElementState::kCustom);
}

CustomElementDefinition* Element::GetCustomElementDefinition() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetCustomElementDefinition();
  }
  return nullptr;
}

CustomElementRegistry* Element::customElementRegistry() const {
  if (!RuntimeEnabledFeatures::ScopedCustomElementRegistryEnabled()) {
    return nullptr;
  }
  // TODO(crbug.com/429140221) Need to evaluate if storing registry
  // in element whenever needed is too memory consuming. For now
  // we'll take the naive approach and assume an element using its tree
  // scope's registry if not explicitly set.
  if (const ElementRareDataVector* data = GetElementRareData()) {
    if (data->HasCustomElementRegistrySet()) {
      return data->GetCustomElementRegistry();
    }
  }

  return GetTreeScope().customElementRegistry();
}

void Element::SetCustomElementRegistry(CustomElementRegistry* registry,
                                       bool explicitly_set) {
  DCHECK(RuntimeEnabledFeatures::ScopedCustomElementRegistryEnabled());
  // If the registry is the same as the tree scope's registry, we typically
  // can clear the registry field in rare data and have the registry implicitly
  // inferred to save memory. We can disable this optimization behavior by
  // "explicitly_set" flag so we can ensure the registry is retained in
  // scenarios like cross document/scope adoption.
  if (registry == GetTreeScope().customElementRegistry() && !explicitly_set) {
    EnsureElementRareData().ClearCustomElementRegistry();
  } else {
    EnsureElementRareData().SetCustomElementRegistry(registry);
  }
}

void Element::SetIsValue(const AtomicString& is_value) {
  DCHECK(IsValue().IsNull()) << "SetIsValue() should be called at most once.";
  EnsureElementRareData().SetIsValue(is_value);
}

const AtomicString& Element::IsValue() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->IsValue();
  }
  return g_null_atom;
}

void Element::SetDidAttachInternals() {
  EnsureElementRareData().SetDidAttachInternals();
}

bool Element::DidAttachInternals() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->DidAttachInternals();
  }
  return false;
}

ElementInternals& Element::EnsureElementInternals() {
  return EnsureElementRareData().EnsureElementInternals(To<HTMLElement>(*this));
}

const ElementInternals* Element::GetElementInternals() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetElementInternals();
  }
  return nullptr;
}

bool Element::CanAttachShadowRoot() const {
  const AtomicString& local_name = localName();
  // Checking IsCustomElement() here is just an optimization
  // because IsValidName is not cheap.
  return (IsCustomElement() && CustomElement::IsValidName(local_name)) ||
         IsValidShadowHostName(local_name);
}

const char* Element::ErrorMessageForAttachShadow(
    String mode,
    bool for_declarative,
    ShadowRootMode& mode_out) const {
  // https://dom.spec.whatwg.org/#concept-attach-a-shadow-root
  // 1. If shadow host’s namespace is not the HTML namespace, then throw a
  // "NotSupportedError" DOMException.
  // 2. If shadow host’s local name is not a valid custom element name,
  // "article", "aside", "blockquote", "body", "div", "footer", "h1", "h2",
  // "h3", "h4", "h5", "h6", "header", "main", "nav", "p", "section", or "span",
  // then throw a "NotSupportedError" DOMException.
  if (!CanAttachShadowRoot()) {
    return "This element does not support attachShadow";
  }

  // 3. If shadow host’s local name is a valid custom element name, or shadow
  // host’s is value is not null, then:
  // 3.1 Let definition be the result of looking up a custom element
  // definition given shadow host’s node document, its namespace, its local
  // name, and its is value.
  // 3.2 If definition is not null and definition’s
  // disable shadow is true, then throw a "NotSupportedError" DOMException.
  // Note: Checking IsCustomElement() is just an optimization because
  // IsValidName() is not cheap.
  if (IsCustomElement() &&
      (CustomElement::IsValidName(localName()) || !IsValue().IsNull())) {
    auto* registry = GetTreeScope().customElementRegistry();
    auto* definition =
        registry ? registry->DefinitionForName(IsValue().IsNull() ? localName()
                                                                  : IsValue())
                 : nullptr;
    if (definition && definition->DisableShadow()) {
      return "attachShadow() is disabled by disabledFeatures static field.";
    }
  }
  if (EqualIgnoringASCIICase(mode, keywords::kOpen)) {
    mode_out = ShadowRootMode::kOpen;
  } else if (EqualIgnoringASCIICase(mode, keywords::kClosed)) {
    mode_out = ShadowRootMode::kClosed;
  } else {
    CHECK(for_declarative);
    return "Invalid declarative shadowrootmode attribute value. Valid values "
           "are \"open\" and \"closed\".";
  }

  if (!GetShadowRoot()) {
    return nullptr;
  }
  // If shadow host has a non-null shadow root and "for declarative" is set,
  // then throw a "NotSupportedError" DOMException.
  if (for_declarative) {
    return "A second declarative shadow root cannot be created on a host.";
  }
  // If shadow host has a non-null shadow root, "for declarative" is unset,
  // and shadow root's "is declarative shadow root" property is false, then
  // throw a "NotSupportedError" DOMException.
  if (!GetShadowRoot()->IsDeclarativeShadowRoot()) {
    return "Shadow root cannot be created on a host which already hosts a "
           "shadow tree.";
  }
  return nullptr;
}

ShadowRoot* Element::attachShadow(const ShadowRootInit* shadow_root_init_dict,
                                  ExceptionState& exception_state) {
  DCHECK(shadow_root_init_dict->hasMode());
  String mode_string =
      V8ShadowRootModeToString(shadow_root_init_dict->mode().AsEnum());
  bool serializable = shadow_root_init_dict->getSerializableOr(false);
  if (serializable) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kElementAttachSerializableShadow);
  }
  bool clonable = shadow_root_init_dict->getClonableOr(false);

  auto focus_delegation = (shadow_root_init_dict->hasDelegatesFocus() &&
                           shadow_root_init_dict->delegatesFocus())
                              ? FocusDelegation::kDelegateFocus
                              : FocusDelegation::kNone;
  auto slot_assignment = (shadow_root_init_dict->hasSlotAssignment() &&
                          shadow_root_init_dict->slotAssignment() ==
                              V8SlotAssignmentMode::Enum::kManual)
                             ? SlotAssignmentMode::kManual
                             : SlotAssignmentMode::kNamed;
  auto reference_target =
      shadow_root_init_dict->hasReferenceTarget()
          ? AtomicString(shadow_root_init_dict->referenceTarget())
          : g_null_atom;

  // 1. Let registry be this's custom element registry.
  // 2. If init["customElementRegistry"] is not null
  // 2-1. Set registry to init["customElementRegistry"].
  bool scoped_registry =
      RuntimeEnabledFeatures::ScopedCustomElementRegistryEnabled() &&
      shadow_root_init_dict->hasCustomElementRegistry() &&
      shadow_root_init_dict->customElementRegistry();
  auto* registry = scoped_registry
                       ? shadow_root_init_dict->customElementRegistry()
                       : GetTreeScope().customElementRegistry();
  // 2-2. If registry's "is scoped" is false and registry is not this's node
  // document's custom element registry, then throw a "NotSupportedError"
  // DOMException.
  if (registry && registry->IsGlobalRegistry() &&
      registry != GetDocument().customElementRegistry()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The registry provided is a global registry from another document");
    return nullptr;
  }

  ShadowRootMode mode;
  if (const char* error_message = ErrorMessageForAttachShadow(
          mode_string, /*for_declarative*/ false, mode)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      error_message);
    return nullptr;
  }

  switch (mode) {
    case ShadowRootMode::kOpen:
      UseCounter::Count(GetDocument(), WebFeature::kElementAttachShadowOpen);
      break;
    case ShadowRootMode::kClosed:
      UseCounter::Count(GetDocument(), WebFeature::kElementAttachShadowClosed);
      break;
    case ShadowRootMode::kUserAgent:
      NOTREACHED();
  }

  // If there's already a declarative shadow root, verify that the existing
  // mode is the same as the requested mode.
  if (auto* existing_shadow = GetShadowRoot()) {
    CHECK(existing_shadow->IsDeclarativeShadowRoot());
    if (existing_shadow->GetMode() != mode) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotSupportedError,
          "The requested mode does not match the existing declarative shadow "
          "root's mode");
      return nullptr;
    }
  }

  ShadowRoot& shadow_root = AttachShadowRootInternal(
      mode, focus_delegation, slot_assignment, registry, serializable, clonable,
      reference_target);

  // Ensure that the returned shadow root is not marked as declarative so that
  // attachShadow() calls after the first one do not succeed for a shadow host
  // with a declarative shadow root.
  shadow_root.SetIsDeclarativeShadowRoot(false);
  return &shadow_root;
}

bool Element::AttachDeclarativeShadowRoot(
    HTMLTemplateElement& template_element,
    String mode_string,
    FocusDelegation focus_delegation,
    SlotAssignmentMode slot_assignment,
    bool serializable,
    bool clonable,
    const AtomicString& adopted_stylesheets,
    const AtomicString& reference_target,
    const bool waiting_for_scoped_registry) {
  // 12. Run attach a shadow root with shadow host equal to declarative shadow
  // host element, mode equal to declarative shadow mode, and delegates focus
  // equal to declarative shadow delegates focus. If an exception was thrown by
  // attach a shadow root, catch it, and ignore the exception.
  ShadowRootMode mode;
  if (const char* error_message = ErrorMessageForAttachShadow(
          mode_string, /*for_declarative*/ true, mode)) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kError, error_message));
    return false;
  }
  CHECK(mode == ShadowRootMode::kOpen || mode == ShadowRootMode::kClosed);

  CustomElementRegistry* registry = nullptr;
  // Get global registry of the document by default.
  if (auto* window = GetDocument().domWindow()) {
    registry = window->customElements();
  }

  // If the declarative shadow root is waiting a scoped registry, set
  // the current registry to null explicitly.
  if (RuntimeEnabledFeatures::ScopedCustomElementRegistryEnabled() &&
      waiting_for_scoped_registry) {
    registry = nullptr;
  }

  ShadowRoot& shadow_root = AttachShadowRootInternal(
      mode, focus_delegation, slot_assignment, registry, serializable, clonable,
      reference_target);
  // 10.8.5. Set declarative shadow host element's shadow host's "is declarative
  // shadow root" property to true.
  shadow_root.SetIsDeclarativeShadowRoot(true);
  // 10.8.7. Set declarative shadow host element's shadow host's "available
  // to element internals" to true.
  shadow_root.SetAvailableToElementInternals(true);
  // 10.8.NEW. Process shadowrootadoptedstylesheets attribute.
  if (RuntimeEnabledFeatures::DeclarativeCSSModulesEnabled()) {
    shadow_root.ProcessAdoptedStylesheetAttribute(adopted_stylesheets);
  }
  return true;
}

ShadowRoot& Element::CreateUserAgentShadowRoot(SlotAssignmentMode mode) {
  DCHECK(!GetShadowRoot());
  GetDocument().SetContainsShadowRoot();
  return CreateAndAttachShadowRoot(ShadowRootMode::kUserAgent, mode);
}

ShadowRoot& Element::AttachShadowRootInternal(
    ShadowRootMode type,
    FocusDelegation focus_delegation,
    SlotAssignmentMode slot_assignment_mode,
    CustomElementRegistry* registry,
    bool serializable,
    bool clonable,
    const AtomicString& reference_target) {
  // SVG <use> is a special case for using this API to create a closed shadow
  // root.
  DCHECK(CanAttachShadowRoot() || IsA<SVGUseElement>(*this));
  DCHECK(type == ShadowRootMode::kOpen || type == ShadowRootMode::kClosed)
      << type;
  DCHECK(!AlwaysCreateUserAgentShadowRoot());
  DCHECK(reference_target.IsNull() ||
         RuntimeEnabledFeatures::ShadowRootReferenceTargetEnabled(
             GetExecutionContext()));

  GetDocument().SetContainsShadowRoot();

  if (auto* shadow_root = GetShadowRoot()) {
    // NEW. If shadow host has a non-null shadow root whose "is declarative
    // shadow root property is true, then remove all of shadow root’s children,
    // in tree order. Return shadow host’s shadow root.
    DCHECK(shadow_root->IsDeclarativeShadowRoot());
    shadow_root->RemoveChildren();
    return *shadow_root;
  }

  // 5. Let shadow be a new shadow root whose node document is this’s node
  // document, host is this, and mode is init’s mode.
  ShadowRoot& shadow_root =
      CreateAndAttachShadowRoot(type, slot_assignment_mode);
  // 6. Set shadow’s delegates focus to init’s delegatesFocus.
  shadow_root.SetDelegatesFocus(focus_delegation ==
                                FocusDelegation::kDelegateFocus);
  // 9. Set shadow’s declarative to false.
  shadow_root.SetIsDeclarativeShadowRoot(false);

  // 12. Set shadow's custom element registry to registry
  if (RuntimeEnabledFeatures::ScopedCustomElementRegistryEnabled()) {
    shadow_root.SetCustomElementRegistry(registry);
  }
  // 11. Set shadow’s serializable to serializable.
  shadow_root.setSerializable(serializable);
  // 10. Set shadow’s clonable to clonable.
  shadow_root.setClonable(clonable);
  // NEW. Set reference target.
  shadow_root.setReferenceTarget(reference_target);

  // 7. If this’s custom element state is "precustomized" or "custom", then set
  // shadow’s available to element internals to true.
  shadow_root.SetAvailableToElementInternals(
      !(IsCustomElement() &&
        GetCustomElementState() != CustomElementState::kCustom &&
        GetCustomElementState() != CustomElementState::kPreCustomized));

  // 8. Set this’s shadow root to shadow.
  return shadow_root;
}

ShadowRoot& Element::AttachShadowRootForTesting(ShadowRootMode type) {
  return AttachShadowRootInternal(type, FocusDelegation::kNone,
                                  SlotAssignmentMode::kNamed,
                                  /*registry*/ nullptr,
                                  /*serializable*/ false,
                                  /*clonable*/ false,
                                  /*reference_target*/ g_null_atom);
}

ShadowRoot* Element::OpenShadowRoot() const {
  ShadowRoot* root = GetShadowRoot();
  return root && root->GetMode() == ShadowRootMode::kOpen ? root : nullptr;
}

ShadowRoot* Element::ClosedShadowRoot() const {
  ShadowRoot* root = GetShadowRoot();
  if (!root) {
    return nullptr;
  }
  return root->GetMode() == ShadowRootMode::kClosed ? root : nullptr;
}

ShadowRoot* Element::AuthorShadowRoot() const {
  ShadowRoot* root = GetShadowRoot();
  if (!root) {
    return nullptr;
  }
  return !root->IsUserAgent() ? root : nullptr;
}

ShadowRoot* Element::UserAgentShadowRoot() const {
  ShadowRoot* root = GetShadowRoot();
  DCHECK(!root || root->IsUserAgent());
  return root;
}

ShadowRoot& Element::EnsureUserAgentShadowRoot(SlotAssignmentMode mode) {
  if (ShadowRoot* shadow_root = UserAgentShadowRoot()) {
    CHECK_EQ(shadow_root->GetMode(), ShadowRootMode::kUserAgent);
    CHECK_EQ(shadow_root->GetSlotAssignmentMode(), mode);
    return *shadow_root;
  }
  ShadowRoot& shadow_root = CreateUserAgentShadowRoot(mode);
  DidAddUserAgentShadowRoot(shadow_root);
  return shadow_root;
}

bool Element::ChildTypeAllowed(NodeType type) const {
  switch (type) {
    case kElementNode:
    case kTextNode:
    case kCommentNode:
    case kProcessingInstructionNode:
    case kCdataSectionNode:
      return true;
    default:
      break;
  }
  return false;
}

namespace {

bool HasSiblingsForNonEmpty(const Node* sibling,
                            Node* (*next_func)(const Node&)) {
  for (; sibling; sibling = next_func(*sibling)) {
    if (sibling->IsElementNode()) {
      return true;
    }
    auto* text_node = DynamicTo<Text>(sibling);
    if (text_node && !text_node->data().empty()) {
      return true;
    }
  }
  return false;
}

}  // namespace

void Element::CheckForEmptyStyleChange(const Node* node_before_change,
                                       const Node* node_after_change) {
  if (!InActiveDocument()) {
    return;
  }
  if (!StyleAffectedByEmpty()) {
    return;
  }
  if (HasSiblingsForNonEmpty(node_before_change,
                             NodeTraversal::PreviousSibling) ||
      HasSiblingsForNonEmpty(node_after_change, NodeTraversal::NextSibling)) {
    return;
  }
  PseudoStateChanged(CSSSelector::kPseudoEmpty);
}

void Element::ChildrenChanged(const ChildrenChange& change) {
  // ContainerNode::ChildrenChanged may run SynchronousMutationObservers which
  // want to do flat tree traversals. If we SetNeedsAssignmentRecalc after those
  // mutation observers, then they won't get to see an up to date version of the
  // flat tree.
  if (ShadowRoot* shadow_root = GetShadowRoot()) {
    shadow_root->SetNeedsAssignmentRecalc();
  }

  ContainerNode::ChildrenChanged(change);

  CheckForEmptyStyleChange(change.sibling_before_change,
                           change.sibling_after_change);

  if (!change.ByParser()) {
    if (change.IsChildElementChange()) {
      Element* changed_element = To<Element>(change.sibling_changed);
      bool removed = change.type == ChildrenChangeType::kElementRemoved;
      CheckForSiblingStyleChanges(
          removed ? kSiblingElementRemoved : kSiblingElementInserted,
          changed_element, change.sibling_before_change,
          change.sibling_after_change);
      GetDocument()
          .GetStyleEngine()
          .ScheduleInvalidationsForHasPseudoAffectedByInsertionOrRemoval(
              this, change.sibling_before_change, *changed_element, removed);
    } else if (change.type == ChildrenChangeType::kAllChildrenRemoved) {
      GetDocument()
          .GetStyleEngine()
          .ScheduleInvalidationsForHasPseudoWhenAllChildrenRemoved(*this);
    }
  }

  if (GetDocument().HasDirAttribute()) {
    AdjustDirectionalityIfNeededAfterChildrenChanged(change);
  }

  AdjustContainerTimingIfNeededAfterChildrenChanged(change);
}

void Element::FinishParsingChildren() {
  SetIsFinishedParsingChildren(true);
  CheckForEmptyStyleChange(this, this);
  CheckForSiblingStyleChanges(kFinishedParsingChildren, nullptr, lastChild(),
                              nullptr);

  if (GetDocument().HasPendingExpectLinkElements()) {
    DCHECK(GetDocument().GetRenderBlockingResourceManager());
    GetDocument()
        .GetRenderBlockingResourceManager()
        ->RemovePendingParsingElement(GetIdAttribute(), this);
  }
  GetDocument()
      .GetStyleEngine()
      .ScheduleInvalidationsForHasPseudoAffectedByInsertionOrRemoval(
          parentElement(), previousSibling(), *this, /* removal */ false);
}

AttrNodeList* Element::GetAttrNodeList() {
  if (ElementRareDataVector* data = GetElementRareData()) {
    return data->GetAttrNodeList();
  }
  return nullptr;
}

void Element::RemoveAttrNodeList() {
  DCHECK(GetAttrNodeList());
  if (ElementRareDataVector* data = GetElementRareData()) {
    data->RemoveAttrNodeList();
  }
}

Attr* Element::setAttributeNodeNS(Attr* attr, ExceptionState& exception_state) {
  return setAttributeNode(attr, exception_state);
}

Attr* Element::removeAttributeNode(Attr* attr,
                                   ExceptionState& exception_state) {
  if (attr->ownerElement() != this) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "The node provided is owned by another element.");
    return nullptr;
  }

  DCHECK_EQ(GetDocument(), attr->GetDocument());

  SynchronizeAttribute(attr->GetQualifiedName());

  wtf_size_t index =
      GetElementData()->Attributes().FindIndex(attr->GetQualifiedName());
  if (index == kNotFound) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "The attribute was not found on this element.");
    return nullptr;
  }

  DetachAttrNodeAtIndex(attr, index);
  return attr;
}

void Element::LangAttributeChanged() {
  // Propagate the change to all descendants.
  for (Element& child : ElementTraversal::ChildrenOf(*this)) {
    if (child.hasAttribute(html_names::kLangAttr)) {
      continue;
    }
    child.LangAttributeChanged();
  }
  SetNeedsStyleRecalc(
      kSubtreeStyleChange,
      StyleChangeReasonForTracing::Create(style_change_reason::kPseudoClass));
  PseudoStateChanged(CSSSelector::kPseudoLang);
}

void Element::ParseAttribute(const AttributeModificationParams& params) {
  // Note that `HTMLElement::ParseAttribute` does not call this base class
  // method for anything other than *namespaced* attributes, e.g. `xml:lang`.
  // Therefore, in most cases, normal HTML attributes will not get handled here.
  if (params.name.Matches(xml_names::kLangAttr)) {
    LangAttributeChanged();
  }
}

// static
std::optional<QualifiedName> Element::ParseAttributeName(
    const AtomicString& namespace_uri,
    const AtomicString& qualified_name,
    ExceptionState& exception_state) {
  AtomicString prefix, local_name;
  if (!Document::ParseQualifiedName(
          qualified_name, prefix, local_name, exception_state,
          Document::QualifiedNameParsingMode::kParsingAttribute)) {
    return std::nullopt;
  }
  DCHECK(!exception_state.HadException());

  QualifiedName q_name(prefix, local_name, namespace_uri);

  if (!Document::HasValidNamespaceForAttributes(q_name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNamespaceError,
        StrCat(
            {"'", namespace_uri, "' is an invalid namespace for attributes."}));
    return std::nullopt;
  }
  return q_name;
}

void Element::setAttributeNS(const AtomicString& namespace_uri,
                             const AtomicString& qualified_name,
                             String value,
                             ExceptionState& exception_state) {
  std::optional<QualifiedName> parsed_name =
      ParseAttributeName(namespace_uri, qualified_name, exception_state);
  if (!parsed_name) {
    return;
  }

  AtomicString trusted_value(TrustedTypesCheckForAttribute(
      *parsed_name, std::move(value), "setAttributeNS", exception_state));
  if (exception_state.HadException()) {
    return;
  }

  setAttribute(*parsed_name, trusted_value);
}

void Element::setAttributeNS(const AtomicString& namespace_uri,
                             const AtomicString& qualified_name,
                             const V8TrustedType* trusted_string,
                             ExceptionState& exception_state) {
  std::optional<QualifiedName> parsed_name =
      ParseAttributeName(namespace_uri, qualified_name, exception_state);
  if (!parsed_name) {
    return;
  }

  AtomicString value(TrustedTypesCheckForAttribute(
      *parsed_name, trusted_string, "setAttributeNS", exception_state));
  if (exception_state.HadException()) {
    return;
  }

  setAttribute(*parsed_name, value);
}

void Element::RemoveAttributeInternal(wtf_size_t index,
                                      AttributeModificationReason reason) {
  MutableAttributeCollection attributes =
      EnsureUniqueElementData().Attributes();
  SECURITY_DCHECK(index < attributes.size());

  QualifiedName name = attributes[index].GetName();
  AtomicString value_being_removed = attributes[index].Value();

  if (reason !=
      AttributeModificationReason::kBySynchronizationOfLazyAttribute) {
    if (!value_being_removed.IsNull()) {
      WillModifyAttribute(name, value_being_removed, g_null_atom);
    } else if (GetCustomElementState() == CustomElementState::kCustom) {
      // This would otherwise be enqueued by willModifyAttribute.
      CustomElement::EnqueueAttributeChangedCallback(
          *this, name, value_being_removed, g_null_atom);
    }
  }

  if (Attr* attr_node = AttrIfExists(name)) {
    DetachAttrNodeFromElementWithValue(attr_node, attributes[index].Value());
  }

  attributes.Remove(index);

  if (reason !=
      AttributeModificationReason::kBySynchronizationOfLazyAttribute) {
    DidRemoveAttribute(name, value_being_removed);
  }

  // TODO(sesse): Consider recalculating attribute_or_class_bloom_ filter here,
  // so that it reflects the removal (but beware of pathological cases
  // where removing all attributes send us into O(n²)).
}

void Element::AppendAttributeInternal(const QualifiedName& name,
                                      const AtomicString& value,
                                      AttributeModificationReason reason) {
  if (reason !=
      AttributeModificationReason::kBySynchronizationOfLazyAttribute) {
    WillModifyAttribute(name, g_null_atom, value);
  }
  attribute_or_class_bloom_ |= FilterForAttribute(name);
  UpdateSubtreeBloomFilterAfterInsert();
  EnsureUniqueElementData().Attributes().Append(name, value);
  if (reason !=
      AttributeModificationReason::kBySynchronizationOfLazyAttribute) {
    DidAddAttribute(name, value);
  }
}

void Element::removeAttributeNS(const AtomicString& namespace_uri,
                                const AtomicString& local_name) {
  removeAttribute(QualifiedName(g_null_atom, local_name, namespace_uri));
}

Attr* Element::getAttributeNode(const AtomicString& local_name) {
  if (!HasElementData()) {
    return nullptr;
  }
  AtomicStringTable::WeakResult hint = WeakLowercaseIfNecessary(local_name);
  SynchronizeAttributeHinted(local_name, hint);
  const Attribute* attribute =
      GetElementData()->Attributes().FindHinted(local_name, hint);
  if (!attribute) {
    return nullptr;
  }
  return EnsureAttr(attribute->GetName());
}

Attr* Element::getAttributeNodeNS(const AtomicString& namespace_uri,
                                  const AtomicString& local_name) {
  if (!HasElementData()) {
    return nullptr;
  }
  QualifiedName q_name(g_null_atom, local_name, namespace_uri);
  SynchronizeAttribute(q_name);
  const Attribute* attribute = GetElementData()->Attributes().Find(q_name);
  if (!attribute) {
    return nullptr;
  }
  return EnsureAttr(attribute->GetName());
}

bool Element::hasAttribute(const AtomicString& local_name) const {
  if (!HasElementData()) {
    return false;
  }
  AtomicStringTable::WeakResult hint = WeakLowercaseIfNecessary(local_name);
  SynchronizeAttributeHinted(local_name, hint);
  return GetElementData()->Attributes().FindHinted(local_name, hint);
}

bool Element::hasAttributeNS(const AtomicString& namespace_uri,
                             const AtomicString& local_name) const {
  if (!HasElementData()) {
    return false;
  }
  QualifiedName q_name(g_null_atom, local_name, namespace_uri);
  SynchronizeAttribute(q_name);
  return GetElementData()->Attributes().Find(q_name);
}

bool Element::IsShadowHostWithDelegatesFocus() const {
  return GetShadowRoot() && GetShadowRoot()->delegatesFocus();
}

// https://html.spec.whatwg.org/C/#get-the-focusable-area
Element* Element::GetFocusableArea(bool in_descendant_traversal) const {
  // GetFocusableArea should only be called as a fallback on elements which
  // aren't mouse and keyboard focusable, unless we are looking for an initial
  // focus candidate for a dialog element in which case we are looking for a
  // keyboard focusable element and will be calling this for mouse focusable
  // elements.
  DCHECK(!IsKeyboardFocusableSlow() ||
         FocusController::AdjustedTabIndex(*this) < 0);

  // TODO(crbug.com/1018619): Support AREA -> IMG delegation.
  if (!IsShadowHostWithDelegatesFocus()) {
    return nullptr;
  }
  Document& doc = GetDocument();
  if (AuthorShadowRoot()) {
    UseCounter::Count(doc, WebFeature::kDelegateFocus);
  }

  Element* focused_element = doc.FocusedElement();
  if (focused_element &&
      IsShadowIncludingInclusiveAncestorOf(*focused_element)) {
    return focused_element;
  }

  DCHECK(GetShadowRoot());
  return GetFocusDelegate(in_descendant_traversal);
}

Element* Element::GetFocusDelegate(bool in_descendant_traversal) const {
  ShadowRoot* shadowroot = GetShadowRoot();
  if (shadowroot && !shadowroot->IsUserAgent() &&
      !shadowroot->delegatesFocus()) {
    return nullptr;
  }

  const ContainerNode* where_to_look = this;
  if (IsShadowHostWithDelegatesFocus()) {
    where_to_look = shadowroot;
  }

  if (Element* autofocus_delegate = where_to_look->GetAutofocusDelegate()) {
    return autofocus_delegate;
  }

  for (Element& descendant : ElementTraversal::DescendantsOf(*where_to_look)) {
    // Dialog elements should only initially focus keyboard focusable elements,
    // not mouse focusable elements.
    if (descendant.IsFocusable() &&
        (!IsA<HTMLDialogElement>(this) ||
         FocusController::AdjustedTabIndex(descendant) >= 0)) {
      return &descendant;
    }
    if (Element* focusable_area =
            descendant.GetFocusableArea(/*in_descendant_traversal=*/true)) {
      return focusable_area;
    }
  }
  return nullptr;
}

void Element::focusForBindings(const FocusOptions* options) {
  Focus(FocusParams(SelectionBehaviorOnFocus::kRestore,
                    mojom::blink::FocusType::kScript,
                    /*capabilities=*/nullptr, options));
}

void Element::Focus() {
  Focus(FocusParams());
}

void Element::Focus(const FocusOptions* options) {
  Focus(FocusParams(SelectionBehaviorOnFocus::kRestore,
                    mojom::blink::FocusType::kNone, /*capabilities=*/nullptr,
                    options));
}

void Element::Focus(const FocusParams& params) {
  if (!isConnected()) {
    return;
  }

  if (!GetDocument().IsFocusAllowed(params.focus_trigger)) {
    return;
  }

  if (GetDocument().FocusedElement() == this) {
    return;
  }

  if (!GetDocument().IsActive()) {
    return;
  }

  auto* frame_owner_element = DynamicTo<HTMLFrameOwnerElement>(this);
  if (frame_owner_element && frame_owner_element->contentDocument() &&
      frame_owner_element->contentDocument()->UnloadStarted()) {
    return;
  }

  FocusOptions* focus_options = nullptr;
  bool should_consume_user_activation = false;
  if (params.focus_trigger == FocusTrigger::kScript) {
    LocalFrame& frame = *GetDocument().GetFrame();
    if (!frame.AllowFocusWithoutUserActivation() &&
        !LocalFrame::HasTransientUserActivation(&frame)) {
      return;
    }

    // Fenced frame focusing should not auto-scroll, since that behavior can
    // be observed by an embedder.
    if (frame.IsInFencedFrameTree()) {
      focus_options = FocusOptions::Create();
      focus_options->setPreventScroll(true);
    }

    // Wait to consume user activation until after the focus takes place.
    if (!frame.AllowFocusWithoutUserActivation()) {
      should_consume_user_activation = true;
    }
  }

  FocusParams params_to_use = FocusParams(
      params.selection_behavior, params.type, params.source_capabilities,
      focus_options ? focus_options : params.options, params.focus_trigger);

  // Ensure we have clean style (including forced display locks).
  GetDocument().UpdateStyleAndLayoutTreeForElement(
      this, DocumentUpdateReason::kFocus);

  // https://html.spec.whatwg.org/C/#focusing-steps
  //
  // 1. If new focus target is not a focusable area, ...
  if (!IsFocusable()) {
    if (Element* new_focus_target = GetFocusableArea()) {
      // Unlike the specification, we re-run focus() for new_focus_target
      // because we can't change |this| in a member function.
      new_focus_target->Focus(FocusParams(
          SelectionBehaviorOnFocus::kReset, mojom::blink::FocusType::kForward,
          /*capabilities=*/nullptr, params_to_use.options));
    }
    // 2. If new focus target is null, then:
    //  2.1. If no fallback target was specified, then return.
    return;
  }
  // If a script called focus(), then the type would be kScript. This means
  // we are activating because of a script action (kScriptFocus). Otherwise,
  // this is a user activation (kUserFocus).
  ActivateDisplayLockIfNeeded(params_to_use.type ==
                                      mojom::blink::FocusType::kScript
                                  ? DisplayLockActivationReason::kScriptFocus
                                  : DisplayLockActivationReason::kUserFocus);

  if (!GetDocument().GetPage()->GetFocusController().SetFocusedElement(
          this, GetDocument().GetFrame(), params_to_use)) {
    return;
  }

  if (GetDocument().FocusedElement() == this) {
    ChromeClient& chrome_client = GetDocument().GetPage()->GetChromeClient();
    if (GetDocument().GetFrame()->HasStickyUserActivation()) {
      // Bring up the keyboard in the context of anything triggered by a user
      // gesture. Since tracking that across arbitrary boundaries (eg.
      // animations) is difficult, for now we match IE's heuristic and bring
      // up the keyboard if there's been any gesture since load.
      chrome_client.ShowVirtualKeyboardOnElementFocus(
          *GetDocument().GetFrame());
    }

    // TODO(bebeaudr): We might want to move the following code into the
    // HasStickyUserActivation condition above once https://crbug.com/1208874 is
    // fixed.
    //
    // Trigger a tooltip to show for the newly focused element only when the
    // focus was set resulting from a keyboard action.
    //
    // TODO(bebeaudr): To also trigger a tooltip when the |params_to_use.type|
    // is kSpatialNavigation, we'll first have to ensure that the fake mouse
    // move event fired by `SpatialNavigationController::DispatchMouseMoveEvent`
    // does not lead to a cursor triggered tooltip update. The only tooltip
    // update that there should be in that case is the one triggered from the
    // spatial navigation keypress. This issue is tracked in
    // https://crbug.com/1206446.
    bool is_focused_from_keypress = false;
    switch (params_to_use.type) {
      case mojom::blink::FocusType::kScript:
        if (GetDocument()
                .GetFrame()
                ->LocalFrameRoot()
                .GetEventHandler()
                .IsHandlingKeyEvent()) {
          is_focused_from_keypress = true;
        }
        break;
      case mojom::blink::FocusType::kForward:
      case mojom::blink::FocusType::kBackward:
      case mojom::blink::FocusType::kAccessKey:
        is_focused_from_keypress = true;
        break;
      default:
        break;
    }

    if (is_focused_from_keypress) {
      chrome_client.ElementFocusedFromKeypress(*GetDocument().GetFrame(), this);
    } else {
      chrome_client.ClearKeyboardTriggeredTooltip(*GetDocument().GetFrame());
    }
  }

  if (should_consume_user_activation) {
    // Fenced frames should consume user activation when attempting to pull
    // focus across a fenced boundary into itself.
    // TODO(crbug.com/848778) Right now the browser can't verify that the
    // renderer properly consumed user activation. When user activation code is
    // migrated to the browser, move this logic to the browser as well.
    LocalFrame::ConsumeTransientUserActivation(GetDocument().GetFrame());
  }
}

void Element::SetFocused(bool now_focused, mojom::blink::FocusType focus_type) {
  EnsureElementRareData().SetWasLastFocusFromUserGesture(
      focus_type != mojom::blink::FocusType::kNone &&
      focus_type != mojom::blink::FocusType::kScript);
  // Recurse up author shadow trees to mark shadow hosts if it matches :focus.
  // TODO(kochi): Handle UA shadows which marks multiple nodes as focused such
  // as <input type="date"> the same way as author shadow.
  if (ShadowRoot* root = ContainingShadowRoot()) {
    if (!root->IsUserAgent()) {
      OwnerShadowHost()->SetFocused(now_focused, focus_type);
    }
  }

  // We'd like to invalidate :focus style for kPage even if element's focus
  // state has not been changed, because the element might have been focused
  // while the page was inactive.
  if (IsFocused() == now_focused &&
      focus_type != mojom::blink::FocusType::kPage) {
    return;
  }

  if (focus_type == mojom::blink::FocusType::kMouse) {
    GetDocument().SetHadKeyboardEvent(false);
  }
  GetDocument().UserActionElements().SetFocused(this, now_focused);

  FocusStateChanged();

  if (GetLayoutObject() || now_focused) {
    return;
  }

  // If :focus sets display: none, we lose focus but still need to recalc our
  // style.
  if (!ChildrenOrSiblingsAffectedByFocus()) {
    SetNeedsStyleRecalc(kLocalStyleChange,
                        StyleChangeReasonForTracing::CreateWithExtraData(
                            style_change_reason::kPseudoClass,
                            style_change_extra_data::g_focus));
  }
  PseudoStateChanged(CSSSelector::kPseudoFocus);

  if (!ChildrenOrSiblingsAffectedByFocusVisible()) {
    SetNeedsStyleRecalc(kLocalStyleChange,
                        StyleChangeReasonForTracing::CreateWithExtraData(
                            style_change_reason::kPseudoClass,
                            style_change_extra_data::g_focus_visible));
  }
  PseudoStateChanged(CSSSelector::kPseudoFocusVisible);

  if (!ChildrenOrSiblingsAffectedByFocusWithin()) {
    SetNeedsStyleRecalc(kLocalStyleChange,
                        StyleChangeReasonForTracing::CreateWithExtraData(
                            style_change_reason::kPseudoClass,
                            style_change_extra_data::g_focus_within));
  }
  PseudoStateChanged(CSSSelector::kPseudoFocusWithin);
}

void Element::SetDragged(bool new_value) {
  if (new_value == IsDragged()) {
    return;
  }

  Node::SetDragged(new_value);

  // If :-webkit-drag sets display: none we lose our dragging but still need
  // to recalc our style.
  if (!GetLayoutObject()) {
    if (new_value) {
      return;
    }
    if (ChildrenOrSiblingsAffectedByDrag()) {
      PseudoStateChanged(CSSSelector::kPseudoDrag);
    } else {
      SetNeedsStyleRecalc(kLocalStyleChange,
                          StyleChangeReasonForTracing::CreateWithExtraData(
                              style_change_reason::kPseudoClass,
                              style_change_extra_data::g_drag));
    }
    return;
  }

  if (GetComputedStyle()->AffectedByDrag()) {
    StyleChangeType change_type =
        GetComputedStyle()->HasPseudoElementStyle(kPseudoIdFirstLetter)
            ? kSubtreeStyleChange
            : kLocalStyleChange;
    SetNeedsStyleRecalc(change_type,
                        StyleChangeReasonForTracing::CreateWithExtraData(
                            style_change_reason::kPseudoClass,
                            style_change_extra_data::g_drag));
  }
  if (ChildrenOrSiblingsAffectedByDrag()) {
    PseudoStateChanged(CSSSelector::kPseudoDrag);
  }
}

void Element::UpdateSelectionOnFocus(
    SelectionBehaviorOnFocus selection_behavior) {
  UpdateSelectionOnFocus(selection_behavior, FocusOptions::Create());
}

void Element::UpdateSelectionOnFocus(
    SelectionBehaviorOnFocus selection_behavior,
    const FocusOptions* options) {
  if (selection_behavior == SelectionBehaviorOnFocus::kNone) {
    return;
  }
  if (IsRootEditableElement(*this)) {
    LocalFrame* frame = GetDocument().GetFrame();
    if (!frame) {
      return;
    }

    // When focusing an editable element in an iframe, don't reset the selection
    // if it already contains a selection.
    if (this == frame->Selection()
                    .ComputeVisibleSelectionInDOMTreeDeprecated()
                    .RootEditableElement()) {
      if (!options->preventScroll()) {
        frame->Selection().RevealSelection();
      }
      return;
    }

    // FIXME: We should restore the previous selection if there is one.
    // Passing DoNotSetFocus as this function is called after
    // FocusController::setFocusedElement() and we don't want to change the
    // focus to a new Element.
    frame->Selection().SetSelection(
        RuntimeEnabledFeatures::RemoveVisibleSelectionInDOMSelectionEnabled()
            ? CreateVisibleSelection(
                  SelectionInDOMTree::Builder()
                      .Collapse(FirstPositionInOrBeforeNode(*this))
                      .Build())
                  .AsSelection()
            : SelectionInDOMTree::Builder()
                  .Collapse(FirstPositionInOrBeforeNode(*this))
                  .Build(),
        SetSelectionOptions::Builder()
            .SetShouldCloseTyping(true)
            .SetShouldClearTypingStyle(true)
            .SetDoNotSetFocus(true)
            .Build());
    if (!options->preventScroll()) {
      frame->Selection().RevealSelection();
    }
  } else if (GetLayoutObject() &&
             !GetLayoutObject()->IsLayoutEmbeddedContent()) {
    if (!options->preventScroll()) {
      auto params = scroll_into_view_util::CreateScrollIntoViewParams();

      // It's common to have menus and list controls that have items slightly
      // overflowing horizontally but the control isn't horizontally
      // scrollable. Navigating through such a list should make sure items are
      // vertically fully visible but avoid horizontal changes. This mostly
      // matches behavior in WebKit and Gecko (though, the latter has the
      // same behavior vertically) and there's some UA-defined wiggle room in
      // the spec for the scrollIntoViewOptions from focus:
      // https://html.spec.whatwg.org/#dom-focus.
      params->align_x->rect_partial =
          mojom::blink::ScrollAlignment::Behavior::kNoScroll;

      scroll_into_view_util::ScrollRectToVisible(*GetLayoutObject(),
                                                 BoundingBoxForScrollIntoView(),
                                                 std::move(params));
    }
  }
}

void Element::blur() {
  CancelSelectionAfterLayout();
  if (AdjustedFocusedElementInTreeScope() == this) {
    Document& doc = GetDocument();
    if (doc.GetPage()) {
      doc.GetPage()->GetFocusController().SetFocusedElement(nullptr,
                                                            doc.GetFrame());
      if (doc.GetFrame()) {
        doc.GetPage()->GetChromeClient().ClearKeyboardTriggeredTooltip(
            *doc.GetFrame());
      }
    } else {
      doc.ClearFocusedElement();
    }
  }
}

bool Element::SupportsSpatialNavigationFocus() const {
  // This function checks whether the element satisfies the extended criteria
  // for the element to be focusable, introduced by spatial navigation feature,
  // i.e. checks if click or keyboard event handler is specified.
  // This is the way to make it possible to navigate to (focus) elements
  // which web designer meant for being active (made them respond to click
  // events).
  if (!IsSpatialNavigationEnabled(GetDocument().GetFrame())) {
    return false;
  }

  return HasSpatialNavigationFocusHeuristics();
}

bool Element::HasSpatialNavigationFocusHeuristics() const {
  if (!GetLayoutObject()) {
    return false;
  }

  if (HasJSBasedEventListeners(event_type_names::kClick) ||
      HasJSBasedEventListeners(event_type_names::kKeydown) ||
      HasJSBasedEventListeners(event_type_names::kKeypress) ||
      HasJSBasedEventListeners(event_type_names::kKeyup) ||
      HasJSBasedEventListeners(event_type_names::kMouseover) ||
      HasJSBasedEventListeners(event_type_names::kMouseenter)) {
    return true;
  }

  // Some web apps use click-handlers to react on clicks within rects that are
  // styled with {cursor: pointer}. Such rects *look* clickable so they probably
  // are. Here we make Hand-trees' tip, the first (biggest) node with {cursor:
  // pointer}, navigable because users shouldn't need to navigate through every
  // sub element that inherit this CSS.
  if (GetComputedStyle()->Cursor() == ECursor::kPointer) {
    bool cursor_was_inherited = false;
    if (RuntimeEnabledFeatures::SpatNavUsesCursorInheritanceEnabled()) {
      cursor_was_inherited = GetComputedStyle()->CursorIsInherited();
    } else {
      cursor_was_inherited =
          ParentComputedStyle() &&
          ParentComputedStyle()->Cursor() == ECursor::kPointer;
    }

    if (!cursor_was_inherited) {
      return true;
    }
  }

  if (!IsSVGElement()) {
    return false;
  }
  return (HasEventListeners(event_type_names::kFocus) ||
          HasEventListeners(event_type_names::kBlur) ||
          HasEventListeners(event_type_names::kFocusin) ||
          HasEventListeners(event_type_names::kFocusout));
}

bool Element::CanBeKeyboardFocusableScroller(
    UpdateBehavior update_behavior) const {
  // A node is scrollable depending on its layout size. As such, it is important
  // to have up to date style and layout before calling IsScrollableNode.
  // However, some lifecycle stages don't allow update here so we use
  // UpdateBehavior to guard this behavior.
  switch (update_behavior) {
    case UpdateBehavior::kAssertNoLayoutUpdates:
      CHECK(!GetDocument().NeedsLayoutTreeUpdate());
      [[fallthrough]];
    case UpdateBehavior::kStyleAndLayout:
      GetDocument().UpdateStyleAndLayoutForNode(this,
                                                DocumentUpdateReason::kFocus);
      break;
    case UpdateBehavior::kNoneForAccessibility:
      if (DisplayLockUtilities::IsDisplayLockedPreventingPaint(this, true)) {
        return false;
      }
      break;
    case UpdateBehavior::kNoneForFocusManagement:
      DCHECK(!DisplayLockUtilities::IsDisplayLockedPreventingPaint(this));
      break;
  }
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      GetDocument().Lifecycle());
  return IsScrollableNode(this);
}

bool Element::ContainsKeyboardFocusableElementsSlow(
    UpdateBehavior update_behavior) const {
  for (const Node* node = FlatTreeTraversal::FirstChild(*this); node;
       node = FlatTreeTraversal::Next(*node, this)) {
    auto* element = DynamicTo<Element>(node);
    if (!element) {
      continue;
    }
    if (element->IsKeyboardFocusableSlow(update_behavior)) {
      return true;
    }
  }
  return false;
}

// This can be slow, because it can require a tree walk. It might be
// a good idea to cache this bit on the element to avoid having to
// recompute it. That would require marking that bit dirty whenever
// a node in the subtree was mutated, or when styles for the subtree
// were recomputed.
bool Element::IsKeyboardFocusableScroller(
    UpdateBehavior update_behavior) const {
  DCHECK(
      CanBeKeyboardFocusableScroller(UpdateBehavior::kAssertNoLayoutUpdates));
  // This condition is to avoid clearing the focus in the middle of a
  // keyboard focused scrolling event. If the scroller is currently focused,
  // then let it continue to be focused even if focusable children are added.
  if (GetDocument().FocusedElement() == this) {
    return true;
  }

  // If the scroller already contains something focusable, then we return false,
  // since it's not a "special" keyboard focusable scroller.
  return !ContainsKeyboardFocusableElementsSlow(update_behavior);
}

void Element::ShowInterestNow() {
  DCHECK(RuntimeEnabledFeatures::HTMLInterestForAttributeEnabled());
  InterestGained(InterestForElement());
}

void Element::LoseInterestNow(InterestLostCancelable cancelable,
                              InterestLostPopoverBehavior behavior) {
  DCHECK(RuntimeEnabledFeatures::HTMLInterestForAttributeEnabled());
  Element* target = InterestForElement();
  DCHECK_EQ(GetInvokerData()->ActiveInterestTarget(), target);
  DCHECK_EQ(GetInterestState(), InterestState::kFullInterest);
  InterestLost(target, cancelable, behavior);
}

// static
void Element::LoseInterestInAllElements(Document& document) {
  // Make a copy, in case events change the list.
  HeapLinkedHashSet<Member<Element>> elements = document.ElementsWithInterest();
  // For each element source in document's active interest sources set, in
  // reverse order:
  for (auto& element : base::Reversed(elements)) {
    if (auto* target = element->InterestForElement()) {
      // 1. Lose interest in source given source's active interest target.
      element->InterestLost(target, InterestLostCancelable::kNotCancelable);
      // 2. If document is not fully active, then return.
      if (!document.IsActive()) {
        return;
      }
    }
  }
}

bool Element::IsKeyboardFocusableSlow(UpdateBehavior update_behavior) const {
  FocusableState focusable_state = Element::IsFocusableState(update_behavior);
  if (focusable_state == FocusableState::kNotFocusable) {
    return false;
  }

  // If the element has a tabindex, then that determines keyboard
  // focusability.
  if (HasElementFlag(ElementFlags::kTabIndexWasSetExplicitly)) {
    return GetIntegralAttribute(html_names::kTabindexAttr, 0) >= 0;
  }
  // If the element is only potentially focusable because it *might* be a
  // keyboard-focusable scroller, then check whether it actually is.
  if (focusable_state == FocusableState::kKeyboardFocusableScroller) {
    return IsKeyboardFocusableScroller(update_behavior);
  }
  // Otherwise, if the element is focusable, then it should be keyboard-
  // focusable.
  DCHECK_EQ(focusable_state, FocusableState::kFocusable);
  return true;
}

bool Element::IsMouseFocusable(UpdateBehavior update_behavior) const {
  FocusableState focusable_state = Element::IsFocusableState(update_behavior);
  if (focusable_state == FocusableState::kNotFocusable) {
    return false;
  }
  // Any element with tabindex (regardless of its value) is mouse focusable.
  if (HasElementFlag(ElementFlags::kTabIndexWasSetExplicitly)) {
    return true;
  }
  DCHECK_EQ(tabIndex(), DefaultTabIndex());
  // If the element's default tabindex is >=0, it should be click focusable.
  if (DefaultTabIndex() >= 0) {
    return true;
  }
  // If the element is only potentially focusable because it might be a
  // keyboard-focusable scroller, then it should not be mouse focusable.
  if (focusable_state == FocusableState::kKeyboardFocusableScroller) {
    return false;
  }
  DCHECK_EQ(focusable_state, FocusableState::kFocusable);
  return true;
}

bool Element::IsClickableFormControlNode() const {
  auto* form_control_element = DynamicTo<HTMLFormControlElement>(this);
  if (form_control_element && form_control_element->FormControlType() !=
                                  mojom::blink::FormControlType::kFieldset) {
    return true;
  }
  return false;
}

bool Element::HasTabIndexWasSetExplicitly() const {
  return HasElementFlag(ElementFlags::kTabIndexWasSetExplicitly);
}

bool Element::IsFocusable(UpdateBehavior update_behavior) const {
  return IsFocusableState(update_behavior) != FocusableState::kNotFocusable;
}

FocusableState Element::IsFocusableState(UpdateBehavior update_behavior) const {
  if (!isConnected() || !IsFocusableStyle(update_behavior)) {
    return FocusableState::kNotFocusable;
  }
  return SupportsFocus(update_behavior);
}

FocusableState Element::SupportsFocus(UpdateBehavior update_behavior) const {
  // SupportsFocus must return true when the element is editable, or else
  // it won't be focusable. Furthermore, supportsFocus cannot just return true
  // always or else tabIndex() will change for all HTML elements.
  if (IsShadowHostWithDelegatesFocus()) {
    return FocusableState::kNotFocusable;
  }
  if (HasElementFlag(ElementFlags::kTabIndexWasSetExplicitly) ||
      IsRootEditableElementWithCounting(*this) ||
      IsScrollMarkerPseudoElement() || SupportsSpatialNavigationFocus()) {
    return FocusableState::kFocusable;
  }
  if (CanBeKeyboardFocusableScroller(update_behavior)) {
    return FocusableState::kKeyboardFocusableScroller;
  }
  return FocusableState::kNotFocusable;
}

bool Element::IsAutofocusable() const {
  // https://html.spec.whatwg.org/C/#global-attributes
  // https://svgwg.org/svg2-draft/struct.html#autofocusattribute
  return (IsHTMLElement() || IsSVGElement()) &&
         FastHasAttribute(html_names::kAutofocusAttr);
}

// This is used by FrameSelection to denote when the active-state of the page
// has changed independent of the focused element changing.
void Element::FocusStateChanged() {
  // If we're just changing the window's active state and the focused node has
  // no layoutObject we can just ignore the state change.
  if (!GetLayoutObject()) {
    return;
  }

  StyleChangeType change_type =
      GetComputedStyle()->HasPseudoElementStyle(kPseudoIdFirstLetter)
          ? kSubtreeStyleChange
          : kLocalStyleChange;
  SetNeedsStyleRecalc(
      change_type,
      StyleChangeReasonForTracing::CreateWithExtraData(
          style_change_reason::kPseudoClass, style_change_extra_data::g_focus));

  PseudoStateChanged(CSSSelector::kPseudoFocus);

  InvalidateIfHasEffectiveAppearance();
  FocusVisibleStateChanged();
  FocusWithinStateChanged();
}

void Element::FocusVisibleStateChanged() {
  StyleChangeType change_type =
      GetComputedStyle()->HasPseudoElementStyle(kPseudoIdFirstLetter)
          ? kSubtreeStyleChange
          : kLocalStyleChange;
  SetNeedsStyleRecalc(change_type,
                      StyleChangeReasonForTracing::CreateWithExtraData(
                          style_change_reason::kPseudoClass,
                          style_change_extra_data::g_focus_visible));

  PseudoStateChanged(CSSSelector::kPseudoFocusVisible);
}

void Element::ActiveViewTransitionStateChanged() {
  SetNeedsStyleRecalc(kLocalStyleChange,
                      StyleChangeReasonForTracing::CreateWithExtraData(
                          style_change_reason::kPseudoClass,
                          style_change_extra_data::g_active_view_transition));
  PseudoStateChanged(CSSSelector::kPseudoActiveViewTransition);
}

void Element::ActiveViewTransitionTypeStateChanged() {
  SetNeedsStyleRecalc(
      kLocalStyleChange,
      StyleChangeReasonForTracing::CreateWithExtraData(
          style_change_reason::kPseudoClass,
          style_change_extra_data::g_active_view_transition_type));
  PseudoStateChanged(CSSSelector::kPseudoActiveViewTransitionType);
}

void Element::OverscrollTargetStateChanged() {
  SetNeedsStyleRecalc(kLocalStyleChange,
                      StyleChangeReasonForTracing::CreateWithExtraData(
                          style_change_reason::kPseudoClass,
                          style_change_extra_data::g_overscroll_target));
  PseudoStateChanged(CSSSelector::kPseudoOverscrollTarget);
}

void Element::FocusWithinStateChanged() {
  if (GetComputedStyle() && GetComputedStyle()->AffectedByFocusWithin()) {
    StyleChangeType change_type =
        GetComputedStyle()->HasPseudoElementStyle(kPseudoIdFirstLetter)
            ? kSubtreeStyleChange
            : kLocalStyleChange;
    SetNeedsStyleRecalc(change_type,
                        StyleChangeReasonForTracing::CreateWithExtraData(
                            style_change_reason::kPseudoClass,
                            style_change_extra_data::g_focus_within));
  }
  PseudoStateChanged(CSSSelector::kPseudoFocusWithin);
}

void Element::SetHasFocusWithinUpToAncestor(bool has_focus_within,
                                            Element* ancestor,
                                            bool need_snap_container_search) {
  bool reached_ancestor = false;
  for (Element* element = this;
       element && (need_snap_container_search || !reached_ancestor);
       element = FlatTreeTraversal::ParentElement(*element)) {
    if (!reached_ancestor && element != ancestor) {
      element->SetHasFocusWithin(has_focus_within);
      element->FocusWithinStateChanged();
    }
    // If |ancestor| or any of its ancestors is a snap container, that snap
    // container needs to know which one of its descendants newly gained or lost
    // focus even if its own HasFocusWithin state has not changed.
    if (element != this && need_snap_container_search) {
      if (const LayoutBox* box = element->GetLayoutBoxForScrolling()) {
        if (box->Style() && !box->Style()->GetScrollSnapType().is_none) {
          // TODO(crbug.com/340983092): We should be able to just call
          // LocalFrameView::AddPendingSnapUpdate, but that results in a snap
          // which cancels ongoing scroll animations.
          // UpdateFocusDataForSnapAreas should be considered a temporary
          // workaround until the linked bug is addressed.
          box->GetScrollableArea()->UpdateFocusDataForSnapAreas();
        }
      }
    }
    reached_ancestor |= element == ancestor;
  }
}

bool Element::IsClickableControl(Node* node) {
  auto* element = DynamicTo<Element>(node);
  if (!element) {
    return false;
  }
  if (element->IsFormControlElement()) {
    return true;
  }
  Element* host = element->OwnerShadowHost();
  if (host && host->IsFormControlElement()) {
    return true;
  }
  while (node && this != node) {
    if (node->HasActivationBehavior()) {
      return true;
    }
    node = node->ParentOrShadowHostNode();
  }
  return false;
}

bool Element::ActivateDisplayLockIfNeeded(DisplayLockActivationReason reason) {
  if (!GetDocument().GetDisplayLockDocumentState().HasActivatableLocks()) {
    return false;
  }

  HeapVector<Member<Element>> activatable_targets;
  for (Node& ancestor : FlatTreeTraversal::InclusiveAncestorsOf(*this)) {
    auto* ancestor_element = DynamicTo<Element>(ancestor);
    if (!ancestor_element) {
      continue;
    }
    if (auto* context = ancestor_element->GetDisplayLockContext()) {
      // If any of the ancestors is not activatable for the given reason, we
      // can't activate.
      if (context->IsLocked() && !context->IsActivatable(reason)) {
        return false;
      }
      activatable_targets.push_back(ancestor_element);
    }
  }

  bool activated = false;
  for (const auto& target : activatable_targets) {
    if (auto* context = target->GetDisplayLockContext()) {
      if (context->ShouldCommitForActivation(reason)) {
        activated = true;
        context->CommitForActivation(reason);
      }
    }
  }
  return activated;
}

void Element::SetIsAdRelated() {
  DCHECK(!IsA<HTMLFrameOwnerElement>(this));

  EnsureElementRareData().EnsureDisplayAdElementMonitor(this);
}

bool Element::IsAdRelated() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetDisplayAdElementMonitor();
  }
  return false;
}

bool Element::ShouldHighlightAd() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    if (const DisplayAdElementMonitor* monitor =
            data->GetDisplayAdElementMonitor()) {
      return monitor->ShouldHighlight();
    }
  }
  return false;
}

bool Element::HasUndoStack() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->HasUndoStack();
  }
  return false;
}

void Element::SetHasUndoStack(bool value) {
  EnsureElementRareData().SetHasUndoStack(value);
}

void Element::SetPseudoElementStylesChangeCounters(bool value) {
  EnsureElementRareData().SetPseudoElementStylesChangeCounters(value);
}

ColumnPseudoElement* Element::GetOrCreateColumnPseudoElementIfNeeded(
    wtf_size_t index,
    const PhysicalRect& column_rect) {
  if (const ComputedStyle* style = GetComputedStyle();
      !style || !style->HasPseudoElementStyle(kPseudoIdColumn)) {
    return nullptr;
  }
  ElementRareDataVector& data = EnsureElementRareData();
  ColumnPseudoElement* column_pseudo_element =
      data.GetColumnPseudoElement(index);
  if (!column_pseudo_element) {
    column_pseudo_element = MakeGarbageCollected<ColumnPseudoElement>(
        /*originating_element=*/this, index);
    data.AddColumnPseudoElement(*column_pseudo_element);
    const ComputedStyle* style =
        column_pseudo_element->CustomStyleForLayoutObject(
            StyleRecalcContext::FromPseudoElementAncestors(*this,
                                                           kPseudoIdColumn));
    if (!style) {
      style = &GetDocument().GetStyleResolver().InitialStyle();
    }
    column_pseudo_element->SetComputedStyle(style);
    column_pseudo_element->InsertedInto(*this);
    probe::PseudoElementCreated(column_pseudo_element);
  }
  column_pseudo_element->SetColumnRect(column_rect);

  if (!column_pseudo_element->GetComputedStyle()->CanGeneratePseudoElement(
          kPseudoIdScrollMarker)) {
    return column_pseudo_element;
  }

  auto* scroll_marker = To<ScrollMarkerPseudoElement>(
      column_pseudo_element->GetPseudoElement(kPseudoIdScrollMarker));
  if (!scroll_marker) {
    scroll_marker =
        MakeGarbageCollected<ScrollMarkerPseudoElement>(column_pseudo_element);
    const ComputedStyle* scroll_marker_style =
        scroll_marker->CustomStyleForLayoutObject(
            StyleRecalcContext::FromPseudoElementAncestors(
                *column_pseudo_element, kPseudoIdScrollMarker));
    if (!PseudoElementLayoutObjectIsNeeded(kPseudoIdScrollMarker,
                                           scroll_marker_style, this)) {
      scroll_marker->Dispose();
      return column_pseudo_element;
    }
    scroll_marker->SetComputedStyle(scroll_marker_style);
    column_pseudo_element->EnsureElementRareData().SetPseudoElement(
        kPseudoIdScrollMarker, scroll_marker);
    scroll_marker->InsertedInto(*column_pseudo_element);
    probe::PseudoElementCreated(scroll_marker);
  }

  return column_pseudo_element;
}

const ColumnPseudoElementsVector* Element::GetColumnPseudoElements() const {
  ElementRareDataVector* data = GetElementRareData();
  if (!data) {
    return nullptr;
  }
  return data->GetColumnPseudoElements();
}

void Element::ClearColumnPseudoElements(wtf_size_t to_keep) {
  ElementRareDataVector* data = GetElementRareData();
  if (!data) {
    return;
  }
  wtf_size_t idx = 0u;
  if (const ColumnPseudoElementsVector* column_pseudo_elements =
          data->GetColumnPseudoElements()) {
    for (PseudoElement* column_pseudo_element : *column_pseudo_elements) {
      if (++idx > to_keep) {
        if (ElementRareDataVector* column_data =
                column_pseudo_element->GetElementRareData()) {
          column_data->ClearPseudoElements();
        }
      }
    }
  }
  data->ClearColumnPseudoElements(to_keep);
}

const OverscrollAreaParentPseudoElementsVector*
Element::GetOverscrollAreaParentPseudoElements() const {
  ElementRareDataVector* data = GetElementRareData();
  if (!data) {
    return nullptr;
  }
  return data->GetOverscrollAreaParentPseudoElements();
}

void Element::SetScrollbarPseudoElementStylesDependOnFontMetrics(bool value) {
  EnsureElementRareData().SetScrollbarPseudoElementStylesDependOnFontMetrics(
      value);
}

bool Element::HasBeenExplicitlyScrolled() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->HasBeenExplicitlyScrolled();
  }
  return false;
}

void Element::SetHasBeenExplicitlyScrolled() {
  EnsureElementRareData().SetHasBeenExplicitlyScrolled();
}

bool Element::AffectedBySubjectHas() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->AffectedBySubjectHas();
  }
  return false;
}

void Element::SetAffectedByStartingStyles() {
  EnsureElementRareData().SetAffectedByStartingStyles();
}

bool Element::AffectedByStartingStyles() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->AffectedByStartingStyles();
  }
  return false;
}

void Element::SetAffectedBySubjectHas() {
  EnsureElementRareData().SetAffectedBySubjectHas();
}

bool Element::AffectedByNonSubjectHas() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->AffectedByNonSubjectHas();
  }
  return false;
}

void Element::SetAffectedByNonSubjectHas() {
  EnsureElementRareData().SetAffectedByNonSubjectHas();
}

bool Element::AncestorsOrAncestorSiblingsAffectedByHas() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->AncestorsOrAncestorSiblingsAffectedByHas();
  }
  return false;
}

void Element::SetAncestorsOrAncestorSiblingsAffectedByHas() {
  EnsureElementRareData().SetAncestorsOrAncestorSiblingsAffectedByHas();
}

unsigned Element::GetSiblingsAffectedByHasFlags() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetSiblingsAffectedByHasFlags();
  }
  return false;
}

bool Element::HasSiblingsAffectedByHasFlags(unsigned flags) const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->HasSiblingsAffectedByHasFlags(flags);
  }
  return false;
}

void Element::SetSiblingsAffectedByHasFlags(unsigned flags) {
  EnsureElementRareData().SetSiblingsAffectedByHasFlags(flags);
}

bool Element::AffectedByPseudoInHas() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->AffectedByPseudoInHas();
  }
  return false;
}

void Element::SetAffectedByPseudoInHas() {
  EnsureElementRareData().SetAffectedByPseudoInHas();
}

bool Element::AncestorsOrSiblingsAffectedByHoverInHas() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->AncestorsOrSiblingsAffectedByHoverInHas();
  }
  return false;
}

void Element::SetAncestorsOrSiblingsAffectedByHoverInHas() {
  EnsureElementRareData().SetAncestorsOrSiblingsAffectedByHoverInHas();
}

bool Element::AncestorsOrSiblingsAffectedByActiveInHas() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->AncestorsOrSiblingsAffectedByActiveInHas();
  }
  return false;
}

void Element::SetAncestorsOrSiblingsAffectedByActiveInHas() {
  EnsureElementRareData().SetAncestorsOrSiblingsAffectedByActiveInHas();
}

bool Element::AncestorsOrSiblingsAffectedByFocusInHas() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->AncestorsOrSiblingsAffectedByFocusInHas();
  }
  return false;
}

void Element::SetAncestorsOrSiblingsAffectedByFocusInHas() {
  EnsureElementRareData().SetAncestorsOrSiblingsAffectedByFocusInHas();
}

bool Element::AncestorsOrSiblingsAffectedByFocusVisibleInHas() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->AncestorsOrSiblingsAffectedByFocusVisibleInHas();
  }
  return false;
}

void Element::SetAncestorsOrSiblingsAffectedByFocusVisibleInHas() {
  EnsureElementRareData().SetAncestorsOrSiblingsAffectedByFocusVisibleInHas();
}

bool Element::AffectedByLogicalCombinationsInHas() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->AffectedByLogicalCombinationsInHas();
  }
  return false;
}

void Element::SetAffectedByLogicalCombinationsInHas() {
  EnsureElementRareData().SetAffectedByLogicalCombinationsInHas();
}

bool Element::AffectedByMultipleHas() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->AffectedByMultipleHas();
  }
  return false;
}

void Element::SetAffectedByMultipleHas() {
  EnsureElementRareData().SetAffectedByMultipleHas();
}

bool Element::IsFocusedElementInDocument() const {
  return this == GetDocument().FocusedElement();
}

Element* Element::AdjustedFocusedElementInTreeScope() const {
  return IsInTreeScope() ? GetTreeScope().AdjustedFocusedElement() : nullptr;
}

bool Element::DispatchFocusEvent(Element* old_focused_element,
                                 mojom::blink::FocusType type,
                                 InputDeviceCapabilities* source_capabilities) {
  Document& document = GetDocument();
  if (DispatchEvent(*FocusEvent::Create(
          event_type_names::kFocus, Event::Bubbles::kNo, document.domWindow(),
          0, old_focused_element, source_capabilities)) !=
      DispatchEventResult::kNotCanceled) {
    return false;
  }
  return true;
}

void Element::DispatchBlurEvent(Element* new_focused_element,
                                mojom::blink::FocusType type,
                                InputDeviceCapabilities* source_capabilities) {
  DispatchEvent(*FocusEvent::Create(
      event_type_names::kBlur, Event::Bubbles::kNo, GetDocument().domWindow(),
      0, new_focused_element, source_capabilities));
}

void Element::DispatchFocusInEvent(
    const AtomicString& event_type,
    Element* old_focused_element,
    mojom::blink::FocusType,
    InputDeviceCapabilities* source_capabilities) {
#if DCHECK_IS_ON()
  DCHECK(!EventDispatchForbiddenScope::IsEventDispatchForbidden());
#endif
  DCHECK(event_type == event_type_names::kFocusin ||
         event_type == event_type_names::kDOMFocusIn);
  DispatchScopedEvent(*FocusEvent::Create(
      event_type, Event::Bubbles::kYes, GetDocument().domWindow(), 0,
      old_focused_element, source_capabilities));
}

void Element::DispatchFocusOutEvent(
    const AtomicString& event_type,
    Element* new_focused_element,
    InputDeviceCapabilities* source_capabilities) {
#if DCHECK_IS_ON()
  DCHECK(!EventDispatchForbiddenScope::IsEventDispatchForbidden());
#endif
  DCHECK(event_type == event_type_names::kFocusout ||
         event_type == event_type_names::kDOMFocusOut);
  DispatchScopedEvent(*FocusEvent::Create(
      event_type, Event::Bubbles::kYes, GetDocument().domWindow(), 0,
      new_focused_element, source_capabilities));
}

String Element::GetInnerHTMLString() const {
  return CreateMarkup(this, kChildrenOnly);
}

String Element::GetOuterHTMLString() const {
  return CreateMarkup(this);
}

V8UnionStringLegacyNullToEmptyStringOrTrustedHTML* Element::innerHTML() const {
  return MakeGarbageCollected<
      V8UnionStringLegacyNullToEmptyStringOrTrustedHTML>(GetInnerHTMLString());
}

V8UnionStringLegacyNullToEmptyStringOrTrustedHTML* Element::outerHTML() const {
  return MakeGarbageCollected<
      V8UnionStringLegacyNullToEmptyStringOrTrustedHTML>(GetOuterHTMLString());
}

void Element::SetInnerHTMLInternal(
    const String& html,
    ParseDeclarativeShadowRoots parse_declarative_shadows,
    ForceHtml force_html,
    std::variant<std::monostate, SetHTMLOptions*, SetHTMLUnsafeOptions*>
        options,
    ExceptionState& exception_state) {
  if (html.empty() && !HasNonInBodyInsertionMode()) {
    setTextContent(html);
  } else {
    // Use null registry to create fragment if the context element is a
    // template element as the container of the document fragment will be a
    // document fragment without browsing context.
    auto* template_element = DynamicTo<HTMLTemplateElement>(*this);
    CustomElementRegistry* registry = GetDocument().customElementRegistry();
    if (RuntimeEnabledFeatures::ScopedCustomElementRegistryEnabled()) {
      registry = template_element ? nullptr : customElementRegistry();
    }
    const ParserContentPolicy content_policy =
        (RuntimeEnabledFeatures::SetHTMLCanRunScriptsEnabled() &&
         std::holds_alternative<SetHTMLUnsafeOptions*>(options) &&
         std::get<SetHTMLUnsafeOptions*>(options)->runScripts())
            ? kAllowScriptingContentAndDoNotMarkAlreadyStarted
            : kAllowScriptingContent;

    if (DocumentFragment* fragment = CreateFragmentForInnerOuterHTML(
            html, this, content_policy, parse_declarative_shadows, force_html,
            std::holds_alternative<std::monostate>(options)
                ? ForceInertTemplate::kDontForce
                : ForceInertTemplate::kForce,
            registry, exception_state)) {
      if (std::holds_alternative<SetHTMLOptions*>(options)) {
        CHECK(RuntimeEnabledFeatures::SanitizerAPIEnabled());
        SanitizerAPI::SanitizeSafeInternal(this, fragment,
                                           std::get<SetHTMLOptions*>(options),
                                           exception_state);
      } else if (std::holds_alternative<SetHTMLUnsafeOptions*>(options)) {
        CHECK(RuntimeEnabledFeatures::SanitizerAPIEnabled());
        SanitizerAPI::SanitizeUnsafeInternal(
            this, fragment, std::get<SetHTMLUnsafeOptions*>(options),
            exception_state);
      } else {
        CHECK(std::holds_alternative<std::monostate>(options));
        // No options; nothing to do.
      }
      ContainerNode* container = this;
      bool swap_dom_parts{false};
      if (template_element) {
        container = template_element->content();
        swap_dom_parts =
            RuntimeEnabledFeatures::DOMPartsAPIEnabled() &&
            template_element->hasAttribute(html_names::kParsepartsAttr);
      }
      ReplaceChildrenWithFragment(container, fragment, exception_state);
      if (swap_dom_parts &&
          !RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled()) {
        // Move the parts list over to the template's content document's
        // DocumentPartRoot.
        To<DocumentFragment>(*container)
            .getPartRoot()
            .SwapPartsList(fragment->getPartRoot());
      }
    }
  }
}

void Element::SetInnerHTMLWithoutTrustedTypes(const String& html,
                                              ExceptionState& exception_state) {
  SetInnerHTMLInternal(html, ParseDeclarativeShadowRoots::kDontParse,
                       ForceHtml::kDontForce, std::monostate{},
                       exception_state);
}

void Element::setInnerHTML(
    const V8UnionStringLegacyNullToEmptyStringOrTrustedHTML* html,
    ExceptionState& exception_state) {
  probe::BreakableLocation(GetExecutionContext(), "Element.setInnerHTML");
  String compliant_html = TrustedTypesCheckForHTML(
      html, GetExecutionContext(), trusted_types_names::kElement,
      trusted_types_names::kInnerHTML, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  SetInnerHTMLWithoutTrustedTypes(compliant_html, exception_state);
}

void Element::SetOuterHTMLWithoutTrustedTypes(const String& html,
                                              ExceptionState& exception_state) {
  Node* p = parentNode();
  if (!p) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNoModificationAllowedError,
        "This element has no parent node.");
    return;
  }

  auto* parent = DynamicTo<Element>(p);
  if (!parent) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNoModificationAllowedError,
        StrCat({"This element's parent is of type '", p->nodeName(),
                "', which is not an element node."}));
    return;
  }

  Node* prev = previousSibling();
  Node* next = nextSibling();

  DocumentFragment* fragment = CreateFragmentForInnerOuterHTML(
      html, parent, kAllowScriptingContent,
      ParseDeclarativeShadowRoots::kDontParse, ForceHtml::kDontForce,
      ForceInertTemplate::kDontForce,
      RuntimeEnabledFeatures::ScopedCustomElementRegistryEnabled()
          ? customElementRegistry()
          : GetDocument().customElementRegistry(),
      exception_state);
  if (exception_state.HadException()) {
    return;
  }

  parent->ReplaceChild(fragment, this, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  Node* node = next ? next->previousSibling() : nullptr;
  if (auto* text = DynamicTo<Text>(node)) {
    MergeWithNextTextNode(text, exception_state);
    if (exception_state.HadException()) {
      return;
    }
  }

  if (auto* prev_text = DynamicTo<Text>(prev)) {
    MergeWithNextTextNode(prev_text, exception_state);
    if (exception_state.HadException()) {
      return;
    }
  }
}

void Element::setOuterHTML(
    const V8UnionStringLegacyNullToEmptyStringOrTrustedHTML* html,
    ExceptionState& exception_state) {
  String compliant_html = TrustedTypesCheckForHTML(
      html, GetExecutionContext(), trusted_types_names::kElement,
      trusted_types_names::kOuterHTML, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  SetOuterHTMLWithoutTrustedTypes(compliant_html, exception_state);
}

// Step 4 of http://domparsing.spec.whatwg.org/#insertadjacenthtml()
Node* Element::InsertAdjacent(const String& where,
                              Node* new_child,
                              ExceptionState& exception_state) {
  if (EqualIgnoringASCIICase(where, "beforeBegin")) {
    if (ContainerNode* parent = parentNode()) {
      parent->InsertBefore(new_child, this, exception_state);
      if (!exception_state.HadException()) {
        return new_child;
      }
    }
    return nullptr;
  }

  if (EqualIgnoringASCIICase(where, "afterBegin")) {
    InsertBefore(new_child, firstChild(), exception_state);
    return exception_state.HadException() ? nullptr : new_child;
  }

  if (EqualIgnoringASCIICase(where, "beforeEnd")) {
    AppendChild(new_child, exception_state);
    return exception_state.HadException() ? nullptr : new_child;
  }

  if (EqualIgnoringASCIICase(where, "afterEnd")) {
    if (ContainerNode* parent = parentNode()) {
      parent->InsertBefore(new_child, nextSibling(), exception_state);
      if (!exception_state.HadException()) {
        return new_child;
      }
    }
    return nullptr;
  }

  exception_state.ThrowDOMException(
      DOMExceptionCode::kSyntaxError,
      StrCat({"The value provided ('", where,
              "') is not one of 'beforeBegin', 'afterBegin', 'beforeEnd', or "
              "'afterEnd'."}));
  return nullptr;
}

void Element::HideNonce() {
  // This is a relatively hot codepath.  Get to the common early return as
  // fast as possible.
  const AtomicString& nonce_value = FastGetAttribute(html_names::kNonceAttr);
  if (nonce_value.empty()) {
    return;
  }
  if (GetDocument().StatePreservingAtomicMoveInProgress()) {
    return;
  }
  if (!InActiveDocument()) {
    return;
  }
  if (GetExecutionContext()
          ->GetContentSecurityPolicy()
          ->HasHeaderDeliveredPolicy()) {
    setAttribute(html_names::kNonceAttr, g_empty_atom);
  }
}

ElementIntersectionObserverData* Element::IntersectionObserverData() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->IntersectionObserverData();
  }
  return nullptr;
}

ElementIntersectionObserverData& Element::EnsureIntersectionObserverData() {
  return EnsureElementRareData().EnsureIntersectionObserverData();
}

HeapHashMap<Member<ResizeObserver>, Member<ResizeObservation>>*
Element::ResizeObserverData() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->ResizeObserverData();
  }
  return nullptr;
}

HeapHashMap<Member<ResizeObserver>, Member<ResizeObservation>>&
Element::EnsureResizeObserverData() {
  return EnsureElementRareData().EnsureResizeObserverData();
}

DisplayLockContext* Element::GetDisplayLockContextFromRareData() const {
  DCHECK(HasDisplayLockContext());
  DCHECK(GetElementRareData());
  return GetElementRareData()->GetDisplayLockContext();
}

DisplayLockContext& Element::EnsureDisplayLockContext() {
  SetHasDisplayLockContext();
  return *EnsureElementRareData().EnsureDisplayLockContext(this);
}

ContainerQueryData* Element::GetContainerQueryData() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetContainerQueryData();
  }
  return nullptr;
}

ContainerQueryEvaluator* Element::GetContainerQueryEvaluator() const {
  if (const ContainerQueryData* cq_data = GetContainerQueryData()) {
    return cq_data->GetContainerQueryEvaluator();
  }
  return nullptr;
}

ContainerQueryEvaluator& Element::EnsureContainerQueryEvaluator() {
  ContainerQueryData& data = EnsureElementRareData().EnsureContainerQueryData();
  ContainerQueryEvaluator* evaluator = data.GetContainerQueryEvaluator();
  if (!evaluator) {
    evaluator = MakeGarbageCollected<ContainerQueryEvaluator>(*this);
    data.SetContainerQueryEvaluator(evaluator);
  }
  return *evaluator;
}

StyleScopeData& Element::EnsureStyleScopeData() {
  return EnsureElementRareData().EnsureStyleScopeData();
}

StyleScopeData* Element::GetStyleScopeData() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetStyleScopeData();
  }
  return nullptr;
}

OutOfFlowData& Element::EnsureOutOfFlowData() {
  return EnsureElementRareData().EnsureOutOfFlowData();
}

OutOfFlowData* Element::GetOutOfFlowData() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetOutOfFlowData();
  }
  return nullptr;
}

bool Element::SetPendingRememberedScrollOffsets(
    const OutOfFlowData::RememberedScrollOffsets* offsets) {
  if (!offsets && !GetOutOfFlowData()) {
    return false;
  }

  return EnsureOutOfFlowData().SetPendingRememberedScrollOffsets(offsets);
}

bool Element::SkippedContainerStyleRecalc() const {
  if (const ContainerQueryData* cq_data = GetContainerQueryData()) {
    return cq_data->SkippedStyleRecalc();
  }
  return false;
}

// Step 1 of http://domparsing.spec.whatwg.org/#insertadjacenthtml()
static Node* ContextNodeForInsertion(const String& where,
                                     Element* element,
                                     ExceptionState& exception_state) {
  if (EqualIgnoringASCIICase(where, "beforeBegin") ||
      EqualIgnoringASCIICase(where, "afterEnd")) {
    Node* parent = element->parentNode();
    if (!parent || IsA<Document>(parent)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNoModificationAllowedError,
          "The element has no parent.");
      return nullptr;
    }
    return parent;
  }
  if (EqualIgnoringASCIICase(where, "afterBegin") ||
      EqualIgnoringASCIICase(where, "beforeEnd")) {
    return element;
  }
  exception_state.ThrowDOMException(
      DOMExceptionCode::kSyntaxError,
      StrCat({"The value provided ('", where,
              "') is not one of 'beforeBegin', 'afterBegin', 'beforeEnd', or "
              "'afterEnd'."}));
  return nullptr;
}

Element* Element::insertAdjacentElement(const String& where,
                                        Element* new_child,
                                        ExceptionState& exception_state) {
  Node* return_value = InsertAdjacent(where, new_child, exception_state);
  return To<Element>(return_value);
}

void Element::insertAdjacentText(const String& where,
                                 const String& text,
                                 ExceptionState& exception_state) {
  InsertAdjacent(where, GetDocument().createTextNode(text), exception_state);
}

void Element::InsertAdjacentHTMLWithoutTrustedTypes(
    const String& where,
    const String& markup,
    ExceptionState& exception_state) {
  Node* context_node = ContextNodeForInsertion(where, this, exception_state);
  if (!context_node) {
    return;
  }

  // Step 2 of http://domparsing.spec.whatwg.org/#insertadjacenthtml()
  Element* context_element;
  if (!IsA<Element>(context_node) ||
      (IsA<HTMLDocument>(context_node->GetDocument()) &&
       IsA<HTMLHtmlElement>(context_node))) {
    context_element =
        MakeGarbageCollected<HTMLBodyElement>(context_node->GetDocument());
  } else {
    context_element = To<Element>(context_node);
  }

  // Step 3 of http://domparsing.spec.whatwg.org/#insertadjacenthtml()
  DocumentFragment* fragment = CreateFragmentForInnerOuterHTML(
      markup, context_element, kAllowScriptingContent,
      ParseDeclarativeShadowRoots::kDontParse, ForceHtml::kDontForce,
      ForceInertTemplate::kDontForce,
      RuntimeEnabledFeatures::ScopedCustomElementRegistryEnabled()
          ? customElementRegistry()
          : GetDocument().customElementRegistry(),
      exception_state);
  if (!fragment) {
    return;
  }
  InsertAdjacent(where, fragment, exception_state);
}

void Element::insertAdjacentHTML(const String& where,
                                 const V8UnionStringOrTrustedHTML* html,
                                 ExceptionState& exception_state) {
  String compliant_html = TrustedTypesCheckForHTML(
      html, GetExecutionContext(), trusted_types_names::kElement,
      trusted_types_names::kInsertAdjacentHTML, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  InsertAdjacentHTMLWithoutTrustedTypes(where, compliant_html, exception_state);
}

void Element::setPointerCapture(PointerId pointer_id,
                                ExceptionState& exception_state) {
  if (GetDocument().GetFrame()) {
    if (!GetDocument().GetFrame()->GetEventHandler().IsPointerEventActive(
            pointer_id)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotFoundError,
          "No active pointer with the given id is found.");
    } else if (!isConnected() ||
               (GetDocument().GetPage() && GetDocument()
                                               .GetPage()
                                               ->GetPointerLockController()
                                               .GetElement())) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "InvalidStateError");
    } else {
      GetDocument().GetFrame()->GetEventHandler().SetPointerCapture(pointer_id,
                                                                    this);
    }
  }
}

void Element::releasePointerCapture(PointerId pointer_id,
                                    ExceptionState& exception_state) {
  if (GetDocument().GetFrame()) {
    if (!GetDocument().GetFrame()->GetEventHandler().IsPointerEventActive(
            pointer_id)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotFoundError,
          "No active pointer with the given id is found.");
    } else {
      GetDocument().GetFrame()->GetEventHandler().ReleasePointerCapture(
          pointer_id, this);
    }
  }
}

bool Element::hasPointerCapture(PointerId pointer_id) const {
  return GetDocument().GetFrame() &&
         GetDocument().GetFrame()->GetEventHandler().HasPointerCapture(
             pointer_id, this);
}

String Element::outerText() {
  // Getting outerText is the same as getting innerText, only
  // setting is different. You would think this should get the plain
  // text for the outer range, but this is wrong, <br> for instance
  // would return different values for inner and outer text by such
  // a rule, but it doesn't in WinIE, and we want to match that.
  return innerText();
}

String Element::TextFromChildren() {
  Text* first_text_node = nullptr;
  bool found_multiple_text_nodes = false;
  unsigned total_length = 0;

  for (Node* child = firstChild(); child; child = child->nextSibling()) {
    auto* child_text_node = DynamicTo<Text>(child);
    if (!child_text_node) {
      continue;
    }
    if (!first_text_node) {
      first_text_node = child_text_node;
    } else {
      found_multiple_text_nodes = true;
    }
    unsigned length = child_text_node->data().length();
    if (length > std::numeric_limits<unsigned>::max() - total_length) {
      return g_empty_string;
    }
    total_length += length;
  }

  if (!first_text_node) {
    return g_empty_string;
  }

  if (first_text_node && !found_multiple_text_nodes) {
    first_text_node->MakeParkable();
    return first_text_node->data();
  }

  StringBuilder content;
  content.ReserveCapacity(total_length);
  for (Node* child = first_text_node; child; child = child->nextSibling()) {
    auto* child_text_node = DynamicTo<Text>(child);
    if (!child_text_node) {
      continue;
    }
    content.Append(child_text_node->data());
  }

  DCHECK_EQ(content.length(), total_length);
  return content.ReleaseString();
}

const AtomicString& Element::ShadowPseudoId() const {
  if (ShadowRoot* root = ContainingShadowRoot()) {
    if (root->IsUserAgent()) {
      return FastGetAttribute(html_names::kPseudoAttr);
    }
  }
  return g_null_atom;
}

void Element::SetShadowPseudoId(const AtomicString& id) {
#if DCHECK_IS_ON()
  {
    // NOTE: This treats "cue" as kPseudoWebKitCustomElement, so "cue"
    // is allowed here.
    CSSSelector::PseudoType type =
        CSSSelectorParser::ParsePseudoType(id, false, &GetDocument());
    DCHECK(type == CSSSelector::kPseudoWebKitCustomElement ||
           type == CSSSelector::kPseudoBlinkInternalElement ||
           type == CSSSelector::kPseudoDetailsContent ||
           type == CSSSelector::kPseudoPermissionIcon ||
           id == shadow_element_names::kPickerSelect)
        << "type: " << type << ", id: " << id;
  }
#endif
  setAttribute(html_names::kPseudoAttr, id);
}

bool Element::IsInDescendantTreeOf(const Element* shadow_host) const {
  DCHECK(shadow_host);
  DCHECK(IsShadowHost(shadow_host));

  for (const Element* ancestor_shadow_host = OwnerShadowHost();
       ancestor_shadow_host;
       ancestor_shadow_host = ancestor_shadow_host->OwnerShadowHost()) {
    if (ancestor_shadow_host == shadow_host) {
      return true;
    }
  }
  return false;
}

namespace {

bool NeedsEnsureComputedStyle(Element& element) {
  const ComputedStyle* style = element.GetComputedStyle();
  return !style || style->IsEnsuredOutsideFlatTree();
}

HeapVector<Member<Element>> CollectAncestorsToEnsure(Element& element) {
  HeapVector<Member<Element>> ancestors;

  Element* ancestor = &element;
  while ((ancestor = DynamicTo<Element>(
              LayoutTreeBuilderTraversal::Parent(*ancestor)))) {
    if (!NeedsEnsureComputedStyle(*ancestor)) {
      break;
    }
    ancestors.push_back(ancestor);
  }

  return ancestors;
}

}  // namespace

const ComputedStyle* Element::EnsureComputedStyle(
    PseudoId pseudo_element_specifier,
    const AtomicString& pseudo_argument) {
  // Style computation should not be triggered when in a NoAllocationScope
  // because there is always a possibility that it could allocate something on
  // the V8 heap.
  DCHECK(ThreadState::Current()->IsAllocationAllowed());

  StyleEngine::InEnsureComputedStyleScope ensure_scope(
      GetDocument().GetStyleEngine());

  if (Element* element =
          GetStyledPseudoElement(pseudo_element_specifier, pseudo_argument)) {
    return element->EnsureComputedStyle();
  }

  if (!InActiveDocument()) {
    return nullptr;
  }

  // EnsureComputedStyle is expected to be called to forcibly compute style for
  // elements in display:none subtrees on otherwise style-clean documents. If
  // you hit this DCHECK, consider if you really need ComputedStyle for
  // display:none elements. If not, use GetComputedStyle() instead.
  // Regardless, you need to UpdateStyleAndLayoutTree() before calling
  // EnsureComputedStyle. In some cases you might be fine using GetComputedStyle
  // without updating the style, but in most cases you want a clean tree for
  // that as well.
  DCHECK(
      !GetDocument().NeedsLayoutTreeUpdateForNodeIncludingDisplayLocked(*this));

  // EnsureComputedStyle is called even for rendered elements which have a non-
  // null ComputedStyle already. Early out to avoid the expensive setup below.
  if (pseudo_element_specifier == kPseudoIdNone) {
    if (const ComputedStyle* style =
            ComputedStyle::NullifyEnsured(GetComputedStyle())) {
      return style;
    }
  }

  // Retrieve a list of (non-inclusive) ancestors that we need to ensure the
  // ComputedStyle for *before* we can ensure the ComputedStyle for this
  // element. Note that the list of ancestors can be empty if |this| is the
  // root of the display:none subtree.
  //
  // The front() element is the LayoutTreeBuilderTraversal::Parent of |this|,
  // and the back() element is the "top-most" ancestor in the chain.
  HeapVector<Member<Element>> ancestors = CollectAncestorsToEnsure(*this);

  Element* top = ancestors.empty() ? this : ancestors.back().Get();

  // Prepare the selector filter to fast reject rules.
  Element* filter_root = FlatTreeTraversal::ParentElement(*top);
  Element* document_element = top->GetDocument().documentElement();

  // The filter doesn't support rejecting rules for elements outside of the
  // flat tree.  Detect that case and disable calls to the filter until
  // https://crbug.com/831568 is fixed.
  bool is_in_flat_tree =
      top == document_element ||
      (filter_root &&
       !filter_root->ComputedStyleRef().IsEnsuredOutsideFlatTree());
  if (!is_in_flat_tree) {
    if (!RuntimeEnabledFeatures::GetComputedStyleOutsideFlatTreeEnabled()) {
      return nullptr;
    }
    filter_root = nullptr;
  }

  SelectorFilterParentScope root_scope(
      filter_root, SelectorFilterParentScope::ScopeType::kRoot);
  SelectorFilter& filter =
      top->GetDocument().GetStyleResolver().GetSelectorFilter();
  GetDocument().GetStyleEngine().UpdateViewportSize();

  // Don't call FromAncestors for elements whose parent is outside the
  // flat-tree, since those elements don't actually participate in style recalc.
  auto style_recalc_context = LayoutTreeBuilderTraversal::Parent(*top)
                                  ? StyleRecalcContext::FromAncestors(*top)
                                  : StyleRecalcContext();
  style_recalc_context.is_outside_flat_tree = !is_in_flat_tree;

  SelectorFilter::Mark mark = filter.SetMark();
  for (Element* ancestor : base::Reversed(ancestors)) {
    const ComputedStyle* style =
        ancestor->EnsureOwnComputedStyle(style_recalc_context, kPseudoIdNone);
    if (is_in_flat_tree) {
      filter.PushParent(*ancestor);
    }
    if (style->IsContainerForSizeContainerQueries()) {
      style_recalc_context.size_container = ancestor;
    }
  }

  const ComputedStyle* style = EnsureOwnComputedStyle(
      style_recalc_context, pseudo_element_specifier, pseudo_argument);

  if (is_in_flat_tree) {
    filter.PopTo(mark);
  }

  return style;
}

const ComputedStyle* Element::EnsureOwnComputedStyle(
    const StyleRecalcContext& style_recalc_context,
    PseudoId pseudo_element_specifier,
    const AtomicString& pseudo_argument) {
  // FIXME: Find and use the layoutObject from the pseudo-element instead of the
  // actual element so that the 'length' properties, which are only known by the
  // layoutObject because it did the layout, will be correct and so that the
  // values returned for the ":selection" pseudo-element will be correct.
  const ComputedStyle* element_style = GetComputedStyle();
  if (NeedsEnsureComputedStyle(*this)) {
    if (element_style && NeedsStyleRecalc()) {
      // RecalcStyle() will not traverse into connected elements outside the
      // flat tree and we may have a dirty element or ancestors if this
      // element is not in the flat tree. If we don't need a style recalc,
      // we can just reuse the ComputedStyle from the last
      // getComputedStyle(). Otherwise, we need to clear the ensured styles
      // for the uppermost dirty ancestor and all of its descendants. If
      // this element was not the uppermost dirty element, we would not end
      // up here because a dirty ancestor would have cleared the
      // ComputedStyle via EnsureComputedStyle and element_style would
      // have been null.
      GetDocument().GetStyleEngine().ClearEnsuredDescendantStyles(*this);
      element_style = nullptr;
    }
    if (!element_style) {
      StyleRecalcContext local_style_recalc_context = style_recalc_context;
      local_style_recalc_context.is_ensuring_style = true;
      const ComputedStyle* new_style = nullptr;
      // TODO(crbug.com/953707): Avoid setting inline style during
      // HTMLImageElement::CustomStyleForLayoutObject.
      if (HasCustomStyleCallbacks() && !IsA<HTMLImageElement>(*this)) {
        new_style = CustomStyleForLayoutObject(local_style_recalc_context);
      } else {
        new_style = OriginalStyleForLayoutObject(local_style_recalc_context);
      }
      element_style = new_style;
      SetComputedStyle(new_style);
    }
  }

  if (!pseudo_element_specifier) {
    return element_style;
  }

  if (pseudo_element_specifier == kPseudoIdSearchText &&
      !RuntimeEnabledFeatures::SearchTextHighlightPseudoEnabled()) {
    return nullptr;
  }

  if (pseudo_element_specifier == kPseudoIdScrollMarker &&
      !style_recalc_context
           .has_scroller_ancestor_with_scroll_marker_group_property) {
    return nullptr;
  }

  if (const ComputedStyle* pseudo_element_style =
          element_style->GetCachedPseudoElementStyle(pseudo_element_specifier,
                                                     pseudo_argument)) {
    return pseudo_element_style;
  }

  const ComputedStyle* layout_parent_style = element_style;
  if (HasDisplayContentsStyle()) {
    LayoutObject* parent_layout_object =
        LayoutTreeBuilderTraversal::ParentLayoutObject(*this);
    if (parent_layout_object) {
      layout_parent_style = parent_layout_object->Style();
    }
  }

  StyleRequest style_request;
  style_request.pseudo_id = pseudo_element_specifier;
  style_request.type = StyleRequest::kForComputedStyle;
  if (style_request.pseudo_id == kPseudoIdSearchText) {
    // getComputedStyle for ::search-text is always :not(:current);
    // see <https://github.com/w3c/csswg-drafts/issues/10297>.
    DCHECK_EQ(style_request.type, StyleRequest::kForComputedStyle);
    style_request.search_text_request = StyleRequest::kNotCurrent;
  }
  if (IsHighlightPseudoElement(pseudo_element_specifier)) {
    const ComputedStyle* highlight_element_style = nullptr;
    if (Element* parent = LayoutTreeBuilderTraversal::ParentElement(*this)) {
      highlight_element_style =
          parent->GetComputedStyle()->HighlightData().Style(
              pseudo_element_specifier, pseudo_argument);
    }
    style_request.parent_override = highlight_element_style;
    // All properties that apply to highlight pseudos are treated as inherited,
    // so we don't need to do anything special regarding display contents (see
    // https://drafts.csswg.org/css-pseudo/#highlight-cascade).
    style_request.layout_parent_override = highlight_element_style;
    style_request.originating_element_style = element_style;
  } else {
    style_request.parent_override = element_style;
    style_request.layout_parent_override = layout_parent_style;
  }
  style_request.pseudo_argument = pseudo_argument;

  StyleRecalcContext child_recalc_context = style_recalc_context;
  child_recalc_context.is_ensuring_style = true;
  if (element_style->IsContainerForSizeContainerQueries() &&
      !PseudoElement::IsLayoutSiblingOfOriginatingElement(
          *this, pseudo_element_specifier)) {
    child_recalc_context.size_container = this;
  }

  const ComputedStyle* result = GetDocument().GetStyleResolver().ResolveStyle(
      this, child_recalc_context, style_request);
  DCHECK(result);
  return element_style->AddCachedPseudoElementStyle(
      result, pseudo_element_specifier, pseudo_argument);
}

bool Element::HasDisplayContentsStyle() const {
  if (const ComputedStyle* style = GetComputedStyle()) {
    return style->Display() == EDisplay::kContents;
  }
  return false;
}

bool Element::ShouldStoreComputedStyle(const ComputedStyle& style) const {
  // If we're in a locked subtree and we're a top layer element, it means that
  // we shouldn't be creating a layout object. This path can happen if we're
  // force-updating style on the locked subtree and reach this node. Note that
  // we already detached layout when this element was added to the top layer, so
  // we simply maintain the fact that it doesn't have a layout object/subtree.
  if (style.IsRenderedInTopLayer(*this) &&
      DisplayLockUtilities::LockedAncestorPreventingPaint(*this)) {
    return false;
  }

  if (LayoutObjectIsNeeded(style)) {
    return true;
  }
  if (auto* svg_element = DynamicTo<SVGElement>(this)) {
    if (!svg_element->HasSVGParent()) {
      return false;
    }
    if (IsA<SVGStopElement>(*this)) {
      return true;
    }
  }

  // The ::picker(select) UA popover element is display:none by default because
  // it's a popover which would prevent style from being calculated on it and
  // its flat tree descendants when its not shown.
  // In the case that the picker is appearance:auto/none:
  //   We need computed style for all flat tree descendants of the picker in
  //   order to get computed style for options. These computed styles are used
  //   by InternalPopupMenu/ExternalPopupMenu for styling options in the native
  //   picker. They are also used during layout by MenuListSelectType to
  //   populate the style of its MenuListInnerElement. In order to do this,
  //   return true from this method.
  // In the case that the picker is appearance:base-select:
  //   We can't return true from this method or else it will mess with top layer
  //   animations of the popover - @starting-style won't work properly. However,
  //   we still want to know whether the picker has appearance:base-select or
  //   not in order to make changes in the accessibility tree among many other
  //   things. In order to do this, we set a bit on the select while we still
  //   have access to the computed style here.
  if (HTMLSelectElement::IsPopoverPickerElement(this)) {
    HTMLSelectElement* select = To<HTMLSelectElement>(OwnerShadowHost());
    if (const ComputedStyle* select_style = select->GetComputedStyle()) {
      // The picker isn't allowed to have base appearance unless the
      // select does too.
      bool is_base_appearance =
          SupportsBaseAppearance(style.EffectiveAppearance()) &&
          select->SupportsBaseAppearance(select_style->EffectiveAppearance());
      select->SetIsAppearanceBasePickerForDisplayNone(is_base_appearance);
      if (!is_base_appearance) {
        return true;
      }
    }
  }

  return style.Display() == EDisplay::kContents;
}

bool Element::IsInCanvasSubtree() const {
  auto* parent = ParentOrShadowHostElement();
  return parent && parent->IsCanvasOrInCanvasSubtree();
}

AtomicString Element::ComputeInheritedLanguage() const {
  const Node* n = this;
  AtomicString value;
  // The language property is inherited, so we iterate over the parents to find
  // the first language.
  do {
    if (n->IsElementNode()) {
      if (const auto* element_data = To<Element>(n)->GetElementData()) {
        AttributeCollection attributes = element_data->Attributes();
        // Spec: xml:lang takes precedence -- http://www.w3.org/TR/xhtml1/#C_7
        if (const Attribute* attribute =
                attributes.Find(xml_names::kLangAttr)) {
          value = attribute->Value();
        } else if (n->IsHTMLElement() || n->IsSVGElement()) {
          attribute = attributes.Find(html_names::kLangAttr);
          if (attribute) {
            value = attribute->Value();
          }
        }
      }
    } else if (auto* document = DynamicTo<Document>(n)) {
      // checking the MIME content-language
      value = document->ContentLanguage();
    }

    n = n->ParentOrShadowHostNode();
  } while (n && value.IsNull());

  return value;
}

Locale& Element::GetLocale() const {
  return GetDocument().GetCachedLocale(ComputeInheritedLanguage());
}

void Element::CancelSelectionAfterLayout() {
  if (GetDocument().FocusedElement() == this) {
    GetDocument().SetShouldUpdateSelectionAfterLayout(false);
  }
}

bool Element::ShouldUpdateBackdropPseudoElement(
    const StyleRecalcChange change) {
  PseudoElement* element = GetPseudoElement(PseudoId::kPseudoIdBackdrop,
                                            /* pseudo_argument */ g_null_atom);
  bool generate_pseudo = CanGeneratePseudoElement(PseudoId::kPseudoIdBackdrop);

  if (element) {
    return !generate_pseudo || change.ShouldUpdatePseudoElement(*element);
  }

  return generate_pseudo;
}

void Element::UpdateBackdropPseudoElement(
    const StyleRecalcChange change,
    const StyleRecalcContext& style_recalc_context) {
  if (!ShouldUpdateBackdropPseudoElement(change)) {
    return;
  }

  if (GetDocument().GetStyleEngine().GetInterleavingRecalcRoot() != this) {
    UpdatePseudoElement(PseudoId::kPseudoIdBackdrop, change,
                        style_recalc_context);
    return;
  }

  // We have a problem when ::backdrop appears on the interleaving container,
  // because in that case ::backdrop's LayoutObject appears before the
  // container's LayoutObject. In other words, it is too late to update
  // ::backdrop at this point. Therefore, we add a pending update and deal with
  // it in a separate pass.
  //
  // See also PostStyleUpdateScope::PseudoData::AddPendingBackdrop.
  if (PostStyleUpdateScope::PseudoData* pseudo_data =
          PostStyleUpdateScope::CurrentPseudoData()) {
    pseudo_data->AddPendingBackdrop(/* originating_element */ *this);
  }
}

void Element::ApplyPendingBackdropPseudoElementUpdate() {
  PseudoElement* element = GetPseudoElement(PseudoId::kPseudoIdBackdrop,
                                            /* pseudo_argument */ g_null_atom);

  if (!element && CanGeneratePseudoElement(PseudoId::kPseudoIdBackdrop)) {
    element = PseudoElement::Create(this, PseudoId::kPseudoIdBackdrop,
                                    /* pseudo_argument */ g_null_atom);
    EnsureElementRareData().SetPseudoElement(PseudoId::kPseudoIdBackdrop,
                                             element,
                                             /* pseudo_argument */ g_null_atom);
    element->InsertedInto(*this);
    GetDocument().AddToTopLayer(element, this);
  }

  DCHECK(element);
  element->SetNeedsStyleRecalc(kLocalStyleChange,
                               StyleChangeReasonForTracing::Create(
                                   style_change_reason::kConditionalBackdrop));
}

void Element::UpdateFirstLetterPseudoElement(StyleUpdatePhase phase) {
  if (CanGeneratePseudoElement(kPseudoIdFirstLetter) ||
      GetPseudoElement(kPseudoIdFirstLetter)) {
    UpdateFirstLetterPseudoElement(
        phase, StyleRecalcContext::FromPseudoElementAncestors(
                   *this, kPseudoIdFirstLetter));
  }
}

void Element::UpdateFirstLetterPseudoElement(
    StyleUpdatePhase phase,
    const StyleRecalcContext& style_recalc_context) {
  // Update the ::first-letter pseudo-elements presence and its style. This
  // method may be called from style recalc or layout tree rebuilding/
  // reattachment. In order to know if an element generates a ::first-letter
  // element, we need to know if:
  //
  // * The element generates a block level box to which ::first-letter applies.
  // * The element's layout subtree generates any first letter text.
  // * None of the descendant blocks generate a ::first-letter element.
  //   (This is not correct according to spec as all block containers should be
  //   able to generate ::first-letter elements around the first letter of the
  //   first formatted text, but Blink is only supporting a single
  //   ::first-letter element which is the innermost block generating a
  //   ::first-letter).
  //
  // We do not always do this at style recalc time as that would have required
  // us to collect the information about how the layout tree will look like
  // after the layout tree is attached. So, instead we will wait until we have
  // an up-to-date layout sub-tree for the element we are considering for
  // ::first-letter.
  //
  // The StyleUpdatePhase tells where we are in the process of updating style
  // and layout tree.

  // We need to update quotes to create the correct text fragments before the
  // first letter element update.
  if (StyleContainmentScopeTree* tree =
          GetDocument().GetStyleEngine().GetStyleContainmentScopeTree()) {
    tree->UpdateItems();
  }

  PseudoElement* element = GetPseudoElement(kPseudoIdFirstLetter);
  if (!element) {
    element =
        CreatePseudoElementIfNeeded(kPseudoIdFirstLetter, style_recalc_context);
    // If we are in Element::AttachLayoutTree, don't mess up the ancestor flags
    // for layout tree attachment/rebuilding. We will unconditionally call
    // AttachLayoutTree for the created pseudo-element immediately after this
    // call.
    if (element && phase != StyleUpdatePhase::kAttachLayoutTree) {
      element->SetNeedsReattachLayoutTree();
    }
    return;
  }

  if (!CanGeneratePseudoElement(kPseudoIdFirstLetter)) {
    GetElementRareData()->SetPseudoElement(kPseudoIdFirstLetter, nullptr);
    return;
  }

  LayoutObject* remaining_text_layout_object =
      FirstLetterPseudoElement::FirstLetterTextLayoutObject(*element);

  if (!remaining_text_layout_object) {
    GetElementRareData()->SetPseudoElement(kPseudoIdFirstLetter, nullptr);
    return;
  }

  if (phase == StyleUpdatePhase::kRebuildLayoutTree &&
      element->NeedsReattachLayoutTree()) {
    // We were already updated in RecalcStyle and ready for reattach.
    DCHECK(element->GetComputedStyle());
    return;
  }

  bool text_node_changed =
      remaining_text_layout_object !=
      To<FirstLetterPseudoElement>(element)->RemainingTextLayoutObject();

  if (phase == StyleUpdatePhase::kAttachLayoutTree) {
    // RemainingTextLayoutObject should have been cleared from DetachLayoutTree.
    DCHECK(!To<FirstLetterPseudoElement>(element)->RemainingTextLayoutObject());
    DCHECK(text_node_changed);
    const ComputedStyle* pseudo_style =
        element->StyleForLayoutObject(style_recalc_context);
    if (PseudoElementLayoutObjectIsNeeded(kPseudoIdFirstLetter, pseudo_style,
                                          this)) {
      element->SetComputedStyle(pseudo_style);
    } else {
      GetElementRareData()->SetPseudoElement(kPseudoIdFirstLetter, nullptr);
    }
    element->ClearNeedsStyleRecalc();
    return;
  }

  StyleRecalcChange change(StyleRecalcChange::kRecalcDescendants);
  // Remaining text part should be next to first-letter pseudo-element.
  // See http://crbug.com/984389 for details.
  if (text_node_changed || remaining_text_layout_object->PreviousSibling() !=
                               element->GetLayoutObject()) {
    change = change.ForceReattachLayoutTree();
  }

  element->RecalcStyle(change, style_recalc_context);

  if (element->NeedsReattachLayoutTree() &&
      !PseudoElementLayoutObjectIsNeeded(kPseudoIdFirstLetter,
                                         element->GetComputedStyle(), this)) {
    GetElementRareData()->SetPseudoElement(kPseudoIdFirstLetter, nullptr);
    GetDocument().GetStyleEngine().PseudoElementRemoved(*this);
  }
}

void Element::ClearPseudoElement(PseudoId pseudo_id,
                                 const AtomicString& pseudo_argument) {
  GetElementRareData()->SetPseudoElement(pseudo_id, nullptr, pseudo_argument);
  GetDocument().GetStyleEngine().PseudoElementRemoved(*this);
}

void Element::UpdateColumnPseudoElements(const StyleRecalcChange change,
                                         const StyleRecalcContext& context) {
  const ElementRareDataVector* data = GetElementRareData();
  if (!data) {
    return;
  }
  const ColumnPseudoElementsVector* columns = data->GetColumnPseudoElements();
  if (!columns) {
    return;
  }
  if (!CanGeneratePseudoElement(kPseudoIdColumn)) {
    return ClearColumnPseudoElements();
  }
  for (ColumnPseudoElement* column : *columns) {
    if (change.ShouldUpdatePseudoElement(*column)) {
      column->RecalcStyle(change, context);
    }
  }
}

PseudoElement* Element::UpdatePseudoElement(
    PseudoId pseudo_id,
    const StyleRecalcChange change,
    const StyleRecalcContext& style_recalc_context,
    const AtomicString& pseudo_argument) {
  PseudoElement* element = GetPseudoElement(pseudo_id, pseudo_argument);
  if (!element) {
    if ((element = CreatePseudoElementIfNeeded(pseudo_id, style_recalc_context,
                                               pseudo_argument))) {
      // ::before and ::after can have a nested ::marker
      element->CreatePseudoElementIfNeeded(kPseudoIdMarker,
                                           style_recalc_context);
      element->SetNeedsReattachLayoutTree();
    }
    return element;
  }

  if (change.ShouldUpdatePseudoElement(*element)) {
    bool generate_pseudo = CanGeneratePseudoElement(pseudo_id);
    if (pseudo_id == kPseudoIdScrollMarker &&
        !style_recalc_context
             .has_scroller_ancestor_with_scroll_marker_group_property) {
      generate_pseudo = false;
    }
    if (generate_pseudo) {
      if (auto* cache = GetDocument().ExistingAXObjectCache()) {
        cache->RemoveSubtree(this, /*remove_root*/ false);
      }
      element->RecalcStyle(change.ForPseudoElement(), style_recalc_context);
      if (element->NeedsReattachLayoutTree() &&
          !PseudoElementLayoutObjectIsNeeded(
              pseudo_id, element->GetComputedStyle(), this)) {
        generate_pseudo = false;
        // If the content property is relying on attr() we should add the
        // originating element's ComputedStyle to the pseudo-element style
        // cache, so that when attribute value changes it will force style
        // invalidation.
        if (element->GetComputedStyle() &&
            element->GetComputedStyle()->HasAttrFunction() &&
            !GetComputedStyle()->GetCachedPseudoElementStyle(pseudo_id,
                                                             g_null_atom)) {
          GetComputedStyle()->AddCachedPseudoElementStyle(
              element->GetComputedStyle(), pseudo_id, g_null_atom);
        }
      }
    }
    if (!generate_pseudo) {
      ClearPseudoElement(pseudo_id, pseudo_argument);
      element = nullptr;
    }
  }

  return element;
}

PseudoElement* Element::CreatePseudoElementIfNeeded(
    PseudoId pseudo_id,
    const StyleRecalcContext& style_recalc_context,
    const AtomicString& pseudo_argument) {
  if (!CanGeneratePseudoElement(pseudo_id)) {
    return nullptr;
  }
  if (pseudo_id == kPseudoIdScrollMarker &&
      !style_recalc_context
           .has_scroller_ancestor_with_scroll_marker_group_property) {
    return nullptr;
  }
  if (pseudo_id == kPseudoIdFirstLetter) {
    if (!FirstLetterPseudoElement::FirstLetterTextLayoutObject(*this)) {
      return nullptr;
    }
  }

  PseudoElement* pseudo_element =
      PseudoElement::Create(this, pseudo_id, pseudo_argument);
  if (!SetAssociatedPseudoElement(pseudo_element, style_recalc_context)) {
    return nullptr;
  }

  probe::PseudoElementCreated(pseudo_element);
  return pseudo_element;
}

bool Element::SetAssociatedPseudoElement(
    PseudoElement* pseudo_element,
    const StyleRecalcContext& style_recalc_context) {
  DCHECK(pseudo_element);
  PseudoId pseudo_id = pseudo_element->GetPseudoId();
  const AtomicString& pseudo_argument = pseudo_element->GetPseudoArgument();
  EnsureElementRareData().SetPseudoElement(pseudo_id, pseudo_element,
                                           pseudo_argument);
  pseudo_element->InsertedInto(*this);

  const ComputedStyle* pseudo_style =
      pseudo_element->StyleForLayoutObject(style_recalc_context);
  if (!PseudoElementLayoutObjectIsNeeded(pseudo_id, pseudo_style, this)) {
    GetElementRareData()->SetPseudoElement(pseudo_id, nullptr, pseudo_argument);
    // If the content property is relying on attr() we should add the
    // originating element's ComputedStyle to the pseudo-element style cache, so
    // that when attribute value changes it will force style invalidation.
    if (pseudo_style && pseudo_style->HasAttrFunction() &&
        !GetComputedStyle()->GetCachedPseudoElementStyle(pseudo_id,
                                                         g_null_atom)) {
      GetComputedStyle()->AddCachedPseudoElementStyle(pseudo_style, pseudo_id,
                                                      g_null_atom);
    }
    return false;
  }

  if (pseudo_id == kPseudoIdBackdrop && IsInTopLayer()) {
    GetDocument().AddToTopLayer(pseudo_element, this);
  }

  pseudo_element->SetComputedStyle(pseudo_style);
  if (pseudo_style->MayUseImplicitAnchor()) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kCSSPseudoElementUsesImplicitAnchor);
    if (RuntimeEnabledFeatures::OriginatingElementIsImplicitAnchorEnabled()) {
      SetMayBeImplicitAnchor();
    }
  }

  // Since we just styled a new pseudo element, we have to inform its potential
  // display lock context of this. This might force-unlock the element due to
  // any number of constraints (no containment, wrong layout type, etc).
  if (DisplayLockContext* display_lock_context =
          pseudo_element->GetDisplayLockContext()) {
    display_lock_context->DidStyleSelf();
  }

  return true;
}

void Element::AttachPseudoElement(PseudoId pseudo_id, AttachContext& context) {
  if (PseudoElement* pseudo_element = GetPseudoElement(pseudo_id)) {
    pseudo_element->AttachLayoutTree(context);
  }
}

void Element::DetachPseudoElement(PseudoId pseudo_id,
                                  bool performing_reattach) {
  if (PseudoElement* pseudo_element = GetPseudoElement(pseudo_id)) {
    pseudo_element->DetachLayoutTree(performing_reattach);
  }
}

PseudoElement* Element::GetPseudoElement(
    PseudoId pseudo_id,
    const AtomicString& pseudo_argument) const {
  if (ElementRareDataVector* data = GetElementRareData()) {
    return data->GetPseudoElement(pseudo_id, pseudo_argument);
  }
  return nullptr;
}

CSSPseudoElement* Element::pseudo(const AtomicString& type) {
  PseudoId pseudo_id = CSSPseudoElement::ConvertTypeToSupportedPseudoId(type);
  return EnsureCSSPseudoElement(pseudo_id);
}

CSSPseudoElement* Element::EnsureCSSPseudoElement(PseudoId pseudo_id) {
  DCHECK(RuntimeEnabledFeatures::CSSPseudoElementInterfaceEnabled());
  if (!CSSPseudoElement::IsSupportedTypeForCSSPseudoElement(pseudo_id)) {
    return nullptr;
  }
  EnsureElementRareData();
  if (CSSPseudoElement* css_pseudo_element =
          GetElementRareData()->GetCSSPseudoElement(pseudo_id)) {
    return css_pseudo_element;
  }
  auto* css_pseudo_element =
      MakeGarbageCollected<CSSPseudoElement>(*this, pseudo_id);
  GetElementRareData()->CacheCSSPseudoElement(pseudo_id, *css_pseudo_element);
  return css_pseudo_element;
}

void Element::CacheCSSPseudoElement(PseudoId pseudo_id,
                                    CSSPseudoElement& pseudo_element) {
  EnsureElementRareData().CacheCSSPseudoElement(pseudo_id, pseudo_element);
}

CSSPseudoElement* Element::GetCSSPseudoElement(PseudoId pseudo_id) const {
  if (ElementRareDataVector* data = GetElementRareData()) {
    return data->GetCSSPseudoElement(pseudo_id);
  }
  return nullptr;
}

bool Element::HasScrollButtonOrMarkerGroupPseudos() const {
  ElementRareDataVector* data = GetElementRareData();
  return data && data->HasScrollButtonOrMarkerGroupPseudos();
}

Element* Element::GetStyledPseudoElement(
    PseudoId pseudo_id,
    const AtomicString& pseudo_argument) const {
  if (!IsTransitionPseudoElement(pseudo_id)) {
    if (pseudo_id == kPseudoIdScrollMarkerGroup) {
      if (const ComputedStyle* style = GetComputedStyle()) {
        if (!style->GetScrollMarkerGroup()) {
          return nullptr;
        }
        pseudo_id = style->HasScrollMarkerGroupBefore()
                        ? kPseudoIdScrollMarkerGroupBefore
                        : kPseudoIdScrollMarkerGroupAfter;
      }
    }
    if (PseudoElement* result = GetPseudoElement(pseudo_id, pseudo_argument)) {
      return result;
    }
    const AtomicString& pseudo_string =
        shadow_element_utils::StringForUAShadowPseudoId(pseudo_id);
    if (pseudo_string != g_null_atom) {
      // This is a pseudo-element that refers to an element in the UA shadow
      // tree (such as a element-backed pseudo-element).  Find it in the
      // shadow tree.
      if (ShadowRoot* root = GetShadowRoot()) {
        if (root->IsUserAgent()) {
          for (Element& el : ElementTraversal::DescendantsOf(*root)) {
            if (el.ShadowPseudoId() == pseudo_string) {
              return &el;
            }
          }
        }
      }
    }

    return nullptr;
  }

  if (!RuntimeEnabledFeatures::ScopedViewTransitionsEnabled()) {
    // The transition pseudos can currently only exist on the document element
    // unless scoped view transitions are enabled.
    if (!IsDocumentElement()) {
      return nullptr;
    }
  }

  // This traverses the pseudo-element hierarchy generated in
  // UpdateTransitionPseudoElements to query nested ::view-transition-group
  // ::view-transition-image-pair and
  // ::view-transition-{old,new} pseudo-elements.
  auto* transition_pseudo = GetPseudoElement(kPseudoIdViewTransition);
  if (!transition_pseudo || pseudo_id == kPseudoIdViewTransition) {
    return transition_pseudo;
  }

  auto* container_pseudo =
      To<ViewTransitionTransitionElement>(transition_pseudo)
          ->FindViewTransitionGroupPseudoElement(pseudo_argument);
  if (!container_pseudo || pseudo_id == kPseudoIdViewTransitionGroup) {
    return container_pseudo;
  }

  if (pseudo_id == kPseudoIdViewTransitionGroupChildren) {
    return container_pseudo->GetPseudoElement(pseudo_id, pseudo_argument);
  }

  auto* wrapper_pseudo = container_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionImagePair, pseudo_argument);
  if (!wrapper_pseudo || pseudo_id == kPseudoIdViewTransitionImagePair) {
    return wrapper_pseudo;
  }

  return wrapper_pseudo->GetPseudoElement(pseudo_id, pseudo_argument);
}

LayoutObject* Element::PseudoElementLayoutObject(PseudoId pseudo_id) const {
  if (Element* element =
          GetStyledPseudoElement(pseudo_id, /*pseudo_argument*/ g_null_atom)) {
    return element->GetLayoutObject();
  }
  return nullptr;
}

bool Element::PseudoElementStylesAffectCounters() const {
  const ComputedStyle* style = GetComputedStyle();
  if (!style) {
    return false;
  }
  const ElementRareDataVector* rare_data = GetElementRareData();
  if (!rare_data) {
    return false;
  }

  if (rare_data->PseudoElementStylesAffectCounters()) {
    return true;
  }

  for (PseudoElement* pseudo_element : rare_data->GetPseudoElements()) {
    if (pseudo_element->GetComputedStyle()->GetCounterDirectives()) {
      return true;
    }
  }

  return false;
}

bool Element::PseudoElementStylesDependOnFontMetrics() const {
  const ComputedStyle* style = GetComputedStyle();
  const ElementRareDataVector* rare_data = GetElementRareData();
  if (style && rare_data &&
      rare_data->ScrollbarPseudoElementStylesDependOnFontMetrics()) {
    return true;
  }

  auto func = [](const ComputedStyle& style) {
    return style.DependsOnFontMetrics();
  };
  return PseudoElementStylesDependOnFunc(func);
}

bool Element::PseudoElementStylesDependOnAttr() const {
  auto func = [](const ComputedStyle& style) {
    return style.HasAttrFunction();
  };
  return PseudoElementStylesDependOnFunc(func);
}

template <typename Functor>
bool Element::PseudoElementStylesDependOnFunc(Functor& func) const {
  const ComputedStyle* style = GetComputedStyle();
  if (!style) {
    return false;
  }

  if (style->HasCachedPseudoElementStyle(func)) {
    return true;
  }

  // If we don't generate a PseudoElement, its style must have been cached on
  // the originating element's ComputedStyle. Hence, it remains to check styles
  // on the generated PseudoElements.
  const ElementRareDataVector* rare_data = GetElementRareData();
  if (!rare_data) {
    return false;
  }

  // Note that |HasAnyPseudoElementStyles()| counts public pseudo-elements only.
  // ::-webkit-scrollbar-*  are internal, and hence are not counted. So we must
  // perform this check after checking scrollbar pseudo-element styles.
  if (!style->HasAnyPseudoElementStyles()) {
    return false;
  }

  for (PseudoElement* pseudo_element : rare_data->GetPseudoElements()) {
    if (func(*pseudo_element->GetComputedStyle())) {
      return true;
    }
  }

  return false;
}

const ComputedStyle* Element::CachedStyleForPseudoElement(
    PseudoId pseudo_id,
    const AtomicString& pseudo_argument) {
  // Highlight pseudos are resolved into StyleHighlightData during originating
  // style recalc, and should never be stored in StyleCachedData.
  DCHECK(!IsHighlightPseudoElement(pseudo_id));

  const ComputedStyle* style = GetComputedStyle();

  if (!style) {
    return nullptr;
  }
  if (pseudo_id <= kLastTrackedPublicPseudoId &&
      !style->HasPseudoElementStyle(pseudo_id)) {
    return nullptr;
  }

  if (const ComputedStyle* cached =
          style->GetCachedPseudoElementStyle(pseudo_id, pseudo_argument)) {
    return cached;
  }

  // When not using Highlight Pseudo Inheritance, as asserted above, the
  // originating element style is the same as the parent style.
  const ComputedStyle* result = UncachedStyleForPseudoElement(
      StyleRequest(pseudo_id, style, style, pseudo_argument));
  if (result) {
    return style->AddCachedPseudoElementStyle(result, pseudo_id,
                                              pseudo_argument);
  }
  return nullptr;
}

const ComputedStyle* Element::UncachedStyleForPseudoElement(
    const StyleRequest& request) {
  // Highlight pseudos are resolved into StyleHighlightData during originating
  // style recalc, where we have the actual StyleRecalcContext.
  DCHECK(!IsHighlightPseudoElement(request.pseudo_id));

  return StyleForPseudoElement(
      StyleRecalcContext::FromPseudoElementAncestors(*this, request.pseudo_id),
      request);
}

const ComputedStyle* Element::StyleForPseudoElement(
    const StyleRecalcContext& style_recalc_context,
    const StyleRequest& request) {
  GetDocument().GetStyleEngine().UpdateViewportSize();

  PseudoId pseudo_id = IsPseudoElement() && request.pseudo_id == kPseudoIdNone
                           ? GetPseudoIdForStyling()
                           : request.pseudo_id;

  const bool is_before_or_after_like =
      pseudo_id == kPseudoIdCheckMark || pseudo_id == kPseudoIdBefore ||
      pseudo_id == kPseudoIdAfter || pseudo_id == kPseudoIdPickerIcon ||
      pseudo_id == kPseudoIdInterestHint;

  if (is_before_or_after_like) {
    DCHECK(request.parent_override);
    DCHECK(request.layout_parent_override);

    const ComputedStyle* layout_parent_style = request.parent_override;
    if (layout_parent_style->Display() == EDisplay::kContents) {
      // TODO(futhark@chromium.org): Calling getComputedStyle for elements
      // outside the flat tree should return empty styles, but currently we do
      // not. See issue https://crbug.com/831568. We can replace the if-test
      // with DCHECK(layout_parent) when that issue is fixed.
      if (Element* layout_parent =
              LayoutTreeBuilderTraversal::LayoutParentElement(*this)) {
        layout_parent_style = layout_parent->GetComputedStyle();
      }
    }
    StyleRequest before_after_request = request;
    before_after_request.layout_parent_override = layout_parent_style;
    const ComputedStyle* result = GetDocument().GetStyleResolver().ResolveStyle(
        this, style_recalc_context, before_after_request);
    if (result) {
      if (result->GetCounterDirectives()) {
        SetPseudoElementStylesChangeCounters(true);
      }
      Element* originating_element_or_self =
          IsPseudoElement()
              ? &To<PseudoElement>(this)->UltimateOriginatingElement()
              : this;
      if (auto* quote =
              DynamicTo<HTMLQuoteElement>(originating_element_or_self)) {
        ComputedStyleBuilder builder(*result);
        quote->AdjustPseudoStyleLocale(builder);
        result = builder.TakeStyle();
      }
    }
    return result;
  }

  if (pseudo_id == kPseudoIdFirstLineInherited) {
    StyleRequest first_line_inherited_request = request;
    first_line_inherited_request.pseudo_id =
        IsPseudoElement() ? To<PseudoElement>(this)->GetPseudoIdForStyling()
                          : kPseudoIdNone;
    first_line_inherited_request.can_trigger_animations = false;
    StyleRecalcContext local_recalc_context(style_recalc_context);
    local_recalc_context.old_style = PostStyleUpdateScope::GetOldStyle(*this);
    Element* target = IsPseudoElement() ? parentElement() : this;
    const ComputedStyle* result = GetDocument().GetStyleResolver().ResolveStyle(
        target, local_recalc_context, first_line_inherited_request);
    if (result) {
      ComputedStyleBuilder builder(*result);
      builder.SetStyleType(kPseudoIdFirstLineInherited);
      result = builder.TakeStyle();
    }
    return result;
  }

  StyleRequest style_request = request;
  if (PseudoElement::IsLayoutSiblingOfOriginatingElement(*this, pseudo_id)) {
    CHECK(request.parent_override);
    const ComputedStyle* layout_parent_style = request.parent_override;
    Element* layout_parent =
        LayoutTreeBuilderTraversal::LayoutParentElement(*this);
    CHECK(layout_parent);
    // Only root element doesn't have layout grand parent.
    if (Element* layout_grand_parent =
            LayoutTreeBuilderTraversal::LayoutParentElement(*layout_parent)) {
      layout_parent_style = layout_grand_parent->GetComputedStyle();
    }
    style_request.layout_parent_override = layout_parent_style;
  }

  const ComputedStyle* result = GetDocument().GetStyleResolver().ResolveStyle(
      this, style_recalc_context, style_request);
  if (result && result->GetCounterDirectives()) {
    SetPseudoElementStylesChangeCounters(true);
  }
  return result;
}

const ComputedStyle* Element::StyleForHighlightPseudoElement(
    const StyleRecalcContext& style_recalc_context,
    const ComputedStyle* highlight_parent,
    const ComputedStyle& originating_style,
    const PseudoId pseudo_id,
    const AtomicString& pseudo_argument) {
  StyleRequest style_request{pseudo_id, highlight_parent, &originating_style,
                             pseudo_argument};
  return StyleForPseudoElement(style_recalc_context, style_request);
}

const ComputedStyle* Element::StyleForSearchTextPseudoElement(
    const StyleRecalcContext& style_recalc_context,
    const ComputedStyle* highlight_parent,
    const ComputedStyle& originating_style,
    StyleRequest::SearchTextRequest search_text_request) {
  StyleRequest style_request{kPseudoIdSearchText, highlight_parent,
                             &originating_style};
  style_request.search_text_request = search_text_request;
  return StyleForPseudoElement(style_recalc_context, style_request);
}

bool Element::CanGeneratePseudoElement(PseudoId pseudo_id) const {
  if (pseudo_id == kPseudoIdViewTransition) {
    return !!ViewTransitionUtils::GetTransition(*this) && !!GetComputedStyle();
  }
  if (pseudo_id == kPseudoIdFirstLetter && IsSVGElement()) {
    return false;
  }
  if (pseudo_id == kPseudoIdCheckMark) {
    // We want to avoid the performance cost of generating the checkmark for
    // old-style selects.
    auto is_option_in_appearance_base_select = [](const Element* e) {
      if (const auto* option = DynamicTo<HTMLOptionElement>(e)) {
        if (const HTMLSelectElement* select = option->OwnerSelectElement()) {
          if (select->UsesMenuList()) {
            return select->PickerIsPopover();
          } else {
            return select->IsAppearanceBase();
          }
        }
      }
      return false;
    };
    const HTMLMenuItemElement* menu_item = DynamicTo<HTMLMenuItemElement>(this);
    const bool checkable_menu_item = menu_item && menu_item->IsCheckable();
    if (!is_option_in_appearance_base_select(this) && !checkable_menu_item) {
      return false;
    }
  }
  if (pseudo_id == kPseudoIdInterestHint && !InterestForElement()) {
    return false;
  }
  if (const ComputedStyle* style = GetComputedStyle()) {
    if (IsDocumentElement()) {
      // The root element is never a scroll container, but is scrolled by the
      // viewport, which is always (programmatically) scrollable.
      // ::scroll-marker-group pseudo-element boxes are generated as children of
      // the root element box, and not as siblings which is the case for other
      // elements.
      if (pseudo_id == kPseudoIdScrollMarkerGroupBefore) {
        return style->HasScrollMarkerGroupBefore();
      }
      if (pseudo_id == kPseudoIdScrollMarkerGroupAfter) {
        return style->HasScrollMarkerGroupAfter();
      }
    }
    if (!RuntimeEnabledFeatures::OverlayPropertyEnabled() &&
        pseudo_id == kPseudoIdBackdrop) {
      return IsInTopLayer();
    }
    return style->CanGeneratePseudoElement(pseudo_id);
  }
  return false;
}

bool Element::HasSiblingBoxPseudoElements() const {
  const ElementRareDataVector* rare_data = GetElementRareData();
  if (!rare_data) {
    return false;
  }
  for (PseudoId pseudo_id :
       {kPseudoIdScrollButtonBlockStart, kPseudoIdScrollButtonInlineStart,
        kPseudoIdScrollButtonInlineEnd, kPseudoIdScrollButtonBlockEnd,
        kPseudoIdScrollMarkerGroupAfter, kPseudoIdScrollMarkerGroupBefore}) {
    if (rare_data->GetPseudoElement(pseudo_id)) {
      return true;
    }
  }
  return false;
}

bool Element::MayTriggerVirtualKeyboard() const {
  return IsEditable(*this);
}

bool Element::matches(const AtomicString& selectors,
                      ExceptionState& exception_state) {
  SelectorQuery* selector_query = GetDocument().GetSelectorQueryCache().Add(
      selectors, GetDocument(), exception_state);
  if (!selector_query) {
    return false;
  }
  return selector_query->Matches(*this);
}

bool Element::matches(const AtomicString& selectors) {
  return matches(selectors, ASSERT_NO_EXCEPTION);
}

Element* Element::closest(const AtomicString& selectors,
                          ExceptionState& exception_state) {
  SelectorQuery* selector_query = GetDocument().GetSelectorQueryCache().Add(
      selectors, GetDocument(), exception_state);
  if (!selector_query) {
    return nullptr;
  }
  return selector_query->Closest(*this);
}

Element* Element::closest(const AtomicString& selectors) {
  return closest(selectors, ASSERT_NO_EXCEPTION);
}

DOMTokenList& Element::classList() {
  ElementRareDataVector& rare_data = EnsureElementRareData();
  if (!rare_data.GetClassList()) {
    auto* class_list =
        MakeGarbageCollected<DOMTokenList>(*this, html_names::kClassAttr);
    class_list->DidUpdateAttributeValue(g_null_atom,
                                        getAttribute(html_names::kClassAttr));
    rare_data.SetClassList(class_list);
  }
  return *rare_data.GetClassList();
}

DOMStringMap& Element::dataset() {
  ElementRareDataVector& rare_data = EnsureElementRareData();
  if (!rare_data.Dataset()) {
    rare_data.SetDataset(MakeGarbageCollected<DatasetDOMStringMap>(this));
  }
  return *rare_data.Dataset();
}

KURL Element::HrefURL() const {
  // FIXME: These all have href() or url(), but no common super class. Why
  // doesn't <link> implement URLUtils?
  if (IsA<HTMLAnchorElement>(*this) || IsA<HTMLAreaElement>(*this) ||
      IsA<HTMLLinkElement>(*this)) {
    return GetURLAttributeAsKURL(html_names::kHrefAttr);
  }
  if (auto* svg_a = DynamicTo<SVGAElement>(*this)) {
    return svg_a->LegacyHrefURL(GetDocument());
  }
  return KURL();
}

String Element::GetURLAttribute(const QualifiedName& name) const {
#if DCHECK_IS_ON()
  if (HasElementData()) {
    if (const Attribute* attribute = Attributes().Find(name)) {
      DCHECK(IsURLAttribute(*attribute));
    }
  }
#endif
  KURL url = GetDocument().CompleteURL(
      StripLeadingAndTrailingHTMLSpaces(getAttribute(name)));
  return url.IsValid() ? url
                       : StripLeadingAndTrailingHTMLSpaces(getAttribute(name));
}

KURL Element::GetURLAttributeAsKURL(const QualifiedName& name) const {
  return GetDocument().CompleteURL(
      StripLeadingAndTrailingHTMLSpaces(getAttribute(name)));
}

KURL Element::GetNonEmptyURLAttribute(const QualifiedName& name) const {
#if DCHECK_IS_ON()
  if (HasElementData()) {
    if (const Attribute* attribute = Attributes().Find(name)) {
      DCHECK(IsURLAttribute(*attribute));
    }
  }
#endif
  String value = StripLeadingAndTrailingHTMLSpaces(getAttribute(name));
  if (value.empty()) {
    return KURL();
  }
  return GetDocument().CompleteURL(value);
}

int Element::GetIntegralAttribute(const QualifiedName& attribute_name) const {
  return GetIntegralAttribute(attribute_name, 0);
}

int Element::GetIntegralAttribute(const QualifiedName& attribute_name,
                                  int default_value) const {
  int integral_value = default_value;
  ParseHTMLInteger(getAttribute(attribute_name), integral_value);
  return integral_value;
}

unsigned int Element::GetUnsignedIntegralAttribute(
    const QualifiedName& attribute_name) const {
  return static_cast<unsigned int>(
      std::max(0, GetIntegralAttribute(attribute_name)));
}

void Element::SetIntegralAttribute(const QualifiedName& attribute_name,
                                   int value) {
  setAttribute(attribute_name, AtomicString::Number(value));
}

void Element::SetUnsignedIntegralAttribute(const QualifiedName& attribute_name,
                                           unsigned value,
                                           unsigned default_value) {
  // Range restrictions are enforced for unsigned IDL attributes that
  // reflect content attributes,
  //   http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#reflecting-content-attributes-in-idl-attributes
  if (value > 0x7fffffffu) {
    value = default_value;
  }
  setAttribute(attribute_name, AtomicString::Number(value));
}

double Element::GetFloatingPointAttribute(const QualifiedName& attribute_name,
                                          double fallback_value) const {
  return ParseToDoubleForNumberType(getAttribute(attribute_name),
                                    fallback_value);
}

void Element::SetFloatingPointAttribute(const QualifiedName& attribute_name,
                                        double value) {
  String serialized_value = SerializeForNumberType(value);
  setAttribute(attribute_name, AtomicString(serialized_value));
}

void Element::SetContainsFullScreenElement(bool flag) {
  SetElementFlag(ElementFlags::kContainsFullScreenElement, flag);
  // When exiting fullscreen, the element's document may not be active.
  if (flag) {
    DCHECK(GetDocument().IsActive());
    GetDocument().GetStyleEngine().EnsureUAStyleForFullscreen(*this);
  }
  PseudoStateChanged(CSSSelector::kPseudoFullScreenAncestor);
}

// Unlike Node::parentOrShadowHostElement, this can cross frame boundaries.
static Element* NextAncestorElement(Element* element) {
  DCHECK(element);
  if (element->ParentOrShadowHostElement()) {
    return element->ParentOrShadowHostElement();
  }

  Frame* frame = element->GetDocument().GetFrame();
  if (!frame || !frame->Owner()) {
    return nullptr;
  }

  // Find the next LocalFrame on the ancestor chain, and return the
  // corresponding <iframe> element for the remote child if it exists.
  while (frame->Tree().Parent() && frame->Tree().Parent()->IsRemoteFrame()) {
    frame = frame->Tree().Parent();
  }

  if (auto* frame_owner_element =
          DynamicTo<HTMLFrameOwnerElement>(frame->Owner())) {
    return frame_owner_element;
  }

  return nullptr;
}

void Element::SetContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(
    bool flag) {
  for (Element* element = NextAncestorElement(this); element;
       element = NextAncestorElement(element)) {
    element->SetContainsFullScreenElement(flag);
  }
}

void Element::SetContainsPersistentVideo(bool value) {
  SetElementFlag(ElementFlags::kContainsPersistentVideo, value);
  PseudoStateChanged(CSSSelector::kPseudoVideoPersistentAncestor);

  // In some rare situations, when the persistent video has been removed from
  // the tree, part of the tree might still carry the flag.
  if (!value && Fullscreen::IsFullscreenElement(*this)) {
    for (Node* node = firstChild(); node;) {
      auto* element = DynamicTo<Element>(node);
      if (!element || !element->ContainsPersistentVideo()) {
        node = node->nextSibling();
        break;
      }

      element->SetContainsPersistentVideo(false);
      node = node->firstChild();
    }
  }
}

void Element::SetIsInTopLayer(bool in_top_layer) {
  if (IsInTopLayer() == in_top_layer) {
    return;
  }
  SetElementFlag(ElementFlags::kIsInTopLayer, in_top_layer);
  if (!isConnected()) {
    return;
  }

  if (!GetDocument().InStyleRecalc()) {
    if (in_top_layer) {
      // Need to force re-attachment in case the element was removed and re-
      // added between two lifecycle updates since the overlay computed value
      // would not change, but the layout object order may have.
      SetForceReattachLayoutTree();
    }
  }
}

ScriptPromise<IDLUndefined> Element::requestPointerLock(
    ScriptState* script_state,
    const PointerLockOptions* options,
    ExceptionState& exception_state) {
  if (!GetDocument().GetPage()) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kWrongDocumentError,
                          "PointerLock cannot be requested when there "
                          "is no frame or that frame has no page."));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  GetDocument().GetPage()->GetPointerLockController().RequestPointerLock(
      resolver, this, options);
  return resolver->Promise();
}

SpellcheckAttributeState Element::GetSpellcheckAttributeState() const {
  const AtomicString& value = FastGetAttribute(html_names::kSpellcheckAttr);
  if (value == g_null_atom) {
    return kSpellcheckAttributeDefault;
  }
  if (EqualIgnoringASCIICase(value, "true") ||
      EqualIgnoringASCIICase(value, "")) {
    return kSpellcheckAttributeTrue;
  }
  if (EqualIgnoringASCIICase(value, "false")) {
    return kSpellcheckAttributeFalse;
  }

  return kSpellcheckAttributeDefault;
}

bool Element::IsSpellCheckingEnabled() const {
  // TODO(crbug.com/1365686): This is not compliant with the spec
  // https://html.spec.whatwg.org/#concept-spellcheck-default
  for (const Element* element = this; element;
       element = element->ParentOrShadowHostElement()) {
    switch (element->GetSpellcheckAttributeState()) {
      case kSpellcheckAttributeTrue:
        return true;
      case kSpellcheckAttributeFalse:
        return false;
      case kSpellcheckAttributeDefault:
        if (const auto* input = DynamicTo<HTMLInputElement>(element)) {
          if (input->HasBeenPasswordField()) {
            return false;
          }
        }
        break;
    }
  }

  if (!GetDocument().GetPage()) {
    return true;
  }

  return GetDocument().GetPage()->GetSettings().GetSpellCheckEnabledByDefault();
}

#if DCHECK_IS_ON()
bool Element::FastAttributeLookupAllowed(const QualifiedName& name) const {
  if (name == html_names::kStyleAttr) {
    return false;
  }

  if (auto* svg_element = DynamicTo<SVGElement>(this)) {
    return !svg_element->IsAnimatableAttribute(name);
  }

  return true;
}
#endif

#if DUMP_NODE_STATISTICS
bool Element::HasNamedNodeMap() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->AttributeMap();
  }
  return false;
}
#endif

inline void Element::UpdateName(const AtomicString& old_name,
                                const AtomicString& new_name) {
  if (!IsInDocumentTree()) {
    return;
  }

  if (old_name == new_name) {
    return;
  }

  NamedItemType type = GetNamedItemType();
  if (type != NamedItemType::kNone) {
    UpdateNamedItemRegistration(type, old_name, new_name);
  }
}

inline void Element::UpdateId(const AtomicString& old_id,
                              const AtomicString& new_id) {
  if (!IsInTreeScope()) {
    return;
  }

  if (old_id == new_id) {
    return;
  }

  DCHECK(IsInTreeScope());
  UpdateId(GetTreeScope(), old_id, new_id);
}

inline void Element::UpdateId(TreeScope& scope,
                              const AtomicString& old_id,
                              const AtomicString& new_id) {
  DCHECK(IsInTreeScope());
  DCHECK_NE(old_id, new_id);

  if (!old_id.empty()) {
    scope.RemoveElementById(old_id, *this);
  }
  if (!new_id.empty()) {
    scope.AddElementById(new_id, *this);
  }

  NamedItemType type = GetNamedItemType();
  if (type == NamedItemType::kNameOrId ||
      type == NamedItemType::kNameOrIdWithName) {
    UpdateIdNamedItemRegistration(type, old_id, new_id);
  }
}

inline void Element::UpdateFocusgroup(const AtomicString& input) {
  ExecutionContext* context = GetExecutionContext();
  if (!RuntimeEnabledFeatures::FocusgroupEnabled(context)) {
    return;
  }

  if (ShadowRoot* shadow_root = ContainingShadowRoot()) {
    shadow_root->SetHasFocusgroupAttributeOnDescendant(true);
  }

  EnsureElementRareData().SetFocusgroupData(
      focusgroup::ParseFocusgroup(this, input));
}

void Element::UpdateFocusgroupInShadowRootIfNeeded() {
  ShadowRoot* shadow_root = GetShadowRoot();
  DCHECK(shadow_root);

  // There's no need to re-run the focusgroup parser on the nodes of the shadow
  // tree if none of them had the focusgroup attribute set.
  if (!shadow_root->HasFocusgroupAttributeOnDescendant()) {
    return;
  }

  Element* ancestor = this;
  bool has_focusgroup_ancestor = false;
  while (ancestor) {
    if (ancestor->GetFocusgroupData().behavior !=
        FocusgroupBehavior::kNoBehavior) {
      has_focusgroup_ancestor = true;
      break;
    }
    ancestor = ancestor->parentElement();
  }

  // We don't need to update the focusgroup value for the ShadowDOM elements if
  // there is no ancestor with a focusgroup value, since the parsing would be
  // exactly the same as the one that happened when we first built the
  // ShadowDOM.
  if (!has_focusgroup_ancestor) {
    return;
  }

  // In theory, we should only reach this point when at least one node within
  // the shadow tree has the focusgroup attribute. However, it's possible to get
  // here if a node initially had the focusgroup attribute but then lost it
  // since we don't reset the `ShadowRoot::HasFocusgroupAttributeOnDescendant`
  // upon removing the attribute.
  //
  // Setting this value back to false before iterating over the nodes of the
  // shadow tree allow us to reset the bit in case an update to the shadow tree
  // removed all focusgroup attributes from the shadow tree. If there's still
  // a focusgroup attribute, then the call to `UpdateFocusgroup` below will
  // make sure that the bit is set to true for the containing shadow root.
  shadow_root->SetHasFocusgroupAttributeOnDescendant(false);

  Node* next = FlatTreeTraversal::Next(*this, this);
  while (next) {
    bool skip_subtree = false;
    if (Element* next_element = DynamicTo<Element>(next)) {
      const AtomicString& focusgroup_value =
          next_element->FastGetAttribute(html_names::kFocusgroupAttr);
      if (!focusgroup_value.IsNull()) {
        next_element->UpdateFocusgroup(focusgroup_value);
      }

      if (auto* next_shadow_root = next_element->GetShadowRoot()) {
        skip_subtree = !next_shadow_root->HasFocusgroupAttributeOnDescendant();
      }
    }

    if (skip_subtree) {
      next = FlatTreeTraversal::NextSkippingChildren(*next, this);
    } else {
      next = FlatTreeTraversal::Next(*next, this);
    }
  }
}

void Element::WillModifyAttribute(const QualifiedName& name,
                                  const AtomicString& old_value,
                                  const AtomicString& new_value) {
  if (name == html_names::kNameAttr) {
    UpdateName(old_value, new_value);
  }

  if (GetCustomElementState() == CustomElementState::kCustom) {
    CustomElement::EnqueueAttributeChangedCallback(*this, name, old_value,
                                                   new_value);
  }

  if (old_value != new_value) {
    GetDocument().GetStyleEngine().AttributeChangedForElement(name, *this);
  }

  if (MutationObserverInterestGroup* recipients =
          MutationObserverInterestGroup::CreateForAttributesMutation(*this,
                                                                     name)) {
    recipients->EnqueueMutationRecord(
        MutationRecord::CreateAttributes(this, name, old_value));
  }
  probe::WillModifyDOMAttr(this, old_value, new_value);
}

DISABLE_CFI_PERF
void Element::DidAddAttribute(const QualifiedName& name,
                              const AtomicString& value) {
  AttributeChanged(AttributeModificationParams(
      name, g_null_atom, value, AttributeModificationReason::kDirectly));
  if (name == html_names::kIdAttr) {
    UpdateId(g_null_atom, value);
  }
  probe::DidModifyDOMAttr(this, name, value);
}

void Element::DidModifyAttribute(const QualifiedName& name,
                                 const AtomicString& old_value,
                                 const AtomicString& new_value,
                                 AttributeModificationReason reason) {
  if (name == html_names::kIdAttr) {
    UpdateId(old_value, new_value);
  }
  AttributeChanged(
      AttributeModificationParams(name, old_value, new_value, reason));
  probe::DidModifyDOMAttr(this, name, new_value);
  // Do not dispatch a DOMSubtreeModified event here; see bug 81141.
}

void Element::DidRemoveAttribute(const QualifiedName& name,
                                 const AtomicString& old_value) {
  if (name == html_names::kIdAttr) {
    UpdateId(old_value, g_null_atom);
  }
  AttributeChanged(AttributeModificationParams(
      name, old_value, g_null_atom, AttributeModificationReason::kDirectly));
  probe::DidRemoveDOMAttr(this, name);
}

static bool NeedsURLResolutionForInlineStyle(const Element& element,
                                             const Document& old_document,
                                             const Document& new_document) {
  if (old_document == new_document) {
    return false;
  }
  if (old_document.BaseURL() == new_document.BaseURL()) {
    return false;
  }
  const CSSPropertyValueSet* style = element.InlineStyle();
  if (!style) {
    return false;
  }
  for (const CSSPropertyValue& property : style->Properties()) {
    if (property.Value().MayContainUrl()) {
      return true;
    }
  }
  return false;
}

static void ReResolveURLsInInlineStyle(const Document& document,
                                       MutableCSSPropertyValueSet& style) {
  for (const CSSPropertyValue& property : style.Properties()) {
    const CSSValue& value = property.Value();
    if (value.MayContainUrl()) {
      value.ReResolveUrl(document);
    }
  }
}

void Element::DidMoveToNewDocument(Document& old_document) {
  ContainerNode::DidMoveToNewDocument(old_document);

  // If the documents differ by quirks mode then they differ by case sensitivity
  // for class and id names so we need to go through the attribute change logic
  // to pick up the new casing in the ElementData. If the id/class is already
  // lower-case, then it's not impacted by quirks mode and no change is
  // necessary.
  if (old_document.InQuirksMode() != GetDocument().InQuirksMode()) {
    // TODO(tkent): If new owner Document has a ShareableElementData matching to
    // this element's attributes, we shouldn't make UniqueElementData, and this
    // element should point to the shareable one.

    if (const AtomicString& id_attr = GetIdAttribute()) {
      if (!id_attr.IsLowerASCII()) {
        EnsureUniqueElementData();
        SetIdAttribute(id_attr);
      }
    }
    if (const AtomicString& class_attr = GetClassAttribute()) {
      if (!class_attr.IsLowerASCII()) {
        EnsureUniqueElementData();
        // Going through setAttribute() to synchronize the attribute is only
        // required when setting the "style" attribute (this sets the "class"
        // attribute) or for an SVG element (in which case `GetClassAttribute`
        // above would already have synchronized).
        SetAttributeInternal(FindAttributeIndex(html_names::kClassAttr),
                             html_names::kClassAttr, class_attr,
                             AttributeModificationReason::kByMoveToNewDocument);
      }
    }
  }
  // TODO(tkent): Even if Documents' modes are same, keeping
  // ShareableElementData owned by old_document isn't right.

  if (NeedsURLResolutionForInlineStyle(*this, old_document, GetDocument())) {
    ReResolveURLsInInlineStyle(GetDocument(), EnsureMutableInlineStyle());
  }

  if (auto* context = GetDisplayLockContext()) {
    context->DidMoveToNewDocument(old_document);
  }
}

void Element::UpdateNamedItemRegistration(NamedItemType type,
                                          const AtomicString& old_name,
                                          const AtomicString& new_name) {
  auto* doc = DynamicTo<HTMLDocument>(GetDocument());
  if (!doc) {
    return;
  }

  if (!old_name.empty()) {
    doc->RemoveNamedItem(old_name);
  }

  if (!new_name.empty()) {
    doc->AddNamedItem(new_name);
  }

  if (type == NamedItemType::kNameOrIdWithName) {
    const AtomicString id = GetIdAttribute();
    if (!id.empty()) {
      if (!old_name.empty() && new_name.empty()) {
        doc->RemoveNamedItem(id);
      } else if (old_name.empty() && !new_name.empty()) {
        doc->AddNamedItem(id);
      }
    }
  }
}

void Element::UpdateIdNamedItemRegistration(NamedItemType type,
                                            const AtomicString& old_id,
                                            const AtomicString& new_id) {
  auto* doc = DynamicTo<HTMLDocument>(GetDocument());
  if (!doc) {
    return;
  }

  if (type == NamedItemType::kNameOrIdWithName && GetNameAttribute().empty()) {
    return;
  }

  if (!old_id.empty()) {
    doc->RemoveNamedItem(old_id);
  }

  if (!new_id.empty()) {
    doc->AddNamedItem(new_id);
  }
}

ScrollOffset Element::SavedLayerScrollOffset() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->SavedLayerScrollOffset();
  }
  return ScrollOffset();
}

void Element::SetSavedLayerScrollOffset(const ScrollOffset& size) {
  if (ElementRareDataVector* data = GetElementRareData()) {
    return data->SetSavedLayerScrollOffset(size);
  } else if (!size.IsZero()) {
    EnsureElementRareData().SetSavedLayerScrollOffset(size);
  }
}

Attr* Element::AttrIfExists(const QualifiedName& name) {
  if (AttrNodeList* attr_node_list = GetAttrNodeList()) {
    for (const auto& attr : *attr_node_list) {
      if (attr->GetQualifiedName().Matches(name)) {
        return attr.Get();
      }
    }
  }
  return nullptr;
}

Attr* Element::EnsureAttr(const QualifiedName& name) {
  Attr* attr_node = AttrIfExists(name);
  if (!attr_node) {
    attr_node = MakeGarbageCollected<Attr>(*this, name);
    GetTreeScope().AdoptIfNeeded(*attr_node);
    EnsureElementRareData().AddAttr(attr_node);
  }
  return attr_node;
}

void Element::DetachAttrNodeFromElementWithValue(Attr* attr_node,
                                                 const AtomicString& value) {
  DCHECK(GetAttrNodeList());
  attr_node->DetachFromElementWithValue(value);

  AttrNodeList* list = GetAttrNodeList();
  wtf_size_t index = list->Find(attr_node);
  DCHECK_NE(index, kNotFound);
  list->EraseAt(index);
  if (list->empty()) {
    RemoveAttrNodeList();
  }
}

void Element::DetachAllAttrNodesFromElement() {
  AttrNodeList* list = GetAttrNodeList();
  if (!list) {
    return;
  }

  AttributeCollection attributes = GetElementData()->Attributes();
  for (const Attribute& attr : attributes) {
    if (Attr* attr_node = AttrIfExists(attr.GetName())) {
      attr_node->DetachFromElementWithValue(attr.Value());
    }
  }

  RemoveAttrNodeList();
}

void Element::WillRecalcStyle(const StyleRecalcChange) {
  DCHECK(HasCustomStyleCallbacks());
}

void Element::DidRecalcStyle(const StyleRecalcChange) {
  DCHECK(HasCustomStyleCallbacks());
}

const ComputedStyle* Element::CustomStyleForLayoutObject(
    const StyleRecalcContext& style_recalc_context) {
  DCHECK(HasCustomStyleCallbacks());
  return OriginalStyleForLayoutObject(style_recalc_context);
}

void Element::AdjustStyle(ComputedStyleBuilder&) {
  DCHECK(HasCustomStyleCallbacks());
}

void Element::CloneAttributesFrom(const Element& other) {
  if (GetElementRareData()) {
    DetachAllAttrNodesFromElement();
  }

  other.SynchronizeAllAttributes();
  if (!other.element_data_) {
    element_data_.Clear();
    return;
  }

  const AtomicString& old_id = GetIdAttribute();
  const AtomicString& new_id = other.GetIdAttribute();

  if (!old_id.IsNull() || !new_id.IsNull()) {
    UpdateId(old_id, new_id);
  }

  const AtomicString& old_name = GetNameAttribute();
  const AtomicString& new_name = other.GetNameAttribute();

  if (!old_name.IsNull() || !new_name.IsNull()) {
    UpdateName(old_name, new_name);
  }

  // Quirks mode makes class and id not case sensitive. We can't share the
  // ElementData if the idForStyleResolution and the className need different
  // casing.
  bool owner_documents_have_different_case_sensitivity = false;
  if (other.HasClass() || other.HasID()) {
    owner_documents_have_different_case_sensitivity =
        other.GetDocument().InQuirksMode() != GetDocument().InQuirksMode();
  }

  // If 'other' has a mutable ElementData, convert it to an immutable one so we
  // can share it between both elements.
  // We can only do this if there are no presentation attributes and sharing the
  // data won't result in different case sensitivity of class or id.
  auto* unique_element_data =
      DynamicTo<UniqueElementData>(other.element_data_.Get());
  if (unique_element_data && !owner_documents_have_different_case_sensitivity &&
      !other.element_data_->PresentationAttributeStyle()) {
    const_cast<Element&>(other).element_data_ =
        unique_element_data->MakeShareableCopy();
  }

  if (!other.element_data_->IsUnique() &&
      !owner_documents_have_different_case_sensitivity &&
      !NeedsURLResolutionForInlineStyle(other, other.GetDocument(),
                                        GetDocument())) {
    element_data_ = other.element_data_;
  } else {
    element_data_ = other.element_data_->MakeUniqueCopy();
  }

  // Since we're going through the list of attributes now, we use the
  // opportunity to recreate the Bloom filter; in particular, it may
  // be different from the source's Bloom filter if it came from a document
  // with different quirks mode setting.
  Element* first_child = ElementTraversal::FirstChild(*this);
  if (!first_child) {
    attribute_or_class_bloom_ = 0;
  } else if (!first_child->nextSibling()) {
    attribute_or_class_bloom_ = first_child->attribute_or_class_bloom_;
  } else {
    // Two or more children left; we don't consider it worth it
    // to try to reset the filter fully.
  }
  for (const Attribute& attr : element_data_->Attributes()) {
    attribute_or_class_bloom_ |= FilterForAttribute(attr.GetName());
    AttributeChanged(
        AttributeModificationParams(attr.GetName(), g_null_atom, attr.Value(),
                                    AttributeModificationReason::kByCloning));
  }
  UpdateSubtreeBloomFilterAfterInsert();

  if (other.nonce() != g_null_atom) {
    setNonce(other.nonce());
  }
}

void Element::CreateUniqueElementData() {
  if (!element_data_) {
    element_data_ = MakeGarbageCollected<UniqueElementData>();
  } else {
    DCHECK(!IsA<UniqueElementData>(element_data_.Get()));
    element_data_ =
        To<ShareableElementData>(element_data_.Get())->MakeUniqueCopy();
  }
}

void Element::SynchronizeStyleAttributeInternal() const {
  DCHECK(IsStyledElement());
  DCHECK(HasElementData());
  DCHECK(GetElementData()->style_attribute_is_dirty());
  GetElementData()->SetStyleAttributeIsDirty(false);
  const CSSPropertyValueSet* inline_style = InlineStyle();
  const_cast<Element*>(this)->SetSynchronizedLazyAttribute(
      html_names::kStyleAttr,
      inline_style ? AtomicString(inline_style->AsText()) : g_empty_atom);
}

CSSStyleDeclaration* Element::style() {
  if (!IsStyledElement()) {
    return nullptr;
  }
  return &EnsureElementRareData().EnsureInlineCSSStyleDeclaration(this);
}

StylePropertyMap* Element::attributeStyleMap() {
  if (!IsStyledElement()) {
    return nullptr;
  }
  return &EnsureElementRareData().EnsureInlineStylePropertyMap(this);
}

StylePropertyMapReadOnly* Element::ComputedStyleMap() {
  return GetDocument().ComputedStyleMap(this);
}

MutableCSSPropertyValueSet& Element::EnsureMutableInlineStyle() {
  DCHECK(IsStyledElement());
  Member<CSSPropertyValueSet>& inline_style =
      EnsureUniqueElementData().inline_style_;
  if (!inline_style) {
    CSSParserMode mode = (!IsHTMLElement() || GetDocument().InQuirksMode())
                             ? kHTMLQuirksMode
                             : kHTMLStandardMode;
    inline_style = MakeGarbageCollected<MutableCSSPropertyValueSet>(mode);
  } else if (!inline_style->IsMutable()) {
    inline_style = inline_style->MutableCopy();
  }
  return *To<MutableCSSPropertyValueSet>(inline_style.Get());
}

void Element::ClearMutableInlineStyleIfEmpty() {
  if (EnsureMutableInlineStyle().IsEmpty()) {
    EnsureUniqueElementData().inline_style_.Clear();
  }
}

void Element::NotifyInlineStyleMutation() {
  if (GetLayoutObject() && GetLayoutObject()->PreviousVisibilityVisible() &&
      GetDocument().GetPage()) {
    GetDocument().GetPage()->Animator().SetHasInlineStyleMutation();
  }
}

inline void Element::SetInlineStyleFromString(
    const AtomicString& new_style_string) {
  DCHECK(IsStyledElement());
  Member<CSSPropertyValueSet>& inline_style = GetElementData()->inline_style_;

  // Avoid redundant work if we're using shared attribute data with already
  // parsed inline style.
  if (inline_style && !GetElementData()->IsUnique()) {
    return;
  }

  // We reconstruct the property set instead of mutating if there is no CSSOM
  // wrapper.  This makes wrapperless property sets immutable and so cacheable.
  if (inline_style && !inline_style->IsMutable()) {
    inline_style.Clear();
  }

  if (!inline_style) {
    inline_style =
        CSSParser::ParseInlineStyleDeclaration(new_style_string, this);
  } else {
    DCHECK(inline_style->IsMutable());
    static_cast<MutableCSSPropertyValueSet*>(inline_style.Get())
        ->ParseDeclarationList(
            new_style_string,
            GetExecutionContext()
                ? GetExecutionContext()->GetSecureContextMode()
                : SecureContextMode::kInsecureContext,
            GetDocument().ElementSheet().Contents());
  }
}

bool Element::IsStyleAttributeChangeAllowed(const AtomicString& style_string) {
  if (auto* shadow_root = ContainingShadowRoot()) {
    if (shadow_root->IsUserAgent()) {
      return true;
    }
  }

  if (auto* context = GetExecutionContext()) {
    if (auto* policy = context->GetContentSecurityPolicyForCurrentWorld()) {
      OrdinalNumber start_line_number = OrdinalNumber::BeforeFirst();
      auto& document = GetDocument();
      if (document.GetScriptableDocumentParser() &&
          !document.IsInDocumentWrite()) {
        start_line_number =
            document.GetScriptableDocumentParser()->LineNumber();
      }
      return policy->AllowInline(
          ContentSecurityPolicy::InlineType::kStyleAttribute, this,
          style_string, String() /* nonce */, document.Url(),
          start_line_number);
    }
  }
  return false;
}

void Element::StyleAttributeChanged(
    const AtomicString& new_style_string,
    AttributeModificationReason modification_reason) {
  DCHECK(IsStyledElement());

  if (new_style_string.IsNull()) {
    EnsureUniqueElementData().inline_style_.Clear();
  } else if (modification_reason == AttributeModificationReason::kByCloning ||
             IsStyleAttributeChangeAllowed(new_style_string)) {
    SetInlineStyleFromString(new_style_string);
  }

  GetElementData()->SetStyleAttributeIsDirty(false);

  SetNeedsStyleRecalc(kLocalStyleChange,
                      StyleChangeReasonForTracing::Create(
                          style_change_reason::kStyleAttributeChange));
  probe::DidInvalidateStyleAttr(this);
}

void Element::InlineStyleChanged() {
  // NOTE: This is conservative; we can be more precise it in the future
  // if need be.
  const bool only_changed_independent_properties = false;

  DCHECK(IsStyledElement());
  InvalidateStyleAttribute(only_changed_independent_properties);
  probe::DidInvalidateStyleAttr(this);

  if (MutationObserverInterestGroup* recipients =
          MutationObserverInterestGroup::CreateForAttributesMutation(
              *this, html_names::kStyleAttr)) {
    // We don't use getAttribute() here to get a style attribute value
    // before the change.
    AtomicString old_value;
    if (const Attribute* attribute =
            GetElementData()->Attributes().Find(html_names::kStyleAttr)) {
      old_value = attribute->Value();
    }
    recipients->EnqueueMutationRecord(MutationRecord::CreateAttributes(
        this, html_names::kStyleAttr, old_value));
    // Need to synchronize every time so that following MutationRecords will
    // have correct oldValues.
    SynchronizeAttribute(html_names::kStyleAttr);
  }
}

void Element::SetInlineStyleProperty(CSSPropertyID property_id,
                                     CSSValueID identifier,
                                     bool important) {
  DCHECK_NE(property_id, CSSPropertyID::kVariable);
  SetInlineStyleProperty(property_id, *CSSIdentifierValue::Create(identifier),
                         important);
}

void Element::SetInlineStyleProperty(CSSPropertyID property_id,
                                     double value,
                                     CSSPrimitiveValue::UnitType unit,
                                     bool important) {
  DCHECK_NE(property_id, CSSPropertyID::kVariable);
  SetInlineStyleProperty(
      property_id, *CSSNumericLiteralValue::Create(value, unit), important);
}

void Element::SetInlineStyleProperty(CSSPropertyID property_id,
                                     const CSSValue& value,
                                     bool important) {
  DCHECK_NE(property_id, CSSPropertyID::kVariable);
  DCHECK(IsStyledElement());
  EnsureMutableInlineStyle().SetProperty(property_id, value, important);
  InlineStyleChanged();
}

bool Element::SetInlineStyleProperty(CSSPropertyID property_id,
                                     const String& value,
                                     bool important) {
  DCHECK_NE(property_id, CSSPropertyID::kVariable);
  DCHECK(IsStyledElement());
  bool did_change =
      EnsureMutableInlineStyle().ParseAndSetProperty(
          property_id, value, important,
          GetExecutionContext() ? GetExecutionContext()->GetSecureContextMode()
                                : SecureContextMode::kInsecureContext,
          GetDocument().ElementSheet().Contents()) >=
      MutableCSSPropertyValueSet::kModifiedExisting;
  if (did_change) {
    InlineStyleChanged();
  }
  return did_change;
}

void Element::SetInlineStyleProperty(const CSSPropertyName& name,
                                     const CSSValue& value,
                                     bool important) {
  DCHECK(IsStyledElement());
  EnsureMutableInlineStyle().SetProperty(name, value, important);
  InlineStyleChanged();
}

bool Element::RemoveInlineStyleProperty(CSSPropertyID property_id) {
  DCHECK(IsStyledElement());
  if (!InlineStyle()) {
    return false;
  }
  bool did_change = EnsureMutableInlineStyle().RemoveProperty(property_id);
  if (did_change) {
    InlineStyleChanged();
  }
  return did_change;
}

bool Element::RemoveInlineStyleProperty(const AtomicString& property_name) {
  DCHECK(IsStyledElement());
  if (!InlineStyle()) {
    return false;
  }
  bool did_change = EnsureMutableInlineStyle().RemoveProperty(property_name);
  if (did_change) {
    InlineStyleChanged();
  }
  return did_change;
}

void Element::RemoveAllInlineStyleProperties() {
  DCHECK(IsStyledElement());
  if (!InlineStyle()) {
    return;
  }
  EnsureMutableInlineStyle().Clear();
  InlineStyleChanged();
}

void Element::UpdatePresentationAttributeStyle() {
  SynchronizeAllAttributes();
  element_data_->SetPresentationAttributeStyleIsDirty(false);
  element_data_->presentation_attribute_style_ =
      ComputePresentationAttributeStyle(*this);
}

CSSPropertyValueSet* Element::CreatePresentationAttributeStyle() {
  HeapVector<CSSPropertyValue, 8> values;
  AttributeCollection attributes = AttributesWithoutUpdate();
  for (const Attribute& attr : attributes) {
    CollectStyleForPresentationAttribute(attr.GetName(), attr.Value(), values);
  }
  CollectExtraStyleForPresentationAttribute(values);
  return ImmutableCSSPropertyValueSet::Create(
      values, IsSVGElement() ? kSVGAttributeMode : kHTMLStandardMode);
}

void Element::AddPropertyToPresentationAttributeStyle(
    HeapVector<CSSPropertyValue, 8>& values,
    CSSPropertyID property_id,
    CSSValueID identifier) {
  DCHECK(IsStyledElement());
  DCHECK_NE(property_id, CSSPropertyID::kWhiteSpace);
  DCHECK(!CSSProperty::Get(property_id).IsShorthand());
  values.emplace_back(CSSPropertyName(property_id),
                      *CSSIdentifierValue::Create(identifier));
}

void Element::AddPropertyToPresentationAttributeStyle(
    HeapVector<CSSPropertyValue, 8>& values,
    CSSPropertyID property_id,
    double value,
    CSSPrimitiveValue::UnitType unit) {
  DCHECK(IsStyledElement());
  DCHECK(!CSSProperty::Get(property_id).IsShorthand());
  values.emplace_back(CSSPropertyName(property_id),
                      *CSSNumericLiteralValue::Create(value, unit));
}

void Element::AddPropertyToPresentationAttributeStyle(
    HeapVector<CSSPropertyValue, 8>& values,
    CSSPropertyID property_id,
    const String& value) {
  DCHECK(IsStyledElement());
  CSSParser::ParseForPresentationStyle(
      values, property_id, value,
      IsSVGElement() ? kSVGAttributeMode : kHTMLStandardMode,
      GetDocument().ElementSheet().Contents(),
      /*execution_context=*/nullptr);
}

void Element::AddPropertyToPresentationAttributeStyle(
    HeapVector<CSSPropertyValue, 8>& values,
    CSSPropertyID property_id,
    const CSSValue& value) {
  DCHECK(IsStyledElement());
  DCHECK(!CSSProperty::Get(property_id).IsShorthand());
  values.emplace_back(CSSPropertyName(property_id), value);
}

void Element::MapLanguageAttributeToLocale(
    const AtomicString& value,
    HeapVector<CSSPropertyValue, 8>& style) {
  if (!value.empty()) {
    // Have to quote so the locale id is treated as a string instead of as a CSS
    // keyword.
    AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kWebkitLocale,
                                            SerializeString(value));

    // FIXME: Remove the following UseCounter code when we collect enough
    // data.
    UseCounter::Count(GetDocument(), WebFeature::kLangAttribute);
    if (IsA<HTMLHtmlElement>(this)) {
      UseCounter::Count(GetDocument(), WebFeature::kLangAttributeOnHTML);
    } else if (IsA<HTMLBodyElement>(this)) {
      UseCounter::Count(GetDocument(), WebFeature::kLangAttributeOnBody);
    }
    String html_language = value.GetString();
    wtf_size_t first_separator = html_language.find('-');
    if (first_separator != kNotFound) {
      html_language = html_language.Left(first_separator);
    }
    String ui_language = DefaultLanguage();
    first_separator = ui_language.find('-');
    if (first_separator != kNotFound) {
      ui_language = ui_language.Left(first_separator);
    }
    first_separator = ui_language.find('_');
    if (first_separator != kNotFound) {
      ui_language = ui_language.Left(first_separator);
    }
    if (!DeprecatedEqualIgnoringCase(html_language, ui_language)) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kLangAttributeDoesNotMatchToUILocale);
    }
  } else {
    // The empty string means the language is explicitly unknown.
    AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kWebkitLocale,
                                            CSSValueID::kAuto);
  }
}

void Element::LogAddElementIfIsolatedWorldAndInDocument(
    const char element[],
    const QualifiedName& attr1) {
  // TODO(crbug.com/361461518): Investigate the root cause of execution context
  // is unexpectedly null.
  if (!GetDocument().GetExecutionContext()) {
    return;
  }

  if (!isConnected() ||
      !V8DOMActivityLogger::HasActivityLoggerInIsolatedWorlds()) {
    return;
  }
  V8DOMActivityLogger* activity_logger =
      V8DOMActivityLogger::CurrentActivityLoggerIfIsolatedWorld(
          GetDocument().GetAgent().isolate());
  if (!activity_logger) {
    return;
  }
  Vector<String, 2> argv;
  argv.push_back(element);
  argv.push_back(FastGetAttribute(attr1));
  activity_logger->LogEvent(GetDocument().GetExecutionContext(),
                            "blinkAddElement", argv);
}

void Element::LogAddElementIfIsolatedWorldAndInDocument(
    const char element[],
    const QualifiedName& attr1,
    const QualifiedName& attr2) {
  // TODO(crbug.com/361461518): Investigate the root cause of execution context
  // is unexpectedly null.
  if (!GetDocument().GetExecutionContext()) {
    return;
  }

  if (!isConnected() ||
      !V8DOMActivityLogger::HasActivityLoggerInIsolatedWorlds()) {
    return;
  }
  V8DOMActivityLogger* activity_logger =
      V8DOMActivityLogger::CurrentActivityLoggerIfIsolatedWorld(
          GetDocument().GetAgent().isolate());
  if (!activity_logger) {
    return;
  }
  Vector<String, 3> argv;
  argv.push_back(element);
  argv.push_back(FastGetAttribute(attr1));
  argv.push_back(FastGetAttribute(attr2));
  activity_logger->LogEvent(GetDocument().GetExecutionContext(),
                            "blinkAddElement", argv);
}

void Element::LogAddElementIfIsolatedWorldAndInDocument(
    const char element[],
    const QualifiedName& attr1,
    const QualifiedName& attr2,
    const QualifiedName& attr3) {
  // TODO(crbug.com/361461518): Investigate the root cause of execution context
  // is unexpectedly null.
  if (!GetDocument().GetExecutionContext()) {
    return;
  }

  if (!isConnected() ||
      !V8DOMActivityLogger::HasActivityLoggerInIsolatedWorlds()) {
    return;
  }
  V8DOMActivityLogger* activity_logger =
      V8DOMActivityLogger::CurrentActivityLoggerIfIsolatedWorld(
          GetDocument().GetAgent().isolate());
  if (!activity_logger) {
    return;
  }
  Vector<String, 4> argv;
  argv.push_back(element);
  argv.push_back(FastGetAttribute(attr1));
  argv.push_back(FastGetAttribute(attr2));
  argv.push_back(FastGetAttribute(attr3));
  activity_logger->LogEvent(GetDocument().GetExecutionContext(),
                            "blinkAddElement", argv);
}

void Element::LogUpdateAttributeIfIsolatedWorldAndInDocument(
    const char element[],
    const AttributeModificationParams& params) {
  if (!isConnected() ||
      !V8DOMActivityLogger::HasActivityLoggerInIsolatedWorlds()) {
    return;
  }
  V8DOMActivityLogger* activity_logger =
      V8DOMActivityLogger::CurrentActivityLoggerIfIsolatedWorld(
          GetDocument().GetAgent().isolate());
  if (!activity_logger) {
    return;
  }
  Vector<String, 4> argv;
  argv.push_back(element);
  argv.push_back(params.name.ToString());
  argv.push_back(params.old_value);
  argv.push_back(params.new_value);
  activity_logger->LogEvent(GetDocument().GetExecutionContext(),
                            "blinkSetAttribute", argv);
}

void Element::Trace(Visitor* visitor) const {
  visitor->Trace(computed_style_);
  visitor->Trace(element_data_);
  ContainerNode::Trace(visitor);
}

bool Element::HasPart() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    if (auto* part = data->GetPart()) {
      return part->length() > 0;
    }
  }
  return false;
}

DOMTokenList* Element::GetPart() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetPart();
  }
  return nullptr;
}

DOMTokenList& Element::part() {
  ElementRareDataVector& rare_data = EnsureElementRareData();
  DOMTokenList* part = rare_data.GetPart();
  if (!part) {
    part = MakeGarbageCollected<DOMTokenList>(*this, html_names::kPartAttr);
    rare_data.SetPart(part);
  }
  return *part;
}

bool Element::HasPartNamesMap() const {
  const NamesMap* names_map = PartNamesMap();
  return names_map && names_map->size() > 0;
}

const NamesMap* Element::PartNamesMap() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->PartNamesMap();
  }
  return nullptr;
}

bool Element::ChildStyleRecalcBlockedByDisplayLock() const {
  auto* context = GetDisplayLockContext();
  return context && !context->ShouldStyleChildren();
}

void Element::ChangeInterestState(Element* target, InterestState new_state) {
  DCHECK_NE(this, target);
  InterestState current_state = GetInterestState();
  if (new_state == current_state) {
    return;
  }
  InvokerData* invoker_data = &EnsureElementRareData().EnsureInvokerData();
  auto& document = GetDocument();
  if (new_state == InterestState::kNoInterest) {
    DCHECK(document.ElementsWithInterest().Contains(this));
    document.ElementsWithInterest().erase(this);
    invoker_data->SetInterestState(InterestState::kNoInterest);
    invoker_data->SetActiveInterestTarget(nullptr);
    if (target) {
      target->RemoveInterestInvokerTargetData();
    }
    // Cancel any tasks that might be running.
    invoker_data->CancelInterestLostTask();
    invoker_data->CancelInterestGainedTask();
  } else {
    DCHECK(!document.ElementsWithInterest().Contains(this));
    document.ElementsWithInterest().insert(this);
    invoker_data->SetInterestState(new_state);
    invoker_data->SetActiveInterestTarget(target);
  }
  PseudoStateChanged(CSSSelector::kPseudoInterestSource);
  if (target) {
    target->PseudoStateChanged(CSSSelector::kPseudoInterestTarget);
  }
}

void Element::ScheduleInterestGainedTask() {
  // This should be called on an interest invoker only.
  auto* target = InterestForElement();
  CHECK(target);
  const ComputedStyle* style =
      ComputedStyle::NullifyEnsured(GetComputedStyle());
  if (!style) {
    return;
  }
  StyleInterestDelay show_delay = style->InterestDelayStart();
  float show_delay_seconds = show_delay.IsNormal()
                                 ? kDefaultInterestDelayStartSeconds
                                 : show_delay.DelaySeconds();
  // If the value is infinite or NaN, don't schedule showing interest.
  if (!std::isfinite(show_delay_seconds)) {
    return;
  }
  CHECK_GE(show_delay_seconds, 0);
  auto& invoker_data = EnsureInvokerData();
  invoker_data.CancelInterestGainedTask();
  // TODO(https://crbug.com/326681249): Perhaps there should be a task runner
  // specific to these events, rather than kMiscPlatformAPI?
  invoker_data.SetInterestGainedTask(PostDelayedCancellableTask(
      *GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI),
      FROM_HERE,
      BindOnce(
          [](Element* invoker, Element* target) {
            if (invoker) {
              invoker->InterestGained(target);
            }
          },
          WrapWeakPersistent(this), WrapWeakPersistent(target)),
      base::Seconds(show_delay_seconds)));
}

void Element::ScheduleInterestLostTask() {
  const ComputedStyle* style =
      ComputedStyle::NullifyEnsured(GetComputedStyle());
  if (!style) {
    return;
  }

  StyleInterestDelay hide_delay = style->InterestDelayEnd();
  float hide_delay_seconds = hide_delay.IsNormal()
                                 ? kDefaultInterestDelayEndSeconds
                                 : hide_delay.DelaySeconds();
  // If the value is infinite or NaN, don't schedule losing interest.
  if (!std::isfinite(hide_delay_seconds)) {
    return;
  }
  CHECK_GE(hide_delay_seconds, 0);
  auto& invoker_data = EnsureInvokerData();
  invoker_data.CancelInterestLostTask();
  // TODO(https://crbug.com/326681249): Perhaps there should be a task runner
  // specific to these events, rather than kMiscPlatformAPI?
  invoker_data.SetInterestLostTask(PostDelayedCancellableTask(
      *GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI),
      FROM_HERE,
      BindOnce(
          [](Element* invoker, Element* target) {
            if (invoker) {
              invoker->InterestLost(target);
            }
          },
          WrapWeakPersistent(this),
          WrapWeakPersistent(invoker_data.ActiveInterestTarget())),
      base::Seconds(hide_delay_seconds)));
}

Element* Element::InterestForElement() const {
  if (!RuntimeEnabledFeatures::HTMLInterestForAttributeEnabled(
          GetDocument().GetExecutionContext())) {
    return nullptr;
  }
  Element* target =
      GetElementAttributeResolvingReferenceTarget(html_names::kInterestforAttr);
  if (!target) {
    return nullptr;
  }
  // An interest invoker relationship isn't valid if the source or target
  // element aren't in a TreeScope.
  if (!IsInTreeScope() || !target->IsInTreeScope()) {
    return nullptr;
  }
  // Check element-specific preconditions.
  if (!IsValidInterestInvoker(*target)) {
    return nullptr;
  }
  return target;
}

Element* Element::SourceInterestInvoker() const {
  if (!RuntimeEnabledFeatures::HTMLInterestForAttributeEnabled()) {
    return nullptr;
  }
  InterestInvokerTargetData* target_data = GetInterestInvokerTargetData();
  if (!target_data) {
    return nullptr;
  }
  Element* invoker = target_data->interestInvoker();
  if (!invoker) {
    return nullptr;
  }
  // The InterestForElement() could be nullptr if the invoker is not in the
  // tree, or not in an active document.
  if (!invoker->InterestForElement()) {
    return nullptr;
  }
  DCHECK_EQ(invoker->InterestForElement(), this);
  DCHECK_NE(invoker->GetInterestState(), InterestState::kNoInterest);
  return invoker;
}

Element::InterestState Element::GetInterestState() {
  if (!RuntimeEnabledFeatures::HTMLInterestForAttributeEnabled()) {
    return InterestState::kNoInterest;
  }
  auto* invoker_data = GetInvokerData();
  if (!invoker_data) {
    return InterestState::kNoInterest;
  }
  return invoker_data->GetInterestState();
}

namespace {

void AllSourceInterestInvokersRecursive(
    Element& target,
    HeapLinkedHashSet<Member<Element>>& sources) {
  DCHECK(RuntimeEnabledFeatures::HTMLInterestForAttributeEnabled());
  if (Element* upstream = target.SourceInterestInvoker();
      upstream && !sources.Contains(upstream)) {
    DCHECK_NE(&target, upstream);
    sources.insert(upstream);
    AllSourceInterestInvokersRecursive(*upstream, sources);
  }
  if (Element* parent = target.parentElement();
      parent && !sources.Contains(parent)) {
    AllSourceInterestInvokersRecursive(*parent, sources);
  }
}

HeapLinkedHashSet<Member<Element>> AllSourceInterestInvokers(Element& target) {
  HeapLinkedHashSet<Member<Element>> sources;
  AllSourceInterestInvokersRecursive(target, sources);
  return sources;
}

}  // namespace

// Mechanics of `interestfor` invokers ("interest invokers"):
//  - It is possible for there to be nested DOM elements that both have the
//    `interestfor` attribute, in which case, *both* can be active at
//    once, when the innermost one is hovered/focused (e.g. because a hover on
//    a descendent element also triggers hover on the ancestor).
//  - It is also possible for one element to be both an interest invoker
//    *and* the target of an interest invoker.
//  - When hover changes, SetHovered(false) is first called on the entire
//    element chain "losing" hover, starting from the leaf and working up to
//    the common ancestor of the element chain "gaining" hover. Then
//    SetHovered(true) is called on the element chain "gaining" hover.
//  - When focus changes, SetFocused(false) is called on just the element
//    losing focus (not any ancestors), and then SetFocused(true) is called on
//    the element gaining focus. Because the ancestor chain is not automatically
//    notified, this function must walk the ancestors manually.
void Element::HandleInterestForHoverOrFocus(InterestSource source,
                                            bool recursive_call) {
  DCHECK(RuntimeEnabledFeatures::HTMLInterestForAttributeEnabled());
  if (!IsInTreeScope() || !GetDocument().IsActive()) {
    return;
  }
  // We manually "bubble" all calls to this function to all ancestors.
  if (!recursive_call) {
    for (auto& node : FlatTreeTraversal::InclusiveAncestorsOf(*this)) {
      if (Element* element = DynamicTo<Element>(node)) {
        element->HandleInterestForHoverOrFocus(source,
                                               /*recursive_call*/ true);
      }
    }
    return;
  }

  InvokerData* invoker_data = GetInvokerData();
  Element* upstream_invoker = SourceInterestInvoker();
  InvokerData* upstream_data =
      upstream_invoker ? upstream_invoker->GetInvokerData() : nullptr;
  DCHECK(!upstream_invoker ||
         (upstream_invoker->InterestForElement() == this &&
          upstream_data->ActiveInterestTarget() == this &&
          upstream_invoker->GetInvokerData()->GetInterestState() !=
              InterestState::kNoInterest));
  if (source == InterestSource::kHover || source == InterestSource::kFocus) {
    if (invoker_data) [[unlikely]] {
      invoker_data->CancelInterestLostTask();
    }
    for (Member<Element> upstream : AllSourceInterestInvokers(*this)) {
      upstream->GetInvokerData()->CancelInterestLostTask();
    }
    if (auto* target = InterestForElement();
        target && (!invoker_data || invoker_data->GetInterestState() ==
                                        InterestState::kNoInterest))
        [[unlikely]] {
      // This is an interest invoker that doesn't already have interest, and was
      // just hovered or focused. Schedule an InterestGained task.
      ScheduleInterestGainedTask();
    }
  } else {
    DCHECK(source == InterestSource::kDeHover ||
           source == InterestSource::kBlur);
    if (invoker_data) [[unlikely]] {
      // This is an interest invoker which was just de-hovered or blurred.
      // Cancel any pending InterestGained tasks, and (if the invoker already
      // has interest) schedule an InterestLost task.
      invoker_data->CancelInterestGainedTask();
      if (invoker_data->GetInterestState() != InterestState::kNoInterest) {
        ScheduleInterestLostTask();
      }
    }
    for (Member<Element> upstream : AllSourceInterestInvokers(*this)) {
      // This is the target of an interest invoker, which was just de-hovered or
      // blurred. Schedule an InterestLost task.
      upstream->ScheduleInterestLostTask();
    }
  }
}

void Element::SetHovered(bool hovered) {
  if (hovered == IsHovered()) {
    return;
  }

  Document& document = GetDocument();
  document.UserActionElements().SetHovered(this, hovered);

  const ComputedStyle* style = GetComputedStyle();
  if (!style || style->AffectedByHover()) {
    StyleChangeType change_type = kLocalStyleChange;
    if (style && style->HasPseudoElementStyle(kPseudoIdFirstLetter)) {
      change_type = kSubtreeStyleChange;
    }
    SetNeedsStyleRecalc(change_type,
                        StyleChangeReasonForTracing::CreateWithExtraData(
                            style_change_reason::kPseudoClass,
                            style_change_extra_data::g_hover));
  }
  PseudoStateChanged(CSSSelector::kPseudoHover);

  InvalidateIfHasEffectiveAppearance();
}

void Element::SetActive(bool active) {
  if (active == IsActive()) {
    return;
  }

  GetDocument().UserActionElements().SetActive(this, active);

  if (!GetLayoutObject()) {
    if (!ChildrenOrSiblingsAffectedByActive()) {
      SetNeedsStyleRecalc(kLocalStyleChange,
                          StyleChangeReasonForTracing::CreateWithExtraData(
                              style_change_reason::kPseudoClass,
                              style_change_extra_data::g_active));
    }
    PseudoStateChanged(CSSSelector::kPseudoActive);
    return;
  }

  if (GetComputedStyle()->AffectedByActive()) {
    StyleChangeType change_type =
        GetComputedStyle()->HasPseudoElementStyle(kPseudoIdFirstLetter)
            ? kSubtreeStyleChange
            : kLocalStyleChange;
    SetNeedsStyleRecalc(change_type,
                        StyleChangeReasonForTracing::CreateWithExtraData(
                            style_change_reason::kPseudoClass,
                            style_change_extra_data::g_active));
  }
  PseudoStateChanged(CSSSelector::kPseudoActive);

  if (!IsDisabledFormControl()) {
    InvalidateIfHasEffectiveAppearance();
  }
}

void Element::InvalidateStyleAttribute(
    bool only_changed_independent_properties) {
  DCHECK(HasElementData());
  GetElementData()->SetStyleAttributeIsDirty(true);
  SetNeedsStyleRecalc(only_changed_independent_properties
                          ? kInlineIndependentStyleChange
                          : kLocalStyleChange,
                      StyleChangeReasonForTracing::Create(
                          style_change_reason::kInlineCSSStyleMutated));
  GetDocument().GetStyleEngine().AttributeChangedForElement(
      html_names::kStyleAttr, *this);
}

void Element::UpdateOverscrollPseudoElements(
    const StyleRecalcChange style_recalc_change,
    const StyleRecalcContext& style_recalc_context) {
  size_t overscroll_area_count = 0;
  if (const ComputedStyle* computed_style = GetComputedStyle()) {
    if (const ScopedCSSNameList* overscroll_area =
            computed_style->OverscrollArea()) {
      overscroll_area_count = overscroll_area->GetNames().size();
    }
  }

  ElementRareDataVector* data = GetElementRareData();
  const OverscrollAreaParentPseudoElementsVector* overscroll_elements =
      data ? data->GetOverscrollAreaParentPseudoElements() : nullptr;

  // Detect if the declared overscroll areas have changed.
  wtf_size_t current_overscroll_area_count =
      overscroll_elements ? overscroll_elements->size() : 0;
  bool overscroll_areas_changed =
      overscroll_area_count != current_overscroll_area_count;
  if (!overscroll_areas_changed) {
    return;
  }

  if (data) {
    data->ClearOverscrollPseudoElements(/* to_keep */ 0);
  }
  if (overscroll_area_count == 0) {
    return;
  }

  const ScopedCSSNameList* overscroll_area =
      GetComputedStyle()->OverscrollArea();
  wtf_size_t index = 0;
  for (const ScopedCSSName* name : overscroll_area->GetNames()) {
    IndexedPseudoElement* pseudo_element =
        MakeGarbageCollected<IndexedPseudoElement>(
            this, kPseudoIdOverscrollAreaParent, index, name->GetName());
    CHECK(SetAssociatedPseudoElement(pseudo_element, style_recalc_context));
    pseudo_element->SetNeedsReattachLayoutTree();
    ++index;
  }
}

void Element::UpdateTransitionPseudoElements(
    const StyleRecalcChange style_recalc_change,
    const StyleRecalcContext& style_recalc_context) {
  const auto* transition = ViewTransitionUtils::GetTransition(*this);

  if (!IsPseudoElement()) {
    PseudoElement* old_transition_pseudo =
        GetPseudoElement(kPseudoIdViewTransition);
    if (old_transition_pseudo &&
        (!transition ||
         !transition->IsGeneratingPseudo(
             To<ViewTransitionPseudoElementBase>(*old_transition_pseudo)))) {
      ClearPseudoElement(kPseudoIdViewTransition);
      // If the transition still exists, it is no longer bound to the style
      // tracker as it is finished.
      transition = nullptr;
    }

    if (!transition) {
      return;
    }

    bool had_transition_pseudo = !!GetPseudoElement(kPseudoIdViewTransition);
    PseudoElement* transition_pseudo =
        UpdatePseudoElement(kPseudoIdViewTransition, style_recalc_change,
                            style_recalc_context, g_null_atom);
    if (transition_pseudo && !had_transition_pseudo) {
      transition_pseudo->UpdateTransitionPseudoElements(style_recalc_change,
                                                        style_recalc_context);
    }
    return;
  }

  if (!IsTransitionPseudoElement(GetPseudoId())) {
    return;
  }

  ViewTransitionPseudoElementBase* transition_pseudo =
      To<ViewTransitionPseudoElementBase>(this);

  if (!transition || !transition->IsGeneratingPseudo(*transition_pseudo)) {
    return;
  }

  switch (GetPseudoId()) {
    case kPseudoIdViewTransition: {
      for (const AtomicString& name :
           transition_pseudo->GetViewTransitionNames()) {
        bool had_group = !!GetPseudoElement(kPseudoIdViewTransitionGroup, name);
        const AtomicString& containing_group_name =
            transition_pseudo->GetContainingGroupName(name);
        if (containing_group_name == g_null_atom) {
          PseudoElement* group = UpdatePseudoElement(
              kPseudoIdViewTransitionGroup, style_recalc_change,
              style_recalc_context, name);
          if (group && !had_group) {
            group->UpdateTransitionPseudoElements(style_recalc_change,
                                                  style_recalc_context);
          }
        } else if (had_group) {
          // During the initial capture phase, the view-transition names are in
          // a flat list. The second capture (post DOM update) will contain the
          // nested hierarchy.
          ClearPseudoElement(kPseudoIdViewTransitionGroup, name);
        }
      }
      break;
    }

    case kPseudoIdViewTransitionGroupChildren: {
      for (const AtomicString& name :
           transition_pseudo->GetViewTransitionNames()) {
        if (transition_pseudo->GetContainingGroupName(name) ==
            transition_pseudo->view_transition_name()) {
          bool had_group =
              !!GetPseudoElement(kPseudoIdViewTransitionGroup, name);
          PseudoElement* group = UpdatePseudoElement(
              kPseudoIdViewTransitionGroup, style_recalc_change,
              style_recalc_context, name);
          if (group && !had_group) {
            group->UpdateTransitionPseudoElements(style_recalc_change,
                                                  style_recalc_context);
          }
        }
      }
      break;
    }

    case kPseudoIdViewTransitionGroup: {
      const AtomicString& group_name =
          transition_pseudo->view_transition_name();
      bool had_image_pair =
          GetPseudoElement(kPseudoIdViewTransitionImagePair, group_name);
      PseudoElement* image_pair = UpdatePseudoElement(
          kPseudoIdViewTransitionImagePair, style_recalc_change,
          style_recalc_context, group_name);
      if (image_pair && !had_image_pair) {
        image_pair->UpdateTransitionPseudoElements(style_recalc_change,
                                                   style_recalc_context);
      }
      bool has_nested_groups = false;
      for (const AtomicString& name :
           transition_pseudo->GetViewTransitionNames()) {
        if (transition_pseudo->GetContainingGroupName(name) == group_name) {
          has_nested_groups = true;
          break;
        }
      }
      // Update view-transition-group-children if existing, but do not create
      // otherwise. Creation is handled above when navigating over the view-
      // transition names.
      if (has_nested_groups) {
        bool had_group_children =
            GetPseudoElement(kPseudoIdViewTransitionGroupChildren, group_name);
        PseudoElement* group_children = UpdatePseudoElement(
            kPseudoIdViewTransitionGroupChildren, style_recalc_change,
            style_recalc_context, group_name);
        if (group_children && !had_group_children) {
          group_children->UpdateTransitionPseudoElements(style_recalc_change,
                                                         style_recalc_context);
        }
      }
      break;
    }

    case kPseudoIdViewTransitionImagePair: {
      const AtomicString& group_name =
          transition_pseudo->view_transition_name();
      UpdatePseudoElement(kPseudoIdViewTransitionOld, style_recalc_change,
                          style_recalc_context, group_name);
      UpdatePseudoElement(kPseudoIdViewTransitionNew, style_recalc_change,
                          style_recalc_context, group_name);
      break;
    }

    case kPseudoIdViewTransitionOld:
    case kPseudoIdViewTransitionNew:
      break;

    default:
      NOTREACHED();
  }
}

bool Element::IsInertRoot() const {
  return FastHasAttribute(html_names::kInertAttr) && IsHTMLElement();
}

FocusgroupData Element::GetFocusgroupData() const {
  ExecutionContext* context = GetExecutionContext();
  if (!RuntimeEnabledFeatures::FocusgroupEnabled(context)) {
    return {};
  }
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetFocusgroupData();
  }
  return {};
}

void Element::SetFocusgroupLastFocused(Element& element) {
  // We can't have a focusgroup at all if the feature is not enabled.
  DCHECK(RuntimeEnabledFeatures::FocusgroupEnabled(GetExecutionContext()));
  // It only makes sense to set this on a focusgroup that supports memory (no
  // memory flag should not be set).
  DCHECK(IsActualFocusgroup(GetFocusgroupData()));
  DCHECK(!(GetFocusgroupData().flags & FocusgroupFlags::kNoMemory));
  EnsureElementRareData().SetFocusgroupLastFocused(&element);
}

void Element::ClearFocusgroupLastFocused() {
  ExecutionContext* context = GetExecutionContext();
  if (!RuntimeEnabledFeatures::FocusgroupEnabled(context)) {
    return;
  }
  if (ElementRareDataVector* data = GetElementRareData()) {
    data->ClearFocusgroupLastFocused();
  }
}

Element* Element::GetFocusgroupLastFocused() const {
  ExecutionContext* context = GetExecutionContext();
  if (!RuntimeEnabledFeatures::FocusgroupEnabled(context)) {
    return nullptr;
  }
  // It only makes sense to check this on a focusgroup.
  DCHECK(IsActualFocusgroup(GetFocusgroupData()));
  if (GetFocusgroupData().flags & FocusgroupFlags::kNoMemory) {
    return nullptr;
  }
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetFocusgroupLastFocused();
  }
  return nullptr;
}

bool Element::checkVisibility(CheckVisibilityOptions* options) const {
  if (options->checkVisibilityCSS()) {
    UseCounter::Count(
        GetDocument(),
        WebFeature::kElementCheckVisibilityOptionCheckVisibilityCSS);
  }
  if (options->checkOpacity()) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kElementCheckVisibilityOptionCheckOpacity);
  }
  if (options->contentVisibilityAuto()) {
    UseCounter::Count(
        GetDocument(),
        WebFeature::kElementCheckVisibilityOptionContentVisibilityAuto);
  }
  if (options->opacityProperty()) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kElementCheckVisibilityOptionOpacityProperty);
  }
  if (options->visibilityProperty()) {
    UseCounter::Count(
        GetDocument(),
        WebFeature::kElementCheckVisibilityOptionVisibilityProperty);
  }

  // If we're checking content-visibility: auto, then we can just check if we're
  // display locked at all. This is because, content-visibility: hidden is
  // always checked, so regardless of _why_ we're locked, the answer will be
  // false if we're locked.
  if (RuntimeEnabledFeatures::CheckVisibilityExtraPropertiesEnabled() &&
      options->contentVisibilityAuto() &&
      DisplayLockUtilities::IsDisplayLockedPreventingPaint(this)) {
    return false;
  }

  // Now, unlock ancestor content-visibility:auto elements. If this element is
  // offscreen and locked due to content-visibility:auto, this method should not
  // count that as invisible. That's checked above.
  DisplayLockUtilities::ScopedForcedUpdate force_locks(
      this, DisplayLockContext::ForcedPhase::kStyleAndLayoutTree,
      /*include_self=*/false, /*only_cv_auto=*/true,
      /*emit_warnings=*/false);
  GetDocument().UpdateStyleAndLayoutTree();

  if (!GetLayoutObject()) {
    return false;
  }

  auto* style = GetComputedStyle();
  if (!style) {
    return false;
  }

  DCHECK(options);
  if ((options->checkVisibilityCSS() ||
       (RuntimeEnabledFeatures::CheckVisibilityExtraPropertiesEnabled() &&
        options->visibilityProperty())) &&
      style->Visibility() != EVisibility::kVisible) {
    return false;
  }

  for (Node& ancestor : FlatTreeTraversal::InclusiveAncestorsOf(*this)) {
    if (Element* ancestor_element = DynamicTo<Element>(ancestor)) {
      // Check for content-visibility:hidden
      if (ancestor_element != this) {
        if (auto* lock = ancestor_element->GetDisplayLockContext()) {
          if (lock->IsLocked() &&
              !lock->IsActivatable(DisplayLockActivationReason::kViewport)) {
            return false;
          }
        }
      }

      // Check for opacity:0
      if (options->checkOpacity() ||
          (RuntimeEnabledFeatures::CheckVisibilityExtraPropertiesEnabled() &&
           options->opacityProperty())) {
        if (style = ancestor_element->GetComputedStyle(); style) {
          if (style->Opacity() == 0.f) {
            return false;
          }
        }
      }
    }
  }

  return true;
}

AtomicStringTable::WeakResult Element::WeakLowercaseIfNecessary(
    const AtomicString& name) const {
  if (name.IsLowerASCII()) [[likely]] {
    return AtomicStringTable::WeakResult(name);
  }
  if (IsHTMLElement() && IsA<HTMLDocument>(GetDocument())) [[likely]] {
    return AtomicStringTable::Instance().WeakFindLowercase(name);
  }
  return AtomicStringTable::WeakResult(name);
}

// Note, SynchronizeAttributeHinted is safe to call between a WeakFind() and
// a check on the AttributeCollection for the element even though it may
// modify the AttributeCollection to insert a "style" attribute. The reason
// is because html_names::kStyleAttr.LocalName() is an AtomicString
// representing "style". This means the AtomicStringTable will always have
// an entry for "style" and a `hint` that corresponds to
// html_names::kStyleAttr.LocalName() will never refer to a deleted object
// thus it is safe to insert html_names::kStyleAttr.LocalName() into the
// AttributeCollection collection after the WeakFind() when `hint` is
// referring to "style". A subsequent lookup will match itself correctly
// without worry for UaF or false positives.
void Element::SynchronizeAttributeHinted(
    const AtomicString& local_name,
    AtomicStringTable::WeakResult hint) const {
  // This version of SynchronizeAttribute() is streamlined for the case where
  // you don't have a full QualifiedName, e.g when called from DOM API.
  if (!HasElementData()) {
    return;
  }
  // TODO(ajwong): Does this unnecessarily synchronize style attributes on
  // SVGElements?
  if (GetElementData()->style_attribute_is_dirty() &&
      hint == html_names::kStyleAttr.LocalName()) {
    DCHECK(IsStyledElement());
    SynchronizeStyleAttributeInternal();
    return;
  }
  if (GetElementData()->svg_attributes_are_dirty()) {
    // We're passing a null namespace argument. svg_names::k*Attr are defined in
    // the null namespace, but for attributes that are not (like 'href' in the
    // XLink NS), this will not do the right thing.

    // TODO(fs): svg_attributes_are_dirty_ stays dirty unless
    // SynchronizeAllSVGAttributes is called. This means that even if
    // Element::SynchronizeAttribute() is called on all attributes,
    // svg_attributes_are_dirty_ remains true.
    To<SVGElement>(this)->SynchronizeSVGAttribute(QualifiedName(local_name));
  }
}

const AtomicString& Element::GetAttributeHinted(
    const AtomicString& name,
    AtomicStringTable::WeakResult hint) const {
  if (!HasElementData()) {
    return g_null_atom;
  }
  SynchronizeAttributeHinted(name, hint);
  if (const Attribute* attribute =
          GetElementData()->Attributes().FindHinted(name, hint)) {
    return attribute->Value();
  }
  return g_null_atom;
}

std::pair<wtf_size_t, const QualifiedName> Element::LookupAttributeQNameHinted(
    AtomicString name,
    AtomicStringTable::WeakResult hint) const {
  if (!HasElementData()) {
    return std::make_pair(kNotFound,
                          QualifiedName(LowercaseIfNecessary(std::move(name))));
  }

  AttributeCollection attributes = GetElementData()->Attributes();
  wtf_size_t index = attributes.FindIndexHinted(name, hint);
  return std::make_pair(
      index, index != kNotFound
                 ? attributes[index].GetName()
                 : QualifiedName(LowercaseIfNecessary(std::move(name))));
}

ALWAYS_INLINE wtf_size_t
Element::ValidateAttributeIndex(wtf_size_t index,
                                const QualifiedName& qname) const {
  // Checks whether attributes[index] points to qname, and re-calculates
  // index if not. This is necessary to accommodate cases where the element
  // is modified *while* we are setting an attribute.
  //
  // See https://crbug.com/333739948.

  if (index == kNotFound) {
    return RuntimeEnabledFeatures::TrustedTypesHTMLEnabled()
               ? FindAttributeIndex(qname)
               : index;
  }

  // If we previously found an attribute, we must also have attribute data.
  DCHECK(HasElementData());

  const AttributeCollection& attributes = GetElementData()->Attributes();
  if (index < attributes.size() && attributes[index].Matches(qname)) {
    return index;
  }

  return FindAttributeIndex(qname);
}

void Element::SetAttributeWithoutValidation(const QualifiedName& name,
                                            const AtomicString& value) {
  SynchronizeAttribute(name);
  SetAttributeInternal(FindAttributeIndex(name), name, value,
                       AttributeModificationReason::kDirectly);
}

void Element::SetAttributeWithValidation(Attr* attribute,
                                         const AtomicString& value,
                                         ExceptionState& exception_state) {
  if (RuntimeEnabledFeatures::TrustedTypesHTMLEnabled()) {
    CHECK(attribute);
    // Corresponds to:
    // https://whatpr.org/dom/1268.html#set-an-existing-attribute-value
    // Eventually: https://dom.spec.whatwg.org/#set-an-existing-attribute-value

    // Step 1: If attribute's element is null, then set attribute [...]
    // Note: Was performed by the caller.
    // Step 2.1: Let originalElement be attribute's element.
    // Note: originalElement is this. We already remember that.

    // Step 2.2: Let verifiedValue be [..] verify attribute value [...].
    AtomicString verified_value = AtomicString(TrustedTypesCheckForAttribute(
        attribute->GetQualifiedName(), value, "setAttribute", exception_state));
    if (exception_state.HadException()) {
      return;
    }

    // Step 2.3: If attribute’s element is null, then set attribute’s value to
    // value, and return.
    // Note: Step 2.2 might have changed element_.
    // Note: Without an owner element, attribute should accept the value without
    //       additional checking.
    if (!attribute->ownerElement()) {
      attribute->setValue(verified_value, ASSERT_NO_EXCEPTION);
      return;
    }

    // Step 2.4: If attribute’s element is not originalElement, then return.
    if (attribute->ownerElement() != this) {
      return;
    }

    // Step 2.5: Change attribute to verifiedValue.
    // Note: We've done all the validations here, so we can now call 'without':
    SetAttributeWithoutValidation(attribute->GetQualifiedName(),
                                  verified_value);
    return;
  }

  // Legacy behaviour. To be removed once TrustedTypesHTML is perma-enabled.
  const QualifiedName name = attribute->GetQualifiedName();
  SynchronizeAttribute(name);

  AtomicString trusted_value(TrustedTypesCheckForAttribute(
      name, value, "setAttribute", exception_state));
  if (exception_state.HadException()) {
    return;
  }

  SetAttributeInternal(FindAttributeIndex(name), name, trusted_value,
                       AttributeModificationReason::kDirectly);
}

void Element::SetSynchronizedLazyAttribute(const QualifiedName& name,
                                           const AtomicString& value) {
  SetAttributeInternal(
      FindAttributeIndex(name), name, value,
      AttributeModificationReason::kBySynchronizationOfLazyAttribute);
}

void Element::SetAttributeHinted(AtomicString local_name,
                                 AtomicStringTable::WeakResult hint,
                                 String value,
                                 ExceptionState& exception_state) {
  bool is_valid = RuntimeEnabledFeatures::RelaxDOMValidNamesEnabled()
                      ? Document::IsValidAttributeLocalNameNewSpec(local_name)
                      : Document::IsValidName(local_name);
  if (!is_valid) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        StrCat({"'", local_name, "' is not a valid attribute name."}));
    return;
  }
  SynchronizeAttributeHinted(local_name, hint);

  auto [index, q_name] =
      LookupAttributeQNameHinted(std::move(local_name), hint);

  // This method is probably the most common case for `setAttribute`.
  // For performance reasons, we'll skip the TT check if we can determine it's
  // unnecessary based on a quick heuristic.
  if (q_name.LocalName().StartsWith("on") ||
      !GetCheckedAttributeTypes().empty()) [[unlikely]] {
    value = TrustedTypesCheckForAttribute(q_name, std::move(value),
                                          "setAttribute", exception_state);
    if (exception_state.HadException()) {
      return;
    }
    // The `TrustedTypesCheckFor` call above may run script, which may modify
    // the current element, which in turn may invalidate the index. So we'll
    // check, and re-calculate it if necessary.
    index = ValidateAttributeIndex(index, q_name);
  } else {
    // Check whether the "real" TT check would have come to the same result.
    // Debug-only, since not running the check at all is the whole point of this
    // branch.
    DCHECK_EQ(value, TrustedTypesCheckForAttribute(
                         q_name, value, "setAttribute", exception_state));
    DCHECK(!exception_state.HadException());
    DCHECK_EQ(index, ValidateAttributeIndex(index, q_name));
  }

  SetAttributeInternal(index, q_name, AtomicString(std::move(value)),
                       AttributeModificationReason::kDirectly);
}

void Element::SetAttributeHinted(AtomicString local_name,
                                 AtomicStringTable::WeakResult hint,
                                 const V8TrustedType* trusted_string,
                                 ExceptionState& exception_state) {
  if (!Document::IsValidName(local_name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        StrCat({"'", local_name, "' is not a valid attribute name."}));
    return;
  }
  SynchronizeAttributeHinted(local_name, hint);

  auto [index, q_name] =
      LookupAttributeQNameHinted(std::move(local_name), hint);
  AtomicString value(TrustedTypesCheckForAttribute(
      q_name, trusted_string, "setAttribute", exception_state));
  if (exception_state.HadException()) {
    return;
  }
  // The `TrustedTypesCheckFor` call above may run script, which may modify
  // the current element, which in turn may invalidate the index. So we'll
  // check, and re-calculate it if necessary.
  index = ValidateAttributeIndex(index, q_name);

  SetAttributeInternal(index, q_name, value,
                       AttributeModificationReason::kDirectly);
}

wtf_size_t Element::FindAttributeIndex(const QualifiedName& name) const {
  if (HasElementData()) {
    return GetElementData()->Attributes().FindIndex(name);
  }
  return kNotFound;
}

ALWAYS_INLINE void Element::SetAttributeInternal(
    wtf_size_t index,
    const QualifiedName& name,
    const AtomicString& new_value,
    AttributeModificationReason reason) {
  if (new_value.IsNull()) {
    if (index != kNotFound) {
      RemoveAttributeInternal(index, reason);
    }
    return;
  }

  if (index == kNotFound) {
    AppendAttributeInternal(name, new_value, reason);
    return;
  }

  const Attribute& existing_attribute =
      GetElementData()->Attributes().at(index);
  QualifiedName existing_attribute_name = existing_attribute.GetName();

  if (new_value == existing_attribute.Value()) {
    if (reason !=
        AttributeModificationReason::kBySynchronizationOfLazyAttribute) {
      WillModifyAttribute(existing_attribute_name, new_value, new_value);
      DidModifyAttribute(existing_attribute_name, new_value, new_value, reason);
    }
  } else {
    Attribute& new_attribute = EnsureUniqueElementData().Attributes().at(index);
    AtomicString existing_attribute_value = std::move(new_attribute.Value());
    if (reason !=
        AttributeModificationReason::kBySynchronizationOfLazyAttribute) {
      WillModifyAttribute(existing_attribute_name, existing_attribute_value,
                          new_value);
    }
    new_attribute.SetValue(new_value);
    if (reason !=
        AttributeModificationReason::kBySynchronizationOfLazyAttribute) {
      DidModifyAttribute(existing_attribute_name, existing_attribute_value,
                         new_value, reason);
    }
  }
}

Attr* Element::setAttributeNode(Attr* attr_node,
                                ExceptionState& exception_state) {
  AtomicString value(TrustedTypesCheckForAttribute(
      attr_node->GetQualifiedName(), attr_node->value(), "setAttributeNode",
      exception_state));
  if (exception_state.HadException()) {
    return nullptr;
  }

  Attr* old_attr_node = AttrIfExists(attr_node->GetQualifiedName());
  if (old_attr_node == attr_node) {
    return attr_node;  // This Attr is already attached to the element.
  }

  // InUseAttributeError: Raised if node is an Attr that is already an attribute
  // of another Element object.  The DOM user must explicitly clone Attr nodes
  // to reuse them in other elements.
  if (attr_node->ownerElement()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInUseAttributeError,
        "The node provided is an attribute node that is already an attribute "
        "of another Element; attribute nodes must be explicitly cloned.");
    return nullptr;
  }

  if (!IsHTMLElement() && IsA<HTMLDocument>(attr_node->GetDocument()) &&
      attr_node->name() != attr_node->name().LowerASCII()) {
    UseCounter::Count(
        GetDocument(),
        WebFeature::
            kNonHTMLElementSetAttributeNodeFromHTMLDocumentNameNotLowercase);
  }

  SynchronizeAllAttributes();
  const UniqueElementData& element_data = EnsureUniqueElementData();

  AttributeCollection attributes = element_data.Attributes();
  wtf_size_t index = attributes.FindIndex(attr_node->GetQualifiedName());
  AtomicString local_name;
  if (index != kNotFound) {
    const Attribute& attr = attributes[index];

    // If the name of the ElementData attribute doesn't
    // (case-sensitively) match that of the Attr node, record it
    // on the Attr so that it can correctly resolve the value on
    // the Element.
    if (!attr.GetName().Matches(attr_node->GetQualifiedName())) {
      local_name = attr.LocalName();
    }

    if (old_attr_node) {
      DetachAttrNodeFromElementWithValue(old_attr_node, attr.Value());
    } else {
      // FIXME: using attrNode's name rather than the
      // Attribute's for the replaced Attr is compatible with
      // all but Gecko (and, arguably, the DOM Level1 spec text.)
      // Consider switching.
      old_attr_node = MakeGarbageCollected<Attr>(
          GetDocument(), attr_node->GetQualifiedName(), attr.Value());
    }
  }

  SetAttributeInternal(index, attr_node->GetQualifiedName(), value,
                       AttributeModificationReason::kDirectly);

  attr_node->AttachToElement(this, local_name);
  GetTreeScope().AdoptIfNeeded(*attr_node);
  EnsureElementRareData().AddAttr(attr_node);

  return old_attr_node;
}

void Element::RemoveAttributeHinted(const AtomicString& name,
                                    AtomicStringTable::WeakResult hint) {
  if (!HasElementData()) {
    return;
  }

  wtf_size_t index = GetElementData()->Attributes().FindIndexHinted(name, hint);
  if (index == kNotFound) {
    if (hint == html_names::kStyleAttr.LocalName() &&
        GetElementData()->style_attribute_is_dirty() && IsStyledElement())
        [[unlikely]] {
      RemoveAllInlineStyleProperties();
    }
    return;
  }

  RemoveAttributeInternal(index, AttributeModificationReason::kDirectly);
}

bool Element::IsDocumentElement() const {
  return this == GetDocument().documentElement();
}

bool Element::IsReplacedElementRespectingCSSOverflow() const {
  // See https://github.com/w3c/csswg-drafts/issues/7144 for details on enabling
  // ink overflow for replaced elements.
  if (GetPseudoId() == kPseudoIdViewTransitionNew ||
      GetPseudoId() == kPseudoIdViewTransitionOld) {
    return true;
  }

  return IsA<HTMLVideoElement>(this) || IsA<HTMLCanvasElement>(this) ||
         IsA<HTMLImageElement>(this) ||
         (IsA<SVGSVGElement>(this) &&
          To<SVGSVGElement>(this)->IsOutermostSVGSVGElement() &&
          !IsDocumentElement()) ||
         IsA<HTMLFrameOwnerElement>(this);
}

AnchorPositionScrollData& Element::EnsureAnchorPositionScrollData() {
  return EnsureElementRareData().EnsureAnchorPositionScrollData(this);
}

void Element::RemoveAnchorPositionScrollData() {
  if (ElementRareDataVector* data = GetElementRareData()) {
    data->RemoveAnchorPositionScrollData();
  }
}

AnchorPositionScrollData* Element::GetAnchorPositionScrollData() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetAnchorPositionScrollData();
  }
  return nullptr;
}

ScrollMarkerGroupData& Element::EnsureScrollTargetGroupData() {
  return EnsureElementRareData().EnsureScrollMarkerGroupData(this);
}

void Element::RemoveScrollTargetGroupData() {
  if (ElementRareDataVector* data = GetElementRareData()) {
    if (ScrollMarkerGroupData* scroll_marker_group_data =
            data->GetScrollMarkerGroupData()) {
      scroll_marker_group_data->ClearFocusGroup();
      GetDocument().RemoveScrollTargetGroup(scroll_marker_group_data);
      data->RemoveScrollMarkerGroupData();
    }
  }
}

ScrollMarkerGroupData* Element::GetScrollTargetGroupData() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetScrollMarkerGroupData();
  }
  return nullptr;
}

void Element::SetScrollTargetGroupContainerData(ScrollMarkerGroupData* data) {
  return EnsureElementRareData().SetScrollMarkerGroupContainerData(data);
}

ScrollMarkerGroupData* Element::GetScrollTargetGroupContainerData() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetScrollMarkerGroupContainerData();
  }
  return nullptr;
}

void Element::SetMayBeImplicitAnchor() {
  bool was_implicit_anchor = MayBeImplicitAnchor();
  EnsureElementRareData().SetMayBeImplicitAnchor();
  if (!was_implicit_anchor && GetLayoutObject()) {
    // Invalidate layout to populate itself into AnchorMap.
    GetLayoutObject()->SetNeedsLayoutAndFullPaintInvalidation(
        layout_invalidation_reason::kAnchorPositioning);
    GetLayoutObject()->MarkMayContainAnchor();
  }
}

bool Element::MayBeImplicitAnchor() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->MayBeImplicitAnchor();
  }
  return false;
}

AnchorElementObserver* Element::GetAnchorElementObserver() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetAnchorElementObserver();
  }
  return nullptr;
}

AnchorElementObserver& Element::EnsureAnchorElementObserver() {
  DCHECK(RuntimeEnabledFeatures::HTMLAnchorAttributeEnabled());
  return EnsureElementRareData().EnsureAnchorElementObserver(this);
}

Element* Element::ImplicitAnchorElement() const {
  if (Element* anchor = anchorElement()) {
    DCHECK(RuntimeEnabledFeatures::HTMLAnchorAttributeEnabled());
    return anchor;
  }
  if (const HTMLElement* html_element = DynamicTo<HTMLElement>(this)) {
    if (Element* internal_anchor = html_element->implicitAnchor()) {
      return internal_anchor;
    }
  }
  if (const PseudoElement* pseudo_element = DynamicTo<PseudoElement>(this)) {
    switch (pseudo_element->GetPseudoId()) {
      case kPseudoIdCheckMark:
      case kPseudoIdBefore:
      case kPseudoIdAfter:
      case kPseudoIdPickerIcon:
      case kPseudoIdInterestHint:
      case kPseudoIdBackdrop:
      case kPseudoIdScrollMarkerGroupBefore:
      case kPseudoIdScrollMarkerGroupAfter:
      case kPseudoIdScrollMarker:
      case kPseudoIdScrollButtonBlockStart:
      case kPseudoIdScrollButtonInlineStart:
      case kPseudoIdScrollButtonInlineEnd:
      case kPseudoIdScrollButtonBlockEnd:
      case kPseudoIdOverscrollAreaParent:
        if (RuntimeEnabledFeatures::
                OriginatingElementIsImplicitAnchorEnabled()) {
          return parentElement();
        }
        return pseudo_element->UltimateOriginatingElement()
            .ImplicitAnchorElement();
      default:
        return nullptr;
    }
  }
  return nullptr;
}

bool Element::RecalcSelfOrAncestorHasContainerTiming() const {
  DCHECK(RuntimeEnabledFeatures::ContainerTimingEnabled());
  if (IsHTMLElement()) {
    if (FastHasAttribute(html_names::kContainertimingAttr)) {
      return true;
    } else if (FastHasAttribute(html_names::kContainertimingIgnoreAttr)) {
      return false;
    }
  }
  Node* parent = parentNode();
  if (parent && parent->SelfOrAncestorHasContainerTiming()) {
    return true;
  }
  return false;
}

void Element::UpdateDescendantHasContainerTiming(bool has_container_timing) {
  DCHECK(RuntimeEnabledFeatures::ContainerTimingEnabled());
  Element* element = ElementTraversal::FirstChild(*this);
  while (element) {
    if (element->IsHTMLElement()) {
      if (element->FastHasAttribute(html_names::kContainertimingAttr) ||
          element->FastHasAttribute(html_names::kContainertimingIgnoreAttr)) {
        element = ElementTraversal::NextSkippingChildren(*element, this);
        continue;
      }
    }
    if (!has_container_timing) {
      if (!element->SelfOrAncestorHasContainerTiming() ||
          element->RecalcSelfOrAncestorHasContainerTiming()) {
        element = ElementTraversal::NextSkippingChildren(*element, this);
        continue;
      }
      element->ClearSelfOrAncestorHasContainerTiming();
    } else {
      if (element->SelfOrAncestorHasContainerTiming() ||
          !element->RecalcSelfOrAncestorHasContainerTiming()) {
        element = ElementTraversal::NextSkippingChildren(*element, this);
        continue;
      }
      element->SetSelfOrAncestorHasContainerTiming();
    }
    element = ElementTraversal::Next(*element, this);
  }
}

bool Element::DoesChildContainerTimingNeedChange(const Node& node) const {
  auto* element = DynamicTo<Element>(node);
  if (element && element->IsHTMLElement() &&
      (element->FastHasAttribute(html_names::kContainertimingAttr) ||
       element->FastHasAttribute(html_names::kContainertimingIgnoreAttr))) {
    return false;
  }
  return SelfOrAncestorHasContainerTiming() !=
         node.SelfOrAncestorHasContainerTiming();
}

bool Element::ShouldAdjustContainerTimingForInsert(
    const ChildrenChange& change) const {
  if (change.type ==
      ChildrenChangeType::kFinishedBuildingDocumentFragmentTree) {
    for (Node& child : NodeTraversal::ChildrenOf(*this)) {
      if (DoesChildContainerTimingNeedChange(child)) {
        return true;
      }
    }
    return false;
  }
  return DoesChildContainerTimingNeedChange(*change.sibling_changed);
}

void Element::AdjustContainerTimingIfNeededAfterChildrenChanged(
    const ChildrenChange& change) {
  if (!RuntimeEnabledFeatures::ContainerTimingEnabled()) {
    return;
  }

  if (!change.IsChildInsertion() ||
      !ShouldAdjustContainerTimingForInsert(change)) {
    return;
  }

  UpdateDescendantHasContainerTiming(
      SelfOrAncestorHasContainerTiming() /* has_container_timing */);
}

void Element::SetHTMLUnsafeWithoutTrustedTypes(
    const String& html,
    ExceptionState& exception_state) {
  UseCounter::Count(GetDocument(), WebFeature::kHTMLUnsafeMethods);
  SetInnerHTMLInternal(html, ParseDeclarativeShadowRoots::kParse,
                       ForceHtml::kForce, std::monostate{}, exception_state);
}

void Element::setHTMLUnsafe(const V8UnionStringOrTrustedHTML* html,
                            ExceptionState& exception_state) {
  UseCounter::Count(GetDocument(), WebFeature::kHTMLUnsafeMethods);
  String compliant_html = TrustedTypesCheckForHTML(
      html, GetExecutionContext(), trusted_types_names::kElement,
      trusted_types_names::kSetHTMLUnsafe, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  SetInnerHTMLInternal(compliant_html, ParseDeclarativeShadowRoots::kParse,
                       ForceHtml::kForce, std::monostate{}, exception_state);
}

void Element::setHTMLUnsafe(const V8UnionStringOrTrustedHTML* html,
                            SetHTMLUnsafeOptions* options,
                            ExceptionState& exception_state) {
  CHECK(RuntimeEnabledFeatures::SanitizerAPIEnabled());
  String compliant_html = TrustedTypesCheckForHTML(
      html, GetExecutionContext(), trusted_types_names::kElement,
      trusted_types_names::kSetHTMLUnsafe, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  SetInnerHTMLInternal(compliant_html, ParseDeclarativeShadowRoots::kParse,
                       ForceHtml::kForce, options, exception_state);
}

void Element::setHTML(const String& html,
                      SetHTMLOptions* options,
                      ExceptionState& exception_state) {
  CHECK(RuntimeEnabledFeatures::SanitizerAPIEnabled());
  SetInnerHTMLInternal(html, ParseDeclarativeShadowRoots::kParse,
                       ForceHtml::kForce, options, exception_state);
}

void Element::SetNamedTriggers(NamedAnimationTriggerMap&& named_triggers) {
  EnsureElementRareData().EnsureAnimationTriggerData().SetNamedTriggers(
      named_triggers);
}

NamedAnimationTriggerMap* Element::NamedTriggers() const {
  ElementRareDataVector* data = GetElementRareData();
  if (!data) {
    return nullptr;
  }

  ElementAnimationTriggerData* trigger_data = data->AnimationTriggerData();
  if (!trigger_data) {
    return nullptr;
  }

  return &trigger_data->NamedTriggers();
}

AnimationTrigger* Element::NamedTrigger(const ScopedCSSName* name) const {
  NamedAnimationTriggerMap* trigger_map = NamedTriggers();
  if (!trigger_map) {
    return nullptr;
  }

  auto it = trigger_map->find(name);
  return it == trigger_map->end() ? nullptr : it->value.Get();
}

#if DCHECK_IS_ON()
void Element::VerifyBloomFilterTreeConsistencyIncludingChildren() const {
  VerifyBloomFilterTreeConsistency();
  for (Element& other_element : ElementTraversal::DescendantsOf(*this)) {
    other_element.VerifyBloomFilterTreeConsistency();
  }
}
#endif

bool Element::IsAppearanceBase() const {
  if (const ComputedStyle* style = GetComputedStyle()) {
    return SupportsBaseAppearance(style->EffectiveAppearance());
  }
  return false;
}

namespace {
std::optional<Element::BaseAppearanceValue> ToBaseAppearanceValue(
    AppearanceValue appearance_value) {
  switch (appearance_value) {
    case AppearanceValue::kBaseSelect:
      return Element::BaseAppearanceValue::kBaseSelect;
    case AppearanceValue::kBase:
      return Element::BaseAppearanceValue::kBase;
    default:
      return std::nullopt;
  }
}
}  // namespace

bool Element::SupportsBaseAppearance(AppearanceValue appearance_value) const {
  if (auto base_appearance_value = ToBaseAppearanceValue(appearance_value)) {
    return SupportsBaseAppearanceInternal(*base_appearance_value);
  }
  return false;
}

OverscrollAreaTracker& Element::EnsureOverscrollAreaTracker() {
  return EnsureElementRareData().EnsureOverscrollAreaTracker(this);
}

OverscrollAreaTracker* Element::OverscrollAreaTracker() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->OverscrollAreaTracker();
  }
  return nullptr;
}

Element* Element::OverscrollContainer() const {
  if (const ElementRareDataVector* data = GetElementRareData()) {
    return data->GetOverscrollContainer();
  }
  return nullptr;
}

void Element::SetOverscrollContainer(Element* element) {
  return EnsureElementRareData().SetOverscrollContainer(element);
}

void Element::ClearOverscrollContainer() {
  if (ElementRareDataVector* data = GetElementRareData()) {
    data->ClearOverscrollContainer();
  }
}

}  // namespace blink
