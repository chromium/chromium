// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_io/native_io_file.h"

#include <utility>

#include "base/check.h"
#include "base/files/file.h"
#include "base/location.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/native_io/native_io_error.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

#if defined(OS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace blink {

namespace {

// Extracts the read/write operation size from the buffer size.
int OperationSize(const DOMArrayBufferView& buffer) {
  // On 32-bit platforms, clamp operation sizes to 2^31-1.
  return base::saturated_cast<int>(buffer.byteLength());
}

}  // namespace

struct NativeIOFile::FileState {
  explicit FileState(base::File file) : file(std::move(file)) {}

  FileState(const FileState&) = delete;
  FileState& operator=(const FileState&) = delete;

  ~FileState() = default;

  // Lock coordinating cross-thread access to the state.
  WTF::Mutex mutex;
  // The file on disk backing this NativeIOFile.
  //
  // The mutex is there to protect us against using the file after it was
  // closed, and against OS-specific behavior around concurrent file access. It
  // should never cause the main (JS) thread to block. This is because the mutex
  // is only taken on the main thread in CloseBackingFile(), which is called
  // when the NativeIOFile is destroyed (which implies there's no pending I/O
  // operation, because all I/O operations hold onto a Persistent<NativeIOFile>)
  // and when the mojo pipe is closed, which currently only happens when the JS
  // context is being torn down.
  //
  // TODO(rstz): Is it possible and worthwhile to remove the mutex and rely
  // exclusively on |NativeIOFile::io_pending_|, or remove
  // |NativeIOFile::io_pending_| in favor of the mutex (might be harder)?
  base::File file GUARDED_BY(mutex);
};

NativeIOFile::NativeIOFile(
    base::File backing_file,
    int64_t backing_file_length,
    HeapMojoRemote<mojom::blink::NativeIOFileHost> backend_file,
    NativeIOCapacityTracker* capacity_tracker,
    ExecutionContext* execution_context)
    : file_length_(backing_file_length),
      file_state_(std::make_unique<FileState>(std::move(backing_file))),
      // TODO(pwnall): Get a dedicated queue when the specification matures.
      resolver_task_runner_(
          execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)),
      backend_file_(std::move(backend_file)),
      capacity_tracker_(capacity_tracker) {
  DCHECK_GE(backing_file_length, 0);
  DCHECK(capacity_tracker);
  backend_file_.set_disconnect_handler(
      WTF::Bind(&NativeIOFile::OnBackendDisconnect, WrapWeakPersistent(this)));
}

NativeIOFile::~NativeIOFile() {
  // Needed to avoid having the base::File destructor close the file descriptor
  // synchronously on the main thread.
  CloseBackingFile();
}

ScriptPromise NativeIOFile::close(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  if (closed_) {
    // close() is idempotent.
    resolver->Resolve();
    return resolver->Promise();
  }

  closed_ = true;

  DCHECK(!queued_close_resolver_) << "Close logic kicked off twice";
  queued_close_resolver_ = resolver;

  if (!io_pending_) {
    // Pretend that a close() promise was queued behind an I/O operation, and
    // the operation just finished. This is less logic than handling the
    // non-queued case separately.
    DispatchQueuedClose();
  }

  return resolver->Promise();
}

ScriptPromise NativeIOFile::getLength(ScriptState* script_state,
                                      ExceptionState& exception_state) {
  if (io_pending_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Another I/O operation is in progress on the same file");
    return ScriptPromise();
  }
  if (closed_) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "The file was already closed"));
    return ScriptPromise();
  }
  io_pending_ = true;

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  // CrossThreadUnretained() is safe here because the NativeIOFile::FileState
  // instance is owned by this NativeIOFile, which is also passed to the task
  // via WrapCrossThreadPersistent. Therefore, the FileState instance is
  // guaranteed to remain alive during the task's execution.
  worker_pool::PostTask(
      FROM_HERE, {base::MayBlock(), base::ThreadPool()},
      CrossThreadBindOnce(&DoGetLength, WrapCrossThreadPersistent(this),
                          WrapCrossThreadPersistent(resolver),
                          CrossThreadUnretained(file_state_.get()),
                          resolver_task_runner_));
  return resolver->Promise();
}

