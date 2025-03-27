// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/web_package/signed_exchange_utils.h"

#include <string_view>

#include "base/base64.h"
#include "base/strings/strcat.h"

namespace content {
namespace signed_exchange_utils {

std::string CreateHeaderIntegrityHashString(
    const net::SHA256HashValue& header_integrity) {
  return base::StrCat({"sha256-", base::Base64Encode(header_integrity)});
}

}  // namespace signed_exchange_utils
}  // namespace content
