// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_DOCUMENT_METADATA_H_
#define PDF_PDFIUM_PDFIUM_DOCUMENT_METADATA_H_

#include <stddef.h>

#include "pdf/document_metadata.h"
#include "third_party/pdfium/public/fpdfview.h"

namespace chrome_pdf {

// Creates a `DocumentMetadata` struct based on the input parameters. `doc` must
// be non-null.
DocumentMetadata GetPDFiumDocumentMetadata(FPDF_DOCUMENT doc,
                                           size_t size_bytes,
                                           size_t page_count,
                                           bool linearized,
                                           bool has_attachments);

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_DOCUMENT_METADATA_H_
