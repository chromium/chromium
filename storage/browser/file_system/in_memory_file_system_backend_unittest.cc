// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/in_memory_file_system_backend.h"

#include <limits>
#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_view_util.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/in_memory_file_registry.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace storage {

namespace {

constexpr char kOrigin[] = "https://example.com";
constexpr char kContents[] = "in memory upload payload";

// Builds the `filesystem:` URL a renderer would be handed for `file` and cracks
// it the same way FileSystemManagerImpl does.
FileSystemURL CrackStagedFile(const InMemoryFileRegistry::ScopedFile& file) {
  const GURL url(GetIsolatedFileSystemRootURIString(
                     GURL(kOrigin), file.filesystem_id(), std::string())
                     .append(file.register_name()));
  return IsolatedContext::GetInstance()->CrackURL(
      url, blink::StorageKey::CreateFromStringForTesting(kOrigin));
}

// Reads the whole stream, which the in-memory reader always satisfies
// synchronously.
std::string ReadAll(FileStreamReader& reader, size_t size) {
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(size);
  const int result = reader.Read(buffer.get(), static_cast<int>(size),
                                 net::CompletionOnceCallback());
  EXPECT_EQ(static_cast<int>(size), result);
  if (result < 0) {
    return std::string();
  }
  return std::string(
      base::as_string_view(buffer->first(static_cast<size_t>(result))));
}

class InMemoryFileSystemBackendTest : public testing::Test {
 protected:
  InMemoryFileRegistry* registry() {
    return InMemoryFileRegistry::GetInstance();
  }

