// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_SPECULATION_RULES_SPECULATION_RULES_TAGS_H_
#define CONTENT_BROWSER_PRELOADING_SPECULATION_RULES_SPECULATION_RULES_TAGS_H_

#include <set>
#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "net/http/structured_headers.h"

namespace content {

// The structure storing the tags of the speculation rules triggered. See
// explainer for more details:
// https://github.com/WICG/nav-speculation/blob/main/speculation-rules-tags.md
class CONTENT_EXPORT SpeculationRulesTags {
 public:
  SpeculationRulesTags();
  // TODO(crbug.com/381687257): Use std::set instead of std::vector.
  explicit SpeculationRulesTags(std::vector<std::optional<std::string>> tags);
  ~SpeculationRulesTags();

  // Copyable and movable.
  SpeculationRulesTags(const SpeculationRulesTags&);
  SpeculationRulesTags& operator=(const SpeculationRulesTags&);
  SpeculationRulesTags(SpeculationRulesTags&& tags) noexcept;
  SpeculationRulesTags& operator=(SpeculationRulesTags&&) noexcept;

  std::optional<std::string> ConvertStringToHeaderString() const;

 private:
  net::structured_headers::List ConvertStringToStructuredHeader() const;

  std::set<std::optional<std::string>> tags_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_SPECULATION_RULES_SPECULATION_RULES_TAGS_H_
