// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"

#include <algorithm>

#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/blink/public/common/font_unique_name_lookup/icu_fold_case_util.h"

namespace blink {

FontTableMatcher::FontTableMatcher(
    const base::ReadOnlySharedMemoryMapping& mapping) {
  base::span<const uint8_t> mem(mapping);
  font_table_.ParseFromArray(mem.data(), mem.size());
}

// static
base::ReadOnlySharedMemoryMapping
FontTableMatcher::MemoryMappingFromFontUniqueNameTable(
    const FontUniqueNameTable& font_unique_name_table) {
  size_t serialization_size = font_unique_name_table.ByteSizeLong();
  CHECK(serialization_size);
  base::MappedReadOnlyRegion mapped_region =
      base::ReadOnlySharedMemoryRegion::Create(serialization_size);
  CHECK(mapped_region.IsValid());
  base::span<uint8_t> mem(mapped_region.mapping);
  font_unique_name_table.SerializeToArray(mem.data(), mem.size());
  return mapped_region.region.Map();
}

std::optional<FontTableMatcher::MatchResult> FontTableMatcher::MatchName(
    const std::string& name_request) const {
  std::string folded_name_request = IcuFoldCase(name_request);

  const auto& name_map = font_table_.name_map();

  auto find_result = std::lower_bound(
      name_map.begin(), name_map.end(), folded_name_request,
      [](const blink::FontUniqueNameTable_UniqueNameToFontMapping& a,
         const std::string& b) {
        // Comp predicate for std::lower_bound needs to return whether a < b,
        // so that it can find a match for "not less than".
        return a.font_name() < b;
      });
  if (find_result == name_map.end() ||
      find_result->font_name() != folded_name_request ||
      static_cast<int>(find_result->font_index()) > font_table_.fonts_size()) {
    return {};
  }

  const auto& found_font = font_table_.fonts()[find_result->font_index()];

  if (found_font.file_path().empty())
    return {};
  return std::optional<MatchResult>(
      {found_font.file_path(), found_font.ttc_index()});
}

size_t FontTableMatcher::AvailableFonts() const {
  return font_table_.fonts_size();
}

bool FontTableMatcher::FontListIsDisjointFrom(
    const FontTableMatcher& other) const {
  std::vector<std::string> paths_self, paths_other, intersection_result;
  for (const auto& indexed_font : font_table_.fonts()) {
    paths_self.push_back(indexed_font.file_path());
  }
  for (const auto& indexed_font_other : other.font_table_.fonts()) {
    paths_other.push_back(indexed_font_other.file_path());
  }
  std::sort(paths_self.begin(), paths_self.end());
  std::sort(paths_other.begin(), paths_other.end());
  std::set_intersection(paths_self.begin(), paths_self.end(),
                        paths_other.begin(), paths_other.end(),
                        std::back_inserter(intersection_result));
  return intersection_result.empty();
}

void FontTableMatcher::SortUniqueNameTableForSearch(
    FontUniqueNameTable* font_table) {
  std::sort(font_table->mutable_name_map()->begin(),
            font_table->mutable_name_map()->end(),
            [](const auto& a, const auto& b) {
              return a.font_name() < b.font_name();
            });
}

}  // namespace blink
