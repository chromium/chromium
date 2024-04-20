// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/cors.h"

#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // TODO(crbug.com/40242947) Add corpus so generated data is higher quality.
  const std::string test_data(reinterpret_cast<const char*>(data), size);
  network::cors::IsCorsSafelistedHeader("accept", test_data);
  network::cors::IsCorsSafelistedHeader("accept-language", test_data);
  network::cors::IsCorsSafelistedHeader("content-language", test_data);
  network::cors::IsCorsSafelistedHeader("content-type", test_data);
  network::cors::IsCorsSafelistedHeader("range", test_data);
  network::cors::IsCorsSafelistedHeader("device-memory", test_data);
  network::cors::IsCorsSafelistedHeader("dpr", test_data);
  network::cors::IsCorsSafelistedHeader("width", test_data);
  network::cors::IsCorsSafelistedHeader("viewport-width", test_data);
  network::cors::IsCorsSafelistedHeader("rtt", test_data);
  network::cors::IsCorsSafelistedHeader("downlink", test_data);
  network::cors::IsCorsSafelistedHeader("ect", test_data);
  network::cors::IsCorsSafelistedHeader("save-data", test_data);
  network::cors::IsCorsSafelistedHeader(test_data, test_data);
  return 0;
}
