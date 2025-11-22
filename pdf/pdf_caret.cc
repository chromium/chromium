// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_caret.h"

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "pdf/accessibility_structs.h"
#include "pdf/page_character_index.h"
#include "pdf/page_orientation.h"
#include "pdf/pdf_caret_client.h"
#include "pdf/region_data.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace chrome_pdf {

namespace {

constexpr SkColor4f kCaretColor = SkColors::kBlack;

// `is_same_char` is true when the actual char's screen rect is being used,
// otherwise false if a different char's screen rect is. A different char's
// screen rect can be used if the actual char does not have a screen rect.
void TransformCaretScreenRectWithRotatedTextDirection(
    gfx::Rect& screen_rect,
    AccessibilityTextDirection rotated_direction,
    bool is_same_char) {
  CHECK_NE(rotated_direction, AccessibilityTextDirection::kNone);

  // Apply an offset if the caret should be at the end of a char. For forward
  // directions, this is necessary when using the previous char's screen rect.
  // For backward directions, this is necessary when using the current char's
  // screen rect.
  const bool is_forward_direction =
      rotated_direction == AccessibilityTextDirection::kLeftToRight ||
      rotated_direction == AccessibilityTextDirection::kTopToBottom;
  const bool needs_offset = is_forward_direction != is_same_char;

  if (rotated_direction == AccessibilityTextDirection::kLeftToRight ||
      rotated_direction == AccessibilityTextDirection::kRightToLeft) {
    if (needs_offset) {
      screen_rect.Offset(screen_rect.width(), 0);
    }
    screen_rect.set_width(PdfCaret::kCaretWidth);
  } else {
    if (needs_offset) {
      screen_rect.Offset(0, screen_rect.height());
    }
    screen_rect.set_height(PdfCaret::kCaretWidth);
  }
}

// Helper for `PdfCaret::GetLogicalKeyAfterTextDirection()` to return the
// converted key.
ui::KeyboardCode GetLogicalKey(int key,
                               ui::KeyboardCode left_key,
                               ui::KeyboardCode right_key,
                               ui::KeyboardCode up_key,
                               ui::KeyboardCode down_key) {
  switch (key) {
    case ui::KeyboardCode::VKEY_LEFT:
      return left_key;
    case ui::KeyboardCode::VKEY_RIGHT:
      return right_key;
    case ui::KeyboardCode::VKEY_UP:
      return up_key;
    case ui::KeyboardCode::VKEY_DOWN:
      return down_key;
    default:
      NOTREACHED();
  }
}

}  // namespace

PdfCaret::PdfCaret(PdfCaretClient* client) : client_(client) {}

PdfCaret::~PdfCaret() = default;

void PdfCaret::SetEnabled(bool enabled) {
  if (enabled_ == enabled) {
    return;
  }

  enabled_ = enabled;
  if (ShouldDrawCaret()) {
    SetScreenRectForCurrentCaret();
  }
  RefreshDisplayState();
}

void PdfCaret::SetVisible(bool visible) {
  if (is_visible_ == visible) {
    return;
  }

  is_visible_ = visible;
  if (ShouldDrawCaret()) {
    SetScreenRectForCurrentCaret();
  }
  RefreshDisplayState();
}

void PdfCaret::SetBlinkInterval(base::TimeDelta interval) {
  if (interval.is_negative() || blink_interval_ == interval) {
    return;
  }

  blink_interval_ = interval;
  RefreshDisplayState();
}

void PdfCaret::SetChar(const PageCharacterIndex& next_char) {
  uint32_t char_count = client_->GetCharCount(next_char.page_index);
  CHECK_LE(next_char.char_index, char_count);

  index_ = next_char;

  const gfx::Rect old_screen_rect = caret_screen_rect_;
  SetScreenRectForCurrentCaret();
  if (is_blink_visible_ && !old_screen_rect.IsEmpty() &&
      old_screen_rect != caret_screen_rect_) {
    client_->InvalidateRect(old_screen_rect);
  }
}

void PdfCaret::SetCharAndDraw(const PageCharacterIndex& next_char) {
  SetChar(next_char);
  if (ShouldDrawCaret()) {
    RefreshDisplayState();
  }
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
  if (!ShouldDrawCaret()) {
    return;
  }
  SetScreenRectForCurrentCaret();
  if (!caret_screen_rect_.IsEmpty()) {
    client_->InvalidateRect(caret_screen_rect_);
  }
}

