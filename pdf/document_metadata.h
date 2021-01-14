// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_DOCUMENT_METADATA_H_
#define PDF_DOCUMENT_METADATA_H_

#include <string>

#include "base/time/time.h"

namespace chrome_pdf {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PdfVersion {
  kUnknown = 0,
  k1_0 = 1,
  k1_1 = 2,
  k1_2 = 3,
  k1_3 = 4,
  k1_4 = 5,
  k1_5 = 6,
  k1_6 = 7,
  k1_7 = 8,
  k1_8 = 9,  // Not an actual version. Kept for metrics purposes.
  k2_0 = 10,
  kMaxValue = k2_0
};

// Document properties, including those specified in the document information
// dictionary (see section 14.3.3 "Document Information Dictionary" of the ISO
// 32000-1 standard), as well as other properties about the file.
// TODO(crbug.com/93619): Finish adding fields like `size_bytes` and
// `is_encrypted`.
struct DocumentMetadata {
  DocumentMetadata();
  DocumentMetadata(const DocumentMetadata&) = delete;
  DocumentMetadata& operator=(const DocumentMetadata&) = delete;
  ~DocumentMetadata();

  // Version of the document
  PdfVersion version = PdfVersion::kUnknown;

  // Whether the document is optimized by linearization.
  bool linearized = false;

  // The document's title.
  std::string title;

  // The name of the document's creator.
  std::string author;

  // The document's subject.
  std::string subject;

  // The document's keywords.
  std::string keywords;

  // The name of the application that created the original document.
  std::string creator;

  // If the document's format was not originally PDF, the name of the
  // application that converted the document to PDF.
  std::string producer;

  // The date and time the document was created.
  base::Time creation_date;

  // The date and time the document was most recently modified
  base::Time mod_date;
};

}  // namespace chrome_pdf

#endif  // PDF_DOCUMENT_METADATA_H_
