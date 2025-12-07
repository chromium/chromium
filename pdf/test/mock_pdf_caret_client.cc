// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/test/mock_pdf_caret_client.h"

#include "ui/gfx/geometry/rect.h"

namespace chrome_pdf {

MockPdfCaretClient::MockPdfCaretClient() = default;

MockPdfCaretClient::~MockPdfCaretClient() = default;

void MockPdfCaretClient::InvalidateRect(const gfx::Rect& rect) {
  invalidated_rect_ = rect;
}

}  // namespace chrome_pdf
