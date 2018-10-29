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

#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/iterators/character_iterator.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
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
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_options.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_file_upload_control.h"
#include "third_party/blink/renderer/core/layout/layout_html_canvas.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/layout_list_marker.h"
#include "third_party/blink/renderer/core/layout/layout_menu_list.h"
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/layout_table_section.h"
#include "third_party/blink/renderer/core/layout/layout_text_control.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_marker.h"
#include "third_party/blink/renderer/core/loader/progress_tracker.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment_traversal.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_image_map_link.h"
#include "third_party/blink/renderer/modules/accessibility/ax_inline_text_box.h"
#include "third_party/blink/renderer/modules/accessibility/ax_mock_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_svg_root.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/image_data_buffer.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/transforms/transform_state.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

using blink::WebLocalizedString;

namespace blink {

using namespace HTMLNames;

AXLayoutObject::AXLayoutObject(LayoutObject* layout_object,
                               AXObjectCacheImpl& ax_object_cache)
    : AXNodeObject(layout_object->GetNode(), ax_object_cache),
      layout_object_(layout_object),
      is_autofill_available_(false) {
// TODO(aleventhal) Get correct current state of autofill.
#if DCHECK_IS_ON()
  layout_object_->SetHasAXObject(true);
#endif
}

AXLayoutObject* AXLayoutObject::Create(LayoutObject* layout_object,
                                       AXObjectCacheImpl& ax_object_cache) {
  return new AXLayoutObject(layout_object, ax_object_cache);
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
  if (!box->HasOverflowClip()) {
    // If overflow is visible it is not scrollable.
    return false;
  }
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
  if (IsHTMLImageElement(node))
    return true;
  if (IsHTMLInputElement(node) &&
      ToHTMLInputElement(node)->HasFallbackContent())
    return true;
  return false;
}

ax::mojom::Role AXLayoutObject::NativeRoleIgnoringAria() const {
  Node* node = layout_object_->GetNode();
  LayoutBoxModelObject* css_box = GetLayoutBoxModelObject();

  if ((css_box && css_box->IsListItem()) || IsHTMLLIElement(node))
    return ax::mojom::Role::kListItem;
  if (layout_object_->IsListMarkerIncludingNG())
    return ax::mojom::Role::kListMarker;
  if (layout_object_->IsBR())
    return ax::mojom::Role::kLineBreak;
  if (layout_object_->IsText())
    return ax::mojom::Role::kStaticText;
  if (layout_object_->IsTable() && node) {
    return IsDataTable() ? ax::mojom::Role::kTable
                         : ax::mojom::Role::kLayoutTable;
  }
  if (layout_object_->IsTableRow() && node)
    return DetermineTableRowRole();
  if (layout_object_->IsTableCell() && node)
    return DetermineTableCellRole();
  if (css_box && IsImageOrAltText(css_box, node)) {
    if (node && node->IsLink())
      return ax::mojom::Role::kImageMap;
    if (IsHTMLInputElement(node))
      return ButtonRoleType();
    if (IsSVGImage())
      return ax::mojom::Role::kSvgRoot;

    return ax::mojom::Role::kImage;
  }

  if (IsHTMLCanvasElement(node))
    return ax::mojom::Role::kCanvas;

  if (css_box && css_box->IsLayoutView())
    return ax::mojom::Role::kRootWebArea;

  if (layout_object_->IsSVGImage())
    return ax::mojom::Role::kImage;
  if (layout_object_->IsSVGRoot())
    return ax::mojom::Role::kSvgRoot;

  // Table sections should be ignored.
  if (layout_object_->IsTableSection())
    return ax::mojom::Role::kIgnored;

  if (layout_object_->IsHR())
    return ax::mojom::Role::kSplitter;

  return AXNodeObject::NativeRoleIgnoringAria();
}

ax::mojom::Role AXLayoutObject::DetermineAccessibilityRole() {
  if (!layout_object_)
    return ax::mojom::Role::kUnknown;

  native_role_ = NativeRoleIgnoringAria();

  if ((aria_role_ = DetermineAriaRoleAttribute()) != ax::mojom::Role::kUnknown)
    return aria_role_;

  // Anything that needs to still be exposed but doesn't have a more specific
  // role should be considered a generic container. Examples are
  // layout blocks with no node, in-page link targets, and plain elements
  // such as a <span> with ARIA markup.
  return native_role_ == ax::mojom::Role::kUnknown
             ? ax::mojom::Role::kGenericContainer
             : native_role_;
}

Node* AXLayoutObject::GetNodeOrContainingBlockNode() const {
  if (IsDetached())
    return nullptr;
  if (GetLayoutObject()->IsAnonymousBlock() &&
      GetLayoutObject()->ContainingBlock()) {
    return GetLayoutObject()->ContainingBlock()->GetNode();
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

  // TODO(accessibility) pursue standards track so that aria-goog-editable
  // becomes aria-editable. At that time, create ariaEditableAttr in
  // html_element.cc. The current version of the editable attribute does not
  // inherit, in order to match the automatic Gecko implementation, but
  // hopefully the standardized version will, in which case a more performant
  // implementation will be required, e.g. cache it or only expose on ancestor,
  // having browser-side propagate it.
  const Element* elem = node->IsElementNode()
                            ? ToElement(node)
                            : FlatTreeTraversal::ParentElement(*node);
  if (elem && elem->hasAttribute("aria-goog-editable")) {
    auto editable = elem->getAttribute("aria-goog-editable");
    return !EqualIgnoringASCIICase("false", editable);
  }

  if (GetLayoutObject()->IsTextControl())
    return true;

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
bool AXLayoutObject::IsRichlyEditable() const {
  if (IsDetached())
    return false;

  const Node* node = GetNodeOrContainingBlockNode();
  if (!node)
    return false;

  // TODO(accessibility) pursue standards track so that aria-goog-editable
  // becomes aria-editable. At that time, create ariaEditableAttr in
  // html_element.cc. The current version of the editable attribute does not
  // inherit, in order to match the automatic Gecko implementation, but
  // hopefully the standardized version will, in which case a more performant
  // implementation will be required, e.g. cache it or only expose on ancestor,
  // having browser-side propagate it.
  const Element* elem = node->IsElementNode()
                            ? ToElement(node)
                            : FlatTreeTraversal::ParentElement(*node);
  if (elem && elem->hasAttribute("aria-goog-editable")) {
    auto editable = elem->getAttribute("aria-goog-editable");
    return !EqualIgnoringASCIICase("false", editable);
  }

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

bool AXLayoutObject::IsLinked() const {
  if (!IsLinkable(*this))
    return false;

  if (auto* anchor = ToHTMLAnchorElementOrNull(AnchorElement()))
    return !anchor->Href().IsEmpty();
  return false;
}

bool AXLayoutObject::IsLoaded() const {
  return !layout_object_->GetDocument().Parser();
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

  Element* focused_element = GetDocument()->FocusedElement();
  if (!focused_element)
    return false;
  AXObject* focused_object = AXObjectCache().GetOrCreate(focused_element);
  if (!focused_object || !focused_object->IsAXLayoutObject())
    return false;

  // A web area is represented by the Document node in the DOM tree, which isn't
  // focusable.  Check instead if the frame's selection controller is focused
  if (focused_object == this ||
      (RoleValue() == ax::mojom::Role::kRootWebArea &&
       GetDocument()->GetFrame()->Selection().FrameIsFocusedAndActive()))
    return true;

  return false;
}

AccessibilitySelectedState AXLayoutObject::IsSelected() const {
  if (!GetLayoutObject() || !GetNode() || !CanSetSelectedAttribute())
    return kSelectedStateUndefined;

  // aria-selected overrides automatic behaviors
  bool is_selected;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kSelected, is_selected))
    return is_selected ? kSelectedStateTrue : kSelectedStateFalse;

  // Tab item with focus in the associated tab
  if (IsTabItem() && IsTabItemSelected())
    return kSelectedStateTrue;

  // Selection follows focus, but ONLY in single selection containers,
  // and only if aria-selected was not present to override
  return IsSelectedFromFocus() ? kSelectedStateTrue : kSelectedStateFalse;
}

// In single selection containers, selection follows focus unless aria_selected
// is set to false.
bool AXLayoutObject::IsSelectedFromFocus() const {
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

//
// Whether objects are ignored, i.e. not included in the tree.
//

AXObjectInclusion AXLayoutObject::DefaultObjectInclusion(
    IgnoredReasons* ignored_reasons) const {
  // The following cases can apply to any element that's a subclass of
  // AXLayoutObject.

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

bool HasAriaAttribute(Element* element) {
  if (!element)
    return false;

  AttributeCollection attributes = element->AttributesWithoutUpdate();
  for (const Attribute& attr : attributes) {
    // Attributes cache their uppercase names.
    if (attr.GetName().LocalNameUpper().StartsWith("ARIA-"))
      return true;
  }

  return false;
}

static bool HasLineBox(const LayoutBlockFlow& block_flow) {
  if (!block_flow.IsLayoutNGMixin())
    return block_flow.FirstLineBox();
  if (block_flow.HasNGInlineNodeData())
    return !block_flow.GetNGInlineNodeData()->items.IsEmpty();
  // TODO(layout-dev): We should call this function after layout completion.
  return false;
}

// Is this the anonymous placeholder for a text control?
bool AXLayoutObject::IsPlaceholder() const {
  AXObject* parent_object = ParentObject();
  if (!parent_object)
    return false;

  LayoutObject* parent_layout_object = parent_object->GetLayoutObject();
  if (!parent_layout_object || !parent_layout_object->IsTextControl())
    return false;

  LayoutTextControl* layout_text_control =
      ToLayoutTextControl(parent_layout_object);
  DCHECK(layout_text_control);

  TextControlElement* text_control_element =
      layout_text_control->GetTextControlElement();
  if (!text_control_element)
    return false;

  HTMLElement* placeholder_element = text_control_element->PlaceholderElement();

  return GetElement() == static_cast<Element*>(placeholder_element);
}

bool AXLayoutObject::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignored_reasons) const {
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif

  if (!layout_object_)
    return true;

  // Check first if any of the common reasons cause this element to be ignored.
  // Then process other use cases that need to be applied to all the various
  // roles that AXLayoutObjects take on.
  AXObjectInclusion decision = DefaultObjectInclusion(ignored_reasons);
  if (decision == kIncludeObject)
    return false;
  if (decision == kIgnoreObject)
    return true;

  if (layout_object_->IsAnonymousBlock() && !IsEditable())
    return true;

  // Ignore continuations, since those are essentially duplicate copies
  // of inline nodes with blocks inside.
  if (layout_object_->IsElementContinuation())
    return true;

  // If this element is within a parent that cannot have children, it should not
  // be exposed.
  if (IsDescendantOfLeafNode()) {
    if (ignored_reasons)
      ignored_reasons->push_back(
          IgnoredReason(kAXAncestorIsLeafNode, LeafNodeAncestor()));
    return true;
  }

  if (RoleValue() == ax::mojom::Role::kIgnored) {
    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXUninteresting));
    return true;
  }

  if (HasInheritedPresentationalRole()) {
    if (ignored_reasons) {
      const AXObject* inherits_from = InheritsPresentationalRoleFrom();
      if (inherits_from == this)
        ignored_reasons->push_back(IgnoredReason(kAXPresentational));
      else
        ignored_reasons->push_back(
            IgnoredReason(kAXInheritsPresentation, inherits_from));
    }
    return true;
  }

  if (IsTableLikeRole() || IsTableRowLikeRole() || IsTableCellLikeRole())
    return false;

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

  // Find out if this element is inside of a label element.  If so, it may be
  // ignored because it's the label for a checkbox or radio button.
  AXObject* control_object = CorrespondingControlForLabelElement();
  if (control_object && control_object->IsCheckboxOrRadio() &&
      control_object->NameFromLabelElement()) {
    if (ignored_reasons) {
      HTMLLabelElement* label = LabelElementContainer();
      if (label && label != GetNode()) {
        AXObject* label_ax_object = AXObjectCache().GetOrCreate(label);
        ignored_reasons->push_back(
            IgnoredReason(kAXLabelContainer, label_ax_object));
      }

      ignored_reasons->push_back(IgnoredReason(kAXLabelFor, control_object));
    }
    return true;
  }

  if (layout_object_->IsBR())
    return false;

  if (CanSetFocusAttribute() && GetNode() && !IsHTMLBodyElement(GetNode()))
    return false;

  if (IsLink())
    return false;

  if (IsInPageLinkTarget())
    return false;

  // A click handler might be placed on an otherwise ignored non-empty block
  // element, e.g. a div. We shouldn't ignore such elements because if an AT
  // sees the |ax::mojom::DefaultActionVerb::kClickAncestor|, it will look for
  // the clickable ancestor and it expects to find one.
  if (IsClickable())
    return false;

  if (layout_object_->IsText()) {
    if (CanIgnoreTextAsEmpty()) {
      if (ignored_reasons)
        ignored_reasons->push_back(IgnoredReason(kAXEmptyText));
      return true;
    }
    return false;
  }

  if (IsHeading())
    return false;

  if (IsLandmarkRelated())
    return false;

  // Header and footer tags may also be exposed as landmark roles but not
  // always.
  if (GetNode() &&
      (GetNode()->HasTagName(headerTag) || GetNode()->HasTagName(footerTag)))
    return false;

  // all controls are accessible
  if (IsControl())
    return false;

  if (AriaRoleAttribute() != ax::mojom::Role::kUnknown)
    return false;

  // don't ignore labels, because they serve as TitleUIElements
  Node* node = layout_object_->GetNode();
  if (IsHTMLLabelElement(node))
    return false;

  // Anything that is content editable should not be ignored.
  // However, one cannot just call node->hasEditableStyle() since that will ask
  // if its parents are also editable. Only the top level content editable
  // region should be exposed.
  if (HasContentEditableAttributeSet())
    return false;

  if (RoleValue() == ax::mojom::Role::kAbbr)
    return false;

  // List items play an important role in defining the structure of lists. They
  // should not be ignored.
  if (RoleValue() == ax::mojom::Role::kListItem)
    return false;

  if (RoleValue() == ax::mojom::Role::kBlockquote)
    return false;

  if (RoleValue() == ax::mojom::Role::kDialog)
    return false;

  if (RoleValue() == ax::mojom::Role::kFigcaption)
    return false;

  if (RoleValue() == ax::mojom::Role::kFigure)
    return false;

  if (RoleValue() == ax::mojom::Role::kContentDeletion)
    return false;

  if (RoleValue() == ax::mojom::Role::kContentInsertion)
    return false;

  if (RoleValue() == ax::mojom::Role::kDetails)
    return false;

  if (RoleValue() == ax::mojom::Role::kMark)
    return false;

  if (RoleValue() == ax::mojom::Role::kMath)
    return false;

  if (RoleValue() == ax::mojom::Role::kMeter)
    return false;

  if (RoleValue() == ax::mojom::Role::kRuby)
    return false;

  if (RoleValue() == ax::mojom::Role::kSplitter)
    return false;

  if (RoleValue() == ax::mojom::Role::kTime)
    return false;

  if (RoleValue() == ax::mojom::Role::kProgressIndicator)
    return false;

  // if this element has aria attributes on it, it should not be ignored.
  if (HasGlobalARIAAttribute())
    return false;

  if (IsImage())
    return false;

  if (IsCanvas()) {
    if (CanvasHasFallbackContent())
      return false;

    const auto* canvas = ToLayoutHTMLCanvasOrNull(layout_object_);
    if (canvas &&
        (canvas->Size().Height() <= 1 || canvas->Size().Width() <= 1)) {
      if (ignored_reasons)
        ignored_reasons->push_back(IgnoredReason(kAXProbablyPresentational));
      return true;
    }

    // Otherwise fall through; use presence of help text, title, or description
    // to decide.
  }

  if (IsWebArea() || layout_object_->IsListMarkerIncludingNG())
    return false;

  // Using the title or accessibility description (so we
  // check if there's some kind of accessible name for the element)
  // to decide an element's visibility is not as definitive as
  // previous checks, so this should remain as one of the last.
  //
  // These checks are simplified in the interest of execution speed;
  // for example, any element having an alt attribute will make it
  // not ignored, rather than just images.
  if (HasAriaAttribute(GetElement()) || !GetAttribute(altAttr).IsEmpty() ||
      !GetAttribute(titleAttr).IsEmpty())
    return false;

  // <span> tags are inline tags and not meant to convey information if they
  // have no other ARIA information on them. If we don't ignore them, they may
  // emit signals expected to come from their parent.
  if (IsHTMLSpanElement(node)) {
    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXUninteresting));
    return true;
  }

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
      ParentObject()->GetLayoutObject()->IsTextControl()) {
    return false;
  }

  // Ignore layout objects that are block flows with inline children. These
  // are usually dummy layout objects that pad out the tree, but there are
  // some exceptions below.
  if (layout_object_->IsLayoutBlockFlow() && layout_object_->ChildrenInline() &&
      !CanSetFocusAttribute()) {
    // If the layout object has any plain text in it, that text will be
    // inside a LineBox, so the layout object will have a first LineBox.
    bool has_any_text = HasLineBox(ToLayoutBlockFlow(*layout_object_));

    // Always include interesting-looking objects.
    if (has_any_text || MouseButtonListener())
      return false;

    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXUninteresting));
    return true;
  }

  // By default, objects should be ignored so that the AX hierarchy is not
  // filled with unnecessary items.
  if (ignored_reasons)
    ignored_reasons->push_back(IgnoredReason(kAXUninteresting));
  return true;
}

