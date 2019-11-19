// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSING_DATA_REMOVER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_BROWSING_DATA_REMOVER_DELEGATE_H_

#include "base/callback_forward.h"

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

class BrowsingDataRemoverDelegate {
 public:
  // Determines whether |origin| matches |origin_type_mask| given
  // the |special_storage_policy|.
  using EmbedderOriginTypeMatcher =
      base::Callback<bool(int origin_type_mask,
                          const url::Origin& origin,
                          storage::SpecialStoragePolicy* policy)>;

  virtual ~BrowsingDataRemoverDelegate() {}

  // Returns a MaskMatcherFunction to match embedder's origin types.
  // This MaskMatcherFunction will be called with an |origin_type_mask|
  // parameter containing ONLY embedder-defined origin types, and must be able
  // to handle ALL embedder-defined typed. It must be static and support
  // being called on the UI and IO thread.
  virtual EmbedderOriginTypeMatcher GetOriginTypeMatcher() = 0;

  // Whether the embedder allows the removal of download history.
  virtual bool MayRemoveDownloadHistory() = 0;

  // Removes embedder-specific data.
  virtual void RemoveEmbedderData(const base::Time& delete_begin,
                                  const base::Time& delete_end,
                                  int remove_mask,
                                  BrowsingDataFilterBuilder* filter_builder,
                                  int origin_type_mask,
                                  base::OnceClosure callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSING_DATA_REMOVER_DELEGATE_H_
