// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/renderer/allowlist.h"

#include "base/check.h"
#include "base/strings/string_split.h"

namespace headless {

Allowlist::Allowlist(std::string list, bool default_allow)
    : storage_(std::move(list)),
      default_allow_(default_allow),
      entries_(base::SplitStringPiece(storage_,
                                      ",",
                                      base::WhitespaceHandling::TRIM_WHITESPACE,
                                      base::SplitResult::SPLIT_WANT_NONEMPTY)) {
}

Allowlist::~Allowlist() = default;

bool Allowlist::IsAllowed(std::string_view str) const {
  for (auto entry : entries_) {
    CHECK(!entry.empty());  // Per SplitResult::SPLIT_WANT_NONEMPTY
    bool entry_is_allow = true;
    if (entry.starts_with('-')) {
      entry_is_allow = false;
      entry.remove_prefix(1);
    }
    if (entry == "*" || entry == str) {
      return entry_is_allow;
    }
  }
  return default_allow_;
}

}  // namespace headless
