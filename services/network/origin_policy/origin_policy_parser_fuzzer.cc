// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/strings/string_piece.h"
#include "services/network/origin_policy/origin_policy_parser.h"

namespace content {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  network::OriginPolicyParser::Parse(
      base::StringPiece(reinterpret_cast<const char*>(data), size));
  return 0;
}

}  // namespace content
