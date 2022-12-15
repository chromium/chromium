// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_io/native_io_file.h"

#include <utility>

#include "base/check.h"
#include "base/files/file.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/checked_math.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_native_io_read_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_native_io_write_result.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/native_io/native_io_error.h"
#include "third_party/blink/renderer/modules/native_io/native_io_file_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace blink {

// State and logic for performing file I/O off the JavaScript thread.
//
// Instances are allocated on the PartitionAlloc heap. Instances cannot be
// garbage-collected, because garbage collected heaps get deallocated when the
// underlying threads are terminated, and we need a guarantee that each
// instance remains alive while it is used by a thread performing file I/O.
//
// Instances are initially constructed on a Blink thread that executes
// JavaScript, which can be Blink's main thread, or a worker thread. Afterwards,
// instances are (mostly*) only accessed on dedicated threads that do blocking
// file I/O.
//
// Mostly*: On macOS < 10.15, SetLength() synchronously accesses FileState on
// the JavaScript thread. This could be fixed with extra thread hopping. We're
// not currently planning to invest in the fix.
class NativeIOFile::FileState
    : public base::RefCountedThreadSafe<NativeIOFile::FileState> {
 public:
  explicit FileState(base::File file) : file_(std::move(file)) {
    DCHECK(file_.IsValid());
  }

  FileState(const FileState&) = delete;
  FileState& operator=(const FileState&) = delete;

  ~FileState() = default;

  // Returns true until Close() is called. Returns false afterwards.
  //
  // On macOS < 10.15, returns false between a TakeFile() call and the
  // corresponding SetFile() call.
  bool IsValid() {
    DCHECK(!IsMainThread());

    base::AutoLock auto_lock(lock_);
    return file_.IsValid();
  }

  void Close() {
    DCHECK(!IsMainThread());

    base::AutoLock auto_lock(lock_);
    DCHECK(file_.IsValid()) << __func__ << " called on invalid file";

    file_.Close();
  }

  // Returns {length, base::File::FILE_OK} in case of success.
  // Returns {invalid number, error} in case of failure.
  std::pair<int64_t, base::File::Error> GetLength() {
    DCHECK(!IsMainThread());

    base::AutoLock auto_lock(lock_);
    DCHECK(file_.IsValid()) << __func__ << " called on invalid file";

    int64_t length = file_.GetLength();
    base::File::Error error =
        (length < 0) ? file_.GetLastFileError() : base::File::FILE_OK;

    return {length, error};
  }

  // Returns {expected_length, base::File::FILE_OK} in case of success.
  // Returns {actual file length, error} in case of failure.
  std::pair<int64_t, base::File::Error> SetLength(int64_t expected_length) {
    DCHECK(!IsMainThread());
    DCHECK_GE(expected_length, 0);

    base::AutoLock auto_lock(lock_);
    DCHECK(file_.IsValid()) << __func__ << " called on invalid file";

    bool success = file_.SetLength(expected_length);
    base::File::Error error =
        success ? base::File::FILE_OK : file_.GetLastFileError();
    int64_t actual_length = success ? expected_length : file_.GetLength();

    return {actual_length, error};
  }

#if BUILDFLAG(IS_MAC)
  // Used to implement browser-side SetLength() on macOS < 10.15.
  base::File TakeFile() {
    base::AutoLock auto_lock(lock_);
    DCHECK(file_.IsValid()) << __func__ << " called on invalid file";

    return std::move(file_);
  }

  // Used to implement browser-side SetLength() on macOS < 10.15.
  void SetFile(base::File file) {
    base::AutoLock auto_lock(lock_);
    DCHECK(!file_.IsValid()) << __func__ << " called on valid file";

    file_ = std::move(file);
  }
#endif  // BUILDFLAG(IS_MAC)

  // Returns {read byte count, base::File::FILE_OK} in case of success.
  // Returns {invalid number, error} in case of failure.
  std::pair<int, base::File::Error> Read(NativeIODataBuffer* buffer,
                                         int64_t file_offset,
                                         int read_size) {
    DCHECK(!IsMainThread());
    DCHECK(buffer);
    DCHECK_GE(file_offset, 0);
    DCHECK_GE(read_size, 0);

    base::AutoLock auto_lock(lock_);
    DCHECK(file_.IsValid()) << __func__ << " called on invalid file";

    int read_bytes = file_.Read(file_offset, buffer->Data(), read_size);
    base::File::Error error =
        (read_bytes < 0) ? file_.GetLastFileError() : base::File::FILE_OK;

    return {read_bytes, error};
  }

  // Returns {0, write_size, base::File::FILE_OK} in case of success.
  // Returns {actual file length, written bytes, base::File::OK} in case of a
  // short write.
  // Returns {actual file length, invalid number, error} in case of failure.
  std::tuple<int64_t, int, base::File::Error> Write(NativeIODataBuffer* buffer,
                                                    int64_t file_offset,
                                                    int write_size) {
    DCHECK(!IsMainThread());
    DCHECK(buffer);
    DCHECK_GE(file_offset, 0);
    DCHECK_GE(write_size, 0);

    base::AutoLock auto_lock(lock_);
    DCHECK(file_.IsValid()) << __func__ << " called on invalid file";

    int written_bytes = file_.Write(file_offset, buffer->Data(), write_size);
    base::File::Error error =
        (written_bytes < 0) ? file_.GetLastFileError() : base::File::FILE_OK;
    int64_t actual_file_length_on_failure = 0;
    if (written_bytes < write_size || error != base::File::FILE_OK) {
      actual_file_length_on_failure = file_.GetLength();
      if (actual_file_length_on_failure < 0 && error != base::File::FILE_OK)
        error = file_.GetLastFileError();
    }

    return {actual_file_length_on_failure, written_bytes, error};
  }

  base::File::Error Flush() {
    DCHECK(!IsMainThread());

    base::AutoLock auto_lock(lock_);
    DCHECK(file_.IsValid()) << __func__ << " called on invalid file";

    bool success = file_.Flush();
    return success ? base::File::FILE_OK : file_.GetLastFileError();
  }

 private:
  // Lock coordinating cross-thread access to the state.
  base::Lock lock_;

  // The file on disk backing this NativeIOFile.
  base::File file_ GUARDED_BY(lock_);
};

