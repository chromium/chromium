// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_COPY_OR_MOVE_OPERATION_DELEGATE_H_
#define STORAGE_BROWSER_FILE_SYSTEM_COPY_OR_MOVE_OPERATION_DELEGATE_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "storage/browser/file_system/recursive_operation_delegate.h"

namespace net {
class DrainableIOBuffer;
class IOBufferWithSize;
}  // namespace net

namespace storage {
class FileStreamReader;
enum class FlushPolicy;
}  // namespace storage

namespace storage {

class FileStreamWriter;

// A delegate class for recursive copy or move operations.
class CopyOrMoveOperationDelegate : public RecursiveOperationDelegate {
 public:
  class CopyOrMoveImpl;
  using CopyProgressCallback = FileSystemOperation::CopyProgressCallback;
  using CopyOrMoveOption = FileSystemOperation::CopyOrMoveOption;
  using ErrorBehavior = FileSystemOperation::ErrorBehavior;

  enum OperationType { OPERATION_COPY, OPERATION_MOVE };

  // Helper to copy a file by reader and writer streams.
  // Export for testing.
  class COMPONENT_EXPORT(STORAGE_BROWSER) StreamCopyHelper {
   public:
    StreamCopyHelper(
        std::unique_ptr<storage::FileStreamReader> reader,
        std::unique_ptr<FileStreamWriter> writer,
        FlushPolicy flush_policy,
        int buffer_size,
        FileSystemOperation::CopyFileProgressCallback file_progress_callback,
        const base::TimeDelta& min_progress_callback_invocation_span);
    ~StreamCopyHelper();

    void Run(StatusCallback callback);

    // Requests cancelling. After the cancelling is done, |callback| passed to
    // Run will be called.
    void Cancel();

   private:
    // Reads the content from the |reader_|.
    void Read();
    void DidRead(int result);

    // Writes the content in |buffer| to |writer_|.
    void Write(scoped_refptr<net::DrainableIOBuffer> buffer);
    void DidWrite(scoped_refptr<net::DrainableIOBuffer> buffer, int result);

    // Flushes the written content in |writer_|.
    void Flush(bool is_eof);
    void DidFlush(bool is_eof, int result);

    std::unique_ptr<storage::FileStreamReader> reader_;
    std::unique_ptr<FileStreamWriter> writer_;
    const FlushPolicy flush_policy_;
    FileSystemOperation::CopyFileProgressCallback file_progress_callback_;
    StatusCallback completion_callback_;
    scoped_refptr<net::IOBufferWithSize> io_buffer_;
    int64_t num_copied_bytes_;
    int64_t previous_flush_offset_;
    base::Time last_progress_callback_invocation_time_;
    base::TimeDelta min_progress_callback_invocation_span_;
    bool cancel_requested_;
    base::WeakPtrFactory<StreamCopyHelper> weak_factory_{this};
    DISALLOW_COPY_AND_ASSIGN(StreamCopyHelper);
  };

  CopyOrMoveOperationDelegate(FileSystemContext* file_system_context,
                              const FileSystemURL& src_root,
                              const FileSystemURL& dest_root,
                              OperationType operation_type,
                              CopyOrMoveOption option,
                              ErrorBehavior error_behavior,
                              const CopyProgressCallback& progress_callback,
                              StatusCallback callback);
  ~CopyOrMoveOperationDelegate() override;

  // RecursiveOperationDelegate overrides:
  void Run() override;
  void RunRecursively() override;
  void ProcessFile(const FileSystemURL& url, StatusCallback callback) override;
  void ProcessDirectory(const FileSystemURL& url,
                        StatusCallback callback) override;
  void PostProcessDirectory(const FileSystemURL& url,
                            StatusCallback callback) override;

 protected:
  void OnCancel() override;

 private:
  void DidCopyOrMoveFile(const FileSystemURL& src_url,
                         const FileSystemURL& dest_url,
                         StatusCallback callback,
                         CopyOrMoveImpl* impl,
                         base::File::Error error);
  void DidTryRemoveDestRoot(StatusCallback callback, base::File::Error error);
  void ProcessDirectoryInternal(const FileSystemURL& src_url,
                                const FileSystemURL& dest_url,
                                StatusCallback callback);
  void DidCreateDirectory(const FileSystemURL& src_url,
                          const FileSystemURL& dest_url,
                          StatusCallback callback,
                          base::File::Error error);
  void PostProcessDirectoryAfterGetMetadata(const FileSystemURL& src_url,
                                            StatusCallback callback,
                                            base::File::Error error,
                                            const base::File::Info& file_info);
  void PostProcessDirectoryAfterTouchFile(const FileSystemURL& src_url,
                                          StatusCallback callback,
                                          base::File::Error error);
  void DidRemoveSourceForMove(StatusCallback callback, base::File::Error error);

  void OnCopyFileProgress(const FileSystemURL& src_url, int64_t size);
  FileSystemURL CreateDestURL(const FileSystemURL& src_url) const;

#if DCHECK_IS_ON()
  bool did_run_ = false;
#endif

  FileSystemURL src_root_;
  FileSystemURL dest_root_;
  bool same_file_system_;
  OperationType operation_type_;
  CopyOrMoveOption option_;
  ErrorBehavior error_behavior_;
  CopyProgressCallback progress_callback_;
  StatusCallback callback_;

  std::map<CopyOrMoveImpl*, std::unique_ptr<CopyOrMoveImpl>> running_copy_set_;
  base::WeakPtrFactory<CopyOrMoveOperationDelegate> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CopyOrMoveOperationDelegate);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_COPY_OR_MOVE_OPERATION_DELEGATE_H_
