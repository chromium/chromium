// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_PROGRESSIVE_SEARCHIFIER_H_
#define PDF_PDFIUM_PDFIUM_PROGRESSIVE_SEARCHIFIER_H_

#include "pdf/pdf_progressive_searchifier.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom-forward.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"

namespace chrome_pdf {

class PdfiumProgressiveSearchifier : public PdfProgressiveSearchifier {
 public:
  PdfiumProgressiveSearchifier();
  PdfiumProgressiveSearchifier(const PdfiumProgressiveSearchifier&) = delete;
  PdfiumProgressiveSearchifier& operator=(const PdfiumProgressiveSearchifier&) =
      delete;
  ~PdfiumProgressiveSearchifier() override;

  // PdfProgressiveSearchifier:
  void AddPage(const SkBitmap& bitmap,
               uint32_t page_index,
               screen_ai::mojom::VisualAnnotationPtr annotation) override;
  void DeletePage(uint32_t page_index) override;
  std::vector<uint8_t> Save() override;

 private:
  // TODO(chuhsuan): Consider moving this to pdf_init.h as
  // pdfium_engine_exports_unittest.cc and pdf.cc have similar ones.
  class ScopedSdkInitializer {
   public:
    ScopedSdkInitializer();

    ScopedSdkInitializer(const ScopedSdkInitializer&) = delete;
    ScopedSdkInitializer& operator=(const ScopedSdkInitializer&) = delete;

    ~ScopedSdkInitializer();
  };

  ScopedSdkInitializer sdk_initializer_;
  ScopedFPDFDocument doc_;
  ScopedFPDFFont font_;
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_PROGRESSIVE_SEARCHIFIER_H_
