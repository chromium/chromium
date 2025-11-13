// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_TEST_MOCK_PDF_CARET_CLIENT_H_
#define PDF_TEST_MOCK_PDF_CARET_CLIENT_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "pdf/accessibility_structs.h"
#include "pdf/page_orientation.h"
#include "pdf/pdf_caret_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/rect.h"

namespace chrome_pdf {

class MockPdfCaretClient : public PdfCaretClient {
 public:
  MockPdfCaretClient();
  MockPdfCaretClient(const MockPdfCaretClient&) = delete;
  MockPdfCaretClient& operator=(const MockPdfCaretClient&) = delete;
  ~MockPdfCaretClient() override;

  const gfx::Rect& invalidated_rect() const { return invalidated_rect_; }

  // PdfCaretClient:
  MOCK_METHOD(void, ClearTextSelection, (), (override));

  MOCK_METHOD(void,
              ExtendAndInvalidateSelectionByChar,
              (const PageCharacterIndex& index),
              (override));

  MOCK_METHOD(uint32_t, GetCharCount, (uint32_t page_index), (const override));

  MOCK_METHOD(PageOrientation, GetCurrentOrientation, (), (const override));

  MOCK_METHOD(std::vector<gfx::Rect>,
              GetScreenRectsForCaret,
              (const PageCharacterIndex& index),
              (const override));

  MOCK_METHOD(std::optional<AccessibilityTextRunInfo>,
              GetTextRunInfoAt,
              (const PageCharacterIndex& index),
              (const override));

  void InvalidateRect(const gfx::Rect& rect) override;

  MOCK_METHOD(bool, IsSelecting, (), (const override));

  MOCK_METHOD(bool,
              IsSynthesizedNewline,
              (const PageCharacterIndex& index),
              (const override));

  MOCK_METHOD(bool, PageIndexInBounds, (int index), (const override));

  MOCK_METHOD(void,
              ScrollToChar,
              (const PageCharacterIndex& index),
              (override));

  MOCK_METHOD(void,
              StartSelection,
              (const PageCharacterIndex& index),
              (override));

 private:
  gfx::Rect invalidated_rect_;
};

}  // namespace chrome_pdf

#endif  // PDF_TEST_MOCK_PDF_CARET_CLIENT_H_
