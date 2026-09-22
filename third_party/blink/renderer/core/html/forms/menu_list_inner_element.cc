// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/menu_list_inner_element.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

MenuListInnerElement::MenuListInnerElement(Document& document)
    : HTMLDivElement(document) {
  SetHasCustomStyleCallbacks();
}

const ComputedStyle* MenuListInnerElement::CustomStyleForLayoutObject(
    const StyleRecalcContext& style_recalc_context) {
  const ComputedStyle& parent_style = OwnerShadowHost()->ComputedStyleRef();

  if (parent_style.HasBaseEffectiveAppearance()) {
    return HTMLDivElement::CustomStyleForLayoutObject(style_recalc_context);
  }

  ComputedStyleBuilder style_builder =
      GetDocument().GetStyleResolver().CreateAnonymousStyleBuilderWithDisplay(
          parent_style, EDisplay::kBlock);

  style_builder.SetFlexGrow(1);
  style_builder.SetFlexShrink(1);
  // min-width: 0; is needed for correct shrinking.
  style_builder.SetMinWidth(Length::Fixed(0));
  if (parent_style.ApplyControlFixedSize(OwnerShadowHost())) {
    style_builder.SetHasLineIfEmpty(true);
  }

  // Clip in the inline direction in order to prevent text from overlapping with
  // the dropdown icon, but don't clip in the block direction to prevent certain
  // fonts from being unexpectedly clipped.
  // https://issues.chromium.org/issues/41144858
  // https://issues.chromium.org/issues/40805967
  // https://issues.chromium.org/issues/379805732
  // https://issues.chromium.org/issues/515072442
  style_builder.SetTextOverflow(parent_style.TextOverflow());
  if (!RuntimeEnabledFeatures::SelectRemoveOverflowHiddenEnabled()) {
    style_builder.SetOverflowX(EOverflow::kHidden);
    style_builder.SetOverflowY(EOverflow::kHidden);
    style_builder.SetShouldIgnoreOverflowPropertyForInlineBlockBaseline();
  } else if (IsHorizontalWritingMode(style_builder.GetWritingMode())) {
    style_builder.SetOverflowY(EOverflow::kVisible);
    style_builder.SetOverflowX(EOverflow::kClip);
  } else {
    style_builder.SetOverflowY(EOverflow::kClip);
    style_builder.SetOverflowX(EOverflow::kVisible);
  }

  style_builder.SetUserModify(EUserModify::kReadOnly);

  if (style_builder.HasInitialLineHeight()) {
    // line-height should be consistent with MenuListIntrinsicBlockSize()
    // in layout_box.cc.
    const SimpleFontData* font_data = style_builder.GetFont()->PrimaryFont();
    if (font_data) {
      style_builder.SetLineHeight(
          Length::Fixed(font_data->GetFontMetrics().Height()));
    } else {
      style_builder.SetLineHeight(Length::Fixed(style_builder.FontSize()));
    }
  }

  // Use margin:auto instead of align-items:center to get safe centering, i.e.
  // when the content overflows, treat it the same as align-items: flex-start.
  // But we only do that for the cases where html.css would otherwise use
  // center.
  if (parent_style.AlignItems().GetComputedPosition() ==
          ItemPosition::kCenter ||
      parent_style.AlignItems().GetComputedPosition() ==
          ItemPosition::kAnchorCenter) {
    style_builder.SetMarginTop(Length());
    style_builder.SetMarginBottom(Length());
    style_builder.SetAlignSelf(StyleSelfAlignmentData(
        ItemPosition::kStart, OverflowAlignment::kDefault));
  }

  // We set margin-* instead of padding-* to clip text by 'overflow: hidden'.
  LogicalToPhysicalSetter margin_setter(style_builder.GetWritingDirection(),
                                        style_builder,
                                        &ComputedStyleBuilder::SetMarginTop,
                                        &ComputedStyleBuilder::SetMarginRight,
                                        &ComputedStyleBuilder::SetMarginBottom,
                                        &ComputedStyleBuilder::SetMarginLeft);
  LayoutTheme& theme = LayoutTheme::GetTheme();
  Length margin_start =
      Length::Fixed(theme.PopupInternalPaddingStart(parent_style));
  Length margin_end = Length::Fixed(
      theme.PopupInternalPaddingEnd(GetDocument().GetFrame(), parent_style));
  margin_setter.SetInlineEnd(margin_end);
  margin_setter.SetInlineStart(margin_start);
  style_builder.SetTextAlign(parent_style.GetTextAlign(true));
  LogicalToPhysicalSetter padding_setter(
      style_builder.GetWritingDirection(), style_builder,
      &ComputedStyleBuilder::SetPaddingTop,
      &ComputedStyleBuilder::SetPaddingRight,
      &ComputedStyleBuilder::SetPaddingBottom,
      &ComputedStyleBuilder::SetPaddingLeft);
  padding_setter.SetBlockStart(
      Length::Fixed(theme.PopupInternalPaddingTop(parent_style)));
  padding_setter.SetBlockEnd(
      Length::Fixed(theme.PopupInternalPaddingBottom(parent_style)));

  if (const ComputedStyle* option_style =
          To<HTMLSelectElement>(OwnerShadowHost())->OptionStyle()) {
    style_builder.SetDirection(option_style->Direction());
    style_builder.SetUnicodeBidi(option_style->GetUnicodeBidi());
  }

  return style_builder.TakeStyle();
}

}  // namespace blink
