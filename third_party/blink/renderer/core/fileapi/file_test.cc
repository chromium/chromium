// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fileapi/file.h"

#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/file/file_utilities.mojom-blink.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/blob/testing/fake_blob.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_mojo.h"
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

using MockRegisterBlobCallback = base::OnceCallback<
    void(const String&, const KURL&, uint64_t, std::optional<base::Time>)>;
class MockFileSystemManager : public mojom::blink::FileSystemManager {
 public:
  explicit MockFileSystemManager(
      const blink::BrowserInterfaceBrokerProxy& broker)
      : broker_(broker) {
    broker.SetBinderForTesting(
        mojom::blink::FileSystemManager::Name_,
        WTF::BindRepeating(&MockFileSystemManager::BindReceiver,
                           WTF::Unretained(this)));
  }

  ~MockFileSystemManager() override {
    broker_.SetBinderForTesting(mojom::blink::FileSystemManager::Name_, {});
  }

  // mojom::blink::FileSystem
  void Open(const scoped_refptr<const SecurityOrigin>& origin,
            mojom::blink::FileSystemType file_system_type,
            OpenCallback callback) override {}
  void ResolveURL(const KURL& filesystem_url,
                  ResolveURLCallback callback) override {}
  void Move(const KURL& src_path,
            const KURL& dest_path,
            MoveCallback callback) override {}
  void Copy(const KURL& src_path,
            const KURL& dest_path,
            CopyCallback callback) override {}
  void Remove(const KURL& path,
              bool recursive,
              RemoveCallback callback) override {}
  void ReadMetadata(const KURL& path, ReadMetadataCallback callback) override {}
  void Create(const KURL& path,
              bool exclusive,
              bool is_directory,
              bool recursive,
              CreateCallback callback) override {}
  void Exists(const KURL& path,
              bool is_directory,
              ExistsCallback callback) override {}
  void ReadDirectory(
      const KURL& path,
      mojo::PendingRemote<mojom::blink::FileSystemOperationListener>
          pending_listener) override {}
  void ReadDirectorySync(const KURL& path,
                         ReadDirectorySyncCallback callback) override {}
  void Write(const KURL& file_path,
             mojo::PendingRemote<mojom::blink::Blob> blob,
             int64_t position,
             mojo::PendingReceiver<mojom::blink::FileSystemCancellableOperation>
                 op_receiver,
             mojo::PendingRemote<mojom::blink::FileSystemOperationListener>
                 pending_listener) override {}
  void WriteSync(const KURL& file_path,
                 mojo::PendingRemote<mojom::blink::Blob> blob,
                 int64_t position,
                 WriteSyncCallback callback) override {}
  void Truncate(
      const KURL& file_path,
      int64_t length,
      mojo::PendingReceiver<mojom::blink::FileSystemCancellableOperation>
          op_receiver,
      TruncateCallback callback) override {}
  void TruncateSync(const KURL& file_path,
                    int64_t length,
                    TruncateSyncCallback callback) override {}
  void CreateSnapshotFile(const KURL& file_path,
                          CreateSnapshotFileCallback callback) override {}
  void GetPlatformPath(const KURL& file_path,
                       GetPlatformPathCallback callback) override {}
  void RegisterBlob(const String& content_type,
                    const KURL& url,
                    uint64_t length,
                    std::optional<base::Time> expected_modification_time,
                    RegisterBlobCallback callback) override {
    std::move(mock_register_blob_callback_)
        .Run(content_type, url, length, expected_modification_time);
    std::move(callback).Run(BlobDataHandle::Create());
  }

  void SetMockRegisterBlobCallback(
      MockRegisterBlobCallback mock_register_blob_callback) {
    mock_register_blob_callback_ = std::move(mock_register_blob_callback);
  }

 private:
  void BindReceiver(mojo::ScopedMessagePipeHandle handle) {
    receivers_.Add(this, mojo::PendingReceiver<mojom::blink::FileSystemManager>(
                             std::move(handle)));
  }

  const BrowserInterfaceBrokerProxy& broker_;
  mojo::ReceiverSet<mojom::blink::FileSystemManager> receivers_;
  MockRegisterBlobCallback mock_register_blob_callback_;
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
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext context;
  auto* const file = MakeGarbageCollected<File>(&context.GetExecutionContext(),
                                                "/native/path");
  MockBlob::Create(file, base::Time());

  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ("/native/path", file->GetPath());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  ExpectTimestampIsNow(*file);
}

