// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/ruleset_parse_result.h"

namespace extensions {

RulesetParseResult::RulesetParseResult() = default;
RulesetParseResult::~RulesetParseResult() = default;
RulesetParseResult::RulesetParseResult(RulesetParseResult&&) = default;
RulesetParseResult& RulesetParseResult::operator=(RulesetParseResult&&) =
    default;

}  // namespace extensions
