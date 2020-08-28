// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_fieldset.h"

#include "third_party/blink/renderer/core/layout/layout_fieldset.h"
#include "third_party/blink/renderer/core/layout/layout_object_factory.h"

namespace blink {

LayoutNGFieldset::LayoutNGFieldset(Element* element)
    : LayoutNGBlockFlow(element) {
  SetChildrenInline(false);
}

void LayoutNGFieldset::AddChild(LayoutObject* new_child,
                                LayoutObject* before_child) {
  LayoutBlock* fieldset_content = To<LayoutBlock>(FirstChild());
  if (!fieldset_content) {
    // We wrap everything inside an anonymous child, which will take care of the
    // fieldset contents. This parent will only be responsible for the fieldset
    // border and the rendered legend, if there is one. Everything else will be
    // done by the anonymous child. This includes display type, multicol,
    // scrollbars, and even padding. Note that the rendered legend (if any) will
    // also be a child of the anonymous object, although it'd be more natural to
    // have it as the first child of this object. The reason is that our layout
    // object tree builder cannot handle such discrepancies between DOM tree and
    // layout tree. Inserting anonymous wrappers is one thing (that is
    // supported). Removing it from its actual DOM siblings and putting it
    // elsewhere, on the other hand, does not work well.

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

    fieldset_content =
        LayoutBlock::CreateAnonymousWithParentAndDisplay(this, display);
    LayoutBox::AddChild(fieldset_content);
  }
  fieldset_content->AddChild(new_child, before_child);
}

// TODO(mstensho): Should probably remove the anonymous child if it becomes
// childless. While an empty anonymous child should have no effect, it doesn't
// seem right to leave it around.

void LayoutNGFieldset::UpdateAnonymousChildStyle(
    const LayoutObject*,
    ComputedStyle& child_style) const {
  // Inherit all properties listed here:
  // https://html.spec.whatwg.org/C/#anonymous-fieldset-content-box

  child_style.SetAlignContent(StyleRef().AlignContent());
  child_style.SetAlignItems(StyleRef().AlignItems());

  child_style.SetBorderBottomLeftRadius(StyleRef().BorderBottomLeftRadius());
  child_style.SetBorderBottomRightRadius(StyleRef().BorderBottomRightRadius());
  child_style.SetBorderTopLeftRadius(StyleRef().BorderTopLeftRadius());
  child_style.SetBorderTopRightRadius(StyleRef().BorderTopRightRadius());

  child_style.SetPaddingTop(StyleRef().PaddingTop());
  child_style.SetPaddingRight(StyleRef().PaddingRight());
  child_style.SetPaddingBottom(StyleRef().PaddingBottom());
  child_style.SetPaddingLeft(StyleRef().PaddingLeft());

  if (StyleRef().SpecifiesColumns()) {
    child_style.SetColumnCount(StyleRef().ColumnCount());
    child_style.SetColumnWidth(StyleRef().ColumnWidth());
  } else {
    child_style.SetHasAutoColumnCount();
    child_style.SetHasAutoColumnWidth();
  }
  child_style.SetColumnGap(StyleRef().ColumnGap());
  child_style.SetColumnFill(StyleRef().GetColumnFill());
  child_style.SetColumnRuleColor(StyleColor(
      LayoutObject::ResolveColor(StyleRef(), GetCSSPropertyColumnRuleColor())));
  child_style.SetColumnRuleStyle(StyleRef().ColumnRuleStyle());
  child_style.SetColumnRuleWidth(StyleRef().ColumnRuleWidth());

  child_style.SetFlexDirection(StyleRef().FlexDirection());
  child_style.SetFlexWrap(StyleRef().FlexWrap());

  child_style.SetGridAutoColumns(StyleRef().GridAutoColumns());
  child_style.SetGridAutoFlow(StyleRef().GetGridAutoFlow());
  child_style.SetGridAutoRows(StyleRef().GridAutoRows());
  child_style.SetGridColumnEnd(StyleRef().GridColumnEnd());
  child_style.SetGridColumnStart(StyleRef().GridColumnStart());
  child_style.SetGridRowEnd(StyleRef().GridRowEnd());
  child_style.SetGridRowStart(StyleRef().GridRowStart());
  child_style.SetGridTemplateColumns(StyleRef().GridTemplateColumns());
  child_style.SetGridTemplateRows(StyleRef().GridTemplateRows());
  child_style.SetNamedGridArea(StyleRef().NamedGridArea());
  child_style.SetNamedGridAreaColumnCount(
      StyleRef().NamedGridAreaColumnCount());
  child_style.SetNamedGridAreaRowCount(StyleRef().NamedGridAreaRowCount());
  child_style.SetRowGap(StyleRef().RowGap());

  child_style.SetJustifyContent(StyleRef().JustifyContent());
  child_style.SetJustifyItems(StyleRef().JustifyItems());
  child_style.SetOverflowX(StyleRef().OverflowX());
  child_style.SetOverflowY(StyleRef().OverflowY());
  child_style.SetUnicodeBidi(StyleRef().GetUnicodeBidi());
}

bool LayoutNGFieldset::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGFieldset || LayoutNGBlockFlow::IsOfType(type);
}

void LayoutNGFieldset::InvalidatePaint(
    const PaintInvalidatorContext& context) const {
  // Fieldset's box decoration painting depends on the legend geometry.
  const LayoutBox* legend_box = LayoutFieldset::FindInFlowLegend(*this);
  if (legend_box && legend_box->ShouldCheckGeometryForPaintInvalidation()) {
    GetMutableForPainting().SetShouldDoFullPaintInvalidation(
        PaintInvalidationReason::kGeometry);
  }
  LayoutNGBlockFlow::InvalidatePaint(context);
}

bool LayoutNGFieldset::BackgroundIsKnownToBeOpaqueInRect(
    const PhysicalRect& local_rect) const {
  // If the field set has a legend, then it probably does not completely fill
  // its background.
  if (LayoutFieldset::FindInFlowLegend(*this))
    return false;

  return LayoutBlockFlow::BackgroundIsKnownToBeOpaqueInRect(local_rect);
}

LayoutUnit LayoutNGFieldset::ScrollWidth() const {
  const LayoutObject* child = FirstChild();
  if (child && child->IsAnonymous())
    return ToLayoutBox(child)->ScrollWidth();
  return LayoutNGBlockFlow::ScrollWidth();
}

LayoutUnit LayoutNGFieldset::ScrollHeight() const {
  const LayoutObject* child = FirstChild();
  if (child && child->IsAnonymous())
    return ToLayoutBox(child)->ScrollHeight();
  return LayoutNGBlockFlow::ScrollHeight();
}

}  // namespace blink
