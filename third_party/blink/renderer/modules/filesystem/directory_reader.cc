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

void RunEntriesCallback(V8EntriesCallback* callback, EntryHeapVector* entries) {
  callback->InvokeAndReportException(nullptr, *entries);
}

}  // namespace

DirectoryReader::DirectoryReader(DOMFileSystemBase* file_system,
                                 const String& full_path)
    : DirectoryReaderBase(file_system, full_path), is_reading_(false) {}

void DirectoryReader::readEntries(V8EntriesCallback* entries_callback,
                                  V8ErrorCallback* error_callback) {
  if (entries_callback_) {
    // Non-null entries_callback_ means multiple readEntries() calls are made
    // concurrently. We don't allow doing it.
    Filesystem()->ReportError(
        WTF::BindOnce(
            [](V8ErrorCallback* error_callback, base::File::Error error) {
              error_callback->InvokeAndReportException(
                  nullptr, file_error::CreateDOMException(error));
            },
            WrapPersistent(error_callback)),
        base::File::FILE_ERROR_FAILED);
    return;
  }

  auto success_callback_wrapper = WTF::BindRepeating(
      [](DirectoryReader* persistent_reader, EntryHeapVector* entries) {
        persistent_reader->AddEntries(*entries);
      },
      WrapPersistentIfNeeded(this));

  if (!is_reading_) {
    is_reading_ = true;
    Filesystem()->ReadDirectory(
        this, full_path_, success_callback_wrapper,
        WTF::BindOnce(&DirectoryReader::OnError, WrapPersistentIfNeeded(this)));
  }

  if (error_ != base::File::FILE_OK) {
    Filesystem()->ReportError(
        WTF::BindOnce(&DirectoryReader::OnError, WrapPersistentIfNeeded(this)),
        error_);
    return;
  }

  if (!has_more_entries_ || !entries_.empty()) {
    EntryHeapVector* entries =
        MakeGarbageCollected<EntryHeapVector>(std::move(entries_));
    DOMFileSystem::ScheduleCallback(
        Filesystem()->GetExecutionContext(),
        WTF::BindOnce(&RunEntriesCallback, WrapPersistent(entries_callback),
                      WrapPersistent(entries)));
    return;
  }

  entries_callback_ = entries_callback;
  error_callback_ = error_callback;
}

void DirectoryReader::AddEntries(const EntryHeapVector& entries_to_add) {
  entries_.AppendVector(entries_to_add);
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
        nullptr, file_error::CreateDOMException(error_));
  }
}

void DirectoryReader::Trace(Visitor* visitor) const {
  visitor->Trace(entries_);
  visitor->Trace(entries_callback_);
  visitor->Trace(error_callback_);
  DirectoryReaderBase::Trace(visitor);
}

}  // namespace blink
