// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_CUPS_IPP_CONSTANTS_H_
#define PRINTING_BACKEND_CUPS_IPP_CONSTANTS_H_

#include "base/component_export.h"
#include "build/chromeos_buildflags.h"

namespace printing {

// property names
COMPONENT_EXPORT(PRINTING) extern const char kIppCollate[];
COMPONENT_EXPORT(PRINTING) extern const char kIppCopies[];
COMPONENT_EXPORT(PRINTING) extern const char kIppColor[];
COMPONENT_EXPORT(PRINTING) extern const char kIppMedia[];
COMPONENT_EXPORT(PRINTING) extern const char kIppDuplex[];
COMPONENT_EXPORT(PRINTING) extern const char kIppRequestingUserName[];
COMPONENT_EXPORT(PRINTING) extern const char kIppResolution[];
COMPONENT_EXPORT(PRINTING) extern const char kIppPin[];
COMPONENT_EXPORT(PRINTING) extern const char kIppPinEncryption[];

// collation values
COMPONENT_EXPORT(PRINTING) extern const char kCollated[];
COMPONENT_EXPORT(PRINTING) extern const char kUncollated[];

#if BUILDFLAG(IS_CHROMEOS_ASH)

COMPONENT_EXPORT(PRINTING) extern const char kIppDocumentAttributes[];
COMPONENT_EXPORT(PRINTING) extern const char kIppJobAttributes[];

COMPONENT_EXPORT(PRINTING) extern const char kPinEncryptionNone[];

COMPONENT_EXPORT(PRINTING) extern const char kOptionFalse[];
COMPONENT_EXPORT(PRINTING) extern const char kOptionTrue[];

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_IPP_CONSTANTS_H_