ScriptPromise NativeIOFile::setLength(ScriptState* script_state,
                                      uint64_t new_length,
                                      ExceptionState& exception_state) {
  if (!base::IsValueInRangeForNumericType<int64_t>(new_length)) {
    // TODO(rstz): Consider throwing QuotaExceededError here.
    exception_state.ThrowTypeError("Quota exceeded.");
    return ScriptPromise();
  }
  if (io_pending_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Another I/O operation is in progress on the same file");
    return ScriptPromise();
  }
  if (closed_) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "The file was already closed"));
    return ScriptPromise();
  }
  int64_t expected_length = base::as_signed(new_length);

  DCHECK_GE(expected_length, 0);
  DCHECK_GE(file_length_, 0);
  static_assert(0 - std::numeric_limits<int32_t>::max() >=
                    std::numeric_limits<int32_t>::min(),
                "The `length_delta` computation below may overflow");
  // Since both values are positive, the arithmetic will not overflow.
  int64_t length_delta = expected_length - file_length_;

  // The available capacity must be reduced before performing an I/O operation
  // that increases the file length. The available capacity must not be
  // reduced before performing an I/O operation that decreases the file
  // length. The capacity tracker then reports at most
  // the capacity that is actually available to the execution context. This
  // prevents double-spending by concurrent I/O operations on different files.
  if (length_delta > 0) {
    if (!capacity_tracker_->ChangeAvailableCapacity(-length_delta)) {
      ThrowNativeIOWithError(exception_state,
                             mojom::blink::NativeIOError::New(
                                 mojom::blink::NativeIOErrorType::kNoSpace,
                                 "No capacity available for this operation"));
      return ScriptPromise();
    }
    file_length_ = expected_length;
  }
  io_pending_ = true;

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

#if defined(OS_MAC)
  // On macOS < 10.15, a sandboxing limitation causes failures in ftruncate()
  // syscalls issued from renderers. For this reason, base::File::SetLength()
  // fails in the renderer. We work around this problem by calling ftruncate()
  // in the browser process. See crbug.com/1084565.
  if (!base::mac::IsAtLeastOS10_15()) {
    // Our system has at most one handle to a file, so we can avoid reasoning
    // through the implications of multiple handles pointing to the same file.
    //
    // To preserve this invariant, we pass this file's handle to the browser
    // process during the SetLength() mojo call, and the browser passes it back
    // when the call completes.
    {
      WTF::MutexLocker locker(file_state_->mutex);
      backend_file_->SetLength(
          expected_length, std::move(file_state_->file),
          WTF::Bind(&NativeIOFile::DidSetLengthIpc, WrapPersistent(this),
                    WrapPersistent(resolver)));
    }
    return resolver->Promise();
  }
#endif  // defined(OS_MAC)

  // CrossThreadUnretained() is safe here because the NativeIOFile::FileState
  // instance is owned by this NativeIOFile, which is also passed to the task
  // via WrapCrossThreadPersistent. Therefore, the FileState instance is
  // guaranteed to remain alive during the task's execution.
  worker_pool::PostTask(
      FROM_HERE, {base::MayBlock(), base::ThreadPool()},
      CrossThreadBindOnce(&DoSetLength, WrapCrossThreadPersistent(this),
                          WrapCrossThreadPersistent(resolver),
                          CrossThreadUnretained(file_state_.get()),
                          resolver_task_runner_, expected_length));
  return resolver->Promise();
}

ScriptPromise NativeIOFile::read(ScriptState* script_state,
                                 MaybeShared<DOMArrayBufferView> buffer,
                                 uint64_t file_offset,
                                 ExceptionState& exception_state) {
  if (!buffer->IsShared()) {
    exception_state.ThrowTypeError(
        "The I/O buffer must be backed by a SharedArrayBuffer");
    return ScriptPromise();
  }

  if (io_pending_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Another I/O operation is in progress on the same file");
    return ScriptPromise();
  }
  if (closed_) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "The file was already closed"));
    return ScriptPromise();
  }
  io_pending_ = true;

  int read_size = OperationSize(*buffer);
  char* read_buffer = static_cast<char*>(buffer->BaseAddressMaybeShared());
  DOMSharedArrayBuffer* read_buffer_keepalive = buffer->BufferShared();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  // The first CrossThreadUnretained() is safe here because the
  // NativeIOFile::FileState instance is owned by this NativeIOFile, which is
  // also passed to the task via WrapCrossThreadPersistent. Therefore, the
  // FileState instance is guaranteed to remain alive during the task's
  // execution.
  //
  // The second CrossThreadUnretained() is safe here because the read buffer is
  // backed by a DOMSharedArrayBuffer that is also passed to the task via
  // WrapCrossThreadPersistent. Therefore, the buffer is guaranteed to remain
  // alive during the task's execution.
  worker_pool::PostTask(
      FROM_HERE, {base::MayBlock(), base::ThreadPool()},
      CrossThreadBindOnce(
          &DoRead, WrapCrossThreadPersistent(this),
          WrapCrossThreadPersistent(resolver),
          WrapCrossThreadPersistent(read_buffer_keepalive),
          CrossThreadUnretained(file_state_.get()), resolver_task_runner_,
          CrossThreadUnretained(read_buffer), file_offset, read_size));
  return resolver->Promise();
}

