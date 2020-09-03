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

#include "cc/input/snap_selection_strategy.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/scroll_into_view_options_or_boolean.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_html_or_trusted_script_or_trusted_script_url.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_get_inner_html_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pointer_lock_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_into_view_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_to_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shadow_root_init.h"
#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/aom/computed_accessible_node.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_selector_watch.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/property_set_css_style_declaration.h"
#include "third_party/blink/renderer/core/css/resolver/selector_filter_parent_scope.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_stats.h"
#include "third_party/blink/renderer/core/css/selector_query.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/dataset_dom_string_map.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/element-inl.h"
#include "third_party/blink/renderer/core/dom/element_data_cache.h"
#include "third_party/blink/renderer/core/dom/element_rare_data.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/first_letter_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder.h"
#include "third_party/blink/renderer/core/dom/mutation_observer_interest_group.h"
#include "third_party/blink/renderer/core/dom/mutation_record.h"
#include "third_party/blink/renderer/core/dom/named_node_map.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/presentation_attribute_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/shadow_root_v0.h"
#include "third_party/blink/renderer/core/dom/slot_assignment.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/v0_insertion_point.h"
#include "third_party/blink/renderer/core/dom/whitespace_attacher.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/events/focus_event.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_list.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_registration_context.h"
#include "third_party/blink/renderer/core/html/forms/html_form_controls_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_options_collection.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/html_table_rows_collection.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/parser/nesting_level_incrementer.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/intersection_observer/element_intersection_observer_data.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_controller.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observation.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"
#include "third_party/blink/renderer/core/svg/svg_a_element.h"
#include "third_party/blink/renderer/core/svg/svg_animated_href.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_activity_logger.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"

namespace blink {

enum class ClassStringContent { kEmpty, kWhiteSpaceOnly, kHasClasses };

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
      if (did_update_children_)
        context->DidStyleChildren();
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
  bool DidUpdateSelfStyle() {
    if (auto* context = element_->GetDisplayLockContext()) {
      bool was_locked = context->IsLocked();
      context->DidStyleSelf();
      return was_locked && !context->IsLocked();
    }
    return false;
  }

  void NotifyUpdateWasBlocked(DisplayLockContext::StyleType style) {
    DCHECK(!ShouldUpdateChildStyle());
    // The only way to be blocked here is if we have a display lock context.
    DCHECK(element_->GetDisplayLockContext());

    element_->GetDisplayLockContext()->NotifyStyleRecalcWasBlocked(style);
  }

  StyleRecalcChange AdjustStyleRecalcChangeForChildren(
      StyleRecalcChange change) {
    DCHECK(element_->GetDisplayLockContext());
    DCHECK(!element_->GetDisplayLockContext()->IsLocked());
    return element_->GetDisplayLockContext()
        ->AdjustStyleRecalcChangeForChildren(change);
  }

 private:
  Element* element_;
  bool did_update_children_ = false;
};

bool IsRootEditableElementWithCounting(const Element& element) {
  bool is_editable = IsRootEditableElement(element);
  Document& doc = element.GetDocument();
  if (!doc.IsActive())
    return is_editable;
  // -webkit-user-modify doesn't affect text control elements.
  if (element.IsTextControl())
    return is_editable;
  const auto* style = element.GetComputedStyle();
  if (!style)
    return is_editable;
  auto user_modify = style->UserModify();
  const AtomicString& ce_value =
      element.FastGetAttribute(html_names::kContenteditableAttr);
  if (ce_value.IsNull() || EqualIgnoringASCIICase(ce_value, "false")) {
    if (user_modify == EUserModify::kReadWritePlaintextOnly) {
      UseCounter::Count(doc, WebFeature::kPlainTextEditingEffective);
      UseCounter::Count(doc, WebFeature::kWebKitUserModifyPlainTextEffective);
      UseCounter::Count(doc, WebFeature::kWebKitUserModifyEffective);
    } else if (user_modify == EUserModify::kReadWrite) {
      UseCounter::Count(doc, WebFeature::kWebKitUserModifyReadWriteEffective);
      UseCounter::Count(doc, WebFeature::kWebKitUserModifyEffective);
    }
  } else if (ce_value.IsEmpty() || EqualIgnoringASCIICase(ce_value, "true")) {
    if (user_modify == EUserModify::kReadWritePlaintextOnly) {
      UseCounter::Count(doc, WebFeature::kPlainTextEditingEffective);
      UseCounter::Count(doc, WebFeature::kWebKitUserModifyPlainTextEffective);
      UseCounter::Count(doc, WebFeature::kWebKitUserModifyEffective);
    } else if (user_modify == EUserModify::kReadOnly) {
      UseCounter::Count(doc, WebFeature::kWebKitUserModifyReadOnlyEffective);
      UseCounter::Count(doc, WebFeature::kWebKitUserModifyEffective);
    }
  } else if (EqualIgnoringASCIICase(ce_value, "plaintext-only")) {
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

// Return true if we're absolutely sure that this node is going to establish a
// new formatting context. Whether or not it establishes a new formatting
// context cannot be accurately determined until we have actually created the
// object (see LayoutBlockFlow::CreatesNewFormattingContext()), so this function
// may (and is allowed to) return false negatives, but NEVER false positives.
bool DefinitelyNewFormattingContext(const Node& node,
                                    const ComputedStyle& style) {
  auto display = style.Display();
  if (display == EDisplay::kInline || display == EDisplay::kContents)
    return false;
  // ::marker may establish a formatting context but still have some dependency
  // on the originating list item, so return false.
  if (node.IsMarkerPseudoElement())
    return false;
  // The only block-container display types that potentially don't establish a
  // new formatting context, are 'block' and 'list-item'.
  if (display != EDisplay::kBlock && display != EDisplay::kListItem) {
    // DETAILS and SUMMARY elements partially or completely ignore the display
    // type, though, and may end up disregarding the display type and just
    // create block containers. And those don't necessarily create a formatting
    // context.
    if (!IsA<HTMLDetailsElement>(node) && !IsA<HTMLSummaryElement>(node))
      return true;
  }
  if (!style.IsOverflowVisible())
    return node.GetDocument().ViewportDefiningElement() != &node;
  if (style.HasOutOfFlowPosition() ||
      (style.IsFloating() && !style.IsFlexOrGridItem()) ||
      style.ContainsPaint() || style.ContainsLayout() ||
      style.SpecifiesColumns())
    return true;
  if (node.GetDocument().documentElement() == &node)
    return true;
  if (const Element* element = DynamicTo<Element>(&node)) {
    // Replaced elements are considered to create a new formatting context, in
    // the sense that they can't possibly have children that participate in the
    // same formatting context as their parent.
    if (IsA<HTMLObjectElement>(element)) {
      // OBJECT elements are special, though. If they use fallback content, they
      // act as regular elements, and we can't claim that they establish a
      // formatting context, just based on element type, since children may very
      // well participate in the same formatting context as the parent of the
      // OBJECT.
      if (!element->ChildrenCanHaveStyle())
        return true;
    } else if (IsA<HTMLImageElement>(element) ||
               element->IsFormControlElement() || element->IsMediaElement() ||
               element->IsFrameOwnerElement()) {
      return true;
    }
  }
  if (const Node* parent = LayoutTreeBuilderTraversal::LayoutParent(node))
    return parent->ComputedStyleRef().IsDisplayFlexibleOrGridBox();
  return false;
}

bool CalculateStyleShouldForceLegacyLayout(const Element& element,
                                           const ComputedStyle& style) {
  Document& document = element.GetDocument();

  if (style.Display() == EDisplay::kLayoutCustom ||
      style.Display() == EDisplay::kInlineLayoutCustom)
    return false;

  // TODO(layout-dev): Once LayoutNG handles inline content editable, we
  // should get rid of following code fragment.
  if (!RuntimeEnabledFeatures::EditingNGEnabled()) {
    if (style.UserModify() != EUserModify::kReadOnly ||
        document.InDesignMode()) {
      UseCounter::Count(document, WebFeature::kLegacyLayoutByEditing);
      return true;
    }
  }

  if (style.IsDeprecatedWebkitBox() &&
      !style.IsDeprecatedWebkitBoxWithVerticalLineClamp()) {
    UseCounter::Count(
        document, WebFeature::kLegacyLayoutByWebkitBoxWithoutVerticalLineClamp);
    return true;
  }

  if (!RuntimeEnabledFeatures::LayoutNGBlockFragmentationEnabled()) {
    // Disable NG for the entire subtree if we're establishing a multicol
    // container.
    if (style.SpecifiesColumns()) {
      UseCounter::Count(document, WebFeature::kLegacyLayoutByMultiCol);
      return true;
    }
  }

  // No printing support in LayoutNG yet.
  if (document.Printing() && element == document.documentElement()) {
    UseCounter::Count(document, WebFeature::kLegacyLayoutByPrinting);
    return true;
  }

  // Fall back to legacy layout for frameset documents. The frameset itself (and
  // the frames) can only create legacy layout objects anyway (no NG counterpart
  // for them yet). However, the layout object for the HTML root element would
  // be an NG one. If we'd then print the document, we'd fall back to legacy
  // layout (because of the above check), which would re-attach all layout
  // objects, which would cause the frameset to lose state of some sort, leaving
  // everything blank when printed.
  if (document.IsFrameSet()) {
    UseCounter::Count(document, WebFeature::kLegacyLayoutByFrameSet);
    return true;
  }

  // 'text-combine-upright' property is not supported yet.
  if (style.HasTextCombine() && !style.IsHorizontalWritingMode()) {
    UseCounter::Count(document, WebFeature::kLegacyLayoutByTextCombine);
    return true;
  }

  if (style.InsideNGFragmentationContext()) {
    // If we're inside an NG block fragmentation context, all fragmentable boxes
    // must be laid out by NG natively. We only allow legacy layout objects if
    // they are monolithic (e.g. replaced content, inline-table, and so
    // on). Inline display types end up on a line, and are therefore monolithic,
    // so we can allow those.
    if (!style.IsDisplayInlineType()) {
      if (style.IsDisplayTableType() || style.IsDisplayFlexibleOrGridBox()) {
        UseCounter::Count(
            document,
            WebFeature::
                kLegacyLayoutByTableFlexGridBlockInNGFragmentationContext);
        return true;
      }
    }
  }

  return false;
}

bool HasLeftwardDirection(const Element& element) {
  auto* style = element.GetComputedStyle();
  if (!style)
    return false;

  WritingMode writing_mode = style->GetWritingMode();
  bool is_rtl = !style->IsLeftToRightDirection();
  return (writing_mode == WritingMode::kHorizontalTb && is_rtl) ||
         writing_mode == WritingMode::kVerticalRl ||
         writing_mode == WritingMode::kSidewaysRl;
}

bool HasUpwardDirection(const Element& element) {
  auto* style = element.GetComputedStyle();
  if (!style)
    return false;

  WritingMode writing_mode = style->GetWritingMode();
  bool is_rtl = !style->IsLeftToRightDirection();
  return (is_rtl && (writing_mode == WritingMode::kVerticalRl ||
                     writing_mode == WritingMode::kVerticalLr ||
                     writing_mode == WritingMode::kSidewaysRl)) ||
         (!is_rtl && writing_mode == WritingMode::kSidewaysLr);
}

// TODO(meredithl): Automatically generate this method once the IDL compiler has
// been refactored. See http://crbug.com/839389 for details.
bool IsElementReflectionAttribute(const QualifiedName& name) {
  if (name == html_names::kAriaActivedescendantAttr)
    return true;
  if (name == html_names::kAriaControlsAttr)
    return true;
  if (name == html_names::kAriaDescribedbyAttr)
    return true;
  if (name == html_names::kAriaDetailsAttr)
    return true;
  if (name == html_names::kAriaErrormessageAttr)
    return true;
  if (name == html_names::kAriaFlowtoAttr)
    return true;
  if (name == html_names::kAriaLabeledbyAttr)
    return true;
  if (name == html_names::kAriaLabelledbyAttr)
    return true;
  if (name == html_names::kAriaOwnsAttr)
    return true;
  return false;
}

HeapVector<Member<Element>>* GetExplicitlySetElementsForAttr(
    Element* element,
    const QualifiedName& name) {
  ExplicitlySetAttrElementsMap* element_attribute_map =
      element->GetDocument().GetExplicitlySetAttrElementsMap(element);
  return element_attribute_map->at(name);
}

// Checks that the given element |candidate| is a descendant of
// |attribute_element|'s  shadow including ancestors.
bool ElementIsDescendantOfShadowIncludingAncestor(
    const Element& attribute_element,
    const Element& candidate) {
  // TODO(meredithl): Update this to allow setting relationships for elements
  // outside of the DOM once the spec is finalized. For consistency and
  // simplicity, for now it is disallowed.
  if (!attribute_element.IsInTreeScope() || !candidate.IsInTreeScope())
    return false;
  ShadowRoot* nearest_root = attribute_element.ContainingShadowRoot();
  const Element* shadow_host = &attribute_element;
  while (nearest_root) {
    shadow_host = &nearest_root->host();
    if (candidate.IsDescendantOf(nearest_root))
      return true;
    nearest_root = shadow_host->ContainingShadowRoot();
  }

  Element* document_element = shadow_host->GetDocument().documentElement();
  return candidate.IsDescendantOf(document_element);
}

// The first algorithm in
// https://html.spec.whatwg.org/C/#the-autofocus-attribute
void EnqueueAutofocus(Element& element) {
  // When an element with the autofocus attribute specified is inserted into a
  // document, run the following steps:
  DCHECK(element.isConnected());
  if (!element.IsAutofocusable())
    return;

  // 1. If the user has indicated (for example, by starting to type in a form
  // control) that they do not wish focus to be changed, then optionally return.

  // We don't implement this optional step. If other browsers have such
  // behavior, we should follow it or standardize it.

  // 2. Let target be the element's node document.
  Document& doc = element.GetDocument();
  LocalDOMWindow* window = doc.domWindow();

  // 3. If target's browsing context is null, then return.
  if (!window)
    return;

  // 4. If target's active sandboxing flag set has the sandboxed automatic
  // features browsing context flag, then return.
  if (window->IsSandboxed(
          network::mojom::blink::WebSandboxFlags::kAutomaticFeatures)) {
    window->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        String::Format(
            "Blocked autofocusing on a <%s> element because the element's "
            "frame "
            "is sandboxed and the 'allow-scripts' permission is not set.",
            element.TagQName().ToString().Ascii().c_str())));
    return;
  }

  // 5. Let topDocument be the active document of target's browsing context's
  // top-level browsing context.
  // 6. If target's origin is not the same as the origin of topDocument,
  // then return.
  if (!doc.IsInMainFrame() &&
      !doc.TopFrameOrigin()->CanAccess(window->GetSecurityOrigin())) {
    window->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        String::Format("Blocked autofocusing on a <%s> element in a "
                       "cross-origin subframe.",
                       element.TagQName().ToString().Ascii().c_str())));
    return;
  }

  element.GetDocument().TopDocument().EnqueueAutofocusCandidate(element);
}

}  // namespace

Element::Element(const QualifiedName& tag_name,
                 Document* document,
                 ConstructionType type)
    : ContainerNode(document, type), tag_name_(tag_name) {}

Element* Element::GetAnimationTarget() {
  return this;
}

bool Element::HasElementFlagInternal(ElementFlags mask) const {
  return GetElementRareData()->HasElementFlag(mask);
}

void Element::SetElementFlag(ElementFlags mask, bool value) {
  if (!HasRareData() && !value)
    return;
  EnsureElementRareData().SetElementFlag(mask, value);
}

void Element::ClearElementFlag(ElementFlags mask) {
  if (!HasRareData())
    return;
  GetElementRareData()->ClearElementFlag(mask);
}

