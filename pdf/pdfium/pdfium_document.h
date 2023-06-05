// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_DOCUMENT_H_
#define PDF_PDFIUM_PDFIUM_DOCUMENT_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_dataavail.h"
#include "third_party/pdfium/public/fpdfview.h"

namespace chrome_pdf {

class DocumentLoader;

class PDFiumDocument {
 public:
  class DownloadHints;
  class FileAccess;
  class FileAvail;

  explicit PDFiumDocument(DocumentLoader* doc_loader);
  PDFiumDocument(const PDFiumDocument&) = delete;
  PDFiumDocument& operator=(const PDFiumDocument&) = delete;
  ~PDFiumDocument();

  FPDF_FILEACCESS& file_access();
  FX_FILEAVAIL& file_availability();
  FX_DOWNLOADHINTS& download_hints();

  FPDF_AVAIL fpdf_availability() const { return fpdf_availability_.get(); }
  FPDF_DOCUMENT doc() const { return doc_handle_.get(); }
  FPDF_FORMHANDLE form() const { return form_handle_.get(); }

  int form_status() const { return form_status_; }

  void CreateFPDFAvailability();
  void ResetFPDFAvailability();

  void LoadDocument(const std::string& password);

  void SetFormStatus();
  void InitializeForm(FPDF_FORMFILLINFO* form_info);

 private:
  const raw_ptr<DocumentLoader> doc_loader_;

  // Interface structure to provide access to document stream.
  std::unique_ptr<FileAccess> file_access_;

  // Interface structure to check data availability in the document stream.
  std::unique_ptr<FileAvail> file_availability_;

  // Interface structure to request data chunks from the document stream.
  std::unique_ptr<DownloadHints> download_hints_;

  // Pointer to the document availability interface.
  ScopedFPDFAvail fpdf_availability_;

  // The PDFium wrapper object for the document. Must come after
  // `fpdf_availability_` to prevent outliving it.
  ScopedFPDFDocument doc_handle_;

  // The PDFium wrapper for form data.  Used even if there are no form controls
  // on the page. Must come after `doc_handle_` to prevent outliving it.
  ScopedFPDFFormHandle form_handle_;

  // Current form availability status.
  int form_status_ = PDF_FORM_NOTAVAIL;
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_DOCUMENT_H_
