// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/structured_headers.h"

#include <stddef.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

namespace {

using InnerList = quiche::structured_headers::InnerList;

ItemParts ToItemParts(Item& item) {
  return {item.item, item.params};
}

InnerListParts ToInnerListParts(InnerList& inner_list) {
  return {inner_list.items, inner_list.params};
}

}  // namespace

struct Dictionary {
  std::vector<net::structured_headers::DictionaryMember> members;
};

ItemParts list_append_item(List& list) {
  return ToItemParts(*list.emplace_back(Item()).GetIfItem());
}

InnerListParts list_append_inner_list(List& list) {
  return ToInnerListParts(*list.emplace_back(InnerList()).GetIfInnerList());
}

ItemParts dictionary_append_item(Dictionary& dictionary, rust::Str key) {
  return ToItemParts(
      *dictionary.members
           .emplace_back(base::RustStrToStringView(key), Member(Item()))
           .second.GetIfItem());
}

ItemParts dictionary_set_item(Dictionary& dictionary, size_t index) {
  Member& member = dictionary.members.at(index).second;
  if (Item* item = member.GetIfItem()) {
    item->params.clear();
    return ToItemParts(*item);
  }
  member = Member(Item());
  return ToItemParts(*member.GetIfItem());
}

InnerListParts dictionary_append_inner_list(Dictionary& dictionary,
                                            rust::Str key) {
  return ToInnerListParts(
      *dictionary.members
           .emplace_back(base::RustStrToStringView(key), Member(InnerList()))
           .second.GetIfInnerList());
}

InnerListParts dictionary_set_inner_list(Dictionary& dictionary, size_t index) {
  Member& member = dictionary.members.at(index).second;
  if (InnerList* inner_list = member.GetIfInnerList()) {
    inner_list->items.clear();
    inner_list->params.clear();
    return ToInnerListParts(*inner_list);
  }
  member = Member(InnerList());
  return ToInnerListParts(*member.GetIfInnerList());
}

ItemParts inner_list_append_item(InnerListItems& items) {
  return ToItemParts(items.emplace_back());
}

BareItem& params_append(Parameters& params, rust::Str key) {
  return params.emplace_back(base::RustStrToStringView(key), BareItem()).second;
}

BareItem& params_get(Parameters& params, size_t index) {
  return params.at(index).second;
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

BASE_FEATURE(kStructuredHeadersInRust, base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

std::optional<ParameterizedItem> ParseItemInternal(std::string_view str,
                                                   bool strict) {
  if (base::FeatureList::IsEnabled(kStructuredHeadersInRust)) {
    ParameterizedItem item;
    bool ok = ParseAndRecordMetrics(kTimeMetricItem, kSuccessMetricItem, [&]() {
      return sfv::decode_item(base::StringViewToRustSlice(str),
                              sfv::ToItemParts(item), strict);
    });
    if (!ok) {
      return std::nullopt;
    }
    return item;
  }

  return ParseAndRecordMetrics(kTimeMetricItem, kSuccessMetricItem, [&]() {
    return quiche::structured_headers::ParseItem(str, strict);
  });
}

std::optional<List> ParseListInternal(std::string_view str, bool strict) {
  if (base::FeatureList::IsEnabled(kStructuredHeadersInRust)) {
    List list;
    bool ok = ParseAndRecordMetrics(kTimeMetricList, kSuccessMetricList, [&] {
      return sfv::decode_list(base::StringViewToRustSlice(str), list, strict);
    });
    if (!ok) {
      return std::nullopt;
    }
    return list;
  }

  return ParseAndRecordMetrics(kTimeMetricList, kSuccessMetricList, [&]() {
    return quiche::structured_headers::ParseList(str, strict);
  });
}

std::optional<Dictionary> ParseDictionaryInternal(std::string_view str,
                                                  bool strict) {
  if (base::FeatureList::IsEnabled(kStructuredHeadersInRust)) {
    sfv::Dictionary dictionary;
    bool ok = ParseAndRecordMetrics(
        kTimeMetricDictionary, kSuccessMetricDictionary, [&] {
          return sfv::decode_dictionary(base::StringViewToRustSlice(str),
                                        dictionary, strict);
        });
    if (!ok) {
      return std::nullopt;
    }
    return Dictionary(std::move(dictionary.members));
  }

  return ParseAndRecordMetrics(
      kTimeMetricDictionary, kSuccessMetricDictionary, [&]() {
        return quiche::structured_headers::ParseDictionary(str, strict);
      });
}

}  // namespace

std::optional<ParameterizedItem> ParseItem(std::string_view str) {
  return ParseItemInternal(str, /*strict=*/false);
}

std::optional<ParameterizedItem> ParseItemStrict(std::string_view str) {
  return ParseItemInternal(str, /*strict=*/true);
}

std::optional<List> ParseList(std::string_view str) {
  return ParseListInternal(str, /*strict=*/false);
}

std::optional<List> ParseListStrict(std::string_view str) {
  return ParseListInternal(str, /*strict=*/true);
}

std::optional<Dictionary> ParseDictionary(std::string_view str) {
  return ParseDictionaryInternal(str, /*strict=*/false);
}

std::optional<Dictionary> ParseDictionaryStrict(std::string_view str) {
  return ParseDictionaryInternal(str, /*strict=*/true);
}

}  // namespace net::structured_headers
