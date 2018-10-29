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


list(APPEND STRINGS_PUBLIC_HEADERS
  "ascii.h"
  "charconv.h"
  "escaping.h"
  "match.h"
  "numbers.h"
  "str_cat.h"
  "string_view.h"
  "strip.h"
  "str_join.h"
  "str_replace.h"
  "str_split.h"
  "substitute.h"
)


list(APPEND STRINGS_INTERNAL_HEADERS
  "internal/char_map.h"
  "internal/charconv_bigint.h"
  "internal/charconv_parse.h"
  "internal/memutil.h"
  "internal/ostringstream.h"
  "internal/resize_uninitialized.h"
  "internal/stl_type_traits.h"
  "internal/str_join_internal.h"
  "internal/str_split_internal.h"
  "internal/utf8.h"
)



# add string library
list(APPEND STRINGS_SRC
  "ascii.cc"
  "charconv.cc"
  "escaping.cc"
  "internal/charconv_bigint.cc"
  "internal/charconv_parse.cc"
  "internal/memutil.cc"
  "internal/memutil.h"
  "internal/utf8.cc"
  "internal/ostringstream.cc"
  "match.cc"
  "numbers.cc"
  "str_cat.cc"
  "str_replace.cc"
  "str_split.cc"
  "string_view.cc"
  "substitute.cc"
  ${STRINGS_PUBLIC_HEADERS}
  ${STRINGS_INTERNAL_HEADERS}
)
set(STRINGS_PUBLIC_LIBRARIES absl::base absl_throw_delegate)

absl_library(
  TARGET
    absl_strings
  SOURCES
    ${STRINGS_SRC}
  PUBLIC_LIBRARIES
    ${STRINGS_PUBLIC_LIBRARIES}
  EXPORT_NAME
    strings
)

# add str_format library
absl_header_library(
  TARGET
    absl_str_format
  PUBLIC_LIBRARIES
    str_format_internal
  EXPORT_NAME
    str_format
)

# str_format_internal
absl_library(
  TARGET
    str_format_internal
  SOURCES
    "internal/str_format/arg.cc"
    "internal/str_format/bind.cc"
    "internal/str_format/extension.cc"
    "internal/str_format/float_conversion.cc"
    "internal/str_format/output.cc"
    "internal/str_format/parser.cc"
    "internal/str_format/arg.h"
    "internal/str_format/bind.h"
    "internal/str_format/checker.h"
    "internal/str_format/extension.h"
    "internal/str_format/float_conversion.h"
    "internal/str_format/output.h"
    "internal/str_format/parser.h"
  PUBLIC_LIBRARIES
    str_format_extension_internal
    absl::strings
    absl::base
    absl::numeric
    absl::container
    absl::span
)

# str_format_extension_internal
absl_library(
  TARGET
    str_format_extension_internal
  SOURCES
    "internal/str_format/extension.cc"
    "internal/str_format/extension.h"
    "internal/str_format/output.cc"
    "internal/str_format/output.h"
  PUBLIC_LIBRARIES
    absl::base
    absl::strings
)

#
## TESTS
#

# test match_test
set(MATCH_TEST_SRC "match_test.cc")
set(MATCH_TEST_PUBLIC_LIBRARIES absl::strings)

absl_test(
  TARGET
    match_test
  SOURCES
    ${MATCH_TEST_SRC}
  PUBLIC_LIBRARIES
    ${MATCH_TEST_PUBLIC_LIBRARIES}
)


# test escaping_test
set(ESCAPING_TEST_SRC "escaping_test.cc")
set(ESCAPING_TEST_PUBLIC_LIBRARIES absl::strings absl::base)

absl_test(
  TARGET
    escaping_test
  SOURCES
    ${ESCAPING_TEST_SRC}
  PUBLIC_LIBRARIES
    ${ESCAPING_TEST_PUBLIC_LIBRARIES}
)


# test ascii_test
set(ASCII_TEST_SRC "ascii_test.cc")
set(ASCII_TEST_PUBLIC_LIBRARIES absl::strings)

absl_test(
  TARGET
    ascii_test
  SOURCES
    ${ASCII_TEST_SRC}
  PUBLIC_LIBRARIES
    ${ASCII_TEST_PUBLIC_LIBRARIES}
)


