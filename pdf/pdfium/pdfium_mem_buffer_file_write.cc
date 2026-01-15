// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_mem_buffer_file_write.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"

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
  // SAFETY: `size` is provided by PDFium which must provide a valid pointer
  // and size.
  // https://pdfium.googlesource.com/pdfium/+/refs/heads/main/public/fpdf_save.h#39
  return buffer->DoWriteBlock(
      UNSAFE_BUFFERS(base::span(static_cast<const uint8_t*>(data), size)));
}

int PDFiumMemBufferFileWrite::DoWriteBlock(base::span<const uint8_t> data) {
  buffer_.insert(buffer_.end(), data.begin(), data.end());
  return 1;
}

}  // namespace chrome_pdf
