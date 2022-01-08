// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_MOCK_QUOTA_DATABASE_H_
#define STORAGE_BROWSER_TEST_MOCK_QUOTA_DATABASE_H_

#include "base/files/file_util.h"
#include "storage/browser/quota/quota_database.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace storage {

// Class for mocking results of calls made to QuotaDatabase.
class MockQuotaDatabase : public QuotaDatabase {
 public:
  explicit MockQuotaDatabase(const base::FilePath& path);
  ~MockQuotaDatabase() override;

  MOCK_METHOD1(DeleteBucketInfo, QuotaError(BucketId));
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_MOCK_QUOTA_DATABASE_H_
