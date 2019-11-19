// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_ipp_constants.h"

#include <cups/cups.h>

namespace printing {

constexpr char kIppCollate[] = "sheet-collate";  // RFC 3381
constexpr char kIppCopies[] = CUPS_COPIES;
constexpr char kIppColor[] = CUPS_PRINT_COLOR_MODE;
constexpr char kIppMedia[] = CUPS_MEDIA;
constexpr char kIppDuplex[] = CUPS_SIDES;
constexpr char kIppResolution[] = "printer-resolution";            // RFC 8011
constexpr char kIppRequestingUserName[] = "requesting-user-name";  // RFC 8011
constexpr char kIppPin[] = "job-password";                       // PWG 5100.11
constexpr char kIppPinEncryption[] = "job-password-encryption";  // PWG 5100.11

// collation values
constexpr char kCollated[] = "collated";
constexpr char kUncollated[] = "uncollated";

#if defined(OS_CHROMEOS)

constexpr char kIppDocumentAttributes[] =
    "document-creation-attributes";                              // PWG 5100.5
constexpr char kIppJobAttributes[] = "job-creation-attributes";  // PWG 5100.11

constexpr char kPinEncryptionNone[] = "none";

constexpr char kOptionFalse[] = "false";
constexpr char kOptionTrue[] = "true";

#endif  // defined(OS_CHROMEOS)

}  // namespace printing
