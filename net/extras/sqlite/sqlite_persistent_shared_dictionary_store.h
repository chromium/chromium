// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_EXTRAS_SQLITE_SQLITE_PERSISTENT_SHARED_DICTIONARY_STORE_H_
#define NET_EXTRAS_SQLITE_SQLITE_PERSISTENT_SHARED_DICTIONARY_STORE_H_

#include <map>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "net/extras/shared_dictionary/shared_dictionary_info.h"
#include "url/origin.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace net {

class SharedDictionaryStorageIsolationKey;

// This class is used for storing SharedDictionary information to the persistent
// storage.
class COMPONENT_EXPORT(NET_EXTRAS) SQLitePersistentSharedDictionaryStore {
 public:
  enum class Error {
    kOk,
    kFailedToInitializeDatabase,
    kInvalidSql,
    kFailedToExecuteSql,
    kFailedToBeginTransaction,
    kFailedToCommitTransaction,
    kInvalidTotalDictSize,
    kFailedToGetTotalDictSize,
    kFailedToSetTotalDictSize,
  };
  struct RegisterDictionaryResult {
    absl::optional<int64_t> primary_key_in_database;
    absl::optional<base::UnguessableToken> disk_cache_key_token_to_be_removed;
    absl::optional<uint64_t> total_dictionary_size;
  };

  using RegisterDictionaryResultOrError =
      base::expected<RegisterDictionaryResult, Error>;
  using DictionaryListOrError =
      base::expected<std::vector<SharedDictionaryInfo>, Error>;
  using DictionaryMapOrError =
      base::expected<std::map<SharedDictionaryStorageIsolationKey,
                              std::vector<SharedDictionaryInfo>>,
                     Error>;

  SQLitePersistentSharedDictionaryStore(
      const base::FilePath& path,
      const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner);

  SQLitePersistentSharedDictionaryStore(
      const SQLitePersistentSharedDictionaryStore&) = delete;
  SQLitePersistentSharedDictionaryStore& operator=(
      const SQLitePersistentSharedDictionaryStore&) = delete;

  ~SQLitePersistentSharedDictionaryStore();

  void GetTotalDictionarySize(
      base::OnceCallback<void(base::expected<uint64_t, Error>)> callback);
  void RegisterDictionary(
      const SharedDictionaryStorageIsolationKey& isolation_key,
      SharedDictionaryInfo dictionary_info,
      base::OnceCallback<void(RegisterDictionaryResultOrError)> callback);
  void GetDictionaries(
      const SharedDictionaryStorageIsolationKey& isolation_key,
      base::OnceCallback<void(DictionaryListOrError)> callback);
  void GetAllDictionaries(
      base::OnceCallback<void(DictionaryMapOrError)> callback);
  void ClearAllDictionaries(base::OnceCallback<void(Error)> callback);

  // TODO(crbug.com/1413922): Add a method to update `last_used_time`.
  // TODO(crbug.com/1413922): Add a method for the garbage collection logic of
  // SharedDictionaryDiskCache by using `disk_cache_key_token`.
  // TODO(crbug.com/1413922): Add a method for the clearing expired dictionary
  // logic using expiration time.
  // TODO(crbug.com/1413922): Add a method for the clearing dictionary logic
  // which will be called from BrowsingDataRemover.

  base::WeakPtr<SQLitePersistentSharedDictionaryStore> GetWeakPtr();

 private:
  class Backend;

  const scoped_refptr<Backend> backend_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SQLitePersistentSharedDictionaryStore> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace net

#endif  // NET_EXTRAS_SQLITE_SQLITE_PERSISTENT_REPORTING_AND_NEL_STORE_H_
