// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_WEB_PACKAGE_SIGNED_EXCHANGE_UTILS_H_
#define CONTENT_COMMON_WEB_PACKAGE_SIGNED_EXCHANGE_UTILS_H_

#include <string>

#include "net/base/hash_value.h"

namespace content {
namespace signed_exchange_utils {

// Serializes the |header_integrity| hash in the form of
// "sha256-<base64-hash-value>".
std::string CreateHeaderIntegrityHashString(
    const net::SHA256HashValue& header_integrity);

}  // namespace signed_exchange_utils
}  // namespace content

#endif  // CONTENT_COMMON_WEB_PACKAGE_SIGNED_EXCHANGE_UTILS_H_
