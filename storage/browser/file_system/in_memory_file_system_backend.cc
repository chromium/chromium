// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/in_memory_file_system_backend.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/in_memory_file_registry.h"
#include "storage/common/file_system/file_system_types.h"
#include "url/gurl.h"

namespace storage {

namespace {

// Looks up the file an in-memory `filesystem:` URL refers to. In-memory files
// are registered under a virtual path, so the URL cracks to an empty path() and
// the file system id is what identifies the file.
scoped_refptr<InMemoryFileRegistry::File> FindFile(const FileSystemURL& url) {
  if (!url.is_valid() || url.type() != kFileSystemTypeInMemory) {
    return nullptr;
  }
  return InMemoryFileRegistry::GetInstance()->Find(url.filesystem_id());
}

base::File::Info ToFileInfo(const InMemoryFileRegistry::File& file) {
  base::File::Info info;
  info.size = base::checked_cast<int64_t>(file.contents().size());
  info.is_directory = false;
  info.is_symbolic_link = false;
  info.last_modified = file.modification_time();
  info.last_accessed = file.modification_time();
  info.creation_time = file.modification_time();
  return info;
}

// Serves the bytes of a single in-memory file. The reader holds its own
// reference to the contents, so reads keep working even if the file is
// unregistered or evicted while they are in flight.
class InMemoryFileStreamReader : public FileStreamReader {
 public:
  InMemoryFileStreamReader(scoped_refptr<InMemoryFileRegistry::File> file,
                           int64_t initial_offset,
                           const base::Time& expected_modification_time)
      : file_(std::move(file)),
        offset_(initial_offset),
        expected_modification_time_(expected_modification_time) {}

  InMemoryFileStreamReader(const InMemoryFileStreamReader&) = delete;
  InMemoryFileStreamReader& operator=(const InMemoryFileStreamReader&) = delete;

  ~InMemoryFileStreamReader() override = default;

  // FileStreamReader:
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override {
    if (!VerifyModificationTime()) {
      return net::ERR_UPLOAD_FILE_CHANGED;
    }
    base::span<const uint8_t> bytes = base::as_byte_span(file_->contents());
    if (offset_ < 0 || offset_ > base::checked_cast<int64_t>(bytes.size())) {
      return net::ERR_REQUEST_RANGE_NOT_SATISFIABLE;
    }
    bytes = bytes.subspan(base::checked_cast<size_t>(offset_));

    const size_t bytes_to_copy =
        std::min(bytes.size(), base::checked_cast<size_t>(buf_len));
    buf->span().first(bytes_to_copy).copy_from(bytes.first(bytes_to_copy));
    offset_ += base::checked_cast<int64_t>(bytes_to_copy);
    return base::checked_cast<int>(bytes_to_copy);
  }

  int64_t GetLength(GetLengthCallback callback) override {
    if (!VerifyModificationTime()) {
      return net::ERR_UPLOAD_FILE_CHANGED;
    }
    // Callers expect the length of the whole file, independent of `offset_`.
    return base::checked_cast<int64_t>(file_->contents().size());
  }

 private:
  bool VerifyModificationTime() const {
    if (expected_modification_time_.is_null()) {
      return true;
    }
    base::File::Info info = ToFileInfo(*file_);
    return FileStreamReader::VerifySnapshotTime(expected_modification_time_,
                                                info);
  }

  const scoped_refptr<InMemoryFileRegistry::File> file_;
  int64_t offset_;
  const base::Time expected_modification_time_;
};

}  // namespace

// Read-only AsyncFileUtil over InMemoryFileRegistry. Only metadata lookups are
// supported; blob reads go through InMemoryFileStreamReader instead.
class InMemoryFileSystemBackend::AsyncFileUtilImpl : public AsyncFileUtil {
 public:
  AsyncFileUtilImpl() = default;
  ~AsyncFileUtilImpl() override = default;

  void GetFileInfo(std::unique_ptr<FileSystemOperationContext> context,
                   const FileSystemURL& url,
                   GetMetadataFieldSet fields,
                   GetFileInfoCallback callback) override {
    scoped_refptr<InMemoryFileRegistry::File> file = FindFile(url);
    if (!file) {
      Reply(std::move(callback), base::File::FILE_ERROR_NOT_FOUND,
            base::File::Info());
      return;
    }
    Reply(std::move(callback), base::File::FILE_OK, ToFileInfo(*file));
  }

