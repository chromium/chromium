/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
 */

#import "third_party/blink/renderer/core/paint/theme_painter_mac.h"

#import <AvailabilityMacros.h>
#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <math.h>
#import "third_party/blink/renderer/core/frame/local_frame_view.h"
#import "third_party/blink/renderer/core/layout/layout_progress.h"
#import "third_party/blink/renderer/core/layout/layout_theme_mac.h"
#import "third_party/blink/renderer/core/layout/layout_view.h"
#import "third_party/blink/renderer/core/paint/paint_info.h"
#import "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#import "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#import "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#import "third_party/blink/renderer/platform/graphics/image.h"
#import "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#import "third_party/blink/renderer/platform/mac/block_exceptions.h"
#import "third_party/blink/renderer/platform/mac/color_mac.h"
#import "third_party/blink/renderer/platform/mac/local_current_graphics_context.h"
#import "third_party/blink/renderer/platform/mac/web_core_ns_cell_extras.h"

// The methods in this file are specific to the Mac OS X platform.

// Forward declare Mac SPIs.
extern "C" {
void _NSDrawCarbonThemeBezel(NSRect frame, BOOL enabled, BOOL flipped);
// Request for public API: rdar://13787640
void _NSDrawCarbonThemeListBox(NSRect frame,
                               BOOL enabled,
                               BOOL flipped,
                               BOOL always_yes);
}

namespace blink {

class ScopedColorSchemeAppearance {
 public:
  ScopedColorSchemeAppearance(WebColorScheme color_scheme) {
    if (@available(macOS 10.14, *)) {
      old_appearance = [NSAppearance currentAppearance];
      [NSAppearance
          setCurrentAppearance:
              [NSAppearance
                  appearanceNamed:color_scheme == WebColorScheme::kDark
                                      ? NSAppearanceNameDarkAqua
                                      : NSAppearanceNameAqua]];
    }
  }
  ~ScopedColorSchemeAppearance() {
    if (@available(macOS 10.14, *))
      [NSAppearance setCurrentAppearance:old_appearance];
  }

