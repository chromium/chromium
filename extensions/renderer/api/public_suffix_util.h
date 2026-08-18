// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_PUBLIC_SUFFIX_UTIL_H_
#define EXTENSIONS_RENDERER_API_PUBLIC_SUFFIX_UTIL_H_

#include <optional>
#include <string>
#include <string_view>

#include "extensions/common/api/public_suffix.h"

namespace extensions::api::public_suffix {

inline constexpr char kInvalidHostname[] = "Invalid hostname";

struct ParsedHostname {
  std::string value;
  bool is_ip_address = false;
};

// Parses, normalizes, and validates `hostname`. Returns std::nullopt for
// invalid hostnames, but note that a single leading dot is accepted.
std::optional<ParsedHostname> ParseHostname(std::string_view hostname);

// Determines whether `hostname` is itself a known public suffix.
bool IsKnownSuffix(const ParsedHostname& hostname);

// Returns the known public suffix for `hostname`, if any.
std::optional<std::string> GetKnownSuffix(const ParsedHostname& hostname);

// Returns the registrable domain for `hostname`, if any.
std::optional<std::string> GetDomain(const ParsedHostname& hostname,
                                     const DomainOptions& options);

}  // namespace extensions::api::public_suffix

#endif  // EXTENSIONS_RENDERER_API_PUBLIC_SUFFIX_UTIL_H_
