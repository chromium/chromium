// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/test/test_document_loader.h"

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "pdf/url_loader_wrapper.h"

namespace chrome_pdf {

TestDocumentLoader::TestDocumentLoader(
    Client* client,
    const base::FilePath::StringType& pdf_name)
    : client_(client) {
  base::FilePath pdf_path;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &pdf_path));
  pdf_path = pdf_path.Append(FILE_PATH_LITERAL("pdf"))
                 .Append(FILE_PATH_LITERAL("test"))
                 .Append(FILE_PATH_LITERAL("data"))
                 .Append(pdf_name);
  CHECK(base::ReadFileToString(pdf_path, &pdf_data_));
}

TestDocumentLoader::~TestDocumentLoader() = default;

bool TestDocumentLoader::Init(std::unique_ptr<URLLoaderWrapper> loader,
                              const std::string& url) {
  NOTREACHED();
  return false;
}

bool TestDocumentLoader::GetBlock(uint32_t position,
                                  uint32_t size,
                                  void* buf) const {
  if (!IsDataAvailable(position, size))
    return false;

  memcpy(buf, pdf_data_.data() + position, size);
  return true;
}

bool TestDocumentLoader::IsDataAvailable(uint32_t position,
                                         uint32_t size) const {
  return position < pdf_data_.size() && size <= pdf_data_.size() &&
         position + size <= pdf_data_.size();
}

void TestDocumentLoader::RequestData(uint32_t position, uint32_t size) {
  client_->OnDocumentComplete();
}

bool TestDocumentLoader::IsDocumentComplete() const {
  return true;
}

uint32_t TestDocumentLoader::GetDocumentSize() const {
  return pdf_data_.size();
}

uint32_t TestDocumentLoader::BytesReceived() const {
  return pdf_data_.size();
}

}  // namespace chrome_pdf