bool PdfCaret::WillHandleKeyDownEvent(const blink::WebKeyboardEvent& event) {
  // The caret is not visible during text selection, so key events should still
  // be handled when not visible.
  return enabled_ && (event.windows_key_code == ui::KeyboardCode::VKEY_LEFT ||
                      event.windows_key_code == ui::KeyboardCode::VKEY_RIGHT ||
                      event.windows_key_code == ui::KeyboardCode::VKEY_UP ||
                      event.windows_key_code == ui::KeyboardCode::VKEY_DOWN);
}

bool PdfCaret::OnKeyDown(const blink::WebKeyboardEvent& event) {
  if (!WillHandleKeyDownEvent(event)) {
    return false;
  }

  ui::KeyboardCode key = GetLogicalKeyAfterTextDirection(
      static_cast<ui::KeyboardCode>(event.windows_key_code));
  bool should_select =
      !!(event.GetModifiers() & blink::WebInputEvent::Modifiers::kShiftKey);
  switch (key) {
    case ui::KeyboardCode::VKEY_LEFT:
      MoveHorizontallyToNextChar(/*move_right=*/false, should_select);
      return true;
    case ui::KeyboardCode::VKEY_RIGHT:
      MoveHorizontallyToNextChar(/*move_right=*/true, should_select);
      return true;
    case ui::KeyboardCode::VKEY_UP:
      MoveVerticallyToNextChar(/*move_down=*/false, should_select);
      return true;
    case ui::KeyboardCode::VKEY_DOWN:
      MoveVerticallyToNextChar(/*move_down=*/true, should_select);
      return true;
    default:
      NOTREACHED();
  }
}

bool PdfCaret::ShouldDrawCaret() const {
  return enabled_ && is_visible_;
}

void PdfCaret::RefreshDisplayState() {
  blink_timer_.Stop();
  is_blink_visible_ = ShouldDrawCaret();
  if (is_blink_visible_ && blink_interval_.is_positive()) {
    blink_timer_.Start(FROM_HERE, blink_interval_, this,
                       &PdfCaret::OnBlinkTimerFired);
  }
  if (!caret_screen_rect_.IsEmpty()) {
    client_->InvalidateRect(caret_screen_rect_);
  }
}

void PdfCaret::OnBlinkTimerFired() {
  CHECK(ShouldDrawCaret());
  CHECK(blink_interval_.is_positive());
  is_blink_visible_ = !is_blink_visible_;
  if (!caret_screen_rect_.IsEmpty()) {
    client_->InvalidateRect(caret_screen_rect_);
  }
}

void PdfCaret::SetScreenRectForCurrentCaret() {
  CaretScreenRectData data = GetScreenRectForCaret(index_);
  caret_screen_rect_ = data.screen_rect;
  cached_screen_rect_index_ = data.actual_index;
}

PdfCaret::CaretScreenRectData PdfCaret::GetScreenRectForCaret(
    const PageCharacterIndex& index) const {
  gfx::Rect screen_rect;
  PageCharacterIndex curr_index = index;

  do {
    screen_rect = GetScreenRectForChar(curr_index);
    if (!screen_rect.IsEmpty()) {
      break;
    }

    // Failed to find a screen rect at the start of the page.
    if (curr_index.char_index == 0) {
      return {screen_rect, curr_index};
    }

    // Synthetic whitespaces and newlines generated by PDFium may not have a
    // screen rect. Find the nearest previous char that has a screen rect.
    --curr_index.char_index;
  } while (true);

  CHECK(!screen_rect.IsEmpty());
  TransformCaretScreenRectWithRotatedTextDirection(
      screen_rect, GetTextDirectionAfterRotationAt(curr_index),
      /*is_same_char=*/index.char_index == curr_index.char_index);
  return {screen_rect, curr_index};
}

gfx::Rect PdfCaret::GetScreenRectForChar(
    const PageCharacterIndex& index) const {
  uint32_t char_count = client_->GetCharCount(index.page_index);
  CHECK_LE(index.char_index, char_count);
  if (char_count > 0 && index.char_index == char_count) {
    return gfx::Rect();
  }

  const std::vector<gfx::Rect> screen_rects =
      client_->GetScreenRectsForCaret(index);
  return !screen_rects.empty() ? screen_rects[0] : gfx::Rect();
}

AccessibilityTextDirection PdfCaret::GetTextDirectionAt(
    const PageCharacterIndex& index) const {
  std::optional<AccessibilityTextRunInfo> text_run =
      client_->GetTextRunInfoAt(index);
  auto direction = AccessibilityTextDirection::kLeftToRight;
  if (text_run.has_value()) {
    direction = text_run.value().direction;
  }
  // Default to LTR.
  return direction == AccessibilityTextDirection::kNone
             ? AccessibilityTextDirection::kLeftToRight
             : direction;
}

