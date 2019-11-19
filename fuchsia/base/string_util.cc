// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/string_util.h"

namespace cr_fuchsia {

std::vector<uint8_t> StringToBytes(base::StringPiece str) {
  const uint8_t* raw_data = reinterpret_cast<const uint8_t*>(str.data());
  return std::vector<uint8_t>(raw_data, raw_data + str.length());
}

base::StringPiece BytesAsString(const std::vector<uint8_t>& bytes) {
  return base::StringPiece(reinterpret_cast<const char*>(bytes.data()),
                           bytes.size());
}

}  // namespace cr_fuchsia
