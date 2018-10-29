/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/filesystem/directory_reader.h"

#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/modules/filesystem/entry.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_callbacks.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

void RunEntriesCallback(
    V8PersistentCallbackInterface<V8EntriesCallback>* callback,
    EntryHeapVector* entries) {
  callback->InvokeAndReportException(nullptr, *entries);
}

}  // namespace

class DirectoryReader::EntriesCallbackHelper final
    : public EntriesCallbacks::OnDidGetEntriesCallback {
 public:
  static EntriesCallbackHelper* Create(DirectoryReader* reader) {
    return new EntriesCallbackHelper(reader);
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(reader_);
    EntriesCallbacks::OnDidGetEntriesCallback::Trace(visitor);
  }

  void OnSuccess(EntryHeapVector* entries) override {
    reader_->AddEntries(*entries);
  }

 private:
  explicit EntriesCallbackHelper(DirectoryReader* reader) : reader_(reader) {}

  // FIXME: This Member keeps the reader alive until all of the readDirectory
  // results are received. crbug.com/350285
  Member<DirectoryReader> reader_;
};

class DirectoryReader::ErrorCallbackHelper final : public ErrorCallbackBase {
 public:
  static ErrorCallbackHelper* Create(DirectoryReader* reader) {
    return new ErrorCallbackHelper(reader);
  }

  void Invoke(base::File::Error error) override { reader_->OnError(error); }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(reader_);
    ErrorCallbackBase::Trace(visitor);
  }

 private:
  explicit ErrorCallbackHelper(DirectoryReader* reader) : reader_(reader) {}

  Member<DirectoryReader> reader_;
};

DirectoryReader::DirectoryReader(DOMFileSystemBase* file_system,
                                 const String& full_path)
    : DirectoryReaderBase(file_system, full_path), is_reading_(false) {}

void DirectoryReader::readEntries(V8EntriesCallback* entries_callback,
                                  V8ErrorCallback* error_callback) {
  if (!is_reading_) {
    is_reading_ = true;
    Filesystem()->ReadDirectory(this, full_path_,
                                EntriesCallbackHelper::Create(this),
                                ErrorCallbackHelper::Create(this));
  }

  if (error_ != base::File::FILE_OK) {
    Filesystem()->ReportError(ScriptErrorCallback::Wrap(error_callback),
                              error_);
    return;
  }

  if (entries_callback_) {
    // Non-null entries_callback_ means multiple readEntries() calls are made
    // concurrently. We don't allow doing it.
    Filesystem()->ReportError(ScriptErrorCallback::Wrap(error_callback),
                              base::File::FILE_ERROR_FAILED);
    return;
  }

  if (!has_more_entries_ || !entries_.IsEmpty()) {
    EntryHeapVector* entries = new EntryHeapVector(std::move(entries_));
    DOMFileSystem::ScheduleCallback(
        Filesystem()->GetExecutionContext(),
        WTF::Bind(
            &RunEntriesCallback,
            WrapPersistent(ToV8PersistentCallbackInterface(entries_callback)),
            WrapPersistent(entries)));
    return;
  }

  entries_callback_ = ToV8PersistentCallbackInterface(entries_callback);
  error_callback_ = ToV8PersistentCallbackInterface(error_callback);
}

void DirectoryReader::AddEntries(const EntryHeapVector& entries) {
  entries_.AppendVector(entries);
  error_callback_ = nullptr;
  if (auto* entries_callback = entries_callback_.Release()) {
    EntryHeapVector entries;
    entries.swap(entries_);
    entries_callback->InvokeAndReportException(nullptr, entries);
  }
}

void DirectoryReader::OnError(base::File::Error error) {
  error_ = error;
  entries_callback_ = nullptr;
  if (auto* error_callback = error_callback_.Release()) {
    error_callback->InvokeAndReportException(
        nullptr, FileError::CreateDOMException(error_));
  }
}

void DirectoryReader::Trace(blink::Visitor* visitor) {
  visitor->Trace(entries_);
  visitor->Trace(entries_callback_);
  visitor->Trace(error_callback_);
  DirectoryReaderBase::Trace(visitor);
}

}  // namespace blink
