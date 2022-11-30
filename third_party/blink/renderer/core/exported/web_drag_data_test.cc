// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_drag_data.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

TEST(WebDragDataTest, items) {
  DataObject* data_object = DataObject::Create();

  // Native file.
  data_object->Add(MakeGarbageCollected<File>("/native/path"));
  // Blob file.
  data_object->Add(MakeGarbageCollected<File>("name", base::Time::UnixEpoch(),
                                              BlobDataHandle::Create()));

  // User visible snapshot file.
  {
    FileMetadata metadata;
    metadata.platform_path = "/native/visible/snapshot";
    data_object->Add(
        File::CreateForFileSystemFile("name", metadata, File::kIsUserVisible));
  }

  // Not user visible snapshot file.
  {
    FileMetadata metadata;
    metadata.platform_path = "/native/not-visible/snapshot";
    data_object->Add(File::CreateForFileSystemFile("name", metadata,
                                                   File::kIsNotUserVisible));
  }

  // User visible file system URL file.
  {
    FileMetadata metadata;
    metadata.length = 1234;
    KURL url(
        "filesystem:http://example.com/isolated/hash/visible-non-native-file");
    data_object->Add(File::CreateForFileSystemFile(
        url, metadata, File::kIsUserVisible, BlobDataHandle::Create()));
  }

  // Not user visible file system URL file.
  {
    FileMetadata metadata;
    metadata.length = 1234;
    KURL url(
        "filesystem:http://example.com/isolated/hash/"
        "not-visible-non-native-file");
    data_object->Add(File::CreateForFileSystemFile(
        url, metadata, File::kIsNotUserVisible, BlobDataHandle::Create()));
  }

  WebDragData data = data_object->ToWebDragData();
  WebVector<WebDragData::Item> items = data.Items();
  ASSERT_EQ(6u, items.size());

  EXPECT_EQ(WebDragData::Item::kStorageTypeFilename, items[0].storage_type);
  EXPECT_EQ("/native/path", items[0].filename_data);
  EXPECT_EQ("path", items[0].display_name_data);

  EXPECT_EQ(WebDragData::Item::kStorageTypeString, items[1].storage_type);
  EXPECT_EQ("text/plain", items[1].string_type);
  EXPECT_EQ("name", items[1].string_data);

  EXPECT_EQ(WebDragData::Item::kStorageTypeFilename, items[2].storage_type);
  EXPECT_EQ("/native/visible/snapshot", items[2].filename_data);
  EXPECT_EQ("name", items[2].display_name_data);

  EXPECT_EQ(WebDragData::Item::kStorageTypeFilename, items[3].storage_type);
  EXPECT_EQ("/native/not-visible/snapshot", items[3].filename_data);
  EXPECT_EQ("name", items[3].display_name_data);

  EXPECT_EQ(WebDragData::Item::kStorageTypeFileSystemFile,
            items[4].storage_type);
  EXPECT_EQ(
      "filesystem:http://example.com/isolated/hash/visible-non-native-file",
      items[4].file_system_url);
  EXPECT_EQ(1234, items[4].file_system_file_size);

  EXPECT_EQ(WebDragData::Item::kStorageTypeFileSystemFile,
            items[5].storage_type);
  EXPECT_EQ(
      "filesystem:http://example.com/isolated/hash/not-visible-non-native-file",
      items[5].file_system_url);
  EXPECT_EQ(1234, items[5].file_system_file_size);
}

}  // namespace blink
