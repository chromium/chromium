// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_mem_buffer_file_write.h"

namespace chrome_pdf {

PDFiumMemBufferFileWrite::PDFiumMemBufferFileWrite() {
  version = 1;
  WriteBlock = &WriteBlockImpl;
}

PDFiumMemBufferFileWrite::~PDFiumMemBufferFileWrite() = default;

std::vector<uint8_t> PDFiumMemBufferFileWrite::TakeBuffer() {
  return std::move(buffer_);
}

// static
int PDFiumMemBufferFileWrite::WriteBlockImpl(FPDF_FILEWRITE* this_file_write,
                                             const void* data,
                                             unsigned long size) {
  auto* buffer = static_cast<PDFiumMemBufferFileWrite*>(this_file_write);
  return buffer->DoWriteBlock(static_cast<const uint8_t*>(data), size);
}

int PDFiumMemBufferFileWrite::DoWriteBlock(const uint8_t* data,
                                           unsigned long size) {
  buffer_.insert(buffer_.end(), data, data + size);
  return 1;
}

}  // namespace chrome_pdf
