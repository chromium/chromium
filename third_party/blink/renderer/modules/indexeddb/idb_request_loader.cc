// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/indexeddb/idb_request_loader.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value_wrapping.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

IDBRequestLoader::IDBRequestLoader(
    Vector<std::unique_ptr<IDBValue>>&& values,
    ExecutionContext* execution_context,
    LoadCompleteCallback&& load_complete_callback)
    : values_(std::move(values)),
      execution_context_(execution_context),
      load_complete_callback_(std::move(load_complete_callback)) {
  DCHECK(IDBValueUnwrapper::IsWrapped(values_));
}

IDBRequestLoader::~IDBRequestLoader() {}

void IDBRequestLoader::Start() {
#if DCHECK_IS_ON()
  DCHECK(!started_) << "Start() was already called";
  started_ = true;
#endif  // DCHECK_IS_ON()

  // TODO(pwnall): Start() / StartNextValue() unwrap large values sequentially.
  //               Consider parallelizing. The main issue is that the Blob reads
  //               will have to be throttled somewhere, and the extra complexity
  //               only benefits applications that use getAll().
  current_value_ = values_.begin();
  StartNextValue();
}

Vector<std::unique_ptr<IDBValue>>&& IDBRequestLoader::Cancel() {
#if DCHECK_IS_ON()
  DCHECK(started_) << "Cancel() called on a loader that hasn't been Start()ed";
  DCHECK(!canceled_) << "Cancel() was already called";
  canceled_ = true;

  DCHECK(file_reader_loading_);
  file_reader_loading_ = false;
#endif  // DCHECK_IS_ON()
  if (loader_) {
    loader_->Cancel();
  }
  // We're not expected to unwrap any more values or run the callback.
  load_complete_callback_.Reset();
  execution_context_.Clear();
  return std::move(values_);
}

void IDBRequestLoader::StartNextValue() {
  IDBValueUnwrapper unwrapper;

  while (true) {
    if (current_value_ == values_.end()) {
      OnLoadComplete(DOMExceptionCode::kNoError);
      return;
    }
    if (unwrapper.Parse(current_value_->get())) {
      break;
    }
    ++current_value_;
  }

  DCHECK(current_value_ != values_.end());

  // The execution context was torn down. The loader will eventually get a
  // Cancel() call. Note that WeakMember sets `execution_context_` to null
  // only when the ExecutionContext is GC-ed, but the context destruction may
  // have happened earlier. Hence, check `IsContextDestroyed()` explicitly.
  if (!execution_context_ || execution_context_->IsContextDestroyed()) {
    return;
  }

  wrapped_data_.reserve(unwrapper.WrapperBlobSize());
#if DCHECK_IS_ON()
  DCHECK(!file_reader_loading_);
  file_reader_loading_ = true;
#endif  // DCHECK_IS_ON()
  loader_ = MakeGarbageCollected<FileReaderLoader>(
      this, execution_context_->GetTaskRunner(TaskType::kDatabaseAccess));
  loader_->Start(unwrapper.WrapperBlobHandle());
}

FileErrorCode IDBRequestLoader::DidStartLoading(uint64_t) {
  return FileErrorCode::kOK;
}

FileErrorCode IDBRequestLoader::DidReceiveData(base::span<const uint8_t> data) {
  DCHECK_LE(wrapped_data_.size() + data.size(), wrapped_data_.capacity())
      << "The reader returned more data than we were prepared for";

  wrapped_data_.AppendSpan(base::as_chars(data));
  return FileErrorCode::kOK;
}

void IDBRequestLoader::DidFinishLoading() {
#if DCHECK_IS_ON()
  DCHECK(started_)
      << "FileReaderLoader called DidFinishLoading() before it was Start()ed";
  DCHECK(!canceled_)
      << "FileReaderLoader called DidFinishLoading() after it was Cancel()ed";

  DCHECK(file_reader_loading_);
  file_reader_loading_ = false;
#endif  // DCHECK_IS_ON()

  IDBValueUnwrapper::Unwrap(std::move(wrapped_data_), **current_value_);
  ++current_value_;

  StartNextValue();
}

void IDBRequestLoader::DidFail(FileErrorCode error_code) {
#if DCHECK_IS_ON()
  DCHECK(started_)
      << "FileReaderLoader called DidFail() before it was Start()ed";
  DCHECK(!canceled_)
      << "FileReaderLoader called DidFail() after it was Cancel()ed";

  DCHECK(file_reader_loading_);
  file_reader_loading_ = false;
#endif  // DCHECK_IS_ON()
  base::UmaHistogramEnumeration("IndexedDB.LargeValueReadError", error_code);
  DOMExceptionCode exception_code = error_code == FileErrorCode::kNotFoundErr
                                        ? DOMExceptionCode::kNotFoundError
                                        : DOMExceptionCode::kDataError;
  OnLoadComplete(exception_code);
}

void IDBRequestLoader::OnLoadComplete(DOMExceptionCode exception_code) {
#if DCHECK_IS_ON()
  DCHECK(started_);
  DCHECK(!canceled_);
#endif  // DCHECK_IS_ON()
  std::move(load_complete_callback_)
      .Run(std::move(values_),
           exception_code != DOMExceptionCode::kNoError
               ? MakeGarbageCollected<DOMException>(
                     exception_code, "Failed to read large IndexedDB value")
               : nullptr);
}

}  // namespace blink
