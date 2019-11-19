// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_file_system/native_file_system_directory_iterator.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_directory_handle.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_directory_iterator_entry.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_error.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_file_handle.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

NativeFileSystemDirectoryIterator::NativeFileSystemDirectoryIterator(
    NativeFileSystemDirectoryHandle* directory,
    ExecutionContext* execution_context)
    : ContextLifecycleObserver(execution_context), directory_(directory) {
  directory_->MojoHandle()->GetEntries(receiver_.BindNewPipeAndPassRemote());
}

ScriptPromise NativeFileSystemDirectoryIterator::next(
    ScriptState* script_state) {
  if (error_) {
    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
    auto result = resolver->Promise();
    native_file_system_error::Reject(resolver, *error_);
    return result;
  }

  if (!entries_.IsEmpty()) {
    NativeFileSystemDirectoryIteratorEntry* result =
        NativeFileSystemDirectoryIteratorEntry::Create();
    result->setValue(entries_.TakeFirst());
    return ScriptPromise::Cast(script_state, ToV8(result, script_state));
  }

  if (waiting_for_more_entries_) {
    DCHECK(!pending_next_);
    pending_next_ = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
    return pending_next_->Promise();
  }

  NativeFileSystemDirectoryIteratorEntry* result =
      NativeFileSystemDirectoryIteratorEntry::Create();
  result->setDone(true);
  return ScriptPromise::Cast(script_state, ToV8(result, script_state));
}

void NativeFileSystemDirectoryIterator::Trace(Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  visitor->Trace(entries_);
  visitor->Trace(pending_next_);
  visitor->Trace(directory_);
}

void NativeFileSystemDirectoryIterator::DidReadDirectory(
    mojom::blink::NativeFileSystemErrorPtr result,
    Vector<mojom::blink::NativeFileSystemEntryPtr> entries,
    bool has_more_entries) {
  if (!GetExecutionContext())
    return;
  if (result->status != mojom::blink::NativeFileSystemStatus::kOk) {
    error_ = std::move(result);
    if (pending_next_) {
      native_file_system_error::Reject(pending_next_, *error_);
      pending_next_ = nullptr;
    }
    return;
  }
  for (auto& e : entries) {
    entries_.push_back(NativeFileSystemHandle::CreateFromMojoEntry(
        std::move(e), GetExecutionContext()));
  }
  waiting_for_more_entries_ = has_more_entries;
  if (pending_next_) {
    ScriptState::Scope scope(pending_next_->GetScriptState());
    pending_next_->Resolve(
        next(pending_next_->GetScriptState()).GetScriptValue());
    pending_next_ = nullptr;
  }
}

void NativeFileSystemDirectoryIterator::Dispose() {
  receiver_.reset();
}

}  // namespace blink