ScriptPromise NativeIOFile::write(ScriptState* script_state,
                                  MaybeShared<DOMArrayBufferView> buffer,
                                  uint64_t file_offset,
                                  ExceptionState& exception_state) {
  if (!buffer->IsShared()) {
    exception_state.ThrowTypeError(
        "The I/O buffer must be backed by a SharedArrayBuffer");
    return ScriptPromise();
  }

  if (io_pending_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Another I/O operation is in progress on the same file");
    return ScriptPromise();
  }
  if (closed_) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "The file was already closed"));
    return ScriptPromise();
  }

  int write_size = OperationSize(*buffer);
  int64_t write_end_offset;
  if (!base::CheckAdd(file_offset, write_size)
           .AssignIfValid(&write_end_offset)) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kNoSpace,
                               "No capacity available for this operation"));
    return ScriptPromise();
  }

  DCHECK_GE(write_end_offset, 0);
  DCHECK_GE(file_length_, 0);
  static_assert(0 - std::numeric_limits<int32_t>::max() >=
                    std::numeric_limits<int32_t>::min(),
                "The `length_delta` computation below may overflow");
  // Since both values are positive, the arithmetic will not overflow.
  int64_t length_delta = write_end_offset - file_length_;
  // The available capacity must be reduced before performing an I/O operation
  // that increases the file length. This prevents double-spending by concurrent
  // I/O operations on different files.
  if (length_delta > 0) {
    if (!capacity_tracker_->ChangeAvailableCapacity(-length_delta)) {
      ThrowNativeIOWithError(exception_state,
                             mojom::blink::NativeIOError::New(
                                 mojom::blink::NativeIOErrorType::kNoSpace,
                                 "No capacity available for this operation"));
      return ScriptPromise();
    }
    file_length_ = write_end_offset;
  }

  io_pending_ = true;

  const char* write_data =
      static_cast<const char*>(buffer->BaseAddressMaybeShared());
  DOMSharedArrayBuffer* read_buffer_keepalive = buffer->BufferShared();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  // The first CrossThreadUnretained() is safe here because the
  // NativeIOFile::FileState instance is owned by this NativeIOFile, which is
  // also passed to the task via WrapCrossThreadPersistent. Therefore, the
  // FileState instance is guaranteed to remain alive during the task's
  // execution.
  //
  // The second CrossThreadUnretained() is safe here because the write data is
  // backed by a DOMSharedArrayBuffer that is also passed to the task via
  // WrapCrossThreadPersistent. Therefore, the data is guaranteed to remain
  // alive during the task's execution.
  worker_pool::PostTask(
      FROM_HERE, {base::MayBlock(), base::ThreadPool()},
      CrossThreadBindOnce(
          &DoWrite, WrapCrossThreadPersistent(this),
          WrapCrossThreadPersistent(resolver),
          WrapCrossThreadPersistent(read_buffer_keepalive),
          CrossThreadUnretained(file_state_.get()), resolver_task_runner_,
          CrossThreadUnretained(write_data), file_offset, write_size));
  return resolver->Promise();
}