# test memutil_test
set(MEMUTIL_TEST_SRC "internal/memutil_test.cc")
set(MEMUTIL_TEST_PUBLIC_LIBRARIES absl::strings)

absl_test(
  TARGET
    memutil_test
  SOURCES
    ${MEMUTIL_TEST_SRC}
  PUBLIC_LIBRARIES
    ${MEMUTIL_TEST_PUBLIC_LIBRARIES}
)


# test utf8_test
set(UTF8_TEST_SRC "internal/utf8_test.cc")
set(UTF8_TEST_PUBLIC_LIBRARIES absl::strings absl::base)

absl_test(
  TARGET
    utf8_test
  SOURCES
    ${UTF8_TEST_SRC}
  PUBLIC_LIBRARIES
    ${UTF8_TEST_PUBLIC_LIBRARIES}
)


# test string_view_test
set(STRING_VIEW_TEST_SRC "string_view_test.cc")
set(STRING_VIEW_TEST_PUBLIC_LIBRARIES absl::strings absl_throw_delegate absl::base)

absl_test(
  TARGET
    string_view_test
  SOURCES
    ${STRING_VIEW_TEST_SRC}
  PUBLIC_LIBRARIES
    ${STRING_VIEW_TEST_PUBLIC_LIBRARIES}
)


# test substitute_test
set(SUBSTITUTE_TEST_SRC "substitute_test.cc")
set(SUBSTITUTE_TEST_PUBLIC_LIBRARIES absl::strings absl::base)

absl_test(
  TARGET
    substitute_test
  SOURCES
    ${SUBSTITUTE_TEST_SRC}
  PUBLIC_LIBRARIES
    ${SUBSTITUTE_TEST_PUBLIC_LIBRARIES}
)


# test str_replace_test
set(STR_REPLACE_TEST_SRC "str_replace_test.cc")
set(STR_REPLACE_TEST_PUBLIC_LIBRARIES absl::strings absl::base absl_throw_delegate)

absl_test(
  TARGET
    str_replace_test
  SOURCES
    ${STR_REPLACE_TEST_SRC}
  PUBLIC_LIBRARIES
    ${STR_REPLACE_TEST_PUBLIC_LIBRARIES}
)


# test str_split_test
set(STR_SPLIT_TEST_SRC "str_split_test.cc")
set(STR_SPLIT_TEST_PUBLIC_LIBRARIES absl::strings absl::base absl_throw_delegate)

absl_test(
  TARGET
    str_split_test
  SOURCES
    ${STR_SPLIT_TEST_SRC}
  PUBLIC_LIBRARIES
    ${STR_SPLIT_TEST_PUBLIC_LIBRARIES}
)


# test ostringstream_test
set(OSTRINGSTREAM_TEST_SRC "internal/ostringstream_test.cc")
set(OSTRINGSTREAM_TEST_PUBLIC_LIBRARIES absl::strings)

absl_test(
  TARGET
    ostringstream_test
  SOURCES
    ${OSTRINGSTREAM_TEST_SRC}
  PUBLIC_LIBRARIES
    ${OSTRINGSTREAM_TEST_PUBLIC_LIBRARIES}
)


# test resize_uninitialized_test
set(RESIZE_UNINITIALIZED_TEST_SRC "internal/resize_uninitialized_test.cc")

absl_test(
  TARGET
    resize_uninitialized_test
  SOURCES
    ${RESIZE_UNINITIALIZED_TEST_SRC}
)


# test str_join_test
set(STR_JOIN_TEST_SRC "str_join_test.cc")
set(STR_JOIN_TEST_PUBLIC_LIBRARIES absl::strings)

absl_test(
  TARGET
    str_join_test
  SOURCES
    ${STR_JOIN_TEST_SRC}
  PUBLIC_LIBRARIES
    ${STR_JOIN_TEST_PUBLIC_LIBRARIES}
)


# test str_cat_test
set(STR_CAT_TEST_SRC "str_cat_test.cc")
set(STR_CAT_TEST_PUBLIC_LIBRARIES absl::strings)

