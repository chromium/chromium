/*
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
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_H_

#include "base/time/time.h"
#include "third_party/blink/public/mojom/frame/color_scheme.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/html/forms/input_type.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/theme_types.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "ui/gfx/geometry/size.h"

namespace ui {
class ColorProvider;
}

namespace blink {

class ComputedStyle;
class ComputedStyleBuilder;
class Element;
class File;
class LocalFrame;
class Node;
class ThemePainter;

class CORE_EXPORT LayoutTheme : public RefCounted<LayoutTheme> {
  USING_FAST_MALLOC(LayoutTheme);

 protected:
  LayoutTheme() = default;

 public:
  virtual ~LayoutTheme() = default;

  static LayoutTheme& GetTheme();

  virtual ThemePainter& Painter() = 0;

  // This method is called whenever style has been computed for an element and
  // the appearance property has been set to a value other than "none".
  // The theme should map in all of the appropriate metrics and defaults given
  // the contents of the style. This includes sophisticated operations like
  // selection of control size based off the font, the disabling of appearance
  // when certain other properties like "border" are set, or if the appearance
  // is not supported by the theme.
  void AdjustStyle(const Element&, ComputedStyleBuilder&);

  // The remaining methods should be implemented by the platform-specific
  // portion of the theme, e.g., layout_theme_mac.mm for macOS.

  // These methods return the theme's extra style sheets rules, to let each
  // platform adjust the default CSS rules in html.css or quirks.css
  virtual String ExtraDefaultStyleSheet();
  virtual String ExtraFullscreenStyleSheet();

  // Whether or not the control has been styled enough by the author to disable
  // the native appearance.
  virtual bool IsControlStyled(AppearanceValue appearance,
                               const ComputedStyleBuilder&) const;

  bool ShouldDrawDefaultFocusRing(const Node*, const ComputedStyle&) const;

  // A method asking if the platform is able to show a calendar picker for a
  // given input type.
  virtual bool SupportsCalendarPicker(InputType::Type) const;

  // Text selection colors.
  Color ActiveSelectionBackgroundColor(
      mojom::blink::ColorScheme color_scheme) const;
  Color InactiveSelectionBackgroundColor(
      mojom::blink::ColorScheme color_scheme) const;
  Color ActiveSelectionForegroundColor(
      mojom::blink::ColorScheme color_scheme) const;
  Color InactiveSelectionForegroundColor(
      mojom::blink::ColorScheme color_scheme) const;
  virtual void SetSelectionColors(Color active_background_color,
                                  Color active_foreground_color,
                                  Color inactive_background_color,
                                  Color inactive_foreground_color);

  // List box selection colors
  Color ActiveListBoxSelectionBackgroundColor(
      mojom::blink::ColorScheme color_scheme) const;
  Color ActiveListBoxSelectionForegroundColor(
      mojom::blink::ColorScheme color_scheme) const;
  Color InactiveListBoxSelectionBackgroundColor(
      mojom::blink::ColorScheme color_scheme) const;
  Color InactiveListBoxSelectionForegroundColor(
      mojom::blink::ColorScheme color_scheme) const;

  virtual Color PlatformSpellingMarkerUnderlineColor() const;
  virtual Color PlatformGrammarMarkerUnderlineColor() const;

  Color PlatformActiveSpellingMarkerHighlightColor() const;

  // Highlight and text colors for TextMatches.
  Color PlatformTextSearchHighlightColor(
      bool active_match,
      bool in_forced_colors,
      mojom::blink::ColorScheme color_scheme,
      const ui::ColorProvider* color_provider,
      bool can_expose_accent_color) const;
  Color PlatformTextSearchColor(bool active_match,
                                bool in_forced_colors,
                                mojom::blink::ColorScheme color_scheme,
                                const ui::ColorProvider* color_provider,
                                bool can_expose_accent_color) const;

  virtual Color FocusRingColor(mojom::blink::ColorScheme color_scheme) const;
  void SetCustomFocusRingColor(const Color&);

  virtual Color TapHighlightColor() const { return kDefaultTapHighlightColor; }

  static Color PlatformDefaultCompositionBackgroundColor() {
    return kDefaultCompositionBackgroundColor;
  }
  void PlatformColorsDidChange();
  virtual void ColorSchemeDidChange();

  void SetCaretBlinkInterval(base::TimeDelta);
  virtual base::TimeDelta CaretBlinkInterval() const;

  // System colors for CSS.
  virtual Color SystemColor(CSSValueID,
                            mojom::blink::ColorScheme color_scheme,
                            const ui::ColorProvider* color_provider,
                            bool can_expose_accent_color) const;

  void AdjustSliderThumbSize(ComputedStyleBuilder&) const;

  virtual int PopupInternalPaddingStart(const ComputedStyle&) const {
    return 0;
  }
  virtual int PopupInternalPaddingEnd(LocalFrame*, const ComputedStyle&) const {
    return 0;
  }
  virtual int PopupInternalPaddingTop(const ComputedStyle&) const { return 0; }
  virtual int PopupInternalPaddingBottom(const ComputedStyle&) const {
    return 0;
  }

  virtual bool PopsMenuByArrowKeys() const { return false; }
  virtual bool PopsMenuByReturnKey() const { return true; }

  virtual String DisplayNameForFile(const File& file) const;

  virtual bool SupportsSelectionForegroundColors() const { return true; }

  virtual bool IsAccentColorCustomized(
      mojom::blink::ColorScheme color_scheme) const;

  // GetSystemAccentColor returns transparent unless there is a special value
  // from the OS color scheme.
  virtual Color GetSystemAccentColor(
      mojom::blink::ColorScheme color_scheme) const;

  // GetAccentColorOrDefault will return GetAccentColor if there is a value from
  // the OS and if it is within an installed WebApp scope, otherwise it will
  // return the default accent color.
  Color GetAccentColorOrDefault(mojom::blink::ColorScheme color_scheme,
                                bool can_expose_accent_color) const;
  // GetAccentColorText returns black or white depending on which can be
  // rendered with enough contrast on the result of GetAccentColorOrDefault.
  Color GetAccentColorText(mojom::blink::ColorScheme color_scheme,
                           bool can_expose_accent_color) const;

  virtual Color SystemHighlightFromColorProvider(
      mojom::blink::ColorScheme color_scheme,
      const ui::ColorProvider* color_provider) const;

 protected:
  static Color active_selection_background_color_;
  static Color active_selection_foreground_color_;
  static Color inactive_selection_background_color_;
  static Color inactive_selection_foreground_color_;
  static Color active_list_box_selection_background_color_dark_mode_;
  static Color active_list_box_selection_foreground_color_dark_mode_;
  static Color inactive_list_box_selection_background_color_dark_mode_;
  static Color inactive_list_box_selection_foreground_color_dark_mode_;

  // The platform selection color.
  virtual Color PlatformActiveSelectionBackgroundColor(
      mojom::blink::ColorScheme color_scheme) const;
  virtual Color PlatformInactiveSelectionBackgroundColor(
      mojom::blink::ColorScheme color_scheme) const;
  virtual Color PlatformActiveSelectionForegroundColor(
      mojom::blink::ColorScheme color_scheme) const;
  virtual Color PlatformInactiveSelectionForegroundColor(
      mojom::blink::ColorScheme color_scheme) const;

  virtual Color PlatformActiveListBoxSelectionBackgroundColor(
      mojom::blink::ColorScheme color_scheme) const;
  virtual Color PlatformInactiveListBoxSelectionBackgroundColor(
      mojom::blink::ColorScheme color_scheme) const;
  virtual Color PlatformActiveListBoxSelectionForegroundColor(
      mojom::blink::ColorScheme color_scheme) const;
  virtual Color PlatformInactiveListBoxSelectionForegroundColor(
      mojom::blink::ColorScheme color_scheme) const;

  // Methods for each appearance value.
  void AdjustCheckboxStyle(ComputedStyleBuilder&) const;
  void AdjustRadioStyle(ComputedStyleBuilder&) const;

  void AdjustPushButtonStyle(ComputedStyleBuilder&) const;
  virtual void AdjustInnerSpinButtonStyle(ComputedStyleBuilder&) const;

  void AdjustMenuListStyle(ComputedStyleBuilder&) const;
  void AdjustSliderThumbStyle(ComputedStyleBuilder&) const;
  virtual void AdjustSearchFieldCancelButtonStyle(ComputedStyleBuilder&) const;

  std::optional<Color> CustomFocusRingColor() const {
    return custom_focus_ring_color_;
  }

  Color DefaultSystemColor(CSSValueID,
                           mojom::blink::ColorScheme color_scheme,
                           const ui::ColorProvider* color_provider,
                           bool can_expose_accent_color) const;
  Color SystemColorFromColorProvider(CSSValueID,
                                     mojom::blink::ColorScheme color_scheme,
                                     const ui::ColorProvider* color_provider,
                                     bool can_expose_accent_color) const;

 private:
  // This function is to be implemented in your platform-specific theme
  // implementation to hand back the appropriate platform theme.
  static LayoutTheme& NativeTheme();

  AppearanceValue AdjustAppearanceWithAuthorStyle(
      AppearanceValue appearance,
      const ComputedStyleBuilder& style);

  AppearanceValue AdjustAppearanceWithElementType(AppearanceValue appearance,
                                                  const Element&);

  void UpdateForcedColorsState();

  std::optional<Color> custom_focus_ring_color_;
  base::TimeDelta caret_blink_interval_ = base::Milliseconds(500);

  // This color is expected to be drawn on a semi-transparent overlay,
  // making it more transparent than its alpha value indicates.
  static constexpr Color kDefaultTapHighlightColor =
      Color::FromRGBA32(0x2e000000);  // 18% black.

  static constexpr Color kDefaultCompositionBackgroundColor =
      Color::FromRGBA32(0xFFFFDD55);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_H_
