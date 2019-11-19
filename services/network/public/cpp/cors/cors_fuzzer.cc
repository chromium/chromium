// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/cors.h"

#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string test_data(reinterpret_cast<const char*>(data), size);
  network::cors::IsCorsSafelistedHeader("device-memory", test_data);
  network::cors::IsCorsSafelistedHeader("width", test_data);
  network::cors::IsCorsSafelistedHeader("content-type", test_data);
  network::cors::IsCorsSafelistedHeader(test_data, test_data);
  return 0;
}
