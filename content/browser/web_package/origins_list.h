// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_ORIGINS_LIST_H_
#define CONTENT_BROWSER_WEB_PACKAGE_ORIGINS_LIST_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {
namespace signed_exchange_utils {

// OriginsList can query if a particular origin |Match|.
// OriginsList can only match HTTPS origins.
class CONTENT_EXPORT OriginsList {
 public:
  OriginsList();

  // Creates an OriginsList from comma-separated list of hosts.
  //
  // Entries starting with "*." will match with subdomains.
  //
  // For example, "example.com,*.google.com" will create an
  // OriginsList that match exactly "example.com" but not its
  // subdomains, and all subdomains of "google.com".
  //
  // Note: Entries should NOT start with "https://", but start from hostname.
  explicit OriginsList(base::StringPiece str);

  OriginsList(OriginsList&&);
  ~OriginsList();

  // Returns true when |this| has an empty list to match
  // (i.e. no origins would match).
  bool IsEmpty() const;

  bool Match(const url::Origin& origin) const;

 private:
  base::flat_set<url::Origin> exact_match_origins_;
  std::vector<url::Origin> subdomain_match_origins_;
};

}  // namespace signed_exchange_utils
}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_ORIGINS_LIST_H_
