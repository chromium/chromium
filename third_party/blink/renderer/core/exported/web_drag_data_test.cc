// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_drag_data.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(WebDragDataTest, items) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext context;
  DataObject* data_object = DataObject::Create();

  // Native file.
  data_object->Add(MakeGarbageCollected<File>(&context.GetExecutionContext(),
                                              "/native/path"));
  // Blob file.
  data_object->Add(MakeGarbageCollected<File>("name", base::Time::UnixEpoch(),
                                              BlobDataHandle::Create()));

  // User visible snapshot file.
  {
    FileMetadata metadata;
    metadata.platform_path = "/native/visible/snapshot";
    data_object->Add(
        File::CreateForFileSystemFile(&context.GetExecutionContext(), "name",
                                      metadata, File::kIsUserVisible));
  }

  // Not user visible snapshot file.
  {
    FileMetadata metadata;
    metadata.platform_path = "/native/not-visible/snapshot";
    data_object->Add(
        File::CreateForFileSystemFile(&context.GetExecutionContext(), "name",
                                      metadata, File::kIsNotUserVisible));
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

  {
    const auto* item = absl::get_if<WebDragData::FilenameItem>(&items[0]);
    ASSERT_TRUE(item != nullptr);
    EXPECT_EQ("/native/path", item->filename);
    EXPECT_EQ("path", item->display_name);
  }

  {
    const auto* item = absl::get_if<WebDragData::StringItem>(&items[1]);
    ASSERT_TRUE(item != nullptr);
    EXPECT_EQ("text/plain", item->type);
    EXPECT_EQ("name", item->data);
  }

  {
    const auto* item = absl::get_if<WebDragData::FilenameItem>(&items[2]);
    ASSERT_TRUE(item != nullptr);
    EXPECT_EQ("/native/visible/snapshot", item->filename);
    EXPECT_EQ("name", item->display_name);
  }

  {
    const auto* item = absl::get_if<WebDragData::FilenameItem>(&items[3]);
    ASSERT_TRUE(item != nullptr);
    EXPECT_EQ("/native/not-visible/snapshot", item->filename);
    EXPECT_EQ("name", item->display_name);
  }

  {
    const auto* item = absl::get_if<WebDragData::FileSystemFileItem>(&items[4]);
    ASSERT_TRUE(item != nullptr);
    EXPECT_EQ(
        "filesystem:http://example.com/isolated/hash/visible-non-native-file",
        item->url);
    EXPECT_EQ(1234, item->size);
  }

  {
    const auto* item = absl::get_if<WebDragData::FileSystemFileItem>(&items[5]);
    ASSERT_TRUE(item != nullptr);
    EXPECT_EQ(
        "filesystem:http://example.com/isolated/hash/"
        "not-visible-non-native-file",
        item->url);
    EXPECT_EQ(1234, item->size);
  }
}

}  // namespace blink