 private:
  NSAppearance* old_appearance;
};

ThemePainterMac::ThemePainterMac(LayoutThemeMac& layout_theme)
    : ThemePainter(), layout_theme_(layout_theme) {}

bool ThemePainterMac::PaintTextField(const Node* node,
                                     const ComputedStyle& style,
                                     const PaintInfo& paint_info,
                                     const IntRect& r) {
  ScopedColorSchemeAppearance appearance(style.UsedColorScheme());
  LocalCurrentGraphicsContext local_context(paint_info.context, r);

  bool use_ns_text_field_cell =
      style.HasEffectiveAppearance() &&
      style.VisitedDependentColor(GetCSSPropertyBackgroundColor()) ==
          Color::kWhite &&
      !style.HasBackgroundImage();

  // We do not use NSTextFieldCell to draw styled text fields since it induces a
  // behavior change while remaining a fragile solution.
  // https://bugs.chromium.org/p/chromium/issues/detail?id=658085#c3
  if (!use_ns_text_field_cell) {
    _NSDrawCarbonThemeBezel(
        CGRect(r),
        LayoutTheme::IsEnabled(node) && !LayoutTheme::IsReadOnlyControl(node),
        YES);
    return false;
  }

  NSTextFieldCell* text_field = layout_theme_.TextField();

  GraphicsContextStateSaver state_saver(paint_info.context);

  [text_field setEnabled:(LayoutTheme::IsEnabled(node) &&
                          !LayoutTheme::IsReadOnlyControl(node))];
  [text_field drawWithFrame:NSRect(r) inView:layout_theme_.DocumentView()];

  [text_field setControlView:nil];

  return false;
}

bool ThemePainterMac::PaintCapsLockIndicator(const LayoutObject& o,
                                             const PaintInfo& paint_info,
                                             const IntRect& r) {
  // This draws the caps lock indicator as it was done by
  // WKDrawCapsLockIndicator.
  ScopedColorSchemeAppearance appearance(o.StyleRef().UsedColorScheme());
  LocalCurrentGraphicsContext local_context(paint_info.context, r);
  CGContextRef c = local_context.CgContext();
  CGMutablePathRef shape = CGPathCreateMutable();

  // To draw the caps lock indicator, draw the shape into a small
  // square that is then scaled to the size of r.
  const CGFloat kSquareSize = 17;

  // Create a rounted square shape.
  CGPathMoveToPoint(shape, NULL, 16.5, 4.5);
  CGPathAddArc(shape, NULL, 12.5, 12.5, 4, 0, M_PI_2, false);
  CGPathAddArc(shape, NULL, 4.5, 12.5, 4, M_PI_2, M_PI, false);
  CGPathAddArc(shape, NULL, 4.5, 4.5, 4, M_PI, 3 * M_PI / 2, false);
  CGPathAddArc(shape, NULL, 12.5, 4.5, 4, 3 * M_PI / 2, 0, false);

  // Draw the arrow - note this is drawing in a flipped coordinate system, so
  // the arrow is pointing down.
  CGPathMoveToPoint(shape, NULL, 8.5, 2);  // Tip point.
  CGPathAddLineToPoint(shape, NULL, 4, 7);
  CGPathAddLineToPoint(shape, NULL, 6.25, 7);
  CGPathAddLineToPoint(shape, NULL, 6.25, 10.25);
  CGPathAddLineToPoint(shape, NULL, 10.75, 10.25);
  CGPathAddLineToPoint(shape, NULL, 10.75, 7);
  CGPathAddLineToPoint(shape, NULL, 13, 7);
  CGPathAddLineToPoint(shape, NULL, 8.5, 2);

  // Draw the rectangle that underneath (or above in the flipped system) the
  // arrow.
  CGPathAddLineToPoint(shape, NULL, 10.75, 12);
  CGPathAddLineToPoint(shape, NULL, 6.25, 12);
  CGPathAddLineToPoint(shape, NULL, 6.25, 14.25);
  CGPathAddLineToPoint(shape, NULL, 10.75, 14.25);
  CGPathAddLineToPoint(shape, NULL, 10.75, 12);

  // Scale and translate the shape.
  CGRect cgr = CGRect(r);
  CGFloat max_x = CGRectGetMaxX(cgr);
  CGFloat min_x = CGRectGetMinX(cgr);
  CGFloat min_y = CGRectGetMinY(cgr);
  CGFloat height_scale = r.Height() / kSquareSize;
  const bool is_rtl = o.StyleRef().Direction() == TextDirection::kRtl;
  CGAffineTransform transform = CGAffineTransformMake(
      height_scale, 0,                              // A  B
      0, height_scale,                              // C  D
      is_rtl ? min_x : max_x - r.Height(), min_y);  // Tx Ty

  CGMutablePathRef paint_path = CGPathCreateMutable();
  CGPathAddPath(paint_path, &transform, shape);
  CGPathRelease(shape);

  CGContextSetRGBFillColor(c, 0, 0, 0, 0.4);
  CGContextBeginPath(c);
  CGContextAddPath(c, paint_path);
  CGContextFillPath(c);
  CGPathRelease(paint_path);

  return false;
}

bool ThemePainterMac::PaintTextArea(const Node* node,
                                    const ComputedStyle& style,
                                    const PaintInfo& paint_info,
                                    const IntRect& r) {
  ScopedColorSchemeAppearance appearance(style.UsedColorScheme());
  LocalCurrentGraphicsContext local_context(paint_info.context, r);
  _NSDrawCarbonThemeListBox(
      CGRect(r),
      LayoutTheme::IsEnabled(node) && !LayoutTheme::IsReadOnlyControl(node),
      YES, YES);
  return false;
}

bool ThemePainterMac::PaintMenuList(const Node* node,
                                    const Document&,
                                    const ComputedStyle& style,
                                    const PaintInfo& paint_info,
                                    const IntRect& r) {
  ScopedColorSchemeAppearance appearance(style.UsedColorScheme());
  layout_theme_.SetPopupButtonCellState(node, style, r);

  NSPopUpButtonCell* popup_button = layout_theme_.PopupButton();

  float zoom_level = style.EffectiveZoom();
  IntSize size = layout_theme_.PopupButtonSizes()[[popup_button controlSize]];
  size.SetHeight(size.Height() * zoom_level);
  size.SetWidth(r.Width());

  // Now inflate it to account for the shadow.
  IntRect inflated_rect = r;
  if (r.Width() >= layout_theme_.MinimumMenuListSize(style))
    inflated_rect = LayoutThemeMac::InflateRect(
        inflated_rect, size, layout_theme_.PopupButtonMargins(), zoom_level);

  LocalCurrentGraphicsContext local_context(
      paint_info.context,
      LayoutThemeMac::InflateRectForFocusRing(inflated_rect));

  if (zoom_level != 1.0f) {
    inflated_rect.SetWidth(inflated_rect.Width() / zoom_level);
    inflated_rect.SetHeight(inflated_rect.Height() / zoom_level);
    paint_info.context.Translate(inflated_rect.X(), inflated_rect.Y());
    paint_info.context.Scale(zoom_level, zoom_level);
    paint_info.context.Translate(-inflated_rect.X(), -inflated_rect.Y());
  }

  NSView* view = layout_theme_.DocumentView();
  [popup_button drawWithFrame:CGRect(inflated_rect) inView:view];
  if (LayoutTheme::IsFocused(node) && style.OutlineStyleIsAuto())
    [popup_button cr_drawFocusRingWithFrame:CGRect(inflated_rect) inView:view];
  [popup_button setControlView:nil];

  return false;
}

bool ThemePainterMac::PaintProgressBar(const LayoutObject& layout_object,
                                       const PaintInfo& paint_info,
                                       const IntRect& rect) {
  if (!layout_object.IsProgress())
    return true;

  ScopedColorSchemeAppearance appearance(
      layout_object.StyleRef().UsedColorScheme());
  const LayoutProgress& layout_progress = ToLayoutProgress(layout_object);
  HIThemeTrackDrawInfo track_info;
  track_info.version = 0;
  NSControlSize control_size =
      layout_theme_.ControlSizeForFont(layout_object.StyleRef());
  if (control_size == NSRegularControlSize)
    track_info.kind = layout_progress.GetPosition() < 0
                          ? kThemeLargeIndeterminateBar
                          : kThemeLargeProgressBar;
  else
    track_info.kind = layout_progress.GetPosition() < 0
                          ? kThemeMediumIndeterminateBar
                          : kThemeMediumProgressBar;

  track_info.bounds = CGRect(IntRect(IntPoint(), rect.Size()));
  track_info.min = 0;
  track_info.max = std::numeric_limits<SInt32>::max();
  track_info.value =
      lround(layout_progress.GetPosition() * nextafter(track_info.max, 0));
  track_info.trackInfo.progress.phase =
      lround(layout_progress.AnimationProgress() *
             nextafter(LayoutThemeMac::kProgressAnimationNumFrames, 0));
  track_info.attributes = kThemeTrackHorizontal;
  track_info.enableState = LayoutTheme::IsActive(layout_object.GetNode())
                               ? kThemeTrackActive
                               : kThemeTrackInactive;
  track_info.reserved = 0;
  track_info.filler1 = 0;

  GraphicsContextStateSaver state_saver(paint_info.context);
  paint_info.context.Translate(rect.X(), rect.Y());

  if (!layout_progress.StyleRef().IsLeftToRightDirection()) {
    paint_info.context.Translate(rect.Width(), 0);
    paint_info.context.Scale(-1, 1);
  }

  IntRect clip_rect = IntRect(IntPoint(), rect.Size());
  LocalCurrentGraphicsContext local_context(paint_info.context, clip_rect);

  CGContextRef cg_context = local_context.CgContext();
  HIThemeDrawTrack(&track_info, 0, cg_context, kHIThemeOrientationNormal);

  return false;
}

bool ThemePainterMac::PaintMenuListButton(const Node* node,
                                          const Document&,
                                          const ComputedStyle& style,
                                          const PaintInfo& paint_info,
                                          const IntRect& r) {
  ScopedColorSchemeAppearance appearance(style.UsedColorScheme());
  IntRect bounds =
      IntRect(r.X() + style.BorderLeftWidth(), r.Y() + style.BorderTopWidth(),
              r.Width() - style.BorderLeftWidth() - style.BorderRightWidth(),
              r.Height() - style.BorderTopWidth() - style.BorderBottomWidth());
  // Since we actually know the size of the control here, we restrict the font
  // scale to make sure the arrows will fit vertically in the bounds
  float font_scale = std::min(
      style.FontSize() / LayoutThemeMac::kBaseFontSize,
      bounds.Height() / (LayoutThemeMac::kMenuListBaseArrowHeight * 2 +
                         LayoutThemeMac::kMenuListBaseSpaceBetweenArrows));
  float center_y = bounds.Y() + bounds.Height() / 2.0f;
  float arrow_height = LayoutThemeMac::kMenuListBaseArrowHeight * font_scale;
  float arrow_width = LayoutThemeMac::kMenuListBaseArrowWidth * font_scale;
  float space_between_arrows =
      LayoutThemeMac::kMenuListBaseSpaceBetweenArrows * font_scale;
  float scaled_padding_end =
      LayoutThemeMac::kMenuListArrowPaddingEnd * style.EffectiveZoom();
  float left_edge;
  if (style.Direction() == TextDirection::kLtr) {
    left_edge = bounds.MaxX() - scaled_padding_end - arrow_width;
  } else {
    left_edge = bounds.X() + scaled_padding_end;
  }
  if (bounds.Width() < arrow_width + scaled_padding_end)
    return false;

  Color color = style.VisitedDependentColor(GetCSSPropertyColor());
  PaintFlags flags = paint_info.context.FillFlags();
  flags.setAntiAlias(true);
  flags.setColor(color.Rgb());

  SkPath arrow1;
  arrow1.moveTo(left_edge, center_y - space_between_arrows / 2.0f);
  arrow1.lineTo(left_edge + arrow_width,
                center_y - space_between_arrows / 2.0f);
  arrow1.lineTo(left_edge + arrow_width / 2.0f,
                center_y - space_between_arrows / 2.0f - arrow_height);

  // Draw the top arrow.
  paint_info.context.DrawPath(arrow1, flags);

  SkPath arrow2;
  arrow2.moveTo(left_edge, center_y + space_between_arrows / 2.0f);
  arrow2.lineTo(left_edge + arrow_width,
                center_y + space_between_arrows / 2.0f);
  arrow2.lineTo(left_edge + arrow_width / 2.0f,
                center_y + space_between_arrows / 2.0f + arrow_height);

  // Draw the bottom arrow.
  paint_info.context.DrawPath(arrow2, flags);
  return false;
}

bool ThemePainterMac::PaintSliderTrack(const LayoutObject& o,
                                       const PaintInfo& paint_info,
                                       const IntRect& r) {
  ScopedColorSchemeAppearance appearance(o.StyleRef().UsedColorScheme());
  PaintSliderTicks(o, paint_info, r);

  float zoom_level = o.StyleRef().EffectiveZoom();
  FloatRect unzoomed_rect(r);

  if (o.StyleRef().EffectiveAppearance() == kSliderHorizontalPart ||
      o.StyleRef().EffectiveAppearance() == kMediaSliderPart) {
    unzoomed_rect.SetY(
        ceilf(unzoomed_rect.Y() + unzoomed_rect.Height() / 2 -
              zoom_level * LayoutThemeMac::kSliderTrackWidth / 2));
    unzoomed_rect.SetHeight(zoom_level * LayoutThemeMac::kSliderTrackWidth);
  } else if (o.StyleRef().EffectiveAppearance() == kSliderVerticalPart) {
    unzoomed_rect.SetX(
        ceilf(unzoomed_rect.X() + unzoomed_rect.Width() / 2 -
              zoom_level * LayoutThemeMac::kSliderTrackWidth / 2));
    unzoomed_rect.SetWidth(zoom_level * LayoutThemeMac::kSliderTrackWidth);
  }

  if (zoom_level != 1) {
    unzoomed_rect.SetWidth(unzoomed_rect.Width() / zoom_level);
    unzoomed_rect.SetHeight(unzoomed_rect.Height() / zoom_level);
  }

  GraphicsContextStateSaver state_saver(paint_info.context);
  if (zoom_level != 1) {
    paint_info.context.Translate(unzoomed_rect.X(), unzoomed_rect.Y());
    paint_info.context.Scale(zoom_level, zoom_level);
    paint_info.context.Translate(-unzoomed_rect.X(), -unzoomed_rect.Y());
  }

  Color fill_color(205, 205, 205);
  Color border_gradient_top_color(109, 109, 109);
  Color border_gradient_bottom_color(181, 181, 181);
  Color shadow_color(0, 0, 0, 118);

  if (!LayoutTheme::IsEnabled(o.GetNode())) {
    Color tint_color(255, 255, 255, 128);
    fill_color = fill_color.Blend(tint_color);
    border_gradient_top_color = border_gradient_top_color.Blend(tint_color);
    border_gradient_bottom_color =
        border_gradient_bottom_color.Blend(tint_color);
    shadow_color = shadow_color.Blend(tint_color);
  }

  Color tint_color;
  if (!LayoutTheme::IsEnabled(o.GetNode()))
    tint_color = Color(255, 255, 255, 128);

  bool is_vertical_slider =
      o.StyleRef().EffectiveAppearance() == kSliderVerticalPart;

  float fill_radius_size = (LayoutThemeMac::kSliderTrackWidth -
                            LayoutThemeMac::kSliderTrackBorderWidth) /
                           2;
  FloatSize fill_radius(fill_radius_size, fill_radius_size);
  FloatRect fill_bounds(EnclosedIntRect(unzoomed_rect));
  FloatRoundedRect fill_rect(fill_bounds, fill_radius, fill_radius, fill_radius,
                             fill_radius);
  paint_info.context.FillRoundedRect(fill_rect, fill_color);

  FloatSize shadow_offset(is_vertical_slider ? 1 : 0,
                          is_vertical_slider ? 0 : 1);
  float shadow_blur = 3;
  float shadow_spread = 0;
  paint_info.context.Save();
  paint_info.context.DrawInnerShadow(fill_rect, shadow_color, shadow_offset,
                                     shadow_blur, shadow_spread);
  paint_info.context.Restore();

  scoped_refptr<Gradient> border_gradient =
      Gradient::CreateLinear(fill_bounds.MinXMinYCorner(),
                             is_vertical_slider ? fill_bounds.MaxXMinYCorner()
                                                : fill_bounds.MinXMaxYCorner());
  border_gradient->AddColorStop(0.0, border_gradient_top_color);
  border_gradient->AddColorStop(1.0, border_gradient_bottom_color);

  FloatRect border_rect(unzoomed_rect);
  border_rect.Inflate(-LayoutThemeMac::kSliderTrackBorderWidth / 2.0);
  float border_radius_size =
      (is_vertical_slider ? border_rect.Width() : border_rect.Height()) / 2;
  FloatSize border_radius(border_radius_size, border_radius_size);
  FloatRoundedRect border_r_rect(border_rect, border_radius, border_radius,
                                 border_radius, border_radius);
  paint_info.context.SetStrokeThickness(
      LayoutThemeMac::kSliderTrackBorderWidth);
  PaintFlags border_flags(paint_info.context.StrokeFlags());
  border_gradient->ApplyToFlags(border_flags, SkMatrix::I());
  paint_info.context.DrawRRect(border_r_rect, border_flags);

  return false;
}

bool ThemePainterMac::PaintSliderThumb(const Node* node,
                                       const ComputedStyle& style,
                                       const PaintInfo& paint_info,
                                       const IntRect& r) {
  ScopedColorSchemeAppearance appearance(style.UsedColorScheme());
  GraphicsContextStateSaver state_saver(paint_info.context);
  float zoom_level = style.EffectiveZoom();

  FloatRect unzoomed_rect(r.X(), r.Y(), LayoutThemeMac::kSliderThumbWidth,
                          LayoutThemeMac::kSliderThumbHeight);
  if (zoom_level != 1.0f) {
    paint_info.context.Translate(unzoomed_rect.X(), unzoomed_rect.Y());
    paint_info.context.Scale(zoom_level, zoom_level);
    paint_info.context.Translate(-unzoomed_rect.X(), -unzoomed_rect.Y());
  }

  Color fill_gradient_top_color(250, 250, 250);
  Color fill_gradient_upper_middle_color(244, 244, 244);
  Color fill_gradient_lower_middle_color(236, 236, 236);
  Color fill_gradient_bottom_color(238, 238, 238);
  Color border_gradient_top_color(151, 151, 151);
  Color border_gradient_bottom_color(128, 128, 128);
  Color shadow_color(0, 0, 0, 36);

  if (!LayoutTheme::IsEnabled(node)) {
    Color tint_color(255, 255, 255, 128);
    fill_gradient_top_color = fill_gradient_top_color.Blend(tint_color);
    fill_gradient_upper_middle_color =
        fill_gradient_upper_middle_color.Blend(tint_color);
    fill_gradient_lower_middle_color =
        fill_gradient_lower_middle_color.Blend(tint_color);
    fill_gradient_bottom_color = fill_gradient_bottom_color.Blend(tint_color);
    border_gradient_top_color = border_gradient_top_color.Blend(tint_color);
    border_gradient_bottom_color =
        border_gradient_bottom_color.Blend(tint_color);
    shadow_color = shadow_color.Blend(tint_color);
  } else if (LayoutTheme::IsPressed(node)) {
    Color tint_color(0, 0, 0, 32);
    fill_gradient_top_color = fill_gradient_top_color.Blend(tint_color);
    fill_gradient_upper_middle_color =
        fill_gradient_upper_middle_color.Blend(tint_color);
    fill_gradient_lower_middle_color =
        fill_gradient_lower_middle_color.Blend(tint_color);
    fill_gradient_bottom_color = fill_gradient_bottom_color.Blend(tint_color);
    border_gradient_top_color = border_gradient_top_color.Blend(tint_color);
    border_gradient_bottom_color =
        border_gradient_bottom_color.Blend(tint_color);
    shadow_color = shadow_color.Blend(tint_color);
  }

  FloatRect border_bounds = unzoomed_rect;
  border_bounds.Inflate(LayoutThemeMac::kSliderThumbBorderWidth / 2.0);

  border_bounds.Inflate(-LayoutThemeMac::kSliderThumbBorderWidth);
  FloatSize shadow_offset(0, 1);
  paint_info.context.SetShadow(
      shadow_offset, LayoutThemeMac::kSliderThumbShadowBlur, shadow_color);
  paint_info.context.SetFillColor(Color::kBlack);
  paint_info.context.FillEllipse(border_bounds);
  paint_info.context.SetDrawLooper(nullptr);

  FloatRect fill_bounds = FloatRect(EnclosedIntRect(unzoomed_rect));
  scoped_refptr<Gradient> fill_gradient = Gradient::CreateLinear(
      fill_bounds.MinXMinYCorner(), fill_bounds.MinXMaxYCorner());
  fill_gradient->AddColorStop(0.0, fill_gradient_top_color);
  fill_gradient->AddColorStop(0.52, fill_gradient_upper_middle_color);
  fill_gradient->AddColorStop(0.52, fill_gradient_lower_middle_color);
  fill_gradient->AddColorStop(1.0, fill_gradient_bottom_color);
  PaintFlags fill_flags(paint_info.context.FillFlags());
  fill_gradient->ApplyToFlags(fill_flags, SkMatrix::I());
  paint_info.context.DrawOval(border_bounds, fill_flags);

  scoped_refptr<Gradient> border_gradient = Gradient::CreateLinear(
      fill_bounds.MinXMinYCorner(), fill_bounds.MinXMaxYCorner());
  border_gradient->AddColorStop(0.0, border_gradient_top_color);
  border_gradient->AddColorStop(1.0, border_gradient_bottom_color);
  paint_info.context.SetStrokeThickness(
      LayoutThemeMac::kSliderThumbBorderWidth);
  PaintFlags border_flags(paint_info.context.StrokeFlags());
  border_gradient->ApplyToFlags(border_flags, SkMatrix::I());
  paint_info.context.DrawOval(border_bounds, border_flags);

  if (LayoutTheme::IsFocused(node)) {
    Path border_path;
    border_path.AddEllipse(border_bounds);
    paint_info.context.DrawFocusRing(border_path, 5, -2,
                                     layout_theme_.FocusRingColor());
  }

  return false;
}

// We don't use controlSizeForFont() for search field decorations because it
// needs to fit into the search field. The font size will already be modified by
// setFontFromControlSize() called on the search field.
static NSControlSize SearchFieldControlSizeForFont(const ComputedStyle& style) {
  int font_size = style.FontSize();
  if (font_size >= 13)
    return NSRegularControlSize;
  if (font_size >= 11)
    return NSSmallControlSize;
  return NSMiniControlSize;
}

bool ThemePainterMac::PaintSearchField(const Node* node,
                                       const ComputedStyle& style,
                                       const PaintInfo& paint_info,
                                       const IntRect& r) {
  ScopedColorSchemeAppearance appearance(style.UsedColorScheme());
  LocalCurrentGraphicsContext local_context(paint_info.context, r);

  NSSearchFieldCell* search = layout_theme_.Search();
  layout_theme_.SetSearchCellState(node, style, r);
  [search setControlSize:SearchFieldControlSizeForFont(style)];

  GraphicsContextStateSaver state_saver(paint_info.context);

  float zoom_level = style.EffectiveZoom();

  IntRect unzoomed_rect = r;
  if (zoom_level != 1.0f) {
    unzoomed_rect.SetWidth(unzoomed_rect.Width() / zoom_level);
    unzoomed_rect.SetHeight(unzoomed_rect.Height() / zoom_level);
    paint_info.context.Translate(unzoomed_rect.X(), unzoomed_rect.Y());
    paint_info.context.Scale(zoom_level, zoom_level);
    paint_info.context.Translate(-unzoomed_rect.X(), -unzoomed_rect.Y());
  }

  // Set the search button to nil before drawing. Then reset it so we can
  // draw it later.
  [search setSearchButtonCell:nil];

  [search drawWithFrame:NSRect(unzoomed_rect)
                 inView:layout_theme_.DocumentView()];

  [search setControlView:nil];
  [search resetSearchButtonCell];

  return false;
}

bool ThemePainterMac::PaintSearchFieldCancelButton(
    const LayoutObject& cancel_button,
    const PaintInfo& paint_info,
    const IntRect& r) {
  if (!cancel_button.GetNode())
    return false;

  ScopedColorSchemeAppearance appearance(
      cancel_button.StyleRef().UsedColorScheme());
  GraphicsContextStateSaver state_saver(paint_info.context);

  float zoom_level = cancel_button.StyleRef().EffectiveZoom();
  FloatRect unzoomed_rect(r);
  if (zoom_level != 1.0f) {
    unzoomed_rect.SetWidth(unzoomed_rect.Width() / zoom_level);
    unzoomed_rect.SetHeight(unzoomed_rect.Height() / zoom_level);
    paint_info.context.Translate(unzoomed_rect.X(), unzoomed_rect.Y());
    paint_info.context.Scale(zoom_level, zoom_level);
    paint_info.context.Translate(-unzoomed_rect.X(), -unzoomed_rect.Y());
  }

  Color fill_color(200, 200, 200);

  if (LayoutTheme::IsPressed(cancel_button.GetNode())) {
    Color tint_color(0, 0, 0, 32);
    fill_color = fill_color.Blend(tint_color);
  }

  float center_x = unzoomed_rect.X() + unzoomed_rect.Width() / 2;
  float center_y = unzoomed_rect.Y() + unzoomed_rect.Height() / 2;
  // The line width is 3px on a regular sized, high DPI NSCancelButtonCell
  // (which is 28px wide).
  float line_width = unzoomed_rect.Width() * 3 / 28;
  // The line length is 16px on a regular sized, high DPI NSCancelButtonCell.
  float line_length = unzoomed_rect.Width() * 16 / 28;

  Path x_path;
  FloatSize line_rect_radius(line_width / 2, line_width / 2);
  x_path.AddRoundedRect(
      FloatRect(-line_length / 2, -line_width / 2, line_length, line_width),
      line_rect_radius, line_rect_radius, line_rect_radius, line_rect_radius);
  x_path.AddRoundedRect(
      FloatRect(-line_width / 2, -line_length / 2, line_width, line_length),
      line_rect_radius, line_rect_radius, line_rect_radius, line_rect_radius);

  paint_info.context.Translate(center_x, center_y);
  paint_info.context.Rotate(deg2rad(45.0));
  paint_info.context.ClipOut(x_path);
  paint_info.context.Rotate(deg2rad(-45.0));
  paint_info.context.Translate(-center_x, -center_y);

  paint_info.context.SetFillColor(fill_color);
  paint_info.context.FillEllipse(unzoomed_rect);

  return false;
}

// FIXME: Share more code with radio buttons.
bool ThemePainterMac::PaintCheckbox(const Node* node,
                                    const Document& document,
                                    const ComputedStyle& style,
                                    const PaintInfo& paint_info,
                                    const IntRect& zoomed_rect) {
  BEGIN_BLOCK_OBJC_EXCEPTIONS

  ScopedColorSchemeAppearance appearance(style.UsedColorScheme());
  ControlStates states = LayoutTheme::ControlStatesForNode(node, style);
  float zoom_factor = style.EffectiveZoom();

  // Determine the width and height needed for the control and prepare the cell
  // for painting.
  NSButtonCell* checkbox_cell =
      LayoutThemeMac::Checkbox(states, zoomed_rect, zoom_factor);
  GraphicsContextStateSaver state_saver(paint_info.context);

  NSControlSize control_size = [checkbox_cell controlSize];
  IntSize zoomed_size = LayoutThemeMac::CheckboxSizes()[control_size];
  zoomed_size.SetWidth(zoomed_size.Width() * zoom_factor);
  zoomed_size.SetHeight(zoomed_size.Height() * zoom_factor);
  IntRect inflated_rect = LayoutThemeMac::InflateRect(
      zoomed_rect, zoomed_size, LayoutThemeMac::CheckboxMargins(control_size),
      zoom_factor);

  if (zoom_factor != 1.0f) {
    inflated_rect.SetWidth(inflated_rect.Width() / zoom_factor);
    inflated_rect.SetHeight(inflated_rect.Height() / zoom_factor);
    paint_info.context.Translate(inflated_rect.X(), inflated_rect.Y());
    paint_info.context.Scale(zoom_factor, zoom_factor);
    paint_info.context.Translate(-inflated_rect.X(), -inflated_rect.Y());
  }

  LocalCurrentGraphicsContext local_context(
      paint_info.context,
      LayoutThemeMac::InflateRectForFocusRing(inflated_rect));
  NSView* view = LayoutThemeMac::EnsuredView(document.View()->Size());
  [checkbox_cell drawWithFrame:NSRect(inflated_rect) inView:view];
  if (states & kFocusControlState)
    [checkbox_cell cr_drawFocusRingWithFrame:NSRect(inflated_rect) inView:view];
  [checkbox_cell setControlView:nil];

  END_BLOCK_OBJC_EXCEPTIONS
  return false;
}

bool ThemePainterMac::PaintRadio(const Node* node,
                                 const Document& document,
                                 const ComputedStyle& style,
                                 const PaintInfo& paint_info,
                                 const IntRect& zoomed_rect) {
  ScopedColorSchemeAppearance appearance(style.UsedColorScheme());
  ControlStates states = LayoutTheme::ControlStatesForNode(node, style);
  float zoom_factor = style.EffectiveZoom();

  // Determine the width and height needed for the control and prepare the cell
  // for painting.
  NSButtonCell* radio_cell =
      LayoutThemeMac::Radio(states, zoomed_rect, zoom_factor);
  GraphicsContextStateSaver state_saver(paint_info.context);

  NSControlSize control_size = [radio_cell controlSize];
  IntSize zoomed_size = LayoutThemeMac::RadioSizes()[control_size];
  zoomed_size.SetWidth(zoomed_size.Width() * zoom_factor);
  zoomed_size.SetHeight(zoomed_size.Height() * zoom_factor);
  IntRect inflated_rect = LayoutThemeMac::InflateRect(
      zoomed_rect, zoomed_size, LayoutThemeMac::RadioMargins(control_size),
      zoom_factor);

  if (zoom_factor != 1.0f) {
    inflated_rect.SetWidth(inflated_rect.Width() / zoom_factor);
    inflated_rect.SetHeight(inflated_rect.Height() / zoom_factor);
    paint_info.context.Translate(inflated_rect.X(), inflated_rect.Y());
    paint_info.context.Scale(zoom_factor, zoom_factor);
    paint_info.context.Translate(-inflated_rect.X(), -inflated_rect.Y());
  }

  LocalCurrentGraphicsContext local_context(
      paint_info.context,
      LayoutThemeMac::InflateRectForFocusRing(inflated_rect));
  BEGIN_BLOCK_OBJC_EXCEPTIONS
  NSView* view = LayoutThemeMac::EnsuredView(document.View()->Size());
  [radio_cell drawWithFrame:NSRect(inflated_rect) inView:view];
  if (states & kFocusControlState)
    [radio_cell cr_drawFocusRingWithFrame:NSRect(inflated_rect) inView:view];
  [radio_cell setControlView:nil];
  END_BLOCK_OBJC_EXCEPTIONS

  return false;
}

bool ThemePainterMac::PaintButton(const Node* node,
                                  const Document& document,
                                  const ComputedStyle& style,
                                  const PaintInfo& paint_info,
                                  const IntRect& zoomed_rect) {
  BEGIN_BLOCK_OBJC_EXCEPTIONS

  ScopedColorSchemeAppearance appearance(style.UsedColorScheme());
  ControlStates states = LayoutTheme::ControlStatesForNode(node, style);
  float zoom_factor = style.EffectiveZoom();

  // Determine the width and height needed for the control and prepare the cell
  // for painting.
  NSButtonCell* button_cell = LayoutThemeMac::Button(
      style.EffectiveAppearance(), states, zoomed_rect, zoom_factor);
  GraphicsContextStateSaver state_saver(paint_info.context);

  NSControlSize control_size = [button_cell controlSize];
  IntSize zoomed_size = LayoutThemeMac::ButtonSizes()[control_size];
  // Buttons don't ever constrain width, so the zoomed width can just be
  // honored.
  zoomed_size.SetWidth(zoomed_rect.Width());
  zoomed_size.SetHeight(zoomed_size.Height() * zoom_factor);
  IntRect inflated_rect = zoomed_rect;
  if ([button_cell bezelStyle] == NSRoundedBezelStyle) {
    // Center the button within the available space.
    if (inflated_rect.Height() > zoomed_size.Height()) {
      inflated_rect.SetY(inflated_rect.Y() +
                         (inflated_rect.Height() - zoomed_size.Height()) / 2);
      inflated_rect.SetHeight(zoomed_size.Height());
    }

    // Now inflate it to account for the shadow.
    inflated_rect = LayoutThemeMac::InflateRect(
        inflated_rect, zoomed_size, LayoutThemeMac::ButtonMargins(control_size),
        zoom_factor);

    if (zoom_factor != 1.0f) {
      inflated_rect.SetWidth(inflated_rect.Width() / zoom_factor);
      inflated_rect.SetHeight(inflated_rect.Height() / zoom_factor);
      paint_info.context.Translate(inflated_rect.X(), inflated_rect.Y());
      paint_info.context.Scale(zoom_factor, zoom_factor);
      paint_info.context.Translate(-inflated_rect.X(), -inflated_rect.Y());
    }
  }

  LocalCurrentGraphicsContext local_context(
      paint_info.context,
      LayoutThemeMac::InflateRectForFocusRing(inflated_rect));
  NSView* view = LayoutThemeMac::EnsuredView(document.View()->Size());

  [button_cell drawWithFrame:NSRect(inflated_rect) inView:view];
  if (states & kFocusControlState)
    [button_cell cr_drawFocusRingWithFrame:NSRect(inflated_rect) inView:view];
  [button_cell setControlView:nil];

  END_BLOCK_OBJC_EXCEPTIONS
  return false;
}

static ThemeDrawState ConvertControlStatesToThemeDrawState(
    ThemeButtonKind kind,
    ControlStates states) {
  if (states & kReadOnlyControlState)
    return kThemeStateUnavailableInactive;
  if (!(states & kEnabledControlState))
    return kThemeStateUnavailableInactive;

  // Do not process PressedState if !EnabledControlState or
  // ReadOnlyControlState.
  if (states & kPressedControlState) {
    if (kind == kThemeIncDecButton || kind == kThemeIncDecButtonSmall ||
        kind == kThemeIncDecButtonMini)
      return states & kSpinUpControlState ? kThemeStatePressedUp
                                          : kThemeStatePressedDown;
    return kThemeStatePressed;
  }
  return kThemeStateActive;
}

bool ThemePainterMac::PaintInnerSpinButton(const Node* node,
                                           const ComputedStyle& style,
                                           const PaintInfo& paint_info,
                                           const IntRect& zoomed_rect) {
  ScopedColorSchemeAppearance appearance(style.UsedColorScheme());
  ControlStates states = LayoutTheme::ControlStatesForNode(node, style);
  float zoom_factor = style.EffectiveZoom();

  // We don't use NSStepperCell because there are no ways to draw an
  // NSStepperCell with the up button highlighted.

  HIThemeButtonDrawInfo draw_info;
  draw_info.version = 0;
  draw_info.state =
      ConvertControlStatesToThemeDrawState(kThemeIncDecButton, states);
  draw_info.adornment = kThemeAdornmentDefault;
  ControlSize control_size = LayoutThemeMac::ControlSizeFromPixelSize(
      LayoutThemeMac::StepperSizes(), zoomed_rect.Size(), zoom_factor);
  if (control_size == NSSmallControlSize)
    draw_info.kind = kThemeIncDecButtonSmall;
  else if (control_size == NSMiniControlSize)
    draw_info.kind = kThemeIncDecButtonMini;
  else
    draw_info.kind = kThemeIncDecButton;

  IntRect rect(zoomed_rect);
  GraphicsContextStateSaver state_saver(paint_info.context);
  if (zoom_factor != 1.0f) {
    rect.SetWidth(rect.Width() / zoom_factor);
    rect.SetHeight(rect.Height() / zoom_factor);
    paint_info.context.Translate(rect.X(), rect.Y());
    paint_info.context.Scale(zoom_factor, zoom_factor);
    paint_info.context.Translate(-rect.X(), -rect.Y());
  }
  CGRect bounds(rect);
  CGRect background_bounds;
  HIThemeGetButtonBackgroundBounds(&bounds, &draw_info, &background_bounds);
  // Center the stepper rectangle in the specified area.
  background_bounds.origin.x =
      bounds.origin.x + (bounds.size.width - background_bounds.size.width) / 2;
  if (background_bounds.size.height < bounds.size.height) {
    int height_diff =
        clampTo<int>(bounds.size.height - background_bounds.size.height);
    background_bounds.origin.y = bounds.origin.y + (height_diff / 2) + 1;
  }

  LocalCurrentGraphicsContext local_context(paint_info.context, rect);
  HIThemeDrawButton(&background_bounds, &draw_info, local_context.CgContext(),
                    kHIThemeOrientationNormal, 0);
  return false;
}

}  // namespace blink
