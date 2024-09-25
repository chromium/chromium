// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSING_DATA_REMOVER_H_
#define CONTENT_PUBLIC_BROWSER_BROWSING_DATA_REMOVER_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace storage {
class SpecialStoragePolicy;
}

namespace url {
class Origin;
}

namespace content {

class BrowsingDataFilterBuilder;
class BrowsingDataRemoverDelegate;
class StoragePartitionConfig;

////////////////////////////////////////////////////////////////////////////////
// BrowsingDataRemover is responsible for removing data related to browsing:
// visits in url database, downloads, cookies ...
//
//  USAGE:
//
//  0. Instantiation.
//
//       content::BrowsingDataRemover* remover =
//           browser_context->GetBrowsingDataRemover();
//
//  1. No observer.
//
//       remover->Remove(base::Time(), base::Time::Max(), REMOVE_COOKIES, ALL);
//
//  2. Using an observer to report when one's own removal task is finished.
//
//       class CookiesDeleter : public content::BrowsingDataRemover::Observer {
//         CookiesDeleter() { remover->AddObserver(this); }
//         ~CookiesDeleter() { remover->RemoveObserver(this); }
//
//         void DeleteCookies() {
//           remover->RemoveAndReply(base::Time(), base::Time::Max(),
//                                   REMOVE_COOKIES, ALL, this);
//         }
//
//         void OnBrowsingDataRemoverDone() {
//           LOG(INFO) << "Cookies were deleted.";
//         }
//       }
//
////////////////////////////////////////////////////////////////////////////////
//
// TODO(crbug.com/40495069): BrowsingDataRemover does not currently support
// plugin data deletion. Use PluginDataRemover instead.
class BrowsingDataRemover {
 public:
  // Mask used for Remove.
  using DataType = uint64_t;
  // Storage datatypes.
  static constexpr DataType DATA_TYPE_APP_CACHE_DEPRECATED = 1 << 0;
  static constexpr DataType DATA_TYPE_FILE_SYSTEMS = 1 << 1;
  static constexpr DataType DATA_TYPE_INDEXED_DB = 1 << 2;
  static constexpr DataType DATA_TYPE_LOCAL_STORAGE = 1 << 3;
  static constexpr DataType DATA_TYPE_WEB_SQL = 1 << 4;
  static constexpr DataType DATA_TYPE_SERVICE_WORKERS = 1 << 5;
  static constexpr DataType DATA_TYPE_CACHE_STORAGE = 1 << 6;
  // This is also persisted, keep with storage datatypes.
  static constexpr DataType DATA_TYPE_BACKGROUND_FETCH = 1 << 14;

  // Used to request the deletion of embedder-specific storage datatypes.
  static constexpr DataType DATA_TYPE_EMBEDDER_DOM_STORAGE = 1 << 7;

  // DOM-accessible storage (https://www.w3.org/TR/clear-site-data/#storage).
  // Has the same effect as selecting all storage datatypes listed above
  // and ones defined by the embedder.
  static constexpr DataType DATA_TYPE_DOM_STORAGE =
      DATA_TYPE_FILE_SYSTEMS | DATA_TYPE_INDEXED_DB | DATA_TYPE_LOCAL_STORAGE |
      DATA_TYPE_WEB_SQL | DATA_TYPE_SERVICE_WORKERS | DATA_TYPE_CACHE_STORAGE |
      DATA_TYPE_EMBEDDER_DOM_STORAGE | DATA_TYPE_BACKGROUND_FETCH;

  // Other datatypes.
  static constexpr DataType DATA_TYPE_COOKIES = 1 << 8;
  static constexpr DataType DATA_TYPE_CACHE = 1 << 10;
  static constexpr DataType DATA_TYPE_DOWNLOADS = 1 << 11;
  static constexpr DataType DATA_TYPE_MEDIA_LICENSES = 1 << 12;

  // REMOVE_NOCHECKS intentionally does not check if the browser context is
  // prohibited from deleting history or downloads.
  static constexpr DataType DATA_TYPE_NO_CHECKS = 1 << 13;

  // 14 is already taken by DATA_TYPE_BACKGROUND_FETCH.

