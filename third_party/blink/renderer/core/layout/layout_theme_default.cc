/*
 * Copyright (C) 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2008, 2009 Google Inc.
 * Copyright (C) 2009 Kenneth Rohde Christiansen
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
 *
 */

#include "third_party/blink/renderer/core/layout/layout_theme_default.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_theme_font_provider.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

// These values all match Safari/Win.
static const float kDefaultControlFontPixelSize = 13;
static const float kDefaultCancelButtonSize = 9;
static const float kMinCancelButtonSize = 5;
static const float kMaxCancelButtonSize = 21;

LayoutThemeDefault::LayoutThemeDefault()
    : LayoutTheme(),
      caret_blink_interval_(LayoutTheme::CaretBlinkInterval()),
      painter_(*this) {}

LayoutThemeDefault::~LayoutThemeDefault() = default;

bool LayoutThemeDefault::ThemeDrawsFocusRing(const ComputedStyle& style) const {
  // This causes Blink to draw the focus rings for us.
  return false;
}

Color LayoutThemeDefault::SystemColor(CSSValueID css_value_id,
                                      WebColorScheme color_scheme) const {
  constexpr Color kDefaultButtonGrayColor(0xffdddddd);
  constexpr Color kDefaultButtonGrayColorDark(0xff444444);
  constexpr Color kDefaultMenuColor(0xfff7f7f7);
  constexpr Color kDefaultMenuColorDark(0xff404040);

  if (css_value_id == CSSValueID::kButtonface) {
    switch (color_scheme) {
      case WebColorScheme::kLight:
        return kDefaultButtonGrayColor;
      case WebColorScheme::kDark:
        return kDefaultButtonGrayColorDark;
    }
  }
  if (css_value_id == CSSValueID::kMenu) {
    switch (color_scheme) {
      case WebColorScheme::kLight:
        return kDefaultMenuColor;
      case WebColorScheme::kDark:
        return kDefaultMenuColorDark;
    }
  }
  return LayoutTheme::SystemColor(css_value_id, color_scheme);
}

// Use the Windows style sheets to match their metrics.
String LayoutThemeDefault::ExtraDefaultStyleSheet() {
  String extra_style_sheet = LayoutTheme::ExtraDefaultStyleSheet();
  String multiple_fields_style_sheet =
      RuntimeEnabledFeatures::InputMultipleFieldsUIEnabled()
          ? UncompressResourceAsASCIIString(
                IDR_UASTYLE_THEME_INPUT_MULTIPLE_FIELDS_CSS)
          : String();
  String windows_style_sheet =
      UncompressResourceAsASCIIString(IDR_UASTYLE_THEME_WIN_CSS);
  String controls_refresh_style_sheet =
      RuntimeEnabledFeatures::FormControlsRefreshEnabled()
          ? UncompressResourceAsASCIIString(
                IDR_UASTYLE_THEME_CONTROLS_REFRESH_CSS)
          : String();
  StringBuilder builder;
  builder.ReserveCapacity(
      extra_style_sheet.length() + multiple_fields_style_sheet.length() +
      windows_style_sheet.length() + controls_refresh_style_sheet.length());
  builder.Append(extra_style_sheet);
  builder.Append(multiple_fields_style_sheet);
  builder.Append(windows_style_sheet);
  builder.Append(controls_refresh_style_sheet);
  return builder.ToString();
}

String LayoutThemeDefault::ExtraQuirksStyleSheet() {
  return UncompressResourceAsASCIIString(IDR_UASTYLE_THEME_WIN_QUIRKS_CSS);
}

Color LayoutThemeDefault::ActiveListBoxSelectionBackgroundColor(
    WebColorScheme color_scheme) const {
  return Color(0x28, 0x28, 0x28);
}

Color LayoutThemeDefault::ActiveListBoxSelectionForegroundColor(
    WebColorScheme color_scheme) const {
  return color_scheme == WebColorScheme::kDark ? Color::kWhite : Color::kBlack;
}

Color LayoutThemeDefault::InactiveListBoxSelectionBackgroundColor(
    WebColorScheme color_scheme) const {
  return Color(0xc8, 0xc8, 0xc8);
}

Color LayoutThemeDefault::InactiveListBoxSelectionForegroundColor(
    WebColorScheme color_scheme) const {
  return Color(0x32, 0x32, 0x32);
}

Color LayoutThemeDefault::PlatformActiveSelectionBackgroundColor(
    WebColorScheme color_scheme) const {
  return active_selection_background_color_;
}

Color LayoutThemeDefault::PlatformInactiveSelectionBackgroundColor(
    WebColorScheme color_scheme) const {
  return inactive_selection_background_color_;
}

Color LayoutThemeDefault::PlatformActiveSelectionForegroundColor(
    WebColorScheme color_scheme) const {
  return active_selection_foreground_color_;
}

