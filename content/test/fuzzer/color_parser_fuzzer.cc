// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "content/public/common/color_parser.h"

namespace content {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  SkColor result;
  ParseCssColorString(std::string(reinterpret_cast<const char*>(data), size),
                      &result);
  return 0;
}

}  // namespace content