  // AVOID_CLOSING_CONNECTIONS is a pseudo-datatype indicating that when
  // deleting COOKIES, BrowsingDataRemover should skip
  // storage backends whose deletion would cause closing network connections.
  // TODO(crbug.com/41363015): Remove when fixed.
  static constexpr DataType DATA_TYPE_AVOID_CLOSING_CONNECTIONS = 1 << 15;

  // Trust Token API (https://github.com/wicg/trust-token-api) persistent
  // storage.
  static constexpr DataType DATA_TYPE_TRUST_TOKENS = 1 << 16;

  // Attribution Reporting
  // (https://github.com/WICG/conversion-measurement-api) persistent
  // storage that was initiated by a site.
  static constexpr DataType DATA_TYPE_ATTRIBUTION_REPORTING_SITE_CREATED =
      1 << 17;

  // Aggregation Service
  // (https://github.com/WICG/attribution-reporting-api/blob/main/AGGREGATE.md#data-processing-through-a-secure-aggregation-service)
  // persistent storage.
  static constexpr DataType DATA_TYPE_AGGREGATION_SERVICE = 1 << 18;

  // Interest groups are stored as part of the Interest Group API experiment
  // Public explainer here:
  // https://github.com/WICG/turtledove/blob/main/FLEDGE.md
  static constexpr DataType DATA_TYPE_INTEREST_GROUPS = 1 << 19;

  // Shared storage API
  // (https://github.com/pythagoraskitty/shared-storage) persistent storage.
  static constexpr DataType DATA_TYPE_SHARED_STORAGE = 1 << 20;

  // Similar to DATA_TYPE_ATTRIBUTION_REPORTING_SITE_INITIATED, but only
  // refers to data stored internally by the API, such as privacy budgeting
  // information.
  static constexpr DataType DATA_TYPE_ATTRIBUTION_REPORTING_INTERNAL = 1 << 21;

  // Private Aggregation API
  // (https://github.com/alexmturner/private-aggregation-api) persistent
  // storage. This only refers to data stored internally by the API, such as
  // privacy budgeting information. Note that currently the API does not persist
  // any other data. Should only be cleared by user-initiated deletions.
  static constexpr DataType DATA_TYPE_PRIVATE_AGGREGATION_INTERNAL = 1 << 22;

  // Similar to DATA_TYPE_INTEREST_GROUPS, but only refers to data stored
  // internally by the API, such as k-Anonymity cache and rate limiting
  // information.
  static constexpr DataType DATA_TYPE_INTEREST_GROUPS_INTERNAL = 1 << 23;

  // Permissions granted by Related Website Sets
  // (https://github.com/WICG/first-party-sets).
  static constexpr DataType DATA_TYPE_RELATED_WEBSITE_SETS_PERMISSIONS = 1
                                                                         << 24;

  // Embedders can add more datatypes beyond this point.
  static constexpr DataType DATA_TYPE_CONTENT_END =
      DATA_TYPE_RELATED_WEBSITE_SETS_PERMISSIONS;

  // All data stored by the Attribution Reporting API.
  static constexpr DataType DATA_TYPE_ATTRIBUTION_REPORTING =
      DATA_TYPE_ATTRIBUTION_REPORTING_SITE_CREATED |
      DATA_TYPE_ATTRIBUTION_REPORTING_INTERNAL;

  // Data stored by APIs in The Privacy Sandbox (https://privacysandbox.com/).
  static constexpr DataType DATA_TYPE_PRIVACY_SANDBOX =
      DATA_TYPE_TRUST_TOKENS | DATA_TYPE_ATTRIBUTION_REPORTING |
      DATA_TYPE_AGGREGATION_SERVICE | DATA_TYPE_INTEREST_GROUPS |
      DATA_TYPE_SHARED_STORAGE | DATA_TYPE_PRIVATE_AGGREGATION_INTERNAL |
      DATA_TYPE_INTEREST_GROUPS_INTERNAL;

  // Internal data stored by APIs in the Privacy Sandbox, e.g. privacy budgeting
  // information.
  static constexpr DataType DATA_TYPE_PRIVACY_SANDBOX_INTERNAL =
      DATA_TYPE_ATTRIBUTION_REPORTING_INTERNAL |
      DATA_TYPE_PRIVATE_AGGREGATION_INTERNAL |
      DATA_TYPE_INTEREST_GROUPS_INTERNAL;

