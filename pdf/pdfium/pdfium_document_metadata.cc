// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_document_metadata.h"

#include <stddef.h>

#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "pdf/document_metadata.h"
#include "pdf/pdf_utils/dates.h"
#include "pdf/pdfium/pdfium_api_string_buffer_adapter.h"
#include "third_party/pdfium/public/fpdf_doc.h"
#include "third_party/pdfium/public/fpdf_formfill.h"
#include "third_party/pdfium/public/fpdfview.h"

namespace chrome_pdf {

namespace {

// Retrieves the value of `field` in the document information dictionary.
// Trims whitespace characters from the retrieved value.
std::string GetTrimmedMetadataByField(FPDF_DOCUMENT doc,
                                      FPDF_BYTESTRING field) {
  CHECK(doc);

  std::u16string metadata = CallPDFiumWideStringBufferApi(
      base::BindRepeating(&FPDF_GetMetaText, doc, field),
      /*check_expected_size=*/false);

  return base::UTF16ToUTF8(base::TrimWhitespace(metadata, base::TRIM_ALL));
}

// Retrieves the version of the PDF (e.g. 1.4 or 2.0) as an enum.
PdfVersion GetDocumentVersion(FPDF_DOCUMENT doc) {
  CHECK(doc);

  int version;
  if (!FPDF_GetFileVersion(doc, &version)) {
    return PdfVersion::kUnknown;
  }

  switch (version) {
    case 10:
      return PdfVersion::k1_0;
    case 11:
      return PdfVersion::k1_1;
    case 12:
      return PdfVersion::k1_2;
    case 13:
      return PdfVersion::k1_3;
    case 14:
      return PdfVersion::k1_4;
    case 15:
      return PdfVersion::k1_5;
    case 16:
      return PdfVersion::k1_6;
    case 17:
      return PdfVersion::k1_7;
    case 18:
      return PdfVersion::k1_8;
    case 20:
      return PdfVersion::k2_0;
    default:
      return PdfVersion::kUnknown;
  }
}

}  // namespace

DocumentMetadata GetPDFiumDocumentMetadata(FPDF_DOCUMENT doc,
                                           size_t size_bytes,
                                           size_t page_count,
                                           bool linearized,
                                           bool has_attachments) {
  CHECK(doc);

  DocumentMetadata doc_metadata;

  doc_metadata.version = GetDocumentVersion(doc);
  doc_metadata.size_bytes = size_bytes;
  doc_metadata.page_count = page_count;
  doc_metadata.linearized = linearized;
  doc_metadata.has_attachments = has_attachments;
  doc_metadata.form_type = static_cast<FormType>(FPDF_GetFormType(doc));

  // Document information dictionary entries
  doc_metadata.title = GetTrimmedMetadataByField(doc, "Title");
  doc_metadata.author = GetTrimmedMetadataByField(doc, "Author");
  doc_metadata.subject = GetTrimmedMetadataByField(doc, "Subject");
  doc_metadata.keywords = GetTrimmedMetadataByField(doc, "Keywords");
  doc_metadata.creator = GetTrimmedMetadataByField(doc, "Creator");
  doc_metadata.producer = GetTrimmedMetadataByField(doc, "Producer");
  doc_metadata.creation_date =
      ParsePdfDate(GetTrimmedMetadataByField(doc, "CreationDate"));
  doc_metadata.mod_date =
      ParsePdfDate(GetTrimmedMetadataByField(doc, "ModDate"));

  return doc_metadata;
}

}  // namespace chrome_pdf
