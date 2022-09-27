// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_SYNC_ACCESS_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_SYNC_ACCESS_HANDLE_H_

#include "base/feature_list.h"
#include "base/sequence_checker.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_access_handle_host.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_read_write_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_file_delegate.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

class ScriptPromiseResolver;

class FileSystemSyncAccessHandle final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit FileSystemSyncAccessHandle(
      ExecutionContext* context,
      FileSystemAccessFileDelegate* file_delegate,
      mojo::PendingRemote<mojom::blink::FileSystemAccessAccessHandleHost>
          access_handle_host);

  FileSystemSyncAccessHandle(const FileSystemSyncAccessHandle&) = delete;
  FileSystemSyncAccessHandle& operator=(const FileSystemSyncAccessHandle&) =
      delete;

  // GarbageCollected
  void Trace(Visitor* visitor) const override;

  ScriptValue close(ScriptState*);

  ScriptValue flush(ScriptState*, ExceptionState&);

  ScriptValue getSize(ScriptState*, ExceptionState&);

  ScriptValue truncate(ScriptState*, uint64_t size, ExceptionState&);

  uint64_t read(MaybeShared<DOMArrayBufferView> buffer,
                FileSystemReadWriteOptions* options,
                ExceptionState&);

  uint64_t write(MaybeShared<DOMArrayBufferView> buffer,
                 FileSystemReadWriteOptions* options,
                 ExceptionState&);

 private:
  void CloseSync(ScriptState*);
  ScriptPromise CloseAsync(ScriptState*);
  void FlushSync(ScriptState*, ExceptionState&);
  ScriptPromise FlushAsync(ScriptState*);
  uint64_t GetSizeSync(ScriptState*, ExceptionState&);
  ScriptPromise GetSizeAsync(ScriptState*);
  void TruncateSync(ScriptState*, uint64_t size, ExceptionState&);
  ScriptPromise TruncateAsync(ScriptState*, uint64_t size);
  uint64_t DoRead(MaybeShared<DOMArrayBufferView> buffer,
                  FileSystemReadWriteOptions* options,
                  ExceptionState&);
  uint64_t DoWrite(MaybeShared<DOMArrayBufferView> buffer,
                   FileSystemReadWriteOptions* options,
                   ExceptionState&);
  void DispatchQueuedClose();

  // Must be called right before calling async methods on file_delegate.
  bool EnterOperation() {
    if (is_all_sync_interface_enabled_) {
      NOTREACHED();
      return false;
    }
    if (io_pending_)
      return false;
    io_pending_ = true;
    return true;
  }

  void ExitOperation() {
    if (is_all_sync_interface_enabled_) {
      NOTREACHED();
      return;
    }
    DCHECK(io_pending_);
    io_pending_ = false;
    DispatchQueuedClose();
  }

  FileSystemAccessFileDelegate* file_delegate() {
    DCHECK(io_pending_ || is_all_sync_interface_enabled_);
    return file_delegate_.Get();
  }

  SEQUENCE_CHECKER(sequence_checker_);

  // TODO(crbug.com/1338340): This method is only used for async methods.
  // Remove once we migrate all methods to be sync.
  //
  // The {OperationScope} is used to call {EnterOperation()} and
  // {ExitOperation()} in the synchronous functions {read} and {write}.
  // {OperationScope} calls {ExitOperation()} automatically in its destructor.
  class OperationScope {
    STACK_ALLOCATED();

   public:
    explicit OperationScope(FileSystemSyncAccessHandle* handle)
        : handle_(handle) {
      entered_operation_ = handle_->EnterOperation();
    }

    ~OperationScope() {
      if (entered_operation_) {
        handle_->ExitOperation();
      }
    }

    bool entered_operation() const { return entered_operation_; }

   private:
    FileSystemSyncAccessHandle* handle_;
    bool entered_operation_;
  };

  // Interface that provides file-like access to the backing storage.
  // The file delegate should only be accessed through the {file_delegate()}
  // getter.
  Member<FileSystemAccessFileDelegate> file_delegate_;

  // Mojo pipe that holds the renderer's write lock on the file.
  HeapMojoRemote<mojom::blink::FileSystemAccessAccessHandleHost>
      access_handle_remote_;

  // TODO(crbug.com/1338340): This method is only used for async methods.
  // Remove once we migrate all methods to be sync.
  //
  // True when an I/O operation other than close is underway.
  //
  // Set to {true} whenever an async operation is started, and back to {false}
  // when the operation resolves its promise.
  // All I/O operations throw an exception if they get called when
  // {io_pending_} is true, except for close(). This ensures that at most one
  // I/O operation is underway at any given time. When close() is called when
  // {io_pending_} is true, then the close() operation gets queued right after
  // the pending I/O operation.
  // {io_pending_} should only be set with the {EnterOperation()} and
  // {ExitOperation()} functions.
  bool io_pending_ = false;

  bool is_closed_ = false;

  // Whether all-sync interface feature is enabled and the async interface is
  // not force-enabled by enterprise policy, as indicated by the switch.
  const bool is_all_sync_interface_enabled_ =
      base::FeatureList::IsEnabled(
          blink::features::kSyncAccessHandleAllSyncSurface) &&
      !RuntimeEnabledFeatures::
          FileSystemSyncAccessHandleAsyncInterfaceOverrideEnabled();

  // crbug.com/1338340: Note that this is only used (and valid) when async
  // methods are in-use before the migration to the all-sync interface.
  //
  // Non-null when a close() I/O is queued behind another I/O operation.
  //
  // Set when close() is called while another I/O operation is underway. Cleared
  // when the queued close() operation is queued.
  Member<ScriptPromiseResolver> queued_close_resolver_;

  // Schedules resolving Promises with file I/O results.
  const scoped_refptr<base::SequencedTaskRunner> resolver_task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_SYNC_ACCESS_HANDLE_H_
