// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_STRING_UTIL_H_
#define FUCHSIA_BASE_STRING_UTIL_H_

#include <cstdint>
#include <vector>

#include "base/containers/span.h"
#include "base/strings/string_piece.h"

namespace cr_fuchsia {

// Creates a byte vector from a string.
std::vector<uint8_t> StringToBytes(base::StringPiece str);

// Creates a string from a byte vector.
base::StringPiece BytesAsString(const std::vector<uint8_t>& bytes);

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_STRING_UTIL_H_
