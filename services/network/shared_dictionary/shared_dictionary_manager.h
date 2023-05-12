// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_MANAGER_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_MANAGER_H_

#include <map>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "net/extras/shared_dictionary/shared_dictionary_storage_isolation_key.h"

namespace base {
namespace android {
class ApplicationStatusListener;
}  // namespace android
class FilePath;
}  //  namespace base

namespace disk_cache {
class BackendFileOperationsFactory;
}  // namespace disk_cache

namespace network {

class SharedDictionaryStorage;

// This class is attached to NetworkContext and manages the dictionaries for
// CompressionDictionaryTransport feature.
class COMPONENT_EXPORT(NETWORK_SERVICE) SharedDictionaryManager {
 public:
  // Returns a SharedDictionaryManager which keeps the whole dictionary
  // information in memory.
  static std::unique_ptr<SharedDictionaryManager> CreateInMemory();

  // Returns a SharedDictionaryManager which keeps the dictionary information
  // on disk.
  static std::unique_ptr<SharedDictionaryManager> CreateOnDisk(
      const base::FilePath& database_path,
      const base::FilePath& cache_directory_path,
#if BUILDFLAG(IS_ANDROID)
      base::android::ApplicationStatusListener* app_status_listener,
#endif  // BUILDFLAG(IS_ANDROID)
      scoped_refptr<disk_cache::BackendFileOperationsFactory>
          file_operations_factory);

  // TODO(crbug.com/1413922): Implement a manager which supports persistence
  // and use if for non-incognito mode. Also, if preventing incognito mode
  // detection isn't that important, and the maintenance cost of two storagee is
  // large, consider removing  SharedDictionaryManager and stopping incognito
  // mode support.

  SharedDictionaryManager(const SharedDictionaryManager&) = delete;
  SharedDictionaryManager& operator=(const SharedDictionaryManager&) = delete;

  virtual ~SharedDictionaryManager();

  // Returns a SharedDictionaryStorage for the `isolation_key`.
  scoped_refptr<SharedDictionaryStorage> GetStorage(
      const net::SharedDictionaryStorageIsolationKey& isolation_key);

  // TODO(crbug.com/1413922): Add a method to delete dictionaries when the user
  // clears the browsing data (BrowsingDataRemover::DATA_TYPE_CACHE and
  // DATA_TYPE_SITE_DATA).

  // Called when the SharedDictionaryStorage for the `isolation_key` is
  // deleted.
  void OnStorageDeleted(
      const net::SharedDictionaryStorageIsolationKey& isolation_key);

 protected:
  SharedDictionaryManager();

  // Called to create a SharedDictionaryStorage for the `isolation_key`. This is
  // called only when there is no matching storage in `storages_`.
  virtual scoped_refptr<SharedDictionaryStorage> CreateStorage(
      const net::SharedDictionaryStorageIsolationKey& isolation_key) = 0;

  base::WeakPtr<SharedDictionaryManager> GetWeakPtr();

 private:
  std::map<net::SharedDictionaryStorageIsolationKey,
           raw_ptr<SharedDictionaryStorage>>
      storages_;
  base::WeakPtrFactory<SharedDictionaryManager> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_MANAGER_H_