bool AXLayoutObject::HasAriaCellRole(Element* elem) const {
  DCHECK(elem);
  const AtomicString& aria_role_str = elem->FastGetAttribute(roleAttr);
  if (aria_role_str.IsEmpty())
    return false;

  ax::mojom::Role aria_role = AriaRoleToWebCoreRole(aria_role_str);
  return aria_role == ax::mojom::Role::kCell ||
         aria_role == ax::mojom::Role::kColumnHeader ||
         aria_role == ax::mojom::Role::kRowHeader;
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
  Node* node = layout->GetNode();
  if (node && node->IsElementNode()) {
    Element* elem = ToElement(node);
    if (HasAriaCellRole(elem))
      return true;
  }

  // Test against the appropriate child text node.
  LayoutInline* layout_inline = ToLayoutInline(layout);
  LayoutObject* child =
      is_after ? layout_inline->FirstChild() : layout_inline->LastChild();
  return CanIgnoreSpaceNextTo(child, is_after);
}

bool AXLayoutObject::CanIgnoreTextAsEmpty() const {
  DCHECK(layout_object_->IsText());
  DCHECK(layout_object_->Parent());

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

const AtomicString& AXLayoutObject::AccessKey() const {
  Node* node = layout_object_->GetNode();
  if (!node)
    return g_null_atom;
  if (!node->IsElementNode())
    return g_null_atom;
  return ToElement(node)->getAttribute(accesskeyAttr);
}

RGBA32 AXLayoutObject::ComputeBackgroundColor() const {
  if (!GetLayoutObject())
    return AXNodeObject::BackgroundColor();

  Color blended_color = Color::kTransparent;
  // Color::blend should be called like this: background.blend(foreground).
  for (LayoutObject* layout_object = GetLayoutObject(); layout_object;
       layout_object = layout_object->Parent()) {
    const AXObject* ax_parent = AXObjectCache().GetOrCreate(layout_object);
    if (ax_parent && ax_parent != this) {
      Color parent_color = ax_parent->BackgroundColor();
      blended_color = parent_color.Blend(blended_color);
      return blended_color.Rgb();
    }

    const ComputedStyle* style = layout_object->Style();
    if (!style || !style->HasBackground())
      continue;

    Color current_color =
        style->VisitedDependentColor(GetCSSPropertyBackgroundColor());
    blended_color = current_color.Blend(blended_color);
    // Continue blending until we get no transparency.
    if (!blended_color.HasAlpha())
      break;
  }

  // If we still have some transparency, blend in the document base color.
  if (blended_color.HasAlpha()) {
    LocalFrameView* view = DocumentFrameView();
    if (view) {
      Color document_base_color = view->BaseBackgroundColor();
      blended_color = document_base_color.Blend(blended_color);
    } else {
      // Default to a white background.
      blended_color.BlendWithWhite();
    }
  }

  return blended_color.Rgb();
}

RGBA32 AXLayoutObject::GetColor() const {
  if (!GetLayoutObject() || IsColorWell())
    return AXNodeObject::GetColor();

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style)
    return AXNodeObject::GetColor();

  Color color = style->VisitedDependentColor(GetCSSPropertyColor());
  return color.Rgb();
}

AtomicString AXLayoutObject::FontFamily() const {
  if (!GetLayoutObject())
    return AXNodeObject::FontFamily();

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style)
    return AXNodeObject::FontFamily();

  FontDescription& font_description =
      const_cast<FontDescription&>(style->GetFontDescription());
  return font_description.FirstFamily().Family();
}

// Font size is in pixels.
float AXLayoutObject::FontSize() const {
  if (!GetLayoutObject())
    return AXNodeObject::FontSize();

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style)
    return AXNodeObject::FontSize();

  return style->ComputedFontSize();
}

