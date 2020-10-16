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

#include "third_party/blink/public/common/css/color_scheme.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/geometry/length_box.h"
#include "third_party/blink/renderer/platform/geometry/length_size.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/theme_types.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class ComputedStyle;
class Element;
class File;
class FontDescription;
class LocalFrame;
class Node;
class ThemePainter;

class CORE_EXPORT LayoutTheme : public RefCounted<LayoutTheme> {
  USING_FAST_MALLOC(LayoutTheme);

 protected:
  LayoutTheme();

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
  void AdjustStyle(const Element*, ComputedStyle&);

  // The remaining methods should be implemented by the platform-specific
  // portion of the theme, e.g., LayoutThemeMac.cpp for Mac OS X.

  // These methods return the theme's extra style sheets rules, to let each
  // platform adjust the default CSS rules in html.css or quirks.css
  virtual String ExtraDefaultStyleSheet();
  virtual String ExtraQuirksStyleSheet();
  virtual String ExtraFullscreenStyleSheet();

  // Whether or not the control has been styled enough by the author to disable
  // the native appearance.
  virtual bool IsControlStyled(ControlPart part, const ComputedStyle&) const;

  bool ShouldDrawDefaultFocusRing(const Node*, const ComputedStyle&) const;

  // A method asking if the platform is able to show a calendar picker for a
  // given input type.
  virtual bool SupportsCalendarPicker(const AtomicString&) const;

  // Text selection colors.
  Color ActiveSelectionBackgroundColor(ColorScheme color_scheme) const;
  Color InactiveSelectionBackgroundColor(ColorScheme color_scheme) const;
  Color ActiveSelectionForegroundColor(ColorScheme color_scheme) const;
  Color InactiveSelectionForegroundColor(ColorScheme color_scheme) const;
  virtual void SetSelectionColors(Color active_background_color,
                                  Color active_foreground_color,
                                  Color inactive_background_color,
                                  Color inactive_foreground_color) {}

  // List box selection colors
  Color ActiveListBoxSelectionBackgroundColor(ColorScheme color_scheme) const;
  Color ActiveListBoxSelectionForegroundColor(ColorScheme color_scheme) const;
  Color InactiveListBoxSelectionBackgroundColor(ColorScheme color_scheme) const;
  Color InactiveListBoxSelectionForegroundColor(ColorScheme color_scheme) const;

  virtual Color PlatformSpellingMarkerUnderlineColor() const;
  virtual Color PlatformGrammarMarkerUnderlineColor() const;

  Color PlatformActiveSpellingMarkerHighlightColor() const;

  // Highlight and text colors for TextMatches.
  Color PlatformTextSearchHighlightColor(bool active_match,
                                         bool in_forced_colors_mode,
                                         ColorScheme color_scheme) const;
  Color PlatformTextSearchColor(bool active_match,
                                bool in_forced_colors_mode,
                                ColorScheme color_scheme) const;

  virtual Color FocusRingColor() const;
  virtual Color PlatformFocusRingColor() const { return Color(0, 0, 0); }
  void SetCustomFocusRingColor(const Color&);
  static Color TapHighlightColor();

  virtual Color PlatformTapHighlightColor() const {
    return LayoutTheme::kDefaultTapHighlightColor;
  }
  virtual Color PlatformDefaultCompositionBackgroundColor() const {
    return kDefaultCompositionBackgroundColor;
  }
  void PlatformColorsDidChange();
  virtual void ColorSchemeDidChange();

  void SetCaretBlinkInterval(base::TimeDelta);
  virtual base::TimeDelta CaretBlinkInterval() const;

  // System fonts and colors for CSS.
  void SystemFont(CSSValueID system_font_id, FontDescription&);
  virtual Color SystemColor(CSSValueID, ColorScheme color_scheme) const;

  virtual void AdjustSliderThumbSize(ComputedStyle&) const;

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

