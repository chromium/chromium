// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_AX_STYLE_DATA_H_
#define CONTENT_BROWSER_ACCESSIBILITY_AX_STYLE_DATA_H_

#include <optional>
#include <vector>

#include "base/check_op.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/accessibility/ax_enums.mojom-data-view.h"

namespace content {

// Struct to hold text styling information for accessibility.
struct CONTENT_EXPORT AXStyleData {
 public:
  // Pairs of start and end indices.
  using RangePairs = std::vector<std::pair<int, int>>;

  std::optional<absl::flat_hash_map<std::u16string, RangePairs>> suggestions;
  // Links with the target url as the map key.
  std::optional<absl::flat_hash_map<std::u16string, RangePairs>> links;
  std::optional<absl::flat_hash_map<float, RangePairs>> text_sizes;
  std::optional<absl::flat_hash_map<ax::mojom::TextStyle, RangePairs>>
      text_styles;
  std::optional<absl::flat_hash_map<ax::mojom::TextPosition, RangePairs>>
      text_positions;
  std::optional<absl::flat_hash_map<int, RangePairs>> foreground_colors;
  std::optional<absl::flat_hash_map<int, RangePairs>> background_colors;
  std::optional<absl::flat_hash_map<std::string, RangePairs>> font_families;
  std::optional<absl::flat_hash_map<std::string, RangePairs>> locales;

  AXStyleData();
  AXStyleData(const AXStyleData&) = delete;
  AXStyleData(AXStyleData&&);

  ~AXStyleData();

  AXStyleData& operator=(const AXStyleData&) = delete;
  AXStyleData& operator=(AXStyleData&&);

  // Adds a range with the given value to the specified field.
  // `start` must be less than or equal to `end`. The indices are allowed to be
  // negative to allow temporary negative intermediate values. The caller is
  // responsible for making sure indices are non-negative by the time they are
  // used. This function simply appends the new range to the relevant map value.
  // It does not do any sorting, merging, or deduping.
  template <class T>
  static void AddRange(std::optional<absl::flat_hash_map<T, RangePairs>>& field,
                       T value,
                       int start,
                       int end) {
    CHECK_LE(start, end);
    if (!field) {
      field.emplace();
    }
    (*field)[std::move(value)].emplace_back(start, end);
  }

  // Returns a string representation of the style data for testing.
  std::string ToStringForTesting() const;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_AX_STYLE_DATA_H_