AccessibilityTextDirection PdfCaret::GetTextDirectionAfterRotationAt(
    const PageCharacterIndex& index) const {
  AccessibilityTextDirection direction = GetTextDirectionAt(index);

  int rotation_steps =
      GetClockwiseRotationSteps(client_->GetCurrentOrientation());
  if (rotation_steps == 0) {
    return direction;
  }

  // Order of text directions when rotating clockwise.
  static constexpr std::array<AccessibilityTextDirection, 4> rotation_cycle = {
      AccessibilityTextDirection::kLeftToRight,
      AccessibilityTextDirection::kTopToBottom,
      AccessibilityTextDirection::kRightToLeft,
      AccessibilityTextDirection::kBottomToTop};

  // Find the position of the current direction in the cycle.
  auto it = std::ranges::find(rotation_cycle, direction);
  CHECK(it != rotation_cycle.end());
  size_t current_index = std::distance(rotation_cycle.begin(), it);

  // Calculate the new direction after rotation.
  size_t new_index = (current_index + rotation_steps) % rotation_cycle.size();
  return rotation_cycle[new_index];
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

  if (!first_visible_) {
    base::UmaHistogramBoolean("PDF.Caret.FirstVisible", true);
    first_visible_ = true;
  }
}

void PdfCaret::MoveToChar(const PageCharacterIndex& new_index,
                          bool should_select) {
  if (!should_select) {
    client_->ClearTextSelection();
    SetVisible(true);
  }

  if (index_ == new_index) {
    return;
  }

  if (!should_select || (!client_->IsSelecting() &&
                         !StartSelection(/*move_right=*/index_ < new_index))) {
    SetCharAndDraw(new_index);
  } else {
    ExtendSelection(new_index);
    SetChar(new_index);
  }

  if (!caret_screen_rect_.IsEmpty()) {
    client_->ScrollToChar(cached_screen_rect_index_);
  }
}

ui::KeyboardCode PdfCaret::GetLogicalKeyAfterTextDirection(
    ui::KeyboardCode key) const {
  switch (GetTextDirectionAt(index_)) {
    case AccessibilityTextDirection::kLeftToRight:
      return GetLogicalKey(key,
                           /*left_key=*/ui::KeyboardCode::VKEY_LEFT,
                           /*right_key=*/ui::KeyboardCode::VKEY_RIGHT,
                           /*up_key=*/ui::KeyboardCode::VKEY_UP,
                           /*down_key=*/ui::KeyboardCode::VKEY_DOWN);
    case AccessibilityTextDirection::kRightToLeft:
      return GetLogicalKey(key,
                           /*left_key=*/ui::KeyboardCode::VKEY_RIGHT,
                           /*right_key=*/ui::KeyboardCode::VKEY_LEFT,
                           /*up_key=*/ui::KeyboardCode::VKEY_UP,
                           /*down_key=*/ui::KeyboardCode::VKEY_DOWN);
    case AccessibilityTextDirection::kTopToBottom:
      return GetLogicalKey(key,
                           /*left_key=*/ui::KeyboardCode::VKEY_DOWN,
                           /*right_key=*/ui::KeyboardCode::VKEY_UP,
                           /*up_key=*/ui::KeyboardCode::VKEY_LEFT,
                           /*down_key=*/ui::KeyboardCode::VKEY_RIGHT);
    case AccessibilityTextDirection::kBottomToTop:
      return GetLogicalKey(key,
                           /*left_key=*/ui::KeyboardCode::VKEY_DOWN,
                           /*right_key=*/ui::KeyboardCode::VKEY_UP,
                           /*up_key=*/ui::KeyboardCode::VKEY_RIGHT,
                           /*down_key=*/ui::KeyboardCode::VKEY_LEFT);
    default:
      NOTREACHED();
  }
}

void PdfCaret::MoveHorizontallyToNextChar(bool move_right, bool should_select) {
  std::optional<PageCharacterIndex> next_char =
      GetAdjacentCaretPos(index_, move_right);
  if (next_char.has_value()) {
    MoveToChar(next_char.value(), should_select);
  }
}

