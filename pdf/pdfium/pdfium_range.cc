// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_range.h"

#include <string>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "pdf/accessibility_structs.h"
#include "pdf/pdfium/pdfium_api_string_buffer_adapter.h"
#include "third_party/pdfium/public/fpdf_searchex.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace chrome_pdf {

namespace {

// Enables AccessibilityTextRunInfo-based screen rects.
// TODO(crbug.com/40448046): Remove this kill switch after a safe rollout.
BASE_FEATURE(kPdfAccessibilityTextRunInfoScreenRects,
             "PdfAccessibilityTextRunInfoScreenRects",
             base::FEATURE_ENABLED_BY_DEFAULT);

void AdjustForBackwardsRange(int& index, int& count) {
  if (count < 0) {
    count *= -1;
    index -= count - 1;
  }
}

// Struct with only the text run info needed for PDFiumRange::GetScreenRects().
struct ScreenRectTextRunInfo {
  gfx::Rect screen_rect;
  size_t char_count;
};

// Returns a ratio between [0, 1].
float GetVerticalOverlap(const gfx::Rect& rect1, const gfx::Rect& rect2) {
  CHECK(!rect1.IsEmpty());
  CHECK(!rect2.IsEmpty());

  gfx::Rect union_rect = rect1;
  union_rect.Union(rect2);

  if (union_rect.height() == rect1.height() ||
      union_rect.height() == rect2.height()) {
    return 1.0f;
  }

  gfx::Rect intersect_rect = rect1;
  intersect_rect.Intersect(rect2);
  return static_cast<float>(intersect_rect.height()) / union_rect.height();
}

// Returns true if there is sufficient horizontal and vertical overlap.
bool ShouldMergeHorizontalRects(const ScreenRectTextRunInfo& text_run1,
                                const ScreenRectTextRunInfo& text_run2) {
  static constexpr float kVerticalOverlapThreshold = 0.8f;
  const gfx::Rect& rect1 = text_run1.screen_rect;
  const gfx::Rect& rect2 = text_run2.screen_rect;
  if (GetVerticalOverlap(rect1, rect2) < kVerticalOverlapThreshold) {
    return false;
  }

  static constexpr float kHorizontalWidthFactor = 1.0f;
  const float average_width1 =
      kHorizontalWidthFactor * rect1.width() / text_run1.char_count;
  const float average_width2 =
      kHorizontalWidthFactor * rect2.width() / text_run2.char_count;
  const float rect1_left = rect1.x() - average_width1;
  const float rect1_right = rect1.right() + average_width1;
  const float rect2_left = rect2.x() - average_width2;
  const float rect2_right = rect2.right() + average_width2;
  return rect1_left < rect2_right && rect1_right > rect2_left;
}

// Since PDFiumPage::GetTextRunInfo() can end a text run for a variety of
// reasons, post-process the collected text run data and merge rectangles.
std::vector<gfx::Rect> MergeAdjacentRects(
    base::span<ScreenRectTextRunInfo> text_runs) {
  std::vector<gfx::Rect> results;
  const ScreenRectTextRunInfo* previous_text_run = nullptr;
  gfx::Rect current_screen_rect;
  for (const auto& text_run : text_runs) {
    if (previous_text_run) {
      // TODO(crbug.com/40448046): Improve vertical text handling.
      // For now, treat all text as horizontal, as that is the majority of the
      // text. Also, PDFiumPage::GetTextPage() has bugs in its heuristics where
      // it mistakenly reports horizontal text as vertical.
      if (ShouldMergeHorizontalRects(*previous_text_run, text_run)) {
        current_screen_rect.Union(text_run.screen_rect);
      } else {
        results.push_back(current_screen_rect);
        current_screen_rect = text_run.screen_rect;
      }
    } else {
      current_screen_rect = text_run.screen_rect;
    }
    previous_text_run = &text_run;
  }

  if (!current_screen_rect.IsEmpty()) {
    results.push_back(current_screen_rect);
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

  if (char_count_ == 0) {
    return cached_screen_rects_;
  }

  FPDF_TEXTPAGE text_page = page_->GetTextPage();
  if (!text_page) {
    return cached_screen_rects_;
  }

  int char_index = char_index_;
  int char_count = char_count_;

  AdjustForBackwardsRange(char_index, char_count);
  DCHECK_GE(char_index, 0) << " start: " << char_index_
                           << " count: " << char_count_;
  DCHECK_LT(char_index, FPDFText_CountChars(text_page))
      << " start: " << char_index_ << " count: " << char_count_;

  if (!base::FeatureList::IsEnabled(kPdfAccessibilityTextRunInfoScreenRects)) {
    int count = FPDFText_CountRects(text_page, char_index, char_count);
    for (int i = 0; i < count; ++i) {
      double left;
      double top;
      double right;
      double bottom;
      FPDFText_GetRect(text_page, i, &left, &top, &right, &bottom);
      gfx::Rect rect = page_->PageToScreen(point, zoom, left, top, right,
                                           bottom, orientation);
      if (rect.IsEmpty()) {
        continue;
      }
      cached_screen_rects_.push_back(rect);
    }

    return cached_screen_rects_;
  }

  std::vector<ScreenRectTextRunInfo> text_runs;
  const int end_char_index = char_index + char_count;
  bool reached_end = false;
  while (!reached_end) {
    // Should not fail because `text_page` is non-null and `char_index` is
    // always in range.
    std::optional<AccessibilityTextRunInfo> text_run_info =
        page_->GetTextRunInfo(char_index);
    CHECK(text_run_info.has_value());

    // Figure out how many characters to process in the for-loop below, and
    // determine if this while-loop iteration reached the end of the range.
    int next_char_index = char_index + text_run_info.value().len;
    reached_end = next_char_index >= end_char_index;
    if (reached_end) {
      next_char_index = end_char_index;
    }

    // Do not use the bounds from `text_run_info`, as those are in the wrong
    // coordinate system. Calculate it here instead.
    gfx::Rect text_run_rect;
    for (int i = char_index; i < next_char_index; ++i) {
      // Use the loose rectangle, which gives the selection a bit more padding.
      // In comparison, the rectangle from FPDFText_GetCharBox() surrounds the
      // glyph too tightly.
      //
      // Should not fail because `text_page` is non-null, `i` is always in
      // range, and the out-parameter is non-null.
      FS_RECTF rect;
      bool got_rect = FPDFText_GetLooseCharBox(text_page, i, &rect);
      CHECK(got_rect);

      // Check for empty `rect` and skip. PDFiumPage::PageToScreen() may round
      // an empty `rect` to a 1x1 `screen_rect`, which is hard to distinguish
      // from an actual 1x1 `rect`.
      if (rect.left == rect.right || rect.top == rect.bottom) {
        continue;
      }

      gfx::Rect screen_rect =
          page_->PageToScreen(point, zoom, rect.left, rect.top, rect.right,
                              rect.bottom, orientation);
      text_run_rect.Union(screen_rect);
    }
    if (!text_run_rect.IsEmpty()) {
      text_runs.emplace_back(text_run_rect, next_char_index - char_index);
    }

    char_index = next_char_index;
  }

  cached_screen_rects_ = MergeAdjacentRects(text_runs);
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
