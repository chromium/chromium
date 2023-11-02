// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_MEM_BUFFER_FILE_WRITE_H_
#define PDF_PDFIUM_PDFIUM_MEM_BUFFER_FILE_WRITE_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "third_party/pdfium/public/fpdf_save.h"

namespace chrome_pdf {

// Implementation of FPDF_FILEWRITE into a memory buffer.
class PDFiumMemBufferFileWrite : public FPDF_FILEWRITE {
 public:
  PDFiumMemBufferFileWrite();
  ~PDFiumMemBufferFileWrite();

  const std::vector<uint8_t>& buffer() const { return buffer_; }
  size_t size() const { return buffer_.size(); }

  std::vector<uint8_t> TakeBuffer();

 private:
  static int WriteBlockImpl(FPDF_FILEWRITE* this_file_write,
                            const void* data,
                            unsigned long size);

  int DoWriteBlock(const uint8_t* data, unsigned long size);

  std::vector<uint8_t> buffer_;
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_MEM_BUFFER_FILE_WRITE_H_