void PdfCaret::MoveVerticallyToNextChar(bool move_down, bool should_select) {
  // Find the next text line by getting the next two sets of newlines that
  // border the text line.

  // Newlines are considered part of the current line. When moving up and on a
  // newline, skip it so that `GetNextNewlineOnPage()` does not use the current
  // newline.
  PageCharacterIndex start_index = index_;
  if (!move_down) {
    start_index = GetNextNonNewlineOnPage(index_, /*move_right=*/false)
                      .value_or(start_index);
  }

  // Search for the first newline.
  std::optional<PageCharacterIndex> first_newline =
      GetNextNewlineOnPage(start_index, move_down);
  uint32_t page_index = index_.page_index;
  const int delta = move_down ? 1 : -1;
  if (!first_newline.has_value()) {
    // If there is no newline, then there is no next line on the current page.
    page_index += delta;
    if (!client_->PageIndexInBounds(page_index)) {
      // There is no page in the `move_down` direction. Stay at the end of the
      // page.
      const PageCharacterIndex end_index = {
          index_.page_index,
          move_down ? client_->GetCharCount(index_.page_index) : 0};
      MoveToChar(end_index, should_select);
      return;
    }
    // Start at the beginning or end of the adjacent page.
    first_newline = {page_index,
                     move_down ? 0 : client_->GetCharCount(page_index)};
  } else {
    // Check for consecutive newlines and skip one of them. There cannot be more
    // than two consecutive newlines.
    //
    // Synthetic newlines cannot be the first or last char on a page.
    CHECK(!WillCaretExitPage(first_newline.value(), /*move_right=*/false));

    PageCharacterIndex adjacent_char = first_newline.value();
    adjacent_char.char_index += delta;
    if (client_->IsSynthesizedNewline(adjacent_char)) {
      first_newline = adjacent_char;
    }
  }

  // Search for the second newline. Start on the adjacent non-newline char.
  start_index = GetNextNonNewlineOnPage(first_newline.value(), move_down)
                    .value_or(first_newline.value());
  std::optional<PageCharacterIndex> second_newline =
      GetNextNewlineOnPage(start_index, move_down);
  if (!second_newline.has_value()) {
    // If there is no newline, then the line starts or ends at one end of the
    // page.
    second_newline = {start_index.page_index,
                      move_down ? client_->GetCharCount(page_index) : 0};
  }

  // When moving up, `first_newline` is after the line and `second_newline` is
  // before the line. Swap them.
  if (first_newline.value().char_index > second_newline.value().char_index) {
    std::swap(first_newline, second_newline);
  }
  MoveToChar(
      GetClosestCharInTextLine(first_newline.value(), second_newline.value()),
      should_select);
}

bool PdfCaret::StartSelection(bool move_right) const {
  if (client_->GetCharCount(index_.page_index) != 0) {
    client_->StartSelection(index_);
    return true;
  }

  // Avoid starting a selection on a no-text page by starting on the adjacent
  // caret position.
  // `GetAdjacentCaretPos()` will never return std::nullopt because the caret
  // should always be moving.
  PageCharacterIndex adjacent_caret_pos =
      GetAdjacentCaretPos(index_, move_right).value();
  if (client_->GetCharCount(adjacent_caret_pos.page_index) != 0) {
    client_->StartSelection(adjacent_caret_pos);
    return true;
  }

  return false;
}

void PdfCaret::ExtendSelection(const PageCharacterIndex& new_index) const {
  if (client_->GetCharCount(new_index.page_index) != 0) {
    client_->ExtendAndInvalidateSelectionByChar(new_index);
    return;
  }

  uint32_t char_count = client_->GetCharCount(index_.page_index);
  if (char_count == 0) {
    // Moving from a no-text page to another no-text page. Do nothing.
    return;
  }

  // When moving to a no-text page, select the remaining text on the original
  // page.
  const bool move_right = index_ < new_index;
  PageCharacterIndex end_index = {index_.page_index,
                                  move_right ? char_count : 0};
  if (end_index == index_) {
    // `end_index` is already part of the selection.
    return;
  }
  client_->ExtendAndInvalidateSelectionByChar(end_index);
}

bool PdfCaret::WillCaretExitPage(const PageCharacterIndex& index,
                                 bool move_right) const {
  if (move_right) {
    return index.char_index == client_->GetCharCount(index.page_index);
  }
  return index.char_index == 0;
}

bool PdfCaret::IndexHasChar(const PageCharacterIndex& index) const {
  return index.char_index < client_->GetCharCount(index.page_index);
}

bool PdfCaret::IsSynthesizedNewline(const PageCharacterIndex& index) const {
  return IndexHasChar(index) && client_->IsSynthesizedNewline(index);
}

