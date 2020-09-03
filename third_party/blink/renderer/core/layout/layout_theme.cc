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
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_options_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/spin_button_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_inner_elements.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
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
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/base/ui_base_features.h"
#include "ui/native_theme/native_theme.h"

// The methods in this file are shared by all themes on every platform.

namespace blink {

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

  if (const auto* input = DynamicTo<HTMLInputElement>(element)) {
    const AtomicString& type = input->type();
    if (type == input_type_names::kCheckbox)
      return kCheckboxPart;
    if (type == input_type_names::kRadio)
      return kRadioPart;
    if (input->IsTextButton())
      return kPushButtonPart;
    if (type == input_type_names::kColor) {
      return input->FastHasAttribute(html_names::kListAttr) ? kMenulistPart
                                                            : kSquareButtonPart;
    }
    if (type == input_type_names::kRange)
      return kSliderHorizontalPart;
    if (type == input_type_names::kSearch)
      return kSearchFieldPart;
    if (type == input_type_names::kDate ||
        type == input_type_names::kDatetimeLocal ||
        type == input_type_names::kMonth || type == input_type_names::kTime ||
        type == input_type_names::kWeek) {
#if defined(OS_ANDROID)
      return kMenulistPart;
#else
      return kTextFieldPart;
#endif
    }
    if (type == input_type_names::kEmail || type == input_type_names::kNumber ||
        type == input_type_names::kPassword || type == input_type_names::kTel ||
        type == input_type_names::kText || type == input_type_names::kUrl)
      return kTextFieldPart;

    // Type=hidden/image/file.
    return kNoControlPart;
  }

  if (element.IsInUserAgentShadowRoot()) {
    const AtomicString& id_value =
        element.FastGetAttribute(html_names::kIdAttr);
    if (id_value == shadow_element_names::SliderThumb())
      return kSliderThumbHorizontalPart;
    if (id_value == shadow_element_names::SearchClearButton() ||
        id_value == shadow_element_names::ClearButton())
      return kSearchFieldCancelButtonPart;

    // Slider container elements and -webkit-meter-inner-element don't have IDs.
    if (IsSliderContainer(element))
      return kSliderHorizontalPart;
    if (element.ShadowPseudoId() == "-webkit-meter-inner-element")
      return kMeterPart;
  }
  return kNoControlPart;
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

LayoutTheme::LayoutTheme() : has_custom_focus_ring_color_(false) {}

ControlPart LayoutTheme::AdjustAppearanceWithAuthorStyle(
    ControlPart part,
    const ComputedStyle& style) {
  if (IsControlStyled(part, style))
    return part == kMenulistPart ? kMenulistButtonPart : kNoControlPart;
  return part;
}

ControlPart LayoutTheme::AdjustAppearanceWithElementType(
    const ComputedStyle& style,
    const Element* element) {
  ControlPart part = style.EffectiveAppearance();
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
      if (IsA<HTMLInputElement>(*element) &&
          To<HTMLInputElement>(*element).type() == input_type_names::kSearch)
        return part;
      return auto_appearance;
  }

  return part;
}

void LayoutTheme::AdjustStyle(ComputedStyle& style, Element* e) {
  ControlPart original_part = style.Appearance();
  style.SetEffectiveAppearance(original_part);
  if (original_part == ControlPart::kNoControlPart)
    return;

  // Force inline and table display styles to be inline-block (except for table-
  // which is block)
  if (style.Display() == EDisplay::kInline ||
      style.Display() == EDisplay::kInlineTable ||
      style.Display() == EDisplay::kTableRowGroup ||
      style.Display() == EDisplay::kTableHeaderGroup ||
      style.Display() == EDisplay::kTableFooterGroup ||
      style.Display() == EDisplay::kTableRow ||
      style.Display() == EDisplay::kTableColumnGroup ||
      style.Display() == EDisplay::kTableColumn ||
      style.Display() == EDisplay::kTableCell ||
      style.Display() == EDisplay::kTableCaption)
    style.SetDisplay(EDisplay::kInlineBlock);
  else if (style.Display() == EDisplay::kListItem ||
           style.Display() == EDisplay::kTable)
    style.SetDisplay(EDisplay::kBlock);

  // TODO(tkent): We should not update Appearance, which is a source of
  // getComputedStyle(). https://drafts.csswg.org/css-ui-4/#propdef-appearance
  // says "Computed value: specified keyword".
  style.SetAppearance(AdjustAppearanceWithAuthorStyle(original_part, style));

  ControlPart part = AdjustAppearanceWithAuthorStyle(
      AdjustAppearanceWithElementType(style, e), style);
  style.SetEffectiveAppearance(part);
  DCHECK_NE(part, kAutoPart);
  if (part == kNoControlPart)
    return;

  AdjustControlPartStyle(style);

  // Call the appropriate style adjustment method based off the appearance
  // value.
  switch (part) {
    case kMenulistPart:
      return AdjustMenuListStyle(style, e);
    case kMenulistButtonPart:
      return AdjustMenuListButtonStyle(style, e);
    case kSliderHorizontalPart:
    case kSliderVerticalPart:
    case kMediaSliderPart:
    case kMediaVolumeSliderPart:
      return AdjustSliderContainerStyle(style, e);
    case kSliderThumbHorizontalPart:
    case kSliderThumbVerticalPart:
      return AdjustSliderThumbStyle(style);
    case kSearchFieldPart:
      return AdjustSearchFieldStyle(style);
    case kSearchFieldCancelButtonPart:
      return AdjustSearchFieldCancelButtonStyle(style);
    default:
      break;
  }
}

