// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/loader/data_document_loader.h"

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/notreached.h"
#include "pdf/loader/url_loader_wrapper.h"

namespace chrome_pdf {

DataDocumentLoader::DataDocumentLoader(base::span<const uint8_t> pdf_data)
    : pdf_data_(pdf_data) {}

DataDocumentLoader::~DataDocumentLoader() = default;

bool DataDocumentLoader::Init(std::unique_ptr<URLLoaderWrapper> loader,
                              const std::string& url) {
  NOTREACHED() << "PDFiumDocument doesn't call this";
}

bool DataDocumentLoader::GetBlock(uint32_t position,
                                  base::span<uint8_t> buf) const {
  if (!IsDataAvailable(position, buf.size())) {
    return false;
  }
  buf.copy_from(pdf_data_.subspan(position, buf.size()));
  return true;
}

bool DataDocumentLoader::IsDataAvailable(uint32_t position,
                                         uint32_t size) const {
  CHECK_LE(position, GetDocumentSize());
  CHECK_LE(size, GetDocumentSize() - position);
  return true;
}

void DataDocumentLoader::RequestData(uint32_t position, uint32_t size) {}

bool DataDocumentLoader::IsDocumentComplete() const {
  return true;
}

uint32_t DataDocumentLoader::GetDocumentSize() const {
  return pdf_data_.size();
}

uint32_t DataDocumentLoader::BytesReceived() const {
  return pdf_data_.size();
}

void DataDocumentLoader::ClearPendingRequests() {}

}  // namespace chrome_pdf
