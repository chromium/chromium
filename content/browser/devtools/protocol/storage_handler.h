// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STORAGE_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STORAGE_HANDLER_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/storage.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"

namespace content {
class StoragePartition;

namespace protocol {

class StorageHandler : public DevToolsDomainHandler,
                       public Storage::Backend {
 public:
  StorageHandler();
  ~StorageHandler() override;

  // content::protocol::DevToolsDomainHandler
  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  Response Disable() override;

  // content::protocol::storage::Backend
  void ClearDataForOrigin(
      const std::string& origin,
      const std::string& storage_types,
      std::unique_ptr<ClearDataForOriginCallback> callback) override;
  void GetUsageAndQuota(
      const String& origin,
      std::unique_ptr<GetUsageAndQuotaCallback> callback) override;

  // Ignores all double calls to track an origin.
  Response TrackCacheStorageForOrigin(const std::string& origin) override;
  Response UntrackCacheStorageForOrigin(const std::string& origin) override;
  Response TrackIndexedDBForOrigin(const std::string& origin) override;
  Response UntrackIndexedDBForOrigin(const std::string& origin) override;

 private:
  // See definition for lifetime information.
  class CacheStorageObserver;
  class IndexedDBObserver;

  // Not thread safe.
  CacheStorageObserver* GetCacheStorageObserver();
  IndexedDBObserver* GetIndexedDBObserver();

  void NotifyCacheStorageListChanged(const std::string& origin);
  void NotifyCacheStorageContentChanged(const std::string& origin,
                                        const std::string& name);
  void NotifyIndexedDBListChanged(const std::string& origin);
  void NotifyIndexedDBContentChanged(const std::string& origin,
                                     const base::string16& database_name,
                                     const base::string16& object_store_name);

  std::unique_ptr<Storage::Frontend> frontend_;
  StoragePartition* storage_partition_;
  std::unique_ptr<CacheStorageObserver> cache_storage_observer_;
  std::unique_ptr<IndexedDBObserver> indexed_db_observer_;

  base::WeakPtrFactory<StorageHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(StorageHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STORAGE_HANDLER_H_
