// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_parser.h"

#include <stdint.h>

#include <string_view>
#include <tuple>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::ignore = content::SmsParser::Parse(
      std::string_view(reinterpret_cast<const char*>(data), size));
  return 0;
}
