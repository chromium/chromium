/*
 * Copyright (C) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink.h"
#include "third_party/blink/public/mojom/render_accessibility.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/accessibility/scoped_blink_ax_event_intent.h"
#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/events/event_util.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/listed_element.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/html_table_row_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_progress.h"
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/line/abstract_inline_text_box.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/style/content_data.h"
#include "third_party/blink/renderer/core/svg/svg_style_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_image_map_link.h"
#include "third_party/blink/renderer/modules/accessibility/ax_inline_text_box.h"
#include "third_party/blink/renderer/modules/accessibility/ax_layout_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_list_box.h"
#include "third_party/blink/renderer/modules/accessibility/ax_list_box_option.h"
#include "third_party/blink/renderer/modules/accessibility/ax_media_control.h"
#include "third_party/blink/renderer/modules/accessibility/ax_media_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list_option.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list_popup.h"
#include "third_party/blink/renderer/modules/accessibility/ax_progress_indicator.h"
#include "third_party/blink/renderer/modules/accessibility/ax_relation_cache.h"
#include "third_party/blink/renderer/modules/accessibility/ax_slider.h"
#include "third_party/blink/renderer/modules/accessibility/ax_validation_message.h"
#include "third_party/blink/renderer/modules/accessibility/ax_virtual_object.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_enums.mojom-blink.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/mojom/ax_relative_bounds.mojom-blink.h"

// Prevent code that runs during the lifetime of the stack from altering the
// document lifecycle. Usually doc is the same as document_, but it can be
// different when it is a popup document. Because it's harmless to test both
// documents, even if they are the same, the scoped check is initialized for
// both documents.
// clang-format off
#if DCHECK_IS_ON()
#define SCOPED_DISALLOW_LIFECYCLE_TRANSITION(document)                        \
  DocumentLifecycle::DisallowTransitionScope scoped1((document).Lifecycle()); \
  DocumentLifecycle::DisallowTransitionScope scoped2(document_->Lifecycle())
#else
#define SCOPED_DISALLOW_LIFECYCLE_TRANSITION(document)
#endif  // DCHECK_IS_ON()
// clang-format on

namespace blink {

namespace {

// Return a node for the current layout object or ancestor layout object.
Node* GetClosestNodeForLayoutObject(const LayoutObject* layout_object) {
  if (!layout_object)
    return nullptr;
  Node* node = layout_object->GetNode();
  return node ? node : GetClosestNodeForLayoutObject(layout_object->Parent());
}

// Return true if display locked or inside slot recalc, false otherwise.
// Also returns false if not a safe time to perform the check.
bool IsDisplayLocked(const Node* node, bool inclusive = false) {
  if (!node)
    return false;
  // The IsDisplayLockedPreventingPaint() function may attempt to do
  // a flat tree traversal of ancestors. If we're in a flat tree traversal
  // forbidden scope, return false. Additionally, flat tree traversal
  // might call AssignedSlot, so if we're in a slot assignment recalc
  // forbidden scope, return false.
  if (node->GetDocument().IsFlatTreeTraversalForbidden() ||
      node->GetDocument()
          .GetSlotAssignmentEngine()
          .HasPendingSlotAssignmentRecalc()) {
    return false;  // Cannot safely perform this check now.
  }
  return DisplayLockUtilities::IsDisplayLockedPreventingPaint(node, inclusive);
}

bool IsDisplayLocked(const LayoutObject* object) {
  bool inclusive = false;
  while (object) {
    if (const auto* node = object->GetNode())
      return IsDisplayLocked(node, inclusive);
    inclusive = true;
    object = object->Parent();
  }
  return false;
}

bool IsActive(Document& document) {
  return document.IsActive() && !document.IsDetached();
}

bool HasAriaCellRole(Element* elem) {
  DCHECK(elem);
  const AtomicString& role_str = elem->FastGetAttribute(html_names::kRoleAttr);
  if (role_str.IsEmpty())
    return false;

  return ui::IsCellOrTableHeader(AXObject::AriaRoleStringToRoleEnum(role_str));
}

// How deep can role="presentation" propagate from this node (inclusive)?
// For example, propagates from table->tbody->tr->td (4).
// Limiting the depth is an optimization that keeps recursion under control.
int RolePresentationPropagationDepth(Node* node) {
  // Check for list markup.
  if (IsA<HTMLMenuElement>(node) || IsA<HTMLUListElement>(node) ||
      IsA<HTMLOListElement>(node)) {
    return 2;
  }

  // Check for <table>
  if (IsA<HTMLTableElement>(node))
    return 4;  // table section, table row, table cells,

  // Check for display: table CSS.
  if (node->GetLayoutObject() && node->GetLayoutObject()->IsTable())
    return 4;

  return 0;
}

// Return true if whitespace is not necessary to keep adjacent_node separate
// in screen reader output from surrounding nodes.
bool CanIgnoreSpaceNextTo(LayoutObject* layout_object,
                          bool is_after,
                          int counter = 0) {
  if (!layout_object)
    return true;

  if (counter > 3)
    return false;  // Don't recurse more than 3 times.

  auto* elem = DynamicTo<Element>(layout_object->GetNode());

  // Can usually ignore space next to a <br>.
  // Exception: if the space was next to a <br> with an ARIA role.
  if (layout_object->IsBR()) {
    // As an example of a <br> with a role, Google Docs uses:
    // <span contenteditable=false> <br role="presentation></span>.
    // This construct hides the <br> from the AX tree and uses the space
    // instead, presenting a hard line break as a soft line break.
    DCHECK(elem);
    return !is_after || !elem->FastHasAttribute(html_names::kRoleAttr);
  }

  // If adjacent to a whitespace character, the current space can be ignored.
  if (layout_object->IsText()) {
    auto* layout_text = To<LayoutText>(layout_object);
    if (layout_text->HasEmptyText())
      return false;
    if (layout_text->GetText().Impl()->ContainsOnlyWhitespaceOrEmpty())
      return true;
    auto adjacent_char =
        is_after ? layout_text->FirstCharacterAfterWhitespaceCollapsing()
                 : layout_text->LastCharacterAfterWhitespaceCollapsing();
    return adjacent_char == ' ' || adjacent_char == '\n' ||
           adjacent_char == '\t';
  }

  // Keep spaces between images and other visible content.
  if (layout_object->IsLayoutImage())
    return false;

  // Do not keep spaces between blocks.
  if (!layout_object->IsLayoutInline())
    return true;

  // If next to an element that a screen reader will always read separately,
  // the the space can be ignored.
  // Elements that are naturally focusable even without a tabindex tend
  // to be rendered separately even if there is no space between them.
  // Some ARIA roles act like table cells and don't need adjacent whitespace to
  // indicate separation.
  // False negatives are acceptable in that they merely lead to extra whitespace
  // static text nodes.
  if (elem && HasAriaCellRole(elem))
    return true;

  // Test against the appropriate child text node.
  auto* layout_inline = To<LayoutInline>(layout_object);
  LayoutObject* child =
      is_after ? layout_inline->FirstChild() : layout_inline->LastChild();
  if (!child && elem) {
    // No children of inline element. Check adjacent sibling in same direction.
    Node* adjacent_node =
        is_after ? NodeTraversal::NextIncludingPseudoSkippingChildren(*elem)
                 : NodeTraversal::PreviousAbsoluteSiblingIncludingPseudo(*elem);
    return adjacent_node &&
           CanIgnoreSpaceNextTo(adjacent_node->GetLayoutObject(), is_after,
                                ++counter);
  }
  return CanIgnoreSpaceNextTo(child, is_after, ++counter);
}

bool IsTextRelevantForAccessibility(const LayoutText& layout_text) {
  if (!layout_text.Parent())
    return false;

  const Node* node = layout_text.GetNode();
  DCHECK(node);  // Anonymous text is processed earlier, doesn't reach here.

#if DCHECK_IS_ON()
  DCHECK(node->GetDocument().Lifecycle().GetState() >=
         DocumentLifecycle::kAfterPerformLayout)
      << "Unclean document at lifecycle "
      << node->GetDocument().Lifecycle().ToString();
#endif

  // Ignore empty text.
  if (layout_text.HasEmptyText())
    return false;

  // Always keep if anything other than collapsible whitespace.
  if (!layout_text.IsAllCollapsibleWhitespace() || layout_text.IsBR())
    return true;

  // Will now look at sibling nodes. We need the closest element to the
  // whitespace markup-wise, e.g. tag1 in these examples:
  // [whitespace] <tag1><tag2>x</tag2></tag1>
  // <span>[whitespace]</span> <tag1><tag2>x</tag2></tag1>.
  // Do not use LayoutTreeBuilderTraversal or FlatTreeTraversal as this may need
  // to be called during slot assignment, when flat tree traversal is forbidden.
  Node* prev_node =
      NodeTraversal::PreviousAbsoluteSiblingIncludingPseudo(*node);
  if (!prev_node)
    return false;

  Node* next_node = NodeTraversal::NextIncludingPseudoSkippingChildren(*node);
  if (!next_node)
    return false;

  // Ignore extra whitespace-only text if a sibling will be presented
  // separately by screen readers whether whitespace is there or not.
  if (CanIgnoreSpaceNextTo(prev_node->GetLayoutObject(), false) ||
      CanIgnoreSpaceNextTo(next_node->GetLayoutObject(), true)) {
    return false;
  }

  // If the prev/next node is also a text node and the adjacent character is
  // not whitespace, CanIgnoreSpaceNextTo will return false. In some cases that
  // is what we want; in other cases it is not. Examples:
  //
  // 1a: <p><span>Hello</span><span>[whitespace]</span><span>World</span></p>
  // 1b: <p><span>Hello</span>[whitespace]<span>World</span></p>
  // 2:  <div><ul><li style="display:inline;">x</li>[whitespace]</ul>y</div>
  //
  // In the first case, we want to preserve the whitespace (crbug.com/435765).
  // In the second case, the whitespace in the markup is not relevant because
  // the "x" is separated from the "y" by virtue of being inside a different
  // block. In order to distinguish these two scenarios, we can use the
  // LayoutBox associated with each node. For the first scenario, each node's
  // LayoutBox is the LayoutBlockFlow associated with the <p>. For the second
  // scenario, the LayoutBox of "x" and the whitespace is the LayoutBlockFlow
  // associated with the <ul>; the LayoutBox of "y" is the one associated with
  // the <div>.
  LayoutBox* box = layout_text.EnclosingBox();
  if (!box)
    return false;

  if (prev_node->GetLayoutObject() && prev_node->GetLayoutObject()->IsText()) {
    LayoutBox* prev_box = prev_node->GetLayoutObject()->EnclosingBox();
    if (prev_box != box)
      return false;
  }

  if (next_node->GetLayoutObject() && next_node->GetLayoutObject()->IsText()) {
    LayoutBox* next_box = next_node->GetLayoutObject()->EnclosingBox();
    if (next_box != box)
      return false;
  }

  return true;
}

bool IsShadowContentRelevantForAccessibility(const Node* node) {
  DCHECK(node->ContainingShadowRoot());

  // Native <img> create extra child nodes to hold alt text.
  if (node->IsInUserAgentShadowRoot() &&
      IsA<HTMLImageElement>(node->OwnerShadowHost())) {
    return false;
  }

  // Don't use non-<option> descendants of an AXMenuList.
  // If the UseAXMenuList flag is on, we use a specialized class AXMenuList
  // for handling the user-agent shadow DOM exposed by a <select> element.
  // That class adds a mock AXMenuListPopup, which adds AXMenuListOption
  // children for <option> descendants only.
  if (AXObjectCacheImpl::UseAXMenuList() && node->IsInUserAgentShadowRoot() &&
      !IsA<HTMLOptionElement>(node)) {
    // Find any ancestor <select> if it is present.
    Node* host = node->OwnerShadowHost();
    auto* select_element = DynamicTo<HTMLSelectElement>(host);
    if (!select_element) {
      // An <optgroup> can be a shadow host too -- look for it's owner <select>.
      if (auto* opt_group_element = DynamicTo<HTMLOptGroupElement>(host))
        select_element = opt_group_element->OwnerSelectElement();
    }
    if (select_element) {
      if (!select_element->GetLayoutObject())
        return select_element->IsInCanvasSubtree();
      // Non-option: only create AXObject if not inside an AXMenuList.
      return !AXObjectCacheImpl::ShouldCreateAXMenuListFor(
          select_element->GetLayoutObject());
    }
  }

  // Outside of AXMenuList descendants, all other non-slot user agent shadow
  // nodes are relevant.
  const HTMLSlotElement* slot_element =
      ToHTMLSlotElementIfSupportsAssignmentOrNull(node);
  if (!slot_element)
    return true;

  // Slots are relevant if they have content.
  // However, this can only be checked during safe times.
  // During other times we must assume that the <slot> is relevant.
  // TODO(accessibility) Consider removing this rule, but it will require
  // a different way of dealing with these PDF test failures:
  // https://chromium-review.googlesource.com/c/chromium/src/+/2965317
  // For some reason the iframe tests hang, waiting for content to change. In
  // other words, returning true here causes some tree updates not to occur.
  if (node->GetDocument().IsFlatTreeTraversalForbidden() ||
      node->GetDocument()
          .GetSlotAssignmentEngine()
          .HasPendingSlotAssignmentRecalc()) {
    return true;
  }

  // If the slot element's host is an <object>/<embed>with any descendant nodes
  // (including whitespace), LayoutTreeBuilderTraversal::FirstChild will
  // return a node. We should only treat that node as slot content if it is
  // being used as fallback content.
  if (const HTMLPlugInElement* plugin_element =
          DynamicTo<HTMLPlugInElement>(node->OwnerShadowHost())) {
    return plugin_element->UseFallbackContent();
  }

  return LayoutTreeBuilderTraversal::FirstChild(*slot_element);
}

bool IsLayoutObjectRelevantForAccessibility(const LayoutObject& layout_object) {
  if (layout_object.IsAnonymous()) {
    // Anonymous means there is no DOM node, and it's been inserted by the
    // layout engine within the tree. An example is an anonymous block that is
    // inserted as a parent of an inline where there are block siblings.
    return AXObjectCacheImpl::IsRelevantPseudoElementDescendant(layout_object);
  }

  Node* node = layout_object.GetNode();
  DCHECK(node) << "Non-anonymous layout objects always have a node";

  if (node->ContainingShadowRoot() &&
      !IsShadowContentRelevantForAccessibility(node)) {
    return false;
  }

  if (layout_object.IsText())
    return IsTextRelevantForAccessibility(To<LayoutText>(layout_object));

  // Menu list option and HTML area elements are indexed by DOM node, never by
  // layout object.
  if (AXObjectCacheImpl::ShouldCreateAXMenuListOptionFor(node))
    return false;

  // TODO(accessibility) Refactor so that the following rules are not repeated
  // in IsNodeRelevantForAccessibility().

  if (IsA<HTMLAreaElement>(node))
    return false;

  if (node->IsPseudoElement())
    return AXObjectCacheImpl::IsRelevantPseudoElement(*node);

  if (const HTMLSlotElement* slot =
          ToHTMLSlotElementIfSupportsAssignmentOrNull(node)) {
    return AXObjectCacheImpl::IsRelevantSlotElement(*slot);
  }

  // <optgroup> is irrelevant inside of a <select> menulist.
  if (auto* opt_group = DynamicTo<HTMLOptGroupElement>(node)) {
    if (auto* select = opt_group->OwnerSelectElement())
      return !select->UsesMenuList();
  }

  // An HTML <title> does not require an AXObject: the document's name is
  // retrieved directly via the inner text.
  if (IsA<HTMLTitleElement>(node))
    return false;

  return true;
}

// -----------------------------------------------------------------------------
// IsNodeRelevantForAccessibility() and IsLayoutObjectRelevantForAccessibility()
// * if the LayoutObject is relevant and not display-locked,
//   GetOrCreate() will return an object that will be an AXLayoutObject or
//   derivative. Note that the node may or may not be relevant.
// * Else if the Node is relevant, GetOrCreate() will return an object that will
//   be an AXNodeObject or derivative.
// * Else neither are relevant, and the tree will be truncated (no descendants)
//   at this point.
// -----------------------------------------------------------------------------
// TODO(accessibility) Merge IsNodeRelevantForAccessibility() and
// IsLayoutObjectRelevantForAccessibility() producing a function like
// GetAXType(node, layout_object) returning kTruncateSubtree,
// kAXNodeObject, or kAXLayoutObject. This will allow some of the checks that
// currently happen twice, to only happen once.
bool IsNodeRelevantForAccessibility(const Node* node,
                                    bool parent_ax_known,
                                    bool is_layout_object_relevant) {
  if (!node || !node->isConnected())
    return false;

  if (const Document* document = DynamicTo<Document>(node))
    return document->GetFrame();  // Only relevant if the document has a frame.

  if (node->ContainingShadowRoot() &&
      !IsShadowContentRelevantForAccessibility(node)) {
    return false;
  }

  if (node->IsTextNode()) {
    // Children of an <iframe> tag will always be replaced by a new Document,
    // either loaded from the iframe src or empty. In fact, we don't even parse
    // them and they are treated like one text node. Consider irrelevant.
    if (AXObject::IsFrame(node->parentElement()))
      return false;

    // Layout has more info available to determine if whitespace is relevant.
    // If display-locked, layout object may be missing or stale:
    // Assume that all display-locked text nodes are relevant.
    if (IsDisplayLocked(node))
      return true;

    // If rendered, decision is from IsLayoutObjectRelevantForAccessibility().
    if (node->GetLayoutObject())
      return is_layout_object_relevant;

    // If unrendered + no parent, it is in a shadow tree. Consider irrelevant.
    if (!node->parentElement()) {
      DCHECK(node->IsInShadowTree());
      return false;
    }

    // If unrendered and in <canvas>, consider even whitespace relevant.
    // TODO(aleventhal) Consider including all text, even unrendered whitespace,
    // whether or not in <canvas>. For now this matches previous behavior.
    // Including all text would allow simply returning true at this point.
    if (node->parentElement()->IsInCanvasSubtree())
      return true;

    // Must be unrendered because of CSS. Consider relevant if non-whitespace.
    // Allowing rendered non-whitespace to be considered relevant will allow
    // use for accessible relations such as labelledby and describedby.
    return !To<Text>(node)->ContainsOnlyWhitespaceOrEmpty();
  }

  const Element* element = DynamicTo<Element>(node);
  if (!element)
    return false;  // Only documents, elements and text nodes get ax objects.

  if (IsA<HTMLAreaElement>(node) && !IsA<HTMLMapElement>(node->parentNode())) {
    return false;  // <area> without parent <map> is not relevant.
  }

  if (IsA<HTMLMapElement>(node))
    return false;  // Contains children for an img, but is not its own object.

  if (node->IsPseudoElement())
    return AXObjectCacheImpl::IsRelevantPseudoElement(*node);

  if (const HTMLSlotElement* slot =
          ToHTMLSlotElementIfSupportsAssignmentOrNull(node)) {
    return AXObjectCacheImpl::IsRelevantSlotElement(*slot);
  }

  // <optgroup> is irrelevant inside of a <select> menulist.
  if (auto* opt_group = DynamicTo<HTMLOptGroupElement>(node)) {
    if (auto* select = opt_group->OwnerSelectElement())
      return !select->UsesMenuList();
  }

  // When there is a layout object, the element is known to be visible, so
  // consider it relevant and return early. Checking the layout object is only
  // useful when display locking (content-visibility) is not used.
  if (node->GetLayoutObject() && !IsDisplayLocked(node, true)) {
    return true;
  }

  // An HTML <title> does not require an AXObject: the document's name is
  // retrieved directly via the inner text.
  if (IsA<HTMLTitleElement>(node))
    return false;

  // Do not consider <head>/<style>/<script> relevant.
  if (IsA<HTMLHeadElement>(node))
    return false;
  if (IsA<HTMLStyleElement>(node))
    return false;
  if (IsA<HTMLScriptElement>(node))
    return false;

  // Style elements in SVG are not display: none, unlike HTML style
  // elements, but they are still hidden and thus treated as irrelevant for
  // accessibility.
  if (IsA<SVGStyleElement>(node))
    return false;

  // Not a <head>/<style>/<script>, or SVG<style>:
  // Use a slower check to see if this node is anywhere inside of a <head>,
  // <style> or <script>.
  // This check is not necessary if the parent_ax is already known, which means
  // we are attempting to add this object from something already relevant in the
  // AX tree, and therefore can't be inside a <head>, <style>, <script> or SVG
  // <style> element.
  if (parent_ax_known)
    return true;  // No need to check inside if the parent exists.

  for (const Element* ancestor = element; ancestor;
       ancestor = ancestor->parentElement()) {
    // Objects inside <head> are irrelevant.
    if (IsA<HTMLHeadElement>(ancestor))
      return false;
    // Objects inside a <style> are irrelevant.
    if (IsA<HTMLStyleElement>(ancestor))
      return false;
    // Objects inside a <script> are irrelevant.
    if (IsA<HTMLScriptElement>(ancestor))
      return false;
    // Elements inside of a frame/iframe are irrelevant unless inside a document
    // that is a child of the frame. In the case where descendants are allowed,
    // they will be in a different document, and therefore this loop will not
    // reach the frame/iframe.
    if (AXObject::IsFrame(ancestor))
      return false;
    // Objects inside an SVG <style> are irrelevant.
    // However, when can this condition be reached?
    if (IsA<SVGStyleElement>(ancestor))
      return false;
  }

  // All other objects are relevant, even if hidden.
  return true;
}

}  // namespace

// static
bool AXObjectCacheImpl::use_ax_menu_list_ = false;

// static
AXObjectCache* AXObjectCacheImpl::Create(Document& document,
                                         const ui::AXMode& ax_mode) {
  return MakeGarbageCollected<AXObjectCacheImpl>(document, ax_mode);
}

AXObjectCacheImpl::AXObjectCacheImpl(Document& document,
                                     const ui::AXMode& ax_mode)
    : document_(document),
      ax_mode_(ax_mode),
      modification_count_(0),
      validation_message_axid_(0),
      active_aria_modal_dialog_(nullptr),
      relation_cache_(std::make_unique<AXRelationCache>(this)),
      accessibility_event_permission_(mojom::blink::PermissionStatus::ASK),
      permission_service_(document.GetExecutionContext()),
      permission_observer_receiver_(this, document.GetExecutionContext()),
      ax_tree_source_(BlinkAXTreeSource::Create(*this)),
      ax_tree_serializer_(
          std::make_unique<ui::AXTreeSerializer<AXObject*>>(ax_tree_source_)) {
  if (document_->LoadEventFinished())
    AddPermissionStatusListener();
  use_ax_menu_list_ = GetSettings()->GetUseAXMenuList();
}

AXObjectCacheImpl::~AXObjectCacheImpl() {
#if DCHECK_IS_ON()
  DCHECK(has_been_disposed_);
#endif
}

void AXObjectCacheImpl::Dispose() {
  DCHECK(!has_been_disposed_) << "Something is wrong, trying to dispose twice.";
  has_been_disposed_ = true;

  for (auto& entry : objects_) {
    AXObject* obj = entry.value;
    obj->Detach();
    RemoveAXID(obj);
  }

  permission_observer_receiver_.reset();
}

void AXObjectCacheImpl::AddInspectorAgent(InspectorAccessibilityAgent* agent) {
  agents_.insert(agent);
}

void AXObjectCacheImpl::RemoveInspectorAgent(
    InspectorAccessibilityAgent* agent) {
  agents_.erase(agent);
}

AXObject* AXObjectCacheImpl::Root() {
  return GetOrCreate(document_);
}

AXObject* AXObjectCacheImpl::ObjectFromAXID(AXID id) const {
  auto it = objects_.find(id);
  return it != objects_.end() ? it->value : nullptr;
}

Node* AXObjectCacheImpl::FocusedElement() {
  Node* focused_node = document_->FocusedElement();
  if (!focused_node)
    focused_node = document_;

  // See if there's a page popup, for example a calendar picker.
  auto* input = DynamicTo<HTMLInputElement>(focused_node);
  if (!input && focused_node->IsInUserAgentShadowRoot()) {
    input = DynamicTo<HTMLInputElement>(focused_node->OwnerShadowHost());
  }
  if (input) {
    if (AXObject* ax_popup = input->PopupRootAXObject()) {
      if (Element* focused_element_in_popup =
              ax_popup->GetDocument()->FocusedElement())
        focused_node = focused_element_in_popup;
    }
  }

  return focused_node;
}

void AXObjectCacheImpl::UpdateLifecycleIfNeeded() {
  if (GetDocument().Lifecycle().GetState() < DocumentLifecycle::kLayoutClean) {
    // Node is in a different, unclean document. This can occur in an open
    // popup. Ensure the popup document has a clean layout before trying to
    // create an AXObject from a node in it.
    if (auto* view = GetDocument().View()) {
      view->UpdateAllLifecyclePhasesExceptPaint(
          DocumentUpdateReason::kAccessibility);
    }
  }
}

AXObject* AXObjectCacheImpl::GetOrCreateFocusedObjectFromNode(Node* node) {
  // TODO(chrishtr): refactor to use UpdateLifecycleIfNeeded.
  if (node->GetDocument() != GetDocument() &&
      node->GetDocument().Lifecycle().GetState() <
          DocumentLifecycle::kLayoutClean) {
    // Node is in a different, unclean document. This can occur in an open
    // popup. Ensure the popup document has a clean layout before trying to
    // create an AXObject from a node in it.
    if (node->GetDocument().View()) {
      node->GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
          DocumentUpdateReason::kAccessibility);
    }
  }

  AXObject* obj = GetOrCreate(node);
  if (!obj)
    return nullptr;

  Settings* settings = GetSettings();
  if (settings && settings->GetAriaModalPrunesAXTree()) {
    // It is possible for the active_aria_modal_dialog_ to become detached in
    // between the time a node claims focus and the time we notify platforms
    // of that focus change. For instance given an aria-modal dialog which was
    // newly unhidden (rather than newly added to the DOM):
    // * HandleFocusedUIElementChanged calls UpdateActiveAriaModalDialog
    // * UpdateActiveAriaModalDialog sets the value of active_aria_modal_dialog_
    //   and then marks the entire tree dirty if that value changed.
    // * The subsequent tree update results in the stored active dialog being
    //   detached and replaced.
    // Should this occur, the focused node we're getting or creating here is
    // not a descendant of active_aria_modal_dialog_ and is thus pruned from
    // the tree. This leads to firing the event on the included parent object,
    // which is likely a non-focusable container.
    // We could probably address this situation in one of the clean-layout
    // functions (e.g. HandleNodeGainedFocusWithCleanLayout). However, because
    // both HandleNodeGainedFocusWithCleanLayout and FocusedObject call
    // GetOrCreateFocusedObjectFromNode, detecting and correcting this issue
    // here seems like it covers more bases.
    // TODO(crbug.com/1328815): We need to take a close look at the aria-modal
    // tree pruning logic to be sure there are not other situations where we
    // incorrectly prune content which should be exposed.
    if (active_aria_modal_dialog_ && active_aria_modal_dialog_->IsDetached())
      UpdateActiveAriaModalDialog(node);
  }

  // the HTML element, for example, is focusable but has an AX object that is
  // ignored
  if (!obj->AccessibilityIsIncludedInTree())
    obj = obj->ParentObjectIncludedInTree();

  return obj;
}

AXObject* AXObjectCacheImpl::FocusedObject() {
  return GetOrCreateFocusedObjectFromNode(FocusedElement());
}

const ui::AXMode& AXObjectCacheImpl::GetAXMode() {
  return ax_mode_;
}

void AXObjectCacheImpl::SetAXMode(const ui::AXMode& ax_mode) {
  ax_mode_ = ax_mode;
}

AXObject* AXObjectCacheImpl::Get(const LayoutObject* layout_object) {
  if (!layout_object)
    return nullptr;

  auto it_id = layout_object_mapping_.find(layout_object);
  AXID ax_id = it_id != layout_object_mapping_.end() ? it_id->value : 0;
  DCHECK(!HashTraits<AXID>::IsDeletedValue(ax_id));

  Node* node = layout_object->GetNode();

  if (!ax_id)
    return node ? Get(node) : nullptr;

  if (IsDisplayLocked(node) ||
      !IsLayoutObjectRelevantForAccessibility(*layout_object)) {
    // Change from AXLayoutObject -> AXNodeObject.
    // We previously saved the node in the cache with its layout object,
    // but now it's in a locked subtree so we should remove the entry with its
    // layout object and replace it with an AXNodeObject created from the node
    // instead. Do this later at a safe time.
    if (node) {
      Invalidate(node->GetDocument(), ax_id);
    } else {
      // Happens if pseudo content no longer relevant.
      Remove(const_cast<LayoutObject*>(layout_object));
      return nullptr;
    }
  }

  auto it_result = objects_.find(ax_id);
  AXObject* result = it_result != objects_.end() ? it_result->value : nullptr;
#if DCHECK_IS_ON()
  DCHECK(result) << "Had AXID for Node but no entry in objects_";
  DCHECK(result->IsAXNodeObject());
  // Do not allow detached objects except when disposing entire tree.
  DCHECK(!result->IsDetached() || has_been_disposed_)
      << "Detached AXNodeObject in map: "
      << "AXID#" << ax_id << " Node=" << node;
#endif
  return result;
}

AXObject* AXObjectCacheImpl::SafeGet(const Node* node,
                                     bool allow_display_locking_invalidation) {
  if (!node)
    return nullptr;

#if DCHECK_IS_ON()
  if (const Element* element = DynamicTo<Element>(node)) {
    if (AccessibleNode* accessible_node = element->ExistingAccessibleNode()) {
      DCHECK(!accessible_node_mapping_.Contains(accessible_node))
          << "The accessible node directly attached to an element should not "
             "have its own AXObject: "
          << element;
    }
  }
#endif

  LayoutObject* layout_object = node->GetLayoutObject();

  AXID layout_id = 0;
  if (layout_object) {
    auto it = layout_object_mapping_.find(layout_object);
    if (it != layout_object_mapping_.end())
      layout_id = it->value;
  }
  DCHECK(!HashTraits<AXID>::IsDeletedValue(layout_id));
  if (layout_id) {
    auto it = objects_.find(layout_id);
    if (it != objects_.end()) {
      if (allow_display_locking_invalidation && IsDisplayLocked(node)) {
        // Change from AXLayoutObject -> AXNodeObject.
        // The node is in a display locked subtree, but we've previously put it
        // in the cache with its layout object.
        Invalidate(layout_object->GetDocument(), layout_id);
      }
      return it->value;
    }
    return nullptr;
  }

  auto it_node = node_object_mapping_.find(node);
  AXID node_id = it_node != node_object_mapping_.end() ? it_node->value : 0;
  DCHECK(!HashTraits<AXID>::IsDeletedValue(node_id));
  if (node_id) {
    auto it = objects_.find(node_id);
    if (it != objects_.end()) {
      if (allow_display_locking_invalidation && layout_object &&
          !IsDisplayLocked(node)) {
        // Change from AXNodeObject -> AXLayoutObject.
        // Has a layout object but no layout_id, meaning that when the AXObject
        // was originally created only for Node*, the LayoutObject* didn't exist
        // yet. This can happen if an AXNodeObject is created for a node that's
        // not laid out, but later something changes and it gets a layoutObject
        // (like if it's reparented). It's also possible the layout object
        // changed.
        Invalidate(layout_object->GetDocument(), node_id);
      }
      return it->value;
    }
  }

  return nullptr;
}

AXObject* AXObjectCacheImpl::Get(const Node* node) {
  if (!node)
    return nullptr;

  if (has_been_disposed_)
    return SafeGet(node);

#if DCHECK_IS_ON()
  if (const Element* element = DynamicTo<Element>(node)) {
    if (AccessibleNode* accessible_node = element->ExistingAccessibleNode()) {
      DCHECK(!accessible_node_mapping_.Contains(accessible_node))
          << "The accessible node directly attached to an element should not "
             "have its own AXObject: "
          << element;
    }
  }
#endif

  LayoutObject* layout_object = node->GetLayoutObject();

  AXID layout_id = 0;
  if (layout_object) {
    auto it = layout_object_mapping_.find(layout_object);
    if (it != layout_object_mapping_.end())
      layout_id = it->value;
  }
  DCHECK(!HashTraits<AXID>::IsDeletedValue(layout_id));

  auto it_node = node_object_mapping_.find(node);
  AXID node_id = it_node != node_object_mapping_.end() ? it_node->value : 0;
  DCHECK(!HashTraits<AXID>::IsDeletedValue(node_id));

  if (!layout_id && !node_id)
    return nullptr;

  // Some elements such as <area> are indexed by DOM node, not by layout object.
  if (!layout_object ||
      !IsLayoutObjectRelevantForAccessibility(*layout_object)) {
    // Only text nodes still are able to become suddenly irrelevant.
    if (layout_id && node->IsTextNode() &&
        !IsNodeRelevantForAccessibility(node, /*parent known*/ false,
                                        /*layout relevant*/ false)) {
      // Layout object and node are now both irrelevant for accessibility.
      // For example, text becomes irrelevant when it changes to whitespace, or
      // if it already is whitespace and the text around it changes to makes it
      // redundant whitespace. In this case, Invalidate(), which will remove
      // objects that are no longer relevant.
      Invalidate(node->GetDocument(), layout_id);
    } else {
      // Layout object is irrelevant, but node object can still be relevant.
      if (!node_id) {
        DCHECK(layout_id);  // One of of node_id, layout_id is non-zero.
        Invalidate(node->GetDocument(), layout_id);
      } else {
        layout_object = nullptr;
        layout_id = 0;
      }
    }
  }

  if (layout_id && IsDisplayLocked(node)) {
    // Change from AXLayoutObject -> AXNodeObject.
    // The node is in a display locked subtree, but we've previously put it in
    // the cache with its layout object.
    Invalidate(node->GetDocument(), layout_id);
  } else if (layout_object && node_id && !layout_id && !IsDisplayLocked(node)) {
    // Change from AXNodeObject -> AXLayoutObject.
    // Has a layout object but no layout_id, meaning that when the AXObject was
    // originally created only for Node*, the LayoutObject* didn't exist yet.
    // This can happen if an AXNodeObject is created for a node that's not laid
    // out, but later something changes and it gets a layoutObject (like if it's
    // reparented). It's also possible the layout object changed.
    Invalidate(layout_object->GetDocument(), node_id);
  }

  if (layout_id) {
    auto it = objects_.find(layout_id);
    AXObject* result = it != objects_.end() ? it->value : nullptr;
#if DCHECK_IS_ON()
    DCHECK(result) << "Had AXID for LayoutObject but no entry in objects_";
    DCHECK(result->IsAXLayoutObject());
    // Do not allow detached objects except when disposing entire tree.
    DCHECK(!result->IsDetached() || has_been_disposed_)
        << "Detached AXLayoutObject in map: "
        << "AXID#" << layout_id << " LayoutObject=" << layout_object;
#endif
    return result;
  }

  DCHECK(node_id);

  auto it_result = objects_.find(node_id);
  AXObject* result = it_result != objects_.end() ? it_result->value : nullptr;
