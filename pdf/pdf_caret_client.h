// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_CARET_CLIENT_H_
#define PDF_PDF_CARET_CLIENT_H_

#include <vector>

#include "ui/gfx/geometry/rect.h"

namespace chrome_pdf {

class PdfCaretClient {
 public:
  virtual ~PdfCaretClient() = default;

  // Returns the char count of the given page. `page_index` must be a valid page
  // index, otherwise crashes.
  virtual int GetCharCount(int page_index) const = 0;

  // Gets the screen rects for the given char. `page_index` must be a valid page
  // index and `char_index` must be in bounds, otherwise crashes.
  virtual std::vector<gfx::Rect> GetScreenRectsForChar(
      int page_index,
      int char_index) const = 0;

  // Notifies the client to invalidate `rect` for the caret.
  virtual void InvalidateRect(const gfx::Rect& rect) {}
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_CARET_CLIENT_H_