void Element::ClearTabIndexExplicitlyIfNeeded() {
  if (HasRareData())
    GetElementRareData()->ClearTabIndexExplicitly();
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

bool Element::IsFocusableStyle() const {
  // TODO(vmpstr): Note that this may be called by accessibility during layout
  // tree attachment, at which point we might not have cleared all of the dirty
  // bits to ensure that the layout tree doesn't need an update. This should be
  // fixable by deferring AX tree updates as a separate phase after layout tree
  // attachment has happened. At that point `InStyleRecalc()` portion of the
  // following DCHECK can be removed.
  DCHECK(
      !GetDocument().IsActive() || GetDocument().InStyleRecalc() ||
      !GetDocument().NeedsLayoutTreeUpdateForNodeIncludingDisplayLocked(*this));
  // Elements in canvas fallback content are not rendered, but they are allowed
  // to be focusable as long as their canvas is displayed and visible.
  if (IsInCanvasSubtree()) {
    const HTMLCanvasElement* canvas =
        Traversal<HTMLCanvasElement>::FirstAncestorOrSelf(*this);
    DCHECK(canvas);
    return canvas->GetLayoutObject() &&
           canvas->GetLayoutObject()->Style()->Visibility() ==
               EVisibility::kVisible;
  }

  // FIXME: Even if we are not visible, we might have a child that is visible.
  // Hyatt wants to fix that some day with a "has visible content" flag or the
  // like.
  return GetLayoutObject() &&
         GetLayoutObject()->Style()->Visibility() == EVisibility::kVisible;
}

Node* Element::Clone(Document& factory, CloneChildrenFlag flag) const {
  if (flag == CloneChildrenFlag::kSkip)
    return &CloneWithoutChildren(&factory);
  Element* copy = &CloneWithChildren(&factory);
  // 7. If node is a shadow host and the clone shadows flag is set, run these
  // steps:
  if (flag == CloneChildrenFlag::kCloneWithShadows) {
    DCHECK(RuntimeEnabledFeatures::DeclarativeShadowDOMEnabled(
        GetExecutionContext()));
    auto* shadow_root = GetShadowRoot();
    if (shadow_root && (shadow_root->GetType() == ShadowRootType::kOpen ||
                        shadow_root->GetType() == ShadowRootType::kClosed)) {
      // 7.1 Run attach a shadow root with shadow host equal to copy, mode equal
      // to node’s shadow root’s mode, and delegates focus equal to node’s
      // shadow root’s delegates focus.
      ShadowRoot& cloned_shadow_root = copy->AttachShadowRootInternal(
          shadow_root->GetType(),
          shadow_root->delegatesFocus() ? FocusDelegation::kDelegateFocus
                                        : FocusDelegation::kNone,
          shadow_root->GetSlotAssignmentMode());
      // 7.2 If node’s shadow root’s "is declarative shadow root" is true, then
      // set copy’s shadow root’s "is declarative shadow root" property to true.
      cloned_shadow_root.SetIsDeclarativeShadowRoot(
          shadow_root->IsDeclarativeShadowRoot());
      // 7.3 If the clone children flag is set, clone all the children of node’s
      // shadow root and append them to copy’s shadow root, with document as
      // specified, the clone children flag being set, and the clone shadows
      // flag being set.
      cloned_shadow_root.CloneChildNodesFrom(*shadow_root, flag);
    }
  }
  return copy;
}

Element& Element::CloneWithChildren(Document* nullable_factory) const {
  Element& clone = CloneWithoutAttributesAndChildren(
      nullable_factory ? *nullable_factory : GetDocument());
  // This will catch HTML elements in the wrong namespace that are not correctly
  // copied.  This is a sanity check as HTML overloads some of the DOM methods.
  DCHECK_EQ(IsHTMLElement(), clone.IsHTMLElement());

  clone.CloneAttributesFrom(*this);
  clone.CloneNonAttributePropertiesFrom(*this, CloneChildrenFlag::kClone);
  clone.CloneChildNodesFrom(*this, CloneChildrenFlag::kClone);
  return clone;
}

Element& Element::CloneWithoutChildren(Document* nullable_factory) const {
  Element& clone = CloneWithoutAttributesAndChildren(
      nullable_factory ? *nullable_factory : GetDocument());
  // This will catch HTML elements in the wrong namespace that are not correctly
  // copied.  This is a sanity check as HTML overloads some of the DOM methods.
  DCHECK_EQ(IsHTMLElement(), clone.IsHTMLElement());

  clone.CloneAttributesFrom(*this);
  clone.CloneNonAttributePropertiesFrom(*this, CloneChildrenFlag::kSkip);
  return clone;
}

Element& Element::CloneWithoutAttributesAndChildren(Document& factory) const {
  return *factory.CreateElement(TagQName(), CreateElementFlags::ByCloneNode(),
                                IsValue());
}

Attr* Element::DetachAttribute(wtf_size_t index) {
  DCHECK(GetElementData());
  const Attribute& attribute = GetElementData()->Attributes().at(index);
  Attr* attr_node = AttrIfExists(attribute.GetName());
  if (attr_node) {
    DetachAttrNodeAtIndex(attr_node, index);
  } else {
    attr_node = MakeGarbageCollected<Attr>(GetDocument(), attribute.GetName(),
                                           attribute.Value());
    RemoveAttributeInternal(index, kNotInSynchronizationOfLazyAttribute);
  }
  return attr_node;
}

void Element::DetachAttrNodeAtIndex(Attr* attr, wtf_size_t index) {
  DCHECK(attr);
  DCHECK(GetElementData());

  const Attribute& attribute = GetElementData()->Attributes().at(index);
  DCHECK(attribute.GetName() == attr->GetQualifiedName());
  DetachAttrNodeFromElementWithValue(attr, attribute.Value());
  RemoveAttributeInternal(index, kNotInSynchronizationOfLazyAttribute);
}

void Element::removeAttribute(const QualifiedName& name) {
  if (!GetElementData())
    return;

  wtf_size_t index = GetElementData()->Attributes().FindIndex(name);
  if (index == kNotFound)
    return;

  RemoveAttributeInternal(index, kNotInSynchronizationOfLazyAttribute);
}

void Element::SetBooleanAttribute(const QualifiedName& name, bool value) {
  if (value)
    setAttribute(name, g_empty_atom);
  else
    removeAttribute(name);
}

bool Element::HasExplicitlySetAttrAssociatedElements(
    const QualifiedName& name) {
  return GetExplicitlySetElementsForAttr(this, name);
}

void Element::SynchronizeContentAttributeAndElementReference(
    const QualifiedName& name) {
  ExplicitlySetAttrElementsMap* element_attribute_map =
      GetDocument().GetExplicitlySetAttrElementsMap(this);
  element_attribute_map->erase(name);
}

void Element::SetElementAttribute(const QualifiedName& name, Element* element) {
  ExplicitlySetAttrElementsMap* explicitly_set_attr_elements_map =
      GetDocument().GetExplicitlySetAttrElementsMap(this);

  // If the reflected element is explicitly null, or is not a member of this
  // elements shadow including ancestor tree, then we remove the content
  // attribute and the explicitly set attr-element.
  // Note this means that explicitly set elements can cross ancestral shadow
  // boundaries, but not descendant ones. See the spec for more details:
  // https://whatpr.org/html/3917/common-dom-interfaces.html#reflecting-content-attributes-in-idl-attributes:concept-shadow-including-ancestor
  if (!element ||
      !ElementIsDescendantOfShadowIncludingAncestor(*this, *element)) {
    explicitly_set_attr_elements_map->erase(name);
    removeAttribute(name);
    return;
  }

  const AtomicString id = element->GetIdAttribute();

  // Explicitly set attr-elements must have a valid id attribute, and also
  // refer to the first element in tree order of |this| elements node tree in
  // order for the content attribute to reflect the ID. Where these conditions
  // aren't met, the content attribute should reflect the empty string. Note
  // that the explicitly set attr-element is still set. See the spec for more
  // details:
  // https://whatpr.org/html/3917/common-dom-interfaces.html#reflecting-content-attributes-in-idl-attributes:root-2
  if (id.IsNull() || GetTreeScope() != element->GetTreeScope() ||
      GetTreeScope().getElementById(id) != element)
    setAttribute(name, g_empty_atom);
  else
    setAttribute(name, id);

  auto result = explicitly_set_attr_elements_map->insert(name, nullptr);
  if (result.is_new_entry) {
    result.stored_value->value =
        MakeGarbageCollected<HeapVector<Member<Element>>>();
  } else {
    result.stored_value->value->clear();
  }
  result.stored_value->value->push_back(element);

  if (isConnected()) {
    if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
      cache->HandleAttributeChanged(name, this);
  }
}

Element* Element::GetElementAttribute(const QualifiedName& name) {
  HeapVector<Member<Element>>* element_attribute_vector =
      GetExplicitlySetElementsForAttr(this, name);
  if (element_attribute_vector) {
    DCHECK_EQ(element_attribute_vector->size(), 1u);
    Element* explicitly_set_element = element_attribute_vector->at(0);
    // Only return the explicit element if it still exists in the same scope.
    if (explicitly_set_element)
      return explicitly_set_element;
  }

  // Compute the attr-associated element, this can be null.
  AtomicString id = getAttribute(name);
  if (id.IsNull())
    return nullptr;

  // Will return null if the id is empty.
  return GetTreeScope().getElementById(id);
}

void Element::SetElementArrayAttribute(
    const QualifiedName& name,
    const base::Optional<HeapVector<Member<Element>>>& given_elements) {
  ExplicitlySetAttrElementsMap* element_attribute_map =
      GetDocument().GetExplicitlySetAttrElementsMap(this);

  if (!given_elements) {
    element_attribute_map->erase(name);
    removeAttribute(name);
    return;
  }

  // Get or create element array, and remove any pre-existing elements.
  // Note that this performs two look ups on |name| within the map, as
  // modifying the content attribute will cause the synchronization steps to
  // be run which with modifies the hash map, and can cause a rehash.
  HeapVector<Member<Element>>* elements = element_attribute_map->at(name);
  if (!elements)
    elements = MakeGarbageCollected<HeapVector<Member<Element>>>();
  else
    elements->clear();
  SpaceSplitString value;

  for (auto element : given_elements.value()) {
    // Elements that are not descendants of this element's shadow including
    // ancestors are dropped.
    if (!ElementIsDescendantOfShadowIncludingAncestor(*this, *element))
      continue;

    // If |value| is null, this means a previous element must have been invalid,
    // and the content attribute should reflect the empty string, so we don't
    // continue trying to compute it.
    if (value.IsNull() && !elements->IsEmpty()) {
      elements->push_back(element);
      continue;
    }

    elements->push_back(element);
    const AtomicString given_element_id = element->GetIdAttribute();

    // We compute the content attribute string as a space separated string of
    // the given |element| ids. Every |element| in |given_elements| must have an
    // id, must be in the same tree scope and must be the first id in tree order
    // with that id, otherwise the content attribute should reflect the empty
    // string.
    if (given_element_id.IsNull() ||
        GetTreeScope() != element->GetTreeScope() ||
        GetTreeScope().getElementById(given_element_id) != element) {
      value.Clear();
      continue;
    }

    // Whitespace between elements is added when the string is serialized.
    value.Add(given_element_id);
  }

  setAttribute(name, value.SerializeToString());
  if (isConnected()) {
    if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
      cache->HandleAttributeChanged(name, this);
  }
  element_attribute_map->Set(name, elements);
}

base::Optional<HeapVector<Member<Element>>> Element::GetElementArrayAttribute(
    const QualifiedName& name) {
  HeapVector<Member<Element>>* explicitly_set_elements =
      GetExplicitlySetElementsForAttr(this, name);

  if (explicitly_set_elements) {
    return *explicitly_set_elements;
  }

  QualifiedName attr = name;

  // Account for labelled vs labeled spelling
  if (attr == html_names::kAriaLabelledbyAttr) {
    attr = hasAttribute(html_names::kAriaLabeledbyAttr) &&
                   !hasAttribute(html_names::kAriaLabelledbyAttr)
               ? html_names::kAriaLabeledbyAttr
               : html_names::kAriaLabelledbyAttr;
  }

  String attribute_value = getAttribute(attr).GetString();
  HeapVector<Member<Element>> content_elements;

  Vector<String> tokens;
  attribute_value = attribute_value.SimplifyWhiteSpace();
  attribute_value.Split(' ', tokens);

  for (auto token : tokens) {
    Element* candidate = GetTreeScope().getElementById(AtomicString(token));
    if (candidate)
      content_elements.push_back(candidate);
  }
  if (content_elements.IsEmpty())
    return base::nullopt;
  return content_elements;
}

NamedNodeMap* Element::attributesForBindings() const {
  ElementRareData& rare_data =
      const_cast<Element*>(this)->EnsureElementRareData();
  if (NamedNodeMap* attribute_map = rare_data.AttributeMap())
    return attribute_map;

  rare_data.SetAttributeMap(
      MakeGarbageCollected<NamedNodeMap>(const_cast<Element*>(this)));
  return rare_data.AttributeMap();
}

Vector<AtomicString> Element::getAttributeNames() const {
  Vector<AtomicString> attributesVector;
  if (!hasAttributes())
    return attributesVector;

  AttributeCollection attributes = element_data_->Attributes();
  attributesVector.ReserveInitialCapacity(attributes.size());
  for (const Attribute& attr : attributes)
    attributesVector.UncheckedAppend(attr.GetName().ToString());
  return attributesVector;
}

ElementAnimations* Element::GetElementAnimations() const {
  if (HasRareData())
    return GetElementRareData()->GetElementAnimations();
  return nullptr;
}

ElementAnimations& Element::EnsureElementAnimations() {
  ElementRareData& rare_data = EnsureElementRareData();
  if (!rare_data.GetElementAnimations())
    rare_data.SetElementAnimations(MakeGarbageCollected<ElementAnimations>());
  return *rare_data.GetElementAnimations();
}

bool Element::HasAnimations() const {
  if (!HasRareData())
    return false;

  ElementAnimations* element_animations =
      GetElementRareData()->GetElementAnimations();
  return element_animations && !element_animations->IsEmpty();
}

Node::NodeType Element::getNodeType() const {
  return kElementNode;
}

bool Element::hasAttribute(const QualifiedName& name) const {
  return hasAttributeNS(name.NamespaceURI(), name.LocalName());
}

bool Element::HasAttributeIgnoringNamespace(
    const AtomicString& local_name) const {
  if (!GetElementData())
    return false;
  WTF::AtomicStringTable::WeakResult hint =
      WeakLowercaseIfNecessary(local_name);
  SynchronizeAttributeHinted(local_name, hint);
  if (hint.IsNull()) {
    return false;
  }
  for (const Attribute& attribute : GetElementData()->Attributes()) {
    if (hint == attribute.LocalName())
      return true;
  }
  return false;
}

void Element::SynchronizeAllAttributes() const {
  if (!GetElementData())
    return;
  // NOTE: AnyAttributeMatches in selector_checker.cc currently assumes that all
  // lazy attributes have a null namespace.  If that ever changes we'll need to
  // fix that code.
  if (GetElementData()->style_attribute_is_dirty()) {
    DCHECK(IsStyledElement());
    SynchronizeStyleAttributeInternal();
  }
  if (GetElementData()->svg_attributes_are_dirty())
    To<SVGElement>(this)->SynchronizeSVGAttribute(AnyQName());
}

const AtomicString& Element::getAttribute(const QualifiedName& name) const {
  if (!GetElementData())
    return g_null_atom;
  SynchronizeAttribute(name);
  if (const Attribute* attribute = GetElementData()->Attributes().Find(name))
    return attribute->Value();
  return g_null_atom;
}

AtomicString Element::LowercaseIfNecessary(const AtomicString& name) const {
  return IsHTMLElement() && IsA<HTMLDocument>(GetDocument()) ? name.LowerASCII()
                                                             : name;
}

const AtomicString& Element::nonce() const {
  return HasRareData() ? GetElementRareData()->GetNonce() : g_null_atom;
}

void Element::setNonce(const AtomicString& nonce) {
  EnsureElementRareData().SetNonce(nonce);
}

void Element::scrollIntoView(ScrollIntoViewOptionsOrBoolean arg) {
  ScrollIntoViewOptions* options = ScrollIntoViewOptions::Create();
  if (arg.IsBoolean()) {
    if (arg.GetAsBoolean())
      options->setBlock("start");
    else
      options->setBlock("end");
    options->setInlinePosition("nearest");
  } else if (arg.IsScrollIntoViewOptions()) {
    options = arg.GetAsScrollIntoViewOptions();
  }
  scrollIntoViewWithOptions(options);
}

void Element::scrollIntoView(bool align_to_top) {
  ScrollIntoViewOptionsOrBoolean arg;
  arg.SetBoolean(align_to_top);
  scrollIntoView(arg);
}

static mojom::blink::ScrollAlignment ToPhysicalAlignment(
    const ScrollIntoViewOptions* options,
    ScrollOrientation axis,
    WritingMode writing_mode,
    bool is_ltr) {
  bool is_horizontal_writing_mode = IsHorizontalWritingMode(writing_mode);
  String alignment =
      ((axis == kHorizontalScroll && is_horizontal_writing_mode) ||
       (axis == kVerticalScroll && !is_horizontal_writing_mode))
          ? options->inlinePosition()
          : options->block();

  if (alignment == "center")
    return ScrollAlignment::CenterAlways();
  if (alignment == "nearest")
    return ScrollAlignment::ToEdgeIfNeeded();
  if (alignment == "start") {
    if (axis == kHorizontalScroll) {
      switch (writing_mode) {
        case WritingMode::kHorizontalTb:
          return is_ltr ? ScrollAlignment::LeftAlways()
                        : ScrollAlignment::RightAlways();
        case WritingMode::kVerticalRl:
        case WritingMode::kSidewaysRl:
          return ScrollAlignment::RightAlways();
        case WritingMode::kVerticalLr:
        case WritingMode::kSidewaysLr:
          return ScrollAlignment::LeftAlways();
        default:
          NOTREACHED();
          return ScrollAlignment::LeftAlways();
      }
    } else {
      switch (writing_mode) {
        case WritingMode::kHorizontalTb:
          return ScrollAlignment::TopAlways();
        case WritingMode::kVerticalRl:
        case WritingMode::kSidewaysRl:
        case WritingMode::kVerticalLr:
          return is_ltr ? ScrollAlignment::TopAlways()
                        : ScrollAlignment::BottomAlways();
        case WritingMode::kSidewaysLr:
          return is_ltr ? ScrollAlignment::BottomAlways()
                        : ScrollAlignment::TopAlways();
        default:
          NOTREACHED();
          return ScrollAlignment::TopAlways();
      }
    }
  }
  if (alignment == "end") {
    if (axis == kHorizontalScroll) {
      switch (writing_mode) {
        case WritingMode::kHorizontalTb:
          return is_ltr ? ScrollAlignment::RightAlways()
                        : ScrollAlignment::LeftAlways();
        case WritingMode::kVerticalRl:
        case WritingMode::kSidewaysRl:
          return ScrollAlignment::LeftAlways();
        case WritingMode::kVerticalLr:
        case WritingMode::kSidewaysLr:
          return ScrollAlignment::RightAlways();
        default:
          NOTREACHED();
          return ScrollAlignment::RightAlways();
      }
    } else {
      switch (writing_mode) {
        case WritingMode::kHorizontalTb:
          return ScrollAlignment::BottomAlways();
        case WritingMode::kVerticalRl:
        case WritingMode::kSidewaysRl:
        case WritingMode::kVerticalLr:
          return is_ltr ? ScrollAlignment::BottomAlways()
                        : ScrollAlignment::TopAlways();
        case WritingMode::kSidewaysLr:
          return is_ltr ? ScrollAlignment::TopAlways()
                        : ScrollAlignment::BottomAlways();
        default:
          NOTREACHED();
          return ScrollAlignment::BottomAlways();
      }
    }
  }

  // Default values
  if (is_horizontal_writing_mode) {
    return (axis == kHorizontalScroll) ? ScrollAlignment::ToEdgeIfNeeded()
                                       : ScrollAlignment::TopAlways();
  }
  return (axis == kHorizontalScroll) ? ScrollAlignment::LeftAlways()
                                     : ScrollAlignment::ToEdgeIfNeeded();
}

void Element::scrollIntoViewWithOptions(const ScrollIntoViewOptions* options) {
  ActivateDisplayLockIfNeeded(DisplayLockActivationReason::kScrollIntoView);
  GetDocument().EnsurePaintLocationDataValidForNode(
      this, DocumentUpdateReason::kJavaScript);
  ScrollIntoViewNoVisualUpdate(options);
}

void Element::ScrollIntoViewNoVisualUpdate(
    const ScrollIntoViewOptions* options) {
  if (!GetLayoutObject() || !GetDocument().GetPage())
    return;

  mojom::blink::ScrollBehavior behavior =
      (options->behavior() == "smooth") ? mojom::blink::ScrollBehavior::kSmooth
                                        : mojom::blink::ScrollBehavior::kAuto;

  WritingMode writing_mode = GetComputedStyle()->GetWritingMode();
  bool is_ltr = GetComputedStyle()->IsLeftToRightDirection();
  auto align_x =
      ToPhysicalAlignment(options, kHorizontalScroll, writing_mode, is_ltr);
  auto align_y =
      ToPhysicalAlignment(options, kVerticalScroll, writing_mode, is_ltr);

  PhysicalRect bounds = BoundingBoxForScrollIntoView();
  GetLayoutObject()->ScrollRectToVisible(
      bounds, ScrollAlignment::CreateScrollIntoViewParams(
                  align_x, align_y, mojom::blink::ScrollType::kProgrammatic,
                  /*make_visible_in_visual_viewport=*/true, behavior));

  GetDocument().SetSequentialFocusNavigationStartingPoint(this);
}

void Element::scrollIntoViewIfNeeded(bool center_if_needed) {
  GetDocument().EnsurePaintLocationDataValidForNode(
      this, DocumentUpdateReason::kJavaScript);

  if (!GetLayoutObject())
    return;

  PhysicalRect bounds = BoundingBoxForScrollIntoView();
  if (center_if_needed) {
    GetLayoutObject()->ScrollRectToVisible(
        bounds, ScrollAlignment::CreateScrollIntoViewParams(
                    ScrollAlignment::CenterIfNeeded(),
                    ScrollAlignment::CenterIfNeeded()));
  } else {
    GetLayoutObject()->ScrollRectToVisible(
        bounds, ScrollAlignment::CreateScrollIntoViewParams(
                    ScrollAlignment::ToEdgeIfNeeded(),
                    ScrollAlignment::ToEdgeIfNeeded()));
  }
}

int Element::OffsetLeft() {
  GetDocument().EnsurePaintLocationDataValidForNode(
      this, DocumentUpdateReason::kJavaScript);
  if (LayoutBoxModelObject* layout_object = GetLayoutBoxModelObject())
    return AdjustForAbsoluteZoom::AdjustLayoutUnit(
               LayoutUnit(
                   layout_object->PixelSnappedOffsetLeft(OffsetParent())),
               layout_object->StyleRef())
        .Round();
  return 0;
}

int Element::OffsetTop() {
  GetDocument().EnsurePaintLocationDataValidForNode(
      this, DocumentUpdateReason::kJavaScript);
  if (LayoutBoxModelObject* layout_object = GetLayoutBoxModelObject())
    return AdjustForAbsoluteZoom::AdjustLayoutUnit(
               LayoutUnit(layout_object->PixelSnappedOffsetTop(OffsetParent())),
               layout_object->StyleRef())
        .Round();
  return 0;
}

int Element::OffsetWidth() {
  GetDocument().EnsurePaintLocationDataValidForNode(
      this, DocumentUpdateReason::kJavaScript);
  if (LayoutBoxModelObject* layout_object = GetLayoutBoxModelObject())
    return AdjustForAbsoluteZoom::AdjustLayoutUnit(
               LayoutUnit(
                   layout_object->PixelSnappedOffsetWidth(OffsetParent())),
               layout_object->StyleRef())
        .Round();
  return 0;
}

int Element::OffsetHeight() {
  GetDocument().EnsurePaintLocationDataValidForNode(
      this, DocumentUpdateReason::kJavaScript);
  if (LayoutBoxModelObject* layout_object = GetLayoutBoxModelObject())
    return AdjustForAbsoluteZoom::AdjustLayoutUnit(
               LayoutUnit(
                   layout_object->PixelSnappedOffsetHeight(OffsetParent())),
               layout_object->StyleRef())
        .Round();
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

  if (LayoutBox* layout_object = GetLayoutBox())
    return AdjustForAbsoluteZoom::AdjustLayoutUnit(layout_object->ClientLeft(),
                                                   layout_object->StyleRef())
        .Round();
  return 0;
}

int Element::clientTop() {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  if (LayoutBox* layout_object = GetLayoutBox())
    return AdjustForAbsoluteZoom::AdjustLayoutUnit(layout_object->ClientTop(),
                                                   layout_object->StyleRef())
        .Round();
  return 0;
}

int Element::clientWidth() {
  // When in strict mode, clientWidth for the document element should return the
  // width of the containing frame.
  // When in quirks mode, clientWidth for the body element should return the
  // width of the containing frame.
  bool in_quirks_mode = GetDocument().InQuirksMode();
  if ((!in_quirks_mode && GetDocument().documentElement() == this) ||
      (in_quirks_mode && IsHTMLElement() && GetDocument().body() == this)) {
    auto* layout_view = GetDocument().GetLayoutView();
    if (layout_view) {
      // TODO(crbug.com/740879): Use per-page overlay scrollbar settings.
      if (!ScrollbarThemeSettings::OverlayScrollbarsEnabled() ||
          !GetDocument().GetFrame()->IsLocalRoot()) {
        GetDocument().UpdateStyleAndLayoutForNode(
            this, DocumentUpdateReason::kJavaScript);
      }
      if (GetDocument().GetPage()->GetSettings().GetForceZeroLayoutHeight())
        return AdjustForAbsoluteZoom::AdjustLayoutUnit(
                   layout_view->OverflowClipRect(PhysicalOffset()).Width(),
                   layout_view->StyleRef())
            .Round();
      return AdjustForAbsoluteZoom::AdjustLayoutUnit(
                 LayoutUnit(layout_view->GetLayoutSize().Width()),
                 layout_view->StyleRef())
          .Round();
    }
  }

  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  if (LayoutBox* layout_object = GetLayoutBox())
    return AdjustForAbsoluteZoom::AdjustLayoutUnit(
               LayoutUnit(
                   layout_object
                       ->PixelSnappedClientWidthWithTableSpecialBehavior()),
               layout_object->StyleRef())
        .Round();
  return 0;
}

int Element::clientHeight() {
  // When in strict mode, clientHeight for the document element should return
  // the height of the containing frame.
  // When in quirks mode, clientHeight for the body element should return the
  // height of the containing frame.
  bool in_quirks_mode = GetDocument().InQuirksMode();

  if ((!in_quirks_mode && GetDocument().documentElement() == this) ||
      (in_quirks_mode && IsHTMLElement() && GetDocument().body() == this)) {
    auto* layout_view = GetDocument().GetLayoutView();
    if (layout_view) {
      // TODO(crbug.com/740879): Use per-page overlay scrollbar settings.
      if (!ScrollbarThemeSettings::OverlayScrollbarsEnabled() ||
          !GetDocument().GetFrame()->IsLocalRoot()) {
        GetDocument().UpdateStyleAndLayoutForNode(
            this, DocumentUpdateReason::kJavaScript);
      }
      if (GetDocument().GetPage()->GetSettings().GetForceZeroLayoutHeight())
        return AdjustForAbsoluteZoom::AdjustLayoutUnit(
                   layout_view->OverflowClipRect(PhysicalOffset()).Height(),
                   layout_view->StyleRef())
            .Round();
      return AdjustForAbsoluteZoom::AdjustLayoutUnit(
                 LayoutUnit(layout_view->GetLayoutSize().Height()),
                 layout_view->StyleRef())
          .Round();
    }
  }

  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  if (LayoutBox* layout_object = GetLayoutBox())
    return AdjustForAbsoluteZoom::AdjustLayoutUnit(
               LayoutUnit(
                   layout_object
                       ->PixelSnappedClientHeightWithTableSpecialBehavior()),
               layout_object->StyleRef())
        .Round();
  return 0;
}

LayoutBox* Element::GetLayoutBoxForScrolling() const {
  LayoutBox* box = GetLayoutBox();
  if (!box || !box->HasNonVisibleOverflow())
    return nullptr;
  return box;
}

double Element::scrollLeft() {
  if (!InActiveDocument())
    return 0;

  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  if (GetDocument().ScrollingElementNoLayout() == this) {
    if (GetDocument().domWindow())
      return GetDocument().domWindow()->scrollX();
    return 0;
  }

  LayoutBox* box = GetLayoutBoxForScrolling();
  if (!box)
    return 0;
  if (PaintLayerScrollableArea* scrollable_area = box->GetScrollableArea()) {
    DCHECK(GetLayoutBox());

    if (HasLeftwardDirection(*this)) {
      UseCounter::Count(
          GetDocument(),
          WebFeature::
              kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTop);
    }

    // In order to keep the behavior of element scroll consistent with document
    // scroll, and consistent with the behavior of other vendors, the scrollLeft
    // of a box is changed to the offset from |ScrollOrigin()|.
    if (RuntimeEnabledFeatures::CSSOMViewScrollCoordinatesEnabled()) {
      return AdjustForAbsoluteZoom::AdjustScroll(
          scrollable_area->GetScrollOffset().Width(), *GetLayoutBox());
    } else {
      return AdjustForAbsoluteZoom::AdjustScroll(
          scrollable_area->ScrollPosition().X(), *GetLayoutBox());
    }
  }

  return 0;
}

double Element::scrollTop() {
  if (!InActiveDocument())
    return 0;

  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  if (GetDocument().ScrollingElementNoLayout() == this) {
    if (GetDocument().domWindow())
      return GetDocument().domWindow()->scrollY();
    return 0;
  }

  LayoutBox* box = GetLayoutBoxForScrolling();
  if (!box)
    return 0;
  if (PaintLayerScrollableArea* scrollable_area = box->GetScrollableArea()) {
    DCHECK(GetLayoutBox());

    if (HasUpwardDirection(*this)) {
      UseCounter::Count(
          GetDocument(),
          WebFeature::
              kElementWithLeftwardOrUpwardOverflowDirection_ScrollLeftOrTop);
    }

    // In order to keep the behavior of element scroll consistent with document
    // scroll, and consistent with the behavior of other vendors, the scrollTop
    // of a box is changed to the offset from |ScrollOrigin()|.
    if (RuntimeEnabledFeatures::CSSOMViewScrollCoordinatesEnabled()) {
      return AdjustForAbsoluteZoom::AdjustScroll(
          scrollable_area->GetScrollOffset().Height(), *GetLayoutBox());
    } else {
      return AdjustForAbsoluteZoom::AdjustScroll(
          scrollable_area->ScrollPosition().Y(), *GetLayoutBox());
    }
  }

  return 0;
}

void Element::setScrollLeft(double new_left) {
  if (!InActiveDocument())
    return;

  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  new_left = ScrollableArea::NormalizeNonFiniteScroll(new_left);

  if (GetDocument().ScrollingElementNoLayout() == this) {
    if (LocalDOMWindow* window = GetDocument().domWindow()) {
      ScrollToOptions* options = ScrollToOptions::Create();
      options->setLeft(new_left);
      window->scrollTo(options);
    }
    return;
  }

  LayoutBox* box = GetLayoutBoxForScrolling();
  if (!box)
    return;
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

    if (RuntimeEnabledFeatures::CSSOMViewScrollCoordinatesEnabled()) {
      ScrollOffset end_offset(new_left * box->Style()->EffectiveZoom(),
                              scrollable_area->GetScrollOffset().Height());
      std::unique_ptr<cc::SnapSelectionStrategy> strategy =
          cc::SnapSelectionStrategy::CreateForEndPosition(
              gfx::ScrollOffset(
                  scrollable_area->ScrollOffsetToPosition(end_offset)),
              true, false);
      base::Optional<FloatPoint> snap_point =
          scrollable_area->GetSnapPositionAndSetTarget(*strategy);
      if (snap_point.has_value()) {
        end_offset =
            scrollable_area->ScrollPositionToOffset(snap_point.value());
      }
      scrollable_area->SetScrollOffset(end_offset,
                                       mojom::blink::ScrollType::kProgrammatic,
                                       mojom::blink::ScrollBehavior::kAuto);
    } else {
      FloatPoint end_point(new_left * box->Style()->EffectiveZoom(),
                           scrollable_area->ScrollPosition().Y());
      std::unique_ptr<cc::SnapSelectionStrategy> strategy =
          cc::SnapSelectionStrategy::CreateForEndPosition(
              gfx::ScrollOffset(end_point), true, false);
      end_point =
          scrollable_area->GetSnapPositionAndSetTarget(*strategy).value_or(
              end_point);

      FloatPoint new_position(end_point.X(),
                              scrollable_area->ScrollPosition().Y());
      scrollable_area->ScrollToAbsolutePosition(
          new_position, mojom::blink::ScrollBehavior::kAuto);
    }
  }
}

