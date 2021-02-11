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
#include "third_party/blink/renderer/core/css/counter_style_map.h"
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
#include "third_party/blink/renderer/core/layout/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/layout_list_marker.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/layout_table_section.h"
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
  DCHECK(layout_object_);
  layout_object_->SetHasAXObject(true);
#endif
}

AXLayoutObject::~AXLayoutObject() {
  DCHECK(IsDetached());
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

  auto* box = To<LayoutBox>(layout_object_);

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

static bool IsImageOrAltText(LayoutObject* layout_object, Node* node) {
  DCHECK(layout_object);
  if (layout_object->IsImage())
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
  DCHECK(layout_object_);
  // Markup did not provide a specific role, so attempt to determine one
  // from the computed style.
  Node* node = GetNode();

  if (layout_object_->IsListItem() || IsA<HTMLLIElement>(node))
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

  if (IsImageOrAltText(layout_object_, node)) {
    if (node && node->IsLink())
      return ax::mojom::blink::Role::kImageMap;
    if (IsA<HTMLInputElement>(node))
      return ButtonRoleType();

    return ax::mojom::blink::Role::kImage;
  }

  if (IsA<HTMLCanvasElement>(node))
    return ax::mojom::blink::Role::kCanvas;

  if (IsA<LayoutView>(layout_object_))
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
  if (!layout_object_) {
    NOTREACHED();
    return ax::mojom::blink::Role::kUnknown;
  }
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

void AXLayoutObject::Detach() {
  AXNodeObject::Detach();

#if DCHECK_IS_ON()
  if (layout_object_)
    layout_object_->SetHasAXObject(false);
#endif
  layout_object_ = nullptr;
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
      layout_object->IsTableSection() || layout_object->IsAnonymousBlock() ||
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

// Returns true if the object is marked user-select:none
bool AXLayoutObject::IsNotUserSelectable() const {
  if (!GetLayoutObject())
    return false;

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style)
    return false;

  return (style->UserSelect() == EUserSelect::kNone);
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
  // the root node of the web area, so force that node to always be unignored.
  if (IsWebArea())
    return false;

  const Node* node = GetNode();
  if (IsA<HTMLHtmlElement>(node))
    return true;

  if (!layout_object_) {
    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXNotRendered));
    return true;
  }

  // Ignore continuations, they're duplicate copies of inline nodes with blocks
  // inside. AXObjects are no longer created for these.
  DCHECK(!layout_object_->IsElementContinuation());

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

  // A LayoutEmbeddedContent is an iframe element or embedded object element or
  // something like that. We don't want to ignore those.
  if (layout_object_->IsLayoutEmbeddedContent())
    return false;

  // Make sure renderers with layers stay in the tree.
  if (GetLayoutObject() && GetLayoutObject()->HasLayer() && node &&
      node->hasChildren()) {
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

    const auto* canvas = DynamicTo<LayoutHTMLCanvas>(GetLayoutObject());
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
    // Ignore TextAlternative of the list marker for SUMMARY because:
    //  - TextAlternatives for disclosure-* are triangle symbol characters used
    //    to visually indicate the expansion state.
    //  - It's redundant. The host DETAILS exposes the expansion state.
    if (layout_object_->Parent()->IsListMarkerForSummary()) {
      if (ignored_reasons)
        ignored_reasons->push_back(IgnoredReason(kAXPresentational));
      return true;
    }
    return false;
  }

  // FIXME(aboxhall): may need to move?
  base::Optional<String> alt_text = GetCSSAltText(node);
  if (alt_text)
    return alt_text->IsEmpty();

  if (IsWebArea())
    return false;
  if (layout_object_->IsListMarkerIncludingAll()) {
    // Ignore TextAlternative of the list marker for SUMMARY because:
    //  - TextAlternatives for disclosure-* are triangle symbol characters used
    //    to visually indicate the expansion state.
    //  - It's redundant. The host DETAILS exposes the expansion state.
    if (layout_object_->IsListMarkerForSummary()) {
      if (ignored_reasons)
        ignored_reasons->push_back(IgnoredReason(kAXPresentational));
      return true;
    }
    return false;
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
  if (node) {
    const TextControlElement* text_control = EnclosingTextControl(node);
    if (text_control && text_control->InnerEditorElement() == node)
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
    if (has_any_text || (node && node->HasAnyEventListeners(
                                     event_util::MouseButtonEventTypes()))) {
      return false;
    }

    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXUninteresting));
    return true;
  }

  // If setting enabled, do not ignore SVG grouping (<g>) elements.
  if (IsA<SVGGElement>(node)) {
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

  // TODO(crbug.com/1166766): Use the 'speak-as' descriptor value following
  // https://drafts.csswg.org/css-counter-styles-3/#counter-style-speak-as
  switch (ListMarker::GetListStyleCategory(*GetDocument(), *computed_style)) {
    case ListMarker::ListStyleCategory::kNone:
      return ax::mojom::blink::ListStyle::kNone;
    case ListMarker::ListStyleCategory::kSymbol: {
      AtomicString counter_style_name =
          computed_style->GetListStyleType()->GetCounterStyleName();
      if (counter_style_name == "disc")
        return ax::mojom::blink::ListStyle::kDisc;
      if (counter_style_name == "circle")
        return ax::mojom::blink::ListStyle::kCircle;
      if (counter_style_name == "square")
        return ax::mojom::blink::ListStyle::kSquare;
      return ax::mojom::blink::ListStyle::kOther;
    }
    case ListMarker::ListStyleCategory::kLanguage: {
      AtomicString counter_style_name =
          computed_style->GetListStyleType()->GetCounterStyleName();
      if (counter_style_name == "decimal")
        return ax::mojom::blink::ListStyle::kNumeric;
      if (counter_style_name == "decimal-leading-zero") {
        // 'decimal-leading-zero' may be overridden by custom counter styles. We
        // return kNumeric only when we are using the predefined counter style.
        if (!RuntimeEnabledFeatures::CSSAtRuleCounterStyleEnabled() ||
            ListMarker::GetCounterStyle(*GetDocument(), *computed_style)
                .IsPredefined())
          return ax::mojom::blink::ListStyle::kNumeric;
      }
      return ax::mojom::blink::ListStyle::kOther;
    }
    case ListMarker::ListStyleCategory::kStaticString:
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
  if (!ax_object.ParentObjectIncludedInTree())
    return nullptr;
  // Returns next object of parent, since next of |ax_object| isn't appeared on
  // line.
  return ax_object.ParentObjectIncludedInTree()->NextOnLine();
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
    result = NextSiblingIncludingIgnored();
  } else if (ShouldUseLayoutNG(*GetLayoutObject())) {
    result = NextOnLineInternalNG(*this);
  } else {
    InlineBox* inline_box = nullptr;
    if (GetLayoutObject()->IsBox()) {
      inline_box = To<LayoutBox>(GetLayoutObject())->InlineBoxWrapper();
    } else if (GetLayoutObject()->IsLayoutInline()) {
      // For performance and memory consumption, LayoutInline may ignore some
      // inline-boxes during line layout because they don't actually impact
      // layout. This is known as "culled inline". We have to recursively look
      // to the LayoutInline's children via "LastLineBoxIncludingCulling".
      inline_box =
          To<LayoutInline>(GetLayoutObject())->LastLineBoxIncludingCulling();
    } else if (GetLayoutObject()->IsText()) {
      inline_box = To<LayoutText>(GetLayoutObject())->LastTextBox();
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
      AXObject* parent = ParentObjectIncludedInTree();
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
      if (parent && !NextSiblingIncludingIgnored())
        result = parent->NextOnLine();
    }
  }

  // For consistency between the forward and backward directions, try to always
  // return leaf nodes.
  // TODO(accessibility) It doesn't make sense to return aria-owned nodes here,
  // as they could be anywhere in the tree, and not on this line.
  // Should probably only use children from LayoutTreeBuilderTraversal() like
  // AXNodeObject::TextFromDescendants() does.
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
  if (!ax_object.ParentObjectIncludedInTree())
    return nullptr;
  // Returns previous object of parent, since next of |ax_object| isn't appeared
  // on line.
  return ax_object.ParentObjectIncludedInTree()->PreviousOnLine();
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
      inline_box = To<LayoutBox>(GetLayoutObject())->InlineBoxWrapper();
    } else if (GetLayoutObject()->IsLayoutInline()) {
      // For performance and memory consumption, LayoutInline may ignore some
      // inline-boxes during line layout because they don't actually impact
      // layout. This is known as "culled inline". We have to recursively look
      // to the LayoutInline's children via "FirstLineBoxIncludingCulling".
      inline_box =
          To<LayoutInline>(GetLayoutObject())->FirstLineBoxIncludingCulling();
    } else if (GetLayoutObject()->IsText()) {
      inline_box = To<LayoutText>(GetLayoutObject())->FirstTextBox();
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
      AXObject* parent = ParentObjectIncludedInTree();
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
      if (parent && parent->FirstChildIncludingIgnored() == this)
        result = parent->PreviousOnLine();
    }
  }

  // For consistency between the forward and backward directions, try to always
  // return leaf nodes.
  // TODO(accessibility) It doesn't make sense to return aria-owned nodes here,
  // as they could be anywhere in the tree, and not on this line.
  // Should probably only use children from LayoutTreeBuilderTraversal() like
  // AXNodeObject::TextFromDescendants() does.
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

  auto* select_element =
      DynamicTo<HTMLSelectElement>(layout_object_->GetNode());
  if (select_element && select_element->UsesMenuList()) {
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

  if (IsTextControl()) {
    // TODO(https://crbug.com/1165853) For contenteditable, compute on browser
    // side instead.
    return GetText();
  }

  if (layout_object_->IsFileUploadControl())
    return To<LayoutFileUploadControl>(layout_object_)->FileTextValue();

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
      auto* layout_text = To<LayoutText>(layout_object_);
      String visible_text = layout_text->PlainText();  // Actual rendered text.
      // If no text boxes we assume this is unrendered end-of-line whitespace.
      // TODO find robust way to deterministically detect end-of-line space.
      if (visible_text.IsEmpty()) {
        // No visible rendered text -- must be whitespace.
        // Either it is useful whitespace for separating words or not.
        if (layout_text->IsAllCollapsibleWhitespace()) {
          if (LastKnownIsIgnoredValue())
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

  PaintLayer* layer = To<LayoutBox>(layout_object_)->Layer();

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
  AXObject* result = AXObjectCache().GetOrCreate(obj);
  if (!result)
    return nullptr;
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

Document* AXLayoutObject::GetDocument() const {
  if (!GetLayoutObject())
    return nullptr;
  return &GetLayoutObject()->GetDocument();
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
            !layout_row->IsTableRow())
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

}  // namespace blink
