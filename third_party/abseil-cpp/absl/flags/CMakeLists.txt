#
# Copyright 2019 The Abseil Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Internal-only target, do not depend on directly.
absl_cc_library(
  NAME
    flags_internal
  SRCS
    "internal/program_name.cc"
  HDRS
    "internal/path_util.h"
    "internal/program_name.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  LINKOPTS
    ${ABSL_DEFAULT_LINKOPTS}
  DEPS
    absl::strings
    absl::synchronization
  PUBLIC
)

absl_cc_library(
  NAME
    flags_config
  SRCS
    "usage_config.cc"
  HDRS
    "config.h"
    "usage_config.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  LINKOPTS
    ${ABSL_DEFAULT_LINKOPTS}
  DEPS
    absl::flags_internal
    absl::core_headers
    absl::strings
    absl::synchronization
)

absl_cc_library(
  NAME
    flags_marshalling
  SRCS
    "marshalling.cc"
  HDRS
    "marshalling.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  LINKOPTS
    ${ABSL_DEFAULT_LINKOPTS}
  DEPS
    absl::core_headers
    absl::strings
    absl::str_format
)

# Internal-only target, do not depend on directly.
absl_cc_library(
  NAME
    flags_handle
  SRCS
    "internal/commandlineflag.cc"
  HDRS
    "internal/commandlineflag.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  LINKOPTS
    ${ABSL_DEFAULT_LINKOPTS}
  DEPS
    absl::flags_config
    absl::flags_marshalling
    absl::base
    absl::core_headers
    absl::strings
    absl::synchronization
    absl::optional
)

# Internal-only target, do not depend on directly.
absl_cc_library(
  NAME
    flags_registry
  SRCS
    "internal/registry.cc"
    "internal/type_erased.cc"
  HDRS
    "internal/registry.h"
    "internal/type_erased.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  LINKOPTS
    ${ABSL_DEFAULT_LINKOPTS}
  DEPS
    absl::flags_config
    absl::flags_handle
    absl::base
    absl::core_headers
    absl::dynamic_annotations
    absl::strings
    absl::synchronization
)

absl_cc_library(
  NAME
    flags
  SRCS
    "flag.cc"
  HDRS
    "declare.h"
    "flag.h"
    "internal/flag.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  LINKOPTS
    ${ABSL_DEFAULT_LINKOPTS}
  DEPS
    absl::flags_config
    absl::flags_handle
    absl::flags_marshalling
    absl::flags_registry
    absl::base
    absl::core_headers
    absl::strings
)

# Internal-only target, do not depend on directly.
absl_cc_library(
  NAME
    flags_usage_internal
  SRCS
    "internal/usage.cc"
  HDRS
    "internal/usage.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  LINKOPTS
    ${ABSL_DEFAULT_LINKOPTS}
  DEPS
    absl::flags_config
    absl::flags
    absl::flags_handle
    absl::flags_internal
    absl::strings
    absl::synchronization
)

absl_cc_library(
  NAME
    flags_usage
  SRCS
    "usage.cc"
  HDRS
    "usage.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  LINKOPTS
    ${ABSL_DEFAULT_LINKOPTS}
  DEPS
    absl::flags_usage_internal
    absl::strings
    absl::synchronization
)

absl_cc_library(
  NAME
    flags_parse
  SRCS
    "parse.cc"
  HDRS
    "internal/parse.h"
    "parse.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  LINKOPTS
    ${ABSL_DEFAULT_LINKOPTS}
  DEPS
    absl::flags_config
    absl::flags
    absl::flags_handle
    absl::flags_internal
    absl::flags_registry
    absl::flags_usage
    absl::strings
    absl::synchronization
)

############################################################################
# Unit tests in alpahabetical order.

absl_cc_test(
  NAME
    flags_commandlineflag_test
  SRCS
    "internal/commandlineflag_test.cc"
  COPTS
    ${ABSL_TEST_COPTS}
  DEPS
    absl::flags
    absl::flags_handle
    absl::flags_registry
    absl::memory
    absl::strings
    gtest_main
)

absl_cc_test(
  NAME
    flags_config_test
  SRCS
    "config_test.cc"
  COPTS
    ${ABSL_TEST_COPTS}
  DEPS
    absl::flags_config
    gtest_main
)

absl_cc_test(
  NAME
    flags_flag_test
  SRCS
    "flag_test.cc"
    "flag_test_defs.cc"
  COPTS
    ${ABSL_TEST_COPTS}
  DEPS
    absl::flags
    absl::strings
    gtest_main
)

absl_cc_test(
  NAME
    flags_marshalling_test
  SRCS
    "marshalling_test.cc"
  COPTS
    ${ABSL_TEST_COPTS}
  DEPS
    absl::flags_marshalling
    gtest_main
)

absl_cc_test(
  NAME
    flags_parse_test
  SRCS
    "parse_test.cc"
  COPTS
    ${ABSL_TEST_COPTS}
  DEPS
    absl::flags
    absl::base
    absl::flags_parse
    absl::scoped_set_env
    absl::span
    absl::strings
    gmock_main
)

absl_cc_test(
  NAME
    flags_path_util_test
  SRCS
    "internal/path_util_test.cc"
  COPTS
    ${ABSL_TEST_COPTS}
  DEPS
    absl::flags_internal
    gtest_main
)

absl_cc_test(
  NAME
    flags_program_name_test
  SRCS
    "internal/program_name_test.cc"
  COPTS
    ${ABSL_TEST_COPTS}
  DEPS
    absl::flags_internal
    absl::strings
    gtest_main
)

absl_cc_test(
  NAME
    flags_type_erased_test
  SRCS
    "internal/type_erased_test.cc"
  COPTS
    ${ABSL_TEST_COPTS}
  DEPS
    absl::flags
    absl::flags_registry
    absl::memory
    absl::strings
    gtest_main
)

absl_cc_test(
  NAME
    flags_usage_config_test
  SRCS
    "usage_config_test.cc"
  COPTS
    ${ABSL_TEST_COPTS}
  DEPS
    absl::flags_config
    absl::flags_internal
    absl::strings
    gtest_main
)

absl_cc_test(
  NAME
    flags_usage_test
  SRCS
    "internal/usage_test.cc"
  COPTS
    ${ABSL_TEST_COPTS}
  DEPS
    absl::flags_config
    absl::flags
    absl::flags_internal
    absl::flags_parse
    absl::flags_usage
    absl::memory
    absl::strings
    gtest
)
