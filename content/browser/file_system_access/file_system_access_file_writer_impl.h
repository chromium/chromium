// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_WRITER_IMPL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_WRITER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "content/browser/file_system_access/file_system_access_file_handle_impl.h"
#include "content/browser/file_system_access/file_system_access_handle_base.h"
#include "content/browser/file_system_access/file_system_access_safe_move_helper.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_writer.mojom.h"

namespace content {

// This is the browser side implementation of the
// FileSystemAccessFileWriter mojom interface. Instances of this class are
// owned by the FileSystemAccessManagerImpl instance passed in to the
// constructor.
//
// This class is not thread safe, all methods must be called from the same
// sequence.
class CONTENT_EXPORT FileSystemAccessFileWriterImpl
    : public FileSystemAccessHandleBase,
      public blink::mojom::FileSystemAccessFileWriter {
 public:
  // Creates a FileWriter that writes in a swap file URL and
  // materializes the changes in the target file URL only after `Close`
  // is invoked and successfully completes. Assumes that swap_url represents a
  // file, and is valid.
  // If no `quarantine_connection_callback` is passed in no quarantine is done,
  // other than setting source information directly if on windows.
  // FileWriters should only be created via the FileSystemAccessManagerImpl.
  FileSystemAccessFileWriterImpl(
      FileSystemAccessManagerImpl* manager,
      base::PassKey<FileSystemAccessManagerImpl> pass_key,
      const BindingContext& context,
      const storage::FileSystemURL& url,
      const storage::FileSystemURL& swap_url,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> swap_lock,
      const SharedHandleState& handle_state,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessFileWriter> receiver,
      bool has_transient_user_activation,
      bool auto_close,
      download::QuarantineConnectionCallback quarantine_connection_callback);
  FileSystemAccessFileWriterImpl(const FileSystemAccessFileWriterImpl&) =
      delete;
  FileSystemAccessFileWriterImpl& operator=(
      const FileSystemAccessFileWriterImpl&) = delete;
  ~FileSystemAccessFileWriterImpl() override;

  const storage::FileSystemURL& swap_url() const { return swap_url_; }
  base::WeakPtr<FileSystemAccessFileWriterImpl> weak_ptr() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return weak_factory_.GetWeakPtr();
  }

  void Write(uint64_t offset,
             mojo::ScopedDataPipeConsumerHandle stream,
             WriteCallback callback) override;

  void Truncate(uint64_t length, TruncateCallback callback) override;
  // The writer will be destroyed upon completion.
  void Close(CloseCallback callback) override;
  // The writer will be destroyed upon completion.
  void Abort(AbortCallback callback) override;

 private:
  // State that is kept for the duration of a write operation, to keep track of
  // progress until the write completes.
  struct WriteState;

  mojo::Receiver<blink::mojom::FileSystemAccessFileWriter> receiver_;

  // If the mojo pipe is severed before either Close() or Abort() is invoked,
  // the transaction is aborted from the OnDisconnect method. Otherwise, the
  // writer will be destroyed upon completion of Close() or Abort().
  void OnDisconnect();

  // Destroys the file writer after calling the close callback.
  void CallCloseCallbackAndDeleteThis(
      blink::mojom::FileSystemAccessErrorPtr result);

  void WriteImpl(uint64_t offset,
                 mojo::ScopedDataPipeConsumerHandle stream,
                 WriteCallback callback);
  void DidWrite(WriteState* state,
                base::File::Error result,
                int64_t bytes,
                bool complete);
  void TruncateImpl(uint64_t length, TruncateCallback callback);
  void CloseImpl(CloseCallback callback);
  void AbortImpl(AbortCallback callback);
  void DidReplaceSwapFile(
      std::unique_ptr<content::FileSystemAccessSafeMoveHelper>
          file_system_access_safe_move_helper,
      blink::mojom::FileSystemAccessErrorPtr result);

  bool is_close_pending() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !close_callback_.is_null();
  }

  // We write using this file URL. When `Close()` is invoked, we
  // execute a move operation from the swap URL to the target URL at `url_`. In
  // most filesystems, this move operation is atomic.
  storage::FileSystemURL swap_url_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Lock on the target file. It is released on destruction.
  scoped_refptr<FileSystemAccessLockManager::LockHandle> lock_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Exclusive lock on the swap file. It is released on destruction.
  scoped_refptr<FileSystemAccessLockManager::LockHandle> swap_lock_
      GUARDED_BY_CONTEXT(sequence_checker_);

  CloseCallback close_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  download::QuarantineConnectionCallback quarantine_connection_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Keeps track of user activation state at creation time for after write
  // checks.
  bool has_transient_user_activation_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  // Changes will be written to the target file even if the stream isn't
  // explicitly closed.
  bool auto_close_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // The writer should not attempt to purge the swap file if the move operation
  // to the target file is successful, since this may incidentally remove the
  // active swap file of a different writer.
  bool should_purge_swap_file_on_destruction_
      GUARDED_BY_CONTEXT(sequence_checker_) = true;

  base::WeakPtr<FileSystemAccessHandleBase> AsWeakPtr() override;

  base::WeakPtrFactory<FileSystemAccessFileWriterImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_WRITER_IMPL_H_
