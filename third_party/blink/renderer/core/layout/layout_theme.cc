/**
 * This file is part of the theme implementation for form controls in WebCore.
 *
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/layout/layout_theme.h"

#include "build/build_config.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_options_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/forms/spin_button_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_inner_elements.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_meter_element.h"
#include "third_party/blink/renderer/core/html/html_progress_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_utils.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_theme_font_provider.h"
#include "third_party/blink/renderer/core/layout/layout_theme_mobile.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_initial_values.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_helper.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/base/ui_base_features.h"
#include "ui/native_theme/native_theme.h"

// The methods in this file are shared by all themes on every platform.

namespace blink {

using mojom::blink::FormControlType;

namespace {

// This function should match to the user-agent stylesheet.
ControlPart AutoAppearanceFor(const Element& element) {
  if (IsA<HTMLButtonElement>(element))
    return kButtonPart;
  if (IsA<HTMLMeterElement>(element))
    return kMeterPart;
  if (IsA<HTMLProgressElement>(element))
    return kProgressBarPart;
  if (IsA<HTMLTextAreaElement>(element))
    return kTextAreaPart;
  if (IsA<SpinButtonElement>(element))
    return kInnerSpinButtonPart;
  if (const auto* select = DynamicTo<HTMLSelectElement>(element))
    return select->UsesMenuList() ? kMenulistPart : kListboxPart;

  if (const auto* input = DynamicTo<HTMLInputElement>(element))
    return input->AutoAppearance();

  if (element.IsInUserAgentShadowRoot()) {
    const AtomicString& id_value =
        element.FastGetAttribute(html_names::kIdAttr);
    if (id_value == shadow_element_names::kIdSliderThumb)
      return kSliderThumbHorizontalPart;
    if (id_value == shadow_element_names::kIdSearchClearButton ||
        id_value == shadow_element_names::kIdClearButton)
      return kSearchFieldCancelButtonPart;

    // Slider container elements and -webkit-meter-inner-element don't have IDs.
    if (IsSliderContainer(element))
      return kSliderHorizontalPart;
    if (element.ShadowPseudoId() ==
        shadow_element_names::kPseudoMeterInnerElement)
      return kMeterPart;
  }
  return kNoControlPart;
}

void ResetBorder(ComputedStyleBuilder& builder) {
  builder.ResetBorderImage();
  builder.ResetBorderTopStyle();
  builder.ResetBorderTopWidth();
  builder.ResetBorderTopColor();
  builder.ResetBorderRightStyle();
  builder.ResetBorderRightWidth();
  builder.ResetBorderRightColor();
  builder.ResetBorderBottomStyle();
  builder.ResetBorderBottomWidth();
  builder.ResetBorderBottomColor();
  builder.ResetBorderLeftStyle();
  builder.ResetBorderLeftWidth();
  builder.ResetBorderLeftColor();
  builder.ResetBorderTopLeftRadius();
  builder.ResetBorderTopRightRadius();
  builder.ResetBorderBottomLeftRadius();
  builder.ResetBorderBottomRightRadius();
}

void ResetPadding(ComputedStyleBuilder& builder) {
  builder.ResetPaddingTop();
  builder.ResetPaddingRight();
  builder.ResetPaddingBottom();
  builder.ResetPaddingLeft();
}

}  // namespace

LayoutTheme& LayoutTheme::GetTheme() {
  if (RuntimeEnabledFeatures::MobileLayoutThemeEnabled()) {
    DEFINE_STATIC_REF(LayoutTheme, layout_theme_mobile,
                      (LayoutThemeMobile::Create()));
    return *layout_theme_mobile;
  }
  return NativeTheme();
}

LayoutTheme::LayoutTheme() : has_custom_focus_ring_color_(false) {
}

ControlPart LayoutTheme::AdjustAppearanceWithAuthorStyle(
    ControlPart part,
    const ComputedStyleBuilder& builder) {
  if (IsControlStyled(part, builder))
    return part == kMenulistPart ? kMenulistButtonPart : kNoControlPart;
  return part;
}

ControlPart LayoutTheme::AdjustAppearanceWithElementType(
    const ComputedStyleBuilder& builder,
    const Element* element) {
  ControlPart part = builder.EffectiveAppearance();
  if (!element)
    return kNoControlPart;

  ControlPart auto_appearance = AutoAppearanceFor(*element);
  if (part == auto_appearance)
    return part;

  switch (part) {
    // No restrictions.
    case kNoControlPart:
    case kMediaSliderPart:
    case kMediaSliderThumbPart:
    case kMediaVolumeSliderPart:
    case kMediaVolumeSliderThumbPart:
    case kMediaControlPart:
      return part;
    case kBaseSelectPart:
      CHECK(RuntimeEnabledFeatures::CustomizableSelectEnabled());
      return IsA<HTMLSelectElement>(element) ||
                     HTMLSelectElement::IsPopoverForAppearanceBase(element)
                 ? part
                 : auto_appearance;

    // Aliases of 'auto'.
    // https://drafts.csswg.org/css-ui-4/#typedef-appearance-compat-auto
    case kAutoPart:
    case kCheckboxPart:
    case kRadioPart:
    case kPushButtonPart:
    case kSquareButtonPart:
    case kInnerSpinButtonPart:
    case kListboxPart:
    case kMenulistPart:
    case kMeterPart:
    case kProgressBarPart:
    case kSliderHorizontalPart:
    case kSliderThumbHorizontalPart:
    case kSearchFieldPart:
    case kSearchFieldCancelButtonPart:
    case kTextAreaPart:
      return auto_appearance;

      // The following keywords should work well for some element types
      // even if their default appearances are different from the keywords.

    case kButtonPart:
      return (auto_appearance == kPushButtonPart ||
              auto_appearance == kSquareButtonPart)
                 ? part
                 : auto_appearance;

    case kMenulistButtonPart:
      return auto_appearance == kMenulistPart ? part : auto_appearance;

    case kSliderVerticalPart:
      return auto_appearance == kSliderHorizontalPart ? part : auto_appearance;

    case kSliderThumbVerticalPart:
      return auto_appearance == kSliderThumbHorizontalPart ? part
                                                           : auto_appearance;

    case kTextFieldPart:
      if (const auto* input_element = DynamicTo<HTMLInputElement>(*element);
          input_element &&
          input_element->FormControlType() == FormControlType::kInputSearch) {
        return part;
      }
      return auto_appearance;
  }

  return part;
}

void LayoutTheme::AdjustStyle(const Element* element,
                              ComputedStyleBuilder& builder) {
  ControlPart original_part = builder.Appearance();
  builder.SetEffectiveAppearance(original_part);
  if (original_part == ControlPart::kNoControlPart)
    return;

  // Force inline and table display styles to be inline-block (except for table-
  // which is block)
  if (builder.Display() == EDisplay::kInline ||
      builder.Display() == EDisplay::kInlineTable ||
      builder.Display() == EDisplay::kTableRowGroup ||
      builder.Display() == EDisplay::kTableHeaderGroup ||
      builder.Display() == EDisplay::kTableFooterGroup ||
      builder.Display() == EDisplay::kTableRow ||
      builder.Display() == EDisplay::kTableColumnGroup ||
      builder.Display() == EDisplay::kTableColumn ||
      builder.Display() == EDisplay::kTableCell ||
      builder.Display() == EDisplay::kTableCaption)
    builder.SetDisplay(EDisplay::kInlineBlock);
  else if (builder.Display() == EDisplay::kListItem ||
           builder.Display() == EDisplay::kTable)
    builder.SetDisplay(EDisplay::kBlock);

  ControlPart part = AdjustAppearanceWithAuthorStyle(
      AdjustAppearanceWithElementType(builder, element), builder);
  builder.SetEffectiveAppearance(part);
  DCHECK_NE(part, kAutoPart);
  if (part == kNoControlPart)
    return;
  DCHECK(element);
  // After this point, a Node must be non-null Element if
  // EffectiveAppearance() != kNoControlPart.

  AdjustControlPartStyle(builder);

  // Call the appropriate style adjustment method based off the appearance
  // value.
  switch (part) {
    case kMenulistPart:
      return AdjustMenuListStyle(builder);
    case kMenulistButtonPart:
      return AdjustMenuListButtonStyle(builder);
    case kSliderThumbHorizontalPart:
    case kSliderThumbVerticalPart:
      return AdjustSliderThumbStyle(builder);
    case kSearchFieldCancelButtonPart:
      return AdjustSearchFieldCancelButtonStyle(builder);
    default:
      break;
  }

  if (IsSliderContainer(*element))
    AdjustSliderContainerStyle(*element, builder);
}

String LayoutTheme::ExtraDefaultStyleSheet() {
  // If you want to add something depending on a runtime flag here,
  // please consider using `@supports blink-feature(flag-name)` in a
  // stylesheet resource file.
  return "@namespace 'http://www.w3.org/1999/xhtml';\n";
}

String LayoutTheme::ExtraFullscreenStyleSheet() {
  return String();
}

Color LayoutTheme::ActiveSelectionBackgroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  Color color = PlatformActiveSelectionBackgroundColor(color_scheme);
#if BUILDFLAG(IS_MAC)
  // BlendWithWhite() darkens Mac system colors too much.
  // Apply .8 (204/255) alpha instead, same as Safari.
  if (color_scheme == mojom::blink::ColorScheme::kDark)
    return Color(color.Red(), color.Green(), color.Blue(), 204);
#endif
  return color.BlendWithWhite();
}

Color LayoutTheme::InactiveSelectionBackgroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return PlatformInactiveSelectionBackgroundColor(color_scheme)
      .BlendWithWhite();
}

Color LayoutTheme::ActiveSelectionForegroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return PlatformActiveSelectionForegroundColor(color_scheme);
}

Color LayoutTheme::InactiveSelectionForegroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return PlatformInactiveSelectionForegroundColor(color_scheme);
}

Color LayoutTheme::ActiveListBoxSelectionBackgroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return PlatformActiveListBoxSelectionBackgroundColor(color_scheme);
}

Color LayoutTheme::InactiveListBoxSelectionBackgroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return PlatformInactiveListBoxSelectionBackgroundColor(color_scheme);
}

Color LayoutTheme::ActiveListBoxSelectionForegroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return PlatformActiveListBoxSelectionForegroundColor(color_scheme);
}

Color LayoutTheme::InactiveListBoxSelectionForegroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return PlatformInactiveListBoxSelectionForegroundColor(color_scheme);
}

Color LayoutTheme::PlatformSpellingMarkerUnderlineColor() const {
  return Color(255, 0, 0);
}

Color LayoutTheme::PlatformGrammarMarkerUnderlineColor() const {
  return Color(192, 192, 192);
}

Color LayoutTheme::PlatformActiveSpellingMarkerHighlightColor() const {
  return Color(255, 0, 0, 102);
}

Color LayoutTheme::PlatformActiveSelectionBackgroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  // Use a blue color by default if the platform theme doesn't define anything.
  return Color(0, 0, 255);
}

Color LayoutTheme::PlatformActiveSelectionForegroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  // Use a white color by default if the platform theme doesn't define anything.
  return Color::kWhite;
}

Color LayoutTheme::PlatformInactiveSelectionBackgroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  // Use a grey color by default if the platform theme doesn't define anything.
  // This color matches Firefox's inactive color.
  return Color(176, 176, 176);
}

Color LayoutTheme::PlatformInactiveSelectionForegroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  // Use a black color by default.
  return Color::kBlack;
}

Color LayoutTheme::PlatformActiveListBoxSelectionBackgroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return PlatformActiveSelectionBackgroundColor(color_scheme);
}

Color LayoutTheme::PlatformActiveListBoxSelectionForegroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return PlatformActiveSelectionForegroundColor(color_scheme);
}

Color LayoutTheme::PlatformInactiveListBoxSelectionBackgroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return PlatformInactiveSelectionBackgroundColor(color_scheme);
}

Color LayoutTheme::PlatformInactiveListBoxSelectionForegroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return PlatformInactiveSelectionForegroundColor(color_scheme);
}

bool LayoutTheme::IsControlStyled(ControlPart part,
                                  const ComputedStyleBuilder& builder) const {
  switch (part) {
    case kPushButtonPart:
    case kSquareButtonPart:
    case kButtonPart:
    case kProgressBarPart:
      return builder.HasAuthorBackground() || builder.HasAuthorBorder();

    case kMeterPart:
      return RuntimeEnabledFeatures::MeterDevolveAppearanceEnabled() &&
             (builder.HasAuthorBackground() || builder.HasAuthorBorder());

    case kMenulistPart:
    case kSearchFieldPart:
    case kTextAreaPart:
    case kTextFieldPart:
      return builder.HasAuthorBackground() || builder.HasAuthorBorder() ||
             builder.BoxShadow();

    default:
      return false;
  }
}

bool LayoutTheme::ShouldDrawDefaultFocusRing(const Node* node,
                                             const ComputedStyle& style) const {
  if (!node)
    return true;
  if (!style.HasEffectiveAppearance() && !node->IsLink())
    return true;
  // We can't use LayoutTheme::isFocused because outline:auto might be
  // specified to non-:focus rulesets.
  if (node->IsFocused() && !node->ShouldHaveFocusAppearance())
    return false;
  return true;
}

void LayoutTheme::AdjustCheckboxStyle(ComputedStyleBuilder& builder) const {
  // padding - not honored by WinIE, needs to be removed.
  ResetPadding(builder);

  // border - honored by WinIE, but looks terrible (just paints in the control
  // box and turns off the Windows XP theme) for now, we will not honor it.
  ResetBorder(builder);

  builder.SetShouldIgnoreOverflowPropertyForInlineBlockBaseline();
  builder.SetInlineBlockBaselineEdge(EInlineBlockBaselineEdge::kBorderBox);
}

void LayoutTheme::AdjustRadioStyle(ComputedStyleBuilder& builder) const {
  // padding - not honored by WinIE, needs to be removed.
  ResetPadding(builder);

  // border - honored by WinIE, but looks terrible (just paints in the control
  // box and turns off the Windows XP theme) for now, we will not honor it.
  ResetBorder(builder);

  builder.SetShouldIgnoreOverflowPropertyForInlineBlockBaseline();
  builder.SetInlineBlockBaselineEdge(EInlineBlockBaselineEdge::kBorderBox);
}

void LayoutTheme::AdjustButtonStyle(ComputedStyleBuilder&) const {}

void LayoutTheme::AdjustInnerSpinButtonStyle(ComputedStyleBuilder&) const {}

void LayoutTheme::AdjustMenuListStyle(ComputedStyleBuilder& builder) const {
  // Menulists should have visible overflow
  // https://bugs.webkit.org/show_bug.cgi?id=21287
  builder.SetOverflowX(EOverflow::kVisible);
  builder.SetOverflowY(EOverflow::kVisible);
}

void LayoutTheme::AdjustMenuListButtonStyle(ComputedStyleBuilder&) const {}

void LayoutTheme::AdjustSliderContainerStyle(
    const Element& element,
    ComputedStyleBuilder& builder) const {
  DCHECK(IsSliderContainer(element));

  if (!IsHorizontalWritingMode(builder.GetWritingMode())) {
    builder.SetTouchAction(TouchAction::kPanX);
  } else if (RuntimeEnabledFeatures::
                 NonStandardAppearanceValueSliderVerticalEnabled() &&
             builder.EffectiveAppearance() == kSliderVerticalPart) {
    builder.SetTouchAction(TouchAction::kPanX);
    builder.SetWritingMode(WritingMode::kVerticalRl);
    // It's always in RTL because the slider value increases up even in LTR.
    builder.SetDirection(TextDirection::kRtl);
  } else {
    builder.SetTouchAction(TouchAction::kPanY);
    builder.SetWritingMode(WritingMode::kHorizontalTb);
    if (To<HTMLInputElement>(element.OwnerShadowHost())->DataList()) {
      builder.SetAlignSelf(StyleSelfAlignmentData(ItemPosition::kCenter,
                                                  OverflowAlignment::kUnsafe));
    }
  }
  builder.SetEffectiveAppearance(kNoControlPart);
}

void LayoutTheme::AdjustSliderThumbStyle(ComputedStyleBuilder& builder) const {
  AdjustSliderThumbSize(builder);
}

void LayoutTheme::AdjustSliderThumbSize(ComputedStyleBuilder&) const {}

void LayoutTheme::AdjustSearchFieldCancelButtonStyle(
    ComputedStyleBuilder&) const {}

void LayoutTheme::PlatformColorsDidChange() {
  Page::PlatformColorsChanged();
}

void LayoutTheme::ColorSchemeDidChange() {
  Page::ColorSchemeChanged();
}

void LayoutTheme::SetCaretBlinkInterval(base::TimeDelta interval) {
  caret_blink_interval_ = interval;
}

base::TimeDelta LayoutTheme::CaretBlinkInterval() const {
  // Disable the blinking caret in web test mode, as it introduces
  // a race condition for the pixel tests. http://b/1198440
  return WebTestSupport::IsRunningWebTest() ? base::TimeDelta()
                                            : caret_blink_interval_;
}

Color LayoutTheme::SystemColor(CSSValueID css_value_id,
                               mojom::blink::ColorScheme color_scheme,
                               const ui::ColorProvider* color_provider,
                               bool is_in_web_app_scope) const {
  if (color_provider && !WebTestSupport::IsRunningWebTest()) {
    return SystemColorFromColorProvider(css_value_id, color_scheme,
                                        color_provider, is_in_web_app_scope);
  }
  return DefaultSystemColor(css_value_id, color_scheme, color_provider,
                            is_in_web_app_scope);
}

Color LayoutTheme::DefaultSystemColor(CSSValueID css_value_id,
                                      mojom::blink::ColorScheme color_scheme,
                                      const ui::ColorProvider* color_provider,
                                      bool is_in_web_app_scope) const {
  // The source for the deprecations commented on below is
  // https://www.w3.org/TR/css-color-4/#deprecated-system-colors.

  switch (css_value_id) {
    case CSSValueID::kAccentcolor:
      return RuntimeEnabledFeatures::CSSSystemAccentColorEnabled()
                 ? GetAccentColorOrDefault(color_scheme, is_in_web_app_scope)
                 : Color();
    case CSSValueID::kAccentcolortext:
      return RuntimeEnabledFeatures::CSSSystemAccentColorEnabled()
                 ? GetAccentColorText(color_scheme, is_in_web_app_scope)
                 : Color();
    case CSSValueID::kActivetext:
      return Color::FromRGBA32(0xFFFF0000);
    case CSSValueID::kButtonborder:
    // The following system colors were deprecated to default to ButtonBorder.
    case CSSValueID::kActiveborder:
    case CSSValueID::kInactiveborder:
    case CSSValueID::kThreeddarkshadow:
    case CSSValueID::kThreedhighlight:
    case CSSValueID::kThreedlightshadow:
    case CSSValueID::kThreedshadow:
    case CSSValueID::kWindowframe:
      return color_scheme == mojom::blink::ColorScheme::kDark
                 ? Color::FromRGBA32(0xFF6B6B6B)
                 : Color::FromRGBA32(0xFF767676);
    case CSSValueID::kButtonface:
    // The following system colors were deprecated to default to ButtonFace.
    case CSSValueID::kButtonhighlight:
    case CSSValueID::kButtonshadow:
    case CSSValueID::kThreedface:
      return color_scheme == mojom::blink::ColorScheme::kDark
                 ? Color::FromRGBA32(0xFF6B6B6B)
                 : Color::FromRGBA32(0xFFEFEFEF);
    case CSSValueID::kButtontext:
      return color_scheme == mojom::blink::ColorScheme::kDark
                 ? Color::FromRGBA32(0xFFFFFFFF)
                 : Color::FromRGBA32(0xFF000000);
    case CSSValueID::kCanvas:
    // The following system colors were deprecated to default to Canvas.
    case CSSValueID::kAppworkspace:
    case CSSValueID::kBackground:
    case CSSValueID::kInactivecaption:
    case CSSValueID::kInfobackground:
    case CSSValueID::kMenu:
    case CSSValueID::kScrollbar:
    case CSSValueID::kWindow:
      return color_scheme == mojom::blink::ColorScheme::kDark
                 ? Color::FromRGBA32(0xFF121212)
                 : Color::FromRGBA32(0xFFFFFFFF);
    case CSSValueID::kCanvastext:
    // The following system colors were deprecated to default to CanvasText.
    case CSSValueID::kActivecaption:
    case CSSValueID::kCaptiontext:
    case CSSValueID::kInfotext:
    case CSSValueID::kMenutext:
    case CSSValueID::kWindowtext:
      return color_scheme == mojom::blink::ColorScheme::kDark
                 ? Color::FromRGBA32(0xFFFFFFFF)
                 : Color::FromRGBA32(0xFF000000);

    case CSSValueID::kField:
      return color_scheme == mojom::blink::ColorScheme::kDark
                 ? Color::FromRGBA32(0xFF3B3B3B)
                 : Color::FromRGBA32(0xFFFFFFFF);
    case CSSValueID::kFieldtext:
      return color_scheme == mojom::blink::ColorScheme::kDark
                 ? Color::FromRGBA32(0xFFFFFFFF)
                 : Color::FromRGBA32(0xFF000000);
    case CSSValueID::kGraytext:
    // The following system color was deprecated to default to GrayText.
    case CSSValueID::kInactivecaptiontext:
      return Color::FromRGBA32(0xFF808080);
    case CSSValueID::kHighlight:
      return ActiveSelectionBackgroundColor(color_scheme);
    case CSSValueID::kHighlighttext:
      return ActiveSelectionForegroundColor(color_scheme);
    case CSSValueID::kLinktext:
      return color_scheme == mojom::blink::ColorScheme::kDark
                 ? Color::FromRGBA32(0xFF9E9EFF)
                 : Color::FromRGBA32(0xFF0000EE);
    case CSSValueID::kMark:
      return Color::FromRGBA32(0xFFFFFF00);
    case CSSValueID::kMarktext:
      return Color::FromRGBA32(0xFF000000);
    case CSSValueID::kText:
      return color_scheme == mojom::blink::ColorScheme::kDark
                 ? Color::FromRGBA32(0xFFFFFFFF)
                 : Color::FromRGBA32(0xFF000000);
    case CSSValueID::kVisitedtext:
      return color_scheme == mojom::blink::ColorScheme::kDark
                  ? Color::FromRGBA32(0xFFD0ADF0)
                  : Color::FromRGBA32(0xFF551A8B);
    case CSSValueID::kSelecteditem:
    case CSSValueID::kInternalActiveListBoxSelection:
      return ActiveListBoxSelectionBackgroundColor(color_scheme);
    case CSSValueID::kSelecteditemtext:
    case CSSValueID::kInternalActiveListBoxSelectionText:
      return ActiveListBoxSelectionForegroundColor(color_scheme);
    case CSSValueID::kInternalInactiveListBoxSelection:
      return InactiveListBoxSelectionBackgroundColor(color_scheme);
    case CSSValueID::kInternalInactiveListBoxSelectionText:
      return InactiveListBoxSelectionForegroundColor(color_scheme);
    case CSSValueID::kInternalSpellingErrorColor:
      return PlatformSpellingMarkerUnderlineColor();
    case CSSValueID::kInternalGrammarErrorColor:
      return PlatformGrammarMarkerUnderlineColor();
    case CSSValueID::kInternalSearchColor:
      return PlatformTextSearchHighlightColor(/* active_match */ false,
                                              /* in_forced_colors */ false,
                                              color_scheme, color_provider,
                                              is_in_web_app_scope);
    case CSSValueID::kInternalSearchTextColor:
      return PlatformTextSearchColor(/* active_match */ false,
                                     /* in_forced_colors */ false, color_scheme,
                                     color_provider, is_in_web_app_scope);
    case CSSValueID::kInternalCurrentSearchColor:
      return PlatformTextSearchHighlightColor(/* active_match */ true,
                                              /* in_forced_colors */ false,
                                              color_scheme, color_provider,
                                              is_in_web_app_scope);
    case CSSValueID::kInternalCurrentSearchTextColor:
      return PlatformTextSearchColor(/* active_match */ true,
                                     /* in_forced_colors */ false, color_scheme,
                                     color_provider, is_in_web_app_scope);
    default:
      break;
  }
  DUMP_WILL_BE_NOTREACHED()
      << getValueName(css_value_id) << " is not a recognized system color";
  return Color();
}

