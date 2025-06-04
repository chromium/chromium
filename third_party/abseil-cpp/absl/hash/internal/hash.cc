// Copyright 2018 The Abseil Authors.
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

#include "absl/hash/internal/hash.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/hash/internal/city.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace hash_internal {

uint64_t MixingHashState::CombineLargeContiguousImpl32(
    const unsigned char* first, size_t len, uint64_t state) {
  while (len >= PiecewiseChunkSize()) {
    // TODO(b/417141985): avoid code duplication with CombineContiguousImpl.
    state =
        Mix(PrecombineLengthMix(state, PiecewiseChunkSize()) ^
                hash_internal::CityHash32(reinterpret_cast<const char*>(first),
                                          PiecewiseChunkSize()),
            kMul);
    len -= PiecewiseChunkSize();
    first += PiecewiseChunkSize();
  }
  // Do not call CombineContiguousImpl for empty range since it is modifying
  // state.
  if (len == 0) {
    return state;
  }
  // Handle the remainder.
  return CombineContiguousImpl(state, first, len,
                               std::integral_constant<int, 4>{});
}

uint64_t MixingHashState::CombineLargeContiguousImpl64(
    const unsigned char* first, size_t len, uint64_t state) {
  while (len >= PiecewiseChunkSize()) {
    state = Hash64(first, PiecewiseChunkSize(), state);
    len -= PiecewiseChunkSize();
    first += PiecewiseChunkSize();
  }
  // Do not call CombineContiguousImpl for empty range since it is modifying
  // state.
  if (len == 0) {
    return state;
  }
  // Handle the remainder.
  return CombineContiguousImpl(state, first, len,
                               std::integral_constant<int, 8>{});
}

ABSL_CONST_INIT const void* const MixingHashState::kSeed = &kSeed;

}  // namespace hash_internal
ABSL_NAMESPACE_END
}  // namespace absl
