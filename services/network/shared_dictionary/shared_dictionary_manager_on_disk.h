// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_MANAGER_ON_DISK_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_MANAGER_ON_DISK_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "net/extras/shared_dictionary/shared_dictionary_info.h"
#include "net/extras/sqlite/sqlite_persistent_shared_dictionary_store.h"
#include "services/network/shared_dictionary/shared_dictionary_disk_cache.h"
#include "services/network/shared_dictionary/shared_dictionary_manager.h"
#include "services/network/shared_dictionary/shared_dictionary_writer_on_disk.h"

class GURL;

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

// A SharedDictionaryManager which persists dictionary information on disk.
class SharedDictionaryManagerOnDisk : public SharedDictionaryManager {
 public:
  SharedDictionaryManagerOnDisk(
      const base::FilePath& database_path,
      const base::FilePath& cache_directory_path,
#if BUILDFLAG(IS_ANDROID)
      base::android::ApplicationStatusListener* app_status_listener,
#endif  // BUILDFLAG(IS_ANDROID)
      scoped_refptr<disk_cache::BackendFileOperationsFactory>
          file_operations_factory);

  SharedDictionaryManagerOnDisk(const SharedDictionaryManagerOnDisk&) = delete;
  SharedDictionaryManagerOnDisk& operator=(
      const SharedDictionaryManagerOnDisk&) = delete;

  ~SharedDictionaryManagerOnDisk() override;

  // SharedDictionaryManager
  scoped_refptr<SharedDictionaryStorage> CreateStorage(
      const net::SharedDictionaryStorageIsolationKey& isolation_key) override;

  SharedDictionaryDiskCache& disk_cache() { return disk_cache_; }
  net::SQLitePersistentSharedDictionaryStore& metadata_store() {
    return metadata_store_;
  }

  scoped_refptr<SharedDictionaryWriter> CreateWriter(
      const net::SharedDictionaryStorageIsolationKey& isolation_key,
      const GURL& url,
      base::Time response_time,
      base::TimeDelta expiration,
      const std::string& match,
      base::OnceCallback<void(net::SharedDictionaryInfo)> callback);

 private:
  void OnDictionaryWrittenInDiskCache(
      const net::SharedDictionaryStorageIsolationKey& isolation_key,
      const GURL& url,
      base::Time response_time,
      base::TimeDelta expiration,
      const std::string& match,
      const base::UnguessableToken& disk_cache_key_token,
      base::OnceCallback<void(net::SharedDictionaryInfo)> callback,
      SharedDictionaryWriterOnDisk::Result result,
      size_t size,
      const net::SHA256HashValue& hash);

  void OnDictionaryWrittenInDatabase(
      net::SharedDictionaryInfo info,
      base::OnceCallback<void(net::SharedDictionaryInfo)> callback,
      net::SQLitePersistentSharedDictionaryStore::
          RegisterDictionaryResultOrError result);

  SharedDictionaryDiskCache disk_cache_;
  net::SQLitePersistentSharedDictionaryStore metadata_store_;

  base::WeakPtrFactory<SharedDictionaryManagerOnDisk> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_MANAGER_ON_DISK_H_
