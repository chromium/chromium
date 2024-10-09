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

#include <iterator>
#include <numeric>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/blink/public/mojom/render_accessibility.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/renderer/core/accessibility/scoped_blink_ax_event_intent.h"
#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/events/event_util.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/listed_element.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/html/html_embed_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_map_element.h"
#include "third_party/blink/renderer/core/html/html_menu_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html/html_olist_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/html_progress_element.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/html_table_row_element.h"
#include "third_party/blink/renderer/core/html/html_title_element.h"
#include "third_party/blink/renderer/core/html/html_ulist_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/inline/abstract_inline_text_box.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_row.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/style/content_data.h"
#include "third_party/blink/renderer/core/svg/svg_graphics_element.h"
#include "third_party/blink/renderer/core/svg/svg_style_element.h"
#include "third_party/blink/renderer/modules/accessibility/aria_notification.h"
#include "third_party/blink/renderer/modules/accessibility/ax_image_map_link.h"
#include "third_party/blink/renderer/modules/accessibility/ax_inline_text_box.h"
#include "third_party/blink/renderer/modules/accessibility/ax_media_control.h"
#include "third_party/blink/renderer/modules/accessibility/ax_media_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_node_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_progress_indicator.h"
#include "third_party/blink/renderer/modules/accessibility/ax_relation_cache.h"
#include "third_party/blink/renderer/modules/accessibility/ax_slider.h"
#include "third_party/blink/renderer/modules/accessibility/ax_validation_message.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_enums.mojom-blink.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_location_and_scroll_updates.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/mojom/ax_location_and_scroll_updates.mojom-blink.h"
#include "ui/accessibility/mojom/ax_relative_bounds.mojom-blink.h"
#if DCHECK_IS_ON()
#include "third_party/blink/renderer/modules/accessibility/ax_debug_utils.h"
#endif

// Prevent code that runs during the lifetime of the stack from altering the
// document lifecycle, for the main document, and the popup document if present.
#if DCHECK_IS_ON()
#define SCOPED_DISALLOW_LIFECYCLE_TRANSITION()                               \
  DocumentLifecycle::DisallowTransitionScope scoped(document_->Lifecycle()); \
  DocumentLifecycle::DisallowTransitionScope scoped2(                        \
      popup_document_ ? popup_document_->Lifecycle()                         \
                      : document_->Lifecycle());
#else
#define SCOPED_DISALLOW_LIFECYCLE_TRANSITION()
#endif  // DCHECK_IS_ON()

namespace blink {

using mojom::blink::FormControlType;

namespace {

Node* RetargetInput(Node* node) {
  // Any click/focus that occurs on a <button> inside of a custom <select>
  // should be treated as if it occurred on the <select>. The custom button is
  // not actually present in the accessibility tree, but the select is present
  // as a Role::kMenuList object.
  if (IsA<HTMLButtonElement>(node)) {
    // Fallback button case.
    Node* possible_select = node->OwnerShadowHost();
    if (!possible_select) {
      // Custom button case.
      possible_select = NodeTraversal::Parent(*node);
    }
    if (auto* select = DynamicTo<HTMLSelectElement>(possible_select)) {
      if (select->IsAppearanceBaseButton() && node == select->SlottedButton()) {
        return select;
      }
    }
  }
  return node;
}

Element* RetargetInput(Element* element) {
  return DynamicTo<Element>(RetargetInput(static_cast<Node*>(element)));
}

bool IsInitialEmptyDocument(const Document& document) {
  // Do not fire for initial empty top document. This helps avoid thrashing the
  // a11y tree, causing an extra serialization.
  // TODO(accessibility) This is an ugly special case -- find a better way.
  // Note: Document::IsInitialEmptyDocument() did not work -- should it?
  if (document.body() && document.body()->hasChildren())
    return false;

  if (document.head() && document.head()->hasChildren())
    return false;

  if (document.ParentDocument())
    return false;

  // No contents and not a child document, return true if about::blank.
  return document.Url().IsAboutBlankURL();
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
  const AtomicString& role_str =
      AXObject::AriaAttribute(*elem, html_names::kRoleAttr);
  if (role_str.empty())
    return false;

  return ui::IsCellOrTableHeader(
      AXObject::FirstValidRoleInRoleString(role_str));
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
    return !is_after ||
           !AXObject::HasAriaAttribute(*elem, html_names::kRoleAttr);
  }

  // If adjacent to a whitespace character, the current space can be ignored.
  if (layout_object->IsText()) {
    auto* layout_text = To<LayoutText>(layout_object);
    if (layout_text->HasEmptyText())
      return false;
    if (layout_text->TransformedText()
            .Impl()
            ->ContainsOnlyWhitespaceOrEmpty()) {
      return true;
    }
    auto adjacent_char =
        is_after ? layout_text->FirstCharacterAfterWhitespaceCollapsing()
                 : layout_text->LastCharacterAfterWhitespaceCollapsing();
    return adjacent_char == ' ' || adjacent_char == '\n' ||
           adjacent_char == '\t';
  }

  // Keep spaces between images and other visible content, in case the image is
  // used inline as a symbol mimicking text. This is not necessary for other
  // types of images, such as a canvas.
  // Note that relying the layout object via IsLayoutImage() was a cause of
  // flakiness, as the layout object could change to a LayoutBlockFlow if the
  // image failed to load. However, we still check IsLayoutImage() in order
  // to detect CSS images, which don't have the same issue of changing layout.
  if (layout_object->IsLayoutImage() || IsA<HTMLImageElement>(elem) ||
      (IsA<HTMLInputElement>(elem) &&
       To<HTMLInputElement>(elem)->FormControlType() ==
           FormControlType::kInputImage)) {
    return false;
  }

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

bool CanIgnoreSpace(const LayoutText& layout_text) {
  Node* node = layout_text.GetNode();

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

bool IsLayoutTextRelevantForAccessibility(const LayoutText& layout_text) {
  if (!layout_text.Parent())
    return false;

  Node* node = layout_text.GetNode();
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

  // Use previous decision for this whitespace. This is helpful for performance,
  // consistency (flake reduction) and code simplicity, as we do not need to
  // recompute block subtrees when inline nodes change. It also helps ensure
  // that whitespace nodes do not change whether they store a layout object
  // at inopportune times.
  // TODO(accessibility) Convert this method and callers of it to member
  // methods so we can access whitespace_ignored_map_ directly.
  AXObjectCacheImpl* cache = static_cast<AXObjectCacheImpl*>(
      node->GetDocument().ExistingAXObjectCache());
  auto& whitespace_ignored_map = cache->whitespace_ignored_map();
  DOMNodeId whitespace_node_id = node->GetDomNodeId();
  auto it = whitespace_ignored_map.find(whitespace_node_id);
  if (it != whitespace_ignored_map.end()) {
    return it->value;
  }

  // Compute ignored value for whitespace and record decision.
  bool ignore_whitespace = CanIgnoreSpace(layout_text);
  // Memoize the result.
  whitespace_ignored_map.insert(whitespace_node_id, ignore_whitespace);
  return ignore_whitespace;
}

bool IsHiddenTextNodeRelevantForAccessibility(const Text& text_node,
                                              bool is_display_locked) {
  // Children of an <iframe> tag will always be replaced by a new Document,
  // either loaded from the iframe src or empty. In fact, we don't even parse
  // them and they are treated like one text node. Consider irrelevant.
  if (AXObject::IsFrame(text_node.parentElement()))
    return false;

  // Layout has more info available to determine if whitespace is relevant.
  // If display-locked, layout object may be missing or stale:
  // Assume that all display-locked text nodes are relevant, but only create
  // an AXNodeObject in order to avoid using a stale layout object.
  if (is_display_locked)
    return true;

  // If unrendered + no parent, it is in a shadow tree. Consider irrelevant.
  if (!text_node.parentElement()) {
    DCHECK(text_node.IsInShadowTree());
    return false;
  }

  // If unrendered and in <canvas>, consider even whitespace relevant.
  if (text_node.parentElement()->IsInCanvasSubtree())
    return true;

  // Must be unrendered because of CSS. Consider relevant if non-whitespace.
  // Allowing rendered non-whitespace to be considered relevant will allow
  // use for accessible relations such as labelledby and describedby.
  return !text_node.ContainsOnlyWhitespaceOrEmpty();
}

bool IsShadowContentRelevantForAccessibility(const Node* node) {
  DCHECK(node->ContainingShadowRoot());

  // Return false if inside a shadow tree of something that can't have children,
  // for example, an <img> has a user agent shadow root containing a <span> for
  // the alt text. Do not create an accessible for that as it would be unable
  // to have a parent that has it as a child.
  if (!AXObject::CanHaveChildren(To<Element>(*node->OwnerShadowHost()))) {
    return false;
  }

  if (node->IsInUserAgentShadowRoot()) {
    // Native <img> create extra child nodes to hold alt text, which are not
    // allowed as children. Note: images can have image map children, but these
    // are moved from the <map> descendants and are not descendants of the
    // image. See AXNodeObject::AddImageMapChildren().
    if (IsA<HTMLImageElement>(node->OwnerShadowHost())) {
      return false;
    }
    // aria-hidden subtrees can safely be pruned when it's in a UA shadow root.
    // Make an exception for file input, which needs to gather its name from
    // aria-hidden contents.
    if (const Element* element = DynamicTo<Element>(node)) {
      if (element->FastGetAttribute(html_names::kAriaHiddenAttr) == "true") {
        return false;
      }

      // <select>'s autofill preview should not be included in the accessibility
      // tree.
      if (element->ShadowPseudoId() ==
          shadow_element_names::kSelectAutofillPreview) {
        return false;
      }
    }
  }

  // If inside a <object>/<embed>, the shadow content is relevant only if it is
  // fallback content.
  if (const HTMLPlugInElement* plugin_element =
          DynamicTo<HTMLPlugInElement>(node->OwnerShadowHost())) {
    return plugin_element->UseFallbackContent();
  }

  // All other shadow content is relevant.
  return true;
}

bool IsLayoutObjectRelevantForAccessibility(const LayoutObject& layout_object) {
  if (layout_object.IsAnonymous()) {
    // Anonymous means there is no DOM node, and it's been inserted by the
    // layout engine within the tree. An example is an anonymous block that is
    // inserted as a parent of an inline where there are block siblings.
    return AXObjectCacheImpl::IsRelevantPseudoElementDescendant(layout_object);
  }

  if (layout_object.IsText())
    return IsLayoutTextRelevantForAccessibility(To<LayoutText>(layout_object));

  // An AXImageMapLink will be created, which does not store the LayoutObject.
  if (IsA<HTMLAreaElement>(layout_object.GetNode()))
    return false;

  return true;
}

bool IsSubtreePrunedForAccessibility(const Element* node) {
  if (IsA<HTMLAreaElement>(node) && !IsA<HTMLMapElement>(node->parentNode()))
    return true;  // <area> without parent <map> is not relevant.

  if (IsA<HTMLMapElement>(node))
    return true;  // Contains children for an img, but is not its own object.

  if (node->HasTagName(html_names::kColgroupTag) ||
      node->HasTagName(html_names::kColTag)) {
    return true;  // Affects table layout, but doesn't get it's own AXObject.
  }

  if (node->IsPseudoElement()) {
    if (!AXObjectCacheImpl::IsRelevantPseudoElement(*node))
      return true;
  }

  if (const HTMLSlotElement* slot =
          ToHTMLSlotElementIfSupportsAssignmentOrNull(node)) {
    if (!AXObjectCacheImpl::IsRelevantSlotElement(*slot))
      return true;
  }

  // An HTML <title> does not require an AXObject: the document's name is
  // retrieved directly via the inner text.
  if (IsA<HTMLTitleElement>(node))
    return true;

  // ::scroll-marker pseudo elements are attached to the
  // ::scroll-marker-group's layout object, so don't create them
  // here, instead they will be created as part of the
  // AXNodeObject::AddPseudoElementChildrenFromLayoutTree.
  if (node->IsScrollMarkerPseudoElement()) {
    return true;
  }

  return false;
}

// Return true if node is head/style/script or any descendant of those.
// Also returns true for descendants of any type of frame, because the frame
// itself is in the tree, but not DOM descendants (their contents are in a
// different document).
bool IsInPrunableHiddenContainerInclusive(const Node& node,
                                          bool parent_ax_known,
                                          bool is_display_locked) {
  int max_depth_to_check = INT_MAX;
  if (parent_ax_known) {
    // Optimization: only need to check the current object if the parent the
    // parent_ax is already known, because it we are attempting to add this
    // object from something already relevant in the AX tree, and therefore
    // can't be inside a <head>, <style>, <script> or SVG <style> element.
    // However, there is an edge case that if it is display locked content
    // we must also check the parent, which can be visible and included
    // in the tree. This edge case is handled to satisfy tests and is not
    // likely to be a real-world condition.
    max_depth_to_check = is_display_locked ? 2 : 1;
  }

  for (const Node* ancestor = &node; ancestor;
       ancestor = ancestor->parentElement()) {
    // Objects inside <head> are pruned.
    if (IsA<HTMLHeadElement>(ancestor))
      return true;
    // Objects inside a <style> are pruned.
    if (IsA<HTMLStyleElement>(ancestor))
      return true;
    // Objects inside a <script> are true.
    if (IsA<HTMLScriptElement>(ancestor))
      return true;
    // Elements inside of a frame/iframe are true unless inside a document
    // that is a child of the frame. In the case where descendants are allowed,
    // they will be in a different document, and therefore this loop will not
    // reach the frame/iframe.
    if (AXObject::IsFrame(ancestor))
      return true;
    // Style elements in SVG are not display: none, unlike HTML style
    // elements, but they are still hidden along with their contents and thus
    // treated as true for accessibility.
    if (IsA<SVGStyleElement>(ancestor))
      return true;

    if (--max_depth_to_check <= 0)
      break;
  }

  // All other nodes are relevant, even if hidden.
  return false;
}

// -----------------------------------------------------------------------------
// DetermineAXObjectType() determines what type of AXObject should be created
// for the given node and layout_object.
// * Pass in the Node, the LayoutObject or both.
// * Passing in |parent_ax_known| when there is  known parent is an optimization
// and does not affect the return value.
// Some general rules:
// * If neither the node nor layout object are relevant for accessibility, will
// return kPruneSubtree, which will cause no AXObject to be created, and
// result in the entire subtree being pruned at that point.
// * If the node is part of a forbidden subtree, then kPruneSubtree is used.
// * If both the node and layout are relevant, kCreateFromLayout is preferred,
// otherwise: kCreateFromNode for relevant nodes, kCreateFromLayout for layout.
// -----------------------------------------------------------------------------
AXObjectType DetermineAXObjectType(const Node* node,
                                   const LayoutObject* layout_object,
                                   bool parent_ax_known = false) {
  DCHECK(layout_object || node);
  bool is_display_locked =
      node ? IsDisplayLocked(node) : IsDisplayLocked(layout_object);
  if (is_display_locked)
    layout_object = nullptr;
  DCHECK(!node || !layout_object || layout_object->GetNode() == node);

  bool is_node_relevant = false;

  if (node) {
    if (!node->isConnected()) {
      return kPruneSubtree;
    }

    if (node->ContainingShadowRoot() &&
        !IsShadowContentRelevantForAccessibility(node)) {
      return kPruneSubtree;
    }

    if (!IsA<Element>(node) && !IsA<Text>(node)) {
      // All remaining types, such as the document node, doctype node.
      return layout_object ? kCreateFromLayout : kPruneSubtree;
    }

    if (const Element* element = DynamicTo<Element>(node)) {
      if (IsSubtreePrunedForAccessibility(element))
        return kPruneSubtree;
      else
        is_node_relevant = true;
    } else {  // Text is the only remaining type.
      if (layout_object) {
        // If there's layout for this text, it will either be pruned or an
        // AXNodeObject with layout will be created for it. The logic of whether
        // to return kCreateFromLayout or kPruneSubtree will come purely from
        // is_layout_relevant further down.
        return IsLayoutObjectRelevantForAccessibility(*layout_object)
                   ? kCreateFromLayout
                   : kPruneSubtree;
      } else {
        // Otherwise, base the decision on the best info we have on the node.
        is_node_relevant = IsHiddenTextNodeRelevantForAccessibility(
            To<Text>(*node), is_display_locked);
      }
    }
  }

  bool is_layout_relevant =
      layout_object && IsLayoutObjectRelevantForAccessibility(*layout_object);

  // Prune if neither the LayoutObject nor Node are relevant.
  if (!is_layout_relevant && !is_node_relevant)
    return kPruneSubtree;

  // If a node is not rendered, prune if it is in head/style/script or a DOM
  // descendant of an iframe.
  if (!is_layout_relevant && IsInPrunableHiddenContainerInclusive(
                                 *node, parent_ax_known, is_display_locked)) {
    return kPruneSubtree;
  }

  return is_layout_relevant ? kCreateFromLayout : kCreateFromNode;
}

const int kSizeMb = 1000000;
const int kSize10Mb = 10 * kSizeMb;
const int kSizeGb = 1000 * kSizeMb;
const int kBucketCount = 100;

void LogNodeDataSizeDistribution(
    const ui::AXNodeData::AXNodeDataSize& node_data_size) {
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Accessibility.Performance.AXObjectCacheImpl.Incremental.Int",
      base::saturated_cast<int>(node_data_size.int_attribute_size), 1,
      kSize10Mb, kBucketCount);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Accessibility.Performance.AXObjectCacheImpl.Incremental.Float",
      base::saturated_cast<int>(node_data_size.float_attribute_size), 1,
      kSize10Mb, kBucketCount);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Accessibility.Performance.AXObjectCacheImpl.Incremental.Bool",
      base::saturated_cast<int>(node_data_size.bool_attribute_size), 1, kSizeMb,
      kBucketCount);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Accessibility.Performance.AXObjectCacheImpl.Incremental.String",
      base::saturated_cast<int>(node_data_size.string_attribute_size), 1,
      kSizeGb, kBucketCount);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Accessibility.Performance.AXObjectCacheImpl.Incremental.IntList",
      base::saturated_cast<int>(node_data_size.int_list_attribhute_size), 1,
      kSize10Mb, kBucketCount);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Accessibility.Performance.AXObjectCacheImpl.Incremental.StringList",
      base::saturated_cast<int>(node_data_size.string_list_attribute_size), 1,
      kSizeGb, kBucketCount);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Accessibility.Performance.AXObjectCacheImpl.Incremental.HTML",
      base::saturated_cast<int>(node_data_size.html_attribute_size), 1, kSizeGb,
      kBucketCount);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Accessibility.Performance.AXObjectCacheImpl.Incremental.ChildIds",
      base::saturated_cast<int>(node_data_size.child_ids_size), 1, kSize10Mb,
      kBucketCount);
}

// Rreturns true if `layout_object`represent a text that is all white spaces and
// is all collapsed. This means that this object will not be accessed by an
// InlineCursor.
static bool IsAllCollapsedWhiteSpace(const LayoutObject& layout_object) {
  if (const auto* layout_text = DynamicTo<LayoutText>(&layout_object)) {
    if (layout_text->StyleRef().ShouldCollapseWhiteSpaces() &&
        layout_text->TransformedText()
            .IsAllSpecialCharacters<Character::IsCollapsibleSpace>()) {
      return true;
    }
  }
  return false;
}

// Returns the previous LayoutObject representing text that is in the same line
// as `layout_object`, nullptr if there are none. `layout_object`must be a child
// of `block_flow`.
LayoutObject* PreviousLayoutObjectTextOnLine(
    const LayoutObject& layout_object,
    const LayoutBlockFlow& block_flow) {
  LayoutObject* previous = layout_object.PreviousInPreOrder(&block_flow);
  while (previous) {
    if (IsA<LayoutText>(previous)) {
      InlineCursor cursor;
      cursor.MoveToIncludingCulledInline(*previous);
      while (cursor) {
        if (cursor.Current()->IsInlineBox() ||
            cursor.Current()->IsLineBreak()) {
          return nullptr;
        }
        cursor.MoveToNextForSameLayoutObject();
      }

      return previous;
    }

    previous = previous->PreviousInPreOrder(&block_flow);
  }

  return nullptr;
}

}  // namespace

#define DEBUG_STRING_CASE(ReasonName)                   \
  case AXObjectCacheImpl::TreeUpdateReason::ReasonName: \
    return #ReasonName

static std::string TreeUpdateReasonAsDebugString(
    const AXObjectCacheImpl::TreeUpdateReason& reason) {
  switch (reason) {
    DEBUG_STRING_CASE(kActiveDescendantChanged);
    DEBUG_STRING_CASE(kAriaExpandedChanged);
    DEBUG_STRING_CASE(kAriaOwnsChanged);
    DEBUG_STRING_CASE(kAriaPressedChanged);
    DEBUG_STRING_CASE(kAriaSelectedChanged);
    DEBUG_STRING_CASE(kCSSAnchorChanged);
    DEBUG_STRING_CASE(kDelayEventFromPostNotification);
    DEBUG_STRING_CASE(kDidShowMenuListPopup);
    DEBUG_STRING_CASE(kEditableTextContentChanged);
    DEBUG_STRING_CASE(kFocusableChanged);
    DEBUG_STRING_CASE(kIdChanged);
    DEBUG_STRING_CASE(kNodeIsAttached);
    DEBUG_STRING_CASE(kNodeGainedFocus);
    DEBUG_STRING_CASE(kNodeLostFocus);
    DEBUG_STRING_CASE(kPostNotificationFromHandleLoadComplete);
    DEBUG_STRING_CASE(kPostNotificationFromHandleLoadStart);
    DEBUG_STRING_CASE(kPostNotificationFromHandleScrolledToAnchor);
    DEBUG_STRING_CASE(kReferenceTargetChanged);
    DEBUG_STRING_CASE(kRemoveValidationMessageObjectFromFocusedUIElement);
    DEBUG_STRING_CASE(
        kRemoveValidationMessageObjectFromValidationMessageObject);
    DEBUG_STRING_CASE(kRoleChangeFromAriaHasPopup);
    DEBUG_STRING_CASE(kRoleChangeFromImageMapName);
    DEBUG_STRING_CASE(kRoleChangeFromRoleOrType);
    DEBUG_STRING_CASE(kRoleMaybeChangedFromEventListener);
    DEBUG_STRING_CASE(kRoleMaybeChangedFromHref);
    DEBUG_STRING_CASE(kRoleMaybeChangedOnSelect);
    DEBUG_STRING_CASE(kSectionOrRegionRoleMaybeChangedFromLabel);
    DEBUG_STRING_CASE(kSectionOrRegionRoleMaybeChangedFromLabelledBy);
    DEBUG_STRING_CASE(kSectionOrRegionRoleMaybeChangedFromTitle);
    DEBUG_STRING_CASE(kTextChangedOnNode);
    DEBUG_STRING_CASE(kTextChangedOnClosestNodeForLayoutObject);
    DEBUG_STRING_CASE(kTextMarkerDataAdded);
    DEBUG_STRING_CASE(kUpdateActiveMenuOption);
    DEBUG_STRING_CASE(kUpdateAriaOwns);
    DEBUG_STRING_CASE(kUpdateTableRole);
    DEBUG_STRING_CASE(kUseMapAttributeChanged);
    DEBUG_STRING_CASE(kValidationMessageVisibilityChanged);
    DEBUG_STRING_CASE(kChildrenChanged);
    DEBUG_STRING_CASE(kMarkAXObjectDirty);
    DEBUG_STRING_CASE(kMarkAXSubtreeDirty);
    DEBUG_STRING_CASE(kTextChangedOnLayoutObject);
  }

  NOTREACHED_IN_MIGRATION();
  return "Unknown";
}

std::string AXObjectCacheImpl::TreeUpdateParams::ToString() {
  std::ostringstream str;
  str << "Tree update: " << TreeUpdateReasonAsDebugString(update_reason);
  if (event != ax::mojom::blink::Event::kNone) {
    str << " with event " << event;
  }

  return str.str();
}

// static
AXObjectCache* AXObjectCacheImpl::Create(Document& document,
                                         const ui::AXMode& ax_mode) {
  return MakeGarbageCollected<AXObjectCacheImpl>(document, ax_mode);
}

AXObjectCacheImpl::AXObjectCacheImpl(Document& document,
                                     const ui::AXMode& ax_mode)
    : document_(document),
#if DCHECK_IS_ON()
      // TODO(accessibility): turn on the UI checker for devtools.
      internal_ui_checker_on_(GetDocument().Url().Protocol() == "chrome"),
#else
      internal_ui_checker_on_(false),
#endif
      ax_mode_(ax_mode),
      validation_message_axid_(0),
      active_aria_modal_dialog_(nullptr),
      render_accessibility_host_(document.GetExecutionContext()),
      ax_tree_source_(BlinkAXTreeSource::Create(*this)) {
  lifecycle_.AdvanceTo(AXObjectCacheLifecycle::kDeferTreeUpdates);
}

AXObjectCacheImpl::~AXObjectCacheImpl() {
  CHECK(HasBeenDisposed());
}

// This is called shortly before the AXObjectCache is deleted.
// The destruction of the AXObjectCache will do most of the cleanup.
void AXObjectCacheImpl::Dispose() {
  lifecycle_.AdvanceTo(AXObjectCacheLifecycle::kDisposing);

  // Detach all objects now. This prevents more work from occurring if we wait
  // for the rendering engine to detach each node individually, because that
  // will cause the renderer to attempt to potentially repair parents, and
  // detach each child individually as Detach() calls ClearChildren().
  for (auto& entry : objects_) {
    AXObject* obj = entry.value;
    obj->Detach();
  }

  // Destroy any pending task to serialize the tree.
  weak_factory_for_serialization_pipeline_.Invalidate();
  weak_factory_for_loc_updates_pipeline_.Invalidate();

  lifecycle_.AdvanceTo(AXObjectCacheLifecycle::kDisposed);
}

void AXObjectCacheImpl::AddInspectorAgent(InspectorAccessibilityAgent* agent) {
  agents_.insert(agent);
}

void AXObjectCacheImpl::RemoveInspectorAgent(
    InspectorAccessibilityAgent* agent) {
  agents_.erase(agent);
}

void AXObjectCacheImpl::EnsureRelationCacheAndInitialTree() {
  if (!relation_cache_) {
    relation_cache_ = std::make_unique<AXRelationCache>(this);
    relation_cache_->Init();

    // Build out initial tree so that AXObjects exist for
    // AXRelationCache::ProcessUpdatesWithCleanLayout();
    // Creating the root will cause its descendants to be created as well.
    if (!Get(document_)) {
      CreateAndInit(document_, document_->GetLayoutView(), nullptr);
    }
  }
}

void AXObjectCacheImpl::EnsureSerializer() {
  if (!ax_tree_serializer_) {
    ax_tree_serializer_ = std::make_unique<ui::AXTreeSerializer<
        const AXObject*, HeapVector<Member<const AXObject>>, ui::AXTreeUpdate*,
        ui::AXTreeData*, ui::AXNodeData>>(ax_tree_source_,
                                          /*crash_on_error*/ true);
  }
}

AXObject* AXObjectCacheImpl::Root() {
  if (AXObject* root = Get(document_)) {
    return root;
  }

  CommitAXUpdates(GetDocument(), /*force*/ true);
  return Get(document_);
}