#if DCHECK_IS_ON()
  DCHECK(result) << "Had AXID for Node but no entry in objects_";
  DCHECK(result->IsAXNodeObject());
  // Do not allow detached objects except when disposing entire tree.
  DCHECK(!result->IsDetached() || has_been_disposed_)
      << "Detached AXNodeObject in map: "
      << "AXID#" << node_id << " Node=" << node;
#endif
  return result;
}

AXObject* AXObjectCacheImpl::Get(AbstractInlineTextBox* inline_text_box) {
  if (!inline_text_box)
    return nullptr;

  auto it_ax = inline_text_box_object_mapping_.find(inline_text_box);
  AXID ax_id =
      it_ax != inline_text_box_object_mapping_.end() ? it_ax->value : 0;
  DCHECK(!HashTraits<AXID>::IsDeletedValue(ax_id));
  if (!ax_id)
    return nullptr;

  auto it_result = objects_.find(ax_id);
  AXObject* result = it_result != objects_.end() ? it_result->value : nullptr;
#if DCHECK_IS_ON()
  DCHECK(result) << "Had AXID for inline text box but no entry in objects_";
  DCHECK(result->IsAXInlineTextBox());
  // Do not allow detached objects except when disposing entire tree.
  DCHECK(!result->IsDetached() || has_been_disposed_)
      << "Detached AXInlineTextBox in map: "
      << "AXID#" << ax_id << " Node=" << inline_text_box->GetText();
#endif
  return result;
}

void AXObjectCacheImpl::Invalidate(Document& document, AXID ax_id) {
  if (GetInvalidatedIds(document).insert(ax_id).is_new_entry)
    ScheduleVisualUpdate(document);
}

AXID AXObjectCacheImpl::GetAXID(Node* node) {
  AXObject* ax_object = GetOrCreate(node);
  if (!ax_object)
    return 0;
  return ax_object->AXObjectID();
}

AXID AXObjectCacheImpl::GetExistingAXID(Node* node) {
  AXObject* ax_object = SafeGet(node);
  if (!ax_object)
    return 0;
  return ax_object->AXObjectID();
}

AXObject* AXObjectCacheImpl::Get(AccessibleNode* accessible_node) {
  if (!accessible_node)
    return nullptr;

  if (accessible_node->element()) {
    DCHECK(!accessible_node_mapping_.Contains(accessible_node))
        << "The accessible node directly attached to an element should not "
           "have its own AXObject: "
        << accessible_node->element();
    // When the AccessibleNode is attached to an element, return the element's
    // accessible object instead.
    return SafeGet(accessible_node->element());
  }

  auto it_ax = accessible_node_mapping_.find(accessible_node);
  AXID ax_id = it_ax != accessible_node_mapping_.end() ? it_ax->value : 0;
  DCHECK(!HashTraits<AXID>::IsDeletedValue(ax_id));
  if (!ax_id)
    return nullptr;

  auto it_result = objects_.find(ax_id);
  AXObject* result = it_result != objects_.end() ? it_result->value : nullptr;
#if DCHECK_IS_ON()
  DCHECK(result) << "Had AXID for accessible_node but no entry in objects_";
  DCHECK(IsA<AXVirtualObject>(result));
  // Do not allow detached objects except when disposing entire tree.
  DCHECK(!result->IsDetached() || has_been_disposed_)
      << "Detached AXVirtualObject in map: "
      << "AXID#" << ax_id << " Node=" << accessible_node->element();
#endif
  return result;
}

AXObject* AXObjectCacheImpl::GetAXImageForMap(HTMLMapElement& map) {
  // Find first child node of <map> that has an AXObject and return it's
  // parent, which should be a native image.
  Node* child = LayoutTreeBuilderTraversal::FirstChild(map);
  while (child) {
    if (AXObject* ax_child = Get(child)) {
      if (AXObject* ax_image = ax_child->CachedParentObject()) {
        DCHECK(!ax_image->IsDetached());
        DCHECK(IsA<HTMLImageElement>(ax_image->GetNode()))
            << "Expected image AX parent of <map>'s DOM child, got: "
            << ax_image->GetNode() << "\n* Map's DOM child was: " << child
            << "\n* ax_image: " << ax_image->ToString(true, true);
        return ax_image;
      }
    }
    child = LayoutTreeBuilderTraversal::NextSibling(*child);
  }
  return nullptr;
}

AXObject* AXObjectCacheImpl::CreateFromRenderer(LayoutObject* layout_object) {
  // FIXME: How could layoutObject->node() ever not be an Element?
  Node* node = layout_object->GetNode();

  // media element
  if (node && node->IsMediaElement())
    return AccessibilityMediaElement::Create(layout_object, *this);

  if (node && node->IsMediaControlElement())
    return AccessibilityMediaControl::Create(layout_object, *this);

  if (IsA<HTMLOptionElement>(node))
    return MakeGarbageCollected<AXListBoxOption>(layout_object, *this);

  if (auto* html_input_element = DynamicTo<HTMLInputElement>(node)) {
    const AtomicString& type = html_input_element->type();
    if (type == input_type_names::kRange)
      return MakeGarbageCollected<AXSlider>(layout_object, *this);
  }

  if (auto* select_element = DynamicTo<HTMLSelectElement>(node)) {
    if (select_element->UsesMenuList()) {
      if (use_ax_menu_list_) {
        DCHECK(ShouldCreateAXMenuListFor(layout_object));
        return MakeGarbageCollected<AXMenuList>(layout_object, *this);
      }
    } else {
      return MakeGarbageCollected<AXListBox>(layout_object, *this);
    }
  }

  // progress bar
  if (layout_object->IsProgress()) {
    return MakeGarbageCollected<AXProgressIndicator>(
        To<LayoutProgress>(layout_object), *this);
  }

  return MakeGarbageCollected<AXLayoutObject>(layout_object, *this);
}