Color LayoutTheme::SystemColorFromColorProvider(
    CSSValueID css_value_id,
    mojom::blink::ColorScheme color_scheme,
    const ui::ColorProvider* color_provider,
    bool is_in_web_app_scope) const {
  SkColor system_theme_color;
  switch (css_value_id) {
    case CSSValueID::kActivetext:
    case CSSValueID::kLinktext:
    case CSSValueID::kVisitedtext:
      system_theme_color =
          color_provider->GetColor(ui::kColorCssSystemHotlight);
      break;
    case CSSValueID::kButtonface:
    case CSSValueID::kButtonhighlight:
    case CSSValueID::kButtonshadow:
    case CSSValueID::kThreedface:
      system_theme_color = color_provider->GetColor(ui::kColorCssSystemBtnFace);
      break;
    case CSSValueID::kButtonborder:
    case CSSValueID::kButtontext:
    // Deprecated colors, see DefaultSystemColor().
    case CSSValueID::kActiveborder:
    case CSSValueID::kInactiveborder:
    case CSSValueID::kThreeddarkshadow:
    case CSSValueID::kThreedhighlight:
    case CSSValueID::kThreedlightshadow:
    case CSSValueID::kThreedshadow:
    case CSSValueID::kWindowframe:
      system_theme_color = color_provider->GetColor(ui::kColorCssSystemBtnText);
      break;
    case CSSValueID::kGraytext:
      system_theme_color =
          color_provider->GetColor(ui::kColorCssSystemGrayText);
      break;
    case CSSValueID::kHighlight:
      return SystemHighlightFromColorProvider(color_scheme, color_provider);
    case CSSValueID::kHighlighttext:
      system_theme_color =
          color_provider->GetColor(ui::kColorCssSystemHighlightText);
      break;
    case CSSValueID::kCanvas:
    case CSSValueID::kField:
    // Deprecated colors, see DefaultSystemColor().
    case CSSValueID::kAppworkspace:
    case CSSValueID::kBackground:
    case CSSValueID::kInactivecaption:
    case CSSValueID::kInfobackground:
    case CSSValueID::kMenu:
    case CSSValueID::kScrollbar:
    case CSSValueID::kWindow:
      system_theme_color = color_provider->GetColor(ui::kColorCssSystemWindow);
      break;
    case CSSValueID::kCanvastext:
    case CSSValueID::kFieldtext:
    // Deprecated colors, see DefaultSystemColor().
    case CSSValueID::kActivecaption:
    case CSSValueID::kCaptiontext:
    case CSSValueID::kInfotext:
    case CSSValueID::kMenutext:
    case CSSValueID::kWindowtext:
      system_theme_color =
          color_provider->GetColor(ui::kColorCssSystemWindowText);
      break;
    default:
      return DefaultSystemColor(css_value_id, color_scheme, color_provider,
                                is_in_web_app_scope);
  }

  return Color::FromSkColor(system_theme_color);
}

