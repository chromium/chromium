// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_REGULAR_FILE_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_REGULAR_FILE_DELEGATE_H_

#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_capacity_allocation_host.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_capacity_tracker.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_file_delegate.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

#if BUILDFLAG(IS_MAC)
#include "third_party/blink/public/mojom/file/file_utilities.mojom-blink.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#endif  // BUILDFLAG(IS_MAC)

namespace blink {

// Non-incognito implementation of the FileSystemAccessFileDelegate. This class
// is a thin wrapper around an OS-level file descriptor.
//
// For async file operations, ownership of the file descriptor is passed to
// `task_runner_` to ensure it stays alive while being used. Ownership is passed
// back to the delegate once the operation completes. The delegate must not be
// used while there is an in-progress operation.
class FileSystemAccessRegularFileDelegate final
    : public FileSystemAccessFileDelegate {
 public:
  // Instances should only be constructed via
  // `FileSystemAccessFileDelegate::Create()`
  explicit FileSystemAccessRegularFileDelegate(
      ExecutionContext* context,
      base::File backing_file,
      int64_t backing_file_size,
      mojo::PendingRemote<mojom::blink::FileSystemAccessCapacityAllocationHost>
          capacity_allocation_host_remote,
      base::PassKey<FileSystemAccessFileDelegate>);

  FileSystemAccessRegularFileDelegate(
      const FileSystemAccessRegularFileDelegate&) = delete;
  FileSystemAccessRegularFileDelegate& operator=(
      const FileSystemAccessRegularFileDelegate&) = delete;

  void Trace(Visitor* visitor) const override {
    FileSystemAccessFileDelegate::Trace(visitor);
    visitor->Trace(capacity_tracker_);
#if BUILDFLAG(IS_MAC)
    visitor->Trace(context_);
    visitor->Trace(file_utilities_host_);
#endif  // BUILDFLAG(IS_MAC)
  }

  base::FileErrorOr<int> Read(int64_t offset,
                              base::span<uint8_t> data) override;
  base::FileErrorOr<int> Write(int64_t offset,
                               const base::span<uint8_t> data) override;
  base::FileErrorOr<int64_t> GetLength() override;
  void GetLengthAsync(
      base::OnceCallback<void(base::FileErrorOr<int64_t>)> callback) override;
  base::FileErrorOr<bool> SetLength(int64_t new_length) override;
  void SetLengthAsync(
      int64_t new_length,
      base::OnceCallback<void(base::File::Error)> callback) override;
  bool Flush() override;
  void FlushAsync(base::OnceCallback<void(bool)> callback) override;
  void Close() override;
  void CloseAsync(base::OnceClosure callback) override;
  bool IsValid() const override { return backing_file_.IsValid(); }

 private:
  // All async file operations perform I/O via a worker pool, off the main
  // thread. To keep `backing_file_` from being garbage-collected, ownership is
  // passed to the worker thread during the file operation. Concurrent file
  // operations is NOT supported.

  // Performs the file I/O part of getSize(), off the main thread.
  static void DoGetLength(
      CrossThreadHandle<FileSystemAccessRegularFileDelegate> delegate,
      CrossThreadOnceFunction<void(base::FileErrorOr<int64_t>)> callback,
      base::File file,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner);
  // Performs the post file I/O part of getSize(), on the main thread.
  void DidGetLength(
      CrossThreadOnceFunction<void(base::FileErrorOr<int64_t>)> callback,
      base::File file,
      base::FileErrorOr<int64_t> error_or_length);

  // Performs the file I/O part of truncate(), off the main thread.
  static void DoSetLength(
      CrossThreadHandle<FileSystemAccessRegularFileDelegate> delegate,
      CrossThreadOnceFunction<void(base::File::Error)> callback,
      base::File file,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      int64_t length);
  // Performs the post file I/O part of truncate(), on the main thread.
  void DidSetLength(CrossThreadOnceFunction<void(base::File::Error)> callback,
                    int64_t new_length,
                    base::File file,
                    base::File::Error error);

  // Performs the file I/O part of flush(), off the main thread.
  static void DoFlush(
      CrossThreadHandle<FileSystemAccessRegularFileDelegate> delegate,
      CrossThreadOnceFunction<void(bool)> callback,
      base::File file,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  // Performs the post file I/O part of flush(), on the main thread.
  void DidFlush(CrossThreadOnceFunction<void(bool)> callback,
                base::File file,
                bool success);

  // Performs the file I/O part of close(), off the main thread.
  static void DoClose(
      CrossThreadHandle<FileSystemAccessRegularFileDelegate> delegate,
      CrossThreadOnceClosure callback,
      base::File file,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  // Performs the post file I/O part of close(), on the main thread.
  void DidClose(CrossThreadOnceClosure callback, base::File file);

  // Called after preconditions for SetLength, including the requesting
  // additional capacity (if needed), have been performed.
  // If `request_capacity_result` is false, requesting capacity for the
  // operation failed.
  void DidCheckSetLengthCapacity(
      base::OnceCallback<void(base::File::Error)> callback,
      int64_t new_length,
      bool request_capacity_result);

#if BUILDFLAG(IS_MAC)
  // We need the FileUtilitiesHost only on Mac, where we have to execute
  // base::File::SetLength on the browser process, see crbug.com/1084565.
  // We need the context_ to create the instance of FileUtilitiesHost lazily.
  Member<ExecutionContext> context_;
  HeapMojoRemote<mojom::blink::FileUtilitiesHost> file_utilities_host_;
#endif  // BUILDFLAG(IS_MAC)

  // The file on disk backing the parent FileSystemFileHandle.
  base::File backing_file_;

  // Tracks the capacity for this file.
  Member<FileSystemAccessCapacityTracker> capacity_tracker_;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_REGULAR_FILE_DELEGATE_H_