AXObject* AXObjectCacheImpl::ObjectFromAXID(AXID id) const {
  auto it = objects_.find(id);
  return it != objects_.end() ? it->value : nullptr;
}

AXObject* AXObjectCacheImpl::FirstObjectWithRole(ax::mojom::blink::Role role) {
  const AXObject* root = Root();
  if (!root || root->IsDetached()) {
    return nullptr;
  }
  return root->FirstObjectWithRole(role);
}

Node* AXObjectCacheImpl::FocusedNode() const {
  Node* focused_node = document_->FocusedElement();
  if (!focused_node)
    focused_node = document_;

  // The custom select's button is not included in the a11y hierarchy. Treat
  // focus on the button as if it's on the <select>.
  focused_node = RetargetInput(focused_node);

  // A popup is showing: return the focus within instead of the focus in the
  // main document. Do not do this for HTML <select>, which has special
  // focus manager using the kActiveDescendantId.
  if (GetPopupDocumentIfShowing() && !IsA<HTMLSelectElement>(focused_node)) {
    if (Node* focus_in_popup = GetPopupDocumentIfShowing()->FocusedElement())
      return focus_in_popup;
  }

  return focused_node;
}

void AXObjectCacheImpl::UpdateLifecycleIfNeeded(Document& document) {
  DCHECK(document.defaultView());
  DCHECK(document.GetFrame());
  DCHECK(document.View());

  document.View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kAccessibility);
}

void AXObjectCacheImpl::UpdateAXForAllDocuments() {
#if DCHECK_IS_ON()
  DCHECK(!IsFrozen())
      << "Don't call UpdateAXForAllDocuments() here; layout and a11y are "
         "already clean at the start of serialization.";
  DCHECK(!updating_layout_and_ax_) << "Undesirable recursion.";
  base::AutoReset<bool> updating(&updating_layout_and_ax_, true);
#endif

  // First update the layout for the main and popup document.
  UpdateLifecycleIfNeeded(GetDocument());
  if (Document* popup_document = GetPopupDocumentIfShowing())
    UpdateLifecycleIfNeeded(*popup_document);

  // Next flush all accessibility events and dirty objects, for both the main
  // and popup document, and update tree if needed.
  if (IsDirty() || HasObjectsPendingSerialization()) {
    CommitAXUpdates(GetDocument(), /*force*/ true);
  }
}

AXObject* AXObjectCacheImpl::FocusedObject() const {
#if DCHECK_IS_ON()
  DCHECK(GetDocument().Lifecycle().GetState() >=
         DocumentLifecycle::kAfterPerformLayout);
  if (GetPopupDocumentIfShowing()) {
    DCHECK(GetPopupDocumentIfShowing()->Lifecycle().GetState() >=
           DocumentLifecycle::kAfterPerformLayout);
  }
#endif

  AXObject* obj = Get(FocusedNode());
  if (!obj) {
    // In rare cases it's possible for the focus to not exist in the tree.
    // An example would be a focused element inside of an image map that
    // gets trimmed.
    // In these cases, treat the focus as on the root object itself, so that
    // AT users have some starting point.
    DLOG(ERROR) << "The focus was not part of the a11y tree: " << FocusedNode();
    return Get(document_);
  }

  // The HTML element, for example, is focusable but has an AX object that is
  // not included in the tree.
  if (!obj->IsIncludedInTree()) {
    obj = obj->ParentObjectIncludedInTree();
  }

  return obj;
}

AXObject* AXObjectCacheImpl::EnsureFocusedObject() {
  DCHECK(lifecycle_.StateAllowsImmediateTreeUpdates());
  AXObject* obj = Get(FocusedNode());

  if (obj) {
    if (!obj->IsAriaHidden()) {
      return obj;
    }
    // Repair illegal usage of aria-hidden: it should never contain the focus.
    // The aria-hidden will be ignored when this occurs.
    DiscardBadAriaHiddenBecauseOfFocus(*obj);
  }

  // Now return the focused object of its included in the tree, otherwise
  // return an included ancestor of the focus.
  obj = FocusedObject();
  CHECK(obj) << "Object could not be recreated with aria-hidden off.";
  CHECK(!obj->IsAriaHidden());
  return obj;
}

const ui::AXMode& AXObjectCacheImpl::GetAXMode() {
  return ax_mode_;
}

void AXObjectCacheImpl::SetAXMode(const ui::AXMode& ax_mode) {
  ax_mode_ = ax_mode;
}

AXObject* AXObjectCacheImpl::Get(const LayoutObject* layout_object,
                                 AXObject* parent_for_repair) const {
  if (!layout_object)
    return nullptr;

  if (Node* node = layout_object->GetNode()) {
    // If there is a node, it is preferred for backing the AXObject.
    DCHECK(!layout_object_mapping_.Contains(layout_object));
    return Get(node);
  }

  auto it_id = layout_object_mapping_.find(layout_object);
  if (it_id == layout_object_mapping_.end()) {
    return nullptr;
  }
  AXID ax_id = it_id->value;
  DCHECK(!WTF::IsHashTraitsDeletedValue<HashTraits<AXID>>(ax_id));

  auto it_result = objects_.find(ax_id);
  AXObject* result = it_result != objects_.end() ? it_result->value : nullptr;
  DCHECK(result) << "Had AXID for Node but no entry in objects_";
  DCHECK(result->IsAXNodeObject());
  // Do not allow detached objects except when disposing entire tree.
  DCHECK(!result->IsDetached() || IsDisposing())
      << "Detached AXNodeObject in map: " << "AXID#" << ax_id
      << " LayoutObject=" << layout_object;

  if (result->ParentObject()) {
    DCHECK(!parent_for_repair || parent_for_repair == result->ParentObject())
        << "If there is both a previous parent, and a parent supplied for "
           "repair, they must match.";
  } else if (parent_for_repair) {
    result->SetParent(parent_for_repair);
  }

  // If there is no node for the AXObject, then it is an anonymous layout
  // object (e.g. a pseudo-element or object introduced to match the structure
  // of content). Such objects can only be created or destroyed via creation of
  // their parents and recursion via AddPseudoElementChildrenFromLayoutTree.
  DCHECK(!result->IsMissingParent() || !result->GetNode())
      << "Had AXObject but is missing parent: " << layout_object << " "
      << result;

  return result;
}

AXObject* AXObjectCacheImpl::Get(const Node* node) const {
  if (!node)
    return nullptr;

  AXID node_id = static_cast<AXID>(DOMNodeIds::ExistingIdForNode(node));
  if (!node_id) {
    // An ID hasn't yet been generated for this DOM node, but ::CreateAndInit()
    // will ensure a DOMNodeID is generated by using node->GetDomNodeId().
    // Therefore if an id doesn't exist for a DOM node, it means that it can't
    // have an associated AXObject.
    return nullptr;
  }

  auto it_result = objects_.find(node_id);
  if (it_result == objects_.end()) {
    return nullptr;
  }

  AXObject* result = it_result->value;
  DCHECK(result) << "AXID#" << node_id
                 << " in map, but matches an AXObject of null, for " << node;

  // When shutting down, allow detached nodes to be in the map, and do not
  // attempt invalidations.
  if (IsDisposing()) {
    return result->IsDetached() ? nullptr : result;
  }

  DCHECK(!result->IsDetached()) << "Detached object was in map.";

  return result;
}

AXObject* AXObjectCacheImpl::Get(AbstractInlineTextBox* inline_text_box) const {
  if (!inline_text_box)
    return nullptr;

  auto it_ax = inline_text_box_object_mapping_.find(inline_text_box);
  AXID ax_id =
      it_ax != inline_text_box_object_mapping_.end() ? it_ax->value : 0;
  if (!ax_id)
    return nullptr;
  DCHECK(!WTF::IsHashTraitsEmptyOrDeletedValue<HashTraits<AXID>>(ax_id));

  auto it_result = objects_.find(ax_id);
  AXObject* result = it_result != objects_.end() ? it_result->value : nullptr;
#if DCHECK_IS_ON()
  DCHECK(result) << "Had AXID for inline text box but no entry in objects_";
  DCHECK(result->IsAXInlineTextBox());
  // Do not allow detached objects except when disposing entire tree.
  DCHECK(!result->IsDetached() || IsDisposing())
      << "Detached AXInlineTextBox in map: " << "AXID#" << ax_id
      << " Node=" << inline_text_box->GetText();
#endif
  return result;
}

AXObject* AXObjectCacheImpl::GetPositionedObjectForAnchor(const AXObject* obj) {
  return relation_cache_->GetPositionedObjectForAnchor(obj);
}

AXObject* AXObjectCacheImpl::GetAnchorForPositionedObject(const AXObject* obj) {
  return relation_cache_->GetAnchorForPositionedObject(obj);
}

AXObject* AXObjectCacheImpl::GetAXImageForMap(const HTMLMapElement& map) {
  // Find first child node of <map> that has an AXObject and return it's
  // parent, which should be a native image.
  Node* child = NodeTraversal::FirstChild(map);
  while (child) {
    if (AXObject* ax_child = Get(child)) {
      if (AXObject* ax_image = ax_child->ParentObject()) {
        if (ax_image->IsDetached()) {
          return nullptr;
        }
        DCHECK(IsA<HTMLImageElement>(ax_image->GetNode()))
            << "Expected image AX parent of <map>'s DOM child, got: "
            << ax_image->GetNode() << "\n* Map's DOM child was: " << child
            << "\n* ax_image: " << ax_image;
        return ax_image;
      }
    }
    child = NodeTraversal::NextSibling(*child);
  }
  return nullptr;
}

AXObject* AXObjectCacheImpl::CreateFromRenderer(LayoutObject* layout_object) {
  Node* node = layout_object->GetNode();

  // media element
  if (node && node->IsMediaElement())
    return AccessibilityMediaElement::Create(layout_object, *this);

  if (node && node->IsMediaControlElement())
    return AccessibilityMediaControl::Create(layout_object, *this);

  if (auto* html_input_element = DynamicTo<HTMLInputElement>(node)) {
    FormControlType type = html_input_element->FormControlType();
    if (type == FormControlType::kInputRange) {
      return MakeGarbageCollected<AXSlider>(layout_object, *this);
    }
  }

  if (IsA<HTMLProgressElement>(node)) {
    return MakeGarbageCollected<AXProgressIndicator>(layout_object, *this);
  }

  return MakeGarbageCollected<AXNodeObject>(layout_object, *this);
}

// static
bool AXObjectCacheImpl::IsRelevantSlotElement(const HTMLSlotElement& slot) {
  DCHECK(AXObject::CanSafelyUseFlatTreeTraversalNow(slot.GetDocument()));
  DCHECK(slot.SupportsAssignment());

  if (slot.IsInUserAgentShadowRoot() &&
      IsA<HTMLSelectElement>(slot.OwnerShadowHost())) {
    return slot.GetIdAttribute() == shadow_element_names::kSelectOptions;
  }

  // HasAssignedNodesNoRecalc() will return false when  the slot is not in the
  // flat tree. We must also return true when the slot has ordinary children
  // (fallback content).
  return slot.HasAssignedNodesNoRecalc() || slot.hasChildren();
}

// static
bool AXObjectCacheImpl::IsRelevantPseudoElement(const Node& node) {
  DCHECK(node.IsPseudoElement());
  if (!node.GetLayoutObject())
    return false;

  // ::before, ::after, ::marker, ::scroll-marker, ::scroll-*-buttons and
  // ::scroll-marker-group are relevant. Allowing these pseudo elements ensures
  // that all visible descendant pseudo content will be reached, despite only
  // being able to walk layout inside of pseudo content. However, AXObjects
  // aren't created for
  // ::first-letter subtrees. The text of ::first-letter is already available in
  // the child text node of the element that the CSS ::first letter applied to.
  if (To<PseudoElement>(node).CanGenerateContent()) {
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
  if (node.IsBackdropPseudoElement() || node.IsViewTransitionPseudoElement()) {
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
      if (AXNodeObject::GetCSSAltText(To<Element>(ancestor->GetNode()))) {
        return false;
      }
      return IsRelevantPseudoElement(*ancestor->GetNode());
    }
    if (!ancestor->IsAnonymous())
      return false;
  }
}

AXObject* AXObjectCacheImpl::CreateFromNode(Node* node) {
  if (auto* area = DynamicTo<HTMLAreaElement>(node))
    return MakeGarbageCollected<AXImageMapLink>(area, *this);

  return MakeGarbageCollected<AXNodeObject>(node, *this);
}

AXObject* AXObjectCacheImpl::CreateFromInlineTextBox(
    AbstractInlineTextBox* inline_text_box) {
  return MakeGarbageCollected<AXInlineTextBox>(inline_text_box, *this);
}

AXObject* AXObjectCacheImpl::GetOrCreate(const Node* node, AXObject* parent) {
  return GetOrCreate(const_cast<Node*>(node), parent);
}

AXObject* AXObjectCacheImpl::GetOrCreate(Node* node, AXObject* parent) {
  CHECK(lifecycle_.StateAllowsImmediateTreeUpdates())
      << "Only create AXObjects while processing AX events and tree: " << node
      << " " << *this;

  if (!node)
    return nullptr;

  CHECK(parent);

  if (AXObject* obj = Get(node)) {
    // The object already exists.
    CHECK(!obj->IsDetached());
    if (!obj->IsMissingParent()) {
      return obj;
    }

    // The parent is provided when the object is being added to the parent.
    // This is expected when re-adding a child to a parent via
    // AXNodeObject::AddChildren(), as the parent on previous children
    // will have been cleared immediately before re-adding any of them.
    obj->SetParent(parent);
    CHECK(!obj->IsMissingParent());
    return obj;
  }

  return CreateAndInit(node, node->GetLayoutObject(), parent);
}

// Caller must provide a node, a layout object, or both (where they match).
AXObject* AXObjectCacheImpl::CreateAndInit(Node* node,
                                           LayoutObject* layout_object,
                                           AXObject* parent) {
  // New AXObjects cannot be created when the tree is frozen.
  // In this state, the tree should already be complete because
  // of FinalizeTree().
  CHECK(lifecycle_.StateAllowsImmediateTreeUpdates())
      << "Only create AXObjects while processing AX events and tree: " << node
      << " " << layout_object << " " << *this;

#if DCHECK_IS_ON()
  DCHECK(node || layout_object);
  DCHECK(!node || !layout_object || layout_object->GetNode() == node);
  DCHECK(parent || node == document_);
  DCHECK(!parent || parent->CanHaveChildren());
  DCHECK(GetDocument().Lifecycle().GetState() >=
         DocumentLifecycle::kAfterPerformLayout)
      << "Unclean document at lifecycle "
      << GetDocument().Lifecycle().ToString();
#endif  // DCHECK_IS_ON()

  if (IsA<Document>(node) && parent) {
    // Root of a popup document:
    if (IsA<HTMLSelectElement>(parent->GetNode())) {
      // HTML <select> has a popup that duplicates nodes from the main
      // document, and therefore we ignore that popup and any nodes in it.
      return nullptr;
    }
    if (!GetPopupDocumentIfShowing()) {
      // The popup document is either not showing yet, or is no longer showing.
      return nullptr;
    }
    // All other document nodes with a parent must match the current popup.
    CHECK_EQ(node, GetPopupDocumentIfShowing());
  }

  // Determine the type of accessibility object to be created.
  AXObjectType ax_type = DetermineAXObjectType(node, layout_object, parent);
  if (ax_type == kPruneSubtree) {
    return nullptr;
  }

#if DCHECK_IS_ON()
  if (node) {
    DCHECK(layout_object || ax_type != kCreateFromLayout);
    DCHECK(node->isConnected());
    DCHECK(node->GetDocument().GetFrame())
        << "Creating AXObject in a dead document: " << node;
    DCHECK(node->IsElementNode() || node->IsTextNode() ||
           node->IsDocumentNode())
        << "Should only attempt to create AXObjects for the following types of "
           "node types: document, element and text."
        << "\n* Node is: " << node;
  } else {
    // No node: will create an AXNodeObject using the LayoutObject.
    DCHECK(layout_object->GetDocument().GetFrame())
        << "Creating AXObject in a dead document: " << layout_object;
    DCHECK_EQ(ax_type, kCreateFromLayout);
    DCHECK(!IsA<LayoutView>(layout_object))
        << "AXObject for document is always created with a node.";
  }
#endif

  // If there is a DOM node, use its dom_node_id, otherwise, generate an AXID.
  // The dom_node_id can be used even if there is also a layout object.
  AXID axid;
  if (node) {
    axid = static_cast<AXID>(node->GetDomNodeId());
    if (ax_tree_serializer_) {
      // In the case where axid is being reused, because a previous AXObject
      // existed for the same node, ensure that the serializer sees it as new.
      ax_tree_serializer_->MarkNodeDirty(axid);
    }
  } else {
    axid = GenerateAXID();
  }
  DCHECK(!base::Contains(objects_, axid));

  // Create the new AXObject.
  AXObject* new_obj = nullptr;
  if (ax_type == kCreateFromLayout) {
    // Prefer to create from renderer if there is a layout object because
    // AXLayoutObjects can provide information about bounding boxes.
    if (!node) {
      DCHECK(!layout_object_mapping_.Contains(layout_object))
          << "Already have an AXObject for " << layout_object;
      layout_object_mapping_.Set(layout_object, axid);
    }
    new_obj = CreateFromRenderer(layout_object);
  } else {
    new_obj = CreateFromNode(node);
  }
  DCHECK(new_obj) << "Could not create AXObject.";

  // Give the AXObject its ID and initialize.
  AssociateAXID(new_obj, axid);
  new_obj->Init(parent);

#if DCHECK_IS_ON()
  Element* element = DynamicTo<Element>(node);
  if (element && !element->IsPseudoElement()) {
    // Ensure that the relation cache is properly initialized with information
    // from this element.
    relation_cache_->CheckRelationsCached(*element);
  }
#endif

  // Eagerly fill out new subtrees.
  new_obj->UpdateChildrenIfNecessary();

  return new_obj;
}

AXObject* AXObjectCacheImpl::GetOrCreate(LayoutObject* layout_object,
                                         AXObject* parent) {
  CHECK(lifecycle_.StateAllowsImmediateTreeUpdates())
      << layout_object << " " << *this;

  CHECK(parent);

  if (!layout_object)
    return nullptr;

  if (AXObject* obj = Get(layout_object, parent)) {
    return obj;
  }

  return CreateAndInit(layout_object->GetNode(), layout_object, parent);
}

AXObject* AXObjectCacheImpl::GetOrCreate(AbstractInlineTextBox* inline_text_box,
                                         AXObject* parent) {
  CHECK(lifecycle_.StateAllowsImmediateTreeUpdates())
      << "Only create AXObjects while processing AX events and tree." << *this;

  if (!inline_text_box)
    return nullptr;

  if (!parent) {
    LayoutText* layout_text_parent = inline_text_box->GetLayoutText();
    DCHECK(layout_text_parent);
    parent = Get(layout_text_parent);
    if (!parent) {
      DCHECK(inline_text_box->GetText().ContainsOnlyWhitespaceOrEmpty() ||
             IsFrozen() ||
             !IsRelevantPseudoElementDescendant(*layout_text_parent))
          << "No parent for non-whitespace inline textbox: "
          << layout_text_parent
          << "\nParent of parent: " << layout_text_parent->Parent();
      return nullptr;
    }
  }

  // Inline textboxes are included if and only if the parent is unignored.
  // If the parent is ignored but included in tree, the inline textbox is
  // still withheld.
  if (parent->IsIgnored()) {
    return nullptr;
  }

  if (AXObject* obj = Get(inline_text_box)) {
#if DCHECK_IS_ON()
    DCHECK(!obj->IsDetached())
        << "AXObject for inline text box should not be detached: " << obj;
    // AXInlineTextbox objects can't get a new parent, unlike other types of
    // accessible objects that can get a new parent because they moved or
    // because of aria-owns.
    // AXInlineTextbox objects are only added via AddChildren() on static text
    // or line break parents. The children are cleared, and detached from their
    // parent before AddChildren() executes. There should be no previous parent.
    DCHECK(parent->RoleValue() == ax::mojom::blink::Role::kStaticText ||
           parent->RoleValue() == ax::mojom::blink::Role::kLineBreak);
    DCHECK(!obj->ParentObject() || obj->ParentObject() == parent)
        << "Mismatched old and new parent:" << "\n* Old parent: "
        << obj->ParentObject() << "\n* New parent: " << parent;
    DCHECK(ui::CanHaveInlineTextBoxChildren(parent->RoleValue()))
        << "Unexpected parent of inline text box: " << parent->RoleValue();
#endif
    DCHECK(obj->ParentObject() == parent);
    return obj;
  }

  // New AXObjects cannot be created when the tree is frozen.
  if (IsFrozen()) {
    return nullptr;
  }

  AXObject* new_obj = CreateFromInlineTextBox(inline_text_box);

  AXID axid = AssociateAXID(new_obj);

  inline_text_box_object_mapping_.Set(inline_text_box, axid);
  new_obj->Init(parent);
  return new_obj;
}

void AXObjectCacheImpl::Remove(AXObject* object, bool notify_parent) {
  DCHECK(object);
  if (object->IsAXInlineTextBox()) {
    Remove(object->GetInlineTextBox(), notify_parent);
  } else if (object->GetNode()) {
    Remove(object->GetNode(), notify_parent);
  } else if (object->GetLayoutObject()) {
    Remove(object->GetLayoutObject(), notify_parent);
  } else {
    Remove(object->AXObjectID(), notify_parent);
  }
}

// This is safe to call even if there isn't a current mapping.
// This is called by other Remove() methods, called by Blink for DOM and layout
// changes, iterating over all removed content in the subtree:
// - When a DOM subtree is removed, it is called with the root node first, and
//   then descending down into the subtree.
// - When layout for a subtree is detached, it is called on layout objects,
//   starting with leaves and moving upward, ending with the subtree root.
void AXObjectCacheImpl::Remove(AXID ax_id, bool notify_parent) {
  CHECK(lifecycle_.StateAllowsRemovingAXObjects()) << *this;

  if (!ax_id)
    return;

  // First, fetch object to operate some cleanup functions on it.
  auto it = objects_.find(ax_id);
  AXObject* obj = it != objects_.end() ? it->value : nullptr;
  if (!obj)
    return;

#if DCHECK_IS_ON()
  if (obj->CachedIsIncludedInTree()) {
    --included_node_count_;
  }
#endif

  if (!IsDisposing() && !HasBeenDisposed()) {
    if (notify_parent && !obj->IsMissingParent()) {
      ChildrenChangedOnAncestorOf(obj);
    }
    // TODO(aleventhal) This is for web tests only, in order to record MarkDirty
    // events. Is there a way to avoid these calls for normal browsing?
    // Maybe we should use dependency injection from AccessibilityController.
    if (auto* client = GetWebLocalFrameClient()) {
      client->HandleAXObjectDetachedForTest(ax_id);
    }
  }

  // Remove references to AXID before detaching, so that nothing will retrieve a
  // detached object, which is illegal.
  RemoveReferencesToAXID(ax_id);

  // RemoveReferencesToAXID can cause the object to detach, in this case,
  // fail gracefully rather than attempting to double detach.
  DUMP_WILL_BE_CHECK(!obj->IsDetached()) << obj;
  if (obj->IsDetached()) {
    // TODO(accessibility): Remove early return and change above assertion
    // to CHECK() once this no longer occurs.
    return;
  }

  obj->Detach();

  // Remove the object.
  // TODO(accessibility) We don't use the return value, can we use .erase()
  // and it will still make sure that the object is cleaned up?
  objects_.Take(ax_id);

  // Removing an aria-modal dialog can affect the entire tree.
  if (active_aria_modal_dialog_ &&
      active_aria_modal_dialog_ == obj->GetElement()) {
    Settings* settings = GetSettings();
    if (settings && settings->GetAriaModalPrunesAXTree()) {
      MarkDocumentDirty();
    }
    active_aria_modal_dialog_ = nullptr;
  }
}

void AXObjectCacheImpl::Remove(LayoutObject* layout_object,
                               bool notify_parent) {
  CHECK(layout_object);

  if (IsA<LayoutView>(layout_object)) {
    // A document is being destroyed.
    // This code is only reached when it is a popup being destroyed.
    // TODO(accessibility) Can we remove this case since Blink calls
    // RemovePopup(document) for us?
    DCHECK(!popup_document_ ||
           popup_document_ == &layout_object->GetDocument());
    // Popup has been destroyed.
    if (popup_document_) {
      RemovePopup(popup_document_);
    }
  }

  // If a DOM node is present, it will have been used to back the AXObject, in
  // which case we need to call Remove(node) instead.
  if (Node* node = layout_object->GetNode()) {
    // Pseudo elements are a special case. The entire subtree needs to be marked
    // dirty so that it is recomputed (it is disappearing or changing).
    if (node->IsPseudoElement()) {
      MarkSubtreeDirty(node);
    }

    if (IsA<HTMLImageElement>(node)) {
      // If an image is removed, ensure its entire subtree is deleted as there
      // may have been children supplied via a map.
      if (auto* layout_image =
              DynamicTo<LayoutImage>(node->GetLayoutObject())) {
        if (auto* map = layout_image->ImageMap()) {
          if (map->ImageElement() == node) {
            RemoveSubtree(map, /*remove_root*/ false);
          }
        }
      }
    }

    Remove(node, notify_parent);
    return;
  }

  auto iter = layout_object_mapping_.find(layout_object);
  if (iter == layout_object_mapping_.end())
    return;

  AXID ax_id = iter->value;
  DCHECK(ax_id);

  layout_object_mapping_.erase(iter);
  Remove(ax_id, false);
}

// This is safe to call even if there isn't a current mapping.
void AXObjectCacheImpl::Remove(Node* node) {
  // Ensure that our plugin serializer, if it exists, is properly
  // reset. Paired with AXNodeObject::Detach.
  if (IsA<HTMLEmbedElement>(node)) {
    ResetPluginTreeSerializer();
  }

  Remove(node, /* notify_parent */ true);
}

void AXObjectCacheImpl::Remove(Node* node, bool notify_parent) {
  DCHECK(node);
  LayoutObject* layout_object = node->GetLayoutObject();
  DCHECK(!layout_object || layout_object_mapping_.find(layout_object) ==
                               layout_object_mapping_.end())
      << "AXObject cannot be backed by both a layout object and node.";

  AXID axid = node->GetDomNodeId();
  whitespace_ignored_map_.erase(axid);

  if (node == active_aria_modal_dialog_) {
    UpdateActiveAriaModalDialog(FocusedNode());
  }

  DCHECK_GE(axid, 1);
  Remove(axid, notify_parent);
}

void AXObjectCacheImpl::RemovePopup(Document* popup_document) {
  // The only 2 documents that partake in the cache are the main document and
  // the popup document. This method is only be called for the popup document,
  // because if the main document is shutting down, the cache is disposed.
  DCHECK(popup_document);

  // This can be called even when GetPopupDocumentIfShowing() when the popup
  // is from a <select size=1>, and in order to avoid duplicate objects, which
  // treat that situations as if there is no popup showing.
  if (!GetPopupDocumentIfShowing()) {
    return;
  }
  DCHECK(IsPopup(*popup_document)) << "Use Dispose() to remove main document.";
  RemoveSubtree(popup_document);

  popup_document_ = nullptr;
  pending_events_to_serialize_.clear();
  tree_update_callback_queue_popup_.clear();
}

