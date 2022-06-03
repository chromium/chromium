// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/inspector_protocol/crdtp/json.h"

namespace content {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::vector<uint8_t> cbor;
  crdtp::json::ConvertJSONToCBOR(crdtp::span<uint8_t>(data, size), &cbor);
  return 0;
}
}  // namespace content
