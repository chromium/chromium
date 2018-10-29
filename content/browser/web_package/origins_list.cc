// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/origins_list.h"

#include "base/strings/string_split.h"
#include "url/gurl.h"

namespace content {
namespace signed_exchange_utils {

constexpr char kSubdomainMatchPrefix[] = "*.";

OriginsList::OriginsList() = default;

OriginsList::OriginsList(base::StringPiece str) {
  std::vector<base::StringPiece> elements = base::SplitStringPiece(
      str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (base::StringPiece element : elements) {
    bool subdomain_match = false;
    if (base::StartsWith(element, kSubdomainMatchPrefix,
                         base::CompareCase::SENSITIVE)) {
      subdomain_match = true;
      element.remove_prefix(sizeof(kSubdomainMatchPrefix) - 1);
    }
    if (base::StartsWith(element,
                         "https:", base::CompareCase::INSENSITIVE_ASCII)) {
      LOG(ERROR) << "OriginsList entry should omit https scheme: \"" << element
                 << "\"";
      continue;
    }

    std::string url_str("https://");
    element.AppendToString(&url_str);
    GURL url(url_str);
    if (!url.is_valid()) {
      LOG(ERROR) << "Failed to parse an OriginsList entry to a valid Origin: \""
                 << element << "\"";
      continue;
    }
    DCHECK(url.SchemeIs("https"));

    url::Origin origin = url::Origin::Create(url);
    if (subdomain_match) {
      subdomain_match_origins_.push_back(origin);
    } else {
      exact_match_origins_.insert(origin);
    }
  }
}

OriginsList::OriginsList(OriginsList&&) = default;
OriginsList::~OriginsList() = default;

bool OriginsList::IsEmpty() const {
  return exact_match_origins_.empty() && subdomain_match_origins_.empty();
}

bool OriginsList::Match(const url::Origin& origin) const {
  // OriginsList only contains HTTPS scheme origins.
  if (origin.scheme() != url::kHttpsScheme) {
    return false;
  }

  if (exact_match_origins_.find(origin) != exact_match_origins_.end()) {
    return true;
  }

  for (const auto& subdomain_match_origin : subdomain_match_origins_) {
    if (origin.DomainIs(subdomain_match_origin.host()) &&
        origin.port() == subdomain_match_origin.port()) {
      return true;
    }
  }

  return false;
}

}  // namespace signed_exchange_utils
}  // namespace content