void Element::setScrollTop(double new_top) {
  if (!InActiveDocument())
    return;

  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  new_top = ScrollableArea::NormalizeNonFiniteScroll(new_top);

  if (GetDocument().ScrollingElementNoLayout() == this) {
    if (LocalDOMWindow* window = GetDocument().domWindow()) {
      ScrollToOptions* options = ScrollToOptions::Create();
      options->setTop(new_top);
      window->scrollTo(options);
    }
    return;
  }

  LayoutBox* box = GetLayoutBoxForScrolling();
  if (!box)
    return;
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

    if (RuntimeEnabledFeatures::CSSOMViewScrollCoordinatesEnabled()) {
      ScrollOffset end_offset(scrollable_area->GetScrollOffset().Width(),
                              new_top * box->Style()->EffectiveZoom());
      std::unique_ptr<cc::SnapSelectionStrategy> strategy =
          cc::SnapSelectionStrategy::CreateForEndPosition(
              gfx::ScrollOffset(
                  scrollable_area->ScrollOffsetToPosition(end_offset)),
              false, true);
      base::Optional<FloatPoint> snap_point =
          scrollable_area->GetSnapPositionAndSetTarget(*strategy);
      if (snap_point.has_value()) {
        end_offset =
            scrollable_area->ScrollPositionToOffset(snap_point.value());
      }

      scrollable_area->SetScrollOffset(end_offset,
                                       mojom::blink::ScrollType::kProgrammatic,
                                       mojom::blink::ScrollBehavior::kAuto);
    } else {
      FloatPoint end_point(scrollable_area->ScrollPosition().X(),
                           new_top * box->Style()->EffectiveZoom());
      std::unique_ptr<cc::SnapSelectionStrategy> strategy =
          cc::SnapSelectionStrategy::CreateForEndPosition(
              gfx::ScrollOffset(end_point), false, true);
      end_point =
          scrollable_area->GetSnapPositionAndSetTarget(*strategy).value_or(
              end_point);
      FloatPoint new_position(scrollable_area->ScrollPosition().X(),
                              end_point.Y());
      scrollable_area->ScrollToAbsolutePosition(
          new_position, mojom::blink::ScrollBehavior::kAuto);
    }
  }
}

int Element::scrollWidth() {
  if (!InActiveDocument())
    return 0;

  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  if (GetDocument().ScrollingElementNoLayout() == this) {
    if (GetDocument().View()) {
      return AdjustForAbsoluteZoom::AdjustInt(
          GetDocument().View()->LayoutViewport()->ContentsSize().Width(),
          GetDocument().GetFrame()->PageZoomFactor());
    }
    return 0;
  }

  if (LayoutBox* box = GetLayoutBox()) {
    return AdjustForAbsoluteZoom::AdjustInt(box->PixelSnappedScrollWidth(),
                                            box);
  }
  return 0;
}

int Element::scrollHeight() {
  if (!InActiveDocument())
    return 0;

  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  if (GetDocument().ScrollingElementNoLayout() == this) {
    if (GetDocument().View()) {
      return AdjustForAbsoluteZoom::AdjustInt(
          GetDocument().View()->LayoutViewport()->ContentsSize().Height(),
          GetDocument().GetFrame()->PageZoomFactor());
    }
    return 0;
  }

  if (LayoutBox* box = GetLayoutBox()) {
    return AdjustForAbsoluteZoom::AdjustInt(box->PixelSnappedScrollHeight(),
                                            box);
  }
  return 0;
}

void Element::scrollBy(double x, double y) {
  ScrollToOptions* scroll_to_options = ScrollToOptions::Create();
  scroll_to_options->setLeft(x);
  scroll_to_options->setTop(y);
  scrollBy(scroll_to_options);
}

void Element::scrollBy(const ScrollToOptions* scroll_to_options) {
  if (!InActiveDocument())
    return;

  // FIXME: This should be removed once scroll updates are processed only after
  // the compositing update. See http://crbug.com/420741.
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  if (GetDocument().ScrollingElementNoLayout() == this) {
    ScrollFrameBy(scroll_to_options);
  } else {
    ScrollLayoutBoxBy(scroll_to_options);
  }
}

void Element::scrollTo(double x, double y) {
  ScrollToOptions* scroll_to_options = ScrollToOptions::Create();
  scroll_to_options->setLeft(x);
  scroll_to_options->setTop(y);
  scrollTo(scroll_to_options);
}

void Element::scrollTo(const ScrollToOptions* scroll_to_options) {
  if (!InActiveDocument())
    return;

  // FIXME: This should be removed once scroll updates are processed only after
  // the compositing update. See http://crbug.com/420741.
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  if (GetDocument().ScrollingElementNoLayout() == this) {
    ScrollFrameTo(scroll_to_options);
  } else {
    ScrollLayoutBoxTo(scroll_to_options);
  }
}

void Element::ScrollLayoutBoxBy(const ScrollToOptions* scroll_to_options) {
  gfx::ScrollOffset displacement;
  if (scroll_to_options->hasLeft()) {
    displacement.set_x(
        ScrollableArea::NormalizeNonFiniteScroll(scroll_to_options->left()));
  }
  if (scroll_to_options->hasTop()) {
    displacement.set_y(
        ScrollableArea::NormalizeNonFiniteScroll(scroll_to_options->top()));
  }

  mojom::blink::ScrollBehavior scroll_behavior =
      mojom::blink::ScrollBehavior::kAuto;
  ScrollableArea::ScrollBehaviorFromString(scroll_to_options->behavior(),
                                           scroll_behavior);
  LayoutBox* box = GetLayoutBoxForScrolling();
  if (!box)
    return;
  if (PaintLayerScrollableArea* scrollable_area = box->GetScrollableArea()) {
    DCHECK(box);
    gfx::ScrollOffset current_position(scrollable_area->ScrollPosition().X(),
                                       scrollable_area->ScrollPosition().Y());
    displacement.Scale(box->Style()->EffectiveZoom());
    gfx::ScrollOffset new_offset(current_position + displacement);
    FloatPoint new_position(new_offset.x(), new_offset.y());

    std::unique_ptr<cc::SnapSelectionStrategy> strategy =
        cc::SnapSelectionStrategy::CreateForEndAndDirection(
            current_position, displacement,
            RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled());
    new_position =
        scrollable_area->GetSnapPositionAndSetTarget(*strategy).value_or(
            new_position);
    scrollable_area->ScrollToAbsolutePosition(new_position, scroll_behavior);
  }
}

void Element::ScrollLayoutBoxTo(const ScrollToOptions* scroll_to_options) {
  mojom::blink::ScrollBehavior scroll_behavior =
      mojom::blink::ScrollBehavior::kAuto;
  ScrollableArea::ScrollBehaviorFromString(scroll_to_options->behavior(),
                                           scroll_behavior);

  LayoutBox* box = GetLayoutBoxForScrolling();
  if (!box)
    return;
  if (PaintLayerScrollableArea* scrollable_area = box->GetScrollableArea()) {
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

    // In order to keep the behavior of element scroll consistent with document
    // scroll, and consistent with the behavior of other vendors, the
    // offsets in |scroll_to_options| are treated as the offset from
    // |ScrollOrigin()|.
    if (RuntimeEnabledFeatures::CSSOMViewScrollCoordinatesEnabled()) {
      ScrollOffset new_offset = scrollable_area->GetScrollOffset();
      if (scroll_to_options->hasLeft()) {
        new_offset.SetWidth(ScrollableArea::NormalizeNonFiniteScroll(
                                scroll_to_options->left()) *
                            box->Style()->EffectiveZoom());
      }
      if (scroll_to_options->hasTop()) {
        new_offset.SetHeight(
            ScrollableArea::NormalizeNonFiniteScroll(scroll_to_options->top()) *
            box->Style()->EffectiveZoom());
      }

      std::unique_ptr<cc::SnapSelectionStrategy> strategy =
          cc::SnapSelectionStrategy::CreateForEndPosition(
              gfx::ScrollOffset(
                  scrollable_area->ScrollOffsetToPosition(new_offset)),
              scroll_to_options->hasLeft(), scroll_to_options->hasTop());
      base::Optional<FloatPoint> snap_point =
          scrollable_area->GetSnapPositionAndSetTarget(*strategy);
      if (snap_point.has_value()) {
        new_offset =
            scrollable_area->ScrollPositionToOffset(snap_point.value());
      }

      scrollable_area->SetScrollOffset(
          new_offset, mojom::blink::ScrollType::kProgrammatic, scroll_behavior);
    } else {
      FloatPoint new_position(scrollable_area->ScrollPosition().X(),
                              scrollable_area->ScrollPosition().Y());
      if (scroll_to_options->hasLeft()) {
        new_position.SetX(ScrollableArea::NormalizeNonFiniteScroll(
                              scroll_to_options->left()) *
                          box->Style()->EffectiveZoom());
      }
      if (scroll_to_options->hasTop()) {
        new_position.SetY(
            ScrollableArea::NormalizeNonFiniteScroll(scroll_to_options->top()) *
            box->Style()->EffectiveZoom());
      }

      std::unique_ptr<cc::SnapSelectionStrategy> strategy =
          cc::SnapSelectionStrategy::CreateForEndPosition(
              gfx::ScrollOffset(new_position), scroll_to_options->hasLeft(),
              scroll_to_options->hasTop());
      new_position =
          scrollable_area->GetSnapPositionAndSetTarget(*strategy).value_or(
              new_position);
      scrollable_area->ScrollToAbsolutePosition(new_position, scroll_behavior);
    }
  }
}

void Element::ScrollFrameBy(const ScrollToOptions* scroll_to_options) {
  gfx::ScrollOffset displacement;
  if (scroll_to_options->hasLeft()) {
    displacement.set_x(
        ScrollableArea::NormalizeNonFiniteScroll(scroll_to_options->left()));
  }
  if (scroll_to_options->hasTop()) {
    displacement.set_y(
        ScrollableArea::NormalizeNonFiniteScroll(scroll_to_options->top()));
  }

  mojom::blink::ScrollBehavior scroll_behavior =
      mojom::blink::ScrollBehavior::kAuto;
  ScrollableArea::ScrollBehaviorFromString(scroll_to_options->behavior(),
                                           scroll_behavior);
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame || !frame->View() || !GetDocument().GetPage())
    return;

  ScrollableArea* viewport = frame->View()->LayoutViewport();
  if (!viewport)
    return;

  displacement.Scale(frame->PageZoomFactor());
  FloatPoint new_position = viewport->ScrollPosition() +
                            FloatPoint(displacement.x(), displacement.y());

  gfx::ScrollOffset current_position(viewport->ScrollPosition());
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForEndAndDirection(
          current_position, displacement,
          RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled());
  new_position =
      viewport->GetSnapPositionAndSetTarget(*strategy).value_or(new_position);
  viewport->SetScrollOffset(viewport->ScrollPositionToOffset(new_position),
                            mojom::blink::ScrollType::kProgrammatic,
                            scroll_behavior);
}

void Element::ScrollFrameTo(const ScrollToOptions* scroll_to_options) {
  mojom::blink::ScrollBehavior scroll_behavior =
      mojom::blink::ScrollBehavior::kAuto;
  ScrollableArea::ScrollBehaviorFromString(scroll_to_options->behavior(),
                                           scroll_behavior);
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame || !frame->View() || !GetDocument().GetPage())
    return;

  ScrollableArea* viewport = frame->View()->LayoutViewport();
  if (!viewport)
    return;

  ScrollOffset new_offset = viewport->GetScrollOffset();
  if (scroll_to_options->hasLeft()) {
    new_offset.SetWidth(
        ScrollableArea::NormalizeNonFiniteScroll(scroll_to_options->left()) *
        frame->PageZoomFactor());
  }
  if (scroll_to_options->hasTop()) {
    new_offset.SetHeight(
        ScrollableArea::NormalizeNonFiniteScroll(scroll_to_options->top()) *
        frame->PageZoomFactor());
  }

  FloatPoint new_position = viewport->ScrollOffsetToPosition(new_offset);
  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForEndPosition(
          gfx::ScrollOffset(new_position), scroll_to_options->hasLeft(),
          scroll_to_options->hasTop());
  new_position =
      viewport->GetSnapPositionAndSetTarget(*strategy).value_or(new_position);
  new_offset = viewport->ScrollPositionToOffset(new_position);
  viewport->SetScrollOffset(new_offset, mojom::blink::ScrollType::kProgrammatic,
                            scroll_behavior);
}

IntRect Element::BoundsInViewport() const {
  GetDocument().EnsurePaintLocationDataValidForNode(
      this, DocumentUpdateReason::kUnknown);

  LocalFrameView* view = GetDocument().View();
  if (!view)
    return IntRect();

  Vector<FloatQuad> quads;

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
          GetLayoutObject()->ObjectBoundingBox()));
    }
  } else {
    // Get the bounding rectangle from the box model.
    if (GetLayoutBoxModelObject())
      GetLayoutBoxModelObject()->AbsoluteQuads(quads);
  }

  if (quads.IsEmpty())
    return IntRect();

  IntRect result = quads[0].EnclosingBoundingBox();
  for (wtf_size_t i = 1; i < quads.size(); ++i)
    result.Unite(quads[i].EnclosingBoundingBox());

  return view->FrameToViewport(result);
}

Vector<IntRect> Element::OutlineRectsInVisualViewport(
    DocumentUpdateReason reason) const {
  Vector<IntRect> rects;

  LocalFrameView* view = GetDocument().View();
  if (!view)
    return rects;

  GetDocument().EnsurePaintLocationDataValidForNode(this, reason);

  LayoutBoxModelObject* layout_object = GetLayoutBoxModelObject();
  if (!layout_object)
    return rects;

  Vector<PhysicalRect> outline_rects = layout_object->OutlineRects(
      PhysicalOffset(),
      layout_object->OutlineRectsShouldIncludeBlockVisualOverflow());
  for (auto& r : outline_rects) {
    PhysicalRect physical_rect = layout_object->LocalToAbsoluteRect(r);
    IntRect absolute_rect =
        view->FrameToViewport(PixelSnappedIntRect(physical_rect));
    rects.push_back(absolute_rect);
  }

  return rects;
}

IntRect Element::VisibleBoundsInVisualViewport() const {
  if (!GetLayoutObject() || !GetDocument().GetPage() ||
      !GetDocument().GetFrame())
    return IntRect();

  // We don't use absoluteBoundingBoxRect() because it can return an IntRect
  // larger the actual size by 1px. crbug.com/470503
  PhysicalRect rect(
      RoundedIntRect(GetLayoutObject()->AbsoluteBoundingBoxFloatRect()));
  PhysicalRect frame_clip_rect =
      GetDocument().View()->GetLayoutView()->ClippingRect(PhysicalOffset());
  rect.Intersect(frame_clip_rect);

  // MapToVisualRectInAncestorSpace, called with a null ancestor argument,
  // returns the viewport-visible rect in the root frame's coordinate space.
  // MapToVisualRectInAncestorSpace applies ancestors' frame's clipping but does
  // not apply (overflow) element clipping.
  GetDocument().View()->GetLayoutView()->MapToVisualRectInAncestorSpace(
      nullptr, rect, kTraverseDocumentBoundaries, kDefaultVisualRectFlags);

  // TODO(layout-dev): Callers of this method don't expect the offset of the
  // local frame root from a remote top-level frame to be applied here. They
  // expect the result to be in the coordinate system of the local root frame.
  // Either the method should be renamed to something which communicates that,
  // or callers should be updated to expect actual top-level frame coordinates.
  rect = GetDocument()
             .GetFrame()
             ->LocalFrameRoot()
             .ContentLayoutObject()
             ->AbsoluteToLocalRect(rect, kTraverseDocumentBoundaries |
                                             kApplyRemoteMainFrameTransform);

  IntRect visible_rect = PixelSnappedIntRect(rect);
  // If the rect is in the coordinates of the main frame, then it should
  // also be clipped to the viewport to account for page scale. For OOPIFs,
  // local frame root -> viewport coordinate conversion is done in the
  // browser process.
  if (GetDocument().GetFrame()->LocalFrameRoot().IsMainFrame()) {
    IntSize viewport_size = GetDocument().GetPage()->GetVisualViewport().Size();
    visible_rect =
        GetDocument().GetPage()->GetVisualViewport().RootFrameToViewport(
            visible_rect);
    visible_rect.Intersect(IntRect(IntPoint(), viewport_size));
  }
  return visible_rect;
}

void Element::ClientQuads(Vector<FloatQuad>& quads) {
  GetDocument().EnsurePaintLocationDataValidForNode(
      this, DocumentUpdateReason::kJavaScript);

  LayoutObject* element_layout_object = GetLayoutObject();
  if (!element_layout_object)
    return;

  // Foreign objects need to convert between SVG and HTML coordinate spaces and
  // cannot use LocalToAbsoluteQuad directly with ObjectBoundingBox which is
  // SVG coordinates and not HTML coordinates. Instead, use the AbsoluteQuads
  // codepath below.
  auto* svg_element = DynamicTo<SVGElement>(this);
  if (svg_element && !element_layout_object->IsSVGRoot() &&
      !element_layout_object->IsSVGForeignObject()) {
    // Get the bounding rectangle from the SVG model.
    // TODO(pdr): ObjectBoundingBox does not include stroke and the spec is not
    // clear (see: https://github.com/w3c/svgwg/issues/339, crbug.com/529734).
    // If stroke is desired, we can update this to use AbsoluteQuads, below.
    if (IsA<SVGGraphicsElement>(svg_element)) {
      quads.push_back(element_layout_object->LocalToAbsoluteQuad(
          element_layout_object->ObjectBoundingBox()));
    }
    return;
  }

  // FIXME: Handle table/inline-table with a caption.
  if (element_layout_object->IsBoxModelObject() ||
      element_layout_object->IsBR())
    element_layout_object->AbsoluteQuads(quads);
}

DOMRectList* Element::getClientRects() {
  Vector<FloatQuad> quads;
  ClientQuads(quads);
  if (quads.IsEmpty())
    return MakeGarbageCollected<DOMRectList>();

  LayoutObject* element_layout_object = GetLayoutObject();
  DCHECK(element_layout_object);
  GetDocument().AdjustFloatQuadsForScrollAndAbsoluteZoom(
      quads, *element_layout_object);
  return MakeGarbageCollected<DOMRectList>(quads);
}

DOMRect* Element::getBoundingClientRect() {
  Vector<FloatQuad> quads;
  ClientQuads(quads);
  if (quads.IsEmpty())
    return DOMRect::Create();

  FloatRect result = quads[0].BoundingBox();
  for (wtf_size_t i = 1; i < quads.size(); ++i)
    result.Unite(quads[i].BoundingBox());

  LayoutObject* element_layout_object = GetLayoutObject();
  DCHECK(element_layout_object);
  GetDocument().AdjustFloatRectForScrollAndAbsoluteZoom(result,
                                                        *element_layout_object);
  return DOMRect::FromFloatRect(result);
}

const AtomicString& Element::computedRole() {
  Document& document = GetDocument();
  if (!document.IsActive())
    return g_null_atom;
  document.UpdateStyleAndLayoutForNode(this, DocumentUpdateReason::kJavaScript);
  UpdateDistributionForFlatTreeTraversal();
  AXContext ax_context(document);
  return ax_context.GetAXObjectCache().ComputedRoleForNode(this);
}

String Element::computedName() {
  Document& document = GetDocument();
  if (!document.IsActive())
    return String();
  document.UpdateStyleAndLayoutForNode(this, DocumentUpdateReason::kJavaScript);
  UpdateDistributionForFlatTreeTraversal();
  AXContext ax_context(document);
  return ax_context.GetAXObjectCache().ComputedNameForNode(this);
}

AccessibleNode* Element::ExistingAccessibleNode() const {
  if (!RuntimeEnabledFeatures::AccessibilityObjectModelEnabled())
    return nullptr;

  if (!HasRareData())
    return nullptr;

  return GetElementRareData()->GetAccessibleNode();
}

AccessibleNode* Element::accessibleNode() {
  if (!RuntimeEnabledFeatures::AccessibilityObjectModelEnabled())
    return nullptr;

  ElementRareData& rare_data = EnsureElementRareData();
  return rare_data.EnsureAccessibleNode(this);
}

bool Element::toggleAttribute(const AtomicString& qualified_name,
                              ExceptionState& exception_state) {
  // https://dom.spec.whatwg.org/#dom-element-toggleattribute
  // 1. If qualifiedName does not match the Name production in XML, then throw
  // an "InvalidCharacterError" DOMException.
  if (!Document::IsValidName(qualified_name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        "'" + qualified_name + "' is not a valid attribute name.");
    return false;
  }
  // 2. If the context object is in the HTML namespace and its node document is
  // an HTML document, then set qualifiedName to qualifiedName in ASCII
  // lowercase.
  AtomicString lowercase_name = LowercaseIfNecessary(qualified_name);
  WTF::AtomicStringTable::WeakResult hint(lowercase_name.Impl());
  // 3. Let attribute be the first attribute in the context object’s attribute
  // list whose qualified name is qualifiedName, and null otherwise.
  // 4. If attribute is null, then
  if (!GetAttributeHinted(lowercase_name, hint)) {
    // 4. 1. If force is not given or is true, create an attribute whose local
    // name is qualified_name, value is the empty string, and node document is
    // the context object’s node document, then append this attribute to the
    // context object, and then return true.
    SetAttributeHinted(lowercase_name, hint, g_empty_atom);
    return true;
  }
  // 5. Otherwise, if force is not given or is false, remove an attribute given
  // qualifiedName and the context object, and then return false.
  RemoveAttributeHinted(lowercase_name, hint);
  return false;
}

