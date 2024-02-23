// Copyright 2020 The Chromium Authors
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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FormType {
  kNone = 0,
  kAcroForm = 1,
  kXFAFull = 2,
  kXFAForeground = 3,
  kMaxValue = kXFAForeground
};

// Document properties, including those specified in the document information
// dictionary (see section 14.3.3 "Document Information Dictionary" of the ISO
// 32000-1:2008 spec).
struct DocumentMetadata {
  DocumentMetadata();
  DocumentMetadata(DocumentMetadata&&) noexcept;
  DocumentMetadata& operator=(DocumentMetadata&& other) noexcept;
  ~DocumentMetadata();

  // Version of the document.
  PdfVersion version = PdfVersion::kUnknown;

  // The size of the document in bytes.
  size_t size_bytes = 0;

  // Number of pages in the document.
  size_t page_count = 0;

  // Whether the document is optimized by linearization (see annex F "Linearized
  // PDF" of the ISO 32000-1:2008 spec).
  bool linearized = false;

  // Whether the document contains file attachments (see section 12.5.6.15 "File
  // Attachment Annotations" of the ISO 32000-1:2008 spec).
  bool has_attachments = false;

  // The type of form contained in the document.
  FormType form_type = FormType::kNone;

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

  // The date and time the document was most recently modified.
  base::Time mod_date;
};

}  // namespace chrome_pdf

#endif  // PDF_DOCUMENT_METADATA_H_
