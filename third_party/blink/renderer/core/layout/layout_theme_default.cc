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

#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_helper.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/base/ui_base_features.h"

namespace blink {

// These values all match Safari/Win.
static const float kDefaultControlFontPixelSize = 13;
static const float kDefaultCancelButtonSize = 9;
static const float kMinCancelButtonSize = 5;
static const float kMaxCancelButtonSize = 21;

Color LayoutThemeDefault::active_selection_background_color_ =
    Color::FromRGBA32(kDefaultActiveSelectionBgColor);
Color LayoutThemeDefault::active_selection_foreground_color_ =
    Color::FromRGBA32(kDefaultActiveSelectionFgColor);
Color LayoutThemeDefault::inactive_selection_background_color_ =
    Color::FromRGBA32(kDefaultInactiveSelectionBgColor);
Color LayoutThemeDefault::inactive_selection_foreground_color_ =
    Color::FromRGBA32(kDefaultInactiveSelectionFgColor);
Color
    LayoutThemeDefault::active_list_box_selection_background_color_dark_mode_ =
        Color::FromRGBA32(0xFF99C8FF);
Color
    LayoutThemeDefault::active_list_box_selection_foreground_color_dark_mode_ =
        Color::FromRGBA32(0xFF3B3B3B);
Color LayoutThemeDefault::
    inactive_list_box_selection_background_color_dark_mode_ =
        Color::FromRGBA32(0x4D3B3B3B);
Color LayoutThemeDefault::
    inactive_list_box_selection_foreground_color_dark_mode_ =
        Color::FromRGBA32(0xFF323232);

LayoutThemeDefault::LayoutThemeDefault() : painter_(*this) {}

LayoutThemeDefault::~LayoutThemeDefault() = default;

// Use the Windows style sheets to match their metrics.
String LayoutThemeDefault::ExtraDefaultStyleSheet() {
  String extra_style_sheet = LayoutTheme::ExtraDefaultStyleSheet();
  String multiple_fields_style_sheet =
      RuntimeEnabledFeatures::InputMultipleFieldsUIEnabled()
          ? UncompressResourceAsASCIIString(
                IDR_UASTYLE_THEME_INPUT_MULTIPLE_FIELDS_CSS)
          : String();
  StringBuilder builder;
  builder.ReserveCapacity(extra_style_sheet.length() +
                          multiple_fields_style_sheet.length());
  builder.Append(extra_style_sheet);
  builder.Append(multiple_fields_style_sheet);
  return builder.ToString();
}

Color LayoutThemeDefault::PlatformActiveSelectionBackgroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return active_selection_background_color_;
}

Color LayoutThemeDefault::PlatformInactiveSelectionBackgroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return inactive_selection_background_color_;
}

Color LayoutThemeDefault::PlatformActiveSelectionForegroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return active_selection_foreground_color_;
}

Color LayoutThemeDefault::PlatformInactiveSelectionForegroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return inactive_selection_foreground_color_;
}

Color LayoutThemeDefault::PlatformActiveListBoxSelectionBackgroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return color_scheme == mojom::blink::ColorScheme::kDark
             ? active_list_box_selection_background_color_dark_mode_
             : PlatformActiveSelectionBackgroundColor(color_scheme);
}

Color LayoutThemeDefault::PlatformInactiveListBoxSelectionBackgroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return color_scheme == mojom::blink::ColorScheme::kDark
             ? inactive_list_box_selection_background_color_dark_mode_
             : PlatformInactiveSelectionBackgroundColor(color_scheme);
}

Color LayoutThemeDefault::PlatformActiveListBoxSelectionForegroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return color_scheme == mojom::blink::ColorScheme::kDark
             ? active_list_box_selection_foreground_color_dark_mode_
             : PlatformActiveSelectionForegroundColor(color_scheme);
}

Color LayoutThemeDefault::PlatformInactiveListBoxSelectionForegroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return color_scheme == mojom::blink::ColorScheme::kDark
             ? inactive_list_box_selection_foreground_color_dark_mode_
             : PlatformInactiveSelectionForegroundColor(color_scheme);
}

gfx::Size LayoutThemeDefault::SliderTickSize() const {
  // The value should be synchronized with a -webkit-slider-container rule in
  // html.css.
  return gfx::Size(1, 4);
}