bool Element::toggleAttribute(const AtomicString& qualified_name,
                              bool force,
                              ExceptionState& exception_state) {
  // https://dom.spec.whatwg.org/#dom-element-toggleattribute
  // 1. If qualifiedName does not match the Name production in XML, then throw
  // an "InvalidCharacterError" DOMException.
  if (!Document::IsValidName(qualified_name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        "'" + qualified_name + "' is not a valid attribute name.");
    return false;
  }
  // 2. If the context object is in the HTML namespace and its node document is
  // an HTML document, then set qualifiedName to qualifiedName in ASCII
  // lowercase.
  AtomicString lowercase_name = LowercaseIfNecessary(qualified_name);
  WTF::AtomicStringTable::WeakResult hint(lowercase_name.Impl());
  // 3. Let attribute be the first attribute in the context object’s attribute
  // list whose qualified name is qualifiedName, and null otherwise.
  // 4. If attribute is null, then
  if (!GetAttributeHinted(lowercase_name, hint)) {
    // 4. 1. If force is not given or is true, create an attribute whose local
    // name is qualified_name, value is the empty string, and node document is
    // the context object’s node document, then append this attribute to the
    // context object, and then return true.
    if (force) {
      SetAttributeHinted(lowercase_name, hint, g_empty_atom);
      return true;
    }
    // 4. 2. Return false.
    return false;
  }
  // 5. Otherwise, if force is not given or is false, remove an attribute given
  // qualifiedName and the context object, and then return false.
  if (!force) {
    RemoveAttributeHinted(lowercase_name, hint);
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

SpecificTrustedType Element::ExpectedTrustedTypeForAttribute(
    const QualifiedName& q_name) const {
  // There are only a handful of namespaced attributes we care about
  // (xlink:href), and all of those have identical Trusted Types
  // properties to their namespace-less counterpart. So we check whether this
  // is one of SVG's 'known' attributes, and if so just check the local
  // name part as usual.
  if (!q_name.NamespaceURI().IsNull() &&
      !SVGAnimatedHref::IsKnownAttribute(q_name)) {
    return SpecificTrustedType::kNone;
  }

  const AttrNameToTrustedType* attribute_types = &GetCheckedAttributeTypes();
  AttrNameToTrustedType::const_iterator iter =
      attribute_types->find(q_name.LocalName());
  if (iter != attribute_types->end())
    return iter->value;

  if (q_name.LocalName().StartsWith("on")) {
    // TODO(jakubvrana): This requires TrustedScript in all attributes
    // starting with "on", including e.g. "one". We use this pattern elsewhere
    // (e.g. in IsEventHandlerAttribute) but it's not ideal. Consider using
    // the event attribute of the resulting AttributeTriggers.
    return SpecificTrustedType::kScript;
  }

  return SpecificTrustedType::kNone;
}

void Element::setAttribute(const QualifiedName& name,
                           const String& string,
                           ExceptionState& exception_state) {
  // TODO(lyf): Removes |exception_state| because this function never throws.
  setAttribute(name, AtomicString(string));
}

static inline AtomicString MakeIdForStyleResolution(const AtomicString& value,
                                                    bool in_quirks_mode) {
  if (in_quirks_mode)
    return value.LowerASCII();
  return value;
}

DISABLE_CFI_PERF
void Element::AttributeChanged(const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (ShadowRoot* parent_shadow_root =
          ShadowRootWhereNodeCanBeDistributedForV0(*this)) {
    if (ShouldInvalidateDistributionWhenAttributeChanged(
            *parent_shadow_root, name, params.new_value))
      parent_shadow_root->SetNeedsDistributionRecalc();
  }
  if (name == html_names::kSlotAttr && params.old_value != params.new_value) {
    if (ShadowRoot* root = V1ShadowRootOfParent())
      root->DidChangeHostChildSlotName(params.old_value, params.new_value);
  }

  ParseAttribute(params);

  GetDocument().IncDOMTreeVersion();

  if (name == html_names::kIdAttr) {
    AtomicString new_id = MakeIdForStyleResolution(
        params.new_value, GetDocument().InQuirksMode());
    if (new_id != GetElementData()->IdForStyleResolution()) {
      AtomicString old_id = GetElementData()->SetIdForStyleResolution(new_id);
      GetDocument().GetStyleEngine().IdChangedForElement(old_id, new_id, *this);
    }
  } else if (name == html_names::kClassAttr) {
    ClassAttributeChanged(params.new_value);
    if (HasRareData() && GetElementRareData()->GetClassList()) {
      GetElementRareData()->GetClassList()->DidUpdateAttributeValue(
          params.old_value, params.new_value);
    }
  } else if (name == html_names::kNameAttr) {
    SetHasName(!params.new_value.IsNull());
  } else if (name == html_names::kPartAttr) {
    part().DidUpdateAttributeValue(params.old_value, params.new_value);
    GetDocument().GetStyleEngine().PartChangedForElement(*this);
  } else if (name == html_names::kExportpartsAttr) {
    EnsureElementRareData().SetPartNamesMap(params.new_value);
    GetDocument().GetStyleEngine().ExportpartsChangedForElement(*this);
  } else if (IsElementReflectionAttribute(name)) {
    SynchronizeContentAttributeAndElementReference(name);
  } else if (IsStyledElement()) {
    if (name == html_names::kStyleAttr) {
      StyleAttributeChanged(params.new_value, params.reason);
    } else if (IsPresentationAttribute(name)) {
      GetElementData()->SetPresentationAttributeStyleIsDirty(true);
      SetNeedsStyleRecalc(kLocalStyleChange,
                          StyleChangeReasonForTracing::FromAttribute(name));
    }
  }

  InvalidateNodeListCachesInAncestors(&name, this, nullptr);

  if (isConnected()) {
    if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache()) {
      if (params.old_value != params.new_value)
        cache->HandleAttributeChanged(name, this);
    }
  }

  if (params.reason == AttributeModificationReason::kDirectly &&
      name == html_names::kTabindexAttr &&
      AdjustedFocusedElementInTreeScope() == this) {
    // The attribute change may cause supportsFocus() to return false
    // for the element which had focus.
    //
    // TODO(tkent): We should avoid updating style.  We'd like to check only
    // DOM-level focusability here.
    GetDocument().UpdateStyleAndLayoutTreeForNode(this);
    if (!SupportsFocus())
      blur();
  }
}

bool Element::HasLegalLinkAttribute(const QualifiedName&) const {
  return false;
}

const QualifiedName& Element::SubResourceAttributeName() const {
  return QualifiedName::Null();
}

template <typename CharacterType>
static inline ClassStringContent ClassStringHasClassName(
    const CharacterType* characters,
    unsigned length) {
  DCHECK_GT(length, 0u);

  unsigned i = 0;
  do {
    if (IsNotHTMLSpace<CharacterType>(characters[i]))
      break;
    ++i;
  } while (i < length);

  if (i == length && length >= 1)
    return ClassStringContent::kWhiteSpaceOnly;

  return ClassStringContent::kHasClasses;
}

static inline ClassStringContent ClassStringHasClassName(
    const AtomicString& new_class_string) {
  unsigned length = new_class_string.length();

  if (!length)
    return ClassStringContent::kEmpty;

  if (new_class_string.Is8Bit())
    return ClassStringHasClassName(new_class_string.Characters8(), length);
  return ClassStringHasClassName(new_class_string.Characters16(), length);
}

void Element::ClassAttributeChanged(const AtomicString& new_class_string) {
  DCHECK(GetElementData());
  ClassStringContent class_string_content_type =
      ClassStringHasClassName(new_class_string);
  const bool should_fold_case = GetDocument().InQuirksMode();
  if (class_string_content_type == ClassStringContent::kHasClasses) {
    const SpaceSplitString old_classes = GetElementData()->ClassNames();
    GetElementData()->SetClass(new_class_string, should_fold_case);
    const SpaceSplitString& new_classes = GetElementData()->ClassNames();
    GetDocument().GetStyleEngine().ClassChangedForElement(old_classes,
                                                          new_classes, *this);
  } else {
    const SpaceSplitString& old_classes = GetElementData()->ClassNames();
    GetDocument().GetStyleEngine().ClassChangedForElement(old_classes, *this);
    if (class_string_content_type == ClassStringContent::kWhiteSpaceOnly)
      GetElementData()->SetClass(new_class_string, should_fold_case);
    else
      GetElementData()->ClearClass();
  }
}

bool Element::ShouldInvalidateDistributionWhenAttributeChanged(
    ShadowRoot& shadow_root,
    const QualifiedName& name,
    const AtomicString& new_value) {
  if (shadow_root.IsV1())
    return false;
  const SelectRuleFeatureSet& feature_set =
      shadow_root.V0().EnsureSelectFeatureSet();

  if (name == html_names::kIdAttr) {
    AtomicString old_id = GetElementData()->IdForStyleResolution();
    AtomicString new_id =
        MakeIdForStyleResolution(new_value, GetDocument().InQuirksMode());
    if (new_id != old_id) {
      if (!old_id.IsEmpty() && feature_set.HasSelectorForId(old_id))
        return true;
      if (!new_id.IsEmpty() && feature_set.HasSelectorForId(new_id))
        return true;
    }
  }

  if (name == html_names::kClassAttr) {
    const AtomicString& new_class_string = new_value;
    if (ClassStringHasClassName(new_class_string) ==
        ClassStringContent::kHasClasses) {
      const SpaceSplitString& old_classes = GetElementData()->ClassNames();
      const SpaceSplitString new_classes(GetDocument().InQuirksMode()
                                             ? new_class_string.LowerASCII()
                                             : new_class_string);
      if (feature_set.CheckSelectorsForClassChange(old_classes, new_classes))
        return true;
    } else {
      const SpaceSplitString& old_classes = GetElementData()->ClassNames();
      if (feature_set.CheckSelectorsForClassChange(old_classes))
        return true;
    }
  }

  return feature_set.HasSelectorForAttribute(name.LocalName());
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
    Vector<Attribute>& attribute_vector) const {
  wtf_size_t destination = 0;
  for (wtf_size_t source = 0; source < attribute_vector.size(); ++source) {
    if (IsScriptingAttribute(attribute_vector[source]))
      continue;

    if (source != destination)
      attribute_vector[destination] = attribute_vector[source];

    ++destination;
  }
  attribute_vector.Shrink(destination);
}

void Element::ParserSetAttributes(const Vector<Attribute>& attribute_vector) {
  DCHECK(!isConnected());
  DCHECK(!parentNode());
  DCHECK(!element_data_);

  if (!attribute_vector.IsEmpty()) {
    if (GetDocument().GetElementDataCache())
      element_data_ =
          GetDocument()
              .GetElementDataCache()
              ->CachedShareableElementDataWithAttributes(attribute_vector);
    else
      element_data_ =
          ShareableElementData::CreateWithAttributes(attribute_vector);
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
  if (GetElementData() == other.GetElementData())
    return true;
  if (GetElementData())
    return GetElementData()->IsEquivalent(other.GetElementData());
  if (other.GetElementData())
    return other.GetElementData()->IsEquivalent(GetElementData());
  return true;
}

String Element::nodeName() const {
  return tag_name_.ToString();
}

AtomicString Element::LocalNameForSelectorMatching() const {
  if (IsHTMLElement() || !IsA<HTMLDocument>(GetDocument()))
    return localName();
  return localName().LowerASCII();
}

const AtomicString& Element::LocateNamespacePrefix(
    const AtomicString& namespace_to_locate) const {
  if (!prefix().IsNull() && namespaceURI() == namespace_to_locate)
    return prefix();

  AttributeCollection attributes = Attributes();
  for (const Attribute& attr : attributes) {
    if (attr.Prefix() == g_xmlns_atom && attr.Value() == namespace_to_locate)
      return attr.LocalName();
  }

  if (Element* parent = parentElement())
    return parent->LocateNamespacePrefix(namespace_to_locate);

  return g_null_atom;
}

const AtomicString Element::ImageSourceURL() const {
  return FastGetAttribute(html_names::kSrcAttr);
}

bool Element::LayoutObjectIsNeeded(const ComputedStyle& style) const {
  return style.Display() != EDisplay::kNone &&
         style.Display() != EDisplay::kContents;
}

LayoutObject* Element::CreateLayoutObject(const ComputedStyle& style,
                                          LegacyLayout legacy) {
  return LayoutObject::CreateObject(this, style, legacy);
}

Node::InsertionNotificationRequest Element::InsertedInto(
    ContainerNode& insertion_point) {
  // need to do superclass processing first so isConnected() is true
  // by the time we reach updateId
  ContainerNode::InsertedInto(insertion_point);

  DCHECK(!HasRareData() || !GetElementRareData()->HasPseudoElements());

  if (!insertion_point.IsInTreeScope())
    return kInsertionDone;

  if (isConnected() && HasRareData()) {
    ElementRareData* rare_data = GetElementRareData();
    if (ElementIntersectionObserverData* observer_data =
            rare_data->IntersectionObserverData()) {
      observer_data->InvalidateCachedRects();
      observer_data->TrackWithController(
          GetDocument().EnsureIntersectionObserverController());
      if (!observer_data->IsEmpty()) {
        if (LocalFrameView* frame_view = GetDocument().View()) {
          frame_view->SetIntersectionObservationState(
              LocalFrameView::kRequired);
        }
      }
    }

    if (auto* context = rare_data->GetDisplayLockContext())
      context->ElementConnected();
  }

  if (isConnected()) {
    EnqueueAutofocus(*this);

    if (GetCustomElementState() == CustomElementState::kCustom)
      CustomElement::EnqueueConnectedCallback(*this);
    else if (IsUpgradedV0CustomElement())
      V0CustomElement::DidAttach(this, GetDocument());
    else if (GetCustomElementState() == CustomElementState::kUndefined)
      CustomElement::TryToUpgrade(*this);
  }

  TreeScope& scope = insertion_point.GetTreeScope();
  if (scope != GetTreeScope())
    return kInsertionDone;

  const AtomicString& id_value = GetIdAttribute();
  if (!id_value.IsNull())
    UpdateId(scope, g_null_atom, id_value);

  const AtomicString& name_value = GetNameAttribute();
  if (!name_value.IsNull())
    UpdateName(g_null_atom, name_value);

  if (parentElement() && parentElement()->IsInCanvasSubtree())
    SetIsInCanvasSubtree(true);

  return kInsertionDone;
}

void Element::RemovedFrom(ContainerNode& insertion_point) {
  bool was_in_document = insertion_point.isConnected();

  SetComputedStyle(nullptr);

  if (Fullscreen::IsFullscreenElement(*this)) {
    SetContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(false);
    if (auto* insertion_point_element = DynamicTo<Element>(insertion_point)) {
      insertion_point_element->SetContainsFullScreenElement(false);
      insertion_point_element
          ->SetContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(
              false);
    }
  }

  if (GetDocument().GetPage())
    GetDocument().GetPage()->GetPointerLockController().ElementRemoved(this);

  SetSavedLayerScrollOffset(ScrollOffset());

  if (insertion_point.IsInTreeScope() && GetTreeScope() == GetDocument()) {
    const AtomicString& id_value = GetIdAttribute();
    if (!id_value.IsNull())
      UpdateId(insertion_point.GetTreeScope(), id_value, g_null_atom);

    const AtomicString& name_value = GetNameAttribute();
    if (!name_value.IsNull())
      UpdateName(name_value, g_null_atom);
  }

  ContainerNode::RemovedFrom(insertion_point);
  if (was_in_document) {
    if (this == GetDocument().CssTarget())
      GetDocument().SetCSSTarget(nullptr);

    if (GetCustomElementState() == CustomElementState::kCustom)
      CustomElement::EnqueueDisconnectedCallback(*this);
    else if (IsUpgradedV0CustomElement())
      V0CustomElement::DidDetach(this, insertion_point.GetDocument());
  }

  GetDocument().GetRootScrollerController().ElementRemoved(*this);

  if (IsInTopLayer()) {
    Fullscreen::ElementRemoved(*this);
    GetDocument().RemoveFromTopLayer(this);
  }

  ClearElementFlag(ElementFlags::kIsInCanvasSubtree);

  if (HasRareData()) {
    ElementRareData* data = GetElementRareData();

    data->ClearRestyleFlags();

    if (ElementAnimations* element_animations = data->GetElementAnimations())
      element_animations->CssAnimations().Cancel();

    if (was_in_document && data->IntersectionObserverData()) {
      data->IntersectionObserverData()->ComputeIntersectionsForTarget(
          IntersectionObservation::kExplicitRootObserversNeedUpdate |
          IntersectionObservation::kImplicitRootObserversNeedUpdate |
          IntersectionObservation::kIgnoreDelay);
      data->IntersectionObserverData()->StopTrackingWithController(
          GetDocument().EnsureIntersectionObserverController());
    }

    if (auto* context = data->GetDisplayLockContext())
      context->ElementDisconnected();

    DCHECK(!data->HasPseudoElements());
  }

  if (GetDocument().GetFrame())
    GetDocument().GetFrame()->GetEventHandler().ElementRemoved(this);
}

void Element::AttachLayoutTree(AttachContext& context) {
  DCHECK(GetDocument().InStyleRecalc());

  const ComputedStyle* style = GetComputedStyle();
  bool being_rendered =
      context.parent && style && !style->IsEnsuredInDisplayNone();
  if (!being_rendered && !ChildNeedsReattachLayoutTree()) {
    Node::AttachLayoutTree(context);
    return;
  }

  AttachContext children_context(context);
  LayoutObject* layout_object = nullptr;
  if (CanParticipateInFlatTree()) {
    if (being_rendered) {
      // If an element requires forced legacy layout, all descendants need it
      // too.
      if (ShouldForceLegacyLayout())
        children_context.force_legacy_layout = true;
      LegacyLayout legacy = children_context.force_legacy_layout
                                ? LegacyLayout::kForce
                                : LegacyLayout::kAuto;
      LayoutTreeBuilderForElement builder(*this, context, style, legacy);
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
  }
  children_context.use_previous_in_flow = true;

  AttachPseudoElement(kPseudoIdMarker, children_context);
  AttachPseudoElement(kPseudoIdBefore, children_context);

  if (ShadowRoot* shadow_root = GetShadowRoot()) {
    // When a shadow root exists, it does the work of attaching the children.
    shadow_root->AttachLayoutTree(children_context);
    Node::AttachLayoutTree(context);
    ClearChildNeedsReattachLayoutTree();
  } else {
    ContainerNode::AttachLayoutTree(children_context);
  }

  AttachPseudoElement(kPseudoIdAfter, children_context);
  AttachPseudoElement(kPseudoIdBackdrop, children_context);

  UpdateFirstLetterPseudoElement(StyleUpdatePhase::kAttachLayoutTree);
  AttachPseudoElement(kPseudoIdFirstLetter, children_context);

  if (layout_object) {
    if (layout_object->AffectsWhitespaceSiblings())
      context.previous_in_flow = layout_object;
  } else {
    context.previous_in_flow = children_context.previous_in_flow;
  }
}

void Element::DetachLayoutTree(bool performing_reattach) {
  HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
  if (HasRareData()) {
    ElementRareData* data = GetElementRareData();
    if (!performing_reattach)
      data->ClearPseudoElements();

    if (ElementAnimations* element_animations = data->GetElementAnimations()) {
      if (performing_reattach) {
        // FIXME: restart compositor animations rather than pull back to the
        // main thread
        element_animations->RestartAnimationOnCompositor();
      } else {
        DocumentLifecycle::DetachScope will_detach(GetDocument().Lifecycle());
        element_animations->CssAnimations().Cancel();
        element_animations->SetAnimationStyleChange(false);
      }
      element_animations->ClearBaseComputedStyle();
    }
  }

  DetachPseudoElement(kPseudoIdMarker, performing_reattach);
  DetachPseudoElement(kPseudoIdBefore, performing_reattach);

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

  DetachPseudoElement(kPseudoIdAfter, performing_reattach);
  DetachPseudoElement(kPseudoIdBackdrop, performing_reattach);
  DetachPseudoElement(kPseudoIdFirstLetter, performing_reattach);

  if (!performing_reattach) {
    UpdateCallbackSelectors(GetComputedStyle(), nullptr);
    SetComputedStyle(nullptr);
  }

  if (!performing_reattach && IsUserActionElement()) {
    if (IsHovered())
      GetDocument().HoveredElementDetached(*this);
    if (InActiveChain())
      GetDocument().ActiveChainNodeDetached(*this);
    GetDocument().UserActionElements().DidDetach(*this);
  }

  GetDocument().GetStyleEngine().ClearNeedsWhitespaceReattachmentFor(this);
}

scoped_refptr<ComputedStyle> Element::StyleForLayoutObject() {
  DCHECK(GetDocument().InStyleRecalc());

  // FIXME: Instead of clearing updates that may have been added from calls to
  // StyleForElement outside RecalcStyle, we should just never set them if we're
  // not inside RecalcStyle.
  if (ElementAnimations* element_animations = GetElementAnimations())
    element_animations->CssAnimations().ClearPendingUpdate();

  scoped_refptr<ComputedStyle> style = HasCustomStyleCallbacks()
                                           ? CustomStyleForLayoutObject()
                                           : OriginalStyleForLayoutObject();
  if (!style) {
    DCHECK(IsPseudoElement());
    return nullptr;
  }

  // StyleForElement() might add active animations so we need to get it again.
  if (ElementAnimations* element_animations = GetElementAnimations()) {
    element_animations->CssAnimations().MaybeApplyPendingUpdate(this);
    element_animations->UpdateAnimationFlags(*style);
  }

  style->UpdateIsStackingContextWithoutContainment(
      this == GetDocument().documentElement(), IsInTopLayer(),
      IsA<SVGForeignObjectElement>(*this));

  return style;
}

scoped_refptr<ComputedStyle> Element::OriginalStyleForLayoutObject() {
  return GetDocument().GetStyleResolver().StyleForElement(this);
}

void Element::RecalcStyleForTraversalRootAncestor() {
  if (!ChildNeedsReattachLayoutTree())
    UpdateFirstLetterPseudoElement(StyleUpdatePhase::kRecalc);
  if (HasCustomStyleCallbacks())
    DidRecalcStyle({});
}

void Element::RecalcStyle(const StyleRecalcChange change) {
  DCHECK(InActiveDocument());
  DCHECK(GetDocument().InStyleRecalc());
  DCHECK(!GetDocument().Lifecycle().InDetach());

  DisplayLockStyleScope display_lock_style_scope(this);
  if (HasCustomStyleCallbacks())
    WillRecalcStyle(change);

  StyleRecalcChange child_change = change.ForChildren();
  if (change.ShouldRecalcStyleFor(*this)) {
    child_change = RecalcOwnStyle(change);
    if (GetStyleChangeType() == kSubtreeStyleChange)
      child_change = child_change.ForceRecalcDescendants();
    ClearNeedsStyleRecalc();
  }

  // We're done with self style, notify the display lock.
  bool did_unlock = display_lock_style_scope.DidUpdateSelfStyle();

  // If the update to self style caused the display-lock to unlock, then we
  // should adjust `child_change` with whatever deferred dirty bits the context
  // had. This ensures that during this call, we will recurse into children for
  // layout changes (if needed).
  if (did_unlock) {
    child_change = display_lock_style_scope.AdjustStyleRecalcChangeForChildren(
        child_change);
  }

  if (!display_lock_style_scope.ShouldUpdateChildStyle()) {
    if (child_change.RecalcChildren()) {
      // If we should've calculated the style for children but was blocked,
      // notify so that we'd come back.
      // Note that the if-clause wouldn't catch cases where
      // ChildNeedsStyleRecalc() is true but child_change.RecalcChildren() is
      // false. Since we are retaining the child dirty bit, that case is
      // automatically handled without needing to notify it here.
      display_lock_style_scope.NotifyUpdateWasBlocked(
          DisplayLockContext::kStyleUpdateDescendants);

    } else if (child_change.TraversePseudoElements(*this)) {
      display_lock_style_scope.NotifyUpdateWasBlocked(
          DisplayLockContext::kStyleUpdatePseudoElements);
    }
    if (HasCustomStyleCallbacks())
      DidRecalcStyle(child_change);
    return;
  }

  if (child_change.TraversePseudoElements(*this)) {
    UpdatePseudoElement(kPseudoIdBackdrop, child_change);
    UpdatePseudoElement(kPseudoIdMarker, child_change);
    UpdatePseudoElement(kPseudoIdBefore, child_change);
  }

  if (child_change.TraverseChildren(*this)) {
    SelectorFilterParentScope filter_scope(*this);
    if (IsActiveV0InsertionPoint(*this)) {
      To<V0InsertionPoint>(this)->RecalcStyleForInsertionPointChildren(
          child_change);
    } else if (ShadowRoot* root = GetShadowRoot()) {
      root->RecalcDescendantStyles(child_change);
      // Sad panda. This is only to clear ensured ComputedStyles for elements
      // outside the flat tree for getComputedStyle() in the cases where we
      // kSubtreeStyleChange. Style invalidation and kLocalStyleChange will
      // make sure we clear out-of-date ComputedStyles outside the flat tree
      // in Element::EnsureComputedStyle().
      if (child_change.RecalcDescendants())
        RecalcDescendantStyles(StyleRecalcChange::kClearEnsured);
    } else if (auto* slot = ToHTMLSlotElementIfSupportsAssignmentOrNull(this)) {
      slot->RecalcStyleForSlotChildren(child_change);
    } else {
      RecalcDescendantStyles(child_change);
    }
  }

  if (child_change.TraversePseudoElements(*this)) {
    UpdatePseudoElement(kPseudoIdAfter, child_change);

    // If we are re-attaching us or any of our descendants, we need to attach
    // the descendants before we know if this element generates a ::first-letter
    // and which element the ::first-letter inherits style from.
    if (!child_change.ReattachLayoutTree() && !ChildNeedsReattachLayoutTree())
      UpdateFirstLetterPseudoElement(StyleUpdatePhase::kRecalc);
  }

  ClearChildNeedsStyleRecalc();
  // We've updated all the children that needs an update (might be 0).
  display_lock_style_scope.DidUpdateChildStyle();

  if (HasCustomStyleCallbacks())
    DidRecalcStyle(child_change);
}

scoped_refptr<ComputedStyle> Element::PropagateInheritedProperties() {
  if (IsPseudoElement())
    return nullptr;
  if (NeedsStyleRecalc())
    return nullptr;
  if (HasAnimations())
    return nullptr;
  const ComputedStyle* parent_style = ParentComputedStyle();
  DCHECK(parent_style);
  const ComputedStyle* style = GetComputedStyle();
  if (!style || style->Animations() || style->Transitions() ||
      style->HasVariableReference() || style->HasVariableDeclaration())
    return nullptr;
  scoped_refptr<ComputedStyle> new_style = ComputedStyle::Clone(*style);
  new_style->PropagateIndependentInheritedProperties(*parent_style);
  INCREMENT_STYLE_STATS_COUNTER(GetDocument().GetStyleEngine(),
                                independent_inherited_styles_propagated, 1);
  return new_style;
}

static const StyleRecalcChange ApplyComputedStyleDiff(
    const StyleRecalcChange change,
    ComputedStyle::Difference diff) {
  if (change.RecalcDescendants() ||
      diff < ComputedStyle::Difference::kPseudoElementStyle)
    return change;
  if (diff == ComputedStyle::Difference::kDisplayAffectingDescendantStyles)
    return change.ForceRecalcDescendants();
  if (diff == ComputedStyle::Difference::kInherited)
    return change.EnsureAtLeast(StyleRecalcChange::kRecalcChildren);
  if (diff == ComputedStyle::Difference::kIndependentInherited)
    return change.EnsureAtLeast(StyleRecalcChange::kIndependentInherit);
  DCHECK(diff == ComputedStyle::Difference::kPseudoElementStyle);
  return change.EnsureAtLeast(StyleRecalcChange::kUpdatePseudoElements);
}

StyleRecalcChange Element::RecalcOwnStyle(const StyleRecalcChange change) {
  DCHECK(GetDocument().InStyleRecalc());
  if (!CanParticipateInFlatTree()) {
    // This is a V0InsertionPoint. This whole block can be removed when Shadow
    // DOM V0 is removed.
    DCHECK(IsV0InsertionPoint());
    if (NeedsStyleRecalc())
      SetComputedStyle(nullptr);
    if (GetForceReattachLayoutTree())
      return change.ForceReattachLayoutTree();
    // Keep recalculating computed style for fallback children as if they were
    // children of the insertion point parent.
    return change;
  }

  if (change.RecalcChildren() && HasRareData() && NeedsStyleRecalc()) {
    // This element needs recalc because its parent changed inherited
    // properties or there was some style change in the ancestry which needed a
    // full subtree recalc. In that case we cannot use the BaseComputedStyle
    // optimization.
    if (ElementAnimations* element_animations =
            GetElementRareData()->GetElementAnimations())
      element_animations->SetAnimationStyleChange(false);
  }

  scoped_refptr<ComputedStyle> new_style;
  scoped_refptr<const ComputedStyle> old_style = GetComputedStyle();

  StyleRecalcChange child_change = change.ForChildren();

  if (ParentComputedStyle()) {
    if (old_style && change.IndependentInherit()) {
      // When propagating inherited changes, we don't need to do a full style
      // recalc if the only changed properties are independent. In this case, we
      // can simply clone the old ComputedStyle and set these directly.
      new_style = PropagateInheritedProperties();
    }
    if (!new_style)
      new_style = StyleForLayoutObject();
    if (new_style && !ShouldStoreComputedStyle(*new_style))
      new_style = nullptr;
  }

  ComputedStyle::Difference diff =
      ComputedStyle::ComputeDifference(old_style.get(), new_style.get());

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

  if (!new_style && HasRareData()) {
    ElementRareData* rare_data = GetElementRareData();
    if (ElementAnimations* element_animations =
            rare_data->GetElementAnimations()) {
      element_animations->CssAnimations().Cancel();
    }
    rare_data->ClearPseudoElements();
  }

  SetComputedStyle(new_style);

  if (!child_change.ReattachLayoutTree() &&
      (GetForceReattachLayoutTree() ||
       ComputedStyle::NeedsReattachLayoutTree(*this, old_style.get(),
                                              new_style.get()))) {
    child_change = child_change.ForceReattachLayoutTree();
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
    probe::DidUpdateComputedStyle(this, old_style.get(), new_style.get());
    if (this == GetDocument().documentElement()) {
      if (GetDocument().GetStyleEngine().UpdateRemUnits(old_style.get(),
                                                        new_style.get())) {
        // Trigger a full document recalc on rem unit changes. We could keep
        // track of which elements depend on rem units like we do for viewport
        // styles, but we assume root font size changes are rare and just
        // recalculate everything.
        child_change = child_change.ForceRecalcDescendants();
      }
    }
    child_change = ApplyComputedStyleDiff(child_change, diff);
    UpdateCallbackSelectors(old_style.get(), new_style.get());
  }

  if (auto* context = GetDisplayLockContext()) {
    // If the context is unlocked, then we should ensure to adjust the child
    // change for children, since this could be the first time we unlocked the
    // context and as a result need to process more of the subtree than we would
    // normally. Note that if this is not the first time, then
    // AdjustStyleRecalcChangeForChildren() won't do any adjustments.
    if (!context->IsLocked())
      child_change = context->AdjustStyleRecalcChangeForChildren(child_change);
  }

  if (new_style) {
    if (old_style && !child_change.RecalcChildren() &&
        old_style->HasChildDependentFlags())
      new_style->CopyChildDependentFlagsFrom(*old_style);
    if (RuntimeEnabledFeatures::LayoutNGEnabled())
      UpdateForceLegacyLayout(*new_style, old_style.get());
  }

  if (child_change.ReattachLayoutTree()) {
    if (new_style || old_style)
      SetNeedsReattachLayoutTree();
    return child_change;
  }

  if (LayoutObject* layout_object = GetLayoutObject()) {
    DCHECK(new_style);
    scoped_refptr<const ComputedStyle> layout_style(std::move(new_style));
    if (auto* pseudo_element = DynamicTo<PseudoElement>(this)) {
      if (layout_style->Display() == EDisplay::kContents) {
        layout_style =
            pseudo_element->LayoutStyleForDisplayContents(*layout_style);
      }
    } else if (auto* html_element = DynamicTo<HTMLHtmlElement>(this)) {
      layout_style = html_element->LayoutStyleForElement(layout_style);
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
    layout_object->SetStyle(layout_style.get(), apply_changes);
  }
  return child_change;
}

void Element::RebuildLayoutTree(WhitespaceAttacher& whitespace_attacher) {
  DCHECK(InActiveDocument());
  DCHECK(parentNode());

  if (NeedsReattachLayoutTree()) {
    AttachContext reattach_context;
    reattach_context.parent =
        LayoutTreeBuilderTraversal::ParentLayoutObject(*this);
    if (reattach_context.parent && reattach_context.parent->ForceLegacyLayout())
      reattach_context.force_legacy_layout = true;
    ReattachLayoutTree(reattach_context);
    whitespace_attacher.DidReattachElement(this,
                                           reattach_context.previous_in_flow);
  } else if (!ChildStyleRecalcBlockedByDisplayLock()) {
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
    if (GetLayoutObject() ||
        (!HasDisplayContentsStyle() && CanParticipateInFlatTree())) {
      whitespace_attacher.DidVisitElement(this);
      if (GetDocument().GetStyleEngine().NeedsWhitespaceReattachment(this))
        local_attacher.SetReattachAllWhitespaceNodes();
      child_attacher = &local_attacher;
    } else {
      child_attacher = &whitespace_attacher;
    }
    RebuildPseudoElementLayoutTree(kPseudoIdAfter, *child_attacher);
    if (GetShadowRoot())
      RebuildShadowRootLayoutTree(*child_attacher);
    else
      RebuildChildrenLayoutTrees(*child_attacher);
    RebuildPseudoElementLayoutTree(kPseudoIdBefore, *child_attacher);
    RebuildMarkerLayoutTree(*child_attacher);
    RebuildPseudoElementLayoutTree(kPseudoIdBackdrop, *child_attacher);
    RebuildFirstLetterLayoutTree();
    ClearChildNeedsReattachLayoutTree();
  }
  DCHECK(!NeedsStyleRecalc());
  DCHECK(!ChildNeedsStyleRecalc() || ChildStyleRecalcBlockedByDisplayLock());
  DCHECK(!NeedsReattachLayoutTree());
  DCHECK(!ChildNeedsReattachLayoutTree() ||
         ChildStyleRecalcBlockedByDisplayLock());
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
    if (element->NeedsRebuildLayoutTree(whitespace_attacher))
      element->RebuildLayoutTree(whitespace_attacher);
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
  UpdateFirstLetterPseudoElement(StyleUpdatePhase::kRebuildLayoutTree);
  if (PseudoElement* element = GetPseudoElement(kPseudoIdFirstLetter)) {
    WhitespaceAttacher whitespace_attacher;
    if (element->NeedsRebuildLayoutTree(whitespace_attacher))
      element->RebuildLayoutTree(whitespace_attacher);
  }
}

void Element::RebuildMarkerLayoutTree(WhitespaceAttacher& whitespace_attacher) {
  if (PseudoElement* marker = GetPseudoElement(kPseudoIdMarker)) {
    // In legacy layout, we need to reattach a marker in this case:
    //
    // <ol><li id="outer"><div id="inner">0</div></li></ol>
    // <script>outer.offsetTop; inner.style.display = "inline";</script>
    //
    // An outside marker must be aligned with the 1st line box in the
    // list item, so legacy layout will insert it inside #inner.
    // But when #inner becomes inline, the LayoutBlockFlow is destroyed,
    // so we need to reinsert it.
    //
    // TODO: SetNeedsReattachLayoutTree() should not be called at this point.
    // The layout tree rebuilding for markers should be done similarly to how
    // it is done for ::first-letter.
    if (LayoutObject* layout_object = GetLayoutObject()) {
      if (layout_object->IsListItem() && !marker->GetLayoutObject())
        marker->SetNeedsReattachLayoutTree();
    }

    if (marker->NeedsRebuildLayoutTree(whitespace_attacher))
      marker->RebuildLayoutTree(whitespace_attacher);
  }
}

void Element::UpdateCallbackSelectors(const ComputedStyle* old_style,
                                      const ComputedStyle* new_style) {
  Vector<String> empty_vector;
  const Vector<String>& old_callback_selectors =
      old_style ? old_style->CallbackSelectors() : empty_vector;
  const Vector<String>& new_callback_selectors =
      new_style ? new_style->CallbackSelectors() : empty_vector;
  if (old_callback_selectors.IsEmpty() && new_callback_selectors.IsEmpty())
    return;
  if (old_callback_selectors != new_callback_selectors)
    CSSSelectorWatch::From(GetDocument())
        .UpdateSelectorMatches(old_callback_selectors, new_callback_selectors);
}

ShadowRoot& Element::CreateAndAttachShadowRoot(ShadowRootType type) {
#if DCHECK_IS_ON()
  NestingLevelIncrementer slot_assignment_recalc_forbidden_scope(
      GetDocument().SlotAssignmentRecalcForbiddenRecursionDepth());
#endif
  HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
  EventDispatchForbiddenScope assert_no_event_dispatch;
  ScriptForbiddenScope forbid_script;

  DCHECK(!GetShadowRoot());

  auto* shadow_root = MakeGarbageCollected<ShadowRoot>(GetDocument(), type);

  if (InActiveDocument()) {
    // We need to call child.RemovedFromFlatTree() before setting a shadow
    // root to the element because detach must use the original flat tree
    // structure before attachShadow happens. We cannot use
    // FlatTreeParentChanged() because we don't know at this point whether a
    // slot will be added and the child assigned to a slot on the next slot
    // assignment update.
    for (Node& child : NodeTraversal::ChildrenOf(*this))
      child.RemovedFromFlatTree();
  }
  EnsureElementRareData().SetShadowRoot(*shadow_root);
  shadow_root->SetParentOrShadowHostNode(this);
  shadow_root->SetParentTreeScope(GetTreeScope());
  if (type == ShadowRootType::V0)
    shadow_root->SetNeedsDistributionRecalc();
  shadow_root->InsertedInto(*this);

  probe::DidPushShadowRoot(this, shadow_root);

  return *shadow_root;
}

ShadowRoot* Element::GetShadowRoot() const {
  return HasRareData() ? GetElementRareData()->GetShadowRoot() : nullptr;
}

void Element::PseudoStateChanged(CSSSelector::PseudoType pseudo) {
  // We can't schedule invaliation sets from inside style recalc otherwise
  // we'd never process them.
  // TODO(esprehn): Make this an ASSERT and fix places that call into this
  // like HTMLSelectElement.
  if (GetDocument().InStyleRecalc())
    return;
  GetDocument().GetStyleEngine().PseudoStateChangedForElement(pseudo, *this);
}

void Element::SetAnimationStyleChange(bool animation_style_change) {
  if (animation_style_change && GetDocument().InStyleRecalc())
    return;
  if (!HasRareData())
    return;
  if (ElementAnimations* element_animations =
          GetElementRareData()->GetElementAnimations())
    element_animations->SetAnimationStyleChange(animation_style_change);
}

void Element::SetNeedsAnimationStyleRecalc() {
  if (GetDocument().InStyleRecalc())
    return;
  if (GetStyleChangeType() != kNoStyleChange)
    return;

  SetNeedsStyleRecalc(kLocalStyleChange, StyleChangeReasonForTracing::Create(
                                             style_change_reason::kAnimation));
  if (NeedsStyleRecalc())
    SetAnimationStyleChange(true);
}

void Element::SetNeedsCompositingUpdate() {
  if (!GetDocument().IsActive())
    return;
  LayoutBoxModelObject* layout_object = GetLayoutBoxModelObject();
  if (!layout_object)
    return;
  if (!layout_object->HasLayer())
    return;
  layout_object->Layer()->SetNeedsCompositingInputsUpdate();

  // Changes to RequiresAcceleratedCompositing change if the PaintLayer is
  // self-painting (see: LayoutEmbeddedContent::LayerTypeRequired).
  if (layout_object->IsLayoutEmbeddedContent())
    layout_object->Layer()->UpdateSelfPaintingLayer();

  // Changes to AdditionalCompositingReasons can change direct compositing
  // reasons which affect paint properties.
  if (layout_object->CanHaveAdditionalCompositingReasons())
    layout_object->SetNeedsPaintPropertyUpdate();
}

void Element::V0SetCustomElementDefinition(
    V0CustomElementDefinition* definition) {
  if (!HasRareData() && !definition)
    return;
  DCHECK(!GetV0CustomElementDefinition());
  EnsureElementRareData().V0SetCustomElementDefinition(definition);
}

V0CustomElementDefinition* Element::GetV0CustomElementDefinition() const {
  if (HasRareData())
    return GetElementRareData()->GetV0CustomElementDefinition();
  return nullptr;
}

void Element::SetCustomElementDefinition(CustomElementDefinition* definition) {
  DCHECK(definition);
  DCHECK(!GetCustomElementDefinition());
  EnsureElementRareData().SetCustomElementDefinition(definition);
  SetCustomElementState(CustomElementState::kCustom);
}

CustomElementDefinition* Element::GetCustomElementDefinition() const {
  if (HasRareData())
    return GetElementRareData()->GetCustomElementDefinition();
  return nullptr;
}

void Element::SetIsValue(const AtomicString& is_value) {
  DCHECK(IsValue().IsNull()) << "SetIsValue() should be called at most once.";
  EnsureElementRareData().SetIsValue(is_value);
}

const AtomicString& Element::IsValue() const {
  if (HasRareData())
    return GetElementRareData()->IsValue();
  return g_null_atom;
}

void Element::SetDidAttachInternals() {
  EnsureElementRareData().SetDidAttachInternals();
}

bool Element::DidAttachInternals() const {
  return HasRareData() && GetElementRareData()->DidAttachInternals();
}

ElementInternals& Element::EnsureElementInternals() {
  return EnsureElementRareData().EnsureElementInternals(To<HTMLElement>(*this));
}

ShadowRoot* Element::createShadowRoot(ExceptionState& exception_state) {
  DCHECK(RuntimeEnabledFeatures::ShadowDOMV0Enabled(GetExecutionContext()));
  if (ShadowRoot* root = GetShadowRoot()) {
    if (root->IsUserAgent()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "Shadow root cannot be created on a host which already hosts a "
          "user-agent shadow tree.");
    } else {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "Shadow root cannot be created on a host which already hosts a "
          "shadow tree.");
    }
    return nullptr;
  }
  if (AlwaysCreateUserAgentShadowRoot()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Shadow root cannot be created on a host which already hosts a "
        "user-agent shadow tree.");
    return nullptr;
  }
  // Some elements make assumptions about what kind of layoutObjects they allow
  // as children so we can't allow author shadows on them for now.
  if (!AreAuthorShadowsAllowed()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "Author-created shadow roots are disabled for this element.");
    return nullptr;
  }

  return &CreateShadowRootInternal();
}

