// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_permissions.h"

namespace chrome_pdf {

// static
PDFiumPermissions PDFiumPermissions::CreateForTesting(
    int permissions_handler_revision,
    unsigned long permission_bits) {
  return PDFiumPermissions(permissions_handler_revision, permission_bits);
}

PDFiumPermissions::PDFiumPermissions(FPDF_DOCUMENT doc)
    : permissions_handler_revision_(FPDF_GetSecurityHandlerRevision(doc)),
      permission_bits_(FPDF_GetDocPermissions(doc)) {}

PDFiumPermissions::PDFiumPermissions(int permissions_handler_revision,
                                     unsigned long permission_bits)
    : permissions_handler_revision_(permissions_handler_revision),
      permission_bits_(permission_bits) {}

bool PDFiumPermissions::HasPermission(
    PDFEngine::DocumentPermission permission) const {
  // PDF 1.7 spec, section 3.5.2 says: "If the revision number is 2 or greater,
  // the operations to which user access can be controlled are as follows: ..."
  //
  // Thus for revision numbers less than 2, permissions are ignored and this
  // always returns true.
  if (permissions_handler_revision_ < 2)
    return true;

  // Handle high quality printing permission separately for security handler
  // revision 3+. See table 3.20 in the PDF 1.7 spec.
  if (permission == PDFEngine::PERMISSION_PRINT_HIGH_QUALITY &&
      permissions_handler_revision_ >= 3) {
    return (permission_bits_ & kPDFPermissionPrintMask) != 0 &&
           (permission_bits_ & kPDFPermissionPrintHighQualityMask) != 0;
  }

  switch (permission) {
    case PDFEngine::PERMISSION_COPY:
      return (permission_bits_ & kPDFPermissionCopyMask) != 0;
    case PDFEngine::PERMISSION_COPY_ACCESSIBLE:
      return (permission_bits_ & kPDFPermissionCopyAccessibleMask) != 0;
    case PDFEngine::PERMISSION_PRINT_LOW_QUALITY:
    case PDFEngine::PERMISSION_PRINT_HIGH_QUALITY:
      // With security handler revision 2 rules, check the same bit for high
      // and low quality. See table 3.20 in the PDF 1.7 spec.
      return (permission_bits_ & kPDFPermissionPrintMask) != 0;
    default:
      return true;
  }
}

}  // namespace chrome_pdf
