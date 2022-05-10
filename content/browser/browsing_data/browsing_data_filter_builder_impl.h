// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_DATA_BROWSING_DATA_FILTER_BUILDER_IMPL_H_
#define CONTENT_BROWSER_BROWSING_DATA_BROWSING_DATA_FILTER_BUILDER_IMPL_H_

#include <set>

#include "content/common/content_export.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "url/origin.h"

namespace content {

class CONTENT_EXPORT BrowsingDataFilterBuilderImpl
    : public BrowsingDataFilterBuilder {
 public:
  explicit BrowsingDataFilterBuilderImpl(Mode mode);

  BrowsingDataFilterBuilderImpl(const BrowsingDataFilterBuilderImpl&) = delete;
  BrowsingDataFilterBuilderImpl& operator=(
      const BrowsingDataFilterBuilderImpl&) = delete;

  ~BrowsingDataFilterBuilderImpl() override;

  // BrowsingDataFilterBuilder implementation:
  void AddOrigin(const url::Origin& origin) override;
  void AddRegisterableDomain(const std::string& registrable_domain) override;
  void SetCookiePartitionKeyCollection(
      const net::CookiePartitionKeyCollection& cookie_partition_key_collection)
      override;
  bool IsCrossSiteClearSiteData() const override;
  bool MatchesAllOriginsAndDomains() override;
  base::RepeatingCallback<bool(const GURL&)> BuildUrlFilter() override;
  base::RepeatingCallback<bool(const url::Origin&)> BuildOriginFilter()
      override;
  network::mojom::ClearDataFilterPtr BuildNetworkServiceFilter() override;
  network::mojom::CookieDeletionFilterPtr BuildCookieDeletionFilter() override;
  base::RepeatingCallback<bool(const std::string& site)> BuildPluginFilter()
      override;
  Mode GetMode() override;
  std::unique_ptr<BrowsingDataFilterBuilder> Copy() override;

 private:
  bool IsEqual(const BrowsingDataFilterBuilder& other) const override;

  Mode mode_;

  std::set<url::Origin> origins_;
  std::set<std::string> domains_;
  net::CookiePartitionKeyCollection cookie_partition_key_collection_ =
      net::CookiePartitionKeyCollection::ContainsAll();
};

}  // content

#endif  // CONTENT_BROWSER_BROWSING_DATA_BROWSING_DATA_FILTER_BUILDER_IMPL_H_
