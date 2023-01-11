// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_SITE_DATA_REMOVER_H_
#define CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_SITE_DATA_REMOVER_H_

#include <inttypes.h>
#include <vector>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/browsing_data_remover.h"

namespace net {
class SchemefulSite;
}  // namespace net

namespace content {

class CONTENT_EXPORT FirstPartySetsSiteDataRemover {
 public:
  // Requests clearing of site data of `sites` with the `remover`, and invokes
  // `callback` with the failed data types from BrowsingDataRemover::DataType
  // enum, 0 indicates success.
  //
  // Currently it only accounts for cookie and storage data types.
  static void RemoveSiteData(BrowsingDataRemover& remover,
                             std::vector<net::SchemefulSite> sites,
                             base::OnceCallback<void(uint64_t)> callback);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_SITE_DATA_REMOVER_H_
