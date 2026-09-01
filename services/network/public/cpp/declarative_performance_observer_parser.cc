// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/declarative_performance_observer_parser.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/map_util.h"
#include "net/http/structured_headers.h"

namespace network {

namespace {

constexpr char kReportTo[] = "report-to";
constexpr char kEntryTypes[] = "entry-types";
constexpr char kIncludeUserTiming[] = "include-user-timing";
constexpr char kCaptureEarlyFailures[] = "capture-early-failures";

constexpr char kEntryTypeNavigation[] = "navigation";
constexpr char kEntryTypeMark[] = "mark";
constexpr char kEntryTypeVisibilityState[] = "visibility-state";
constexpr char kEntryTypeLargestContentfulPaint[] = "largest-contentful-paint";

std::optional<mojom::PerformanceEntryType> ParseEntryType(
    std::string_view type_str) {
  if (type_str == kEntryTypeNavigation) {
    return mojom::PerformanceEntryType::kNavigation;
  }
  if (type_str == kEntryTypeMark) {
    return mojom::PerformanceEntryType::kMark;
  }
  if (type_str == kEntryTypeVisibilityState) {
    return mojom::PerformanceEntryType::kVisibilityState;
  }
  if (type_str == kEntryTypeLargestContentfulPaint) {
    return mojom::PerformanceEntryType::kLargestContentfulPaint;
  }
  return std::nullopt;
}

net::structured_headers::Item* FindItem(
    net::structured_headers::Dictionary& dict LIFETIME_BOUND,
    std::string_view key) {
  auto* member = base::FindOrNull(dict, key);
  if (!member) {
    return nullptr;
  }
  auto item_and_params = member->GetWithParamsIfItem();
  return item_and_params.has_value() ? &item_and_params->first : nullptr;
}

std::vector<net::structured_headers::ParameterizedItem>* FindInnerList(
    net::structured_headers::Dictionary& dict LIFETIME_BOUND,
    std::string_view key) {
  auto* member = base::FindOrNull(dict, key);
  if (!member) {
    return nullptr;
  }
  auto inner_list_and_params = member->GetWithParamsIfInnerList();
  return inner_list_and_params.has_value() ? &inner_list_and_params->first
                                           : nullptr;
}

}  // namespace

mojom::DeclarativePerformanceObserverPolicyPtr
ParseDeclarativePerformanceObserverPolicy(std::string_view header) {
  auto dict = net::structured_headers::ParseDictionary(header);
  if (!dict) {
    return nullptr;
  }

  auto policy = mojom::DeclarativePerformanceObserverPolicy::New();

  if (auto* item = FindItem(*dict, kReportTo)) {
    if (std::string* str = item->GetIfString()) {
      policy->reporting_endpoint = std::move(*str);
    }
  }

  if (const auto* inner_list = FindInnerList(*dict, kEntryTypes)) {
    for (const auto& item : *inner_list) {
      const std::string* type_str = item.item.GetIfToken();
      if (!type_str) {
        type_str = item.item.GetIfString();
      }
      if (type_str) {
        if (auto type = ParseEntryType(*type_str)) {
          policy->entry_types.push_back(*type);
        }
      }
    }
  }

  if (auto* inner_list = FindInnerList(*dict, kIncludeUserTiming)) {
    std::vector<std::string> user_timing;
    for (auto& item : *inner_list) {
      if (std::string* str = item.item.GetIfString()) {
        user_timing.emplace_back(std::move(*str));
      }
    }
    policy->include_user_timing = std::move(user_timing);
  }

  if (const auto* item = FindItem(*dict, kCaptureEarlyFailures)) {
    if (const bool* boolean = item->GetIfBoolean()) {
      policy->capture_early_failures = *boolean;
    }
  }

  return policy;
}

}  // namespace network
