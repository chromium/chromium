// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_storage_registry.h"

#include "storage/browser/blob/blob_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {
namespace {

TEST(BlobStorageRegistry, UUIDRegistration) {
  const std::string kBlob1 = "Blob1";
  const std::string kType = "type1";
  const std::string kDisposition = "disp1";
  BlobStorageRegistry registry;

  EXPECT_FALSE(registry.DeleteEntry(kBlob1));
  EXPECT_EQ(0u, registry.blob_count());

  BlobEntry* entry = registry.CreateEntry(kBlob1, kType, kDisposition);
  ASSERT_NE(nullptr, entry);
  EXPECT_EQ(BlobStatus::PENDING_QUOTA, entry->status());
  EXPECT_EQ(kType, entry->content_type());
  EXPECT_EQ(kDisposition, entry->content_disposition());
  EXPECT_EQ(0u, entry->refcount());

  EXPECT_EQ(entry, registry.GetEntry(kBlob1));
  EXPECT_TRUE(registry.DeleteEntry(kBlob1));
  entry = registry.CreateEntry(kBlob1, kType, kDisposition);

  EXPECT_EQ(1u, registry.blob_count());
}

}  // namespace
}  // namespace storage
