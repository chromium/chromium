/*
 * This file is part of the theme implementation for form controls in WebCore.
 *
 * Copyright (C) 2005 Apple Computer, Inc.
 * Copyright (C) 2008, 2009 Google, Inc.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_MAC_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_MAC_H_

#import <AppKit/AppKit.h>

#include "base/mac/scoped_nsobject.h"
#import "third_party/blink/renderer/core/layout/layout_theme.h"
#import "third_party/blink/renderer/core/paint/theme_painter_mac.h"
#import "third_party/blink/renderer/platform/wtf/hash_map.h"

@class BlinkLayoutThemeNotificationObserver;

namespace blink {

class LayoutThemeMac final : public LayoutTheme {
 public:
  static scoped_refptr<LayoutTheme> Create();

  void AddVisualOverflow(const Node*,
                         const ComputedStyle&,
                         IntRect& border_box) override;

  bool IsControlStyled(ControlPart part, const ComputedStyle&) const override;

  Color PlatformActiveSelectionBackgroundColor(
      WebColorScheme color_scheme) const override;
  Color PlatformInactiveSelectionBackgroundColor(
      WebColorScheme color_scheme) const override;
  Color PlatformActiveSelectionForegroundColor(
      WebColorScheme color_scheme) const override;
  Color PlatformActiveListBoxSelectionBackgroundColor(
      WebColorScheme color_scheme) const override;
  Color PlatformActiveListBoxSelectionForegroundColor(
      WebColorScheme color_scheme) const override;
  Color PlatformInactiveListBoxSelectionBackgroundColor(
      WebColorScheme color_scheme) const override;
  Color PlatformInactiveListBoxSelectionForegroundColor(
      WebColorScheme color_scheme) const override;
  Color PlatformSpellingMarkerUnderlineColor() const override;
  Color PlatformGrammarMarkerUnderlineColor() const override;
  Color PlatformFocusRingColor() const override;

  ScrollbarControlSize ScrollbarControlSizeForPart(ControlPart part) override {
    return part == kListboxPart ? kSmallScrollbar : kRegularScrollbar;
  }

  void PlatformColorsDidChange() override;

  // System fonts.
  void SystemFont(CSSValueID system_font_id,
                  FontSelectionValue& font_slope,
                  FontSelectionValue& font_weight,
                  float& font_size,
                  AtomicString& font_family) const override;

  int MinimumMenuListSize(const ComputedStyle&) const override;

  void AdjustSliderThumbSize(ComputedStyle&) const override;

  IntSize SliderTickSize() const override;
  int SliderTickOffsetFromTrackCenter() const override;

  int PopupInternalPaddingStart(const ComputedStyle&) const override;
  int PopupInternalPaddingEnd(LocalFrame*, const ComputedStyle&) const override;
  int PopupInternalPaddingTop(const ComputedStyle&) const override;
  int PopupInternalPaddingBottom(const ComputedStyle&) const override;

  bool PopsMenuByArrowKeys() const override { return true; }
  bool PopsMenuBySpaceKey() const final { return true; }

  // Returns the repeat interval of the animation for the progress bar.
  base::TimeDelta AnimationRepeatIntervalForProgressBar() const override;
  // Returns the duration of the animation for the progress bar.
  base::TimeDelta AnimationDurationForProgressBar() const override;

  Color SystemColor(CSSValueID, WebColorScheme color_scheme) const override;

  bool SupportsSelectionForegroundColors() const override { return false; }

  bool IsModalColorChooser() const override { return false; }

 protected:
  LayoutThemeMac();
  ~LayoutThemeMac() override;

  void AdjustMenuListStyle(ComputedStyle&, Element*) const override;
  void AdjustMenuListButtonStyle(ComputedStyle&, Element*) const override;
  void AdjustSearchFieldStyle(ComputedStyle&) const override;
  void AdjustSearchFieldCancelButtonStyle(ComputedStyle&) const override;

 public:
  // Constants and methods shared with ThemePainterMac

  // Get the control size based off the font. Used by some of the controls (like
  // buttons).
  NSControlSize ControlSizeForFont(const ComputedStyle&) const;
  NSControlSize ControlSizeForSystemFont(const ComputedStyle&) const;
  void SetControlSize(NSCell*,
                      const IntSize* sizes,
                      const IntSize& min_size,
                      float zoom_level = 1.0f);
  void SetSizeFromFont(ComputedStyle&, const IntSize* sizes) const;
  IntSize SizeForFont(const ComputedStyle&, const IntSize* sizes) const;
  IntSize SizeForSystemFont(const ComputedStyle&, const IntSize* sizes) const;
  void SetFontFromControlSize(ComputedStyle&, NSControlSize) const;

  void UpdateCheckedState(NSCell*, const Node*);
  void UpdateEnabledState(NSCell*, const Node*);
  void UpdateFocusedState(NSCell*, const Node*, const ComputedStyle&);
  void UpdatePressedState(NSCell*, const Node*);

  // Helpers for adjusting appearance and for painting

  void SetPopupButtonCellState(const Node*,
                               const ComputedStyle&,
                               const IntRect&);
  const IntSize* PopupButtonSizes() const;
  const int* PopupButtonMargins() const;
  const int* PopupButtonPadding(NSControlSize) const;
  const IntSize* MenuListSizes() const;

  const IntSize* SearchFieldSizes() const;
  const IntSize* CancelButtonSizes() const;
  void SetSearchCellState(const Node*, const ComputedStyle&, const IntRect&);
  void SetSearchFieldSize(ComputedStyle&) const;

  NSPopUpButtonCell* PopupButton() const;
  NSSearchFieldCell* Search() const;
  NSTextFieldCell* TextField() const;

  // A view associated to the contained document. Subclasses may not have such a
  // view and return a fake.
  NSView* DocumentView() const;

  void UpdateActiveState(NSCell*, const Node*);

  // We estimate the animation rate of a Mac OS X progress bar is 33 fps.
  // Hard code the value here because we haven't found API for it.
  static constexpr base::TimeDelta kProgressAnimationFrameRate =
      base::TimeDelta::FromMilliseconds(33);
  // Mac OS X progress bar animation seems to have 256 frames.
  static constexpr double kProgressAnimationNumFrames = 256;

  static constexpr float kBaseFontSize = 11.0f;
  static constexpr float kMenuListBaseArrowHeight = 4.0f;
  static constexpr float kMenuListBaseArrowWidth = 5.0f;
  static constexpr float kMenuListBaseSpaceBetweenArrows = 2.0f;
  static const int kMenuListArrowPaddingStart = 4;
  static const int kMenuListArrowPaddingEnd = 4;
  static const int kSliderThumbWidth = 15;
  static const int kSliderThumbHeight = 15;
  static const int kSliderThumbShadowBlur = 1;
  static const int kSliderThumbBorderWidth = 1;
  static const int kSliderTrackWidth = 5;
  static const int kSliderTrackBorderWidth = 1;

  LayoutUnit BaselinePositionAdjustment(const ComputedStyle&) const override;

  FontDescription ControlFont(ControlPart,
                              const FontDescription&,
                              float zoom_factor) const override;

  LengthSize GetControlSize(ControlPart,
                            const FontDescription&,
                            const LengthSize&,
                            float zoom_factor) const override;
  LengthSize MinimumControlSize(ControlPart,
                                const FontDescription&,
                                float zoom_factor,
                                const ComputedStyle& style) const override;

  LengthBox ControlPadding(ControlPart,
                           const FontDescription&,
                           const Length& zoomed_box_top,
                           const Length& zoomed_box_right,
                           const Length& zoomed_box_bottom,
                           const Length& zoomed_box_left,
                           float zoom_factor) const override;
  LengthBox ControlBorder(ControlPart,
                          const FontDescription&,
                          const LengthBox& zoomed_box,
                          float zoom_factor) const override;

  bool ControlRequiresPreWhiteSpace(ControlPart part) const override {
    return part == kPushButtonPart;
  }

  // Add visual overflow (e.g., the check on an OS X checkbox). The rect passed
  // in is in zoomed coordinates so the inflation should take that into account
  // and make sure the inflation amount is also scaled by the zoomFactor.
  void AddVisualOverflowHelper(ControlPart,
                               ControlStates,
                               float zoom_factor,
                               IntRect& border_box) const;

  // Adjust style as per platform selection.
  void AdjustControlPartStyle(ComputedStyle&) override;

  // Function for ThemePainter
  static CORE_EXPORT IntRect InflateRect(const IntRect&,
                                         const IntSize&,
                                         const int* margins,
                                         float zoom_level = 1.0f);

  // Inflate an IntRect to account for its focus ring.
  // TODO: Consider using computing the focus ring's bounds with
  // -[NSCell focusRingMaskBoundsForFrame:inView:]).
  static CORE_EXPORT IntRect InflateRectForFocusRing(const IntRect&);

  static CORE_EXPORT LengthSize CheckboxSize(const FontDescription&,
                                             const LengthSize& zoomed_size,
                                             float zoom_factor);
  static CORE_EXPORT NSButtonCell* Checkbox(ControlStates,
                                            const IntRect& zoomed_rect,
                                            float zoom_factor);
  static CORE_EXPORT const IntSize* CheckboxSizes();
  static CORE_EXPORT const int* CheckboxMargins(NSControlSize);
  static CORE_EXPORT NSView* EnsuredView(const IntSize&);

  static CORE_EXPORT const IntSize* RadioSizes();
  static CORE_EXPORT const int* RadioMargins(NSControlSize);
  static CORE_EXPORT LengthSize RadioSize(const FontDescription&,
                                          const LengthSize& zoomed_size,
                                          float zoom_factor);
  static CORE_EXPORT NSButtonCell* Radio(ControlStates,
                                         const IntRect& zoomed_rect,
                                         float zoom_factor);

  static CORE_EXPORT const IntSize* ButtonSizes();
  static CORE_EXPORT const int* ButtonMargins(NSControlSize);
  static CORE_EXPORT NSButtonCell* Button(ControlPart,
                                          ControlStates,
                                          const IntRect& zoomed_rect,
                                          float zoom_factor);

  static CORE_EXPORT NSControlSize
  ControlSizeFromPixelSize(const IntSize* sizes,
                           const IntSize& min_zoomed_size,
                           float zoom_factor);
  static CORE_EXPORT const IntSize* StepperSizes();

 protected:
  String ExtraFullscreenStyleSheet() override;

  // Controls color values returned from platformFocusRingColor(). systemColor()
  // will be used when false.
  bool UsesTestModeFocusRingColor() const;

  bool ShouldUseFallbackTheme(const ComputedStyle&) const override;

  void AdjustProgressBarBounds(ComputedStyle&) const override;

 private:
  const int* ProgressBarHeights() const;
  const int* ProgressBarMargins(NSControlSize) const;
  String FileListNameForWidth(Locale&,
                              const FileList*,
                              const Font&,
                              int width) const override;
  String ExtraDefaultStyleSheet() override;
  bool ThemeDrawsFocusRing(const ComputedStyle&) const override;

  ThemePainter& Painter() override { return painter_; }

  mutable base::scoped_nsobject<NSPopUpButtonCell> popup_button_;
  mutable base::scoped_nsobject<NSSearchFieldCell> search_;
  mutable base::scoped_nsobject<NSTextFieldCell> text_field_;

  mutable HashMap<CSSValueID, RGBA32> system_color_cache_;

  base::scoped_nsobject<BlinkLayoutThemeNotificationObserver>
      notification_observer_;

  ThemePainterMac painter_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_THEME_MAC_H_
