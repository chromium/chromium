// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_MOCK_MOCK_BACKEND_IMPL_H_
#define NET_DISK_CACHE_MOCK_MOCK_BACKEND_IMPL_H_

#include "net/disk_cache/disk_cache.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace disk_cache {

class BackendMock : public Backend {
 public:
  explicit BackendMock(net::CacheType cache_type);
  ~BackendMock() override;

  MOCK_METHOD(int32_t, GetEntryCount, (), (const, override));
  MOCK_METHOD(EntryResult,
              OpenOrCreateEntry,
              (const std::string& key,
               net::RequestPriority priority,
               EntryResultCallback callback),
              (override));
  MOCK_METHOD(EntryResult,
              OpenEntry,
              (const std::string& key,
               net::RequestPriority priority,
               EntryResultCallback),
              (override));
  MOCK_METHOD(EntryResult,
              CreateEntry,
              (const std::string& key,
               net::RequestPriority priority,
               EntryResultCallback callback),
              (override));
  MOCK_METHOD(net::Error,
              DoomEntry,
              (const std::string& key,
               net::RequestPriority priority,
               CompletionOnceCallback callback),
              (override));
  MOCK_METHOD(net::Error,
              DoomAllEntries,
              (CompletionOnceCallback callback),
              (override));
  MOCK_METHOD(net::Error,
              DoomEntriesBetween,
              (base::Time initial_time,
               base::Time end_time,
               CompletionOnceCallback callback),
              (override));
  MOCK_METHOD(net::Error,
              DoomEntriesSince,
              (base::Time initial_time, CompletionOnceCallback callback),
              (override));
  MOCK_METHOD(int64_t,
              CalculateSizeOfAllEntries,
              (Int64CompletionOnceCallback callback),
              (override));
  MOCK_METHOD(int64_t,
              CalculateSizeOfEntriesBetween,
              (base::Time initial_time,
               base::Time end_time,
               Int64CompletionOnceCallback callback),
              (override));
  MOCK_METHOD(std::unique_ptr<Iterator>, CreateIterator, (), (override));
  MOCK_METHOD(void, GetStats, (base::StringPairs * stats), (override));
  MOCK_METHOD(void, OnExternalCacheHit, (const std::string& key), (override));
  MOCK_METHOD(uint8_t,
              GetEntryInMemoryData,
              (const std::string& key),
              (override));
  MOCK_METHOD(void,
              SetEntryInMemoryData,
              (const std::string& key, uint8_t data),
              (override));
  MOCK_METHOD(int64_t, MaxFileSize, (), (const, override));
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_MOCK_MOCK_BACKEND_IMPL_H_
