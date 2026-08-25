// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/structured_headers.h"

#include <optional>
#include <string_view>

#include "base/feature.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_view_rust.h"
#include "base/time/time.h"
#include "third_party/rust/sfv/v0_15/wrapper/functions.h"
#include "third_party/rust/sfv/v0_15/wrapper/lib.rs.h"

// This namespace defines FFI-friendly functions that are called from Rust in
// //third_party/rust/sfv/v0_15/wrapper/.
namespace sfv {

Member& list_append_member(List& list) {
  return list.emplace_back();
}

Member& dictionary_reset_key(Dictionary& dictionary, rust::Str key) {
  Member& member = dictionary[base::RustStrToStringView(key)];
  member = net::structured_headers::ParameterizedMember();
  return member;
}

void set_member_boolean(Member& member, bool v) {
  member.member.emplace_back().item = net::structured_headers::Item(v);
}

void set_member_integer(Member& member, int64_t v) {
  member.member.emplace_back().item = net::structured_headers::Item(v);
}

void set_member_decimal(Member& member, double v) {
  member.member.emplace_back().item = net::structured_headers::Item(v);
}

void set_member_string(Member& member, rust::Str v) {
  member.member.emplace_back().item =
      net::structured_headers::Item(std::string(v));
}

void set_member_token(Member& member, rust::Str v) {
  member.member.emplace_back().item = net::structured_headers::Item(
      std::string(v), net::structured_headers::Item::kTokenType);
}

void set_member_byte_sequence(Member& member, rust::Slice<const uint8_t> v) {
  member.member.emplace_back().item = net::structured_headers::Item(
      std::string(v.begin(), v.end()),
      net::structured_headers::Item::kByteSequenceType);
}

void set_member_inner_list(Member& member) {
  member.member_is_inner_list = true;
}

// Parameters is a type alias in net::structured_headers, so it cannot be
// forward-declared. To keep the FFI header (functions.h) clean, we use an
// opaque tag class there and reinterpret_cast it here to the actual type.
Parameters& get_member_params(Member& member) {
  return *reinterpret_cast<Parameters*>(&member.params);
}

Parameters& get_item_params(Member& member) {
  return *reinterpret_cast<Parameters*>(&member.member.back().params);
}

namespace {
void set_parameter(Parameters& parameters,
                   rust::Str key,
                   net::structured_headers::Item value) {
  auto& params =
      reinterpret_cast<net::structured_headers::Parameters&>(parameters);
  std::string_view key_view = base::RustStrToStringView(key);
  for (auto& param : params) {
    if (param.first == key_view) {
      param.second = std::move(value);
      return;
    }
  }
  params.emplace_back(std::string(key_view), std::move(value));
}
}  // namespace

void set_parameter_boolean(Parameters& params, rust::Str key, bool v) {
  set_parameter(params, key, net::structured_headers::Item(v));
}

void set_parameter_integer(Parameters& params, rust::Str key, int64_t v) {
  set_parameter(params, key, net::structured_headers::Item(v));
}

void set_parameter_decimal(Parameters& params, rust::Str key, double v) {
  set_parameter(params, key, net::structured_headers::Item(v));
}

void set_parameter_string(Parameters& params, rust::Str key, rust::Str v) {
  set_parameter(params, key, net::structured_headers::Item(std::string(v)));
}

void set_parameter_token(Parameters& params, rust::Str key, rust::Str v) {
  set_parameter(params, key,
                net::structured_headers::Item(
                    std::string(v), net::structured_headers::Item::kTokenType));
}

void set_parameter_byte_sequence(Parameters& params,
                                 rust::Str key,
                                 rust::Slice<const uint8_t> v) {
  set_parameter(params, key,
                net::structured_headers::Item(
                    std::string(v.begin(), v.end()),
                    net::structured_headers::Item::kByteSequenceType));
}

}  // namespace sfv

namespace net::structured_headers {

namespace {

constexpr char kTimeMetricItem[] = "Net.StructuredHeaders.ParseItem.Time";
constexpr char kTimeMetricList[] = "Net.StructuredHeaders.ParseList.Time";
constexpr char kTimeMetricDictionary[] =
    "Net.StructuredHeaders.ParseDictionary.Time";

constexpr char kSuccessMetricItem[] = "Net.StructuredHeaders.ParseItem.Success";
constexpr char kSuccessMetricList[] = "Net.StructuredHeaders.ParseList.Success";
constexpr char kSuccessMetricDictionary[] =
    "Net.StructuredHeaders.ParseDictionary.Success";

template <typename Parse>
auto ParseAndRecordMetrics(std::string_view time_metric,
                           std::string_view success_metric,
                           Parse&& parse) {
  const base::TimeTicks start = base::TimeTicks::Now();
  auto result = parse();
  base::UmaHistogramMicrosecondsTimes(time_metric,
                                      base::TimeTicks::Now() - start);
  base::UmaHistogramBoolean(success_metric, !!result);
  return result;
}

}  // namespace

BASE_FEATURE(kStructuredHeadersInRust, base::FEATURE_DISABLED_BY_DEFAULT);

std::optional<ParameterizedItem> ParseItem(std::string_view str) {
  if (base::FeatureList::IsEnabled(kStructuredHeadersInRust)) {
    ParameterizedMember member;
    bool ok = ParseAndRecordMetrics(kTimeMetricItem, kSuccessMetricItem, [&]() {
      return sfv::decode_item(base::StringViewToRustSlice(str), member);
    });
    if (!ok) {
      return std::nullopt;
    }
    return ParameterizedItem(std::move(member.member.back().item),
                             std::move(member.params));
  }

  return ParseAndRecordMetrics(kTimeMetricItem, kSuccessMetricItem, [&]() {
    return quiche::structured_headers::ParseItem(str);
  });
}

std::optional<List> ParseList(std::string_view str) {
  if (base::FeatureList::IsEnabled(kStructuredHeadersInRust)) {
    List list;
    bool ok = ParseAndRecordMetrics(kTimeMetricList, kSuccessMetricList, [&] {
      return sfv::decode_list(base::StringViewToRustSlice(str), list);
    });
    if (!ok) {
      return std::nullopt;
    }
    return list;
  }

  return ParseAndRecordMetrics(kTimeMetricList, kSuccessMetricList, [&]() {
    return quiche::structured_headers::ParseList(str);
  });
}

std::optional<Dictionary> ParseDictionary(std::string_view str) {
  if (base::FeatureList::IsEnabled(kStructuredHeadersInRust)) {
    Dictionary dictionary;
    bool ok = ParseAndRecordMetrics(
        kTimeMetricDictionary, kSuccessMetricDictionary, [&] {
          return sfv::decode_dictionary(base::StringViewToRustSlice(str),
                                        dictionary);
        });
    if (!ok) {
      return std::nullopt;
    }
    return dictionary;
  }

  return ParseAndRecordMetrics(
      kTimeMetricDictionary, kSuccessMetricDictionary,
      [&]() { return quiche::structured_headers::ParseDictionary(str); });
}

}  // namespace net::structured_headers
