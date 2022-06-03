// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/structured_headers.h"

namespace net {
namespace structured_headers {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::StringPiece input(reinterpret_cast<const char*>(data), size);
  ParseItem(input);
  ParseListOfLists(input);
  ParseParameterisedList(input);
  return 0;
}

}  // namespace structured_headers
}  // namespace net
