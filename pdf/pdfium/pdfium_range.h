// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_RANGE_H_
#define PDF_PDFIUM_PDFIUM_RANGE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "pdf/page_orientation.h"
#include "pdf/pdf_rect.h"
#include "pdf/pdfium/pdfium_page.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace chrome_pdf {

constexpr char16_t kZeroWidthSpace = 0x200B;
constexpr char16_t kPDFSoftHyphenMarker = 0xFFFE;

// Helper for identifying characters that PDFium outputs, via FPDFText_GetText,
// that have special meaning, but should not be included in things like copied
// text or when running find.
bool IsIgnorableCharacter(char16_t c);

// Describes location of a string of characters.
class PDFiumRange {
 public:
  // Specifies the tightness of the bounding box for GetRectsWithTightness().
  enum class PdfBoundsTightness {
    // All character bounds that form the results are loose, which gives the
    // selection a bit more padding.
    kLoose,

    // The character bounds surrounds character glyphs tightly on the top and
    // bottom. Left and right are the same as `kLoose`.
    kTightVertical,
  };

  // Shorthand for the 3-params ctor, with `char_index` set to 0 and
  // `char_count` set to the number of characters in `page`.
  static PDFiumRange AllTextOnPage(PDFiumPage* page);

  // Like the constructor below, but the range must be specified in the forward
  // direction. The returned object is constructed backwards with an adjusted
  // `char_index` and a negative `char_count`. e.g.
  // (char_index=0, char_count=3) in the forward direction becomes
  // (char_index=3, char_count=-3) in the backward direction.
  static PDFiumRange CreateBackwards(PDFiumPage* page,
                                     int char_index,
                                     int char_count);

  // See definition of `char_index_` and `char_count_` for the semantics of
  // `char_index` and `char_count`, respectively.
  PDFiumRange(PDFiumPage* page, int char_index, int char_count);
  PDFiumRange(const PDFiumRange&);
  PDFiumRange& operator=(const PDFiumRange&);
  PDFiumRange(PDFiumRange&&) noexcept;
  PDFiumRange& operator=(PDFiumRange&&) noexcept;
  ~PDFiumRange();

  // Update how many characters are in the selection.  Could be negative if
  // backwards.
  void SetCharCount(int char_count);

  uint32_t page_index() const { return page_->index(); }
  int char_index() const { return char_index_; }
  int char_count() const { return char_count_; }

  // Gets bounding rectangles of this range in screen coordinates, based on the
  // input params. This uses `PdfBoundsTightness::kLoose` so the selection
  // better matches text selection in Blink and in other applications.
  const std::vector<gfx::Rect>& GetScreenRects(
      const gfx::Point& point,
      double zoom,
      PageOrientation orientation) const;

  // Gets bounding rectangles of this range in PDF coordinates.
  std::vector<PdfRect> GetRects() const;
  std::vector<PdfRect> GetRectsWithTightness(
      PdfBoundsTightness tightness) const;

  // Gets the string of characters in this range.
  std::u16string GetText() const;

 private:
  PDFiumPage::ScopedUnloadPreventer page_unload_preventer_;

  // The page containing the range. Must outlive `this`.
  raw_ptr<PDFiumPage> page_;
  // Index of first character. Must be a positive value. Examples:
  // - 0 is to the left of the first character.
  // - 1 is between the first character and the second character.
  // - N, for a page with N characters, is to the right of the last character.
  int char_index_;
  // How many characters are part of this range (negative if backwards).
  int char_count_;

  // Cache of ScreenRect, and the associated variables used when caching it.
  mutable std::vector<gfx::Rect> cached_screen_rects_;
  mutable gfx::Point cached_screen_rects_point_;
  mutable double cached_screen_rects_zoom_ = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_RANGE_H_
