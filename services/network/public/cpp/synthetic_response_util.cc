// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/synthetic_response_util.h"

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "mojo/public/cpp/system/result_for_metrics.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/features.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace network {

namespace {

// Returns a vector of header names to be ignored, parsed from the feature flag.
const std::vector<std::string>& GetIgnoredHeadersForSyntheticResponse() {
  static const base::NoDestructor<std::vector<std::string>>
      ignored_headers_list([]() {
        const std::string ignored_headers_str =
            features::kServiceWorkerSyntheticResponseIgnoredHeaders.Get();
        return base::SplitString(ignored_headers_str, ",",
                                 base::TRIM_WHITESPACE,
                                 base::SPLIT_WANT_NONEMPTY);
      }());
  return *ignored_headers_list;
}

void MaybeReportHeaderInconsistency(
    const absl::flat_hash_map<std::string, std::vector<std::string>>
        expected_headers,
    const absl::flat_hash_map<std::string, std::vector<std::string>>
        actual_headers) {
  if (!features::kServiceWorkerSyntheticResponseReportInconsistentHeader
           .Get()) {
    return;
  }

  for (const auto& item : expected_headers) {
    if (!actual_headers.contains(item.first)) {
      // The header doesn't exist.
      SCOPED_CRASH_KEY_STRING256("SyntheticResponse", "NoHeader", item.first);
      base::debug::DumpWithoutCrashing();
    } else if (actual_headers.at(item.first) != item.second) {
      // The header value is wrong.
      SCOPED_CRASH_KEY_STRING256("SyntheticResponse", "WrongHeader",
                                 item.first);
      SCOPED_CRASH_KEY_STRING256(
          "SyntheticResponse", "IncomingValue",
          base::JoinString(actual_headers.at(item.first), ","));
      SCOPED_CRASH_KEY_STRING256("SyntheticResponse", "StoredValue",
                                 base::JoinString(item.second, ","));
      base::debug::DumpWithoutCrashing();
    }
  }
  for (const auto& item : actual_headers) {
    if (!expected_headers.contains(item.first)) {
      // Unexpected header exists.
      SCOPED_CRASH_KEY_STRING256("SyntheticResponse", "NotExpectedHeader",
                                 item.first);
      SCOPED_CRASH_KEY_STRING256("SyntheticResponse", "NotExpectedValue",
                                 base::JoinString(item.second, ","));
      base::debug::DumpWithoutCrashing();
    }
  }
}

bool CheckHeaderConsistencyForSyntheticResponseImpl(
    const net::HttpResponseHeaders& actual_headers,
    const net::HttpResponseHeaders& expected_headers,
    const std::vector<std::string>& ignored_headers) {
  auto collect_significant_headers =
      [&](const net::HttpResponseHeaders& headers) {
        absl::flat_hash_map<std::string, std::vector<std::string>> collected;
        size_t iter = 0;
        std::string name;
        std::string value;
        while (headers.EnumerateHeaderLines(&iter, &name, &value)) {
          bool is_ignored = false;
          for (const auto& ignored_header : ignored_headers) {
            if (base::EqualsCaseInsensitiveASCII(name, ignored_header)) {
              is_ignored = true;
              break;
            }
          }

          if (!is_ignored) {
            // Normalize header names to lowercase for case-insensitive map
            // keys.
            collected[base::ToLowerASCII(name)].push_back(value);
          }
        }

        for (auto& item : collected) {
          auto& values = item.second;
          // Optimize the common case of single-value headers.
          if (values.size() < 2) {
            continue;
          }
          // Sort and remove duplicate values for the same header name.
          std::ranges::sort(values);
          auto [first, last] = std::ranges::unique(values);
          values.erase(first, last);
        }

        return collected;
      };

  auto significant_actual_headers = collect_significant_headers(actual_headers);
  auto significant_expected_headers =
      collect_significant_headers(expected_headers);

  bool result = significant_actual_headers == significant_expected_headers;
  if (!result) {
    MaybeReportHeaderInconsistency(significant_actual_headers,
                                   significant_expected_headers);
  }

  return result;
}

}  // namespace

bool CheckHeaderConsistencyForSyntheticResponse(
    const net::HttpResponseHeaders& actual_headers,
    const net::HttpResponseHeaders& expected_headers) {
  return CheckHeaderConsistencyForSyntheticResponseImpl(
      actual_headers, expected_headers,
      GetIgnoredHeadersForSyntheticResponse());
}

bool CheckHeaderConsistencyForSyntheticResponseForTesting(  // IN-TEST
    const net::HttpResponseHeaders& actual_headers,
    const net::HttpResponseHeaders& expected_headers,
    const std::vector<std::string>& ignored_headers) {
  return CheckHeaderConsistencyForSyntheticResponseImpl(
      actual_headers, expected_headers, ignored_headers);
}

WriteSyntheticResponseFallbackResult WriteSyntheticResponseFallbackBody(
    mojo::ScopedDataPipeProducerHandle& response_body_stream) {
  CHECK(response_body_stream.is_valid());
  static constexpr std::string_view kFallbackBody =
      "<meta http-equiv=\"refresh\" content=\"0;\" />";
  size_t num_bytes = 0;
  MojoResult result = response_body_stream->WriteData(
      base::as_byte_span(kFallbackBody), MOJO_WRITE_DATA_FLAG_ALL_OR_NONE,
      num_bytes);
  base::UmaHistogramEnumeration(
      "ServiceWorker.SyntheticResponse.WriteFallbackBodyResult",
      mojo::MojoResultToMetricsEnum(result));

  return {result, num_bytes};
}

}  // namespace network