String AXLayoutObject::ImageDataUrl(const IntSize& max_size) const {
  Node* node = GetNode();
  if (!node)
    return String();

  ImageBitmapOptions options;
  ImageBitmap* image_bitmap = nullptr;
  Document* document = &node->GetDocument();
  if (auto* image = ToHTMLImageElementOrNull(node)) {
    image_bitmap = ImageBitmap::Create(image, base::Optional<IntRect>(),
                                       document, options);
  } else if (auto* canvas = ToHTMLCanvasElementOrNull(node)) {
    image_bitmap =
        ImageBitmap::Create(canvas, base::Optional<IntRect>(), options);
  } else if (auto* video = ToHTMLVideoElementOrNull(node)) {
    image_bitmap = ImageBitmap::Create(video, base::Optional<IntRect>(),
                                       document, options);
  }
  if (!image_bitmap)
    return String();

  scoped_refptr<StaticBitmapImage> bitmap_image = image_bitmap->BitmapImage();
  if (!bitmap_image)
    return String();

  sk_sp<SkImage> image = bitmap_image->PaintImageForCurrentFrame().GetSkImage();
  if (!image || image->width() <= 0 || image->height() <= 0)
    return String();

  // Determine the width and height of the output image, using a proportional
  // scale factor such that it's no larger than |maxSize|, if |maxSize| is not
  // empty. It only resizes the image to be smaller (if necessary), not
  // larger.
  float x_scale =
      max_size.Width() ? max_size.Width() * 1.0 / image->width() : 1.0;
  float y_scale =
      max_size.Height() ? max_size.Height() * 1.0 / image->height() : 1.0;
  float scale = std::min(x_scale, y_scale);
  if (scale >= 1.0)
    scale = 1.0;
  int width = std::round(image->width() * scale);
  int height = std::round(image->height() * scale);

  // Draw the scaled image into a bitmap in native format.
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32(width, height, kPremul_SkAlphaType));
  SkCanvas canvas(bitmap);
  canvas.clear(SK_ColorTRANSPARENT);
  canvas.drawImageRect(image, SkRect::MakeIWH(width, height), nullptr);

  // Copy the bits into a buffer in RGBA_8888 unpremultiplied format
  // for encoding.
  SkImageInfo info = SkImageInfo::Make(width, height, kRGBA_8888_SkColorType,
                                       kUnpremul_SkAlphaType);
  size_t row_bytes = info.minRowBytes();
  Vector<char> pixel_storage(
      SafeCast<wtf_size_t>(info.computeByteSize(row_bytes)));
  SkPixmap pixmap(info, pixel_storage.data(), row_bytes);
  if (!SkImage::MakeFromBitmap(bitmap)->readPixels(pixmap, 0, 0))
    return String();

  // Encode as a PNG and return as a data url.
  std::unique_ptr<ImageDataBuffer> buffer = ImageDataBuffer::Create(pixmap);

  if (!buffer)
    return String();

  return buffer->ToDataURL(kMimeTypePng, 1.0);
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

ax::mojom::TextDirection AXLayoutObject::GetTextDirection() const {
  if (!GetLayoutObject())
    return AXNodeObject::GetTextDirection();

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style)
    return AXNodeObject::GetTextDirection();

  if (style->IsHorizontalWritingMode()) {
    switch (style->Direction()) {
      case TextDirection::kLtr:
        return ax::mojom::TextDirection::kLtr;
      case TextDirection::kRtl:
        return ax::mojom::TextDirection::kRtl;
    }
  } else {
    switch (style->Direction()) {
      case TextDirection::kLtr:
        return ax::mojom::TextDirection::kTtb;
      case TextDirection::kRtl:
        return ax::mojom::TextDirection::kBtt;
    }
  }

  return AXNodeObject::GetTextDirection();
}

ax::mojom::TextPosition AXLayoutObject::GetTextPosition() const {
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
      return ax::mojom::TextPosition::kSubscript;
    case EVerticalAlign::kSuper:
      return ax::mojom::TextPosition::kSuperscript;
  }
}

int AXLayoutObject::TextLength() const {
  if (!IsTextControl())
    return -1;

  return GetText().length();
}

TextStyle AXLayoutObject::GetTextStyle() const {
  if (!GetLayoutObject())
    return AXNodeObject::GetTextStyle();

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style)
    return AXNodeObject::GetTextStyle();

  unsigned text_style = kTextStyleNone;
  if (style->GetFontWeight() == BoldWeightValue())
    text_style |= kTextStyleBold;
  if (style->GetFontDescription().Style() == ItalicSlopeValue())
    text_style |= kTextStyleItalic;
  if (style->GetTextDecoration() == TextDecoration::kUnderline)
    text_style |= kTextStyleUnderline;
  if (style->GetTextDecoration() == TextDecoration::kLineThrough)
    text_style |= kTextStyleLineThrough;

  return static_cast<TextStyle>(text_style);
}

KURL AXLayoutObject::Url() const {
  if (IsAnchor() && IsHTMLAnchorElement(layout_object_->GetNode())) {
    if (HTMLAnchorElement* anchor = ToHTMLAnchorElement(AnchorElement()))
      return anchor->Href();
  }

  if (IsWebArea())
    return layout_object_->GetDocument().Url();

  if (IsImage() && IsHTMLImageElement(layout_object_->GetNode()))
    return ToHTMLImageElement(*layout_object_->GetNode()).Src();

  if (IsInputImage())
    return ToHTMLInputElement(layout_object_->GetNode())->Src();

  return KURL();
}

//
// Inline text boxes.
//

void AXLayoutObject::LoadInlineTextBoxes() {
  if (!GetLayoutObject())
    return;

  if (GetLayoutObject()->IsText()) {
    ClearChildren();
    AddInlineTextBoxChildren(true);
    return;
  }

  for (const auto& child : children_) {
    child->LoadInlineTextBoxes();
  }
}

static bool ShouldUseLayoutNG(const LayoutObject& layout_object) {
  return (layout_object.IsLayoutInline() || layout_object.IsText()) &&
         layout_object.ContainingNGBlockFlow();
}

// Note: |NextOnLineInternalNG()| returns null when fragment for |layout_object|
// is culled as legacy layout version since |LayoutInline::LastLineBox()|
// returns null when it is culled.
// See also |PreviousOnLineInternalNG()| which is identical except for using
// "next" and |back()| instead of "previous" and |front()|.
static AXObject* NextOnLineInternalNG(const AXObject& ax_object) {
  DCHECK(ax_object.GetLayoutObject());
  const LayoutObject& layout_object = *ax_object.GetLayoutObject();
  DCHECK(!layout_object.IsListMarkerIncludingNG()) << layout_object;
  DCHECK(ShouldUseLayoutNG(layout_object)) << layout_object;
  const auto fragments = NGPaintFragment::InlineFragmentsFor(&layout_object);
  if (fragments.IsEmpty() || fragments.IsInLayoutNGInlineFormattingContext())
    return nullptr;
  for (NGPaintFragmentTraversalContext runner =
           NGPaintFragmentTraversal::NextInlineLeafOf(
               NGPaintFragmentTraversalContext::Create(&fragments.back()));
       !runner.IsNull();
       runner = NGPaintFragmentTraversal::NextInlineLeafOf(runner)) {
    LayoutObject* layout_object = runner.GetFragment()->GetLayoutObject();
    if (AXObject* result = ax_object.AXObjectCache().GetOrCreate(layout_object))
      return result;
  }
  if (!ax_object.ParentObject())
    return nullptr;
  // Returns next object of parent, since next of |ax_object| isn't appeared on
  // line.
  return ax_object.ParentObject()->NextOnLine();
}

