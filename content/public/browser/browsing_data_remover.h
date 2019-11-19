// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSING_DATA_REMOVER_H_
#define CONTENT_PUBLIC_BROWSER_BROWSING_DATA_REMOVER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
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

////////////////////////////////////////////////////////////////////////////////
// BrowsingDataRemover is responsible for removing data related to browsing:
// visits in url database, downloads, cookies ...
//
//  USAGE:
//
//  0. Instantiation.
//
//       content::BrowsingDataRemover* remover =
//           content::BrowserContext::GetBrowsingDataRemover(browser_context);
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
// TODO(crbug.com/668114): BrowsingDataRemover does not currently support plugin
// data deletion. Use PluginDataRemover instead.
class BrowsingDataRemover {
 public:
  // Mask used for Remove.
  enum DataType {
    // Storage datatypes.
    DATA_TYPE_APP_CACHE = 1 << 0,
    DATA_TYPE_FILE_SYSTEMS = 1 << 1,
    DATA_TYPE_INDEXED_DB = 1 << 2,
    DATA_TYPE_LOCAL_STORAGE = 1 << 3,
    DATA_TYPE_WEB_SQL = 1 << 4,
    DATA_TYPE_SERVICE_WORKERS = 1 << 5,
    DATA_TYPE_CACHE_STORAGE = 1 << 6,
    // This is also persisted, keep with storage datatypes.
    DATA_TYPE_BACKGROUND_FETCH = 1 << 14,

    // Used to request the deletion of embedder-specific storage datatypes.
    DATA_TYPE_EMBEDDER_DOM_STORAGE = 1 << 7,

    // DOM-accessible storage (https://www.w3.org/TR/clear-site-data/#storage).
    // Has the same effect as selecting all storage datatypes listed above
    // and ones defined by the embedder.
    DATA_TYPE_DOM_STORAGE =
        DATA_TYPE_APP_CACHE | DATA_TYPE_FILE_SYSTEMS | DATA_TYPE_INDEXED_DB |
        DATA_TYPE_LOCAL_STORAGE | DATA_TYPE_WEB_SQL |
        DATA_TYPE_SERVICE_WORKERS | DATA_TYPE_CACHE_STORAGE |
        DATA_TYPE_EMBEDDER_DOM_STORAGE | DATA_TYPE_BACKGROUND_FETCH,

    // Other datatypes.
    DATA_TYPE_COOKIES = 1 << 8,
    DATA_TYPE_CACHE = 1 << 10,
    DATA_TYPE_DOWNLOADS = 1 << 11,
    DATA_TYPE_MEDIA_LICENSES = 1 << 12,

    // REMOVE_NOCHECKS intentionally does not check if the browser context is
    // prohibited from deleting history or downloads.
    DATA_TYPE_NO_CHECKS = 1 << 13,

    // AVOID_CLOSING_CONNECTIONS is a pseudo-datatype indicating that when
    // deleting COOKIES, BrowsingDataRemover should skip
    // storage backends whose deletion would cause closing network connections.
    // TODO(crbug.com/798760): Remove when fixed.
    DATA_TYPE_AVOID_CLOSING_CONNECTIONS = 1 << 15,

    // Embedders can add more datatypes beyond this point.
    DATA_TYPE_CONTENT_END = DATA_TYPE_AVOID_CLOSING_CONNECTIONS,
  };

  enum OriginType {
    // Web storage origins that StoragePartition recognizes as NOT protected
    // according to its special storage policy.
    ORIGIN_TYPE_UNPROTECTED_WEB = 1 << 0,

    // Web storage origins that StoragePartition recognizes as protected
    // according to its special storage policy.
    ORIGIN_TYPE_PROTECTED_WEB = 1 << 1,

    // Embedders can add more origin types beyond this point.
    ORIGIN_TYPE_CONTENT_END = ORIGIN_TYPE_PROTECTED_WEB,
  };

  // A helper enum to report the deletion of cookies and/or cache. Do not
  // reorder the entries, as this enum is passed to UMA.
  // A Java counterpart will be generated for this enum so that it can be
  // logged on Android.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.browsing_data
  enum CookieOrCacheDeletionChoice {
    NEITHER_COOKIES_NOR_CACHE,
    ONLY_COOKIES,
    ONLY_CACHE,
    BOTH_COOKIES_AND_CACHE,
    MAX_CHOICE_VALUE
  };

  // Observer is notified when its own removal task is done.
  class Observer {
   public:
    // Called when a removal task is finished. Note that every removal task can
    // only have one observer attached to it, and only that one is called.
    virtual void OnBrowsingDataRemoverDone() = 0;

   protected:
    virtual ~Observer() {}
  };

  // A delegate that will take care of deleting embedder-specific data.
  virtual void SetEmbedderDelegate(
      BrowsingDataRemoverDelegate* embedder_delegate) = 0;

  // Determines whether |origin| matches the |origin_type_mask| according to
  // the |special_storage_policy|.
  virtual bool DoesOriginMatchMask(
      int origin_type_mask,
      const url::Origin& origin,
      storage::SpecialStoragePolicy* special_storage_policy) = 0;

  // Removes browsing data within the given |time_range|, with datatypes being
  // specified by |remove_mask| and origin types by |origin_type_mask|.
  virtual void Remove(const base::Time& delete_begin,
                      const base::Time& delete_end,
                      int remove_mask,
                      int origin_type_mask) = 0;

  // A version of the above that in addition informs the |observer| when the
  // removal task is finished.
  virtual void RemoveAndReply(const base::Time& delete_begin,
                              const base::Time& delete_end,
                              int remove_mask,
                              int origin_type_mask,
                              Observer* observer) = 0;

  // Like Remove(), but in case of URL-keyed only removes data whose URL match
  // |filter_builder| (e.g. are on certain origin or domain).
  // RemoveWithFilter() currently only works with FILTERABLE_DATA_TYPES.
  virtual void RemoveWithFilter(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      int remove_mask,
      int origin_type_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter_builder) = 0;

  // A version of the above that in addition informs the |observer| when the
  // removal task is finished.
  virtual void RemoveWithFilterAndReply(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      int remove_mask,
      int origin_type_mask,
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
  virtual const base::Time& GetLastUsedBeginTime() = 0;
  virtual const base::Time& GetLastUsedEndTime() = 0;
  virtual int GetLastUsedRemovalMask() = 0;
  virtual int GetLastUsedOriginTypeMask() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSING_DATA_REMOVER_H_
