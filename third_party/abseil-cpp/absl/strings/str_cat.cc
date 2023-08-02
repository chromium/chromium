// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/strings/str_cat.h"

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

// ----------------------------------------------------------------------
// StrCat()
//    This merges the given strings or integers, with no delimiter. This
//    is designed to be the fastest possible way to construct a string out
//    of a mix of raw C strings, string_views, strings, and integer values.
// ----------------------------------------------------------------------

namespace strings_internal {

bool HaveOverlap(const std::string& x, absl::string_view y) {
  if (y.empty()) return false;
  // TODO(b/290623057): Re-evaluate the check below: it detects when buffers
  // overlap (which is good) but it also introduces undefined behaviour when
  // buffers don't overlap (substracting pointers that do not belong to the same
  // array is UB [expr.add]). In other words, if compiler assumes that a program
  // never has UB, then it can replace `assert(HaveOverlap(x, y))` with
  // `assert(false)`.
  return (uintptr_t(y.data() - x.data()) <= uintptr_t(x.size()));
}

#if defined(__GNUC__) && !defined(__clang__)
char* AppendAlphaNum(char* dst, const AlphaNum& a) {
  return std::copy_n(a.data(), a.size(), dst);
}
#endif

}  // namespace strings_internal

ABSL_NAMESPACE_END
}  // namespace absl
