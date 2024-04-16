// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_COMMON_STRING_UTIL_H_
#define FUCHSIA_WEB_COMMON_STRING_UTIL_H_

#include <cstdint>
#include <string_view>
#include <vector>


// Creates a byte vector from a string.
std::vector<uint8_t> StringToBytes(std::string_view str);

// Creates a string from a byte vector.
std::string_view BytesAsString(const std::vector<uint8_t>& bytes);

#endif  // FUCHSIA_WEB_COMMON_STRING_UTIL_H_
