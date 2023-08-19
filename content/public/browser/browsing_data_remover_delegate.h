// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSING_DATA_REMOVER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_BROWSING_DATA_REMOVER_DELEGATE_H_

#include <string>
#include <vector>
#include "base/functional/callback_forward.h"

namespace base {
class Time;
}

namespace storage {
class SpecialStoragePolicy;
}

namespace url {
class Origin;
}

namespace content {

class BrowsingDataFilterBuilder;
class StoragePartition;

class BrowsingDataRemoverDelegate {
 public:
  // Determines whether |origin| matches |origin_type_mask| given
  // the |special_storage_policy|.
  using EmbedderOriginTypeMatcher =
      base::RepeatingCallback<bool(uint64_t origin_type_mask,
                                   const url::Origin& origin,
                                   storage::SpecialStoragePolicy* policy)>;

  virtual ~BrowsingDataRemoverDelegate() {}

  // The embedder can define domains in the given StoragePartition for which
  // cookies are only deleted after all other deletions are finished.
  virtual std::vector<std::string> GetDomainsForDeferredCookieDeletion(
      StoragePartition* storage_partition,
      uint64_t remove_mask) = 0;

  // Returns a MaskMatcherFunction to match embedder's origin types.
  // This MaskMatcherFunction will be called with an |origin_type_mask|
  // parameter containing ONLY embedder-defined origin types, and must be able
  // to handle ALL embedder-defined typed. It must be static and support
  // being called on the UI and IO thread.
  virtual EmbedderOriginTypeMatcher GetOriginTypeMatcher() = 0;

  // Whether the embedder allows the removal of download history.
  virtual bool MayRemoveDownloadHistory() = 0;

  // Removes embedder-specific data. Once done, calls the |callback| and passes
  // back any data types for which the deletion failed (which will always be a
  // subset of the |remove_mask|).
  virtual void RemoveEmbedderData(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      uint64_t remove_mask,
      BrowsingDataFilterBuilder* filter_builder,
      uint64_t origin_type_mask,
      base::OnceCallback<void(/*failed_data_types=*/uint64_t)> callback) = 0;

  // Called when the BrowsingDataRemover starts executing a task.
  virtual void OnStartRemoving() {}

  // Called when the BrowsingDataRemover is done executing all the tasks in its
  // queue.
  virtual void OnDoneRemoving() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSING_DATA_REMOVER_DELEGATE_H_
