// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/pki/string_util.h"

#include "third_party/boringssl/src/include/openssl/mem.h"

namespace net::string_util {

bool IsAscii(std::string_view str) {
  for (unsigned char c : str) {
    if (c > 127) {
      return false;
    }
  }
  return true;
}

bool IsEqualNoCase(std::string_view str1, std::string_view str2) {
  if (str1.size() != str2.size()) {
    return false;
  }
  return std::equal(str2.cbegin(), str2.cend(), str1.cbegin(),
                    [](const unsigned char a, const unsigned char b) {
                      return OPENSSL_tolower(a) == OPENSSL_tolower(b);
                    });
}

bool EndsWithNoCase(std::string_view str, std::string_view suffix) {
  if (suffix.size() > str.size()) {
    return false;
  }
  return std::equal(suffix.crbegin(), suffix.crend(), str.crbegin(),
                    [](const unsigned char a, const unsigned char b) {
                      return OPENSSL_tolower(a) == OPENSSL_tolower(b);
                    });
}

bool StartsWithNoCase(std::string_view str, std::string_view prefix) {
  if (str.size() < prefix.size()) {
    return false;
  }
  return std::equal(prefix.cbegin(), prefix.cend(), str.cbegin(),
                    [](const unsigned char a, const unsigned char b) {
                      return OPENSSL_tolower(a) == OPENSSL_tolower(b);
                    });
}

// TODO(bbe) get rid of this once we can c++20.
bool EndsWith(std::string_view str, std::string_view suffix) {
  return suffix.size() <= str.size() &&
         suffix == str.substr(str.size() - suffix.size());
}

// TODO(bbe) get rid of this once we can c++20.
bool StartsWith(std::string_view str, std::string_view prefix) {
  return prefix.size() <= str.size() && prefix == str.substr(0, prefix.size());
}

}  // namespace net::string_util
