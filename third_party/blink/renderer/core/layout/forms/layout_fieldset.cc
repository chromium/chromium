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

  // Adjust before_child so that it avoids the <legend>.
  if (before_child && before_child->IsRenderedLegend()) {
    const Node* before_node =
        LayoutTreeBuilderTraversal::NextLayoutSibling(*before_child->GetNode());
    before_child = before_node ? before_node->GetLayoutObject() : nullptr;
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
  switch (Style().Display()) {
    case EDisplay::kFlex:
    case EDisplay::kInlineFlex:
      display = EDisplay::kFlex;
      break;
    case EDisplay::kGrid:
    case EDisplay::kInlineGrid:
      display = EDisplay::kGrid;
      break;
    case EDisplay::kGridLanes:
    case EDisplay::kInlineGridLanes:
      display = EDisplay::kGridLanes;
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
  const auto& content_style = fieldset_content->Style();
  const bool is_fixed_container =
      fieldset_content->ComputeIsFixedContainer(content_style);
  fieldset_content->SetCanContainFixedPositionObjects(is_fixed_container);
  fieldset_content->SetCanContainAbsolutePositionObjects(
      fieldset_content->ComputeIsAbsoluteContainer(content_style,
                                                   is_fixed_container));
}

void LayoutFieldset::UpdateAnonymousChildStyle(
    const LayoutObject*,
    ComputedStyleBuilder& child_style_builder) const {
  // Inherit all properties listed here:
  // https://html.spec.whatwg.org/C/#anonymous-fieldset-content-box

  child_style_builder.SetAlignContent(Style().AlignContent());
  child_style_builder.SetAlignItems(Style().AlignItems());

  child_style_builder.SetBorderBottomLeftRadius(
      Style().BorderBottomLeftRadius());
  child_style_builder.SetBorderBottomRightRadius(
      Style().BorderBottomRightRadius());
  child_style_builder.SetBorderTopLeftRadius(Style().BorderTopLeftRadius());
  child_style_builder.SetBorderTopRightRadius(Style().BorderTopRightRadius());

  child_style_builder.SetPaddingTop(Style().PaddingTop());
  child_style_builder.SetPaddingRight(Style().PaddingRight());
  child_style_builder.SetPaddingBottom(Style().PaddingBottom());
  child_style_builder.SetPaddingLeft(Style().PaddingLeft());

  child_style_builder.SetBoxDecorationBreak(Style().BoxDecorationBreak());

  if (Style().SpecifiesColumns() && AllowsColumns()) {
    if (!Style().HasAutoColumnCount()) {
      child_style_builder.SetColumnCount(Style().ColumnCount());
    }
    if (!Style().HasAutoColumnWidth()) {
      child_style_builder.SetColumnWidth(Style().ColumnWidth());
    }
    if (!Style().HasAutoColumnHeight()) {
      child_style_builder.SetColumnHeight(Style().ColumnHeight());
    }
    child_style_builder.SetColumnWrap(Style().ColumnWrap());
  }
  child_style_builder.SetColumnGap(Style().ColumnGap());
  child_style_builder.SetColumnFill(Style().GetColumnFill());
  child_style_builder.SetColumnRuleColor(Style().ColumnRuleColor());
  child_style_builder.SetRowRuleColor(Style().RowRuleColor());
  child_style_builder.SetColumnRuleStyle(Style().ColumnRuleStyle());
  child_style_builder.SetRowRuleStyle(Style().RowRuleStyle());
  child_style_builder.SetColumnRuleWidth(
      GapDataList<int>(Style().ColumnRuleWidth()));
  child_style_builder.SetRowRuleWidth(GapDataList<int>(Style().RowRuleWidth()));

  child_style_builder.SetColumnRuleBreak(Style().ColumnRuleBreak());
  child_style_builder.SetRowRuleBreak(Style().RowRuleBreak());
  child_style_builder.SetRuleOverlap(Style().RuleOverlap());
  child_style_builder.SetColumnRuleVisibilityItems(
      Style().ColumnRuleVisibilityItems());
  child_style_builder.SetRowRuleVisibilityItems(
      Style().RowRuleVisibilityItems());
  child_style_builder.SetColumnRuleInsetCapStart(
      Style().ColumnRuleInsetCapStart());
  child_style_builder.SetColumnRuleInsetCapEnd(Style().ColumnRuleInsetCapEnd());
  child_style_builder.SetColumnRuleInsetJunctionStart(
      Style().ColumnRuleInsetJunctionStart());
  child_style_builder.SetColumnRuleInsetJunctionEnd(
      Style().ColumnRuleInsetJunctionEnd());
  child_style_builder.SetRowRuleInsetCapStart(Style().RowRuleInsetCapStart());
  child_style_builder.SetRowRuleInsetCapEnd(Style().RowRuleInsetCapEnd());
  child_style_builder.SetRowRuleInsetJunctionStart(
      Style().RowRuleInsetJunctionStart());
  child_style_builder.SetRowRuleInsetJunctionEnd(
      Style().RowRuleInsetJunctionEnd());

  child_style_builder.SetFlexDirection(Style().FlexDirection());
  child_style_builder.SetFlexWrap(Style().FlexWrap());

  child_style_builder.SetGridAutoColumns(Style().GridAutoColumns());
  child_style_builder.SetGridAutoFlow(Style().GetGridAutoFlow());
  child_style_builder.SetGridAutoRows(Style().GridAutoRows());
  child_style_builder.SetGridColumnEnd(Style().GridColumnEnd());
  child_style_builder.SetGridColumnStart(Style().GridColumnStart());
  child_style_builder.SetGridRowEnd(Style().GridRowEnd());
  child_style_builder.SetGridRowStart(Style().GridRowStart());

  // grid-template-columns, grid-template-rows, grid-template-areas
  child_style_builder.SetGridTemplateColumns(
      Style().SpecifiedGridTemplateColumns());
  child_style_builder.SetGridTemplateRows(Style().SpecifiedGridTemplateRows());
  child_style_builder.SetGridTemplateAreas(Style().GridTemplateAreas());

  child_style_builder.SetRowGap(Style().RowGap());

  child_style_builder.SetJustifyContent(Style().JustifyContent());
  child_style_builder.SetJustifyItems(Style().JustifyItems());
  child_style_builder.SetOverflowX(Style().OverflowX());
  child_style_builder.SetOverflowY(Style().OverflowY());
  child_style_builder.SetScrollbarGutter(Style().ScrollbarGutter());
  child_style_builder.SetUnicodeBidi(Style().GetUnicodeBidi());
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