// This is safe to call even if there isn't a current mapping.
void AXObjectCacheImpl::Remove(AbstractInlineTextBox* inline_text_box) {
  Remove(inline_text_box, /* notify_parent */ true);
}

void AXObjectCacheImpl::Remove(AbstractInlineTextBox* inline_text_box,
                               bool notify_parent) {
  if (!inline_text_box)
    return;

  auto iter = inline_text_box_object_mapping_.find(inline_text_box);
  if (iter == inline_text_box_object_mapping_.end())
    return;

  AXID ax_id = iter->value;
  inline_text_box_object_mapping_.erase(iter);

  Remove(ax_id, notify_parent);
}

void AXObjectCacheImpl::RemoveIncludedSubtree(AXObject* object,
                                              bool remove_root) {
  DCHECK(object);
  if (object->IsDetached()) {
    return;
  }

  for (const auto& ax_child : object->CachedChildrenIncludingIgnored()) {
    RemoveIncludedSubtree(ax_child, /* remove_root */ true);
  }
  if (remove_root) {
    Remove(object, /* notify_parent */ false);
  }
}

void AXObjectCacheImpl::RemoveAXObjectsInLayoutSubtree(
    LayoutObject* subtree_root) {
  Remove(subtree_root, /*notify_parent*/ true);

  LayoutObject* iter = subtree_root;
  while ((iter = iter->NextInPreOrder(subtree_root)) != nullptr) {
    Remove(iter, /*notify_parent*/ false);
  }
}

void AXObjectCacheImpl::RemoveSubtree(const Node* node, bool remove_root) {
  if (!node || !node->isConnected()) {
    return;
  }
  RemoveSubtree(node, remove_root, /*notify_parent*/ true);
}

void AXObjectCacheImpl::RemoveSubtree(const Node* node) {
  RemoveSubtree(node, /*remove_root*/ true);
}

void AXObjectCacheImpl::RemoveSubtree(const Node* node,
                                      bool remove_root,
                                      bool notify_parent) {
  DCHECK(node);
  AXObject* object = Get(node);
  if (!object && !remove_root) {
    // Nothing remaining to do for this subtree. Already removed.
    return;
  }

  if (const HTMLMapElement* map_element = DynamicTo<HTMLMapElement>(node)) {
    // If this node is an image map, it is necessary to notify the <img> node
    // associated with this map that its children will be deleted. The a11y tree
    // will add the children of the image map as children of the image itself
    // (see AXNodeObject::AddImageMapChildren for more details). However, the
    // dom node traversal below would delete these children without notifying
    // their parent that children will change, so this special check here is a
    // must. For all other cases, this is not necessary because the parent is
    // part of the subtree removal or will be notified via notify_parent defined
    // above.
    if (AXObject* image_ax_object = GetAXImageForMap(*map_element)) {
      // Note here that an image will only be returned if the map has children
      // and at least one of them points to an image, so it is guaranteed that
      // we are not notifying a parent if children are not being removed.
      // **important**: this call must come before the node traversal remove
      // below since that could remove a child which would cause it to not point
      // to its image parent, making it impossible to notify the parent.
      NotifyParentChildrenChanged(image_ax_object);
    }
  }

  // Remove children found through dom traversal.
  for (Node* child_node = NodeTraversal::FirstChild(*node); child_node;
       child_node = NodeTraversal::NextSibling(*child_node)) {
    RemoveSubtree(child_node, /* remove_root */ true,
                  /* notify_parent */ false);
  }

  if (!object) {
    return;
  }

  // When removing children, use the cached children to avoid creating a child
  // just to destroy it.
  for (AXObject* ax_included_child : object->CachedChildrenIncludingIgnored()) {
    if (ax_included_child->ParentObjectIfPresent() != object) {
      continue;
    }
    if (ui::CanHaveInlineTextBoxChildren(object->RoleValue())) {
      // Just remove child inline textboxes, don't use their node which is the
      // same as that static text's parent and would cause an infinite loop.
      Remove(ax_included_child, /* notify_parent */ false);
    } else if (ax_included_child->GetNode()) {
      DCHECK(ax_included_child->GetNode() != node);
      RemoveSubtree(ax_included_child->GetNode(),
                    /* remove_root */ true,
                    /* notify_parent */ false);
    } else {
      RemoveIncludedSubtree(ax_included_child, /* remove_root */ true);
    }
  }

  // The code below uses ChildrenChangedWithCleanLayout() instead of
  // notify_parent param in Remove(), which would be queued, and it needs to
  // happen immediately.
  AXObject* parent_to_notify = nullptr;
  if (notify_parent) {
    // Find the parent to notify:
    // If the root is being removed, then it's the root's parent.
    // If the root isn't being removed, its child subtrees are being removed,
    // and thus the root itself is the parent who's children are changing.
    parent_to_notify = remove_root ? object->ParentObjectIfPresent() : object;
  }
  if (remove_root) {
    Remove(object, /* notify_parent */ false);
  }
  if (parent_to_notify) {
    NotifyParentChildrenChanged(parent_to_notify);
  }
}

// All generated AXIDs are negative, ranging from kFirstGeneratedRendererNodeID
// to kLastGeneratedRendererNodeID, in order to avoid conflict with the ids
// reused from dom_node_ids, which are positive, and generated IDs on the
// browser side, which are negative, starting at -1.
AXID AXObjectCacheImpl::GenerateAXID() const {
  // The first id is close to INT_MIN/2, leaving plenty of room for negative
  // generated IDs both here and on the browser side, but starting at an even
  // number makes it easier to read when debugging.
  static AXID last_used_id = ui::kFirstGeneratedRendererNodeID;

  // Generate a new ID.
  AXID obj_id = last_used_id;
  do {
    if (--obj_id == ui::kLastGeneratedRendererNodeID) {
      // This is very unlikely to happen, but if we find that it happens, we
      // could gracefully turn off a11y instead of crashing the renderer.
      CHECK(!has_axid_generator_looped_)
          << "Not enough room more generated accessibility objects.";
      has_axid_generator_looped_ = true;
      obj_id = ui::kFirstGeneratedRendererNodeID;
    }
  } while (has_axid_generator_looped_ && objects_.Contains(obj_id));

  DCHECK(!WTF::IsHashTraitsEmptyOrDeletedValue<HashTraits<AXID>>(obj_id));

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

  AXID new_axid = use_axid ? use_axid : GenerateAXID();

  bool should_have_node_id = obj->IsAXNodeObject() && obj->GetNode();
  DCHECK_EQ(should_have_node_id, IsDOMNodeID(new_axid))
      << "An AXID is also a DOMNodeID (positive integer) if any only if the "
         "AXObject is an AXNodeObject with a DOM node.";

  obj->SetAXObjectID(new_axid);
  objects_.Set(new_axid, obj);

  return new_axid;
}

void AXObjectCacheImpl::RemoveReferencesToAXID(AXID obj_id) {
  DCHECK(!WTF::IsHashTraitsDeletedValue<HashTraits<AXID>>(obj_id));

  // Clear AXIDs from maps. Note: do not need to erase id from
  // changed_bounds_ids_, a set which is cleared each time
  // SerializeLocationChanges() is finished. Also, do not need to erase id from
  // invalidated_ids_main_ or invalidated_ids_popup_, which are cleared each
  // time ProcessInvalidatedObjects() finishes, and having extra ids in those
  // sets is not harmful.

  cached_bounding_boxes_.erase(obj_id);

  if (IsDOMNodeID(obj_id)) {
    // Optimization: these maps only contain ids for AXObjects with a DOM node.
    fixed_or_sticky_node_ids_.erase(obj_id);
    // Only objects with a DOM node can be in the relation cache.
    if (relation_cache_) {
      relation_cache_->RemoveAXID(obj_id);
    }
    // Allow the new AXObject for the same node to be serialized correctly.
    nodes_with_pending_children_changed_.erase(obj_id);
  } else {
    // Non-DOM ids should never find their way into these maps.
    DCHECK(!fixed_or_sticky_node_ids_.Contains(obj_id));
    DCHECK(!nodes_with_pending_children_changed_.Contains(obj_id));
  }
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
    InvalidateBoundingBox(id);
}

bool AXObjectCacheImpl::CanDeferTreeUpdate(Document* tree_update_document) {
  DCHECK(lifecycle_.StateAllowsDeferTreeUpdates()) << *this;
  DCHECK(!IsFrozen());

  if (!IsActive(GetDocument()) || tree_updates_paused_)
    return false;

  // Ensure the tree update document is in a good state.
  if (!tree_update_document || !IsActive(*tree_update_document)) {
    return false;
  }

  if (tree_update_document != document_) {
    // If the popup_document_ is null, throw this tree update away, because:
    // - Updates that occur BEFORE the popup is tracked in a11y don't matter,
    // as we will build the entire popup's AXObject subtree once we are
    // notified about the popup.
    // - Updates that occur AFTER the popup is no longer tracked could occur
    // while the popup is currently closing, in which case the updates are no
    // longer useful.
    if (!popup_document_) {
      return false;
    }
    // If we are queuing an update to a document other than the main document,
    // then it must be in an active popup document. The cache would never
    // receive notifications from other documents.
    DUMP_WILL_BE_CHECK_EQ(tree_update_document, popup_document_)
        << "Update in non-main, non-popup document: "
        << tree_update_document->Url().GetString();
  }

  return true;
}

bool AXObjectCacheImpl::PauseTreeUpdatesIfQueueFull() {
  // Check the main document's queue. If there are too many entries, pause all
  // updates and resume later after rebuilding the tree from scratch.
  // Popup is excluded because it's controlled by us and will not have too many
  // updates. In the case of a web page having too many updates, we need to
  // clear all queues, including the popup's.
  if (tree_update_callback_queue_main_.size() >= max_pending_updates_) {
    UpdateNumTreeUpdatesQueuedBeforeLayoutHistogram();
    tree_updates_paused_ = true;
    LOG(INFO) << "Accessibility tree update queue is too big, updates have "
                 "been paused";
    // Clear updates from both documents.
    tree_update_callback_queue_main_.clear();
    tree_update_callback_queue_popup_.clear();
    pending_events_to_serialize_.clear();
    return true;
  }

  return false;
}

void AXObjectCacheImpl::DeferTreeUpdate(
    AXObjectCacheImpl::TreeUpdateReason update_reason,
    Node* node,
    ax::mojom::blink::Event event) {
  CHECK(node);
  CHECK(lifecycle_.StateAllowsDeferTreeUpdates()) << *this;

  Document& tree_update_document = node->GetDocument();
  if (!CanDeferTreeUpdate(&tree_update_document)) {
    return;
  }

  if (PauseTreeUpdatesIfQueueFull()) {
    return;
  }

  TreeUpdateCallbackQueue& queue =
      GetTreeUpdateCallbackQueue(tree_update_document);

  TreeUpdateParams* tree_update = MakeGarbageCollected<TreeUpdateParams>(
      node, 0u, ComputeEventFrom(), active_event_from_action_,
      ActiveEventIntents(), update_reason, event);

  queue.push_back(tree_update);

  if (AXObject* obj = Get(node)) {
    obj->InvalidateCachedValues();
  }

  // These events are fired during RunPostLifecycleTasks(),
  // ensure there is a document lifecycle update scheduled.
  if (IsImmediateProcessingRequired(tree_update)) {
    // Ensure that processing of tree updates occurs immediately in cases
    // where a user action such as focus or selection occurs, so that the user
    // gets immediate feedback.
    ScheduleImmediateSerialization();
  } else {
    // Otherwise, batch updates to improve performance.
    ScheduleAXUpdate();
  }
}
void AXObjectCacheImpl::DeferTreeUpdate(
    AXObjectCacheImpl::TreeUpdateReason update_reason,
    AXObject* obj,
    ax::mojom::blink::Event event,
    bool invalidate_cached_values) {
  // Called for updates that do not have a DOM node, e.g. a children or text
  // changed event that occurs on an anonymous layout block flow.
  CHECK(obj);
  CHECK(lifecycle_.StateAllowsDeferTreeUpdates()) << *this;

  if (obj->IsDetached()) {
    return;
  }

  CHECK(obj->AXObjectID());

  Document* tree_update_document = obj->GetDocument();

  if (!CanDeferTreeUpdate(tree_update_document)) {
    return;
  }

  if (PauseTreeUpdatesIfQueueFull()) {
    return;
  }

  TreeUpdateCallbackQueue& queue =
      GetTreeUpdateCallbackQueue(*tree_update_document);

  queue.push_back(MakeGarbageCollected<TreeUpdateParams>(
      nullptr, obj->AXObjectID(), ComputeEventFrom(), active_event_from_action_,
      ActiveEventIntents(), update_reason, event));

  if (invalidate_cached_values) {
    obj->InvalidateCachedValues();
  }

  // These events are fired during RunPostLifecycleTasks(),
  // ensure there is a document lifecycle update scheduled.
  ScheduleAXUpdate();
}

void AXObjectCacheImpl::SelectionChanged(Node* node) {
  if (!node)
    return;

  PostNotification(&GetDocument(),
                   ax::mojom::blink::Event::kDocumentSelectionChanged);

  // If there is a text control, mark it dirty to serialize
  // IntAttribute::kTextSelStart/kTextSelEnd changes.
  // TODO(accessibility) Remove once we remove kTextSelStart/kTextSelEnd.
  if (TextControlElement* text_control = EnclosingTextControl(node))
    MarkElementDirty(text_control);
}

void AXObjectCacheImpl::StyleChanged(const LayoutObject* layout_object,
                                     bool visibility_or_inertness_changed) {
  DCHECK(layout_object);
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION();
  AXObject* ax_object = Get(layout_object->GetNode());
  if (!ax_object) {
    // No object exists to mark dirty yet -- there can sometimes be a layout in
    // the initial empty document, or style has changed before the object cache
    // becomes aware that the node exists. It's too early for the style change
    // to be useful.
    return;
  }

  if (visibility_or_inertness_changed) {
    ChildrenChanged(ax_object);
    ChildrenChanged(ax_object->ParentObject());
  }
  MarkAXObjectDirty(ax_object);
}

void AXObjectCacheImpl::CSSAnchorChanged(const LayoutObject* positioned_obj) {
  if (Node* node = positioned_obj->GetNode()) {
    DeferTreeUpdate(TreeUpdateReason::kCSSAnchorChanged, node);
  }
}

void AXObjectCacheImpl::TextChanged(Node* node) {
  if (!node)
    return;

  // A text changed event is redundant with children changed on the same node.
  if (AXID node_id = static_cast<AXID>(node->GetDomNodeId())) {
    if (nodes_with_pending_children_changed_.find(node_id) !=
        nodes_with_pending_children_changed_.end()) {
      return;
    }
  }

  DeferTreeUpdate(TreeUpdateReason::kTextChangedOnNode, node);
}

// Return a node for the current layout object or ancestor layout object.
Node* AXObjectCacheImpl::GetClosestNodeForLayoutObject(
    const LayoutObject* layout_object) {
  if (!layout_object) {
    return nullptr;
  }
  Node* node = layout_object->GetNode();
  return node ? node : GetClosestNodeForLayoutObject(layout_object->Parent());
}

void AXObjectCacheImpl::TextChanged(const LayoutObject* layout_object) {
  if (!layout_object)
    return;

  // The node may be null when the text changes on an anonymous layout object,
  // such as a layout block flow that is inserted to parent an inline object
  // when it has a block sibling.
  Node* node = GetClosestNodeForLayoutObject(layout_object);
  if (node) {
    // If the text changed in a pseudo element, rebuild the entire subtree.
    if (node->IsPseudoElement()) {
      RemoveAXObjectsInLayoutSubtree(node->GetLayoutObject());
    } else if (AXID node_id = static_cast<AXID>(node->GetDomNodeId())) {
      // Text changed is redundant with children changed on the same node.
      if (base::Contains(nodes_with_pending_children_changed_, node_id)) {
        return;
      }
    }

    DeferTreeUpdate(TreeUpdateReason::kTextChangedOnClosestNodeForLayoutObject,
                    node);
    return;
  }

  if (Get(layout_object)) {
    DeferTreeUpdate(TreeUpdateReason::kTextChangedOnLayoutObject,
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
        obj->IsIncludedInTree()) {
      if (obj->ShouldLoadInlineTextBoxes()) {
        // Update inline text box children.
        ChildrenChangedWithCleanLayout(optional_node_for_relation_update, obj);
        return;
      }
    }

    MarkAXObjectDirtyWithCleanLayout(obj);
  }

  if (optional_node_for_relation_update) {
    CHECK(relation_cache_);
    relation_cache_->UpdateRelatedTree(optional_node_for_relation_update, obj);
  }
}

void AXObjectCacheImpl::TextChangedWithCleanLayout(Node* node) {
  if (!node)
    return;

  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));
  TextChangedWithCleanLayout(node, Get(node));
}

bool AXObjectCacheImpl::HasBadAriaHidden(const AXObject& obj) const {
  return nodes_with_bad_aria_hidden.Contains(obj.AXObjectID());
}

void AXObjectCacheImpl::DiscardBadAriaHiddenBecauseOfElement(
    const AXObject& obj) {
  bool is_first_time =
      nodes_with_bad_aria_hidden.insert(obj.AXObjectID()).is_new_entry;

  if (!is_first_time) {
    return;
  }

  Element& element = *obj.GetElement();
  element.AddConsoleMessage(
      mojom::blink::ConsoleMessageSource::kRendering,
      mojom::blink::ConsoleMessageLevel::kError,
      String::Format(
          "Blocked aria-hidden on a <%s> element because it would hide the "
          "entire accessibility tree from assistive technology users. For more "
          "details, see the aria-hidden section of the WAI-ARIA specification "
          "at https://w3c.github.io/aria/#aria-hidden.",
          element.TagQName().ToString().Ascii().c_str()));
}

void AXObjectCacheImpl::DiscardBadAriaHiddenBecauseOfFocus(AXObject& obj) {
  // aria-hidden markup requires an element.
  Element& focused_element = *obj.GetElement();

  // Traverse all the way to the root in case there are multiple
  // ancestors with aria-hidden. Any aria-hidden="true" on any ancestor will
  // be ignored.
  AXObject* bad_aria_hidden_ancestor = nullptr;
  for (AXObject* ancestor = &obj; ancestor;
       ancestor = ancestor->ParentObject()) {
    if (ancestor->IsAriaAttributeTrue(html_names::kAriaHiddenAttr)) {
      bad_aria_hidden_ancestor = ancestor;
      nodes_with_bad_aria_hidden.insert(ancestor->AXObjectID());
    }
  }
  // Invalidate the subtree and rebuild it now that this aria-hidden has
  // been marked as bad and will be ignored.
  CHECK(bad_aria_hidden_ancestor)
      << "An aria-hidden node did not have an aria-hidden ancestor.";

  if (bad_aria_hidden_ancestor->GetElement()) {
    bad_aria_hidden_ancestor->GetElement()->AddConsoleMessage(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kError,
        String::Format(
            "Blocked aria-hidden on an element because its descendant retained "
            "focus. The focus must not be hidden from assistive technology "
            "users. Avoid using aria-hidden on a focused element or its "
            "ancestor. Consider using the inert attribute instead, which will "
            "also prevent focus. For more details, see the aria-hidden section "
            "of the WAI-ARIA specification at "
            "https://w3c.github.io/aria/#aria-hidden.\n"
            "Element with focus: %s\nAncestor with aria-hidden: ",
            focused_element.TagQName().ToString().Ascii().c_str()));
  }

  Node* bad_aria_hidden_ancestor_node = bad_aria_hidden_ancestor->GetNode();
  AXObject* ancestor_to_rebuild = bad_aria_hidden_ancestor->ParentObject();
  while (ancestor_to_rebuild) {
    ancestor_to_rebuild->SetNeedsToUpdateChildren();
    if (ancestor_to_rebuild->IsIncludedInTree()) {
      break;
    }
    ancestor_to_rebuild = ancestor_to_rebuild->ParentObject();
  }
  // The root is always included, so ancestor_to_rebuild is never null.
  DCHECK(ancestor_to_rebuild);
  RemoveSubtree(bad_aria_hidden_ancestor_node);
  CHECK(bad_aria_hidden_ancestor->IsDetached());

  ancestor_to_rebuild->UpdateChildrenIfNecessary();
  bad_aria_hidden_ancestor = Get(bad_aria_hidden_ancestor_node);
  CHECK(!bad_aria_hidden_ancestor->IsAriaHiddenRoot());
  CHECK(!bad_aria_hidden_ancestor->IsAriaHidden());
}

void AXObjectCacheImpl::DocumentTitleChanged() {
  DocumentLifecycle::DisallowTransitionScope disallow(document_->Lifecycle());

  AXObject* root = Get(document_);
  if (root)
    PostNotification(root, ax::mojom::blink::Event::kDocumentTitleChanged);
}

bool AXObjectCacheImpl::IsReadyToProcessTreeUpdatesForNode(const Node* node) {
  DCHECK(node);

  // The maximum number of nodes after whitespace is parsed before a tree update
  // should occur. The value was chosen based on what was needed to eliminate
  // flakiness in existing tests and may need adjustment. Example: the
  // `AccessibilityCSSPseudoElementsSeparatedByWhitespace` Yielding Parser test
  // regularly fails if this value is set to 2, but passes if set to at least 3.
  constexpr int kMaxAllowedTreeUpdatePauses = 3;

  // If we have a node that must be fully parsed before updates can continue,
  // we're ready to process tree updates only if that node has finished parsing
  // its children. In this scenario, the maximum number of tree update pauses is
  // irrelevant.
  if (node_to_parse_before_more_tree_updates_) {
    return node_to_parse_before_more_tree_updates_->IsFinishedParsingChildren();
  }

  // There should be no reason to pause for a script element. Plus if we pause
  // for the script element, the slow-document-load.html web test fails.
  if (IsA<HTMLScriptElement>(node)) {
    return true;
  }

  if (auto* text = DynamicTo<Text>(node)) {
    if (!text->ContainsOnlyWhitespaceOrEmpty()) {
      return true;
    }

    // Whitespace at the end of parsed content is a problem because we won't
    // know if that whitespace node is relevant until we have some text or a
    // block node. And we won't know the layout of a node at connection time.
    // Therefore, if this is a whitespace node, reset the maximum number of
    // allowed pauses and wait.
    allowed_tree_update_pauses_remaining_ = kMaxAllowedTreeUpdatePauses;
    return false;
  }

  // If the node following a whitespace node is a pseudo element, we won't have
  // its contents at the time the node is connected. Those contents can impact
  // the relevance of the whitespace node. So remain paused if node is a pseudo
  // element, without resetting the maximum number of allowed pauses.
  if (node->IsPseudoElement()) {
    return false;
  }

  // No new reason to pause, and there are no prior requested pauses remaining.
  if (!allowed_tree_update_pauses_remaining_) {
    return true;
  }

  // No new reason to pause, but we're not ready to unpause yet. So decrement
  // the number of pauses requested and wait for the next connected node.
  CHECK_GT(allowed_tree_update_pauses_remaining_, 0u);
  allowed_tree_update_pauses_remaining_--;
  return false;
}

void AXObjectCacheImpl::NodeIsConnected(Node* node) {
  if (IsParsingMainDocument()) {
    if (IsReadyToProcessTreeUpdatesForNode(node)) {
      node_to_parse_before_more_tree_updates_ = nullptr;
      allowed_tree_update_pauses_remaining_ = 0;
    }
  } else {
    // Handle case where neither NodeIsAttached() nor SubtreeIsAttached() will
    // be called for this node. This occurs for nodes that are added to
    // display:none subtrees. Ensure that these nodes partake in the AX tree.
    ChildrenChanged(node->parentNode());
  }

  // Process relations.
  if (Element* element = DynamicTo<Element>(node)) {
    if (relation_cache_) {
      // Register relation ids so that reverse relations can be computed.
      relation_cache_->CacheRelationIds(*element);
      ScheduleAXUpdate();
    }
    if (AXObject::HasARIAOwns(element)) {
      DeferTreeUpdate(TreeUpdateReason::kUpdateAriaOwns, element);
    }
    if (element->HasID()) {
      DeferTreeUpdate(TreeUpdateReason::kIdChanged, element);
    }
  }
}

void AXObjectCacheImpl::UpdateAriaOwnsWithCleanLayout(Node* node) {
  // Process any relation attributes that can affect ax objects already created.
  // Force computation of aria-owns, so that original parents that already
  // computed their children get the aria-owned children removed.
  if (IsA<Element>(node) && AXObject::HasARIAOwns(To<Element>(node))) {
    if (AXObject* obj = Get(node)) {
      CHECK(relation_cache_);
      relation_cache_->UpdateAriaOwnsWithCleanLayout(obj);
    }
  }
}

void AXObjectCacheImpl::SubtreeIsAttached(Node* node) {
  // If the node is the root of a display locked subtree, or was previously
  // display:none, the entire AXObject subtree needs to be destroyed and rebuilt
  // using AXNodeObjects with LayoutObjects.
  // TODO(accessibility): try to improve performance by keeping the existing
  // subtree but setting the LayoutObject and recomputing relevant values,
  // including the role and the ignored state.
  AXObject* obj = Get(node);
  if (!obj) {
    if (!node->GetLayoutObject() && !node->IsFinishedParsingChildren() &&
        !node_to_parse_before_more_tree_updates_) {
      // Unrendered subtrees that are not fully parsed are unsafe to
      // process until they are complete, because there are no NodeIsAttached()
      // signals for incrementally loaded content.
      node_to_parse_before_more_tree_updates_ = node;
    }

    // No AX subtree to invalidate: just add an AXObject for this node.
    // It will automatically add its subtree.
    ChildrenChanged(LayoutTreeBuilderTraversal::Parent(*node));
    // Ensure that aria-owns is updated on this element once the above
    // children changed causes it to have an AXObject.
    if (AXObject::HasARIAOwns(DynamicTo<Element>(node))) {
      DeferTreeUpdate(TreeUpdateReason::kUpdateAriaOwns, node);
    }
    return;
  }

  // Note that technically we do not need to remove the root node for a
  // display-locking (content-visibility) change, since it is only the
  // descendants that gain or lose their objects, but its easier to be
  // consistent here.
  RemoveSubtree(node);
}

