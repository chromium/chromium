// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_PERMISSIONS_H_
#define PDF_PDFIUM_PDFIUM_PERMISSIONS_H_

#include <stdint.h>

#include "pdf/pdfium/pdfium_engine.h"
#include "third_party/pdfium/public/fpdfview.h"

namespace chrome_pdf {

// See Table 3.20 in the PDF 1.7 spec for details on how to interpret permission
// bits. Exposed for use in testing.
constexpr uint32_t kPDFPermissionBit03PrintMask = 1 << 2;
constexpr uint32_t kPDFPermissionBit05CopyMask = 1 << 4;
constexpr uint32_t kPDFPermissionBit10CopyAccessibleMask = 1 << 9;
constexpr uint32_t kPDFPermissionBit12PrintHighQualityMask = 1 << 11;

// The permissions for a given FPDF_DOCUMENT.
class PDFiumPermissions final {
 public:
  static PDFiumPermissions CreateForTesting(int permissions_handler_revision,
                                            uint32_t permission_bits);

  explicit PDFiumPermissions(FPDF_DOCUMENT doc);

  bool HasPermission(DocumentPermission permission) const;

 private:
  // For unit tests.
  PDFiumPermissions(int permissions_handler_revision, uint32_t permission_bits);

  bool HasPermissionBits(uint32_t mask) const {
    return (permission_bits_ & mask) == mask;
  }

  // Permissions security handler revision number. -1 for unknown.
  const int permissions_handler_revision_;

  // Permissions bitfield.
  const uint32_t permission_bits_;
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_PERMISSIONS_H_
