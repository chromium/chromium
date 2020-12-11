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

#include "third_party/blink/renderer/modules/filesystem/data_transfer_item_file_system.h"

#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_item.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/modules/filesystem/directory_entry.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_path.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system.h"
#include "third_party/blink/renderer/modules/filesystem/dragged_isolated_file_system_impl.h"
#include "third_party/blink/renderer/modules/filesystem/entry.h"
#include "third_party/blink/renderer/modules/filesystem/file_entry.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/file_metadata.h"

namespace blink {

// static
Entry* DataTransferItemFileSystem::webkitGetAsEntry(ScriptState* script_state,
                                                    DataTransferItem& item) {
  if (!item.GetDataObjectItem()->IsFilename())
    return nullptr;

  // For dragged files getAsFile must be pretty lightweight.
  Blob* file = item.getAsFile();
  // The clipboard may not be in a readable state.
  if (!file)
    return nullptr;
  DCHECK(IsA<File>(file));

  auto* context = ExecutionContext::From(script_state);
  if (!context)
    return nullptr;

  DOMFileSystem* dom_file_system =
      DraggedIsolatedFileSystemImpl::GetDOMFileSystem(
          item.GetDataTransfer()->GetDataObject(), context,
          *item.GetDataObjectItem());
  if (!dom_file_system) {
    // IsolatedFileSystem may not be enabled.
    return nullptr;
  }

  // The dropped entries are mapped as top-level entries in the isolated
  // filesystem.
  String virtual_path = DOMFilePath::Append("/", To<File>(file)->name());

  // FIXME: This involves synchronous file operation. Consider passing file type
  // data when we dispatch drag event.
  FileMetadata metadata;
  if (!GetFileMetadata(To<File>(file)->GetPath(), *context, metadata))
    return nullptr;

  if (metadata.type == FileMetadata::kTypeDirectory)
    return MakeGarbageCollected<DirectoryEntry>(dom_file_system, virtual_path);
  return MakeGarbageCollected<FileEntry>(dom_file_system, virtual_path);
}

}  // namespace blink
