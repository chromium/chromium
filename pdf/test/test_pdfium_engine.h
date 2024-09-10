// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_TEST_TEST_PDFIUM_ENGINE_H_
#define PDF_TEST_TEST_PDFIUM_ENGINE_H_

#include <stdint.h>

#include <vector>

#include "base/values.h"
#include "pdf/buildflags.h"
#include "pdf/document_attachment_info.h"
#include "pdf/document_metadata.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chrome_pdf {

class TestPDFiumEngine : public PDFiumEngine {
 public:
  // Page number.
  static constexpr uint32_t kPageNumber = 13u;

  // Dummy loaded data.
  static constexpr uint8_t kLoadedData[] = {'l', 'o', 'a', 'd', 'e', 'd'};

  // Dummy save data.
  static constexpr uint8_t kSaveData[] = {'s', 'a', 'v', 'e'};

  explicit TestPDFiumEngine(PDFiumEngineClient* client);

  TestPDFiumEngine(const TestPDFiumEngine&) = delete;

  TestPDFiumEngine& operator=(const TestPDFiumEngine&) = delete;

  ~TestPDFiumEngine() override;

  MOCK_METHOD(void, PageOffsetUpdated, (const gfx::Vector2d&), (override));

  MOCK_METHOD(void, PluginSizeUpdated, (const gfx::Size&), (override));

  MOCK_METHOD(void, ScrolledToXPosition, (int), (override));
  MOCK_METHOD(void, ScrolledToYPosition, (int), (override));

  MOCK_METHOD(void,
              Paint,
              (const gfx::Rect&,
               SkBitmap&,
               std::vector<gfx::Rect>&,
               std::vector<gfx::Rect>&),
              (override));

  MOCK_METHOD(bool,
              HandleInputEvent,
              (const blink::WebInputEvent&),
              (override));

  MOCK_METHOD(std::vector<uint8_t>,
              PrintPages,
              (const std::vector<int>&, const blink::WebPrintParams&),
              (override));

  MOCK_METHOD(void, ZoomUpdated, (double), (override));

  MOCK_METHOD(gfx::Size,
              ApplyDocumentLayout,
              (const DocumentLayout::Options&),
              (override));

  MOCK_METHOD(bool, CanEditText, (), (const override));

  MOCK_METHOD(bool, HasPermission, (DocumentPermission), (const override));

  MOCK_METHOD(void, SelectAll, (), (override));

  const std::vector<DocumentAttachmentInfo>& GetDocumentAttachmentInfoList()
      const override;

  const DocumentMetadata& GetDocumentMetadata() const override;

  int GetNumberOfPages() const override;

  MOCK_METHOD(bool, IsPageVisible, (int), (const override));

  MOCK_METHOD(gfx::Rect, GetPageContentsRect, (int), (override));

  MOCK_METHOD(gfx::Rect, GetPageScreenRect, (int), (const override));

  // Returns an empty bookmark list.
  base::Value::List GetBookmarks() override;

  MOCK_METHOD(void, SetGrayscale, (bool), (override));

  uint32_t GetLoadedByteSize() override;

  bool ReadLoadedBytes(uint32_t length, void* buffer) override;

#if BUILDFLAG(ENABLE_PDF_INK2)
  MOCK_METHOD(gfx::Size, GetThumbnailSize, (int, float), (override));
#endif

  std::vector<uint8_t> GetSaveData() override;

  MOCK_METHOD(void, SetCaretPosition, (const gfx::Point&), (override));

  MOCK_METHOD(void, OnDocumentCanceled, (), (override));

  MOCK_METHOD(void, SetFormHighlight, (bool), (override));

  MOCK_METHOD(void, ClearTextSelection, (), (override));

 protected:
  std::vector<DocumentAttachmentInfo>& doc_attachment_info_list() {
    return doc_attachment_info_list_;
  }

  DocumentMetadata& metadata() { return metadata_; }

 private:
  std::vector<DocumentAttachmentInfo> doc_attachment_info_list_;

  DocumentMetadata metadata_;
};

}  // namespace chrome_pdf

#endif  // PDF_TEST_TEST_PDFIUM_ENGINE_H_
