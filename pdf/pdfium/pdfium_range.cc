// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_range.h"

#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/debug/alias.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_util.h"
#include "pdf/accessibility_structs.h"
#include "pdf/pdfium/pdfium_api_string_buffer_adapter.h"
#include "pdf/pdfium/pdfium_api_wrappers.h"
#include "third_party/pdfium/public/fpdf_searchex.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace chrome_pdf {

namespace {

void AdjustForBackwardsRange(int& index, int& count) {
  if (count < 0) {
    count *= -1;
    index -= count;
  }
}

// Struct with only the text run info needed for PDFiumRange::GetRects().
struct PdfRectTextRunInfo {
  // Default loose bounds.
  PdfRect pdf_rect;

  // Tight bounds. Only used with certain `PdfBoundsTightness` values.
  PdfRect tight_pdf_rect;

  size_t char_count;
};

// Returns a ratio between [0, 1].
float GetVerticalOverlap(const PdfRect& rect1, const PdfRect& rect2) {
  CHECK(!rect1.IsEmpty());
  CHECK(!rect2.IsEmpty());

  PdfRect union_rect = rect1;
  union_rect.Union(rect2);

  if (union_rect.height() == rect1.height() ||
      union_rect.height() == rect2.height()) {
    return 1.0f;
  }

  PdfRect intersect_rect = rect1;
  intersect_rect.Intersect(rect2);
  return intersect_rect.height() / union_rect.height();
}

// Returns true if there is sufficient horizontal and vertical overlap.
// Only considers `PdfRectTextRunInfo::pdf_rect`, so the merging decision is
// consistent, regardless of the `PDFiumRange::PdfBoundsTightness` value.
bool ShouldMergeHorizontalRects(const PdfRectTextRunInfo& text_run1,
                                const PdfRectTextRunInfo& text_run2) {
  static constexpr float kVerticalOverlapThreshold = 0.8f;
  const PdfRect& rect1 = text_run1.pdf_rect;
  const PdfRect& rect2 = text_run2.pdf_rect;
  if (GetVerticalOverlap(rect1, rect2) < kVerticalOverlapThreshold) {
    return false;
  }

  static constexpr float kHorizontalWidthFactor = 1.0f;
  const float average_width1 =
      kHorizontalWidthFactor * rect1.width() / text_run1.char_count;
  const float average_width2 =
      kHorizontalWidthFactor * rect2.width() / text_run2.char_count;
  const float rect1_left = rect1.left() - average_width1;
  const float rect1_right = rect1.right() + average_width1;
  const float rect2_left = rect2.left() - average_width2;
  const float rect2_right = rect2.right() + average_width2;
  return rect1_left < rect2_right && rect1_right > rect2_left;
}

// Since PDFiumPage::GetTextRunInfo() can end a text run for a variety of
// reasons, post-process the collected text run data and merge rectangles.
std::vector<PdfRect> MergeAdjacentRects(
    base::span<PdfRectTextRunInfo> text_runs,
    PDFiumRange::PdfBoundsTightness tightness) {
  std::vector<PdfRect> results;
  const PdfRectTextRunInfo* previous_text_run = nullptr;
  PdfRect current_pdf_rect;
  for (const auto& text_run : text_runs) {
    PdfRect effective_rect = text_run.pdf_rect;
    if (tightness == PDFiumRange::PdfBoundsTightness::kTightVertical) {
      *effective_rect.writable_bottom() = text_run.tight_pdf_rect.bottom();
      *effective_rect.writable_top() = text_run.tight_pdf_rect.top();
    }
    if (previous_text_run) {
      // TODO(crbug.com/40448046): Improve vertical text handling.
      // For now, treat all text as horizontal, as that is the majority of the
      // text. Also, PDFiumPage::GetTextPage() has bugs in its heuristics where
      // it mistakenly reports horizontal text as vertical.
      if (ShouldMergeHorizontalRects(*previous_text_run, text_run)) {
        current_pdf_rect.Union(effective_rect);
      } else {
        results.push_back(current_pdf_rect);
        current_pdf_rect = effective_rect;
      }
    } else {
      current_pdf_rect = effective_rect;
    }
    previous_text_run = &text_run;
  }

  if (!current_pdf_rect.IsEmpty()) {
    results.push_back(current_pdf_rect);
  }
  return results;
}

}  // namespace

bool IsIgnorableCharacter(char16_t c) {
  return c == kZeroWidthSpace || c == kPDFSoftHyphenMarker;
}

// static
PDFiumRange PDFiumRange::AllTextOnPage(PDFiumPage* page) {
  return PDFiumRange(page, 0, page->GetCharCount());
}

// static
PDFiumRange PDFiumRange::CreateBackwards(PDFiumPage* page,
                                         int char_index,
                                         int char_count) {
  CHECK_GE(char_count, 0);
  PDFiumRange range(page, char_index, char_count);
  if (char_count > 0) {
    range.char_index_ += char_count;
    range.char_count_ *= -1;
  }
  return range;
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
  if (char_count == char_count_) {
    return;
  }

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

  std::vector<PdfRect> rects = GetRects();
  cached_screen_rects_.reserve(rects.size());
  for (const auto& rect : rects) {
    cached_screen_rects_.push_back(
        page_->PageToScreen(point, zoom, rect, orientation));
  }
  return cached_screen_rects_;
}