ScriptPromise NativeIOFile::flush(ScriptState* script_state,
                                  ExceptionState& exception_state) {
  // This implementation of flush attempts to physically store the data it has
  // written on disk. This behaviour might change in the future in order to
  // support more performant but less reliable persistency guarantees.
  if (io_pending_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Another I/O operation is in progress on the same file");
    return ScriptPromise();
  }
  if (closed_) {
    ThrowNativeIOWithError(exception_state,
                           mojom::blink::NativeIOError::New(
                               mojom::blink::NativeIOErrorType::kInvalidState,
                               "The file was already closed"));
    return ScriptPromise();
  }
  io_pending_ = true;

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  // CrossThreadUnretained() is safe here because the NativeIOFile::FileState
  // instance is owned by this NativeIOFile, which is also passed to the task
  // via WrapCrossThreadPersistent. Therefore, the FileState instance is
  // guaranteed to remain alive during the task's execution.
  worker_pool::PostTask(
      FROM_HERE, {base::MayBlock(), base::ThreadPool()},
      CrossThreadBindOnce(&DoFlush, WrapCrossThreadPersistent(this),
                          WrapCrossThreadPersistent(resolver),
                          CrossThreadUnretained(file_state_.get()),
                          resolver_task_runner_));
  return resolver->Promise();
}

void NativeIOFile::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(queued_close_resolver_);
  visitor->Trace(backend_file_);
  visitor->Trace(capacity_tracker_);
}

void NativeIOFile::OnBackendDisconnect() {
  backend_file_.reset();
  CloseBackingFile();
}

void NativeIOFile::DispatchQueuedClose() {
  DCHECK(!io_pending_)
      << "Dispatching close() concurrently with other I/O operations is racy";

  if (!queued_close_resolver_)
    return;

  DCHECK(closed_) << "close() resolver queued without setting closed_";
  ScriptPromiseResolver* resolver = queued_close_resolver_;
  queued_close_resolver_ = nullptr;

  worker_pool::PostTask(
      FROM_HERE, {base::MayBlock(), base::ThreadPool()},
      CrossThreadBindOnce(&DoClose, WrapCrossThreadPersistent(this),
                          WrapCrossThreadPersistent(resolver),
                          CrossThreadUnretained(file_state_.get()),
                          resolver_task_runner_));
}

// static
void NativeIOFile::DoClose(
    CrossThreadPersistent<NativeIOFile> native_io_file,
    CrossThreadPersistent<ScriptPromiseResolver> resolver,
    NativeIOFile::FileState* file_state,
    scoped_refptr<base::SequencedTaskRunner> resolver_task_runner) {
  DCHECK(!IsMainThread()) << "File I/O should not happen on the main thread";

  {
    WTF::MutexLocker locker(file_state->mutex);
    DCHECK(file_state->file.IsValid())
        << "file I/O operation queued after file closed";
    file_state->file.Close();
  }

  PostCrossThreadTask(
      *resolver_task_runner, FROM_HERE,
      CrossThreadBindOnce(&NativeIOFile::DidClose, std::move(native_io_file),
                          std::move(resolver)));
}

void NativeIOFile::DidClose(
    CrossThreadPersistent<ScriptPromiseResolver> resolver) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid()) {
    // If the context was torn down, the backend is disconnecting or
    // disconnected. No need to report that the file is closing.
    return;
  }

  if (!backend_file_.is_bound()) {
    // If the backend went away, no need to tell it that the file was closed.
    resolver->Resolve();
    return;
  }
  backend_file_->Close(
      WTF::Bind([](ScriptPromiseResolver* resolver) { resolver->Resolve(); },
                WrapPersistent(resolver.Get())));
}

// static
void NativeIOFile::DoGetLength(
    CrossThreadPersistent<NativeIOFile> native_io_file,
    CrossThreadPersistent<ScriptPromiseResolver> resolver,
    NativeIOFile::FileState* file_state,
    scoped_refptr<base::SequencedTaskRunner> resolver_task_runner) {
  DCHECK(!IsMainThread()) << "File I/O should not happen on the main thread";
  base::File::Error get_length_error;
  int64_t length = -1;
  {
    WTF::MutexLocker mutex_locker(file_state->mutex);
    DCHECK(file_state->file.IsValid())
        << "file I/O operation queued after file closed";
    length = file_state->file.GetLength();
    get_length_error = (length < 0) ? file_state->file.GetLastFileError()
                                    : base::File::FILE_OK;
  }

  PostCrossThreadTask(
      *resolver_task_runner, FROM_HERE,
      CrossThreadBindOnce(&NativeIOFile::DidGetLength,
                          std::move(native_io_file), std::move(resolver),
                          length, get_length_error));
}

