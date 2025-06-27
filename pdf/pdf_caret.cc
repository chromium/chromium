// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_caret.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/time/time.h"
#include "pdf/accessibility_structs.h"
#include "pdf/pdf_caret_client.h"
#include "pdf/region_data.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace chrome_pdf {

namespace {

constexpr SkColor4f kCaretColor = SkColors::kBlack;
constexpr int kCaretWidth = 1;

}  // namespace

PdfCaret::PdfCaret(PdfCaretClient* client) : client_(client) {}

PdfCaret::~PdfCaret() = default;

void PdfCaret::SetVisibility(bool is_visible) {
  if (is_visible_ == is_visible) {
    return;
  }

  is_visible_ = is_visible;
  // TODO(crbug.com/427242881): Determine the starting position of the caret.
  if (is_visible && page_index_ == -1 && char_index_ == -1) {
    SetChar(PageCharacterIndex(0, 0));
  }
  RefreshDisplayState();
}

void PdfCaret::SetBlinkInterval(base::TimeDelta interval) {
  if (interval.is_negative()) {
    return;
  }

  blink_interval_ = interval;
  RefreshDisplayState();
}

bool PdfCaret::MaybeDrawCaret(const RegionData& region,
                              const gfx::Rect& dirty_in_screen) const {
  if (!is_blink_visible_) {
    return false;
  }

  gfx::Rect visible_caret =
      gfx::IntersectRects(caret_screen_rect_, dirty_in_screen);
  if (visible_caret.IsEmpty()) {
    return false;
  }

  visible_caret.Offset(-dirty_in_screen.OffsetFromOrigin());
  Draw(region, visible_caret);
  return true;
}

void PdfCaret::OnGeometryChanged() {
  if (!is_visible_) {
    return;
  }

  caret_screen_rect_ = GetScreenRectForChar(page_index_, char_index_);
  client_->InvalidateRect(caret_screen_rect_);
}

void PdfCaret::RefreshDisplayState() {
  blink_timer_.Stop();
  if (is_visible_ && blink_interval_.is_positive()) {
    blink_timer_.Start(FROM_HERE, blink_interval_, this,
                       &PdfCaret::OnBlinkTimerFired);
  }
  is_blink_visible_ = is_visible_;
  client_->InvalidateRect(caret_screen_rect_);
}

void PdfCaret::OnBlinkTimerFired() {
  CHECK(is_visible_);
  CHECK(blink_interval_.is_positive());
  is_blink_visible_ = !is_blink_visible_;
  client_->InvalidateRect(caret_screen_rect_);
}

void PdfCaret::SetChar(const PageCharacterIndex& next_char) {
  int page_index = next_char.page_index;
  int char_index = next_char.char_index;

  int char_count = client_->GetCharCount(page_index);
  CHECK_GT(char_count, 0);

  CHECK_GE(char_index, 0);
  CHECK_LE(char_index, char_count);

  page_index_ = page_index;
  char_index_ = char_index;

  caret_screen_rect_ = GetScreenRectForChar(page_index_, char_index_);
  RefreshDisplayState();
}

gfx::Rect PdfCaret::GetScreenRectForChar(int page_index, int char_index) const {
  std::vector<gfx::Rect> screen_rects =
      client_->GetScreenRectsForChar(page_index, char_index);
  CHECK(!screen_rects.empty());

  gfx::Rect& screen_rect = screen_rects[0];
  screen_rect.set_width(kCaretWidth);
  return screen_rect;
}

void PdfCaret::Draw(const RegionData& region, const gfx::Rect& rect) const {
  int l = rect.x();
  int t = rect.y();
  int w = rect.width();
  int h = rect.height();
  for (int y = t; y < t + h; ++y) {
    base::span<uint8_t> row =
        region.buffer.subspan(y * region.stride, region.stride);
    for (int x = l; x < l + w; ++x) {
      size_t pixel_index = x * 4;
      if (pixel_index + 2 < row.size()) {
        row[pixel_index] =
            static_cast<uint8_t>(row[pixel_index] * kCaretColor.fB);
        row[pixel_index + 1] =
            static_cast<uint8_t>(row[pixel_index + 1] * kCaretColor.fG);
        row[pixel_index + 2] =
            static_cast<uint8_t>(row[pixel_index + 2] * kCaretColor.fR);
      }
    }
  }
}

}  // namespace chrome_pdf
