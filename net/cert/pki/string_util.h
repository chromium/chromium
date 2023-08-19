// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_PKI_STRING_UTIL_H_
#define NET_CERT_PKI_STRING_UTIL_H_

#include "net/base/net_export.h"

#include <stdint.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace net::string_util {

// Returns true if the characters in |str| are all ASCII, false otherwise.
NET_EXPORT_PRIVATE bool IsAscii(std::string_view str);

// Compares |str1| and |str2| ASCII case insensitively (independent of locale).
// Returns true if |str1| and |str2| match.
NET_EXPORT_PRIVATE bool IsEqualNoCase(std::string_view str1,
                                      std::string_view str2);

// Compares |str1| and |prefix| ASCII case insensitively (independent of
// locale). Returns true if |str1| starts with |prefix|.
NET_EXPORT_PRIVATE bool StartsWithNoCase(std::string_view str,
                                         std::string_view prefix);

// Compares |str1| and |suffix| ASCII case insensitively (independent of
// locale). Returns true if |str1| starts with |suffix|.
NET_EXPORT_PRIVATE bool EndsWithNoCase(std::string_view str,
                                       std::string_view suffix);

// Finds and replaces all occurrences of |find| of non zero length with
// |replace| in |str|, returning the result.
NET_EXPORT_PRIVATE std::string FindAndReplace(std::string_view str,
                                              std::string_view find,
                                              std::string_view replace);

// TODO(bbe) transition below to c++20
// Compares |str1| and |prefix|. Returns true if |str1| starts with |prefix|.
NET_EXPORT_PRIVATE bool StartsWith(std::string_view str,
                                   std::string_view prefix);

// TODO(bbe) transition below to c++20
// Compares |str1| and |suffix|. Returns true if |str1| ends with |suffix|.
NET_EXPORT_PRIVATE bool EndsWith(std::string_view str, std::string_view suffix);

// Returns a hexadecimal string encoding |data| of length |length|.
NET_EXPORT_PRIVATE std::string HexEncode(const uint8_t* data, size_t length);

// Returns a decimal string representation of |i|.
NET_EXPORT_PRIVATE std::string NumberToDecimalString(int i);

// Splits |str| on |split_char| returning the list of resulting strings.
NET_EXPORT_PRIVATE std::vector<std::string_view> SplitString(
    std::string_view str,
    char split_char);

}  // namespace net::string_util

#endif  // NET_CERT_PKI_STRING_UTIL_H_
