include_rules = [
  "+crypto",
  "+mojo/public",
  "+net/net_jni_headers",
  "+third_party/apple_apsl",
  "+third_party/boringssl/src/include",
  "+third_party/nss",
  "+third_party/protobuf/src/google/protobuf",
  "+third_party/zlib",

  # Most of net should not depend on icu, and brotli to keep size down when
  # built as a library.
  "-base/i18n",
  "-third_party/brotli",
  "-third_party/icu",
]

specific_include_rules = {
  # Within net, only used by file: requests.
  "directory_lister(\.cc|_unittest\.cc)": [
    "+base/i18n",
  ],

  # Functions largely not used by the rest of net.
  "directory_listing\.cc": [
    "+base/i18n",
  ],

  # Within net, only used by file: requests.
  "filename_util_icu\.cc": [
    "+base/i18n/file_util_icu.h",
  ],

  # Consolidated string functions that depend on icu.
  "net_string_util_icu\.cc": [
    "+base/i18n/case_conversion.h",
    "+base/i18n/i18n_constants.h",
    "+base/i18n/icu_string_conversions.h",
    "+third_party/icu/source/common/unicode/ucnv.h"
  ],

  "websocket_channel\.h": [
    "+base/i18n",
  ],

  "ftp_util\.cc": [
    "+base/i18n",
    "+third_party/icu",
  ],
  "ftp_directory_listing_parser\.cc": [
    "+base/i18n",
  ],

  "run_all_unittests\.cc": [
    "+mojo/core/embedder",
  ],

  "brotli_source_stream\.cc": [
    "+third_party/brotli",
  ],

  "ssl_client_socket_impl\.cc": [
    "+third_party/brotli",
  ],

  "fuzzer_test_support.cc": [
    "+base/i18n",
  ],

  # Dependencies specific for fuzz targets and other fuzzing-related code.
  ".*fuzz.*": [
    "+third_party/libprotobuf-mutator",  # This is needed for LPM-based fuzzers.
  ]
}

skip_child_includes = [
  "third_party",
]
