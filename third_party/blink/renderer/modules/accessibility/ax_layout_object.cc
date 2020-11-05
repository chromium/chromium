/*
* Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/accessibility/ax_layout_object.h"

#include <algorithm>
#include <memory>
#include <string>

#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/event_util.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/forms/labels_node_list.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_table_caption_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_col_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_file_upload_control.h"
#include "third_party/blink/renderer/core/layout/layout_html_canvas.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_list_marker.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/layout_table_section.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"
#include "third_party/blink/renderer/core/loader/progress_tracker.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"
#include "third_party/blink/renderer/core/svg/svg_g_element.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_image_map_link.h"
#include "third_party/blink/renderer/modules/accessibility/ax_inline_text_box.h"
#include "third_party/blink/renderer/modules/accessibility/ax_mock_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_svg_root.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

AXLayoutObject::AXLayoutObject(LayoutObject* layout_object,
                               AXObjectCacheImpl& ax_object_cache)
    : AXNodeObject(layout_object->GetNode(), ax_object_cache),
      layout_object_(layout_object) {
// TODO(aleventhal) Get correct current state of autofill.
#if DCHECK_IS_ON()
  layout_object_->SetHasAXObject(true);
#endif
}

AXLayoutObject::~AXLayoutObject() {
  DCHECK(IsDetached());
}

LayoutBoxModelObject* AXLayoutObject::GetLayoutBoxModelObject() const {
  if (!layout_object_ || !layout_object_->IsBoxModelObject())
    return nullptr;
  return ToLayoutBoxModelObject(layout_object_);
}

bool IsProgrammaticallyScrollable(LayoutBox* box) {
  if (!box->IsScrollContainer())
    return false;

  // Return true if the content is larger than the available space.
  return box->PixelSnappedScrollWidth() != box->PixelSnappedClientWidth() ||
         box->PixelSnappedScrollHeight() != box->PixelSnappedClientHeight();
}

ScrollableArea* AXLayoutObject::GetScrollableAreaIfScrollable() const {
  if (IsWebArea())
    return DocumentFrameView()->LayoutViewport();

  if (!layout_object_ || !layout_object_->IsBox())
    return nullptr;

  LayoutBox* box = ToLayoutBox(layout_object_);

  // This should possibly use box->CanBeScrolledAndHasScrollableArea() as it
  // used to; however, accessibility must consider any kind of non-visible
  // overflow as programmatically scrollable. Unfortunately
  // LayoutBox::CanBeScrolledAndHasScrollableArea() method calls
  // LayoutBox::CanBeProgramaticallyScrolled() which does not consider
  // visibility:hidden content to be programmatically scrollable, although it
  // certainly is, and can even be scrolled by selecting and using shift+arrow
  // keys. It should be noticed that the new code used here reduces the overall
  // amount of work as well.
  // It is not sufficient to expose it only in the anoymous child, because that
  // child is truncated in platform accessibility trees, which present the
  // textfield as a leaf.
  ScrollableArea* scrollable_area = box->GetScrollableArea();
  if (scrollable_area && IsProgrammaticallyScrollable(box))
    return scrollable_area;

  return nullptr;
}

static bool IsImageOrAltText(LayoutBoxModelObject* box, Node* node) {
  if (box && box->IsImage())
    return true;
  if (IsA<HTMLImageElement>(node))
    return true;
  auto* html_input_element = DynamicTo<HTMLInputElement>(node);
  if (html_input_element && html_input_element->HasFallbackContent())
    return true;
  return false;
}

ax::mojom::blink::Role AXLayoutObject::RoleFromLayoutObject(
    ax::mojom::blink::Role dom_role) const {
  // Markup did not provide a specific role, so attempt to determine one
  // from the computed style.
  Node* node = layout_object_->GetNode();
  LayoutBoxModelObject* css_box = GetLayoutBoxModelObject();

  if ((css_box && css_box->IsListItem()) || IsA<HTMLLIElement>(node))
    return ax::mojom::blink::Role::kListItem;
  if (layout_object_->IsListMarkerIncludingAll())
    return ax::mojom::blink::Role::kListMarker;
  if (layout_object_->IsBR())
    return ax::mojom::blink::Role::kLineBreak;
  if (layout_object_->IsText())
    return ax::mojom::blink::Role::kStaticText;

  // Chrome exposes both table markup and table CSS as a tables, letting
  // the screen reader determine what to do for CSS tables. If this line
  // is reached, then it is not an HTML table, and therefore will only be
  // considered a data table if ARIA markup indicates it is a table.
  if (layout_object_->IsTable() && node)
    return ax::mojom::blink::Role::kLayoutTable;
  if (layout_object_->IsTableSection())
    return DetermineTableSectionRole();
  if (layout_object_->IsTableRow() && node)
    return DetermineTableRowRole();
  if (layout_object_->IsTableCell() && node)
    return DetermineTableCellRole();

  if (css_box && IsImageOrAltText(css_box, node)) {
    if (node && node->IsLink())
      return ax::mojom::blink::Role::kImageMap;
    if (IsA<HTMLInputElement>(node))
      return ButtonRoleType();
    if (IsSVGImage())
      return ax::mojom::blink::Role::kSvgRoot;

    return ax::mojom::blink::Role::kImage;
  }

  if (IsA<HTMLCanvasElement>(node))
    return ax::mojom::blink::Role::kCanvas;

  if (IsA<LayoutView>(css_box))
    return ax::mojom::blink::Role::kRootWebArea;

  if (layout_object_->IsSVGImage())
    return ax::mojom::blink::Role::kImage;
  if (layout_object_->IsSVGRoot())
    return ax::mojom::blink::Role::kSvgRoot;

  if (layout_object_->IsHR())
    return ax::mojom::blink::Role::kSplitter;

  // TODO(accessibility): refactor this method to take no argument and instead
  // default to returning kUnknownRole, the caller can then check for this and
  // return a different value if they prefer.
  return dom_role;
}

ax::mojom::blink::Role AXLayoutObject::DetermineAccessibilityRole() {
  if (!layout_object_)
    return ax::mojom::blink::Role::kUnknown;
  if (GetCSSAltText(GetNode())) {
    const ComputedStyle* style = GetNode()->GetComputedStyle();
    ContentData* content_data = style->GetContentData();

    // We just check the first item of the content list to determine the
    // appropriate role, should only ever be image or text.
    ax::mojom::blink::Role role = ax::mojom::blink::Role::kStaticText;
    if (content_data->IsImage())
      role = ax::mojom::blink::Role::kImage;

    return role;
  }
  native_role_ = NativeRoleIgnoringAria();

  if ((aria_role_ = DetermineAriaRoleAttribute()) !=
      ax::mojom::blink::Role::kUnknown) {
    return aria_role_;
  }

  // Anything that needs to still be exposed but doesn't have a more specific
  // role should be considered a generic container. Examples are
  // layout blocks with no node, in-page link targets, and plain elements
  // such as a <span> with ARIA markup.
  return native_role_ == ax::mojom::blink::Role::kUnknown
             ? ax::mojom::blink::Role::kGenericContainer
             : native_role_;
}

Node* AXLayoutObject::GetNodeOrContainingBlockNode() const {
  if (IsDetached())
    return nullptr;

  // For legacy layout, or editable list marker when disabling EditingNG.
  if (layout_object_->IsListMarker()) {
    // Return the originating list item node.
    return layout_object_->GetNode()->parentNode();
  }

  // For LayoutNG list marker.
  // Note: When EditingNG is disabled, editable list items are laid out legacy
  // layout even if LayoutNG enabled.
  if (auto* list_marker = ListMarker::Get(layout_object_)) {
    // Return the originating list item node.
    return list_marker->ListItem(*layout_object_)->GetNode();
  }

  if (layout_object_->IsAnonymous()) {
    if (LayoutBlock* layout_block =
            LayoutObject::FindNonAnonymousContainingBlock(layout_object_)) {
      return layout_block->GetNode();
    }
    return nullptr;
  }

  return GetNode();
}

void AXLayoutObject::Init() {
  AXNodeObject::Init();
}

void AXLayoutObject::Detach() {
  AXNodeObject::Detach();

  DetachRemoteSVGRoot();

#if DCHECK_IS_ON()
  if (layout_object_)
    layout_object_->SetHasAXObject(false);
#endif
  layout_object_ = nullptr;
}

bool AXLayoutObject::IsDetached() const {
  return !layout_object_ || AXObject::IsDetached();
}

bool AXLayoutObject::IsAXLayoutObject() const {
  return true;
}

//
// Check object role or purpose.
//

static bool IsLinkable(const AXObject& object) {
  if (!object.GetLayoutObject())
    return false;

  // See https://wiki.mozilla.org/Accessibility/AT-Windows-API for the elements
  // Mozilla considers linkable.
  return object.IsLink() || object.IsImage() ||
         object.GetLayoutObject()->IsText();
}

// Requires layoutObject to be present because it relies on style
// user-modify. Don't move this logic to AXNodeObject.
bool AXLayoutObject::IsEditable() const {
  if (IsDetached())
    return false;

  const Node* node = GetNodeOrContainingBlockNode();
  if (!node)
    return false;

  const auto* elem = DynamicTo<Element>(node);
  if (!elem)
    elem = FlatTreeTraversal::ParentElement(*node);
  if (GetLayoutObject()->IsTextControlIncludingNG())
    return true;

  // Contrary to Firefox, we mark editable all auto-generated content, such as
  // list bullets and soft line breaks, that are contained within an editable
  // container.
  if (HasEditableStyle(*node))
    return true;

  if (IsWebArea()) {
    Document& document = GetLayoutObject()->GetDocument();
    HTMLElement* body = document.body();
    if (body && HasEditableStyle(*body)) {
      AXObject* ax_body = AXObjectCache().GetOrCreate(body);
      return ax_body && ax_body != ax_body->AriaHiddenRoot();
    }

    return HasEditableStyle(document);
  }

  return AXNodeObject::IsEditable();
}

// Requires layoutObject to be present because it relies on style
// user-modify. Don't move this logic to AXNodeObject.
// Returns true for a contenteditable or any descendant of it.
bool AXLayoutObject::IsRichlyEditable() const {
  if (IsDetached())
    return false;

  const Node* node = GetNodeOrContainingBlockNode();
  if (!node)
    return false;

  const Element* elem = DynamicTo<Element>(node);
  if (!elem)
    elem = FlatTreeTraversal::ParentElement(*node);

  // Contrary to Firefox, we mark richly editable all auto-generated content,
  // such as list bullets and soft line breaks, that are contained within a
  // richly editable container.
  if (HasRichlyEditableStyle(*node))
    return true;

  if (IsWebArea()) {
    Document& document = layout_object_->GetDocument();
    HTMLElement* body = document.body();
    if (body && HasRichlyEditableStyle(*body)) {
      AXObject* ax_body = AXObjectCache().GetOrCreate(body);
      return ax_body && ax_body != ax_body->AriaHiddenRoot();
    }

    return HasRichlyEditableStyle(document);
  }

  return AXNodeObject::IsRichlyEditable();
}

bool AXLayoutObject::IsLineBreakingObject() const {
  if (IsDetached())
    return AXNodeObject::IsLineBreakingObject();

  const LayoutObject* layout_object = GetLayoutObject();
  if (layout_object->IsBR() || layout_object->IsLayoutBlock() ||
      layout_object->IsAnonymousBlock() ||
      (layout_object->IsLayoutBlockFlow() &&
       layout_object->StyleRef().IsDisplayBlockContainer())) {
    return true;
  }

  return AXNodeObject::IsLineBreakingObject();
}

bool AXLayoutObject::IsLinked() const {
  if (!IsLinkable(*this))
    return false;

  if (auto* anchor = DynamicTo<HTMLAnchorElement>(AnchorElement()))
    return !anchor->Href().IsEmpty();
  return false;
}

bool AXLayoutObject::IsOffScreen() const {
  DCHECK(layout_object_);
  IntRect content_rect =
      PixelSnappedIntRect(layout_object_->VisualRectInDocument());
  LocalFrameView* view = layout_object_->GetFrame()->View();
  IntRect view_rect(IntPoint(), view->Size());
  view_rect.Intersect(content_rect);
  return view_rect.IsEmpty();
}

bool AXLayoutObject::IsVisited() const {
  // FIXME: Is it a privacy violation to expose visited information to
  // accessibility APIs?
  return layout_object_->Style()->IsLink() &&
         layout_object_->Style()->InsideLink() ==
             EInsideLink::kInsideVisitedLink;
}

//
// Check object state.
//

bool AXLayoutObject::IsFocused() const {
  if (!GetDocument())
    return false;

  // A web area is represented by the Document node in the DOM tree, which isn't
  // focusable.  Check instead if the frame's selection controller is focused.
  if (IsWebArea() &&
      GetDocument()->GetFrame()->Selection().FrameIsFocusedAndActive()) {
    return true;
  }

  Element* focused_element = GetDocument()->FocusedElement();
  return focused_element && focused_element == GetElement();
}

// aria-grabbed is deprecated in WAI-ARIA 1.1.
AccessibilityGrabbedState AXLayoutObject::IsGrabbed() const {
  if (!SupportsARIADragging())
    return kGrabbedStateUndefined;

  const AtomicString& grabbed = GetAttribute(html_names::kAriaGrabbedAttr);
  return EqualIgnoringASCIICase(grabbed, "true") ? kGrabbedStateTrue
                                                 : kGrabbedStateFalse;
}

AccessibilitySelectedState AXLayoutObject::IsSelected() const {
  if (!GetLayoutObject() || !GetNode() || !IsSubWidget())
    return kSelectedStateUndefined;

  // The aria-selected attribute overrides automatic behaviors.
  bool is_selected;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kSelected, is_selected))
    return is_selected ? kSelectedStateTrue : kSelectedStateFalse;

  // The selection should only follow the focus when the aria-selected attribute
  // is marked as required or implied for this element in the ARIA specs.
  // If this object can't follow the focus, then we can't say that it's selected
  // nor that it's not.
  if (!SelectionShouldFollowFocus())
    return kSelectedStateUndefined;

  // Selection follows focus, but ONLY in single selection containers, and only
  // if aria-selected was not present to override.
  return IsSelectedFromFocus() ? kSelectedStateTrue : kSelectedStateFalse;
}

// In single selection containers, selection follows focus unless aria_selected
// is set to false. This is only valid for a subset of elements.
bool AXLayoutObject::IsSelectedFromFocus() const {
  if (!SelectionShouldFollowFocus())
    return false;

  // A tab item can also be selected if it is associated to a focused tabpanel
  // via the aria-labelledby attribute.
  if (IsTabItem() && IsTabItemSelected())
    return kSelectedStateTrue;

  // If not a single selection container, selection does not follow focus.
  AXObject* container = ContainerWidget();
  if (!container || container->IsMultiSelectable())
    return false;

  // If this object is not accessibility focused, then it is not selected from
  // focus.
  AXObject* focused_object = AXObjectCache().FocusedObject();
  if (focused_object != this &&
      (!focused_object || focused_object->ActiveDescendant() != this))
    return false;

  // In single selection container and accessibility focused => true if
  // aria-selected wasn't used as an override.
  bool is_selected;
  return !HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kSelected,
                                        is_selected);
}

// Returns true if the object is marked user-select:none
bool AXLayoutObject::IsNotUserSelectable() const {
  if (!GetLayoutObject())
    return false;

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style)
    return false;

  return (style->UserSelect() == EUserSelect::kNone);
}

// Returns true if the node's aria-selected attribute should be set to true
// when the node is focused. This is true for only a subset of roles.
bool AXLayoutObject::SelectionShouldFollowFocus() const {
  switch (RoleValue()) {
    case ax::mojom::blink::Role::kListBoxOption:
    case ax::mojom::blink::Role::kMenuListOption:
    case ax::mojom::blink::Role::kTab:
      return true;
    default:
      break;
  }
  return false;
}

//
// Whether objects are ignored, i.e. not included in the tree.
//

AXObjectInclusion AXLayoutObject::DefaultObjectInclusion(
    IgnoredReasons* ignored_reasons) const {
  if (!layout_object_) {
    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXNotRendered));
    return kIgnoreObject;
  }

  if (layout_object_->Style()->Visibility() != EVisibility::kVisible) {
    // aria-hidden is meant to override visibility as the determinant in AX
    // hierarchy inclusion.
    if (AOMPropertyOrARIAAttributeIsFalse(AOMBooleanProperty::kHidden))
      return kDefaultBehavior;

    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXNotVisible));
    return kIgnoreObject;
  }

  return AXObject::DefaultObjectInclusion(ignored_reasons);
}

static bool HasLineBox(const LayoutBlockFlow& block_flow) {
  if (!block_flow.IsLayoutNGMixin())
    return block_flow.FirstLineBox();
  if (block_flow.HasNGInlineNodeData())
    return !block_flow.GetNGInlineNodeData()->IsEmptyInline();
  // TODO(layout-dev): We should call this function after layout completion.
  return false;
}

// Is this the anonymous placeholder for a text control?
bool AXLayoutObject::IsPlaceholder() const {
  AXObject* parent_object = ParentObject();
  if (!parent_object)
    return false;

  LayoutObject* parent_layout_object = parent_object->GetLayoutObject();
  if (!parent_layout_object ||
      !parent_layout_object->IsTextControlIncludingNG())
    return false;

  const auto* text_control_element =
      To<TextControlElement>(parent_layout_object->GetNode());
  HTMLElement* placeholder_element = text_control_element->PlaceholderElement();

  return GetElement() == static_cast<Element*>(placeholder_element);
}

bool AXLayoutObject::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignored_reasons) const {
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif

  // All nodes must have an unignored parent within their tree under
  // kRootWebArea, so force kRootWebArea to always be unignored.
  if (role_ == ax::mojom::blink::Role::kRootWebArea)
    return false;

  if (IsA<HTMLHtmlElement>(GetNode()))
    return true;

  if (!layout_object_) {
    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXNotRendered));
    return true;
  }

  // Ignore continuations, since those are essentially duplicate copies
  // of inline nodes with blocks inside.
  if (layout_object_->IsElementContinuation()) {
    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXUninteresting));
    return true;
  }

  // Check first if any of the common reasons cause this element to be ignored.
  AXObjectInclusion default_inclusion = DefaultObjectInclusion(ignored_reasons);
  if (default_inclusion == kIncludeObject)
    return false;
  if (default_inclusion == kIgnoreObject)
    return true;

  AXObjectInclusion semantic_inclusion =
      ShouldIncludeBasedOnSemantics(ignored_reasons);
  if (semantic_inclusion == kIncludeObject)
    return false;
  if (semantic_inclusion == kIgnoreObject)
    return true;

  if (layout_object_->IsAnonymousBlock() && !IsEditable()) {
    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXUninteresting));
    return true;
  }

  // A LayoutEmbeddedContent is an iframe element or embedded object element or
  // something like that. We don't want to ignore those.
  if (layout_object_->IsLayoutEmbeddedContent())
    return false;

  // Make sure renderers with layers stay in the tree.
  if (GetLayoutObject() && GetLayoutObject()->HasLayer() && GetNode() &&
      GetNode()->hasChildren()) {
    if (IsPlaceholder()) {
      // Placeholder is already exposed via AX attributes, do not expose as
      // child of text input. Therefore, if there is a child of a text input,
      // it will contain the value.
      if (ignored_reasons)
        ignored_reasons->push_back(IgnoredReason(kAXPresentational));
      return true;
    }

    return false;
  }

  if (IsCanvas()) {
    if (CanvasHasFallbackContent())
      return false;

    const auto* canvas = ToLayoutHTMLCanvasOrNull(GetLayoutObject());
    if (canvas &&
        (canvas->Size().Height() <= 1 || canvas->Size().Width() <= 1)) {
      if (ignored_reasons)
        ignored_reasons->push_back(IgnoredReason(kAXProbablyPresentational));
      return true;
    }

    // Otherwise fall through; use presence of help text, title, or description
    // to decide.
  }

  if (layout_object_->IsBR())
    return false;

  if (layout_object_->IsText()) {
    if (CanIgnoreTextAsEmpty()) {
      if (ignored_reasons)
        ignored_reasons->push_back(IgnoredReason(kAXEmptyText));
      return true;
    }
    return false;
  }

  // FIXME(aboxhall): may need to move?
  base::Optional<String> alt_text = GetCSSAltText(GetNode());
  if (alt_text)
    return alt_text->IsEmpty();

  if (IsWebArea() || layout_object_->IsListMarkerIncludingAll())
    return false;

  // Positioned elements and scrollable containers are important for
  // determining bounding boxes.
  if (IsScrollableContainer())
    return false;
  if (layout_object_->IsPositioned())
    return false;

  // Inner editor element of editable area with empty text provides bounds
  // used to compute the character extent for index 0. This is the same as
  // what the caret's bounds would be if the editable area is focused.
  if (ParentObject() && ParentObject()->GetLayoutObject() &&
      ParentObject()->GetLayoutObject()->IsTextControlIncludingNG()) {
    return false;
  }

  // Ignore layout objects that are block flows with inline children. These
  // are usually dummy layout objects that pad out the tree, but there are
  // some exceptions below.
  auto* block_flow = DynamicTo<LayoutBlockFlow>(*layout_object_);
  if (block_flow && block_flow->ChildrenInline()) {
    // If the layout object has any plain text in it, that text will be
    // inside a LineBox, so the layout object will have a first LineBox.
    const bool has_any_text = HasLineBox(*block_flow);

    // Always include interesting-looking objects.
    if (has_any_text ||
        (GetNode() && GetNode()->HasAnyEventListeners(
                          event_util::MouseButtonEventTypes()))) {
      return false;
    }

    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXUninteresting));
    return true;
  }

  // If setting enabled, do not ignore SVG grouping (<g>) elements.
  if (IsA<SVGGElement>(GetNode())) {
    Settings* settings = GetDocument()->GetSettings();
    if (settings->GetAccessibilityIncludeSvgGElement()) {
      return false;
    }
  }

  // By default, objects should be ignored so that the AX hierarchy is not
  // filled with unnecessary items.
  if (ignored_reasons)
    ignored_reasons->push_back(IgnoredReason(kAXUninteresting));
  return true;
}

bool AXLayoutObject::HasAriaCellRole(Element* elem) const {
  DCHECK(elem);
  const AtomicString& aria_role_str =
      elem->FastGetAttribute(html_names::kRoleAttr);
  if (aria_role_str.IsEmpty())
    return false;

  ax::mojom::blink::Role aria_role = AriaRoleToWebCoreRole(aria_role_str);
  return aria_role == ax::mojom::blink::Role::kCell ||
         aria_role == ax::mojom::blink::Role::kColumnHeader ||
         aria_role == ax::mojom::blink::Role::kRowHeader;
}

// Return true if whitespace is not necessary to keep adjacent_node separate
// in screen reader output from surrounding nodes.
bool AXLayoutObject::CanIgnoreSpaceNextTo(LayoutObject* layout,
                                          bool is_after) const {
  if (!layout)
    return true;

  // If adjacent to a whitespace character, the current space can be ignored.
  if (layout->IsText()) {
    LayoutText* layout_text = ToLayoutText(layout);
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
  if (layout->IsLayoutImage())
    return false;

  // Do not keep spaces between blocks.
  if (!layout->IsLayoutInline())
    return true;

  // If next to an element that a screen reader will always read separately,
  // the the space can be ignored.
  // Elements that are naturally focusable even without a tabindex tend
  // to be rendered separately even if there is no space between them.
  // Some ARIA roles act like table cells and don't need adjacent whitespace to
  // indicate separation.
  // False negatives are acceptable in that they merely lead to extra whitespace
  // static text nodes.
  // TODO(aleventhal) Do we want this? Is it too hard/complex for Braille/Cvox?
  auto* elem = DynamicTo<Element>(layout->GetNode());
  if (elem && HasAriaCellRole(elem)) {
    return true;
  }

  // Test against the appropriate child text node.
  LayoutInline* layout_inline = ToLayoutInline(layout);
  LayoutObject* child =
      is_after ? layout_inline->FirstChild() : layout_inline->LastChild();
  return CanIgnoreSpaceNextTo(child, is_after);
}

bool AXLayoutObject::CanIgnoreTextAsEmpty() const {
  if (!layout_object_ || !layout_object_->IsText() || !layout_object_->Parent())
    return false;

  LayoutText* layout_text = ToLayoutText(layout_object_);

  // Ignore empty text
  if (layout_text->HasEmptyText()) {
    return true;
  }

  // Don't ignore node-less text (e.g. list bullets)
  Node* node = GetNode();
  if (!node)
    return false;

  // Always keep if anything other than collapsible whitespace.
  if (!layout_text->IsAllCollapsibleWhitespace())
    return false;

  // Will now look at sibling nodes.
  // Using "skipping children" methods as we need the closest element to the
  // whitespace markup-wise, e.g. tag1 in these examples:
  // [whitespace] <tag1><tag2>x</tag2></tag1>
  // <span>[whitespace]</span> <tag1><tag2>x</tag2></tag1>
  Node* prev_node = FlatTreeTraversal::PreviousSkippingChildren(*node);
  if (!prev_node)
    return true;

  Node* next_node = FlatTreeTraversal::NextSkippingChildren(*node);
  if (!next_node)
    return true;

  // Ignore extra whitespace-only text if a sibling will be presented
  // separately by screen readers whether whitespace is there or not.
  if (CanIgnoreSpaceNextTo(prev_node->GetLayoutObject(), false) ||
      CanIgnoreSpaceNextTo(next_node->GetLayoutObject(), true))
    return true;

  // Text elements with empty whitespace are returned, because of cases
  // such as <span>Hello</span><span> </span><span>World</span>. Keeping
  // the whitespace-only node means we now correctly expose "Hello World".
  // See crbug.com/435765.
  return false;
}

//
// Properties of static elements.
//

ax::mojom::blink::ListStyle AXLayoutObject::GetListStyle() const {
  const LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object)
    return AXNodeObject::GetListStyle();

  const ComputedStyle* computed_style = layout_object->Style();
  if (!computed_style)
    return AXNodeObject::GetListStyle();

  const StyleImage* style_image = computed_style->ListStyleImage();
  if (style_image && !style_image->ErrorOccurred())
    return ax::mojom::blink::ListStyle::kImage;

  switch (computed_style->ListStyleType()) {
    case EListStyleType::kNone:
      return ax::mojom::blink::ListStyle::kNone;
    case EListStyleType::kDisc:
      return ax::mojom::blink::ListStyle::kDisc;
    case EListStyleType::kCircle:
      return ax::mojom::blink::ListStyle::kCircle;
    case EListStyleType::kSquare:
      return ax::mojom::blink::ListStyle::kSquare;
    case EListStyleType::kDecimal:
    case EListStyleType::kDecimalLeadingZero:
      return ax::mojom::blink::ListStyle::kNumeric;
    default:
      return ax::mojom::blink::ListStyle::kOther;
  }
}

String AXLayoutObject::GetText() const {
  if (IsPasswordFieldAndShouldHideValue()) {
    if (!GetLayoutObject())
      return String();

    const ComputedStyle* style = GetLayoutObject()->Style();
    if (!style)
      return String();

    unsigned unmasked_text_length = AXNodeObject::GetText().length();
    if (!unmasked_text_length)
      return String();

    UChar mask_character = 0;
    switch (style->TextSecurity()) {
      case ETextSecurity::kNone:
        break;  // Fall through to the non-password branch.
      case ETextSecurity::kDisc:
        mask_character = kBulletCharacter;
        break;
      case ETextSecurity::kCircle:
        mask_character = kWhiteBulletCharacter;
        break;
      case ETextSecurity::kSquare:
        mask_character = kBlackSquareCharacter;
        break;
    }
    if (mask_character) {
      StringBuilder masked_text;
      masked_text.ReserveCapacity(unmasked_text_length);
      for (unsigned i = 0; i < unmasked_text_length; ++i)
        masked_text.Append(mask_character);
      return masked_text.ToString();
    }
  }

  return AXNodeObject::GetText();
}

ax::mojom::blink::WritingDirection AXLayoutObject::GetTextDirection() const {
  if (!GetLayoutObject())
    return AXNodeObject::GetTextDirection();

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style)
    return AXNodeObject::GetTextDirection();

  if (style->IsHorizontalWritingMode()) {
    switch (style->Direction()) {
      case TextDirection::kLtr:
        return ax::mojom::blink::WritingDirection::kLtr;
      case TextDirection::kRtl:
        return ax::mojom::blink::WritingDirection::kRtl;
    }
  } else {
    switch (style->Direction()) {
      case TextDirection::kLtr:
        return ax::mojom::blink::WritingDirection::kTtb;
      case TextDirection::kRtl:
        return ax::mojom::blink::WritingDirection::kBtt;
    }
  }

  return AXNodeObject::GetTextDirection();
}

ax::mojom::blink::TextPosition AXLayoutObject::GetTextPosition() const {
  if (!GetLayoutObject())
    return AXNodeObject::GetTextPosition();

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style)
    return AXNodeObject::GetTextPosition();

  switch (style->VerticalAlign()) {
    case EVerticalAlign::kBaseline:
    case EVerticalAlign::kMiddle:
    case EVerticalAlign::kTextTop:
    case EVerticalAlign::kTextBottom:
    case EVerticalAlign::kTop:
    case EVerticalAlign::kBottom:
    case EVerticalAlign::kBaselineMiddle:
    case EVerticalAlign::kLength:
      return AXNodeObject::GetTextPosition();
    case EVerticalAlign::kSub:
      return ax::mojom::blink::TextPosition::kSubscript;
    case EVerticalAlign::kSuper:
      return ax::mojom::blink::TextPosition::kSuperscript;
  }
}

static unsigned TextStyleFlag(ax::mojom::blink::TextStyle text_style_enum) {
  return static_cast<unsigned>(1 << static_cast<int>(text_style_enum));
}

void AXLayoutObject::GetTextStyleAndTextDecorationStyle(
    int32_t* text_style,
    ax::mojom::blink::TextDecorationStyle* text_overline_style,
    ax::mojom::blink::TextDecorationStyle* text_strikethrough_style,
    ax::mojom::blink::TextDecorationStyle* text_underline_style) const {
  if (!GetLayoutObject()) {
    AXNodeObject::GetTextStyleAndTextDecorationStyle(
        text_style, text_overline_style, text_strikethrough_style,
        text_underline_style);
    return;
  }
  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style) {
    AXNodeObject::GetTextStyleAndTextDecorationStyle(
        text_style, text_overline_style, text_strikethrough_style,
        text_underline_style);
    return;
  }

  *text_style = 0;
  *text_overline_style = ax::mojom::blink::TextDecorationStyle::kNone;
  *text_strikethrough_style = ax::mojom::blink::TextDecorationStyle::kNone;
  *text_underline_style = ax::mojom::blink::TextDecorationStyle::kNone;

  if (style->GetFontWeight() == BoldWeightValue())
    *text_style |= TextStyleFlag(ax::mojom::blink::TextStyle::kBold);
  if (style->GetFontDescription().Style() == ItalicSlopeValue())
    *text_style |= TextStyleFlag(ax::mojom::blink::TextStyle::kItalic);

  for (const auto& decoration : style->AppliedTextDecorations()) {
    if (EnumHasFlags(decoration.Lines(), TextDecoration::kOverline)) {
      *text_style |= TextStyleFlag(ax::mojom::blink::TextStyle::kOverline);
      *text_overline_style =
          TextDecorationStyleToAXTextDecorationStyle(decoration.Style());
    }
    if (EnumHasFlags(decoration.Lines(), TextDecoration::kLineThrough)) {
      *text_style |= TextStyleFlag(ax::mojom::blink::TextStyle::kLineThrough);
      *text_strikethrough_style =
          TextDecorationStyleToAXTextDecorationStyle(decoration.Style());
    }
    if (EnumHasFlags(decoration.Lines(), TextDecoration::kUnderline)) {
      *text_style |= TextStyleFlag(ax::mojom::blink::TextStyle::kUnderline);
      *text_underline_style =
          TextDecorationStyleToAXTextDecorationStyle(decoration.Style());
    }
  }
}

ax::mojom::blink::TextDecorationStyle
AXLayoutObject::TextDecorationStyleToAXTextDecorationStyle(
    const blink::ETextDecorationStyle text_decoration_style) {
  switch (text_decoration_style) {
    case ETextDecorationStyle::kDashed:
      return ax::mojom::blink::TextDecorationStyle::kDashed;
    case ETextDecorationStyle::kSolid:
      return ax::mojom::blink::TextDecorationStyle::kSolid;
    case ETextDecorationStyle::kDotted:
      return ax::mojom::blink::TextDecorationStyle::kDotted;
    case ETextDecorationStyle::kDouble:
      return ax::mojom::blink::TextDecorationStyle::kDouble;
    case ETextDecorationStyle::kWavy:
      return ax::mojom::blink::TextDecorationStyle::kWavy;
  }

  NOTREACHED();
  return ax::mojom::blink::TextDecorationStyle::kNone;
}

static bool ShouldUseLayoutNG(const LayoutObject& layout_object) {
  return (layout_object.IsInline() || layout_object.IsLayoutInline() ||
          layout_object.IsText()) &&
         layout_object.ContainingNGBlockFlow();
}

// Note: |NextOnLineInternalNG()| returns null when fragment for |layout_object|
// is culled as legacy layout version since |LayoutInline::LastLineBox()|
// returns null when it is culled.
// See also |PreviousOnLineInternalNG()| which is identical except for using
// "next" and |back()| instead of "previous" and |front()|.
static AXObject* NextOnLineInternalNG(const AXObject& ax_object) {
  DCHECK(!ax_object.IsDetached());
  const LayoutObject& layout_object = *ax_object.GetLayoutObject();
  DCHECK(ShouldUseLayoutNG(layout_object)) << layout_object;
  if (layout_object.IsBoxListMarkerIncludingNG() ||
      !layout_object.IsInLayoutNGInlineFormattingContext())
    return nullptr;
  NGInlineCursor cursor;
  cursor.MoveTo(layout_object);
  if (!cursor)
    return nullptr;
  for (;;) {
    cursor.MoveToNextInlineLeafOnLine();
    if (!cursor)
      break;
    LayoutObject* runner_layout_object = cursor.CurrentMutableLayoutObject();
    if (AXObject* result =
            ax_object.AXObjectCache().GetOrCreate(runner_layout_object))
      return result;
  }
  if (!ax_object.ParentObject())
    return nullptr;
  // Returns next object of parent, since next of |ax_object| isn't appeared on
  // line.
  return ax_object.ParentObject()->NextOnLine();
}

AXObject* AXLayoutObject::NextOnLine() const {
  // If this is the last object on the line, nullptr is returned. Otherwise, all
  // AXLayoutObjects, regardless of role and tree depth, are connected to the
  // next inline text box on the same line. If there is no inline text box, they
  // are connected to the next leaf AXObject.
  if (IsDetached())
    return nullptr;

  AXObject* result = nullptr;
  if (GetLayoutObject()->IsBoxListMarkerIncludingNG()) {
    // A list marker should be followed by a list item on the same line. The
    // list item might have no text children, so we don't eagerly descend to the
    // inline text box.
    //
    // For example, <li><button aria-label="button"></button></li>.
    //
    // This AXLayoutObject might not be included in the accessibility tree at
    // all, so "RawNextSibling" needs to be used to walk the layout tree.
    result = RawNextSibling();
  } else if (ShouldUseLayoutNG(*GetLayoutObject())) {
    result = NextOnLineInternalNG(*this);
  } else {
    InlineBox* inline_box = nullptr;
    if (GetLayoutObject()->IsBox()) {
      inline_box = ToLayoutBox(GetLayoutObject())->InlineBoxWrapper();
    } else if (GetLayoutObject()->IsLayoutInline()) {
      // For performance and memory consumption, LayoutInline may ignore some
      // inline-boxes during line layout because they don't actually impact
      // layout. This is known as "culled inline". We have to recursively look
      // to the LayoutInline's children via "LastLineBoxIncludingCulling".
      inline_box =
          ToLayoutInline(GetLayoutObject())->LastLineBoxIncludingCulling();
    } else if (GetLayoutObject()->IsText()) {
      inline_box = ToLayoutText(GetLayoutObject())->LastTextBox();
    }

    if (!inline_box)
      return nullptr;

    for (InlineBox* next = inline_box->NextOnLine(); next;
         next = next->NextOnLine()) {
      LayoutObject* layout_object =
          LineLayoutAPIShim::LayoutObjectFrom(next->GetLineLayoutItem());
      result = AXObjectCache().GetOrCreate(layout_object);
      if (result)
        break;
    }

    if (!result) {
      AXObject* parent = ParentObject();
      // Our parent object could have been created based on an ignored inline or
      // inline block spanning multiple lines. We need to ensure that we are
      // really at the end of our parent before attempting to connect to the
      // next AXObject that is on the same line as its last line.
      //
      // For example, look at the following layout tree:
      // LayoutBlockFlow
      // ++LayoutInline
      // ++++LayoutText "Beginning of line one "
      // ++++AnonymousLayoutInline
      // ++++++LayoutText "end of line one"
      // ++++++LayoutBR
      // ++++++LayoutText "Beginning of line two "
      // ++++LayoutText "End of line two"
      //
      // If we are on kStaticText "End of line one", and retrieve the parent
      // AXObject, it will be the anonymous layout inline which actually ends
      // somewhere in the second line, not the first line. Its "NextOnLine"
      // AXObject will be kStaticText "End of line two", which is obviously
      // wrong.
      //
      // Note that we can't use AXObject::IndexInParent() to do this, because
      // for performance reasons we don't define it on objects that are not
      // included in the accessibility tree at all.
      if (parent && !RawNextSibling())
        result = parent->NextOnLine();
    }
  }

  // For consistency between the forward and backward directions, try to always
  // return leaf nodes.
  if (result && result->ChildCountIncludingIgnored())
    return result->DeepestFirstChildIncludingIgnored();
  return result;
}

// Note: |PreviousOnLineInlineNG()| returns null when fragment for
// |layout_object| is culled as legacy layout version since
// |LayoutInline::FirstLineBox()| returns null when it is culled. See also
// |NextOnLineNG()| which is identical except for using "previous" and |front()|
// instead of "next" and |back()|.
static AXObject* PreviousOnLineInlineNG(const AXObject& ax_object) {
  DCHECK(!ax_object.IsDetached());
  const LayoutObject& layout_object = *ax_object.GetLayoutObject();
  DCHECK(ShouldUseLayoutNG(layout_object)) << layout_object;
  if (layout_object.IsBoxListMarkerIncludingNG() ||
      !layout_object.IsInLayoutNGInlineFormattingContext()) {
    return nullptr;
  }
  NGInlineCursor cursor;
  cursor.MoveTo(layout_object);
  if (!cursor)
    return nullptr;
  for (;;) {
    cursor.MoveToPreviousInlineLeafOnLine();
    if (!cursor)
      break;
    LayoutObject* earlier_layout_object = cursor.CurrentMutableLayoutObject();
    if (AXObject* result =
            ax_object.AXObjectCache().GetOrCreate(earlier_layout_object)) {
      return result;
    }
  }
  if (!ax_object.ParentObject())
    return nullptr;
  // Returns previous object of parent, since next of |ax_object| isn't appeared
  // on line.
  return ax_object.ParentObject()->PreviousOnLine();
}

AXObject* AXLayoutObject::PreviousOnLine() const {
  // If this is the first object on the line, nullptr is returned. Otherwise,
  // all AXLayoutObjects, regardless of role and tree depth, are connected to
  // the previous inline text box on the same line. If there is no inline text
  // box, they are connected to the previous leaf AXObject.
  if (IsDetached())
    return nullptr;

  AXObject* result = nullptr;
  AXObject* previous_sibling = AccessibilityIsIncludedInTree()
                                   ? PreviousSiblingIncludingIgnored()
                                   : nullptr;
  if (previous_sibling && previous_sibling->GetLayoutObject() &&
      previous_sibling->GetLayoutObject()->IsLayoutNGOutsideListMarker()) {
    // A list item should be proceeded by a list marker on the same line.
    result = previous_sibling;
  } else if (ShouldUseLayoutNG(*GetLayoutObject())) {
    result = PreviousOnLineInlineNG(*this);
  } else {
    InlineBox* inline_box = nullptr;
    if (GetLayoutObject()->IsBox()) {
      inline_box = ToLayoutBox(GetLayoutObject())->InlineBoxWrapper();
    } else if (GetLayoutObject()->IsLayoutInline()) {
      // For performance and memory consumption, LayoutInline may ignore some
      // inline-boxes during line layout because they don't actually impact
      // layout. This is known as "culled inline". We have to recursively look
      // to the LayoutInline's children via "FirstLineBoxIncludingCulling".
      inline_box =
          ToLayoutInline(GetLayoutObject())->FirstLineBoxIncludingCulling();
    } else if (GetLayoutObject()->IsText()) {
      inline_box = ToLayoutText(GetLayoutObject())->FirstTextBox();
    }

    if (!inline_box)
      return nullptr;

    for (InlineBox* prev = inline_box->PrevOnLine(); prev;
         prev = prev->PrevOnLine()) {
      LayoutObject* layout_object =
          LineLayoutAPIShim::LayoutObjectFrom(prev->GetLineLayoutItem());
      result = AXObjectCache().GetOrCreate(layout_object);
      if (result)
        break;
    }

    if (!result) {
      AXObject* parent = ParentObject();
      // Our parent object could have been created based on an ignored inline or
      // inline block spanning multiple lines. We need to ensure that we are
      // really at the start of our parent before attempting to connect to the
      // previous AXObject that is on the same line as its first line.
      //
      // For example, fook at the following layout tree:
      // LayoutBlockFlow
      // ++LayoutInline
      // ++++LayoutText "Beginning of line one "
      // ++++AnonymousLayoutInline
      // ++++++LayoutText "end of line one"
      // ++++++LayoutBR
      // ++++++LayoutText "Line two"
      //
      // If we are on kStaticText "Line two", and retrieve the parent AXObject,
      // it will be the anonymous layout inline which actually started somewhere
      // in the first line, not the second line. Its "PreviousOnLine" AXObject
      // will be kStaticText "Start of line one", which is obviously wrong.
      //
      // Note that we can't use AXObject::IndexInParent() to do this, because
      // for performance reasons we don't define it on objects that are not
      // included in the accessibility tree at all.
      if (parent && parent->RawFirstChild() == this)
        result = parent->PreviousOnLine();
    }
  }

  // For consistency between the forward and backward directions, try to always
  // return leaf nodes.
  if (result && result->ChildCountIncludingIgnored())
    return result->DeepestLastChildIncludingIgnored();
  return result;
}

//
// Properties of interactive elements.
//

String AXLayoutObject::StringValue() const {
  if (!layout_object_)
    return String();

  LayoutBoxModelObject* css_box = GetLayoutBoxModelObject();

  auto* select_element =
      DynamicTo<HTMLSelectElement>(layout_object_->GetNode());
  if (css_box && select_element && select_element->UsesMenuList()) {
    // LayoutMenuList will go straight to the text() of its selected item.
    // This has to be overridden in the case where the selected item has an ARIA
    // label.
    int selected_index = select_element->SelectedListIndex();
    const HeapVector<Member<HTMLElement>>& list_items =
        select_element->GetListItems();
    if (selected_index >= 0 &&
        static_cast<size_t>(selected_index) < list_items.size()) {
      const AtomicString& overridden_description =
          list_items[selected_index]->FastGetAttribute(
              html_names::kAriaLabelAttr);
      if (!overridden_description.IsNull())
        return overridden_description;
    }
    return select_element->InnerElement().innerText();
  }

  if (IsWebArea()) {
    // FIXME: Why would a layoutObject exist when the Document isn't attached to
    // a frame?
    if (layout_object_->GetFrame())
      return String();

    NOTREACHED();
  }

  if (IsTextControl())
    return GetText();

  if (layout_object_->IsFileUploadControl())
    return ToLayoutFileUploadControl(layout_object_)->FileTextValue();

  // Handle other HTML input elements that aren't text controls, like date and
  // time controls, by returning their value converted to text, with the
  // exception of checkboxes and radio buttons (which would return "on"), and
  // buttons which will return their name.
  // https://html.spec.whatwg.org/C/#dom-input-value
  if (const auto* input = DynamicTo<HTMLInputElement>(GetNode())) {
    if (input->type() == input_type_names::kFile)
      return input->FileStatusText();
    if (input->type() != input_type_names::kButton &&
        input->type() != input_type_names::kCheckbox &&
        input->type() != input_type_names::kImage &&
        input->type() != input_type_names::kRadio &&
        input->type() != input_type_names::kReset &&
        input->type() != input_type_names::kSubmit) {
      return input->value();
    }
  }

  // ARIA combobox can get value from  inner contents.
  if (AriaRoleAttribute() == ax::mojom::blink::Role::kComboBoxMenuButton) {
    AXObjectSet visited;
    return TextFromDescendants(visited, false);
  }

  // FIXME: We might need to implement a value here for more types
  // FIXME: It would be better not to advertise a value at all for the types for
  // which we don't implement one; this would require subclassing or making
  // accessibilityAttributeNames do something other than return a single static
  // array.
  return String();
}

String AXLayoutObject::TextAlternative(bool recursive,
                                       bool in_aria_labelled_by_traversal,
                                       AXObjectSet& visited,
                                       ax::mojom::blink::NameFrom& name_from,
                                       AXRelatedObjectVector* related_objects,
                                       NameSources* name_sources) const {
  if (layout_object_) {
    base::Optional<String> text_alternative = GetCSSAltText(GetNode());
    bool found_text_alternative = false;
    if (text_alternative) {
      if (name_sources) {
        name_sources->push_back(NameSource(false));
        name_sources->back().type = ax::mojom::blink::NameFrom::kAttribute;
        name_sources->back().text = text_alternative.value();
      }
      return text_alternative.value();
    }
    if (layout_object_->IsBR()) {
      text_alternative = String("\n");
      found_text_alternative = true;
    } else if (layout_object_->IsText() &&
               (!recursive || !layout_object_->IsCounter())) {
      LayoutText* layout_text = ToLayoutText(layout_object_);
      String visible_text = layout_text->PlainText();  // Actual rendered text.
      // If no text boxes we assume this is unrendered end-of-line whitespace.
      // TODO find robust way to deterministically detect end-of-line space.
      if (visible_text.IsEmpty()) {
        // No visible rendered text -- must be whitespace.
        // Either it is useful whitespace for separating words or not.
        if (layout_text->IsAllCollapsibleWhitespace()) {
          if (cached_is_ignored_)
            return "";
          // If no textboxes, this was whitespace at the line's end.
          text_alternative = " ";
        } else {
          text_alternative = layout_text->GetText();
        }
      } else {
        text_alternative = visible_text;
      }
      found_text_alternative = true;
    } else if (layout_object_->IsListMarkerForNormalContent() && !recursive) {
      text_alternative =
          To<LayoutListMarker>(layout_object_)->TextAlternative();
      found_text_alternative = true;
    } else if (!recursive) {
      if (ListMarker* marker = ListMarker::Get(layout_object_)) {
        text_alternative = marker->TextAlternative(*layout_object_);
        found_text_alternative = true;
      }
    }

    if (found_text_alternative) {
      name_from = ax::mojom::blink::NameFrom::kContents;
      if (name_sources) {
        name_sources->push_back(NameSource(false));
        name_sources->back().type = name_from;
        name_sources->back().text = text_alternative.value();
      }
      return text_alternative.value();
    }
  }

  return AXNodeObject::TextAlternative(recursive, in_aria_labelled_by_traversal,
                                       visited, name_from, related_objects,
                                       name_sources);
}

//
// Hit testing.
//

AXObject* AXLayoutObject::AccessibilityHitTest(const IntPoint& point) const {
  if (!layout_object_ || !layout_object_->HasLayer() ||
      !layout_object_->IsBox())
    return nullptr;

    // Must be called with lifecycle >= pre-paint clean
#if DCHECK_IS_ON()
  DCHECK_GE(GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);
#endif

  PaintLayer* layer = ToLayoutBox(layout_object_)->Layer();

  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive |
                         HitTestRequest::kRetargetForInert);
  HitTestLocation location(point);
  HitTestResult hit_test_result = HitTestResult(request, location);
  layer->HitTest(location, hit_test_result,
                 PhysicalRect(PhysicalRect::InfiniteIntRect()));

  Node* node = hit_test_result.InnerNode();
  if (!node)
    return nullptr;

  if (auto* area = DynamicTo<HTMLAreaElement>(node))
    return AccessibilityImageMapHitTest(area, point);

  if (auto* option = DynamicTo<HTMLOptionElement>(node)) {
    node = option->OwnerSelectElement();
    if (!node)
      return nullptr;
  }

  // If |node| is in a user-agent shadow tree, reassign it as the host to hide
  // details in the shadow tree. Previously this was implemented by using
  // Retargeting (https://dom.spec.whatwg.org/#retarget), but this caused
  // elements inside regular shadow DOMs to be ignored by screen reader. See
  // crbug.com/1111800 and crbug.com/1048959.
  const TreeScope& tree_scope = node->GetTreeScope();
  if (auto* shadow_root = DynamicTo<ShadowRoot>(tree_scope.RootNode())) {
    if (shadow_root->IsUserAgent())
      node = &shadow_root->host();
  }

  LayoutObject* obj = node->GetLayoutObject();
  if (!obj)
    return nullptr;

  AXObject* result = AXObjectCache().GetOrCreate(obj);
  result->UpdateChildrenIfNecessary();

  // Allow the element to perform any hit-testing it might need to do to reach
  // non-layout children.
  result = result->ElementAccessibilityHitTest(point);
  if (result && result->AccessibilityIsIgnored()) {
    // If this element is the label of a control, a hit test should return the
    // control.
    if (auto* ax_object = DynamicTo<AXLayoutObject>(result)) {
      AXObject* control_object =
          ax_object->CorrespondingControlAXObjectForLabelElement();
      if (control_object && control_object->NameFromLabelElement())
        return control_object;
    }

    result = result->ParentObjectUnignored();
  }

  return result;
}

AXObject* AXLayoutObject::ElementAccessibilityHitTest(
    const IntPoint& point) const {
  if (IsSVGImage())
    return RemoteSVGElementHitTest(point);

  return AXObject::ElementAccessibilityHitTest(point);
}

//
// Low-level accessibility tree exploration, only for use within the
// accessibility module.
//
// LAYOUT TREE WALKING ALGORITHM
//
// The fundamental types of elements in the Blink layout tree are block
// elements and inline elements. It can get a little confusing when
// an inline element has both inline and block children, for example:
//
//   <a href="#">
//     Before Block
//     <div>
//       In Block
//     </div>
//     Outside Block
//   </a>
//
// Blink wants to maintain the invariant that all of the children of a node
// are either all block or all inline, so it creates three anonymous blocks:
//
//      #1 LayoutBlockFlow (anonymous)
//        #2 LayoutInline A continuation=#4
//          #3 LayoutText "Before Block"
//      #4 LayoutBlockFlow (anonymous) continuation=#8
//        #5 LayoutBlockFlow DIV
//          #6 LayoutText "In Block"
//      #7 LayoutBlockFlow (anonymous)
//        #8 LayoutInline A is_continuation
//          #9 LayoutText "Outside Block"
//
// For a good explanation of why this is done, see this blog entry. It's
// describing WebKit in 2007, but the fundamentals haven't changed much.
//
// https://webkit.org/blog/115/webcore-rendering-ii-blocks-and-inlines/
//
// Now, it's important to understand that we couldn't just use the layout
// tree as the accessibility tree as-is, because the div is no longer
// inside the link! In fact, the link has been split into two different
// nodes, #2 and #8. Luckily, the layout tree contains continuations to
// help us untangle situations like this.
//
// Here's the algorithm we use to walk the layout tree in order to build
// the accessibility tree:
//
// 1. When computing the first child or next sibling of a node, skip over any
//    LayoutObjects that are continuations.
//
// 2. When computing the next sibling of a node and there are no more siblings
//    in the layout tree, see if the parent node has a continuation, and if
//    so follow it and make that the next sibling.
//
// 3. When computing the first child of a node that has a continuation but
//    no children in the layout tree, the continuation is the first child.
//
// The end result is this tree, which we use as the basis for the
// accessibility tree.
//
//      #1 LayoutBlockFlow (anonymous)
//        #2 LayoutInline A
//          #3 LayoutText "Before Block"
//          #4 LayoutBlockFlow (anonymous)
//            #5 LayoutBlockFlow DIV
//              #6 LayoutText "In Block"
//            #8 LayoutInline A is_continuation
//              #9 LayoutText "Outside Block"
//      #7 LayoutBlockFlow (anonymous)
//
// This algorithm results in an accessibility tree that preserves containment
// (i.e. the contents of the link in the example above are descendants of the
// link node) while including all of the rich layout detail from the layout
// tree.
//
// There are just a couple of other corner cases to walking the layout tree:
//
// * Walk tables in table order (thead, tbody, tfoot), which may not match
//   layout order.
// * Skip CSS first-letter nodes.
//

// Given a layout object, return the start of the continuation chain.
static inline LayoutInline* StartOfContinuations(LayoutObject* layout_object) {
  // See LAYOUT TREE WALKING ALGORITHM, above, for more context as to why
  // we need to do this.

  // For inline elements, if it's a continuation, the start of the chain
  // is always the primary layout object associated with the node.
  if (layout_object->IsInlineElementContinuation())
    return ToLayoutInline(layout_object->GetNode()->GetLayoutObject());

  // Blocks with a previous continuation always have a next continuation,
  // so we can get the next continuation and do the same trick to get
  // the primary layout object associated with the node.
  auto* layout_block_flow = DynamicTo<LayoutBlockFlow>(layout_object);
  if (layout_block_flow && layout_block_flow->InlineElementContinuation()) {
    LayoutInline* result =
        ToLayoutInline(layout_block_flow->InlineElementContinuation()
                           ->GetNode()
                           ->GetLayoutObject());
    DCHECK_NE(result, layout_object);
    return result;
  }

  return nullptr;
}

// See LAYOUT TREE WALKING ALGORITHM, above, for details.
static inline LayoutObject* ParentLayoutObject(LayoutObject* layout_object) {
  if (!layout_object)
    return nullptr;

  // If the node is a continuation, the parent is the start of the continuation
  // chain.  See LAYOUT TREE WALKING ALGORITHM, above, for more context as to
  // why we need to do this.
  LayoutObject* start_of_conts = StartOfContinuations(layout_object);
  if (start_of_conts)
    return start_of_conts;

  // Otherwise just return the parent in the layout tree.
  return layout_object->Parent();
}

// See LAYOUT TREE WALKING ALGORITHM, above, for details.
// Return true if this layout object is the continuation of some other
// layout object.
static bool IsContinuation(LayoutObject* layout_object) {
  if (layout_object->IsElementContinuation())
    return true;

  auto* block_flow = DynamicTo<LayoutBlockFlow>(layout_object);
  return block_flow && block_flow->IsAnonymousBlock() &&
         block_flow->Continuation();
}

// See LAYOUT TREE WALKING ALGORITHM, above, for details.
// Return the continuation of this layout object, or nullptr if it doesn't
// have one.
LayoutObject* GetContinuation(LayoutObject* layout_object) {
  if (layout_object->IsLayoutInline())
    return ToLayoutInline(layout_object)->Continuation();

  if (auto* block_flow = DynamicTo<LayoutBlockFlow>(layout_object))
    return block_flow->Continuation();

  return nullptr;
}

// See LAYOUT TREE WALKING ALGORITHM, above, for details.
AXObject* AXLayoutObject::RawFirstChild() const {
  if (!layout_object_)
    return nullptr;

  // Walk sections of a table (thead, tbody, tfoot) in visual order.
  // Note: always call RecalcSectionsIfNeeded() before accessing
  // the sections of a LayoutTable.
  if (layout_object_->IsTable()) {
    LayoutNGTableInterface* table =
        ToInterface<LayoutNGTableInterface>(layout_object_);
    table->RecalcSectionsIfNeeded();
    LayoutNGTableSectionInterface* top_section = table->TopSectionInterface();
    return AXObjectCache().GetOrCreate(
        top_section ? top_section->ToMutableLayoutObject() : nullptr);
  }

  LayoutObject* first_child = layout_object_->SlowFirstChild();

  // CSS first-letter pseudo element is handled as continuation. Returning it
  // will result in duplicated elements.
  if (first_child && first_child->IsText() &&
      ToLayoutText(first_child)->IsTextFragment() &&
      ToLayoutTextFragment(first_child)->GetFirstLetterPseudoElement())
    return nullptr;

  // Skip over continuations.
  while (first_child && IsContinuation(first_child))
    first_child = first_child->NextSibling();

  // If there's a first child that's not a continuation, return that.
  if (first_child)
    return AXObjectCache().GetOrCreate(first_child);

  // Finally check if this object has no children but it has a continuation
  // itself - and if so, it's the first child.
  LayoutObject* continuation = GetContinuation(layout_object_);
  if (continuation)
    return AXObjectCache().GetOrCreate(continuation);

  return nullptr;
}

// See LAYOUT TREE WALKING ALGORITHM, above, for details.
AXObject* AXLayoutObject::RawNextSibling() const {
  if (!layout_object_)
    return nullptr;

  // Walk sections of a table (thead, tbody, tfoot) in visual order.
  if (layout_object_->IsTableSection()) {
    const LayoutNGTableSectionInterface* section =
        ToInterface<LayoutNGTableSectionInterface>(layout_object_);
    const LayoutNGTableSectionInterface* section_below =
        section->TableInterface()->SectionBelowInterface(section,
                                                         kSkipEmptySections);
    // const_cast is necessary to avoid creating non-const versions of
    // table interfaces.
    LayoutObject* section_below_layout_object = const_cast<LayoutObject*>(
        section_below ? section_below->ToLayoutObject() : nullptr);
    return AXObjectCache().GetOrCreate(section_below_layout_object);
  }

  // If it's not a continuation, just get the next sibling from the
  // layout tree, skipping over continuations.
  if (!IsContinuation(layout_object_)) {
    LayoutObject* next_sibling = layout_object_->NextSibling();
    while (next_sibling && IsContinuation(next_sibling))
      next_sibling = next_sibling->NextSibling();

    if (next_sibling)
      return AXObjectCache().GetOrCreate(next_sibling);
  }

  // If we've run out of siblings, check to see if the parent of this
  // object has a continuation, and if so, follow it.
  LayoutObject* parent = layout_object_->Parent();
  if (parent) {
    LayoutObject* continuation = GetContinuation(parent);
    if (continuation)
      return AXObjectCache().GetOrCreate(continuation);
  }

  return nullptr;
}

//
// High-level accessibility tree access.
//

AXObject* AXLayoutObject::ComputeParent() const {
  DCHECK(!IsDetached());
  if (!layout_object_)
    return nullptr;

  if (AriaRoleAttribute() == ax::mojom::blink::Role::kMenuBar)
    return AXObjectCache().GetOrCreate(layout_object_->Parent());

  if (GetNode())
    return AXNodeObject::ComputeParent();

  LayoutObject* parent_layout_obj = ParentLayoutObject(layout_object_);
  if (parent_layout_obj)
    return AXObjectCache().GetOrCreate(parent_layout_obj);

  // A WebArea's parent should be the page popup owner, if any, otherwise null.
  if (IsWebArea()) {
    LocalFrame* frame = layout_object_->GetFrame();
    return AXObjectCache().GetOrCreate(frame->PagePopupOwner());
  }

  return nullptr;
}

AXObject* AXLayoutObject::ComputeParentIfExists() const {
  if (!layout_object_)
    return nullptr;

  if (AriaRoleAttribute() == ax::mojom::blink::Role::kMenuBar)
    return AXObjectCache().Get(layout_object_->Parent());

  if (GetNode())
    return AXNodeObject::ComputeParentIfExists();

  LayoutObject* parent_layout_obj = ParentLayoutObject(layout_object_);
  if (parent_layout_obj)
    return AXObjectCache().Get(parent_layout_obj);

  // A WebArea's parent should be the page popup owner, if any, otherwise null.
  if (IsWebArea()) {
    LocalFrame* frame = layout_object_->GetFrame();
    return AXObjectCache().Get(frame->PagePopupOwner());
  }

  return nullptr;
}

bool AXLayoutObject::CanHaveChildren() const {
  if (!layout_object_)
    return false;
  if (GetCSSAltText(GetNode()))
    return false;
  if (layout_object_->IsListMarkerForNormalContent())
    return false;
  return AXNodeObject::CanHaveChildren();
}

//
// DOM and layout tree access.
//

Node* AXLayoutObject::GetNode() const {
  return GetLayoutObject() ? GetLayoutObject()->GetNode() : nullptr;
}

Document* AXLayoutObject::GetDocument() const {
  if (!GetLayoutObject())
    return nullptr;
  return &GetLayoutObject()->GetDocument();
}

LocalFrameView* AXLayoutObject::DocumentFrameView() const {
  if (!GetLayoutObject())
    return nullptr;

  // this is the LayoutObject's Document's LocalFrame's LocalFrameView
  return GetLayoutObject()->GetDocument().View();
}

Element* AXLayoutObject::AnchorElement() const {
  if (!layout_object_)
    return nullptr;

  AXObjectCacheImpl& cache = AXObjectCache();
  LayoutObject* curr_layout_object;

  // Search up the layout tree for a LayoutObject with a DOM node. Defer to an
  // earlier continuation, though.
  for (curr_layout_object = layout_object_;
       curr_layout_object && !curr_layout_object->GetNode();
       curr_layout_object = curr_layout_object->Parent()) {
    auto* curr_block_flow = DynamicTo<LayoutBlockFlow>(curr_layout_object);
    if (!curr_block_flow || !curr_block_flow->IsAnonymousBlock())
      continue;

    if (LayoutObject* continuation = curr_block_flow->Continuation())
      return cache.GetOrCreate(continuation)->AnchorElement();
  }
  // bail if none found
  if (!curr_layout_object)
    return nullptr;

  // Search up the DOM tree for an anchor element.
  // NOTE: this assumes that any non-image with an anchor is an
  // HTMLAnchorElement
  Node* node = curr_layout_object->GetNode();
  if (!node)
    return nullptr;
  for (Node& runner : NodeTraversal::InclusiveAncestorsOf(*node)) {
    if (IsA<HTMLAnchorElement>(runner))
      return To<Element>(&runner);

    if (LayoutObject* layout_object = runner.GetLayoutObject()) {
      AXObject* ax_object = cache.GetOrCreate(layout_object);
      if (ax_object && ax_object->IsAnchor())
        return To<Element>(&runner);
    }
  }

  return nullptr;
}

//
// Modify or take an action on an object.
//

bool AXLayoutObject::OnNativeSetValueAction(const String& string) {
  if (!GetNode() || !GetNode()->IsElementNode())
    return false;
  if (!layout_object_ || !layout_object_->IsBoxModelObject())
    return false;

  LayoutBoxModelObject* layout_object = ToLayoutBoxModelObject(layout_object_);
  auto* html_input_element = DynamicTo<HTMLInputElement>(*GetNode());
  if (html_input_element && layout_object->IsTextFieldIncludingNG()) {
    html_input_element->setValue(
        string, TextFieldEventBehavior::kDispatchInputAndChangeEvent);
    return true;
  }

  if (auto* text_area_element = DynamicTo<HTMLTextAreaElement>(*GetNode())) {
    DCHECK(layout_object->IsTextAreaIncludingNG());
    text_area_element->setValue(
        string, TextFieldEventBehavior::kDispatchInputAndChangeEvent);
    return true;
  }

  if (HasContentEditableAttributeSet()) {
    ExceptionState exception_state(v8::Isolate::GetCurrent(),
                                   ExceptionState::kExecutionContext, nullptr,
                                   nullptr);
    To<HTMLElement>(GetNode())->setInnerText(string, exception_state);
    if (exception_state.HadException()) {
      exception_state.ClearException();
      return false;
    }
    return true;
  }

  return false;
}

//
// Notifications that this object may have changed.
//

void AXLayoutObject::HandleActiveDescendantChanged() {
  if (!GetLayoutObject() || !GetNode() || !GetDocument())
    return;

  Node* focused_node = GetDocument()->FocusedElement();
  if (focused_node == GetNode()) {
    AXObject* active_descendant = ActiveDescendant();
    if (active_descendant && active_descendant->IsSelectedFromFocus()) {
      // In single selection containers, selection follows focus, so a selection
      // changed event must be fired. This ensures the AT is notified that the
      // selected state has changed, so that it does not read "unselected" as
      // the user navigates through the items.
      AXObjectCache().HandleAriaSelectedChangedWithCleanLayout(
          active_descendant->GetNode());
    }

    // Mark this node dirty. AXEventGenerator will automatically infer
    // that the active descendant changed.
    AXObjectCache().MarkAXObjectDirty(this, false);
  }
}

void AXLayoutObject::HandleAriaExpandedChanged() {
  // Find if a parent of this object should handle aria-expanded changes.
  AXObject* container_parent = this->ParentObject();
  while (container_parent) {
    bool found_parent = false;

    switch (container_parent->RoleValue()) {
      case ax::mojom::blink::Role::kLayoutTable:
      case ax::mojom::blink::Role::kTree:
      case ax::mojom::blink::Role::kTreeGrid:
      case ax::mojom::blink::Role::kGrid:
      case ax::mojom::blink::Role::kTable:
        found_parent = true;
        break;
      default:
        break;
    }

    if (found_parent)
      break;

    container_parent = container_parent->ParentObject();
  }

  // Post that the row count changed.
  if (container_parent) {
    AXObjectCache().PostNotification(container_parent,
                                     ax::mojom::blink::Event::kRowCountChanged);
  }

  // Post that the specific row either collapsed or expanded.
  AccessibilityExpanded expanded = IsExpanded();
  if (!expanded)
    return;

  if (RoleValue() == ax::mojom::blink::Role::kRow ||
      RoleValue() == ax::mojom::blink::Role::kTreeItem) {
    ax::mojom::blink::Event notification =
        ax::mojom::blink::Event::kRowExpanded;
    if (expanded == kExpandedCollapsed)
      notification = ax::mojom::blink::Event::kRowCollapsed;

    AXObjectCache().PostNotification(this, notification);
  } else {
    AXObjectCache().PostNotification(this,
                                     ax::mojom::blink::Event::kExpandedChanged);
  }
}

bool AXLayoutObject::IsAutofillAvailable() const {
  // Autofill state is stored in AXObjectCache.
  WebAXAutofillState state = AXObjectCache().GetAutofillState(AXObjectID());
  return state == WebAXAutofillState::kAutofillAvailable;
}

void AXLayoutObject::HandleAutofillStateChanged(WebAXAutofillState state) {
  // Autofill state is stored in AXObjectCache.
  AXObjectCache().SetAutofillState(AXObjectID(), state);
}

// The following is a heuristic used to determine if a
// <table> should be with ax::mojom::blink::Role::kTable or
// ax::mojom::blink::Role::kLayoutTable.
bool AXLayoutObject::IsDataTable() const {
  if (!layout_object_ || !GetNode())
    return false;

  // If it has an ARIA role, it's definitely a data table.
  AtomicString role;
  if (HasAOMPropertyOrARIAAttribute(AOMStringProperty::kRole, role))
    return true;

  // When a section of the document is contentEditable, all tables should be
  // treated as data tables, otherwise users may not be able to work with rich
  // text editors that allow creating and editing tables.
  if (GetNode() && HasEditableStyle(*GetNode()))
    return true;

  // This employs a heuristic to determine if this table should appear.
  // Only "data" tables should be exposed as tables.
  // Unfortunately, there is no good way to determine the difference
  // between a "layout" table and a "data" table.
  auto* table_element = DynamicTo<HTMLTableElement>(GetNode());
  if (!table_element)
    return false;

  // If there is a caption element, summary, THEAD, or TFOOT section, it's most
  // certainly a data table
  if (!table_element->Summary().IsEmpty() || table_element->tHead() ||
      table_element->tFoot() || table_element->caption())
    return true;

  // if someone used "rules" attribute than the table should appear
  if (!table_element->Rules().IsEmpty())
    return true;

  // if there's a colgroup or col element, it's probably a data table.
  if (Traversal<HTMLTableColElement>::FirstChild(*table_element))
    return true;

  // If there are at least 20 rows, we'll call it a data table.
  HTMLTableRowsCollection* rows = table_element->rows();
  int num_rows = rows->length();
  if (num_rows >= 20)
    return true;
  if (num_rows <= 0)
    return false;

  int num_cols_in_first_body = rows->Item(0)->cells()->length();
  // If there's only one cell, it's not a good AXTable candidate.
  if (num_rows == 1 && num_cols_in_first_body == 1)
    return false;

  // Store the background color of the table to check against cell's background
  // colors.
  const ComputedStyle* table_style = layout_object_->Style();
  if (!table_style)
    return false;

  Color table_bg_color =
      table_style->VisitedDependentColor(GetCSSPropertyBackgroundColor());
  bool has_cell_spacing = table_style->HorizontalBorderSpacing() &&
                          table_style->VerticalBorderSpacing();

  // check enough of the cells to find if the table matches our criteria
  // Criteria:
  //   1) must have at least one valid cell (and)
  //   2) at least half of cells have borders (or)
  //   3) at least half of cells have different bg colors than the table, and
  //      there is cell spacing
  unsigned valid_cell_count = 0;
  unsigned bordered_cell_count = 0;
  unsigned background_difference_cell_count = 0;
  unsigned cells_with_top_border = 0;
  unsigned cells_with_bottom_border = 0;
  unsigned cells_with_left_border = 0;
  unsigned cells_with_right_border = 0;

  Color alternating_row_colors[5];
  int alternating_row_color_count = 0;
  for (int row = 0; row < num_rows; ++row) {
    HTMLTableRowElement* row_element = rows->Item(row);
    int n_cols = row_element->cells()->length();
    for (int col = 0; col < n_cols; ++col) {
      const Element* cell = row_element->cells()->item(col);
      if (!cell)
        continue;
      // Any <th> tag -> treat as data table.
      if (cell->HasTagName(html_names::kThTag))
        return true;

      // Check for an explicitly assigned a "data" table attribute.
      auto* cell_elem = DynamicTo<HTMLTableCellElement>(*cell);
      if (cell_elem) {
        if (!cell_elem->Headers().IsEmpty() || !cell_elem->Abbr().IsEmpty() ||
            !cell_elem->Axis().IsEmpty() ||
            !cell_elem->FastGetAttribute(html_names::kScopeAttr).IsEmpty())
          return true;
      }

      LayoutObject* cell_layout_object = cell->GetLayoutObject();
      if (!cell_layout_object || !cell_layout_object->IsLayoutBlock())
        continue;

      const LayoutBlock* cell_layout_block =
          To<LayoutBlock>(cell_layout_object);
      if (cell_layout_block->Size().Width() < 1 ||
          cell_layout_block->Size().Height() < 1)
        continue;

      valid_cell_count++;

      const ComputedStyle* computed_style = cell_layout_block->Style();
      if (!computed_style)
        continue;

      // If the empty-cells style is set, we'll call it a data table.
      if (computed_style->EmptyCells() == EEmptyCells::kHide)
        return true;

      // If a cell has matching bordered sides, call it a (fully) bordered cell.
      if ((cell_layout_block->BorderTop() > 0 &&
           cell_layout_block->BorderBottom() > 0) ||
          (cell_layout_block->BorderLeft() > 0 &&
           cell_layout_block->BorderRight() > 0))
        bordered_cell_count++;

      // Also keep track of each individual border, so we can catch tables where
      // most cells have a bottom border, for example.
      if (cell_layout_block->BorderTop() > 0)
        cells_with_top_border++;
      if (cell_layout_block->BorderBottom() > 0)
        cells_with_bottom_border++;
      if (cell_layout_block->BorderLeft() > 0)
        cells_with_left_border++;
      if (cell_layout_block->BorderRight() > 0)
        cells_with_right_border++;

      // If the cell has a different color from the table and there is cell
      // spacing, then it is probably a data table cell (spacing and colors take
      // the place of borders).
      Color cell_color = computed_style->VisitedDependentColor(
          GetCSSPropertyBackgroundColor());
      if (has_cell_spacing && table_bg_color != cell_color &&
          cell_color.Alpha() != 1)
        background_difference_cell_count++;

      // If we've found 10 "good" cells, we don't need to keep searching.
      if (bordered_cell_count >= 10 || background_difference_cell_count >= 10)
        return true;

      // For the first 5 rows, cache the background color so we can check if
      // this table has zebra-striped rows.
      if (row < 5 && row == alternating_row_color_count) {
        LayoutObject* layout_row = cell_layout_block->Parent();
        if (!layout_row || !layout_row->IsBoxModelObject() ||
            !ToLayoutBoxModelObject(layout_row)->IsTableRow())
          continue;
        const ComputedStyle* row_computed_style = layout_row->Style();
        if (!row_computed_style)
          continue;
        Color row_color = row_computed_style->VisitedDependentColor(
            GetCSSPropertyBackgroundColor());
        alternating_row_colors[alternating_row_color_count] = row_color;
        alternating_row_color_count++;
      }
    }
  }

  // if there is less than two valid cells, it's not a data table
  if (valid_cell_count <= 1)
    return false;

  // half of the cells had borders, it's a data table
  unsigned needed_cell_count = valid_cell_count / 2;
  if (bordered_cell_count >= needed_cell_count ||
      cells_with_top_border >= needed_cell_count ||
      cells_with_bottom_border >= needed_cell_count ||
      cells_with_left_border >= needed_cell_count ||
      cells_with_right_border >= needed_cell_count)
    return true;

  // half had different background colors, it's a data table
  if (background_difference_cell_count >= needed_cell_count)
    return true;

  // Check if there is an alternating row background color indicating a zebra
  // striped style pattern.
  if (alternating_row_color_count > 2) {
    Color first_color = alternating_row_colors[0];
    for (int k = 1; k < alternating_row_color_count; k++) {
      // If an odd row was the same color as the first row, its not alternating.
      if (k % 2 == 1 && alternating_row_colors[k] == first_color)
        return false;
      // If an even row is not the same as the first row, its not alternating.
      if (!(k % 2) && alternating_row_colors[k] != first_color)
        return false;
    }
    return true;
  }

  return false;
}

unsigned AXLayoutObject::ColumnCount() const {
  if (AriaRoleAttribute() != ax::mojom::blink::Role::kUnknown)
    return AXNodeObject::ColumnCount();

  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || !layout_object->IsTable() || !layout_object->GetNode())
    return AXNodeObject::ColumnCount();

  LayoutNGTableInterface* table =
      ToInterface<LayoutNGTableInterface>(layout_object);
  table->RecalcSectionsIfNeeded();
  LayoutNGTableSectionInterface* table_section = table->TopSectionInterface();
  if (!table_section)
    return AXNodeObject::ColumnCount();

  return table_section->NumEffectiveColumns();
}

unsigned AXLayoutObject::RowCount() const {
  if (AriaRoleAttribute() != ax::mojom::blink::Role::kUnknown)
    return AXNodeObject::RowCount();

  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || !layout_object->IsTable() || !layout_object->GetNode())
    return AXNodeObject::RowCount();

  LayoutNGTableInterface* table =
      ToInterface<LayoutNGTableInterface>(layout_object);
  table->RecalcSectionsIfNeeded();

  unsigned row_count = 0;
  const LayoutNGTableSectionInterface* table_section =
      table->TopSectionInterface();
  if (!table_section)
    return AXNodeObject::RowCount();

  while (table_section) {
    row_count += table_section->NumRows();
    table_section =
        table->SectionBelowInterface(table_section, kSkipEmptySections);
  }
  return row_count;
}

unsigned AXLayoutObject::ColumnIndex() const {
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || !layout_object->GetNode())
    return AXNodeObject::ColumnIndex();

  if (layout_object->IsTableCell()) {
    const LayoutNGTableCellInterface* cell =
        ToInterface<LayoutNGTableCellInterface>(layout_object);
    return cell->TableInterface()->AbsoluteColumnToEffectiveColumn(
        cell->AbsoluteColumnIndex());
  }

  return AXNodeObject::ColumnIndex();
}

unsigned AXLayoutObject::RowIndex() const {
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || !layout_object->GetNode())
    return AXNodeObject::RowIndex();

  unsigned row_index = 0;
  const LayoutNGTableSectionInterface* row_section = nullptr;
  const LayoutNGTableInterface* table = nullptr;
  if (layout_object->IsTableRow()) {
    const LayoutNGTableRowInterface* row =
        ToInterface<LayoutNGTableRowInterface>(layout_object);
    row_index = row->RowIndex();
    row_section = row->SectionInterface();
    table = row->TableInterface();
  } else if (layout_object->IsTableCell()) {
    const LayoutNGTableCellInterface* cell =
        ToInterface<LayoutNGTableCellInterface>(layout_object);
    row_index = cell->RowIndex();
    row_section = cell->SectionInterface();
    table = cell->TableInterface();
  } else {
    return AXNodeObject::RowIndex();
  }

  if (!table || !row_section)
    return AXNodeObject::RowIndex();

  // Since our table might have multiple sections, we have to offset our row
  // appropriately.
  table->RecalcSectionsIfNeeded();
  const LayoutNGTableSectionInterface* section = table->TopSectionInterface();
  while (section && section != row_section) {
    row_index += section->NumRows();
    section = table->SectionBelowInterface(section, kSkipEmptySections);
  }

  return row_index;
}

unsigned AXLayoutObject::ColumnSpan() const {
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || !layout_object->IsTableCell())
    return AXNodeObject::ColumnSpan();

  const LayoutNGTableCellInterface* cell =
      ToInterface<LayoutNGTableCellInterface>(layout_object);
  unsigned absolute_first_col = cell->AbsoluteColumnIndex();
  unsigned absolute_last_col = absolute_first_col + cell->ColSpan() - 1;
  unsigned effective_first_col =
      cell->TableInterface()->AbsoluteColumnToEffectiveColumn(
          absolute_first_col);
  unsigned effective_last_col =
      cell->TableInterface()->AbsoluteColumnToEffectiveColumn(
          absolute_last_col);
  return effective_last_col - effective_first_col + 1;
}

unsigned AXLayoutObject::RowSpan() const {
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || !layout_object->IsTableCell())
    return AXNodeObject::ColumnSpan();

  LayoutNGTableCellInterface* cell =
      ToInterface<LayoutNGTableCellInterface>(layout_object);
  return cell->ResolvedRowSpan();
}

ax::mojom::blink::SortDirection AXLayoutObject::GetSortDirection() const {
  if (RoleValue() != ax::mojom::blink::Role::kRowHeader &&
      RoleValue() != ax::mojom::blink::Role::kColumnHeader) {
    return ax::mojom::blink::SortDirection::kNone;
  }

  const AtomicString& aria_sort =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kSort);
  if (aria_sort.IsEmpty())
    return ax::mojom::blink::SortDirection::kNone;
  if (EqualIgnoringASCIICase(aria_sort, "none"))
    return ax::mojom::blink::SortDirection::kNone;
  if (EqualIgnoringASCIICase(aria_sort, "ascending"))
    return ax::mojom::blink::SortDirection::kAscending;
  if (EqualIgnoringASCIICase(aria_sort, "descending"))
    return ax::mojom::blink::SortDirection::kDescending;

  // Technically, illegal values should be exposed as is, but this does
  // not seem to be worth the implementation effort at this time.
  return ax::mojom::blink::SortDirection::kOther;
}

AXObject* AXLayoutObject::CellForColumnAndRow(unsigned target_column_index,
                                              unsigned target_row_index) const {
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || !layout_object->IsTable()) {
    return AXNodeObject::CellForColumnAndRow(target_column_index,
                                             target_row_index);
  }

  LayoutNGTableInterface* table =
      ToInterface<LayoutNGTableInterface>(layout_object);
  table->RecalcSectionsIfNeeded();

  LayoutNGTableSectionInterface* table_section = table->TopSectionInterface();
  if (!table_section) {
    return AXNodeObject::CellForColumnAndRow(target_column_index,
                                             target_row_index);
  }

  unsigned row_offset = 0;
  while (table_section) {
    // Iterate backwards through the rows in case the desired cell has a rowspan
    // and exists in a previous row.
    for (LayoutNGTableRowInterface* row = table_section->LastRowInterface();
         row; row = row->PreviousRowInterface()) {
      unsigned row_index = row->RowIndex() + row_offset;
      for (LayoutNGTableCellInterface* cell = row->LastCellInterface(); cell;
           cell = cell->PreviousCellInterface()) {
        unsigned absolute_first_col = cell->AbsoluteColumnIndex();
        unsigned absolute_last_col = absolute_first_col + cell->ColSpan() - 1;
        unsigned effective_first_col =
            cell->TableInterface()->AbsoluteColumnToEffectiveColumn(
                absolute_first_col);
        unsigned effective_last_col =
            cell->TableInterface()->AbsoluteColumnToEffectiveColumn(
                absolute_last_col);
        unsigned row_span = cell->ResolvedRowSpan();
        if (target_column_index >= effective_first_col &&
            target_column_index <= effective_last_col &&
            target_row_index >= row_index &&
            target_row_index < row_index + row_span) {
          return AXObjectCache().GetOrCreate(cell->ToMutableLayoutObject());
        }
      }
    }

    row_offset += table_section->NumRows();
    table_section =
        table->SectionBelowInterface(table_section, kSkipEmptySections);
  }

  return nullptr;
}

bool AXLayoutObject::FindAllTableCellsWithRole(ax::mojom::blink::Role role,
                                               AXObjectVector& cells) const {
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || !layout_object->IsTable())
    return false;

  LayoutNGTableInterface* table =
      ToInterface<LayoutNGTableInterface>(layout_object);
  table->RecalcSectionsIfNeeded();

  LayoutNGTableSectionInterface* table_section = table->TopSectionInterface();
  if (!table_section)
    return true;

  while (table_section) {
    for (LayoutNGTableRowInterface* row = table_section->FirstRowInterface();
         row; row = row->NextRowInterface()) {
      for (LayoutNGTableCellInterface* cell = row->FirstCellInterface(); cell;
           cell = cell->NextCellInterface()) {
        AXObject* ax_cell =
            AXObjectCache().GetOrCreate(cell->ToMutableLayoutObject());
        if (ax_cell && ax_cell->RoleValue() == role)
          cells.push_back(ax_cell);
      }
    }

    table_section =
        table->SectionBelowInterface(table_section, kSkipEmptySections);
  }

  return true;
}

void AXLayoutObject::ColumnHeaders(AXObjectVector& headers) const {
  if (!FindAllTableCellsWithRole(ax::mojom::blink::Role::kColumnHeader,
                                 headers)) {
    AXNodeObject::ColumnHeaders(headers);
  }
}

void AXLayoutObject::RowHeaders(AXObjectVector& headers) const {
  if (!FindAllTableCellsWithRole(ax::mojom::blink::Role::kRowHeader, headers))
    AXNodeObject::RowHeaders(headers);
}

AXObject* AXLayoutObject::HeaderObject() const {
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || !layout_object->IsTableRow())
    return nullptr;

  LayoutNGTableRowInterface* row =
      ToInterface<LayoutNGTableRowInterface>(layout_object);
  for (LayoutNGTableCellInterface* cell = row->FirstCellInterface(); cell;
       cell = cell->NextCellInterface()) {
    AXObject* ax_cell =
        cell ? AXObjectCache().GetOrCreate(cell->ToMutableLayoutObject())
             : nullptr;
    if (ax_cell && ax_cell->RoleValue() == ax::mojom::blink::Role::kRowHeader)
      return ax_cell;
  }

  return nullptr;
}

//
// Private.
//

bool AXLayoutObject::IsTabItemSelected() const {
  if (!IsTabItem() || !GetLayoutObject())
    return false;

  Node* node = GetNode();
  if (!node || !node->IsElementNode())
    return false;

  // The ARIA spec says a tab item can also be selected if it is aria-labeled by
  // a tabpanel that has keyboard focus inside of it, or if a tabpanel in its
  // aria-controls list has KB focus inside of it.
  AXObject* focused_element = AXObjectCache().FocusedObject();
  if (!focused_element)
    return false;

  HeapVector<Member<Element>> elements;
  if (!HasAOMPropertyOrARIAAttribute(AOMRelationListProperty::kControls,
                                     elements))
    return false;

  for (const auto& element : elements) {
    AXObject* tab_panel = AXObjectCache().GetOrCreate(element);

    // A tab item should only control tab panels.
    if (!tab_panel ||
        tab_panel->RoleValue() != ax::mojom::blink::Role::kTabPanel) {
      continue;
    }

    AXObject* check_focus_element = focused_element;
    // Check if the focused element is a descendant of the element controlled by
    // the tab item.
    while (check_focus_element) {
      if (tab_panel == check_focus_element)
        return true;
      check_focus_element = check_focus_element->ParentObject();
    }
  }

  return false;
}

AXObject* AXLayoutObject::AccessibilityImageMapHitTest(
    HTMLAreaElement* area,
    const IntPoint& point) const {
  if (!area)
    return nullptr;

  AXObject* parent = AXObjectCache().GetOrCreate(area->ImageElement());
  if (!parent)
    return nullptr;

  for (const auto& child : parent->ChildrenIncludingIgnored()) {
    if (child->GetBoundsInFrameCoordinates().Contains(point))
      return child.Get();
  }

  return nullptr;
}

void AXLayoutObject::DetachRemoteSVGRoot() {
  if (AXSVGRoot* root = RemoteSVGRootElement())
    root->SetParent(nullptr);
}

AXObject* AXLayoutObject::RemoteSVGElementHitTest(const IntPoint& point) const {
  AXObject* remote = RemoteSVGRootElement();
  if (!remote)
    return nullptr;

  IntSize offset =
      point - RoundedIntPoint(GetBoundsInFrameCoordinates().Location());
  return remote->AccessibilityHitTest(IntPoint(offset));
}

// The boundingBox for elements within the remote SVG element needs to be offset
// by its position within the parent page, otherwise they are in relative
// coordinates only.
void AXLayoutObject::OffsetBoundingBoxForRemoteSVGElement(
    LayoutRect& rect) const {
  for (AXObject* parent = ParentObject(); parent;
       parent = parent->ParentObject()) {
    if (parent->IsAXSVGRoot()) {
      rect.MoveBy(
          parent->ParentObject()->GetBoundsInFrameCoordinates().Location());
      break;
    }
  }
}

}  // namespace blink