absl_test(
  TARGET
    str_cat_test
  SOURCES
    ${STR_CAT_TEST_SRC}
  PUBLIC_LIBRARIES
    ${STR_CAT_TEST_PUBLIC_LIBRARIES}
)


# test numbers_test
set(NUMBERS_TEST_SRC "numbers_test.cc")
set(NUMBERS_TEST_PUBLIC_LIBRARIES absl::strings)

absl_test(
  TARGET
    numbers_test
  SOURCES
    ${NUMBERS_TEST_SRC}
  PUBLIC_LIBRARIES
    ${NUMBERS_TEST_PUBLIC_LIBRARIES}
)


# test strip_test
set(STRIP_TEST_SRC "strip_test.cc")
set(STRIP_TEST_PUBLIC_LIBRARIES absl::strings)

absl_test(
  TARGET
    strip_test
  SOURCES
    ${STRIP_TEST_SRC}
  PUBLIC_LIBRARIES
    ${STRIP_TEST_PUBLIC_LIBRARIES}
)


# test char_map_test
set(CHAR_MAP_TEST_SRC "internal/char_map_test.cc")
set(CHAR_MAP_TEST_PUBLIC_LIBRARIES absl::strings)

absl_test(
  TARGET
    char_map_test
  SOURCES
    ${CHAR_MAP_TEST_SRC}
  PUBLIC_LIBRARIES
    ${CHAR_MAP_TEST_PUBLIC_LIBRARIES}
)


# test charconv_test
set(CHARCONV_TEST_SRC "charconv_test.cc")
set(CHARCONV_TEST_PUBLIC_LIBRARIES absl::strings)

absl_test(
  TARGET
    charconv_test
  SOURCES
    ${CHARCONV_TEST_SRC}
  PUBLIC_LIBRARIES
    ${CHARCONV_TEST_PUBLIC_LIBRARIES}
)


# test charconv_parse_test
set(CHARCONV_PARSE_TEST_SRC "internal/charconv_parse_test.cc")
set(CHARCONV_PARSE_TEST_PUBLIC_LIBRARIES absl::strings)

absl_test(
  TARGET
    charconv_parse_test
  SOURCES
    ${CHARCONV_PARSE_TEST_SRC}
  PUBLIC_LIBRARIES
    ${CHARCONV_PARSE_TEST_PUBLIC_LIBRARIES}
)


# test charconv_bigint_test
set(CHARCONV_BIGINT_TEST_SRC "internal/charconv_bigint_test.cc")
set(CHARCONV_BIGINT_TEST_PUBLIC_LIBRARIES absl::strings)

absl_test(
  TARGET
    charconv_bigint_test
  SOURCES
    ${CHARCONV_BIGINT_TEST_SRC}
  PUBLIC_LIBRARIES
    ${CHARCONV_BIGINT_TEST_PUBLIC_LIBRARIES}
)
# test str_format_test
absl_test(
  TARGET
    str_format_test
  SOURCES
    "str_format_test.cc"
  PUBLIC_LIBRARIES
    absl::base
    absl::str_format
    absl::strings
)

# test str_format_bind_test
absl_test(
  TARGET
    str_format_bind_test
  SOURCES
    "internal/str_format/bind_test.cc"
  PUBLIC_LIBRARIES
    str_format_internal
)

# test str_format_checker_test
absl_test(
  TARGET
    str_format_checker_test
  SOURCES
    "internal/str_format/checker_test.cc"
  PUBLIC_LIBRARIES
    absl::str_format
)

# test str_format_convert_test
absl_test(
  TARGET
    str_format_convert_test
  SOURCES
    "internal/str_format/convert_test.cc"
  PUBLIC_LIBRARIES
    str_format_internal
    absl::numeric
)

# test str_format_output_test
absl_test(
  TARGET
    str_format_output_test
  SOURCES
    "internal/str_format/output_test.cc"
  PUBLIC_LIBRARIES
    str_format_extension_internal
)

# test str_format_parser_test
absl_test(
  TARGET
    str_format_parser_test
  SOURCES
    "internal/str_format/parser_test.cc"
  PUBLIC_LIBRARIES
    str_format_internal
    absl::base
)


