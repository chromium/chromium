// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/inspector_protocol/crdtp/json.h"

// A corpus for this fuzzer is located in
// devtools_protocol_encoding_cbor_fuzzer_corpus.
// The files contained therein were generated from JSON examples,
// by running the transcode utility from
// https://chromium.googlesource.com/deps/inspector_protocol/

namespace content {
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string json;
  crdtp::json::ConvertCBORToJSON(crdtp::span<uint8_t>(data, size), &json);
  return 0;
}
}  // namespace content
