// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_CUPS_IPP_CONSTANTS_H_
#define PRINTING_BACKEND_CUPS_IPP_CONSTANTS_H_

#include "build/chromeos_buildflags.h"
#include "printing/printing_export.h"

namespace printing {

// property names
PRINTING_EXPORT extern const char kIppCollate[];
PRINTING_EXPORT extern const char kIppCopies[];
PRINTING_EXPORT extern const char kIppColor[];
PRINTING_EXPORT extern const char kIppMedia[];
PRINTING_EXPORT extern const char kIppDuplex[];
PRINTING_EXPORT extern const char kIppRequestingUserName[];
PRINTING_EXPORT extern const char kIppResolution[];
PRINTING_EXPORT extern const char kIppPin[];
PRINTING_EXPORT extern const char kIppPinEncryption[];

// collation values
PRINTING_EXPORT extern const char kCollated[];
PRINTING_EXPORT extern const char kUncollated[];

#if BUILDFLAG(IS_CHROMEOS_ASH)

PRINTING_EXPORT extern const char kIppDocumentAttributes[];
PRINTING_EXPORT extern const char kIppJobAttributes[];

PRINTING_EXPORT extern const char kPinEncryptionNone[];

PRINTING_EXPORT extern const char kOptionFalse[];
PRINTING_EXPORT extern const char kOptionTrue[];

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_IPP_CONSTANTS_H_
