// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/structured_headers.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/feature.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_view_rust.h"
#include "base/strings/string_view_util.h"
#include "base/time/time.h"
#include "third_party/rust/sfv/v0_15/wrapper/functions.h"
#include "third_party/rust/sfv/v0_15/wrapper/lib.rs.h"

// This namespace defines FFI-friendly functions that are called from Rust in
// //third_party/rust/sfv/v0_15/wrapper/.
namespace sfv {

Item& list_append_item(List& list) {
  return *list.emplace_back(Item()).GetIfItem();
}

InnerList& list_append_inner_list(List& list) {
  return *list.emplace_back(InnerList()).GetIfInnerList();
}

Item& dictionary_set_item(Dictionary& dictionary, rust::Str key) {
  auto& member = dictionary[base::RustStrToStringView(key)];
  member = net::structured_headers::ParameterizedMember(Item());
  return *member.GetIfItem();
}

InnerList& dictionary_set_inner_list(Dictionary& dictionary, rust::Str key) {
  auto& member = dictionary[base::RustStrToStringView(key)];
  member = net::structured_headers::ParameterizedMember(InnerList());
  return *member.GetIfInnerList();
}

Item& inner_list_append_item(InnerList& inner_list) {
  return inner_list.items.emplace_back();
}

BareItem& get_item_bare_item(Item& item) {
  return item.item;
}

void set_bare_item_boolean(BareItem& out, bool v) {
  out = BareItem(v);
}

void set_bare_item_integer(BareItem& out, int64_t v) {
  out = BareItem(v);
}

void set_bare_item_decimal(BareItem& out, double v) {
  out = BareItem(v);
}

void set_bare_item_string(BareItem& out, rust::Str v) {
  out = BareItem(BareItem::string, base::RustStrToStringView(v));
}

void set_bare_item_token(BareItem& out, rust::Str v) {
  out = BareItem(BareItem::token, base::RustStrToStringView(v));
}

void set_bare_item_byte_sequence(BareItem& out, rust::Slice<const uint8_t> v) {
  out = BareItem(BareItem::byte_sequence, base::as_string_view(v));
}

// Parameters is a type alias in net::structured_headers, so it cannot be
// forward-declared. To keep the FFI header (functions.h) clean, we use an
// opaque tag class there and reinterpret_cast it here to the actual type.
Parameters& get_inner_list_params(InnerList& inner_list) {
  return *reinterpret_cast<Parameters*>(&inner_list.params);
}

Parameters& get_item_params(Item& item) {
  return *reinterpret_cast<Parameters*>(&item.params);
}

BareItem& get_or_insert_param(Parameters& parameters, rust::Str key) {
  auto& params =
      reinterpret_cast<net::structured_headers::Parameters&>(parameters);
  std::string_view key_view = base::RustStrToStringView(key);
  for (auto& param : params) {
    if (param.first == key_view) {
      return param.second;
    }
  }
  return params.emplace_back(key_view, BareItem()).second;
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
    ParameterizedItem item;
    bool ok = ParseAndRecordMetrics(kTimeMetricItem, kSuccessMetricItem, [&]() {
      return sfv::decode_item(base::StringViewToRustSlice(str), item.item,
                              sfv::get_item_params(item));
    });
    if (!ok) {
      return std::nullopt;
    }
    return item;
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