  base::test::TaskEnvironment task_environment_;
  InMemoryFileSystemBackend backend_;
};

TEST_F(InMemoryFileSystemBackendTest, HandlesOnlyInMemoryType) {
  EXPECT_TRUE(backend_.CanHandleType(kFileSystemTypeInMemory));
  EXPECT_FALSE(backend_.CanHandleType(kFileSystemTypeIsolated));
  EXPECT_FALSE(backend_.CanHandleType(kFileSystemTypeLocal));
  EXPECT_FALSE(backend_.CanHandleType(kFileSystemTypeTemporary));
}

TEST_F(InMemoryFileSystemBackendTest, RegisteredFileCracksToInMemoryType) {
  InMemoryFileRegistry::ScopedFile file =
      registry()->Register("payload.txt", kContents, base::Time::Now());
  ASSERT_TRUE(file.is_valid());
  EXPECT_EQ("payload.txt", file.register_name());
  EXPECT_EQ(static_cast<int64_t>(std::string(kContents).size()), file.length());

  const FileSystemURL url = CrackStagedFile(file);
  ASSERT_TRUE(url.is_valid());
  EXPECT_EQ(kFileSystemTypeIsolated, url.mount_type());
  EXPECT_EQ(kFileSystemTypeInMemory, url.type());
  EXPECT_EQ(file.filesystem_id(), url.mount_filesystem_id());
}

TEST_F(InMemoryFileSystemBackendTest, StreamsContentsFromMemory) {
  InMemoryFileRegistry::ScopedFile file =
      registry()->Register("payload.txt", kContents, base::Time::Now());
  ASSERT_TRUE(file.is_valid());
  const FileSystemURL url = CrackStagedFile(file);
  ASSERT_TRUE(url.is_valid());

  // Streaming must be supported, otherwise blobs would fall back to
  // CreateSnapshotFile() and require a real file on disk.
  EXPECT_TRUE(backend_.SupportsStreaming(url));

  std::unique_ptr<FileStreamReader> reader = backend_.CreateFileStreamReader(
      url, /*offset=*/0, kMaximumLength, base::Time(), /*context=*/nullptr,
      base::NullCallback());
  ASSERT_TRUE(reader);
  EXPECT_EQ(static_cast<int64_t>(std::string(kContents).size()),
            reader->GetLength(FileStreamReader::GetLengthCallback()));
  EXPECT_EQ(kContents, ReadAll(*reader, std::string(kContents).size()));
}

TEST_F(InMemoryFileSystemBackendTest, ReadsFromOffset) {
  InMemoryFileRegistry::ScopedFile file =
      registry()->Register("payload.txt", kContents, base::Time::Now());
  ASSERT_TRUE(file.is_valid());
  const FileSystemURL url = CrackStagedFile(file);
  ASSERT_TRUE(url.is_valid());

  std::unique_ptr<FileStreamReader> reader = backend_.CreateFileStreamReader(
      url, /*offset=*/3, kMaximumLength, base::Time(), /*context=*/nullptr,
      base::NullCallback());
  ASSERT_TRUE(reader);
  // GetLength() reports the whole file, independent of the offset.
  EXPECT_EQ(static_cast<int64_t>(std::string(kContents).size()),
            reader->GetLength(FileStreamReader::GetLengthCallback()));
  EXPECT_EQ(std::string(kContents).substr(3),
            ReadAll(*reader, std::string(kContents).size() - 3));
}

TEST_F(InMemoryFileSystemBackendTest, UnknownFileHasNoReader) {
  InMemoryFileRegistry::ScopedFile file =
      registry()->Register("payload.txt", kContents, base::Time::Now());
  ASSERT_TRUE(file.is_valid());
  const FileSystemURL url = CrackStagedFile(file);
  ASSERT_TRUE(url.is_valid());

  // Dropping the handle unregisters the contents, so the file can no longer be
  // opened even though the URL is still well formed.
  file = InMemoryFileRegistry::ScopedFile();
  EXPECT_FALSE(backend_.CreateFileStreamReader(
      url, /*offset=*/0, kMaximumLength, base::Time(), /*context=*/nullptr,
      base::NullCallback()));
}

TEST_F(InMemoryFileSystemBackendTest, InFlightReadSurvivesUnregistration) {
  InMemoryFileRegistry::ScopedFile file =
      registry()->Register("payload.txt", kContents, base::Time::Now());
  ASSERT_TRUE(file.is_valid());
  const FileSystemURL url = CrackStagedFile(file);
  ASSERT_TRUE(url.is_valid());

  std::unique_ptr<FileStreamReader> reader = backend_.CreateFileStreamReader(
      url, /*offset=*/0, kMaximumLength, base::Time(), /*context=*/nullptr,
      base::NullCallback());
  ASSERT_TRUE(reader);

  // The reader holds its own reference to the bytes.
  file = InMemoryFileRegistry::ScopedFile();
  EXPECT_EQ(kContents, ReadAll(*reader, std::string(kContents).size()));
}

TEST_F(InMemoryFileSystemBackendTest, ReadFailsWhenModificationTimeDiffers) {
  const base::Time modification_time = base::Time::Now();
  InMemoryFileRegistry::ScopedFile file =
      registry()->Register("payload.txt", kContents, modification_time);
  ASSERT_TRUE(file.is_valid());
  const FileSystemURL url = CrackStagedFile(file);
  ASSERT_TRUE(url.is_valid());

  std::unique_ptr<FileStreamReader> reader = backend_.CreateFileStreamReader(
      url, /*offset=*/0, kMaximumLength, modification_time + base::Seconds(1),
      /*context=*/nullptr, base::NullCallback());
  ASSERT_TRUE(reader);
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(16);
  EXPECT_EQ(net::ERR_UPLOAD_FILE_CHANGED,
            reader->Read(buffer.get(), 16, net::CompletionOnceCallback()));
}

TEST_F(InMemoryFileSystemBackendTest, GetFileInfo) {
  const base::Time modification_time = base::Time::Now();
  InMemoryFileRegistry::ScopedFile file =
      registry()->Register("payload.txt", kContents, modification_time);
  ASSERT_TRUE(file.is_valid());
  const FileSystemURL url = CrackStagedFile(file);
  ASSERT_TRUE(url.is_valid());
  AsyncFileUtil* async_file_util =
      backend_.GetAsyncFileUtil(kFileSystemTypeInMemory);

  base::test::TestFuture<base::File::Error, const base::File::Info&> future;
  async_file_util->GetFileInfo(/*context=*/nullptr, url,
                               AsyncFileUtil::GetMetadataFieldSet::All(),
                               future.GetCallback());
  auto [error, info] = future.Take();
  EXPECT_EQ(base::File::FILE_OK, error);
  EXPECT_EQ(static_cast<int64_t>(std::string(kContents).size()), info.size);
  EXPECT_FALSE(info.is_directory);
  EXPECT_EQ(modification_time, info.last_modified);

  // Dropping the handle unregisters the contents, so the file is no longer
  // found even though the URL is still well formed.
  file = InMemoryFileRegistry::ScopedFile();
  async_file_util->GetFileInfo(/*context=*/nullptr, url,
                               AsyncFileUtil::GetMetadataFieldSet::All(),
                               future.GetCallback());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, future.Get<0>());
}

