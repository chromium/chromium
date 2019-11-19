/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/filesystem/html_input_element_file_system.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file_list.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/modules/filesystem/directory_entry.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_path.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system.h"
#include "third_party/blink/renderer/modules/filesystem/entry.h"
#include "third_party/blink/renderer/modules/filesystem/file_entry.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

// static
EntryHeapVector HTMLInputElementFileSystem::webkitEntries(
    ScriptState* script_state,
    HTMLInputElement& input) {
  EntryHeapVector entries;
  FileList* files = input.files();

  if (!files)
    return entries;

  DOMFileSystem* filesystem = DOMFileSystem::CreateIsolatedFileSystem(
      ExecutionContext::From(script_state), input.DroppedFileSystemId());
  if (!filesystem) {
    // Drag-drop isolated filesystem is not available.
    return entries;
  }

  for (unsigned i = 0; i < files->length(); ++i) {
    File* file = files->item(i);

    // FIXME: This involves synchronous file operation.
    FileMetadata metadata;
    if (!GetFileMetadata(file->GetPath(), metadata))
      continue;

    // The dropped entries are mapped as top-level entries in the isolated
    // filesystem.
    String virtual_path = DOMFilePath::Append("/", file->name());
    if (metadata.type == FileMetadata::kTypeDirectory) {
      entries.push_back(
          MakeGarbageCollected<DirectoryEntry>(filesystem, virtual_path));
    } else {
      entries.push_back(
          MakeGarbageCollected<FileEntry>(filesystem, virtual_path));
    }
  }
  return entries;
}

}  // namespace blink
