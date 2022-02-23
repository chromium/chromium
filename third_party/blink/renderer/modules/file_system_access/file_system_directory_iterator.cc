// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_directory_iterator.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_error.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_directory_handle.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_file_handle.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

FileSystemDirectoryIterator::FileSystemDirectoryIterator(
    FileSystemDirectoryHandle* directory,
    Mode mode,
    ExecutionContext* execution_context)
    : ExecutionContextClient(execution_context),
      mode_(mode),
      directory_(directory),
      receiver_(this, execution_context) {
  directory_->MojoHandle()->GetEntries(receiver_.BindNewPipeAndPassRemote(
      execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
}

ScriptPromise FileSystemDirectoryIterator::next(ScriptState* script_state) {
  if (error_) {
    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
    auto result = resolver->Promise();
    file_system_access_error::Reject(resolver, *error_);
    return result;
  }

  if (!entries_.IsEmpty()) {
    FileSystemHandle* handle = entries_.TakeFirst();
    ScriptValue result;
    switch (mode_) {
      case Mode::kKey:
        result = V8IteratorResult(script_state, handle->name());
        break;
      case Mode::kValue:
        result = V8IteratorResult(script_state, handle);
        break;
      case Mode::kKeyValue:
        HeapVector<ScriptValue, 2> keyvalue;
        keyvalue.push_back(ScriptValue(
            script_state->GetIsolate(),
            ToV8Traits<IDLString>::ToV8(script_state, handle->name())));
        keyvalue.push_back(ScriptValue(
            script_state->GetIsolate(),
            ToV8Traits<FileSystemHandle>::ToV8(script_state, handle)));
        result = V8IteratorResult(script_state, keyvalue);
        break;
    }
    return ScriptPromise::Cast(script_state, result);
  }

  if (waiting_for_more_entries_) {
    DCHECK(!pending_next_);
    pending_next_ = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
    return pending_next_->Promise();
  }

  return ScriptPromise::Cast(script_state, V8IteratorResultDone(script_state));
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
    pending_next_->Resolve(
        next(pending_next_->GetScriptState()).AsScriptValue());
    pending_next_ = nullptr;
  }
}

}  // namespace blink