void NativeIOFile::DidGetLength(
    CrossThreadPersistent<ScriptPromiseResolver> resolver,
    int64_t length,
    base::File::Error get_length_error) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  DCHECK(io_pending_) << "I/O operation performed without io_pending_ set";
  if (get_length_error == base::File::FILE_OK) {
    DCHECK_EQ(file_length_, length)
        << "`file_length_` is not an upper bound anymore";
  }

  io_pending_ = false;

  DispatchQueuedClose();

  if (length < 0) {
    DCHECK_NE(get_length_error, base::File::FILE_OK)
        << "Negative length reported with no error set";
    blink::RejectNativeIOWithError(resolver, get_length_error);
    return;
  }
  DCHECK_EQ(get_length_error, base::File::FILE_OK)
      << "File error reported when length is nonnegative";
  // getLength returns an unsigned integer, which is different from e.g.,
  // base::File and POSIX. The uses for negative integers are error handling,
  // which is done through exceptions, and seeking from an offset without type
  // conversions, which is not supported by NativeIO.
  resolver->Resolve(length);
}

// static
void NativeIOFile::DoSetLength(
    CrossThreadPersistent<NativeIOFile> native_io_file,
    CrossThreadPersistent<ScriptPromiseResolver> resolver,
    NativeIOFile::FileState* file_state,
    scoped_refptr<base::SequencedTaskRunner> resolver_task_runner,
    int64_t expected_length) {
  DCHECK(!IsMainThread()) << "File I/O should not happen on the main thread";

  base::File::Error set_length_error;
  int64_t actual_length;
  {
    WTF::MutexLocker mutex_locker(file_state->mutex);
    DCHECK(file_state->file.IsValid())
        << "file I/O operation queued after file closed";
    bool success = file_state->file.SetLength(expected_length);
    set_length_error =
        success ? base::File::FILE_OK : file_state->file.GetLastFileError();
    actual_length = success ? expected_length : file_state->file.GetLength();
  }

  PostCrossThreadTask(
      *resolver_task_runner, FROM_HERE,
      CrossThreadBindOnce(&NativeIOFile::DidSetLengthIo,
                          std::move(native_io_file), std::move(resolver),
                          actual_length, set_length_error));
}

void NativeIOFile::DidSetLengthIo(
    CrossThreadPersistent<ScriptPromiseResolver> resolver,
    int64_t actual_length,
    base::File::Error set_length_error) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  DCHECK(io_pending_) << "I/O operation performed without io_pending_ set";
  io_pending_ = false;

  if (actual_length >= 0) {
    DCHECK_LE(actual_length, file_length_)
        << "file_length_ should be an upper bound during I/O";
    if (actual_length < file_length_) {
      // For successful length decreases, this logic returns freed up
      // capacity. For unsuccessful length increases, this logic returns
      // unused capacity.
      bool change_success = capacity_tracker_->ChangeAvailableCapacity(
          file_length_ - actual_length);
      DCHECK(change_success) << "Capacity increases should always succeed";
      file_length_ = actual_length;
    }
  } else {
    DCHECK(set_length_error != base::File::FILE_OK);
    // base::File::SetLength() failed. Then, attempting to File::GetLength()
    // failed as well. We don't have a reliable measure of the file's length,
    // and the file descriptor is probably unusable. Force-closing the file
    // without reclaiming any capacity minimizes the risk of overusing our
    // allocation.
    if (!closed_) {
      closed_ = true;
      queued_close_resolver_ =
          MakeGarbageCollected<ScriptPromiseResolver>(script_state);
    }
  }

  DispatchQueuedClose();

  if (set_length_error != base::File::FILE_OK) {
    RejectNativeIOWithError(resolver, set_length_error);
    return;
  }
  resolver->Resolve();
}

