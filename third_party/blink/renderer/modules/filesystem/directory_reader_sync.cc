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
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

DirectoryReaderSync::DirectoryReaderSync(DOMFileSystemBase* file_system,
                                         const String& full_path)
    : DirectoryReaderBase(file_system, full_path) {}

EntrySyncHeapVector DirectoryReaderSync::readEntries(
    ExceptionState& exception_state) {
  auto success_callback_wrapper = WTF::BindRepeating(
      [](DirectoryReaderSync* persistent_reader, EntryHeapVector* entries) {
        persistent_reader->entries_.ReserveCapacity(
            persistent_reader->entries_.size() + entries->size());
        for (const auto& entry : *entries) {
          persistent_reader->entries_.UncheckedAppend(
              EntrySync::Create(entry.Get()));
        }
      },
      WrapPersistentIfNeeded(this));
  auto error_callback_wrapper = WTF::Bind(
      [](DirectoryReaderSync* persistent_reader, base::File::Error error) {
        persistent_reader->error_code_ = error;
      },
      WrapPersistentIfNeeded(this));

  if (!has_called_read_directory_) {
    Filesystem()->ReadDirectory(this, full_path_, success_callback_wrapper,
                                std::move(error_callback_wrapper),
                                DOMFileSystemBase::kSynchronous);
    has_called_read_directory_ = true;
  }

  DCHECK(!has_more_entries_);

  if (error_code_ != base::File::FILE_OK) {
    file_error::ThrowDOMException(exception_state, error_code_);
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
