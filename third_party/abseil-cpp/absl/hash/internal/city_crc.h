// Copyright 2018 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// This file declares the subset of the CityHash functions that require
// _mm_crc32_u64().  See the CityHash README for details.
//
// Functions in the CityHash family are not suitable for cryptography.

#ifndef ABSL_HASH_INTERNAL_CITY_CRC_H_
#define ABSL_HASH_INTERNAL_CITY_CRC_H_

#include "absl/hash/internal/city.h"

namespace absl {
namespace hash_internal {

// Hash function for a byte array.
uint128 CityHashCrc128(const char *s, size_t len);

// Hash function for a byte array.  For convenience, a 128-bit seed is also
// hashed into the result.
uint128 CityHashCrc128WithSeed(const char *s, size_t len, uint128 seed);

// Hash function for a byte array.  Sets result[0] ... result[3].
void CityHashCrc256(const char *s, size_t len, uint64_t *result);

}  // namespace hash_internal
}  // namespace absl

#endif  // ABSL_HASH_INTERNAL_CITY_CRC_H_