void AXObjectCacheImpl::NodeIsAttached(Node* node) {
  CHECK(node);
  CHECK(node->isConnected());
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION();

  // Ensure that ChildrenChanged() occurs on the correct parent in the case
  // where Blink layout code did not have a corresponding LayoutObject parent
  // to fire ChildrenChanged() on, such as in a display:contents case.
  ChildrenChanged(AXObject::GetParentNodeForComputeParent(*this, node));

  // It normally is not necessary to process text nodes here, because we'll
  // also get a call for the attachment of the parent element. However in the
  // YieldingParser scenario, the `previousOnLineId` can be unexpectedly null
  // for whitespace-only nodes whose inclusion had not yet been determined.
  // Sample flake: AccessibilityContenteditableDocsLi. Therefore, find the
  // highest `LayoutInline` ancestor and mark it dirty.
  if (Text* text = DynamicTo<Text>(node)) {
    if (text->ContainsOnlyWhitespaceOrEmpty()) {
      if (auto* layout_object = node->GetLayoutObject()) {
        auto* layout_parent = layout_object->Parent();
        while (layout_parent && layout_parent->Parent() &&
               layout_parent->Parent()->IsLayoutInline()) {
          layout_parent = layout_parent->Parent();
        }
        MarkSubtreeDirty(layout_parent->GetNode());
      }
    }
    return;
  }

  Document* document = DynamicTo<Document>(node);
  if (document) {
    Element* focused_element = GetDocument().FocusedElement();
    // A popup is being shown.
    if (IsA<HTMLSelectElement>(focused_element)) {
      // HTML <select> has a popup that duplicates nodes from the main
      // document, and therefore we ignore that popup and any nodes in it.
      return;
    }
    DCHECK(*document != GetDocument());
    DCHECK(!popup_document_) << "Last popup was not cleared.";
    DCHECK(!popup_document_ || popup_document_ == document)
        << "Last popup was not cleared: " << (void*)popup_document_;
    popup_document_ = document;
    DCHECK(IsPopup(*document));
    // Fire children changed on the focused element that owns this popup.
    ChildrenChanged(focused_element);
    return;
  }
  if (node->GetLayoutObject()) {
    // Handle subtree that was previously display:none gaining layout.
    if (AXObject* obj = Get(node); obj && !obj->GetLayoutObject()) {
      // Had a previous AXObject, but wasn't an AXLayoutObject, even though
      // there is a layout object available.
      RemoveSubtree(node);
      return;
    }
    if ((IsA<HTMLTableElement>(node) || IsA<HTMLSelectElement>(node) ||
         node->GetLayoutObject()->IsAtomicInlineLevel()) &&
        !node->IsFinishedParsingChildren() &&
        !node_to_parse_before_more_tree_updates_) {
      // * Tables must be fully parsed before building, because many of the
      //   computed properties require the entire table.
      // * Custom selects must be fully parsed before building because of
      //   flakes where the entire subtree was not populated. The exact reason
      //   is unclear, but it could be related to the unique use of the flat
      //   vs. natural DOM tree.
      // TODO(accessibility) Fix root select issue, while still passing
      // All/YieldingParserDumpAccessibilityTreeTest.AccessibilityCustomSelect/blink.
      // * Inline text boxes must know their children in order to determine
      //   whether they can be ignored;
      node_to_parse_before_more_tree_updates_ = node;
    }

    // Rare edge case: if an image is added, it could have changed the order of
    // images with the same usemap in the document. Only the first image for a
    // given <map> should have the <area> children. Therefore, get the current
    // primary image before it's updated, and ensure its children are
    // recalculated.
    if (IsA<HTMLImageElement>(node)) {
      if (HTMLMapElement* map = AXObject::GetMapForImage(node)) {
        HTMLImageElement* primary_image_element = map->ImageElement();
        if (node != primary_image_element) {
          // This is a secondary image for its map, and therefore the map does
          // not apply to it. Make sure the primary image recomputes its
          // children.
          ChildrenChanged(primary_image_element);
        } else if (AXObject* ax_previous_parent = GetAXImageForMap(*map)) {
          // This is the primary image for its map and the map's children
          // were previously parented by an AXObject for an <img>
          if (ax_previous_parent->GetNode() != node) {
            // The previous AXObject parent for the maps children does not
            // match!
            ChildrenChanged(ax_previous_parent);
            ax_previous_parent->ClearChildren();
          }
        }
      }
    }
  }

  DeferTreeUpdate(TreeUpdateReason::kNodeIsAttached, node);
}

void AXObjectCacheImpl::NodeIsAttachedWithCleanLayout(Node* node) {
  if (!node || !node->isConnected()) {
    return;
  }

  Element* element = DynamicTo<Element>(node);

#if DCHECK_IS_ON()
  DCHECK(node->GetDocument().Lifecycle().GetState() >=
         DocumentLifecycle::kLayoutClean)
      << "Unclean document at lifecycle "
      << node->GetDocument().Lifecycle().ToString();
#endif  // DCHECK_IS_ON()

  if (AccessibleNode::GetPropertyOrARIAAttributeValue(
          element, AOMRelationProperty::kActiveDescendant)) {
    HandleActiveDescendantChangedWithCleanLayout(element);
  }

  AXObject* obj = Get(node);
  CHECK(obj);
  CHECK(obj->ParentObject());

  if (element && element->HasID()) {
    MaybeNewRelationTarget(*node, obj);
  }

  if (IsA<HTMLAreaElement>(node)) {
    ChildrenChangedWithCleanLayout(obj);
  }

  // Check if a row or cell's table changed to or from a data table.
  if (IsA<HTMLTableRowElement>(node) || IsA<HTMLTableCellElement>(node)) {
    Element* parent = node->parentElement();
    while (parent) {
      if (DynamicTo<HTMLTableElement>(parent)) {
        break;
      }
      parent = parent->parentElement();
    }
    if (parent) {
      UpdateTableRoleWithCleanLayout(parent);
    }
    TableCellRoleMaybeChanged(node);
  }
}

void AXObjectCacheImpl::NotifyParentChildrenChanged(AXObject* parent) {
  if (!parent) {
    return;
  }
  if (lifecycle_.StateAllowsImmediateTreeUpdates()) {
    ChildrenChangedWithCleanLayout(parent);
  } else {
    AXObject* ax_ancestor = ChildrenChanged(parent);
    if (!ax_ancestor) {
      return;
    }

    CHECK(!IsFrozen())
        << "Attempting to change children on an ancestor is dangerous during "
           "serialization, because the ancestor may have already been "
           "visited. Reaching this line indicates that AXObjectCacheImpl did "
           "not handle a signal and call ChildrenChanged() earlier."
        << "\nParent: " << parent << "\nAncestor: " << ax_ancestor;
  }
}

// Note: do not call this when a child is becoming newly included, because
// it will return early if |obj| was last known to be unincluded.
void AXObjectCacheImpl::ChildrenChangedOnAncestorOf(AXObject* obj) {
  DCHECK(obj);
  DCHECK(!obj->IsDetached());

  // Clear children of ancestors in order to ensure this detached object is not
  // cached in an ancestor's list of children:
  // Any ancestor up to the first included ancestor can contain the now-detached
  // child in it's cached children, and therefore must update children.
  NotifyParentChildrenChanged(obj->ParentObjectIfPresent());
}

void AXObjectCacheImpl::ChildrenChangedWithCleanLayout(AXObject* obj) {
  if (AXObject* ax_ancestor_for_notification = InvalidateChildren(obj)) {
    if (ax_ancestor_for_notification->GetNode() &&
        nodes_with_pending_children_changed_.Contains(
            ax_ancestor_for_notification->AXObjectID())) {
      return;
    }
    ChildrenChangedWithCleanLayout(ax_ancestor_for_notification->GetNode(),
                                   ax_ancestor_for_notification);
  }
}

AXObject* AXObjectCacheImpl::ChildrenChanged(AXObject* obj) {
  CHECK(lifecycle_.StateAllowsDeferTreeUpdates())
      << "Call ChildrenChangedWithCleanLayout() directly while processing "
         "deferred events."
      << *this;
  if (AXObject* ax_ancestor_for_notification = InvalidateChildren(obj)) {
    // Don't enqueue a deferred event on the same node more than once.
    CHECK(lifecycle_.StateAllowsDeferTreeUpdates());
    if (ax_ancestor_for_notification->GetNode() &&
        !nodes_with_pending_children_changed_
             .insert(ax_ancestor_for_notification->AXObjectID())
             .is_new_entry) {
      return nullptr;
    }

    DeferTreeUpdate(TreeUpdateReason::kChildrenChanged,
                    ax_ancestor_for_notification);
    return ax_ancestor_for_notification;
  }
  return nullptr;
}

AXObject* AXObjectCacheImpl::InvalidateChildren(AXObject* obj) {
  if (!obj)
    return nullptr;

  // Clear children of ancestors in order to ensure this detached object is not
  // cached an ancestor's list of children:
  AXObject* ancestor = obj;
  while (ancestor) {
    if (ancestor->NeedsToUpdateChildren() || ancestor->IsDetached())
      return nullptr;  // Processing has already occurred for this ancestor.
    ancestor->SetNeedsToUpdateChildren();

    // Any ancestor up to the first included ancestor can contain the
    // now-detached child in it's cached children, and therefore must update
    // children.
    if (ancestor->CachedIsIncludedInTree()) {
      break;
    }

    ancestor = ancestor->ParentObject();
  }

  // Only process ChildrenChanged() events on the included ancestor. This allows
  // deduping of ChildrenChanged() occurrences within the same subtree.
  // For example, if a subtree has unincluded children, but included
  // grandchildren have changed, only the root children changed needs to be
  // processed.
  if (!ancestor)
    return nullptr;

  // Return ancestor to fire children changed notification on.
  DCHECK(ancestor->CachedIsIncludedInTree())
      << "ChildrenChanged() must only be called on included nodes: "
      << ancestor;

  return ancestor;
}

void AXObjectCacheImpl::SlotAssignmentWillChange(Node* node) {
  ChildrenChanged(node);
}

void AXObjectCacheImpl::ChildrenChanged(Node* node) {
  ChildrenChanged(Get(node));
}

void AXObjectCacheImpl::ChildrenChanged(const LayoutObject* layout_object) {
  if (!layout_object)
    return;

  // Ensure that this object is touched, so that Get() can Invalidate() it if
  // necessary, e.g. to change whether it contains a LayoutObject.
  Get(layout_object);

  // Update using nearest node (walking ancestors if necessary).
  Node* node = GetClosestNodeForLayoutObject(layout_object);
  if (!node)
    return;

  ChildrenChanged(node);
}

void AXObjectCacheImpl::ChildrenChangedWithCleanLayout(Node* node) {
  if (AXObject* obj = Get(node)) {
    ChildrenChangedWithCleanLayout(node, obj);
  }
}

// TODO can node be non-optional?
void AXObjectCacheImpl::ChildrenChangedWithCleanLayout(Node* optional_node,
                                                       AXObject* obj) {
  CHECK(obj);
  CHECK(!obj->IsDetached());

#if DCHECK_IS_ON()
  if (optional_node) {
    DCHECK_EQ(obj->GetNode(), optional_node);
    DCHECK_EQ(obj, Get(optional_node));
  }
  Document* document = obj->GetDocument();
  DCHECK(document);
  DCHECK(document->Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean)
      << "Unclean document at lifecycle " << document->Lifecycle().ToString();
#endif  // DCHECK_IS_ON()

  obj->ChildrenChangedWithCleanLayout();
  DUMP_WILL_BE_CHECK(!obj->IsDetached());
  if (optional_node) {
    CHECK(relation_cache_);
    relation_cache_->UpdateRelatedTree(optional_node, obj);
  }

  TableCellRoleMaybeChanged(optional_node);
}

void AXObjectCacheImpl::FinalizeTree() {
  if (Root()->HasDirtyDescendants()) {
    HeapDeque<Member<AXObject>> objects_to_process;
    objects_to_process.push_back(Root());
    while (!objects_to_process.empty()) {
      AXObject* obj = objects_to_process.front();
      objects_to_process.pop_front();
      if (obj->IsDetached()) {
        continue;
      }
      obj->UpdateChildrenIfNecessary();
      if (obj->HasDirtyDescendants()) {
        obj->SetHasDirtyDescendants(false);
        for (auto& child : obj->ChildrenIncludingIgnored()) {
          objects_to_process.push_back(child);
        }
      }
    }
  }

  CheckTreeIsFinalized();
}

void AXObjectCacheImpl::CheckStyleIsComplete(Document& document) const {
#if EXPENSIVE_DCHECKS_ARE_ON()
  Element* root_element = document.documentElement();
  if (!root_element) {
    return;
  }

  {
    // Check that all style is up-to-date when layout is clean, when a11y is on.
    // This allows content-visibility: auto subtrees to have proper a11y
    // semantics, e.g. for the hidden and focusable states.
    Node* node = root_element;
    do {
      CHECK(!node->NeedsStyleRecalc()) << "Need style on: " << node;
      auto* element = DynamicTo<Element>(node);
      const ComputedStyle* style =
          element ? element->GetComputedStyle() : nullptr;
      if (!style || style->ContentVisibility() == EContentVisibility::kHidden ||
          style->IsEnsuredInDisplayNone()) {
        // content-visibility:hidden nodes are an exception and do not
        // compute style.
        node =
            LayoutTreeBuilderTraversal::NextSkippingChildren(*node, &document);
      } else {
        node = LayoutTreeBuilderTraversal::Next(*node, &document);
      }
    } while (node);
  }

  {
    // Check results of ChildNeedsStyleRecalc() as well, just to be sure there
    // isn't a discrepancy there.
    Node* node = root_element;
    do {
      auto* element = DynamicTo<Element>(node);
      const ComputedStyle* style =
          element ? element->GetComputedStyle() : nullptr;
      if (!style || style->ContentVisibility() == EContentVisibility::kHidden ||
          style->IsEnsuredInDisplayNone()) {
        // content-visibility:hidden nodes are an exception and do not
        // compute style.
        node =
            LayoutTreeBuilderTraversal::NextSkippingChildren(*node, &document);
        continue;
      }
      CHECK(!node->ChildNeedsStyleRecalc()) << "Need style on child: " << node;
      node = LayoutTreeBuilderTraversal::Next(*node, &document);
    } while (node);
  }
#endif
}

void AXObjectCacheImpl::CheckTreeIsFinalized() {
  CHECK(!Root()->NeedsToUpdateCachedValues());

#if DCHECK_IS_ON()

  // Skip check if document load is not complete.
  if (!GetDocument().IsLoadCompleted()) {
    return;
  }

  // After the first 5 checks, only check the tree every 5000 ms.
  tree_check_counter_++;
  auto now = base::Time::Now();
  if (tree_check_counter_ > 5 &&
      last_tree_check_time_stamp_ - now < base::Milliseconds(5000)) {
    return;
  }
  last_tree_check_time_stamp_ = now;

  // The following checks can make tests flaky if the tree being checked
  // is quite large. Therefore cap the number of objects we check.
  constexpr int kMaxObjectsToCheckAfterTreeUpdate = 5000;
  if (objects_.size() > kMaxObjectsToCheckAfterTreeUpdate) {
    DLOG(INFO)
        << "AXObjectCacheImpl::CheckTreeIsFinalized: Only checking first "
        << kMaxObjectsToCheckAfterTreeUpdate
        << " items in objects_ (size: " << objects_.size() << ")";
  }

  // First loop checks that tree structure is consistent.
  int count = 0;
  for (const auto& entry : objects_) {
    if (count > kMaxObjectsToCheckAfterTreeUpdate) {
      break;
    }

    const AXObject* object = entry.value;
    DCHECK(!object->IsDetached());
    DCHECK(object->GetDocument());
    DCHECK(object->GetDocument()->GetFrame())
        << "An object in a closed document should have been removed:"
        << "\n* Object: " << object;
    DCHECK(!object->IsMissingParent())
        << "No object should be missing its parent: " << "\n* Object: "
        << object << "\n* Computed parent: " << object->ComputeParent();
    // Check whether cached values need an update before using any getters that
    // will update them.
    DCHECK(!object->NeedsToUpdateCachedValues())
        << "No cached values should require an update: " << "\n* Object: "
        << object;
    DCHECK(!object->ChildrenNeedToUpdateCachedValues())
        << "Cached values for children should not require an update: "
        << "\n* Object: " << object;
    if (object->IsIncludedInTree()) {
      // All cached children must be included.
      for (const auto& child : object->CachedChildrenIncludingIgnored()) {
        CHECK(child->IsIncludedInTree())
            << "Included parent cannot have unincluded child:" << "\n* Parent: "
            << object << "\n* Child: " << child;
      }
      if (!object->IsRoot()) {
        // Parent must have this child in its cached children.
        AXObject* included_parent = object->ParentObjectIncludedInTree();
        CHECK(included_parent);
        const HeapVector<Member<AXObject>>& siblings =
            included_parent->CachedChildrenIncludingIgnored();
        DCHECK(siblings.Contains(object))
            << "Object was not included in its parent: " << "\n* Object: "
            << object
            << "\n* Included parent: " << included_parent;
      }
    }
    count++;
  }

  // Second loop checks that all dirty bits to update properties or children
  // have been cleared.
  count = 0;
  for (const auto& entry : objects_) {
    if (count > kMaxObjectsToCheckAfterTreeUpdate) {
      break;
    }
    const AXObject* object = entry.value;
    if (object->HasDirtyDescendants()) {
      // This an error: log the top ancestor that still has dirty descendants.
      const AXObject* ancestor = object;
      while (ancestor && ancestor->ParentObjectIncludedInTree() &&
             ancestor->ParentObjectIncludedInTree()->HasDirtyDescendants()) {
        ancestor = ancestor->ParentObjectIncludedInTree();
      }
      AXObject* included_parent = ancestor->ParentObjectIncludedInTree();
      if (!included_parent) {
        included_parent = Root();
      }
      DCHECK(!ancestor->HasDirtyDescendants())
          << "No subtrees should be flagged as needing updates at this point:"
          << "\n* Object: " << ancestor
          << "\n* Included parent: " << included_parent->GetAXTreeForThis();
    }
    AXObject* included_parent = object->ParentObjectIncludedInTree();
    if (!included_parent) {
      included_parent = Root();
    }
    DCHECK(!object->NeedsToUpdateChildren())
        << "No children in the tree should require an update at this point: "
        << "\n* Object: " << object
        << "\n* Included parent: " << included_parent;

    count++;
  }
#endif
}

int AXObjectCacheImpl::GetDeferredEventsDelay() const {
  // The amount of time, in milliseconds, to wait before sending non-interactive
  // events that are deferred before the initial page load.
  constexpr int kDelayForDeferredUpdatesBeforePageLoad = 350;

  // The amount of time, in milliseconds, to wait before sending non-interactive
  // events that are deferred after the initial page load.
  // Shync with same constant in CrossPlatformAccessibilityBrowserTest.
  constexpr int kDelayForDeferredUpdatesAfterPageLoad = 150;

  return GetDocument().IsLoadCompleted()
             ? kDelayForDeferredUpdatesAfterPageLoad
             : kDelayForDeferredUpdatesBeforePageLoad;
}

int AXObjectCacheImpl::GetLocationSerializationDelay() {
  // The amount of time, in milliseconds, to wait in between location updates
  // when the changed nodes don't include the focused node.
  constexpr int kDelayForLocationUpdatesNonFocused = 500;

  // The amount of time, in milliseconds, to wait in between location updates
  // when the changed nodes includes the focused node.
  constexpr int kDelayForLocationUpdatesFocused = 75;

  // It's important for the user to have access to any changes to the
  // currently focused object, so schedule serializations (almost )immediately
  // if that object changes. The root is an exception because it often has focus
  // while the page is loading.
  DOMNodeId focused_node_id = FocusedNode()->GetDomNodeId();
  if (focused_node_id != document_->GetDomNodeId() &&
      changed_bounds_ids_.Contains(focused_node_id)) {
    return kDelayForLocationUpdatesFocused;
  }

  return kDelayForLocationUpdatesNonFocused;
}

void AXObjectCacheImpl::CommitAXUpdates(Document& document, bool force) {
  if (IsPopup(document)) {
    // Only process popup document together with main document.
    DCHECK_EQ(&document, GetPopupDocumentIfShowing());
    // Since a change occurred in the popup, processing of both documents will
    // be needed. A visual update on the main document will force this.
    ScheduleAXUpdate();
    return;
  }

  DCHECK_EQ(document, GetDocument());
  if (!GetDocument().IsActive()) {
    return;
  }

  CheckStyleIsComplete(document);

  // Don't update the tree at an awkward time during page load.
  // Example: when the last node is whitespace, there is not yet enough context
  // to determine the relevance of the whitespace.
  if ((allowed_tree_update_pauses_remaining_ ||
       node_to_parse_before_more_tree_updates_) &&
      !force) {
    if (IsParsingMainDocument()) {
      return;
    }
    allowed_tree_update_pauses_remaining_ = 0;
    node_to_parse_before_more_tree_updates_ = nullptr;
  }

  if (tree_updates_paused_) {
    // Unpause tree updates and rebuild the tree from the root.
    // TODO(accessibility): Add more testing for this feature.
    // TODO(accessibility): Consider waiting until serialization batching timer
    // fires, so that the pause is a bit longer.
    LOG(INFO) << "Accessibility tree updates will be resumed after rebuilding "
                 "the tree from root";
    mark_all_dirty_ = true;
    tree_updates_paused_ = false;
  }

  // Something occurred which requires an immediate serialization.
  if (serialize_immediately_) {
    force = true;
    serialize_immediately_ = false;
  }

  if (!force) {
    // Process the current tree changes unless not enough time has passed, or
    // another serialization is already in flight.
    if (IsSerializationInFlight()) {
      // Another serialization is in flight. When it's finished, this method
      // will be called again.
      return;
    }

    const auto& now = base::Time::Now();
    const auto& delay_between_serializations =
        base::Milliseconds(GetDeferredEventsDelay());
    const auto& elapsed_since_last_serialization =
        now - last_serialization_timestamp_;
    const auto& delay_until_next_serialization =
        delay_between_serializations - elapsed_since_last_serialization;
    if (delay_until_next_serialization.is_positive()) {
      // No serialization needed yet, will serialize after a delay.
      // Set a timer to call this method again, if one isn't already set.
      if (!weak_factory_for_serialization_pipeline_.HasWeakCells()) {
        document.GetTaskRunner(blink::TaskType::kInternalDefault)
            ->PostDelayedTask(
                FROM_HERE,
                WTF::BindOnce(
                    &AXObjectCacheImpl::ScheduleAXUpdate,
                    WrapPersistent(weak_factory_for_serialization_pipeline_
                                       .GetWeakCell())),
                delay_until_next_serialization);
      }
      return;
    }
  }

  weak_factory_for_serialization_pipeline_.Invalidate();

  if (GetPopupDocumentIfShowing()) {
    UpdateLifecycleIfNeeded(*GetPopupDocumentIfShowing());
    CheckStyleIsComplete(*GetPopupDocumentIfShowing());
  }

  lifecycle_.AdvanceTo(AXObjectCacheLifecycle::kProcessDeferredUpdates);

  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
      "Accessibility.Performance.TotalAccessibilityCleanLayoutLifecycleStages");
  TRACE_EVENT0("accessibility",
               load_sent_
                   ? "TotalAccessibilityCleanLayoutLifecycleStages"
                   : "TotalAccessibilityCleanLayoutLifecycleStagesLoading");

  // Upon exiting this function, listen for tree updates again.
  absl::Cleanup lifecycle_returns_to_queueing_updates = [this] {
    lifecycle_.EnsureStateAtMost(AXObjectCacheLifecycle::kDeferTreeUpdates);
  };

  SCOPED_DISALLOW_LIFECYCLE_TRANSITION();

  // ------------ Process deferred events and update tree  --------------------
  {
    {
      SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
          "Accessibility.Performance.ProcessDeferredUpdatesLifecycleStage");
      TRACE_EVENT0("accessibility",
                   load_sent_ ? "ProcessDeferredUpdatesLifecycleStage"
                              : "ProcessDeferredUpdatesLifecycleStageLoading");

      // If this is the first update, ensure that both an initial tree exists
      // and that the relation cache is initialized. Any existing content with
      // aria-owns relation be added to the relation cache's queue for
      // processing.
      EnsureRelationCacheAndInitialTree();

      // Update (create or remove) validation child of root, if it is needed, so
      // that the tree can be frozen in the correct state.
      ValidationMessageObjectIfInvalid();

      // If MarkDocumentDirty() was called, do it now, so that the entire tree
      // is invalidated before updating it.
      if (mark_all_dirty_) {
        MarkDocumentDirtyWithCleanLayout();
      }

      // Call the queued callback methods that do processing which must occur
      // when layout is clean. These callbacks are stored in
      // |tree_update_callback_queue_|, and have names like
      // FooBarredWithCleanLayout().
      if (IsDirty()) {
        if (GetPopupDocumentIfShowing()) {
          ProcessCleanLayoutCallbacks(*GetPopupDocumentIfShowing());
        }
        ProcessCleanLayoutCallbacks(document);
      }
    }

    // At this point, the popup queue should be clear, and we must ensure this
    // even if nothing is dirty. It seems that there are cases where it
    // IsDirty() returns false where there is no popup document, but there are
    // entries in the popup queue.
    // TODO(https://crbug.com/1507396): It is unclear when this happens, but it
    // explains why we still have a full popup queue in CheckTreeIsFinalized().
    // DCHECKs have been added elsewhere to help discover the underlying cause.
    // For now, keep this line in order to pass CheckTreeIsFinalized().
    tree_update_callback_queue_popup_.clear();

    {
#if defined(REDUCE_AX_INLINE_TEXTBOXES)
      // On Android, the inline textboxes of focused editable subtrees are
      // always loaded, but only if inline text boxes are enabled.
      if (ax_mode_.has_mode(ui::AXMode::kInlineTextBoxes)) {
        AXObject* focus = FocusedObject();
        if (focus && focus->IsEditableRoot()) {
          focus->LoadInlineTextBoxes();
        }
      }
#endif

      mark_all_dirty_ = false;

      // All tree updates have been processed.
      DUMP_WILL_BE_CHECK(!IsMainDocumentDirty());
      DUMP_WILL_BE_CHECK(!IsPopupDocumentDirty());

      // Clean up any remaining unprocessed aria-owns relations, which can
      // result from processing deferred tree updates. For example, if an object
      // is created without a parent, RepairChildrenOfIncludedParent() may be
      // called, which in some cases can queue multiple aria-owns relations that
      // point to the same node to be added to the processing queue.
      relation_cache_->ProcessUpdatesWithCleanLayout();

      EnsureFocusedObject();

      CHECK(tree_update_callback_queue_main_.empty());
      CHECK(tree_update_callback_queue_popup_.empty());
      CHECK(nodes_with_pending_children_changed_.empty());

      {
        lifecycle_.AdvanceTo(AXObjectCacheLifecycle::kFinalizingTree);
        SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
            "Accessibility.Performance.FinalizingTreeLifecycleStage");
        TRACE_EVENT0("accessibility",
                     load_sent_ ? "FinalizingTreeLifecycleStage"
                                : "FinalizingTreeLifecycleStageLoading");

        // Build out tree, such that each node has computed its children.
        FinalizeTree();

        CHECK(tree_update_callback_queue_main_.empty());
        CHECK(tree_update_callback_queue_popup_.empty());
        CHECK(nodes_with_pending_children_changed_.empty());

        // Updating the tree did not add dirty objects.
        DUMP_WILL_BE_CHECK(!IsDirty());
      }
    }
  }

  lifecycle_.AdvanceTo(AXObjectCacheLifecycle::kSerialize);
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
      "Accessibility.Performance.SerializeLifecycleStage");
  TRACE_EVENT0("accessibility", load_sent_ ? "SerializeLifecycleStage"
                                           : "SerializeLifecycleStageLoading");

  // Check whether serializations are needed, or whether we are just here to
  // update as part of a tree snapshot.
  if (!ax_mode_.has_mode(ui::AXMode::kWebContents)) {
    return;
  }

  // Serialize the current tree changes unless not enough time has passed, or
  // another serialization is already in flight.
  if (IsSerializationInFlight()) {
    // Another serialization is in flight. When it's finished, this method
    // will be called again.
    return;
  }

  // ------------------------ Freeze and serialize ---------------------------
  {
    // The frozen state begins immediately after processing deferred events.
    ScopedFreezeAXCache scoped_freeze_cache(*this);

    // ***** Serialize *****
    // Check whether there are dirty objects ready to be serialized.
    // TODO(accessibility) It's a bit confusing that this can be true when the
    // IsDirty() is false, but this is the case for objects marked dirty from
    // RenderAccessibilityImpl, e.g. for the kEndOfTest event.
    bool did_serialize = false;
    if (HasObjectsPendingSerialization()) {
      did_serialize = SerializeUpdatesAndEvents();
    }

    // ***** Serialize Location Changes *****
    // Even if there are no dirty objects, we ensure pending location changes
    // are sent.
    if (reset_token_ && !changed_bounds_ids_.empty()) {
      DCHECK(!did_serialize);  // Location changes should have been sent with
                               // full serialization.
      SerializeLocationChanges();
    }

    // ***** Update Inspector Views *****
    // Accessibility is now clean for both documents: AXObjects can be safely
    // traversed and AXObject's properties can be safely fetched.
    // TODO(accessibility) Now that both documents are always processed at the
    // same time, consider modifying the InspectorAccessibilityAgent so that
    // only the callback for the main document is needed.
    for (auto agent : agents_) {
      agent->AXReadyCallback(document);
      if (GetPopupDocumentIfShowing()) {
        agent->AXReadyCallback(*GetPopupDocumentIfShowing());
      }
    }

    DUMP_WILL_BE_CHECK(!IsDirty());
    // TODO(accessibility): in the future, we may break up serialization into
    // pieces to reduce jank, in which case this assertion will not hold.
    DUMP_WILL_BE_CHECK(!HasObjectsPendingSerialization() || !did_serialize)
        << "A serialization occurred but dirty objects remained.";
  }
}

