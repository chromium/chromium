// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_EXTRAS_SQLITE_SQLITE_PERSISTENT_SHARED_DICTIONARY_STORE_H_
#define NET_EXTRAS_SQLITE_SQLITE_PERSISTENT_SHARED_DICTIONARY_STORE_H_

#include <map>
#include <set>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "net/extras/shared_dictionary/shared_dictionary_info.h"
#include "net/extras/shared_dictionary/shared_dictionary_usage_info.h"
#include "url/origin.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace net {

class SharedDictionaryIsolationKey;

// This class is used for storing SharedDictionary information to the persistent
// storage.
class COMPONENT_EXPORT(NET_EXTRAS) SQLitePersistentSharedDictionaryStore {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Error {
    kOk = 0,
    kFailedToInitializeDatabase = 1,
    kInvalidSql = 2,
    kFailedToExecuteSql = 3,
    kFailedToBeginTransaction = 4,
    kFailedToCommitTransaction = 5,
    kInvalidTotalDictSize = 6,
    kFailedToGetTotalDictSize = 7,
    kFailedToSetTotalDictSize = 8,
    kTooBigDictionary = 9,
    kMaxValue = kTooBigDictionary
  };
  class COMPONENT_EXPORT(NET_EXTRAS) RegisterDictionaryResult {
   public:
    RegisterDictionaryResult(
        int64_t primary_key_in_database,
        std::optional<base::UnguessableToken> replaced_disk_cache_key_token,
        std::set<base::UnguessableToken> evicted_disk_cache_key_tokens,
        uint64_t total_dictionary_size,
        uint64_t total_dictionary_count);
    ~RegisterDictionaryResult();

    RegisterDictionaryResult(const RegisterDictionaryResult& other);
    RegisterDictionaryResult(RegisterDictionaryResult&& other);
    RegisterDictionaryResult& operator=(const RegisterDictionaryResult& other);
    RegisterDictionaryResult& operator=(RegisterDictionaryResult&& other);

    int64_t primary_key_in_database() const { return primary_key_in_database_; }
    const std::optional<base::UnguessableToken>& replaced_disk_cache_key_token()
        const {
      return replaced_disk_cache_key_token_;
    }
    const std::set<base::UnguessableToken>& evicted_disk_cache_key_tokens()
        const {
      return evicted_disk_cache_key_tokens_;
    }
    uint64_t total_dictionary_size() const { return total_dictionary_size_; }
    uint64_t total_dictionary_count() const { return total_dictionary_count_; }

   private:
    int64_t primary_key_in_database_;
    std::optional<base::UnguessableToken> replaced_disk_cache_key_token_;
    std::set<base::UnguessableToken> evicted_disk_cache_key_tokens_;
    uint64_t total_dictionary_size_;
    uint64_t total_dictionary_count_;
  };

  using SizeOrError = base::expected<uint64_t, Error>;
  using RegisterDictionaryResultOrError =
      base::expected<RegisterDictionaryResult, Error>;
  using DictionaryListOrError =
      base::expected<std::vector<SharedDictionaryInfo>, Error>;
  using DictionaryMapOrError = base::expected<
      std::map<SharedDictionaryIsolationKey, std::vector<SharedDictionaryInfo>>,
      Error>;
  using UnguessableTokenSetOrError =
      base::expected<std::set<base::UnguessableToken>, Error>;
  using UsageInfoOrError =
      base::expected<std::vector<SharedDictionaryUsageInfo>, Error>;
  using OriginListOrError = base::expected<std::vector<url::Origin>, Error>;

  SQLitePersistentSharedDictionaryStore(
      const base::FilePath& path,
      const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner);

  SQLitePersistentSharedDictionaryStore(
      const SQLitePersistentSharedDictionaryStore&) = delete;
  SQLitePersistentSharedDictionaryStore& operator=(
      const SQLitePersistentSharedDictionaryStore&) = delete;

  ~SQLitePersistentSharedDictionaryStore();

  void GetTotalDictionarySize(base::OnceCallback<void(SizeOrError)> callback);
  void RegisterDictionary(
      const SharedDictionaryIsolationKey& isolation_key,
      SharedDictionaryInfo dictionary_info,
      const uint64_t max_size_per_site,
      const uint64_t max_count_per_site,
      base::OnceCallback<void(RegisterDictionaryResultOrError)> callback);
  void GetDictionaries(
      const SharedDictionaryIsolationKey& isolation_key,
      base::OnceCallback<void(DictionaryListOrError)> callback);
  void GetAllDictionaries(
      base::OnceCallback<void(DictionaryMapOrError)> callback);
  void GetUsageInfo(base::OnceCallback<void(UsageInfoOrError)> callback);
  void GetOriginsBetween(const base::Time start_time,
                         const base::Time end_time,
                         base::OnceCallback<void(OriginListOrError)> callback);
  void ClearAllDictionaries(
      base::OnceCallback<void(UnguessableTokenSetOrError)> callback);
  void ClearDictionaries(
      const base::Time start_time,
      const base::Time end_time,
      base::RepeatingCallback<bool(const GURL&)> url_matcher,
      base::OnceCallback<void(UnguessableTokenSetOrError)> callback);
  void ClearDictionariesForIsolationKey(
      const SharedDictionaryIsolationKey& isolation_key,
      base::OnceCallback<void(UnguessableTokenSetOrError)> callback);
  void DeleteExpiredDictionaries(
      const base::Time now,
      base::OnceCallback<void(UnguessableTokenSetOrError)> callback);
  // Deletes dictionaries in order of `last_used_time` if the total size of all
  // dictionaries exceeds `cache_max_size` or the total dictionary count exceeds
  // `cache_max_count` until the total size reaches `size_low_watermark` and the
  // total count reaches `count_low_watermark`. If `cache_max_size` is zero, the
  // size limitation is ignored.
  void ProcessEviction(
      const uint64_t cache_max_size,
      const uint64_t size_low_watermark,
      const uint64_t cache_max_count,
      const uint64_t count_low_watermark,
      base::OnceCallback<void(UnguessableTokenSetOrError)> callback);
  void GetAllDiskCacheKeyTokens(
      base::OnceCallback<void(UnguessableTokenSetOrError)> callback);
  void DeleteDictionariesByDiskCacheKeyTokens(
      std::set<base::UnguessableToken> disk_cache_key_tokens,
      base::OnceCallback<void(Error)> callback);
  void UpdateDictionaryLastFetchTime(const int64_t primary_key_in_database,
                                     const base::Time last_fetch_time,
                                     base::OnceCallback<void(Error)> callback);
  void UpdateDictionaryLastUsedTime(int64_t primary_key_in_database,
                                    base::Time last_used_time);

  base::WeakPtr<SQLitePersistentSharedDictionaryStore> GetWeakPtr();

 private:
  class Backend;

  const scoped_refptr<Backend> backend_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SQLitePersistentSharedDictionaryStore> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace net

#endif  // NET_EXTRAS_SQLITE_SQLITE_PERSISTENT_SHARED_DICTIONARY_STORE_H_
