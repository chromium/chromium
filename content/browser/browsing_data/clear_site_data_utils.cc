// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/clear_site_data_utils.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/browsing_data/browsing_data_remover_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

namespace {

// Finds the BrowserContext associated with the request and requests
// the actual clearing of data for `origin`. The data types to be deleted
// are determined by `clear_site_data_types` and `storage_buckets_to_remove`.
// `web_contents_getter` identifies the WebContents from which the request
// originated. Must be run on the UI thread. The `callback` will be executed
// on the IO thread.
class SiteDataClearer : public BrowsingDataRemover::Observer {
 public:
  SiteDataClearer(
      BrowserContext* browser_context,
      std::optional<StoragePartitionConfig> storage_partition_config,
      const url::Origin& origin,
      const ClearSiteDataTypeSet clear_site_data_types,
      const std::set<std::string>& storage_buckets_to_remove,
      bool avoid_closing_connections,
      std::optional<net::CookiePartitionKey> cookie_partition_key,
      std::optional<blink::StorageKey> storage_key,
      bool partitioned_state_allowed_only,
      base::OnceClosure callback)
      : storage_partition_config_(std::move(storage_partition_config)),
        origin_(origin),
        clear_site_data_types_(clear_site_data_types),
        storage_buckets_to_remove_(storage_buckets_to_remove),
        avoid_closing_connections_(avoid_closing_connections),
        cookie_partition_key_(std::move(cookie_partition_key)),
        storage_key_(std::move(storage_key)),
        partitioned_state_allowed_only_(partitioned_state_allowed_only),
        callback_(std::move(callback)),
        pending_task_count_(0),
        remover_(nullptr) {
    remover_ =
        BrowserContextImpl::From(browser_context)->GetBrowsingDataRemover();
    DCHECK(remover_);
    scoped_observation_.Observe(remover_.get());
  }

  ~SiteDataClearer() override {
    // This SiteDataClearer class is self-owned, and the only way for it to be
    // destroyed should be the "delete this" part in
    // OnBrowsingDataRemoverDone() function, and it invokes the |callback_|. So
    // when this destructor is called, the |callback_| should be null.
    DCHECK(!callback_);
  }

  void RunAndDestroySelfWhenDone() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // Cookies and channel IDs are scoped to
    // a) eTLD+1 of |origin|'s host if |origin|'s host is a registrable domain
    //    or a subdomain thereof
    // b) |origin|'s host exactly if it is an IP address or an internal hostname
    //    (e.g. "localhost" or "fileserver").
    if (clear_site_data_types_.Has(ClearSiteDataType::kCookies)) {
      std::string domain = GetDomainAndRegistry(
          origin_,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

      if (domain.empty()) {
        domain = origin_.host();  // IP address or internal hostname.
      }

      std::unique_ptr<BrowsingDataFilterBuilder> cookie_filter_builder(
          BrowsingDataFilterBuilder::Create(
              BrowsingDataFilterBuilder::Mode::kDelete));
      cookie_filter_builder->AddRegisterableDomain(domain);
      cookie_filter_builder->SetCookiePartitionKeyCollection(
          net::CookiePartitionKeyCollection::FromOptional(
              cookie_partition_key_));
      cookie_filter_builder->SetPartitionedCookiesOnly(
          partitioned_state_allowed_only_);
      if (storage_partition_config_.has_value()) {
        cookie_filter_builder->SetStoragePartitionConfig(
            storage_partition_config_.value());
      }

      pending_task_count_++;
      uint64_t remove_mask = BrowsingDataRemover::DATA_TYPE_COOKIES;
      if (avoid_closing_connections_) {
        remove_mask |= BrowsingDataRemover::DATA_TYPE_AVOID_CLOSING_CONNECTIONS;
      }
      remover_->RemoveWithFilterAndReply(
          base::Time(), base::Time::Max(), remove_mask,
          BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
              BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
          std::move(cookie_filter_builder), this);
    }

    // Storage buckets
    if (!storage_buckets_to_remove_.empty()) {
      pending_task_count_++;

      // For storage buckets, no mask is being passed per se. Therefore, when
      // the storage buckets are successfully removed, the `failed_data_types`
      // arg should be set to 0 to align with existing behaviour in this class.
      remover_->RemoveStorageBucketsAndReply(
          storage_partition_config_,
          storage_key_.value_or(blink::StorageKey::CreateFirstParty(origin_)),
          storage_buckets_to_remove_,
          base::BindOnce(&SiteDataClearer::OnBrowsingDataRemoverDone,
                         weak_factory_.GetWeakPtr(), 0));
    }

    // Delete origin-scoped data.
    uint64_t remove_mask = 0;
    if (clear_site_data_types_.Has(ClearSiteDataType::kStorage)) {
      remove_mask |= BrowsingDataRemover::DATA_TYPE_DOM_STORAGE;
      remove_mask |= BrowsingDataRemover::DATA_TYPE_PRIVACY_SANDBOX;
      // Internal data should not be removed by site-initiated deletions.
      remove_mask &= ~BrowsingDataRemover::DATA_TYPE_PRIVACY_SANDBOX_INTERNAL;
    }

    if (clear_site_data_types_.Has(ClearSiteDataType::kCache)) {
      remove_mask |= BrowsingDataRemover::DATA_TYPE_CACHE;
    }

    if (remove_mask) {
      std::unique_ptr<BrowsingDataFilterBuilder> origin_filter_builder(
          BrowsingDataFilterBuilder::Create(
              BrowsingDataFilterBuilder::Mode::kDelete));
      origin_filter_builder->AddOrigin(origin_);
      origin_filter_builder->SetStorageKey(storage_key_);
      if (storage_partition_config_.has_value()) {
        origin_filter_builder->SetStoragePartitionConfig(
            storage_partition_config_.value());
      }

      pending_task_count_++;
      remover_->RemoveWithFilterAndReply(
          base::Time(), base::Time::Max(), remove_mask,
          BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
              BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
          std::move(origin_filter_builder), this);
    }

    // We clear client hints for both cookie and cache clears.
    if (clear_site_data_types_.HasAny({ClearSiteDataType::kCookies,
                                       ClearSiteDataType::kCache,
                                       ClearSiteDataType::kClientHints})) {
      pending_task_count_++;

      // For client hints, no mask is being passed per se. Therefore, when
      // the client hints are successfully removed, the `failed_data_types`
      // arg should be set to 0 to align with existing behaviour in this class.
      remover_->ClearClientHintCacheAndReply(
          origin_, base::BindOnce(&SiteDataClearer::OnBrowsingDataRemoverDone,
                                  weak_factory_.GetWeakPtr(), 0));
    }

    DCHECK_GT(pending_task_count_, 0);
  }

