// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_FILE_STREAM_WRITER_H_
#define STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_FILE_STREAM_WRITER_H_

#include <stdint.h>

#include <memory>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/task_runner_bound_observer_list.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"

namespace storage {

class FileSystemContext;
class FileStreamWriter;

class COMPONENT_EXPORT(STORAGE_BROWSER) SandboxFileStreamWriter
    : public FileStreamWriter {
 public:
  SandboxFileStreamWriter(FileSystemContext* file_system_context,
                          const FileSystemURL& url,
                          int64_t initial_offset,
                          const UpdateObserverList& observers);

  SandboxFileStreamWriter(const SandboxFileStreamWriter&) = delete;
  SandboxFileStreamWriter& operator=(const SandboxFileStreamWriter&) = delete;

  ~SandboxFileStreamWriter() override;

  // FileStreamWriter overrides.
  int Write(net::IOBuffer* buf,
            int buf_len,
            net::CompletionOnceCallback callback) override;
  int Cancel(net::CompletionOnceCallback callback) override;
  int Flush(FlushMode flush_mode,
            net::CompletionOnceCallback callback) override;

  // Used only by tests.
  void set_default_quota(int64_t quota) { default_quota_ = quota; }

 private:
  // Performs quota calculation and calls file_writer_->Write().
  // Will either return synchronously, or run asynchronously and call
  // |write_callback_|.
  int WriteInternal(net::IOBuffer* buf, int buf_len);

  // Callbacks that are chained for the first write.  This eventually calls
  // WriteInternal.
  void DidCreateSnapshotFile(net::CompletionOnceCallback callback,
                             base::File::Error file_error,
                             const base::File::Info& file_info,
                             const base::FilePath& platform_path,
                             scoped_refptr<ShareableFileReference> file_ref);
  void DidGetBucketSpaceRemaining(
      net::CompletionOnceCallback callback,
      storage::QuotaErrorOr<int64_t> space_remaining);
  void DidInitializeForWrite(net::IOBuffer* buf, int buf_len, int init_status);

  // Will call |write_callback_| if set, or return synchronously.
  void DidWrite(int write_response);

  void DidFlush(net::CompletionOnceCallback callback, int result);

  // Stops the in-flight operation, calls |cancel_callback_| and returns true
  // if there's a pending cancel request.
  bool CancelIfRequested();

  scoped_refptr<FileSystemContext> file_system_context_;
  FileSystemURL url_;
  int64_t initial_offset_;
  std::unique_ptr<FileStreamWriter> file_writer_;
  net::CompletionOnceCallback write_callback_;
  net::CompletionOnceCallback cancel_callback_;

  UpdateObserverList observers_;

  base::FilePath file_path_;
  int64_t file_size_;
  int64_t total_bytes_written_;
  int64_t allowed_bytes_to_write_;
  bool has_pending_operation_;

  int64_t default_quota_;

  base::WeakPtrFactory<SandboxFileStreamWriter> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_FILE_STREAM_WRITER_H_
