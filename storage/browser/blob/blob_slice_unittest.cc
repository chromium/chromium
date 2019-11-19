// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_storage_context.h"

#include <memory>

#include "base/files/file_path.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/blob_entry.h"
#include "storage/browser/blob/shareable_blob_data_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {
namespace {
const char kType[] = "type";
const char kDisposition[] = "";
const char kId[] = "uuid";
}  // namespace

// Historically BlobSlice was a separate class. All that functionality
// was merged into BlobDataBuilder though, so now this test just tests that
// subset of the BlobDataBuilder functionality.
class BlobSliceTest : public testing::Test {
 protected:
  BlobSliceTest() = default;
  ~BlobSliceTest() override = default;

  scoped_refptr<ShareableBlobDataItem> CreateDataItem(size_t size) {
    std::vector<uint8_t> bytes(size);
    for (size_t i = 0; i < size; ++i)
      bytes[i] = i;
    return scoped_refptr<ShareableBlobDataItem>(new ShareableBlobDataItem(
        BlobDataItem::CreateBytes(bytes), ShareableBlobDataItem::QUOTA_NEEDED));
  }

  scoped_refptr<ShareableBlobDataItem> CreateFileItem(size_t offset,
                                                      size_t size) {
    return scoped_refptr<ShareableBlobDataItem>(new ShareableBlobDataItem(
        BlobDataItem::CreateFile(base::FilePath(FILE_PATH_LITERAL("kFakePath")),
                                 offset, size, base::Time::Max()),
        ShareableBlobDataItem::POPULATED_WITHOUT_QUOTA));
  }

  scoped_refptr<ShareableBlobDataItem> CreateTempFileItem(size_t offset,
                                                          size_t size) {
    return scoped_refptr<ShareableBlobDataItem>(new ShareableBlobDataItem(
        BlobDataItem::CreateFutureFile(offset, size, 0),
        ShareableBlobDataItem::QUOTA_NEEDED));
  }

  void Slice(BlobDataBuilder& builder,
             BlobEntry* source,
             uint64_t slice_offset,
             uint64_t slice_size) {
    builder.SliceBlob(source, slice_offset, slice_size);
  }

  void ExpectSlice(scoped_refptr<ShareableBlobDataItem> dest_item,
                   const BlobEntry::ItemCopyEntry& copy,
                   scoped_refptr<ShareableBlobDataItem> source_item,
                   size_t slice_offset,
                   size_t size) {
    EXPECT_EQ(source_item, copy.source_item);
    EXPECT_EQ(slice_offset, copy.source_item_offset);
    EXPECT_EQ(dest_item, copy.dest_item);
    EXPECT_EQ(size, dest_item->item()->length());
    EXPECT_EQ(ShareableBlobDataItem::QUOTA_NEEDED, dest_item->state());
    EXPECT_EQ(BlobDataItem::Type::kBytesDescription, dest_item->item()->type());
  }
};

TEST_F(BlobSliceTest, FullItem) {
  const size_t kSize = 5u;

  BlobEntry data(kType, kDisposition);
  scoped_refptr<ShareableBlobDataItem> item = CreateDataItem(kSize);
  data.AppendSharedBlobItem(item);

  BlobDataBuilder builder(kId);
  Slice(builder, &data, 0, 5);
  EXPECT_TRUE(builder.copies().empty());
  ASSERT_EQ(1u, builder.items().size());
  EXPECT_EQ(item, builder.items()[0]);
}

TEST_F(BlobSliceTest, SliceSingleItem) {
  const size_t kSize = 5u;

  BlobEntry data(kType, kDisposition);
  scoped_refptr<ShareableBlobDataItem> item = CreateDataItem(kSize);
  data.AppendSharedBlobItem(item);

  BlobDataBuilder builder(kId);
  Slice(builder, &data, 1, 3);
  ASSERT_EQ(1u, builder.copies().size());
  ASSERT_EQ(1u, builder.items().size());

  ExpectSlice(builder.items()[0], builder.copies()[0], item, 1, 3);
}

