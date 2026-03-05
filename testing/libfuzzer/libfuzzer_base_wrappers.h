// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_LIBFUZZER_LIBFUZZER_BASE_WRAPPERS_H_
#define TESTING_LIBFUZZER_LIBFUZZER_BASE_WRAPPERS_H_

#include <stddef.h>
#include <stdint.h>

// Deliberately do not include "base/containers/span.h" so that users are
// forced to include it themselves.
#include "base/compiler_specific.h"

// Defines an `LLVMFuzzerTestOneInput` function that accepts a `base::span<const
// uint8_t>` rather than a pointer-and-size pair, avoiding the need for manual
// construction of such spans.
//
// Example usage:
//
// ```cpp
// #include "base/containers/span.h"
// #include "base/strings/string_view_util.h"
// #include "testing/libfuzzer/libfuzzer_base_wrappers.h"
//
// DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> data) {
//   SomeFunctionAcceptingSpan(data);
//   SomeFunctionAcceptingStringView(base::as_string_view(data));
//   SomeFunctionAcceptingSpanOfChars(base::as_chars(data));
//   return 0;
// }
// ```
#define DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(arg)                         \
  static int TestOneInput(arg);                                             \
  extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) { \
    /* SAFETY: Inputs are supplied by the fuzzer infrastructure. */         \
    return TestOneInput(                                                    \
        UNSAFE_BUFFERS(base::span<const uint8_t>(data, size)));             \
  }                                                                         \
  static int TestOneInput(arg)

#endif  // TESTING_LIBFUZZER_LIBFUZZER_BASE_WRAPPERS_H_