AXObject* AXLayoutObject::NextOnLine() const {
  if (!GetLayoutObject())
    return nullptr;

  AXObject* result = nullptr;
  if (GetLayoutObject()->IsListMarkerIncludingNG()) {
    AXObject* next_sibling = RawNextSibling();
    if (!next_sibling || !next_sibling->Children().size())
      return nullptr;
    result = next_sibling->Children()[0].Get();
  } else if (ShouldUseLayoutNG(*GetLayoutObject())) {
    result = NextOnLineInternalNG(*this);
  } else {
    InlineBox* inline_box = nullptr;
    if (GetLayoutObject()->IsLayoutInline()) {
      inline_box = ToLayoutInline(GetLayoutObject())->LastLineBox();
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

    if (!result && ParentObject())
      result = ParentObject()->NextOnLine();
  }

  // For consistency between the forward and backward directions, try to always
  // return leaf nodes.
  while (result && result->Children().size())
    result = result->Children()[0].Get();

  return result;
}

// Note: |PreviousOnLineInlineNG()| returns null when fragment for
// |layout_object| is culled as legacy layout version since
// |LayoutInline::FirstLineBox()| returns null when it is culled. See also
// |NextOnLineNG()| which is identical except for using "previous" and |front()|
// instead of "next" and |back()|.
static AXObject* PreviousOnLineInlineNG(const AXObject& ax_object) {
  DCHECK(ax_object.GetLayoutObject());
  const LayoutObject& layout_object = *ax_object.GetLayoutObject();
  DCHECK(!layout_object.IsListMarkerIncludingNG()) << layout_object;
  DCHECK(ShouldUseLayoutNG(layout_object)) << layout_object;
  const auto fragments = NGPaintFragment::InlineFragmentsFor(&layout_object);
  if (fragments.IsEmpty() || fragments.IsInLayoutNGInlineFormattingContext())
    return nullptr;
  for (NGPaintFragmentTraversalContext runner =
           NGPaintFragmentTraversal::PreviousInlineLeafOf(
               NGPaintFragmentTraversalContext::Create(&fragments.front()));
       !runner.IsNull();
       runner = NGPaintFragmentTraversal::PreviousInlineLeafOf(runner)) {
    LayoutObject* layout_object = runner.GetFragment()->GetLayoutObject();
    if (AXObject* result = ax_object.AXObjectCache().GetOrCreate(layout_object))
      return result;
  }
  if (!ax_object.ParentObject())
    return nullptr;
  // Returns previous object of parent, since next of |ax_object| isn't appeared
  // on line.
  return ax_object.ParentObject()->PreviousOnLine();
}

AXObject* AXLayoutObject::PreviousOnLine() const {
  if (!GetLayoutObject())
    return nullptr;

  AXObject* result = nullptr;
  if (ShouldUseLayoutNG(*GetLayoutObject())) {
    result = PreviousOnLineInlineNG(*this);
  } else {
    InlineBox* inline_box = nullptr;
    if (GetLayoutObject()->IsLayoutInline()) {
      inline_box = ToLayoutInline(GetLayoutObject())->FirstLineBox();
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

    if (!result && ParentObject())
      result = ParentObject()->PreviousOnLine();
  }

  // For consistency between the forward and backward directions, try to always
  // return leaf nodes.
  while (result && result->Children().size())
    result = result->Children()[result->Children().size() - 1].Get();

  return result;
}

//
// Properties of interactive elements.
//

String AXLayoutObject::StringValue() const {
  if (!layout_object_)
    return String();

  LayoutBoxModelObject* css_box = GetLayoutBoxModelObject();

  if (css_box && css_box->IsMenuList()) {
    // LayoutMenuList will go straight to the text() of its selected item.
    // This has to be overridden in the case where the selected item has an ARIA
    // label.
    HTMLSelectElement* select_element =
        ToHTMLSelectElement(layout_object_->GetNode());
    int selected_index = select_element->selectedIndex();
    const HeapVector<Member<HTMLElement>>& list_items =
        select_element->GetListItems();
    if (selected_index >= 0 &&
        static_cast<size_t>(selected_index) < list_items.size()) {
      const AtomicString& overridden_description =
          list_items[selected_index]->FastGetAttribute(aria_labelAttr);
      if (!overridden_description.IsNull())
        return overridden_description;
    }
    return ToLayoutMenuList(layout_object_)->GetText();
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
  // https://html.spec.whatwg.org/multipage/forms.html#dom-input-value
  if (const auto* input = ToHTMLInputElementOrNull(GetNode())) {
    if (input->type() != InputTypeNames::button &&
        input->type() != InputTypeNames::checkbox &&
        input->type() != InputTypeNames::image &&
        input->type() != InputTypeNames::radio &&
        input->type() != InputTypeNames::reset &&
        input->type() != InputTypeNames::submit) {
      return input->value();
    }
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
                                       ax::mojom::NameFrom& name_from,
                                       AXRelatedObjectVector* related_objects,
                                       NameSources* name_sources) const {
  if (layout_object_) {
    String text_alternative;
    bool found_text_alternative = false;

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
    } else if (layout_object_->IsListMarker() && !recursive) {
      text_alternative = ToLayoutListMarker(layout_object_)->TextAlternative();
      found_text_alternative = true;
    } else if (layout_object_->IsLayoutNGListMarker() && !recursive) {
      text_alternative =
          ToLayoutNGListMarker(layout_object_)->TextAlternative();
      found_text_alternative = true;
    }

    if (found_text_alternative) {
      name_from = ax::mojom::NameFrom::kContents;
      if (name_sources) {
        name_sources->push_back(NameSource(false));
        name_sources->back().type = name_from;
        name_sources->back().text = text_alternative;
      }
      return text_alternative;
    }
  }

  return AXNodeObject::TextAlternative(recursive, in_aria_labelled_by_traversal,
                                       visited, name_from, related_objects,
                                       name_sources);
}

//
// ARIA attributes.
//

void AXLayoutObject::AriaOwnsElements(AXObjectVector& owns) const {
  AccessibilityChildrenFromAOMProperty(AOMRelationListProperty::kOwns, owns);
}

void AXLayoutObject::AriaDescribedbyElements(
    AXObjectVector& describedby) const {
  AccessibilityChildrenFromAOMProperty(AOMRelationListProperty::kDescribedBy,
                                       describedby);
}

ax::mojom::HasPopup AXLayoutObject::HasPopup() const {
  const AtomicString& has_popup =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kHasPopUp);
  if (!has_popup.IsNull()) {
    if (EqualIgnoringASCIICase(has_popup, "false"))
      return ax::mojom::HasPopup::kFalse;

    if (EqualIgnoringASCIICase(has_popup, "listbox"))
      return ax::mojom::HasPopup::kListbox;

    if (EqualIgnoringASCIICase(has_popup, "tree"))
      return ax::mojom::HasPopup::kTree;

    if (EqualIgnoringASCIICase(has_popup, "grid"))
      return ax::mojom::HasPopup::kGrid;

    if (EqualIgnoringASCIICase(has_popup, "dialog"))
      return ax::mojom::HasPopup::kDialog;

    // To provide backward compatibility with ARIA 1.0 content,
    // user agents MUST treat an aria-haspopup value of true
    // as equivalent to a value of menu.
    // And unknown value also return menu too.
    if (EqualIgnoringASCIICase(has_popup, "true") ||
        EqualIgnoringASCIICase(has_popup, "menu") || !has_popup.IsEmpty())
      return ax::mojom::HasPopup::kMenu;
  }

  // ARIA 1.1 default value of haspopup for combobox is "listbox".
  if (RoleValue() == ax::mojom::Role::kComboBoxMenuButton ||
      RoleValue() == ax::mojom::Role::kTextFieldWithComboBox)
    return ax::mojom::HasPopup::kListbox;

  return AXObject::HasPopup();
}

// TODO : Aria-dropeffect and aria-grabbed are deprecated in aria 1.1
// Also those properties are expected to be replaced by a new feature in
// a future version of WAI-ARIA. After that we will re-implement them
// following new spec.
bool AXLayoutObject::SupportsARIADragging() const {
  const AtomicString& grabbed = GetAttribute(aria_grabbedAttr);
  return EqualIgnoringASCIICase(grabbed, "true") ||
         EqualIgnoringASCIICase(grabbed, "false");
}

bool AXLayoutObject::SupportsARIADropping() const {
  const AtomicString& drop_effect = GetAttribute(aria_dropeffectAttr);
  return !drop_effect.IsEmpty();
}

bool AXLayoutObject::SupportsARIAFlowTo() const {
  return !GetAttribute(aria_flowtoAttr).IsEmpty();
}

bool AXLayoutObject::SupportsARIAOwns() const {
  if (!layout_object_)
    return false;
  const AtomicString& aria_owns = GetAttribute(aria_ownsAttr);

  return !aria_owns.IsEmpty();
}

//
// ARIA live-region features.
//

const AtomicString& AXLayoutObject::LiveRegionStatus() const {
  DEFINE_STATIC_LOCAL(const AtomicString, live_region_status_assertive,
                      ("assertive"));
  DEFINE_STATIC_LOCAL(const AtomicString, live_region_status_polite,
                      ("polite"));
  DEFINE_STATIC_LOCAL(const AtomicString, live_region_status_off, ("off"));

  const AtomicString& live_region_status =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kLive);
  // These roles have implicit live region status.
  if (live_region_status.IsEmpty()) {
    switch (RoleValue()) {
      case ax::mojom::Role::kAlert:
        return live_region_status_assertive;
      case ax::mojom::Role::kLog:
      case ax::mojom::Role::kStatus:
        return live_region_status_polite;
      case ax::mojom::Role::kTimer:
      case ax::mojom::Role::kMarquee:
        return live_region_status_off;
      default:
        break;
    }
  }

  return live_region_status;
}

const AtomicString& AXLayoutObject::LiveRegionRelevant() const {
  DEFINE_STATIC_LOCAL(const AtomicString, default_live_region_relevant,
                      ("additions text"));
  const AtomicString& relevant =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kRelevant);

  // Default aria-relevant = "additions text".
  if (relevant.IsEmpty())
    return default_live_region_relevant;

  return relevant;
}

//
// Hit testing.
//

