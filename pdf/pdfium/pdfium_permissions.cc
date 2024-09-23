// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_permissions.h"

#include <stdint.h>

#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"

namespace chrome_pdf {

// static
PDFiumPermissions PDFiumPermissions::CreateForTesting(
    int permissions_handler_revision,
    uint32_t permission_bits) {
  return PDFiumPermissions(permissions_handler_revision, permission_bits);
}

// Note that `FPDF_GetDocPermissions()` returns `unsigned long`, but is
// specified to return a 32-bit integer. The implementation also uses `uint32_t`
// internally.
PDFiumPermissions::PDFiumPermissions(FPDF_DOCUMENT doc)
    : permissions_handler_revision_(FPDF_GetSecurityHandlerRevision(doc)),
      permission_bits_(
          base::checked_cast<uint32_t>(FPDF_GetDocPermissions(doc))) {}

PDFiumPermissions::PDFiumPermissions(int permissions_handler_revision,
                                     uint32_t permission_bits)
    : permissions_handler_revision_(permissions_handler_revision),
      permission_bits_(permission_bits) {}

bool PDFiumPermissions::HasPermission(DocumentPermission permission) const {
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
      case DocumentPermission::kCopy:
      case DocumentPermission::kCopyAccessible:
        // Check the same copy bit for all copying permissions.
        return HasPermissionBits(kPDFPermissionBit05CopyMask);
      case DocumentPermission::kPrintLowQuality:
      case DocumentPermission::kPrintHighQuality:
        // Check the same printing bit for all printing permissions.
        return HasPermissionBits(kPDFPermissionBit03PrintMask);
    }
    NOTREACHED();
  } else {
    // Security handler revision 3+ have different rules for interpreting the
    // bits in `permission_bits_`.
    switch (permission) {
      case DocumentPermission::kCopy:
        return HasPermissionBits(kPDFPermissionBit05CopyMask);
      case DocumentPermission::kCopyAccessible:
        return HasPermissionBits(kPDFPermissionBit10CopyAccessibleMask);
      case DocumentPermission::kPrintLowQuality:
        return HasPermissionBits(kPDFPermissionBit03PrintMask);
      case DocumentPermission::kPrintHighQuality:
        return HasPermissionBits(kPDFPermissionBit03PrintMask |
                                 kPDFPermissionBit12PrintHighQualityMask);
    }
    NOTREACHED();
  }
}

}  // namespace chrome_pdf
