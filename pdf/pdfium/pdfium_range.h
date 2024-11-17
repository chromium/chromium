// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_RANGE_H_
#define PDF_PDFIUM_PDFIUM_RANGE_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "pdf/page_orientation.h"
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
  // Shorthand for the 3-params ctor, with `char_index` set to 0 and
  // `char_count` set to the number of characters in `page`.
  static PDFiumRange AllTextOnPage(PDFiumPage* page);

  PDFiumRange(PDFiumPage* page, int char_index, int char_count);
  PDFiumRange(const PDFiumRange&);
  PDFiumRange& operator=(const PDFiumRange&);
  PDFiumRange(PDFiumRange&&) noexcept;
  PDFiumRange& operator=(PDFiumRange&&) noexcept;
  ~PDFiumRange();

  // Update how many characters are in the selection.  Could be negative if
  // backwards.
  void SetCharCount(int char_count);

  int page_index() const { return page_->index(); }
  int char_index() const { return char_index_; }
  int char_count() const { return char_count_; }

  // Gets bounding rectangles of range in screen coordinates.
  const std::vector<gfx::Rect>& GetScreenRects(
      const gfx::Point& point,
      double zoom,
      PageOrientation orientation) const;

  // Gets the string of characters in this range.
  std::u16string GetText() const;

 private:
  PDFiumPage::ScopedUnloadPreventer page_unload_preventer_;

  // The page containing the range. Must outlive `this`.
  raw_ptr<PDFiumPage> page_;
  // Index of first character.
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
