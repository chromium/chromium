// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_text.h"

#include <iterator>
#include <string>

#include "base/check_op.h"

namespace chrome_pdf {
namespace {

// Makes a substring of `input` containing the characters in [start, end) with
// the `location` rectangle cut so that the returned `location` covers only the
// glyphs in the range.
//
// TODO(crbug.com/510015130): check `is_horizontal`: if false the rectangle
// would need to be split on the y-axis instead of the x-axis.
InkTextInfo MakeSubstrTextInfo(const InkTextInfo& input,
                               size_t start,
                               size_t end) {
  CHECK_LT(start, input.glyphs.size());
  CHECK_LE(end, input.glyphs.size());
  CHECK_EQ(input.glyphs.size(), input.glyph_positions.size());

  InkTextInfo split(input.font_id, {}, {}, input.location, input.is_horizontal);

  const float left = input.glyph_positions[start];
  const float right = end < input.glyph_positions.size()
                          ? input.glyph_positions[end]
                          : input.location.width();

  for (size_t i = start; i < end; ++i) {
    split.glyphs.push_back(input.glyphs[i]);
    split.glyph_positions.push_back(input.glyph_positions[i] - left);
  }
  split.location.set_x(left + split.location.x());
  split.location.set_width(right - left);
  return split;
}

// Because PDF text objects only support 1D glyph positioning, it is necessary
// to split runs of text that have 2D glyph offsets from Harfbuzz such that each
// PDF text object contains text all on the same y-axis position.
//
// So, take in an `input` InkTextInfo, which represents a run of text all on one
// line with the same typeface, plus the y-axis `offsets` for each glyph in
// that run. Then split the `input` into multiple InkTextInfo objects based on
// the y-axis position and apply the relevant `location` rectangle adjustments
// to incorporate the y-axis offsets.
//
// It's not necessary to call this if `offsets` is all zero, it will just push a
// copy of `input` in that case.
//
// TODO(crbug.com/510015130): check `is_horizontal`: if false `offsets` would be
// interpreted as x-axis offsets instead of y-axis. The documentation above
// needs to be updated too because the axes will flip.
std::vector<InkTextInfo> Split2DOffsets(const InkTextInfo& input,
                                        const std::vector<float>& offsets) {
  CHECK(!offsets.empty());
  CHECK_EQ(offsets.size(), input.glyphs.size());
  CHECK_EQ(offsets.size(), input.glyph_positions.size());

  std::vector<InkTextInfo> results;
  size_t run_start = 0;
  for (size_t i = 1; i <= offsets.size(); ++i) {
    bool is_boundary = i == offsets.size() || offsets[i - 1] != offsets[i];
    if (!is_boundary) {
      continue;
    }
    InkTextInfo split = MakeSubstrTextInfo(input, run_start, i);
    split.location.set_y(offsets[i - 1] + split.location.y());
    results.push_back(std::move(split));
    run_start = i;
  }
  return results;
}

// The PDFium API to set glyph positions requires that the first glyph position
// is always 0, so the first glyph position must be specified entirely in the
// `location` rectangle.
//
// This function normalizes the InkTextInfo to move any non-zero first glyph
// position into the `location` rectangle.
//
// TODO(crbug.com/510015130): check `is_horizontal`: if false `glyph_positions`
// would be interpreted as y-axis offsets instead of x-axis.
void MaybeCorrectNonZeroFirstOffset(InkTextInfo& input) {
  CHECK(!input.glyph_positions.empty());
  const float first_position = input.glyph_positions.front();
  if (first_position == 0.0f) {
    return;
  }
  input.location.set_x(first_position + input.location.x());
  for (float& position : input.glyph_positions) {
    position -= first_position;
  }
  // Note: The first position is now nearly guaranteed to be 0.0f or -0.0f. It's
  // possible if there was a Infinity or NaN to get a different answer but now
  // it's relatively safe to assume the first value is 0 and skip it in
  // FPDFText_SetPositions() calls.
}

}  // namespace

InkTextBoxAttributes::InkTextBoxAttributes(gfx::RectF rect,
                                           SkColor color,
                                           float css_font_size,
                                           TextTypeface typeface,
                                           TextAlignment alignment,
                                           int orientation,
                                           bool is_bold,
                                           bool is_italic,
                                           const std::string& text)
    : rect(rect),
      color(color),
      css_font_size(css_font_size),
      typeface(typeface),
      alignment(alignment),
      orientation(orientation),
      is_bold(is_bold),
      is_italic(is_italic),
      text(text) {}
InkTextBoxAttributes::~InkTextBoxAttributes() = default;

InkTextInfo::InkTextInfo(FontId font_id,
                         std::vector<uint32_t> glyphs,
                         std::vector<float> glyph_positions,
                         gfx::RectF location,
                         bool is_horizontal)
    : font_id(font_id),
      glyphs(glyphs),
      glyph_positions(glyph_positions),
      location(location),
      is_horizontal(is_horizontal) {}
InkTextInfo::InkTextInfo(InkTextInfo&&) noexcept = default;
InkTextInfo& InkTextInfo::operator=(InkTextInfo&&) noexcept = default;
InkTextInfo::~InkTextInfo() = default;

std::vector<InkTextInfo> InkTextInfo::SplitTypefaceRuns(
    const std::vector<pdf::mojom::InkTextRunPtr>& text_runs,
    float effective_zoom) {
  std::vector<InkTextInfo> results;
  for (const pdf::mojom::InkTextRunPtr& text_run : text_runs) {
    float left_edge = text_run->location.x();
    float prev_right_edge_advance = 0;
    for (size_t i = 0; i < text_run->typeface_runs.size(); ++i) {
      const pdf::mojom::InkTypefaceRunPtr& typeface_run =
          text_run->typeface_runs[i];
      // TODO(crbug.com/510015130): handle vertical text.
      CHECK(typeface_run->is_horizontal);

      const float right_edge_advance =
          i + 1 < text_run->typeface_runs.size() &&
                  !text_run->typeface_runs[i + 1]->glyphs.empty()
              ? text_run->typeface_runs[i + 1]->glyphs.front()->total_advance
              : text_run->location.width();
      const float right_edge = right_edge_advance + text_run->location.x();
      gfx::RectF run_location(left_edge, text_run->location.y(),
                              right_edge - left_edge,
                              text_run->location.height());
      run_location.Scale(1.0 / effective_zoom);
      left_edge = right_edge;

      std::vector<uint32_t> glyphs;
      // This is the total_advance + writing direction axis offsets
      std::vector<float> glyph_positions;
      // This is the offsets for the non-writing direction axis.
      std::vector<float> glyph_offsets;
      const size_t num_glyphs = typeface_run->glyphs.size();
      glyphs.reserve(num_glyphs);
      glyph_positions.reserve(num_glyphs);
      glyph_offsets.reserve(num_glyphs);
      for (const pdf::mojom::InkGlyphInfoPtr& glyph_info :
           typeface_run->glyphs) {
        gfx::Vector2dF position(glyph_info->offset.x() +
                                    glyph_info->total_advance -
                                    prev_right_edge_advance,
                                glyph_info->offset.y());
        position.Scale(1.0 / effective_zoom);

        glyph_positions.push_back(position.x());
        glyph_offsets.push_back(position.y());
        glyphs.push_back(glyph_info->glyph);
      }
      prev_right_edge_advance = right_edge_advance;

      CHECK_EQ(glyphs.size(), glyph_positions.size());
      if (glyphs.empty()) {
        continue;
      }
      InkTextInfo output_info(FontId(typeface_run->typeface_id),
                              std::move(glyphs), std::move(glyph_positions),
                              run_location, typeface_run->is_horizontal);
      MaybeCorrectNonZeroFirstOffset(output_info);

      const bool all_zero =
          std::ranges::all_of(glyph_offsets, [](float v) { return v == 0; });
      if (all_zero) {
        results.push_back(std::move(output_info));
      } else {
        std::vector<InkTextInfo> split_infos =
            Split2DOffsets(output_info, glyph_offsets);
        results.insert(results.end(),
                       std::make_move_iterator(split_infos.begin()),
                       std::make_move_iterator(split_infos.end()));
      }
    }
  }
  return results;
}

}  // namespace chrome_pdf
