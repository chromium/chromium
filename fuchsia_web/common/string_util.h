// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_COMMON_STRING_UTIL_H_
#define FUCHSIA_WEB_COMMON_STRING_UTIL_H_

#include <cstdint>
#include <vector>

#include "base/strings/string_piece.h"

// Creates a byte vector from a string.
std::vector<uint8_t> StringToBytes(base::StringPiece str);

// Creates a string from a byte vector.
base::StringPiece BytesAsString(const std::vector<uint8_t>& bytes);

#endif  // FUCHSIA_WEB_COMMON_STRING_UTIL_H_
