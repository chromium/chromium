// Copyright 2022 The Abseil Authors
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

#ifndef ABSL_STRINGS_INTERNAL_HAS_ABSL_STRINGIFY_H_
#define ABSL_STRINGS_INTERNAL_HAS_ABSL_STRINGIFY_H_
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/has_absl_stringify.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

namespace strings_internal {

template <typename T, typename = void>
struct ABSL_DEPRECATED("Use absl::HasAbslStringify") HasAbslStringify
    : std::false_type {};

template <typename T>
struct ABSL_DEPRECATED("Use absl::HasAbslStringify") HasAbslStringify<
    T, std::enable_if_t<std::is_void<decltype(AbslStringify(
           std::declval<strings_internal::UnimplementedSink&>(),
           std::declval<const T&>()))>::value>> : std::true_type {};

}  // namespace strings_internal

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_STRINGS_INTERNAL_HAS_ABSL_STRINGIFY_H_
