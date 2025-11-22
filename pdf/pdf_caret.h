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
#include "ui/events/keycodes/keyboard_codes.h"
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

  explicit PdfCaret(PdfCaretClient* client);
  PdfCaret(const PdfCaret&) = delete;
  PdfCaret& operator=(const PdfCaret&) = delete;
  virtual ~PdfCaret();

  bool enabled() const { return enabled_; }

  // Sets whether the caret is enabled. No-op if state does not change. Draws
  // the caret if it should be visible, hides it otherwise. See
  // `ShouldDrawCaret()` for when the caret will be visible.
  void SetEnabled(bool enabled);

  // Sets whether the caret should be visible. No-op if state does not change.
  // Draws the caret if it should be visible, hides it otherwise. Note that even
  // if `visible` is true, the caret may still be hidden if the caret is
  // disabled. This state can be desired if the caller wants the caret to be
  // visible again on re-enable. See `ShouldDrawCaret()` for when the caret will
  // be visible.
  void SetVisible(bool visible);

  // Sets how often the caret should blink. If the interval is set to 0, the
  // caret will not blink. No-op if `interval` is negative.
  void SetBlinkInterval(base::TimeDelta interval);

  // Sets the caret's char position and updates its screen rect. Invalidates the
  // old caret rect if visible but not the new caret rect. Requires a page with
  // at least one char and a valid char index (from 0 up to the page's char
  // count, inclusive), otherwise crashes. Use this over `SetCharAndDraw()` when
  // the new caret should not appear on screen (e.g. during text selection).
  void SetChar(const PageCharacterIndex& next_char);

  // Same as `SetChar()`, but also draws the caret at the new position if
  // visible. Use this over `SetChar()` when the caret should appear on screen
  // immediately.
  void SetCharAndDraw(const PageCharacterIndex& next_char);

  // Draws the caret on the canvas if it is visible within any paint updates in
  // `dirty_in_screen`. Returns true if the caret was drawn, false otherwise.
  bool MaybeDrawCaret(const RegionData& region,
                      const gfx::Rect& dirty_in_screen) const;

  // Recalculates the caret's screen position and invalidates its area when the
  // viewport geometry changes.
  void OnGeometryChanged();

  // Returns whether `OnKeyDown()` will handle `event`. Only arrow key events
  // are handled. Events are not handled if the caret is disabled.
  bool WillHandleKeyDownEvent(const blink::WebKeyboardEvent& event);

  // Handles key events that move the caret. See `WillHandleKeyDownEvent()` for
  // what key events are handled. Returns true when the key event is handled,
  // false otherwise. Virtual to support testing.
  virtual bool OnKeyDown(const blink::WebKeyboardEvent& event);

 private:
  // Return result of `GetScreenRectForCaret()`.
  struct CaretScreenRectData {
    gfx::Rect screen_rect;
    PageCharacterIndex actual_index;
  };

  // Returns whether the caret should be drawn. It should only be drawn when the
  // caret is enabled and set as visible.
  bool ShouldDrawCaret() const;

  // Refreshes the caret's display state, drawing or hiding the caret depending
  // on the value of `ShouldDrawCaret()` and resetting the blink timer depending
  // on the value of `is_blinking_`.
  void RefreshDisplayState();

  // Called by `blink_timer_` to toggle caret visibility.
  void OnBlinkTimerFired();

  // Calculates and sets `caret_screen_rect_` and `cached_screen_rect_index_`
  // using the current `index_`.
  void SetScreenRectForCurrentCaret();

  // Returns the screen rect and index for the current caret if it were placed
  // at `index`. For chars without a defined rect (like synthetic newlines), it
  // calculates a position based on the preceding char.
  CaretScreenRectData GetScreenRectForCaret(
      const PageCharacterIndex& index) const;

  // Returns the screen rect for a char, which may be empty.
  gfx::Rect GetScreenRectForChar(const PageCharacterIndex& index) const;

  // Returns the text direction of `index`.
  AccessibilityTextDirection GetTextDirectionAt(
      const PageCharacterIndex& index) const;

  // Same as `GetTextDirectionAt()`, but takes page rotations into account.
  AccessibilityTextDirection GetTextDirectionAfterRotationAt(
      const PageCharacterIndex& index) const;

  // Draws `rect` as the caret on `region`.
  void Draw(const RegionData& region, const gfx::Rect& rect) const;

  // Moves the caret to `new_index`. If `should_select` is true, then the text
  // selection will be extended to `new_index`, starting from the original caret
  // position if not yet text selecting. If `should_select` is false, text
  // selection will be cleared, and the caret will be set visible.
  void MoveToChar(const PageCharacterIndex& new_index, bool should_select);

  // Returns the arrow key converted from the `key` input after taking text
  // direction into account. E.g. if the text direction is RTL and `key` is
  // `ui::KeyboardCode::VKEY_LEFT`, the return result will be
  // `ui::KeyboardCode::VKEY_RIGHT`. `key` must be an arrow key, otherwise
  // crashes.
  ui::KeyboardCode GetLogicalKeyAfterTextDirection(ui::KeyboardCode key) const;

  // Determines the next valid char, handling moving horizontally to a char on a
  // different page and ignoring newlines. Does nothing if the current char
  // cannot move to a valid page or char.
  void MoveHorizontallyToNextChar(bool move_right, bool should_select);

  // Same as `MoveHorizontallyToNextChar()`, but moves in the vertical
  // direction.
  void MoveVerticallyToNextChar(bool move_down, bool should_select);

  // This should only be called when the caret is moving. Starts a new text
  // selection at the current caret position, adjusting the exact index
  // depending on the direction specified by `move_right`.
  bool StartSelection(bool move_right) const;

  // Extends the text selection to `new_index`. Must already be selecting text,
  // otherwise does nothing. Never extends to a non-text page. Instead, the text
  // selection will be extended to the end of the page of the original caret
  // position.
  void ExtendSelection(const PageCharacterIndex& new_index) const;

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

  // The actual char index used to determine the caret's screen rect. This can
  // differ from `index_` if `index_` points to a char without a screen rect.
  PageCharacterIndex cached_screen_rect_index_;

  // Whether the caret is enabled.
  bool enabled_ = false;

  // Whether the caret is visible.
  bool is_visible_ = false;

  // Whether the caret is visible on screen, taking into account blinking.
  bool is_blink_visible_ = false;

  // Whether the caret has been drawn on screen at least once. Only used to
  // report metrics.
  mutable bool first_visible_ = false;

  // How often the caret should blink. 0 if the caret should not blink. Never
  // negative.
  base::TimeDelta blink_interval_ = kDefaultBlinkInterval;

  gfx::Rect caret_screen_rect_;

  base::RepeatingTimer blink_timer_;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_CARET_H_
