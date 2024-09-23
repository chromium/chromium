// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_RUST_TESTS_TEST_SUPPORT_H_
#define MOJO_PUBLIC_RUST_TESTS_TEST_SUPPORT_H_

#include <cstdint>

#include "mojo/public/cpp/bindings/tests/validation_test_input_parser.h"

// Only for the standalone Rust tests. When Rust Mojo code is integrated into
// the wider Chrome build, this will not be used since Chrome initializes Mojo
// itself.
void InitializeMojoEmbedder(std::uint32_t argc, const char* const* argv);

#endif  // MOJO_PUBLIC_RUST_TESTS_TEST_SUPPORT_H_
