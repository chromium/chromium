// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_permissions.h"

#include "base/notreached.h"

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

  if (permissions_handler_revision_ == 2) {
    // Security handler revision 2 rules are simple.
    switch (permission) {
      case PDFEngine::PERMISSION_COPY:
      case PDFEngine::PERMISSION_COPY_ACCESSIBLE:
        // Check the same copy bit for all copying permissions.
        return (permission_bits_ & kPDFPermissionCopyMask) != 0;
      case PDFEngine::PERMISSION_PRINT_LOW_QUALITY:
      case PDFEngine::PERMISSION_PRINT_HIGH_QUALITY:
        // Check the same printing bit for all printing permissions.
        return (permission_bits_ & kPDFPermissionPrintMask) != 0;
    }
  } else {
    // Security handler revision 3+ have different rules for interpreting the
    // bits in `permission_bits_`.
    switch (permission) {
      case PDFEngine::PERMISSION_COPY:
        return (permission_bits_ & kPDFPermissionCopyMask) != 0;
      case PDFEngine::PERMISSION_COPY_ACCESSIBLE:
        return (permission_bits_ & kPDFPermissionCopyAccessibleMask) != 0;
      case PDFEngine::PERMISSION_PRINT_LOW_QUALITY:
        return (permission_bits_ & kPDFPermissionPrintMask) != 0;
      case PDFEngine::PERMISSION_PRINT_HIGH_QUALITY:
        return (permission_bits_ & kPDFPermissionPrintMask) != 0 &&
               (permission_bits_ & kPDFPermissionPrintHighQualityMask) != 0;
    }
  }
  NOTREACHED() << "Unknown permission " << permission;
  return true;
}

}  // namespace chrome_pdf
