// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CONTENT_INDEX_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_CONTENT_INDEX_CONTEXT_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/public/browser/content_index_provider.h"
#include "third_party/blink/public/mojom/content_index/content_index.mojom.h"

class SkBitmap;

namespace url {
class Origin;
}  // namespace url

namespace content {

// Owned by the Storage Partition. This is used by the ContentIndexProvider to
// query auxiliary data for its entries from the right source.
class CONTENT_EXPORT ContentIndexContext {
 public:
  using GetAllEntriesCallback =
      base::OnceCallback<void(blink::mojom::ContentIndexError,
                              std::vector<ContentIndexEntry>)>;
  using GetEntryCallback =
      base::OnceCallback<void(base::Optional<ContentIndexEntry>)>;
  using GetIconsCallback = base::OnceCallback<void(std::vector<SkBitmap>)>;

  ContentIndexContext() = default;
  virtual ~ContentIndexContext() = default;

  // Returns all available icons for the entry identified by
  // |service_worker_registration_id| and |description_id|.
  // The number of icons and the sizes are chosen by the ContentIndexProvider.
  // Must be called on the UI thread. |callback| must be invoked on the UI
  // the UI thread.
  virtual void GetIcons(int64_t service_worker_registration_id,
                        const std::string& description_id,
                        GetIconsCallback callback) = 0;

  // Must be called on the UI thread.
  virtual void GetAllEntries(GetAllEntriesCallback callback) = 0;

  // Must be called on the UI thread.
  virtual void GetEntry(int64_t service_worker_registration_id,
                        const std::string& description_id,
                        GetEntryCallback callback) = 0;

  // Called when a user deleted an item. Must be called on the UI thread.
  virtual void OnUserDeletedItem(int64_t service_worker_registration_id,
                                 const url::Origin& origin,
                                 const std::string& description_id) = 0;

  DISALLOW_COPY_AND_ASSIGN(ContentIndexContext);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONTENT_INDEX_CONTEXT_H_