Color LayoutThemeDefault::PlatformInactiveSelectionForegroundColor(
    WebColorScheme color_scheme) const {
  return inactive_selection_foreground_color_;
}

IntSize LayoutThemeDefault::SliderTickSize() const {
  if (RuntimeEnabledFeatures::FormControlsRefreshEnabled())
    return IntSize(1, 4);
  else
    return IntSize(1, 6);
}

int LayoutThemeDefault::SliderTickOffsetFromTrackCenter() const {
  if (RuntimeEnabledFeatures::FormControlsRefreshEnabled())
    return 7;
  else
    return -16;
}

void LayoutThemeDefault::AdjustSliderThumbSize(ComputedStyle& style) const {
  if (!Platform::Current()->ThemeEngine())
    return;

  IntSize size = Platform::Current()->ThemeEngine()->GetSize(
      WebThemeEngine::kPartSliderThumb);

  float zoom_level = style.EffectiveZoom();
  if (style.EffectiveAppearance() == kSliderThumbHorizontalPart) {
    style.SetWidth(Length::Fixed(size.Width() * zoom_level));
    style.SetHeight(Length::Fixed(size.Height() * zoom_level));
  } else if (style.EffectiveAppearance() == kSliderThumbVerticalPart) {
    style.SetWidth(Length::Fixed(size.Height() * zoom_level));
    style.SetHeight(Length::Fixed(size.Width() * zoom_level));
  }
}

void LayoutThemeDefault::SetSelectionColors(Color active_background_color,
                                            Color active_foreground_color,
                                            Color inactive_background_color,
                                            Color inactive_foreground_color) {
  active_selection_background_color_ = active_background_color;
  active_selection_foreground_color_ = active_foreground_color;
  inactive_selection_background_color_ = inactive_background_color;
  inactive_selection_foreground_color_ = inactive_foreground_color;
  PlatformColorsDidChange();
}

void LayoutThemeDefault::SetCheckboxSize(ComputedStyle& style) const {
  // If the width and height are both specified, then we have nothing to do.
  if (!style.Width().IsIntrinsicOrAuto() && !style.Height().IsAuto())
    return;

  IntSize size = Platform::Current()->ThemeEngine()->GetSize(
      WebThemeEngine::kPartCheckbox);
  float zoom_level = style.EffectiveZoom();
  size.SetWidth(size.Width() * zoom_level);
  size.SetHeight(size.Height() * zoom_level);
  SetMinimumSizeIfAuto(style, size);
  SetSizeIfAuto(style, size);
}

void LayoutThemeDefault::SetRadioSize(ComputedStyle& style) const {
  // If the width and height are both specified, then we have nothing to do.
  if (!style.Width().IsIntrinsicOrAuto() && !style.Height().IsAuto())
    return;

  IntSize size =
      Platform::Current()->ThemeEngine()->GetSize(WebThemeEngine::kPartRadio);
  float zoom_level = style.EffectiveZoom();
  size.SetWidth(size.Width() * zoom_level);
  size.SetHeight(size.Height() * zoom_level);
  SetMinimumSizeIfAuto(style, size);
  SetSizeIfAuto(style, size);
}

void LayoutThemeDefault::AdjustInnerSpinButtonStyle(
    ComputedStyle& style) const {
  IntSize size = Platform::Current()->ThemeEngine()->GetSize(
      WebThemeEngine::kPartInnerSpinButton);

  float zoom_level = style.EffectiveZoom();
  style.SetWidth(Length::Fixed(size.Width() * zoom_level));
  style.SetMinWidth(Length::Fixed(size.Width() * zoom_level));
}

bool LayoutThemeDefault::ShouldOpenPickerWithF4Key() const {
  return true;
}

bool LayoutThemeDefault::SupportsHover(const ComputedStyle& style) const {
  return true;
}

Color LayoutThemeDefault::PlatformFocusRingColor() const {
  constexpr Color focus_ring_color(0xFFE59700);
  return focus_ring_color;
}

void LayoutThemeDefault::SystemFont(CSSValueID system_font_id,
                                    FontSelectionValue& font_slope,
                                    FontSelectionValue& font_weight,
                                    float& font_size,
                                    AtomicString& font_family) const {
  LayoutThemeFontProvider::SystemFont(system_font_id, font_slope, font_weight,
                                      font_size, font_family);
}

int LayoutThemeDefault::MinimumMenuListSize(const ComputedStyle& style) const {
  return 0;
}

// Return a rectangle that has the same center point as |original|, but with a
// size capped at |width| by |height|.
IntRect Center(const IntRect& original, int width, int height) {
  width = std::min(original.Width(), width);
  height = std::min(original.Height(), height);
  int x = original.X() + (original.Width() - width) / 2;
  int y = original.Y() + (original.Height() - height) / 2;

  return IntRect(x, y, width, height);
}

