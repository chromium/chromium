// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/test/test_pdfium_engine.h"

#include <stdint.h>
#include <string.h>

#include <iterator>
#include <vector>

#include "base/check_op.h"
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
    : PDFiumEngine(client, PDFiumFormFiller::ScriptOption::kNoJavaScript) {}

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

uint32_t TestPDFiumEngine::GetLoadedByteSize() {
  return sizeof(kLoadedData);
}

bool TestPDFiumEngine::ReadLoadedBytes(uint32_t length, void* buffer) {
  DCHECK_LE(length, GetLoadedByteSize());
  memcpy(buffer, kLoadedData, length);
  return true;
}

std::vector<uint8_t> TestPDFiumEngine::GetSaveData() {
  return std::vector<uint8_t>(std::begin(kSaveData), std::end(kSaveData));
}

}  // namespace chrome_pdf
