// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_URL_UTILS_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_URL_UTILS_IMPL_H_

#include <optional>
#include <string>
#include <string_view>

#include "quiche/common/platform/api/quiche_export.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace quiche {

// Produces concrete URLs in |target| from templated ones in |uri_template|.
// Parameters are URL-encoded. Collects the names of any expanded variables in
// |vars_found|. Supports templates up to level 3 as specified in RFC 6570,
// though without checking for disallowed characters in variable names. Returns
// true if the template was parseable, false if it was malformed.
QUICHE_EXPORT bool ExpandURITemplateImpl(
    const std::string& uri_template,
    const absl::flat_hash_map<std::string, std::string>& parameters,
    std::string* target,
    absl::flat_hash_set<std::string>* vars_found = nullptr);

// Decodes a URL-encoded string and converts it to ASCII. If the decoded input
// contains non-ASCII characters, decoding fails and std::nullopt is returned.
QUICHE_EXPORT std::optional<std::string> AsciiUrlDecodeImpl(
    std::string_view input);

}  // namespace quiche

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_URL_UTILS_IMPL_H_