String LayoutTheme::ExtraDefaultStyleSheet() {
  return g_empty_string;
}

String LayoutTheme::ExtraQuirksStyleSheet() {
  return String();
}

String LayoutTheme::ExtraFullscreenStyleSheet() {
  return String();
}

Color LayoutTheme::ActiveSelectionBackgroundColor(
    WebColorScheme color_scheme) const {
  return PlatformActiveSelectionBackgroundColor(color_scheme).BlendWithWhite();
}

Color LayoutTheme::InactiveSelectionBackgroundColor(
    WebColorScheme color_scheme) const {
  return PlatformInactiveSelectionBackgroundColor(color_scheme)
      .BlendWithWhite();
}

Color LayoutTheme::ActiveSelectionForegroundColor(
    WebColorScheme color_scheme) const {
  return PlatformActiveSelectionForegroundColor(color_scheme);
}

Color LayoutTheme::InactiveSelectionForegroundColor(
    WebColorScheme color_scheme) const {
  return PlatformInactiveSelectionForegroundColor(color_scheme);
}

Color LayoutTheme::ActiveListBoxSelectionBackgroundColor(
    WebColorScheme color_scheme) const {
  return PlatformActiveListBoxSelectionBackgroundColor(color_scheme);
}

Color LayoutTheme::InactiveListBoxSelectionBackgroundColor(
    WebColorScheme color_scheme) const {
  return PlatformInactiveListBoxSelectionBackgroundColor(color_scheme);
}

Color LayoutTheme::ActiveListBoxSelectionForegroundColor(
    WebColorScheme color_scheme) const {
  return PlatformActiveListBoxSelectionForegroundColor(color_scheme);
}

Color LayoutTheme::InactiveListBoxSelectionForegroundColor(
    WebColorScheme color_scheme) const {
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
    WebColorScheme color_scheme) const {
  // Use a blue color by default if the platform theme doesn't define anything.
  return Color(0, 0, 255);
}

Color LayoutTheme::PlatformActiveSelectionForegroundColor(
    WebColorScheme color_scheme) const {
  // Use a white color by default if the platform theme doesn't define anything.
  return Color::kWhite;
}

Color LayoutTheme::PlatformInactiveSelectionBackgroundColor(
    WebColorScheme color_scheme) const {
  // Use a grey color by default if the platform theme doesn't define anything.
  // This color matches Firefox's inactive color.
  return Color(176, 176, 176);
}

Color LayoutTheme::PlatformInactiveSelectionForegroundColor(
    WebColorScheme color_scheme) const {
  // Use a black color by default.
  return Color::kBlack;
}

Color LayoutTheme::PlatformActiveListBoxSelectionBackgroundColor(
    WebColorScheme color_scheme) const {
  return PlatformActiveSelectionBackgroundColor(color_scheme);
}

Color LayoutTheme::PlatformActiveListBoxSelectionForegroundColor(
    WebColorScheme color_scheme) const {
  return PlatformActiveSelectionForegroundColor(color_scheme);
}

Color LayoutTheme::PlatformInactiveListBoxSelectionBackgroundColor(
    WebColorScheme color_scheme) const {
  return PlatformInactiveSelectionBackgroundColor(color_scheme);
}

Color LayoutTheme::PlatformInactiveListBoxSelectionForegroundColor(
    WebColorScheme color_scheme) const {
  return PlatformInactiveSelectionForegroundColor(color_scheme);
}

