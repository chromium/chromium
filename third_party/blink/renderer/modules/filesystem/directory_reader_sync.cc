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

#include "third_party/blink/renderer/modules/filesystem/directory_reader_sync.h"

#include "third_party/blink/renderer/modules/filesystem/directory_entry.h"
#include "third_party/blink/renderer/modules/filesystem/directory_entry_sync.h"
#include "third_party/blink/renderer/modules/filesystem/entry_sync.h"
#include "third_party/blink/renderer/modules/filesystem/file_entry_sync.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_callbacks.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class DirectoryReaderSync::EntriesCallbackHelper final
    : public EntriesCallbacks::OnDidGetEntriesCallback {
 public:
  static EntriesCallbackHelper* Create(DirectoryReaderSync* reader) {
    return new EntriesCallbackHelper(reader);
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(reader_);
    EntriesCallbacks::OnDidGetEntriesCallback::Trace(visitor);
  }

  void OnSuccess(EntryHeapVector* entries) override {
    reader_->entries_.ReserveCapacity(reader_->entries_.size() +
                                      entries->size());
    for (const auto& entry : *entries) {
      reader_->entries_.UncheckedAppend(EntrySync::Create(entry.Get()));
    }
  }

 private:
  explicit EntriesCallbackHelper(DirectoryReaderSync* reader)
      : reader_(reader) {}

  Member<DirectoryReaderSync> reader_;
};

class DirectoryReaderSync::ErrorCallbackHelper final
    : public ErrorCallbackBase {
 public:
  static ErrorCallbackHelper* Create(DirectoryReaderSync* reader) {
    return new ErrorCallbackHelper(reader);
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(reader_);
    ErrorCallbackBase::Trace(visitor);
  }

  void Invoke(base::File::Error error) override {
    reader_->error_code_ = error;
  }

 private:
  explicit ErrorCallbackHelper(DirectoryReaderSync* reader) : reader_(reader) {}

  Member<DirectoryReaderSync> reader_;
};

DirectoryReaderSync::DirectoryReaderSync(DOMFileSystemBase* file_system,
                                         const String& full_path)
    : DirectoryReaderBase(file_system, full_path) {}

EntrySyncHeapVector DirectoryReaderSync::readEntries(
    ExceptionState& exception_state) {
  if (!has_called_read_directory_) {
    Filesystem()->ReadDirectory(
        this, full_path_, EntriesCallbackHelper::Create(this),
        ErrorCallbackHelper::Create(this), DOMFileSystemBase::kSynchronous);
    has_called_read_directory_ = true;
  }

  DCHECK(!has_more_entries_);

  if (error_code_ != base::File::FILE_OK) {
    FileError::ThrowDOMException(exception_state, error_code_);
    return EntrySyncHeapVector();
  }

  EntrySyncHeapVector result;
  result.swap(entries_);
  return result;
}

void DirectoryReaderSync::Trace(blink::Visitor* visitor) {
  visitor->Trace(entries_);
  DirectoryReaderBase::Trace(visitor);
}

}  // namespace blink
