// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_INK_TEXT_H_
#define PDF_PDF_INK_TEXT_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "pdf/mojom/pdf.mojom.h"
#include "pdf/page_orientation.h"
#include "pdf/pdf_ink_ids.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

using SkColor = uint32_t;

namespace chrome_pdf {

// These values are persisted in PDFs as integers. Do not change the assigned
// integer values to maintain backward compatibility.
// LINT.IfChange(TextTypeface)
enum class TextTypeface {
  kSansSerif = 0,
  kSerif = 1,
  kMonospace = 2,
  kMinValue = kSansSerif,
  kMaxValue = kMonospace,
};
// LINT.ThenChange(
//   //chrome/browser/resources/pdf/constants.ts:TextTypeface,
//   //tools/metrics/histograms/metadata/pdf/enums.xml:PDFInk2TextAnnotationTypeface
// )

// These values are persisted in PDFs as integers. Do not change the assigned
// integer values to maintain backward compatibility.
// LINT.IfChange(TextAlignment)
enum class TextAlignment {
  kLeft = 0,
  kCenter = 1,
  kRight = 2,
  kMinValue = kLeft,
  kMaxValue = kRight,
};
// LINT.ThenChange(
//   //chrome/browser/resources/pdf/constants.ts:TextAlignment,
//   //tools/metrics/histograms/metadata/pdf/enums.xml:PDFInk2TextAnnotationAlignment
// )

std::string TextTypefaceToString(TextTypeface typeface);
std::string TextAlignmentToString(TextAlignment alignment);

struct InkTextBoxAttributes {
  bool operator==(const InkTextBoxAttributes& other) const = default;

  // `rect` is in CSS screen coordinates.
  gfx::RectF rect;
  SkColor color;
  float css_font_size;
  TextTypeface typeface;
  TextAlignment alignment;
  // The orientation of the textbox relative to the PDF page, in number of 90
  // degree clockwise rotations from 0 to 3.
  int orientation;
  // The orientation of the viewport (in 90 degree clockwise rotations) when the
  // text annotation was committed.
  PageOrientation viewport_orientation;
  bool is_bold;
  bool is_italic;
  bool is_strikethrough;
  std::string text;
};

// Holds metadata and reconstructed contents for a textbox extracted from marked
// page streams.
struct InkTextBox {
  InkTextBox(int id, InkTextBoxAttributes attributes);
  InkTextBox(const InkTextBox&) = delete;
  InkTextBox& operator=(const InkTextBox&) = delete;
  InkTextBox(InkTextBox&&) noexcept;
  InkTextBox& operator=(InkTextBox&&) noexcept;
  ~InkTextBox();

  // The unique textbox identifier read directly from the PDF's marked content
  // parameter (`TextboxId`), binding text object fragments together.
  int id;

  // The globally unique ID generated during load.
  InkLoadedTextId ink_loaded_text_id;

  InkTextBoxAttributes attributes;
};

// Key: 0-based page index.
// Value: Vector of textboxes on that page.
using DocumentInkTextBoxesMap = std::map<int, std::vector<InkTextBox>>;

struct InkTextInfo {
  InkTextInfo(FontId font_id,
              std::vector<uint32_t> glyphs,
              std::vector<float> glyph_positions,
              gfx::RectF location,
              bool is_horizontal,
              std::u16string text);
  InkTextInfo(FontId font_id,
              std::vector<uint32_t> glyphs,
              std::vector<float> glyph_positions,
              gfx::RectF location,
              bool is_horizontal,
              bool is_synthetic_bold,
              bool is_synthetic_italic,
              std::u16string text);
  InkTextInfo(InkTextInfo&&) noexcept;
  InkTextInfo& operator=(InkTextInfo&&) noexcept;
  ~InkTextInfo();

  FontId font_id;
  std::vector<uint32_t> glyphs;

  // Positions relative to the origin of the `location` rect in CSS pixels.
  // if is_horizontal is true, x-axis, if false, y-axis. Has the same length as
  // `glyphs`.
  std::vector<float> glyph_positions;

  // In CSS pixels. Based on top left of screen origin.
  gfx::RectF location;
  bool is_horizontal;
  bool is_synthetic_bold;
  bool is_synthetic_italic;

  // The UTF-16 text represented by the glyphs in this InkTextInfo. The length
  // of `glyphs` and `text` are not the same in general. So it's not possible to
  // take substrings of `text` at this point.
  std::u16string text;
};

// Represents a single line of text within an annotation, containing one or more
// `InkTextInfo` page objects (e.g. if the line spans multiple typefaces).
struct InkTextLine {
  InkTextLine(const gfx::RectF& location, std::vector<InkTextInfo> text_info);
  explicit InkTextLine(InkTextInfo single_text_info);
  InkTextLine(InkTextLine&&) noexcept;
  InkTextLine& operator=(InkTextLine&&) noexcept;
  ~InkTextLine();

  // Convert <textarea> metrics from blink::WebFormControlElement::GetTextInfo()
  // into `InkTextLine` objects, each containing one or more `InkTextInfo`s for
  // the glyphs of each text FPDF_PAGEOBJECT. Runs without glyphs/typeface runs
  // are skipped.
  //
  // All input numbers are physical pixels. All output numbers are CSS pixels.
  // `effective_zoom` is the ratio between physical pixels and CSS pixels.
  //
  // PDFium requires:
  //   - The glyph IDs to render in the correct order
  //   - One typeface per text FPDF_PAGEOBJECT
  //   - 1D glyph positions
  //   - The first glyph position must be 0 (so it must be included in the
  //     rectangle position)
  //
  // Blink provides:
  //   - Harfbuzz glyph positioning data, which is total_advance and 2D offset
  //   - `text_runs` that contain multiple typefaces for one location rectangle
  static std::vector<InkTextLine> BlinkTextInfoToPDFTextLines(
      const std::vector<pdf::mojom::InkTextRunPtr>& text_runs,
      float effective_zoom);

  // In CSS pixels. Based on top left of screen origin.
  gfx::RectF location;
  std::vector<InkTextInfo> text_info;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_TEXT_H_
