// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_ERROR_UTILS_H_
#define EXTENSIONS_COMMON_ERROR_UTILS_H_

#include <string>

#include "base/containers/span.h"
#include "base/strings/string_piece.h"

namespace extensions {

class ErrorUtils {
 public:
  static std::string FormatErrorMessage(
      base::StringPiece format,
      base::span<const base::StringPiece> args);

  template <typename... Args>
  static std::string FormatErrorMessage(base::StringPiece format,
                                        base::StringPiece s1,
                                        const Args&... args) {
    const base::StringPiece pieces[] = {s1, args...};
    return FormatErrorMessage(format, pieces);
  }

  static std::u16string FormatErrorMessageUTF16(
      base::StringPiece format,
      base::span<const base::StringPiece> args);

  template <typename... Args>
  static std::u16string FormatErrorMessageUTF16(base::StringPiece format,
                                                base::StringPiece s1,
                                                const Args&... args) {
    const base::StringPiece pieces[] = {s1, args...};
    return FormatErrorMessageUTF16(format, pieces);
  }
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_ERROR_UTILS_H_
