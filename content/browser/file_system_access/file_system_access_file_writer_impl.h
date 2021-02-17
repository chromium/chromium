// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_WRITER_IMPL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_WRITER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "components/download/public/common/quarantine_connection.h"
#include "components/download/quarantine/quarantine.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "content/browser/file_system_access/file_system_access_file_handle_impl.h"
#include "content/browser/file_system_access/file_system_access_handle_base.h"
#include "content/common/content_export.h"
#include "content/public/browser/file_system_access_permission_context.h"
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
  // If no |quarantine_connection_callback| is passed in no quarantine is done,
  // other than setting source information directly if on windows.
  // FileWriters should only be created via the FileSystemAccessManagerImpl.
  FileSystemAccessFileWriterImpl(
      FileSystemAccessManagerImpl* manager,
      base::PassKey<FileSystemAccessManagerImpl> pass_key,
      const BindingContext& context,
      const storage::FileSystemURL& url,
      const storage::FileSystemURL& swap_url,
      const SharedHandleState& handle_state,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessFileWriter> receiver,
      bool has_transient_user_activation,
      bool auto_close,
      download::QuarantineConnectionCallback quarantine_connection_callback);
  ~FileSystemAccessFileWriterImpl() override;

  const storage::FileSystemURL& swap_url() const { return swap_url_; }
  const base::WeakPtr<FileSystemAccessFileWriterImpl> weak_ptr() const {
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

  using HashCallback = base::OnceCallback<
      void(base::File::Error error, const std::string& hash, int64_t size)>;
  void ComputeHashForSwapFileForTesting(HashCallback callback) {
    ComputeHashForSwapFile(std::move(callback));
  }

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
  void DoAfterWriteCheck(base::File::Error hash_result,
                         const std::string& hash,
                         int64_t size);
  void DidAfterWriteCheck(
      FileSystemAccessPermissionContext::AfterWriteCheckResult result);
  void DidSwapFileSkipQuarantine(base::File::Error result);
  void DidSwapFileDoQuarantine(
      const storage::FileSystemURL& target_url,
      const GURL& referrer_url,
      mojo::Remote<quarantine::mojom::Quarantine> quarantine_remote,
      base::File::Error result);
  void DidAnnotateFile(
      mojo::Remote<quarantine::mojom::Quarantine> quarantine_remote,
      quarantine::mojom::QuarantineFileResult result);

  // After write and quarantine checks should apply to paths on all filesystems
  // except temporary file systems.
  // TOOD(crbug.com/1103076): Extend this check to non-native paths.
  bool RequireSecurityChecks() const {
    return url().type() != storage::kFileSystemTypeTemporary;
  }

  void ComputeHashForSwapFile(HashCallback callback);

  bool is_close_pending() const { return !close_callback_.is_null(); }

  // We write using this file URL. When `Close()` is invoked, we
  // execute a move operation from the swap URL to the target URL at `url_`. In
  // most filesystems, this move operation is atomic.
  storage::FileSystemURL swap_url_;

  CloseCallback close_callback_;

  download::QuarantineConnectionCallback quarantine_connection_callback_;

  // Keeps track of user activation state at creation time for after write
  // checks.
  bool has_transient_user_activation_ = false;

  // Changes will be written to the target file even if the stream isn't
  // explicitly closed.
  bool auto_close_ = false;

  base::WeakPtr<FileSystemAccessHandleBase> AsWeakPtr() override;

  base::WeakPtrFactory<FileSystemAccessFileWriterImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FileSystemAccessFileWriterImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_WRITER_IMPL_H_
