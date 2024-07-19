// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_COPY_OR_MOVE_OPERATION_DELEGATE_H_
#define STORAGE_BROWSER_FILE_SYSTEM_COPY_OR_MOVE_OPERATION_DELEGATE_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/recursive_operation_delegate.h"

namespace net {
class DrainableIOBuffer;
class IOBufferWithSize;
}  // namespace net

namespace storage {

class FileStreamReader;
class FileStreamWriter;
enum class FlushPolicy;
enum class FlushMode;

// A delegate class for recursive copy or move operations.
class COMPONENT_EXPORT(STORAGE_BROWSER) CopyOrMoveOperationDelegate final
    : public RecursiveOperationDelegate {
 public:
  class CopyOrMoveImpl;

  using CopyOrMoveOptionSet = FileSystemOperation::CopyOrMoveOptionSet;
  using ErrorBehavior = FileSystemOperation::ErrorBehavior;

  enum OperationType { OPERATION_COPY, OPERATION_MOVE };

  // Helper to copy a file by reader and writer streams.
  // Export for testing.
  class COMPONENT_EXPORT(STORAGE_BROWSER) StreamCopyHelper {
   public:
    StreamCopyHelper(
        std::unique_ptr<FileStreamReader> reader,
        std::unique_ptr<FileStreamWriter> writer,
        FlushPolicy flush_policy,
        int buffer_size,
        FileSystemOperation::CopyFileProgressCallback file_progress_callback,
        const base::TimeDelta& min_progress_callback_invocation_span);

    StreamCopyHelper(const StreamCopyHelper&) = delete;
    StreamCopyHelper& operator=(const StreamCopyHelper&) = delete;

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
    void Flush(FlushMode flush_mode);
    void DidFlush(FlushMode flush_mode, int result);

    std::unique_ptr<FileStreamReader> reader_;
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
  };

  CopyOrMoveOperationDelegate(
      FileSystemContext* file_system_context,
      const FileSystemURL& src_root,
      const FileSystemURL& dest_root,
      OperationType operation_type,
      CopyOrMoveOptionSet options,
      ErrorBehavior error_behavior,
      std::unique_ptr<CopyOrMoveHookDelegate> copy_or_move_hook_delegate,
      StatusCallback callback);

  CopyOrMoveOperationDelegate(const CopyOrMoveOperationDelegate&) = delete;
  CopyOrMoveOperationDelegate& operator=(const CopyOrMoveOperationDelegate&) =
      delete;

  ~CopyOrMoveOperationDelegate() override;

  // RecursiveOperationDelegate overrides:
  void Run() override;
  void RunRecursively() override;
  void ProcessFile(const FileSystemURL& url, StatusCallback callback) override;
  void ProcessDirectory(const FileSystemURL& url,
                        StatusCallback callback) override;
  void PostProcessDirectory(const FileSystemURL& url,
                            StatusCallback callback) override;
  base::WeakPtr<RecursiveOperationDelegate> AsWeakPtr() override;

  // Posts a closure to run later but will not be run if `this` object is
  // destroyed.
  template <typename Functor, typename... Args>
  void RunCopyOrMoveHookDelegateCallbackLater(Functor&& functor,
                                              Args&&... args) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<CopyOrMoveOperationDelegate> delegate,
               base::OnceClosure closure) {
              if (!delegate) {
                return;
              }
              // Do not run the closure if delegate is gone as the
              // parameters to the closure are bound to the lifecycle
              // of delegate.
              std::move(closure).Run();
            },
            weak_factory_.GetWeakPtr(),
            base::BindOnce(std::forward<Functor>(functor),
                           base::Unretained(copy_or_move_hook_delegate_.get()),
                           std::forward<Args>(args)...)));
  }

  // Force a given source URL to produce an error for a copy or a
  // cross-filesystem move.
  static void SetErrorUrlForTest(const FileSystemURL* url);

 protected:
  void OnCancel() override;

 private:
  void DoProcessFile(const FileSystemURL& url,
                     FileSystemURL dest,
                     StatusCallback callback,
                     base::File::Error error);
  void DidCopyOrMoveFile(StatusCallback callback,
                         CopyOrMoveImpl* impl,
                         base::File::Error error);
  void DidTryRemoveDestRoot(StatusCallback callback, base::File::Error error);
  void ProcessDirectoryInternal(const FileSystemURL& src_url,
                                const FileSystemURL& dest_url,
                                StatusCallback callback,
                                base::File::Error error);
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
  void DidRemoveSourceForMove(const FileSystemURL& src_url,
                              StatusCallback callback,
                              base::File::Error error);

  void FinishOperation(base::File::Error error);

  FileSystemURL CreateDestURL(const FileSystemURL& src_url) const;

#if DCHECK_IS_ON()
  bool did_run_ = false;
#endif

  const FileSystemURL src_root_;
  const FileSystemURL dest_root_;
  bool same_file_system_;
  const OperationType operation_type_;
  const CopyOrMoveOptionSet options_;
  const ErrorBehavior error_behavior_;
  std::unique_ptr<CopyOrMoveHookDelegate> copy_or_move_hook_delegate_;
  StatusCallback callback_;

  std::map<CopyOrMoveImpl*, std::unique_ptr<CopyOrMoveImpl>> running_copy_set_;
  base::WeakPtrFactory<CopyOrMoveOperationDelegate> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_COPY_OR_MOVE_OPERATION_DELEGATE_H_
