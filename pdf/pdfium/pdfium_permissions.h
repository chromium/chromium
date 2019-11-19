// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_PERMISSIONS_H_
#define PDF_PDFIUM_PDFIUM_PERMISSIONS_H_

#include "pdf/pdf_engine.h"
#include "third_party/pdfium/public/fpdfview.h"

namespace chrome_pdf {

// See Table 3.20 in the PDF 1.7 spec for details on how to interpret permission
// bits. Exposed for use in testing.
constexpr uint32_t kPDFPermissionPrintMask = 1 << 2;
constexpr uint32_t kPDFPermissionPrintHighQualityMask = 1 << 11;
constexpr uint32_t kPDFPermissionCopyMask = 1 << 4;
constexpr uint32_t kPDFPermissionCopyAccessibleMask = 1 << 9;

// The permissions for a given FPDF_DOCUMENT.
class PDFiumPermissions final {
 public:
  static PDFiumPermissions CreateForTesting(int permissions_handler_revision,
                                            unsigned long permission_bits);

  explicit PDFiumPermissions(FPDF_DOCUMENT doc);

  bool HasPermission(PDFEngine::DocumentPermission permission) const;

 private:
  // For unit tests.
  PDFiumPermissions(int permissions_handler_revision,
                    unsigned long permission_bits);

  // Permissions security handler revision number. -1 for unknown.
  const int permissions_handler_revision_;

  // Permissions bitfield.
  const unsigned long permission_bits_;
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_PERMISSIONS_H_