  // Returns size of one slider tick mark for a horizontal track.
  // For vertical tracks we rotate it and use it. i.e. Width is always length
  // along the track.
  virtual IntSize SliderTickSize() const = 0;
  // Returns the distance of slider tick origin from the slider track center.
  virtual int SliderTickOffsetFromTrackCenter() const = 0;

  // Functions for <select> elements.
  virtual bool DelegatesMenuListRendering() const;
  // This function has no effect for LayoutThemeAndroid, of which
  // DelegatesMenuListRendering() always returns true.
  void SetDelegatesMenuListRenderingForTesting(bool flag);
  virtual bool PopsMenuByArrowKeys() const { return false; }
  virtual bool PopsMenuByReturnKey() const { return true; }

  virtual String DisplayNameForFile(const File& file) const;

  virtual bool SupportsSelectionForegroundColors() const { return true; }

  // Adjust style as per platform selection.
  virtual void AdjustControlPartStyle(ComputedStyle&);

 protected:
  // The platform selection color.
  virtual Color PlatformActiveSelectionBackgroundColor(
      ColorScheme color_scheme) const;
  virtual Color PlatformInactiveSelectionBackgroundColor(
      ColorScheme color_scheme) const;
  virtual Color PlatformActiveSelectionForegroundColor(
      ColorScheme color_scheme) const;
  virtual Color PlatformInactiveSelectionForegroundColor(
      ColorScheme color_scheme) const;

  virtual Color PlatformActiveListBoxSelectionBackgroundColor(
      ColorScheme color_scheme) const;
  virtual Color PlatformInactiveListBoxSelectionBackgroundColor(
      ColorScheme color_scheme) const;
  virtual Color PlatformActiveListBoxSelectionForegroundColor(
      ColorScheme color_scheme) const;
  virtual Color PlatformInactiveListBoxSelectionForegroundColor(
      ColorScheme color_scheme) const;

  // Methods for each appearance value.
  virtual void AdjustCheckboxStyle(ComputedStyle&) const;
  virtual void SetCheckboxSize(ComputedStyle&) const {}

  virtual void AdjustRadioStyle(ComputedStyle&) const;
  virtual void SetRadioSize(ComputedStyle&) const {}

  virtual void AdjustButtonStyle(ComputedStyle&) const;
  virtual void AdjustInnerSpinButtonStyle(ComputedStyle&) const;

  virtual void AdjustMenuListStyle(ComputedStyle&) const;
  virtual void AdjustMenuListButtonStyle(ComputedStyle&) const;
  virtual void AdjustSliderContainerStyle(const Element&, ComputedStyle&) const;
  virtual void AdjustSliderThumbStyle(ComputedStyle&) const;
  virtual void AdjustSearchFieldStyle(ComputedStyle&) const;
  virtual void AdjustSearchFieldCancelButtonStyle(ComputedStyle&) const;

  bool HasCustomFocusRingColor() const;
  Color GetCustomFocusRingColor() const;

 private:
  // This function is to be implemented in your platform-specific theme
  // implementation to hand back the appropriate platform theme.
  static LayoutTheme& NativeTheme();

  ControlPart AdjustAppearanceWithAuthorStyle(ControlPart part,
                                              const ComputedStyle& style);

  ControlPart AdjustAppearanceWithElementType(const ComputedStyle& style,
                                              const Element* element);

  Color custom_focus_ring_color_;
  bool has_custom_focus_ring_color_;
  base::TimeDelta caret_blink_interval_ =
      base::TimeDelta::FromMilliseconds(500);

  bool delegates_menu_list_rendering_ = false;

  // This color is expected to be drawn on a semi-transparent overlay,
  // making it more transparent than its alpha value indicates.
  static const RGBA32 kDefaultTapHighlightColor = 0x66000000;

  static const RGBA32 kDefaultCompositionBackgroundColor = 0xFFFFDD55;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_H_
