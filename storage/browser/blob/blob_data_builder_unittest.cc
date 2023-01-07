// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_data_builder.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

TEST(BlobDataBuilderTest, TestFutureFiles) {
  const std::string kId = "id";
  const uint64_t kFileId = 13;

  auto item = BlobDataItem::CreateFutureFile(0, 10, kFileId);
  EXPECT_TRUE(item->IsFutureFileItem());
  EXPECT_EQ(kFileId, item->GetFutureFileID());

  BlobDataBuilder builder(kId);
  builder.AppendFutureFile(0, 10, kFileId);
  EXPECT_TRUE(builder.items()[0]->item()->IsFutureFileItem());
  EXPECT_EQ(kFileId, builder.items()[0]->item()->GetFutureFileID());
}

}  // namespace storage
