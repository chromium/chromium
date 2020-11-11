// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_io/native_io_file.h"

#include <utility>

#include "base/check.h"
#include "base/files/file.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
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
    HeapMojoRemote<mojom::blink::NativeIOFileHost> backend_file,
    ExecutionContext* execution_context)
    : file_state_(std::make_unique<FileState>(std::move(backing_file))),
      // TODO(pwnall): Get a dedicated queue when the specification matures.
      resolver_task_runner_(
          execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)),
      backend_file_(std::move(backend_file)) {
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
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The file was already closed");
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
                                      uint64_t length,
                                      ExceptionState& exception_state) {
  if (!base::IsValueInRangeForNumericType<int64_t>(length)) {
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
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The file was already closed");
    return ScriptPromise();
  }
  io_pending_ = true;

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  // Calls to base::File::SetLength() are routed through the browser process,
  // see crbug.com/1084565.
  //
  // We keep a single handle per file, so this handle is passed to the browser
  // process and is given back to the renderer afterwards.
  {
    WTF::MutexLocker locker(file_state_->mutex);
    backend_file_->SetLength(
        base::as_signed(length), std::move(file_state_->file),
        WTF::Bind(&NativeIOFile::DidSetLength, WrapPersistent(this),
                  WrapPersistent(resolver)));
  }

  return resolver->Promise();
}

ScriptPromise NativeIOFile::read(ScriptState* script_state,
                                 MaybeShared<DOMArrayBufferView> buffer,
                                 uint64_t file_offset,
                                 ExceptionState& exception_state) {
  if (!buffer.View()->IsShared()) {
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
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The file was already closed");
    return ScriptPromise();
  }
  io_pending_ = true;

  int read_size = OperationSize(*buffer.View());
  char* read_buffer =
      static_cast<char*>(buffer.View()->BaseAddressMaybeShared());
  DOMSharedArrayBuffer* read_buffer_keepalive = buffer.View()->BufferShared();

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
  if (!buffer.View()->IsShared()) {
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
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The file was already closed");
    return ScriptPromise();
  }
  io_pending_ = true;

  int write_size = OperationSize(*buffer.View());
  const char* write_data =
      static_cast<const char*>(buffer.View()->BaseAddressMaybeShared());
  DOMSharedArrayBuffer* read_buffer_keepalive = buffer.View()->BufferShared();

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
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The file was already closed");
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
  base::File::Error get_length_error = base::File::FILE_OK;
  int64_t length = -1;
  {
    WTF::MutexLocker mutex_locker(file_state->mutex);
    DCHECK(file_state->file.IsValid())
        << "file I/O operation queued after file closed";
    length = file_state->file.GetLength();
    if (length < 0) {
      get_length_error = file_state->file.GetLastFileError();
    }
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
  io_pending_ = false;

  DispatchQueuedClose();

  if (length < 0) {
    DCHECK(get_length_error != base::File::FILE_OK)
        << "Negative length reported with no error set";
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kOperationError,
        "getLength() failed"));
    return;
  }
  DCHECK(get_length_error == base::File::FILE_OK)
      << "File error reported when length is nonnegative";
  // getLength returns an unsigned integer, which is different from e.g.,
  // base::File and POSIX. The uses for negative integers are error handling,
  // which is done through exceptions, and seeking from an offset without type
  // conversions, which is not supported by NativeIO.
  resolver->Resolve(length);
}

void NativeIOFile::DidSetLength(ScriptPromiseResolver* resolver,
                                bool backend_success,
                                base::File backing_file) {
  DCHECK(backing_file.IsValid()) << "browser returned closed file";
  {
    WTF::MutexLocker locker(file_state_->mutex);
    file_state_->file = std::move(backing_file);
  }

  DCHECK(io_pending_) << "I/O operation performed without io_pending_ set";
  io_pending_ = false;

  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  if (!backend_success) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kUnknownError,
        "setLength() failed"));
    return;
  }

  resolver->Resolve();
}

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
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kOperationError,
        "read() failed"));
    return;
  }
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
  base::File::Error write_error;
  {
    WTF::MutexLocker mutex_locker(file_state->mutex);
    DCHECK(file_state->file.IsValid())
        << "file I/O operation queued after file closed";
    written_bytes = file_state->file.Write(file_offset, write_data, write_size);
    write_error = (written_bytes < 0) ? file_state->file.GetLastFileError()
                                      : base::File::FILE_OK;
  }

  PostCrossThreadTask(
      *resolver_task_runner, FROM_HERE,
      CrossThreadBindOnce(&NativeIOFile::DidWrite, std::move(native_io_file),
                          std::move(resolver), written_bytes, write_error));
}

void NativeIOFile::DidWrite(
    CrossThreadPersistent<ScriptPromiseResolver> resolver,
    int written_bytes,
    base::File::Error write_error) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  DCHECK(io_pending_) << "I/O operation performed without io_pending_ set";
  io_pending_ = false;

  DispatchQueuedClose();

  if (written_bytes < 0) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kOperationError,
        "write() failed"));
    return;
  }
  resolver->Resolve(written_bytes);
}

// static
void NativeIOFile::DoFlush(
    CrossThreadPersistent<NativeIOFile> native_io_file,
    CrossThreadPersistent<ScriptPromiseResolver> resolver,
    NativeIOFile::FileState* file_state,
    scoped_refptr<base::SequencedTaskRunner> resolver_task_runner) {
  DCHECK(!IsMainThread()) << "File I/O should not happen on the main thread";
  bool success = false;
  {
    WTF::MutexLocker mutex_locker(file_state->mutex);
    DCHECK(file_state->file.IsValid())
        << "file I/O operation queued after file closed";
    success = file_state->file.Flush();
  }

  PostCrossThreadTask(
      *resolver_task_runner, FROM_HERE,
      CrossThreadBindOnce(&NativeIOFile::DidFlush, std::move(native_io_file),
                          std::move(resolver), success));
}

void NativeIOFile::DidFlush(
    CrossThreadPersistent<ScriptPromiseResolver> resolver,
    bool success) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);

  DCHECK(io_pending_) << "I/O operation performed without io_pending_ set";
  io_pending_ = false;

  DispatchQueuedClose();

  if (!success) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kOperationError,
        "flush() failed"));
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
