/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008, 2009 Google, Inc.
 * All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_DEFAULT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_DEFAULT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/paint/theme_painter_default.h"

namespace blink {

class LayoutProgress;

class CORE_EXPORT LayoutThemeDefault : public LayoutTheme {
 public:
  String ExtraDefaultStyleSheet() override;
  String ExtraQuirksStyleSheet() override;

  Color SystemColor(CSSValueID, WebColorScheme color_scheme) const override;

  bool ThemeDrawsFocusRing(const ComputedStyle&) const override;

  // List Box selection color
  virtual Color ActiveListBoxSelectionBackgroundColor(
      WebColorScheme color_scheme) const;
  virtual Color ActiveListBoxSelectionForegroundColor(
      WebColorScheme color_scheme) const;
  virtual Color InactiveListBoxSelectionBackgroundColor(
      WebColorScheme color_scheme) const;
  virtual Color InactiveListBoxSelectionForegroundColor(
      WebColorScheme color_scheme) const;

  Color PlatformActiveSelectionBackgroundColor(
      WebColorScheme color_scheme) const override;
  Color PlatformInactiveSelectionBackgroundColor(
      WebColorScheme color_scheme) const override;
  Color PlatformActiveSelectionForegroundColor(
      WebColorScheme color_scheme) const override;
  Color PlatformInactiveSelectionForegroundColor(
      WebColorScheme color_scheme) const override;

  IntSize SliderTickSize() const override;
  int SliderTickOffsetFromTrackCenter() const override;
  void AdjustSliderThumbSize(ComputedStyle&) const override;

  void SetCheckboxSize(ComputedStyle&) const override;
  void SetRadioSize(ComputedStyle&) const override;
  void AdjustInnerSpinButtonStyle(ComputedStyle&) const override;
  void AdjustButtonStyle(ComputedStyle&) const override;

  bool PopsMenuBySpaceKey() const final { return true; }
  bool PopsMenuByReturnKey() const final { return true; }
  bool PopsMenuByAltDownUpOrF4Key() const override { return true; }

  bool ShouldOpenPickerWithF4Key() const override;

  Color PlatformTapHighlightColor() const override {
    return Color(kDefaultTapHighlightColor);
  }

  // A method asking if the theme's controls actually care about redrawing
  // when hovered.
  bool SupportsHover(const ComputedStyle&) const final;

  void SetSelectionColors(Color active_background_color,
                          Color active_foreground_color,
                          Color inactive_background_color,
                          Color inactive_foreground_color) override;
  Color PlatformFocusRingColor() const override;

  // System fonts.
  void SystemFont(CSSValueID system_font_id,
                  FontSelectionValue& font_slope,
                  FontSelectionValue& font_weight,
                  float& font_size,
                  AtomicString& font_family) const override;

  int MinimumMenuListSize(const ComputedStyle&) const override;

  void AdjustSearchFieldStyle(ComputedStyle&) const override;
  void AdjustSearchFieldCancelButtonStyle(ComputedStyle&) const override;

  // MenuList refers to an unstyled menulist (meaning a menulist without
  // background-color or border set) and MenuListButton refers to a styled
  // menulist (a menulist with background-color or border set). They have
  // this distinction to support showing aqua style themes whenever they
  // possibly can, which is something we don't want to replicate.
  //
  // In short, we either go down the MenuList code path or the MenuListButton
  // codepath. We never go down both. And in both cases, they layout the
  // entire menulist.
  void AdjustMenuListStyle(ComputedStyle&, Element*) const override;
  void AdjustMenuListButtonStyle(ComputedStyle&, Element*) const override;

  base::TimeDelta AnimationRepeatIntervalForProgressBar() const override;
  base::TimeDelta AnimationDurationForProgressBar() const override;

  // These methods define the padding for the MenuList's inner block.
  int PopupInternalPaddingStart(const ComputedStyle&) const override;
  int PopupInternalPaddingEnd(LocalFrame*, const ComputedStyle&) const override;
  int PopupInternalPaddingTop(const ComputedStyle&) const override;
  int PopupInternalPaddingBottom(const ComputedStyle&) const override;
  // This returns a value based on scrollbar thickness.  It's not 0 even in
  // overlay scrollbar mode.  On Android, this doesn't match to scrollbar
  // thickness, which is 3px or 4px, and we use the value from the default Aura
  // theme.
  int MenuListArrowWidthInDIP() const;
  float ClampedMenuListArrowPaddingSize(LocalFrame*,
                                        const ComputedStyle&) const;

 protected:
  LayoutThemeDefault();
  ~LayoutThemeDefault() override;

  IntRect DeterminateProgressValueRectFor(LayoutProgress*,
                                          const IntRect&) const;
  IntRect IndeterminateProgressValueRectFor(LayoutProgress*,
                                            const IntRect&) const;

 private:
  ThemePainter& Painter() override { return painter_; }
  void DidChangeThemeEngine() override;

  int MenuListInternalPadding(const ComputedStyle&, int padding) const;

  static const RGBA32 kDefaultTapHighlightColor = 0x2e000000;  // 18% black.
  base::TimeDelta caret_blink_interval_;

  Color active_selection_background_color_ = 0xff1e90ff;
  Color active_selection_foreground_color_ = Color::kBlack;
  Color inactive_selection_background_color_ = 0xffc8c8c8;
  Color inactive_selection_foreground_color_ = 0xff323232;

  ThemePainterDefault painter_;
  // Cached values for crbug.com/673754.
  mutable float cached_menu_list_arrow_zoom_level_ = 0;
  mutable float cached_menu_list_arrow_padding_size_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_DEFAULT_H_