Color LayoutTheme::SystemHighlightFromColorProvider(
    mojom::blink::ColorScheme color_scheme,
    const ui::ColorProvider* color_provider) const {
  SkColor system_highlight_color =
      color_provider->GetColor(ui::kColorCssSystemHighlight);
  return Color::FromSkColor(system_highlight_color).BlendWithWhite();
}

Color LayoutTheme::PlatformTextSearchHighlightColor(
    bool active_match,
    bool in_forced_colors,
    mojom::blink::ColorScheme color_scheme,
    const ui::ColorProvider* color_provider,
    bool is_in_web_app_scope) const {
  if (active_match) {
    if (in_forced_colors) {
      return GetTheme().SystemColor(CSSValueID::kHighlight, color_scheme,
                                    color_provider, is_in_web_app_scope);
    }
    return Color(255, 150, 50);  // Orange.
  }
  return Color(255, 255, 0);  // Yellow.
}

Color LayoutTheme::PlatformTextSearchColor(
    bool active_match,
    bool in_forced_colors,
    mojom::blink::ColorScheme color_scheme,
    const ui::ColorProvider* color_provider,
    bool is_in_web_app_scope) const {
  if (in_forced_colors && active_match) {
    return GetTheme().SystemColor(CSSValueID::kHighlighttext, color_scheme,
                                  color_provider, is_in_web_app_scope);
  }
  return Color::kBlack;
}

