// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fileapi/file.h"

#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/file/file_utilities.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/blob/testing/fake_blob.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

class MockBlob : public FakeBlob {
 public:
  static void Create(File* file, base::Time modified_time) {
    mojo::PendingRemote<mojom::blink::Blob> remote;
    PostCrossThreadTask(
        *base::ThreadPool::CreateSingleThreadTaskRunner({}), FROM_HERE,
        CrossThreadBindOnce(
            [](const String& uuid,
               mojo::PendingReceiver<mojom::blink::Blob> receiver,
               base::Time modified_time) {
              mojo::MakeSelfOwnedReceiver(
                  std::make_unique<MockBlob>(uuid, modified_time),
                  std::move(receiver));
            },
            file->Uuid(), remote.InitWithNewPipeAndPassReceiver(),
            modified_time));
    file->GetBlobDataHandle()->SetBlobRemoteForTesting(std::move(remote));
  }

  MockBlob(const String& uuid, base::Time modified_time)
      : FakeBlob(uuid), modified_time_(modified_time) {}

  void Clone(mojo::PendingReceiver<mojom::blink::Blob> receiver) override {
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<MockBlob>(uuid_, modified_time_), std::move(receiver));
  }

  void CaptureSnapshot(CaptureSnapshotCallback callback) override {
    std::move(callback).Run(
        /*size=*/0, NullableTimeToOptionalTime(modified_time_));
  }

 private:
  base::Time modified_time_;
};

void ExpectTimestampIsNow(const File& file) {
  const base::Time now = base::Time::Now();
  const base::TimeDelta delta_now = now - base::Time::UnixEpoch();
  // Because lastModified() applies floor() internally, we should compare
  // integral millisecond values.
  EXPECT_GE(file.lastModified(), delta_now.InMilliseconds());
  EXPECT_GE(file.LastModifiedTime(), now);
}

}  // namespace

TEST(FileTest, NativeFileWithoutTimestamp) {
  auto* const file = MakeGarbageCollected<File>("/native/path");
  MockBlob::Create(file, base::Time());

  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ("/native/path", file->GetPath());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  ExpectTimestampIsNow(*file);
}

TEST(FileTest, NativeFileWithUnixEpochTimestamp) {
  auto* const file = MakeGarbageCollected<File>("/native/path");
  MockBlob::Create(file, base::Time::UnixEpoch());

  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ(0, file->lastModified());
  EXPECT_EQ(base::Time::UnixEpoch(), file->LastModifiedTime());
}

TEST(FileTest, NativeFileWithApocalypseTimestamp) {
  auto* const file = MakeGarbageCollected<File>("/native/path");
  MockBlob::Create(file, base::Time::Max());

  EXPECT_TRUE(file->HasBackingFile());

  EXPECT_EQ((base::Time::Max() - base::Time::UnixEpoch()).InMilliseconds(),
            file->lastModified());
  EXPECT_EQ(base::Time::Max(), file->LastModifiedTime());
}

TEST(FileTest, BlobBackingFileWithoutTimestamp) {
  auto* const file = MakeGarbageCollected<File>("name", absl::nullopt,
                                                BlobDataHandle::Create());
  EXPECT_FALSE(file->HasBackingFile());
  EXPECT_TRUE(file->GetPath().IsEmpty());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  ExpectTimestampIsNow(*file);
}

TEST(FileTest, BlobBackingFileWithWindowsEpochTimestamp) {
  auto* const file = MakeGarbageCollected<File>("name", base::Time(),
                                                BlobDataHandle::Create());
  EXPECT_FALSE(file->HasBackingFile());
  EXPECT_TRUE(file->GetPath().IsEmpty());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  EXPECT_EQ((base::Time() - base::Time::UnixEpoch()).InMilliseconds(),
            file->lastModified());
  EXPECT_EQ(base::Time(), file->LastModifiedTime());
}

TEST(FileTest, BlobBackingFileWithUnixEpochTimestamp) {
  const scoped_refptr<BlobDataHandle> blob_data_handle =
      BlobDataHandle::Create();
  auto* const file = MakeGarbageCollected<File>("name", base::Time::UnixEpoch(),
                                                blob_data_handle);
  EXPECT_FALSE(file->HasBackingFile());
  EXPECT_TRUE(file->GetPath().IsEmpty());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  EXPECT_EQ(INT64_C(0), file->lastModified());
  EXPECT_EQ(base::Time::UnixEpoch(), file->LastModifiedTime());
}

TEST(FileTest, BlobBackingFileWithApocalypseTimestamp) {
  constexpr base::Time kMaxTime = base::Time::Max();
  auto* const file =
      MakeGarbageCollected<File>("name", kMaxTime, BlobDataHandle::Create());
  EXPECT_FALSE(file->HasBackingFile());
  EXPECT_TRUE(file->GetPath().IsEmpty());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  EXPECT_EQ((kMaxTime - base::Time::UnixEpoch()).InMilliseconds(),
            file->lastModified());
  EXPECT_EQ(kMaxTime, file->LastModifiedTime());
}

TEST(FileTest, fileSystemFileWithNativeSnapshot) {
  FileMetadata metadata;
  metadata.platform_path = "/native/snapshot";
  File* const file =
      File::CreateForFileSystemFile("name", metadata, File::kIsUserVisible);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ("/native/snapshot", file->GetPath());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
}

