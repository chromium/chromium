// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_LOADER_DATA_DOCUMENT_LOADER_H_
#define PDF_LOADER_DATA_DOCUMENT_LOADER_H_

#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "pdf/loader/document_loader.h"

namespace chrome_pdf {

// A simple loader that loads from data in memory.
class DataDocumentLoader : public DocumentLoader {
 public:
  explicit DataDocumentLoader(base::span<const uint8_t> pdf_data);
  ~DataDocumentLoader() override;

  // DocumentLoader:
  bool Init(std::unique_ptr<URLLoaderWrapper> loader,
            const std::string& url) override;
  bool GetBlock(uint32_t position, base::span<uint8_t> buf) const override;
  bool IsDataAvailable(uint32_t position, uint32_t size) const override;
  void RequestData(uint32_t position, uint32_t size) override;
  bool IsDocumentComplete() const override;
  uint32_t GetDocumentSize() const override;
  uint32_t BytesReceived() const override;
  void ClearPendingRequests() override;

 private:
  const base::raw_span<const uint8_t> pdf_data_;
};

}  // namespace chrome_pdf

#endif  // PDF_LOADER_DATA_DOCUMENT_LOADER_H_
