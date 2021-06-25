// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_TEST_TEST_DOCUMENT_LOADER_H_
#define PDF_TEST_TEST_DOCUMENT_LOADER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "pdf/document_loader.h"
#include "pdf/range_set.h"

namespace chrome_pdf {

// Loads test PDFs from pdf/test/data.
class TestDocumentLoader : public DocumentLoader {
 public:
  // |pdf_name| is the base name for a PDF file.
  TestDocumentLoader(Client* client,
                     const base::FilePath::StringType& pdf_name);
  ~TestDocumentLoader() override;

  // Simulates loading up to |max_bytes| more data, returning `true` if there is
  // more data to load (that is, IsDocumentComplete() returns `false`).
  bool SimulateLoadData(uint32_t max_bytes);

  // DocumentLoader:
  bool Init(std::unique_ptr<URLLoaderWrapper> loader,
            const std::string& url) override;
  bool GetBlock(uint32_t position, uint32_t size, void* buf) const override;
  bool IsDataAvailable(uint32_t position, uint32_t size) const override;
  void RequestData(uint32_t position, uint32_t size) override;
  bool IsDocumentComplete() const override;
  uint32_t GetDocumentSize() const override;
  uint32_t BytesReceived() const override;
  void ClearPendingRequests() override;

 private:
  Client* const client_;
  std::string pdf_data_;

  // Not using ChunkStream, for more fine-grained control over request size.
  uint32_t received_bytes_ = 0;
  RangeSet received_ranges_;
  RangeSet pending_ranges_;
};

}  // namespace chrome_pdf

#endif  // PDF_TEST_TEST_DOCUMENT_LOADER_H_