  void CreateOrOpen(std::unique_ptr<FileSystemOperationContext> context,
                    const FileSystemURL& url,
                    uint32_t file_flags,
                    CreateOrOpenCallback callback) override {
    // In-memory files are not backed by a base::File, so they are never opened
    // as one; they are only read through InMemoryFileStreamReader.
    NOTREACHED();
  }

  void EnsureFileExists(std::unique_ptr<FileSystemOperationContext> context,
                        const FileSystemURL& url,
                        EnsureFileExistsCallback callback) override {
    Reply(std::move(callback), base::File::FILE_ERROR_SECURITY,
          /*created=*/false);
  }

  void CreateDirectory(std::unique_ptr<FileSystemOperationContext> context,
                       const FileSystemURL& url,
                       bool exclusive,
                       bool recursive,
                       StatusCallback callback) override {
    FailWithSecurityError(std::move(callback));
  }

  void ReadDirectory(std::unique_ptr<FileSystemOperationContext> context,
                     const FileSystemURL& url,
                     ReadDirectoryCallback callback) override {
    // In-memory file systems only ever hold a single file, never a directory.
    // Still, renderers with read access to the file system can ask for a
    // listing, so this must fail gracefully.
    Reply(std::move(callback), base::File::FILE_ERROR_NOT_A_DIRECTORY,
          EntryList(), /*has_more=*/false);
  }

  void Touch(std::unique_ptr<FileSystemOperationContext> context,
             const FileSystemURL& url,
             const base::Time& last_access_time,
             const base::Time& last_modified_time,
             StatusCallback callback) override {
    FailWithSecurityError(std::move(callback));
  }

  void Truncate(std::unique_ptr<FileSystemOperationContext> context,
                const FileSystemURL& url,
                int64_t length,
                StatusCallback callback) override {
    FailWithSecurityError(std::move(callback));
  }

  void CopyFileLocal(std::unique_ptr<FileSystemOperationContext> context,
                     const FileSystemURL& src_url,
                     const FileSystemURL& dest_url,
                     CopyOrMoveOptionSet options,
                     CopyFileProgressCallback progress_callback,
                     StatusCallback callback) override {
    FailWithSecurityError(std::move(callback));
  }

  void MoveFileLocal(std::unique_ptr<FileSystemOperationContext> context,
                     const FileSystemURL& src_url,
                     const FileSystemURL& dest_url,
                     CopyOrMoveOptionSet options,
                     StatusCallback callback) override {
    FailWithSecurityError(std::move(callback));
  }

  void CopyInForeignFile(std::unique_ptr<FileSystemOperationContext> context,
                         const base::FilePath& src_file_path,
                         const FileSystemURL& dest_url,
                         StatusCallback callback) override {
    FailWithSecurityError(std::move(callback));
  }

  void DeleteFile(std::unique_ptr<FileSystemOperationContext> context,
                  const FileSystemURL& url,
                  StatusCallback callback) override {
    FailWithSecurityError(std::move(callback));
  }

  void DeleteDirectory(std::unique_ptr<FileSystemOperationContext> context,
                       const FileSystemURL& url,
                       StatusCallback callback) override {
    FailWithSecurityError(std::move(callback));
  }

  void DeleteRecursively(std::unique_ptr<FileSystemOperationContext> context,
                         const FileSystemURL& url,
                         StatusCallback callback) override {
    FailWithSecurityError(std::move(callback));
  }

  void CreateSnapshotFile(std::unique_ptr<FileSystemOperationContext> context,
                          const FileSystemURL& url,
                          CreateSnapshotFileCallback callback) override {
    // Creating a snapshot would mean writing the contents to disk, which is
    // exactly what this file system exists to avoid. Callers are expected to
    // stream instead (see SupportsStreaming()), but some generic paths, e.g.
    // copying to another file system, may still ask for a snapshot.
    Reply(std::move(callback), base::File::FILE_ERROR_SECURITY,
          base::File::Info(), base::FilePath(),
          scoped_refptr<ShareableFileReference>());
  }

