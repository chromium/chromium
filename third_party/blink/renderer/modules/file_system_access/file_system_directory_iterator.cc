// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_directory_iterator.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_error.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_directory_handle.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

FileSystemDirectoryIterator::FileSystemDirectoryIterator(
    FileSystemDirectoryHandle* directory,
    Mode mode,
    ExecutionContext* execution_context)
    : ActiveScriptWrappable<FileSystemDirectoryIterator>({}),
      ExecutionContextClient(execution_context),
      mode_(mode),
      directory_(directory),
      receiver_(this, execution_context) {
  directory_->MojoHandle()->GetEntries(receiver_.BindNewPipeAndPassRemote(
      execution_context->GetTaskRunner(TaskType::kStorage)));
}

ScriptPromise FileSystemDirectoryIterator::next(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return nextImpl(script_state, exception_state.GetContext());
}

ScriptPromise FileSystemDirectoryIterator::nextImpl(
    ScriptState* script_state,
    const ExceptionContext& exception_context) {
  // TODO(crbug.com/1087157): The bindings layer should implement async
  // iterable. Until it gets implemented, this class (and especially this
  // member function) implements the behavior of async iterable in its own way.
  // Use of bindings internal code (use of bindings:: internal namespace) should
  // be gone once https://crbug.com/1087157 gets resolved.
  if (error_) {
    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
        script_state, exception_context);
    auto result = resolver->Promise();
    file_system_access_error::Reject(resolver, *error_);
    return result;
  }

  if (!entries_.empty()) {
    FileSystemHandle* handle = entries_.TakeFirst();
    v8::Local<v8::Value> result;
    switch (mode_) {
      case Mode::kKey:
        result = bindings::ESCreateIterResultObject(
            script_state, false,
            ToV8Traits<IDLString>::ToV8(script_state, handle->name())
                .ToLocalChecked());
        break;
      case Mode::kValue:
        result = bindings::ESCreateIterResultObject(
            script_state, false,
            ToV8Traits<FileSystemHandle>::ToV8(script_state, handle)
                .ToLocalChecked());
        break;
      case Mode::kKeyValue:
        result = bindings::ESCreateIterResultObject(
            script_state, false,
            ToV8Traits<IDLString>::ToV8(script_state, handle->name())
                .ToLocalChecked(),
            ToV8Traits<FileSystemHandle>::ToV8(script_state, handle)
                .ToLocalChecked());
        break;
    }
    return ScriptPromise::Cast(script_state, result);
  }

  if (waiting_for_more_entries_) {
    DCHECK(!pending_next_);
    pending_next_ = MakeGarbageCollected<ScriptPromiseResolver>(
        script_state, exception_context);
    return pending_next_->Promise();
  }

  return ScriptPromise::Cast(
      script_state,
      bindings::ESCreateIterResultObject(
          script_state, true, v8::Undefined(script_state->GetIsolate())));
}

bool FileSystemDirectoryIterator::HasPendingActivity() const {
  return pending_next_;
}

void FileSystemDirectoryIterator::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(receiver_);
  visitor->Trace(entries_);
  visitor->Trace(pending_next_);
  visitor->Trace(directory_);
}

void FileSystemDirectoryIterator::DidReadDirectory(
    mojom::blink::FileSystemAccessErrorPtr result,
    Vector<mojom::blink::FileSystemAccessEntryPtr> entries,
    bool has_more_entries) {
  if (!GetExecutionContext())
    return;
  if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
    error_ = std::move(result);
    if (pending_next_) {
      file_system_access_error::Reject(pending_next_, *error_);
      pending_next_ = nullptr;
    }
    return;
  }
  for (auto& e : entries) {
    entries_.push_back(FileSystemHandle::CreateFromMojoEntry(
        std::move(e), GetExecutionContext()));
  }
  waiting_for_more_entries_ = has_more_entries;
  if (pending_next_) {
    ScriptState::Scope scope(pending_next_->GetScriptState());
    pending_next_->Resolve(nextImpl(pending_next_->GetScriptState(),
                                    pending_next_->GetExceptionContext())
                               .AsScriptValue());
    pending_next_ = nullptr;
  }
}

}  // namespace blink
