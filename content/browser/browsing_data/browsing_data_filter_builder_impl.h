// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_DATA_BROWSING_DATA_FILTER_BUILDER_IMPL_H_
#define CONTENT_BROWSER_BROWSING_DATA_BROWSING_DATA_FILTER_BUILDER_IMPL_H_

#include <optional>
#include <set>
#include <string>

#include "content/common/content_export.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "url/origin.h"

namespace content {

class CONTENT_EXPORT BrowsingDataFilterBuilderImpl
    : public BrowsingDataFilterBuilder {
 public:
  explicit BrowsingDataFilterBuilderImpl(Mode mode);

  BrowsingDataFilterBuilderImpl(Mode mode, OriginMatchingMode origin_mode);

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
  void SetStorageKey(
      const std::optional<blink::StorageKey>& storage_key) override;
  bool HasStorageKey() const override;
  bool MatchesWithSavedStorageKey(
      const blink::StorageKey& other_key) const override;
  bool MatchesAllOriginsAndDomains() override;
  bool MatchesMostOriginsAndDomains() override;
  bool MatchesNothing() override;
  void SetPartitionedCookiesOnly(bool value) override;
  bool PartitionedCookiesOnly() const override;
  void SetStoragePartitionConfig(
      const StoragePartitionConfig& storage_partition_config) override;
  std::optional<StoragePartitionConfig> GetStoragePartitionConfig() override;
  base::RepeatingCallback<bool(const GURL&)> BuildUrlFilter() override;
  content::StoragePartition::StorageKeyMatcherFunction BuildStorageKeyFilter()
      override;
  network::mojom::ClearDataFilterPtr BuildNetworkServiceFilter() override;
  network::mojom::CookieDeletionFilterPtr BuildCookieDeletionFilter() override;
  base::RepeatingCallback<bool(const std::string& site)> BuildPluginFilter()
      override;
  Mode GetMode() override;
  const std::set<url::Origin>& GetOrigins() const override;
  const std::set<std::string>& GetRegisterableDomains() const override;
  std::unique_ptr<BrowsingDataFilterBuilder> Copy() override;

  OriginMatchingMode GetOriginModeForTesting() const;

  const net::CookiePartitionKeyCollection&
  GetCookiePartitionKeyCollectionForTesting() const;

 private:
  bool IsEqual(const BrowsingDataFilterBuilder& other) const override;

  Mode mode_;
  OriginMatchingMode origin_mode_;

  std::set<url::Origin> origins_;
  std::set<std::string> domains_;
  net::CookiePartitionKeyCollection cookie_partition_key_collection_ =
      net::CookiePartitionKeyCollection::ContainsAll();
  std::optional<blink::StorageKey> storage_key_ = std::nullopt;
  bool partitioned_cookies_only_ = false;
  std::optional<StoragePartitionConfig> storage_partition_config_ =
      std::nullopt;
};

}  // content

#endif  // CONTENT_BROWSER_BROWSING_DATA_BROWSING_DATA_FILTER_BUILDER_IMPL_H_