TEST_F(BlobSliceTest, SliceSingleLastItem) {
  const size_t kSize1 = 5u;
  const size_t kSize2 = 10u;

  BlobEntry data(kType, kDisposition);
  scoped_refptr<ShareableBlobDataItem> item1 = CreateDataItem(kSize1);
  scoped_refptr<ShareableBlobDataItem> item2 = CreateDataItem(kSize2);
  data.AppendSharedBlobItem(item1);
  data.AppendSharedBlobItem(item2);

  BlobDataBuilder builder(kId);
  Slice(builder, &data, 6, 2);
  ASSERT_EQ(1u, builder.copies().size());
  ASSERT_EQ(1u, builder.items().size());

  ExpectSlice(builder.items()[0], builder.copies()[0], item2, 1, 2);
}

TEST_F(BlobSliceTest, SliceAcrossTwoItems) {
  const size_t kSize1 = 5u;
  const size_t kSize2 = 10u;

  BlobEntry data(kType, kDisposition);
  scoped_refptr<ShareableBlobDataItem> item1 = CreateDataItem(kSize1);
  scoped_refptr<ShareableBlobDataItem> item2 = CreateDataItem(kSize2);
  data.AppendSharedBlobItem(item1);
  data.AppendSharedBlobItem(item2);

  BlobDataBuilder builder(kId);
  Slice(builder, &data, 4, 10);
  ASSERT_EQ(2u, builder.copies().size());
  ASSERT_EQ(2u, builder.items().size());

  ExpectSlice(builder.items()[0], builder.copies()[0], item1, 4, 1);
  ExpectSlice(builder.items()[1], builder.copies()[1], item2, 0, 9);
}

TEST_F(BlobSliceTest, SliceFileAndLastItem) {
  const size_t kSize1 = 5u;
  const size_t kSize2 = 10u;

  BlobEntry data(kType, kDisposition);
  scoped_refptr<ShareableBlobDataItem> item1 = CreateFileItem(0u, kSize1);
  scoped_refptr<ShareableBlobDataItem> item2 = CreateDataItem(kSize2);
  data.AppendSharedBlobItem(item1);
  data.AppendSharedBlobItem(item2);

  BlobDataBuilder builder(kId);
  Slice(builder, &data, 4, 2);
  ASSERT_EQ(1u, builder.copies().size());
  ASSERT_EQ(2u, builder.items().size());

  EXPECT_EQ(*CreateFileItem(4u, 1u)->item(), *builder.items()[0]->item());
  ExpectSlice(builder.items()[1], builder.copies()[0], item2, 0, 1);
}

TEST_F(BlobSliceTest, SliceAcrossLargeItem) {
  const size_t kSize1 = 5u;
  const size_t kSize2 = 10u;
  const size_t kSize3 = 10u;

  BlobEntry data(kType, kDisposition);
  scoped_refptr<ShareableBlobDataItem> item1 = CreateDataItem(kSize1);
  scoped_refptr<ShareableBlobDataItem> item2 = CreateFileItem(0u, kSize2);
  scoped_refptr<ShareableBlobDataItem> item3 = CreateDataItem(kSize3);
  data.AppendSharedBlobItem(item1);
  data.AppendSharedBlobItem(item2);
  data.AppendSharedBlobItem(item3);

  BlobDataBuilder builder(kId);
  Slice(builder, &data, 2, 20);
  ASSERT_EQ(2u, builder.copies().size());
  ASSERT_EQ(3u, builder.items().size());

  ExpectSlice(builder.items()[0], builder.copies()[0], item1, 2, 3);
  EXPECT_EQ(item2, builder.items()[1]);
  ExpectSlice(builder.items()[2], builder.copies()[1], item3, 0, 7);
}

TEST_F(BlobSliceTest, SliceTempFileItem) {
  BlobEntry data(kType, kDisposition);
  scoped_refptr<ShareableBlobDataItem> item1 = CreateTempFileItem(1u, 10u);
  data.AppendSharedBlobItem(item1);

  BlobDataBuilder builder(kId);
  Slice(builder, &data, 2, 5);
  ASSERT_EQ(1u, builder.copies().size());
  ASSERT_EQ(1u, builder.items().size());
  auto dest_item = builder.items()[0];
  EXPECT_EQ(item1, builder.copies()[0].source_item);
  EXPECT_EQ(2u, builder.copies()[0].source_item_offset);
  EXPECT_EQ(dest_item, builder.copies()[0].dest_item);
  EXPECT_EQ(5u, dest_item->item()->length());
  EXPECT_EQ(ShareableBlobDataItem::POPULATED_WITHOUT_QUOTA, dest_item->state());
  EXPECT_EQ(BlobDataItem::Type::kFile, dest_item->item()->type());
}

}  // namespace storage
