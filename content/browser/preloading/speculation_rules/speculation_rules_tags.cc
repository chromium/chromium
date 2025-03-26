// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/speculation_rules/speculation_rules_tags.h"

#include "base/strings/string_util.h"
#include "net/http/structured_headers.h"

namespace content {

SpeculationRulesTags::SpeculationRulesTags() = default;
SpeculationRulesTags::SpeculationRulesTags(
    std::vector<std::optional<std::string>> tags)
    : tags_(std::move(tags)) {}
SpeculationRulesTags::SpeculationRulesTags(SpeculationRulesTags&& tags) =
    default;
SpeculationRulesTags::SpeculationRulesTags(const SpeculationRulesTags& tags) =
    default;

SpeculationRulesTags::~SpeculationRulesTags() = default;

net::structured_headers::List
SpeculationRulesTags::ConvertStringToStructuredHeader() {
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

  if (tag_list.size() == 0) {
    // Put the default value.
    tag_list.push_back(net::structured_headers::ParameterizedMember(
        net::structured_headers::Item(
            "null", net::structured_headers::Item::kTokenType),
        {}));
  }

  return tag_list;
}

std::optional<std::string> SpeculationRulesTags::ConvertStringToHeaderString() {
  return SerializeList(ConvertStringToStructuredHeader());
}

}  // namespace content
