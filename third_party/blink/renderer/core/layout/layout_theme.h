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

#include "third_party/blink/public/platform/web_color_scheme.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
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
class FileList;
class Font;
class FontDescription;
class HTMLInputElement;
class LengthSize;
class Locale;
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

  // This function is called after associated WebThemeEngine instance
  // was replaced. This is called only in tests.
  virtual void DidChangeThemeEngine() {}

  static void SetSizeIfAuto(ComputedStyle&, const IntSize&);
  // Sets the minimum size to |part_size| or |min_part_size| as appropriate
  // according to the given style, if they are specified.
  static void SetMinimumSize(ComputedStyle&,
                             const LengthSize* part_size,
                             const LengthSize* min_part_size = nullptr);
  // SetMinimumSizeIfAuto must be called before SetSizeIfAuto, because we
  // will not set a minimum size if an explicit size is set, and SetSizeIfAuto
  // sets an explicit size.
  static void SetMinimumSizeIfAuto(ComputedStyle&, const IntSize&);

  // This method is called whenever style has been computed for an element and
  // the appearance property has been set to a value other than "none".
  // The theme should map in all of the appropriate metrics and defaults given
  // the contents of the style. This includes sophisticated operations like
  // selection of control size based off the font, the disabling of appearance
  // when certain other properties like "border" are set, or if the appearance
  // is not supported by the theme.
  void AdjustStyle(ComputedStyle&, Element*);

  // The remaining methods should be implemented by the platform-specific
  // portion of the theme, e.g., LayoutThemeMac.cpp for Mac OS X.

  // These methods return the theme's extra style sheets rules, to let each
  // platform adjust the default CSS rules in html.css or quirks.css
  virtual String ExtraDefaultStyleSheet();
  virtual String ExtraQuirksStyleSheet();
  virtual String ExtraFullscreenStyleSheet();

  // A method to obtain the baseline position adjustment needed for a "leaf"
  // control. This will only be used if a baseline position cannot be determined
  // by examining child content.
  // Checkboxes and radio buttons are examples of controls that need to do this.
  virtual LayoutUnit BaselinePositionAdjustment(const ComputedStyle&) const;

  // A method for asking if a control is a container or not.  Leaf controls have
  // to have some special behavior (like the baseline position API above).
  bool IsControlContainer(ControlPart) const;

  // Whether or not the control has been styled enough by the author to disable
  // the native appearance.
  virtual bool IsControlStyled(ControlPart part, const ComputedStyle&) const;

  // Some controls may spill out of their containers (e.g., the check on an OSX
  // 10.9 checkbox). Add this "visual overflow" to the object's border box rect.
  virtual void AddVisualOverflow(const Node*,
                                 const ComputedStyle&,
                                 IntRect& border_box) {}

  // This method is called whenever a control state changes on a particular
  // themed object, e.g., the mouse becomes pressed or a control becomes
  // disabled. The ControlState parameter indicates which state has changed
  // (from having to not having, or vice versa).
  bool ControlStateChanged(const Node*,
                           const ComputedStyle&,
                           ControlState) const;

  bool ShouldDrawDefaultFocusRing(const Node*, const ComputedStyle&) const;

  // A method asking if the theme's controls actually care about redrawing when
  // hovered.
  virtual bool SupportsHover(const ComputedStyle&) const { return false; }

  // A method asking if the platform is able to show a calendar picker for a
  // given input type.
  virtual bool SupportsCalendarPicker(const AtomicString&) const;

  // Text selection colors.
  Color ActiveSelectionBackgroundColor(WebColorScheme color_scheme) const;
  Color InactiveSelectionBackgroundColor(WebColorScheme color_scheme) const;
  Color ActiveSelectionForegroundColor(WebColorScheme color_scheme) const;
  Color InactiveSelectionForegroundColor(WebColorScheme color_scheme) const;
  virtual void SetSelectionColors(Color active_background_color,
                                  Color active_foreground_color,
                                  Color inactive_background_color,
                                  Color inactive_foreground_color) {}

  // List box selection colors
  Color ActiveListBoxSelectionBackgroundColor(
      WebColorScheme color_scheme) const;
  Color ActiveListBoxSelectionForegroundColor(
      WebColorScheme color_scheme) const;
  Color InactiveListBoxSelectionBackgroundColor(
      WebColorScheme color_scheme) const;
  Color InactiveListBoxSelectionForegroundColor(
      WebColorScheme color_scheme) const;

  virtual Color PlatformSpellingMarkerUnderlineColor() const;
  virtual Color PlatformGrammarMarkerUnderlineColor() const;

  Color PlatformActiveSpellingMarkerHighlightColor() const;

  // Highlight and text colors for TextMatches.
  Color PlatformTextSearchHighlightColor(bool active_match,
                                         bool in_forced_colors_mode,
                                         WebColorScheme color_scheme) const;
  Color PlatformTextSearchColor(bool active_match,
                                bool in_forced_colors_mode,
                                WebColorScheme color_scheme) const;

  virtual bool IsFocusRingOutset() const;
  Color FocusRingColor() const;
  virtual Color PlatformFocusRingColor() const { return Color(0, 0, 0); }
  void SetCustomFocusRingColor(const Color&);
  static Color TapHighlightColor();

  // Root element text color. It can be different from the initial color in
  // other color schemes than the light theme.
  Color RootElementColor(WebColorScheme) const;

  virtual Color PlatformTapHighlightColor() const {
    return LayoutTheme::kDefaultTapHighlightColor;
  }
  virtual Color PlatformDefaultCompositionBackgroundColor() const {
    return kDefaultCompositionBackgroundColor;
  }
  virtual void PlatformColorsDidChange();
  virtual void ColorSchemeDidChange();

  void SetCaretBlinkInterval(base::TimeDelta);
  virtual base::TimeDelta CaretBlinkInterval() const;

  // System fonts and colors for CSS.
  virtual void SystemFont(CSSValueID system_font_id,
                          FontSelectionValue& font_slope,
                          FontSelectionValue& font_weight,
                          float& font_size,
                          AtomicString& font_family) const = 0;
  void SystemFont(CSSValueID system_font_id, FontDescription&);
  virtual Color SystemColor(CSSValueID, WebColorScheme color_scheme) const;

  virtual int MinimumMenuListSize(const ComputedStyle&) const { return 0; }

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

  virtual ScrollbarControlSize ScrollbarControlSizeForPart(ControlPart) {
    return kRegularScrollbar;
  }

  virtual void AdjustProgressBarBounds(ComputedStyle& style) const {}

  // Returns the repeat interval of the animation for the progress bar.
  virtual base::TimeDelta AnimationRepeatIntervalForProgressBar() const;
  // Returns the duration of the animation for the progress bar.
  virtual base::TimeDelta AnimationDurationForProgressBar() const;

  // Returns size of one slider tick mark for a horizontal track.
  // For vertical tracks we rotate it and use it. i.e. Width is always length
  // along the track.
  virtual IntSize SliderTickSize() const = 0;
  // Returns the distance of slider tick origin from the slider track center.
  virtual int SliderTickOffsetFromTrackCenter() const = 0;

  virtual bool ShouldHaveSpinButton(HTMLInputElement*) const;

  // Functions for <select> elements.
  virtual bool DelegatesMenuListRendering() const { return false; }
  virtual bool PopsMenuByArrowKeys() const { return false; }
  virtual bool PopsMenuBySpaceKey() const { return false; }
  virtual bool PopsMenuByReturnKey() const { return false; }
  virtual bool PopsMenuByAltDownUpOrF4Key() const { return false; }

  virtual String FileListNameForWidth(Locale&,
                                      const FileList*,
                                      const Font&,
                                      int width) const;

  virtual bool ShouldOpenPickerWithF4Key() const;

  virtual bool SupportsSelectionForegroundColors() const { return true; }

  virtual bool IsModalColorChooser() const { return true; }

  virtual bool ShouldUseFallbackTheme(const ComputedStyle&) const;

  // Methods used to adjust the ComputedStyles of controls.

  // The font description result should have a zoomed font size.
  virtual FontDescription ControlFont(ControlPart,
                                      const FontDescription& font_description,
                                      float /*zoomFactor*/) const {
    return font_description;
  }

  // The size here is in zoomed coordinates already.  If a new size is returned,
  // it also needs to be in zoomed coordinates.
  virtual LengthSize GetControlSize(ControlPart,
                                    const FontDescription&,
                                    const LengthSize& zoomed_size,
                                    float /*zoomFactor*/) const {
    return zoomed_size;
  }

  // Returns the minimum size for a control in zoomed coordinates.
  virtual LengthSize MinimumControlSize(ControlPart,
                                        const FontDescription&,
                                        float /*zoomFactor*/,
                                        const ComputedStyle& style) const {
    return LengthSize(Length::Fixed(0), Length::Fixed(0));
  }

  // Allows the theme to modify the existing padding/border.
  virtual LengthBox ControlPadding(ControlPart,
                                   const FontDescription&,
                                   const Length& zoomed_box_top,
                                   const Length& zoomed_box_right,
                                   const Length& zoomed_box_bottom,
                                   const Length& zoomed_box_left,
                                   float zoom_factor) const;
  virtual LengthBox ControlBorder(ControlPart,
                                  const FontDescription&,
                                  const LengthBox& zoomed_box,
                                  float zoom_factor) const;

  // Whether or not whitespace: pre should be forced on always.
  virtual bool ControlRequiresPreWhiteSpace(ControlPart) const { return false; }

  // Adjust style as per platform selection.
  virtual void AdjustControlPartStyle(ComputedStyle&);

 protected:
  // The platform selection color.
  virtual Color PlatformActiveSelectionBackgroundColor(
      WebColorScheme color_scheme) const;
  virtual Color PlatformInactiveSelectionBackgroundColor(
      WebColorScheme color_scheme) const;
  virtual Color PlatformActiveSelectionForegroundColor(
      WebColorScheme color_scheme) const;
  virtual Color PlatformInactiveSelectionForegroundColor(
      WebColorScheme color_scheme) const;

  virtual Color PlatformActiveListBoxSelectionBackgroundColor(
      WebColorScheme color_scheme) const;
  virtual Color PlatformInactiveListBoxSelectionBackgroundColor(
      WebColorScheme color_scheme) const;
  virtual Color PlatformActiveListBoxSelectionForegroundColor(
      WebColorScheme color_scheme) const;
  virtual Color PlatformInactiveListBoxSelectionForegroundColor(
      WebColorScheme color_scheme) const;

  virtual bool ThemeDrawsFocusRing(const ComputedStyle&) const = 0;

  // Methods for each appearance value.
  virtual void AdjustCheckboxStyle(ComputedStyle&) const;
  virtual void SetCheckboxSize(ComputedStyle&) const {}

  virtual void AdjustRadioStyle(ComputedStyle&) const;
  virtual void SetRadioSize(ComputedStyle&) const {}

  virtual void AdjustButtonStyle(ComputedStyle&) const;
  virtual void AdjustInnerSpinButtonStyle(ComputedStyle&) const;

  virtual void AdjustMenuListStyle(ComputedStyle&, Element*) const;
  virtual void AdjustMenuListButtonStyle(ComputedStyle&, Element*) const;
  virtual void AdjustSliderContainerStyle(ComputedStyle&, Element*) const;
  virtual void AdjustSliderThumbStyle(ComputedStyle&) const;
  virtual void AdjustSearchFieldStyle(ComputedStyle&) const;
  virtual void AdjustSearchFieldCancelButtonStyle(ComputedStyle&) const;
  void AdjustStyleUsingFallbackTheme(ComputedStyle&);
  void AdjustCheckboxStyleUsingFallbackTheme(ComputedStyle&) const;
  void AdjustRadioStyleUsingFallbackTheme(ComputedStyle&) const;

 public:
  // Methods for state querying
  static ControlStates ControlStatesForNode(const Node*, const ComputedStyle&);
  static bool IsActive(const Node*);
  static bool IsChecked(const Node*);
  static bool IsIndeterminate(const Node*);
  static bool IsEnabled(const Node*);
  static bool IsFocused(const Node*);
  static bool IsPressed(const Node*);
  static bool IsSpinUpButtonPartPressed(const Node*);
  static bool IsHovered(const Node*);
  static bool IsSpinUpButtonPartHovered(const Node*);
  static bool IsReadOnlyControl(const Node*);

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

  // This color is expected to be drawn on a semi-transparent overlay,
  // making it more transparent than its alpha value indicates.
  static const RGBA32 kDefaultTapHighlightColor = 0x66000000;

  static const RGBA32 kDefaultCompositionBackgroundColor = 0xFFFFDD55;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_H_