// Returns true if |node| is an <option> element and its parent <select>
// is a menu list (not a list box).
// static
bool AXObjectCacheImpl::ShouldCreateAXMenuListOptionFor(const Node* node) {
  auto* option_element = DynamicTo<HTMLOptionElement>(node);
  if (!option_element)
    return false;

  if (auto* select = option_element->OwnerSelectElement())
    return ShouldCreateAXMenuListFor(select->GetLayoutObject());

  return false;
}

// static
bool AXObjectCacheImpl::ShouldCreateAXMenuListFor(LayoutObject* layout_object) {
  if (!layout_object)
    return false;

  if (!AXObjectCacheImpl::UseAXMenuList())
    return false;

  if (auto* select = DynamicTo<HTMLSelectElement>(layout_object->GetNode()))
    return select->UsesMenuList();

  return false;
}

// static
bool AXObjectCacheImpl::IsRelevantSlotElement(const HTMLSlotElement& slot) {
  // A <slot> descendant of a node that is still in the DOM but no longer
  // rendered will return true for Node::isConnected() and false for
  // AXObject::IsDetached(). But from the perspective of platform ATs, this
  // subtree is not connected and is detached unless it is canvas fallback
  // content. In order to detect this condition, we look to the first non-slot
  // parent. If it has a layout object, the <slot>'s contents are rendered.
  // If it doesn't, but it's in the canvas subtree, those contents should be
  // treated as canvas fallback content.
  //
  // The alternative way to determine whether the <slot> is still relevant for
  // rendering is to iterate FlatTreeTraversal::Parent until you get to the last
  // parent, and see if it's a document. If it is not a document, then it is not
  // relevant. This seems much slower than just checking GetLayoutObject() as it
  // needs to iterate the parent chain. However, checking GetLayoutObject()
  // could produce null in the case of something that is
  // content-visibility:auto. This means that any slotted content inside
  // content-visibility:auto may be removed from the AX tree depending on
  // whether it was recently rendered.
  //
  // TODO(accessibility) This fails for the web test
  // detach-locked-slot-children-crash.html with --force-renderer-accessibility.
  // See web_tests/FlagExpectations/force-renderer-accessibility.
  // There should be a better way to accomplish this.
  // Could a new function be added to the slot element?
  const Node* parent = LayoutTreeBuilderTraversal::Parent(slot);
  if (const HTMLSlotElement* parent_slot =
          ToHTMLSlotElementIfSupportsAssignmentOrNull(parent)) {
    return AXObjectCacheImpl::IsRelevantSlotElement(*parent_slot);
  }

  if (parent && parent->GetLayoutObject())
    return true;

  const Element* parent_element = DynamicTo<Element>(parent);
  if (!parent_element)
    return false;

  // Authors can include elements as "Fallback content" inside a <canvas> in
  // order to provide an alternative means to interact with the canvas using
  // a screen reader. Those should always be included.
  if (parent_element->IsInCanvasSubtree())
    return true;

  // LayoutObject::CreateObject() will not create an object for elements
  // with display:contents. If we do not include a <slot> for that reason,
  // any descendants will be not be included in the accessibility tree.
  return parent_element->HasDisplayContentsStyle();
}

// static
bool AXObjectCacheImpl::IsRelevantPseudoElement(const Node& node) {
  DCHECK(node.IsPseudoElement());
  if (!node.GetLayoutObject())
    return false;

  // ::before, ::after and ::marker are relevant.
  // Allowing these pseudo elements ensures that all visible descendant
  // pseudo content will be reached, despite only being able to walk layout
  // inside of pseudo content.
  // However, AXObjects aren't created for ::first-letter subtrees. The text
  // of ::first-letter is already available in the child text node of the
  // element that the CSS ::first letter applied to.
  if (node.IsMarkerPseudoElement() || node.IsBeforePseudoElement() ||
      node.IsAfterPseudoElement()) {
    // Ignore non-inline whitespace content, which is used by many pages as
    // a "Micro Clearfix Hack" to clear floats without extra HTML tags. See
    // http://nicolasgallagher.com/micro-clearfix-hack/
    if (node.GetLayoutObject()->IsInline())
      return true;  // Inline: not a clearfix hack.
    if (!node.parentNode()->GetLayoutObject() ||
        node.parentNode()->GetLayoutObject()->IsInline()) {
      return true;  // Parent inline: not a clearfix hack.
    }
    const ComputedStyle* style = node.GetLayoutObject()->Style();
    DCHECK(style);
    ContentData* content_data = style->GetContentData();
    if (!content_data)
      return true;
    if (!content_data->IsText())
      return true;  // Not text: not a clearfix hack.
    if (!To<TextContentData>(content_data)
             ->GetText()
             .ContainsOnlyWhitespaceOrEmpty()) {
      return true;  // Not whitespace: not a clearfix hack.
    }
    return false;  // Is the clearfix hack: ignore pseudo element.
  }

  // ::first-letter is relevant if and only if its parent layout object is a
  // relevant pseudo element. If it's not a pseudo element, then this the
  // ::first-letter text would end up being repeated in the AX Tree.
  if (node.IsFirstLetterPseudoElement()) {
    LayoutObject* layout_parent = node.GetLayoutObject()->Parent();
    DCHECK(layout_parent);
    Node* layout_parent_node = layout_parent->GetNode();
    return layout_parent_node && layout_parent_node->IsPseudoElement() &&
           IsRelevantPseudoElement(*layout_parent_node);
  }

  // The remaining possible pseudo element types are not relevant.
  if (node.IsBackdropPseudoElement() ||
      node.IsDocumentTransitionPseudoElement()) {
    return false;
  }

  // If this is reached, then a new pseudo element type was added and is not
  // yet handled by accessibility. See  PseudoElementTagName() in
  // pseudo_element.cc for all possible types.
  SANITIZER_NOTREACHED() << "Unhandled type of pseudo element on: " << node;
  return false;
}

// static
bool AXObjectCacheImpl::IsRelevantPseudoElementDescendant(
    const LayoutObject& layout_object) {
  if (layout_object.IsText() && To<LayoutText>(layout_object).HasEmptyText())
    return false;
  const LayoutObject* ancestor = &layout_object;
  while (true) {
    ancestor = ancestor->Parent();
    if (!ancestor)
      return false;
    if (ancestor->IsPseudoElement()) {
      // When an ancestor is exposed using CSS alt text, descendants are pruned.
      if (AXNodeObject::GetCSSAltText(ancestor->GetNode()))
        return false;
      return IsRelevantPseudoElement(*ancestor->GetNode());
    }
    if (!ancestor->IsAnonymous())
      return false;
  }
}

AXObject* AXObjectCacheImpl::CreateFromNode(Node* node) {
  if (ShouldCreateAXMenuListOptionFor(node)) {
    return MakeGarbageCollected<AXMenuListOption>(To<HTMLOptionElement>(node),
                                                  *this);
  }

  if (auto* area = DynamicTo<HTMLAreaElement>(node))
    return MakeGarbageCollected<AXImageMapLink>(area, *this);

  return MakeGarbageCollected<AXNodeObject>(node, *this);
}

AXObject* AXObjectCacheImpl::CreateFromInlineTextBox(
    AbstractInlineTextBox* inline_text_box) {
  return MakeGarbageCollected<AXInlineTextBox>(inline_text_box, *this);
}

AXObject* AXObjectCacheImpl::GetOrCreate(AccessibleNode* accessible_node,
                                         AXObject* parent) {
  if (AXObject* obj = Get(accessible_node))
    return obj;

  DCHECK_EQ(accessible_node->GetDocument(), &GetDocument());

  DCHECK(parent)
      << "A virtual object must have a parent, and cannot exist without one. "
         "The parent is set when the object is constructed.";

  DCHECK(!accessible_node->element())
      << "The accessible node directly attached to an element should not "
         "have its own AXObject, since the AXObject will be keyed off of the "
         "element instead: "
      << accessible_node->element();

  if (!parent->CanHaveChildren())
    return nullptr;

  AXObject* new_obj =
      MakeGarbageCollected<AXVirtualObject>(*this, accessible_node);
  const AXID ax_id = AssociateAXID(new_obj);
  accessible_node_mapping_.Set(accessible_node, ax_id);
  new_obj->Init(parent);
  return new_obj;
}

AXObject* AXObjectCacheImpl::GetOrCreate(const Node* node) {
  return GetOrCreate(node, nullptr);
}

AXObject* AXObjectCacheImpl::GetOrCreate(Node* node) {
  return GetOrCreate(node, nullptr);
}

AXObject* AXObjectCacheImpl::GetOrCreate(const Node* node,
                                         AXObject* parent_if_known) {
  return GetOrCreate(const_cast<Node*>(node), parent_if_known);
}

AXObject* AXObjectCacheImpl::GetOrCreate(Node* node,
                                         AXObject* parent_if_known) {
  if (!node)
    return nullptr;

  if (AXObject* obj = Get(node))
    return obj;

  return CreateAndInit(node, parent_if_known);
}

AXObject* AXObjectCacheImpl::CreateAndInit(Node* node,
                                           AXObject* parent_if_known,
                                           AXID use_axid) {
  DCHECK(node);
  DCHECK(!parent_if_known || parent_if_known->CanHaveChildren());

  // If the node has a layout object, prefer using that as the primary key for
  // the AXObject, with the exception of the HTMLAreaElement and nodes within
  // a locked subtree, which are created based on its node.
  LayoutObject* layout_object = node->GetLayoutObject();
  if (layout_object && IsLayoutObjectRelevantForAccessibility(*layout_object) &&
      !IsDisplayLocked(layout_object)) {
    return CreateAndInit(layout_object, parent_if_known, use_axid);
  }

  if (!IsNodeRelevantForAccessibility(node, parent_if_known, false))
    return nullptr;

#if DCHECK_IS_ON()
  DCHECK(node->isConnected());
  DCHECK(node->IsElementNode() || node->IsTextNode() || node->IsDocumentNode());
  Document* document = &node->GetDocument();
  DCHECK(document);
  DCHECK(document->Lifecycle().GetState() >=
         DocumentLifecycle::kAfterPerformLayout)
      << "Unclean document at lifecycle " << document->Lifecycle().ToString();
  DCHECK_NE(node, document_)
      << "The document's AXObject is backed by its layout object.";
#endif  // DCHECK_IS_ON()

  // Return null if inside a shadow tree of something that can't have children,
  // for example, an <img> has a user agent shadow root containing a <span> for
  // the alt text. Do not create an accessible for that as it would be unable
  // to have a parent that has it as a child.
  if (node->IsInShadowTree()) {
    AXObject* shadow_host = Get(node->OwnerShadowHost());
    if (shadow_host && !shadow_host->CanHaveChildren())
      return nullptr;
  }

#if DCHECK_IS_ON()
  if (!IsA<HTMLOptionElement>(node) && node->IsInUserAgentShadowRoot()) {
    if (Node* owner_shadow_host = node->OwnerShadowHost()) {
      DCHECK(!AXObjectCacheImpl::ShouldCreateAXMenuListFor(
          owner_shadow_host->GetLayoutObject()))
          << "DOM descendants of an AXMenuList should not be added to the AX "
             "hierarchy, except for the AXMenuListOption children added in "
             "AXMenuListPopup. An attempt was made to create an AXObject for: "
          << node;
    }
  }
#endif

  AXObject* parent = parent_if_known
                         ? parent_if_known
                         : AXObject::ComputeNonARIAParent(*this, node);
  // An AXObject backed only by a DOM node must have a parent, because it's
  // never the root, which will always have a layout object.
  if (!parent)
    return nullptr;

  DCHECK(parent->CanHaveChildren());

  // One of the above calls could have already created the planned object via a
  // recursive call to GetOrCreate(). If so, just return that object.
  if (node_object_mapping_.Contains(node))
    return Get(node);

  AXObject* new_obj = CreateFromNode(node);

  // Will crash later if we have two objects for the same node.
  DCHECK(!node_object_mapping_.Contains(node))
      << "Already have an AXObject for " << node;

  const AXID ax_id = AssociateAXID(new_obj, use_axid);
  DCHECK(!HashTraits<AXID>::IsDeletedValue(ax_id));
  node_object_mapping_.Set(node, ax_id);
  new_obj->Init(parent);

  return new_obj;
}

AXObject* AXObjectCacheImpl::GetOrCreate(LayoutObject* layout_object) {
  return GetOrCreate(layout_object, nullptr);
}

AXObject* AXObjectCacheImpl::GetOrCreate(LayoutObject* layout_object,
                                         AXObject* parent_if_known) {
  if (!layout_object)
    return nullptr;

  if (AXObject* obj = Get(layout_object))
    return obj;

  return CreateAndInit(layout_object, parent_if_known);
}

AXObject* AXObjectCacheImpl::CreateAndInit(LayoutObject* layout_object,
                                           AXObject* parent_if_known,
                                           AXID use_axid) {
#if DCHECK_IS_ON()
  DCHECK(layout_object);
  Document* document = &layout_object->GetDocument();
  DCHECK(document);
  DCHECK(document->Lifecycle().GetState() >=
         DocumentLifecycle::kAfterPerformLayout)
      << "Unclean document at lifecycle " << document->Lifecycle().ToString();
  DCHECK(!parent_if_known || parent_if_known->CanHaveChildren());
#endif  // DCHECK_IS_ON()
  if (!IsLayoutObjectRelevantForAccessibility(*layout_object))
    return nullptr;

  Node* node = layout_object->GetNode();

  if (node && !IsNodeRelevantForAccessibility(node, parent_if_known, true))
    return nullptr;

  // Return null if inside a shadow tree of something that can't have children,
  // for example, an <img> has a user agent shadow root containing a <span> for
  // the alt text. Do not create an accessible for that as it would be unable
  // to have a parent that has it as a child.
  if (node && node->IsInShadowTree()) {
    AXObject* shadow_host = Get(node->OwnerShadowHost());
    if (shadow_host && !shadow_host->CanHaveChildren())
      return nullptr;
  }

#if DCHECK_IS_ON()
  if (node && !IsA<HTMLOptionElement>(node) &&
      node->IsInUserAgentShadowRoot()) {
    if (Node* owner_shadow_host = node->OwnerShadowHost()) {
      DCHECK(!AXObjectCacheImpl::ShouldCreateAXMenuListFor(
          owner_shadow_host->GetLayoutObject()))
          << "DOM descendants of an AXMenuList should not be added to the AX "
             "hierarchy, except for the AXMenuListOption children added in "
             "AXMenuListPopup. An attempt was made to create an AXObject for: "
          << node;
    }
  }
#endif

  // Prefer creating AXNodeObjects over AXLayoutObjects in locked subtrees
  // (e.g. content-visibility: auto), even if a LayoutObject is available,
  // because the LayoutObject is not guaranteed to be up-to-date (it might come
  // from a previous layout update), or even it is up-to-date, it may not remain
  // up-to-date. Blink doesn't update style/layout for nodes in locked
  // subtrees, so creating a matching AXLayoutObjects could lead to the use of
  // old information. Note that Blink will recreate the AX objects as
  // AXLayoutObjects when a locked element is activated, aka it becomes visible.
  // Visit https://wicg.github.io/display-locking/#accessibility for more info.
  if (IsDisplayLocked(layout_object)) {
    if (!node) {
      // Nodeless objects such as anonymous blocks do not get accessible objects
      // in a locked subtree. Anonymous blocks are added to help layout when
      // a block and inline are siblings.
      // This prevents an odd mixture of ax objects in a locked subtree, e.g.
      // AXNodeObjects when there is a node, and AXLayoutObjects
      // when there isn't. The locked subtree should not have AXLayoutObjects.
      return nullptr;
    }
    return CreateAndInit(node, parent_if_known, use_axid);
  }

  AXObject* parent = parent_if_known ? parent_if_known
                                     : AXObject::ComputeNonARIAParent(
                                           *this, node, layout_object);
  if (node == document_)
    DCHECK(!parent);
  else if (!parent)
    return nullptr;
  else
    DCHECK(parent->CanHaveChildren());

  // One of the above calls could have already created the planned object via a
  // recursive call to GetOrCreate(). If so, just return that object.
  // Example: parent calls Init() => ComputeAccessibilityIsIgnored() =>
  // CanSetFocusAttribute() => CanBeActiveDescendant() =>
  // IsARIAControlledByTextboxWithActiveDescendant() => GetOrCreate().
  if (layout_object_mapping_.Contains(layout_object)) {
    AXObject* result = Get(layout_object);
    DCHECK(result) << "Missing cached AXObject for " << layout_object;
    return result;
  }

  AXObject* new_obj = CreateFromRenderer(layout_object);

  DCHECK(new_obj) << "Could not create AXObject for " << layout_object;

  // Will crash later if we have two objects for the same layoutObject.
  DCHECK(!layout_object_mapping_.Contains(layout_object))
      << "Already have an AXObject for " << layout_object;

  const AXID axid = AssociateAXID(new_obj, use_axid);
  layout_object_mapping_.Set(layout_object, axid);
  new_obj->Init(parent);

  return new_obj;
}

AXObject* AXObjectCacheImpl::GetOrCreate(AbstractInlineTextBox* inline_text_box,
                                         AXObject* parent) {
  if (!inline_text_box)
    return nullptr;

  if (!parent) {
    LayoutObject* layout_text_parent = inline_text_box->GetLayoutObject();
    DCHECK(layout_text_parent);
    DCHECK(layout_text_parent->IsText());
    parent = GetOrCreate(layout_text_parent);
    if (!parent) {
      DCHECK(inline_text_box->GetText().ContainsOnlyWhitespaceOrEmpty() ||
             !IsRelevantPseudoElementDescendant(*layout_text_parent))
          << "No parent for non-whitespace inline textbox: "
          << layout_text_parent
          << "\nParent of parent: " << layout_text_parent->Parent();
      return nullptr;
    }
  }

  if (AXObject* obj = Get(inline_text_box)) {
#if DCHECK_IS_ON()
    DCHECK(!obj->IsDetached())
        << "AXObject for inline text box should not be detached: "
        << obj->ToString(true, true);
    // AXInlineTextbox objects can't get a new parent, unlike other types of
    // accessible objects that can get a new parent because they moved or
    // because of aria-owns.
    // AXInlineTextbox objects are only added via AddChildren() on static text
    // or line break parents. The children are cleared, and detached from their
    // parent before AddChildren() executes. There should be no previous parent.
    DCHECK(parent->RoleValue() == ax::mojom::blink::Role::kStaticText ||
           parent->RoleValue() == ax::mojom::blink::Role::kLineBreak);
    DCHECK(!obj->CachedParentObject() || obj->CachedParentObject() == parent);
#endif
    obj->SetParent(parent);
    return obj;
  }

  AXObject* new_obj = CreateFromInlineTextBox(inline_text_box);

  const AXID axid = AssociateAXID(new_obj);

  inline_text_box_object_mapping_.Set(inline_text_box, axid);
  new_obj->Init(parent);
  return new_obj;
}

AXObject* AXObjectCacheImpl::CreateAndInit(ax::mojom::blink::Role role,
                                           AXObject* parent) {
  DCHECK(parent);
  DCHECK(parent->CanHaveChildren());
  AXObject* obj = nullptr;

  switch (role) {
    case ax::mojom::blink::Role::kMenuListPopup:
      DCHECK(use_ax_menu_list_);
      obj = MakeGarbageCollected<AXMenuListPopup>(*this);
      break;
    default:
      obj = nullptr;
  }

  if (!obj)
    return nullptr;

  AssociateAXID(obj);

  obj->Init(parent);
  return obj;
}