 private:
  // BrowsingDataRemover::Observer:
  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override {
    DCHECK(pending_task_count_);
    if (--pending_task_count_) {
      return;
    }

    std::move(callback_).Run();
    delete this;
  }

  const std::optional<StoragePartitionConfig> storage_partition_config_;
  const url::Origin origin_;
  const ClearSiteDataTypeSet clear_site_data_types_;
  const std::set<std::string> storage_buckets_to_remove_;
  const bool avoid_closing_connections_;
  const std::optional<net::CookiePartitionKey> cookie_partition_key_;
  const std::optional<blink::StorageKey> storage_key_;
  const bool partitioned_state_allowed_only_;
  base::OnceClosure callback_;
  int pending_task_count_ = 0;
  raw_ptr<BrowsingDataRemoverImpl> remover_ = nullptr;
  base::ScopedObservation<BrowsingDataRemover, BrowsingDataRemover::Observer>
      scoped_observation_{this};
  base::WeakPtrFactory<SiteDataClearer> weak_factory_{this};
};

}  // namespace

void ClearSiteData(
    base::WeakPtr<BrowserContext> browser_context,
    std::optional<StoragePartitionConfig> storage_partition_config,
    const url::Origin& origin,
    const ClearSiteDataTypeSet clear_site_data_types,
    const std::set<std::string>& storage_buckets_to_remove,
    bool avoid_closing_connections,
    std::optional<net::CookiePartitionKey> cookie_partition_key,
    std::optional<blink::StorageKey> storage_key,
    bool partitioned_state_allowed_only,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // It's not possible to clear all storage and also only specific buckets.
  DCHECK(!clear_site_data_types.Has(ClearSiteDataType::kStorage) ||
         storage_buckets_to_remove.empty());
  if (!browser_context) {
    std::move(callback).Run();
    return;
  }
  (new SiteDataClearer(browser_context.get(), storage_partition_config, origin,
                       clear_site_data_types, storage_buckets_to_remove,
                       avoid_closing_connections, cookie_partition_key,
                       storage_key, partitioned_state_allowed_only,
                       std::move(callback)))
      ->RunAndDestroySelfWhenDone();
}

}  // namespace content
