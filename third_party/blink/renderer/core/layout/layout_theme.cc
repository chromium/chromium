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
#include "third_party/blink/renderer/core/dom/scroll_button_pseudo_element.h"
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
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"

// The methods in this file are shared by all themes on every platform.

namespace blink {

using mojom::blink::FormControlType;

namespace {

// This function should match to the user-agent stylesheet.
AppearanceValue AutoAppearanceFor(const Element& element) {
  if (IsA<HTMLButtonElement>(element)) {
    return AppearanceValue::kButton;
  }
  if (IsA<ScrollButtonPseudoElement>(element)) {
    return AppearanceValue::kButton;
  }
  if (IsA<HTMLMeterElement>(element)) {
    return AppearanceValue::kMeter;
  }
  if (IsA<HTMLProgressElement>(element)) {
    return AppearanceValue::kProgressBar;
  }
  if (IsA<HTMLTextAreaElement>(element)) {
    return AppearanceValue::kTextArea;
  }
  if (IsA<SpinButtonElement>(element)) {
    return AppearanceValue::kInnerSpinButton;
  }
  if (const auto* select = DynamicTo<HTMLSelectElement>(element)) {
    return select->UsesMenuList() ? AppearanceValue::kMenulist
                                  : AppearanceValue::kListbox;
  }

  if (const auto* input = DynamicTo<HTMLInputElement>(element)) {
    return input->AutoAppearance();
  }

  if (element.IsInUserAgentShadowRoot()) {
    const AtomicString& id_value =
        element.FastGetAttribute(html_names::kIdAttr);
    if (id_value == shadow_element_names::kIdSliderThumb)
      return AppearanceValue::kSliderThumbHorizontal;
    if (id_value == shadow_element_names::kIdSearchClearButton ||
        id_value == shadow_element_names::kIdClearButton)
      return AppearanceValue::kSearchFieldCancelButton;

    // Slider container elements and -webkit-meter-inner-element don't have IDs.
    if (IsSliderContainer(element))
      return AppearanceValue::kSliderHorizontal;
    if (element.ShadowPseudoId() ==
        shadow_element_names::kPseudoMeterInnerElement)
      return AppearanceValue::kMeter;
  }
  return AppearanceValue::kNone;
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

bool SystemAccentColorAllowed() {
  return RuntimeEnabledFeatures::CSSSystemAccentColorEnabled() ||
         RuntimeEnabledFeatures::CSSAccentColorKeywordEnabled();
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

AppearanceValue LayoutTheme::AdjustAppearanceWithAuthorStyle(
    AppearanceValue appearance,
    const ComputedStyleBuilder& builder) {
  if (IsControlStyled(appearance, builder)) {
    return appearance == AppearanceValue::kMenulist
               ? AppearanceValue::kMenulistButton
               : AppearanceValue::kNone;
  }
  return appearance;
}

AppearanceValue LayoutTheme::AdjustAppearanceWithElementType(
    const ComputedStyleBuilder& builder,
    const Element* element) {
  AppearanceValue appearance = builder.EffectiveAppearance();
  if (!element)
    return AppearanceValue::kNone;

  AppearanceValue auto_appearance = AutoAppearanceFor(*element);
  if (appearance == auto_appearance) {
    return appearance;
  }

  switch (appearance) {
    // No restrictions.
    case AppearanceValue::kNone:
    case AppearanceValue::kMediaSlider:
    case AppearanceValue::kMediaSliderThumb:
    case AppearanceValue::kMediaVolumeSlider:
    case AppearanceValue::kMediaVolumeSliderThumb:
    case AppearanceValue::kMediaControl:
      return appearance;
    case AppearanceValue::kBaseSelect:
    case AppearanceValue::kBase:
      return element->SupportsBaseAppearance(appearance) ? appearance
                                                         : auto_appearance;

    // Aliases of 'auto'.
    // https://drafts.csswg.org/css-ui-4/#typedef-appearance-compat-auto
    case AppearanceValue::kAuto:
    case AppearanceValue::kCheckbox:
    case AppearanceValue::kRadio:
    case AppearanceValue::kPushButton:
    case AppearanceValue::kSquareButton:
    case AppearanceValue::kInnerSpinButton:
    case AppearanceValue::kListbox:
    case AppearanceValue::kMenulist:
    case AppearanceValue::kMeter:
    case AppearanceValue::kProgressBar:
    case AppearanceValue::kSliderHorizontal:
    case AppearanceValue::kSliderThumbHorizontal:
    case AppearanceValue::kSearchField:
    case AppearanceValue::kSearchFieldCancelButton:
    case AppearanceValue::kTextArea:
      return auto_appearance;

      // The following keywords should work well for some element types
      // even if their default appearances are different from the keywords.

    case AppearanceValue::kButton:
      return (auto_appearance == AppearanceValue::kPushButton ||
              auto_appearance == AppearanceValue::kSquareButton)
                 ? appearance
                 : auto_appearance;

    case AppearanceValue::kMenulistButton:
      return auto_appearance == AppearanceValue::kMenulist ? appearance
                                                           : auto_appearance;

    case AppearanceValue::kSliderVertical:
      return auto_appearance == AppearanceValue::kSliderHorizontal
                 ? appearance
                 : auto_appearance;

    case AppearanceValue::kSliderThumbVertical:
      return auto_appearance == AppearanceValue::kSliderThumbHorizontal
                 ? appearance
                 : auto_appearance;

    case AppearanceValue::kTextField:
      if (const auto* input_element = DynamicTo<HTMLInputElement>(*element);
          input_element &&
          input_element->FormControlType() == FormControlType::kInputSearch) {
        return appearance;
      }
      return auto_appearance;
  }

  return appearance;
}

void LayoutTheme::AdjustStyle(const Element* element,
                              ComputedStyleBuilder& builder) {
  AppearanceValue original_appearance = builder.Appearance();
  builder.SetEffectiveAppearance(original_appearance);
  if (original_appearance == AppearanceValue::kNone) {
    return;
  }

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

  AppearanceValue appearance = AdjustAppearanceWithAuthorStyle(
      AdjustAppearanceWithElementType(builder, element), builder);
  builder.SetEffectiveAppearance(appearance);
  DCHECK_NE(appearance, AppearanceValue::kAuto);
  if (appearance == AppearanceValue::kNone) {
    return;
  }
  DCHECK(element);
  // After this point, a Node must be non-null Element if
  // EffectiveAppearance() != AppearanceValue::kNone.

  AdjustControlPartStyle(builder);

  // Call the appropriate style adjustment method based off the appearance
  // value.
  switch (appearance) {
    case AppearanceValue::kMenulist:
      return AdjustMenuListStyle(builder);
    case AppearanceValue::kMenulistButton:
      return AdjustMenuListButtonStyle(builder);
    case AppearanceValue::kSliderThumbHorizontal:
    case AppearanceValue::kSliderThumbVertical:
      return AdjustSliderThumbStyle(builder);
    case AppearanceValue::kSearchFieldCancelButton:
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

bool LayoutTheme::IsControlStyled(AppearanceValue appearance,
                                  const ComputedStyleBuilder& builder) const {
  switch (appearance) {
    case AppearanceValue::kPushButton:
    case AppearanceValue::kSquareButton:
    case AppearanceValue::kButton:
    case AppearanceValue::kProgressBar:
      return builder.HasAuthorBackground() || builder.HasAuthorBorder();

    case AppearanceValue::kMeter:
      return builder.HasAuthorBackground() || builder.HasAuthorBorder();

    case AppearanceValue::kMenulist:
    case AppearanceValue::kSearchField:
    case AppearanceValue::kTextArea:
    case AppearanceValue::kTextField:
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
             builder.EffectiveAppearance() ==
                 AppearanceValue::kSliderVertical) {
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
  builder.SetEffectiveAppearance(AppearanceValue::kNone);
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
      return RuntimeEnabledFeatures::CSSAccentColorKeywordEnabled()
                 ? GetAccentColorOrDefault(color_scheme, is_in_web_app_scope)
                 : Color();
    case CSSValueID::kAccentcolortext:
      return RuntimeEnabledFeatures::CSSAccentColorKeywordEnabled()
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
      << GetCSSValueName(css_value_id) << " is not a recognized system color";
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
      system_theme_color =
          color_provider->GetColor(ui::kColorCssSystemActiveText);
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
    case CSSValueID::kField:
      system_theme_color = color_provider->GetColor(ui::kColorCssSystemField);
      break;
    case CSSValueID::kFieldtext:
      system_theme_color =
          color_provider->GetColor(ui::kColorCssSystemFieldText);
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
    case CSSValueID::kLinktext:
      system_theme_color =
          color_provider->GetColor(ui::kColorCssSystemLinkText);
      break;
    case CSSValueID::kVisitedtext:
      system_theme_color =
          color_provider->GetColor(ui::kColorCssSystemVisitedText);
      break;
    case CSSValueID::kCanvas:
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
    case AppearanceValue::kCheckbox:
      return AdjustCheckboxStyle(builder);
    case AppearanceValue::kRadio:
      return AdjustRadioStyle(builder);
    case AppearanceValue::kPushButton:
    case AppearanceValue::kSquareButton:
    case AppearanceValue::kButton:
      return AdjustButtonStyle(builder);
    case AppearanceValue::kInnerSpinButton:
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
  if (!SystemAccentColorAllowed()) {
    return false;
  }

  return WebThemeEngineHelper::GetNativeThemeEngine()
      ->GetAccentColor()
      .has_value();
}

Color LayoutTheme::GetSystemAccentColor(
    mojom::blink::ColorScheme color_scheme) const {
  if (!SystemAccentColorAllowed()) {
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
  if (RuntimeEnabledFeatures::CSSAccentColorKeywordEnabled() &&
      is_in_web_app_scope) {
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