TEST(FileTest, NativeFileWithUnixEpochTimestamp) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext context;
  auto* const file = MakeGarbageCollected<File>(&context.GetExecutionContext(),
                                                "/native/path");
  MockBlob::Create(file, base::Time::UnixEpoch());

  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ(0, file->lastModified());
  EXPECT_EQ(base::Time::UnixEpoch(), file->LastModifiedTime());
}

TEST(FileTest, NativeFileWithApocalypseTimestamp) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext context;
  auto* const file = MakeGarbageCollected<File>(&context.GetExecutionContext(),
                                                "/native/path");
  MockBlob::Create(file, base::Time::Max());

  EXPECT_TRUE(file->HasBackingFile());

  EXPECT_EQ((base::Time::Max() - base::Time::UnixEpoch()).InMilliseconds(),
            file->lastModified());
  EXPECT_EQ(base::Time::Max(), file->LastModifiedTime());
}

TEST(FileTest, BlobBackingFileWithoutTimestamp) {
  test::TaskEnvironment task_environment;
  auto* const file = MakeGarbageCollected<File>("name", std::nullopt,
                                                BlobDataHandle::Create());
  EXPECT_FALSE(file->HasBackingFile());
  EXPECT_TRUE(file->GetPath().empty());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  ExpectTimestampIsNow(*file);
}

TEST(FileTest, BlobBackingFileWithWindowsEpochTimestamp) {
  test::TaskEnvironment task_environment;
  auto* const file = MakeGarbageCollected<File>("name", base::Time(),
                                                BlobDataHandle::Create());
  EXPECT_FALSE(file->HasBackingFile());
  EXPECT_TRUE(file->GetPath().empty());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  EXPECT_EQ((base::Time() - base::Time::UnixEpoch()).InMilliseconds(),
            file->lastModified());
  EXPECT_EQ(base::Time(), file->LastModifiedTime());
}

TEST(FileTest, BlobBackingFileWithUnixEpochTimestamp) {
  test::TaskEnvironment task_environment;
  const scoped_refptr<BlobDataHandle> blob_data_handle =
      BlobDataHandle::Create();
  auto* const file = MakeGarbageCollected<File>("name", base::Time::UnixEpoch(),
                                                blob_data_handle);
  EXPECT_FALSE(file->HasBackingFile());
  EXPECT_TRUE(file->GetPath().empty());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  EXPECT_EQ(INT64_C(0), file->lastModified());
  EXPECT_EQ(base::Time::UnixEpoch(), file->LastModifiedTime());
}

TEST(FileTest, BlobBackingFileWithApocalypseTimestamp) {
  test::TaskEnvironment task_environment;
  constexpr base::Time kMaxTime = base::Time::Max();
  auto* const file =
      MakeGarbageCollected<File>("name", kMaxTime, BlobDataHandle::Create());
  EXPECT_FALSE(file->HasBackingFile());
  EXPECT_TRUE(file->GetPath().empty());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  EXPECT_EQ((kMaxTime - base::Time::UnixEpoch()).InMilliseconds(),
            file->lastModified());
  EXPECT_EQ(kMaxTime, file->LastModifiedTime());
}

TEST(FileTest, fileSystemFileWithNativeSnapshot) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext context;
  FileMetadata metadata;
  metadata.platform_path = "/native/snapshot";
  File* const file = File::CreateForFileSystemFile(
      &context.GetExecutionContext(), "name", metadata, File::kIsUserVisible);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ("/native/snapshot", file->GetPath());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
}

TEST(FileTest, fileSystemFileWithNativeSnapshotAndSize) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext context;
  FileMetadata metadata;
  metadata.length = 1024ll;
  metadata.platform_path = "/native/snapshot";
  File* const file = File::CreateForFileSystemFile(
      &context.GetExecutionContext(), "name", metadata, File::kIsUserVisible);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ("/native/snapshot", file->GetPath());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
}

TEST(FileTest, FileSystemFileWithWindowsEpochTimestamp) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext context;
  FileMetadata metadata;
  metadata.length = INT64_C(1025);
  metadata.modification_time = base::Time();
  metadata.platform_path = "/native/snapshot";
  File* const file = File::CreateForFileSystemFile(
      &context.GetExecutionContext(), "name", metadata, File::kIsUserVisible);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ("/native/snapshot", file->GetPath());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  EXPECT_EQ(UINT64_C(1025), file->size());
  EXPECT_EQ((base::Time() - base::Time::UnixEpoch()).InMilliseconds(),
            file->lastModified());
  EXPECT_EQ(base::Time(), file->LastModifiedTime());
}