// Do not pass a depth if the entire subtree of AXObjects should be removed.
void AXObjectCacheImpl::RemoveAXObjectsInLayoutSubtree(AXObject* subtree,
                                                       int depth = -999) {
  if (!subtree || depth == 0)
    return;

  depth--;

  LayoutObject* layout_object = subtree->GetLayoutObject();
  if (layout_object) {
    LayoutObject* layout_child = layout_object->SlowFirstChild();
    while (layout_child) {
      RemoveAXObjectsInLayoutSubtree(Get(layout_child), depth);
      layout_child = layout_child->NextSibling();
    }
  }

  Remove(subtree);
}

void AXObjectCacheImpl::Remove(AXObject* object) {
  DCHECK(object);
  if (object->GetNode())
    Remove(object->GetNode());
  else if (object->GetLayoutObject())
    Remove(object->GetLayoutObject());
  else if (object->GetAccessibleNode())
    Remove(object->GetAccessibleNode());
  else
    Remove(object->AXObjectID());
}

// This is safe to call even if there isn't a current mapping.
// This is called by other Remove() methods, called by Blink for DOM and layout
// changes, iterating over all removed content in the subtree:
// - When a DOM subtree is removed, it is called with the root node first, and
//   then descending down into the subtree.
// - When layout for a subtree is detached, it is called on layout objects,
//   starting with leaves and moving upward, ending with the subtree root.
void AXObjectCacheImpl::Remove(AXID ax_id) {
  if (!ax_id)
    return;

  // First, fetch object to operate some cleanup functions on it.
  auto it = objects_.find(ax_id);
  AXObject* obj = it != objects_.end() ? it->value : nullptr;
  if (!obj)
    return;

  ChildrenChangedOnAncestorOf(obj);
  obj->Detach();

  RemoveAXID(obj);

  // Finally, remove the object.
  // TODO(accessibility) We don't use the return value, can we use .erase()
  // and it will still make sure that the object is cleaned up?
  if (!objects_.Take(ax_id))
    return;

  DCHECK_EQ(objects_.size(), ids_in_use_.size());
}

// This is safe to call even if there isn't a current mapping.
void AXObjectCacheImpl::Remove(AccessibleNode* accessible_node) {
  DCHECK(accessible_node);

  auto iter = accessible_node_mapping_.find(accessible_node);
  if (iter == accessible_node_mapping_.end())
    return;

  AXID ax_id = iter->value;
  accessible_node_mapping_.erase(iter);

  Remove(ax_id);
}

// This is safe to call even if there isn't a current mapping.
bool AXObjectCacheImpl::Remove(LayoutObject* layout_object) {
  if (!layout_object)
    return false;

  auto iter = layout_object_mapping_.find(layout_object);
  if (iter == layout_object_mapping_.end())
    return false;

  AXID ax_id = iter->value;
  DCHECK(ax_id);

  layout_object_mapping_.erase(iter);
  Remove(ax_id);

  return true;
}

// This is safe to call even if there isn't a current mapping.
void AXObjectCacheImpl::Remove(Node* node) {
  if (!node)
    return;

  LayoutObject* layout_object = node->GetLayoutObject();

  // A layout object will be used whenever it is available and relevant. It's
  // the preferred backing object, rather than the DOM node.
  if (Remove(node->GetLayoutObject())) {
    DCHECK_EQ(node_object_mapping_.find(node), node_object_mapping_.end())
        << "AXObject cannot be backed by both a layout object and node.";
    return;
  }

  auto iter = node_object_mapping_.find(node);
  if (iter != node_object_mapping_.end()) {
    DCHECK(!layout_object || layout_object_mapping_.find(layout_object) ==
                                 layout_object_mapping_.end())
        << "AXObject cannot be backed by both a layout object and node.";
    AXID ax_id = iter->value;
    DCHECK(ax_id);
    node_object_mapping_.erase(iter);
    Remove(ax_id);
  }
}

// This is safe to call even if there isn't a current mapping.
void AXObjectCacheImpl::Remove(AbstractInlineTextBox* inline_text_box) {
  if (!inline_text_box)
    return;

  auto iter = inline_text_box_object_mapping_.find(inline_text_box);
  if (iter == inline_text_box_object_mapping_.end())
    return;

  AXID ax_id = iter->value;
  inline_text_box_object_mapping_.erase(iter);

  Remove(ax_id);
}

AXID AXObjectCacheImpl::GenerateAXID() const {
  static AXID last_used_id = 0;

  // Generate a new ID.
  AXID obj_id = last_used_id;
  do {
    ++obj_id;
  } while (!obj_id || HashTraits<AXID>::IsDeletedValue(obj_id) ||
           ids_in_use_.Contains(obj_id));

  last_used_id = obj_id;

  return obj_id;
}

void AXObjectCacheImpl::AddToFixedOrStickyNodeList(const AXObject* object) {
  DCHECK(object);
  DCHECK(!object->IsDetached());
  fixed_or_sticky_node_ids_.insert(object->AXObjectID());
}

AXID AXObjectCacheImpl::AssociateAXID(AXObject* obj, AXID use_axid) {
  // Check for already-assigned ID.
  DCHECK(!obj->AXObjectID()) << "Object should not already have an AXID";

  const AXID new_axid = use_axid ? use_axid : GenerateAXID();

  ids_in_use_.insert(new_axid);
  obj->SetAXObjectID(new_axid);
  objects_.Set(new_axid, obj);

  return new_axid;
}

void AXObjectCacheImpl::RemoveAXID(AXObject* object) {
  if (!object)
    return;

  fixed_or_sticky_node_ids_.clear();

  if (active_aria_modal_dialog_ == object)
    active_aria_modal_dialog_ = nullptr;

  AXID obj_id = object->AXObjectID();
  if (!obj_id)
    return;
  DCHECK(!HashTraits<AXID>::IsDeletedValue(obj_id));
  DCHECK(ids_in_use_.Contains(obj_id));
  object->SetAXObjectID(0);
  ids_in_use_.erase(obj_id);
  autofill_state_map_.erase(obj_id);

  relation_cache_->RemoveAXID(obj_id);
}

AXObject* AXObjectCacheImpl::NearestExistingAncestor(Node* node) {
  // Find the nearest ancestor that already has an accessibility object, since
  // we might be in the middle of a layout.
  while (node) {
    if (AXObject* obj = Get(node))
      return obj;
    node = node->parentNode();
  }
  return nullptr;
}

void AXObjectCacheImpl::UpdateNumTreeUpdatesQueuedBeforeLayoutHistogram() {
  UMA_HISTOGRAM_COUNTS_100000(
      "Blink.Accessibility.NumTreeUpdatesQueuedBeforeLayout",
      tree_update_callback_queue_main_.size() +
          tree_update_callback_queue_popup_.size());
}

void AXObjectCacheImpl::InvalidateBoundingBoxForFixedOrStickyPosition() {
  for (AXID id : fixed_or_sticky_node_ids_)
    changed_bounds_ids_.insert(id);
}

void AXObjectCacheImpl::DeferTreeUpdateInternal(base::OnceClosure callback,
                                                AXObject* obj) {
  // Called for updates that do not have a DOM node, e.g. a children or text
  // changed event that occurs on an anonymous layout block flow.
  DCHECK(obj);

  if (!IsActive(GetDocument()) || tree_updates_paused_)
    return;

  if (obj->IsDetached())
    return;

  Document* tree_update_document = obj->GetDocument();

  // Ensure the tree update document is in a good state.
  if (!tree_update_document || !IsActive(*tree_update_document))
    return;

  TreeUpdateCallbackQueue& queue =
      GetTreeUpdateCallbackQueue(*tree_update_document);

  if (queue.size() >= max_pending_updates_) {
    UpdateNumTreeUpdatesQueuedBeforeLayoutHistogram();

    tree_updates_paused_ = true;
    LOG(INFO) << "Accessibility tree update queue is too big, updates have "
                 "been paused";
    queue.clear();
    return;
  }

#if DCHECK_IS_ON()
  // TODO(accessibility) Restore this check. Currently must be removed because a
  // loop in ProcessDeferredAccessibilityEvents() is allowed to queue deferred
  // ChildrenChanged() events and process them.
  // DCHECK(!tree_update_document->GetPage()->Animator().IsServicingAnimations()
  // ||
  //        (tree_update_document->Lifecycle().GetState() <
  //             DocumentLifecycle::kInAccessibility ||
  //         tree_update_document->Lifecycle().StateAllowsDetach()))
  //     << "DeferTreeUpdateInternal should only be outside of the lifecycle or
  //     "
  //        "before the accessibility state:"
  //     << "\n* IsServicingAnimations: "
  //     << tree_update_document->GetPage()->Animator().IsServicingAnimations()
  //     << "\n* Lifecycle: " << tree_update_document->Lifecycle().ToString();
#endif

  queue.push_back(MakeGarbageCollected<TreeUpdateParams>(
      obj->GetNode(), obj->AXObjectID(), ComputeEventFrom(),
      active_event_from_action_, ActiveEventIntents(), std::move(callback)));

  // These events are fired during DocumentLifecycle::kInAccessibility,
  // ensure there is a document lifecycle update scheduled.
  ScheduleVisualUpdate(*tree_update_document);
}

void AXObjectCacheImpl::DeferTreeUpdateInternal(base::OnceClosure callback,
                                                const Node* node) {
  DCHECK(node);

  if (!IsActive(GetDocument()) || tree_updates_paused_)
    return;

  Document& tree_update_document = node->GetDocument();

  // Ensure the tree update document is in a good state.
  if (!IsActive(tree_update_document))
    return;

  TreeUpdateCallbackQueue& queue =
      GetTreeUpdateCallbackQueue(tree_update_document);

  if (queue.size() >= max_pending_updates_) {
    UpdateNumTreeUpdatesQueuedBeforeLayoutHistogram();

    tree_updates_paused_ = true;
    LOG(INFO) << "Accessibility tree update queue is too big, updates have "
                 "been paused";
    queue.clear();
    return;
  }

#if DCHECK_IS_ON()
  // TODO(accessibility) Consider re-adding. However, it conflicts with some
  // calls from HandleTextMarkerDataAdded(), which need to defer even when
  // already in clean layout. Removing this is not dangerous -- it helped ensure
  // that we weren't bothering to defer when layout is already clean. It's
  // actually ok if that's wrong here or there.
  // DCHECK(!tree_update_document.GetPage()->Animator().IsServicingAnimations()
  // ||
  //        (tree_update_document.Lifecycle().GetState() <
  //             DocumentLifecycle::kInAccessibility ||
  //         tree_update_document.Lifecycle().StateAllowsDetach()))
  //     << "DeferTreeUpdateInternal should only be outside of the lifecycle or
  //     "
  //        "before the accessibility state:"
  //     << "\n* IsServicingAnimations: "
  //     << tree_update_document.GetPage()->Animator().IsServicingAnimations()
  //     << "\n* Lifecycle: " << tree_update_document.Lifecycle().ToString();
#endif

  queue.push_back(MakeGarbageCollected<TreeUpdateParams>(
      node, 0, ComputeEventFrom(), active_event_from_action_,
      ActiveEventIntents(), std::move(callback)));

  // These events are fired during DocumentLifecycle::kInAccessibility,
  // ensure there is a document lifecycle update scheduled.
  ScheduleVisualUpdate(tree_update_document);
}

void AXObjectCacheImpl::DeferTreeUpdate(
    void (AXObjectCacheImpl::*method)(const Node*),
    const Node* node) {
  base::OnceClosure callback =
      WTF::Bind(method, WrapWeakPersistent(this), WrapWeakPersistent(node));
  DeferTreeUpdateInternal(std::move(callback), node);
}

void AXObjectCacheImpl::DeferTreeUpdate(
    void (AXObjectCacheImpl::*method)(Node*),
    Node* node) {
  base::OnceClosure callback =
      WTF::Bind(method, WrapWeakPersistent(this), WrapWeakPersistent(node));
  DeferTreeUpdateInternal(std::move(callback), node);
}

void AXObjectCacheImpl::DeferTreeUpdate(
    void (AXObjectCacheImpl::*method)(Node* node,
                                      ax::mojom::blink::Event event),
    Node* node,
    ax::mojom::blink::Event event) {
  base::OnceClosure callback = WTF::Bind(method, WrapWeakPersistent(this),
                                         WrapWeakPersistent(node), event);
  DeferTreeUpdateInternal(std::move(callback), node);
}

void AXObjectCacheImpl::DeferTreeUpdate(
    void (AXObjectCacheImpl::*method)(const QualifiedName&, Element* element),
    const QualifiedName& attr_name,
    Element* element) {
  base::OnceClosure callback = WTF::Bind(
      method, WrapWeakPersistent(this), attr_name, WrapWeakPersistent(element));
  DeferTreeUpdateInternal(std::move(callback), element);
}

void AXObjectCacheImpl::DeferTreeUpdate(
    void (AXObjectCacheImpl::*method)(Node*, AXObject*),
    AXObject* obj) {
  Node* node = obj ? obj->GetNode() : nullptr;
  base::OnceClosure callback =
      WTF::Bind(method, WrapWeakPersistent(this), WrapWeakPersistent(node),
                WrapWeakPersistent(obj));
  if (obj) {
    DeferTreeUpdateInternal(std::move(callback), obj);
  } else {
    DeferTreeUpdateInternal(std::move(callback), node);
  }
}

void AXObjectCacheImpl::SelectionChanged(Node* node) {
  if (!node)
    return;

  Settings* settings = GetSettings();
  if (settings && settings->GetAriaModalPrunesAXTree())
    UpdateActiveAriaModalDialog(node);

  DeferTreeUpdate(&AXObjectCacheImpl::PostNotification, &GetDocument(),
                  ax::mojom::blink::Event::kDocumentSelectionChanged);

  // If there is a text control, mark it dirty to serialize
  // IntAttribute::kTextSelStart/kTextSelEnd changes.
  // TODO(accessibility) Remove once we remove kTextSelStart/kTextSelEnd.
  if (TextControlElement* text_control = EnclosingTextControl(node))
    MarkElementDirty(text_control);
}

void AXObjectCacheImpl::UpdateReverseTextRelations(
    const AXObject* relation_source,
    const Vector<String>& target_ids) {
  relation_cache_->UpdateReverseTextRelations(relation_source, target_ids);
}

void AXObjectCacheImpl::StyleChanged(const LayoutObject* layout_object) {
  DCHECK(layout_object);
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(layout_object->GetDocument());
  Node* node = GetClosestNodeForLayoutObject(layout_object);
  if (node)
    DeferTreeUpdate(&AXObjectCacheImpl::StyleChangedWithCleanLayout, node);
}

void AXObjectCacheImpl::StyleChangedWithCleanLayout(Node* node) {
  DCHECK(node);
  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));

  // There is a ton of style change notifications coming from newly-opened
  // calendar popups for pickers. Solving that problem is what inspired the
  // approach below, which is likely true for all elements.
  //
  // If we don't know about an object, then its style did not change as far as
  // we (and ATs) are concerned. For this reason, don't call GetOrCreate.
  AXObject* obj = Get(node);
  if (!obj)
    return;

  DCHECK(!obj->IsDetached());

  // If the foreground or background color on an item inside a container which
  // supports selection changes, it can be the result of the selection changing
  // as well as the container losing focus. We handle these notifications via
  // their state changes, so no need to mark them dirty here.
  AXObject* parent = obj->CachedParentObject();
  if (parent && ui::IsContainerWithSelectableChildren(parent->RoleValue()))
    return;

  MarkAXObjectDirtyWithCleanLayout(obj);
}

void AXObjectCacheImpl::TextChanged(Node* node) {
  if (!node)
    return;

  // A text changed event is redundant with children changed on the same node.
  if (nodes_with_pending_children_changed_.find(node) !=
      nodes_with_pending_children_changed_.end()) {
    return;
  }

  DeferTreeUpdate(&AXObjectCacheImpl::TextChangedWithCleanLayout, node);
}

void AXObjectCacheImpl::TextChanged(const LayoutObject* layout_object) {
  if (!layout_object)
    return;

  // The node may be null when the text changes on an anonymous layout object,
  // such as a layout block flow that is inserted to parent an inline object
  // when it has a block sibling.
  Node* node = GetClosestNodeForLayoutObject(layout_object);
  if (node) {
    // A text changed event is redundant with children changed on the same node.
    if (nodes_with_pending_children_changed_.find(node) !=
        nodes_with_pending_children_changed_.end()) {
      return;
    }

    DeferTreeUpdate(&AXObjectCacheImpl::TextChangedWithCleanLayout, node);
    return;
  }

  if (Get(layout_object)) {
    DeferTreeUpdate(&AXObjectCacheImpl::TextChangedWithCleanLayout,
                    Get(layout_object));
  }
}

void AXObjectCacheImpl::TextChangedWithCleanLayout(
    Node* optional_node_for_relation_update,
    AXObject* obj) {
  if (obj ? obj->IsDetached() : !optional_node_for_relation_update)
    return;

#if DCHECK_IS_ON()
  Document* document = obj ? obj->GetDocument()
                           : &optional_node_for_relation_update->GetDocument();
  DCHECK(document->Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean)
      << "Unclean document at lifecycle " << document->Lifecycle().ToString();
#endif  // DCHECK_IS_ON()

  if (obj) {
    if (obj->RoleValue() == ax::mojom::blink::Role::kStaticText &&
        obj->LastKnownIsIncludedInTreeValue()) {
      Settings* settings = GetSettings();
      if (settings && settings->GetInlineTextBoxAccessibilityEnabled()) {
        // Update inline text box children.
        ChildrenChangedWithCleanLayout(optional_node_for_relation_update, obj);
        return;
      }
    }

    MarkAXObjectDirtyWithCleanLayout(obj);
  }

  if (optional_node_for_relation_update)
    relation_cache_->UpdateRelatedTree(optional_node_for_relation_update, obj);
}

void AXObjectCacheImpl::TextChangedWithCleanLayout(Node* node) {
  if (!node)
    return;

  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));
  TextChangedWithCleanLayout(node, GetOrCreate(node));
}

void AXObjectCacheImpl::FocusableChangedWithCleanLayout(Element* element) {
  DCHECK(element);
  DCHECK(!element->GetDocument().NeedsLayoutTreeUpdateForNode(*element));
  AXObject* obj = GetOrCreate(element);
  if (!obj)
    return;

  if (obj->IsAriaHidden()) {
    // Elements that are hidden but focusable are not ignored. Therefore, if a
    // hidden element's focusable state changes, it's ignored state must be
    // recomputed. It may be newly included in the tree, which means the
    // parents must be updated.
    // TODO(accessibility) Is this necessary? We have other places in the code
    // that automatically do a children changed on parents of nodes whose
    // ignored or included states change.
    ChildrenChangedWithCleanLayout(obj->CachedParentObject());
  }

  // Refresh the focusable state and State::kIgnored on the exposed object.
  MarkAXObjectDirtyWithCleanLayout(obj);
}

void AXObjectCacheImpl::DocumentTitleChanged() {
  DocumentLifecycle::DisallowTransitionScope disallow(document_->Lifecycle());

  AXObject* root = Get(document_);
  if (root)
    PostNotification(root, ax::mojom::blink::Event::kDocumentTitleChanged);
}

void AXObjectCacheImpl::UpdateCacheAfterNodeIsAttached(Node* node) {
  DCHECK(node);
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(node->GetDocument());
  DeferTreeUpdate(
      &AXObjectCacheImpl::UpdateCacheAfterNodeIsAttachedWithCleanLayout, node);
}

