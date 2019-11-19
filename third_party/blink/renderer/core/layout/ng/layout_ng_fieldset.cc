// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_fieldset.h"

#include "third_party/blink/renderer/core/layout/layout_object_factory.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"

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

    scoped_refptr<ComputedStyle> new_style =
        ComputedStyle::CreateAnonymousStyleWithDisplay(StyleRef(),
                                                       EDisplay::kFlowRoot);
    // TODO(crbug.com/875235): When the paint code is ready for anonymous
    // scrollable containers, inherit overflow-x and overflow-y here.

    new_style->SetPaddingTop(StyleRef().PaddingTop());
    new_style->SetPaddingRight(StyleRef().PaddingRight());
    new_style->SetPaddingBottom(StyleRef().PaddingBottom());
    new_style->SetPaddingLeft(StyleRef().PaddingLeft());

    if (StyleRef().SpecifiesColumns()) {
      new_style->SetColumnCount(StyleRef().ColumnCount());
      new_style->SetColumnWidth(StyleRef().ColumnWidth());
    } else {
      new_style->SetHasAutoColumnCount();
      new_style->SetHasAutoColumnWidth();
    }
    new_style->SetColumnGap(StyleRef().ColumnGap());
    new_style->SetColumnFill(StyleRef().GetColumnFill());

    new_style->SetFlexDirection(StyleRef().FlexDirection());
    new_style->SetJustifyContent(StyleRef().JustifyContent());

    // TODO(mstensho): Inherit all properties listed here:
    // https://html.spec.whatwg.org/C/#the-fieldset-and-legend-elements

    fieldset_content = LayoutBlock::CreateAnonymousWithParentAndDisplay(
        this, new_style->Display());
    fieldset_content->SetStyle(std::move(new_style));
    LayoutBox::AddChild(fieldset_content);
  }
  fieldset_content->AddChild(new_child, before_child);
}

// TODO(mstensho): Should probably remove the anonymous child if it becomes
// childless. While an empty anonymous child should have no effect, it doesn't
// seem right to leave it around.

bool LayoutNGFieldset::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGFieldset || LayoutNGBlockFlow::IsOfType(type);
}

void LayoutNGFieldset::Paint(const PaintInfo& paint_info) const {
  // TODO(crbug.com/988015): This override should not be needed when painting
  // fragment is enabled in parent classes.
  if (!RuntimeEnabledFeatures::LayoutNGFragmentPaintEnabled()) {
    if (const NGPhysicalBoxFragment* fragment = CurrentFragment()) {
      NGBoxFragmentPainter(*fragment, PaintFragment()).Paint(paint_info);
      return;
    }
  }

  LayoutNGBlockFlow::Paint(paint_info);
}

}  // namespace blink
