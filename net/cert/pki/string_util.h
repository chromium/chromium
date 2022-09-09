// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_PKI_STRING_UTIL_H_
#define NET_CERT_PKI_STRING_UTIL_H_

#include "net/base/net_export.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace net::string_util {

NET_EXPORT_PRIVATE bool IsAscii(std::string_view str);

NET_EXPORT_PRIVATE bool IsEqualNoCase(std::string_view str1,
                                      std::string_view str2);

NET_EXPORT_PRIVATE bool StartsWithNoCase(std::string_view str,
                                         std::string_view prefix);

NET_EXPORT_PRIVATE bool EndsWithNoCase(std::string_view str,
                                       std::string_view suffix);

// TODO(bbe) transition below to c++20
NET_EXPORT_PRIVATE bool StartsWith(std::string_view str,
                                   std::string_view prefix);

NET_EXPORT_PRIVATE bool EndsWith(std::string_view str, std::string_view suffix);

}  // namespace net::string_util

#endif  // NET_CERT_PKI_STRING_UTIL_H_
