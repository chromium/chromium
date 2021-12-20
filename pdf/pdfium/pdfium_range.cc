// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_range.h"

#include "base/check_op.h"
#include "base/containers/cxx20_erase.h"
#include "base/strings/string_util.h"
#include "pdf/pdfium/pdfium_api_string_buffer_adapter.h"
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

PDFiumRange::PDFiumRange(PDFiumPage* page, int char_index, int char_count)
    : page_(page), char_index_(char_index), char_count_(char_count) {
  // TODO(crbug.com/1279497): Demote this CHECK to a DCHECK after the violating
  // caller is caught.
  CHECK(page_);
#if DCHECK_IS_ON()
  AdjustForBackwardsRange(char_index, char_count);
  DCHECK_LE(char_count, FPDFText_CountChars(page_->GetTextPage()));
#endif
}

PDFiumRange::PDFiumRange(const PDFiumRange& that) = default;

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
    PDFiumAPIStringBufferAdapter<std::u16string> api_string_adapter(
        &result, count, /*check_expected_size=*/false);
    unsigned short* data =
        reinterpret_cast<unsigned short*>(api_string_adapter.GetData());
    int written = FPDFText_GetText(page_->GetTextPage(), index, count, data);
    api_string_adapter.Close(written);

    const gfx::RectF page_bounds = page_->GetCroppedRect();
    std::u16string in_bound_text;
    in_bound_text.reserve(result.size());
    for (int i = 0; i < count; ++i) {
      gfx::RectF char_bounds = page_->GetCharBounds(index + i);

      // Make sure `char_bounds` has a minimum size so Intersects() works
      // correctly.
      if (char_bounds.IsEmpty()) {
        static constexpr gfx::SizeF kMinimumSize(0.0001f, 0.0001f);
        char_bounds.set_size(kMinimumSize);
      }
      if (page_bounds.Intersects(char_bounds))
        in_bound_text += result[i];
    }
    result = in_bound_text;
    base::EraseIf(result, IsIgnorableCharacter);
  }

  return result;
}

}  // namespace chrome_pdf
