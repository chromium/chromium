// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_TEST_TEST_PDFIUM_ENGINE_H_
#define PDF_TEST_TEST_PDFIUM_ENGINE_H_

#include <stdint.h>

#include <vector>

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "pdf/document_attachment_info.h"
#include "pdf/document_metadata.h"
#include "pdf/pdf_engine.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chrome_pdf {

class TestPDFiumEngine : public PDFiumEngine {
 public:
  // Page number.
  static constexpr uint32_t kPageNumber = 13u;

  // Dummy data to save.
  static constexpr uint8_t kSaveData[] = {'1', '2', '3'};

  explicit TestPDFiumEngine(PDFEngine::Client* client);

  TestPDFiumEngine(const TestPDFiumEngine&) = delete;

  TestPDFiumEngine& operator=(const TestPDFiumEngine&) = delete;

  ~TestPDFiumEngine() override;

  MOCK_METHOD(void, PageOffsetUpdated, (const gfx::Vector2d&), (override));

  MOCK_METHOD(void, PluginSizeUpdated, (const gfx::Size&), (override));

  MOCK_METHOD(void, ScrolledToXPosition, (int), (override));
  MOCK_METHOD(void, ScrolledToYPosition, (int), (override));

  MOCK_METHOD(bool,
              HandleInputEvent,
              (const blink::WebInputEvent&),
              (override));

  MOCK_METHOD(std::vector<uint8_t>,
              PrintPages,
              (const std::vector<int>& page_numbers,
               const blink::WebPrintParams& print_params),
              (override));

  MOCK_METHOD(void, ZoomUpdated, (double), (override));

  MOCK_METHOD(gfx::Size,
              ApplyDocumentLayout,
              (const DocumentLayout::Options&),
              (override));

  bool HasPermission(DocumentPermission permission) const override;

  const std::vector<DocumentAttachmentInfo>& GetDocumentAttachmentInfoList()
      const override;

  const DocumentMetadata& GetDocumentMetadata() const override;

  int GetNumberOfPages() const override;

  // Returns an empty bookmark list.
  base::Value::List GetBookmarks() override;

  MOCK_METHOD(void, SetGrayscale, (bool), (override));

  uint32_t GetLoadedByteSize() override;

  bool ReadLoadedBytes(uint32_t length, void* buffer) override;

  std::vector<uint8_t> GetSaveData() override;

  MOCK_METHOD(void, SetCaretPosition, (const gfx::Point&), (override));

  void SetPermissions(const std::vector<DocumentPermission>& permissions);

 protected:
  std::vector<DocumentAttachmentInfo>& doc_attachment_info_list() {
    return doc_attachment_info_list_;
  }

  DocumentMetadata& metadata() { return metadata_; }

 private:
  std::vector<DocumentAttachmentInfo> doc_attachment_info_list_;

  DocumentMetadata metadata_;

  base::flat_set<DocumentPermission> permissions_;
};

}  // namespace chrome_pdf

#endif  // PDF_TEST_TEST_PDFIUM_ENGINE_H_
