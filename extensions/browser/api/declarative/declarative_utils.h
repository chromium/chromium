// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_DECLARATIVE_UTILS_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_DECLARATIVE_UTILS_H_

#include <optional>
#include <string>

namespace extensions::declarative {

// Returns true if `rule_id` is present and valid UTF-8 (lone surrogates would
// be replaced with U+FFFD when serialized to JSON in `StateStore`).
bool IsValidRuleId(const std::optional<std::string>& rule_id);

}  // namespace extensions::declarative

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_DECLARATIVE_UTILS_H_