AXObject* AXLayoutObject::AccessibilityHitTest(const IntPoint& point) const {
  if (!layout_object_ || !layout_object_->HasLayer() ||
      !layout_object_->IsBox())
    return nullptr;

  auto* frame_view = DocumentFrameView();
  if (!frame_view || !frame_view->UpdateAllLifecyclePhasesExceptPaint())
    return nullptr;

  PaintLayer* layer = ToLayoutBox(layout_object_)->Layer();

  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location(point);
  HitTestResult hit_test_result = HitTestResult(request, location);
  layer->HitTest(location, hit_test_result,
                 LayoutRect(LayoutRect::InfiniteIntRect()));

  Node* node = hit_test_result.InnerNode();
  if (!node)
    return nullptr;

  if (auto* area = ToHTMLAreaElementOrNull(node))
    return AccessibilityImageMapHitTest(area, point);

  if (auto* option = ToHTMLOptionElementOrNull(node)) {
    node = option->OwnerSelectElement();
    if (!node)
      return nullptr;
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
    if (result->IsAXLayoutObject()) {
      AXObject* control_object =
          ToAXLayoutObject(result)->CorrespondingControlForLabelElement();
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
  if (layout_object->IsLayoutBlockFlow() &&
      ToLayoutBlockFlow(layout_object)->InlineElementContinuation()) {
    LayoutInline* result = ToLayoutInline(ToLayoutBlockFlow(layout_object)
                                              ->InlineElementContinuation()
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

  if (layout_object->IsAnonymousBlock() && layout_object->IsLayoutBlockFlow() &&
      ToLayoutBlockFlow(layout_object)->Continuation())
    return true;

  return false;
}

// See LAYOUT TREE WALKING ALGORITHM, above, for details.
// Return the continuation of this layout object, or nullptr if it doesn't
// have one.
LayoutObject* GetContinuation(LayoutObject* layout_object) {
  if (layout_object->IsLayoutInline())
    return ToLayoutInline(layout_object)->Continuation();

  if (layout_object->IsLayoutBlockFlow())
    return ToLayoutBlockFlow(layout_object)->Continuation();

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
    LayoutTable* table = ToLayoutTable(layout_object_);
    table->RecalcSectionsIfNeeded();
    return AXObjectCache().GetOrCreate(table->TopSection());
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
    LayoutTableSection* section = ToLayoutTableSection(layout_object_);
    return AXObjectCache().GetOrCreate(
        section->Table()->SectionBelow(section, kSkipEmptySections));
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

  if (AriaRoleAttribute() == ax::mojom::Role::kMenuBar)
    return AXObjectCache().GetOrCreate(layout_object_->Parent());

  // menuButton and its corresponding menu are DOM siblings, but Accessibility
  // needs them to be parent/child.
  if (AriaRoleAttribute() == ax::mojom::Role::kMenu) {
    AXObject* parent = MenuButtonForMenu();
    if (parent)
      return parent;
  }

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

  if (AriaRoleAttribute() == ax::mojom::Role::kMenuBar)
    return AXObjectCache().Get(layout_object_->Parent());

  // menuButton and its corresponding menu are DOM siblings, but Accessibility
  // needs them to be parent/child.
  if (AriaRoleAttribute() == ax::mojom::Role::kMenu) {
    AXObject* parent = MenuButtonForMenuIfExists();
    if (parent)
      return parent;
  }

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

void AXLayoutObject::AddChildren() {
  if (IsDetached())
    return;

  if (IsHTMLCanvasElement(GetNode()))
    return AXNodeObject::AddChildren();

  // If the need to add more children in addition to existing children arises,
  // childrenChanged should have been called, leaving the object with no
  // children.
  DCHECK(!have_children_);
  have_children_ = true;

  AXObjectVector owned_children;
  ComputeAriaOwnsChildren(owned_children);

  for (AXObject* obj = RawFirstChild(); obj; obj = obj->RawNextSibling()) {
    if (!AXObjectCache().IsAriaOwned(obj)) {
      obj->SetParent(this);
      AddChild(obj);
    }
  }

  AddHiddenChildren();
  AddPopupChildren();
  AddImageMapChildren();
  AddRemoteSVGChildren();
  AddTableChildren();
  AddInlineTextBoxChildren(false);
  AddAccessibleNodeChildren();

  for (const auto& child : children_) {
    if (!child->CachedParentObject())
      child->SetParent(this);
  }

  for (const auto& owned_child : owned_children)
    AddChild(owned_child);
}

bool AXLayoutObject::CanHaveChildren() const {
  if (!layout_object_)
    return false;

  return AXNodeObject::CanHaveChildren();
}

//
// Properties of the object's owning document or page.
//

double AXLayoutObject::EstimatedLoadingProgress() const {
  if (!layout_object_)
    return 0;

  if (IsLoaded())
    return 1.0;

  if (LocalFrame* frame = layout_object_->GetDocument().GetFrame())
    return frame->Loader().Progress().EstimatedProgress();
  return 0;
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
    if (curr_layout_object->IsAnonymousBlock() &&
        curr_layout_object->IsLayoutBlockFlow()) {
      LayoutObject* continuation =
          ToLayoutBlockFlow(curr_layout_object)->Continuation();
      if (continuation)
        return cache.GetOrCreate(continuation)->AnchorElement();
    }
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
    if (IsHTMLAnchorElement(runner) ||
        (runner.GetLayoutObject() &&
         cache.GetOrCreate(runner.GetLayoutObject())->IsAnchor()))
      return ToElement(&runner);
  }

  return nullptr;
}

AtomicString AXLayoutObject::Language() const {
  // Uses the style engine to figure out the object's language.
  // The style engine relies on, for example, the "lang" attribute of the
  // current node and its ancestors, and the document's "content-language"
  // header. See the Language of a Node Spec at
  // https://html.spec.whatwg.org/multipage/dom.html#language

  if (!GetLayoutObject())
    return AXNodeObject::Language();

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style || !style->Locale())
    return AXNodeObject::Language();

  Vector<String> languages;
  String(style->Locale()).Split(',', languages);
  if (languages.IsEmpty())
    return AXNodeObject::Language();
  return AtomicString(languages[0].StripWhiteSpace());
}

//
// Functions that retrieve the current selection.
//

AXObject::AXSelection AXLayoutObject::Selection() const {
  AXSelection text_selection = TextControlSelection();
  if (text_selection.IsValid())
    return text_selection;

  if (!GetLayoutObject() || !GetLayoutObject()->GetFrame())
    return {};

  VisibleSelection selection =
      GetLayoutObject()
          ->GetFrame()
          ->Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated();
  if (selection.IsNone())
    return {};

  VisiblePosition visible_start = selection.VisibleStart();
  Position start = visible_start.ToParentAnchoredPosition();
  TextAffinity start_affinity = visible_start.Affinity();
  Node* anchor_node = start.AnchorNode();
  DCHECK(anchor_node);
  AXLayoutObject* anchor_object = nullptr;

  // Find the closest node that has a corresponding AXObject.
  // This is because some nodes may be aria hidden or might not even have
  // a layout object if they are part of the shadow DOM.
  while (anchor_node) {
    anchor_object = GetUnignoredObjectFromNode(*anchor_node);
    if (anchor_object)
      break;

    if (anchor_node->nextSibling())
      anchor_node = anchor_node->nextSibling();
    else
      anchor_node = anchor_node->parentNode();
  }
  if (!anchor_object)
    return {};
  int anchor_offset = anchor_object->IndexForVisiblePosition(visible_start);
  DCHECK_GE(anchor_offset, 0);
  if (selection.IsCaret()) {
    return {anchor_object, anchor_offset, start_affinity,
            anchor_object, anchor_offset, start_affinity};
  }

  VisiblePosition visible_end = selection.VisibleEnd();
  Position end = visible_end.ToParentAnchoredPosition();
  TextAffinity end_affinity = visible_end.Affinity();
  Node* focus_node = end.AnchorNode();
  DCHECK(focus_node);
  AXLayoutObject* focus_object = nullptr;
  while (focus_node) {
    focus_object = GetUnignoredObjectFromNode(*focus_node);
    if (focus_object)
      break;

    if (focus_node->previousSibling())
      focus_node = focus_node->previousSibling();
    else
      focus_node = focus_node->parentNode();
  }
  if (!focus_object)
    return {};
  int focus_offset = focus_object->IndexForVisiblePosition(visible_end);
  DCHECK_GE(focus_offset, 0);
  return {anchor_object, anchor_offset, start_affinity,
          focus_object,  focus_offset,  end_affinity};
}

// Gets only the start and end offsets of the selection computed using the
// current object as the starting point. Returns a null selection if there is
// no selection in the subtree rooted at this object.
AXObject::AXSelection AXLayoutObject::SelectionUnderObject() const {
  AXSelection text_selection = TextControlSelection();
  if (text_selection.IsValid())
    return text_selection;

  if (!GetNode() || !GetLayoutObject()->GetFrame())
    return {};

  VisibleSelection selection =
      GetLayoutObject()
          ->GetFrame()
          ->Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated();
  Range* selection_range = CreateRange(FirstEphemeralRangeOf(selection));
  ContainerNode* parent_node = GetNode()->parentNode();
  int node_index = GetNode()->NodeIndex();
  if (!selection_range
      // Selection is contained in node.
      || !(parent_node &&
           selection_range->comparePoint(parent_node, node_index,
                                         IGNORE_EXCEPTION_FOR_TESTING) < 0 &&
           selection_range->comparePoint(parent_node, node_index + 1,
                                         IGNORE_EXCEPTION_FOR_TESTING) > 0)) {
    return {};
  }

  int start = IndexForVisiblePosition(selection.VisibleStart());
  DCHECK_GE(start, 0);
  int end = IndexForVisiblePosition(selection.VisibleEnd());
  DCHECK_GE(end, 0);

  return {start, end};
}

AXObject::AXSelection AXLayoutObject::TextControlSelection() const {
  if (!GetLayoutObject())
    return {};

  LayoutObject* layout = nullptr;
  if (GetLayoutObject()->IsTextControl()) {
    layout = GetLayoutObject();
  } else {
    Element* focused_element = GetDocument()->FocusedElement();
    if (focused_element && focused_element->GetLayoutObject() &&
        focused_element->GetLayoutObject()->IsTextControl())
      layout = focused_element->GetLayoutObject();
  }

  if (!layout)
    return {};

  AXObject* ax_object = AXObjectCache().GetOrCreate(layout);
  if (!ax_object || !ax_object->IsAXLayoutObject())
    return {};

  VisibleSelection selection =
      layout->GetFrame()
          ->Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated();
  TextControlElement* text_control =
      ToLayoutTextControl(layout)->GetTextControlElement();
  DCHECK(text_control);
  int start = text_control->selectionStart();
  int end = text_control->selectionEnd();

  return {ax_object, start, selection.VisibleStart().Affinity(),
          ax_object, end,   selection.VisibleEnd().Affinity()};
}

int AXLayoutObject::IndexForVisiblePosition(
    const VisiblePosition& position) const {
  if (GetLayoutObject() && GetLayoutObject()->IsTextControl()) {
    TextControlElement* text_control =
        ToLayoutTextControl(GetLayoutObject())->GetTextControlElement();
    return text_control->IndexForVisiblePosition(position);
  }

  if (!GetNode())
    return 0;

  Position index_position = position.DeepEquivalent();
  if (index_position.IsNull())
    return 0;

  Position start_position = Position::FirstPositionInNode(*GetNode());
  if (start_position > index_position) {
    // TODO(chromium-accessibility): We reach here only when passing a position
    // outside of the node range. This shouldn't happen, but is happening as
    // found in crbug.com/756435.
    LOG(WARNING) << "AX position out of node range";
    return 0;
  }

  return TextIterator::RangeLength(start_position, index_position);
}

AXLayoutObject* AXLayoutObject::GetUnignoredObjectFromNode(Node& node) const {
  if (IsDetached())
    return nullptr;

  AXObject* ax_object = AXObjectCache().GetOrCreate(&node);
  if (!ax_object)
    return nullptr;

  if (ax_object->IsAXLayoutObject() && !ax_object->AccessibilityIsIgnored())
    return ToAXLayoutObject(ax_object);

  return nullptr;
}

//
// Modify or take an action on an object.
//

// Convert from an accessible object and offset to a VisiblePosition.
static VisiblePosition ToVisiblePosition(AXObject* obj, int offset) {
  if (!obj || offset < 0)
    return VisiblePosition();

  // Some objects don't have an associated node, e.g. |LayoutListMarker|.
  if (obj->GetLayoutObject() && !obj->GetNode() && obj->ParentObject()) {
    return ToVisiblePosition(obj->ParentObject(), obj->IndexInParent());
  }

  if (!obj->GetNode())
    return VisiblePosition();

  Node* node = obj->GetNode();
  if (!node->IsTextNode()) {
    int child_count = obj->Children().size();

    // Place position immediately before the container node, if there were no
    // children.
    if (child_count == 0) {
      if (!obj->ParentObject())
        return VisiblePosition();
      return ToVisiblePosition(obj->ParentObject(), obj->IndexInParent());
    }

    // The offsets are child offsets over the AX tree. Note that we allow
    // for the offset to equal the number of children as |Range| does.
    if (offset < 0 || offset > child_count)
      return VisiblePosition();

    // Clamp to between 0 and child count - 1.
    int clamped_offset =
        static_cast<unsigned>(offset) > (obj->Children().size() - 1)
            ? offset - 1
            : offset;

    AXObject* child_obj = obj->Children()[clamped_offset];
    Node* child_node = child_obj->GetNode();
    // If a particular child can't be selected, expand to select the whole
    // object.
    if (!child_node || !child_node->parentNode())
      return ToVisiblePosition(obj->ParentObject(), obj->IndexInParent());

    // The index in parent.
    int adjusted_offset = child_node->NodeIndex();

    // If we had to clamp the offset above, the client wants to select the
    // end of the node.
    if (clamped_offset != offset)
      adjusted_offset++;

    return CreateVisiblePosition(
        Position::EditingPositionOf(child_node->parentNode(), adjusted_offset));
  }

  // If it is a text node, we need to call some utility functions that use a
  // TextIterator to walk the characters of the node and figure out the position
  // corresponding to the visible character at position |offset|.
  ContainerNode* parent = node->parentNode();
  if (!parent)
    return VisiblePosition();

  VisiblePosition node_position = blink::VisiblePositionBeforeNode(*node);
  int node_index = blink::IndexForVisiblePosition(node_position, parent);
  return blink::VisiblePositionForIndex(node_index + offset, parent);
}

bool AXLayoutObject::OnNativeSetSelectionAction(const AXSelection& selection) {
  if (!GetLayoutObject() || !selection.IsValid())
    return false;

  AXObject* anchor_object =
      selection.anchor_object ? selection.anchor_object.Get() : this;
  AXObject* focus_object =
      selection.focus_object ? selection.focus_object.Get() : this;

  if (!IsValidSelectionBound(anchor_object) ||
      !IsValidSelectionBound(focus_object)) {
    return false;
  }

  if (anchor_object->GetLayoutObject()->GetNode() &&
      anchor_object->GetLayoutObject()->GetNode()->DispatchEvent(
          *Event::CreateCancelableBubble(EventTypeNames::selectstart)) !=
          DispatchEventResult::kNotCanceled)
    return false;

  if (!IsValidSelectionBound(anchor_object) ||
      !IsValidSelectionBound(focus_object)) {
    return false;
  }

  // The selection offsets are offsets into the accessible value.
  if (anchor_object == focus_object &&
      anchor_object->GetLayoutObject()->IsTextControl()) {
    TextControlElement* text_control =
        ToLayoutTextControl(anchor_object->GetLayoutObject())
            ->GetTextControlElement();
    if (selection.anchor_offset <= selection.focus_offset) {
      text_control->SetSelectionRange(selection.anchor_offset,
                                      selection.focus_offset,
                                      kSelectionHasForwardDirection);
    } else {
      text_control->SetSelectionRange(selection.focus_offset,
                                      selection.anchor_offset,
                                      kSelectionHasBackwardDirection);
    }
    return true;
  }

  LocalFrame* frame = GetLayoutObject()->GetFrame();
  if (!frame || !frame->Selection().IsAvailable())
    return false;

  // TODO(editing-dev): Use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  see http://crbug.com/590369 for more details.
  // This callsite should probably move up the stack.
  frame->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  // Set the selection based on visible positions, because the offsets in
  // accessibility nodes are based on visible indexes, which often skips
  // redundant whitespace, for example.
  VisiblePosition anchor_visible_position =
      ToVisiblePosition(anchor_object, selection.anchor_offset);
  VisiblePosition focus_visible_position =
      ToVisiblePosition(focus_object, selection.focus_offset);
  if (anchor_visible_position.IsNull() || focus_visible_position.IsNull())
    return false;

  frame->Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(anchor_visible_position.ToPositionWithAffinity())
          .Extend(focus_visible_position.DeepEquivalent())
          .Build());
  return true;
}

bool AXLayoutObject::IsValidSelectionBound(const AXObject* bound_object) const {
  return GetLayoutObject() && bound_object && !bound_object->IsDetached() &&
         bound_object->IsAXLayoutObject() && bound_object->GetLayoutObject() &&
         bound_object->GetLayoutObject()->GetFrame() ==
             GetLayoutObject()->GetFrame() &&
         &bound_object->AXObjectCache() == &AXObjectCache();
}

bool AXLayoutObject::OnNativeSetValueAction(const String& string) {
  if (!GetNode() || !GetNode()->IsElementNode())
    return false;
  if (!layout_object_ || !layout_object_->IsBoxModelObject())
    return false;

  LayoutBoxModelObject* layout_object = ToLayoutBoxModelObject(layout_object_);
  if (layout_object->IsTextField() && IsHTMLInputElement(*GetNode())) {
    ToHTMLInputElement(*GetNode())
        .setValue(string, kDispatchInputAndChangeEvent);
    return true;
  }

  if (layout_object->IsTextArea() && IsHTMLTextAreaElement(*GetNode())) {
    ToHTMLTextAreaElement(*GetNode())
        .setValue(string, kDispatchInputAndChangeEvent);
    return true;
  }

  return false;
}

//
// Notifications that this object may have changed.
//

void AXLayoutObject::HandleActiveDescendantChanged() {
  if (!GetLayoutObject())
    return;

  AXObject* focused_object = AXObjectCache().FocusedObject();
  if (focused_object == this) {
    AXObject* active_descendant = ActiveDescendant();
    if (active_descendant && active_descendant->IsSelectedFromFocus()) {
      // In single selection containers, selection follows focus, so a selection
      // changed event must be fired. This ensures the AT is notified that the
      // selected state has changed, so that it does not read "unselected" as
      // the user navigates through the items.
      AXObjectCache().HandleAriaSelectedChanged(active_descendant->GetNode());
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
      case ax::mojom::Role::kLayoutTable:
      case ax::mojom::Role::kTree:
      case ax::mojom::Role::kTreeGrid:
      case ax::mojom::Role::kGrid:
      case ax::mojom::Role::kTable:
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
                                     ax::mojom::Event::kRowCountChanged);
  }

  // Post that the specific row either collapsed or expanded.
  AccessibilityExpanded expanded = IsExpanded();
  if (!expanded)
    return;

  if (RoleValue() == ax::mojom::Role::kRow ||
      RoleValue() == ax::mojom::Role::kTreeItem) {
    ax::mojom::Event notification = ax::mojom::Event::kRowExpanded;
    if (expanded == kExpandedCollapsed)
      notification = ax::mojom::Event::kRowCollapsed;

    AXObjectCache().PostNotification(this, notification);
  } else {
    AXObjectCache().PostNotification(this, ax::mojom::Event::kExpandedChanged);
  }
}

void AXLayoutObject::HandleAutofillStateChanged(bool is_available) {
  if (is_autofill_available_ != is_available) {
    is_autofill_available_ = is_available;
    // Reusing the value change event in order to invalidate, even though the
    // value did not necessarily change.
    // TODO(dmazzoni) change to using a MarkDirty() API.
    AXObjectCache().PostNotification(this, ax::mojom::Event::kValueChanged);
  }
}

void AXLayoutObject::TextChanged() {
  if (!layout_object_)
    return;

  Settings* settings = GetDocument()->GetSettings();
  if (settings && settings->GetInlineTextBoxAccessibilityEnabled() &&
      RoleValue() == ax::mojom::Role::kStaticText)
    ChildrenChanged();

  // Do this last - AXNodeObject::textChanged posts live region announcements,
  // and we should update the inline text boxes first.
  AXNodeObject::TextChanged();
}

//
// Text metrics. Most of these should be deprecated, needs major cleanup.
//

static LayoutObject* LayoutObjectFromPosition(const Position& position) {
  DCHECK(position.IsNotNull());
  Node* layout_object_node = nullptr;
  switch (position.AnchorType()) {
    case PositionAnchorType::kOffsetInAnchor:
      layout_object_node = position.ComputeNodeAfterPosition();
      if (!layout_object_node || !layout_object_node->GetLayoutObject())
        layout_object_node = position.AnchorNode()->lastChild();
      break;

    case PositionAnchorType::kBeforeAnchor:
    case PositionAnchorType::kAfterAnchor:
      break;

    case PositionAnchorType::kBeforeChildren:
      layout_object_node = position.AnchorNode()->firstChild();
      break;
    case PositionAnchorType::kAfterChildren:
      layout_object_node = position.AnchorNode()->lastChild();
      break;
  }
  if (!layout_object_node || !layout_object_node->GetLayoutObject())
    layout_object_node = position.AnchorNode();
  return layout_object_node->GetLayoutObject();
}

static bool LayoutObjectContainsPosition(LayoutObject* target,
                                         const Position& position) {
  for (LayoutObject* layout_object = LayoutObjectFromPosition(position);
       layout_object && layout_object->GetNode();
       layout_object = layout_object->Parent()) {
    if (layout_object == target)
      return true;
  }
  return false;
}

// NOTE: Consider providing this utility method as AX API
int AXLayoutObject::Index(const VisiblePosition& position) const {
  if (position.IsNull() || !IsTextControl())
    return -1;

  if (LayoutObjectContainsPosition(layout_object_, position.DeepEquivalent()))
    return IndexForVisiblePosition(position);

  return -1;
}

VisiblePosition AXLayoutObject::VisiblePositionForIndex(int index) const {
  if (!layout_object_)
    return VisiblePosition();

  if (layout_object_->IsTextControl())
    return ToLayoutTextControl(layout_object_)
        ->GetTextControlElement()
        ->VisiblePositionForIndex(index);

  Node* node = layout_object_->GetNode();
  if (!node)
    return VisiblePosition();

  if (index <= 0)
    return CreateVisiblePosition(FirstPositionInOrBeforeNode(*node));

  Position start, end;
  bool selected = Range::selectNodeContents(node, start, end);
  if (!selected)
    return VisiblePosition();

  CharacterIterator it(start, end);
  it.Advance(index - 1);
  return CreateVisiblePosition(Position(it.CurrentContainer(), it.EndOffset()),
                               TextAffinity::kUpstream);
}

void AXLayoutObject::AddInlineTextBoxChildren(bool force) {
  Document* document = GetDocument();
  if (!document)
    return;

  Settings* settings = document->GetSettings();
  if (!force &&
      (!settings || !settings->GetInlineTextBoxAccessibilityEnabled()))
    return;

  if (!GetLayoutObject() || !GetLayoutObject()->IsText())
    return;

  if (GetLayoutObject()->NeedsLayout()) {
    // If a LayoutText needs layout, its inline text boxes are either
    // nonexistent or invalid, so defer until the layout happens and
    // the layoutObject calls AXObjectCacheImpl::inlineTextBoxesUpdated.
    return;
  }

  LayoutText* layout_text = ToLayoutText(GetLayoutObject());
  for (scoped_refptr<AbstractInlineTextBox> box =
           layout_text->FirstAbstractInlineTextBox();
       box.get(); box = box->NextInlineTextBox()) {
    AXObject* ax_object = AXObjectCache().GetOrCreate(box.get());
    if (!ax_object->AccessibilityIsIgnored())
      children_.push_back(ax_object);
  }
}

void AXLayoutObject::LineBreaks(Vector<int>& line_breaks) const {
  if (!IsTextControl())
    return;

  VisiblePosition visible_pos = VisiblePositionForIndex(0);
  VisiblePosition prev_visible_pos = visible_pos;
  visible_pos = NextLinePosition(visible_pos, LayoutUnit(), kHasEditableAXRole);
  // nextLinePosition moves to the end of the current line when there are
  // no more lines.
  while (visible_pos.IsNotNull() &&
         !InSameLine(prev_visible_pos, visible_pos)) {
    line_breaks.push_back(IndexForVisiblePosition(visible_pos));
    prev_visible_pos = visible_pos;
    visible_pos =
        NextLinePosition(visible_pos, LayoutUnit(), kHasEditableAXRole);

    // Make sure we always make forward progress.
    if (visible_pos.DeepEquivalent().CompareTo(
            prev_visible_pos.DeepEquivalent()) < 0)
      break;
  }
}

// The following is a heuristic used to determine if a
// <table> should be with ax::mojom::Role::kTable or
// ax::mojom::Role::kLayoutTable.
bool AXLayoutObject::IsDataTable() const {
  if (!layout_object_ || !GetNode())
    return false;

  // If it has an ARIA role, it's definitely a data table.
  AtomicString role;
  if (HasAOMPropertyOrARIAAttribute(AOMStringProperty::kRole, role))
    return true;

  if (!layout_object_->IsTable())
    return false;

  // When a section of the document is contentEditable, all tables should be
  // treated as data tables, otherwise users may not be able to work with rich
  // text editors that allow creating and editing tables.
  if (GetNode() && HasEditableStyle(*GetNode()))
    return true;

  // If there's no node, it's definitely a layout table. This happens
  // when table CSS styles are used without a complete table DOM structure.
  LayoutTable* table = ToLayoutTable(layout_object_);
  table->RecalcSectionsIfNeeded();
  Node* table_node = table->GetNode();
  if (!table_node || !IsHTMLTableElement(table_node))
    return false;

  // This employs a heuristic to determine if this table should appear.
  // Only "data" tables should be exposed as tables.
  // Unfortunately, there is no good way to determine the difference
  // between a "layout" table and a "data" table.
  HTMLTableElement* table_element = ToHTMLTableElement(table_node);

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

  // go through the cell's and check for tell-tale signs of "data" table status
  // cells have borders, or use attributes like headers, abbr, scope or axis
  table->RecalcSectionsIfNeeded();
  LayoutTableSection* first_body = table->FirstBody();
  if (!first_body)
    return false;

  int num_cols_in_first_body = first_body->NumEffectiveColumns();
  int num_rows = first_body->NumRows();
  // If there's only one cell, it's not a good AXTable candidate.
  if (num_rows == 1 && num_cols_in_first_body == 1)
    return false;

  // If there are at least 20 rows, we'll call it a data table.
  if (num_rows >= 20)
    return true;

  // Store the background color of the table to check against cell's background
  // colors.
  const ComputedStyle* table_style = table->Style();
  if (!table_style)
    return false;
  Color table_bg_color =
      table_style->VisitedDependentColor(GetCSSPropertyBackgroundColor());

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
    int n_cols = first_body->NumCols(row);
    for (int col = 0; col < n_cols; ++col) {
      LayoutTableCell* cell = first_body->PrimaryCellAt(row, col);
      if (!cell)
        continue;
      Node* cell_node = cell->GetNode();
      if (!cell_node)
        continue;

      if (cell->Size().Width() < 1 || cell->Size().Height() < 1)
        continue;

      valid_cell_count++;

      // Any <th> tag -> treat as data table.
      if (cell_node->HasTagName(thTag))
        return true;

      // In this case, the developer explicitly assigned a "data" table
      // attribute.
      if (IsHTMLTableCellElement(*cell_node)) {
        HTMLTableCellElement& cell_element = ToHTMLTableCellElement(*cell_node);
        if (!cell_element.Headers().IsEmpty() ||
            !cell_element.Abbr().IsEmpty() || !cell_element.Axis().IsEmpty() ||
            !cell_element.FastGetAttribute(scopeAttr).IsEmpty())
          return true;
      }

      const ComputedStyle* computed_style = cell->Style();
      if (!computed_style)
        continue;

      // If the empty-cells style is set, we'll call it a data table.
      if (computed_style->EmptyCells() == EEmptyCells::kHide)
        return true;

      // If a cell has matching bordered sides, call it a (fully) bordered cell.
      if ((cell->BorderTop() > 0 && cell->BorderBottom() > 0) ||
          (cell->BorderLeft() > 0 && cell->BorderRight() > 0))
        bordered_cell_count++;

      // Also keep track of each individual border, so we can catch tables where
      // most cells have a bottom border, for example.
      if (cell->BorderTop() > 0)
        cells_with_top_border++;
      if (cell->BorderBottom() > 0)
        cells_with_bottom_border++;
      if (cell->BorderLeft() > 0)
        cells_with_left_border++;
      if (cell->BorderRight() > 0)
        cells_with_right_border++;

      // If the cell has a different color from the table and there is cell
      // spacing, then it is probably a data table cell (spacing and colors take
      // the place of borders).
      Color cell_color = computed_style->VisitedDependentColor(
          GetCSSPropertyBackgroundColor());
      if (table->HBorderSpacing() > 0 && table->VBorderSpacing() > 0 &&
          table_bg_color != cell_color && cell_color.Alpha() != 1)
        background_difference_cell_count++;

      // If we've found 10 "good" cells, we don't need to keep searching.
      if (bordered_cell_count >= 10 || background_difference_cell_count >= 10)
        return true;

      // For the first 5 rows, cache the background color so we can check if
      // this table has zebra-striped rows.
      if (row < 5 && row == alternating_row_color_count) {
        LayoutObject* layout_row = cell->Parent();
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
  if (AriaRoleAttribute() != ax::mojom::Role::kUnknown)
    return AXNodeObject::ColumnCount();

  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || !layout_object->IsTable() || !layout_object->GetNode())
    return AXNodeObject::ColumnCount();

  LayoutTable* table = ToLayoutTable(layout_object);
  table->RecalcSectionsIfNeeded();
  LayoutTableSection* table_section = table->TopSection();
  if (!table_section)
    return AXNodeObject::ColumnCount();

  return table_section->NumEffectiveColumns();
}

unsigned AXLayoutObject::RowCount() const {
  if (AriaRoleAttribute() != ax::mojom::Role::kUnknown)
    return AXNodeObject::RowCount();

  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || !layout_object->IsTable() || !layout_object->GetNode())
    return AXNodeObject::RowCount();

  LayoutTable* table = ToLayoutTable(layout_object);
  table->RecalcSectionsIfNeeded();

  unsigned row_count = 0;
  LayoutTableSection* table_section = table->TopSection();
  if (!table_section)
    return AXNodeObject::RowCount();

  while (table_section) {
    row_count += table_section->NumRows();
    table_section = table->SectionBelow(table_section, kSkipEmptySections);
  }
  return row_count;
}

unsigned AXLayoutObject::ColumnIndex() const {
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || !layout_object->GetNode())
    return AXNodeObject::ColumnIndex();

  if (layout_object->IsTableCell()) {
    LayoutTableCell* cell = ToLayoutTableCell(layout_object);
    return cell->Table()->AbsoluteColumnToEffectiveColumn(
        cell->AbsoluteColumnIndex());
  }

  return AXNodeObject::ColumnIndex();
}

unsigned AXLayoutObject::RowIndex() const {
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || !layout_object->GetNode())
    return AXNodeObject::RowIndex();

  unsigned row_index = 0;
  LayoutTableSection* row_section = nullptr;
  LayoutTable* table = nullptr;
  if (layout_object->IsTableRow()) {
    LayoutTableRow* row = ToLayoutTableRow(layout_object);
    row_index = row->RowIndex();
    row_section = row->Section();
    table = row->Table();
  } else if (layout_object->IsTableCell()) {
    LayoutTableCell* cell = ToLayoutTableCell(layout_object);
    row_index = cell->RowIndex();
    row_section = cell->Section();
    table = cell->Table();
  } else {
    return AXNodeObject::RowIndex();
  }

  if (!table || !row_section)
    return AXNodeObject::RowIndex();

  // Since our table might have multiple sections, we have to offset our row
  // appropriately.
  table->RecalcSectionsIfNeeded();
  LayoutTableSection* section = table->TopSection();
  while (section && section != row_section) {
    row_index += section->NumRows();
    section = table->SectionBelow(section, kSkipEmptySections);
  }

  return row_index;
}

