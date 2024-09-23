// Copyright (C) 2013 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "third_party/blink/renderer/platform/graphics/styled_stroke_data.h"

#include <memory>
#include <optional>

#include "third_party/blink/renderer/platform/graphics/stroke_data.h"

namespace blink {

namespace {

float SelectBestDashGap(float stroke_length,
                        float dash_length,
                        float gap_length,
                        bool closed_path) {
  // Determine what number of dashes gives the minimum deviation from
  // gap_length between dashes. Set the gap to that width.
  float available_length =
      closed_path ? stroke_length : stroke_length + gap_length;
  float min_num_dashes = floorf(available_length / (dash_length + gap_length));
  float max_num_dashes = min_num_dashes + 1;
  float min_num_gaps = closed_path ? min_num_dashes : min_num_dashes - 1;
  float max_num_gaps = closed_path ? max_num_dashes : max_num_dashes - 1;
  float min_gap = (stroke_length - min_num_dashes * dash_length) / min_num_gaps;
  float max_gap = (stroke_length - max_num_dashes * dash_length) / max_num_gaps;
  return (max_gap <= 0) ||
                 (fabs(min_gap - gap_length) < fabs(max_gap - gap_length))
             ? min_gap
             : max_gap;
}

// The length of the dash relative to the line thickness for dashed stroking.
// A different dash length may be used when dashes are adjusted to better fit a
// given length path. Thin lines need longer dashes to avoid looking like dots
// when drawn.
float DashLengthRatio(float thickness) {
  return thickness >= 3 ? 2.0 : 3.0;
}

// The length of the gap between dashes relative to the line thickness for
// dashed stroking. A different gap may be used when dashes are adjusted to
// better fit a given length path. Thin lines need longer gaps to avoid looking
// like a continuous line when drawn.
float DashGapRatio(float thickness) {
  return thickness >= 3 ? 1.0 : 2.0;
}

struct DashDescription {
  SkScalar intervals[2];
  cc::PaintFlags::Cap cap = cc::PaintFlags::kDefault_Cap;
};

std::optional<DashDescription> DashEffectFromStrokeStyle(
    const StyledStrokeData& data,
    const StyledStrokeData::GeometryInfo& info) {
  const float dash_width =
      info.dash_thickness ? info.dash_thickness : data.Thickness();
  if (StyledStrokeData::StrokeIsDashed(dash_width, data.Style())) {
    float dash_length = dash_width;
    float gap_length = dash_length;
    if (data.Style() == kDashedStroke) {
      dash_length *= DashLengthRatio(dash_width);
      gap_length *= DashGapRatio(dash_width);
    }
    if (info.path_length <= dash_length * 2) {
      // No space for dashes
      return std::nullopt;
    }
    float two_dashes_with_gap_length = 2 * dash_length + gap_length;
    if (info.closed_path) {
      two_dashes_with_gap_length += gap_length;
    }
    if (info.path_length <= two_dashes_with_gap_length) {
      // Exactly 2 dashes proportionally sized
      float multiplier = info.path_length / two_dashes_with_gap_length;
      return DashDescription{
          {dash_length * multiplier, gap_length * multiplier},
          cc::PaintFlags::kDefault_Cap};
    }
    float gap = gap_length;
    if (data.Style() == kDashedStroke) {
      gap = SelectBestDashGap(info.path_length, dash_length, gap_length,
                              info.closed_path);
    }
    return DashDescription{{dash_length, gap}, cc::PaintFlags::kDefault_Cap};
  }
  if (data.Style() == kDottedStroke) {
    // Adjust the width to get equal dot spacing as much as possible.
    float per_dot_length = dash_width * 2;
    if (info.path_length < per_dot_length) {
      // Not enough space for 2 dots. Just draw 1 by giving a gap that is
      // bigger than the length.
      return DashDescription{{0, per_dot_length},
                             cc::PaintFlags::Cap::kRound_Cap};
    }
    // Epsilon ensures that we get a whole dot at the end of the line,
    // even if that dot is a little inside the true endpoint. Without it
    // we can drop the end dot due to rounding along the line.
    static const float kEpsilon = 1.0e-2f;
    float gap = SelectBestDashGap(info.path_length, dash_width, dash_width,
                                  info.closed_path);
    return DashDescription{{0, gap + dash_width - kEpsilon},
                           cc::PaintFlags::Cap::kRound_Cap};
  }
  return std::nullopt;
}

}  // namespace

void StyledStrokeData::SetupPaint(cc::PaintFlags* flags) const {
  SetupPaint(flags, {});
}

void StyledStrokeData::SetupPaint(cc::PaintFlags* flags,
                                  const GeometryInfo& info) const {
  flags->setStyle(cc::PaintFlags::kStroke_Style);
  flags->setStrokeWidth(SkFloatToScalar(thickness_));
  flags->setStrokeCap(cc::PaintFlags::kDefault_Cap);
  flags->setStrokeJoin(cc::PaintFlags::kDefault_Join);
  flags->setStrokeMiter(SkFloatToScalar(4));
  SetupPaintDashPathEffect(flags, info);
}

void StyledStrokeData::SetupPaintDashPathEffect(
    cc::PaintFlags* flags,
    const GeometryInfo& info) const {
  if (auto dash = DashEffectFromStrokeStyle(*this, info)) {
    flags->setPathEffect(cc::PathEffect::MakeDash(dash->intervals, 2, 0));
    flags->setStrokeCap(dash->cap);
  } else {
    flags->setPathEffect(nullptr);
  }
}

StrokeData StyledStrokeData::ConvertToStrokeData(
    const GeometryInfo& info) const {
  StrokeData stroke_data;
  stroke_data.SetThickness(thickness_);
  if (auto dash = DashEffectFromStrokeStyle(*this, info)) {
    stroke_data.SetDashEffect(cc::PathEffect::MakeDash(dash->intervals, 2, 0));
    stroke_data.SetLineCap(static_cast<LineCap>(dash->cap));
  }
  return stroke_data;
}

bool StyledStrokeData::StrokeIsDashed(float width, StrokeStyle style) {
  return style == kDashedStroke || (style == kDottedStroke && width <= 3);
}

}  // namespace blink