void AXObjectCacheImpl::UpdateCacheAfterNodeIsAttachedWithCleanLayout(
    Node* node) {
  if (!node || !node->isConnected())
    return;

  // Ignore attached nodes that are not elements, including text nodes and
  // #shadow-root nodes. This matches previous implementations that worked,
  // but it is not clear if that could potentially lead to missing content.
  Element* element = DynamicTo<Element>(node);
  if (!element)
    return;

  Document* document = &node->GetDocument();
  if (!document)
    return;

#if DCHECK_IS_ON()
  DCHECK(document->Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean)
      << "Unclean document at lifecycle " << document->Lifecycle().ToString();
#endif  // DCHECK_IS_ON()

  // Process any relation attributes that can affect ax objects already created.

  // Force computation of aria-owns, so that original parents that already
  // computed their children get the aria-owned children removed.
  if (AXObject::HasARIAOwns(element))
    HandleAttributeChangedWithCleanLayout(html_names::kAriaOwnsAttr, element);

  MaybeNewRelationTarget(*node, Get(node));

  // Even if the node or parent are ignored, an ancestor may need to include
  // descendants of the attached node, thus ChildrenChangedWithCleanLayout()
  // must be called. It handles ignored logic, ensuring that the first ancestor
  // that should have this as a child will be updated.
  ChildrenChangedWithCleanLayout(
      Get(LayoutTreeBuilderTraversal::Parent(*node)));

  // Once we have reached the threshhold number of roles that forces a data
  // table, invalidate the AXTable if it was previously a layout table, so that
  // its subtree recomputes roles.
  if (IsA<HTMLTableRowElement>(node)) {
    if (auto* table_element =
            Traversal<HTMLTableElement>::FirstAncestor(*node)) {
      if (table_element->rows()->length() >=
          AXObjectCacheImpl::kDataTableHeuristicMinRows) {
        if (AXObject* ax_table = Get(table_element)) {
          if (ax_table->RoleValue() == ax::mojom::blink::Role::kLayoutTable)
            HandleRoleChangeWithCleanLayout(table_element);
        }
      }
    }
  }
}

void AXObjectCacheImpl::DidInsertChildrenOfNode(Node* node) {
  // If a node is inserted that is a descendant of a leaf node in the
  // accessibility tree, notify the root of that subtree that its children have
  // changed.
  DCHECK(node);
  while (node) {
    if (SafeGet(node, true)) {
      TextChanged(node);
      return;
    }
    node = NodeTraversal::Parent(*node);
  }
}

// Note: do not call this when a child is becoming newly included, because
// it will return early if |obj| was last known to be unincluded.
void AXObjectCacheImpl::ChildrenChangedOnAncestorOf(AXObject* obj) {
  DCHECK(obj);
  DCHECK(!obj->IsDetached());
  DCHECK(!IsFrozen())
      << "Attempting to change children on an ancestor is dangerous during "
         "serialization, because the ancestor may have already been "
         "visited. Reaching this line indicates that AXObjectCacheImpl did "
         "not handle a signal and call ChilldrenChanged() earlier."
      << "\nChild: " << obj->ToString(true)
      << "\nParent: " << obj->CachedParentObject()->ToString(true);

  // If |obj| is not included, and it has no included descendants, then there is
  // nothing in any ancestor's cached children that needs clearing. This rule
  // improves performance when removing an entire subtree of unincluded nodes.
  // For example, if a <div id="root" style="display:none"> will be
  // included because it is a potential relation target. If unincluded
  // descendants change, no ChildrenChanged() processing is necessary, because
  // #root has no children.
  if (!obj->LastKnownIsIncludedInTreeValue() &&
      obj->CachedChildrenIncludingIgnored().IsEmpty()) {
    return;
  }

  // Clear children of ancestors in order to ensure this detached object is not
  // cached an ancestor's list of children:
  // Any ancestor up to the first included ancestor can contain the now-detached
  // child in it's cached children, and therefore must update children.
  ChildrenChanged(obj->CachedParentObject());
}

void AXObjectCacheImpl::ChildrenChangedWithCleanLayout(AXObject* obj) {
  if (AXObject* ax_ancestor_for_notification = InvalidateChildren(obj)) {
    ChildrenChangedWithCleanLayout(ax_ancestor_for_notification->GetNode(),
                                   ax_ancestor_for_notification);
  }
}

void AXObjectCacheImpl::ChildrenChanged(AXObject* obj) {
  if (AXObject* ax_ancestor_for_notification = InvalidateChildren(obj)) {
    DeferTreeUpdate(&AXObjectCacheImpl::ChildrenChangedWithCleanLayout,
                    ax_ancestor_for_notification);
  }
}

AXObject* AXObjectCacheImpl::InvalidateChildren(AXObject* obj) {
  if (!obj)
    return nullptr;

  // Clear children of ancestors in order to ensure this detached object is not
  // cached an ancestor's list of children:
  // Any ancestor up to the first included ancestor can contain the now-detached
  // child in it's cached children, and therefore must update children.
  AXObject* ancestor = obj;
  while (ancestor && !ancestor->LastKnownIsIncludedInTreeValue()) {
    if (ancestor->NeedsToUpdateChildren() || ancestor->IsDetached())
      return nullptr;  // Processing has already occurred for this ancestor.
    ancestor->SetNeedsToUpdateChildren();
    ancestor = ancestor->CachedParentObject();
  }

  // Only process ChildrenChanged() events on the included ancestor. This allows
  // deduping of ChildrenChanged() occurrences within the same subtree.
  // For example, if a subtree has unincluded children, but included
  // grandchildren have changed, only the root children changed needs to be
  // processed.
  if (!ancestor)
    return nullptr;
  // Don't enqueue a deferred event on the same node more than once.
  if (ancestor->GetNode() &&
      !nodes_with_pending_children_changed_.insert(ancestor->GetNode())
           .is_new_entry) {
    return nullptr;
  }

  // Return ancestor to fire children changed notification on.
  DCHECK(ancestor->LastKnownIsIncludedInTreeValue())
      << "ChildrenChanged() must only be called on included nodes: "
      << ancestor->ToString(true, true);

  return ancestor;
}

void AXObjectCacheImpl::SlotAssignmentWillChange(Node* node) {
  // Use SafeGet(), because right before slot assignment is a dangerous time to
  // test whether the slot must be invalidated, because this currently requires
  // looking at the <slot> children in
  // IsShadowContentRelevantForAccessibility(), resulting in an infinite loop
  // as looking at the children causes slot assignment to be recalculated.
  // TODO(accessibility) In the future this may be simplified.
  // See crbug.com/1219311.
  ChildrenChanged(SafeGet(node));
}

void AXObjectCacheImpl::ChildrenChanged(Node* node) {
  // Use SafeGet() because there is no guarantee that layout is clean right now.
  ChildrenChanged(SafeGet(node));
}

// ChildrenChanged gets called a lot. For the accessibility tests that
// measure performance when many nodes change, ChildrenChanged can be
// called tens of thousands of times. We need to balance catching changes
// for this metric with not slowing the perf bots down significantly.
// Tracing every 25 calls is an attempt at achieving that balance and
// may need to be adjusted further.
constexpr int kChildrenChangedTraceFrequency = 25;

void AXObjectCacheImpl::ChildrenChanged(const LayoutObject* layout_object) {
  static int children_changed_counter = 0;
  if (++children_changed_counter % kChildrenChangedTraceFrequency == 0) {
    TRACE_EVENT0("accessibility",
                 "AXObjectCacheImpl::ChildrenChanged(LayoutObject)");
  }

  if (!layout_object)
    return;

  // Ensure that this object is touched, so that Get() can Invalidate() it if
  // necessary, e.g. to change whether it's an AXNodeObject <--> AXLayoutObject.
  Get(layout_object);

  // Update using nearest node (walking ancestors if necessary).
  Node* node = GetClosestNodeForLayoutObject(layout_object);
  if (!node)
    return;

  ChildrenChanged(node);
}

void AXObjectCacheImpl::ChildrenChanged(AccessibleNode* accessible_node) {
  DCHECK(accessible_node);
  ChildrenChanged(Get(accessible_node));
}

void AXObjectCacheImpl::ChildrenChangedWithCleanLayout(Node* node) {
  if (!node)
    return;

  LayoutObject* layout_object = node->GetLayoutObject();
  AXID layout_id = 0;
  if (layout_object) {
    auto it = layout_object_mapping_.find(layout_object);
    if (it != layout_object_mapping_.end())
      layout_id = it->value;
  }
  DCHECK(!HashTraits<AXID>::IsDeletedValue(layout_id));

  auto it = node_object_mapping_.find(node);
  AXID node_id = it != node_object_mapping_.end() ? it->value : 0;
  DCHECK(!HashTraits<AXID>::IsDeletedValue(node_id));
  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));

  ChildrenChangedWithCleanLayout(node, Get(node));
}

void AXObjectCacheImpl::ChildrenChangedWithCleanLayout(Node* optional_node,
                                                       AXObject* obj) {
  if (HTMLMapElement* map_element = DynamicTo<HTMLMapElement>(optional_node)) {
    obj = GetAXImageForMap(*map_element);
    if (!obj)
      return;
    optional_node = obj->GetNode();
  }
  if (obj ? obj->IsDetached() : !optional_node)
    return;

#if DCHECK_IS_ON()
  if (obj && optional_node) {
    DCHECK_EQ(obj->GetNode(), optional_node);
    DCHECK_EQ(obj, SafeGet(optional_node));
  }
  Document* document = obj ? obj->GetDocument() : &optional_node->GetDocument();
  DCHECK(document);
  DCHECK(document->Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean)
      << "Unclean document at lifecycle " << document->Lifecycle().ToString();
#endif  // DCHECK_IS_ON()

  if (obj) {
    obj->ChildrenChangedWithCleanLayout();
    // TODO(accessibility) Only needed for <select> size changes.
    // This can turn into a DCHECK if the shadow DOM is used for <select>
    // elements instead of AXMenuList* and AXListBox* classes.
    if (obj->IsDetached())
      return;
  }

  if (optional_node)
    relation_cache_->UpdateRelatedTree(optional_node, obj);
}

void AXObjectCacheImpl::ProcessDeferredAccessibilityEvents(Document& document) {
  ProcessDeferredAccessibilityEventsImpl(document);

  // Accessibility is now clean: AXObjects can be safely traversed and
  // AXObject's properties can be safely fetched.
  for (auto agent : agents_)
    agent->AXReadyCallback(document);

  // TODO(chrishtr) Accessibility serializations should happen now, on the
  // condition that enough time has passed since the last serialization.
}

void AXObjectCacheImpl::ProcessDeferredAccessibilityEventsImpl(
    Document& document) {
  TRACE_EVENT0("accessibility", "ProcessDeferredAccessibilityEvents");

  DCHECK(document.Lifecycle().GetState() == DocumentLifecycle::kInAccessibility)
      << "Deferred events should only be processed during the "
         "accessibility document lifecycle.";

  // When tree updates are paused, IsDirty() will return false. In this
  // situation we should not return early because we would never trigger the
  // code that resumes the tree updates, inside ProcessCleanLayoutCallbacks.
  if (!IsDirty() && !tree_updates_paused_)
    return;

  DCHECK(GetDocument().IsAccessibilityEnabled())
      << "ProcessDeferredAccessibilityEvents should not perform work when "
         "accessibility is not enabled."
      << "\n* IsPopup? " << IsPopup(document);

  SCOPED_UMA_HISTOGRAM_TIMER(
      "Accessibility.Performance.ProcessDeferredAccessibilityEvents");

#if DCHECK_IS_ON()
  int loop_counter = 0;
#endif

  do {
    // Destroy and recreate any objects which are no longer valid, for example
    // they used AXNodeObject and now must be an AXLayoutObject, or vice-versa.
    // Also fires children changed on the parent of these nodes.
    ProcessInvalidatedObjects(document);

    // Call the queued callback methods that do processing which must occur when
    // layout is clean. These callbacks are stored in
    // tree_update_callback_queue_, and have names like
    // FooBarredWithCleanLayout().
    ProcessCleanLayoutCallbacks(document);

    // Changes to ids or aria-owns may have resulted in queued up relation
    // cache work; do that now.
    relation_cache_->ProcessUpdatesWithCleanLayout();

    // Keep going if there are more ids to invalidate or children changes to
    // process from previous steps. For examople, a display locked
    // (content-visibility:auto) element could be invalidated as it is scrolled
    // in or out of view, causing Invalidate() to add it to invalidated_ids_.
    // As ProcessInvalidatedObjects() refreshes the objectt and calls
    // ChildrenChanged() on the parent, more objects may be invalidated, or
    // more objects may have children changed called on them.
#if DCHECK_IS_ON()
    DCHECK_LE(++loop_counter, 100) << "Probable infinite loop detected.";
#endif
  } while (!nodes_with_pending_children_changed_.IsEmpty() ||
           !GetInvalidatedIds(document).IsEmpty());

  // Send events to RenderAccessibilityImpl, which serializes them and then
  // sends the serialized events and dirty objects to the browser process.
  PostNotifications(document);
}

bool AXObjectCacheImpl::IsDirty() const {
  if (tree_updates_paused_)
    return false;
  return !tree_update_callback_queue_main_.IsEmpty() ||
         !tree_update_callback_queue_popup_.IsEmpty() ||
         !notifications_to_post_main_.IsEmpty() ||
         !notifications_to_post_popup_.IsEmpty() ||
         !invalidated_ids_main_.IsEmpty() ||
         !invalidated_ids_popup_.IsEmpty() || relation_cache_->IsDirty();
}

void AXObjectCacheImpl::EmbeddingTokenChanged(HTMLFrameOwnerElement* element) {
  if (!element)
    return;

  MarkElementDirty(element);
}

bool AXObjectCacheImpl::IsPopup(Document& document) const {
  // There are 1-2 documents per AXObjectCache: the main document and
  // sometimes a popup document.
  int is_popup = document != GetDocument();
#if DCHECK_IS_ON()
  if (is_popup) {
    // Verify that the popup document's owner is the main document.
    LocalFrame* frame = document.GetFrame();
    DCHECK(frame);
    Element* popup_owner = frame->PagePopupOwner();
    DCHECK(popup_owner);
    DCHECK_EQ(popup_owner->GetDocument(), GetDocument())
        << "The popup document's owner should be in the main document.";
    Page* main_page = GetDocument().GetPage();
    DCHECK(main_page);
    // TODO(accessibility) Verify that the main document's popup is |document|.
    // PagePopupController* popup_controller =
    //     PagePopupController::From(*main_page);
    // DCHECK(popup_controller);
    // AXObject* popup_root_ax_object = popup_controller->RootAXObject();
    // DCHECK(popup_root_ax_object);
    // DCHECK_EQ(popup_root_ax_object->GetDocument(), &document)
    //     << "There can be only one active popup document.";
  }
#endif
  return is_popup;
}

HashSet<AXID>& AXObjectCacheImpl::GetInvalidatedIds(Document& document) {
  return IsPopup(document) ? invalidated_ids_popup_ : invalidated_ids_main_;
}

AXObjectCacheImpl::TreeUpdateCallbackQueue&
AXObjectCacheImpl::GetTreeUpdateCallbackQueue(Document& document) {
  return IsPopup(document) ? tree_update_callback_queue_popup_
                           : tree_update_callback_queue_main_;
}

HeapVector<Member<AXObjectCacheImpl::AXEventParams>>&
AXObjectCacheImpl::GetNotificationsToPost(Document& document) {
  return IsPopup(document) ? notifications_to_post_popup_
                           : notifications_to_post_main_;
}

void AXObjectCacheImpl::ProcessInvalidatedObjects(Document& document) {
  // Create a new object with the same AXID as the old one.
  // Currently only supported for objects with a backing node.
  // Returns the new object.
  auto refresh = [this](AXObject* current) {
    DCHECK(current);
    Node* node = current->GetNode();
    DCHECK(node) << "Refresh() is currently only supported for objects "
                    "with a backing node.";
    bool is_ax_layout_object = current->GetLayoutObject();
    bool will_be_ax_layout_object =
        node->GetLayoutObject() &&
        IsLayoutObjectRelevantForAccessibility(*node->GetLayoutObject()) &&
        !IsDisplayLocked(node->GetLayoutObject());
    if (is_ax_layout_object == will_be_ax_layout_object)
      return static_cast<AXObject*>(nullptr);  // No change in the AXObject.

    // When a pseudo element loses its layout, destroy all of the nodeless
    // descendant objects (they could not be individually invalidated because
    // only AXObjects with a node can be invalidated).
    if (!will_be_ax_layout_object && node->IsPseudoElement()) {
      for (const auto& ax_child : current->UnignoredChildren())
        RemoveAXObjectsInLayoutSubtree(ax_child);
    }

    AXID retained_axid = current->AXObjectID();
    // Remove from relevant maps, but not from relation cache, as the relations
    // between AXIDs will still be the same.
    node_object_mapping_.erase(node);
    if (is_ax_layout_object) {
      layout_object_mapping_.erase(current->GetLayoutObject());
    } else {
      DCHECK(will_be_ax_layout_object);
      DCHECK(node->GetLayoutObject());
      DCHECK(!layout_object_mapping_.Contains(node->GetLayoutObject()))
          << node << " " << node->GetLayoutObject();
    }

    ChildrenChangedOnAncestorOf(current);
    current->Detach();

    // TODO(accessibility) We don't use the return value, can we use .erase()
    // and it will still make sure that the object is cleaned up?
    objects_.Take(retained_axid);
    // Do not pass in the previous parent. It may not end up being the same,
    // e.g. in the case of a <select> option where the select changed size.
    // TODO(accessibility) That may be the only example of this, in which case
    // it could be handled in RoleChangedWithCleanLayout(), and the cached
    // parent could be used.
    AXObject* new_object = CreateAndInit(node, nullptr, retained_axid);
    if (new_object) {
      // Any owned objects need to reset their parent_ to point to the
      // new object.
      if (AXObject::HasARIAOwns(DynamicTo<Element>(node)) &&
          AXRelationCache::IsValidOwner(new_object)) {
        relation_cache_->UpdateAriaOwnsWithCleanLayout(new_object, true);
      }
    } else {
      // Failed to create, so remove object completely.
      RemoveAXID(current);
    }

    return new_object;
  };

  // ChildrenChanged() calls from below work may invalidate more objects.
  // Thereore, work from a separate list of ids, allowing new invalidated_ids.
  HashSet<AXID> old_invalidated_ids;
  old_invalidated_ids.swap(GetInvalidatedIds(document));
  for (AXID ax_id : old_invalidated_ids) {
    AXObject* object = ObjectFromAXID(ax_id);
    if (!object || object->IsDetached())
      continue;
    DCHECK_EQ(object->GetDocument(), &document);

#if defined(AX_FAIL_FAST_BUILD)
    bool did_use_layout_object_traversal =
        object->ShouldUseLayoutObjectTraversalForChildren();
#endif

    // Invalidate children on the first available non-detached parent that is
    // included in the tree. Sometimes a cached parent is detached because
    // an object was detached in the middle of the tree, and cached parents
    // are not corrected until the call to UpdateChildrenIfNecessary() below.
    AXObject* parent = object;
    while (true) {
      AXObject* candidate_parent = parent->CachedParentObject();
      if (!candidate_parent || candidate_parent->IsDetached()) {
        // The cached parent pointed to a detached AXObject. Compute a new
        // candidate parent and repair the cached parent now, so that
        // refreshing and initializing the new object can occur (a parent is
        // required).
        candidate_parent = parent->ComputeParent();
        if (candidate_parent)
          parent->SetParent(candidate_parent);
      }

      parent = candidate_parent;
      if (!parent)
        break;  // No higher candidate parent found, will invalidate |parent|.

      // Queue up a ChildrenChanged() call for this parent.
      if (parent->LastKnownIsIncludedInTreeValue())
        break;  // Stop here (otherwise continue to higher ancestor).
    }

    if (!parent) {
      // If no parent is possible, prune from the tree.
      Remove(object);
      continue;
    }

    AXObject* new_object = refresh(object);
    MarkAXObjectDirtyWithCleanLayout(new_object);

#if defined(AX_FAIL_FAST_BUILD)
    SANITIZER_CHECK(!new_object ||
                    new_object->ShouldUseLayoutObjectTraversalForChildren() ==
                        did_use_layout_object_traversal)
        << "This should no longer be possible, an object only uses layout "
           "object traversal if it is part of a pseudo element subtree, "
           "and that never changes: "
        << new_object->ToString(true, true);
#endif
  }
}

