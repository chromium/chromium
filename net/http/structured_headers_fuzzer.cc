// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/http/structured_headers.h"

#include <string_view>

namespace net {
namespace structured_headers {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string_view input(reinterpret_cast<const char*>(data), size);
  ParseItem(input);
  ParseListOfLists(input);
  ParseParameterisedList(input);
  return 0;
}

}  // namespace structured_headers
}  // namespace net
