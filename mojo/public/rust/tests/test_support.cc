// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Similar to support.cc, this patches over changes from the old Mojo SDK to
// Mojo now. Test-specific helpers are here.

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/tests/validation_test_input_parser.h"

extern "C" {

// Only for the standalone Rust tests. When Rust Mojo code is integrated into
// the wider Chrome build, this will not be used since Chrome initializes Mojo
// itself.
void InitializeMojoEmbedder(std::uint32_t argc, const char* const* argv) {
  // Some mojo internals check command line flags, so we must initialize it
  // here.
  base::CommandLine::Init(argc, argv);
  mojo::core::Init();
}

// Parses test data for Mojo message validation tests. Used for
// encoding/decoding tests. See `mojo::test::ParseValidationTestInput` for more
// details.
//
// Replacement for function removed from core C API. Wraps the equivalent C++
// API function.
const char* ParseValidationTest(const char* input,
                                std::size_t* num_handles,
                                std::uint8_t** data,
                                std::size_t* data_len) {
  std::vector<std::uint8_t> data_vec;
  std::string error;
  if (!mojo::test::ParseValidationTestInput(std::string(input), &data_vec,
                                            num_handles, &error)) {
    char* error_c_str = reinterpret_cast<char*>(malloc(error.size() + 1));
    strncpy(error_c_str, error.c_str(), error.size() + 1);
    return error_c_str;
  }

  *data_len = data_vec.size();
  *data = reinterpret_cast<std::uint8_t*>(malloc(data_vec.size()));
  memcpy(*data, data_vec.data(), data_vec.size());
  return nullptr;
}

}  // extern "C"