void AXObjectCacheImpl::ProcessCleanLayoutCallbacks(Document& document) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(document);

  if (tree_updates_paused_) {
    ChildrenChangedWithCleanLayout(nullptr, GetOrCreate(&document));
    tree_updates_paused_ = false;
    LOG(INFO) << "Accessibility tree updates resumed after rebuilding the tree "
                 "from root";
    return;
  }

  UpdateNumTreeUpdatesQueuedBeforeLayoutHistogram();

  TreeUpdateCallbackQueue old_tree_update_callback_queue;
  GetTreeUpdateCallbackQueue(document).swap(old_tree_update_callback_queue);
  nodes_with_pending_children_changed_.clear();

  for (auto& tree_update : old_tree_update_callback_queue) {
    const Node* node = tree_update->node;
    AXID axid = tree_update->axid;

    // Need either an DOM node or an AXObject to be a valid update.
    // These may have been destroyed since the original update occurred.
    if (!node) {
      if (!axid || !ObjectFromAXID(axid))
        continue;
    }

#if DCHECK_IS_ON()
    if (axid) {
      AXObject* obj = ObjectFromAXID(axid);
      if (obj) {
        DCHECK(!obj->IsDetached());
        if (node) {
          DCHECK_EQ(node, obj->GetNode());
          DCHECK_EQ(SafeGet(node), obj);
        }
        DCHECK_EQ(obj->GetDocument(), document);
      }
    }
#endif
    base::OnceClosure& callback = tree_update->callback;
    // Insure the update is for the correct document.
    // If no node, this update must be from an AXObject with no DOM node,
    // such as an AccessibleNode. In that case, ensure the update is in the
    // main document.
    Document& tree_update_document = node ? node->GetDocument() : GetDocument();
    if (document != tree_update_document) {
      // Document does not match one of the supported documents for this
      // AXObjectCacheImpl. This can happen if a node is adopted by another
      // document. In this case, we throw the callback on the floor.
      continue;
    }

    FireTreeUpdatedEventImmediately(
        document, tree_update->event_from, tree_update->event_from_action,
        tree_update->event_intents, std::move(callback));
  }
}

void AXObjectCacheImpl::PostNotifications(Document& document) {
  HeapVector<Member<AXEventParams>> old_notifications_to_post;
  GetNotificationsToPost(document).swap(old_notifications_to_post);
  for (auto& params : old_notifications_to_post) {
    AXObject* obj = params->target;

    if (!obj || !obj->AXObjectID())
      continue;

    if (obj->IsDetached())
      continue;

    DCHECK_EQ(obj->GetDocument(), &document)
        << "Wrong document in PostNotifications";

    ax::mojom::blink::Event event_type = params->event_type;
    ax::mojom::blink::EventFrom event_from = params->event_from;
    ax::mojom::blink::Action event_from_action = params->event_from_action;
    const BlinkAXEventIntentsSet& event_intents = params->event_intents;
    FireAXEventImmediately(obj, event_type, event_from, event_from_action,
                           event_intents);
  }
}

void AXObjectCacheImpl::PostNotification(const LayoutObject* layout_object,
                                         ax::mojom::blink::Event notification) {
  if (!layout_object)
    return;
  PostNotification(Get(layout_object), notification);
}

void AXObjectCacheImpl::PostNotification(Node* node,
                                         ax::mojom::blink::Event notification) {
  if (!node)
    return;
  PostNotification(Get(node), notification);
}

void AXObjectCacheImpl::EnsurePostNotification(
    Node* node,
    ax::mojom::blink::Event notification) {
  if (!node)
    return;
  PostNotification(GetOrCreate(node), notification);
}

void AXObjectCacheImpl::PostNotification(AXObject* object,
                                         ax::mojom::blink::Event event_type) {
  if (!object || !object->AXObjectID() || object->IsDetached())
    return;

  modification_count_++;

  Document& document = *object->GetDocument();

  // It's possible for FireAXEventImmediately to post another notification.
  // If we're still in the accessibility document lifecycle, fire these events
  // immediately rather than deferring them.
  if (document.Lifecycle().GetState() == DocumentLifecycle::kInAccessibility) {
    FireAXEventImmediately(object, event_type, ComputeEventFrom(),
                           active_event_from_action_, ActiveEventIntents());
    return;
  }

  GetNotificationsToPost(document).push_back(
      MakeGarbageCollected<AXEventParams>(
          object, event_type, ComputeEventFrom(), active_event_from_action_,
          ActiveEventIntents()));

  // These events are fired during DocumentLifecycle::kInAccessibility,
  // ensure there is a visual update scheduled.
  ScheduleVisualUpdate(document);
}

void AXObjectCacheImpl::ScheduleVisualUpdate(Document& document) {
  // Scheduling visual updates before the document is finished loading can
  // interfere with event ordering. In any case, at least one visual update will
  // occur between now and when the document load is complete.
  if (!document.IsLoadCompleted())
    return;

  // If there was a document change that doesn't trigger a lifecycle update on
  // its own, (e.g. because it doesn't make layout dirty), make sure we run
  // lifecycle phases to update the computed accessibility tree.
  LocalFrameView* frame_view = document.View();
  Page* page = document.GetPage();
  if (!frame_view || !page)
    return;

  if (!frame_view->CanThrottleRendering() &&
      (!document.GetPage()->Animator().IsServicingAnimations() ||
       document.Lifecycle().GetState() >=
           DocumentLifecycle::kInAccessibility)) {
    page->Animator().ScheduleVisualUpdate(document.GetFrame());
  }
}

void AXObjectCacheImpl::FireTreeUpdatedEventImmediately(
    Document& document,
    ax::mojom::blink::EventFrom event_from,
    ax::mojom::blink::Action event_from_action,
    const BlinkAXEventIntentsSet& event_intents,
    base::OnceClosure callback) {
  DCHECK_EQ(document.Lifecycle().GetState(),
            DocumentLifecycle::kInAccessibility);

  base::AutoReset<ax::mojom::blink::EventFrom> event_from_resetter(
      &active_event_from_, event_from);
  base::AutoReset<ax::mojom::blink::Action> event_from_action_resetter(
      &active_event_from_action_, event_from_action);
  ScopedBlinkAXEventIntent defered_event_intents(event_intents.AsVector(),
                                                 &document);
  std::move(callback).Run();
}

void AXObjectCacheImpl::FireAXEventImmediately(
    AXObject* obj,
    ax::mojom::blink::Event event_type,
    ax::mojom::blink::EventFrom event_from,
    ax::mojom::blink::Action event_from_action,
    const BlinkAXEventIntentsSet& event_intents) {
  DCHECK_EQ(obj->GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kInAccessibility);

#if DCHECK_IS_ON()
  // Make sure none of the layout views are in the process of being laid out.
  // Notifications should only be sent after the layoutObject has finished
  auto* ax_layout_object = DynamicTo<AXLayoutObject>(obj);
  if (ax_layout_object) {
    LayoutObject* layout_object = ax_layout_object->GetLayoutObject();
    if (layout_object && layout_object->View())
      DCHECK(!layout_object->View()->GetLayoutState());
  }

  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(*obj->GetDocument());
#endif  // DCHECK_IS_ON()

  if (event_type == ax::mojom::blink::Event::kChildrenChanged &&
      obj->CachedParentObject()) {
    const bool was_ignored = obj->LastKnownIsIgnoredValue();
    const bool was_in_tree = obj->LastKnownIsIncludedInTreeValue();
    obj->UpdateCachedAttributeValuesIfNeeded(false);
    const bool is_ignored = obj->LastKnownIsIgnoredValue();
    const bool is_in_tree = obj->LastKnownIsIncludedInTreeValue();

    if (is_ignored != was_ignored || was_in_tree != is_in_tree)
      ChildrenChangedWithCleanLayout(obj->CachedParentObject());
  }

  PostPlatformNotification(obj, event_type, event_from, event_from_action,
                           event_intents);
}

bool AXObjectCacheImpl::IsAriaOwned(const AXObject* object) const {
  return relation_cache_->IsAriaOwned(object);
}

AXObject* AXObjectCacheImpl::ValidatedAriaOwner(const AXObject* object) const {
  return relation_cache_->ValidatedAriaOwner(object);
}

void AXObjectCacheImpl::ValidatedAriaOwnedChildren(
    const AXObject* owner,
    HeapVector<Member<AXObject>>& owned_children) {
  DCHECK(GetDocument().Lifecycle().GetState() >=
         DocumentLifecycle::kLayoutClean);
  relation_cache_->ValidatedAriaOwnedChildren(owner, owned_children);
}

bool AXObjectCacheImpl::MayHaveHTMLLabel(const HTMLElement& elem) {
  // Return false if this type of element will not accept a <label for> label.
  if (!elem.IsLabelable())
    return false;

  // Return true if a <label for> pointed to this element at some point.
  if (relation_cache_->MayHaveHTMLLabelViaForAttribute(elem))
    return true;

  // Return true if any amcestor is a label, as in <label><input></label>.
  return Traversal<HTMLLabelElement>::FirstAncestor(elem);
}

void AXObjectCacheImpl::CheckedStateChanged(Node* node) {
  DeferTreeUpdate(&AXObjectCacheImpl::PostNotification, node,
                  ax::mojom::blink::Event::kCheckedStateChanged);
}

void AXObjectCacheImpl::ListboxOptionStateChanged(HTMLOptionElement* option) {
  PostNotification(option, ax::mojom::Event::kCheckedStateChanged);
}

void AXObjectCacheImpl::ListboxSelectedChildrenChanged(
    HTMLSelectElement* select) {
  PostNotification(select, ax::mojom::Event::kSelectedChildrenChanged);
}

void AXObjectCacheImpl::ListboxActiveIndexChanged(HTMLSelectElement* select) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(select->GetDocument());

  auto* ax_object = DynamicTo<AXListBox>(Get(select));
  if (!ax_object)
    return;

  ax_object->ActiveIndexChanged();
}

void AXObjectCacheImpl::LocationChanged(const LayoutObject* layout_object) {
  // No need to send this notification if the object is aria-hidden.
  // Note that if the node is ignored for other reasons, it still might
  // be important to send this notification if any of its children are
  // visible - but in the case of aria-hidden we can safely ignore it.
  // Use CachedIsAriaHidden() instead of IsAriaHidden() because layout is not
  // clean here, and it's better to do the optimization up front. This is okay
  // because if the cached aria-hidden becomes stale, then the entire subtree
  // will be invalidated anyway.
  AXObject* obj = Get(layout_object);
  if (obj && obj->CachedIsAriaHidden())
    return;

  PostNotification(layout_object, ax::mojom::Event::kLocationChanged);
}

void AXObjectCacheImpl::ImageLoaded(const LayoutObject* layout_object) {
  AXObject* obj = Get(layout_object);
  MarkAXObjectDirty(obj);
}

void AXObjectCacheImpl::HandleClicked(Node* node) {
  if (AXObject* obj = Get(node))
    PostNotification(obj, ax::mojom::Event::kClicked);
}

void AXObjectCacheImpl::HandleAttributeChanged(
    const QualifiedName& attr_name,
    AccessibleNode* accessible_node) {
  if (!accessible_node)
    return;
  modification_count_++;
  if (AXObject* obj = Get(accessible_node))
    PostNotification(obj, ax::mojom::Event::kAriaAttributeChanged);
}

void AXObjectCacheImpl::HandleAriaExpandedChangeWithCleanLayout(Node* node) {
  if (!node)
    return;

  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(node->GetDocument());

  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));
  if (AXObject* obj = GetOrCreate(node))
    obj->HandleAriaExpandedChanged();
}

void AXObjectCacheImpl::HandleAriaPressedChangedWithCleanLayout(
    Element* element) {
  AXObject* ax_object = Get(element);
  if (!ax_object)
    return;

  ax::mojom::blink::Role previous_role = ax_object->RoleValue();
  bool was_toggle_button =
      previous_role == ax::mojom::blink::Role::kToggleButton;
  bool is_toggle_button = ax_object->HasAttribute(html_names::kAriaPressedAttr);

  if (was_toggle_button != is_toggle_button)
    HandleRoleChangeWithCleanLayout(element);
  else
    PostNotification(element, ax::mojom::blink::Event::kCheckedStateChanged);
}

void AXObjectCacheImpl::HandleAriaSelectedChangedWithCleanLayout(Node* node) {
  DCHECK(node);
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(node->GetDocument());

  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));
  AXObject* obj = Get(node);
  if (!obj)
    return;

  PostNotification(obj, ax::mojom::Event::kCheckedStateChanged);

  AXObject* listbox = obj->ParentObjectUnignored();
  if (listbox && listbox->RoleValue() == ax::mojom::Role::kListBox) {
    // Ensure listbox options are in sync as selection status may have changed
    MarkAXSubtreeDirty(listbox);
    PostNotification(listbox, ax::mojom::Event::kSelectedChildrenChanged);
  }
}

void AXObjectCacheImpl::HandleNodeLostFocusWithCleanLayout(Node* node) {
  DCHECK(node);
  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));
  AXObject* obj = Get(node);
  if (!obj)
    return;

  TRACE_EVENT1("accessibility",
               "AXObjectCacheImpl::HandleNodeLostFocusWithCleanLayout", "id",
               obj->AXObjectID());
  PostNotification(obj, ax::mojom::Event::kBlur);

  if (AXObject* active_descendant = obj->ActiveDescendant()) {
    if (active_descendant->IsSelectedFromFocusSupported())
      HandleAriaSelectedChangedWithCleanLayout(active_descendant->GetNode());
  }
}

void AXObjectCacheImpl::HandleNodeGainedFocusWithCleanLayout(Node* node) {
  node = FocusedElement();  // Needs to get this with clean layout.
  if (!node || !node->GetDocument().View())
    return;

  // TODO(chrishtr): refactor to use UpdateLifecycleIfNeeded.
  if (node->GetDocument().NeedsLayoutTreeUpdateForNode(*node)) {
    // This should only occur when focus goes into a popup document. The main
    // document has an updated layout, but the popup does not.
    DCHECK_NE(document_, node->GetDocument());
    node->GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
        DocumentUpdateReason::kAccessibility);
  }

  AXObject* obj = GetOrCreateFocusedObjectFromNode(node);
  if (!obj)
    return;

  TRACE_EVENT1("accessibility",
               "AXObjectCacheImpl::HandleNodeGainedFocusWithCleanLayout", "id",
               obj->AXObjectID());
  PostNotification(obj, ax::mojom::Event::kFocus);

  if (AXObject* active_descendant = obj->ActiveDescendant()) {
    if (active_descendant->IsSelectedFromFocusSupported())
      HandleAriaSelectedChangedWithCleanLayout(active_descendant->GetNode());
  }
}

// This might be the new target of a relation. Handle all possible cases.
void AXObjectCacheImpl::MaybeNewRelationTarget(Node& node, AXObject* obj) {
  // Track reverse relations
  relation_cache_->UpdateRelatedTree(&node, obj);

  if (!obj)
    return;

  DCHECK_EQ(obj->GetNode(), &node);

  // Check whether aria-activedescendant on the focused object points to
  // |obj|. If so, fire activedescendantchanged event now. This is only for
  // ARIA active descendants, not in a native control like a listbox, which
  // has its own initial active descendant handling.
  Node* focused_node = document_->FocusedElement();
  if (focused_node) {
    AXObject* focus = Get(focused_node);
    if (focus &&
        focus->GetAOMPropertyOrARIAAttribute(
            AOMRelationProperty::kActiveDescendant) == &node &&
        obj->CanBeActiveDescendant()) {
      focus->HandleActiveDescendantChanged();
    }
  }
}

void AXObjectCacheImpl::HandleActiveDescendantChangedWithCleanLayout(
    Node* node) {
  DCHECK(node);
  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));
  // Changing the active descendant should trigger recomputing all
  // cached values even if it doesn't result in a notification, because
  // it can affect what's focusable or not.
  modification_count_++;

  if (AXObject* obj = GetOrCreate(node))
    obj->HandleActiveDescendantChanged();
}

// A <section> or role=region uses the region role if and only if it has a name.
void AXObjectCacheImpl::SectionOrRegionRoleMaybeChanged(Element* element) {
  AXObject* ax_object = Get(element);
  if (!ax_object)
    return;

  // Require <section> or role="region" markup.
  if (!element->HasTagName(html_names::kSectionTag) &&
      ax_object->RawAriaRole() != ax::mojom::blink::Role::kRegion) {
    return;
  }

  // If role would stay the same, do nothing.
  if (ax_object->RoleValue() == ax_object->DetermineAccessibilityRole())
    return;

  HandleRoleChangeWithCleanLayout(element);
}

// Be as safe as possible about changes that could alter the accessibility role,
// as this may require a different subclass of AXObject.
// Role changes are disallowed by the spec but we must handle it gracefully, see
// https://www.w3.org/TR/wai-aria-1.1/#h-roles for more information.
void AXObjectCacheImpl::HandleRoleChangeWithCleanLayout(Node* node) {
  if (!node)
    return;  // Virtual AOM node.

  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));

  // Remove the current object and make the parent reconsider its children.
  if (AXObject* obj = GetOrCreate(node)) {
    // When the role of `obj` is changed, its AXObject needs to be destroyed and
    // a new one needs to be created in its place. We destroy the current
    // AXObject in this method and call ChildrenChangeWithCleanLayout() on the
    // parent so that future updates to its children will create the alert.
    ChildrenChangedWithCleanLayout(obj->CachedParentObject());
    if (int depth = RolePresentationPropagationDepth(node)) {
      // If role changes on a table, menu, or list invalidate the subtree of
      // objects that may require a specific parent role in order to keep their
      // role. For example, rows and cells require a table ancestor, and list
      // items require a parent list (must be direct DOM parent).
      RemoveAXObjectsInLayoutSubtree(obj, depth);
    } else {
      // The children of this thing need to detach from parent.
      Remove(obj);
    }
    // The aria-owns relation may have changed if the role changed,
    // because some roles allow aria-owns and others don't.
    // In addition, any owned objects need to reset their parent_ to point
    // to the new object.
    if (AXObject* new_object = GetOrCreate(node))
      relation_cache_->UpdateAriaOwnsWithCleanLayout(new_object, true);
  }
}

void AXObjectCacheImpl::HandleAriaHiddenChangedWithCleanLayout(Node* node) {
  if (!node)
    return;

  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(node->GetDocument());
  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));

  AXObject* obj = GetOrCreate(node);
  if (!obj)
    return;

  // https://www.w3.org/TR/wai-aria-1.1/#aria-hidden
  // An element is considered hidden if it, or any of its ancestors are not
  // rendered or have their aria-hidden attribute value set to true.
  AXObject* parent = obj->ParentObject();
  if (parent) {
    // If the parent is inert or aria-hidden, then the subtree will be
    // ignored and changing aria-hidden will have no effect.
    if (parent->IsInert() || parent->IsAriaHidden())
      return;
    // If the parent is 'display: none', then the subtree will be ignored and
    // changing aria-hidden will have no effect.
    if (parent->GetLayoutObject()) {
      // For elements with layout objects we can get their style directly.
      if (parent->GetLayoutObject()->Style()->Display() == EDisplay::kNone)
        return;
    } else if (Element* parent_element = parent->GetElement()) {
      // No layout object: must ensure computed style.
      const ComputedStyle* parent_style = parent_element->EnsureComputedStyle();
      if (!parent_style || parent_style->IsEnsuredInDisplayNone())
        return;
    }
    // Unlike AXObject's |IsVisible| or |IsHiddenViaStyle| this method does not
    // consider 'visibility: [hidden|collapse]', because while the visibility
    // property is inherited it can be overridden by any descendant by providing
    // 'visibility: visible' so it would be safest to invalidate the subtree in
    // such a case.
  }

  // Changing the aria hidden state should trigger recomputing all
  // cached values even if it doesn't result in a notification, because
  // it affects accessibility ignored state.
  modification_count_++;

  // Invalidate the subtree because aria-hidden affects the
  // accessibility ignored state for the entire subtree.
  MarkAXSubtreeDirtyWithCleanLayout(obj);
  ChildrenChangedWithCleanLayout(obj->CachedParentObject());
}

void AXObjectCacheImpl::HandleAttributeChanged(const QualifiedName& attr_name,
                                               Element* element) {
  DCHECK(element);
  DeferTreeUpdate(&AXObjectCacheImpl::HandleAttributeChangedWithCleanLayout,
                  attr_name, element);
}

