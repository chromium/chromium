// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "net/proxy_resolution/proxy_list.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  net::ProxyList list;
  std::string input(data, data + size);
  list.SetFromPacString(input);
  return 0;
}
