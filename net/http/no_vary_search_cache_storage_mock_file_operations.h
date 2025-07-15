// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_NO_VARY_SEARCH_CACHE_STORAGE_MOCK_FILE_OPERATIONS_H_
#define NET_HTTP_NO_VARY_SEARCH_CACHE_STORAGE_MOCK_FILE_OPERATIONS_H_

#include "net/http/no_vary_search_cache_storage_file_operations.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {

// Mock implementation of NoVarySearchCacheStorageFileOperations.
class MockFileOperations : public NoVarySearchCacheStorageFileOperations {
 public:
  MockFileOperations();
  ~MockFileOperations() override;

  MOCK_METHOD(bool, Init, (), (override));
  MOCK_METHOD((base::expected<LoadResult, base::File::Error>),
              Load,
              (std::string_view filename, size_t max_size),
              (override));
  MOCK_METHOD((base::expected<void, base::File::Error>),
              AtomicSave,
              (std::string_view filename,
               base::span<const base::span<const uint8_t>> segments),
              (override));
  MOCK_METHOD((base::expected<std::unique_ptr<Writer>, base::File::Error>),
              CreateWriter,
              (std::string_view filename),
              (override));
};

// Mock implementation of NoVarySearchCacheStorageFileOperations::Writer. This
// can be returned from CreateWriter() after setting expectations, like this:
//
//  auto mock_writer = std::make_unique<StrictMock<MockWriter>>();
//  EXPECT_CALL(*mock_writer, Write).WillOnce(Return(true));
//  EXPECT_CALL(file_operations, CreateWriter)
//    .WillOnce(Return(std::move(mock_writer)));
//
class MockWriter : public NoVarySearchCacheStorageFileOperations::Writer {
 public:
  MockWriter();
  ~MockWriter() override;

  MOCK_METHOD(bool, Write, (base::span<const uint8_t> data), (override));
};

}  // namespace net

#endif  // NET_HTTP_NO_VARY_SEARCH_CACHE_STORAGE_MOCK_FILE_OPERATIONS_H_