unsigned AXLayoutObject::ColumnSpan() const {
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || !layout_object->IsTableCell())
    return AXNodeObject::ColumnSpan();

  LayoutTableCell* cell = ToLayoutTableCell(layout_object);
  unsigned absolute_first_col = cell->AbsoluteColumnIndex();
  unsigned absolute_last_col = absolute_first_col + cell->ColSpan() - 1;
  unsigned effective_first_col =
      cell->Table()->AbsoluteColumnToEffectiveColumn(absolute_first_col);
  unsigned effective_last_col =
      cell->Table()->AbsoluteColumnToEffectiveColumn(absolute_last_col);
  return effective_last_col - effective_first_col + 1;
}

unsigned AXLayoutObject::RowSpan() const {
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || !layout_object->IsTableCell())
    return AXNodeObject::ColumnSpan();

  LayoutTableCell* cell = ToLayoutTableCell(layout_object);
  return cell->ResolvedRowSpan();
}

ax::mojom::SortDirection AXLayoutObject::GetSortDirection() const {
  if (RoleValue() != ax::mojom::Role::kRowHeader &&
      RoleValue() != ax::mojom::Role::kColumnHeader)
    return ax::mojom::SortDirection::kNone;

  const AtomicString& aria_sort =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kSort);
  if (aria_sort.IsEmpty())
    return ax::mojom::SortDirection::kNone;
  if (EqualIgnoringASCIICase(aria_sort, "none"))
    return ax::mojom::SortDirection::kNone;
  if (EqualIgnoringASCIICase(aria_sort, "ascending"))
    return ax::mojom::SortDirection::kAscending;
  if (EqualIgnoringASCIICase(aria_sort, "descending"))
    return ax::mojom::SortDirection::kDescending;

  // Technically, illegal values should be exposed as is, but this does
  // not seem to be worth the implementation effort at this time.
  return ax::mojom::SortDirection::kOther;
}

