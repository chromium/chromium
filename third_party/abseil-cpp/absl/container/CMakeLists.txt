#
# Copyright 2017 The Abseil Authors.
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


list(APPEND CONTAINER_PUBLIC_HEADERS
  "fixed_array.h"
  "flat_hash_map.h"
  "flat_hash_set.h"
  "inlined_vector.h"
  "node_hash_map.h"
  "node_hash_set.h"
)


list(APPEND CONTAINER_INTERNAL_HEADERS
  "internal/compressed_tuple.h"
  "internal/container_memory.h"
  "internal/hash_function_defaults.h"
  "internal/hash_generator_testing.h"
  "internal/hash_policy_testing.h"
  "internal/hash_policy_traits.h"
  "internal/hashtable_debug.h"
  "internal/layout.h"
  "internal/node_hash_policy.h"
  "internal/raw_hash_map.h"
  "internal/raw_hash_set.h"
  "internal/test_instance_tracker.h"
  "internal/tracked.h"
  "internal/unordered_map_constructor_test.h"
  "internal/unordered_map_lookup_test.h"
  "internal/unordered_map_modifiers_test.h"
  "internal/unordered_set_constructor_test.h"
  "internal/unordered_set_lookup_test.h"
  "internal/unordered_set_modifiers_test.h"
)


absl_library(
  TARGET
    absl_container
  SOURCES
    "internal/raw_hash_set.cc"
  EXPORT_NAME
    container
)


#
## TESTS
#

list(APPEND TEST_INSTANCE_TRACKER_LIB_SRC
  "internal/test_instance_tracker.cc"
  ${CONTAINER_PUBLIC_HEADERS}
  ${CONTAINER_INTERNAL_HEADERS}
)


absl_library(
  TARGET
    test_instance_tracker_lib
  SOURCES
    ${TEST_INSTANCE_TRACKER_LIB_SRC}
  PUBLIC_LIBRARIES
    absl::container
)



# test fixed_array_test
set(FIXED_ARRAY_TEST_SRC "fixed_array_test.cc")
set(FIXED_ARRAY_TEST_PUBLIC_LIBRARIES absl::base absl_throw_delegate test_instance_tracker_lib)

absl_test(
  TARGET
    fixed_array_test
  SOURCES
    ${FIXED_ARRAY_TEST_SRC}
  PUBLIC_LIBRARIES
    ${FIXED_ARRAY_TEST_PUBLIC_LIBRARIES}
  PRIVATE_COMPILE_FLAGS
    ${ABSL_EXCEPTIONS_FLAG}
)



absl_test(
  TARGET
    fixed_array_test_noexceptions
  SOURCES
    ${FIXED_ARRAY_TEST_SRC}
  PUBLIC_LIBRARIES
    ${FIXED_ARRAY_TEST_PUBLIC_LIBRARIES}
)


# test fixed_array_exception_safety_test
set(FIXED_ARRAY_EXCEPTION_SAFETY_TEST_SRC "fixed_array_exception_safety_test.cc")
set(FIXED_ARRAY_EXCEPTION_SAFETY_TEST_PUBLIC_LIBRARIES
  absl::container
  absl_base_internal_exception_safety_testing
)

absl_test(
  TARGET
    fixed_array_exception_safety_test
  SOURCES
    ${FIXED_ARRAY_EXCEPTION_SAFETY_TEST_SRC}
  PUBLIC_LIBRARIES
    ${FIXED_ARRAY_EXCEPTION_SAFETY_TEST_PUBLIC_LIBRARIES}
  PRIVATE_COMPILE_FLAGS
    ${ABSL_EXCEPTIONS_FLAG}
)


# test inlined_vector_test
set(INLINED_VECTOR_TEST_SRC "inlined_vector_test.cc")
set(INLINED_VECTOR_TEST_PUBLIC_LIBRARIES absl::base absl_throw_delegate test_instance_tracker_lib)

absl_test(
  TARGET
    inlined_vector_test
  SOURCES
    ${INLINED_VECTOR_TEST_SRC}
  PUBLIC_LIBRARIES
    ${INLINED_VECTOR_TEST_PUBLIC_LIBRARIES}
)

absl_test(
  TARGET
    inlined_vector_test_noexceptions
  SOURCES
    ${INLINED_VECTOR_TEST_SRC}
  PUBLIC_LIBRARIES
    ${INLINED_VECTOR_TEST_PUBLIC_LIBRARIES}
  PRIVATE_COMPILE_FLAGS
    ${ABSL_NOEXCEPTION_CXXFLAGS}
)


# test test_instance_tracker_test
set(TEST_INSTANCE_TRACKER_TEST_SRC "internal/test_instance_tracker_test.cc")
set(TEST_INSTANCE_TRACKER_TEST_PUBLIC_LIBRARIES absl::base absl_throw_delegate test_instance_tracker_lib)


absl_test(
  TARGET
    test_instance_tracker_test
  SOURCES
    ${TEST_INSTANCE_TRACKER_TEST_SRC}
  PUBLIC_LIBRARIES
    ${TEST_INSTANCE_TRACKER_TEST_PUBLIC_LIBRARIES}
)


absl_test(
  TARGET
    raw_hash_set_test
  SOURCES
    "internal/raw_hash_set_test.cc"
  PUBLIC_LIBRARIES
    absl::base absl::hash absl_throw_delegate test_instance_tracker_lib
)