ShadowRoot& Element::CreateShadowRootInternal() {
  DCHECK(RuntimeEnabledFeatures::ShadowDOMV0Enabled(GetExecutionContext()));
  DCHECK(!ClosedShadowRoot());
  DCHECK(AreAuthorShadowsAllowed());
  DCHECK(!AlwaysCreateUserAgentShadowRoot());
  GetDocument().SetShadowCascadeOrder(ShadowCascadeOrder::kShadowCascadeV0);
  return CreateAndAttachShadowRoot(ShadowRootType::V0);
}

bool Element::CanAttachShadowRoot() const {
  const AtomicString& tag_name = localName();
  // Checking Is{V0}CustomElement() here is just an optimization
  // because IsValidName is not cheap.
  return (IsCustomElement() && CustomElement::IsValidName(tag_name)) ||
         (IsV0CustomElement() && V0CustomElement::IsValidName(tag_name)) ||
         tag_name == html_names::kArticleTag ||
         tag_name == html_names::kAsideTag ||
         tag_name == html_names::kBlockquoteTag ||
         tag_name == html_names::kBodyTag || tag_name == html_names::kDivTag ||
         tag_name == html_names::kFooterTag || tag_name == html_names::kH1Tag ||
         tag_name == html_names::kH2Tag || tag_name == html_names::kH3Tag ||
         tag_name == html_names::kH4Tag || tag_name == html_names::kH5Tag ||
         tag_name == html_names::kH6Tag || tag_name == html_names::kHeaderTag ||
         tag_name == html_names::kNavTag || tag_name == html_names::kMainTag ||
         tag_name == html_names::kPTag || tag_name == html_names::kSectionTag ||
         tag_name == html_names::kSpanTag;
}

const char* Element::ErrorMessageForAttachShadow() const {
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
    auto* registry = CustomElement::Registry(*this);
    auto* definition =
        registry ? registry->DefinitionForName(IsValue().IsNull() ? localName()
                                                                  : IsValue())
                 : nullptr;
    if (definition && definition->DisableShadow()) {
      return "attachShadow() is disabled by disabledFeatures static field.";
    }
  }

  // 4. If shadow host has a non-null shadow root whose "is declarative shadow
  // root" property is false, then throw an "NotSupportedError" DOMException.
  if (GetShadowRoot() && !GetShadowRoot()->IsDeclarativeShadowRoot()) {
    return "Shadow root cannot be created on a host "
           "which already hosts a shadow tree.";
  }
  return nullptr;
}

ShadowRoot* Element::attachShadow(const ShadowRootInit* shadow_root_init_dict,
                                  ExceptionState& exception_state) {
  DCHECK(shadow_root_init_dict->hasMode());
  ShadowRootType type = shadow_root_init_dict->mode() == "open"
                            ? ShadowRootType::kOpen
                            : ShadowRootType::kClosed;
  if (type == ShadowRootType::kOpen)
    UseCounter::Count(GetDocument(), WebFeature::kElementAttachShadowOpen);
  else
    UseCounter::Count(GetDocument(), WebFeature::kElementAttachShadowClosed);

  auto focus_delegation = (shadow_root_init_dict->hasDelegatesFocus() &&
                           shadow_root_init_dict->delegatesFocus())
                              ? FocusDelegation::kDelegateFocus
                              : FocusDelegation::kNone;
  auto slot_assignment = (shadow_root_init_dict->hasSlotAssignment() &&
                          shadow_root_init_dict->slotAssignment() == "manual")
                             ? SlotAssignmentMode::kManual
                             : SlotAssignmentMode::kAuto;

  if (const char* error_message = ErrorMessageForAttachShadow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      error_message);
    return nullptr;
  }
  return &AttachShadowRootInternal(type, focus_delegation, slot_assignment);
}

void Element::AttachDeclarativeShadowRoot(HTMLTemplateElement* template_element,
                                          ShadowRootType type,
                                          FocusDelegation focus_delegation,
                                          SlotAssignmentMode slot_assignment) {
  DCHECK(template_element);
  DCHECK(type == ShadowRootType::kOpen || type == ShadowRootType::kClosed);
  UseCounter::Count(GetDocument(), WebFeature::kDeclarativeShadowRoot);

  // 12. Run attach a shadow root with shadow host equal to declarative shadow
  // host element, mode equal to declarative shadow mode, and delegates focus
  // equal to declarative shadow delegates focus. If an exception was thrown by
  // attach a shadow root, catch it, and ignore the exception.
  if (const char* error_message = ErrorMessageForAttachShadow()) {
    template_element->SetDeclarativeShadowRootType(
        DeclarativeShadowRootType::kNone);
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kError, error_message));
    return;
  }
  ShadowRoot& shadow_root =
      AttachShadowRootInternal(type, focus_delegation, slot_assignment);
  // 13.1. Set declarative shadow host element's shadow host's "is declarative
  // shadow root" property to true.
  shadow_root.SetIsDeclarativeShadowRoot(true);
  // 13.2. Append the declarative template element's DocumentFragment to the
  // newly-created shadow root.
  shadow_root.appendChild(template_element->DeclarativeShadowContent());
  // 13.3. Remove the declarative template element from the document.
  template_element->remove();
}

ShadowRoot& Element::CreateUserAgentShadowRoot() {
  DCHECK(!GetShadowRoot());
  return CreateAndAttachShadowRoot(ShadowRootType::kUserAgent);
}

ShadowRoot& Element::AttachShadowRootInternal(
    ShadowRootType type,
    FocusDelegation focus_delegation,
    SlotAssignmentMode slot_assignment_mode) {
  // SVG <use> is a special case for using this API to create a closed shadow
  // root.
  DCHECK(CanAttachShadowRoot() || IsA<SVGUseElement>(*this));
  DCHECK(type == ShadowRootType::kOpen || type == ShadowRootType::kClosed)
      << type;
  DCHECK(!AlwaysCreateUserAgentShadowRoot());

  GetDocument().SetShadowCascadeOrder(ShadowCascadeOrder::kShadowCascadeV1);

  if (auto* shadow_root = GetShadowRoot()) {
    // 5. If shadow host has a non-null shadow root whose "is declarative shadow
    // root property is true, then remove all of shadow root’s children, in tree
    // order. Return shadow host’s shadow root.
    DCHECK(shadow_root->IsDeclarativeShadowRoot());
    shadow_root->RemoveChildren();
    return *shadow_root;
  }

  // 6. Let shadow be a new shadow root whose node document is shadow host’s
  // node document, host is shadow host, and mode is mode.
  // 9. Set shadow host’s shadow root to shadow.
  ShadowRoot& shadow_root = CreateAndAttachShadowRoot(type);
  // 7. Set shadow’s delegates focus to delegates focus.
  shadow_root.SetDelegatesFocus(focus_delegation ==
                                FocusDelegation::kDelegateFocus);
  // 8. Set shadow’s "is declarative shadow root" property to false.
  shadow_root.SetIsDeclarativeShadowRoot(false);
  shadow_root.SetSlotAssignmentMode(slot_assignment_mode);
  return shadow_root;
}

ShadowRoot* Element::OpenShadowRoot() const {
  ShadowRoot* root = GetShadowRoot();
  if (!root)
    return nullptr;
  return root->GetType() == ShadowRootType::V0 ||
                 root->GetType() == ShadowRootType::kOpen
             ? root
             : nullptr;
}

ShadowRoot* Element::ClosedShadowRoot() const {
  ShadowRoot* root = GetShadowRoot();
  if (!root)
    return nullptr;
  return root->GetType() == ShadowRootType::kClosed ? root : nullptr;
}

ShadowRoot* Element::AuthorShadowRoot() const {
  ShadowRoot* root = GetShadowRoot();
  if (!root)
    return nullptr;
  return !root->IsUserAgent() ? root : nullptr;
}

ShadowRoot* Element::UserAgentShadowRoot() const {
  ShadowRoot* root = GetShadowRoot();
  DCHECK(!root || root->IsUserAgent());
  return root;
}

ShadowRoot& Element::EnsureUserAgentShadowRoot() {
  if (ShadowRoot* shadow_root = UserAgentShadowRoot()) {
    DCHECK(shadow_root->GetType() == ShadowRootType::kUserAgent);
    return *shadow_root;
  }
  ShadowRoot& shadow_root =
      CreateAndAttachShadowRoot(ShadowRootType::kUserAgent);
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
    if (sibling->IsElementNode())
      return true;
    auto* text_node = DynamicTo<Text>(sibling);
    if (text_node && !text_node->data().IsEmpty())
      return true;
  }
  return false;
}

}  // namespace

void Element::CheckForEmptyStyleChange(const Node* node_before_change,
                                       const Node* node_after_change) {
  if (!InActiveDocument())
    return;
  if (!StyleAffectedByEmpty())
    return;
  if (HasSiblingsForNonEmpty(node_before_change,
                             NodeTraversal::PreviousSibling) ||
      HasSiblingsForNonEmpty(node_after_change, NodeTraversal::NextSibling)) {
    return;
  }
  PseudoStateChanged(CSSSelector::kPseudoEmpty);
}

void Element::ChildrenChanged(const ChildrenChange& change) {
  ContainerNode::ChildrenChanged(change);

  CheckForEmptyStyleChange(change.sibling_before_change,
                           change.sibling_after_change);

  if (!change.ByParser() && change.IsChildElementChange())
    CheckForSiblingStyleChanges(
        change.type == ChildrenChangeType::kElementRemoved
            ? kSiblingElementRemoved
            : kSiblingElementInserted,
        To<Element>(change.sibling_changed), change.sibling_before_change,
        change.sibling_after_change);

  if (ShadowRoot* shadow_root = GetShadowRoot())
    shadow_root->SetNeedsDistributionRecalcWillBeSetNeedsAssignmentRecalc();
}

void Element::FinishParsingChildren() {
  SetIsFinishedParsingChildren(true);
  CheckForEmptyStyleChange(this, this);
  CheckForSiblingStyleChanges(kFinishedParsingChildren, nullptr, lastChild(),
                              nullptr);
}

AttrNodeList* Element::GetAttrNodeList() {
  return HasRareData() ? GetElementRareData()->GetAttrNodeList() : nullptr;
}

