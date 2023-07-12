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

#include "third_party/blink/renderer/modules/filesystem/entry_sync.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/filesystem/directory_entry.h"
#include "third_party/blink/renderer/modules/filesystem/directory_entry_sync.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_path.h"
#include "third_party/blink/renderer/modules/filesystem/file_entry_sync.h"
#include "third_party/blink/renderer/modules/filesystem/metadata.h"
#include "third_party/blink/renderer/modules/filesystem/sync_callback_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

EntrySync* EntrySync::Create(EntryBase* entry) {
  if (entry->isFile()) {
    return MakeGarbageCollected<FileEntrySync>(entry->file_system_,
                                               entry->full_path_);
  }
  return MakeGarbageCollected<DirectoryEntrySync>(entry->file_system_,
                                                  entry->full_path_);
}

Metadata* EntrySync::getMetadata(ExceptionState& exception_state) {
  auto* sync_helper = MakeGarbageCollected<MetadataCallbacksSyncHelper>();

  auto success_callback_wrapper =
      WTF::BindOnce(&MetadataCallbacksSyncHelper::OnSuccess,
                    WrapPersistentIfNeeded(sync_helper));
  auto error_callback_wrapper =
      WTF::BindOnce(&MetadataCallbacksSyncHelper::OnError,
                    WrapPersistentIfNeeded(sync_helper));

  file_system_->GetMetadata(this, std::move(success_callback_wrapper),
                            std::move(error_callback_wrapper),
                            DOMFileSystemBase::kSynchronous);
  return sync_helper->GetResultOrThrow(exception_state);
}

EntrySync* EntrySync::moveTo(DirectoryEntrySync* parent,
                             const String& name,
                             ExceptionState& exception_state) const {
  auto* helper = MakeGarbageCollected<EntryCallbacksSyncHelper>();

  auto success_callback_wrapper = WTF::BindOnce(
      &EntryCallbacksSyncHelper::OnSuccess, WrapPersistentIfNeeded(helper));
  auto error_callback_wrapper = WTF::BindOnce(
      &EntryCallbacksSyncHelper::OnError, WrapPersistentIfNeeded(helper));

  file_system_->Move(this, parent, name, std::move(success_callback_wrapper),
                     std::move(error_callback_wrapper),
                     DOMFileSystemBase::kSynchronous);
  Entry* entry = helper->GetResultOrThrow(exception_state);
  return entry ? EntrySync::Create(entry) : nullptr;
}

EntrySync* EntrySync::copyTo(DirectoryEntrySync* parent,
                             const String& name,
                             ExceptionState& exception_state) const {
  auto* sync_helper = MakeGarbageCollected<EntryCallbacksSyncHelper>();

  auto success_callback_wrapper =
      WTF::BindOnce(&EntryCallbacksSyncHelper::OnSuccess,
                    WrapPersistentIfNeeded(sync_helper));
  auto error_callback_wrapper = WTF::BindOnce(
      &EntryCallbacksSyncHelper::OnError, WrapPersistentIfNeeded(sync_helper));

  file_system_->Copy(this, parent, name, std::move(success_callback_wrapper),
                     std::move(error_callback_wrapper),
                     DOMFileSystemBase::kSynchronous);

  Entry* entry = sync_helper->GetResultOrThrow(exception_state);
  return entry ? EntrySync::Create(entry) : nullptr;
}

void EntrySync::remove(ExceptionState& exception_state) const {
  auto* sync_helper = MakeGarbageCollected<VoidCallbacksSyncHelper>();

  auto error_callback_wrapper = WTF::BindOnce(
      &VoidCallbacksSyncHelper::OnError, WrapPersistentIfNeeded(sync_helper));

  file_system_->Remove(this, VoidCallbacks::SuccessCallback(),
                       std::move(error_callback_wrapper),
                       DOMFileSystemBase::kSynchronous);
  sync_helper->GetResultOrThrow(exception_state);
}

DirectoryEntrySync* EntrySync::getParent() const {
  // Sync verion of getParent doesn't throw exceptions.
  String parent_path = DOMFilePath::GetDirectory(fullPath());
  return MakeGarbageCollected<DirectoryEntrySync>(file_system_, parent_path);
}

EntrySync::EntrySync(DOMFileSystemBase* file_system, const String& full_path)
    : EntryBase(file_system, full_path) {}

void EntrySync::Trace(Visitor* visitor) const {
  EntryBase::Trace(visitor);
}

}  // namespace blink
