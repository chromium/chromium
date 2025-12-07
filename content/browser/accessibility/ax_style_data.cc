// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/ax_style_data.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"

namespace {

using RangePairs = content::AXStyleData::RangePairs;

std::string RangePairsToString(const RangePairs& range_pairs) {
  std::vector<std::string> pair_strs;
  pair_strs.reserve(range_pairs.size());
  for (const auto& range : range_pairs) {
    pair_strs.emplace_back(
        base::StrCat({"(", base::NumberToString(range.first), ",",
                      base::NumberToString(range.second), ")"}));
  }
  return base::StrCat({"[", base::JoinString(pair_strs, ", "), "]"});
}

template <typename T>
std::string ToDebugString(
    const std::optional<absl::flat_hash_map<T, RangePairs>>& range_pairs_map,
    const std::string& name) {
  if (!range_pairs_map) {
    return "";
  }
  std::string out = base::StrCat({"  ", name, ":\n"});
  for (const auto& entry : *range_pairs_map) {
    base::StrAppend(&out,
                    {"    value=", base::ToString(entry.first), "\n",
                     "    ranges=", RangePairsToString(entry.second), "\n"});
  }
  return out;
}

}  // namespace

namespace content {

AXStyleData::AXStyleData() = default;
AXStyleData::AXStyleData(AXStyleData&&) = default;

AXStyleData::~AXStyleData() = default;

AXStyleData& AXStyleData::operator=(AXStyleData&&) = default;

std::string AXStyleData::ToStringForTesting() const {
  return base::StrCat({
      "AXStyleData{\n",
      ToDebugString(suggestions, "suggestions"),
      ToDebugString(links, "links"),
      ToDebugString(text_sizes, "text_sizes"),
      ToDebugString(text_styles, "text_styles"),
      ToDebugString(text_positions, "text_positions"),
      ToDebugString(foreground_colors, "foreground_colors"),
      ToDebugString(background_colors, "background_colors"),
      ToDebugString(font_families, "font_families"),
      ToDebugString(locales, "locales"),
      "}",
  });
}

}  // namespace content
