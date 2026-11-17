// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_CONSTANTS_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_CONSTANTS_H_

namespace extensions::declarative {

inline constexpr char kDuplicateRuleIdError[] = "Duplicate rule ID: %s";
inline constexpr char kInvalidRuleIdError[] =
    "Invalid rule ID: must be present and valid UTF-8";
inline constexpr char kErrorCannotRemoveManifestRules[] =
    "Rules declared in the 'event_rules' manifest field cannot be removed";

}  // namespace extensions::declarative

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_CONSTANTS_H_