#if defined(OS_MAC)
void NativeIOFile::DidSetLengthIpc(
    ScriptPromiseResolver* resolver,
    base::File backing_file,
    int64_t actual_length,
    mojom::blink::NativeIOErrorPtr set_length_error) {
  DCHECK(backing_file.IsValid()) << "browser returned closed file";
  {
    WTF::MutexLocker locker(file_state_->mutex);
    file_state_->file = std::move(backing_file);
  }
  ScriptState* script_state = resolver->GetScriptState();

  DCHECK(io_pending_) << "I/O operation performed without io_pending_ set";
  io_pending_ = false;

  if (actual_length >= 0) {
    DCHECK_LE(actual_length, file_length_)
        << "file_length_ should be an upper bound during I/O";
    if (actual_length < file_length_) {
      // For successful length decreases, this logic returns freed up
      // capacity. For unsuccessful length increases, this logic returns
      // unused capacity.
      bool change_success = capacity_tracker_->ChangeAvailableCapacity(
          file_length_ - actual_length);
      DCHECK(change_success) << "Capacity increases should always succeed";
      file_length_ = actual_length;
    }
  } else {
    DCHECK(set_length_error->type != mojom::blink::NativeIOErrorType::kSuccess);
    // base::File::SetLength() failed. Then, attempting to File::GetLength()
    // failed as well. We don't have a reliable measure of the file's length,
    // and the file descriptor is probably unusable. Force-closing the file
    // without reclaiming any capacity minimizes the risk of overusing our
    // allocation.
    if (!closed_) {
      closed_ = true;
      queued_close_resolver_ =
          MakeGarbageCollected<ScriptPromiseResolver>(script_state);
    }
  }

  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  DispatchQueuedClose();

  if (set_length_error->type != mojom::blink::NativeIOErrorType::kSuccess) {
    blink::RejectNativeIOWithError(resolver, std::move(set_length_error));
    return;
  }

  resolver->Resolve();
}
#endif  // defined(OS_MAC)

// static
void NativeIOFile::DoRead(
    CrossThreadPersistent<NativeIOFile> native_io_file,
    CrossThreadPersistent<ScriptPromiseResolver> resolver,
    CrossThreadPersistent<DOMSharedArrayBuffer> read_buffer_keepalive,
    NativeIOFile::FileState* file_state,
    scoped_refptr<base::SequencedTaskRunner> resolver_task_runner,
    char* read_buffer,
    uint64_t file_offset,
    int read_size) {
  DCHECK(!IsMainThread()) << "File I/O should not happen on the main thread";

  int read_bytes;
  base::File::Error read_error;
  {
    WTF::MutexLocker mutex_locker(file_state->mutex);
    DCHECK(file_state->file.IsValid())
        << "file I/O operation queued after file closed";
    read_bytes = file_state->file.Read(file_offset, read_buffer, read_size);
    read_error = (read_bytes < 0) ? file_state->file.GetLastFileError()
                                  : base::File::FILE_OK;
  }

  PostCrossThreadTask(
      *resolver_task_runner, FROM_HERE,
      CrossThreadBindOnce(&NativeIOFile::DidRead, std::move(native_io_file),
                          std::move(resolver), read_bytes, read_error));
}

void NativeIOFile::DidRead(
    CrossThreadPersistent<ScriptPromiseResolver> resolver,
    int read_bytes,
    base::File::Error read_error) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  DCHECK(io_pending_) << "I/O operation performed without io_pending_ set";
  io_pending_ = false;

  DispatchQueuedClose();

  if (read_bytes < 0) {
    DCHECK_NE(read_error, base::File::FILE_OK)
        << "Negative bytes read reported with no error set";
    blink::RejectNativeIOWithError(resolver, read_error);
    return;
  }
  DCHECK_EQ(read_error, base::File::FILE_OK)
      << "Error set but positive number of bytes read.";
  resolver->Resolve(read_bytes);
}

// static
void NativeIOFile::DoWrite(
    CrossThreadPersistent<NativeIOFile> native_io_file,
    CrossThreadPersistent<ScriptPromiseResolver> resolver,
    CrossThreadPersistent<DOMSharedArrayBuffer> write_data_keepalive,
    NativeIOFile::FileState* file_state,
    scoped_refptr<base::SequencedTaskRunner> resolver_task_runner,
    const char* write_data,
    uint64_t file_offset,
    int write_size) {
  DCHECK(!IsMainThread()) << "File I/O should not happen on the main thread";

  int written_bytes;
  int64_t actual_file_length_on_failure = 0;
  base::File::Error write_error;
  {
    WTF::MutexLocker mutex_locker(file_state->mutex);
    DCHECK(file_state->file.IsValid())
        << "file I/O operation queued after file closed";
    written_bytes = file_state->file.Write(file_offset, write_data, write_size);
    write_error = (written_bytes < 0) ? file_state->file.GetLastFileError()
                                      : base::File::FILE_OK;
    if (written_bytes < write_size || write_error != base::File::FILE_OK) {
      actual_file_length_on_failure = file_state->file.GetLength();
      if (actual_file_length_on_failure < 0 &&
          write_error != base::File::FILE_OK) {
        write_error = file_state->file.GetLastFileError();
      }
    }
  }

  PostCrossThreadTask(
      *resolver_task_runner, FROM_HERE,
      CrossThreadBindOnce(&NativeIOFile::DidWrite, std::move(native_io_file),
                          std::move(resolver), written_bytes, write_error,
                          write_size, actual_file_length_on_failure));
}