bool AXObjectCacheImpl::SerializeUpdatesAndEvents() {
  CHECK(HasObjectsPendingSerialization());
  CHECK(!IsSerializationInFlight());
  DCHECK(!ax_mode_.is_mode_off());
  CHECK(ax_mode_.has_mode(ui::AXMode::kWebContents));
  CHECK(lifecycle_.StateAllowsSerialization()) << *this;

  if (!GetDocument().GetFrame()) {
    return false;
  }

  // Dirty objects are present, but we cannot serialize until there is an
  // embedding token, which may not be present when the cache is first
  // initialized.
  const std::optional<base::UnguessableToken>& embedding_token =
      GetDocument().GetFrame()->GetEmbeddingToken();
  if (!embedding_token || embedding_token->is_empty()) {
    return false;
  }

  auto* client = GetWebLocalFrameClient();
  CHECK(client);

  // TODO(accessibility): Review why this value is inconsistent with
  //       ax_mode_.has_mode(ui::AXMode::kWebContents)
  if (!client->IsAccessibilityEnabled()) {
    return false;
  }

  OnSerializationStartSend();

  bool had_end_of_test_event = false;

  // Keep track of load complete messages. When a load completes, it's a good
  // time to inject a stylesheet for image annotation debugging.
  bool had_load_complete_messages = false;

  std::vector<ui::AXTreeUpdate> updates;
  std::vector<ui::AXEvent> events;

  // Serialize all dirty objects in the list at this point in time, stopping
  // either when the queue is empty, or the number of remaining objects to
  // serialize has been reached.
  GetUpdatesAndEventsForSerialization(updates, events, had_end_of_test_event,
                                      had_load_complete_messages);

  /* Clear the pending updates and events as they're about to be serialized */
  pending_objects_to_serialize_.clear();
  pending_events_to_serialize_.clear();

  if (had_end_of_test_event) {
    ui::AXEvent end_of_test(Root()->AXObjectID(),
                            ax::mojom::blink::Event::kEndOfTest);
    if (!IsDirty() && GetDocument().IsLoadCompleted()) {
      // Everything is clean and the document is fully loaded, so kEndOfTest
      // signal can be fired.
      events.emplace_back(end_of_test);
    } else {
      DLOG(ERROR) << "Had end of test event, but document is still dirty.";
      // Document is still dirty, queue up another end of test and process
      // immediately.
      AddEventToSerializationQueue(end_of_test, /*serialize_immediately*/ true);
    }
  }

  // updates.empty() -implies-> events.empty()
  DCHECK(!updates.empty() || events.empty())
      << "Every event must have at least one corresponding update because "
         "events cause their related nodes to be marked dirty.";
  DCHECK(!updates.empty());

  // There should be no more dirty objects.
  CHECK(!HasObjectsPendingSerialization());

  /* If there's location updates pending, send them on the way. */
  ui::AXLocationAndScrollUpdates location_and_scroll_changes;
  if (!changed_bounds_ids_.empty()) {
    location_and_scroll_changes = TakeLocationChangsForSerialization();
  }

  /* Send the actual serialization message.*/
  bool success = client->SendAccessibilitySerialization(
      std::move(updates), std::move(events),
      std::move(location_and_scroll_changes), had_load_complete_messages);

  if (!success) {
    // In some cases, like in web tests or if a11y is off, serialization doesn't
    // really occur and thus the function will return false.
    // Cancel serialization to avoid stalling pipeline.
    OnSerializationCancelled();
  }

  if (had_load_complete_messages) {
    load_sent_ = true;
  }

  CHECK(serialization_in_flight_ == success);
  return success;
}

bool AXObjectCacheImpl::IsParsingMainDocument() const {
  return GetDocument().Parser() &&
         !GetDocument().GetAgent().isolate()->InContext();
}

bool AXObjectCacheImpl::IsMainDocumentDirty() const {
  return !tree_update_callback_queue_main_.empty();
}

bool AXObjectCacheImpl::IsPopupDocumentDirty() const {
  if (!popup_document_) {
    // This should have been cleared in RemovePopup(), but technically the
    // popup could be null without calling that, since it's a weak pointer.
    DCHECK(tree_update_callback_queue_popup_.empty());
    return false;
  }
  return !tree_update_callback_queue_popup_.empty();
}

bool AXObjectCacheImpl::IsDirty() {
  if (!GetDocument().IsActive()) {
    return false;
  }

  if (mark_all_dirty_) {
    return true;
  }
  if (IsMainDocumentDirty() || IsPopupDocumentDirty() || !relation_cache_ ||
      relation_cache_->IsDirty()) {
    return true;
  }
  if (Root()->NeedsToUpdateChildren() || Root()->HasDirtyDescendants()) {
    return true;
  }
  // If tree updates are paused, consider the cache dirty. The next time
  // CommitAXUpdates() is called, the entire tree will be
  // rebuilt from the root.
  if (tree_updates_paused_) {
    return true;
  }
  return false;
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
  if (is_popup) {
#if DCHECK_IS_ON()
    // Verify that the popup document's owner is the main document.
    LocalFrame* frame = document.GetFrame();
    DCHECK(frame);
    Element* popup_owner = frame->PagePopupOwner();
    DCHECK(popup_owner);
    DCHECK_EQ(popup_owner->GetDocument(), GetDocument())
        << "The popup document's owner should be in the main document.";
    Page* main_page = GetDocument().GetPage();
    DCHECK(main_page);
#endif
    return &document == GetPopupDocumentIfShowing();
  }
  return is_popup;
}

AXObjectCacheImpl::TreeUpdateCallbackQueue&
AXObjectCacheImpl::GetTreeUpdateCallbackQueue(Document& document) {
  return IsPopup(document) ? tree_update_callback_queue_popup_
                           : tree_update_callback_queue_main_;
}

void AXObjectCacheImpl::ProcessCleanLayoutCallbacks(Document& document) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION();

  UpdateNumTreeUpdatesQueuedBeforeLayoutHistogram();

  CHECK(!IsFrozen());

  TreeUpdateCallbackQueue old_tree_update_callback_queue;
  GetTreeUpdateCallbackQueue(document).swap(old_tree_update_callback_queue);
  nodes_with_pending_children_changed_.clear();
  last_value_change_node_ = ui::AXNodeData::kInvalidAXID;

  for (TreeUpdateParams* tree_update : old_tree_update_callback_queue) {
    if (tree_update->node) {
      if (tree_update->node->GetDocument() == document) {
        FireTreeUpdatedEventForNode(tree_update);
      }
    } else {
      FireTreeUpdatedEventForAXID(tree_update, document);
    }
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

void AXObjectCacheImpl::PostNotification(AXObject* object,
                                         ax::mojom::blink::Event event_type) {
  if (!object || !object->AXObjectID() || object->IsDetached())
    return;

  ax::mojom::blink::EventFrom event_from = ComputeEventFrom();

  // If PostNotification is called while outside of processing deferred events,
  // defer it to to happen later while processing deferred_events.
  // TODO(accessibility): Replace calls of PostNotification with direct cleaner
  // calls to DeferTreeUpdate.
  if (lifecycle_.StateAllowsDeferTreeUpdates()) {
    // TODO(accessibility): Investigate why invalidate_cached_values needs to be
    // false here and maybe remove it from signature once it's not needed
    // anymore.
    DeferTreeUpdate(TreeUpdateReason::kDelayEventFromPostNotification, object,
                    event_type, /*invalidate_cached_values=*/false);

    if (IsImmediateProcessingRequiredForEvent(event_from, object, event_type)) {
      ScheduleImmediateSerialization();
    }
    return;
  }

  ax::mojom::blink::Action event_from_action = active_event_from_action_;
  const BlinkAXEventIntentsSet& event_intents = ActiveEventIntents();

#if DCHECK_IS_ON()
  // Make sure that we're not in the process of being laid out. Notifications
  // should only be sent after the LayoutObject has finished
  DCHECK(GetDocument().Lifecycle().GetState() !=
         DocumentLifecycle::kInPerformLayout);

  SCOPED_DISALLOW_LIFECYCLE_TRANSITION();
#endif  // DCHECK_IS_ON()

  PostPlatformNotification(object, event_type, event_from, event_from_action,
                           event_intents);
}

void AXObjectCacheImpl::ScheduleAXUpdate() const {
  // A visual update will force accessibility to be updated as well.
  // Scheduling visual updates before the document is finished loading can
  // interfere with event ordering. In any case, at least one visual update will
  // occur between now and when the document load is complete.
  if (!GetDocument().IsLoadCompleted())
    return;

  // If there was a document change that doesn't trigger a lifecycle update on
  // its own, (e.g. because it doesn't make layout dirty), make sure we run
  // lifecycle phases to update the computed accessibility tree.
  LocalFrameView* frame_view = GetDocument().View();
  Page* page = GetDocument().GetPage();
  if (!frame_view || !page)
    return;

  if (!frame_view->CanThrottleRendering() &&
      !GetDocument().GetPage()->Animator().IsServicingAnimations()) {
    page->Animator().ScheduleVisualUpdate(GetDocument().GetFrame());
  }
}

void AXObjectCacheImpl::FireTreeUpdatedEventForAXID(
    TreeUpdateParams* tree_update,
    Document& document) {
  if (!tree_update->axid) {
    // No node and no AXID means that it was a node update, but the
    // WeakMember<Node> is no longer available.
    return;
  }

  AXObject* ax_object = ObjectFromAXID(tree_update->axid);
  if (!ax_object || ax_object->IsDetached()) {
    return;
  }

  if (ax_object->GetNode() && !ax_object->GetNode()->isConnected()) {
    return;
  }

  if (document != *ax_object->GetDocument()) {
    return;
  }

  DUMP_WILL_BE_CHECK(!ax_object->IsMissingParent())
      << tree_update->ToString() << " on " << ax_object;

  // Update cached attributes for all changed nodes before serialization,
  // because updating ignored/included can cause tree structure changes, and
  // the tree structure needs to be stable before serialization begins.
  ax_object->UpdateCachedAttributeValuesIfNeeded(/* notify_parent */ true);
  if (ax_object->IsDetached()) {
    return;
  }

  base::AutoReset<ax::mojom::blink::EventFrom> event_from_resetter(
      &active_event_from_, tree_update->event_from);
  base::AutoReset<ax::mojom::blink::Action> event_from_action_resetter(
      &active_event_from_action_, tree_update->event_from_action);
  ScopedBlinkAXEventIntent defered_event_intents(
      tree_update->event_intents.AsVector(), ax_object->GetDocument());

  // Kept here for convenient debugging:
  // LOG(ERROR) << tree_update->ToString() << " on " << ax_object;

  switch (tree_update->update_reason) {
    case TreeUpdateReason::kChildrenChanged:
      ChildrenChangedWithCleanLayout(ax_object->GetNode(), ax_object);
      break;
    case TreeUpdateReason::kDelayEventFromPostNotification:
      PostNotification(ax_object, tree_update->event);
      break;
    case TreeUpdateReason::kMarkAXObjectDirty:
      MarkAXObjectDirtyWithCleanLayout(ax_object);
      break;
    case TreeUpdateReason::kMarkAXSubtreeDirty:
      MarkAXSubtreeDirtyWithCleanLayout(ax_object);
      break;
    case TreeUpdateReason::kTextChangedOnLayoutObject:
      TextChangedWithCleanLayout(ax_object->GetNode(), ax_object);
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Update reason not handled: "
                                << static_cast<int>(tree_update->update_reason);
  }

  // Ensure that new subtrees are filled out. Any new AXObjects added will
  // also add their children.
  if (!ax_object->IsDetached()) {
    ax_object->UpdateChildrenIfNecessary();
  }
}

void AXObjectCacheImpl::FireTreeUpdatedEventForNode(
    TreeUpdateParams* tree_update) {
  Node* node = tree_update->node;
  CHECK(node);
  if (!node->isConnected()) {
    return;
  }

  AXObject* ax_object = Get(node);
  if (!ax_object) {
    return;
  }

  ax_object->UpdateCachedAttributeValuesIfNeeded(/* notify_parent */ true);
  if (ax_object->IsDetached()) {
    return;
  }

  DUMP_WILL_BE_CHECK(!ax_object->IsMissingParent())
      << tree_update->ToString() << " on " << ax_object;

  base::AutoReset<ax::mojom::blink::EventFrom> event_from_resetter(
      &active_event_from_, tree_update->event_from);
  base::AutoReset<ax::mojom::blink::Action> event_from_action_resetter(
      &active_event_from_action_, tree_update->event_from_action);
  ScopedBlinkAXEventIntent defered_event_intents(
      tree_update->event_intents.AsVector(), &node->GetDocument());

  // Kept here for convenient debugging:
  // LOG(ERROR) << tree_update->ToString() << " on " << ax_object;

  switch (tree_update->update_reason) {
    case TreeUpdateReason::kActiveDescendantChanged:
      HandleActiveDescendantChangedWithCleanLayout(node);
      break;
    case TreeUpdateReason::kAriaExpandedChanged:
      HandleAriaExpandedChangeWithCleanLayout(node);
      break;
    case TreeUpdateReason::kAriaOwnsChanged:
      AriaOwnsChangedWithCleanLayout(node);
      break;
    case TreeUpdateReason::kAriaPressedChanged:
      HandleAriaPressedChangedWithCleanLayout(node);
      break;
    case TreeUpdateReason::kAriaSelectedChanged:
      HandleAriaSelectedChangedWithCleanLayout(node);
      break;
    case TreeUpdateReason::kCSSAnchorChanged:
      CSSAnchorChangedWithCleanLayout(node);
      break;
    case TreeUpdateReason::kDidShowMenuListPopup:
      HandleUpdateMenuListPopupWithCleanLayout(node, /*did_show*/ true);
      break;
    case TreeUpdateReason::kEditableTextContentChanged:
      HandleEditableTextContentChangedWithCleanLayout(node);
      break;
    case TreeUpdateReason::kIdChanged:
      // When the id attribute changes, the relations its in may also change.
      MaybeNewRelationTarget(*node, ax_object);
      break;
    case TreeUpdateReason::kNodeGainedFocus:
      HandleNodeGainedFocusWithCleanLayout(node);
      break;
    case TreeUpdateReason::kNodeLostFocus:
      HandleNodeLostFocusWithCleanLayout(node);
      break;
    case TreeUpdateReason::kPostNotificationFromHandleLoadComplete:
    case TreeUpdateReason::kPostNotificationFromHandleLoadStart:
    case TreeUpdateReason::kPostNotificationFromHandleScrolledToAnchor:
      PostNotification(node, tree_update->event);
      break;
    case TreeUpdateReason::kReferenceTargetChanged:
      // When a shadow root's reference target changes, relations referring
      // to the shadow host may change since they will be forwarded to
      // the new reference target.
      MaybeNewRelationTarget(*node, ax_object);
      break;
    case TreeUpdateReason::kRemoveValidationMessageObjectFromFocusedUIElement:
      RemoveValidationMessageObjectWithCleanLayout(node);
      break;
    case TreeUpdateReason::kRoleChangeFromAriaHasPopup:
    case TreeUpdateReason::kRoleChangeFromImageMapName:
    case TreeUpdateReason::kRoleChangeFromRoleOrType:
      HandleRoleChangeWithCleanLayout(node);
      break;
    case TreeUpdateReason::kRoleMaybeChangedFromEventListener:
    case TreeUpdateReason::kRoleMaybeChangedFromHref:
    case TreeUpdateReason::kRoleMaybeChangedOnSelect:
      HandleRoleMaybeChangedWithCleanLayout(node);
      break;
    case TreeUpdateReason::kSectionOrRegionRoleMaybeChangedFromLabel:
    case TreeUpdateReason::kSectionOrRegionRoleMaybeChangedFromLabelledBy:
    case TreeUpdateReason::kSectionOrRegionRoleMaybeChangedFromTitle:
      SectionOrRegionRoleMaybeChangedWithCleanLayout(node);
      break;
    case TreeUpdateReason::kTextChangedOnNode:
    case TreeUpdateReason::kTextChangedOnClosestNodeForLayoutObject:
      TextChangedWithCleanLayout(node);
      break;
    case TreeUpdateReason::kTextMarkerDataAdded:
      HandleTextMarkerDataAddedWithCleanLayout(node);
      break;
    case TreeUpdateReason::kUpdateActiveMenuOption:
      HandleUpdateMenuListPopupWithCleanLayout(node);
      break;
    case TreeUpdateReason::kNodeIsAttached:
      NodeIsAttachedWithCleanLayout(node);
      break;
    case TreeUpdateReason::kUpdateAriaOwns:
      UpdateAriaOwnsWithCleanLayout(node);
      break;
    case TreeUpdateReason::kUpdateTableRole:
      UpdateTableRoleWithCleanLayout(node);
      break;
    case TreeUpdateReason::kUseMapAttributeChanged:
      HandleUseMapAttributeChangedWithCleanLayout(node);
      break;
    case TreeUpdateReason::kValidationMessageVisibilityChanged:
      HandleValidationMessageVisibilityChangedWithCleanLayout(node);
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Update reason not handled: "
                                << static_cast<int>(tree_update->update_reason);
  }
  // Ensure that new subtrees are filled out. Any new AXObjects added will
  // also add their children.
  if (!ax_object->IsDetached()) {
    ax_object->UpdateChildrenIfNecessary();
  }
}

bool AXObjectCacheImpl::IsAriaOwned(const AXObject* object, bool checks) const {
  return relation_cache_ ? relation_cache_->IsAriaOwned(object, checks) : false;
}

AXObject* AXObjectCacheImpl::ValidatedAriaOwner(const AXObject* object) const {
  DCHECK(GetDocument().Lifecycle().GetState() >=
         DocumentLifecycle::kLayoutClean);
  CHECK(relation_cache_);
  return relation_cache_->ValidatedAriaOwner(object);
}

void AXObjectCacheImpl::ValidatedAriaOwnedChildren(
    const AXObject* owner,
    HeapVector<Member<AXObject>>& owned_children) {
  DCHECK(GetDocument().Lifecycle().GetState() >=
         DocumentLifecycle::kLayoutClean);
  CHECK(relation_cache_);
  relation_cache_->ValidatedAriaOwnedChildren(owner, owned_children);
}

bool AXObjectCacheImpl::MayHaveHTMLLabel(const HTMLElement& elem) {
  CHECK(elem.GetDocument().Lifecycle().GetState() >=
        DocumentLifecycle::kLayoutClean)
      << "Unclean document at lifecycle " << elem.GetDocument().ToString();
  CHECK(relation_cache_);

  // Return false if this type of element will not accept a <label for> label.
  if (!elem.IsLabelable())
    return false;

  // Return true if a <label for> pointed to this element at some point.
  if (relation_cache_->MayHaveHTMLLabelViaForAttribute(elem)) {
    return true;
  }

  // Return true if any ancestor is a label, as in <label><input></label>.
  if (Traversal<HTMLLabelElement>::FirstAncestor(elem)) {
    return true;
  }

  // If the element is the reference target of its shadow host, also check if
  // the host may have a label.
  if (ShadowRoot* shadow_root = elem.ContainingShadowRoot()) {
    if (shadow_root->referenceTargetElement() == &elem) {
      if (HTMLElement* host = DynamicTo<HTMLElement>(shadow_root->host())) {
        return MayHaveHTMLLabel(*host);
      }
    }
  }

  return false;
}

bool AXObjectCacheImpl::IsLabelOrDescription(Element& element) {
  if (IsA<HTMLLabelElement>(element)) {
    return true;
  }
  CHECK(relation_cache_);
  return relation_cache_ && relation_cache_->IsARIALabelOrDescription(element);
}

void AXObjectCacheImpl::CheckedStateChanged(Node* node) {
  PostNotification(node, ax::mojom::blink::Event::kCheckedStateChanged);
}

void AXObjectCacheImpl::ListboxOptionStateChanged(HTMLOptionElement* option) {
  PostNotification(option, ax::mojom::Event::kCheckedStateChanged);
}

void AXObjectCacheImpl::ListboxSelectedChildrenChanged(
    HTMLSelectElement* select) {
  PostNotification(select, ax::mojom::Event::kSelectedChildrenChanged);
}

void AXObjectCacheImpl::ListboxActiveIndexChanged(HTMLSelectElement* select) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION();

  if (select->IsFocused()) {
    if (HTMLOptionElement* option = select->ActiveSelectionEnd()) {
      DOMNodeId option_id = option->GetDomNodeId();
      if (option_id != last_selected_list_option_) {
        PostNotification(select,
                         ax::mojom::blink::Event::kActiveDescendantChanged);
        last_selected_list_option_ = option_id;
      }
    }
  }
}

void AXObjectCacheImpl::SetMenuListOptionsBounds(
    HTMLSelectElement* select,
    const WTF::Vector<gfx::Rect>& options_bounds) {
  CHECK(select->PopupIsVisible());
  CHECK_EQ(select->GetDocument(), GetDocument());
  options_bounds_ = options_bounds;
  current_menu_list_axid_ = select->GetDomNodeId();
}

const WTF::Vector<gfx::Rect>* AXObjectCacheImpl::GetOptionsBounds(
    const AXObject& ax_menu_list) const {
  if (RuntimeEnabledFeatures::CustomizableSelectEnabled()) {
    // Customizable select does not render in a special popup document and does
    // not need to supply bounding boxes via options_bounds_.
    HTMLSelectElement* select = To<HTMLSelectElement>(ax_menu_list.GetNode());
    if (select->IsAppearanceBasePicker()) {
      CHECK(!current_menu_list_axid_);
      CHECK(options_bounds_.empty());
      return nullptr;
    }
  }

  if (!current_menu_list_axid_ ||
      current_menu_list_axid_ != ax_menu_list.AXObjectID()) {
    return nullptr;
  }

  CHECK_EQ(ax_menu_list.IsExpanded(), kExpandedExpanded);
  CHECK(options_bounds_.size());

  return &options_bounds_;
}

void AXObjectCacheImpl::ImageLoaded(const LayoutObject* layout_object) {
  MarkElementDirty(layout_object->GetNode());
}

void AXObjectCacheImpl::HandleClicked(Node* node) {
  if (AXObject* obj = Get(RetargetInput(node))) {
    PostNotification(obj, ax::mojom::Event::kClicked);
  }
}

void AXObjectCacheImpl::HandleAriaNotification(
    const Node* node,
    const String& announcement,
    const AriaNotificationOptions* options) {
  auto* obj = Get(node);

  if (!obj) {
    return;
  }

  // We use `insert` regardless of whether there is an entry for this node in
  // `aria_notifications_` since, if there is one, it won't be replaced.
  // The return value of `insert` is a pair of an iterator to the entry, called
  // `stored_value`, and a boolean; `stored_value` contains a pair with the key
  // of the entry and a `value` reference to its mapped `AriaNotifications`.
  auto& node_notifications =
      aria_notifications_.insert(obj->AXObjectID(), AriaNotifications())
          .stored_value->value;

  node_notifications.Add(announcement, options);
  DeferTreeUpdate(TreeUpdateReason::kMarkAXObjectDirty, obj);
}

AriaNotifications AXObjectCacheImpl::RetrieveAriaNotifications(
    const AXObject* obj) {
  DCHECK(obj);

  // Conveniently, `Take` returns an empty `AriaNotifications` if there's no
  // entry in `aria_notifications_` associated to the given object.
  return aria_notifications_.Take(obj->AXObjectID());
}

void AXObjectCacheImpl::UpdateTableRoleWithCleanLayout(Node* table) {
  if (AXObject* ax_table = Get(table)) {
    if (ax_table->RoleValue() == ax::mojom::blink::Role::kLayoutTable &&
        ax_table->IsDataTable()) {
      HandleRoleChangeWithCleanLayout(table);
    }
  }
}

void AXObjectCacheImpl::HandleAriaExpandedChangeWithCleanLayout(Node* node) {
  if (!node)
    return;

  SCOPED_DISALLOW_LIFECYCLE_TRANSITION();

  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));
  if (AXObject* obj = Get(node)) {
    obj->HandleAriaExpandedChanged();
  }
}

void AXObjectCacheImpl::HandleAriaPressedChangedWithCleanLayout(Node* node) {
  AXObject* ax_object = Get(node);
  if (!ax_object)
    return;

  ax::mojom::blink::Role previous_role = ax_object->RoleValue();
  bool was_toggle_button =
      previous_role == ax::mojom::blink::Role::kToggleButton;
  bool is_toggle_button =
      ax_object->HasAriaAttribute(html_names::kAriaPressedAttr);

  if (was_toggle_button != is_toggle_button)
    HandleRoleChangeWithCleanLayout(node);
  else
    PostNotification(node, ax::mojom::blink::Event::kCheckedStateChanged);
}

