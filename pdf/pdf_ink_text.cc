// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_text.h"

#include <iterator>
#include <string>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/notreached.h"

namespace chrome_pdf {
namespace {

struct ExtraGlyphInfo {
  float offset;
  uint32_t character_index;
};

// Makes a substring of `input` containing the characters in [start, end) with
// the `location` rectangle cut so that the returned `location` covers only the
// glyphs in the range.
//
// TODO(crbug.com/510015130): check `is_horizontal`: if false the rectangle
// would need to be split on the y-axis instead of the x-axis.
// TODO(crbug.com/507508097): Correctly handle RTL text.
InkTextInfo MakeSubstrTextInfo(const InkTextInfo& input,
                               float y_offset,
                               base::span<const ExtraGlyphInfo> glyph_info,
                               size_t start,
                               size_t end) {
  CHECK_LT(start, input.glyphs.size());
  CHECK_LE(end, input.glyphs.size());
  CHECK_LT(start, end);
  CHECK_EQ(input.glyphs.size(), input.glyph_positions.size());

  // TODO(crbug.com/507508097): Correctly handle RTL text. The most immediate
  // problem is that in RTL text the glyphs have been reversed in order by Blink
  // so while `total_advance` is ascending `character_index` will be descending.
  // Also PDFium will reverse the string in ActualText if it heuristically
  // determines that the text is RTL. All of that is possible to handle
  // correctly. The real problem is handling that with 2D glyph positioning at
  // the same time. If the entire string is wrapped in a reverse ActualText it
  // copies correctly but the highlight rect is the size of a single character.
  // Reversing the order of the glyphs in the PDF stream and wrapping each
  // individually with ActualText ends up not getting the right string and the
  // 2D offsets end up inserting spaces and newlines in between the glyphs.
  const bool is_rtl =
      glyph_info.back().character_index < glyph_info.front().character_index;

  const size_t count = end - start;
  const float left = input.glyph_positions[start];
  const float right = end < input.glyph_positions.size()
                          ? input.glyph_positions[end]
                          : input.location.width();

  std::vector<uint32_t> glyphs =
      base::ToVector(base::span(input.glyphs).subspan(start, count));
  std::vector<float> glyph_positions =
      base::ToVector(base::span(input.glyph_positions).subspan(start, count),
                     [&left](float pos) { return pos - left; });
  gfx::RectF location(/*x=*/input.location.x() + left,
                      /*y=*/input.location.y() + y_offset,
                      /*width=*/right - left,
                      /*height=*/input.location.height());
  uint32_t start_char = glyph_info[start].character_index;
  size_t end_char = end == glyph_info.size() ? input.text.size()
                                             : glyph_info[end].character_index;
  size_t num_chars = end_char - start_char;
  return InkTextInfo(input.font_id, std::move(glyphs),
                     std::move(glyph_positions), location, input.is_horizontal,
                     input.is_synthetic_bold, input.is_synthetic_italic,
                     !is_rtl ? input.text.substr(start_char, num_chars) : u"");
}

// Because PDF text objects only support 1D glyph positioning, it is necessary
// to split runs of text that have 2D glyph offsets from Harfbuzz such that each
// PDF text object contains text all on the same y-axis position.
//
// So, take in an `input` InkTextInfo, which represents a run of text all on one
// line with the same typeface, plus the y-axis offsets in `glyph_info.offset`
// for each glyph in that run. Then split the `input` into multiple InkTextInfo
// objects based on the y-axis position and apply the relevant `location`
// rectangle adjustments to incorporate the y-axis offsets.
//
// Additionally handle taking a substring of the `input.text` using the glyph
// character_index data in `glyph_info`.
//
// It's not necessary to call this if `glyph_info.offsets` is all zero, it will
// just push a copy of `input` in that case.
//
// TODO(crbug.com/510015130): check `is_horizontal`: if false
// `glyph_info.offsets` would be interpreted as x-axis offsets instead of
// y-axis. The documentation above needs to be updated too because the axes will
// flip.
// TODO(crbug.com/507508097): This function can sometimes split strings smaller
// than Harfbuzz glyph clusters and this leaves InkTextInfo objects with an
// empty string which inserts /ActualText <FEFF> spans in the PDF. Ideally the
// correct behavior should be to group multiple InkTextInfo objects into a
// single Span mark so that the smallest ActualText string is a single complete
// Harfbuzz glyph cluster.
std::vector<InkTextInfo> Split2DOffsets(
    const InkTextInfo& input,
    base::span<const ExtraGlyphInfo> glyph_info) {
  CHECK(!glyph_info.empty());
  CHECK_EQ(glyph_info.size(), input.glyphs.size());
  CHECK_EQ(glyph_info.size(), input.glyph_positions.size());

  std::vector<InkTextInfo> results;
  size_t run_start = 0;
  for (size_t i = 1; i <= glyph_info.size(); ++i) {
    bool is_boundary = i == glyph_info.size() ||
                       glyph_info[i - 1].offset != glyph_info[i].offset;
    if (!is_boundary) {
      continue;
    }
    results.push_back(MakeSubstrTextInfo(input, glyph_info[i - 1].offset,
                                         glyph_info, run_start, i));
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

std::string TextTypefaceToString(TextTypeface typeface) {
  switch (typeface) {
    case TextTypeface::kSansSerif:
      return "sans-serif";
    case TextTypeface::kSerif:
      return "serif";
    case TextTypeface::kMonospace:
      return "monospace";
  }
  NOTREACHED();
}

std::string TextAlignmentToString(TextAlignment alignment) {
  switch (alignment) {
    case TextAlignment::kLeft:
      return "left";
    case TextAlignment::kCenter:
      return "center";
    case TextAlignment::kRight:
      return "right";
  }
  NOTREACHED();
}


InkTextBox::InkTextBox(int id, InkTextBoxAttributes attributes)
    : id(id), attributes(std::move(attributes)) {}
InkTextBox::InkTextBox(InkTextBox&&) noexcept = default;
InkTextBox& InkTextBox::operator=(InkTextBox&&) noexcept = default;
InkTextBox::~InkTextBox() = default;

InkTextInfo::InkTextInfo(FontId font_id,
                         std::vector<uint32_t> glyphs,
                         std::vector<float> glyph_positions,
                         gfx::RectF location,
                         bool is_horizontal,
                         std::u16string text)
    : InkTextInfo(font_id,
                  std::move(glyphs),
                  std::move(glyph_positions),
                  location,
                  is_horizontal,
                  /*is_synthetic_bold=*/false,
                  /*is_synthetic_italic=*/false,
                  std::move(text)) {}
InkTextInfo::InkTextInfo(FontId font_id,
                         std::vector<uint32_t> glyphs,
                         std::vector<float> glyph_positions,
                         gfx::RectF location,
                         bool is_horizontal,
                         bool is_synthetic_bold,
                         bool is_synthetic_italic,
                         std::u16string text)
    : font_id(font_id),
      glyphs(std::move(glyphs)),
      glyph_positions(std::move(glyph_positions)),
      location(location),
      is_horizontal(is_horizontal),
      is_synthetic_bold(is_synthetic_bold),
      is_synthetic_italic(is_synthetic_italic),
      text(std::move(text)) {}
InkTextInfo::InkTextInfo(InkTextInfo&&) noexcept = default;
InkTextInfo& InkTextInfo::operator=(InkTextInfo&&) noexcept = default;
InkTextInfo::~InkTextInfo() = default;

InkTextLine::InkTextLine(const gfx::RectF& location,
                         std::vector<InkTextInfo> text_info)
    : location(location), text_info(std::move(text_info)) {}

InkTextLine::InkTextLine(InkTextInfo single_text_info)
    : location(single_text_info.location) {
  text_info.push_back(std::move(single_text_info));
}

InkTextLine::InkTextLine(InkTextLine&&) noexcept = default;
InkTextLine& InkTextLine::operator=(InkTextLine&&) noexcept = default;
InkTextLine::~InkTextLine() = default;

std::vector<InkTextLine> InkTextLine::BlinkTextInfoToPDFTextLines(
    const std::vector<pdf::mojom::InkTextRunPtr>& text_runs,
    float effective_zoom) {
  std::vector<InkTextLine> results;
  results.reserve(text_runs.size());
  for (const pdf::mojom::InkTextRunPtr& text_run : text_runs) {
    const std::vector<pdf::mojom::InkTypefaceRunPtr>& typeface_runs =
        text_run->typeface_runs;

    // Create an InkTextInfo to represent `text_run`, which will later be split
    // into typeface runs.
    //
    // Note: The font_id, is_synthetic_bold, is_synthetic_italic, and
    // is_horizontal members here are placeholder values to be replaced later
    // when `text_run_info` is split into typeface_runs. The reason is
    // these parameters change for each typeface run.
    //
    // TODO(crbug.com/510015130): It probably isn't possible for `text_run`
    // to contain typeface runs with different values of is_horizontal. That
    // needs to be confirmed. It might be correct to use the value of the first
    // typeface run (if not empty) here.
    InkTextInfo text_run_info(FontId(0), {}, {}, text_run->location, true,
                              text_run->text);
    text_run_info.location.Scale(1.0f / effective_zoom);
    std::vector<ExtraGlyphInfo> extra_glyph_info;
    size_t glyphs_count = 0;
    for (const pdf::mojom::InkTypefaceRunPtr& typeface_run : typeface_runs) {
      glyphs_count += typeface_run->glyphs.size();
    }
    text_run_info.glyphs.reserve(glyphs_count);
    text_run_info.glyph_positions.reserve(glyphs_count);
    extra_glyph_info.reserve(glyphs_count);

    // Flatten `typeface_runs` into `text_run_info` and `extra_glyph_info` for
    // the whole `text_run`.
    for (const pdf::mojom::InkTypefaceRunPtr& typeface_run : typeface_runs) {
      CHECK(!typeface_run->glyphs.empty());
      // TODO(crbug.com/510015130): handle vertical text.
      CHECK(typeface_run->is_horizontal);

      for (const pdf::mojom::InkGlyphInfoPtr& glyph_info :
           typeface_run->glyphs) {
        text_run_info.glyphs.push_back(glyph_info->glyph);
        text_run_info.glyph_positions.push_back(
            (glyph_info->total_advance + glyph_info->offset.x()) /
            effective_zoom);
        extra_glyph_info.emplace_back(glyph_info->offset.y() / effective_zoom,
                                      glyph_info->character_index);
      }
    }

    // Process `text_run_info` and `extra_glyph_info` into separate
    // `InkTextInfo` structs each representing a single PDF text object using
    // the information in `typeface_runs`.
    std::vector<InkTextInfo> line_text_infos;
    size_t run_start = 0;
    base::span<ExtraGlyphInfo> extra_glyph_info_span(extra_glyph_info);
    for (const pdf::mojom::InkTypefaceRunPtr& typeface_run : typeface_runs) {
      const size_t run_end = run_start + typeface_run->glyphs.size();
      CHECK_EQ(text_run_info.is_horizontal, typeface_run->is_horizontal);
      InkTextInfo typeface_run_info =
          MakeSubstrTextInfo(text_run_info, /*y_offset=*/0,
                             extra_glyph_info_span, run_start, run_end);
      typeface_run_info.font_id = FontId(typeface_run->typeface_id);
      typeface_run_info.is_synthetic_bold = typeface_run->is_synthetic_bold;
      typeface_run_info.is_synthetic_italic = typeface_run->is_synthetic_italic;

      MaybeCorrectNonZeroFirstOffset(typeface_run_info);

      base::span<ExtraGlyphInfo> typeface_run_glyph_info =
          extra_glyph_info_span.subspan(run_start, typeface_run->glyphs.size());
      const bool all_zero = std::ranges::all_of(
          typeface_run_glyph_info,
          [](const ExtraGlyphInfo& info) { return info.offset == 0; });
      if (all_zero) {
        line_text_infos.push_back(std::move(typeface_run_info));
      } else {
        // Convert the character_index values to indexes into
        // `typeface_run_info`.text (from indexes into `text_run`.text).
        //
        // TODO(crbug.com/507508097): Correctly handle RTL text. Without this
        // condition this index adjustment causes underflow wrap-around.
        const bool is_rtl = extra_glyph_info_span.back().character_index <
                            extra_glyph_info_span.front().character_index;
        if (!is_rtl) {
          uint32_t first_character_index =
              typeface_run_glyph_info.front().character_index;
          for (ExtraGlyphInfo& info : typeface_run_glyph_info) {
            info.character_index -= first_character_index;
          }
        }
        std::vector<InkTextInfo> split_infos =
            Split2DOffsets(typeface_run_info, typeface_run_glyph_info);
        line_text_infos.insert(line_text_infos.end(),
                               std::make_move_iterator(split_infos.begin()),
                               std::make_move_iterator(split_infos.end()));
      }
      run_start = run_end;
    }
    if (!line_text_infos.empty()) {
      results.push_back(
          InkTextLine(text_run_info.location, std::move(line_text_infos)));
    }
  }
  return results;
}

}  // namespace chrome_pdf
