// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative/declarative_utils.h"

#include "base/strings/string_util.h"

namespace extensions::declarative {

bool IsValidRuleId(const std::optional<std::string>& rule_id) {
  return rule_id.has_value() &&
         base::IsStringUTF8AllowingNoncharacters(*rule_id);
}

}  // namespace extensions::declarative