void LayoutThemeDefault::AdjustButtonStyle(ComputedStyle& style) const {
  if (style.EffectiveAppearance() == kPushButtonPart) {
    // Ignore line-height.
    style.SetLineHeight(ComputedStyleInitialValues::InitialLineHeight());
  }
}

void LayoutThemeDefault::AdjustSearchFieldStyle(ComputedStyle& style) const {
  // Ignore line-height.
  style.SetLineHeight(ComputedStyleInitialValues::InitialLineHeight());
}

void LayoutThemeDefault::AdjustSearchFieldCancelButtonStyle(
    ComputedStyle& style) const {
  // Scale the button size based on the font size
  float font_scale = style.FontSize() / kDefaultControlFontPixelSize;
  int cancel_button_size = static_cast<int>(lroundf(std::min(
      std::max(kMinCancelButtonSize, kDefaultCancelButtonSize * font_scale),
      kMaxCancelButtonSize)));
  style.SetWidth(Length::Fixed(cancel_button_size));
  style.SetHeight(Length::Fixed(cancel_button_size));
}

void LayoutThemeDefault::AdjustMenuListStyle(ComputedStyle& style,
                                             Element*) const {
  // Height is locked to auto on all browsers.
  style.SetLineHeight(ComputedStyleInitialValues::InitialLineHeight());
}

void LayoutThemeDefault::AdjustMenuListButtonStyle(ComputedStyle& style,
                                                   Element* e) const {
  AdjustMenuListStyle(style, e);
}

// The following internal paddings are in addition to the user-supplied padding.
// Matches the Firefox behavior.

int LayoutThemeDefault::PopupInternalPaddingStart(
    const ComputedStyle& style) const {
  return MenuListInternalPadding(style, 4);
}

int LayoutThemeDefault::PopupInternalPaddingEnd(
    LocalFrame* frame,
    const ComputedStyle& style) const {
  if (!style.HasEffectiveAppearance())
    return 0;
  return 1 * style.EffectiveZoom() +
         ClampedMenuListArrowPaddingSize(frame, style);
}

int LayoutThemeDefault::PopupInternalPaddingTop(
    const ComputedStyle& style) const {
  return MenuListInternalPadding(style, 1);
}

int LayoutThemeDefault::PopupInternalPaddingBottom(
    const ComputedStyle& style) const {
  return MenuListInternalPadding(style, 1);
}

int LayoutThemeDefault::MenuListArrowWidthInDIP() const {
  int width = Platform::Current()
                  ->ThemeEngine()
                  ->GetSize(WebThemeEngine::kPartScrollbarUpArrow)
                  .width;
  return width > 0 ? width : 15;
}

float LayoutThemeDefault::ClampedMenuListArrowPaddingSize(
    LocalFrame* frame,
    const ComputedStyle& style) const {
  if (cached_menu_list_arrow_padding_size_ > 0 &&
      style.EffectiveZoom() == cached_menu_list_arrow_zoom_level_)
    return cached_menu_list_arrow_padding_size_;
  cached_menu_list_arrow_zoom_level_ = style.EffectiveZoom();
  int original_size = MenuListArrowWidthInDIP();
  int scaled_size = frame->GetPage()->GetChromeClient().WindowToViewportScalar(
      frame, original_size);
  // The result should not be samller than the scrollbar thickness in order to
  // secure space for scrollbar in popup.
  float device_scale = 1.0f * scaled_size / original_size;
  float size;
  if (cached_menu_list_arrow_zoom_level_ < device_scale) {
    size = scaled_size;
  } else {
    // The value should be zoomed though scrollbars aren't scaled by zoom.
    // crbug.com/432795.
    size = original_size * cached_menu_list_arrow_zoom_level_;
  }
  cached_menu_list_arrow_padding_size_ = size;
  return size;
}

void LayoutThemeDefault::DidChangeThemeEngine() {
  cached_menu_list_arrow_zoom_level_ = 0;
  cached_menu_list_arrow_padding_size_ = 0;
}

int LayoutThemeDefault::MenuListInternalPadding(const ComputedStyle& style,
                                                int padding) const {
  if (!style.HasEffectiveAppearance())
    return 0;
  return padding * style.EffectiveZoom();
}

//
// The following values come from the defaults of GTK+.
//
constexpr int kProgressAnimationFrames = 10;
constexpr base::TimeDelta kProgressAnimationInterval =
    base::TimeDelta::FromMilliseconds(125);

base::TimeDelta LayoutThemeDefault::AnimationRepeatIntervalForProgressBar()
    const {
  return kProgressAnimationInterval;
}

base::TimeDelta LayoutThemeDefault::AnimationDurationForProgressBar() const {
  return kProgressAnimationInterval * kProgressAnimationFrames *
         2;  // "2" for back and forth
}

}  // namespace blink
