// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_CARET_CLIENT_H_
#define PDF_PDF_CARET_CLIENT_H_

#include <stdint.h>

#include <vector>

#include "ui/gfx/geometry/rect.h"

namespace chrome_pdf {

struct PageCharacterIndex;

class PdfCaretClient {
 public:
  virtual ~PdfCaretClient() = default;

  // Returns the char count of the given page. `page_index` must be a valid page
  // index, otherwise crashes.
  virtual uint32_t GetCharCount(uint32_t page_index) const = 0;

  // Gets the screen rects for the given char. `index` must be a valid char on a
  // page, otherwise crashes.
  virtual std::vector<gfx::Rect> GetScreenRectsForChar(
      const PageCharacterIndex& index) const = 0;

  // Notifies the client to invalidate `rect` for the caret.
  virtual void InvalidateRect(const gfx::Rect& rect) {}
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_CARET_CLIENT_H_
