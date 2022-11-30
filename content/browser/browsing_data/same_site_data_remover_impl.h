// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_DATA_SAME_SITE_DATA_REMOVER_IMPL_H_
#define CONTENT_BROWSER_BROWSING_DATA_SAME_SITE_DATA_REMOVER_IMPL_H_

#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_context.h"

namespace net {
class CookieStore;
}

namespace url {
class Origin;
}

namespace content {

class StoragePartition;

class CONTENT_EXPORT SameSiteDataRemoverImpl {
 public:
  explicit SameSiteDataRemoverImpl(BrowserContext* browser_context);

  SameSiteDataRemoverImpl(const SameSiteDataRemoverImpl&) = delete;
  SameSiteDataRemoverImpl& operator=(const SameSiteDataRemoverImpl&) = delete;

  ~SameSiteDataRemoverImpl();

  // Returns a set containing domains associated with deleted SameSite=None
  // cookies.
  const std::set<std::string>& GetDeletedDomainsForTesting();

  // Deletes cookies with SameSite attribute value NO_RESTRICTION
  // from the CookieStore and stores domains of deleted cookies in the
  // same_site_none_domains_ vector. The closure is called after cookies
  // have been deleted.
  void DeleteSameSiteNoneCookies(base::OnceClosure closure);

  // Clears additional storage for domains that have cookies with SameSite value
  // NO_RESTRICTION from the StoragePartition. Storage is cleared based on the
  // domains of the cookies deleted in DeleteSameSiteNoneCookies().
  //
  // This is called safely on an instance which is destroyed after the function
  // call since it's not needed for the function execution.
  void ClearStoragePartitionData(base::OnceClosure closure);

  // Clears storage for origins in the provided set.
  void ClearStoragePartitionForOrigins(base::OnceClosure closure,
                                       std::set<url::Origin> origins);

  // For testing purposes only.
  void OverrideStoragePartitionForTesting(StoragePartition* storage_partition);

 private:
  raw_ptr<BrowserContext> browser_context_;
  raw_ptr<StoragePartition> storage_partition_;
  std::set<std::string> same_site_none_domains_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_DATA_SAME_SITE_DATA_REMOVER_IMPL_H_