TEST(FileTest, fileSystemFileWithNativeSnapshotAndSize) {
  FileMetadata metadata;
  metadata.length = 1024ll;
  metadata.platform_path = "/native/snapshot";
  File* const file =
      File::CreateForFileSystemFile("name", metadata, File::kIsUserVisible);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ("/native/snapshot", file->GetPath());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
}

TEST(FileTest, FileSystemFileWithWindowsEpochTimestamp) {
  FileMetadata metadata;
  metadata.length = INT64_C(1025);
  metadata.modification_time = base::Time();
  metadata.platform_path = "/native/snapshot";
  File* const file =
      File::CreateForFileSystemFile("name", metadata, File::kIsUserVisible);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ("/native/snapshot", file->GetPath());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  EXPECT_EQ(UINT64_C(1025), file->size());
  EXPECT_EQ((base::Time() - base::Time::UnixEpoch()).InMilliseconds(),
            file->lastModified());
  EXPECT_EQ(base::Time(), file->LastModifiedTime());
}

TEST(FileTest, FileSystemFileWithUnixEpochTimestamp) {
  FileMetadata metadata;
  metadata.length = INT64_C(1025);
  metadata.modification_time = base::Time::UnixEpoch();
  metadata.platform_path = "/native/snapshot";
  File* const file =
      File::CreateForFileSystemFile("name", metadata, File::kIsUserVisible);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ("/native/snapshot", file->GetPath());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  EXPECT_EQ(UINT64_C(1025), file->size());
  EXPECT_EQ(INT64_C(0), file->lastModified());
  EXPECT_EQ(base::Time::UnixEpoch(), file->LastModifiedTime());
}

TEST(FileTest, FileSystemFileWithApocalypseTimestamp) {
  constexpr base::Time kMaxTime = base::Time::Max();
  FileMetadata metadata;
  metadata.length = INT64_C(1025);
  metadata.modification_time = kMaxTime;
  metadata.platform_path = "/native/snapshot";
  File* const file =
      File::CreateForFileSystemFile("name", metadata, File::kIsUserVisible);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ("/native/snapshot", file->GetPath());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  EXPECT_EQ(UINT64_C(1025), file->size());
  EXPECT_EQ((kMaxTime - base::Time::UnixEpoch()).InMilliseconds(),
            file->lastModified());
  EXPECT_EQ(kMaxTime, file->LastModifiedTime());
}

TEST(FileTest, fileSystemFileWithoutNativeSnapshot) {
  KURL url("filesystem:http://example.com/isolated/hash/non-native-file");
  FileMetadata metadata;
  metadata.length = 0;
  File* const file =
      File::CreateForFileSystemFile(url, metadata, File::kIsUserVisible);
  EXPECT_FALSE(file->HasBackingFile());
  EXPECT_TRUE(file->GetPath().IsEmpty());
  EXPECT_EQ(url, file->FileSystemURL());
}

TEST(FileTest, hsaSameSource) {
  auto* const native_file_a1 = MakeGarbageCollected<File>("/native/pathA");
  auto* const native_file_a2 = MakeGarbageCollected<File>("/native/pathA");
  auto* const native_file_b = MakeGarbageCollected<File>("/native/pathB");

  const scoped_refptr<BlobDataHandle> blob_data_a = BlobDataHandle::Create();
  const scoped_refptr<BlobDataHandle> blob_data_b = BlobDataHandle::Create();
  const base::Time kEpoch = base::Time::UnixEpoch();
  auto* const blob_file_a1 =
      MakeGarbageCollected<File>("name", kEpoch, blob_data_a);
  auto* const blob_file_a2 =
      MakeGarbageCollected<File>("name", kEpoch, blob_data_a);
  auto* const blob_file_b =
      MakeGarbageCollected<File>("name", kEpoch, blob_data_b);

  KURL url_a("filesystem:http://example.com/isolated/hash/non-native-file-A");
  KURL url_b("filesystem:http://example.com/isolated/hash/non-native-file-B");
  FileMetadata metadata;
  metadata.length = 0;
  File* const file_system_file_a1 =
      File::CreateForFileSystemFile(url_a, metadata, File::kIsUserVisible);
  File* const file_system_file_a2 =
      File::CreateForFileSystemFile(url_a, metadata, File::kIsUserVisible);
  File* const file_system_file_b =
      File::CreateForFileSystemFile(url_b, metadata, File::kIsUserVisible);

  EXPECT_FALSE(native_file_a1->HasSameSource(*blob_file_a1));
  EXPECT_FALSE(blob_file_a1->HasSameSource(*file_system_file_a1));
  EXPECT_FALSE(file_system_file_a1->HasSameSource(*native_file_a1));

  EXPECT_TRUE(native_file_a1->HasSameSource(*native_file_a1));
  EXPECT_TRUE(native_file_a1->HasSameSource(*native_file_a2));
  EXPECT_FALSE(native_file_a1->HasSameSource(*native_file_b));

  EXPECT_TRUE(blob_file_a1->HasSameSource(*blob_file_a1));
  EXPECT_TRUE(blob_file_a1->HasSameSource(*blob_file_a2));
  EXPECT_FALSE(blob_file_a1->HasSameSource(*blob_file_b));

  EXPECT_TRUE(file_system_file_a1->HasSameSource(*file_system_file_a1));
  EXPECT_TRUE(file_system_file_a1->HasSameSource(*file_system_file_a2));
  EXPECT_FALSE(file_system_file_a1->HasSameSource(*file_system_file_b));
}

}  // namespace blink
