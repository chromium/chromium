// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <iostream>
#include <string>

// Very simple fuzzer to allow us to (manually) test that string comparison
// instrumentation is working. If so, this should almost immediately crash,
// otherwise it will run almost forever because there's no real chance of
// hitting upon 'fish' by accident.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string the_string(reinterpret_cast<const char*>(data), size);

  if (the_string == "fish") {
    std::cout << "Found fish\n";
    exit(1);
  }

  return 0;
}