void AXObjectCacheImpl::HandleAttributeChangedWithCleanLayout(
    const QualifiedName& attr_name,
    Element* element) {
  DCHECK(element);
  DCHECK(!element->GetDocument().NeedsLayoutTreeUpdateForNode(*element));
  if (attr_name == html_names::kRoleAttr ||
      attr_name == html_names::kTypeAttr) {
    HandleRoleChangeWithCleanLayout(element);
  } else if (attr_name == html_names::kSizeAttr ||
             attr_name == html_names::kAriaHaspopupAttr) {
    // Role won't change on edits, so avoid invalidation so that object is not
    // destroyed during editing.
    if (AXObject* obj = Get(element)) {
      if (!obj->IsTextField())
        HandleRoleChangeWithCleanLayout(element);
    }
  } else if (attr_name == html_names::kAltAttr) {
    TextChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kTitleAttr) {
    TextChangedWithCleanLayout(element);
    SectionOrRegionRoleMaybeChanged(element);
  } else if (attr_name == html_names::kForAttr &&
             IsA<HTMLLabelElement>(*element)) {
    LabelChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kIdAttr) {
    MaybeNewRelationTarget(*element, Get(element));
  } else if (attr_name == html_names::kTabindexAttr) {
    FocusableChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kDisabledAttr ||
             attr_name == html_names::kReadonlyAttr) {
    MarkElementDirtyWithCleanLayout(element);
  } else if (attr_name == html_names::kValueAttr) {
    HandleValueChanged(element);
  } else if (attr_name == html_names::kMinAttr ||
             attr_name == html_names::kMaxAttr) {
    MarkElementDirtyWithCleanLayout(element);
  } else if (attr_name == html_names::kStepAttr) {
    MarkElementDirtyWithCleanLayout(element);
  } else if (attr_name == html_names::kUsemapAttr) {
    HandleUseMapAttributeChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kNameAttr) {
    HandleNameAttributeChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kControlsAttr) {
    ChildrenChangedWithCleanLayout(element);
  }

  if (!attr_name.LocalName().StartsWith("aria-"))
    return;

  // Perform updates specific to each attribute.
  if (attr_name == html_names::kAriaActivedescendantAttr) {
    HandleActiveDescendantChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kAriaValuenowAttr ||
             attr_name == html_names::kAriaValuetextAttr) {
    HandleValueChanged(element);
  } else if (attr_name == html_names::kAriaLabelAttr ||
             attr_name == html_names::kAriaLabeledbyAttr ||
             attr_name == html_names::kAriaLabelledbyAttr) {
    TextChangedWithCleanLayout(element);
    SectionOrRegionRoleMaybeChanged(element);
  } else if (attr_name == html_names::kAriaDescriptionAttr ||
             attr_name == html_names::kAriaDescribedbyAttr) {
    TextChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kAriaCheckedAttr) {
    PostNotification(element, ax::mojom::blink::Event::kCheckedStateChanged);
  } else if (attr_name == html_names::kAriaPressedAttr) {
    HandleAriaPressedChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kAriaSelectedAttr) {
    HandleAriaSelectedChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kAriaExpandedAttr) {
    HandleAriaExpandedChangeWithCleanLayout(element);
  } else if (attr_name == html_names::kAriaHiddenAttr) {
    HandleAriaHiddenChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kAriaInvalidAttr) {
    MarkElementDirtyWithCleanLayout(element);
  } else if (attr_name == html_names::kAriaErrormessageAttr) {
    MarkElementDirtyWithCleanLayout(element);
  } else if (attr_name == html_names::kAriaOwnsAttr) {
    if (AXObject* obj = GetOrCreate(element))
      relation_cache_->UpdateAriaOwnsWithCleanLayout(obj);
  } else {
    PostNotification(element, ax::mojom::Event::kAriaAttributeChanged);
  }
}

void AXObjectCacheImpl::HandleUseMapAttributeChangedWithCleanLayout(
    Element* element) {
  if (!IsA<HTMLImageElement>(element))
    return;
  // Get an area (aka image link) from the previous usemap.
  AXObject* ax_image = Get(element);
  AXObject* ax_image_link =
      ax_image ? ax_image->FirstChildIncludingIgnored() : nullptr;
  HTMLMapElement* previous_map =
      ax_image_link && ax_image_link->GetNode()
          ? Traversal<HTMLMapElement>::FirstAncestor(*ax_image_link->GetNode())
          : nullptr;
  // Both the old and new image may change image <--> image map.
  HandleRoleChangeWithCleanLayout(element);
  if (previous_map)
    HandleRoleChangeWithCleanLayout(previous_map->ImageElement());
}

void AXObjectCacheImpl::HandleNameAttributeChangedWithCleanLayout(
    Element* element) {
  // Changing a map name can alter an image's role and children.
  // The name has already changed, so we can no longer find the primary image
  // via the DOM. Use an area child's parent to find the old image.
  // If the old image was treated as a map, and now isn't, it will take care
  // of updating any other image that is newly associated with the map,
  // via AXNodeObject::AddImageMapChildren().
  if (HTMLMapElement* map = DynamicTo<HTMLMapElement>(element)) {
    if (AXObject* ax_previous_image = GetAXImageForMap(*map))
      HandleRoleChangeWithCleanLayout(ax_previous_image->GetNode());
  }
}

AXObject* AXObjectCacheImpl::GetOrCreateValidationMessageObject() {
  AXObject* message_ax_object = nullptr;
  // Create only if it does not already exist.
  if (validation_message_axid_) {
    message_ax_object = ObjectFromAXID(validation_message_axid_);
  }
  if (message_ax_object) {
    DCHECK(!message_ax_object->IsDetached());
    message_ax_object->SetParent(Root());  // Reattach to parent (root).
  } else {
    message_ax_object = MakeGarbageCollected<AXValidationMessage>(*this);
    DCHECK(message_ax_object);
    // Cache the validation message container for reuse.
    validation_message_axid_ = AssociateAXID(message_ax_object);
    // Validation message alert object is a child of the document, as not all
    // form controls can have a child. Also, there are form controls such as
    // listbox that technically can have children, but they are probably not
    // expected to have alerts within AT client code.
    message_ax_object->Init(Root());
  }
  return message_ax_object;
}

AXObject* AXObjectCacheImpl::ValidationMessageObjectIfInvalid(
    bool notify_children_changed) {
  Element* focused_element = document_->FocusedElement();
  if (focused_element) {
    ListedElement* form_control = ListedElement::From(*focused_element);
    if (form_control && !form_control->IsNotCandidateOrValid()) {
      // These must both be true:
      // * Focused control is currently invalid.
      // * Validation message was previously created but hidden
      // from timeout or currently visible.
      bool was_validation_message_already_created = validation_message_axid_;
      if (was_validation_message_already_created ||
          form_control->IsValidationMessageVisible()) {
        AXObject* focused_object = FocusedObject();
        if (focused_object) {
          // Return as long as the focused form control isn't overriding with a
          // different message via aria-errormessage.
          bool override_native_validation_message =
              focused_object->GetAOMPropertyOrARIAAttribute(
                  AOMRelationProperty::kErrorMessage);
          if (!override_native_validation_message) {
            AXObject* message = GetOrCreateValidationMessageObject();
            DCHECK(message);
            DCHECK(!message->IsDetached());
            if (notify_children_changed &&
                Root()->FirstChildIncludingIgnored() != message) {
              // Only notify children changed if not already processing new root
              // children, and the root doesn't already have this child.
              ChildrenChanged(document_);
            }
            DCHECK_EQ(message->CachedParentObject(), Root());
            return message;
          }
        }
      }
    }
  }

  // No focused, invalid form control.
  if (validation_message_axid_) {
    DeferTreeUpdate(
        &AXObjectCacheImpl::RemoveValidationMessageObjectWithCleanLayout,
        document_);
  }
  return nullptr;
}

void AXObjectCacheImpl::RemoveValidationMessageObjectWithCleanLayout(
    Node* document) {
  DCHECK_EQ(document, document_);
  if (validation_message_axid_) {
    // Remove when it becomes hidden, so that a new object is created the next
    // time the message becomes visible. It's not possible to reuse the same
    // alert, because the event generator will not generate an alert event if
    // the same object is hidden and made visible quickly, which occurs if the
    // user submits the form when an alert is already visible.
    Remove(validation_message_axid_);
    validation_message_axid_ = 0;
    ChildrenChanged(document_);
  }
}

// Native validation error popup for focused form control in current document.
void AXObjectCacheImpl::HandleValidationMessageVisibilityChanged(
    const Node* form_control) {
  DCHECK(form_control);
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(form_control->GetDocument());

  DeferTreeUpdate(&AXObjectCacheImpl::
                      HandleValidationMessageVisibilityChangedWithCleanLayout,
                  form_control);
}

void AXObjectCacheImpl::HandleValidationMessageVisibilityChangedWithCleanLayout(
    const Node* form_control) {
#if DCHECK_IS_ON()
  DCHECK(form_control);
  Document* document = &form_control->GetDocument();
  DCHECK(document);
  DCHECK(document->Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean)
      << "Unclean document at lifecycle " << document->Lifecycle().ToString();
#endif  // DCHECK_IS_ON()

  AXObject* message_ax_object = ValidationMessageObjectIfInvalid(
      /* Fire children changed on root if it gains message child */ true);
  if (message_ax_object)  // May be invisible now.
    MarkAXObjectDirtyWithCleanLayout(message_ax_object);

  // If the form control is invalid, it will now have an error message relation
  // to the message container.
  MarkElementDirtyWithCleanLayout(form_control);
}

void AXObjectCacheImpl::HandleEventListenerAdded(
    const Node& node,
    const AtomicString& event_type) {
  // If this is the first |event_type| listener for |node|, handle the
  // subscription change.
  if (node.NumberOfEventListeners(event_type) == 1)
    HandleEventSubscriptionChanged(node, event_type);
}

void AXObjectCacheImpl::HandleEventListenerRemoved(
    const Node& node,
    const AtomicString& event_type) {
  // If there are no more |event_type| listeners for |node|, handle the
  // subscription change.
  if (node.NumberOfEventListeners(event_type) == 0)
    HandleEventSubscriptionChanged(node, event_type);
}

bool AXObjectCacheImpl::DoesEventListenerImpactIgnoredState(
    const AtomicString& event_type) const {
  return event_util::IsMouseButtonEventType(event_type);
}

void AXObjectCacheImpl::HandleEventSubscriptionChanged(
    const Node& node,
    const AtomicString& event_type) {
  // Adding or Removing an event listener for certain events may affect whether
  // a node or its descendants should be accessibility ignored.
  if (!DoesEventListenerImpactIgnoredState(event_type))
    return;

  // Adding/removing a listener may affect the ignored state of node's AXObject.
  modification_count_++;
  MarkElementDirty(&node);
  // If the ignored state changes, the parent's children may have changed.
  if (AXObject* obj = SafeGet(&node)) {
    if (obj->CachedParentObject())
      ChildrenChanged(obj->CachedParentObject());
  }
}

void AXObjectCacheImpl::LabelChangedWithCleanLayout(Element* element) {
  // Will call back to TextChanged() when done updating relation cache.
  relation_cache_->LabelChanged(element);
}

void AXObjectCacheImpl::InlineTextBoxesUpdated(LayoutObject* layout_object) {
  if (!InlineTextBoxAccessibilityEnabled())
    return;

  auto it = layout_object_mapping_.find(layout_object);
  AXID ax_id = it != layout_object_mapping_.end() ? it->value : 0;
  DCHECK(!HashTraits<AXID>::IsDeletedValue(ax_id));

  // Only update if the accessibility object already exists and it's
  // not already marked as dirty.
  // Do not use Get(): it does extra work to determine whether the object should
  // be invalidated, including calling IsLayoutObjectRelevantForAccessibility(),
  // which uses the NGInlineCursor. However, the NGInlineCursor cannot be used
  // while inline boxes are being updated.
  if (ax_id) {
    AXObject* obj = objects_.at(ax_id);
    DCHECK(obj);
    DCHECK(obj->IsAXLayoutObject());
    DCHECK(!obj->IsDetached());
    if (!obj->NeedsToUpdateChildren()) {
      obj->SetNeedsToUpdateChildren();
      PostNotification(obj, ax::mojom::blink::Event::kChildrenChanged);
    }
  }
}

Settings* AXObjectCacheImpl::GetSettings() {
  return document_->GetSettings();
}

bool AXObjectCacheImpl::InlineTextBoxAccessibilityEnabled() {
  Settings* settings = GetSettings();
  if (!settings)
    return false;
  return settings->GetInlineTextBoxAccessibilityEnabled();
}

const Element* AXObjectCacheImpl::RootAXEditableElement(const Node* node) {
  const Element* result = RootEditableElement(*node);
  const auto* element = DynamicTo<Element>(node);
  if (!element)
    element = node->parentElement();

  for (; element; element = element->parentElement()) {
    if (NodeIsTextControl(element))
      result = element;
  }

  return result;
}

bool AXObjectCacheImpl::NodeIsTextControl(const Node* node) {
  if (!node)
    return false;

  const AXObject* ax_object = GetOrCreate(const_cast<Node*>(node));
  return ax_object && ax_object->IsTextField();
}

bool IsNodeAriaVisible(Node* node) {
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return false;

  bool is_null = true;
  bool hidden = AccessibleNode::GetPropertyOrARIAAttribute(
      element, AOMBooleanProperty::kHidden, is_null);
  return !is_null && !hidden;
}

void AXObjectCacheImpl::PostPlatformNotification(
    AXObject* obj,
    ax::mojom::blink::Event event_type,
    ax::mojom::blink::EventFrom event_from,
    ax::mojom::blink::Action event_from_action,
    const BlinkAXEventIntentsSet& event_intents) {
  obj = GetSerializationTarget(obj);
  if (!obj)
    return;

  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(document_->AXObjectCacheOwner().GetFrame());
  if (web_frame && web_frame->Client()) {
    ui::AXEvent event;
    event.id = obj->AXObjectID();
    event.event_type = event_type;
    event.event_from = event_from;
    event.event_from_action = event_from_action;
    event.event_intents.resize(event_intents.size());
    // We need to filter out the counts from every intent.
    std::transform(event_intents.begin(), event_intents.end(),
                   event.event_intents.begin(),
                   [](const auto& intent) { return intent.key.intent(); });
    for (auto agent : agents_)
      agent->AXEventFired(obj, event_type);
    web_frame->Client()->PostAccessibilityEvent(event);
  }
}

void AXObjectCacheImpl::MarkAXObjectDirtyWithCleanLayoutHelper(
    AXObject* obj,
    bool subtree,
    ax::mojom::blink::EventFrom event_from,
    ax::mojom::blink::Action event_from_action) {
  obj = GetSerializationTarget(obj);
  if (!obj)
    return;

  WebLocalFrameImpl* webframe = WebLocalFrameImpl::FromFrame(
      obj->GetDocument()->AXObjectCacheOwner().GetFrame());
  if (webframe && webframe->Client()) {
    webframe->Client()->MarkWebAXObjectDirty(WebAXObject(obj), subtree,
                                             event_from, event_from_action);
  }

  obj->UpdateCachedAttributeValuesIfNeeded(true);
  for (auto agent : agents_)
    agent->AXObjectModified(obj, subtree);
}

void AXObjectCacheImpl::MarkAXObjectDirtyWithCleanLayoutAndEvent(
    AXObject* obj,
    ax::mojom::blink::EventFrom event_from,
    ax::mojom::blink::Action event_from_action) {
  MarkAXObjectDirtyWithCleanLayoutHelper(obj, false, event_from,
                                         event_from_action);
}

void AXObjectCacheImpl::MarkAXObjectDirtyWithCleanLayout(AXObject* obj) {
  MarkAXObjectDirtyWithCleanLayoutHelper(obj, false, active_event_from_,
                                         active_event_from_action_);
}

void AXObjectCacheImpl::MarkAXSubtreeDirtyWithCleanLayout(AXObject* obj) {
  MarkAXObjectDirtyWithCleanLayoutHelper(obj, true, active_event_from_,
                                         active_event_from_action_);
}

void AXObjectCacheImpl::MarkAXObjectDirty(AXObject* obj) {
  if (!obj)
    return;
  base::OnceClosure callback =
      WTF::Bind(&AXObjectCacheImpl::MarkAXObjectDirtyWithCleanLayout,
                WrapWeakPersistent(this), WrapWeakPersistent(obj));
  DeferTreeUpdateInternal(std::move(callback), obj);
}

void AXObjectCacheImpl::MarkAXSubtreeDirty(AXObject* obj) {
  if (!obj)
    return;
  base::OnceClosure callback =
      WTF::Bind(&AXObjectCacheImpl::MarkAXSubtreeDirtyWithCleanLayout,
                WrapWeakPersistent(this), WrapWeakPersistent(obj));
  DeferTreeUpdateInternal(std::move(callback), obj);
}

void AXObjectCacheImpl::MarkElementDirty(const Node* element) {
  // Warning, if no AXObject exists for element, nothing is marked dirty.
  MarkAXObjectDirty(Get(element));
}

void AXObjectCacheImpl::MarkElementDirtyWithCleanLayout(const Node* element) {
  // Warning, if no AXObject exists for element, nothing is marked dirty.
  MarkAXObjectDirtyWithCleanLayout(Get(element));
}

AXObject* AXObjectCacheImpl::GetSerializationTarget(AXObject* obj) {
  if (!obj || obj->IsDetached() || !obj->GetDocument() ||
      !obj->GetDocument()->View() ||
      !obj->GetDocument()->View()->GetFrame().GetPage()) {
    return nullptr;
  }

  // A <slot> descendant of a node that is still in the DOM but no longer
  // rendered will return true for Node::isConnected() and false for
  // AXObject::IsDetached(). But from the perspective of platform ATs, this
  // subtree is not connected and is detached.
  // TODO(accessibility): The relevance check probably applies to all nodes
  // not just slot elements.
  if (const HTMLSlotElement* slot =
          ToHTMLSlotElementIfSupportsAssignmentOrNull(obj->GetNode())) {
    if (!AXObjectCacheImpl::IsRelevantSlotElement(*slot))
      return nullptr;
  }

  // Ensure still in tree.
  if (obj->IsMissingParent()) {
    // TODO(accessibility) Only needed because of <select> size changes.
    // This should become a DCHECK(!obj->IsMissingParent()) once the shadow DOM
    // is used for <select> elements instead of AXMenuList* and AXListBox*
    // classes.
    if (!RestoreParentOrPrune(obj))
      return nullptr;
  }

  // Return included in tree object.
  if (obj->AccessibilityIsIncludedInTree())
    return obj;

  return obj->ParentObjectIncludedInTree();
}

AXObject* AXObjectCacheImpl::RestoreParentOrPrune(AXObject* child) {
  AXObject* parent = child->ComputeParentOrNull();
  if (parent)
    child->SetParent(parent);
  else  // If no parent is possible, the child is no longer part of the tree.
    Remove(child);

  return parent;
}

void AXObjectCacheImpl::HandleFocusedUIElementChanged(
    Element* old_focused_element,
    Element* new_focused_element) {
  TRACE_EVENT0("accessibility",
               "AXObjectCacheImpl::HandleFocusedUIElementChanged");
  Document& focused_doc =
      new_focused_element ? new_focused_element->GetDocument() : *document_;

#if DCHECK_IS_ON()
  // The focus can be in a different document when a popup is open.
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(focused_doc);
#endif  // DCHECK_IS_ON()

  if (focused_doc.GetPage() && focused_doc.GetPage()->InsidePortal())
    return;  // Elements inside a portal are not considered focusable.

  if (validation_message_axid_) {
    DeferTreeUpdate(
        &AXObjectCacheImpl::RemoveValidationMessageObjectWithCleanLayout,
        document_);
  }

  if (!new_focused_element) {
    // When focus is cleared, implicitly focus the document by sending a blur.
    if (GetDocument().documentElement()) {
      DeferTreeUpdate(&AXObjectCacheImpl::HandleNodeLostFocusWithCleanLayout,
                      GetDocument().documentElement());
    }
    return;
  }

  Page* page = new_focused_element->GetDocument().GetPage();
  if (!page)
    return;

  if (old_focused_element) {
    DeferTreeUpdate(&AXObjectCacheImpl::HandleNodeLostFocusWithCleanLayout,
                    old_focused_element);
  }

  Settings* settings = GetSettings();
  if (settings && settings->GetAriaModalPrunesAXTree())
    UpdateActiveAriaModalDialog(new_focused_element);

  DeferTreeUpdate(&AXObjectCacheImpl::HandleNodeGainedFocusWithCleanLayout,
                  FocusedElement());
}

