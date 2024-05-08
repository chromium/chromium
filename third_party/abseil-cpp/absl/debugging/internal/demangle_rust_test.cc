// Copyright 2024 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/debugging/internal/demangle_rust.h"

#include <cstddef>
#include <string>

#include "gtest/gtest.h"
#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace debugging_internal {
namespace {

// If DemangleRustSymbolEncoding(mangled, <buffer with room for buffer_size
// chars>, buffer_size) returns true and seems not to have overrun its output
// buffer, returns the string written by DemangleRustSymbolEncoding; otherwise
// returns an error message.
std::string ResultOfDemangling(const char* mangled, std::size_t buffer_size) {
  // Fill the buffer with something other than NUL so we test whether Demangle
  // appends trailing NUL as expected.
  std::string buffer(buffer_size + 1, '~');
  constexpr char kCanaryCharacter = 0x7f;  // arbitrary unlikely value
  buffer[buffer_size] = kCanaryCharacter;
  if (!DemangleRustSymbolEncoding(mangled, &buffer[0], buffer_size)) {
    return "Failed parse";
  }
  if (buffer[buffer_size] != kCanaryCharacter) {
    return "Buffer overrun by output: " + buffer.substr(0, buffer_size + 1)
        + "...";
  }
  return buffer.data();  // Not buffer itself: this trims trailing padding.
}

// Tests that DemangleRustSymbolEncoding converts mangled into plaintext given
// enough output buffer space but returns false and avoids overrunning a buffer
// that is one byte too short.
//
// The lambda wrapping allows ASSERT_EQ to branch out the first time an
// expectation is not satisfied, preventing redundant errors for the same bug.
//
// We test first with excess space so that if the algorithm just computes the
// wrong answer, it will be clear from the error log that the bounds checks are
// unlikely to be the code at fault.
#define EXPECT_DEMANGLING(mangled, plaintext) \
  do { \
    [] { \
      constexpr std::size_t plenty_of_space = sizeof(plaintext) + 128; \
      constexpr std::size_t just_enough_space = sizeof(plaintext); \
      constexpr std::size_t one_byte_too_few = sizeof(plaintext) - 1; \
      const char* expected_plaintext = plaintext; \
      const char* expected_error = "Failed parse"; \
      ASSERT_EQ(ResultOfDemangling(mangled, plenty_of_space), \
                expected_plaintext); \
      ASSERT_EQ(ResultOfDemangling(mangled, just_enough_space), \
                expected_plaintext); \
      ASSERT_EQ(ResultOfDemangling(mangled, one_byte_too_few), \
                expected_error); \
    }(); \
  } while (0)

// Tests that DemangleRustSymbolEncoding rejects the given input (typically, a
// truncation of a real Rust symbol name).
#define EXPECT_DEMANGLING_FAILS(mangled) \
    do { \
      constexpr std::size_t plenty_of_space = 1024; \
      const char* expected_error = "Failed parse"; \
      EXPECT_EQ(ResultOfDemangling(mangled, plenty_of_space), expected_error); \
    } while (0)

// Piping grep -C 1 _R demangle_test.cc into your favorite c++filt
// implementation allows you to verify that the goldens below are reasonable.

TEST(DemangleRust, EmptyDemangling) {
  EXPECT_TRUE(DemangleRustSymbolEncoding("_RC0", nullptr, 0));
}

TEST(DemangleRust, FunctionAtCrateLevel) {
  EXPECT_DEMANGLING("_RNvC10crate_name9func_name", "crate_name::func_name");
  EXPECT_DEMANGLING(
      "_RNvCs09azAZ_10crate_name9func_name", "crate_name::func_name");
}

TEST(DemangleRust, TruncationsOfFunctionAtCrateLevel) {
  EXPECT_DEMANGLING_FAILS("_R");
  EXPECT_DEMANGLING_FAILS("_RN");
  EXPECT_DEMANGLING_FAILS("_RNvC");
  EXPECT_DEMANGLING_FAILS("_RNvC10");
  EXPECT_DEMANGLING_FAILS("_RNvC10crate_nam");
  EXPECT_DEMANGLING_FAILS("_RNvC10crate_name");
  EXPECT_DEMANGLING_FAILS("_RNvC10crate_name9");
  EXPECT_DEMANGLING_FAILS("_RNvC10crate_name9func_nam");
  EXPECT_DEMANGLING_FAILS("_RNvCs");
  EXPECT_DEMANGLING_FAILS("_RNvCs09azAZ");
  EXPECT_DEMANGLING_FAILS("_RNvCs09azAZ_");
}

TEST(DemangleRust, VendorSuffixes) {
  EXPECT_DEMANGLING("_RNvC10crate_name9func_name.!@#", "crate_name::func_name");
  EXPECT_DEMANGLING("_RNvC10crate_name9func_name$!@#", "crate_name::func_name");
}

TEST(DemangleRust, UnicodeIdentifiers) {
  EXPECT_DEMANGLING("_RNvC7ice_cap17Eyjafjallajökull",
                    "ice_cap::Eyjafjallajökull");
  EXPECT_DEMANGLING("_RNvC7ice_caps_u19Eyjafjallajkull_jtb",
                    "ice_cap::{Punycode Eyjafjallajkull_jtb}");
}

TEST(DemangleRust, FunctionInModule) {
  EXPECT_DEMANGLING("_RNvNtCs09azAZ_10crate_name11module_name9func_name",
                    "crate_name::module_name::func_name");
}

TEST(DemangleRust, FunctionInFunction) {
  EXPECT_DEMANGLING(
      "_RNvNvCs09azAZ_10crate_name15outer_func_name15inner_func_name",
      "crate_name::outer_func_name::inner_func_name");
}

TEST(DemangleRust, ClosureInFunction) {
  EXPECT_DEMANGLING(
      "_RNCNvCs09azAZ_10crate_name9func_name0",
      "crate_name::func_name::{closure#0}");
  EXPECT_DEMANGLING(
      "_RNCNvCs09azAZ_10crate_name9func_name0Cs123_12client_crate",
      "crate_name::func_name::{closure#0}");
}

TEST(DemangleRust, ClosureNumbering) {
  EXPECT_DEMANGLING(
      "_RNCNvCs09azAZ_10crate_name9func_names_0Cs123_12client_crate",
      "crate_name::func_name::{closure#1}");
  EXPECT_DEMANGLING(
      "_RNCNvCs09azAZ_10crate_name9func_names0_0Cs123_12client_crate",
      "crate_name::func_name::{closure#2}");
  EXPECT_DEMANGLING(
      "_RNCNvCs09azAZ_10crate_name9func_names9_0Cs123_12client_crate",
      "crate_name::func_name::{closure#11}");
  EXPECT_DEMANGLING(
      "_RNCNvCs09azAZ_10crate_name9func_namesa_0Cs123_12client_crate",
      "crate_name::func_name::{closure#12}");
  EXPECT_DEMANGLING(
      "_RNCNvCs09azAZ_10crate_name9func_namesz_0Cs123_12client_crate",
      "crate_name::func_name::{closure#37}");
  EXPECT_DEMANGLING(
      "_RNCNvCs09azAZ_10crate_name9func_namesA_0Cs123_12client_crate",
      "crate_name::func_name::{closure#38}");
  EXPECT_DEMANGLING(
      "_RNCNvCs09azAZ_10crate_name9func_namesZ_0Cs123_12client_crate",
      "crate_name::func_name::{closure#63}");
  EXPECT_DEMANGLING(
      "_RNCNvCs09azAZ_10crate_name9func_names10_0Cs123_12client_crate",
      "crate_name::func_name::{closure#64}");
  EXPECT_DEMANGLING(
      "_RNCNvCs09azAZ_10crate_name9func_namesg6_0Cs123_12client_crate",
      "crate_name::func_name::{closure#1000}");
}

TEST(DemangleRust, ClosureNumberOverflowingInt) {
  EXPECT_DEMANGLING(
      "_RNCNvCs09azAZ_10crate_name9func_names1234567_0Cs123_12client_crate",
      "crate_name::func_name::{closure#?}");
}

TEST(DemangleRust, UnexpectedlyNamedClosure) {
  EXPECT_DEMANGLING(
      "_RNCNvCs123_10crate_name9func_name12closure_nameCs456_12client_crate",
      "crate_name::func_name::{closure:closure_name#0}");
  EXPECT_DEMANGLING(
      "_RNCNvCs123_10crate_name9func_names2_12closure_nameCs456_12client_crate",
      "crate_name::func_name::{closure:closure_name#4}");
}

TEST(DemangleRust, ItemNestedInsideClosure) {
  EXPECT_DEMANGLING(
      "_RNvNCNvCs123_10crate_name9func_name015inner_func_nameCs_12client_crate",
      "crate_name::func_name::{closure#0}::inner_func_name");
}

TEST(DemangleRust, Shim) {
  EXPECT_DEMANGLING(
      "_RNSNvCs123_10crate_name9func_name6vtableCs456_12client_crate",
      "crate_name::func_name::{shim:vtable#0}");
}

TEST(DemangleRust, UnknownUppercaseNamespace) {
  EXPECT_DEMANGLING(
      "_RNXNvCs123_10crate_name9func_name14mystery_objectCs456_12client_crate",
      "crate_name::func_name::{X:mystery_object#0}");
}

TEST(DemangleRust, NestedUppercaseNamespaces) {
  EXPECT_DEMANGLING(
      "_RNCNXNYCs123_10crate_names0_1ys1_1xs2_0Cs456_12client_crate",
      "crate_name::{Y:y#2}::{X:x#3}::{closure#4}");
}


}  // namespace
}  // namespace debugging_internal
ABSL_NAMESPACE_END
}  // namespace absl