// In single selection containers, selection follows focus, so a selection
// changed event must be fired. This ensures the AT is notified that the
// selected state has changed, so that it does not read "unselected" as
// the user navigates through the items. The event generator will handle
// the correct events as long as the old and newly selected objects are marked
// dirty.
void AXObjectCacheImpl::HandleAriaSelectedChangedWithCleanLayout(Node* node) {
  DCHECK(node);
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION();

  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));
  AXObject* obj = Get(node);
  if (!obj)
    return;

  // Mark the previous selected item dirty if it was selected via "selection
  // follows focus".
  if (last_selected_from_active_descendant_)
    MarkElementDirtyWithCleanLayout(last_selected_from_active_descendant_);

  // Mark the newly selected item dirty, and track it for use in the future.
  MarkAXObjectDirtyWithCleanLayout(obj);
  if (obj->IsSelectedFromFocus())
    last_selected_from_active_descendant_ = node;

  PostNotification(obj, ax::mojom::Event::kCheckedStateChanged);

  // TODO(accessibility): this may no longer be needed as it can be generated
  // from the browser side, and could be expensive for many items.
  AXObject* listbox = obj->ParentObjectUnignored();
  if (listbox && listbox->RoleValue() == ax::mojom::Role::kListBox) {
    // Ensure listbox options are in sync as selection status may have changed
    MarkAXSubtreeDirtyWithCleanLayout(listbox);
    PostNotification(listbox, ax::mojom::Event::kSelectedChildrenChanged);
  }
}

void AXObjectCacheImpl::HandleNodeLostFocusWithCleanLayout(Node* node) {
  DCHECK(node);
  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));
  AXObject* obj = Get(node);
  if (!obj)
    return;

  PostNotification(obj, ax::mojom::Event::kBlur);

  if (AXObject* active_descendant = obj->ActiveDescendant()) {
    if (active_descendant->IsSelectedFromFocusSupported())
      HandleAriaSelectedChangedWithCleanLayout(active_descendant->GetNode());
  }
}

void AXObjectCacheImpl::HandleNodeGainedFocusWithCleanLayout(Node* node) {
  AXObject* obj = EnsureFocusedObject();

  PostNotification(obj, ax::mojom::Event::kFocus);

  if (AXObject* active_descendant = obj->ActiveDescendant()) {
    if (active_descendant->IsSelectedFromFocusSupported())
      HandleAriaSelectedChangedWithCleanLayout(active_descendant->GetNode());
  }
}

// This might be the new target of a relation. Handle all possible cases.
void AXObjectCacheImpl::MaybeNewRelationTarget(Node& node, AXObject* obj) {
  // Track reverse relations
  CHECK(relation_cache_);
  relation_cache_->UpdateRelatedTree(&node, obj);
  if (Element* element = DynamicTo<Element>(node)) {
    relation_cache_->UpdateRelatedTreeForIdChange(*element);
  }
}

void AXObjectCacheImpl::HandleActiveDescendantChangedWithCleanLayout(
    Node* node) {
  DCHECK(node);
  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));

  if (AXObject* obj = Get(node)) {
    obj->HandleActiveDescendantChanged();
  }
}

// A <section> or role=region uses the region role if and only if it has a name.
void AXObjectCacheImpl::SectionOrRegionRoleMaybeChangedWithCleanLayout(
    Node* node) {
  TextChangedWithCleanLayout(node);
  Element* element = To<Element>(node);
  AXObject* ax_object = Get(element);
  if (!ax_object)
    return;

  // Require <section> or role="region" markup.
  if (!element->HasTagName(html_names::kSectionTag) &&
      ax_object->DetermineRawAriaRole() != ax::mojom::blink::Role::kRegion) {
    return;
  }

  HandleRoleMaybeChangedWithCleanLayout(element);
}

void AXObjectCacheImpl::TableCellRoleMaybeChanged(Node* node) {
  if (!node) {
    return;
  }
  // The role for a table cell depends in complex ways on multiple of its
  // siblings (see DecideRoleFromSiblings). Rather than attempt to reproduce
  // that logic here for invalidation, just recompute the role of all siblings
  // when new table cells are added.
  if (auto* cell = DynamicTo<HTMLTableCellElement>(node)) {
    for (auto* prev = LayoutTreeBuilderTraversal::PreviousSibling(*cell); prev;
         prev = LayoutTreeBuilderTraversal::PreviousSibling(*prev)) {
      HandleRoleMaybeChangedWithCleanLayout(prev);
    }
    HandleRoleMaybeChangedWithCleanLayout(cell);
    for (auto* next = LayoutTreeBuilderTraversal::NextSibling(*cell); next;
         next = LayoutTreeBuilderTraversal::PreviousSibling(*next)) {
      HandleRoleMaybeChangedWithCleanLayout(next);
    }
  }
}

void AXObjectCacheImpl::HandleRoleMaybeChangedWithCleanLayout(Node* node) {
  if (AXObject* obj = Get(node)) {
    // If role would stay the same, do nothing.
    if (obj->RoleValue() == obj->DetermineRoleValue()) {
      return;
    }

    HandleRoleChangeWithCleanLayout(node);
  }
}

// Be as safe as possible about changes that could alter the accessibility role,
// as this may require a different subclass of AXObject.
// Role changes are disallowed by the spec but we must handle it gracefully, see
// https://www.w3.org/TR/wai-aria-1.1/#h-roles for more information.
void AXObjectCacheImpl::HandleRoleChangeWithCleanLayout(Node* node) {
  DCHECK(node);
  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));

  // Remove the current object and make the parent reconsider its children.
  if (AXObject* obj = Get(node)) {
    bool was_owned = IsAriaOwned(obj);
    AXObject* parent = obj->ParentObjectIncludedInTree();
    ChildrenChangedOnAncestorOf(obj);

    // The positioned object may have changed to/from a tooltip so a details
    // relationship may need to be added/removed from the anchor.
    if (AXObject* anchor = relation_cache_->GetAnchorForPositionedObject(obj)) {
      MarkElementDirtyWithCleanLayout(anchor->GetElement());
    }

    if (!obj->IsDetached()) {
      // Remove and rebuild the subtree, because some descendant computations
      // rely on the role of ancestors.
      // Examples, whether rows return true from SupportsNameFromContents(),
      // propagation of role="none", some table descendant roles.
      RemoveSubtree(node, /* remove_root */ true, /* notify_parent */ false);
    }

    if (was_owned) {
      relation_cache_->UpdateAriaOwnsWithCleanLayout(parent, /*force*/ true);
    }

    // A previous call could have detached the parent.
    if (parent->IsDetached()) {
      return;
    }

    // Calling GetOrCreate(node) will not only create a new object with the
    // correct role, it will also repair all parent-child relationships from the
    // included ancestor done. If a new AXObject is not possible it will remove
    // the subtree.
    parent->UpdateChildrenIfNecessary();
    if (AXObject* new_object = Get(node)) {
      relation_cache_->UpdateAriaOwnsWithCleanLayout(new_object,
                                                     /*force*/ true);
      new_object->UpdateChildrenIfNecessary();
      // Need to mark dirty because the dom_node_id-based ID remains the same,
      // and therefore the serializer may not automatically serialize this node
      // from the children changed on the parent.
      MarkAXSubtreeDirtyWithCleanLayout(new_object);
    }
  }
}

void AXObjectCacheImpl::HandleAttributeChanged(const QualifiedName& attr_name,
                                               Element* element) {
  DCHECK(element);
  if (attr_name.LocalName().StartsWith("aria-")) {
    // Perform updates specific to each attribute.
    if (attr_name == html_names::kAriaActivedescendantAttr) {
      if (relation_cache_) {
        relation_cache_->UpdateReverseActiveDescendantRelations(*element);
      }
      DeferTreeUpdate(TreeUpdateReason::kActiveDescendantChanged, element);
    } else if (attr_name == html_names::kAriaValuenowAttr ||
               attr_name == html_names::kAriaValuetextAttr) {
      HandleValueChanged(element);
    } else if (attr_name == html_names::kAriaLabelAttr) {
      TextChanged(element);
      DeferTreeUpdate(
          TreeUpdateReason::kSectionOrRegionRoleMaybeChangedFromLabel, element);
    } else if (attr_name == html_names::kAriaLabeledbyAttr ||
               attr_name == html_names::kAriaLabelledbyAttr) {
      if (relation_cache_) {
        relation_cache_->UpdateReverseTextRelations(*element, attr_name);
      }
      DeferTreeUpdate(
          TreeUpdateReason::kSectionOrRegionRoleMaybeChangedFromLabelledBy,
          element);
      TextChanged(element);
    } else if (attr_name == html_names::kAriaDescriptionAttr) {
      TextChanged(element);
    } else if (attr_name == html_names::kAriaDescribedbyAttr) {
      if (relation_cache_) {
        relation_cache_->UpdateReverseTextRelations(*element, attr_name);
      }
      TextChanged(element);
    } else if (attr_name == html_names::kAriaCheckedAttr) {
      PostNotification(element, ax::mojom::blink::Event::kCheckedStateChanged);
    } else if (attr_name == html_names::kAriaPressedAttr) {
      DeferTreeUpdate(TreeUpdateReason::kAriaPressedChanged, element);
    } else if (attr_name == html_names::kAriaSelectedAttr) {
      DeferTreeUpdate(TreeUpdateReason::kAriaSelectedChanged, element);
    } else if (attr_name == html_names::kAriaExpandedAttr) {
      DeferTreeUpdate(TreeUpdateReason::kAriaExpandedChanged, element);
    } else if (attr_name == html_names::kAriaHiddenAttr) {
      // Removing the subtree will also notify its parent that children changed,
      // causing the subtree to recursively be rebuilt with correct cached
      // values. In addition, changes to aria-hidden can affect with aria-owns
      // within the subtree are considered valid. Removing the subtree forces
      // any stale assumptions regarding aria-owns to be tossed, and the
      // resulting tree structure changes to occur as the subtree is rebuilt,
      // including restoring the natural parent of previously owned children
      // if the owner becomes aria-hidden.
      RemoveSubtree(element);
    } else if (attr_name == html_names::kAriaOwnsAttr) {
      if (relation_cache_) {
        relation_cache_->UpdateReverseOwnsRelations(*element);
      }
      DeferTreeUpdate(TreeUpdateReason::kAriaOwnsChanged, element);
    } else if (attr_name == html_names::kAriaHaspopupAttr) {
      if (AXObject* obj = Get(element)) {
        if (obj->RoleValue() == ax::mojom::blink::Role::kButton ||
            obj->RoleValue() == ax::mojom::blink::Role::kPopUpButton) {
          // The aria-haspopup attribute can switch the role between kButton and
          // kPopupButton.
          DeferTreeUpdate(TreeUpdateReason::kRoleChangeFromAriaHasPopup,
                          element);
        } else {
          MarkElementDirty(element);
        }
      }
    } else if (attr_name == html_names::kAriaControlsAttr ||
               attr_name == html_names::kAriaDetailsAttr ||
               attr_name == html_names::kAriaErrormessageAttr ||
               attr_name == html_names::kAriaFlowtoAttr) {
      MarkElementDirty(element);
      if (relation_cache_) {
        relation_cache_->UpdateReverseOtherRelations(*element);
      }
    } else {
      MarkElementDirty(element);
    }
    return;
  }

  if (attr_name == html_names::kRoleAttr ||
      attr_name == html_names::kTypeAttr) {
    DeferTreeUpdate(TreeUpdateReason::kRoleChangeFromRoleOrType, element);
  } else if (attr_name == html_names::kSizeAttr ||
             attr_name == html_names::kMultipleAttr) {
    if (IsA<HTMLSelectElement>(element)) {
      DeferTreeUpdate(TreeUpdateReason::kRoleMaybeChangedOnSelect, element);
    }
  } else if (attr_name == html_names::kAltAttr) {
    TextChanged(element);
  } else if (attr_name == html_names::kTitleAttr) {
    DeferTreeUpdate(TreeUpdateReason::kSectionOrRegionRoleMaybeChangedFromTitle,
                    element);
  } else if (attr_name == html_names::kForAttr) {
    if (relation_cache_) {
      if (HTMLLabelElement* label = DynamicTo<HTMLLabelElement>(element)) {
        if (Node* label_target = relation_cache_->LabelChanged(*label)) {
          // If label_target's subtree was ignored because it was hidden, it
          // will no longer be, because labels must be unignored to partake
          // in name calculations.
          MarkElementDirty(label_target);
        }
      }
    }
  } else if (attr_name == html_names::kIdAttr) {
    DeferTreeUpdate(TreeUpdateReason::kIdChanged, element);
  } else if (attr_name == html_names::kClassAttr) {
    MarkElementDirty(element);  // Reserialize the class.
  } else if (attr_name == html_names::kTabindexAttr) {
    MarkElementDirty(element);
  } else if (attr_name == html_names::kValueAttr) {
    HandleValueChanged(element);
  } else if (attr_name == html_names::kDisabledAttr ||
             attr_name == html_names::kReadonlyAttr ||
             attr_name == html_names::kMinAttr ||
             attr_name == html_names::kMaxAttr ||
             attr_name == html_names::kStepAttr) {
    MarkElementDirty(element);
  } else if (attr_name == html_names::kUsemapAttr) {
    DeferTreeUpdate(TreeUpdateReason::kUseMapAttributeChanged, element);
  } else if (attr_name == html_names::kNameAttr) {
    HandleNameAttributeChanged(element);
  } else if (attr_name == html_names::kControlsAttr) {
    ChildrenChanged(element);
  } else if (attr_name == html_names::kHrefAttr) {
    DeferTreeUpdate(TreeUpdateReason::kRoleMaybeChangedFromHref, element);
  } else if (attr_name == html_names::kLangAttr) {
    MarkElementDirty(element);
    // ATs may look at the language of the document as a whole on the root web
    // area. Since the root's language can come from the <html> element's
    // language, if the language changes on <html>, we need to update the root.
    if (element == document_->documentElement()) {
      MarkElementDirty(document_);
    }
  }
}

void AXObjectCacheImpl::HandleUseMapAttributeChangedWithCleanLayout(
    Node* node) {
  if (!IsA<HTMLImageElement>(node)) {
    return;
  }
  // Get an area (aka image link) from the previous usemap.
  AXObject* ax_image = Get(node);
  AXObject* ax_image_link =
      ax_image ? ax_image->FirstChildIncludingIgnored() : nullptr;
  HTMLMapElement* previous_map =
      ax_image_link && ax_image_link->GetNode()
          ? Traversal<HTMLMapElement>::FirstAncestor(*ax_image_link->GetNode())
          : nullptr;
  // Both the old and new image may change image <--> image map.
  HandleRoleChangeWithCleanLayout(node);
  if (previous_map)
    HandleRoleChangeWithCleanLayout(previous_map->ImageElement());
}

void AXObjectCacheImpl::HandleNameAttributeChanged(Node* node) {
  HTMLMapElement* map = DynamicTo<HTMLMapElement>(node);
  if (!map) {
    return;
  }

  // Changing a map name can alter an image's role and children.
  // First update any image that may have used the old map name.
  if (AXObject* ax_previous_image = GetAXImageForMap(*map)) {
    DeferTreeUpdate(TreeUpdateReason::kRoleChangeFromImageMapName,
                    ax_previous_image->GetElement());
  }

  // Then, update any image which may use the new map name.
  HTMLImageElement* new_image = map->ImageElement();
  if (new_image) {
    if (AXObject* obj = Get(new_image)) {
      DeferTreeUpdate(TreeUpdateReason::kRoleChangeFromImageMapName,
                      obj->GetElement());
    }
  }
}

AXObject* AXObjectCacheImpl::GetOrCreateValidationMessageObject() {
  // New AXObjects cannot be created when the tree is frozen.
  AXObject* message_ax_object = nullptr;
  // Create only if it does not already exist.
  if (validation_message_axid_) {
    message_ax_object = ObjectFromAXID(validation_message_axid_);
  }
  if (message_ax_object) {
    DCHECK(!message_ax_object->IsDetached());
    if (message_ax_object->IsMissingParent()) {
      message_ax_object->SetParent(Root());  // Reattach to parent (root).
    } else {
      DCHECK(message_ax_object->ParentObject() == Root());
    }
  } else {
    if (IsFrozen()) {
      return nullptr;
    }
    message_ax_object = MakeGarbageCollected<AXValidationMessage>(*this);
    CHECK(message_ax_object);
    CHECK(!message_ax_object->IsDetached());
    // Cache the validation message container for reuse.
    validation_message_axid_ = AssociateAXID(message_ax_object);
    // Validation message alert object is a child of the document, as not all
    // form controls can have a child. Also, there are form controls such as
    // listbox that technically can have children, but they are probably not
    // expected to have alerts within AT client code.
    message_ax_object->Init(Root());
  }
  CHECK(!message_ax_object->IsDetached());
  return message_ax_object;
}

AXObject* AXObjectCacheImpl::ValidationMessageObjectIfInvalid() {
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
        HeapVector<Member<Element>> error_messages;
        // Create the validation message unless the focused form control is
        // overriding it with a different message via aria-errormessage.
        if (!AccessibleNode::GetPropertyOrARIAAttribute(
                focused_element, AOMRelationListProperty::kErrorMessage,
                error_messages)) {
          AXObject* message = GetOrCreateValidationMessageObject();
          CHECK(message);
          CHECK(!message->IsDetached());
          CHECK_EQ(message->ParentObject(), Root());
          return message;
        }
      }
    }
  }

  // No focused, invalid form control.
  if (validation_message_axid_) {
    RemoveValidationMessageObjectWithCleanLayout(document_);
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
    Remove(validation_message_axid_, /* notify_parent */ false);
    validation_message_axid_ = 0;
  }
  ChildrenChangedWithCleanLayout(document_);
}