std::optional<PageCharacterIndex> PdfCaret::GetAdjacentCaretPos(
    const PageCharacterIndex& index,
    bool move_right) const {
  if (!WillCaretExitPage(index, move_right)) {
    const int delta = move_right ? 1 : -1;
    PageCharacterIndex next_char = {index.page_index, index.char_index + delta};
    // Newlines synthetically created by PDFium have empty screen rects.
    // Skip consecutive newlines.
    if (IsSynthesizedNewline(index) && IsSynthesizedNewline(next_char)) {
      // Synthetic newlines cannot be the first or last char on a page.
      CHECK(!WillCaretExitPage(next_char, move_right));

      // There cannot be more than two consecutive synthetic newlines.
      next_char.char_index += delta;
    }
    return next_char;
  }

  uint32_t page_index = index.page_index;

  // If `move_right` is true, move one page to the right if possible.
  if (move_right) {
    ++page_index;
    if (!client_->PageIndexInBounds(page_index)) {
      // There is no next page. Stay at current position.
      return std::nullopt;
    }
    return PageCharacterIndex(page_index, 0);
  }

  // Otherwise, move one page to the left if possible.
  if (page_index == 0) {
    // There is no previous page. Stay at current position.
    return std::nullopt;
  }

  --page_index;
  return PageCharacterIndex(page_index, client_->GetCharCount(page_index));
}

std::optional<PageCharacterIndex> PdfCaret::GetNextNonNewlineOnPage(
    const PageCharacterIndex& index,
    bool move_right) const {
  PageCharacterIndex curr_index = index;
  const int delta = move_right ? 1 : -1;
  while (!WillCaretExitPage(curr_index, move_right) &&
         IsSynthesizedNewline(curr_index)) {
    curr_index.char_index += delta;
  }

  if (curr_index == index) {
    return std::nullopt;
  }

  return curr_index;
}

std::optional<PageCharacterIndex> PdfCaret::GetNextNewlineOnPage(
    const PageCharacterIndex& index,
    bool move_right) const {
  PageCharacterIndex curr_index = index;
  const int delta = move_right ? 1 : -1;
  while (!WillCaretExitPage(curr_index, move_right) &&
         !IsSynthesizedNewline(curr_index)) {
    curr_index.char_index += delta;
  }

  if (WillCaretExitPage(curr_index, move_right)) {
    // Could not find a newline.
    return std::nullopt;
  }

  return curr_index;
}

PageCharacterIndex PdfCaret::GetClosestCharInTextLine(
    const PageCharacterIndex& start_newline,
    const PageCharacterIndex& end_newline) const {
  const uint32_t page_index = start_newline.page_index;
  CHECK_EQ(page_index, end_newline.page_index);

  // The start newline is not part of the text line, so skip to the right.
  PageCharacterIndex line_start =
      GetNextNonNewlineOnPage(start_newline, /*move_right=*/true)
          .value_or(start_newline);

  const gfx::Point caret_center = caret_screen_rect_.CenterPoint();

  // The property of distances of characters in a line from a point is unimodal
  // (it decreases to a minimum, then increases). A binary search that compares
  // mid and mid+1 can find this minimum efficiently.

  // Cache the results of `GetScreenRectForCaret()` to avoid redundant calls.
  base::flat_map<uint32_t, uint64_t> distances;
  auto get_cached_distance = [&](uint32_t char_index) {
    if (!distances.contains(char_index)) {
      gfx::Rect screen_rect =
          GetScreenRectForCaret({page_index, char_index}).screen_rect;
      gfx::Vector2d distance_vector = caret_center - screen_rect.CenterPoint();
      // Just need to compare relative lengths. Length squared is cheaper to
      // compute.
      distances[char_index] = distance_vector.LengthSquared();
    }
    return distances[char_index];
  };

  uint32_t low_char_index = line_start.char_index;
  uint32_t high_char_index = end_newline.char_index;
  while (low_char_index < high_char_index) {
    uint32_t mid_char_index =
        low_char_index + (high_char_index - low_char_index) / 2;
    int64_t mid_distance = get_cached_distance(mid_char_index);
    int64_t mid_right_distance = get_cached_distance(mid_char_index + 1);

    if (mid_distance < mid_right_distance) {
      // The closest character is in the left half, including mid.
      high_char_index = mid_char_index;
    } else {
      // The closest character is in the right half, not including mid.
      low_char_index = mid_char_index + 1;
    }
  }

  return {page_index, low_char_index};
}

}  // namespace chrome_pdf
