// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_SPECULATION_RULES_SPECULATION_RULES_TAGS_H_
#define CONTENT_BROWSER_PRELOADING_SPECULATION_RULES_SPECULATION_RULES_TAGS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "content/common/content_export.h"

namespace content {

// The structure storing the tags of the speculation rules triggered. See
// explainer for more details:
// https://github.com/WICG/nav-speculation/blob/main/speculation-rules-tags.md
class CONTENT_EXPORT SpeculationRulesTags {
 public:
  SpeculationRulesTags();
  explicit SpeculationRulesTags(
      const std::vector<std::optional<std::string>>& tags);
  ~SpeculationRulesTags();

  // Copyable and movable.
  SpeculationRulesTags(const SpeculationRulesTags&);
  SpeculationRulesTags& operator=(const SpeculationRulesTags&);
  SpeculationRulesTags(SpeculationRulesTags&&) noexcept;
  SpeculationRulesTags& operator=(SpeculationRulesTags&&) noexcept;

  std::optional<std::string> ConvertStringToHeaderString() const;

 private:
  base::flat_set<std::string> tags_;
  bool has_null_{false};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_SPECULATION_RULES_SPECULATION_RULES_TAGS_H_