  // Data types stored within a StoragePartition (i.e. not Profile-scoped).
  static constexpr DataType DATA_TYPE_ON_STORAGE_PARTITION =
      DATA_TYPE_DOM_STORAGE | DATA_TYPE_COOKIES |
      DATA_TYPE_AVOID_CLOSING_CONNECTIONS | DATA_TYPE_CACHE |
      DATA_TYPE_APP_CACHE_DEPRECATED | DATA_TYPE_PRIVACY_SANDBOX;

  using OriginType = uint64_t;
  // Web storage origins that StoragePartition recognizes as NOT protected
  // according to its special storage policy.
  static constexpr OriginType ORIGIN_TYPE_UNPROTECTED_WEB = 1 << 0;

  // Web storage origins that StoragePartition recognizes as protected
  // according to its special storage policy.
  static constexpr OriginType ORIGIN_TYPE_PROTECTED_WEB = 1 << 1;

  // Embedders can add more origin types beyond this point.
  static constexpr OriginType ORIGIN_TYPE_CONTENT_END =
      ORIGIN_TYPE_PROTECTED_WEB;

  // Observer is notified when its own removal task is done.
  class Observer {
   public:
    // Called when a removal task is finished. Note that every removal task can
    // only have one observer attached to it, and only that one is called.
    // |failed_data_types| is a bitmask of DataTypes (including those defined by
    // embedders) for which the deletion did not successfully complete. It will
    // always be a subset of the |remove_mask| passed into Remove*().
    virtual void OnBrowsingDataRemoverDone(uint64_t failed_data_types) = 0;

   protected:
    virtual ~Observer() {}
  };

  // A delegate that will take care of deleting embedder-specific data.
  virtual void SetEmbedderDelegate(
      BrowsingDataRemoverDelegate* embedder_delegate) = 0;

  // Determines whether |origin| matches the |origin_type_mask| according to
  // the |special_storage_policy|.
  virtual bool DoesOriginMatchMaskForTesting(
      uint64_t origin_type_mask,
      const url::Origin& origin,
      storage::SpecialStoragePolicy* special_storage_policy) = 0;

  // Removes browsing data within the given |time_range|, with datatypes being
  // specified by |remove_mask| and origin types by |origin_type_mask|.
  virtual void Remove(const base::Time& delete_begin,
                      const base::Time& delete_end,
                      uint64_t remove_mask,
                      uint64_t origin_type_mask) = 0;

  // A version of the above that applies only removes entries that match the
  // provided filter.
  virtual void RemoveWithFilter(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      uint64_t remove_mask,
      uint64_t origin_type_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter_builder) = 0;

  // A version of the above that in addition informs the |observer| when the
  // removal task is finished.
  virtual void RemoveAndReply(const base::Time& delete_begin,
                              const base::Time& delete_end,
                              uint64_t remove_mask,
                              uint64_t origin_type_mask,
                              Observer* observer) = 0;

  // A version of the above that in addition informs the |observer| when the
  // removal task is finished.
  virtual void RemoveWithFilterAndReply(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      uint64_t remove_mask,
      uint64_t origin_type_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
      Observer* observer) = 0;

  // Observers.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // A |callback| that will be called just before a deletion task is completed
  // and observers are notified. The receiver must respond by calling
  // |continue_to_completion| to finish the task. Used in tests to artificially
  // prolong execution.
  virtual void SetWouldCompleteCallbackForTesting(
      const base::RepeatingCallback<
          void(base::OnceClosure continue_to_completion)>& callback) = 0;

  // Parameters of the last call are exposed to be used by tests. Removal and
  // origin type masks equal to -1 mean that no removal has ever been executed.
  // TODO(msramek): If other consumers than tests are interested in this,
  // consider returning them in OnBrowsingDataRemoverDone() callback. If not,
  // consider simplifying this interface by removing these methods and changing
  // the tests to record the parameters using GMock instead.
  virtual const base::Time& GetLastUsedBeginTimeForTesting() = 0;
  virtual uint64_t GetLastUsedRemovalMaskForTesting() = 0;
  virtual uint64_t GetLastUsedOriginTypeMaskForTesting() = 0;
  virtual std::optional<StoragePartitionConfig>
  GetLastUsedStoragePartitionConfigForTesting() = 0;
  virtual uint64_t GetPendingTaskCountForTesting() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSING_DATA_REMOVER_H_