TEST(FileTest, FileSystemFileWithUnixEpochTimestamp) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext context;
  FileMetadata metadata;
  metadata.length = INT64_C(1025);
  metadata.modification_time = base::Time::UnixEpoch();
  metadata.platform_path = "/native/snapshot";
  File* const file = File::CreateForFileSystemFile(
      &context.GetExecutionContext(), "name", metadata, File::kIsUserVisible);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ("/native/snapshot", file->GetPath());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  EXPECT_EQ(UINT64_C(1025), file->size());
  EXPECT_EQ(INT64_C(0), file->lastModified());
  EXPECT_EQ(base::Time::UnixEpoch(), file->LastModifiedTime());
}

TEST(FileTest, FileSystemFileWithApocalypseTimestamp) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext context;
  constexpr base::Time kMaxTime = base::Time::Max();
  FileMetadata metadata;
  metadata.length = INT64_C(1025);
  metadata.modification_time = kMaxTime;
  metadata.platform_path = "/native/snapshot";
  File* const file = File::CreateForFileSystemFile(
      &context.GetExecutionContext(), "name", metadata, File::kIsUserVisible);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ("/native/snapshot", file->GetPath());
  EXPECT_TRUE(file->FileSystemURL().IsEmpty());
  EXPECT_EQ(UINT64_C(1025), file->size());
  EXPECT_EQ((kMaxTime - base::Time::UnixEpoch()).InMilliseconds(),
            file->lastModified());
  EXPECT_EQ(kMaxTime, file->LastModifiedTime());
}

TEST(FileTest, fileSystemFileWithoutNativeSnapshot) {
  test::TaskEnvironment task_environment;
  KURL url("filesystem:http://example.com/isolated/hash/non-native-file");
  FileMetadata metadata;
  metadata.length = 0;
  File* const file = File::CreateForFileSystemFile(
      url, metadata, File::kIsUserVisible, BlobDataHandle::Create());
  EXPECT_FALSE(file->HasBackingFile());
  EXPECT_TRUE(file->GetPath().empty());
  EXPECT_EQ(url, file->FileSystemURL());
}

TEST(FileTest, hsaSameSource) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext context;
  auto* const native_file_a1 = MakeGarbageCollected<File>(
      &context.GetExecutionContext(), "/native/pathA");
  auto* const native_file_a2 = MakeGarbageCollected<File>(
      &context.GetExecutionContext(), "/native/pathA");
  auto* const native_file_b = MakeGarbageCollected<File>(
      &context.GetExecutionContext(), "/native/pathB");

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
  File* const file_system_file_a1 = File::CreateForFileSystemFile(
      url_a, metadata, File::kIsUserVisible, BlobDataHandle::Create());
  File* const file_system_file_a2 = File::CreateForFileSystemFile(
      url_a, metadata, File::kIsUserVisible, BlobDataHandle::Create());
  File* const file_system_file_b = File::CreateForFileSystemFile(
      url_b, metadata, File::kIsUserVisible, BlobDataHandle::Create());

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

TEST(FileTest, createForFileSystem) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("http://example.com"));
  Document& document = scope.GetDocument();
  base::RunLoop run_loop;

  KURL filesystem_url(
      "filesystem:http://example.com/isolated/hash/non-native-file");
  FileMetadata metadata;
  metadata.length = 0;

  MockFileSystemManager manager(
      document.GetFrame()->GetBrowserInterfaceBroker());
  manager.SetMockRegisterBlobCallback(base::BindLambdaForTesting(
      [&](const String& content_type, const KURL& url, uint64_t length,
          std::optional<base::Time> expected_modification_time) {
        EXPECT_EQ(metadata.length, static_cast<int64_t>(length));
        EXPECT_EQ("", content_type);
        EXPECT_EQ(url, filesystem_url);
        run_loop.Quit();
      }));

  File* const file = File::CreateForFileSystemFile(
      *document.GetExecutionContext(), filesystem_url, metadata,
      File::kIsUserVisible);

  run_loop.Run();
  EXPECT_TRUE(file);
}
}  // namespace blink
