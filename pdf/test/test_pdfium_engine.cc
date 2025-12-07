// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/test/test_pdfium_engine.h"

#include <stdint.h>

#include <iterator>
#include <vector>

#include "base/containers/span.h"
#include "base/values.h"
#include "pdf/document_attachment_info.h"
#include "pdf/document_metadata.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_form_filler.h"

namespace chrome_pdf {

// static
const uint32_t TestPDFiumEngine::kPageNumber;

// static
const uint8_t TestPDFiumEngine::kLoadedData[];

// static
const uint8_t TestPDFiumEngine::kSaveData[];

TestPDFiumEngine::TestPDFiumEngine(PDFiumEngineClient* client)
    : PDFiumEngine(client, PDFiumFormFiller::ScriptOption::kNoJavaScript) {
  ON_CALL(*this, GetLoadedByteSize)
      .WillByDefault(testing::Return(sizeof(kLoadedData)));

  ON_CALL(*this, GetSaveData)
      .WillByDefault(testing::Return(
          std::vector<uint8_t>(std::begin(kSaveData), std::end(kSaveData))));

  ON_CALL(*this, ReadLoadedBytes)
      .WillByDefault([](uint32_t offset, base::span<uint8_t> buffer) {
        buffer.copy_from(
            base::span(kLoadedData).subspan(offset, buffer.size()));
        return true;
      });
}

TestPDFiumEngine::~TestPDFiumEngine() = default;

const std::vector<DocumentAttachmentInfo>&
TestPDFiumEngine::GetDocumentAttachmentInfoList() const {
  return doc_attachment_info_list_;
}

const DocumentMetadata& TestPDFiumEngine::GetDocumentMetadata() const {
  return metadata_;
}

int TestPDFiumEngine::GetNumberOfPages() const {
  return static_cast<int>(kPageNumber);
}

base::Value::List TestPDFiumEngine::GetBookmarks() {
  return base::Value::List();
}

}  // namespace chrome_pdf
