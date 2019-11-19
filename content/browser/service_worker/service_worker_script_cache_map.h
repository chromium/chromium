// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CACHE_MAP_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CACHE_MAP_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_database.h"
#include "content/common/content_export.h"
#include "net/base/completion_once_callback.h"
#include "net/url_request/url_request_status.h"

class GURL;

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerVersion;
class ServiceWorkerResponseMetadataWriter;

// Class that maintains the mapping between urls and a resource id
// for a particular version's implicit script resources.
class CONTENT_EXPORT ServiceWorkerScriptCacheMap {
 public:
  int64_t LookupResourceId(const GURL& url);

  // Used during the initial run of a new version to build the map
  // of resources ids.
  void NotifyStartedCaching(const GURL& url, int64_t resource_id);
  void NotifyFinishedCaching(const GURL& url,
                             int64_t size_bytes,
                             net::Error net_error,
                             const std::string& status_message);

  // Used to retrieve the results of the initial run of a new version.
  void GetResources(
      std::vector<ServiceWorkerDatabase::ResourceRecord>* resources);

  // Used when loading an existing version.
  void SetResources(
     const std::vector<ServiceWorkerDatabase::ResourceRecord>& resources);

  // Writes the metadata of the existing script.
  void WriteMetadata(const GURL& url,
                     base::span<const uint8_t> data,
                     net::CompletionOnceCallback callback);
  // Clears the metadata of the existing script.
  void ClearMetadata(const GURL& url, net::CompletionOnceCallback callback);

  size_t size() const { return resource_map_.size(); }

  const net::URLRequestStatus& main_script_status() const {
    return main_script_status_;
  }

  const std::string& main_script_status_message() const {
    return main_script_status_message_;
  }

 private:
  typedef std::map<GURL, ServiceWorkerDatabase::ResourceRecord> ResourceMap;

  // The version objects owns its script cache and provides a rawptr to it.
  friend class ServiceWorkerVersion;
  friend class ServiceWorkerVersionBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerReadFromCacheJobTest, ResourceNotFound);

  ServiceWorkerScriptCacheMap(
      ServiceWorkerVersion* owner,
      base::WeakPtr<ServiceWorkerContextCore> context);
  ~ServiceWorkerScriptCacheMap();

  void OnMetadataWritten(
      std::unique_ptr<ServiceWorkerResponseMetadataWriter> writer,
      net::CompletionOnceCallback callback,
      int result);

  ServiceWorkerVersion* owner_;
  base::WeakPtr<ServiceWorkerContextCore> context_;
  ResourceMap resource_map_;
  net::URLRequestStatus main_script_status_;
  std::string main_script_status_message_;

  base::WeakPtrFactory<ServiceWorkerScriptCacheMap> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerScriptCacheMap);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CACHE_MAP_H_
