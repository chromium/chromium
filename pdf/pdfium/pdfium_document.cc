// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_document.h"

#include <utility>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "pdf/loader/document_loader.h"

namespace chrome_pdf {

class PDFiumDocument::FileAvail : public FX_FILEAVAIL {
 public:
  explicit FileAvail(DocumentLoader* doc_loader) : doc_loader_(doc_loader) {
    DCHECK(doc_loader);
    version = 1;
    IsDataAvail = &FileAvail::IsDataAvailImpl;
  }

 private:
  // PDFium interface to check is block of data is available.
  static FPDF_BOOL IsDataAvailImpl(FX_FILEAVAIL* param,
                                   size_t offset,
                                   size_t size) {
    auto* file_avail = static_cast<FileAvail*>(param);
    return file_avail->doc_loader_->IsDataAvailable(offset, size);
  }

  raw_ptr<DocumentLoader> doc_loader_;
};

class PDFiumDocument::DownloadHints : public FX_DOWNLOADHINTS {
 public:
  explicit DownloadHints(DocumentLoader* doc_loader) : doc_loader_(doc_loader) {
    DCHECK(doc_loader);
    version = 1;
    AddSegment = &DownloadHints::AddSegmentImpl;
  }

 private:
  // PDFium interface to request download of the block of data.
  static void AddSegmentImpl(FX_DOWNLOADHINTS* param,
                             size_t offset,
                             size_t size) {
    auto* download_hints = static_cast<DownloadHints*>(param);
    return download_hints->doc_loader_->RequestData(offset, size);
  }

  raw_ptr<DocumentLoader> doc_loader_;
};

class PDFiumDocument::FileAccess : public FPDF_FILEACCESS {
 public:
  explicit FileAccess(DocumentLoader* doc_loader) : doc_loader_(doc_loader) {
    DCHECK(doc_loader);
    m_FileLen = 0;
    m_GetBlock = &FileAccess::GetBlockImpl;
    m_Param = this;
  }

 private:
  // PDFium interface to get block of data.
  static int GetBlockImpl(void* param,
                          unsigned long position,
                          unsigned char* buffer,
                          unsigned long size) {
    auto* file_access = static_cast<FileAccess*>(param);
    return file_access->doc_loader_->GetBlock(position, size, buffer);
  }

  raw_ptr<DocumentLoader> doc_loader_;
};

PDFiumDocument::PDFiumDocument(DocumentLoader* doc_loader)
    : doc_loader_(doc_loader),
      file_access_(std::make_unique<FileAccess>(doc_loader)),
      file_availability_(std::make_unique<FileAvail>(doc_loader)),
      download_hints_(std::make_unique<DownloadHints>(doc_loader)) {}

PDFiumDocument::~PDFiumDocument() = default;

FPDF_FILEACCESS& PDFiumDocument::file_access() {
  return *file_access_;
}

FX_FILEAVAIL& PDFiumDocument::file_availability() {
  return *file_availability_;
}

FX_DOWNLOADHINTS& PDFiumDocument::download_hints() {
  return *download_hints_;
}

void PDFiumDocument::CreateFPDFAvailability() {
  fpdf_availability_.reset(
      FPDFAvail_Create(file_availability_.get(), file_access_.get()));
}

void PDFiumDocument::ResetFPDFAvailability() {
  fpdf_availability_.reset();
}

void PDFiumDocument::LoadDocument(const std::string& password) {
  const char* password_cstr = password.empty() ? nullptr : password.c_str();
  if (doc_loader_->IsDocumentComplete() &&
      !FPDFAvail_IsLinearized(fpdf_availability_.get())) {
    doc_handle_.reset(
        FPDF_LoadCustomDocument(file_access_.get(), password_cstr));
  } else {
    doc_handle_.reset(
        FPDFAvail_GetDocument(fpdf_availability_.get(), password_cstr));
  }
}

void PDFiumDocument::SetFormStatus() {
  form_status_ =
      FPDFAvail_IsFormAvail(fpdf_availability_.get(), download_hints_.get());
}

void PDFiumDocument::InitializeForm(FPDF_FORMFILLINFO* form_info) {
  form_handle_.reset(FPDFDOC_InitFormFillEnvironment(doc(), form_info));
}

}  // namespace chrome_pdf
