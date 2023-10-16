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

#include "third_party/blink/renderer/modules/filesystem/dragged_isolated_file_system_impl.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

DraggedIsolatedFileSystemImpl::DraggedIsolatedFileSystemImpl(
    DataObject& data_object)
    : Supplement(data_object) {}

DOMFileSystem* DraggedIsolatedFileSystemImpl::GetDOMFileSystem(
    DataObject* host,
    ExecutionContext* execution_context,
    const DataObjectItem& item) {
  if (!item.HasFileSystemId())
    return nullptr;
  const String file_system_id = item.FileSystemId();
  DraggedIsolatedFileSystemImpl* dragged_isolated_file_system = From(host);
  if (!dragged_isolated_file_system)
    return nullptr;
  auto it = dragged_isolated_file_system->filesystems_.find(file_system_id);
  if (it != dragged_isolated_file_system->filesystems_.end())
    return it->value.Get();
  return dragged_isolated_file_system->filesystems_
      .insert(file_system_id, DOMFileSystem::CreateIsolatedFileSystem(
                                  execution_context, file_system_id))
      .stored_value->value;
}

// static
const char DraggedIsolatedFileSystemImpl::kSupplementName[] =
    "DraggedIsolatedFileSystemImpl";

DraggedIsolatedFileSystemImpl* DraggedIsolatedFileSystemImpl::From(
    DataObject* data_object) {
  DCHECK(IsMainThread());
  return Supplement<DataObject>::From<DraggedIsolatedFileSystemImpl>(
      data_object);
}

void DraggedIsolatedFileSystemImpl::Trace(Visitor* visitor) const {
  visitor->Trace(filesystems_);
  Supplement<DataObject>::Trace(visitor);
}

void DraggedIsolatedFileSystemImpl::PrepareForDataObject(
    DataObject* data_object) {
  DCHECK(IsMainThread());
  DraggedIsolatedFileSystemImpl* file_system =
      MakeGarbageCollected<DraggedIsolatedFileSystemImpl>(*data_object);
  ProvideTo(*data_object, file_system);
}

}  // namespace blink