// Native validation error popup for focused form control in current document.
void AXObjectCacheImpl::HandleValidationMessageVisibilityChanged(
    Node* form_control) {
  DCHECK(form_control);
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION();

  DeferTreeUpdate(TreeUpdateReason::kValidationMessageVisibilityChanged,
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

  if (AXObject* message_ax_object = ValidationMessageObjectIfInvalid()) {
    MarkAXObjectDirtyWithCleanLayout(message_ax_object);
  }

  ChildrenChangedWithCleanLayout(Root());

  // If the form control is invalid, it will now have an error message relation
  // to the message container.
  MarkElementDirtyWithCleanLayout(form_control);
}

void AXObjectCacheImpl::HandleEventListenerAdded(
    Node& node,
    const AtomicString& event_type) {
  // If this is the first |event_type| listener for |node|, handle the
  // subscription change.
  if (node.NumberOfEventListeners(event_type) == 1)
    HandleEventSubscriptionChanged(node, event_type);
}

void AXObjectCacheImpl::HandleEventListenerRemoved(
    Node& node,
    const AtomicString& event_type) {
  // If there are no more |event_type| listeners for |node|, handle the
  // subscription change.
  if (node.NumberOfEventListeners(event_type) == 0)
    HandleEventSubscriptionChanged(node, event_type);
}

void AXObjectCacheImpl::HandleReferenceTargetChanged(Element& element) {
  DeferTreeUpdate(TreeUpdateReason::kReferenceTargetChanged, &element);
}

bool AXObjectCacheImpl::DoesEventListenerImpactIgnoredState(
    const AtomicString& event_type,
    const Node& node) const {
  // An SVG graphics element with a focus event listener is focusable, which
  // causes it to be unignored.
  if (auto* svg_graphics_element = DynamicTo<SVGGraphicsElement>(node)) {
    if (svg_graphics_element->HasFocusEventListeners()) {
      return true;
    }
  }
  // A mouse event listener causes a node to be unignored.
  return event_util::IsMouseButtonEventType(event_type);
}

void AXObjectCacheImpl::HandleEventSubscriptionChanged(
    Node& node,
    const AtomicString& event_type) {
  // Adding or Removing an event listener for certain events may affect whether
  // a node or its descendants should be accessibility ignored.
  if (!DoesEventListenerImpactIgnoredState(event_type, node)) {
    return;
  }

  MarkElementDirty(&node);
  // If the ignored state changes, the parent's children may have changed.
  if (AXObject* obj = Get(&node)) {
    if (!obj->IsDetached()) {
      if (obj->ParentObject()) {
        ChildrenChanged(obj->ParentObject());
        // ChildrenChanged() can cause the obj to be detached.
        if (obj->IsDetached()) {
          return;
        }
      }

      DeferTreeUpdate(TreeUpdateReason::kRoleMaybeChangedFromEventListener,
                      &node);
    }
  }
}

void AXObjectCacheImpl::CSSAnchorChangedWithCleanLayout(Node* positioned_node) {
  relation_cache_->UpdateCSSAnchorFor(positioned_node);
}

void AXObjectCacheImpl::AriaOwnsChangedWithCleanLayout(Node* node) {
  CHECK(relation_cache_);
  if (AXObject* obj = Get(node)) {
    relation_cache_->UpdateAriaOwnsWithCleanLayout(obj);
  }
}

void AXObjectCacheImpl::InlineTextBoxesUpdated(LayoutObject* layout_object) {
  if (AXObject* obj = Get(layout_object)) {
    // Only update if the accessibility object already exists and it's
    // not already marked as dirty.
    CHECK(!obj->IsDetached());
    if (obj->ShouldLoadInlineTextBoxes()) {
      obj->SetNeedsToUpdateChildren();
      obj->ClearChildren();
      MarkAXObjectDirty(obj);
    }
  }
}

Settings* AXObjectCacheImpl::GetSettings() {
  return document_->GetSettings();
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

  const AXObject* ax_object = Get(const_cast<Node*>(node));
  return ax_object && ax_object->IsTextField();
}

WebLocalFrameClient* AXObjectCacheImpl::GetWebLocalFrameClient() const {
  DCHECK(document_);
  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(document_->AXObjectCacheOwner().GetFrame());
  if (!web_frame)
    return nullptr;
  WebLocalFrameClient* client = web_frame->Client();
  DCHECK(client);
  return client;
}

bool AXObjectCacheImpl::IsImmediateProcessingRequiredForEvent(
    ax::mojom::blink::EventFrom& event_from,
    AXObject* target,
    ax::mojom::blink::Event& event_type) const {
  // Already scheduled for immediate mode.
  if (serialize_immediately_) {
    return true;
  }

  // Actions should result in an immediate response.
  if (event_from == ax::mojom::blink::EventFrom::kAction) {
    return true;
  }

  // It's important for the user to have access to any changes to the
  // currently focused object, so schedule serializations immediately if that
  // object changes. The root is an exception because it often has focus while
  // the page is loading.
  if (target->GetNode() != document_ && target->IsFocused()) {
    return true;
  }

  switch (event_type) {
    case ax::mojom::blink::Event::kActiveDescendantChanged:
    case ax::mojom::blink::Event::kBlur:
    case ax::mojom::blink::Event::kCheckedStateChanged:
    case ax::mojom::blink::Event::kClicked:
    case ax::mojom::blink::Event::kDocumentSelectionChanged:
    case ax::mojom::blink::Event::kExpandedChanged:
    case ax::mojom::blink::Event::kFocus:
    case ax::mojom::blink::Event::kHover:
    case ax::mojom::blink::Event::kLoadComplete:
    case ax::mojom::blink::Event::kLoadStart:
    case ax::mojom::blink::Event::kRowExpanded:
    case ax::mojom::blink::Event::kScrolledToAnchor:
    case ax::mojom::blink::Event::kSelectedChildrenChanged:
    case ax::mojom::blink::Event::kValueChanged:
      return true;

    case ax::mojom::blink::Event::kDocumentTitleChanged:
    case ax::mojom::blink::Event::kLayoutComplete:
    case ax::mojom::blink::Event::kLocationChanged:
    case ax::mojom::blink::Event::kRowCollapsed:
    case ax::mojom::blink::Event::kRowCountChanged:
    case ax::mojom::blink::Event::kScrollPositionChanged:
    case ax::mojom::blink::Event::kTextChanged:
      return false;

    // These events are not fired from Blink.
    // This list is duplicated in WebFrameTestProxy::PostAccessibilityEvent().
    case ax::mojom::blink::Event::kAlert:
    case ax::mojom::blink::Event::kAriaAttributeChangedDeprecated:
    case ax::mojom::blink::Event::kAutocorrectionOccured:
    case ax::mojom::blink::Event::kChildrenChanged:
    case ax::mojom::blink::Event::kControlsChanged:
    case ax::mojom::blink::Event::kEndOfTest:
    case ax::mojom::blink::Event::kFocusAfterMenuClose:
    case ax::mojom::blink::Event::kFocusContext:
    case ax::mojom::blink::Event::kHide:
    case ax::mojom::blink::Event::kHitTestResult:
    case ax::mojom::blink::Event::kImageFrameUpdated:
    case ax::mojom::blink::Event::kLiveRegionCreated:
    case ax::mojom::blink::Event::kLiveRegionChanged:
    case ax::mojom::blink::Event::kMediaStartedPlaying:
    case ax::mojom::blink::Event::kMediaStoppedPlaying:
    case ax::mojom::blink::Event::kMenuEnd:
    case ax::mojom::blink::Event::kMenuListValueChangedDeprecated:
    case ax::mojom::blink::Event::kMenuPopupEnd:
    case ax::mojom::blink::Event::kMenuPopupStart:
    case ax::mojom::blink::Event::kMenuStart:
    case ax::mojom::blink::Event::kMouseCanceled:
    case ax::mojom::blink::Event::kMouseDragged:
    case ax::mojom::blink::Event::kMouseMoved:
    case ax::mojom::blink::Event::kMousePressed:
    case ax::mojom::blink::Event::kMouseReleased:
    case ax::mojom::blink::Event::kNone:
    case ax::mojom::blink::Event::kSelection:
    case ax::mojom::blink::Event::kSelectionAdd:
    case ax::mojom::blink::Event::kSelectionRemove:
    case ax::mojom::blink::Event::kShow:
    case ax::mojom::blink::Event::kStateChanged:
    case ax::mojom::blink::Event::kTextSelectionChanged:
    case ax::mojom::blink::Event::kTooltipClosed:
    case ax::mojom::blink::Event::kTooltipOpened:
    case ax::mojom::blink::Event::kTreeChanged:
    case ax::mojom::blink::Event::kWindowActivated:
    case ax::mojom::blink::Event::kWindowDeactivated:
    case ax::mojom::blink::Event::kWindowVisibilityChanged:
      // Never fired from Blink.
      NOTREACHED_IN_MIGRATION()
          << "Event not expected from Blink: " << event_type;
      return false;
  }
}

bool AXObjectCacheImpl::IsImmediateProcessingRequired(
    TreeUpdateParams* tree_update) const {
  // For now, immediate processing is never required for deferred AXObject
  // updates, and this method doesn't need to be called for that case.
  CHECK(!tree_update->axid);

  // Already scheduled for immediate mode.
  if (serialize_immediately_) {
    return true;
  }

  // Get some initial content as soon as possible.
  if (objects_.size() <= 1) {
    return true;
  }

  // Actions should result in an immediate response.
  if (tree_update->event_from_action != ax::mojom::blink::Action::kNone) {
    return true;
  }

  // It's important for the user to have access to any changes to the
  // currently focused object, so schedule serializations immediately if that
  // object changes. The root is an exception because it often has focus while
  // the page is loading.
  if (tree_update->node != document_ &&
      tree_update->node == document_->FocusedElement()) {
    return true;
  }

  switch (tree_update->update_reason) {
    // These updates are associated with a Node:
    case TreeUpdateReason::kActiveDescendantChanged:
    case TreeUpdateReason::kAriaExpandedChanged:
    case TreeUpdateReason::kAriaPressedChanged:
    case TreeUpdateReason::kAriaSelectedChanged:
    case TreeUpdateReason::kDidShowMenuListPopup:
    case TreeUpdateReason::kEditableTextContentChanged:
    case TreeUpdateReason::kNodeGainedFocus:
    case TreeUpdateReason::kNodeLostFocus:
    case TreeUpdateReason::kPostNotificationFromHandleLoadComplete:
    case TreeUpdateReason::kUpdateActiveMenuOption:
    case TreeUpdateReason::kValidationMessageVisibilityChanged:
      return true;

    case TreeUpdateReason::kAriaOwnsChanged:
    case TreeUpdateReason::kCSSAnchorChanged:
    case TreeUpdateReason::kDelayEventFromPostNotification:
    case TreeUpdateReason::kFocusableChanged:
    case TreeUpdateReason::kIdChanged:
    case TreeUpdateReason::kNodeIsAttached:
    case TreeUpdateReason::kPostNotificationFromHandleLoadStart:
    case TreeUpdateReason::kPostNotificationFromHandleScrolledToAnchor:
    case TreeUpdateReason::kReferenceTargetChanged:
    case TreeUpdateReason::kRemoveValidationMessageObjectFromFocusedUIElement:
    case TreeUpdateReason::
        kRemoveValidationMessageObjectFromValidationMessageObject:
    case TreeUpdateReason::kRoleChangeFromAriaHasPopup:
    case TreeUpdateReason::kRoleChangeFromImageMapName:
    case TreeUpdateReason::kRoleChangeFromRoleOrType:
    case TreeUpdateReason::kRoleMaybeChangedFromEventListener:
    case TreeUpdateReason::kRoleMaybeChangedFromHref:
    case TreeUpdateReason::kRoleMaybeChangedOnSelect:
    case TreeUpdateReason::kSectionOrRegionRoleMaybeChangedFromLabel:
    case TreeUpdateReason::kSectionOrRegionRoleMaybeChangedFromLabelledBy:
    case TreeUpdateReason::kSectionOrRegionRoleMaybeChangedFromTitle:
    case TreeUpdateReason::kTextChangedOnNode:
    case TreeUpdateReason::kTextChangedOnClosestNodeForLayoutObject:
    case TreeUpdateReason::kTextMarkerDataAdded:
    case TreeUpdateReason::kUpdateAriaOwns:
    case TreeUpdateReason::kUpdateTableRole:
    case TreeUpdateReason::kUseMapAttributeChanged:
      return false;

    // These updates are associated with an AXID:
    case TreeUpdateReason::kChildrenChanged:
    case TreeUpdateReason::kMarkAXObjectDirty:
    case TreeUpdateReason::kMarkAXSubtreeDirty:
    case TreeUpdateReason::kTextChangedOnLayoutObject:
      return false;
  }
}

// The lifecycle serialization works as follows:
// 1) Dirty objects and events are fired through
// AXObjectCacheImpl::PostPlatformNotification which in turn makes a call to
// AXObjectCacheImpl::AddEventToSerializationQueue to queue it.
//
// 2) When the lifecycle is ready to be serialized,
// AXObjectCacheImpl::CommitAXUpdates is called which first
// checks if it's time to make a new serialization, and if not, it will early
// return in order to add a delay between serializations.
//
// 3) AXObjectCacheImpl::CommitAXUpdates then calls
// RenderAccessibilityImpl:AXReadyCallback to start serialization process.
//
// Check the below CL for more information:
// https://chromium-review.googlesource.com/c/chromium/src/+/4994320
void AXObjectCacheImpl::AddEventToSerializationQueue(
    const ui::AXEvent& event,
    bool immediate_serialization) {
  CHECK(lifecycle_.StateAllowsQueueingEventsForSerialization()) << *this;

  AXObject* obj = ObjectFromAXID(event.id);
  DCHECK(!obj->IsDetached());

  pending_events_to_serialize_.push_back(event);

  AddDirtyObjectToSerializationQueue(
      obj, event.event_from, event.event_from_action, event.event_intents);

  if (immediate_serialization) {
    ScheduleImmediateSerialization();
  }
}

void AXObjectCacheImpl::OnSerializationCancelled() {
  serialization_in_flight_ = false;
}

void AXObjectCacheImpl::OnSerializationStartSend() {
  serialization_in_flight_ = true;
}

bool AXObjectCacheImpl::IsSerializationInFlight() const {
  return serialization_in_flight_;
}

void AXObjectCacheImpl::OnSerializationReceived() {
  serialization_in_flight_ = false;
  last_serialization_timestamp_ = base::Time::Now();

  // Another serialization may be needed, in the case where the AXObjectCache is
  // dirty. In that case, make sure a visual update is scheduled so that
  // AXReadyCallback() will be called. ScheduleAXUpdate() will only schedule a
  // visual update if the AXObjectCache is dirty.
  if (serialize_immediately_after_current_serialization_) {
    serialize_immediately_after_current_serialization_ = false;
    ScheduleImmediateSerialization();
  } else {
    ScheduleAXUpdate();
  }
}

void AXObjectCacheImpl::ScheduleImmediateSerialization() {
  if (IsSerializationInFlight()) {
    // Wait until current serialization message has been received.
    serialize_immediately_after_current_serialization_ = true;
    return;
  }

  serialize_immediately_ = true;

  // Call ScheduleAXUpdate() to ensure lifecycle does not get stalled.
  // Will call AXReadyCallback() at the next available opportunity.
  ScheduleAXUpdate();
}

void AXObjectCacheImpl::PostPlatformNotification(
    AXObject* obj,
    ax::mojom::blink::Event event_type,
    ax::mojom::blink::EventFrom event_from,
    ax::mojom::blink::Action event_from_action,
    const BlinkAXEventIntentsSet& event_intents) {
  CHECK(lifecycle_.StateAllowsQueueingEventsForSerialization()) << *this;

  obj = GetSerializationTarget(obj);
  if (!obj)
    return;

  ui::AXEvent event;
  event.id = obj->AXObjectID();
  event.event_type = event_type;
  event.event_from = event_from;
  event.event_from_action = event_from_action;
  event.event_intents.resize(event_intents.size());
  // We need to filter out the counts from every intent.
  base::ranges::transform(
      event_intents, event.event_intents.begin(),
      [](const auto& intent) { return intent.key.intent(); });
  for (auto agent : agents_)
    agent->AXEventFired(obj, event_type);

  // Since we're in the middle of processing deferred events anyways, we know
  // this will be immediately serialized.
  AddEventToSerializationQueue(event, /* immediate_serialization */ false);

  // TODO(aleventhal) This is for web tests only, in order to record MarkDirty
  // events. Is there a way to avoid these calls for normal browsing?
  // Maybe we should use dependency injection from AccessibilityController.
  if (auto* client = GetWebLocalFrameClient()) {
    client->HandleWebAccessibilityEventForTest(event);
  }
}

void AXObjectCacheImpl::MarkAXObjectDirtyWithCleanLayoutHelper(
    AXObject* obj,
    ax::mojom::blink::EventFrom event_from,
    ax::mojom::blink::Action event_from_action) {
  CHECK(!IsFrozen());
  obj = GetSerializationTarget(obj);
  if (!obj)
    return;

  obj->SetAncestorsHaveDirtyDescendants();

  // If the content is inside the popup, mark the owning element dirty.
  // TODO(aleventhal): not sure why this works, but now that we run a11y in
  // PostRunLifecycleTasks(), we need this, otherwise the pending updates in
  // the popup aren't processed.
  if (IsPopup(*obj->GetDocument())) {
    MarkElementDirtyWithCleanLayout(GetDocument().FocusedElement());
  }

  // TODO(aleventhal) This is for web tests only, in order to record MarkDirty
  // events. Is there a way to avoid these calls for normal browsing?
  // Maybe we should use dependency injection from AccessibilityController.
  if (auto* client = GetWebLocalFrameClient()) {
    client->HandleWebAccessibilityEventForTest(
        WebAXObject(obj), "MarkDirty", std::vector<ui::AXEventIntent>());
  }

  std::vector<ui::AXEventIntent> event_intents;
  AddDirtyObjectToSerializationQueue(obj, event_from, event_from_action,
                                     event_intents);

  obj->UpdateCachedAttributeValuesIfNeeded(true);
}

void AXObjectCacheImpl::MarkAXObjectDirtyWithCleanLayout(AXObject* obj) {
  if (!obj) {
    return;
  }
  MarkAXObjectDirtyWithCleanLayoutHelper(obj, active_event_from_,
                                         active_event_from_action_);
  if (!obj->IsIncludedInTree()) {
    obj = obj->ParentObjectIncludedInTree();
  }
  for (auto agent : agents_) {
    agent->AXObjectModified(obj, /*subtree*/ false);
  }
}

void AXObjectCacheImpl::MarkAXObjectDirty(AXObject* obj) {
  if (!obj)
    return;

  // TODO(accessibility) Consider catching all redundant dirty object work,
  // perhaps by setting a flag on the AXObject, or by adding the id to a set of
  // already-dirtied objects.
  DeferTreeUpdate(TreeUpdateReason::kMarkAXObjectDirty, obj);
}

void AXObjectCacheImpl::NotifySubtreeDirty(AXObject* obj) {
  DUMP_WILL_BE_CHECK(obj->IsIncludedInTree());

  // Note: if there is no serializer yet, then there is nothing to mark dirty
  // for serialization purposes yet -- effectively everything starts out dirty
  // in a new serializer.
  if (ax_tree_serializer_) {
    ax_tree_serializer_->MarkSubtreeDirty(obj->AXObjectID());
  }
  for (auto agent : agents_) {
    agent->AXObjectModified(obj, /*subtree*/ true);
  }
}

void AXObjectCacheImpl::MarkAXSubtreeDirtyWithCleanLayout(AXObject* obj) {
  if (!obj) {
    return;
  }
  if (!obj->IsIncludedInTree()) {
    for (const auto& included_child : obj->ChildrenIncludingIgnored()) {
      MarkAXSubtreeDirtyWithCleanLayout(included_child);
    }
    return;
  }

  MarkAXObjectDirtyWithCleanLayoutHelper(obj, active_event_from_,
                                         active_event_from_action_);
  NotifySubtreeDirty(obj);
}

void AXObjectCacheImpl::MarkAXSubtreeDirty(AXObject* obj) {
  if (!obj)
    return;

  DeferTreeUpdate(TreeUpdateReason::kMarkAXSubtreeDirty, obj);
}

void AXObjectCacheImpl::MarkSubtreeDirty(Node* node) {
  if (AXObject* obj = Get(node)) {
    MarkAXSubtreeDirty(obj);
  } else if (node) {
    // There is no AXObject, so there is no subtree to mark dirty.
    MarkElementDirty(node);
  }
}

void AXObjectCacheImpl::MarkDocumentDirty() {
  CHECK(!IsFrozen());
  mark_all_dirty_ = true;

  ScheduleAXUpdate();
}

void AXObjectCacheImpl::MarkDocumentDirtyWithCleanLayout() {
  // This function will cause everything to be reserialized from the root down,
  // but will not create new AXObjects, which avoids resetting the user's
  // position in the content.
  DCHECK(mark_all_dirty_);

  // Assume all nodes in the tree need to recompute their properties.
  // Note that objects can remain in the tree without being re-created.
  // However, they will be dropped if they are no longer needed as the tree
  // structure is rebuilt from the top down.
  for (auto& entry : objects_) {
    AXObject* object = entry.value;
    DCHECK(!object->IsDetached());
    object->InvalidateCachedValues();
  }

  // Don't keep previous parent-child relationships.
  // This loop operates on a copy of values in the objects_ map, because some
  // entries may be removed from objects_ while iterating.
  HeapVector<Member<AXObject>> objects;
  CopyValuesToVector(objects_, objects);
  for (auto& object : objects) {
    if (!object->IsDetached()) {
      object->SetNeedsToUpdateChildren();
    }
  }

  // Clear anything about to be serialized, because everything will be
  // reserialized anyway.
  pending_events_to_serialize_.clear();
  pending_objects_to_serialize_.clear();
  changed_bounds_ids_.clear();
  cached_bounding_boxes_.clear();

  // Tell the serializer that everything will need to be serialized.
  DCHECK(Root());
  Root()->SetHasDirtyDescendants(true);
  MarkAXSubtreeDirtyWithCleanLayout(Root());
  ChildrenChangedWithCleanLayout(Root());
  // Do not trim out load complete messages, they must be fired.
  if (!load_sent_ && GetDocument().IsLoadCompleted()) {
    PostNotification(&GetDocument(), ax::mojom::blink::Event::kLoadComplete);
  }
}

void AXObjectCacheImpl::ResetSerializer() {
  if (ax_tree_serializer_) {
    ax_tree_serializer_->Reset();
  }
  if (plugin_serializer_.get()) {
    plugin_serializer_->Reset();
  }

  // Clear anything about to be serialized, because everything will be
  // reserialized anyway.
  pending_objects_to_serialize_.clear();
  pending_events_to_serialize_.clear();
  changed_bounds_ids_.clear();
  cached_bounding_boxes_.clear();

  // Send the serialization at the next available opportunity.
  ScheduleAXUpdate();
}

void AXObjectCacheImpl::MarkElementDirty(const Node* element) {
  // Warning, if no AXObject exists for element, nothing is marked dirty.
  MarkAXObjectDirty(Get(element));
}

WTF::Vector<TextChangedOperation>*
AXObjectCacheImpl::GetFromTextOperationInNodeIdMap(AXID id) {
  auto it = text_operation_in_node_ids_.find(id);
  if (it != text_operation_in_node_ids_.end()) {
    return &it.Get()->value;
  }
  return nullptr;
}

void AXObjectCacheImpl::ClearTextOperationInNodeIdMap() {
  text_operation_in_node_ids_.clear();
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

  // Return included in tree object.
  if (obj->IsIncludedInTree())
    return obj;

  return obj->ParentObjectIncludedInTree();
}

void AXObjectCacheImpl::RestoreParentOrPrune(Node* child_node) {
  AXObject* child = Get(child_node);
  if (child) {
    ChildrenChangedOnAncestorOf(child);
    // The previous call can cause child to become detached.
    if (child->IsDetached()) {
      child = nullptr;
    }
  }

  AXObject* parent = child ? child->ComputeParentOrNull()
                           : AXObject::ComputeNonARIAParent(*this, child_node);
  if (parent && child) {
    child->SetParent(parent);
    ChildrenChangedOnAncestorOf(child);
  } else {
    // If no parent is currently available, the child may no longer be part of
    // the tree. Remove the child's subtree and ask the parent (if any) to
    // rebuild its subtree.
    RemoveSubtree(child_node);
    if (lifecycle_.StateAllowsImmediateTreeUpdates()) {
      ChildrenChangedWithCleanLayout(parent);
    } else {
      ChildrenChanged(parent);
    }
  }
}

void AXObjectCacheImpl::HandleFocusedUIElementChanged(
    Element* old_focused_element,
    Element* new_focused_element) {
#if DCHECK_IS_ON()
  // The focus can be in a different document when a popup is open.
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION();
#endif  // DCHECK_IS_ON()

  if (validation_message_axid_) {
    DeferTreeUpdate(
        TreeUpdateReason::kRemoveValidationMessageObjectFromFocusedUIElement,
        document_);
  }

  if (!new_focused_element) {
    // When focus is cleared, implicitly focus the document by sending a blur.
    if (GetDocument().documentElement()) {
      DeferTreeUpdate(TreeUpdateReason::kNodeLostFocus,
                      GetDocument().documentElement());
    }
    return;
  }

  Page* page = new_focused_element->GetDocument().GetPage();
  if (!page)
    return;

  new_focused_element = RetargetInput(new_focused_element);
  old_focused_element = RetargetInput(old_focused_element);

  if (old_focused_element) {
    DeferTreeUpdate(TreeUpdateReason::kNodeLostFocus, old_focused_element);
  }

  UpdateActiveAriaModalDialog(new_focused_element);

  DeferTreeUpdate(TreeUpdateReason::kNodeGainedFocus, FocusedNode());
}

// Check if the focused node is inside an active aria-modal dialog. If so, we
// should mark the cache as dirty to recompute the ignored status of each node.
void AXObjectCacheImpl::UpdateActiveAriaModalDialog(Node* focused_node) {
  Settings* settings = GetSettings();
  if (!settings || !settings->GetAriaModalPrunesAXTree()) {
    return;
  }

  Element* new_active_aria_modal = AncestorAriaModalDialog(focused_node);
  if (active_aria_modal_dialog_ == new_active_aria_modal)
    return;

  active_aria_modal_dialog_ = new_active_aria_modal;
  MarkDocumentDirty();
}

Element* AXObjectCacheImpl::AncestorAriaModalDialog(Node* node) {
  // Find an element with role=dialog|alertdialog and aria-modal="true" that
  // either contains the focus, or is focused.
  do {
    Element* element = DynamicTo<Element>(node);
    if (element) {
      const AtomicString& role_str =
          AXObject::AriaAttribute(*element, html_names::kRoleAttr);
      if (!role_str.empty() &&
          ui::IsDialog(AXObject::FirstValidRoleInRoleString(role_str))) {
        if (AXObject::IsAriaAttributeTrue(*element,
                                          html_names::kAriaModalAttr)) {
          return element;
        }
      }
    }
    node = FlatTreeTraversal::Parent(*node);
  } while (node);

  return nullptr;
}

Element* AXObjectCacheImpl::GetActiveAriaModalDialog() const {
  return active_aria_modal_dialog_;
}

ui::AXLocationAndScrollUpdates
AXObjectCacheImpl::TakeLocationChangsForSerialization() {
  CHECK(!changed_bounds_ids_.empty());

  TRACE_EVENT0("accessibility",
               load_sent_ ? "TakeLocationChangsForSerialization"
                          : "TakeLocationChangsForSerializationLoading");
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
      "Accessibility.Performance.TakeLocationChangsForSerialization");

  ui::AXLocationAndScrollUpdates changes;

  // Reserve is just an optimization. The actual value doesn't have to be
  // accurate but just an estimate. Assume the changes will always be half and
  // half.
  changes.location_changes.reserve(changed_bounds_ids_.size());
  changes.scroll_changes.reserve(changed_bounds_ids_.size());

  for (AXID changed_bounds_id : changed_bounds_ids_) {
    if (AXObject* obj = ObjectFromAXID(changed_bounds_id)) {
      DCHECK(!obj->IsDetached());
      // Only update locations that are already known.
      auto bounds = cached_bounding_boxes_.find(changed_bounds_id);
      if (bounds == cached_bounding_boxes_.end()) {
        continue;
      }

      ui::AXRelativeBounds new_location;
      bool clips_children;
      obj->PopulateAXRelativeBounds(new_location, &clips_children);
      gfx::Point scroll_offset = obj->GetScrollOffset();

      if (bounds->value.bounds != new_location) {
        changes.location_changes.emplace_back(changed_bounds_id, new_location);
      }

      if (bounds->value.scroll_x != scroll_offset.x() ||
          bounds->value.scroll_y != scroll_offset.y()) {
        changes.scroll_changes.emplace_back(
            changed_bounds_id, scroll_offset.x(), scroll_offset.y());
      }

      cached_bounding_boxes_.Set(
          changed_bounds_id,
          CachedLocationChange(new_location, scroll_offset.x(),
                               scroll_offset.y()));
    }
  }

  changed_bounds_ids_.clear();
  last_location_serialization_time_ =
      base::Time::Now();  // Since this method is non-recoverable, update the
                          // time here and assume this serializtion will arrive.
  return changes;
}

void AXObjectCacheImpl::SerializeLocationChanges() {
  // We wait until the document load is complete because layout often shifts
  // during the load process.
  if (!GetDocument().IsLoadCompleted()) {
    return;
  }

  CHECK(GetDocument().IsActive());
  TRACE_EVENT0("accessibility", load_sent_ ? "SerializeLocationChanges"
                                           : "SerializeLocationChangesLoading");
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
      "Accessibility.Performance.SerializeLocationChanges");

  // Ensure enough time has passed since last locations serialization.
  Document& document = GetDocument();
  const auto& now = base::Time::Now();
  const auto& delay_between_serializations =
      base::Milliseconds(GetLocationSerializationDelay());
  const auto& elapsed_since_last_serialization =
      now - last_location_serialization_time_;
  const auto& delay_until_next_serialization =
      delay_between_serializations - elapsed_since_last_serialization;
  if (delay_until_next_serialization.is_positive()) {
    // No serialization needed yet, will serialize after a delay.
    // Set a timer to call this method again, if one isn't already set.
    if (!weak_factory_for_loc_updates_pipeline_.HasWeakCells()) {
      document.GetTaskRunner(blink::TaskType::kInternalDefault)
          ->PostDelayedTask(
              FROM_HERE,
              WTF::BindOnce(
                  &AXObjectCacheImpl::ScheduleAXUpdate,
                  WrapPersistent(
                      weak_factory_for_loc_updates_pipeline_.GetWeakCell())),
              delay_until_next_serialization);
    }
    return;
  }

  weak_factory_for_loc_updates_pipeline_.Invalidate();

  ui::AXLocationAndScrollUpdates changes = TakeLocationChangsForSerialization();

  // Convert to blink mojom type
  ax::mojom::blink::AXLocationAndScrollUpdatesPtr location_and_scroll_changes =
      ax::mojom::blink::AXLocationAndScrollUpdates::New();
  for (auto& item : changes.location_changes) {
    location_and_scroll_changes->location_changes.push_back(
        ax::mojom::blink::AXLocationChange::New(item.id, item.new_location));
  }
  for (auto& item : changes.scroll_changes) {
    location_and_scroll_changes->scroll_changes.push_back(
        ax::mojom::blink::AXScrollChange::New(item.id, item.scroll_x,
                                              item.scroll_y));
  }

  if (!location_and_scroll_changes->location_changes.empty() ||
      !location_and_scroll_changes->scroll_changes.empty()) {
    CHECK(reset_token_);
    GetOrCreateRemoteRenderAccessibilityHost()->HandleAXLocationChanges(
        std::move(location_and_scroll_changes), *reset_token_);
  }
}

bool AXObjectCacheImpl::SerializeEntireTree(
    size_t max_node_count,
    base::TimeDelta timeout,
    ui::AXTreeUpdate* response,
    std::set<ui::AXSerializationErrorFlag>* out_error) {
  // Ensure that an initial tree exists.
  CHECK(IsFrozen());
  CHECK(!IsDirty());
  CHECK(Root());
  CHECK(!Root()->IsDetached());
  CHECK(GetDocument().IsActive());

  BlinkAXTreeSource* tree_source =
      BlinkAXTreeSource::Create(*this, /* is_snapshot */ true);
  // The new tree source is frozen for its entire lifetime.
  tree_source->Freeze();

  // The serializer returns an ui::AXTreeUpdate, which can store a complete
  // or a partial accessibility tree. AXTreeSerializer is stateful, but the
  // first time you serialize from a brand-new tree you're guaranteed to get a
  // complete tree.
  ui::AXTreeSerializer<const AXObject*, HeapVector<Member<const AXObject>>,
                       ui::AXTreeUpdate*, ui::AXTreeData*, ui::AXNodeData>
      serializer(tree_source);

  if (max_node_count)
    serializer.set_max_node_count(max_node_count);
  if (!timeout.is_zero())
    serializer.set_timeout(timeout);

  bool success = serializer.SerializeChanges(Root(), response, out_error);
  CHECK(success)
      << "Serializer failed. Should have hit DCHECK inside of serializer.";

  if (RuntimeEnabledFeatures::AccessibilitySerializationSizeMetricsEnabled()) {
    // For a tree snapshot, we don't break down by type.
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Accessibility.Performance.AXObjectCacheImpl.Snapshot",
        base::saturated_cast<int>(response->ByteSize()), 1, kSizeGb,
        kBucketCount);
  }

  return true;
}

void AXObjectCacheImpl::AddDirtyObjectToSerializationQueue(
    const AXObject* obj,
    ax::mojom::blink::EventFrom event_from,
    ax::mojom::blink::Action event_from_action,
    const std::vector<ui::AXEventIntent>& event_intents) {
  CHECK(!IsFrozen());
  CHECK(lifecycle_.StateAllowsQueueingAXObjectsForSerialization()) << *this;

  // If not included, cannot be serialized, so there is no need to queue.
  if (!obj->IsIncludedInTree()) {
    return;
  }

  // Add to object to a queue that will be sent to the serializer in
  // SerializeDirtyObjectsAndEvents().
  pending_objects_to_serialize_.push_back(
      AXDirtyObject::Create(obj, event_from, event_from_action, event_intents));

  // ensure there is a document lifecycle update scheduled for plugin
  // containers.
  if (obj->GetElement() && DynamicTo<HTMLPlugInElement>(obj->GetElement())) {
    ScheduleImmediateSerialization();
  }
}

