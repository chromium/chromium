// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/http2/platform/impl/random_util_helper_impl.h"

namespace http2 {
namespace test {

Http2String RandomStringImpl(RandomBase* random,
                             int len,
                             Http2StringPiece alphabet) {
  Http2String random_string;
  random_string.reserve(len);
  for (int i = 0; i < len; ++i)
    random_string.push_back(alphabet[random->Uniform(alphabet.size())]);
  return random_string;
}

}  // namespace test
}  // namespace http2
