// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_CARET_CLIENT_H_
#define PDF_PDF_CARET_CLIENT_H_

#include <stdint.h>

#include <vector>

#include "pdf/accessibility_structs.h"
#include "pdf/page_orientation.h"
#include "ui/gfx/geometry/rect.h"

namespace chrome_pdf {

struct PageCharacterIndex;

class PdfCaretClient {
 public:
  virtual ~PdfCaretClient() = default;

  // Clears the current text selection.
  virtual void ClearTextSelection() {}

  // Extends the text selection to `index` and invalidates the updated
  // selection area. Does nothing if not yet text selecting.
  virtual void ExtendAndInvalidateSelectionByChar(
      const PageCharacterIndex& index) {}

  // Returns the char count of the given page. `page_index` must be a valid page
  // index, otherwise crashes.
  virtual uint32_t GetCharCount(uint32_t page_index) const = 0;

  // Returns the current layout orientation.
  virtual PageOrientation GetCurrentOrientation() const = 0;

  // Gets the screen rects for the caret at `index`. `index` must be a valid
  // char on a page, otherwise crashes. If the page does not have any text, and
  // `index.char_index` is 0, it will return a vector with a default caret
  // screen rect at the top-left of the PDF page. If the PDF page is too small
  // to display the default caret, then the screen rect will be empty.
  virtual std::vector<gfx::Rect> GetScreenRectsForCaret(
      const PageCharacterIndex& index) const = 0;

  // Returns the text run containing `index`. If `index` is an invalid char or
  // if the page has no text, returns `std::nullopt` instead.
  virtual std::optional<AccessibilityTextRunInfo> GetTextRunInfoAt(
      const PageCharacterIndex& index) const = 0;

  // Notifies the client to invalidate `rect` for the caret.
  virtual void InvalidateRect(const gfx::Rect& rect) {}

  // Returns whether the client is selecting text containing at least one char.
  virtual bool IsSelecting() const = 0;

  // Returns whether the char at `index` is a synthesized newline (i.e. '\r' or
  // '\n'). `index` must be a valid char on a page, otherwise crashes.
  virtual bool IsSynthesizedNewline(const PageCharacterIndex& index) const = 0;

  // Returns whether `index` is a valid 0-based page index.
  virtual bool PageIndexInBounds(int index) const = 0;

  // Scrolls to `index` so it is visible. `index` must be a valid char on a
  // page, otherwise crashes. If the page does not have any text, and
  // `index.char_index` is 0, scrolls to the page instead.
  virtual void ScrollToChar(const PageCharacterIndex& index) {}

  // Starts a new text selection at `index` without selecting it. Does nothing
  // if already selecting text.
  virtual void StartSelection(const PageCharacterIndex& index) {}
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_CARET_CLIENT_H_
