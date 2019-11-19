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

#include "third_party/blink/renderer/modules/filesystem/dom_file_system_sync.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/modules/filesystem/directory_entry_sync.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_path.h"
#include "third_party/blink/renderer/modules/filesystem/file_entry_sync.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_callbacks.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_dispatcher.h"
#include "third_party/blink/renderer/modules/filesystem/file_writer_sync.h"
#include "third_party/blink/renderer/modules/filesystem/sync_callback_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/file_metadata.h"

namespace blink {

class FileWriterBase;

DOMFileSystemSync::DOMFileSystemSync(DOMFileSystemBase* file_system)
    : DOMFileSystemSync(file_system->context_,
                        file_system->name(),
                        file_system->GetType(),
                        file_system->RootURL()) {}

DOMFileSystemSync::DOMFileSystemSync(ExecutionContext* context,
                                     const String& name,
                                     mojom::blink::FileSystemType type,
                                     const KURL& root_url)
    : DOMFileSystemBase(context, name, type, root_url),
      root_entry_(
          MakeGarbageCollected<DirectoryEntrySync>(this, DOMFilePath::kRoot)) {}

DOMFileSystemSync::~DOMFileSystemSync() = default;

void DOMFileSystemSync::ReportError(ErrorCallback error_callback,
                                    base::File::Error error) {
  std::move(error_callback).Run(error);
}

DirectoryEntrySync* DOMFileSystemSync::root() {
  return root_entry_.Get();
}

namespace {

class CreateFileHelper final : public SnapshotFileCallbackBase {
 public:
  class CreateFileResult : public GarbageCollected<CreateFileResult> {
   public:
    static CreateFileResult* Create() {
      return MakeGarbageCollected<CreateFileResult>();
    }

    CreateFileResult() : failed_(false), error_(base::File::FILE_OK) {}

    bool failed_;
    base::File::Error error_;
    Member<File> file_;

    void Trace(blink::Visitor* visitor) { visitor->Trace(file_); }
  };

  static std::unique_ptr<SnapshotFileCallbackBase> Create(
      CreateFileResult* result,
      const String& name,
      const KURL& url,
      mojom::blink::FileSystemType type) {
    return base::WrapUnique(static_cast<SnapshotFileCallbackBase*>(
        new CreateFileHelper(result, name, url, type)));
  }

  void DidFail(base::File::Error error) override {
    result_->failed_ = true;
    result_->error_ = error;
  }

  ~CreateFileHelper() override = default;

  void DidCreateSnapshotFile(const FileMetadata& metadata,
                             scoped_refptr<BlobDataHandle> snapshot) override {
    // We can't directly use the snapshot blob data handle because the content
    // type on it hasn't been set.  The |snapshot| param is here to provide a a
    // chain of custody thru thread bridging that is held onto until *after*
    // we've coined a File with a new handle that has the correct type set on
    // it. This allows the blob storage system to track when a temp file can and
    // can't be safely deleted.

    result_->file_ =
        DOMFileSystemBase::CreateFile(metadata, url_, type_, name_);
  }

 private:
  CreateFileHelper(CreateFileResult* result,
                   const String& name,
                   const KURL& url,
                   mojom::blink::FileSystemType type)
      : result_(result), name_(name), url_(url), type_(type) {}

  Persistent<CreateFileResult> result_;
  String name_;
  KURL url_;
  mojom::blink::FileSystemType type_;
};

}  // namespace

File* DOMFileSystemSync::CreateFile(const FileEntrySync* file_entry,
                                    ExceptionState& exception_state) {
  KURL file_system_url = CreateFileSystemURL(file_entry);
  CreateFileHelper::CreateFileResult* result(
      CreateFileHelper::CreateFileResult::Create());
  FileSystemDispatcher::From(context_).CreateSnapshotFileSync(
      file_system_url, CreateFileHelper::Create(result, file_entry->name(),
                                                file_system_url, GetType()));
  if (result->failed_) {
    file_error::ThrowDOMException(
        exception_state, result->error_,
        "Could not create '" + file_entry->name() + "'.");
    return nullptr;
  }
  return result->file_.Get();
}

FileWriterSync* DOMFileSystemSync::CreateWriter(
    const FileEntrySync* file_entry,
    ExceptionState& exception_state) {
  DCHECK(file_entry);

  auto* file_writer = MakeGarbageCollected<FileWriterSync>(context_);

  auto* sync_helper = MakeGarbageCollected<FileWriterCallbacksSyncHelper>();

  auto success_callback_wrapper =
      WTF::Bind(&FileWriterCallbacksSyncHelper::OnSuccess,
                WrapPersistentIfNeeded(sync_helper));
  auto error_callback_wrapper =
      WTF::Bind(&FileWriterCallbacksSyncHelper::OnError,
                WrapPersistentIfNeeded(sync_helper));

  auto callbacks = std::make_unique<FileWriterCallbacks>(
      file_writer, std::move(success_callback_wrapper),
      std::move(error_callback_wrapper), context_);

  FileSystemDispatcher::From(context_).InitializeFileWriterSync(
      CreateFileSystemURL(file_entry), std::move(callbacks));

  FileWriterBase* success = sync_helper->GetResultOrThrow(exception_state);
  return success ? file_writer : nullptr;
}

void DOMFileSystemSync::Trace(blink::Visitor* visitor) {
  visitor->Trace(root_entry_);
  DOMFileSystemBase::Trace(visitor);
}

}  // namespace blink
