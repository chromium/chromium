// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_CUPS_IPP_CONSTANTS_H_
#define PRINTING_BACKEND_CUPS_IPP_CONSTANTS_H_

#include "base/component_export.h"
#include "build/build_config.h"

namespace printing {

// operation attributes
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppDocumentFormat[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppDocumentName[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppJobId[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppJobName[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppLastDocument[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppPin[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppPinEncryption[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppPrinterUri[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppRequestingUserName[];

// job attributes
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppCollate[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppCopies[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppColor[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppMedia[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppDuplex[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppResolution[];

// collation values
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kCollated[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kUncollated[];

#if BUILDFLAG(IS_CHROMEOS)

COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppDocumentAttributes[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppJobAttributes[];

COMPONENT_EXPORT(PRINT_BACKEND) extern const char kPinEncryptionNone[];

COMPONENT_EXPORT(PRINT_BACKEND) extern const char kOptionFalse[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kOptionTrue[];

COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppClientInfo[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppClientName[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppClientPatches[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppClientStringVersion[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppClientType[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kIppClientVersion[];

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_IPP_CONSTANTS_H_