bool LayoutTheme::IsControlStyled(ControlPart part,
                                  const ComputedStyle& style) const {
  switch (part) {
    case kPushButtonPart:
    case kSquareButtonPart:
    case kButtonPart:
    case kProgressBarPart:
      return style.HasAuthorBackground() || style.HasAuthorBorder();

    case kMenulistPart:
    case kSearchFieldPart:
    case kTextAreaPart:
    case kTextFieldPart:
      return style.HasAuthorBackground() || style.HasAuthorBorder() ||
             style.BoxShadow();

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

bool LayoutTheme::IsChecked(const Node* node) {
  if (auto* input = DynamicTo<HTMLInputElement>(node))
    return input->ShouldAppearChecked();
  return false;
}

bool LayoutTheme::IsIndeterminate(const Node* node) {
  if (auto* input = DynamicTo<HTMLInputElement>(node))
    return input->ShouldAppearIndeterminate();
  return false;
}

bool LayoutTheme::IsEnabled(const Node* node) {
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return true;
  return !element->IsDisabledFormControl();
}

bool LayoutTheme::IsPressed(const Node* node) {
  if (!node)
    return false;
  return node->IsActive();
}

bool LayoutTheme::IsReadOnlyControl(const Node* node) {
  auto* form_control_element = DynamicTo<HTMLFormControlElement>(node);
  return form_control_element && form_control_element->IsReadOnly();
}

bool LayoutTheme::IsHovered(const Node* node) {
  if (!node)
    return false;
  return node->IsHovered();
}

void LayoutTheme::AdjustCheckboxStyle(ComputedStyle& style) const {
  // A summary of the rules for checkbox designed to match WinIE:
  // width/height - honored (WinIE actually scales its control for small widths,
  // but lets it overflow for small heights.)
  // font-size - not honored (control has no text), but we use it to decide
  // which control size to use.
  SetCheckboxSize(style);

  // padding - not honored by WinIE, needs to be removed.
  style.ResetPadding();

  // border - honored by WinIE, but looks terrible (just paints in the control
  // box and turns off the Windows XP theme) for now, we will not honor it.
  style.ResetBorder();
}

void LayoutTheme::AdjustRadioStyle(ComputedStyle& style) const {
  // A summary of the rules for checkbox designed to match WinIE:
  // width/height - honored (WinIE actually scales its control for small widths,
  // but lets it overflow for small heights.)
  // font-size - not honored (control has no text), but we use it to decide
  // which control size to use.
  SetRadioSize(style);

  // padding - not honored by WinIE, needs to be removed.
  style.ResetPadding();

  // border - honored by WinIE, but looks terrible (just paints in the control
  // box and turns off the Windows XP theme) for now, we will not honor it.
  style.ResetBorder();
}

void LayoutTheme::AdjustButtonStyle(ComputedStyle& style) const {}

void LayoutTheme::AdjustInnerSpinButtonStyle(ComputedStyle&) const {}

void LayoutTheme::AdjustMenuListStyle(ComputedStyle& style, Element*) const {
  // Menulists should have visible overflow
  // https://bugs.webkit.org/show_bug.cgi?id=21287
  style.SetOverflowX(EOverflow::kVisible);
  style.SetOverflowY(EOverflow::kVisible);
}

void LayoutTheme::AdjustMenuListButtonStyle(ComputedStyle&, Element*) const {}

void LayoutTheme::AdjustSliderContainerStyle(ComputedStyle& style,
                                             Element* e) const {
  if (e && (e->ShadowPseudoId() == "-webkit-media-slider-container" ||
            e->ShadowPseudoId() == "-webkit-slider-container")) {
    if (style.EffectiveAppearance() == kSliderVerticalPart) {
      style.SetTouchAction(TouchAction::kPanX);
      style.SetEffectiveAppearance(kNoControlPart);
      style.SetWritingMode(WritingMode::kVerticalRl);
      // It's always in RTL because the slider value increases up even in LTR.
      style.SetDirection(TextDirection::kRtl);
    } else {
      style.SetTouchAction(TouchAction::kPanY);
      style.SetEffectiveAppearance(kNoControlPart);
      style.SetWritingMode(WritingMode::kHorizontalTb);
    }
  }
}

void LayoutTheme::AdjustSliderThumbStyle(ComputedStyle& style) const {
  AdjustSliderThumbSize(style);
}

void LayoutTheme::AdjustSliderThumbSize(ComputedStyle&) const {}

void LayoutTheme::AdjustSearchFieldStyle(ComputedStyle&) const {}

void LayoutTheme::AdjustSearchFieldCancelButtonStyle(ComputedStyle&) const {}

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

static FontDescription& GetCachedFontDescription(CSSValueID system_font_id) {
  DEFINE_STATIC_LOCAL(FontDescription, caption, ());
  DEFINE_STATIC_LOCAL(FontDescription, icon, ());
  DEFINE_STATIC_LOCAL(FontDescription, menu, ());
  DEFINE_STATIC_LOCAL(FontDescription, message_box, ());
  DEFINE_STATIC_LOCAL(FontDescription, small_caption, ());
  DEFINE_STATIC_LOCAL(FontDescription, status_bar, ());
  DEFINE_STATIC_LOCAL(FontDescription, webkit_mini_control, ());
  DEFINE_STATIC_LOCAL(FontDescription, webkit_small_control, ());
  DEFINE_STATIC_LOCAL(FontDescription, webkit_control, ());
  DEFINE_STATIC_LOCAL(FontDescription, default_description, ());
  switch (system_font_id) {
    case CSSValueID::kCaption:
      return caption;
    case CSSValueID::kIcon:
      return icon;
    case CSSValueID::kMenu:
      return menu;
    case CSSValueID::kMessageBox:
      return message_box;
    case CSSValueID::kSmallCaption:
      return small_caption;
    case CSSValueID::kStatusBar:
      return status_bar;
    case CSSValueID::kWebkitMiniControl:
      return webkit_mini_control;
    case CSSValueID::kWebkitSmallControl:
      return webkit_small_control;
    case CSSValueID::kWebkitControl:
      return webkit_control;
    case CSSValueID::kNone:
      return default_description;
    default:
      NOTREACHED();
      return default_description;
  }
}

void LayoutTheme::SystemFont(CSSValueID system_font_id,
                             FontDescription& font_description) {
  font_description = GetCachedFontDescription(system_font_id);
  if (font_description.IsAbsoluteSize())
    return;

  FontSelectionValue font_slope = NormalSlopeValue();
  FontSelectionValue font_weight = NormalWeightValue();
  float font_size = 0;
  AtomicString font_family;
  LayoutThemeFontProvider::SystemFont(system_font_id, font_slope, font_weight,
                                      font_size, font_family);
  font_description.SetStyle(font_slope);
  font_description.SetWeight(font_weight);
  font_description.SetSpecifiedSize(font_size);
  font_description.SetIsAbsoluteSize(true);
  font_description.FirstFamily().SetFamily(font_family);
  font_description.SetGenericFamily(FontDescription::kNoFamily);
}

Color LayoutTheme::SystemColor(CSSValueID css_value_id,
                               WebColorScheme color_scheme) const {
  switch (css_value_id) {
    case CSSValueID::kActiveborder:
      return 0xFFFFFFFF;
    case CSSValueID::kActivecaption:
      return 0xFFCCCCCC;
    case CSSValueID::kActivetext:
      return 0xFFFF0000;
    case CSSValueID::kAppworkspace:
      return color_scheme == WebColorScheme::kDark ? 0xFF000000 : 0xFFFFFFFF;
    case CSSValueID::kBackground:
      return 0xFF6363CE;
    case CSSValueID::kButtonface:
      return color_scheme == WebColorScheme::kDark ? 0xFF444444 : 0xFFDDDDDD;
    case CSSValueID::kButtonhighlight:
      return 0xFFDDDDDD;
    case CSSValueID::kButtonshadow:
      return 0xFF888888;
    case CSSValueID::kButtontext:
      return color_scheme == WebColorScheme::kDark ? 0xFFAAAAAA : 0xFF000000;
    case CSSValueID::kCaptiontext:
      return color_scheme == WebColorScheme::kDark ? 0xFFFFFFFF : 0xFF000000;
    case CSSValueID::kField:
      return color_scheme == WebColorScheme::kDark ? 0xFF000000 : 0xFFFFFFFF;
    case CSSValueID::kFieldtext:
      return color_scheme == WebColorScheme::kDark ? 0xFFFFFFFF : 0xFF000000;
    case CSSValueID::kGraytext:
      return 0xFF808080;
    case CSSValueID::kHighlight:
      return 0xFFB5D5FF;
    case CSSValueID::kHighlighttext:
      return color_scheme == WebColorScheme::kDark ? 0xFFFFFFFF : 0xFF000000;
    case CSSValueID::kInactiveborder:
      return 0xFFFFFFFF;
    case CSSValueID::kInactivecaption:
      return 0xFFFFFFFF;
    case CSSValueID::kInactivecaptiontext:
      return 0xFF7F7F7F;
    case CSSValueID::kInfobackground:
      return color_scheme == WebColorScheme::kDark ? 0xFFB46E32 : 0xFFFBFCC5;
    case CSSValueID::kInfotext:
      return color_scheme == WebColorScheme::kDark ? 0xFFFFFFFF : 0xFF000000;
    case CSSValueID::kLinktext:
      return 0xFF0000EE;
    case CSSValueID::kMenu:
      return color_scheme == WebColorScheme::kDark ? 0xFF404040 : 0xFFF7F7F7;
    case CSSValueID::kMenutext:
      return color_scheme == WebColorScheme::kDark ? 0xFFFFFFFF : 0xFF000000;
    case CSSValueID::kScrollbar:
      return 0xFFFFFFFF;
    case CSSValueID::kText:
      return color_scheme == WebColorScheme::kDark ? 0xFFFFFFFF : 0xFF000000;
    case CSSValueID::kThreeddarkshadow:
      return 0xFF666666;
    case CSSValueID::kThreedface:
      return 0xFFC0C0C0;
    case CSSValueID::kThreedhighlight:
      return 0xFFDDDDDD;
    case CSSValueID::kThreedlightshadow:
      return 0xFFC0C0C0;
    case CSSValueID::kThreedshadow:
      return 0xFF888888;
    case CSSValueID::kVisitedtext:
      return 0xFF551A8B;
    case CSSValueID::kWindow:
    case CSSValueID::kCanvas:
      return color_scheme == WebColorScheme::kDark ? 0xFF000000 : 0xFFFFFFFF;
    case CSSValueID::kWindowframe:
      return 0xFFCCCCCC;
    case CSSValueID::kWindowtext:
    case CSSValueID::kCanvastext:
      return color_scheme == WebColorScheme::kDark ? 0xFFFFFFFF : 0xFF000000;
    case CSSValueID::kInternalActiveListBoxSelection:
      return ActiveListBoxSelectionBackgroundColor(color_scheme);
    case CSSValueID::kInternalActiveListBoxSelectionText:
      return ActiveListBoxSelectionForegroundColor(color_scheme);
    case CSSValueID::kInternalInactiveListBoxSelection:
      return InactiveListBoxSelectionBackgroundColor(color_scheme);
    case CSSValueID::kInternalInactiveListBoxSelectionText:
      return InactiveListBoxSelectionForegroundColor(color_scheme);
    default:
      break;
  }
  NOTREACHED();
  return Color();
}

Color LayoutTheme::PlatformTextSearchHighlightColor(
    bool active_match,
    bool in_forced_colors_mode,
    WebColorScheme color_scheme) const {
  if (active_match) {
    if (in_forced_colors_mode)
      return GetTheme().SystemColor(CSSValueID::kHighlight, color_scheme);
    return Color(255, 150, 50);  // Orange.
  }
  return Color(255, 255, 0);     // Yellow.
}

Color LayoutTheme::PlatformTextSearchColor(bool active_match,
                                           bool in_forced_colors_mode,
                                           WebColorScheme color_scheme) const {
  if (in_forced_colors_mode && active_match)
    return GetTheme().SystemColor(CSSValueID::kHighlighttext, color_scheme);
  return Color::kBlack;
}

Color LayoutTheme::TapHighlightColor() {
  return GetTheme().PlatformTapHighlightColor();
}

void LayoutTheme::SetCustomFocusRingColor(const Color& c) {
  custom_focus_ring_color_ = c;
  has_custom_focus_ring_color_ = true;
}

Color LayoutTheme::FocusRingColor() const {
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

bool LayoutTheme::SupportsCalendarPicker(const AtomicString& type) const {
  DCHECK(RuntimeEnabledFeatures::InputMultipleFieldsUIEnabled());
  if (features::IsFormControlsRefreshEnabled() &&
      type == input_type_names::kTime)
    return true;

  return type == input_type_names::kDate ||
         type == input_type_names::kDatetime ||
         type == input_type_names::kDatetimeLocal ||
         type == input_type_names::kMonth || type == input_type_names::kWeek;
}

void LayoutTheme::AdjustControlPartStyle(ComputedStyle& style) {
  // Call the appropriate style adjustment method based off the appearance
  // value.
  switch (style.EffectiveAppearance()) {
    case kCheckboxPart:
      return AdjustCheckboxStyle(style);
    case kRadioPart:
      return AdjustRadioStyle(style);
    case kPushButtonPart:
    case kSquareButtonPart:
    case kButtonPart:
      return AdjustButtonStyle(style);
    case kInnerSpinButtonPart:
      return AdjustInnerSpinButtonStyle(style);
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

}  // namespace blink
