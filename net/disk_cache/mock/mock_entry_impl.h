// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_MOCK_MOCK_ENTRY_IMPL_H_
#define NET_DISK_CACHE_MOCK_MOCK_ENTRY_IMPL_H_

#include "net/disk_cache/disk_cache.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace disk_cache {

class EntryMock : public Entry {
 public:
  EntryMock();
  ~EntryMock() override;

  // Manual override of the Close function because the Entry interface expects
  // the Close override to cleanup the class (including deleting itself).
  void Close() override { delete this; }

  MOCK_METHOD(void, Doom, (), (override));
  MOCK_METHOD(std::string, GetKey, (), (const, override));
  MOCK_METHOD(base::Time, GetLastUsed, (), (const, override));
  MOCK_METHOD(base::Time, GetLastModified, (), (const, override));
  MOCK_METHOD(int32_t, GetDataSize, (int index), (const, override));
  MOCK_METHOD(int,
              ReadData,
              (int index,
               int offset,
               IOBuffer* buf,
               int buf_len,
               CompletionOnceCallback callback),
              (override));
  MOCK_METHOD(int,
              WriteData,
              (int index,
               int offset,
               IOBuffer* buf,
               int buf_len,
               CompletionOnceCallback callback,
               bool truncate),
              (override));
  MOCK_METHOD(int,
              ReadSparseData,
              (int64_t offset,
               IOBuffer* buf,
               int buf_len,
               CompletionOnceCallback callback),
              (override));
  MOCK_METHOD(int,
              WriteSparseData,
              (int64_t offset,
               IOBuffer* buf,
               int buf_len,
               CompletionOnceCallback callback),
              (override));
  MOCK_METHOD(RangeResult,
              GetAvailableRange,
              (int64_t offset, int len, RangeResultCallback callback),
              (override));
  MOCK_METHOD(bool, CouldBeSparse, (), (const, override));
  MOCK_METHOD(void, CancelSparseIO, (), (override));
  MOCK_METHOD(net::Error,
              ReadyForSparseIO,
              (CompletionOnceCallback callback),
              (override));
  MOCK_METHOD(void, SetLastUsedTimeForTest, (base::Time time), (override));
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_MOCK_MOCK_ENTRY_IMPL_H_
