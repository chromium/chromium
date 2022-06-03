// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/menu_list_inner_element.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

MenuListInnerElement::MenuListInnerElement(Document& document)
    : HTMLDivElement(document) {
  SetHasCustomStyleCallbacks();
}

scoped_refptr<ComputedStyle> MenuListInnerElement::CustomStyleForLayoutObject(
    const StyleRecalcContext& style_recalc_context) {
  const ComputedStyle& parent_style = OwnerShadowHost()->ComputedStyleRef();
  scoped_refptr<ComputedStyle> style =
      GetDocument().GetStyleResolver().CreateAnonymousStyleWithDisplay(
          parent_style, EDisplay::kBlock);
  style->SetFlexGrow(1);
  style->SetFlexShrink(1);
  // min-width: 0; is needed for correct shrinking.
  style->SetMinWidth(Length::Fixed(0));
  style->SetHasLineIfEmpty(true);
  style->SetOverflowX(EOverflow::kHidden);
  style->SetOverflowY(EOverflow::kHidden);
  style->SetShouldIgnoreOverflowPropertyForInlineBlockBaseline();
  style->SetTextOverflow(parent_style.TextOverflow());
  style->SetUserModify(EUserModify::kReadOnly);

  if (style->LineHeight() == ComputedStyleInitialValues::InitialLineHeight()) {
    // line-height should be consistent with MenuListIntrinsicBlockSize()
    // in layout_box.cc.
    const SimpleFontData* font_data = style->GetFont().PrimaryFont();
    if (font_data)
      style->SetLineHeight(Length::Fixed(font_data->GetFontMetrics().Height()));
    else
      style->SetLineHeight(Length::Fixed(style->FontSize()));
  }

  // Use margin:auto instead of align-items:center to get safe centering, i.e.
  // when the content overflows, treat it the same as align-items: flex-start.
  // But we only do that for the cases where html.css would otherwise use
  // center.
  if (parent_style.AlignItemsPosition() == ItemPosition::kCenter) {
    style->SetMarginTop(Length());
    style->SetMarginBottom(Length());
    style->SetAlignSelfPosition(ItemPosition::kFlexStart);
  }

  // We set margin-left/right instead of padding-left/right to clip text by
  // 'overflow: hidden'.
  LayoutTheme& theme = LayoutTheme::GetTheme();
  Length margin_start =
      Length::Fixed(theme.PopupInternalPaddingStart(parent_style));
  Length margin_end = Length::Fixed(
      theme.PopupInternalPaddingEnd(GetDocument().GetFrame(), parent_style));
  if (parent_style.IsLeftToRightDirection()) {
    style->SetMarginLeft(margin_start);
    style->SetMarginRight(margin_end);
  } else {
    style->SetMarginLeft(margin_end);
    style->SetMarginRight(margin_start);
  }
  style->SetTextAlign(parent_style.GetTextAlign(true));
  style->SetPaddingTop(
      Length::Fixed(theme.PopupInternalPaddingTop(parent_style)));
  style->SetPaddingBottom(
      Length::Fixed(theme.PopupInternalPaddingBottom(parent_style)));

  if (const ComputedStyle* option_style =
          To<HTMLSelectElement>(OwnerShadowHost())->OptionStyle()) {
    style->SetDirection(option_style->Direction());
    style->SetUnicodeBidi(option_style->GetUnicodeBidi());
  }

  return style;
}

}  // namespace blink