 private:
  template <typename Callback, typename... Args>
  static void Reply(Callback callback, Args... args) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), args...));
  }

  static void FailWithSecurityError(StatusCallback callback) {
    Reply(std::move(callback), base::File::FILE_ERROR_SECURITY);
  }
};

InMemoryFileSystemBackend::InMemoryFileSystemBackend()
    : async_file_util_(std::make_unique<AsyncFileUtilImpl>()) {}

InMemoryFileSystemBackend::~InMemoryFileSystemBackend() = default;

bool InMemoryFileSystemBackend::CanHandleType(FileSystemType type) const {
  return type == kFileSystemTypeInMemory;
}

void InMemoryFileSystemBackend::Initialize(FileSystemContext* context) {}

void InMemoryFileSystemBackend::ResolveURL(const FileSystemURL& url,
                                           OpenFileSystemMode mode,
                                           ResolveURLCallback callback) {
  // In-memory file systems are only ever reached through an isolated file
  // system URL handed out by the browser; they cannot be opened by name.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), GURL(), std::string(),
                                base::File::FILE_ERROR_SECURITY));
}

AsyncFileUtil* InMemoryFileSystemBackend::GetAsyncFileUtil(
    FileSystemType type) {
  DCHECK_EQ(type, kFileSystemTypeInMemory);
  return async_file_util_.get();
}

WatcherManager* InMemoryFileSystemBackend::GetWatcherManager(
    FileSystemType type) {
  return nullptr;
}

CopyOrMoveFileValidatorFactory*
InMemoryFileSystemBackend::GetCopyOrMoveFileValidatorFactory(
    FileSystemType type,
    base::File::Error* error_code) {
  DCHECK(error_code);
  *error_code = base::File::FILE_OK;
  return nullptr;
}

std::unique_ptr<FileSystemOperation>
InMemoryFileSystemBackend::CreateFileSystemOperation(
    OperationType type,
    const FileSystemURL& url,
    FileSystemContext* context,
    base::File::Error* error_code) const {
  return FileSystemOperation::Create(
      type, url, context,
      std::make_unique<FileSystemOperationContext>(context));
}

bool InMemoryFileSystemBackend::SupportsStreaming(
    const FileSystemURL& url) const {
  // Blobs must read through CreateFileStreamReader(). The alternative,
  // AsyncFileUtil::CreateSnapshotFile(), would require a real file on disk.
  return true;
}

bool InMemoryFileSystemBackend::HasInplaceCopyImplementation(
    FileSystemType type) const {
  return false;
}

std::unique_ptr<FileStreamReader>
InMemoryFileSystemBackend::CreateFileStreamReader(
    const FileSystemURL& url,
    int64_t offset,
    int64_t max_bytes_to_read,
    const base::Time& expected_modification_time,
    FileSystemContext* context,
    file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
        file_access) const {
  scoped_refptr<InMemoryFileRegistry::File> file = FindFile(url);
  if (!file) {
    return nullptr;
  }
  // `max_bytes_to_read` is only a prefetch hint; the contents are already in
  // memory and callers bound each Read() themselves, so it is ignored.
  return std::make_unique<InMemoryFileStreamReader>(std::move(file), offset,
                                                    expected_modification_time);
}

std::unique_ptr<FileStreamWriter>
InMemoryFileSystemBackend::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64_t offset,
    FileSystemContext* context) const {
  // Read-only: write access to in-memory file systems is never granted.
  NOTREACHED();
}

FileSystemQuotaUtil* InMemoryFileSystemBackend::GetQuotaUtil() {
  // No quota support.
  return nullptr;
}

const UpdateObserverList* InMemoryFileSystemBackend::GetUpdateObservers(
    FileSystemType type) const {
  return nullptr;
}

const ChangeObserverList* InMemoryFileSystemBackend::GetChangeObservers(
    FileSystemType type) const {
  return nullptr;
}

const AccessObserverList* InMemoryFileSystemBackend::GetAccessObservers(
    FileSystemType type) const {
  return nullptr;
}

}  // namespace storage