// Write access to in-memory file systems is never granted, so no writer is ever
// requested.
TEST_F(InMemoryFileSystemBackendTest, IsReadOnly) {
  InMemoryFileRegistry::ScopedFile file =
      registry()->Register("payload.txt", kContents, base::Time::Now());
  ASSERT_TRUE(file.is_valid());
  const FileSystemURL url = CrackStagedFile(file);
  ASSERT_TRUE(url.is_valid());

  EXPECT_NOTREACHED_DEATH(
      backend_.CreateFileStreamWriter(url, /*offset=*/0, /*context=*/nullptr));
}

// In-memory files are not backed by a base::File, so nothing opens them as one;
// they are only ever streamed.
TEST_F(InMemoryFileSystemBackendTest, OnlyStreamingIsSupported) {
  InMemoryFileRegistry::ScopedFile file =
      registry()->Register("payload.txt", kContents, base::Time::Now());
  ASSERT_TRUE(file.is_valid());
  const FileSystemURL url = CrackStagedFile(file);
  ASSERT_TRUE(url.is_valid());

  EXPECT_NOTREACHED_DEATH(
      backend_.GetAsyncFileUtil(kFileSystemTypeInMemory)
          ->CreateOrOpen(/*context=*/nullptr, url,
                         base::File::FLAG_OPEN | base::File::FLAG_READ,
                         base::DoNothing()));
}

// Renderers that were granted read access to the file system can ask for a
// directory listing or a snapshot, so both must fail gracefully.
TEST_F(InMemoryFileSystemBackendTest, RefusesDirectoryListingsAndSnapshots) {
  InMemoryFileRegistry::ScopedFile file =
      registry()->Register("payload.txt", kContents, base::Time::Now());
  ASSERT_TRUE(file.is_valid());
  const FileSystemURL url = CrackStagedFile(file);
  ASSERT_TRUE(url.is_valid());
  AsyncFileUtil* async_file_util =
      backend_.GetAsyncFileUtil(kFileSystemTypeInMemory);

  base::test::TestFuture<base::File::Error, AsyncFileUtil::EntryList, bool>
      read_directory_future;
  async_file_util->ReadDirectory(/*context=*/nullptr, url,
                                 read_directory_future.GetRepeatingCallback());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_A_DIRECTORY,
            read_directory_future.Get<0>());

  base::test::TestFuture<base::File::Error, const base::File::Info&,
                         const base::FilePath&,
                         scoped_refptr<ShareableFileReference>>
      snapshot_future;
  async_file_util->CreateSnapshotFile(/*context=*/nullptr, url,
                                      snapshot_future.GetCallback());
  EXPECT_EQ(base::File::FILE_ERROR_SECURITY, snapshot_future.Get<0>());
}

}  // namespace

}  // namespace storage
