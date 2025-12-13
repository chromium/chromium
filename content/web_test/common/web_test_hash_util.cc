// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "crypto/obsolete/md5.h"

namespace content {

std::string Md5AsHexForWebTestPixels(base::span<const uint8_t> pixels) {
  return base::HexEncodeLower(crypto::obsolete::Md5::Hash(pixels));
}

}  // namespace content
