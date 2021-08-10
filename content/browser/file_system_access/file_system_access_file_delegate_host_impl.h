// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_DELEGATE_HOST_IMPL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_DELEGATE_HOST_IMPL_H_

#include "base/bind_post_task.h"
#include "components/services/storage/public/cpp/big_io_buffer.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_delegate_host.mojom.h"

namespace content {

// Browser side implementation of the FileSystemAccessFileDelegateHost mojom
// interface. Instances of this class are owned by the
// FileSystemAccessAccessHandleHostImpl instance of the associated URL, which
// constructs it.
class CONTENT_EXPORT FileSystemAccessFileDelegateHostImpl
    : public blink::mojom::FileSystemAccessFileDelegateHost {
 public:
  FileSystemAccessFileDelegateHostImpl(
      FileSystemAccessManagerImpl* manager,
      const storage::FileSystemURL& url,
      base::PassKey<FileSystemAccessAccessHandleHostImpl> pass_key,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessFileDelegateHost>
          receiver);
  ~FileSystemAccessFileDelegateHostImpl() override;

  // blink::mojom::FileSystemAccessFileDelegateHost:
  void Read(uint64_t offset,
            uint64_t bytes_to_read,
            ReadCallback callback) override;
  void Write(uint64_t offset,
             mojo::ScopedDataPipeConsumerHandle data,
             WriteCallback callback) override;
  void GetLength(GetLengthCallback callback) override;
  void SetLength(uint64_t length, SetLengthCallback callback) override;

 private:
  // State that is kept for the duration of a write operation, to keep track of
  // progress until the write completes.
  struct WriteState;

  // Copied from FileSystemAccessHandleBase.
  template <typename... MethodArgs,
            typename... ArgsMinusCallback,
            typename... CallbackArgs>
  void DoFileSystemOperation(
      const base::Location& from_here,
      storage::FileSystemOperationRunner::OperationID (
          storage::FileSystemOperationRunner::*method)(MethodArgs...),
      base::OnceCallback<void(CallbackArgs...)> callback,
      ArgsMinusCallback&&... args) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Wrap the passed in callback in one that posts a task back to the current
    // sequence.
    auto wrapped_callback = base::BindPostTask(
        base::SequencedTaskRunnerHandle::Get(), std::move(callback));

    // And then post a task to the sequence bound operation runner to run the
    // provided method with the provided arguments (and the wrapped callback).
    //
    // FileSystemOperationRunner assumes file_system_context() is kept alive, to
    // make sure this happens it is bound to a DoNothing callback.
    manager()
        ->operation_runner()
        .AsyncCall(base::IgnoreResult(method))
        .WithArgs(std::forward<ArgsMinusCallback>(args)...,
                  std::move(wrapped_callback))
        .Then(base::BindOnce(
            base::DoNothing::Once<scoped_refptr<storage::FileSystemContext>>(),
            base::WrapRefCounted(file_system_context())));
  }
  // Same as the previous overload, but using RepeatingCallback and
  // BindRepeating instead.
  template <typename... MethodArgs,
            typename... ArgsMinusCallback,
            typename... CallbackArgs>
  void DoFileSystemOperation(
      const base::Location& from_here,
      storage::FileSystemOperationRunner::OperationID (
          storage::FileSystemOperationRunner::*method)(MethodArgs...),
      base::RepeatingCallback<void(CallbackArgs...)> callback,
      ArgsMinusCallback&&... args) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Wrap the passed in callback in one that posts a task back to the current
    // sequence.
    auto wrapped_callback = base::BindRepeating(
        [](scoped_refptr<base::SequencedTaskRunner> runner,
           const base::RepeatingCallback<void(CallbackArgs...)>& callback,
           CallbackArgs... args) {
          runner->PostTask(
              FROM_HERE,
              base::BindOnce(callback, std::forward<CallbackArgs>(args)...));
        },
        base::SequencedTaskRunnerHandle::Get(), std::move(callback));

    // And then post a task to the sequence bound operation runner to run the
    // provided method with the provided arguments (and the wrapped callback).
    //
    // FileSystemOperationRunner assumes file_system_context() is kept alive, to
    // make sure this happens it is bound to a DoNothing callback.
    manager()
        ->operation_runner()
        .AsyncCall(base::IgnoreResult(method))
        .WithArgs(std::forward<ArgsMinusCallback>(args)...,
                  std::move(wrapped_callback))
        .Then(base::BindOnce(
            base::DoNothing::Once<scoped_refptr<storage::FileSystemContext>>(),
            base::WrapRefCounted(file_system_context())));
  }

  void OnDisconnect();

  FileSystemAccessManagerImpl* manager() { return manager_; }
  storage::FileSystemContext* file_system_context() {
    return manager()->context();
  }
  const storage::FileSystemURL& url() { return url_; }

  void DidRead(scoped_refptr<storage::BigIOBuffer> buffer,
               ReadCallback callback,
               int rv);
  void DidWrite(WriteState* state,
                base::File::Error result,
                int64_t bytes,
                bool complete);

  // This is safe, since the manager owns the
  // FileSystemAccessAccessHandleHostImpl which owns this class.
  FileSystemAccessManagerImpl* const manager_;
  const storage::FileSystemURL url_;

  mojo::Receiver<blink::mojom::FileSystemAccessFileDelegateHost> receiver_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FileSystemAccessFileDelegateHostImpl> weak_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_DELEGATE_HOST_IMPL_H_