Color LayoutTheme::TapHighlightColor() {
  return GetTheme().PlatformTapHighlightColor();
}

void LayoutTheme::SetCustomFocusRingColor(const Color& c) {
  const bool changed =
      !has_custom_focus_ring_color_ || custom_focus_ring_color_ != c;
  custom_focus_ring_color_ = c;
  has_custom_focus_ring_color_ = true;
  if (changed) {
    Page::PlatformColorsChanged();
  }
}

Color LayoutTheme::FocusRingColor(
    mojom::blink::ColorScheme color_scheme) const {
  return has_custom_focus_ring_color_ ? custom_focus_ring_color_
                                      : GetTheme().PlatformFocusRingColor();
}

bool LayoutTheme::DelegatesMenuListRendering() const {
  return delegates_menu_list_rendering_;
}

void LayoutTheme::SetDelegatesMenuListRenderingForTesting(bool flag) {
  delegates_menu_list_rendering_ = flag;
}

String LayoutTheme::DisplayNameForFile(const File& file) const {
  return file.name();
}

bool LayoutTheme::SupportsCalendarPicker(InputType::Type type) const {
  DCHECK(RuntimeEnabledFeatures::InputMultipleFieldsUIEnabled());
  return type == InputType::Type::kTime || type == InputType::Type::kDate ||
         type == InputType::Type::kDateTimeLocal ||
         type == InputType::Type::kMonth || type == InputType::Type::kWeek;
}