static ax::mojom::Role DecideRoleFromSibling(LayoutTableCell* sibling_cell) {
  if (!sibling_cell)
    return ax::mojom::Role::kCell;

  if (Node* sibling_node = sibling_cell->GetNode()) {
    if (sibling_node->HasTagName(thTag))
      return ax::mojom::Role::kColumnHeader;
    if (sibling_node->HasTagName(tdTag))
      return ax::mojom::Role::kRowHeader;
  }

  return ax::mojom::Role::kCell;
}

ax::mojom::Role AXLayoutObject::DetermineTableRowRole() const {
  AXObject* parent = ParentObjectUnignored();
  if (!parent)
    return ax::mojom::Role::kGenericContainer;

  if (parent->RoleValue() == ax::mojom::Role::kLayoutTable)
    return ax::mojom::Role::kLayoutTableRow;

  if (parent->IsTableLikeRole())
    return ax::mojom::Role::kRow;

  return ax::mojom::Role::kGenericContainer;
}

ax::mojom::Role AXLayoutObject::DetermineTableCellRole() const {
  DCHECK(layout_object_);

  AXObject* parent = ParentObjectUnignored();
  if (!parent || !parent->IsTableRowLikeRole())
    return ax::mojom::Role::kGenericContainer;

  AXObject* grandparent = parent->ParentObjectUnignored();
  if (!grandparent || !grandparent->IsTableLikeRole())
    return ax::mojom::Role::kGenericContainer;

  if (parent->RoleValue() == ax::mojom::Role::kLayoutTableRow)
    return ax::mojom::Role::kLayoutTableCell;

  if (!parent->IsTableRowLikeRole())
    return ax::mojom::Role::kGenericContainer;

  if (!GetNode() || !GetNode()->HasTagName(thTag))
    return ax::mojom::Role::kCell;

  const AtomicString& scope = GetAttribute(scopeAttr);
  if (EqualIgnoringASCIICase(scope, "row") ||
      EqualIgnoringASCIICase(scope, "rowgroup"))
    return ax::mojom::Role::kRowHeader;
  if (EqualIgnoringASCIICase(scope, "col") ||
      EqualIgnoringASCIICase(scope, "colgroup"))
    return ax::mojom::Role::kColumnHeader;

  // Check the previous cell and the next cell on the same row.
  LayoutTableCell* layout_cell = ToLayoutTableCell(layout_object_);
  ax::mojom::Role header_role = ax::mojom::Role::kCell;
  // if header is preceded by header cells on the same row, then it is a
  // column header. If it is preceded by other cells then it's a row header.
  if ((header_role = DecideRoleFromSibling(layout_cell->PreviousCell())) !=
      ax::mojom::Role::kCell)
    return header_role;

  // if header is followed by header cells on the same row, then it is a
  // column header. If it is followed by other cells then it's a row header.
  if ((header_role = DecideRoleFromSibling(layout_cell->NextCell())) !=
      ax::mojom::Role::kCell)
    return header_role;

  // If there are no other cells on that row, then it is a column header.
  return ax::mojom::Role::kColumnHeader;
}

