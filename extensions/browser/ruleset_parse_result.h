// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_RULESET_PARSE_RESULT_H_
#define EXTENSIONS_BROWSER_RULESET_PARSE_RESULT_H_

#include <optional>
#include <string>
#include <vector>

#include "base/values.h"
#include "extensions/common/install_warning.h"

namespace extensions {

struct RulesetParseResult {
  RulesetParseResult();
  ~RulesetParseResult();
  RulesetParseResult(RulesetParseResult&&);
  RulesetParseResult& operator=(RulesetParseResult&&);

  // Non-empty on failure.
  std::optional<std::string> error;

  // Valid if `error` is std::nullopt. Clients should not use these fields in
  // case of a failure since these may be partially populated.
  std::vector<InstallWarning> warnings;
  base::Value::Dict ruleset_install_prefs;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_RULESET_PARSE_RESULT_H_
