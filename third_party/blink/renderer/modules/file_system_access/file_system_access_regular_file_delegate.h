// Copyright 2021 The Chromium Authors. All rights reserved.
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
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

#if defined(OS_MAC)
#include "third_party/blink/public/mojom/file/file_utilities.mojom-blink.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#endif  // defined(OS_MAC)

namespace blink {

// Non-incognito implementation of the FileSystemAccessFileDelegate. This class
// is a thin wrapper around an OS-level file descriptor.
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
#if defined(OS_MAC)
    visitor->Trace(context_);
    visitor->Trace(file_utilities_host_);
#endif  // defined(OS_MAC)
  }

  base::FileErrorOr<int> Read(int64_t offset,
                              base::span<uint8_t> data) override;
  base::FileErrorOr<int> Write(int64_t offset,
                               const base::span<uint8_t> data) override;

  void GetLength(
      base::OnceCallback<void(base::FileErrorOr<int64_t>)> callback) override;
  void SetLength(int64_t new_length,
                 base::OnceCallback<void(bool)> callback) override;

  void Flush(base::OnceCallback<void(bool)> callback) override;
  void Close(base::OnceClosure callback) override;

  bool IsValid() const override { return backing_file_.IsValid(); }

 private:
  static void DoGetLength(
      CrossThreadPersistent<FileSystemAccessRegularFileDelegate> delegate,
      CrossThreadOnceFunction<void(base::FileErrorOr<int64_t>)>
          wrapped_callback,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner);
  static void DoSetLength(
      CrossThreadPersistent<FileSystemAccessRegularFileDelegate> delegate,
      CrossThreadOnceFunction<void(bool)> wrapped_callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      int64_t length);
  static void DoFlush(
      CrossThreadPersistent<FileSystemAccessRegularFileDelegate> delegate,
      CrossThreadOnceFunction<void(bool)> wrapped_callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  static void DoClose(
      CrossThreadPersistent<FileSystemAccessRegularFileDelegate> delegate,
      CrossThreadOnceClosure wrapped_callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Called after preconditions for SetLength, including the requesting
  // additional capacity (if needed), have been performed.
  // If `request_capacity_result` is false, requesting capacity for the
  // operation failed.
  void DidCheckSetLengthCapacity(base::OnceCallback<void(bool)> callback,
                                 int64_t new_length,
                                 bool request_capacity_result);

  // Called after SetLength was successfully performed.
  void DidSuccessfulSetLength(int64_t new_length,
                              CrossThreadOnceFunction<void(bool)> callback);

#if defined(OS_MAC)
  void DidSetLengthIPC(base::OnceCallback<void(bool)> callback,
                       int64_t new_length,
                       base::File file,
                       bool result);

  // We need the FileUtilitiesHost only on Mac, where we have to execute
  // base::File::SetLength on the browser process, see crbug.com/1084565.
  // We need the context_ to create the instance of FileUtilitiesHost lazily.
  Member<ExecutionContext> context_;
  HeapMojoRemote<mojom::blink::FileUtilitiesHost> file_utilities_host_;
#endif  // defined(OS_MAC)

  // The file on disk backing the parent FileSystemFileHandle.
  base::File backing_file_;

  // Tracks the capacity for this file.
  Member<FileSystemAccessCapacityTracker> capacity_tracker_;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_REGULAR_FILE_DELEGATE_H_
