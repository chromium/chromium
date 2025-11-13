// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"

// A simple unit-test style driver for libfuzzer tests.
// Usage: <fuzzer_test> <file>...

#include <stddef.h>
#include <stdint.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

// Libfuzzer API.
extern "C" {
// User function.
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);
// Initialization function.
__attribute__((weak)) int LLVMFuzzerInitialize(int* argc, char*** argv);
// Mutation function provided by libFuzzer.
size_t LLVMFuzzerMutate(uint8_t* Data, size_t Size, size_t MaxSize);
}

std::vector<uint8_t> readFile(std::string path) {
  std::ifstream in(path);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
}

size_t LLVMFuzzerMutate(uint8_t* Data, size_t Size, size_t MaxSize) {
  return 0;
}

int main(int argc, char** argv) {
  base::span<char*> args =
      UNSAFE_BUFFERS(base::span(argv, base::checked_cast<size_t>(argc)));
  if (args.size() == 1) {
    std::cerr
        << "Usage: " << args[0]
        << " <file>...\n"
           "\n"
           "Alternatively, try building this target with "
           "use_libfuzzer=true for a better test driver. For details see:\n"
           "\n"
           "https://chromium.googlesource.com/chromium/src/+/main/"
           "testing/libfuzzer/getting_started.md"
        << std::endl;
    exit(1);
  }

  if (LLVMFuzzerInitialize) {
    LLVMFuzzerInitialize(&argc, &argv);
  }

  for (char* arg : args) {
    std::cout << arg << std::endl;
    auto v = readFile(arg);
    LLVMFuzzerTestOneInput(v.data(), v.size());
  }
}