void LayoutTheme::AdjustControlPartStyle(ComputedStyleBuilder& builder) {
  // Call the appropriate style adjustment method based off the appearance
  // value.
  switch (builder.EffectiveAppearance()) {
    case kCheckboxPart:
      return AdjustCheckboxStyle(builder);
    case kRadioPart:
      return AdjustRadioStyle(builder);
    case kPushButtonPart:
    case kSquareButtonPart:
    case kButtonPart:
      return AdjustButtonStyle(builder);
    case kInnerSpinButtonPart:
      return AdjustInnerSpinButtonStyle(builder);
    default:
      break;
  }
}

bool LayoutTheme::HasCustomFocusRingColor() const {
  return has_custom_focus_ring_color_;
}

Color LayoutTheme::GetCustomFocusRingColor() const {
  return custom_focus_ring_color_;
}

bool LayoutTheme::IsAccentColorCustomized(
    mojom::blink::ColorScheme color_scheme) const {
  if (!RuntimeEnabledFeatures::CSSSystemAccentColorEnabled()) {
    return false;
  }

  return WebThemeEngineHelper::GetNativeThemeEngine()
      ->GetAccentColor()
      .has_value();
}

Color LayoutTheme::GetSystemAccentColor(
    mojom::blink::ColorScheme color_scheme) const {
  if (!RuntimeEnabledFeatures::CSSSystemAccentColorEnabled()) {
    return Color();
  }

  // Currently only plumbed through on ChromeOS and Windows.
  const auto& accent_color =
      WebThemeEngineHelper::GetNativeThemeEngine()->GetAccentColor();
  if (!accent_color.has_value()) {
    return Color();
  }
  return Color::FromSkColor(accent_color.value());
}

Color LayoutTheme::GetAccentColorOrDefault(
    mojom::blink::ColorScheme color_scheme,
    bool is_in_web_app_scope) const {
  // This is from the kAccent color from NativeThemeBase::GetControlColor
  const Color kDefaultAccentColor = Color(0x00, 0x75, 0xFF);
  Color accent_color = Color();
  // Currently OS-defined accent color is exposed via System AccentColor keyword
  // ONLY for installed WebApps where fingerprinting risk is not as large of a
  // risk.
  if (is_in_web_app_scope) {
    accent_color = GetSystemAccentColor(color_scheme);
  }
  return accent_color == Color() ? kDefaultAccentColor : accent_color;
}

Color LayoutTheme::GetAccentColorText(mojom::blink::ColorScheme color_scheme,
                                      bool is_in_web_app_scope) const {
  Color accent_color =
      GetAccentColorOrDefault(color_scheme, is_in_web_app_scope);
  // This logic matches AccentColorText in Firefox. If the accent color to draw
  // text on is dark, then use white. If it's light, then use dark.
  return color_utils::GetRelativeLuminance4f(accent_color.toSkColor4f()) <= 128
             ? Color::kWhite
             : Color::kBlack;
}

}  // namespace blink