void Element::RemoveAttrNodeList() {
  DCHECK(GetAttrNodeList());
  if (HasRareData())
    GetElementRareData()->RemoveAttrNodeList();
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

void Element::ParseAttribute(const AttributeModificationParams& params) {
  if (params.name == html_names::kTabindexAttr) {
    int tabindex = 0;
    if (params.new_value.IsEmpty() ||
        !ParseHTMLInteger(params.new_value, tabindex)) {
      ClearTabIndexExplicitlyIfNeeded();
    } else {
      // We only set when value is in integer range.
      SetTabIndexExplicitly();
    }
  } else if (params.name == xml_names::kLangAttr) {
    PseudoStateChanged(CSSSelector::kPseudoLang);
  }
}

bool Element::ParseAttributeName(QualifiedName& out,
                                 const AtomicString& namespace_uri,
                                 const AtomicString& qualified_name,
                                 ExceptionState& exception_state) {
  AtomicString prefix, local_name;
  if (!Document::ParseQualifiedName(qualified_name, prefix, local_name,
                                    exception_state))
    return false;
  DCHECK(!exception_state.HadException());

  QualifiedName q_name(prefix, local_name, namespace_uri);

  if (!Document::HasValidNamespaceForAttributes(q_name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNamespaceError,
        "'" + namespace_uri + "' is an invalid namespace for attributes.");
    return false;
  }

  out = q_name;
  return true;
}

void Element::setAttributeNS(
    const AtomicString& namespace_uri,
    const AtomicString& qualified_name,
    const StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL&
        string_or_trusted,
    ExceptionState& exception_state) {
  QualifiedName parsed_name = g_any_name;
  if (!ParseAttributeName(parsed_name, namespace_uri, qualified_name,
                          exception_state))
    return;

  AtomicString value(TrustedTypesCheckFor(
      ExpectedTrustedTypeForAttribute(parsed_name), string_or_trusted,
      GetExecutionContext(), exception_state));
  if (exception_state.HadException())
    return;

  setAttribute(parsed_name, value);
}

void Element::RemoveAttributeInternal(
    wtf_size_t index,
    SynchronizationOfLazyAttribute in_synchronization_of_lazy_attribute) {
  MutableAttributeCollection attributes =
      EnsureUniqueElementData().Attributes();
  SECURITY_DCHECK(index < attributes.size());

  QualifiedName name = attributes[index].GetName();
  AtomicString value_being_removed = attributes[index].Value();

  if (!in_synchronization_of_lazy_attribute) {
    if (!value_being_removed.IsNull()) {
      WillModifyAttribute(name, value_being_removed, g_null_atom);
    } else if (GetCustomElementState() == CustomElementState::kCustom) {
      // This would otherwise be enqueued by willModifyAttribute.
      CustomElement::EnqueueAttributeChangedCallback(
          *this, name, value_being_removed, g_null_atom);
    }
  }

  if (Attr* attr_node = AttrIfExists(name))
    DetachAttrNodeFromElementWithValue(attr_node, attributes[index].Value());

  attributes.Remove(index);

  if (!in_synchronization_of_lazy_attribute)
    DidRemoveAttribute(name, value_being_removed);
}

void Element::AppendAttributeInternal(
    const QualifiedName& name,
    const AtomicString& value,
    SynchronizationOfLazyAttribute in_synchronization_of_lazy_attribute) {
  if (!in_synchronization_of_lazy_attribute)
    WillModifyAttribute(name, g_null_atom, value);
  EnsureUniqueElementData().Attributes().Append(name, value);
  if (!in_synchronization_of_lazy_attribute)
    DidAddAttribute(name, value);
}

void Element::removeAttributeNS(const AtomicString& namespace_uri,
                                const AtomicString& local_name) {
  removeAttribute(QualifiedName(g_null_atom, local_name, namespace_uri));
}

Attr* Element::getAttributeNode(const AtomicString& local_name) {
  if (!GetElementData())
    return nullptr;
  WTF::AtomicStringTable::WeakResult hint =
      WeakLowercaseIfNecessary(local_name);
  SynchronizeAttributeHinted(local_name, hint);
  const Attribute* attribute =
      GetElementData()->Attributes().FindHinted(local_name, hint);
  if (!attribute)
    return nullptr;
  return EnsureAttr(attribute->GetName());
}

Attr* Element::getAttributeNodeNS(const AtomicString& namespace_uri,
                                  const AtomicString& local_name) {
  if (!GetElementData())
    return nullptr;
  QualifiedName q_name(g_null_atom, local_name, namespace_uri);
  SynchronizeAttribute(q_name);
  const Attribute* attribute = GetElementData()->Attributes().Find(q_name);
  if (!attribute)
    return nullptr;
  return EnsureAttr(attribute->GetName());
}

bool Element::hasAttribute(const AtomicString& local_name) const {
  if (!GetElementData())
    return false;
  WTF::AtomicStringTable::WeakResult hint =
      WeakLowercaseIfNecessary(local_name);
  SynchronizeAttributeHinted(local_name, hint);
  return GetElementData()->Attributes().FindIndexHinted(local_name, hint) !=
         kNotFound;
}

bool Element::hasAttributeNS(const AtomicString& namespace_uri,
                             const AtomicString& local_name) const {
  if (!GetElementData())
    return false;
  QualifiedName q_name(g_null_atom, local_name, namespace_uri);
  SynchronizeAttribute(q_name);
  return GetElementData()->Attributes().Find(q_name);
}

bool Element::DelegatesFocus() const {
  return AuthorShadowRoot() && AuthorShadowRoot()->delegatesFocus();
}

// https://html.spec.whatwg.org/C/#get-the-focusable-area
Element* Element::GetFocusableArea() const {
  DCHECK(!IsFocusable());
  // TODO(crbug.com/1018619): Support AREA -> IMG delegation.
  if (!DelegatesFocus())
    return nullptr;
  Document& doc = GetDocument();
  UseCounter::Count(doc, WebFeature::kDelegateFocus);

  // TODO(https://github.com/w3c/webcomponents/issues/840): We'd like to
  // standardize this behavior.
  Element* focused_element = doc.FocusedElement();
  if (focused_element && IsShadowIncludingInclusiveAncestorOf(*focused_element))
    return focused_element;

  // Slide the focus to its inner node.
  return FocusController::FindFocusableElementInShadowHost(*this);
}

void Element::focus() {
  focus(FocusParams());
}

void Element::focus(const FocusOptions* options) {
  focus(FocusParams(SelectionBehaviorOnFocus::kRestore,
                    mojom::blink::FocusType::kNone, nullptr, options));
}

void Element::focus(const FocusParams& params) {
  if (!isConnected())
    return;

  if (!GetDocument().IsFocusAllowed())
    return;

  if (GetDocument().FocusedElement() == this)
    return;

  if (!GetDocument().IsActive())
    return;

  auto* frame_owner_element = DynamicTo<HTMLFrameOwnerElement>(this);
  if (frame_owner_element && frame_owner_element->contentDocument() &&
      frame_owner_element->contentDocument()->UnloadStarted())
    return;

  // Ensure we have clean style (including forced display locks).
  GetDocument().UpdateStyleAndLayoutTreeForNode(this);

  // https://html.spec.whatwg.org/C/#focusing-steps
  //
  // 1. If new focus target is not a focusable area, ...
  if (!IsFocusable()) {
    if (Element* new_focus_target = GetFocusableArea()) {
      // Unlike the specification, we re-run focus() for new_focus_target
      // because we can't change |this| in a member function.
      new_focus_target->focus(FocusParams(SelectionBehaviorOnFocus::kReset,
                                          mojom::blink::FocusType::kForward,
                                          nullptr, params.options));
    }
    // 2. If new focus target is null, then:
    //  2.1. If no fallback target was specified, then return.
    return;
  }
  // If script called focus(), then the type would be none. This means we are
  // activating because of a script action (kScriptFocus). Otherwise, this is a
  // user activation (kUserFocus).
  ActivateDisplayLockIfNeeded(params.type == mojom::blink::FocusType::kNone
                                  ? DisplayLockActivationReason::kScriptFocus
                                  : DisplayLockActivationReason::kUserFocus);

  if (!GetDocument().GetPage()->GetFocusController().SetFocusedElement(
          this, GetDocument().GetFrame(), params))
    return;

  if (GetDocument().FocusedElement() == this &&
      GetDocument().GetFrame()->HasStickyUserActivation()) {
    // Bring up the keyboard in the context of anything triggered by a user
    // gesture. Since tracking that across arbitrary boundaries (eg.
    // animations) is difficult, for now we match IE's heuristic and bring
    // up the keyboard if there's been any gesture since load.
    GetDocument()
        .GetPage()
        ->GetChromeClient()
        .ShowVirtualKeyboardOnElementFocus(*GetDocument().GetFrame());
  }
}

void Element::UpdateFocusAppearance(
    SelectionBehaviorOnFocus selection_behavior) {
  UpdateFocusAppearanceWithOptions(selection_behavior, FocusOptions::Create());
}

void Element::UpdateFocusAppearanceWithOptions(
    SelectionBehaviorOnFocus selection_behavior,
    const FocusOptions* options) {
  if (selection_behavior == SelectionBehaviorOnFocus::kNone)
    return;
  if (IsRootEditableElement(*this)) {
    LocalFrame* frame = GetDocument().GetFrame();
    if (!frame)
      return;

    // When focusing an editable element in an iframe, don't reset the selection
    // if it already contains a selection.
    if (this == frame->Selection()
                    .ComputeVisibleSelectionInDOMTreeDeprecated()
                    .RootEditableElement())
      return;

    // FIXME: We should restore the previous selection if there is one.
    // Passing DoNotSetFocus as this function is called after
    // FocusController::setFocusedElement() and we don't want to change the
    // focus to a new Element.
    frame->Selection().SetSelection(
        SelectionInDOMTree::Builder()
            .Collapse(FirstPositionInOrBeforeNode(*this))
            .Build(),
        SetSelectionOptions::Builder()
            .SetShouldCloseTyping(true)
            .SetShouldClearTypingStyle(true)
            .SetDoNotSetFocus(true)
            .Build());
    if (!options->preventScroll())
      frame->Selection().RevealSelection();
  } else if (GetLayoutObject() &&
             !GetLayoutObject()->IsLayoutEmbeddedContent()) {
    if (!options->preventScroll()) {
      auto params = ScrollAlignment::CreateScrollIntoViewParams();

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

      GetLayoutObject()->ScrollRectToVisible(BoundingBoxForScrollIntoView(),
                                             std::move(params));
    }
  }
}

void Element::blur() {
  CancelFocusAppearanceUpdate();
  if (AdjustedFocusedElementInTreeScope() == this) {
    Document& doc = GetDocument();
    if (doc.GetPage()) {
      doc.GetPage()->GetFocusController().SetFocusedElement(nullptr,
                                                            doc.GetFrame());
    } else {
      doc.ClearFocusedElement();
    }
  }
}

bool Element::SupportsFocus() const {
  // FIXME: supportsFocus() can be called when layout is not up to date.
  // Logic that deals with the layoutObject should be moved to
  // layoutObjectIsFocusable().
  // But supportsFocus must return true when the element is editable, or else
  // it won't be focusable. Furthermore, supportsFocus cannot just return true
  // always or else tabIndex() will change for all HTML elements.
  if (DelegatesFocus())
    return false;
  return HasElementFlag(ElementFlags::kTabIndexWasSetExplicitly) ||
         IsRootEditableElementWithCounting(*this) ||
         SupportsSpatialNavigationFocus();
}

bool Element::SupportsSpatialNavigationFocus() const {
  // This function checks whether the element satisfies the extended criteria
  // for the element to be focusable, introduced by spatial navigation feature,
  // i.e. checks if click or keyboard event handler is specified.
  // This is the way to make it possible to navigate to (focus) elements
  // which web designer meant for being active (made them respond to click
  // events).
  if (!IsSpatialNavigationEnabled(GetDocument().GetFrame()))
    return false;

  if (!GetLayoutObject())
    return false;

  if (HasJSBasedEventListeners(event_type_names::kClick) ||
      HasJSBasedEventListeners(event_type_names::kKeydown) ||
      HasJSBasedEventListeners(event_type_names::kKeypress) ||
      HasJSBasedEventListeners(event_type_names::kKeyup) ||
      HasJSBasedEventListeners(event_type_names::kMouseover) ||
      HasJSBasedEventListeners(event_type_names::kMouseenter))
    return true;

  // Some web apps use click-handlers to react on clicks within rects that are
  // styled with {cursor: pointer}. Such rects *look* clickable so they probably
  // are. Here we make Hand-trees' tip, the first (biggest) node with {cursor:
  // pointer}, navigable because users shouldn't need to navigate through every
  // sub element that inherit this CSS.
  if (GetComputedStyle()->Cursor() == ECursor::kPointer &&
      ParentComputedStyle()->Cursor() != ECursor::kPointer) {
    return true;
  }

  if (!IsSVGElement())
    return false;
  return (HasEventListeners(event_type_names::kFocus) ||
          HasEventListeners(event_type_names::kBlur) ||
          HasEventListeners(event_type_names::kFocusin) ||
          HasEventListeners(event_type_names::kFocusout));
}

bool Element::IsFocusable() const {
  return Element::IsMouseFocusable() || Element::IsKeyboardFocusable();
}

bool Element::IsFocusableStyleAfterUpdate() const {
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
  // Note that this isn't a part of `IsFocusableStyle()` because there are
  // callers of that function which cannot do a layout tree update (e.g.
  // accessibility).
  GetDocument().UpdateStyleAndLayoutTreeForNode(this);
  return IsFocusableStyle();
}

bool Element::IsKeyboardFocusable() const {
  return isConnected() && !IsInert() && IsFocusableStyleAfterUpdate() &&
         ((SupportsFocus() &&
           GetIntegralAttribute(html_names::kTabindexAttr, 0) >= 0) ||
          (RuntimeEnabledFeatures::KeyboardFocusableScrollersEnabled() &&
           IsScrollableNode(this))) &&
         !DisplayLockPreventsActivation(
             DisplayLockActivationReason::kUserFocus);
}

bool Element::IsMouseFocusable() const {
  return isConnected() && !IsInert() && IsFocusableStyleAfterUpdate() &&
         SupportsFocus() &&
         !DisplayLockPreventsActivation(
             DisplayLockActivationReason::kUserFocus);
}

bool Element::IsAutofocusable() const {
  // https://html.spec.whatwg.org/C/#global-attributes
  // https://svgwg.org/svg2-draft/struct.html#autofocusattribute
  return (IsHTMLElement() || IsSVGElement()) &&
         FastHasAttribute(html_names::kAutofocusAttr);
}

bool Element::ActivateDisplayLockIfNeeded(DisplayLockActivationReason reason) {
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled() ||
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount() ==
          GetDocument()
              .GetDisplayLockDocumentState()
              .DisplayLockBlockingAllActivationCount())
    return false;
  const_cast<Element*>(this)->UpdateDistributionForFlatTreeTraversal();

  HeapVector<std::pair<Member<Element>, Member<Element>>> activatable_targets;
  for (Node& ancestor : FlatTreeTraversal::InclusiveAncestorsOf(*this)) {
    auto* ancestor_element = DynamicTo<Element>(ancestor);
    if (!ancestor_element)
      continue;
    if (auto* context = ancestor_element->GetDisplayLockContext()) {
      // If any of the ancestors is not activatable for the given reason, we
      // can't activate.
      if (context->IsLocked() && !context->IsActivatable(reason))
        return false;
      activatable_targets.push_back(std::make_pair(
          ancestor_element, &ancestor.GetTreeScope().Retarget(*this)));
    }
  }

  bool activated = false;
  for (const auto& target : activatable_targets) {
    // Dispatch event on activatable ancestor (target.first), with
    // the retargeted element (target.second) as the |activatedElement|.
    if (auto* context = target.first->GetDisplayLockContext()) {
      if (context->ShouldCommitForActivation(reason)) {
        activated = true;
        context->CommitForActivationWithSignal(target.second, reason);
      }
    }
  }
  return activated;
}

bool Element::DisplayLockPreventsActivation(
    DisplayLockActivationReason reason) const {
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled())
    return false;

  if (GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount() == 0)
    return false;

  const_cast<Element*>(this)->UpdateDistributionForFlatTreeTraversal();
  // TODO(vmpstr): Similar to Document::EnsurePaintLocationDataValidForNode(),
  // this iterates up to the ancestor hierarchy looking for locked display
  // locks. This is inefficient, particularly since it's unlikely that this will
  // yield any "true" results in practice. We need to come up with a way to
  // check whether a node is in a locked subtree quickly.
  // See crbug.com/924550 for more details.
  for (const Node& current : FlatTreeTraversal::InclusiveAncestorsOf(*this)) {
    auto* current_element = DynamicTo<Element>(current);
    if (!current_element)
      continue;
    if (auto* context = current_element->GetDisplayLockContext()) {
      if (context->IsLocked() && !context->IsActivatable(reason))
        return true;
    }
  }
  return false;
}

bool Element::StyleShouldForceLegacyLayoutInternal() const {
  return GetElementRareData()->StyleShouldForceLegacyLayout();
}

void Element::SetStyleShouldForceLegacyLayoutInternal(bool force) {
  EnsureElementRareData().SetStyleShouldForceLegacyLayout(force);
}

bool Element::ShouldForceLegacyLayoutForChildInternal() const {
  return GetElementRareData()->ShouldForceLegacyLayoutForChild();
}

void Element::SetShouldForceLegacyLayoutForChildInternal(bool force) {
  EnsureElementRareData().SetShouldForceLegacyLayoutForChild(force);
}

void Element::UpdateForceLegacyLayout(const ComputedStyle& new_style,
                                      const ComputedStyle* old_style) {
  // ::first-letter may cause structure discrepancies between DOM and layout
  //  tree (in layout the layout object will be wrapped around the actual text
  //  layout object, which may be deep down in the tree somewhere, while in DOM,
  //  the pseudo element will be a direct child of the node that matched the
  //  ::first-letter selector). Because of that, it's going to be tricky to
  //  determine whether we need to force legacy layout or not. Luckily, the
  //  ::first-letter pseudo element cannot introduce the need for legacy layout
  //  on its own, so just bail. We'll do whatever the parent layout object does.
  if (IsFirstLetterPseudoElement())
    return;
  bool old_force = old_style && ShouldForceLegacyLayout();
  SetStyleShouldForceLegacyLayout(
      CalculateStyleShouldForceLegacyLayout(*this, new_style));
  if (ShouldForceLegacyLayout()) {
    if (!old_force) {
      if (const LayoutObject* layout_object = GetLayoutObject()) {
        // Forced legacy layout is inherited down the layout tree, so even if we
        // just decided here on the DOM side that we need forced legacy layout,
        // check with the LayoutObject whether this is news and that it really
        // needs to be reattached.
        if (!layout_object->ForceLegacyLayout())
          SetNeedsReattachLayoutTree();
      }
    }
    // Even if we also previously forced legacy layout, we may need to introduce
    // forced legacy layout in the ancestry, e.g. if this element no longer
    // establishes a new formatting context.
    ForceLegacyLayoutInFormattingContext(new_style);

    // If we're inside an NG fragmentation context, we also need the entire
    // fragmentation context to fall back to legacy layout. Note that once this
    // has happened, the fragmentation context will be locked to legacy layout,
    // even if all the reasons for requiring it in the first place disappear
    // (e.g. if the only reason was a table, and that table is removed, we'll
    // still be using legacy layout).
    if (new_style.InsideNGFragmentationContext())
      ForceLegacyLayoutInFragmentationContext(new_style);
  } else if (old_force) {
    // TODO(mstensho): If we have ancestors that got legacy layout just because
    // of this child, we should clean it up, and switch the subtree back to NG,
    // rather than being stuck with legacy forever.
    SetNeedsReattachLayoutTree();
  }
}

void Element::ForceLegacyLayoutInFormattingContext(
    const ComputedStyle& new_style) {
  // TableNG requires that table elements are either all NG, or all Legacy.
  bool needs_traverse_to_table =
      RuntimeEnabledFeatures::LayoutNGTableEnabled() &&
      new_style.IsDisplayTableType();
  bool found_bfc = DefinitelyNewFormattingContext(*this, new_style);
  for (Element* ancestor = this; !found_bfc || needs_traverse_to_table;) {
    ancestor =
        DynamicTo<Element>(LayoutTreeBuilderTraversal::Parent(*ancestor));
    if (!ancestor || ancestor->ShouldForceLegacyLayout())
      break;
    const ComputedStyle* style = ancestor->GetComputedStyle();
    if (style->Display() == EDisplay::kNone)
      break;
    found_bfc = found_bfc || DefinitelyNewFormattingContext(*ancestor, *style);
    if (found_bfc && !needs_traverse_to_table) {
      needs_traverse_to_table =
          RuntimeEnabledFeatures::LayoutNGTableEnabled() &&
          style->IsDisplayTableType();
    }
    if (needs_traverse_to_table) {
      if (style->IsDisplayTableBox())
        needs_traverse_to_table = false;
    }
    ancestor->SetShouldForceLegacyLayoutForChild(true);
    ancestor->SetNeedsReattachLayoutTree();
  }
}

void Element::ForceLegacyLayoutInFragmentationContext(
    const ComputedStyle& new_style) {
  // This element cannot be laid out natively by LayoutNG. We now need to switch
  // all enclosing block fragmentation contexts over to using legacy
  // layout. Find the element that establishes the fragmentation context, and
  // switch it over to legacy layout. Note that we walk the parent chain here,
  // and not the containing block chain. This means that we may get false
  // positives; e.g. if there's an absolutely positioned table, whose containing
  // block of the table is on the outside of the fragmentation context, we're
  // still going to fall back to legacy.
  Element* parent;
  for (Element* walker = this; walker; walker = parent) {
    parent = DynamicTo<Element>(LayoutTreeBuilderTraversal::Parent(*walker));
    if (!walker->GetComputedStyle()->SpecifiesColumns())
      continue;

    // Found an element that establishes a fragmentation context. Force it to do
    // legacy layout. Keep looking for outer fragmentation contexts, since we
    // need to force them over to legacy as well.
    walker->SetShouldForceLegacyLayoutForChild(true);
    walker->SetNeedsReattachLayoutTree();
    if (parent && !parent->GetComputedStyle()->InsideNGFragmentationContext())
      return;
  }
  DCHECK(GetDocument().Printing());
  // Force legacy layout on the entire document, since we're printing, and
  // there's some fragmentable box that needs legacy layout inside somewhere.
  Element* root = GetDocument().documentElement();
  root->SetShouldForceLegacyLayoutForChild(true);
  root->SetNeedsReattachLayoutTree();
}

bool Element::IsFocusedElementInDocument() const {
  return this == GetDocument().FocusedElement();
}

Element* Element::AdjustedFocusedElementInTreeScope() const {
  return IsInTreeScope() ? ContainingTreeScope().AdjustedFocusedElement()
                         : nullptr;
}

void Element::DispatchFocusEvent(Element* old_focused_element,
                                 mojom::blink::FocusType type,
                                 InputDeviceCapabilities* source_capabilities) {
  DispatchEvent(*FocusEvent::Create(
      event_type_names::kFocus, Event::Bubbles::kNo, GetDocument().domWindow(),
      0, old_focused_element, source_capabilities));
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

String Element::innerHTML() const {
  return CreateMarkup(this, kChildrenOnly);
}

String Element::outerHTML() const {
  return CreateMarkup(this);
}

void Element::setInnerHTML(const String& html,
                           ExceptionState& exception_state) {
  probe::BreakableLocation(GetExecutionContext(), "Element.setInnerHTML");
  if (html.IsEmpty() && !HasNonInBodyInsertionMode()) {
    setTextContent(html);
  } else {
    if (DocumentFragment* fragment = CreateFragmentForInnerOuterHTML(
            html, this, kAllowScriptingContent, "innerHTML", exception_state)) {
      ContainerNode* container = this;
      if (auto* template_element = DynamicTo<HTMLTemplateElement>(*this)) {
        // Allow replacing innerHTML on declarative shadow templates, prior to
        // their closing tag being parsed.
        container = template_element->IsDeclarativeShadowRoot()
                        ? template_element->DeclarativeShadowContent()
                        : template_element->content();
      }
      ReplaceChildrenWithFragment(container, fragment, exception_state);
    }
  }
}

String Element::getInnerHTML(const GetInnerHTMLOptions* options) const {
  DCHECK(RuntimeEnabledFeatures::DeclarativeShadowDOMEnabled(
      GetExecutionContext()));
  ClosedRootsSet include_closed_roots;
  if (options->hasClosedRoots()) {
    for (auto& shadow_root : options->closedRoots()) {
      include_closed_roots.insert(shadow_root);
    }
  }
  return CreateMarkup(
      this, kChildrenOnly, kDoNotResolveURLs,
      options->includeShadowRoots() ? kIncludeShadowRoots : kNoShadowRoots,
      include_closed_roots);
}

void Element::setOuterHTML(const String& html,
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
        "This element's parent is of type '" + p->nodeName() +
            "', which is not an element node.");
    return;
  }

  Node* prev = previousSibling();
  Node* next = nextSibling();

  DocumentFragment* fragment = CreateFragmentForInnerOuterHTML(
      html, parent, kAllowScriptingContent, "outerHTML", exception_state);
  if (exception_state.HadException())
    return;

  parent->ReplaceChild(fragment, this, exception_state);
  if (exception_state.HadException())
    return;

  Node* node = next ? next->previousSibling() : nullptr;
  if (auto* text = DynamicTo<Text>(node)) {
    MergeWithNextTextNode(text, exception_state);
    if (exception_state.HadException())
      return;
  }

  if (auto* prev_text = DynamicTo<Text>(prev)) {
    MergeWithNextTextNode(prev_text, exception_state);
    if (exception_state.HadException())
      return;
  }
}