AXObject* AXLayoutObject::CellForColumnAndRow(unsigned target_column_index,
                                              unsigned target_row_index) const {
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || !layout_object->IsTable()) {
    return AXNodeObject::CellForColumnAndRow(target_column_index,
                                             target_row_index);
  }

  LayoutTable* table = ToLayoutTable(layout_object);
  table->RecalcSectionsIfNeeded();

  LayoutTableSection* table_section = table->TopSection();
  if (!table_section) {
    return AXNodeObject::CellForColumnAndRow(target_column_index,
                                             target_row_index);
  }

  unsigned row_offset = 0;
  while (table_section) {
    // Iterate backwards through the rows in case the desired cell has a rowspan
    // and exists in a previous row.
    for (LayoutTableRow* row = table_section->LastRow(); row;
         row = row->PreviousRow()) {
      unsigned row_index = row->RowIndex() + row_offset;
      for (LayoutTableCell* cell = row->LastCell(); cell;
           cell = cell->PreviousCell()) {
        unsigned absolute_first_col = cell->AbsoluteColumnIndex();
        unsigned absolute_last_col = absolute_first_col + cell->ColSpan() - 1;
        unsigned effective_first_col =
            cell->Table()->AbsoluteColumnToEffectiveColumn(absolute_first_col);
        unsigned effective_last_col =
            cell->Table()->AbsoluteColumnToEffectiveColumn(absolute_last_col);
        unsigned row_span = cell->ResolvedRowSpan();
        if (target_column_index >= effective_first_col &&
            target_column_index <= effective_last_col &&
            target_row_index >= row_index &&
            target_row_index < row_index + row_span) {
          return AXObjectCache().GetOrCreate(cell);
        }
      }
    }

    row_offset += table_section->NumRows();
    table_section = table->SectionBelow(table_section, kSkipEmptySections);
  }

  return nullptr;
}

bool AXLayoutObject::FindAllTableCellsWithRole(ax::mojom::Role role,
                                               AXObjectVector& cells) const {
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || !layout_object->IsTable())
    return false;

  LayoutTable* table = ToLayoutTable(layout_object);
  table->RecalcSectionsIfNeeded();

  LayoutTableSection* table_section = table->TopSection();
  if (!table_section)
    return true;

  while (table_section) {
    for (LayoutTableRow* row = table_section->FirstRow(); row;
         row = row->NextRow()) {
      for (LayoutTableCell* cell = row->FirstCell(); cell;
           cell = cell->NextCell()) {
        AXObject* ax_cell = AXObjectCache().GetOrCreate(cell);
        if (ax_cell && ax_cell->RoleValue() == role)
          cells.push_back(ax_cell);
      }
    }

    table_section = table->SectionBelow(table_section, kSkipEmptySections);
  }

  return true;
}

void AXLayoutObject::ColumnHeaders(AXObjectVector& headers) const {
  if (!FindAllTableCellsWithRole(ax::mojom::Role::kColumnHeader, headers))
    AXNodeObject::ColumnHeaders(headers);
}

void AXLayoutObject::RowHeaders(AXObjectVector& headers) const {
  if (!FindAllTableCellsWithRole(ax::mojom::Role::kRowHeader, headers))
    AXNodeObject::RowHeaders(headers);
}

AXObject* AXLayoutObject::HeaderObject() const {
  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || !layout_object->IsTableRow())
    return nullptr;

  LayoutTableRow* row = ToLayoutTableRow(layout_object);
  for (LayoutTableCell* cell = row->FirstCell(); cell;
       cell = cell->NextCell()) {
    AXObject* ax_cell = AXObjectCache().GetOrCreate(cell);
    if (ax_cell && ax_cell->RoleValue() == ax::mojom::Role::kRowHeader)
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
    if (!tab_panel || tab_panel->RoleValue() != ax::mojom::Role::kTabPanel)
      continue;

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

  for (const auto& child : parent->Children()) {
    if (child->GetBoundsInFrameCoordinates().Contains(point))
      return child.Get();
  }

  return nullptr;
}

bool AXLayoutObject::IsSVGImage() const {
  return RemoteSVGRootElement();
}

void AXLayoutObject::DetachRemoteSVGRoot() {
  if (AXSVGRoot* root = RemoteSVGRootElement())
    root->SetParent(nullptr);
}

AXSVGRoot* AXLayoutObject::RemoteSVGRootElement() const {
  // FIXME(dmazzoni): none of this code properly handled multiple references to
  // the same remote SVG document. I'm disabling this support until it can be
  // fixed properly.
  return nullptr;
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

// Hidden children are those that are not laid out or visible, but are
// specifically marked as aria-hidden=false,
// meaning that they should be exposed to the AX hierarchy.
void AXLayoutObject::AddHiddenChildren() {
  Node* node = this->GetNode();
  if (!node)
    return;

  // First do a quick run through to determine if we have any hidden nodes (most
  // often we will not).  If we do have hidden nodes, we need to determine where
  // to insert them so they match DOM order as close as possible.
  bool should_insert_hidden_nodes = false;
  for (Node& child : NodeTraversal::ChildrenOf(*node)) {
    if (!child.GetLayoutObject() && IsNodeAriaVisible(&child)) {
      should_insert_hidden_nodes = true;
      break;
    }
  }

  if (!should_insert_hidden_nodes)
    return;

  // Iterate through all of the children, including those that may have already
  // been added, and try to insert hidden nodes in the correct place in the DOM
  // order.
  unsigned insertion_index = 0;
  for (Node& child : NodeTraversal::ChildrenOf(*node)) {
    if (child.GetLayoutObject()) {
      // Find out where the last layout sibling is located within m_children.
      if (AXObject* child_object =
              AXObjectCache().Get(child.GetLayoutObject())) {
        if (child_object->AccessibilityIsIgnored()) {
          const auto& children = child_object->Children();
          child_object = children.size() ? children.back().Get() : nullptr;
        }
        if (child_object)
          insertion_index = children_.Find(child_object) + 1;
        continue;
      }
    }

    if (!IsNodeAriaVisible(&child))
      continue;

    unsigned previous_size = children_.size();
    if (insertion_index > previous_size)
      insertion_index = previous_size;

    InsertChild(AXObjectCache().GetOrCreate(&child), insertion_index);
    insertion_index += (children_.size() - previous_size);
  }
}

void AXLayoutObject::AddImageMapChildren() {
  LayoutBoxModelObject* css_box = GetLayoutBoxModelObject();
  if (!css_box || !css_box->IsLayoutImage())
    return;

  HTMLMapElement* map = ToLayoutImage(css_box)->ImageMap();
  if (!map)
    return;

  for (HTMLAreaElement& area :
       Traversal<HTMLAreaElement>::DescendantsOf(*map)) {
    // add an <area> element for this child if it has a link
    AXObject* obj = AXObjectCache().GetOrCreate(&area);
    if (obj) {
      AXImageMapLink* area_object = ToAXImageMapLink(obj);
      area_object->SetParent(this);
      DCHECK_NE(area_object->AXObjectID(), 0U);
      if (!area_object->AccessibilityIsIgnored())
        children_.push_back(area_object);
      else
        AXObjectCache().Remove(area_object->AXObjectID());
    }
  }
}

void AXLayoutObject::AddPopupChildren() {
  if (!IsHTMLInputElement(GetNode()))
    return;
  if (AXObject* ax_popup = ToHTMLInputElement(GetNode())->PopupRootAXObject())
    children_.push_back(ax_popup);
}

void AXLayoutObject::AddRemoteSVGChildren() {
  AXSVGRoot* root = RemoteSVGRootElement();
  if (!root)
    return;

  root->SetParent(this);

  if (root->AccessibilityIsIgnored()) {
    for (const auto& child : root->Children())
      children_.push_back(child);
  } else {
    children_.push_back(root);
  }
}

void AXLayoutObject::AddTableChildren() {
  if (!IsTableLikeRole())
    return;

  AXObjectCacheImpl& ax_cache = AXObjectCache();
  if (layout_object_->IsTable()) {
    LayoutTable* table = ToLayoutTable(layout_object_);
    table->RecalcSectionsIfNeeded();
    Node* table_node = table->GetNode();
    if (IsHTMLTableElement(table_node)) {
      if (HTMLTableCaptionElement* caption =
              ToHTMLTableElement(table_node)->caption()) {
        AXObject* caption_object = ax_cache.GetOrCreate(caption);
        if (caption_object && !caption_object->AccessibilityIsIgnored())
          children_.push_front(caption_object);
      }
    }
  }
}

}  // namespace blink
