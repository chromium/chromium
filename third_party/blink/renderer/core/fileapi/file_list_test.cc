// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fileapi/file_list.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/file_metadata.h"

namespace blink {

TEST(FileListTest, pathsForUserVisibleFiles) {
  auto* const file_list = MakeGarbageCollected<FileList>();

  // Native file.
  file_list->Append(MakeGarbageCollected<File>("/native/path"));

  // Blob file.
  const scoped_refptr<BlobDataHandle> blob_data_handle =
      BlobDataHandle::Create();
  file_list->Append(MakeGarbageCollected<File>("name", base::Time::UnixEpoch(),
                                               blob_data_handle));

  // User visible snapshot file.
  {
    FileMetadata metadata;
    metadata.platform_path = "/native/visible/snapshot";
    file_list->Append(
        File::CreateForFileSystemFile("name", metadata, File::kIsUserVisible));
  }

  // Not user visible snapshot file.
  {
    FileMetadata metadata;
    metadata.platform_path = "/native/not-visible/snapshot";
    file_list->Append(File::CreateForFileSystemFile("name", metadata,
                                                    File::kIsNotUserVisible));
  }

  // User visible file system URL file.
  {
    KURL url(
        "filesystem:http://example.com/isolated/hash/visible-non-native-file");
    FileMetadata metadata;
    metadata.length = 0;
    file_list->Append(
        File::CreateForFileSystemFile(url, metadata, File::kIsUserVisible));
  }

  // Not user visible file system URL file.
  {
    KURL url(
        "filesystem:http://example.com/isolated/hash/"
        "not-visible-non-native-file");
    FileMetadata metadata;
    metadata.length = 0;
    file_list->Append(
        File::CreateForFileSystemFile(url, metadata, File::kIsNotUserVisible));
  }

  Vector<base::FilePath> paths = file_list->PathsForUserVisibleFiles();

  ASSERT_EQ(3u, paths.size());
  EXPECT_EQ(FILE_PATH_LITERAL("/native/path"), paths[0].value());
  EXPECT_EQ(FILE_PATH_LITERAL("/native/visible/snapshot"), paths[1].value());
  EXPECT_EQ(FILE_PATH_LITERAL("visible-non-native-file"), paths[2].value())
      << "Files not backed by a native file should return name.";
}

}  // namespace blink