// Step 4 of http://domparsing.spec.whatwg.org/#insertadjacenthtml()
Node* Element::InsertAdjacent(const String& where,
                              Node* new_child,
                              ExceptionState& exception_state) {
  if (EqualIgnoringASCIICase(where, "beforeBegin")) {
    if (ContainerNode* parent = parentNode()) {
      parent->InsertBefore(new_child, this, exception_state);
      if (!exception_state.HadException())
        return new_child;
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
      if (!exception_state.HadException())
        return new_child;
    }
    return nullptr;
  }

  exception_state.ThrowDOMException(
      DOMExceptionCode::kSyntaxError,
      "The value provided ('" + where +
          "') is not one of 'beforeBegin', 'afterBegin', "
          "'beforeEnd', or 'afterEnd'.");
  return nullptr;
}

void Element::HideNonce() {
  const AtomicString& nonce_value = FastGetAttribute(html_names::kNonceAttr);
  if (nonce_value.IsEmpty())
    return;
  if (!InActiveDocument())
    return;
  if (GetExecutionContext()
          ->GetContentSecurityPolicy()
          ->HasHeaderDeliveredPolicy()) {
    setAttribute(html_names::kNonceAttr, g_empty_atom);
  }
}

ElementIntersectionObserverData* Element::IntersectionObserverData() const {
  if (HasRareData())
    return GetElementRareData()->IntersectionObserverData();
  return nullptr;
}

ElementIntersectionObserverData& Element::EnsureIntersectionObserverData() {
  return EnsureElementRareData().EnsureIntersectionObserverData();
}

HeapHashMap<Member<ResizeObserver>, Member<ResizeObservation>>*
Element::ResizeObserverData() const {
  if (HasRareData())
    return GetElementRareData()->ResizeObserverData();
  return nullptr;
}

HeapHashMap<Member<ResizeObserver>, Member<ResizeObservation>>&
Element::EnsureResizeObserverData() {
  return EnsureElementRareData().EnsureResizeObserverData();
}

DisplayLockContext* Element::GetDisplayLockContext() const {
  if (!RuntimeEnabledFeatures::CSSContentVisibilityEnabled())
    return nullptr;
  return HasRareData() ? GetElementRareData()->GetDisplayLockContext()
                       : nullptr;
}

DisplayLockContext& Element::EnsureDisplayLockContext() {
  return *EnsureElementRareData().EnsureDisplayLockContext(this);
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
      EqualIgnoringASCIICase(where, "beforeEnd"))
    return element;
  exception_state.ThrowDOMException(
      DOMExceptionCode::kSyntaxError,
      "The value provided ('" + where +
          "') is not one of 'beforeBegin', 'afterBegin', "
          "'beforeEnd', or 'afterEnd'.");
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

void Element::insertAdjacentHTML(const String& where,
                                 const String& markup,
                                 ExceptionState& exception_state) {
  Node* context_node = ContextNodeForInsertion(where, this, exception_state);
  if (!context_node)
    return;

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
      markup, context_element, kAllowScriptingContent, "insertAdjacentHTML",
      exception_state);
  if (!fragment)
    return;
  InsertAdjacent(where, fragment, exception_state);
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
    if (!child_text_node)
      continue;
    if (!first_text_node)
      first_text_node = child_text_node;
    else
      found_multiple_text_nodes = true;
    unsigned length = child_text_node->data().length();
    if (length > std::numeric_limits<unsigned>::max() - total_length)
      return g_empty_string;
    total_length += length;
  }

  if (!first_text_node)
    return g_empty_string;

  if (first_text_node && !found_multiple_text_nodes) {
    first_text_node->MakeParkable();
    return first_text_node->data();
  }

  StringBuilder content;
  content.ReserveCapacity(total_length);
  for (Node* child = first_text_node; child; child = child->nextSibling()) {
    auto* child_text_node = DynamicTo<Text>(child);
    if (!child_text_node)
      continue;
    content.Append(child_text_node->data());
  }

  DCHECK_EQ(content.length(), total_length);
  return content.ToString();
}

const AtomicString& Element::ShadowPseudoId() const {
  if (ShadowRoot* root = ContainingShadowRoot()) {
    if (root->IsUserAgent())
      return FastGetAttribute(html_names::kPseudoAttr);
  }
  return g_null_atom;
}

void Element::SetShadowPseudoId(const AtomicString& id) {
  DCHECK(CSSSelector::ParsePseudoType(id, false) ==
             CSSSelector::kPseudoWebKitCustomElement ||
         CSSSelector::ParsePseudoType(id, false) ==
             CSSSelector::kPseudoBlinkInternalElement);
  setAttribute(html_names::kPseudoAttr, id);
}

bool Element::IsInDescendantTreeOf(const Element* shadow_host) const {
  DCHECK(shadow_host);
  DCHECK(IsShadowHost(shadow_host));

  for (const Element* ancestor_shadow_host = OwnerShadowHost();
       ancestor_shadow_host;
       ancestor_shadow_host = ancestor_shadow_host->OwnerShadowHost()) {
    if (ancestor_shadow_host == shadow_host)
      return true;
  }
  return false;
}

const ComputedStyle* Element::EnsureComputedStyle(
    PseudoId pseudo_element_specifier) {
  if (PseudoElement* element = GetPseudoElement(pseudo_element_specifier))
    return element->EnsureComputedStyle();

  if (!InActiveDocument())
    return nullptr;

  // EnsureComputedStyle is expected to be called to forcibly compute style for
  // elements in display:none subtrees on otherwise style-clean documents. If
  // you hit this DCHECK, consider if you really need ComputedStyle for
  // display:none elements. If not, use GetComputedStyle() instead.
  // Regardlessly, you need to UpdateStyleAndLayoutTree() before calling
  // EnsureComputedStyle. In some cases you might be fine using GetComputedStyle
  // without updating the style, but in most cases you want a clean tree for
  // that as well.
  //
  // Adjacent styling bits may be set and affect NeedsLayoutTreeUpdateForNode as
  // part of EnsureComputedStyle in an ancestor chain.
  // (see CSSComputedStyleDeclarationTest::NeedsAdjacentStyleRecalc). It is OK
  // that it happens, but we need to ignore the effect on
  // NeedsLayoutTreeUpdateForNodeIncludingDisplayLocked here.
  DCHECK(!GetDocument().NeedsLayoutTreeUpdateForNodeIncludingDisplayLocked(
      *this, true /* ignore_adjacent_style */));

  // FIXME: Find and use the layoutObject from the pseudo element instead of the
  // actual element so that the 'length' properties, which are only known by the
  // layoutObject because it did the layout, will be correct and so that the
  // values returned for the ":selection" pseudo-element will be correct.
  const ComputedStyle* element_style = GetComputedStyle();
  if (!element_style || element_style->IsEnsuredOutsideFlatTree()) {
    if (CanParticipateInFlatTree()) {
      if (ContainerNode* parent = LayoutTreeBuilderTraversal::Parent(*this)) {
        parent->EnsureComputedStyle();
        if (element_style)
          element_style = GetComputedStyle();
      }
      if (element_style && NeedsStyleRecalc()) {
        // RecalcStyle() will not traverse into connected elements outside the
        // flat tree and we may have a dirty element or ancestors if this
        // element is not in the flat tree. If we don't need a style recalc,
        // we can just re-use the ComputedStyle from the last
        // getComputedStyle(). Otherwise, we need to clear the ensured styles
        // for the uppermost dirty ancestor and all of its descendants. If
        // this element was not the uppermost dirty element, we would not end
        // up here because a dirty ancestor would have cleared the
        // ComputedStyle in the recursive call above and element_style would
        // have been null.
        GetDocument().GetStyleEngine().ClearEnsuredDescendantStyles(*this);
        element_style = nullptr;
      }
    } else {
      element_style = nullptr;
    }
    if (!element_style) {
      scoped_refptr<ComputedStyle> new_style = nullptr;
      // TODO(crbug.com/953707): Avoid setting inline style during
      // HTMLImageElement::CustomStyleForLayoutObject.
      if (HasCustomStyleCallbacks() && !IsA<HTMLImageElement>(*this))
        new_style = CustomStyleForLayoutObject();
      else
        new_style = OriginalStyleForLayoutObject();
      element_style = new_style.get();
      new_style->SetIsEnsuredInDisplayNone();
      SetComputedStyle(std::move(new_style));
    }
  }

  if (!pseudo_element_specifier)
    return element_style;

  if (const ComputedStyle* pseudo_element_style =
          element_style->GetCachedPseudoElementStyle(pseudo_element_specifier))
    return pseudo_element_style;

  const ComputedStyle* layout_parent_style = element_style;
  if (HasDisplayContentsStyle()) {
    LayoutObject* parent_layout_object =
        LayoutTreeBuilderTraversal::ParentLayoutObject(*this);
    if (parent_layout_object)
      layout_parent_style = parent_layout_object->Style();
  }

  scoped_refptr<ComputedStyle> result =
      GetDocument().GetStyleResolver().PseudoStyleForElement(
          this,
          PseudoElementStyleRequest(
              pseudo_element_specifier,
              PseudoElementStyleRequest::kForComputedStyle),
          element_style, layout_parent_style);
  DCHECK(result);
  result->SetIsEnsuredInDisplayNone();
  return element_style->AddCachedPseudoElementStyle(std::move(result));
}

bool Element::HasDisplayContentsStyle() const {
  if (const ComputedStyle* style = GetComputedStyle())
    return style->Display() == EDisplay::kContents;
  return false;
}

bool Element::ShouldStoreComputedStyle(const ComputedStyle& style) const {
  if (LayoutObjectIsNeeded(style))
    return true;
  if (auto* svg_element = DynamicTo<SVGElement>(this)) {
    if (!svg_element->HasSVGParent())
      return false;
    if (IsA<SVGStopElement>(*this))
      return true;
  }
  return style.Display() == EDisplay::kContents;
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
        } else {
          attribute = attributes.Find(html_names::kLangAttr);
          if (attribute)
            value = attribute->Value();
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

void Element::CancelFocusAppearanceUpdate() {
  if (GetDocument().FocusedElement() == this)
    GetDocument().CancelFocusAppearanceUpdate();
}

void Element::UpdateFirstLetterPseudoElement(StyleUpdatePhase phase) {
  // Update the ::first-letter pseudo elements presence and its style. This
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

  PseudoElement* element = GetPseudoElement(kPseudoIdFirstLetter);
  if (!element) {
    element = CreatePseudoElementIfNeeded(kPseudoIdFirstLetter);
    // If we are in Element::AttachLayoutTree, don't mess up the ancestor flags
    // for layout tree attachment/rebuilding. We will unconditionally call
    // AttachLayoutTree for the created pseudo element immediately after this
    // call.
    if (element && phase != StyleUpdatePhase::kAttachLayoutTree)
      element->SetNeedsReattachLayoutTree();
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
    scoped_refptr<ComputedStyle> pseudo_style = element->StyleForLayoutObject();
    if (PseudoElementLayoutObjectIsNeeded(pseudo_style.get(), this))
      element->SetComputedStyle(std::move(pseudo_style));
    else
      GetElementRareData()->SetPseudoElement(kPseudoIdFirstLetter, nullptr);
    element->ClearNeedsStyleRecalc();
    return;
  }

  StyleRecalcChange change(StyleRecalcChange::kRecalcDescendants);
  // Remaining text part should be next to first-letter pseudo element.
  // See http://crbug.com/984389 for details.
  if (text_node_changed || remaining_text_layout_object->PreviousSibling() !=
                               element->GetLayoutObject())
    change = change.ForceReattachLayoutTree();
  element->RecalcStyle(change);

  if (element->NeedsReattachLayoutTree() &&
      !PseudoElementLayoutObjectIsNeeded(element->GetComputedStyle(), this)) {
    GetElementRareData()->SetPseudoElement(kPseudoIdFirstLetter, nullptr);
    GetDocument().GetStyleEngine().PseudoElementRemoved(*this);
  }
}

void Element::UpdatePseudoElement(PseudoId pseudo_id,
                                  const StyleRecalcChange change) {
  PseudoElement* element = GetPseudoElement(pseudo_id);
  if (!element) {
    if ((element = CreatePseudoElementIfNeeded(pseudo_id))) {
      // ::before and ::after can have a nested ::marker
      element->CreatePseudoElementIfNeeded(kPseudoIdMarker);
      element->SetNeedsReattachLayoutTree();
    }
    return;
  }

  if (change.ShouldUpdatePseudoElement(*element)) {
    if (CanGeneratePseudoElement(pseudo_id)) {
      element->RecalcStyle(change.ForPseudoElement());
      if (!element->NeedsReattachLayoutTree())
        return;
      if (PseudoElementLayoutObjectIsNeeded(element->GetComputedStyle(), this))
        return;
    }
    GetElementRareData()->SetPseudoElement(pseudo_id, nullptr);
    GetDocument().GetStyleEngine().PseudoElementRemoved(*this);
  }
}

PseudoElement* Element::CreatePseudoElementIfNeeded(PseudoId pseudo_id) {
  if (!CanGeneratePseudoElement(pseudo_id))
    return nullptr;
  if (pseudo_id == kPseudoIdFirstLetter) {
    if (!FirstLetterPseudoElement::FirstLetterTextLayoutObject(*this))
      return nullptr;
  }

  PseudoElement* pseudo_element = PseudoElement::Create(this, pseudo_id);
  EnsureElementRareData().SetPseudoElement(pseudo_id, pseudo_element);
  pseudo_element->InsertedInto(*this);

  scoped_refptr<ComputedStyle> pseudo_style =
      pseudo_element->StyleForLayoutObject();
  if (!PseudoElementLayoutObjectIsNeeded(pseudo_style.get(), this)) {
    GetElementRareData()->SetPseudoElement(pseudo_id, nullptr);
    return nullptr;
  }

  if (pseudo_id == kPseudoIdBackdrop)
    GetDocument().AddToTopLayer(pseudo_element, this);

  pseudo_element->SetComputedStyle(pseudo_style);

  // Most pseudo elements get their style calculated upon insertion, which means
  // that we don't get to RecalcOwnStyle() (regular DOM nodes do get there,
  // since their style isn't calculated directly upon insertion). Need to check
  // now if the element requires legacy layout.
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    pseudo_element->UpdateForceLegacyLayout(*pseudo_style, nullptr);

  probe::PseudoElementCreated(pseudo_element);

  return pseudo_element;
}

void Element::AttachPseudoElement(PseudoId pseudo_id, AttachContext& context) {
  if (PseudoElement* pseudo_element = GetPseudoElement(pseudo_id))
    pseudo_element->AttachLayoutTree(context);
}

void Element::DetachPseudoElement(PseudoId pseudo_id,
                                  bool performing_reattach) {
  if (PseudoElement* pseudo_element = GetPseudoElement(pseudo_id))
    pseudo_element->DetachLayoutTree(performing_reattach);
}

PseudoElement* Element::GetPseudoElement(PseudoId pseudo_id) const {
  return HasRareData() ? GetElementRareData()->GetPseudoElement(pseudo_id)
                       : nullptr;
}

LayoutObject* Element::PseudoElementLayoutObject(PseudoId pseudo_id) const {
  if (PseudoElement* element = GetPseudoElement(pseudo_id))
    return element->GetLayoutObject();
  return nullptr;
}

bool Element::PseudoElementStylesDependOnFontMetrics() const {
  const ComputedStyle* style = GetComputedStyle();
  if (!style)
    return false;
  if (style->CachedPseudoElementStylesDependOnFontMetrics())
    return true;

  // Note that |HasAnyPseudoElementStyles()| counts public pseudo elements only.
  // ::-webkit-scrollbar-*  are internal, and hence are not counted. So we must
  // perform this check after checking the cached pseudo element styles to avoid
  // false negatives.
  if (!style->HasAnyPseudoElementStyles())
    return false;

  // If we don't generate a PseudoElement, its style must have been cached on
  // the originating element's ComputedStyle. Hence, it remains to check styles
  // on the generated PseudoElements.
  if (!HasRareData())
    return false;
  for (PseudoElement* pseudo_element :
       GetElementRareData()->GetPseudoElements()) {
    if (pseudo_element->GetComputedStyle()->DependsOnFontMetrics())
      return true;
  }

  return false;
}

const ComputedStyle* Element::CachedStyleForPseudoElement(
    const PseudoElementStyleRequest& request) {
  const ComputedStyle* style = GetComputedStyle();

  if (!style || (request.pseudo_id < kFirstInternalPseudoId &&
                 !style->HasPseudoElementStyle(request.pseudo_id))) {
    return nullptr;
  }

  if (const ComputedStyle* cached =
          style->GetCachedPseudoElementStyle(request.pseudo_id))
    return cached;

  scoped_refptr<ComputedStyle> result = StyleForPseudoElement(request, style);
  if (result)
    return style->AddCachedPseudoElementStyle(std::move(result));
  return nullptr;
}

scoped_refptr<ComputedStyle> Element::StyleForPseudoElement(
    const PseudoElementStyleRequest& request,
    const ComputedStyle* parent_style) {
  const ComputedStyle* style = GetComputedStyle();
  const bool is_before_or_after = request.pseudo_id == kPseudoIdBefore ||
                                  request.pseudo_id == kPseudoIdAfter;

  DCHECK(style);
  DCHECK(!parent_style || !is_before_or_after);

  if (is_before_or_after) {
    const ComputedStyle* layout_parent_style = style;
    if (style->Display() == EDisplay::kContents) {
      // TODO(futhark@chromium.org): Calling getComputedStyle for elements
      // outside the flat tree should return empty styles, but currently we do
      // not. See issue https://crbug.com/831568. We can replace the if-test
      // with DCHECK(layout_parent) when that issue is fixed.
      if (Node* layout_parent =
              LayoutTreeBuilderTraversal::LayoutParent(*this)) {
        layout_parent_style = layout_parent->GetComputedStyle();
      }
    }
    return GetDocument().GetStyleResolver().PseudoStyleForElement(
        this, request, style, layout_parent_style);
  }

  if (!parent_style)
    parent_style = style;

  if (request.pseudo_id == kPseudoIdFirstLineInherited) {
    scoped_refptr<ComputedStyle> result =
        GetDocument().GetStyleResolver().StyleForElement(this, parent_style,
                                                         parent_style);
    result->SetStyleType(kPseudoIdFirstLineInherited);
    return result;
  }

  return GetDocument().GetStyleResolver().PseudoStyleForElement(
      this, request, parent_style, parent_style);
}

bool Element::CanGeneratePseudoElement(PseudoId pseudo_id) const {
  if (pseudo_id == kPseudoIdBackdrop && !IsInTopLayer())
    return false;
  if (pseudo_id == kPseudoIdFirstLetter && IsSVGElement())
    return false;
  if (const ComputedStyle* style = GetComputedStyle())
    return style->CanGeneratePseudoElement(pseudo_id);
  return false;
}

bool Element::MayTriggerVirtualKeyboard() const {
  return HasEditableStyle(*this);
}

bool Element::matches(const AtomicString& selectors,
                      ExceptionState& exception_state) {
  SelectorQuery* selector_query = GetDocument().GetSelectorQueryCache().Add(
      selectors, GetDocument(), exception_state);
  if (!selector_query)
    return false;
  return selector_query->Matches(*this);
}

bool Element::matches(const AtomicString& selectors) {
  return matches(selectors, ASSERT_NO_EXCEPTION);
}

Element* Element::closest(const AtomicString& selectors,
                          ExceptionState& exception_state) {
  SelectorQuery* selector_query = GetDocument().GetSelectorQueryCache().Add(
      selectors, GetDocument(), exception_state);
  if (!selector_query)
    return nullptr;
  return selector_query->Closest(*this);
}

Element* Element::closest(const AtomicString& selectors) {
  return closest(selectors, ASSERT_NO_EXCEPTION);
}

DOMTokenList& Element::classList() {
  ElementRareData& rare_data = EnsureElementRareData();
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
  ElementRareData& rare_data = EnsureElementRareData();
  if (!rare_data.Dataset())
    rare_data.SetDataset(MakeGarbageCollected<DatasetDOMStringMap>(this));
  return *rare_data.Dataset();
}

KURL Element::HrefURL() const {
  // FIXME: These all have href() or url(), but no common super class. Why
  // doesn't <link> implement URLUtils?
  if (IsA<HTMLAnchorElement>(*this) || IsA<HTMLAreaElement>(*this) ||
      IsA<HTMLLinkElement>(*this))
    return GetURLAttribute(html_names::kHrefAttr);
  if (auto* svg_a = DynamicTo<SVGAElement>(*this))
    return svg_a->LegacyHrefURL(GetDocument());
  return KURL();
}

KURL Element::GetURLAttribute(const QualifiedName& name) const {
#if DCHECK_IS_ON()
  if (GetElementData()) {
    if (const Attribute* attribute = Attributes().Find(name))
      DCHECK(IsURLAttribute(*attribute));
  }
#endif
  return GetDocument().CompleteURL(
      StripLeadingAndTrailingHTMLSpaces(getAttribute(name)));
}

KURL Element::GetNonEmptyURLAttribute(const QualifiedName& name) const {
#if DCHECK_IS_ON()
  if (GetElementData()) {
    if (const Attribute* attribute = Attributes().Find(name))
      DCHECK(IsURLAttribute(*attribute));
  }
#endif
  String value = StripLeadingAndTrailingHTMLSpaces(getAttribute(name));
  if (value.IsEmpty())
    return KURL();
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
  if (value > 0x7fffffffu)
    value = default_value;
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
    GetDocument().GetStyleEngine().EnsureUAStyleForFullscreen();
  }
  PseudoStateChanged(CSSSelector::kPseudoFullScreenAncestor);
}

// Unlike Node::parentOrShadowHostElement, this can cross frame boundaries.
static Element* NextAncestorElement(Element* element) {
  DCHECK(element);
  if (element->ParentOrShadowHostElement())
    return element->ParentOrShadowHostElement();

  Frame* frame = element->GetDocument().GetFrame();
  if (!frame || !frame->Owner())
    return nullptr;

  // Find the next LocalFrame on the ancestor chain, and return the
  // corresponding <iframe> element for the remote child if it exists.
  while (frame->Tree().Parent() && frame->Tree().Parent()->IsRemoteFrame())
    frame = frame->Tree().Parent();

  if (auto* frame_owner_element =
          DynamicTo<HTMLFrameOwnerElement>(frame->Owner()))
    return frame_owner_element;

  return nullptr;
}

void Element::SetContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(
    bool flag) {
  for (Element* element = NextAncestorElement(this); element;
       element = NextAncestorElement(element))
    element->SetContainsFullScreenElement(flag);
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
  if (IsInTopLayer() == in_top_layer)
    return;
  SetElementFlag(ElementFlags::kIsInTopLayer, in_top_layer);
  if (!isConnected())
    return;
  if (!GetDocument().InStyleRecalc())
    SetForceReattachLayoutTree();
}

ScriptValue Element::requestPointerLock(ScriptState* script_state,
                                        const PointerLockOptions* options,
                                        ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise;
  if (GetDocument().GetPage()) {
    promise =
        GetDocument().GetPage()->GetPointerLockController().RequestPointerLock(
            resolver, this, exception_state, options);
  } else {
    promise = resolver->Promise();
    exception_state.ThrowDOMException(
        DOMExceptionCode::kWrongDocumentError,
        "PointerLock cannot be request when there "
        "is no frame or that frame has no page.");
  }

  if (RuntimeEnabledFeatures::PointerLockOptionsEnabled(
          GetExecutionContext())) {
    if (exception_state.HadException())
      resolver->Reject(exception_state);
    return promise.GetScriptValue();
  }

  // The current spec for PointerLock does not have any language about throwing
  // exceptions. Therefore to be spec compliant we must clear all exceptions.
  // When behind our experimental flag however, we will throw exceptions which
  // should be caught as a promise rejection.
  exception_state.ClearException();

  // Detach the resolver, since we are not using it, to prepare it for garbage
  // collection.
  resolver->Detach();
  return ScriptValue();
}

SpellcheckAttributeState Element::GetSpellcheckAttributeState() const {
  const AtomicString& value = FastGetAttribute(html_names::kSpellcheckAttr);
  if (value == g_null_atom)
    return kSpellcheckAttributeDefault;
  if (EqualIgnoringASCIICase(value, "true") ||
      EqualIgnoringASCIICase(value, ""))
    return kSpellcheckAttributeTrue;
  if (EqualIgnoringASCIICase(value, "false"))
    return kSpellcheckAttributeFalse;

  return kSpellcheckAttributeDefault;
}

