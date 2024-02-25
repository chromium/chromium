// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CACHE_MAP_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CACHE_MAP_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/services/storage/public/mojom/service_worker_storage_control.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"

class GURL;

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerVersion;

// Class that maintains the mapping between urls and a resource id
// for a particular version's implicit script resources.
class CONTENT_EXPORT ServiceWorkerScriptCacheMap {
 public:
  ServiceWorkerScriptCacheMap(const ServiceWorkerScriptCacheMap&) = delete;
  ServiceWorkerScriptCacheMap& operator=(const ServiceWorkerScriptCacheMap&) =
      delete;

  int64_t LookupResourceId(const GURL& url);
  std::optional<std::string> LookupSha256Checksum(const GURL& url);

  // Used during the initial run of a new version to build the map
  // of resources ids.
  void NotifyStartedCaching(const GURL& url, int64_t resource_id);
  void NotifyFinishedCaching(const GURL& url,
                             int64_t size_bytes,
                             const std::string& sha256_checksum,
                             net::Error net_error,
                             const std::string& status_message);

  // Used to retrieve the results of the initial run of a new version.
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> GetResources()
      const;

  // Used when loading an existing version.
  void SetResources(
      const std::vector<storage::mojom::ServiceWorkerResourceRecordPtr>&
          resources);

  // Updates sha256_checksum to the resource. sha256_checksum can be updated
  // after the update check process.
  void UpdateSha256Checksum(const GURL& url,
                            const std::string& sha256_checksum);

  // Writes the metadata of the existing script.
  void WriteMetadata(const GURL& url,
                     base::span<const uint8_t> data,
                     net::CompletionOnceCallback callback);
  // Clears the metadata of the existing script.
  void ClearMetadata(const GURL& url, net::CompletionOnceCallback callback);

  size_t size() const { return resource_map_.size(); }

  // net::Error code from trying to load the main script resource.
  int main_script_net_error() const { return main_script_net_error_; }

  const std::string& main_script_status_message() const {
    return main_script_status_message_;
  }

 private:
  typedef std::map<GURL, storage::mojom::ServiceWorkerResourceRecordPtr>
      ResourceMap;

  // The version objects owns its script cache and provides a rawptr to it.
  friend class ServiceWorkerVersion;
  friend class ServiceWorkerVersionBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerReadFromCacheJobTest, ResourceNotFound);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerBrowserTest,
                           DispatchFetchEventToBrokenWorker);

  ServiceWorkerScriptCacheMap(
      ServiceWorkerVersion* owner,
      base::WeakPtr<ServiceWorkerContextCore> context);
  ~ServiceWorkerScriptCacheMap();

  void OnWriterDisconnected(uint64_t callback_id);
  void OnMetadataWritten(
      mojo::Remote<storage::mojom::ServiceWorkerResourceMetadataWriter>,
      uint64_t callback_id,
      int result);

  void RunCallback(uint64_t callback_id, int result);

  raw_ptr<ServiceWorkerVersion> owner_;
  base::WeakPtr<ServiceWorkerContextCore> context_;
  ResourceMap resource_map_;
  int main_script_net_error_ = net::OK;
  std::string main_script_status_message_;
  uint64_t next_callback_id_ = 0;
  base::flat_map</*callback_id=*/uint64_t, net::CompletionOnceCallback>
      callbacks_;

  base::WeakPtrFactory<ServiceWorkerScriptCacheMap> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CACHE_MAP_H_