std::vector<PdfRect> PDFiumRange::GetRects() const {
  return GetRectsWithTightness(PdfBoundsTightness::kLoose);
}

std::vector<PdfRect> PDFiumRange::GetRectsWithTightness(
    PdfBoundsTightness tightness) const {
  if (char_count_ == 0) {
    return {};
  }

  FPDF_TEXTPAGE text_page = page_->GetTextPage();
  if (!text_page) {
    return {};
  }

  // TODO(crbug.com/458015240): Remove debugging data after fixing the bug.
  const int char_index_debug = char_index_;
  const int char_count_debug = char_count_;
  base::debug::Alias(&char_index_debug);
  base::debug::Alias(&char_count_debug);

  int char_index = char_index_;
  int char_count = char_count_;

  AdjustForBackwardsRange(char_index, char_count);
  CHECK_GE(char_index, 0) << " start: " << char_index_
                          << " count: " << char_count_;
  CHECK_LT(char_index, FPDFText_CountChars(text_page))
      << " start: " << char_index_ << " count: " << char_count_;

  std::vector<PdfRectTextRunInfo> text_runs;
  const int end_char_index = char_index + char_count;
  bool reached_end = false;
  while (!reached_end) {
    // Should not fail because `text_page` is non-null and `char_index` is
    // always in range.
    std::optional<AccessibilityTextRunInfo> text_run_info =
        page_->GetTextRunInfoAt(char_index);
    CHECK(text_run_info.has_value());

    // Figure out how many characters to process in the for-loop below, and
    // determine if this while-loop iteration reached the end of the range.
    base::CheckedNumeric<uint32_t> safe_next_char_index =
        text_run_info.value().start_index;
    safe_next_char_index += text_run_info.value().len;
    int next_char_index;
    CHECK(safe_next_char_index.AssignIfValid(&next_char_index));
    reached_end = next_char_index >= end_char_index;
    if (reached_end) {
      next_char_index = end_char_index;
    }

    // Do not use the bounds from `text_run_info`, as those are in the wrong
    // coordinate system. Calculate it here instead.
    PdfRect text_run_rect;
    PdfRect tight_text_run_rect;
    for (int i = char_index; i < next_char_index; ++i) {
      // Use the loose rectangle, which gives the selection a bit more padding.
      // In comparison, the rectangle from FPDFText_GetCharBox() surrounds the
      // glyph too tightly.
      //
      // Should not fail because `text_page` is non-null, `i` is always in
      // range, and the out-parameter is non-null.
      PdfRect rect;
      bool got_rect =
          FPDFText_GetLooseCharBox(text_page, i, &FsRectFFromPdfRect(rect));
      CHECK(got_rect);
      text_run_rect.Union(rect);

      if (tightness == PdfBoundsTightness::kTightVertical) {
        // GetTextRunInfo() should not fail for the same reason as the
        // FPDFText_GetLooseCharBox() call above.
        tight_text_run_rect.Union(GetTextCharBox(text_page, i).value());
      }
    }
    if (!text_run_rect.IsEmpty()) {
      text_runs.emplace_back(/*pdf_rect=*/text_run_rect,
                             /*tight_pdf_rect=*/tight_text_run_rect,
                             /*char_count=*/next_char_index - char_index);
    }

    char_index = next_char_index;
  }

  return MergeAdjacentRects(text_runs, tightness);
}

std::u16string PDFiumRange::GetText() const {
  if (char_count_ == 0) {
    return std::u16string();
  }

  int index = char_index_;
  int count = char_count_;
  AdjustForBackwardsRange(index, count);
  CHECK_GT(count, 0);

  std::u16string result;
  {
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
    // Let `api_string_adapter` go out of scope to avoid having a potentially
    // dangling pointer to `result`.
  }

  const gfx::RectF page_bounds = page_->GetCroppedRect();
  std::u16string in_bound_text;
  in_bound_text.reserve(result.size());

  // If FPDFText_GetText() trimmed off characters, figure out how many were
  // trimmed from the front. Store the result in `index_offset`, so the
  // IsCharInPageBounds() calls below can have the correct index.
  CHECK_GE(static_cast<size_t>(count), result.size());
  size_t trimmed_count = static_cast<size_t>(count) - result.size();
  int index_offset = index;
  while (trimmed_count) {
    if (FPDFText_GetTextIndexFromCharIndex(page_->GetTextPage(),
                                           index_offset) >= 0) {
      break;
    }
    --trimmed_count;
    ++index_offset;
  }

  for (size_t i = 0; i < result.size(); ++i) {
    // Filter out characters outside the page bounds, which are semantically
    // not part of the page.
    if (page_->IsCharInPageBounds(index_offset + i, page_bounds)) {
      in_bound_text += result[i];
    }
  }
  result = std::move(in_bound_text);
  std::erase_if(result, IsIgnorableCharacter);

  return result;
}

}  // namespace chrome_pdf
