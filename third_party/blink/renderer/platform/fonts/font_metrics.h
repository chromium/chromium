/*
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_METRICS_H_

#include "third_party/blink/renderer/platform/fonts/font_baseline.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

class SkFont;

namespace blink {

const unsigned kGDefaultUnitsPerEm = 1000;
class FontPlatformData;

class FontMetrics {
  DISALLOW_NEW();

 public:
  FontMetrics()
      : units_per_em_(kGDefaultUnitsPerEm),
        ascent_(0),
        descent_(0),
        line_gap_(0),
        line_spacing_(0),
        x_height_(0),
        zero_width_(0),
        underlinethickness_(0),
        underline_position_(0),
        ascent_int_(0),
        descent_int_(0),
        has_x_height_(false),
        has_zero_width_(false) {}

  unsigned UnitsPerEm() const { return units_per_em_; }
  void SetUnitsPerEm(unsigned units_per_em) { units_per_em_ = units_per_em; }

  float FloatAscent(FontBaseline baseline_type = kAlphabeticBaseline) const {
    if (baseline_type == kAlphabeticBaseline)
      return ascent_;
    return FloatHeight() / 2;
  }

  void SetAscent(float ascent) {
    ascent_ = ascent;
    ascent_int_ = static_cast<int>(lroundf(ascent));
  }

  float FloatDescent(FontBaseline baseline_type = kAlphabeticBaseline) const {
    if (baseline_type == kAlphabeticBaseline)
      return descent_;
    return FloatHeight() / 2;
  }

  void SetDescent(float descent) {
    descent_ = descent;
    descent_int_ = static_cast<int>(lroundf(descent));
  }

  float FloatHeight(FontBaseline baseline_type = kAlphabeticBaseline) const {
    return FloatAscent() + FloatDescent();
  }

  float FloatLineGap() const { return line_gap_; }
  void SetLineGap(float line_gap) { line_gap_ = line_gap; }

  float FloatLineSpacing() const { return line_spacing_; }
  void SetLineSpacing(float line_spacing) { line_spacing_ = line_spacing; }

  float XHeight() const { return x_height_; }
  void SetXHeight(float x_height) {
    x_height_ = x_height;
    has_x_height_ = true;
  }

  bool HasXHeight() const { return has_x_height_ && x_height_ > 0; }
  void SetHasXHeight(bool has_x_height) { has_x_height_ = has_x_height; }

  // Integer variants of certain metrics, used for HTML rendering.
  int Ascent(FontBaseline baseline_type = kAlphabeticBaseline) const {
    if (baseline_type == kAlphabeticBaseline)
      return ascent_int_;
    return Height() - Height() / 2;
  }

  int Descent(FontBaseline baseline_type = kAlphabeticBaseline) const {
    if (baseline_type == kAlphabeticBaseline)
      return descent_int_;
    return Height() / 2;
  }

  int Height(FontBaseline baseline_type = kAlphabeticBaseline) const {
    return Ascent() + Descent();
  }

  int LineGap() const { return static_cast<int>(lroundf(line_gap_)); }
  int LineSpacing() const { return static_cast<int>(lroundf(line_spacing_)); }

  // LayoutUnit variants of certain metrics.
  // LayoutNG should use LayoutUnit for the block progression metrics.
  // TODO(kojii): Consider keeping converted values.
  LayoutUnit FixedAscent(
      FontBaseline baseline_type = kAlphabeticBaseline) const {
    return LayoutUnit::FromFloatRound(FloatAscent(baseline_type));
  }

  LayoutUnit FixedDescent(
      FontBaseline baseline_type = kAlphabeticBaseline) const {
    return LayoutUnit::FromFloatRound(FloatDescent(baseline_type));
  }

  LayoutUnit FixedLineSpacing() const {
    return LayoutUnit::FromFloatRound(line_spacing_);
  }

  bool HasIdenticalAscentDescentAndLineGap(const FontMetrics& other) const {
    return Ascent() == other.Ascent() && Descent() == other.Descent() &&
           LineGap() == other.LineGap();
  }

  float ZeroWidth() const { return zero_width_; }
  void SetZeroWidth(float zero_width) {
    zero_width_ = zero_width;
    has_zero_width_ = true;
  }

  bool HasZeroWidth() const { return has_zero_width_; }
  void SetHasZeroWidth(bool has_zero_width) {
    has_zero_width_ = has_zero_width;
  }

  float UnderlineThickness() const { return underlinethickness_; }
  void SetUnderlineThickness(float underline_thickness) {
    underlinethickness_ = underline_thickness;
  }

  float UnderlinePosition() const { return underline_position_; }
  void SetUnderlinePosition(float underline_position) {
    underline_position_ = underline_position;
  }

  // Unfortunately we still need to keep metrics adjustments for certain Mac
  // fonts, see crbug.com/445830. Also, a potentially better solution for the
  // subpixel_ascent_descent flag would be to move line layout to LayoutUnit
  // instead of int boundaries, see crbug.com/707807 and crbug.com/706298.
  static void AscentDescentWithHacks(
      float& ascent,
      float& descent,
      unsigned& visual_overflow_inflation_for_ascent,
      unsigned& visual_overflow_inflation_for_descent,
      const FontPlatformData&,
      const SkFont&,
      bool subpixel_ascent_descent = false);

 private:
  friend class SimpleFontData;

  void Reset() {
    units_per_em_ = kGDefaultUnitsPerEm;
    ascent_ = 0;
    descent_ = 0;
    ascent_int_ = 0;
    descent_int_ = 0;
    line_gap_ = 0;
    line_spacing_ = 0;
    x_height_ = 0;
    has_x_height_ = false;
    underlinethickness_ = 0;
    underline_position_ = 0;
  }

  unsigned units_per_em_;
  float ascent_;
  float descent_;
  float line_gap_;
  float line_spacing_;
  float x_height_;
  float zero_width_;
  float underlinethickness_;
  float underline_position_;
  int ascent_int_;
  int descent_int_;
  bool has_x_height_;
  bool has_zero_width_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_METRICS_H_
