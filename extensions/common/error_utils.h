// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_ERROR_UTILS_H_
#define EXTENSIONS_COMMON_ERROR_UTILS_H_

#include <string>

#include "base/strings/string16.h"
#include "base/strings/string_piece.h"

namespace extensions {

class ErrorUtils {
 public:
  // Creates an error messages from a pattern.
  static std::string FormatErrorMessage(base::StringPiece format,
                                        base::StringPiece s1);

  static std::string FormatErrorMessage(base::StringPiece format,
                                        base::StringPiece s1,
                                        base::StringPiece s2);

  static std::string FormatErrorMessage(base::StringPiece format,
                                        base::StringPiece s1,
                                        base::StringPiece s2,
                                        base::StringPiece s3);

  static base::string16 FormatErrorMessageUTF16(base::StringPiece format,
                                                base::StringPiece s1);

  static base::string16 FormatErrorMessageUTF16(base::StringPiece format,
                                                base::StringPiece s1,
                                                base::StringPiece s2);

  static base::string16 FormatErrorMessageUTF16(base::StringPiece format,
                                                base::StringPiece s1,
                                                base::StringPiece s2,
                                                base::StringPiece s3);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_ERROR_UTILS_H_
