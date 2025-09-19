// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_CARET_H_
#define PDF_PDF_CARET_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "pdf/page_character_index.h"
#include "pdf/pdf_caret_client.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {
class WebKeyboardEvent;
}

namespace chrome_pdf {

struct RegionData;

// Manages the text caret for text selection and navigation within a PDF. This
// class handles caret drawing, blinking, position updates, and keyboard-driven
// movement. For now, only used if Ink2 text highlighting is enabled.
//
// When moving by lines, caret movement is based on the geometric proximity of
// characters. This works well for standard text layouts, but has limitations.
// The implementation currently assumes a top-to-bottom text flow, and may not
// behave as expected with multi-column layouts or unconventional page designs
// (e.g. text lines that are not ordered from top to bottom).
class PdfCaret {
 public:
  // The pixel width of the caret.
  static constexpr int kCaretWidth = 1;

  // The default interval the caret should blink if not set by
  // `SetBlinkInterval()`. Exposed for testing.
  static constexpr base::TimeDelta kDefaultBlinkInterval =
      base::Milliseconds(500);

  // PdfCaret should only be instantiated on a text page with chars.
  PdfCaret(PdfCaretClient* client, const PageCharacterIndex& index);
  PdfCaret(const PdfCaret&) = delete;
  PdfCaret& operator=(const PdfCaret&) = delete;
  ~PdfCaret();

  // Sets the visibility of the caret. No-op if visibility does not change. If
  // `is_visible` is true, the caret will be drawn, hidden otherwise.
  void SetVisibility(bool is_visible);

  // Sets how often the caret should blink. If the interval is set to 0, the
  // caret will not blink. No-op if `interval` is negative.
  void SetBlinkInterval(base::TimeDelta interval);

  // Sets the caret's char position and updates its screen rect. Requires a
  // page with at least one char and a valid char index (from 0 up to the page's
  // char count, inclusive), otherwise crashes.
  void SetChar(const PageCharacterIndex& next_char);

  // Draws the caret on the canvas if it is visible within any paint updates in
  // `dirty_in_screen`. Returns true if the caret was drawn, false otherwise.
  bool MaybeDrawCaret(const RegionData& region,
                      const gfx::Rect& dirty_in_screen) const;

  // Recalculates the caret's screen position and invalidates its area when the
  // viewport geometry changes.
  void OnGeometryChanged();

  // Handles key presses that move the caret. Returns true when the key press is
  // handled, false otherwise.
  bool OnKeyDown(const blink::WebKeyboardEvent& event);

 private:
  // Refreshes the caret's display state, drawing or hiding the caret depending
  // on the value of `is_visible_` and resetting the blink timer depending on
  // the value of `is_blinking_`.
  void RefreshDisplayState();

  // Called by `blink_timer_` to toggle caret visibility.
  void OnBlinkTimerFired();

  // Returns the screen rect for the current caret if it were placed at `index`.
  // For chars without a defined rect (like synthetic newlines), it calculates a
  // position based on the preceding char.
  gfx::Rect GetScreenRectForCaret(const PageCharacterIndex& index) const;

  // Returns the screen rect for a char, which may be empty.
  gfx::Rect GetScreenRectForChar(const PageCharacterIndex& index) const;

  // Draws `rect` as the caret on `region`.
  void Draw(const RegionData& region, const gfx::Rect& rect) const;

  // Determines the next valid char, handling moving horizontally to a char on a
  // different page and ignoring newlines. Does nothing if the current char
  // cannot move to a valid page or char.
  void MoveHorizontallyToNextChar(bool move_right);

  // Same as `MoveHorizontallyToNextChar()`, but moves in the vertical
  // direction.
  void MoveVerticallyToNextChar(bool move_down);

  // Returns whether moving the caret from `index` will cause it to exit the
  // page or not. Does not consider whether there are any adjacent pages.
  bool WillCaretExitPage(const PageCharacterIndex& index,
                         bool move_right) const;

  // Returns whether `index` is a valid char or not. False when `index` is the
  // last caret position of a page.
  bool IndexHasChar(const PageCharacterIndex& index) const;

  // Returns whether `index` is a synthesized newline or not.
  bool IsSynthesizedNewline(const PageCharacterIndex& index) const;

  // Returns the adjacent caret position to `index`, moving in the direction
  // indicated by `move_right`. Moves across pages if necessary. This can return
  // caret positions on no-text pages. Returns `std::nullopt` if no adjacent
  // position is available.
  std::optional<PageCharacterIndex> GetAdjacentCaretPos(
      const PageCharacterIndex& index,
      bool move_right) const;

  // Gets the `PageCharacterIndex` of the next non-newline char. Starts from
  // `index` and skips past consecutive newlines on a page, moving in the
  // direction specified by `move_right`. Returns `std::nullopt` if `index` is
  // already a non-newline char or no non-newline char is found.
  std::optional<PageCharacterIndex> GetNextNonNewlineOnPage(
      const PageCharacterIndex& index,
      bool move_right) const;

  // Gets the `PageCharacterIndex` of the next newline on a page. Starts from
  // `index` and moves in the direction specified by `move_right`. Returns
  // `std::nullopt` if no more newlines are found.
  std::optional<PageCharacterIndex> GetNextNewlineOnPage(
      const PageCharacterIndex& index,
      bool move_right) const;

  // Gets the `PageCharacterIndex` of the char within a single line of text,
  // bounded by `start_newline` exclusive and `end_newline` inclusive, that is
  // closest to the current caret from center to center.
  PageCharacterIndex GetClosestCharInTextLine(
      const PageCharacterIndex& start_newline,
      const PageCharacterIndex& end_newline) const;

  // Client must outlive `this`.
  const raw_ptr<PdfCaretClient> client_;

  // The current caret position.
  // The char index can be max char count on the page, since the cursor can be
  // to the right of the last char.
  PageCharacterIndex index_;

  // Whether the caret is visible.
  bool is_visible_ = false;

  // Whether the caret is visible on screen, taking into account blinking.
  bool is_blink_visible_ = false;

  // How often the caret should blink. 0 if the caret should not blink. Never
  // negative.
  base::TimeDelta blink_interval_ = kDefaultBlinkInterval;

  gfx::Rect caret_screen_rect_;

  base::RepeatingTimer blink_timer_;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_CARET_H_