bool Element::IsSpellCheckingEnabled() const {
  for (const Element* element = this; element;
       element = element->ParentOrShadowHostElement()) {
    switch (element->GetSpellcheckAttributeState()) {
      case kSpellcheckAttributeTrue:
        return true;
      case kSpellcheckAttributeFalse:
        return false;
      case kSpellcheckAttributeDefault:
        break;
    }
  }

  if (!GetDocument().GetPage())
    return true;

  return GetDocument().GetPage()->GetSettings().GetSpellCheckEnabledByDefault();
}

#if DCHECK_IS_ON()
bool Element::FastAttributeLookupAllowed(const QualifiedName& name) const {
  if (name == html_names::kStyleAttr)
    return false;

  if (auto* svg_element = DynamicTo<SVGElement>(this))
    return !svg_element->IsAnimatableAttribute(name);

  return true;
}
#endif

#ifdef DUMP_NODE_STATISTICS
bool Element::HasNamedNodeMap() const {
  return HasRareData() && GetElementRareData()->AttributeMap();
}
#endif

inline void Element::UpdateName(const AtomicString& old_name,
                                const AtomicString& new_name) {
  if (!IsInDocumentTree())
    return;

  if (old_name == new_name)
    return;

  NamedItemType type = GetNamedItemType();
  if (type != NamedItemType::kNone)
    UpdateNamedItemRegistration(type, old_name, new_name);
}

inline void Element::UpdateId(const AtomicString& old_id,
                              const AtomicString& new_id) {
  if (!IsInTreeScope())
    return;

  if (old_id == new_id)
    return;

  UpdateId(ContainingTreeScope(), old_id, new_id);
}

inline void Element::UpdateId(TreeScope& scope,
                              const AtomicString& old_id,
                              const AtomicString& new_id) {
  DCHECK(IsInTreeScope());
  DCHECK_NE(old_id, new_id);

  if (!old_id.IsEmpty())
    scope.RemoveElementById(old_id, *this);
  if (!new_id.IsEmpty())
    scope.AddElementById(new_id, *this);

  NamedItemType type = GetNamedItemType();
  if (type == NamedItemType::kNameOrId ||
      type == NamedItemType::kNameOrIdWithName)
    UpdateIdNamedItemRegistration(type, old_id, new_id);
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
    if (IsUpgradedV0CustomElement()) {
      V0CustomElement::AttributeDidChange(this, name.LocalName(), old_value,
                                          new_value);
    }
  }

  if (MutationObserverInterestGroup* recipients =
          MutationObserverInterestGroup::CreateForAttributesMutation(*this,
                                                                     name))
    recipients->EnqueueMutationRecord(
        MutationRecord::CreateAttributes(this, name, old_value));

  probe::WillModifyDOMAttr(this, old_value, new_value);
}

DISABLE_CFI_PERF
void Element::DidAddAttribute(const QualifiedName& name,
                              const AtomicString& value) {
  if (name == html_names::kIdAttr)
    UpdateId(g_null_atom, value);
  AttributeChanged(AttributeModificationParams(
      name, g_null_atom, value, AttributeModificationReason::kDirectly));
  probe::DidModifyDOMAttr(this, name, value);
  DispatchSubtreeModifiedEvent();
}

void Element::DidModifyAttribute(const QualifiedName& name,
                                 const AtomicString& old_value,
                                 const AtomicString& new_value) {
  if (name == html_names::kIdAttr)
    UpdateId(old_value, new_value);
  AttributeChanged(AttributeModificationParams(
      name, old_value, new_value, AttributeModificationReason::kDirectly));
  probe::DidModifyDOMAttr(this, name, new_value);
  // Do not dispatch a DOMSubtreeModified event here; see bug 81141.
}

void Element::DidRemoveAttribute(const QualifiedName& name,
                                 const AtomicString& old_value) {
  if (name == html_names::kIdAttr)
    UpdateId(old_value, g_null_atom);
  AttributeChanged(AttributeModificationParams(
      name, old_value, g_null_atom, AttributeModificationReason::kDirectly));
  probe::DidRemoveDOMAttr(this, name);
  DispatchSubtreeModifiedEvent();
}

static bool NeedsURLResolutionForInlineStyle(const Element& element,
                                             const Document& old_document,
                                             const Document& new_document) {
  if (old_document == new_document)
    return false;
  if (old_document.BaseURL() == new_document.BaseURL())
    return false;
  const CSSPropertyValueSet* style = element.InlineStyle();
  if (!style)
    return false;
  for (unsigned i = 0; i < style->PropertyCount(); ++i) {
    if (style->PropertyAt(i).Value().MayContainUrl())
      return true;
  }
  return false;
}

static void ReResolveURLsInInlineStyle(const Document& document,
                                       MutableCSSPropertyValueSet& style) {
  for (unsigned i = 0; i < style.PropertyCount(); ++i) {
    const CSSValue& value = style.PropertyAt(i).Value();
    if (value.MayContainUrl())
      value.ReResolveUrl(document);
  }
}

void Element::DidMoveToNewDocument(Document& old_document) {
  Node::DidMoveToNewDocument(old_document);

  // If the documents differ by quirks mode then they differ by case sensitivity
  // for class and id names so we need to go through the attribute change logic
  // to pick up the new casing in the ElementData.
  if (old_document.InQuirksMode() != GetDocument().InQuirksMode()) {
    // TODO(tkent): If new owner Document has a ShareableElementData matching to
    // this element's attributes, we shouldn't make UniqueElementData, and this
    // element should point to the shareable one.
    EnsureUniqueElementData();

    if (HasID())
      SetIdAttribute(GetIdAttribute());
    if (HasClass())
      setAttribute(html_names::kClassAttr, GetClassAttribute());
  }
  // TODO(tkent): Even if Documents' modes are same, keeping
  // ShareableElementData owned by old_document isn't right.

  if (NeedsURLResolutionForInlineStyle(*this, old_document, GetDocument()))
    ReResolveURLsInInlineStyle(GetDocument(), EnsureMutableInlineStyle());

  if (auto* context = GetDisplayLockContext())
    context->DidMoveToNewDocument(old_document);
}

void Element::UpdateNamedItemRegistration(NamedItemType type,
                                          const AtomicString& old_name,
                                          const AtomicString& new_name) {
  auto* doc = DynamicTo<HTMLDocument>(GetDocument());
  if (!doc)
    return;

  if (!old_name.IsEmpty())
    doc->RemoveNamedItem(old_name);

  if (!new_name.IsEmpty())
    doc->AddNamedItem(new_name);

  if (type == NamedItemType::kNameOrIdWithName) {
    const AtomicString id = GetIdAttribute();
    if (!id.IsEmpty()) {
      if (!old_name.IsEmpty() && new_name.IsEmpty())
        doc->RemoveNamedItem(id);
      else if (old_name.IsEmpty() && !new_name.IsEmpty())
        doc->AddNamedItem(id);
    }
  }
}

void Element::UpdateIdNamedItemRegistration(NamedItemType type,
                                            const AtomicString& old_id,
                                            const AtomicString& new_id) {
  auto* doc = DynamicTo<HTMLDocument>(GetDocument());
  if (!doc)
    return;

  if (type == NamedItemType::kNameOrIdWithName && GetNameAttribute().IsEmpty())
    return;

  if (!old_id.IsEmpty())
    doc->RemoveNamedItem(old_id);

  if (!new_id.IsEmpty())
    doc->AddNamedItem(new_id);
}

ScrollOffset Element::SavedLayerScrollOffset() const {
  return HasRareData() ? GetElementRareData()->SavedLayerScrollOffset()
                       : ScrollOffset();
}

void Element::SetSavedLayerScrollOffset(const ScrollOffset& size) {
  if (size.IsZero() && !HasRareData())
    return;
  EnsureElementRareData().SetSavedLayerScrollOffset(size);
}

Attr* Element::AttrIfExists(const QualifiedName& name) {
  if (AttrNodeList* attr_node_list = GetAttrNodeList()) {
    for (const auto& attr : *attr_node_list) {
      if (attr->GetQualifiedName().Matches(name))
        return attr.Get();
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
  if (list->IsEmpty())
    RemoveAttrNodeList();
}

void Element::DetachAllAttrNodesFromElement() {
  AttrNodeList* list = GetAttrNodeList();
  if (!list)
    return;

  AttributeCollection attributes = GetElementData()->Attributes();
  for (const Attribute& attr : attributes) {
    if (Attr* attr_node = AttrIfExists(attr.GetName()))
      attr_node->DetachFromElementWithValue(attr.Value());
  }

  RemoveAttrNodeList();
}

Node::InsertionNotificationRequest Node::InsertedInto(
    ContainerNode& insertion_point) {
  DCHECK(!ChildNeedsStyleInvalidation());
  DCHECK(!NeedsStyleInvalidation());
  DCHECK(insertion_point.isConnected() || insertion_point.IsInShadowTree() ||
         IsContainerNode());
  if (insertion_point.isConnected()) {
    SetFlag(kIsConnectedFlag);
    insertion_point.GetDocument().IncrementNodeCount();
  }
  if (ParentOrShadowHostNode()->IsInShadowTree())
    SetFlag(kIsInShadowTreeFlag);
  if (ChildNeedsDistributionRecalc() &&
      !insertion_point.ChildNeedsDistributionRecalc())
    insertion_point.MarkAncestorsWithChildNeedsDistributionRecalc();
  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->ChildrenChanged(&insertion_point);
  return kInsertionDone;
}

void Node::RemovedFrom(ContainerNode& insertion_point) {
  DCHECK(insertion_point.isConnected() || IsContainerNode() ||
         IsInShadowTree());
  if (insertion_point.isConnected()) {
    ClearNeedsStyleRecalc();
    ClearChildNeedsStyleRecalc();
    ClearNeedsStyleInvalidation();
    ClearChildNeedsStyleInvalidation();
    ClearFlag(kIsConnectedFlag);
    insertion_point.GetDocument().DecrementNodeCount();
  }
  if (IsInShadowTree() && !ContainingTreeScope().RootNode().IsShadowRoot())
    ClearFlag(kIsInShadowTreeFlag);
  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache()) {
    cache->Remove(this);
    cache->ChildrenChanged(&insertion_point);
  }
}

void Element::WillRecalcStyle(const StyleRecalcChange) {
  DCHECK(HasCustomStyleCallbacks());
}

void Element::DidRecalcStyle(const StyleRecalcChange) {
  DCHECK(HasCustomStyleCallbacks());
}

scoped_refptr<ComputedStyle> Element::CustomStyleForLayoutObject() {
  DCHECK(HasCustomStyleCallbacks());
  return OriginalStyleForLayoutObject();
}

void Element::CloneAttributesFrom(const Element& other) {
  if (HasRareData())
    DetachAllAttrNodesFromElement();

  other.SynchronizeAllAttributes();
  if (!other.element_data_) {
    element_data_.Clear();
    return;
  }

  const AtomicString& old_id = GetIdAttribute();
  const AtomicString& new_id = other.GetIdAttribute();

  if (!old_id.IsNull() || !new_id.IsNull())
    UpdateId(old_id, new_id);

  const AtomicString& old_name = GetNameAttribute();
  const AtomicString& new_name = other.GetNameAttribute();

  if (!old_name.IsNull() || !new_name.IsNull())
    UpdateName(old_name, new_name);

  // Quirks mode makes class and id not case sensitive. We can't share the
  // ElementData if the idForStyleResolution and the className need different
  // casing.
  bool owner_documents_have_different_case_sensitivity = false;
  if (other.HasClass() || other.HasID())
    owner_documents_have_different_case_sensitivity =
        other.GetDocument().InQuirksMode() != GetDocument().InQuirksMode();

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
                                        GetDocument()))
    element_data_ = other.element_data_;
  else
    element_data_ = other.element_data_->MakeUniqueCopy();

  for (const Attribute& attr : element_data_->Attributes()) {
    AttributeChanged(
        AttributeModificationParams(attr.GetName(), g_null_atom, attr.Value(),
                                    AttributeModificationReason::kByCloning));
  }

  if (other.nonce() != g_null_atom)
    setNonce(other.nonce());
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
  DCHECK(GetElementData());
  DCHECK(GetElementData()->style_attribute_is_dirty());
  GetElementData()->SetStyleAttributeIsDirty(false);
  const CSSPropertyValueSet* inline_style = InlineStyle();
  const_cast<Element*>(this)->SetSynchronizedLazyAttribute(
      html_names::kStyleAttr,
      inline_style ? AtomicString(inline_style->AsText()) : g_empty_atom);
}

CSSStyleDeclaration* Element::style() {
  if (!IsStyledElement())
    return nullptr;
  return &EnsureElementRareData().EnsureInlineCSSStyleDeclaration(this);
}

StylePropertyMap* Element::attributeStyleMap() {
  if (!IsStyledElement())
    return nullptr;
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

inline void Element::SetInlineStyleFromString(
    const AtomicString& new_style_string) {
  DCHECK(IsStyledElement());
  Member<CSSPropertyValueSet>& inline_style = GetElementData()->inline_style_;

  // Avoid redundant work if we're using shared attribute data with already
  // parsed inline style.
  if (inline_style && !GetElementData()->IsUnique())
    return;

  // We reconstruct the property set instead of mutating if there is no CSSOM
  // wrapper.  This makes wrapperless property sets immutable and so cacheable.
  if (inline_style && !inline_style->IsMutable())
    inline_style.Clear();

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

void Element::StyleAttributeChanged(
    const AtomicString& new_style_string,
    AttributeModificationReason modification_reason) {
  DCHECK(IsStyledElement());
  WTF::OrdinalNumber start_line_number = WTF::OrdinalNumber::BeforeFirst();
  if (GetDocument().GetScriptableDocumentParser() &&
      !GetDocument().IsInDocumentWrite())
    start_line_number =
        GetDocument().GetScriptableDocumentParser()->LineNumber();

  if (new_style_string.IsNull()) {
    EnsureUniqueElementData().inline_style_.Clear();
  } else if (modification_reason == AttributeModificationReason::kByCloning ||
             (ContainingShadowRoot() &&
              ContainingShadowRoot()->IsUserAgent()) ||
             (GetExecutionContext() &&
              GetExecutionContext()
                  ->GetContentSecurityPolicyForCurrentWorld()
                  ->AllowInline(
                      ContentSecurityPolicy::InlineType::kStyleAttribute, this,
                      new_style_string, String() /* nonce */,
                      GetDocument().Url(), start_line_number))) {
    SetInlineStyleFromString(new_style_string);
  }

  GetElementData()->SetStyleAttributeIsDirty(false);

  SetNeedsStyleRecalc(kLocalStyleChange,
                      StyleChangeReasonForTracing::Create(
                          style_change_reason::kStyleSheetChange));
  probe::DidInvalidateStyleAttr(this);
}

void Element::InlineStyleChanged() {
  DCHECK(IsStyledElement());
  InvalidateStyleAttribute();
  probe::DidInvalidateStyleAttr(this);

  if (MutationObserverInterestGroup* recipients =
          MutationObserverInterestGroup::CreateForAttributesMutation(
              *this, html_names::kStyleAttr)) {
    // We don't use getAttribute() here to get a style attribute value
    // before the change.
    AtomicString old_value;
    if (const Attribute* attribute =
            GetElementData()->Attributes().Find(html_names::kStyleAttr))
      old_value = attribute->Value();
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
  SetInlineStyleProperty(property_id, *CSSIdentifierValue::Create(identifier),
                         important);
}

void Element::SetInlineStyleProperty(CSSPropertyID property_id,
                                     double value,
                                     CSSPrimitiveValue::UnitType unit,
                                     bool important) {
  SetInlineStyleProperty(
      property_id, *CSSNumericLiteralValue::Create(value, unit), important);
}

void Element::SetInlineStyleProperty(CSSPropertyID property_id,
                                     const CSSValue& value,
                                     bool important) {
  DCHECK(IsStyledElement());
  EnsureMutableInlineStyle().SetProperty(property_id, value, important);
  InlineStyleChanged();
}

bool Element::SetInlineStyleProperty(CSSPropertyID property_id,
                                     const String& value,
                                     bool important) {
  DCHECK(IsStyledElement());
  bool did_change =
      EnsureMutableInlineStyle()
          .SetProperty(property_id, value, important,
                       GetExecutionContext()
                           ? GetExecutionContext()->GetSecureContextMode()
                           : SecureContextMode::kInsecureContext,
                       GetDocument().ElementSheet().Contents())
          .did_change;
  if (did_change)
    InlineStyleChanged();
  return did_change;
}

bool Element::RemoveInlineStyleProperty(CSSPropertyID property_id) {
  DCHECK(IsStyledElement());
  if (!InlineStyle())
    return false;
  bool did_change = EnsureMutableInlineStyle().RemoveProperty(property_id);
  if (did_change)
    InlineStyleChanged();
  return did_change;
}

bool Element::RemoveInlineStyleProperty(const AtomicString& property_name) {
  DCHECK(IsStyledElement());
  if (!InlineStyle())
    return false;
  bool did_change = EnsureMutableInlineStyle().RemoveProperty(property_name);
  if (did_change)
    InlineStyleChanged();
  return did_change;
}

void Element::RemoveAllInlineStyleProperties() {
  DCHECK(IsStyledElement());
  if (!InlineStyle())
    return;
  EnsureMutableInlineStyle().Clear();
  InlineStyleChanged();
}

void Element::UpdatePresentationAttributeStyle() {
  SynchronizeAllAttributes();
  // ShareableElementData doesn't store presentation attribute style, so make
  // sure we have a UniqueElementData.
  UniqueElementData& element_data = EnsureUniqueElementData();
  element_data.SetPresentationAttributeStyleIsDirty(false);
  element_data.presentation_attribute_style_ =
      ComputePresentationAttributeStyle(*this);
}

CSSPropertyValueSet* Element::CreatePresentationAttributeStyle() {
  auto* style = MakeGarbageCollected<MutableCSSPropertyValueSet>(
      IsSVGElement() ? kSVGAttributeMode : kHTMLStandardMode);
  AttributeCollection attributes = AttributesWithoutUpdate();
  for (const Attribute& attr : attributes)
    CollectStyleForPresentationAttribute(attr.GetName(), attr.Value(), style);
  if (IsSVGElement())
    To<SVGElement>(this)->CollectStyleForAnimatedPresentationAttributes(style);
  return style;
}

void Element::AddPropertyToPresentationAttributeStyle(
    MutableCSSPropertyValueSet* style,
    CSSPropertyID property_id,
    CSSValueID identifier) {
  DCHECK(IsStyledElement());
  style->SetProperty(property_id, *CSSIdentifierValue::Create(identifier));
}

void Element::AddPropertyToPresentationAttributeStyle(
    MutableCSSPropertyValueSet* style,
    CSSPropertyID property_id,
    double value,
    CSSPrimitiveValue::UnitType unit) {
  DCHECK(IsStyledElement());
  style->SetProperty(property_id, *CSSNumericLiteralValue::Create(value, unit));
}

void Element::AddPropertyToPresentationAttributeStyle(
    MutableCSSPropertyValueSet* style,
    CSSPropertyID property_id,
    const String& value) {
  DCHECK(IsStyledElement());
  style->SetProperty(property_id, value, false,
                     GetExecutionContext()
                         ? GetExecutionContext()->GetSecureContextMode()
                         : SecureContextMode::kInsecureContext,
                     GetDocument().ElementSheet().Contents());
}

void Element::AddPropertyToPresentationAttributeStyle(
    MutableCSSPropertyValueSet* style,
    CSSPropertyID property_id,
    const CSSValue& value) {
  DCHECK(IsStyledElement());
  style->SetProperty(property_id, value);
}

void Element::LogAddElementIfIsolatedWorldAndInDocument(
    const char element[],
    const QualifiedName& attr1) {
  if (!isConnected())
    return;
  V8DOMActivityLogger* activity_logger =
      V8DOMActivityLogger::CurrentActivityLoggerIfIsolatedWorldForMainThread();
  if (!activity_logger)
    return;
  Vector<String, 2> argv;
  argv.push_back(element);
  argv.push_back(FastGetAttribute(attr1));
  activity_logger->LogEvent("blinkAddElement", argv.size(), argv.data());
}

void Element::LogAddElementIfIsolatedWorldAndInDocument(
    const char element[],
    const QualifiedName& attr1,
    const QualifiedName& attr2) {
  if (!isConnected())
    return;
  V8DOMActivityLogger* activity_logger =
      V8DOMActivityLogger::CurrentActivityLoggerIfIsolatedWorldForMainThread();
  if (!activity_logger)
    return;
  Vector<String, 3> argv;
  argv.push_back(element);
  argv.push_back(FastGetAttribute(attr1));
  argv.push_back(FastGetAttribute(attr2));
  activity_logger->LogEvent("blinkAddElement", argv.size(), argv.data());
}

void Element::LogAddElementIfIsolatedWorldAndInDocument(
    const char element[],
    const QualifiedName& attr1,
    const QualifiedName& attr2,
    const QualifiedName& attr3) {
  if (!isConnected())
    return;
  V8DOMActivityLogger* activity_logger =
      V8DOMActivityLogger::CurrentActivityLoggerIfIsolatedWorldForMainThread();
  if (!activity_logger)
    return;
  Vector<String, 4> argv;
  argv.push_back(element);
  argv.push_back(FastGetAttribute(attr1));
  argv.push_back(FastGetAttribute(attr2));
  argv.push_back(FastGetAttribute(attr3));
  activity_logger->LogEvent("blinkAddElement", argv.size(), argv.data());
}

void Element::LogUpdateAttributeIfIsolatedWorldAndInDocument(
    const char element[],
    const AttributeModificationParams& params) {
  if (!isConnected())
    return;
  V8DOMActivityLogger* activity_logger =
      V8DOMActivityLogger::CurrentActivityLoggerIfIsolatedWorldForMainThread();
  if (!activity_logger)
    return;
  Vector<String, 4> argv;
  argv.push_back(element);
  argv.push_back(params.name.ToString());
  argv.push_back(params.old_value);
  argv.push_back(params.new_value);
  activity_logger->LogEvent("blinkSetAttribute", argv.size(), argv.data());
}

void Element::Trace(Visitor* visitor) const {
  visitor->Trace(element_data_);
  ContainerNode::Trace(visitor);
}

bool Element::HasPart() const {
  if (HasRareData()) {
    if (auto* part = GetElementRareData()->GetPart()) {
      return part->length() > 0;
    }
  }
  return false;
}

DOMTokenList* Element::GetPart() const {
  return HasRareData() ? GetElementRareData()->GetPart() : nullptr;
}

DOMTokenList& Element::part() {
  ElementRareData& rare_data = EnsureElementRareData();
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
  return HasRareData() ? GetElementRareData()->PartNamesMap() : nullptr;
}

bool Element::ChildStyleRecalcBlockedByDisplayLock() const {
  auto* context = GetDisplayLockContext();
  return context && !context->ShouldStyleChildren();
}

void Element::SetHovered(bool hovered) {
  if (hovered == IsHovered())
    return;

  GetDocument().UserActionElements().SetHovered(this, hovered);

  const ComputedStyle* style = GetComputedStyle();
  if (!style || style->AffectedByHover()) {
    StyleChangeType change_type = kLocalStyleChange;
    if (style && style->HasPseudoElementStyle(kPseudoIdFirstLetter))
      change_type = kSubtreeStyleChange;
    SetNeedsStyleRecalc(change_type,
                        StyleChangeReasonForTracing::CreateWithExtraData(
                            style_change_reason::kPseudoClass,
                            style_change_extra_data::g_hover));
  }
  if (ChildrenOrSiblingsAffectedByHover())
    PseudoStateChanged(CSSSelector::kPseudoHover);

  InvalidateIfHasEffectiveAppearance();
}

void Element::SetActive(bool active) {
  if (active == IsActive())
    return;

  GetDocument().UserActionElements().SetActive(this, active);

  if (!GetLayoutObject()) {
    if (ChildrenOrSiblingsAffectedByActive()) {
      PseudoStateChanged(CSSSelector::kPseudoActive);
    } else {
      SetNeedsStyleRecalc(kLocalStyleChange,
                          StyleChangeReasonForTracing::CreateWithExtraData(
                              style_change_reason::kPseudoClass,
                              style_change_extra_data::g_active));
    }
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
  if (ChildrenOrSiblingsAffectedByActive())
    PseudoStateChanged(CSSSelector::kPseudoActive);

  if (!IsDisabledFormControl())
    InvalidateIfHasEffectiveAppearance();
}

void Element::InvalidateStyleAttribute() {
  DCHECK(GetElementData());
  GetElementData()->SetStyleAttributeIsDirty(true);
  SetNeedsStyleRecalc(kLocalStyleChange,
                      StyleChangeReasonForTracing::Create(
                          style_change_reason::kInlineCSSStyleMutated));
  GetDocument().GetStyleEngine().AttributeChangedForElement(
      html_names::kStyleAttr, *this);
}

}  // namespace blink