void AXObjectCacheImpl::GetUpdatesAndEventsForSerialization(
    std::vector<ui::AXTreeUpdate>& updates,
    std::vector<ui::AXEvent>& events,
    bool& had_end_of_test_event,
    bool& had_load_complete_messages) {
  HashSet<int32_t> already_serialized_ids;

  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kLayoutClean);
  DCHECK(!popup_document_ || popup_document_->Lifecycle().GetState() >=
                                 DocumentLifecycle::kLayoutClean);
  DUMP_WILL_BE_CHECK(HasObjectsPendingSerialization());

  DCHECK_GE(pending_objects_to_serialize_.size(),
            pending_events_to_serialize_.size())
      << "There should be at least as many updates as events, because events "
         "always mark a node dirty.";

  EnsureSerializer();

  if (plugin_tree_source_ && IsDirty()) {
    // If the document is dirty, ensure the plugin serializer is reset.
    CHECK(plugin_serializer_.get());
    plugin_serializer_->Reset();
  }
  ui::AXNodeData::AXNodeDataSize node_data_size;
  for (auto& current_dirty_object : pending_objects_to_serialize_) {
    const AXObject* obj = current_dirty_object->obj;

    // Dirty objects can be added using MarkWebAXObjectDirty(obj) from other
    // parts of the code as well, so we need to ensure the object still
    // exists.
    if (!obj || obj->IsDetached()) {
      continue;
    }

    DCHECK(obj->GetDocument()->GetFrame())
        << "An object in a closed document should have been detached via "
           "Remove(): "
        << obj;

    // Cannot serialize unincluded object.
    // Only included objects are marked dirty, but this can happen if the
    // object becomes unincluded after it was originally marked dirty, in which
    // cas a children changed will also be fired on the included ancestor. The
    // children changed event on the ancestor means that attempting to
    // serialize this unincluded object is not necessary.
    if (!obj->IsIncludedInTree())
      continue;

    DCHECK(obj->AXObjectID());

    if (already_serialized_ids.Contains(obj->AXObjectID()))
      continue;  // No need to serialize, was already present.

    updates.emplace_back();
    ui::AXTreeUpdate& update = updates.back();
    update.event_from = current_dirty_object->event_from;
    update.event_from_action = current_dirty_object->event_from_action;
    update.event_intents = std::move(current_dirty_object->event_intents);

    bool success = ax_tree_serializer_->SerializeChanges(obj, &update);

    DCHECK(success);
    DCHECK_GT(update.nodes.size(), 0U);

    for (auto& node_data : update.nodes) {
      AXID id = node_data.id;
      DCHECK(id);
      // Kept here for convenient debugging:
      // DVLOG(1) << "*** AX Serialize: " << ObjectFromAXID(id)->ToString();
      already_serialized_ids.insert(node_data.id);

      // Now that the bounding box for this node is serialized, we can clear the
      // node from changed_bounds_ids_ to avoid sending it in
      // SerializeLocationChanges() later.
      changed_bounds_ids_.erase(id);
    }

    DCHECK(already_serialized_ids.Contains(obj->AXObjectID()))
        << "Did not serialize original node, so it was probably not included "
           "in its parent's children, and should never have been marked dirty "
           "in the first place: "
        << obj->ToString()
        << "\nParent: " << obj->ParentObjectIncludedInTree()
        << "\nIndex in parent: "
        << obj->ParentObjectIncludedInTree()
               ->CachedChildrenIncludingIgnored()
               .Find(obj);

    // If there's a plugin, force the tree data to be generated in every
    // message so the plugin can merge its own tree data changes.
    AddPluginTreeToUpdate(&update);

    if (RuntimeEnabledFeatures::
            AccessibilitySerializationSizeMetricsEnabled()) {
      update.AccumulateSize(node_data_size);
    }
  }

  if (RuntimeEnabledFeatures::AccessibilitySerializationSizeMetricsEnabled()) {
    LogNodeDataSizeDistribution(node_data_size);
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Accessibility.Performance.AXObjectCacheImpl.Incremental",
        base::saturated_cast<int>(node_data_size.ByteSize()), 1, kSizeGb,
        kBucketCount);
  }

  // Loop over each event and generate an updated event message.
  for (ui::AXEvent& event : pending_events_to_serialize_) {
    if (event.event_type == ax::mojom::blink::Event::kEndOfTest) {
      had_end_of_test_event = true;
      continue;
    }

    if (!base::Contains(already_serialized_ids, event.id)) {
      // Node no longer exists or could not be serialized.
      // Kept here for convenient debugging:
      // DVLOG(1) << "Dropped AXEvent: " << event.event_type << " on "
      //          << ObjectFromAXID(event.id);
      continue;
    }

#if DCHECK_IS_ON()
    AXObject* obj = ObjectFromAXID(event.id);
    DCHECK(obj && !obj->IsDetached())
        << "Detached object for AXEvent: " << event.event_type << " on #"
        << event.id;
#endif

    if (event.event_type == ax::mojom::blink::Event::kLoadComplete) {
      if (had_load_complete_messages)
        continue;  // De-dupe.
      had_load_complete_messages = true;
    }

    events.push_back(event);

    // Kept here for convenient debugging:
    // DVLOG(1) << "AXEvent: " << event.event_type << " on "
    //          << ObjectFromAXID(event.id);
  }

#if DCHECK_IS_ON()
  // Always compute this state.
  UpdatePluginIncludedNodeCount();

  CheckTreeConsistency(*this, *ax_tree_serializer_, plugin_serializer_.get());

  // Provide the expected node count in the last update, so that
  // AXTree::Unserialize() can check for tree consistency on the browser side.
  if (!updates.back().tree_checks) {
    updates.back().tree_checks.emplace();
  }
  updates.back().tree_checks->node_count =
      GetIncludedNodeCount() + GetPluginIncludedNodeCount();
#endif  // DCHECK_IS_ON()
}

#if DCHECK_IS_ON()
void AXObjectCacheImpl::UpdateIncludedNodeCount(const AXObject* obj) {
  if (obj->IsIncludedInTree()) {
    ++included_node_count_;
  } else {
    --included_node_count_;
  }
}

void AXObjectCacheImpl::UpdatePluginIncludedNodeCount() {
  plugin_included_node_count_ = 0;

  // If the serializer is empty, it means we cleared it at some point e.g. when
  // detaching the embed. In those cases, it's correct to skip computing the
  // count from the plugin tree source which has no idea it was detached.
  if (!plugin_serializer_.get() ||
      plugin_serializer_->ClientTreeNodeCount() == 0) {
    return;
  }

  if (plugin_tree_source_ && plugin_tree_source_->GetRoot()) {
    std::stack<const ui::AXNode*> nodes;
    nodes.push(plugin_tree_source_->GetRoot());
    while (!nodes.empty()) {
      const ui::AXNode* child = nodes.top();
      nodes.pop();
      plugin_included_node_count_++;
      for (size_t i = 0; i < plugin_tree_source_->GetChildCount(child); i++) {
        nodes.push(plugin_tree_source_->ChildAt(child, i));
      }
    }
  }
}

bool AXObjectCacheImpl::IsInternalUICheckerOn(const AXObject& obj) const {
  if (internal_ui_checker_on_) {
    return true;
  }
  // Also turn on for nodes that are inside of a UA shadow root, which is
  // used for complex form controls built into the browser.
  return obj.GetNode() && obj.GetNode()->IsInUserAgentShadowRoot();
}
#endif  // DCHECK_IS_ON()

void AXObjectCacheImpl::GetImagesToAnnotate(
    ui::AXTreeUpdate& update,
    std::vector<ui::AXNodeData*>& nodes) {
  for (auto& node : update.nodes) {
    AXObject* src = ObjectFromAXID(node.id);
    if (!src || src->IsDetached() || !src->IsIncludedInTree() ||
        (src->IsIgnored() &&
         !node.HasState(ax::mojom::blink::State::kFocusable))) {
      continue;
    }

    if (src->IsImage()) {
      nodes.push_back(&node);
      // This else clause matches links/documents because we would like to find
      // an image that is in the near-descendant subtree of the link/document,
      // since that image may be semantically representative of that
      // link/document. See FindExactlyOneInnerImageInMaxDepthThree (not in
      // this file), which is used by the caller of this method to find such
      // an image.
    } else if ((src->IsLink() || ui::IsPlatformDocument(node.role)) &&
               node.GetNameFrom() != ax::mojom::blink::NameFrom::kAttribute) {
      nodes.push_back(&node);
    }
  }
}

HeapMojoRemote<blink::mojom::blink::RenderAccessibilityHost>&
AXObjectCacheImpl::GetOrCreateRemoteRenderAccessibilityHost() {
  if (!render_accessibility_host_) {
    GetDocument().GetFrame()->GetBrowserInterfaceBroker().GetInterface(
        render_accessibility_host_.BindNewPipeAndPassReceiver(
            document_->GetTaskRunner(TaskType::kUserInteraction)));
  }
  return render_accessibility_host_;
}

void AXObjectCacheImpl::HandleInitialFocus() {
  PostNotification(document_, ax::mojom::Event::kFocus);
}

void AXObjectCacheImpl::HandleEditableTextContentChanged(Node* node) {
  if (!node)
    return;

  SCOPED_DISALLOW_LIFECYCLE_TRANSITION();

  DeferTreeUpdate(TreeUpdateReason::kEditableTextContentChanged, node);
}

void AXObjectCacheImpl::HandleDeletionOrInsertionInTextField(
    const SelectionInDOMTree& changed_selection,
    bool is_deletion) {
  Position start_pos = changed_selection.ComputeStartPosition();
  Position end_pos = changed_selection.ComputeEndPosition();

#if DCHECK_IS_ON()
  Document& selection_document =
      start_pos.ComputeContainerNode()->GetDocument();
  DCHECK(selection_document.Lifecycle().GetState() >=
         DocumentLifecycle::kAfterPerformLayout)
      << "Unclean document at lifecycle "
      << selection_document.Lifecycle().ToString();
#endif

  // Currently there are scenarios where the start/end are not offset in
  // anchor, if this is the case, we need to compute their offset in the
  // container node since we need this information on the browser side.
  int start_offset = start_pos.ComputeOffsetInContainerNode();
  int end_offset = end_pos.ComputeOffsetInContainerNode();

  AXObject* start_obj = Get(start_pos.ComputeContainerNode());
  AXObject* end_obj = Get(end_pos.ComputeContainerNode());
  if (!start_obj || !end_obj) {
    return;
  }

  AXObject* text_field_obj = start_obj->GetTextFieldAncestor();
  if (!text_field_obj) {
    return;
  }

  auto it = text_operation_in_node_ids_.find(text_field_obj->AXObjectID());
  ax::mojom::blink::Command op = is_deletion
                                     ? ax::mojom::blink::Command::kDelete
                                     : ax::mojom::blink::Command::kInsert;
  if (it != text_operation_in_node_ids_.end()) {
    it->value.push_back(TextChangedOperation(start_offset, end_offset,
                                             start_obj->AXObjectID(),
                                             end_obj->AXObjectID(), op));
  } else {
    WTF::Vector<TextChangedOperation> info{
        TextChangedOperation(start_offset, end_offset, start_obj->AXObjectID(),
                             end_obj->AXObjectID(), op)};
    text_operation_in_node_ids_.Set(text_field_obj->AXObjectID(), info);
  }
}

void AXObjectCacheImpl::HandleEditableTextContentChangedWithCleanLayout(
    Node* node) {
  AXObject* obj = Get(node);
  if (obj) {
    obj = obj->GetTextFieldAncestor();
  }

  PostNotification(obj, ax::mojom::Event::kValueChanged);
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
  DeferTreeUpdate(TreeUpdateReason::kTextMarkerDataAdded, start);
  if (start != end) {
    DeferTreeUpdate(TreeUpdateReason::kTextMarkerDataAdded, end);
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
           .empty()) {
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
           .empty();
  if (has_spelling_or_grammar_markers) {
    if (nodes_with_spelling_or_grammar_markers_.insert(node->GetDomNodeId())
            .is_new_entry) {
      ChildrenChangedWithCleanLayout(node);
    }
  } else {
    const auto& iter =
        nodes_with_spelling_or_grammar_markers_.find(node->GetDomNodeId());
    if (iter != nodes_with_spelling_or_grammar_markers_.end()) {
      nodes_with_spelling_or_grammar_markers_.erase(iter);
      ChildrenChangedWithCleanLayout(node);
    }
  }
}

void AXObjectCacheImpl::HandleValueChanged(Node* node) {
  // Avoid duplicate processing of rapid value changes, e.g. on a slider being
  // dragged, or a progress meter.
  AXObject* ax_object = Get(node);
  if (ax_object) {
    if (last_value_change_node_ == ax_object->AXObjectID())
      return;
    last_value_change_node_ = ax_object->AXObjectID();
  }

  PostNotification(node, ax::mojom::Event::kValueChanged);

  // If it's a slider, invalidate the thumb's bounding box.
  if (ax_object && ax_object->RoleValue() == ax::mojom::blink::Role::kSlider &&
      !ax_object->NeedsToUpdateChildren() &&
      ax_object->ChildCountIncludingIgnored() == 1) {
    InvalidateBoundingBox(ax_object->ChildAtIncludingIgnored(0)->AXObjectID());
  }
}

void AXObjectCacheImpl::HandleUpdateActiveMenuOption(Node* menu_list) {
  DeferTreeUpdate(TreeUpdateReason::kUpdateActiveMenuOption, menu_list);
}

void AXObjectCacheImpl::HandleUpdateMenuListPopupWithCleanLayout(
    Node* menu_list,
    bool did_show) {
  AXObject* ax_menu_list = Get(menu_list);
  if (!ax_menu_list) {
    return;
  }
  AXObject* ax_popup = ax_menu_list->FirstChildIncludingIgnored();
  if (!ax_popup ||
      ax_popup->RoleValue() != ax::mojom::blink::Role::kMenuListPopup) {
    last_selected_from_active_descendant_ = nullptr;
    return;
  }
  AXObject* active_descendant = ax_popup->ActiveDescendant();
  if (did_show) {
    // On first appearance, mark everything dirty, because the hidden state
    // will change on most descendants.
    MarkAXSubtreeDirtyWithCleanLayout(ax_menu_list);
  } else {
    // Mark the previously selected item dirty so that its updated selection
    // state is reserialized.
    if (last_selected_from_active_descendant_) {
      MarkElementDirtyWithCleanLayout(last_selected_from_active_descendant_);
    }
  }
  if (active_descendant) {
    MarkAXObjectDirtyWithCleanLayout(active_descendant);
    PostNotification(ax_popup,
                     ax::mojom::blink::Event::kActiveDescendantChanged);
    last_selected_from_active_descendant_ = active_descendant->GetNode();
  }
}

void AXObjectCacheImpl::DidShowMenuListPopup(Node* menu_list) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION();
  CHECK(menu_list);
  DeferTreeUpdate(TreeUpdateReason::kDidShowMenuListPopup, menu_list);
}

void AXObjectCacheImpl::DidHideMenuListPopup(Node* menu_list) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION();
  CHECK(menu_list);
  current_menu_list_axid_ = 0;
  options_bounds_ = {};
  MarkAXSubtreeDirty(Get(menu_list));
}

void AXObjectCacheImpl::HandleLoadStart(Document* document) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION();
  // Popups do not need to fire load start or load complete , because ATs do not
  // regard popups as documents -- that is an implementation detail of the
  // browser. The AT regards popups as part of a widget, and a load start or
  // load complete event would only potentially confuse the AT.
  if (!IsPopup(*document) && !IsInitialEmptyDocument(*document)) {
    DeferTreeUpdate(TreeUpdateReason::kPostNotificationFromHandleLoadStart,
                    document, ax::mojom::blink::Event::kLoadStart);
  }
}

void AXObjectCacheImpl::HandleLoadComplete(Document* document) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION();

  // TODO(accessibility) Change this to a DCHECK, but that would fail right now
  // in navigation API tests.
  if (!document->IsLoadCompleted())
    return;

  // Popups do not need to fire load start or load complete , because ATs do not
  // regard popups as documents -- that is an implementation detail of the
  // browser. The AT regards popups as part of a widget, and a load start or
  // load complete event would only potentially confuse the AT.
  if (!IsPopup(*document) && !IsInitialEmptyDocument(*document)) {
    DeferTreeUpdate(TreeUpdateReason::kPostNotificationFromHandleLoadComplete,
                    document, ax::mojom::blink::Event::kLoadComplete);
  }
}

void AXObjectCacheImpl::HandleScrolledToAnchor(const Node* anchor_node) {
  if (!anchor_node)
    return;

  DeferTreeUpdate(TreeUpdateReason::kPostNotificationFromHandleScrolledToAnchor,
                  const_cast<Node*>(anchor_node),
                  ax::mojom::blink::Event::kScrolledToAnchor);
}

void AXObjectCacheImpl::InvalidateBoundingBox(
    const LayoutObject* layout_object) {
  if (AXObject* obj = Get(const_cast<LayoutObject*>(layout_object))) {
    InvalidateBoundingBox(obj->AXObjectID());
  }
}

void AXObjectCacheImpl::InvalidateBoundingBox(const AXID& id) {
  changed_bounds_ids_.insert(id);
}

void AXObjectCacheImpl::SetCachedBoundingBox(AXID id,
                                             const ui::AXRelativeBounds& bounds,
                                             const int scroll_x,
                                             const int scroll_y) {
  // When a bounding box of a node is serialized, we store the last value for it
  // in cached_bounding_boxes_, to help with comparing if it really changed
  // or not when sending another serialization later.
  cached_bounding_boxes_.Set(id,
                             CachedLocationChange(bounds, scroll_x, scroll_y));
}

void AXObjectCacheImpl::HandleScrollPositionChanged(
    LayoutObject* layout_object) {
  if (layout_object->GetDocument() != document_) {
    return;
  }

  SCOPED_DISALLOW_LIFECYCLE_TRANSITION();

  // When the scroll position position changes, mark the bounding boxes of all
  // fixed/sticky positioned objects for reserialization, because they are
  // relative to the top left of the document.
  InvalidateBoundingBoxForFixedOrStickyPosition();

  Node* node = GetClosestNodeForLayoutObject(layout_object);
  if (node) {
    InvalidateBoundingBox(node->GetDomNodeId());
  }
}

const AtomicString& AXObjectCacheImpl::ComputedRoleForNode(Node* node) {
  // Accessibility tree must be updated before getting an object.
  // Disallow a scope transition on the main document (which needs to already be
  // updated to its correct lifecycle state at this point, or else there would
  // be an illegal re-entrance to its lifecycle), but not for any popup document
  // that is open. That's because popup documents update their lifecycle async
  // from the main document, and hence any forced update to the popup document's
  // lifecycle here is not re-entrance but rather a "forced" lifecycle update.
  DocumentLifecycle::DisallowTransitionScope scoped(document_->Lifecycle());
  CommitAXUpdates(GetDocument(), /*force*/ true);
  ScopedFreezeAXCache scoped_freeze_cache(*this);
  AXObject* obj = Get(node);
  return AXObject::AriaRoleName(obj ? obj->ComputeFinalRoleForSerialization()
                                    : ax::mojom::blink::Role::kUnknown);
}

String AXObjectCacheImpl::ComputedNameForNode(Node* node) {
  // Accessibility tree must be updated before getting an object. See comment in
  // ComputedRoleForNode() for explanation of disallow transition scope usage.
  DocumentLifecycle::DisallowTransitionScope scoped(document_->Lifecycle());
  CommitAXUpdates(GetDocument(), /*force*/ true);
  ScopedFreezeAXCache scoped_freeze_cache(*this);
  AXObject* obj = Get(node);
  return obj ? obj->ComputedName() : "";
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
                                              const PhysicalRect& rect) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION();

  AXObject* obj = Get(element);
  if (!obj)
    return;

  AXObject* ax_canvas = Get(canvas);
  if (!ax_canvas)
    return;

  obj->SetElementRect(rect, ax_canvas);
}

void AXObjectCacheImpl::Trace(Visitor* visitor) const {
  visitor->Trace(agents_);
  visitor->Trace(document_);
  visitor->Trace(popup_document_);
  visitor->Trace(last_selected_from_active_descendant_);
  visitor->Trace(layout_object_mapping_);
  visitor->Trace(inline_text_box_object_mapping_);
  visitor->Trace(active_aria_modal_dialog_);

  visitor->Trace(objects_);
  visitor->Trace(next_on_line_map_);
  visitor->Trace(processed_blocks_);
  visitor->Trace(previous_on_line_map_);

  visitor->Trace(tree_update_callback_queue_main_);
  visitor->Trace(tree_update_callback_queue_popup_);
  visitor->Trace(render_accessibility_host_);
  visitor->Trace(ax_tree_source_);
  visitor->Trace(pending_objects_to_serialize_);
  visitor->Trace(node_to_parse_before_more_tree_updates_);
  visitor->Trace(weak_factory_for_serialization_pipeline_);
  visitor->Trace(weak_factory_for_loc_updates_pipeline_);

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

WebAXAutofillSuggestionAvailability
AXObjectCacheImpl::GetAutofillSuggestionAvailability(AXID id) const {
  auto iter = autofill_suggestion_availability_map_.find(id);
  if (iter == autofill_suggestion_availability_map_.end()) {
    return WebAXAutofillSuggestionAvailability::kNoSuggestions;
  }
  return iter->value;
}

void AXObjectCacheImpl::SetAutofillSuggestionAvailability(
    AXID id,
    WebAXAutofillSuggestionAvailability suggestion_availability) {
  WebAXAutofillSuggestionAvailability previous_suggestion_availability =
      GetAutofillSuggestionAvailability(id);
  if (suggestion_availability != previous_suggestion_availability) {
    autofill_suggestion_availability_map_.Set(id, suggestion_availability);
    MarkAXObjectDirty(ObjectFromAXID(id));
  }
}

void AXObjectCacheImpl::AddPluginTreeToUpdate(ui::AXTreeUpdate* update) {
  if (!plugin_tree_source_) {
    return;
  }

  // Conceptually, a plugin tree "stitches" itself into an existing Blink
  // accessibility node. For example, the node could be an <embed>. The plugin
  // tree itself contains a root who's parent is the target of the stitching
  // (e.g. the <embed> in the Blink accessibility tree). The plugin tree manages
  // its own tree of nodes and the below logic handles how that gets integrated
  // into Blink accessibility.

  CHECK(plugin_serializer_.get());

  // Search for the Blink accessibility node onto which we want to stitch the
  // plugin tree.
  for (ui::AXNodeData& node : update->nodes) {
    if (node.role == ax::mojom::Role::kEmbeddedObject) {
      // The embed node should already exist in the blink tree source's client
      // tree.
      CHECK(ax_tree_serializer_->IsInClientTree(
          ax_tree_source_->GetFromId(node.id)));

      // The plugin tree contains its own tree source, serializer pair. It isn't
      // using Blink's source, serializer pair because its backing template tree
      // source type is a pure AXNodeData.
      const ui::AXNode* root = plugin_tree_source_->GetRoot();
      if (!root) {
        // The tree may not yet be ready.
        continue;
      }
      node.child_ids.push_back(root->id());

      // Serialize changes and integrate them into Blink accessibility's tree
      // updates.
      ui::AXTreeUpdate plugin_update;
      plugin_serializer_->SerializeChanges(root, &plugin_update);

      update->nodes.reserve(update->nodes.size() + plugin_update.nodes.size());
      base::ranges::move(plugin_update.nodes,
                         std::back_inserter(update->nodes));

      if (plugin_tree_source_->GetTreeData(&update->tree_data)) {
        update->has_tree_data = true;
      }
      break;
    }
  }
}

ui::AXTreeSource<const ui::AXNode*, ui::AXTreeData*, ui::AXNodeData>*
AXObjectCacheImpl::GetPluginTreeSource() {
  return plugin_tree_source_.get();
}

void AXObjectCacheImpl::SetPluginTreeSource(
    ui::AXTreeSource<const ui::AXNode*, ui::AXTreeData*, ui::AXNodeData>*
        source) {
  if (plugin_tree_source_.get() == source) {
    return;
  }

  plugin_tree_source_ = source;
  plugin_serializer_ =
      source ? std::make_unique<PluginAXTreeSerializer>(source) : nullptr;
}

ui::AXTreeSerializer<const ui::AXNode*,
                     std::vector<const ui::AXNode*>,
                     ui::AXTreeUpdate*,
                     ui::AXTreeData*,
                     ui::AXNodeData>*
AXObjectCacheImpl::GetPluginTreeSerializer() {
  return plugin_serializer_.get();
}

void AXObjectCacheImpl::ResetPluginTreeSerializer() {
  if (plugin_serializer_.get()) {
    plugin_serializer_->Reset();
  }
}

void AXObjectCacheImpl::MarkPluginDescendantDirty(ui::AXNodeID node_id) {
  if (plugin_serializer_.get()) {
    plugin_serializer_->MarkSubtreeDirty(node_id);
  }
}

void AXObjectCacheImpl::ComputeNodesOnLine(const LayoutObject* layout_object) {
  // The following computation is expensive.
  //
  // This function works as follows:
  // 1. If a layout object associated with an AXNodeObject has its data already
  // computed, we finish early;
  // 2. If the associated Layout Block that the inline element is contained is
  // already processed, we finish early. Note that 2 must come after 1, since
  // retrieving the block is not so cheap;
  // 3. For each line of this layout block flow, we connect and store the layout
  // objects that are part of this line. They are later used in
  // Next|PreviousOnLine.
  //
  // The main advantage of this approach is to be able to, in a single pass,
  // compute the next and previous objects in a single line.
  if (!layout_object) {
    return;
  }
  if (!layout_object->IsInline() ||
      !layout_object->IsInLayoutNGInlineFormattingContext()) {
    return;
  }
  if (CachedNextOnLine(layout_object)) {
    return;
  }
  const LayoutBlockFlow* block_flow = layout_object->FragmentItemsContainer();
  if (!block_flow) {
    return;
  }
  if (!processed_blocks_.insert(block_flow).is_new_entry) {
    return;
  }
  InlineCursor cursor(*block_flow);
  if (!cursor) {
    // Cursor may be null if all objects of this cursor are collapsed.
    return;
  }

  // Important! The next call to MoveToNextInlineLeaf() below fails if we are
  // not inside a line.
  cursor.MoveToFirstLine();
  if (!cursor) {
    return;
  }

  do {
    InlineCursor line_cursor = cursor;

    // Moves to first LayoutObject that a11y cares about.
    line_cursor.MoveToNextInlineLeaf();
    while (line_cursor) {
      auto* line_object = line_cursor.CurrentMutableLayoutObject();
      line_cursor.MoveToNextInlineLeafOnLine();

      if (line_object) {
        auto* next_line_object =
            line_cursor ? line_cursor.CurrentMutableLayoutObject() : nullptr;
        if (next_line_object) {
          next_on_line_map_.insert(line_object, next_line_object);
          previous_on_line_map_.insert(next_line_object, line_object);
        } else {
          // Reached the end of the line. Check if it contains a trailing white
          // space that was not visited by the inline cursor because it was
          // collapsed.
          // The white space at the end of the line is important for a11y
          // because if it is not part of the line text, a screen reader may not
          // know that it has reached a previous line when going back to the
          // previous line.
          ConnectToTrailingWhitespaceOnLine(*line_object, *block_flow);
        }
      }
    }
    cursor.MoveToNextLine();
  } while (cursor);
}

void AXObjectCacheImpl::ConnectToTrailingWhitespaceOnLine(
    const LayoutObject& line_object,
    const LayoutBlockFlow& block_flow) {
  LayoutObject* trailing_whitespace_object =
      PreviousLayoutObjectTextOnLine(line_object, block_flow);
  if (!trailing_whitespace_object) {
    return;
  }
  if (!IsAllCollapsedWhiteSpace(*trailing_whitespace_object)) {
    return;
  }
  if (AXObject* obj = Get(trailing_whitespace_object);
      obj && obj->IsIncludedInTree()) {
    if (auto* previous = PreviousLayoutObjectTextOnLine(
            *trailing_whitespace_object, block_flow)) {
      // `trailing_whitespace_object` has a LayoutObject that is in
      // the same line, connect them here.
      // Note: we use `Set()` here to override if a value exists, where in
      // `ComputeNodesOnLine()` we use `insert()`, which does not. This is
      // necessary in case a object in the line pointed to something else, where
      // now it needs to point to the trailing whitespace.
      next_on_line_map_.Set(previous, trailing_whitespace_object);
      previous_on_line_map_.Set(trailing_whitespace_object, previous);
    }
  }
}

const LayoutObject* AXObjectCacheImpl::CachedNextOnLine(
    const LayoutObject* layout_object) {
  auto it = next_on_line_map_.find(layout_object);
  if (it != next_on_line_map_.end()) {
    return it->value;
  }
  return nullptr;
}

const LayoutObject* AXObjectCacheImpl::CachedPreviousOnLine(
    const LayoutObject* layout_object) {
  auto it = previous_on_line_map_.find(layout_object);
  if (it != previous_on_line_map_.end()) {
    return it->value;
  }
  return nullptr;
}

void AXObjectCacheImpl::ClearCachedNodesOnLine() {
  next_on_line_map_.clear();
  previous_on_line_map_.clear();
  processed_blocks_.clear();
}

std::ostream& operator<<(std::ostream& stream, const AXObjectCacheImpl& cache) {
  return stream << "AXObjectCache: " << cache.lifecycle().ToString();
}

}  // namespace blink
