// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/speculation_rules/speculation_rules_tags.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/strings/string_util.h"
#include "net/http/structured_headers.h"

namespace content {

SpeculationRulesTags::SpeculationRulesTags() : has_null_(true) {}

SpeculationRulesTags::SpeculationRulesTags(
    const std::vector<std::optional<std::string>>& tags) {
  std::vector<std::string> entries;
  entries.reserve(tags.size());

  for (const auto& tag : tags) {
    if (tag.has_value()) {
      entries.emplace_back(*tag);
    } else {
      has_null_ = true;
    }
  }

  tags_ = base::flat_set<std::string>(entries);
}

SpeculationRulesTags::~SpeculationRulesTags() = default;

SpeculationRulesTags::SpeculationRulesTags(const SpeculationRulesTags&) =
    default;
SpeculationRulesTags& SpeculationRulesTags::operator=(
    const SpeculationRulesTags&) = default;
SpeculationRulesTags::SpeculationRulesTags(SpeculationRulesTags&&) noexcept =
    default;
SpeculationRulesTags& SpeculationRulesTags::operator=(
    SpeculationRulesTags&&) noexcept = default;

std::optional<std::string> SpeculationRulesTags::ConvertStringToHeaderString()
    const {
  net::structured_headers::List tag_list;
  tag_list.reserve(tags_.size() + has_null_);

  if (has_null_) {
    tag_list.emplace_back(net::structured_headers::Item(
        net::structured_headers::Item::token, "null"));
  }

  for (const std::string& tag : tags_) {
    CHECK(std::ranges::all_of(tag, base::IsAsciiPrintable<char>));
    tag_list.emplace_back(net::structured_headers::Item(
        net::structured_headers::Item::string, tag));
  }

  return net::structured_headers::SerializeList(tag_list);
}

}  // namespace content
