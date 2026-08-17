// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/declarative_performance_observer_parser.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

}  // namespace

mojom::DeclarativePerformanceObserverPolicyPtr
ParseDeclarativePerformanceObserverPolicy(std::string_view header) {
  auto dict = net::structured_headers::ParseDictionary(header);
  if (!dict) {
    return nullptr;
  }

  auto policy = mojom::DeclarativePerformanceObserverPolicy::New();

  if (auto* member = base::FindOrNull(*dict, kReportTo);
      member && !member->member_is_inner_list && !member->member.empty()) {
    if (std::string* str = member->member.front().item.GetIfString()) {
      policy->reporting_endpoint = std::move(*str);
    }
  }

  if (const auto* member = base::FindOrNull(*dict, kEntryTypes);
      member && member->member_is_inner_list) {
    for (const auto& item : member->member) {
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

  if (auto* member = base::FindOrNull(*dict, kIncludeUserTiming);
      member && member->member_is_inner_list) {
    std::vector<std::string> user_timing;
    for (auto& item : member->member) {
      if (std::string* str = item.item.GetIfString()) {
        user_timing.emplace_back(std::move(*str));
      }
    }
    policy->include_user_timing = std::move(user_timing);
  }

  if (const auto* member = base::FindOrNull(*dict, kCaptureEarlyFailures);
      member && !member->member_is_inner_list && !member->member.empty()) {
    if (const bool* boolean = member->member.front().item.GetIfBoolean()) {
      policy->capture_early_failures = *boolean;
    }
  }

  return policy;
}

}  // namespace network