NativeIOFile::NativeIOFile(
    base::File backing_file,
    int64_t backing_file_length,
    HeapMojoRemote<mojom::blink::NativeIOFileHost> backend_file,
    NativeIOCapacityTracker* capacity_tracker,
    ExecutionContext* execution_context)
    : file_length_(backing_file_length),
      file_state_(base::MakeRefCounted<FileState>(std::move(backing_file))),
      // TODO(pwnall): Get a dedicated queue when the specification matures.
      resolver_task_runner_(
          execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)),
      backend_file_(std::move(backend_file)),
      capacity_tracker_(capacity_tracker) {
  DCHECK_GE(backing_file_length, 0);
  DCHECK(capacity_tracker);
  backend_file_.set_disconnect_handler(WTF::BindOnce(
      &NativeIOFile::OnBackendDisconnect, WrapWeakPersistent(this)));
}

NativeIOFile::~NativeIOFile() {
  // Needed to avoid having the FileState destructor close the file descriptor
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
    DCHECK(file_state_)
        << "file_state_ nulled out without setting closed_ or io_pending_";

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
  DCHECK(file_state_)
      << "file_state_ nulled out without setting closed_ or io_pending_";

  io_pending_ = true;
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  worker_pool::PostTask(
      FROM_HERE, {base::MayBlock()},
      CrossThreadBindOnce(&DoGetLength, MakeCrossThreadHandle(this),
                          MakeCrossThreadHandle(resolver), file_state_,
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
  DCHECK(file_state_)
      << "file_state_ nulled out without setting closed_ or io_pending_";

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

#if BUILDFLAG(IS_MAC)
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
    base::File file = file_state_->TakeFile();
    backend_file_->SetLength(
        expected_length, std::move(file),
        WTF::BindOnce(&NativeIOFile::DidSetLengthIpc, WrapPersistent(this),
                      WrapPersistent(resolver)));
    return resolver->Promise();
  }
#endif  // BUILDFLAG(IS_MAC)

  worker_pool::PostTask(
      FROM_HERE, {base::MayBlock()},
      CrossThreadBindOnce(&DoSetLength, MakeCrossThreadHandle(this),
                          MakeCrossThreadHandle(resolver), file_state_,
                          resolver_task_runner_, expected_length));
  return resolver->Promise();
}

ScriptPromise NativeIOFile::read(ScriptState* script_state,
                                 NotShared<DOMArrayBufferView> buffer,
                                 uint64_t file_offset,
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
  DCHECK(file_state_)
      << "file_state_ nulled out without setting closed_ or io_pending_";

  // TODO(pwnall): This assignment should move right before the
  // worker_pool::PostTask() call.
  //
  // `io_pending_` should only be set to true when we know for sure we'll post a
  // task that eventually results in getting `io_pending_` set back to false.
  // Having `io_pending_` set to true in an early return case (rejecting with an
  // exception) leaves the NativeIOFile "stuck" in a state where all future I/O
  // method calls will reject.
  io_pending_ = true;

  int read_size = NativeIOOperationSize(*buffer);

  std::unique_ptr<NativeIODataBuffer> result_buffer_data =
      NativeIODataBuffer::Create(script_state, buffer, exception_state);
  if (!result_buffer_data) {
    return ScriptPromise();
  }
  DCHECK(result_buffer_data->IsValid());
  DCHECK(buffer->IsDetached());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  worker_pool::PostTask(
      FROM_HERE, {base::MayBlock()},
      CrossThreadBindOnce(&DoRead, MakeCrossThreadHandle(this),
                          MakeCrossThreadHandle(resolver), file_state_,
                          resolver_task_runner_, std::move(result_buffer_data),
                          file_offset, read_size));
  return resolver->Promise();
}

ScriptPromise NativeIOFile::write(ScriptState* script_state,
                                  NotShared<DOMArrayBufferView> buffer,
                                  uint64_t file_offset,
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
  DCHECK(file_state_)
      << "file_state_ nulled out without setting closed_ or io_pending_";

  int write_size = NativeIOOperationSize(*buffer);
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

  // TODO(pwnall): This assignment should move right before the
  // worker_pool::PostTask() call.
  //
  // `io_pending_` should only be set to true when we know for sure we'll post a
  // task that eventually results in getting `io_pending_` set back to false.
  // Having `io_pending_` set to true in an early return case (rejecting with an
  // exception) leaves the NativeIOFile "stuck" in a state where all future I/O
  // method calls will reject.
  io_pending_ = true;

  std::unique_ptr<NativeIODataBuffer> result_buffer_data =
      NativeIODataBuffer::Create(script_state, buffer, exception_state);
  if (!result_buffer_data) {
    return ScriptPromise();
  }
  DCHECK(result_buffer_data->IsValid());
  DCHECK(buffer->IsDetached());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  worker_pool::PostTask(
      FROM_HERE, {base::MayBlock()},
      CrossThreadBindOnce(&DoWrite, MakeCrossThreadHandle(this),
                          MakeCrossThreadHandle(resolver), file_state_,
                          resolver_task_runner_, std::move(result_buffer_data),
                          file_offset, write_size));
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
  DCHECK(file_state_)
      << "file_state_ nulled out without setting closed_ or io_pending_";

  io_pending_ = true;
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  worker_pool::PostTask(
      FROM_HERE, {base::MayBlock()},
      CrossThreadBindOnce(&DoFlush, MakeCrossThreadHandle(this),
                          MakeCrossThreadHandle(resolver), file_state_,
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

  scoped_refptr<FileState> file_state = std::move(file_state_);
  DCHECK(!file_state_);

  worker_pool::PostTask(
      FROM_HERE, {base::MayBlock()},
      CrossThreadBindOnce(&DoClose, MakeCrossThreadHandle(this),
                          MakeCrossThreadHandle(resolver),
                          std::move(file_state), resolver_task_runner_));
}

// static
void NativeIOFile::DoClose(
    CrossThreadHandle<NativeIOFile> native_io_file,
    CrossThreadHandle<ScriptPromiseResolver> resolver,
    scoped_refptr<NativeIOFile::FileState> file_state,
    scoped_refptr<base::SequencedTaskRunner> resolver_task_runner) {
  DCHECK(!IsMainThread()) << "File I/O should not happen on the main thread";
  DCHECK(file_state);
  DCHECK(file_state->IsValid())
      << "File I/O operation queued after file closed";
  DCHECK(resolver_task_runner);

  file_state->Close();

  PostCrossThreadTask(
      *resolver_task_runner, FROM_HERE,
      CrossThreadBindOnce(
          &NativeIOFile::DidClose,
          MakeUnwrappingCrossThreadHandle(std::move(native_io_file)),
          MakeUnwrappingCrossThreadHandle(std::move(resolver))));
}

void NativeIOFile::DidClose(ScriptPromiseResolver* resolver) {
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
  backend_file_->Close(WTF::BindOnce(
      [](ScriptPromiseResolver* resolver) { resolver->Resolve(); },
      WrapPersistent(resolver)));
}

// static
void NativeIOFile::DoGetLength(
    CrossThreadHandle<NativeIOFile> native_io_file,
    CrossThreadHandle<ScriptPromiseResolver> resolver,
    scoped_refptr<NativeIOFile::FileState> file_state,
    scoped_refptr<base::SequencedTaskRunner> resolver_task_runner) {
  DCHECK(!IsMainThread()) << "File I/O should not happen on the main thread";
  DCHECK(file_state);
  DCHECK(file_state->IsValid())
      << "File I/O operation queued after file closed";
  DCHECK(resolver_task_runner);

  auto [length, get_length_error] = file_state->GetLength();

  PostCrossThreadTask(
      *resolver_task_runner, FROM_HERE,
      CrossThreadBindOnce(
          &NativeIOFile::DidGetLength,
          MakeUnwrappingCrossThreadHandle(std::move(native_io_file)),
          MakeUnwrappingCrossThreadHandle(std::move(resolver)), length,
          get_length_error));
}

void NativeIOFile::DidGetLength(ScriptPromiseResolver* resolver,
                                int64_t length,
                                base::File::Error get_length_error) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

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
    CrossThreadHandle<NativeIOFile> native_io_file,
    CrossThreadHandle<ScriptPromiseResolver> resolver,
    scoped_refptr<NativeIOFile::FileState> file_state,
    scoped_refptr<base::SequencedTaskRunner> resolver_task_runner,
    int64_t expected_length) {
  DCHECK(!IsMainThread()) << "File I/O should not happen on the main thread";
  DCHECK(file_state);
  DCHECK(file_state->IsValid())
      << "File I/O operation queued after file closed";
  DCHECK(resolver_task_runner);
  DCHECK_GE(expected_length, 0);

  auto [actual_length, set_length_error] =
      file_state->SetLength(expected_length);

  PostCrossThreadTask(
      *resolver_task_runner, FROM_HERE,
      CrossThreadBindOnce(
          &NativeIOFile::DidSetLengthIo,
          MakeUnwrappingCrossThreadHandle(std::move(native_io_file)),
          MakeUnwrappingCrossThreadHandle(std::move(resolver)), actual_length,
          set_length_error));
}

void NativeIOFile::DidSetLengthIo(ScriptPromiseResolver* resolver,
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

#if BUILDFLAG(IS_MAC)
void NativeIOFile::DidSetLengthIpc(
    ScriptPromiseResolver* resolver,
    base::File backing_file,
    int64_t actual_length,
    mojom::blink::NativeIOErrorPtr set_length_error) {
  DCHECK(backing_file.IsValid()) << "browser returned closed file";
  file_state_->SetFile(std::move(backing_file));
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
#endif  // BUILDFLAG(IS_MAC)

// static
void NativeIOFile::DoRead(
    CrossThreadHandle<NativeIOFile> native_io_file,
    CrossThreadHandle<ScriptPromiseResolver> resolver,
    scoped_refptr<NativeIOFile::FileState> file_state,
    scoped_refptr<base::SequencedTaskRunner> resolver_task_runner,
    std::unique_ptr<NativeIODataBuffer> result_buffer_data,
    uint64_t file_offset,
    int read_size) {
  DCHECK(!IsMainThread()) << "File I/O should not happen on the main thread";
  DCHECK(file_state);
  DCHECK(file_state->IsValid())
      << "File I/O operation queued after file closed";
  DCHECK(resolver_task_runner);
  DCHECK(result_buffer_data);
  DCHECK(result_buffer_data->IsValid());
  DCHECK_GE(read_size, 0);
#if DCHECK_IS_ON()
  DCHECK_LE(static_cast<size_t>(read_size), result_buffer_data->DataLength());
#endif  // DCHECK_IS_ON()

  auto [read_bytes, read_error] =
      file_state->Read(result_buffer_data.get(), file_offset, read_size);

  PostCrossThreadTask(
      *resolver_task_runner, FROM_HERE,
      CrossThreadBindOnce(
          &NativeIOFile::DidRead,
          MakeUnwrappingCrossThreadHandle(std::move(native_io_file)),
          MakeUnwrappingCrossThreadHandle(std::move(resolver)),
          std::move(result_buffer_data), read_bytes, read_error));
}

void NativeIOFile::DidRead(
    ScriptPromiseResolver* resolver,
    std::unique_ptr<NativeIODataBuffer> result_buffer_data,
    int read_bytes,
    base::File::Error read_error) {
  DCHECK(result_buffer_data);
  DCHECK(result_buffer_data->IsValid());

  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

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
  NativeIOReadResult* read_result = MakeGarbageCollected<NativeIOReadResult>();
  read_result->setBuffer(result_buffer_data->Take());
  read_result->setReadBytes(read_bytes);
  resolver->Resolve(read_result);
}

// static
void NativeIOFile::DoWrite(
    CrossThreadHandle<NativeIOFile> native_io_file,
    CrossThreadHandle<ScriptPromiseResolver> resolver,
    scoped_refptr<NativeIOFile::FileState> file_state,
    scoped_refptr<base::SequencedTaskRunner> resolver_task_runner,
    std::unique_ptr<NativeIODataBuffer> result_buffer_data,
    uint64_t file_offset,
    int write_size) {
  DCHECK(!IsMainThread()) << "File I/O should not happen on the main thread";
  DCHECK(file_state);
  DCHECK(file_state->IsValid())
      << "File I/O operation queued after file closed";
  DCHECK(resolver_task_runner);
  DCHECK(result_buffer_data);
  DCHECK(result_buffer_data->IsValid());
  DCHECK_GE(write_size, 0);
#if DCHECK_IS_ON()
  DCHECK_LE(static_cast<size_t>(write_size), result_buffer_data->DataLength());
#endif  // DCHECK_IS_ON()

  auto [actual_file_length_on_failure, written_bytes, write_error] =
      file_state->Write(result_buffer_data.get(), file_offset, write_size);

  PostCrossThreadTask(
      *resolver_task_runner, FROM_HERE,
      CrossThreadBindOnce(
          &NativeIOFile::DidWrite,
          MakeUnwrappingCrossThreadHandle(std::move(native_io_file)),
          MakeUnwrappingCrossThreadHandle(std::move(resolver)),
          std::move(result_buffer_data), written_bytes, write_error, write_size,
          actual_file_length_on_failure));
}

void NativeIOFile::DidWrite(
    ScriptPromiseResolver* resolver,
    std::unique_ptr<NativeIODataBuffer> result_buffer_data,
    int written_bytes,
    base::File::Error write_error,
    int write_size,
    int64_t actual_file_length_on_failure) {
  DCHECK(result_buffer_data);
  DCHECK(result_buffer_data->IsValid());

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
  NativeIOWriteResult* write_result =
      MakeGarbageCollected<NativeIOWriteResult>();
  write_result->setBuffer(result_buffer_data->Take());
  write_result->setWrittenBytes(written_bytes);
  resolver->Resolve(write_result);
}

// static
void NativeIOFile::DoFlush(
    CrossThreadHandle<NativeIOFile> native_io_file,
    CrossThreadHandle<ScriptPromiseResolver> resolver,
    scoped_refptr<NativeIOFile::FileState> file_state,
    scoped_refptr<base::SequencedTaskRunner> resolver_task_runner) {
  DCHECK(!IsMainThread()) << "File I/O should not happen on the main thread";
  DCHECK(file_state);
  DCHECK(file_state->IsValid())
      << "File I/O operation queued after file closed";

  base::File::Error flush_error = file_state->Flush();

  PostCrossThreadTask(
      *resolver_task_runner, FROM_HERE,
      CrossThreadBindOnce(
          &NativeIOFile::DidFlush,
          MakeUnwrappingCrossThreadHandle(std::move(native_io_file)),
          MakeUnwrappingCrossThreadHandle(std::move(resolver)), flush_error));
}

void NativeIOFile::DidFlush(ScriptPromiseResolver* resolver,
                            base::File::Error flush_error) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

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

  if (!file_state_) {
    // Avoid posting a cross-thread task if the file is already closed. This is
    // the expected path.
    return;
  }

  scoped_refptr<FileState> file_state = std::move(file_state_);
  DCHECK(!file_state_);

  worker_pool::PostTask(FROM_HERE, {base::MayBlock()},
                        CrossThreadBindOnce(
                            [](scoped_refptr<FileState> file_state) {
                              DCHECK(file_state);
                              file_state->Close();
                            },
                            std::move(file_state)));
}

}  // namespace blink
