// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/speculation_rules/speculation_rules_tags.h"

#include "base/strings/string_util.h"
#include "net/http/structured_headers.h"

namespace content {

SpeculationRulesTags::SpeculationRulesTags()
    : SpeculationRulesTags({std::nullopt}) {}

SpeculationRulesTags::SpeculationRulesTags(
    std::vector<std::optional<std::string>> tags) {
  CHECK(!tags.empty());
  for (auto& tag : tags) {
    tags_.insert(std::move(tag));
  }
}

SpeculationRulesTags::~SpeculationRulesTags() = default;

SpeculationRulesTags::SpeculationRulesTags(const SpeculationRulesTags&) =
    default;
SpeculationRulesTags& SpeculationRulesTags::operator=(
    const SpeculationRulesTags&) = default;
SpeculationRulesTags::SpeculationRulesTags(
    SpeculationRulesTags&& tags) noexcept = default;
SpeculationRulesTags& SpeculationRulesTags::operator=(
    SpeculationRulesTags&&) noexcept = default;

net::structured_headers::List
SpeculationRulesTags::ConvertStringToStructuredHeader() const {
  net::structured_headers::List tag_list;

  for (const std::optional<std::string>& tag : tags_) {
    if (tag.has_value()) {
      CHECK(std::all_of(tag.value().begin(), tag.value().end(),
                        base::IsAsciiPrintable<char>));
      tag_list.push_back(net::structured_headers::ParameterizedMember(
          net::structured_headers::Item(tag.value()), {}));
    } else {
      tag_list.push_back(net::structured_headers::ParameterizedMember(
          net::structured_headers::Item(
              "null", net::structured_headers::Item::kTokenType),
          {}));
    }
  }

  return tag_list;
}

std::optional<std::string> SpeculationRulesTags::ConvertStringToHeaderString()
    const {
  return SerializeList(ConvertStringToStructuredHeader());
}

}  // namespace content
