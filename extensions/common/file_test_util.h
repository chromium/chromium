// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef EXTENSIONS_COMMON_FILE_TEST_UTIL_H_
#define EXTENSIONS_COMMON_FILE_TEST_UTIL_H_

#include "base/strings/string_piece.h"

namespace base {
class FilePath;
}

namespace extensions {
namespace file_test_util {

// Writes |content| to |path|. Returns true if writing was successful,
// verifying the number of bytes written equals the size of |content|.
bool WriteFile(const base::FilePath& path, base::StringPiece content);

}  // namespace file_test_util
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FILE_TEST_UTIL_H_
