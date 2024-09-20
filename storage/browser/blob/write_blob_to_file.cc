// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/write_blob_to_file.h"

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/file_access/scoped_file_access.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/shareable_blob_data_item.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_writer_delegate.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "third_party/blink/public/common/blob/blob_utils.h"

namespace storage {
namespace {
using DelegateNoProgressWriteCallback = base::OnceCallback<void(
    base::File::Error result,
    int64_t bytes,
    FileWriterDelegate::WriteProgressStatus write_status)>;
using CopyCallback = base::OnceCallback<mojom::WriteBlobToFileResult(
    file_access::ScopedFileAccess)>;

struct WriteState {
  std::unique_ptr<FileWriterDelegate> delegate;
  DelegateNoProgressWriteCallback callback;
  base::CheckedNumeric<int64_t> total_bytes;
};

// Utility function to ignore all progress events returned when running
// FileWriterDelegate::Start. This means that there is either a single call for
// a success or failure. This wrapper also handles owning the lifetime of the
// FileWriterDelegate, which it destructs after receiving a success or error.
FileWriterDelegate::DelegateWriteCallback IgnoreProgressWrapper(
    std::unique_ptr<FileWriterDelegate> delegate,
    DelegateNoProgressWriteCallback callback) {
  return base::BindRepeating(
      [](WriteState* write_state, base::File::Error result, int64_t bytes,
         FileWriterDelegate::WriteProgressStatus write_status) {
        DCHECK_GE(bytes, 0);
        DCHECK(!write_state->callback.is_null());
        DCHECK(write_state->delegate);
        if (result == base::File::FILE_OK) {
          DCHECK(write_status == FileWriterDelegate::SUCCESS_COMPLETED ||
                 write_status == FileWriterDelegate::SUCCESS_IO_PENDING);
        } else {
          DCHECK(write_status == FileWriterDelegate::ERROR_WRITE_STARTED ||
                 write_status == FileWriterDelegate::ERROR_WRITE_NOT_STARTED);
        }

        write_state->total_bytes += bytes;
        if (write_status == FileWriterDelegate::SUCCESS_IO_PENDING)
          return;
        std::move(write_state->callback)
            .Run(result, write_state->total_bytes.ValueOrDie(), write_status);
        // This is necessary because FileWriterDelegate owns this repeating
        // callback, so there is a cyclic dependency.
        write_state->delegate.reset();
      },
      base::Owned(new WriteState{std::move(delegate), std::move(callback), 0}));
}

// Copied from file_util_posix.cc, with the addition of |offset| and |max_size|
// parameters. The parameters apply to the |infile|. The |bytes_copied|
// parameter keeps track of the number of bytes written to |outfile|.
// Note - this function can still succeed if the size |infile| is less than
// |max_size|. The caller should use |bytes_copied| to know exactly how many
// bytes were copied.
bool CopyFileContentsWithOffsetAndSize(base::File* infile,
                                       base::File* outfile,
                                       int64_t* bytes_copied,
                                       int64_t offset,
                                       int64_t max_size) {
  static constexpr size_t kBufferSize = 32768;
  DCHECK_GE(max_size, 0);
  *bytes_copied = 0;
  base::CheckedNumeric<int64_t> checked_max_size = max_size;

  std::vector<char> buffer(
      std::min(kBufferSize, base::checked_cast<size_t>(max_size)));
  infile->Seek(base::File::FROM_BEGIN, offset);

  for (;;) {
    size_t bytes_to_read =
        std::min(buffer.size(),
                 base::checked_cast<size_t>(checked_max_size.ValueOrDie()));
    std::optional<size_t> bytes_read = infile->ReadAtCurrentPos(
        base::as_writable_byte_span(buffer).first(bytes_to_read));
    if (!bytes_read.has_value()) {
      return false;
    }
    if (bytes_read.value() == 0) {
      return true;
    }
    checked_max_size -= bytes_read.value();
    if (!checked_max_size.IsValid()) {
      return false;
    }
    // Allow for partial writes
    auto span_to_write = base::as_byte_span(buffer).first(bytes_read.value());
    while (!span_to_write.empty()) {
      std::optional<size_t> bytes_written_partial =
          outfile->WriteAtCurrentPos(span_to_write);
      if (!bytes_written_partial.has_value()) {
        return false;
      }
      span_to_write = span_to_write.subspan(bytes_written_partial.value());
      *bytes_copied += bytes_written_partial.value();
    }
    return true;
  }

  NOTREACHED();
}

// Copies the contents of |copy_from| to |copy_to|, with the given |offset| and
// optional |size| applied to the |copy_from| file. The
// |expected_last_modified_copy_from| must match, within a second, the last
// modified time of the |copy_from| file. Afterwards, the |last_modified| date
// is optionally saved as the last modified & last accessed time of |copy_to|.
// If |flush_on_close| is true, then Flush is called on the |copy_to| file
// before it is closed. The `file_access::ScopedFileAccess` parameter is
// expected to allow access to the source file. It has to be kept in scope of
// this function because it must be alive while the copy operation is happening.
mojom::WriteBlobToFileResult CopyFileAndMaybeWriteTimeModified(
    const base::FilePath& copy_from,
    base::Time expected_last_modified_copy_from,
    const base::FilePath& copy_to,
    int64_t offset,
    std::optional<int64_t> size,
    std::optional<base::Time> last_modified,
    bool flush_on_close,
    file_access::ScopedFileAccess) {
  // Do a full file copy if the sizes match and there is no offset.
  if (offset == 0) {
    base::File::Info info;
    base::GetFileInfo(copy_from, &info);
    if (!FileStreamReader::VerifySnapshotTime(expected_last_modified_copy_from,
                                              info)) {
      return mojom::WriteBlobToFileResult::kInvalidBlob;
    }
    if (!size || info.size == size.value()) {
      bool success = base::CopyFile(copy_from, copy_to);
      if (!success)
        return mojom::WriteBlobToFileResult::kIOError;
      if (last_modified && !base::TouchFile(copy_to, last_modified.value(),
                                            last_modified.value())) {
        return mojom::WriteBlobToFileResult::kTimestampError;
      }
      return mojom::WriteBlobToFileResult::kSuccess;
    }
  }

  // Do a manual file-to-file copy. This will overwrite the file if there
  // already is one.
  base::File infile =
      base::File(copy_from, base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::File outfile(copy_to,
                     base::File::FLAG_WRITE | base::File::FLAG_CREATE_ALWAYS);
  if (!outfile.IsValid())
    return mojom::WriteBlobToFileResult::kIOError;

  base::File::Info info;
  infile.GetInfo(&info);
  if (!FileStreamReader::VerifySnapshotTime(expected_last_modified_copy_from,
                                            info)) {
    return mojom::WriteBlobToFileResult::kInvalidBlob;
  }

  int64_t bytes_copied = 0;
  if (!CopyFileContentsWithOffsetAndSize(
          &infile, &outfile, &bytes_copied, offset,
          size.value_or(std::numeric_limits<int64_t>::max()))) {
    return mojom::WriteBlobToFileResult::kIOError;
  }
  if (size && bytes_copied != size.value())
    return mojom::WriteBlobToFileResult::kInvalidBlob;

  if (last_modified &&
      !outfile.SetTimes(last_modified.value(), last_modified.value())) {
    // If the file modification time isn't set correctly, then reading will
    // fail.
    return mojom::WriteBlobToFileResult::kTimestampError;
  }
  if (flush_on_close)
    outfile.Flush();
  outfile.Close();
  return mojom::WriteBlobToFileResult::kSuccess;
}

mojom::WriteBlobToFileResult CreateEmptyFileAndMaybeSetModifiedTime(
    base::FilePath file_path,
    std::optional<base::Time> last_modified,
    bool flush_on_write) {
  base::File file(file_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  bool file_success = file.created();
  if (!file_success) {
    return mojom::WriteBlobToFileResult::kIOError;
  }
  if (flush_on_write)
    file.Flush();
  file.Close();
  if (last_modified && !base::TouchFile(file_path, last_modified.value(),
                                        last_modified.value())) {
    // If the file modification time isn't set correctly, then reading
    // the blob later will fail. Thus, failing to save it is an error.
    file.Close();
    return mojom::WriteBlobToFileResult::kTimestampError;
  }
  return mojom::WriteBlobToFileResult::kSuccess;
}

void HandleModifiedTimeOnBlobFileWriteComplete(
    base::FilePath file_path,
    std::optional<base::Time> last_modified,
    bool flush_on_write,
    mojom::BlobStorageContext::WriteBlobToFileCallback callback,
    base::File::Error rv,
    int64_t bytes_written,
    FileWriterDelegate::WriteProgressStatus write_status) {
  bool success = write_status == FileWriterDelegate::SUCCESS_COMPLETED;
  if (!success) {
    std::move(callback).Run(mojom::WriteBlobToFileResult::kIOError);
    return;
  }
  if (success && !bytes_written) {
    // Special Case 1: Success but no bytes were written, so just create
    // an empty file (LocalFileStreamWriter only creates a file
    // if data is actually written).
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(CreateEmptyFileAndMaybeSetModifiedTime,
                       std::move(file_path), last_modified, flush_on_write),
        std::move(callback));
    return;
  } else if (success && last_modified) {
    // Special Case 2: Success and |last_modified| needs to be set. Set
    // that before reporting write completion.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(
            [](int64_t bytes_written, base::FilePath file_path,
               std::optional<base::Time> last_modified) {
              if (!base::TouchFile(file_path, last_modified.value(),
                                   last_modified.value())) {
                // If the file modification time isn't set correctly, then
                // reading the blob later will fail. Thus, failing to save it is
                // an error.
                return mojom::WriteBlobToFileResult::kTimestampError;
              }
              return mojom::WriteBlobToFileResult::kSuccess;
            },
            bytes_written, std::move(file_path), last_modified),
        std::move(callback));
    return;
  }
  std::move(callback).Run(mojom::WriteBlobToFileResult::kSuccess);
}

void PostCopyTaskToFileThreadIfAllowed(
    CopyCallback copy_cb,
    mojom::BlobStorageContext::WriteBlobToFileCallback callback,
    file_access::ScopedFileAccess scoped_file_access) {
  if (!scoped_file_access.is_allowed()) {
    std::move(callback).Run(mojom::WriteBlobToFileResult::kIOError);
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(std::move(copy_cb), std::move(scoped_file_access)),
      std::move(callback));
}

void WriteConstructedBlobToFile(
    std::unique_ptr<BlobDataHandle> blob_handle,
    const base::FilePath& file_path,
    bool flush_on_write,
    std::optional<base::Time> last_modified,
    mojom::BlobStorageContext::WriteBlobToFileCallback callback,
    BlobStatus status) {
  DCHECK(!last_modified || !last_modified.value().is_null());
  if (status != BlobStatus::DONE) {
    DCHECK(BlobStatusIsError(status));
    std::move(callback).Run(mojom::WriteBlobToFileResult::kInvalidBlob);
    return;
  }
  // Check if we can do a copy optimization.
  // TODO(dmurph): Optimize the case of IDB blobs, which have a type
  // kReadableDataHandle.
  std::unique_ptr<BlobDataSnapshot> snapshot = blob_handle->CreateSnapshot();
  const auto& items = snapshot->items();
  if (items.size() == 1) {
    const BlobDataItem& item = *items[0];
    if (item.type() == BlobDataItem::Type::kFile) {
      // The File API cannot handle uint64_t.
      std::optional<int64_t> optional_size = item.length();
      if (item.length() == blink::BlobUtils::kUnknownSize) {
        // The blob system uses a special value (max uint64_t) to denote an
        // unknown file size. This means the whole file should be copied.
        optional_size = std::nullopt;
      } else if (item.length() > std::numeric_limits<int64_t>::max()) {
        std::move(callback).Run(mojom::WriteBlobToFileResult::kError);
        return;
      }
      if (item.offset() > std::numeric_limits<int64_t>::max()) {
        std::move(callback).Run(mojom::WriteBlobToFileResult::kError);
        return;
      }

      base::OnceCallback<void(file_access::ScopedFileAccess)> post_copy_task =
          base::BindOnce(
              PostCopyTaskToFileThreadIfAllowed,
              base::BindOnce(CopyFileAndMaybeWriteTimeModified, item.path(),
                             item.expected_modification_time(), file_path,
                             item.offset(), std::move(optional_size),
                             std::move(last_modified), flush_on_write),
              std::move(callback));
      if (item.file_access()) {
        item.file_access().Run({item.path()}, std::move(post_copy_task));
      } else {
        std::move(post_copy_task).Run(file_access::ScopedFileAccess::Allowed());
      }
      return;
    }
  }

  // If not, copy the BlobReader and FileStreamWriter.
  std::unique_ptr<FileStreamWriter> writer =
      FileStreamWriter::CreateForLocalFile(
          base::ThreadPool::CreateTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE})
              .get(),
          file_path, /*initial_offset=*/0,
          FileStreamWriter::CREATE_NEW_FILE_ALWAYS);

  FlushPolicy policy = flush_on_write || last_modified
                           ? FlushPolicy::FLUSH_ON_COMPLETION
                           : FlushPolicy::NO_FLUSH_ON_COMPLETION;
  auto delegate =
      std::make_unique<FileWriterDelegate>(std::move(writer), policy);

  auto* raw_delegate = delegate.get();
  raw_delegate->Start(
      blob_handle->CreateReader(),
      IgnoreProgressWrapper(
          std::move(delegate),
          base::BindOnce(HandleModifiedTimeOnBlobFileWriteComplete, file_path,
                         std::move(last_modified), flush_on_write,
                         std::move(callback))));
}

}  // namespace

void WriteBlobToFile(
    std::unique_ptr<BlobDataHandle> blob_handle,
    const base::FilePath& file_path,
    bool flush_on_write,
    std::optional<base::Time> last_modified,
    mojom::BlobStorageContext::WriteBlobToFileCallback callback) {
  auto* blob_handle_ptr = blob_handle.get();
  blob_handle_ptr->RunOnConstructionComplete(base::BindOnce(
      &WriteConstructedBlobToFile, std::move(blob_handle), file_path,
      flush_on_write, last_modified, std::move(callback)));
}

}  // namespace storage
