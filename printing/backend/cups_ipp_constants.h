// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_CUPS_IPP_CONSTANTS_H_
#define PRINTING_BACKEND_CUPS_IPP_CONSTANTS_H_

namespace printing {

// property names
extern const char kIppCollate[];
extern const char kIppCopies[];
extern const char kIppColor[];
extern const char kIppMedia[];
extern const char kIppDuplex[];
extern const char kIppRequestingUserName[];
extern const char kIppResolution[];
extern const char kIppPin[];
extern const char kIppPinEncryption[];

// collation values
extern const char kCollated[];
extern const char kUncollated[];

#if defined(OS_CHROMEOS)

extern const char kIppDocumentAttributes[];
extern const char kIppJobAttributes[];

extern const char kPinEncryptionNone[];

extern const char kOptionFalse[];
extern const char kOptionTrue[];

#endif  // defined(OS_CHROMEOS)

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_IPP_CONSTANTS_H_