// Check if the focused node is inside an active aria-modal dialog. If so, we
// should mark the cache as dirty to recompute the ignored status of each node.
void AXObjectCacheImpl::UpdateActiveAriaModalDialog(Node* node) {
  AXObject* new_active_aria_modal = AncestorAriaModalDialog(node);
  if (active_aria_modal_dialog_ == new_active_aria_modal)
    return;

  active_aria_modal_dialog_ = new_active_aria_modal;
  modification_count_++;
  MarkAXSubtreeDirty(Root());
}

AXObject* AXObjectCacheImpl::AncestorAriaModalDialog(Node* node) {
  for (Element* ancestor = Traversal<Element>::FirstAncestorOrSelf(*node);
       ancestor; ancestor = Traversal<Element>::FirstAncestor(*ancestor)) {
    if (!ancestor->FastHasAttribute(html_names::kAriaModalAttr))
      continue;

    AtomicString aria_modal =
        ancestor->FastGetAttribute(html_names::kAriaModalAttr);
    if (!EqualIgnoringASCIICase(aria_modal, "true")) {
      continue;
    }

    AXObject* ancestor_ax_object = GetOrCreate(ancestor);
    if (!ancestor_ax_object)
      return nullptr;
    ax::mojom::blink::Role ancestor_role = ancestor_ax_object->RoleValue();

    if (!ui::IsDialog(ancestor_role))
      continue;

    return ancestor_ax_object;
  }
  return nullptr;
}

AXObject* AXObjectCacheImpl::GetActiveAriaModalDialog() const {
  return active_aria_modal_dialog_;
}

void AXObjectCacheImpl::SerializeLocationChanges() {
  if (changed_bounds_ids_.IsEmpty())
    return;
  Vector<mojom::blink::LocationChangesPtr> changes;
  changes.ReserveCapacity(changed_bounds_ids_.size());
  for (AXID changed_bounds_id : changed_bounds_ids_) {
    if (AXObject* obj = ObjectFromAXID(changed_bounds_id)) {
      // Only update locations that are already known.
      auto bounds = cached_bounding_boxes_.find(changed_bounds_id);
      if (bounds == cached_bounding_boxes_.end())
        continue;

      ui::AXRelativeBounds new_location;
      bool clips_children;
      obj->PopulateAXRelativeBounds(new_location, &clips_children);
      if (bounds->value == new_location)
        continue;

      cached_bounding_boxes_.Set(changed_bounds_id, new_location);
      changes.push_back(
          mojom::blink::LocationChanges::New(changed_bounds_id, new_location));
    }
  }
  changed_bounds_ids_.clear();
  if (!changes.IsEmpty()) {
    GetOrCreateRemoteRenderAccessibilityHost()->HandleAXLocationChanges(
        std::move(changes));
  }
}

bool AXObjectCacheImpl::SerializeEntireTree(bool exclude_offscreen,
                                            size_t max_node_count,
                                            base::TimeDelta timeout,
                                            ui::AXTreeUpdate* response) {
  BlinkAXTreeSource tree_source(*this);

  tree_source.set_exclude_offscreen(exclude_offscreen);

  // The serializer returns an ui::AXTreeUpdate, which can store a complete
  // or a partial accessibility tree. AXTreeSerializer is stateful, but the
  // first time you serialize from a brand-new tree you're guaranteed to get a
  // complete tree.
  ui::AXTreeSerializer<AXObject*> serializer(&tree_source);

  if (max_node_count)
    serializer.set_max_node_count(max_node_count);
  if (!timeout.is_zero())
    serializer.set_timeout(timeout);

  tree_source.Freeze();
  if (serializer.SerializeChanges(tree_source.GetRoot(), response)) {
    tree_source.Thaw();
    return true;
  }

  // It's possible for the page to fail to serialize the first time due to
  // aria-owns rearranging the page while it's being scanned. Try a second
  // time.
  *response = ui::AXTreeUpdate();
  bool result = serializer.SerializeChanges(tree_source.GetRoot(), response);
  tree_source.Thaw();
  return result;
}

mojo::Remote<blink::mojom::blink::RenderAccessibilityHost>&
AXObjectCacheImpl::GetOrCreateRemoteRenderAccessibilityHost() {
  if (!render_accessibility_host_) {
    GetDocument().GetFrame()->GetBrowserInterfaceBroker().GetInterface(
        render_accessibility_host_.BindNewPipeAndPassReceiver());
  }
  return render_accessibility_host_;
}

void AXObjectCacheImpl::HandleInitialFocus() {
  PostNotification(document_, ax::mojom::Event::kFocus);
}

void AXObjectCacheImpl::HandleEditableTextContentChanged(Node* node) {
  if (!node)
    return;

  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(node->GetDocument());

  DeferTreeUpdate(
      &AXObjectCacheImpl::HandleEditableTextContentChangedWithCleanLayout,
      node);
}

void AXObjectCacheImpl::HandleEditableTextContentChangedWithCleanLayout(
    Node* node) {
  AXObject* obj = nullptr;
  do {
    obj = GetOrCreate(node);
  } while (!obj && (node = node->parentNode()));
  if (!obj)
    return;

  while (obj && !obj->IsTextField())
    obj = obj->ParentObject();

  PostNotification(obj, ax::mojom::Event::kValueChanged);
}

void AXObjectCacheImpl::HandleScaleAndLocationChanged(Document* document) {
  if (!document)
    return;
  PostNotification(document, ax::mojom::Event::kLocationChanged);
}

void AXObjectCacheImpl::HandleTextFormControlChanged(Node* node) {
  HandleEditableTextContentChanged(node);
}

void AXObjectCacheImpl::HandleTextMarkerDataAdded(Node* start, Node* end) {
  DCHECK(start);
  DCHECK(end);
  DCHECK(IsA<Text>(start));
  DCHECK(IsA<Text>(end));

  // Notify the client of new text marker data.
  // Ensure there is a delay so that the final marker state can be evaluated.
  DeferTreeUpdate(&AXObjectCacheImpl::HandleTextMarkerDataAddedWithCleanLayout,
                  start);
  if (start != end) {
    DeferTreeUpdate(
        &AXObjectCacheImpl::HandleTextMarkerDataAddedWithCleanLayout, end);
  }
}

void AXObjectCacheImpl::HandleTextMarkerDataAddedWithCleanLayout(Node* node) {
  Text* text_node = To<Text>(node);
  // If non-spelling/grammar markers are present, assume that children changed
  // should be called.
  DocumentMarkerController& marker_controller = GetDocument().Markers();
  const DocumentMarker::MarkerTypes non_spelling_or_grammar_markers(
      DocumentMarker::kTextMatch | DocumentMarker::kActiveSuggestion |
      DocumentMarker::kSuggestion | DocumentMarker::kTextFragment |
      DocumentMarker::kCustomHighlight);
  if (!marker_controller.MarkersFor(*text_node, non_spelling_or_grammar_markers)
           .IsEmpty()) {
    ChildrenChangedWithCleanLayout(node);
    return;
  }

  // Spelling and grammar markers are removed and then readded in quick
  // succession. By checking these here (on a slight delay), we can determine
  // whether the presence of one of these markers actually changed, and only
  // fire ChildrenChangedWithCleanLayout() if they did.
  const DocumentMarker::MarkerTypes spelling_and_grammar_markers(
      DocumentMarker::DocumentMarker::kSpelling |
      DocumentMarker::DocumentMarker::kGrammar);
  bool has_spelling_or_grammar_markers =
      !marker_controller.MarkersFor(*text_node, spelling_and_grammar_markers)
           .IsEmpty();
  if (has_spelling_or_grammar_markers) {
    if (nodes_with_spelling_or_grammar_markers_.insert(node).is_new_entry)
      ChildrenChangedWithCleanLayout(node);
  } else {
    const auto& iter = nodes_with_spelling_or_grammar_markers_.find(node);
    if (iter != nodes_with_spelling_or_grammar_markers_.end()) {
      nodes_with_spelling_or_grammar_markers_.erase(iter);
      ChildrenChangedWithCleanLayout(node);
    }
  }
}

void AXObjectCacheImpl::HandleValueChanged(Node* node) {
  PostNotification(node, ax::mojom::Event::kValueChanged);

  // If it's a slider, invalidate the thumb's bounding box.
  AXObject* ax_object = Get(node);
  if (ax_object && ax_object->RoleValue() == ax::mojom::blink::Role::kSlider &&
      !ax_object->NeedsToUpdateChildren() &&
      ax_object->ChildCountIncludingIgnored() == 1) {
    changed_bounds_ids_.insert(
        ax_object->ChildAtIncludingIgnored(0)->AXObjectID());
  }
}

void AXObjectCacheImpl::HandleUpdateActiveMenuOption(Node* menu_list) {
  if (!use_ax_menu_list_) {
    MarkElementDirty(menu_list);
    return;
  }

  DeferTreeUpdate(
      &AXObjectCacheImpl::HandleUpdateActiveMenuOptionWithCleanLayout,
      menu_list);
}

void AXObjectCacheImpl::HandleUpdateActiveMenuOptionWithCleanLayout(
    Node* menu_list) {
  if (AXMenuList* ax_menu_list = DynamicTo<AXMenuList>(GetOrCreate(menu_list)))
    ax_menu_list->DidUpdateActiveOption();
}

void AXObjectCacheImpl::DidShowMenuListPopup(LayoutObject* menu_list) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(menu_list->GetDocument());

  DCHECK(menu_list->GetNode());
  DeferTreeUpdate(&AXObjectCacheImpl::DidShowMenuListPopupWithCleanLayout,
                  menu_list->GetNode());
}

void AXObjectCacheImpl::DidShowMenuListPopupWithCleanLayout(Node* menu_list) {
  if (!use_ax_menu_list_) {
    MarkAXObjectDirtyWithCleanLayout(Get(menu_list));
    return;
  }

  auto* ax_object = DynamicTo<AXMenuList>(Get(menu_list));
  if (ax_object)
    ax_object->DidShowPopup();
}

void AXObjectCacheImpl::DidHideMenuListPopup(LayoutObject* menu_list) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(menu_list->GetDocument());

  DCHECK(menu_list->GetNode());
  DeferTreeUpdate(&AXObjectCacheImpl::DidHideMenuListPopupWithCleanLayout,
                  menu_list->GetNode());
}

void AXObjectCacheImpl::DidHideMenuListPopupWithCleanLayout(Node* menu_list) {
  if (!use_ax_menu_list_) {
    MarkAXObjectDirtyWithCleanLayout(Get(menu_list));
    return;
  }

  auto* ax_object = DynamicTo<AXMenuList>(Get(menu_list));
  if (ax_object)
    ax_object->DidHidePopup();
}

void AXObjectCacheImpl::HandleLoadStart(Document* document) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(*document);
  MarkAXObjectDirty(Get(document));
  DeferTreeUpdate(&AXObjectCacheImpl::EnsurePostNotification, document,
                  ax::mojom::blink::Event::kLoadStart);
}

void AXObjectCacheImpl::HandleLoadComplete(Document* document) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(*document);

  AddPermissionStatusListener();
  DeferTreeUpdate(&AXObjectCacheImpl::HandleLoadCompleteWithCleanLayout,
                  document);
}

void AXObjectCacheImpl::HandleLoadCompleteWithCleanLayout(Node* document_node) {
  DCHECK(document_node);
  DCHECK(IsA<Document>(document_node));
#if DCHECK_IS_ON()
  Document* document = To<Document>(document_node);
  DCHECK(document->Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean)
      << "Unclean document at lifecycle " << document->Lifecycle().ToString();
#endif  // DCHECK_IS_ON()

  AddPermissionStatusListener();
  PostNotification(GetOrCreate(document_node),
                   ax::mojom::blink::Event::kLoadComplete);
}

void AXObjectCacheImpl::HandleLayoutComplete(Document* document) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(*document);
  if (document->Lifecycle().GetState() >=
      DocumentLifecycle::kAfterPerformLayout) {
    PostNotification(GetOrCreate(document),
                     ax::mojom::blink::Event::kLayoutComplete);
  } else {
    DeferTreeUpdate(&AXObjectCacheImpl::EnsurePostNotification, document,
                    ax::mojom::blink::Event::kLayoutComplete);
  }
}

void AXObjectCacheImpl::HandleScrolledToAnchor(const Node* anchor_node) {
  if (!anchor_node)
    return;

  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(anchor_node->GetDocument());

  AXObject* obj = GetOrCreate(anchor_node->GetLayoutObject());
  if (!obj)
    return;
  if (!obj->AccessibilityIsIncludedInTree())
    obj = obj->ParentObjectUnignored();
  PostNotification(obj, ax::mojom::Event::kScrolledToAnchor);
}

void AXObjectCacheImpl::HandleFrameRectsChanged(Document& document) {
  MarkElementDirty(&document);
}

void AXObjectCacheImpl::InvalidateBoundingBox(
    const LayoutObject* layout_object) {
  if (AXObject* obj = Get(const_cast<LayoutObject*>(layout_object))) {
    changed_bounds_ids_.insert(obj->AXObjectID());
  }
}

void AXObjectCacheImpl::SetCachedBoundingBox(
    AXID id,
    const ui::AXRelativeBounds& bounds) {
  cached_bounding_boxes_.Set(id, bounds);
}

void AXObjectCacheImpl::SerializerClearedNode(AXID id) {
  cached_bounding_boxes_.erase(id);
}

void AXObjectCacheImpl::HandleScrollPositionChanged(
    LocalFrameView* frame_view) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(*frame_view->GetFrame().GetDocument());

  InvalidateBoundingBoxForFixedOrStickyPosition();
  MarkElementDirty(document_);
  DeferTreeUpdate(&AXObjectCacheImpl::EnsurePostNotification, document_,
                  ax::mojom::blink::Event::kLayoutComplete);
}

void AXObjectCacheImpl::HandleScrollPositionChanged(
    LayoutObject* layout_object) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(layout_object->GetDocument());
  InvalidateBoundingBoxForFixedOrStickyPosition();
  Node* node = GetClosestNodeForLayoutObject(layout_object);
  if (node) {
    MarkElementDirty(node);
    DeferTreeUpdate(&AXObjectCacheImpl::EnsurePostNotification, node,
                    ax::mojom::blink::Event::kLayoutComplete);
  }
}

const AtomicString& AXObjectCacheImpl::ComputedRoleForNode(Node* node) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(node->GetDocument());

  AXObject* obj = GetOrCreate(node);
  if (!obj)
    return AXObject::ARIARoleName(ax::mojom::blink::Role::kUnknown);
  return AXObject::ARIARoleName(obj->RoleValue());
}

String AXObjectCacheImpl::ComputedNameForNode(Node* node) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(node->GetDocument());
  AXObject* obj = GetOrCreate(node);
  if (!obj)
    return "";

  return obj->ComputedName();
}

void AXObjectCacheImpl::OnTouchAccessibilityHover(const gfx::Point& location) {
  DocumentLifecycle::DisallowTransitionScope disallow(document_->Lifecycle());
  AXObject* hit = Root()->AccessibilityHitTest(location);
  if (hit) {
    // Ignore events on a frame or plug-in, because the touch events
    // will be re-targeted there and we don't want to fire duplicate
    // accessibility events.
    if (hit->GetLayoutObject() &&
        hit->GetLayoutObject()->IsLayoutEmbeddedContent())
      return;

    PostNotification(hit, ax::mojom::Event::kHover);
  }
}

void AXObjectCacheImpl::SetCanvasObjectBounds(HTMLCanvasElement* canvas,
                                              Element* element,
                                              const LayoutRect& rect) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(element->GetDocument());

  AXObject* obj = GetOrCreate(element);
  if (!obj)
    return;

  AXObject* ax_canvas = GetOrCreate(canvas);
  if (!ax_canvas)
    return;

  obj->SetElementRect(rect, ax_canvas);
}

void AXObjectCacheImpl::AddPermissionStatusListener() {
  if (!document_->GetExecutionContext())
    return;

  // Passing an Origin to Mojo crashes if the host is empty because
  // blink::SecurityOrigin sets unique to false, but url::Origin sets
  // unique to true. This only happens for some obscure corner cases
  // like on Android where the system registers unusual protocol handlers,
  // and we don't need any special permissions in those cases.
  //
  // http://crbug.com/759528 and http://crbug.com/762716
  if (document_->Url().Protocol() != "file" &&
      document_->Url().Host().IsEmpty()) {
    return;
  }

  if (permission_service_.is_bound())
    permission_service_.reset();

  ConnectToPermissionService(
      document_->GetExecutionContext(),
      permission_service_.BindNewPipeAndPassReceiver(
          document_->GetTaskRunner(TaskType::kUserInteraction)));

  if (permission_observer_receiver_.is_bound())
    permission_observer_receiver_.reset();

  mojo::PendingRemote<mojom::blink::PermissionObserver> observer;
  permission_observer_receiver_.Bind(
      observer.InitWithNewPipeAndPassReceiver(),
      document_->GetTaskRunner(TaskType::kUserInteraction));
  permission_service_->AddPermissionObserver(
      CreatePermissionDescriptor(
          mojom::blink::PermissionName::ACCESSIBILITY_EVENTS),
      accessibility_event_permission_, std::move(observer));
}

void AXObjectCacheImpl::OnPermissionStatusChange(
    mojom::PermissionStatus status) {
  accessibility_event_permission_ = status;
}

bool AXObjectCacheImpl::CanCallAOMEventListeners() const {
  return accessibility_event_permission_ == mojom::PermissionStatus::GRANTED;
}

void AXObjectCacheImpl::RequestAOMEventListenerPermission() {
  if (accessibility_event_permission_ != mojom::PermissionStatus::ASK)
    return;

  if (!permission_service_.is_bound())
    return;

  permission_service_->RequestPermission(
      CreatePermissionDescriptor(
          mojom::blink::PermissionName::ACCESSIBILITY_EVENTS),
      LocalFrame::HasTransientUserActivation(document_->GetFrame()),
      WTF::Bind(&AXObjectCacheImpl::OnPermissionStatusChange,
                WrapPersistent(this)));
}

void AXObjectCacheImpl::Trace(Visitor* visitor) const {
  visitor->Trace(agents_);
  visitor->Trace(document_);
  visitor->Trace(accessible_node_mapping_);
  visitor->Trace(layout_object_mapping_);
  visitor->Trace(node_object_mapping_);
  visitor->Trace(active_aria_modal_dialog_);

  visitor->Trace(objects_);
  visitor->Trace(notifications_to_post_main_);
  visitor->Trace(notifications_to_post_popup_);
  visitor->Trace(permission_service_);
  visitor->Trace(permission_observer_receiver_);
  visitor->Trace(tree_update_callback_queue_main_);
  visitor->Trace(tree_update_callback_queue_popup_);
  visitor->Trace(nodes_with_pending_children_changed_);
  visitor->Trace(nodes_with_spelling_or_grammar_markers_);
  visitor->Trace(ax_tree_source_);
  AXObjectCache::Trace(visitor);
}

ax::mojom::blink::EventFrom AXObjectCacheImpl::ComputeEventFrom() {
  if (active_event_from_ != ax::mojom::blink::EventFrom::kNone)
    return active_event_from_;

  if (document_ && document_->View() &&
      LocalFrame::HasTransientUserActivation(
          &(document_->View()->GetFrame()))) {
    return ax::mojom::blink::EventFrom::kUser;
  }

  return ax::mojom::blink::EventFrom::kPage;
}

WebAXAutofillState AXObjectCacheImpl::GetAutofillState(AXID id) const {
  auto iter = autofill_state_map_.find(id);
  if (iter == autofill_state_map_.end())
    return WebAXAutofillState::kNoSuggestions;
  return iter->value;
}

void AXObjectCacheImpl::SetAutofillState(AXID id, WebAXAutofillState state) {
  WebAXAutofillState previous_state = GetAutofillState(id);
  if (state != previous_state) {
    autofill_state_map_.Set(id, state);
    MarkAXObjectDirty(ObjectFromAXID(id));
  }
}

}  // namespace blink
