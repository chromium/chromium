// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/web_package/signed_exchange_utils.h"

#include "base/base64.h"

namespace content {
namespace signed_exchange_utils {

std::string CreateHeaderIntegrityHashString(
    const net::SHA256HashValue& header_integrity) {
  std::string header_integrity_base64;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(header_integrity.data),
                        sizeof(header_integrity.data)),
      &header_integrity_base64);
  return std::string("sha256-") + header_integrity_base64;
}

}  // namespace signed_exchange_utils
}  // namespace content