int LayoutThemeDefault::SliderTickOffsetFromTrackCenter() const {
  // The value should be synchronized with a -webkit-slider-container rule in
  // html.css and LayoutThemeAndroid::ExtraDefaultStyleSheet().
  return 7;
}

void LayoutThemeDefault::AdjustSliderThumbSize(
    ComputedStyleBuilder& builder) const {
  gfx::Size size = WebThemeEngineHelper::GetNativeThemeEngine()->GetSize(
      WebThemeEngine::kPartSliderThumb);

  float zoom_level = builder.EffectiveZoom();
  if (builder.EffectiveAppearance() == kSliderThumbHorizontalPart) {
    builder.SetWidth(Length::Fixed(size.width() * zoom_level));
    builder.SetHeight(Length::Fixed(size.height() * zoom_level));
  } else if (builder.EffectiveAppearance() == kSliderThumbVerticalPart) {
    builder.SetWidth(Length::Fixed(size.height() * zoom_level));
    builder.SetHeight(Length::Fixed(size.width() * zoom_level));
  }
}

void LayoutThemeDefault::SetSelectionColors(Color active_background_color,
                                            Color active_foreground_color,
                                            Color inactive_background_color,
                                            Color inactive_foreground_color) {
  if (active_selection_background_color_ != active_background_color ||
      active_selection_foreground_color_ != active_foreground_color ||
      inactive_selection_background_color_ != inactive_background_color ||
      inactive_selection_foreground_color_ != inactive_foreground_color) {
    active_selection_background_color_ = active_background_color;
    active_selection_foreground_color_ = active_foreground_color;
    inactive_selection_background_color_ = inactive_background_color;
    inactive_selection_foreground_color_ = inactive_foreground_color;
    PlatformColorsDidChange();
  }
}

void LayoutThemeDefault::AdjustInnerSpinButtonStyle(
    ComputedStyleBuilder& style) const {
  gfx::Size size = WebThemeEngineHelper::GetNativeThemeEngine()->GetSize(
      WebThemeEngine::kPartInnerSpinButton);

  float zoom_level = style.EffectiveZoom();
  if (IsHorizontalWritingMode(style.GetWritingMode())) {
    style.SetWidth(Length::Fixed(size.width() * zoom_level));
    style.SetMinWidth(Length::Fixed(size.width() * zoom_level));
  } else {
    style.SetHeight(Length::Fixed(size.width() * zoom_level));
    style.SetMinHeight(Length::Fixed(size.width() * zoom_level));
  }
}

Color LayoutThemeDefault::PlatformFocusRingColor() const {
  constexpr Color focus_ring_color = Color::FromRGBA32(0xFFE59700);
  return focus_ring_color;
}

void LayoutThemeDefault::AdjustButtonStyle(
    ComputedStyleBuilder& builder) const {
  // Ignore line-height.
  if (builder.EffectiveAppearance() == kPushButtonPart)
    builder.SetLineHeight(ComputedStyleInitialValues::InitialLineHeight());
}

void LayoutThemeDefault::AdjustSearchFieldCancelButtonStyle(
    ComputedStyleBuilder& builder) const {
  // Scale the button size based on the font size
  float font_scale = builder.FontSize() / kDefaultControlFontPixelSize;
  int cancel_button_size = static_cast<int>(lroundf(std::min(
      std::max(kMinCancelButtonSize, kDefaultCancelButtonSize * font_scale),
      kMaxCancelButtonSize)));
  builder.SetWidth(Length::Fixed(cancel_button_size));
  builder.SetHeight(Length::Fixed(cancel_button_size));
}

void LayoutThemeDefault::AdjustMenuListStyle(
    ComputedStyleBuilder& builder) const {
  LayoutTheme::AdjustMenuListStyle(builder);
  // Height is locked to auto on all browsers.
  builder.ResetLineHeight();
}

void LayoutThemeDefault::AdjustMenuListButtonStyle(
    ComputedStyleBuilder& builder) const {
  AdjustMenuListStyle(builder);
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
  int width = WebThemeEngineHelper::GetNativeThemeEngine()
                  ->GetSize(WebThemeEngine::kPartScrollbarUpArrow)
                  .width();
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

int LayoutThemeDefault::MenuListInternalPadding(const ComputedStyle& style,
                                                int padding) const {
  if (!style.HasEffectiveAppearance())
    return 0;
  return padding * style.EffectiveZoom();
}

}  // namespace blink
