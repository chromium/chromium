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

#include "third_party/blink/renderer/modules/filesystem/file_entry.h"

#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/modules/filesystem/async_callback_helper.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_callbacks.h"
#include "third_party/blink/renderer/modules/filesystem/file_writer.h"

namespace blink {

FileEntry::FileEntry(DOMFileSystemBase* file_system, const String& full_path)
    : Entry(file_system, full_path) {}

void FileEntry::createWriter(V8FileWriterCallback* success_callback,
                             V8ErrorCallback* error_callback) {
  auto success_callback_wrapper =
      success_callback
          ? WTF::BindOnce(
                [](V8FileWriterCallback* persistent_callback,
                   FileWriterBase* file_writer) {
                  // The call sites must pass a FileWriter in |file_writer|.
                  persistent_callback->InvokeAndReportException(
                      nullptr, static_cast<FileWriter*>(file_writer));
                },
                WrapPersistent(success_callback))
          : FileWriterCallbacks::SuccessCallback();
  auto error_callback_wrapper =
      AsyncCallbackHelper::ErrorCallback(error_callback);

  filesystem()->CreateWriter(this, std::move(success_callback_wrapper),
                             std::move(error_callback_wrapper));
}

void FileEntry::file(V8FileCallback* success_callback,
                     V8ErrorCallback* error_callback) {
  auto success_callback_wrapper =
      AsyncCallbackHelper::SuccessCallback<File>(success_callback);
  auto error_callback_wrapper =
      AsyncCallbackHelper::ErrorCallback(error_callback);

  filesystem()->CreateFile(this, std::move(success_callback_wrapper),
                           std::move(error_callback_wrapper));
}

void FileEntry::Trace(Visitor* visitor) const {
  Entry::Trace(visitor);
}

}  // namespace blink
