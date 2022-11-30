// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "net/websockets/websocket_extension_parser.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  net::WebSocketExtensionParser parser;
  parser.Parse(reinterpret_cast<const char*>(data), size);

  return 0;
}
