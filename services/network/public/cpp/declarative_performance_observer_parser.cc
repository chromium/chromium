// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/declarative_performance_observer_parser.h"

#include "base/strings/string_util.h"
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

  if (dict->contains(kReportTo)) {
    const auto& member = dict->at(kReportTo);
    if (!member.member_is_inner_list && !member.member.empty() &&
        member.member[0].item.is_string()) {
      policy->reporting_endpoint = member.member[0].item.GetString();
    }
  }

  if (dict->contains(kEntryTypes)) {
    const auto& member = dict->at(kEntryTypes);
    if (member.member_is_inner_list) {
      for (const auto& item : member.member) {
        if (item.item.is_token() || item.item.is_string()) {
          const std::string& type_str = item.item.GetString();
          if (auto type = ParseEntryType(type_str)) {
            policy->entry_types.push_back(*type);
          }
        }
      }
    }
  }

  if (dict->contains(kIncludeUserTiming)) {
    const auto& member = dict->at(kIncludeUserTiming);
    if (member.member_is_inner_list) {
      std::vector<std::string> user_timing;
      for (const auto& item : member.member) {
        if (item.item.is_string()) {
          user_timing.push_back(item.item.GetString());
        }
      }
      policy->include_user_timing = std::move(user_timing);
    }
  }

  if (dict->contains(kCaptureEarlyFailures)) {
    const auto& member = dict->at(kCaptureEarlyFailures);
    if (!member.member_is_inner_list && !member.member.empty() &&
        member.member[0].item.is_boolean()) {
      policy->capture_early_failures = member.member[0].item.GetBoolean();
    }
  }

  return policy;
}

}  // namespace network
