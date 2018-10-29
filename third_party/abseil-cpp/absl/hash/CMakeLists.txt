#
# Copyright 2018 The Abseil Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

list(APPEND HASH_PUBLIC_HEADERS
  "hash.h"
)

list(APPEND HASH_INTERNAL_HEADERS
  "internal/city.h"
  "internal/city_crc.h"
  "internal/hash.h"
)

# absl_hash library
list(APPEND HASH_SRC
  "internal/city.cc"
  "internal/hash.cc"
  ${HASH_PUBLIC_HEADERS}
  ${HASH_INTERNAL_HEADERS}
)

set(HASH_PUBLIC_LIBRARIES absl::hash absl::container absl::strings absl::str_format absl::utility)

absl_library(
  TARGET
    absl_hash
  SOURCES
    ${HASH_SRC}
  PUBLIC_LIBRARIES
    ${HASH_PUBLIC_LIBRARIES}
  EXPORT_NAME
    hash
)

#
## TESTS
#

# testing support
set(HASH_TEST_HEADERS hash_testing.h internal/spy_hash_state.h)
set(HASH_TEST_PUBLIC_LIBRARIES absl::hash absl::container absl::numeric absl::strings absl::str_format)

# hash_test
set(HASH_TEST_SRC "hash_test.cc" ${HASH_TEST_HEADERS})

absl_test(
  TARGET
    hash_test
  SOURCES
    ${HASH_TEST_SRC}
  PUBLIC_LIBRARIES
    ${HASH_TEST_PUBLIC_LIBRARIES}
)

# hash_test
set(CITY_TEST_SRC "internal/city_test.cc")

absl_test(
  TARGET
    city_test
  SOURCES
    ${CITY_TEST_SRC}
  PUBLIC_LIBRARIES
    ${HASH_TEST_PUBLIC_LIBRARIES}
)