void NativeIOFile::DidWrite(
    CrossThreadPersistent<ScriptPromiseResolver> resolver,
    int written_bytes,
    base::File::Error write_error,
    int write_size,
    int64_t actual_file_length_on_failure) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  DCHECK(io_pending_) << "I/O operation performed without io_pending_ set";
  io_pending_ = false;

  if (write_error != base::File::FILE_OK || written_bytes < write_size) {
    if (actual_file_length_on_failure >= 0) {
      DCHECK_LE(actual_file_length_on_failure, file_length_)
          << "file_length_ should be an upper bound during I/O";
      if (actual_file_length_on_failure < file_length_) {
        bool change_success = capacity_tracker_->ChangeAvailableCapacity(
            file_length_ - actual_file_length_on_failure);
        DCHECK(change_success) << "Capacity increases should always succeed";
        file_length_ = actual_file_length_on_failure;
      }
    } else {
      DCHECK(write_error != base::File::FILE_OK);
      // base::File::Write() failed. Then, attempting to File::GetLength()
      // failed as well. We don't have a reliable measure of the file's length,
      // and the file descriptor is probably unusable. Force-closing the file
      // without reclaiming any capacity minimizes the risk of overusing our
      // allocation.
      if (!closed_) {
        closed_ = true;
        queued_close_resolver_ =
            MakeGarbageCollected<ScriptPromiseResolver>(script_state);
      }
    }
  }

  DispatchQueuedClose();

  if (write_error != base::File::FILE_OK) {
    blink::RejectNativeIOWithError(resolver, write_error);
    return;
  }
  DCHECK_EQ(write_error, base::File::FILE_OK);

  resolver->Resolve(written_bytes);
}

// static
void NativeIOFile::DoFlush(
    CrossThreadPersistent<NativeIOFile> native_io_file,
    CrossThreadPersistent<ScriptPromiseResolver> resolver,
    NativeIOFile::FileState* file_state,
    scoped_refptr<base::SequencedTaskRunner> resolver_task_runner) {
  DCHECK(!IsMainThread()) << "File I/O should not happen on the main thread";
  base::File::Error flush_error;
  {
    WTF::MutexLocker mutex_locker(file_state->mutex);
    DCHECK(file_state->file.IsValid())
        << "file I/O operation queued after file closed";
    bool success = file_state->file.Flush();
    flush_error =
        success ? base::File::FILE_OK : file_state->file.GetLastFileError();
  }

  PostCrossThreadTask(
      *resolver_task_runner, FROM_HERE,
      CrossThreadBindOnce(&NativeIOFile::DidFlush, std::move(native_io_file),
                          std::move(resolver), flush_error));
}

void NativeIOFile::DidFlush(
    CrossThreadPersistent<ScriptPromiseResolver> resolver,
    base::File::Error flush_error) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  DCHECK(io_pending_) << "I/O operation performed without io_pending_ set";
  io_pending_ = false;

  DispatchQueuedClose();

  if (flush_error != base::File::FILE_OK) {
    blink::RejectNativeIOWithError(resolver, flush_error);
    return;
  }
  resolver->Resolve();
}

void NativeIOFile::CloseBackingFile() {
  closed_ = true;
  file_state_->mutex.lock();
  base::File backing_file = std::move(file_state_->file);
  file_state_->mutex.unlock();

  if (!backing_file.IsValid()) {
    // Avoid posting a cross-thread task if the file is already closed. This is
    // the expected path.
    return;
  }

  worker_pool::PostTask(
      FROM_HERE, {base::MayBlock(), base::ThreadPool()},
      CrossThreadBindOnce([](base::File file) { file.Close(); },
                          std::move(backing_file)));
}

}  // namespace blink
