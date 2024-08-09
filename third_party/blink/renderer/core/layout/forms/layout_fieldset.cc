// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/forms/layout_fieldset.h"

#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"

namespace blink {

LayoutFieldset::LayoutFieldset(Element* element) : LayoutBlockFlow(element) {
  SetChildrenInline(false);
}

LayoutBlock* LayoutFieldset::FindAnonymousFieldsetContentBox() const {
  LayoutObject* first_child = FirstChild();
  if (!first_child) {
    return nullptr;
  }
  if (first_child->IsAnonymous()) {
    return To<LayoutBlock>(first_child);
  }
  LayoutObject* last_child = first_child->NextSibling();
  DCHECK(!last_child || !last_child->NextSibling());
  if (last_child && last_child->IsAnonymous()) {
    return To<LayoutBlock>(last_child);
  }
  return nullptr;
}

void LayoutFieldset::AddChild(LayoutObject* new_child,
                              LayoutObject* before_child) {
  if (!new_child->IsText() && !new_child->IsAnonymous()) {
    // Adding a child LayoutObject always causes reattach of <fieldset>. So
    // |before_child| is always nullptr.
    // See HTMLFieldSetElement::DidRecalcStyle().
    DCHECK(!before_child);
  } else if (before_child && before_child->IsRenderedLegend()) {
    // Whitespace changes resulting from removed nodes are handled in
    // MarkForWhitespaceReattachment(), and don't trigger
    // HTMLFieldSetElement::DidRecalcStyle(). So the fieldset is not
    // reattached. We adjust |before_child| instead.
    Node* before_node =
        LayoutTreeBuilderTraversal::NextLayoutSibling(*before_child->GetNode());
    before_child = before_node ? before_node->GetLayoutObject() : nullptr;
  }

  // https://html.spec.whatwg.org/C/#the-fieldset-and-legend-elements
  // > * If the element has a rendered legend, then that element is expected
  // >   to be the first child box.
  // > * The anonymous fieldset content box is expected to appear after the
  // >   rendered legend and is expected to contain the content (including
  // >   the '::before' and '::after' pseudo-elements) of the fieldset
  // >   element except for the rendered legend, if there is one.

  if (new_child->IsRenderedLegendCandidate() && !FindInFlowLegend()) {
    LayoutBlockFlow::AddChild(new_child, FirstChild());
    return;
  }
  LayoutBlock* fieldset_content = FindAnonymousFieldsetContentBox();
  DCHECK(fieldset_content);
  fieldset_content->AddChild(new_child, before_child);
}

void LayoutFieldset::InsertedIntoTree() {
  LayoutBlockFlow::InsertedIntoTree();

  if (FindAnonymousFieldsetContentBox()) {
    return;
  }

  // We wrap everything inside an anonymous child, which will take care of the
  // fieldset contents. This parent will only be responsible for the fieldset
  // border and the rendered legend, if there is one. Everything else will be
  // done by the anonymous child. This includes display type, multicol,
  // scrollbars, and even padding.

  // TODO(crbug.com/875235): Consider other display types not mentioned in the
  // spec (ex. EDisplay::kLayoutCustom).
  EDisplay display = EDisplay::kFlowRoot;
  switch (StyleRef().Display()) {
    case EDisplay::kFlex:
    case EDisplay::kInlineFlex:
      display = EDisplay::kFlex;
      break;
    case EDisplay::kGrid:
    case EDisplay::kInlineGrid:
      display = EDisplay::kGrid;
      break;
    default:
      break;
  }

  LayoutBlock* fieldset_content =
      LayoutBlock::CreateAnonymousWithParentAndDisplay(this, display);
  LayoutBox::AddChild(fieldset_content);
  // Update CanContain*PositionObjects flag again though
  // CreateAnonymousWithParentAndDisplay() already called them because
  // ComputeIs*Container() depends on Parent().
  fieldset_content->SetCanContainAbsolutePositionObjects(
      fieldset_content->ComputeIsAbsoluteContainer(fieldset_content->Style()));
  fieldset_content->SetCanContainFixedPositionObjects(
      fieldset_content->ComputeIsFixedContainer(fieldset_content->Style()));
}

void LayoutFieldset::UpdateAnonymousChildStyle(
    const LayoutObject*,
    ComputedStyleBuilder& child_style_builder) const {
  // Inherit all properties listed here:
  // https://html.spec.whatwg.org/C/#anonymous-fieldset-content-box

  child_style_builder.SetAlignContent(StyleRef().AlignContent());
  child_style_builder.SetAlignItems(StyleRef().AlignItems());

  child_style_builder.SetBorderBottomLeftRadius(
      StyleRef().BorderBottomLeftRadius());
  child_style_builder.SetBorderBottomRightRadius(
      StyleRef().BorderBottomRightRadius());
  child_style_builder.SetBorderTopLeftRadius(StyleRef().BorderTopLeftRadius());
  child_style_builder.SetBorderTopRightRadius(
      StyleRef().BorderTopRightRadius());

  child_style_builder.SetPaddingTop(StyleRef().PaddingTop());
  child_style_builder.SetPaddingRight(StyleRef().PaddingRight());
  child_style_builder.SetPaddingBottom(StyleRef().PaddingBottom());
  child_style_builder.SetPaddingLeft(StyleRef().PaddingLeft());

  child_style_builder.SetBoxDecorationBreak(StyleRef().BoxDecorationBreak());

  if (StyleRef().SpecifiesColumns() && AllowsColumns()) {
    child_style_builder.SetColumnCount(StyleRef().ColumnCount());
    child_style_builder.SetColumnWidth(StyleRef().ColumnWidth());
  } else {
    child_style_builder.SetHasAutoColumnCount();
    child_style_builder.SetHasAutoColumnWidth();
  }
  child_style_builder.SetColumnGap(StyleRef().ColumnGap());
  child_style_builder.SetColumnFill(StyleRef().GetColumnFill());
  child_style_builder.SetColumnRuleColor(StyleColor(
      LayoutObject::ResolveColor(StyleRef(), GetCSSPropertyColumnRuleColor())));
  child_style_builder.SetColumnRuleStyle(StyleRef().ColumnRuleStyle());
  child_style_builder.SetColumnRuleWidth(StyleRef().ColumnRuleWidth());

  child_style_builder.SetFlexDirection(StyleRef().FlexDirection());
  child_style_builder.SetFlexWrap(StyleRef().FlexWrap());

  child_style_builder.SetGridAutoColumns(StyleRef().GridAutoColumns());
  child_style_builder.SetGridAutoFlow(StyleRef().GetGridAutoFlow());
  child_style_builder.SetGridAutoRows(StyleRef().GridAutoRows());
  child_style_builder.SetGridColumnEnd(StyleRef().GridColumnEnd());
  child_style_builder.SetGridColumnStart(StyleRef().GridColumnStart());
  child_style_builder.SetGridRowEnd(StyleRef().GridRowEnd());
  child_style_builder.SetGridRowStart(StyleRef().GridRowStart());

  // grid-template-columns, grid-template-rows, grid-template-areas
  child_style_builder.SetGridTemplateColumns(StyleRef().GridTemplateColumns());
  child_style_builder.SetGridTemplateRows(StyleRef().GridTemplateRows());
  child_style_builder.SetGridTemplateAreas(StyleRef().GridTemplateAreas());

  child_style_builder.SetRowGap(StyleRef().RowGap());

  child_style_builder.SetJustifyContent(StyleRef().JustifyContent());
  child_style_builder.SetJustifyItems(StyleRef().JustifyItems());
  child_style_builder.SetOverflowX(StyleRef().OverflowX());
  child_style_builder.SetOverflowY(StyleRef().OverflowY());
  child_style_builder.SetUnicodeBidi(StyleRef().GetUnicodeBidi());

  // scroll-start
  child_style_builder.SetScrollStartX(StyleRef().ScrollStartX());
  child_style_builder.SetScrollStartY(StyleRef().ScrollStartY());
}

void LayoutFieldset::InvalidatePaint(
    const PaintInvalidatorContext& context) const {
  // Fieldset's box decoration painting depends on the legend geometry.
  const LayoutBox* legend_box = FindInFlowLegend();
  if (legend_box && legend_box->ShouldCheckLayoutForPaintInvalidation()) {
    GetMutableForPainting().SetShouldDoFullPaintInvalidation(
        PaintInvalidationReason::kLayout);
  }
  LayoutBlockFlow::InvalidatePaint(context);
}

bool LayoutFieldset::BackgroundIsKnownToBeOpaqueInRect(
    const PhysicalRect& local_rect) const {
  // If the field set has a legend, then it probably does not completely fill
  // its background.
  if (FindInFlowLegend()) {
    return false;
  }

  return LayoutBlockFlow::BackgroundIsKnownToBeOpaqueInRect(local_rect);
}

LayoutUnit LayoutFieldset::ScrollWidth() const {
  if (const auto* content = FindAnonymousFieldsetContentBox()) {
    return content->ScrollWidth();
  }
  return LayoutBlockFlow::ScrollWidth();
}

LayoutUnit LayoutFieldset::ScrollHeight() const {
  if (const auto* content = FindAnonymousFieldsetContentBox()) {
    return content->ScrollHeight();
  }
  return LayoutBlockFlow::ScrollHeight();
}

// static
LayoutBox* LayoutFieldset::FindInFlowLegend(const LayoutBlock& fieldset) {
  DCHECK(fieldset.IsFieldset());
  for (LayoutObject* legend = fieldset.FirstChild(); legend;
       legend = legend->NextSibling()) {
    if (legend->IsRenderedLegendCandidate()) {
      return To<LayoutBox>(legend);
    }
  }
  return nullptr;
}

}  // namespace blink
