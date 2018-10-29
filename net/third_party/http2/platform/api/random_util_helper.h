// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_HTTP2_PLATFORM_API_RANDOM_UTIL_HELPER_H_
#define NET_THIRD_PARTY_HTTP2_PLATFORM_API_RANDOM_UTIL_HELPER_H_

#include "net/third_party/http2/platform/impl/random_util_helper_impl.h"

namespace http2 {

namespace test {

inline Http2String RandomString(RandomBase* random,
                                int len,
                                Http2StringPiece alphabet) {
  return RandomStringImpl(random, len, alphabet);
}

}  // namespace test

}  // namespace http2

#endif  // NET_THIRD_PARTY_HTTP2_PLATFORM_API_RANDOM_UTIL_HELPER_H_
