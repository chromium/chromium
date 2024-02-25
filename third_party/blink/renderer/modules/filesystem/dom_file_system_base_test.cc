// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/filesystem/dom_file_system_base.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom-blink.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class DOMFileSystemBaseTest : public testing::Test {
 public:
  DOMFileSystemBaseTest() {
    file_path_ = test::BlinkRootDir() +
                 "/renderer/modules/filesystem/dom_file_system_base_test.cc";
    GetFileMetadata(file_path_, *context_, file_metadata_);
    file_metadata_.platform_path = file_path_;
  }
  ~DOMFileSystemBaseTest() override { context_->NotifyContextDestroyed(); }

 protected:
  test::TaskEnvironment task_environment_;
  Persistent<ExecutionContext> context_ =
      MakeGarbageCollected<NullExecutionContext>();
  String file_path_;
  FileMetadata file_metadata_;
};

TEST_F(DOMFileSystemBaseTest, externalFilesystemFilesAreUserVisible) {
  KURL root_url = DOMFileSystemBase::CreateFileSystemRootURL(
      "http://chromium.org/", mojom::blink::FileSystemType::kExternal);

  File* file = DOMFileSystemBase::CreateFile(
      context_, file_metadata_, root_url,
      mojom::blink::FileSystemType::kExternal, "dom_file_system_base_test.cc");
  EXPECT_TRUE(file);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ(File::kIsUserVisible, file->GetUserVisibility());
  EXPECT_EQ("dom_file_system_base_test.cc", file->name());
  EXPECT_EQ(file_path_, file->GetPath());
}

TEST_F(DOMFileSystemBaseTest, temporaryFilesystemFilesAreNotUserVisible) {
  KURL root_url = DOMFileSystemBase::CreateFileSystemRootURL(
      "http://chromium.org/", mojom::blink::FileSystemType::kTemporary);

  File* file = DOMFileSystemBase::CreateFile(
      context_, file_metadata_, root_url,
      mojom::blink::FileSystemType::kTemporary, "UserVisibleName.txt");
  EXPECT_TRUE(file);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ(File::kIsNotUserVisible, file->GetUserVisibility());
  EXPECT_EQ("UserVisibleName.txt", file->name());
  EXPECT_EQ(file_path_, file->GetPath());
}

TEST_F(DOMFileSystemBaseTest, persistentFilesystemFilesAreNotUserVisible) {
  KURL root_url = DOMFileSystemBase::CreateFileSystemRootURL(
      "http://chromium.org/", mojom::blink::FileSystemType::kPersistent);

  File* file = DOMFileSystemBase::CreateFile(
      context_, file_metadata_, root_url,
      mojom::blink::FileSystemType::kPersistent, "UserVisibleName.txt");
  EXPECT_TRUE(file);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ(File::kIsNotUserVisible, file->GetUserVisibility());
  EXPECT_EQ("UserVisibleName.txt", file->name());
  EXPECT_EQ(file_path_, file->GetPath());
}

}  // namespace blink
