// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_range.h"

#include <string>

#include "base/check_op.h"
#include "base/strings/string_util.h"
#include "pdf/pdfium/pdfium_api_string_buffer_adapter.h"
#include "third_party/pdfium/public/fpdf_searchex.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace chrome_pdf {

namespace {

void AdjustForBackwardsRange(int& index, int& count) {
  if (count < 0) {
    count *= -1;
    index -= count - 1;
  }
}

}  // namespace

bool IsIgnorableCharacter(char16_t c) {
  return c == kZeroWidthSpace || c == kPDFSoftHyphenMarker;
}

// static
PDFiumRange PDFiumRange::AllTextOnPage(PDFiumPage* page) {
  return PDFiumRange(page, 0, page->GetCharCount());
}

PDFiumRange::PDFiumRange(PDFiumPage* page, int char_index, int char_count)
    : page_unload_preventer_(page),
      page_(page),
      char_index_(char_index),
      char_count_(char_count) {
  DCHECK(page_);
  // Ensure page load, while `page_unload_preventer_` prevents page unload.
  // This prevents GetScreenRects() from triggering page loads, which can have
  // surprising side effects, considering GetScreenRects() is const.
  [[maybe_unused]] FPDF_TEXTPAGE text_page = page_->GetTextPage();
#if DCHECK_IS_ON()
  AdjustForBackwardsRange(char_index, char_count);
  DCHECK_LE(char_count, FPDFText_CountChars(text_page));
#endif
}

PDFiumRange::PDFiumRange(const PDFiumRange&) = default;

PDFiumRange& PDFiumRange::operator=(const PDFiumRange&) = default;

PDFiumRange::PDFiumRange(PDFiumRange&&) noexcept = default;

PDFiumRange& PDFiumRange::operator=(PDFiumRange&&) noexcept = default;

PDFiumRange::~PDFiumRange() = default;

void PDFiumRange::SetCharCount(int char_count) {
  char_count_ = char_count;
#if DCHECK_IS_ON()
  int dummy_index = 0;
  AdjustForBackwardsRange(dummy_index, char_count);
  DCHECK_LE(char_count, FPDFText_CountChars(page_->GetTextPage()));
#endif

  cached_screen_rects_point_ = gfx::Point();
  cached_screen_rects_zoom_ = 0;
}

const std::vector<gfx::Rect>& PDFiumRange::GetScreenRects(
    const gfx::Point& point,
    double zoom,
    PageOrientation orientation) const {
  if (point == cached_screen_rects_point_ &&
      zoom == cached_screen_rects_zoom_) {
    return cached_screen_rects_;
  }

  cached_screen_rects_.clear();
  cached_screen_rects_point_ = point;
  cached_screen_rects_zoom_ = zoom;

  int char_index = char_index_;
  int char_count = char_count_;
  if (char_count == 0)
    return cached_screen_rects_;

  AdjustForBackwardsRange(char_index, char_count);
  DCHECK_GE(char_index, 0) << " start: " << char_index_
                           << " count: " << char_count_;
  DCHECK_LT(char_index, FPDFText_CountChars(page_->GetTextPage()))
      << " start: " << char_index_ << " count: " << char_count_;

  int count = FPDFText_CountRects(page_->GetTextPage(), char_index, char_count);
  for (int i = 0; i < count; ++i) {
    double left;
    double top;
    double right;
    double bottom;
    FPDFText_GetRect(page_->GetTextPage(), i, &left, &top, &right, &bottom);
    gfx::Rect rect =
        page_->PageToScreen(point, zoom, left, top, right, bottom, orientation);
    if (rect.IsEmpty())
      continue;
    cached_screen_rects_.push_back(rect);
  }

  return cached_screen_rects_;
}

std::u16string PDFiumRange::GetText() const {
  int index = char_index_;
  int count = char_count_;
  std::u16string result;
  if (count == 0)
    return result;

  AdjustForBackwardsRange(index, count);
  if (count > 0) {
    // Note that the `expected_size` value includes the NUL terminator.
    //
    // Cannot set `check_expected_size` to true here because the fix to
    // https://crbug.com/pdfium/1139 made it such that FPDFText_GetText() is
    // not always consistent with FPDFText_CountChars() and may trim characters.
    //
    // Instead, treat `count` as the requested count, but use the size of
    // `result` as the source of truth for how many characters
    // FPDFText_GetText() actually wrote out.
    PDFiumAPIStringBufferAdapter<std::u16string> api_string_adapter(
        &result, /*expected_size=*/count + 1, /*check_expected_size=*/false);
    unsigned short* data =
        reinterpret_cast<unsigned short*>(api_string_adapter.GetData());
    int written = FPDFText_GetText(page_->GetTextPage(), index, count, data);
    // FPDFText_GetText() returns 0 on failure. Never negative value.
    DCHECK_GE(written, 0);
    api_string_adapter.Close(written);

    const gfx::RectF page_bounds = page_->GetCroppedRect();
    std::u16string in_bound_text;
    in_bound_text.reserve(result.size());

    // If FPDFText_GetText() trimmed off characters, figure out how many were
    // trimmed from the front. Store the result in `index_offset`, so the
    // IsCharInPageBounds() calls below can have the correct index.
    CHECK_GE(static_cast<size_t>(count), result.size());
    size_t trimmed_count = static_cast<size_t>(count) - result.size();
    int index_offset = 0;
    while (trimmed_count) {
      if (FPDFText_GetTextIndexFromCharIndex(page_->GetTextPage(),
                                             index + index_offset) >= 0) {
        break;
      }
      --trimmed_count;
      ++index_offset;
    }

    for (size_t i = 0; i < result.size(); ++i) {
      // Filter out characters outside the page bounds, which are semantically
      // not part of the page.
      if (page_->IsCharInPageBounds(index + index_offset + i, page_bounds))
        in_bound_text += result[i];
    }
    result = in_bound_text;
    std::erase_if(result, IsIgnorableCharacter);
  }

  return result;
}

}  // namespace chrome_pdf
