// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/base/host_port_pair.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string test_data(reinterpret_cast<const char*>(data), size);
  net::HostPortPair::FromString(test_data);
  return 0;
}
