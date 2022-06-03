// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/test/test_document_loader.h"

#include <stdint.h>

#include "base/base_paths.h"
#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "pdf/range_set.h"
#include "pdf/url_loader_wrapper.h"
#include "ui/gfx/range/range.h"

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

// TODO(crbug.com/1056817): Consider faking out URLLoaderWrapper, to avoid
// simulating the behavior of DocumentLoaderImpl (although that would result in
// 64 KiB loads).
bool TestDocumentLoader::SimulateLoadData(uint32_t max_bytes) {
  CHECK_GT(max_bytes, 0U);
  if (IsDocumentComplete())
    return false;

  // Approximate the behavior of DocumentLoaderImpl::ContinueDownload() by
  // either reading from the start of the next pending range, or from the
  // beginning of the document (skipping any received ranges).
  RangeSet candidate_ranges(gfx::Range(
      pending_ranges_.IsEmpty() ? 0 : pending_ranges_.First().start(),
      GetDocumentSize()));
  candidate_ranges.Subtract(received_ranges_);
  CHECK(!candidate_ranges.IsEmpty());

  gfx::Range request_range = candidate_ranges.First();
  if (request_range.length() > max_bytes)
    request_range.set_end(request_range.start() + max_bytes);

  // Simulate fetching the requested range.
  received_bytes_ += request_range.length();
  received_ranges_.Union(request_range);
  pending_ranges_.Subtract(request_range);
  client_->OnNewDataReceived();

  bool is_pending = !IsDocumentComplete();
  if (is_pending)
    client_->OnPendingRequestComplete();
  else
    client_->OnDocumentComplete();
  return is_pending;
}

bool TestDocumentLoader::Init(std::unique_ptr<URLLoaderWrapper> loader,
                              const std::string& url) {
  NOTREACHED() << "PDFiumEngine skips this call when testing";
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
  CHECK_LE(position, GetDocumentSize());
  CHECK_LE(size, GetDocumentSize() - position);
  gfx::Range range(position, position + size);
  return range.is_empty() || received_ranges_.Contains(range);
}

void TestDocumentLoader::RequestData(uint32_t position, uint32_t size) {
  if (IsDataAvailable(position, size))
    return;

  // DocumentLoaderImpl requests chunks of 64 KiB, but that is uninteresting for
  // small test files, so use byte ranges instead.
  RangeSet request_ranges(gfx::Range(position, position + size));
  request_ranges.Subtract(received_ranges_);
  pending_ranges_.Union(request_ranges);
}

bool TestDocumentLoader::IsDocumentComplete() const {
  return BytesReceived() == GetDocumentSize();
}

uint32_t TestDocumentLoader::GetDocumentSize() const {
  return pdf_data_.size();
}

uint32_t TestDocumentLoader::BytesReceived() const {
  return received_bytes_;
}

void TestDocumentLoader::ClearPendingRequests() {
  pending_ranges_.Clear();
}

}  // namespace chrome_pdf
