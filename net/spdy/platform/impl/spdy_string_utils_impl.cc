// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/platform/impl/spdy_string_utils_impl.h"

#include <string>

namespace spdy {

bool SpdyHexDecodeToUInt32Impl(absl::string_view data, uint32_t* out) {
  if (data.empty() || data.size() > 8u)
    return false;
  // Pad with leading zeros.
  std::string data_padded(8u, '0');
  memcpy(&data_padded[8u - data.size()], data.data(), data.size());
  return base::HexStringToUInt(data_padded, out);
}

}  // namespace spdy
