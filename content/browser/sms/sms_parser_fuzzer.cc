// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <tuple>

#include "base/strings/string_piece.h"
#include "content/browser/sms/sms_parser.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::ignore = content::SmsParser::Parse(
      base::StringPiece(reinterpret_cast<const char*>(data), size));
  return 0;
}
