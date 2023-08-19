// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_ipp_constants.h"

#include <cups/cups.h>

#include "build/build_config.h"

namespace printing {

// operation attributes
constexpr char kIppDocumentFormat[] = "document-format";         // RFC 8011
constexpr char kIppDocumentName[] = "document-name";             // RFC 8011
constexpr char kIppJobId[] = "job-id";                           // RFC 8011
constexpr char kIppJobName[] = "job-name";                       // RFC 8011
constexpr char kIppLastDocument[] = "last-document";             // RFC 8011
constexpr char kIppPin[] = "job-password";                       // PWG 5100.11
constexpr char kIppPinEncryption[] = "job-password-encryption";  // PWG 5100.11
constexpr char kIppPrinterUri[] = "printer-uri";                 // RFC 8011
constexpr char kIppRequestedAttributes[] = "requested-attributes";  // RFC 8011
constexpr char kIppRequestingUserName[] = "requesting-user-name";  // RFC 8011

// printer attributes
constexpr char kIppMediaColDatabase[] = "media-col-database";

// job attributes
constexpr char kIppCollate[] = "multiple-document-handling";  // PWG 5100.19
constexpr char kIppCopies[] = CUPS_COPIES;
constexpr char kIppColor[] = CUPS_PRINT_COLOR_MODE;
constexpr char kIppMedia[] = CUPS_MEDIA;
constexpr char kIppMediaCol[] = "media-col";  // PWG 5100.7
constexpr char kIppDuplex[] = CUPS_SIDES;
constexpr char kIppResolution[] = "printer-resolution";  // RFC 8011

// collation values
constexpr char kCollated[] = "separate-documents-collated-copies";
constexpr char kUncollated[] = "separate-documents-uncollated-copies";

// media-col collection members (all from PWG 5100.7)
constexpr char kIppMediaBottomMargin[] = "media-bottom-margin";
constexpr char kIppMediaLeftMargin[] = "media-left-margin";
constexpr char kIppMediaRightMargin[] = "media-right-margin";
constexpr char kIppMediaSize[] = "media-size";
constexpr char kIppMediaSource[] = "media-source";
constexpr char kIppMediaTopMargin[] = "media-top-margin";
constexpr char kIppMediaType[] = "media-type";
constexpr char kIppXDimension[] = "x-dimension";
constexpr char kIppYDimension[] = "y-dimension";

#if BUILDFLAG(IS_CHROMEOS)

constexpr char kIppDocumentAttributes[] =
    "document-creation-attributes";                              // PWG 5100.5
constexpr char kIppJobAttributes[] = "job-creation-attributes";  // PWG 5100.11

constexpr char kPinEncryptionNone[] = "none";

constexpr char kOptionFalse[] = "false";
constexpr char kOptionTrue[] = "true";

// client-info
constexpr char kIppClientInfo[] = "client-info";
constexpr char kIppClientName[] = "client-name";
constexpr char kIppClientPatches[] = "client-patches";
constexpr char kIppClientStringVersion[] = "client-string-version";
constexpr char kIppClientType[] = "client-type";
constexpr char kIppClientVersion[] = "client-version";

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace printing
