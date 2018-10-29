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

#include "third_party/blink/renderer/modules/filesystem/file_system_callbacks.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/modules/filesystem/directory_entry.h"
#include "third_party/blink/renderer/modules/filesystem/directory_reader.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_path.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system_base.h"
#include "third_party/blink/renderer/modules/filesystem/entry.h"
#include "third_party/blink/renderer/modules/filesystem/file_entry.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_base_handle.h"
#include "third_party/blink/renderer/modules/filesystem/file_writer.h"
#include "third_party/blink/renderer/modules/filesystem/metadata.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

FileSystemCallbacksBase::FileSystemCallbacksBase(
    ErrorCallbackBase* error_callback,
    DOMFileSystemBase* file_system,
    ExecutionContext* context)
    : error_callback_(error_callback),
      file_system_(file_system),
      execution_context_(context) {
  DCHECK(execution_context_);

  if (file_system_)
    file_system_->AddPendingCallbacks();
}

FileSystemCallbacksBase::~FileSystemCallbacksBase() {
  if (file_system_)
    file_system_->RemovePendingCallbacks();
}

void FileSystemCallbacksBase::DidFail(base::File::Error error) {
  if (error_callback_) {
    InvokeOrScheduleCallback(&ErrorCallbackBase::Invoke,
                             error_callback_.Release(), error);
  }
}

bool FileSystemCallbacksBase::ShouldScheduleCallback() const {
  return execution_context_ && execution_context_->IsContextPaused();
}

template <typename CallbackMemberFunction,
          typename CallbackClass,
          typename... Args>
void FileSystemCallbacksBase::InvokeOrScheduleCallback(
    CallbackMemberFunction&& callback_member_function,
    CallbackClass&& callback_object,
    Args&&... args) {
  DCHECK(callback_object);

  if (ShouldScheduleCallback()) {
    DOMFileSystem::ScheduleCallback(
        execution_context_.Get(),
        WTF::Bind(callback_member_function, WrapPersistent(callback_object),
                  WrapPersistentIfNeeded(args)...));
  } else {
    ((*callback_object).*callback_member_function)(args...);
  }
  execution_context_.Clear();
}

// ScriptErrorCallback --------------------------------------------------------

// static
ScriptErrorCallback* ScriptErrorCallback::Wrap(V8ErrorCallback* callback) {
  // DOMFileSystem operations take an optional (nullable) callback. If a
  // script callback was not passed, don't bother creating a dummy wrapper
  // and checking during invoke().
  if (!callback)
    return nullptr;
  return new ScriptErrorCallback(callback);
}

void ScriptErrorCallback::Trace(blink::Visitor* visitor) {
  ErrorCallbackBase::Trace(visitor);
  visitor->Trace(callback_);
}

void ScriptErrorCallback::Invoke(base::File::Error error) {
  callback_->InvokeAndReportException(nullptr,
                                      FileError::CreateDOMException(error));
};

ScriptErrorCallback::ScriptErrorCallback(V8ErrorCallback* callback)
    : callback_(ToV8PersistentCallbackInterface(callback)) {}

// PromiseErrorCallback -------------------------------------------------------

PromiseErrorCallback::PromiseErrorCallback(ScriptPromiseResolver* resolver)
    : resolver_(resolver) {}

void PromiseErrorCallback::Trace(Visitor* visitor) {
  ErrorCallbackBase::Trace(visitor);
  visitor->Trace(resolver_);
}

void PromiseErrorCallback::Invoke(base::File::Error error) {
  resolver_->Reject(FileError::CreateDOMException(error));
}

// EntryCallbacks -------------------------------------------------------------

void EntryCallbacks::OnDidGetEntryV8Impl::Trace(blink::Visitor* visitor) {
  visitor->Trace(callback_);
  OnDidGetEntryCallback::Trace(visitor);
}

void EntryCallbacks::OnDidGetEntryV8Impl::OnSuccess(Entry* entry) {
  callback_->InvokeAndReportException(nullptr, entry);
}

EntryCallbacks::OnDidGetEntryPromiseImpl::OnDidGetEntryPromiseImpl(
    ScriptPromiseResolver* resolver)
    : resolver_(resolver) {}

void EntryCallbacks::OnDidGetEntryPromiseImpl::Trace(Visitor* visitor) {
  OnDidGetEntryCallback::Trace(visitor);
  visitor->Trace(resolver_);
}

void EntryCallbacks::OnDidGetEntryPromiseImpl::OnSuccess(Entry* entry) {
  resolver_->Resolve(entry->asFileSystemHandle());
}

std::unique_ptr<AsyncFileSystemCallbacks> EntryCallbacks::Create(
    OnDidGetEntryCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context,
    DOMFileSystemBase* file_system,
    const String& expected_path,
    bool is_directory) {
  return base::WrapUnique(new EntryCallbacks(success_callback, error_callback,
                                             context, file_system,
                                             expected_path, is_directory));
}

EntryCallbacks::EntryCallbacks(OnDidGetEntryCallback* success_callback,
                               ErrorCallbackBase* error_callback,
                               ExecutionContext* context,
                               DOMFileSystemBase* file_system,
                               const String& expected_path,
                               bool is_directory)
    : FileSystemCallbacksBase(error_callback, file_system, context),
      success_callback_(success_callback),
      expected_path_(expected_path),
      is_directory_(is_directory) {}

void EntryCallbacks::DidSucceed() {
  if (!success_callback_)
    return;

  Entry* entry = is_directory_ ? static_cast<Entry*>(DirectoryEntry::Create(
                                     file_system_, expected_path_))
                               : static_cast<Entry*>(FileEntry::Create(
                                     file_system_, expected_path_));
  InvokeOrScheduleCallback(&OnDidGetEntryCallback::OnSuccess,
                           success_callback_.Release(), entry);
}

// EntriesCallbacks -----------------------------------------------------------

std::unique_ptr<AsyncFileSystemCallbacks> EntriesCallbacks::Create(
    OnDidGetEntriesCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context,
    DirectoryReaderBase* directory_reader,
    const String& base_path) {
  return base::WrapUnique(new EntriesCallbacks(
      success_callback, error_callback, context, directory_reader, base_path));
}

EntriesCallbacks::EntriesCallbacks(OnDidGetEntriesCallback* success_callback,
                                   ErrorCallbackBase* error_callback,
                                   ExecutionContext* context,
                                   DirectoryReaderBase* directory_reader,
                                   const String& base_path)
    : FileSystemCallbacksBase(error_callback,
                              directory_reader->Filesystem(),
                              context),
      success_callback_(success_callback),
      directory_reader_(directory_reader),
      base_path_(base_path),
      entries_(new HeapVector<Member<Entry>>()) {
  DCHECK(directory_reader_);
}

void EntriesCallbacks::DidReadDirectoryEntry(const String& name,
                                             bool is_directory) {
  DOMFileSystemBase* filesystem = directory_reader_->Filesystem();
  const String& path = DOMFilePath::Append(base_path_, name);
  Entry* entry =
      is_directory
          ? static_cast<Entry*>(DirectoryEntry::Create(filesystem, path))
          : static_cast<Entry*>(FileEntry::Create(filesystem, path));
  entries_->push_back(entry);
}

void EntriesCallbacks::DidReadDirectoryEntries(bool has_more) {
  directory_reader_->SetHasMoreEntries(has_more);
  EntryHeapVector* entries = new EntryHeapVector(std::move(*entries_));

  if (!success_callback_)
    return;

  InvokeOrScheduleCallback(&OnDidGetEntriesCallback::OnSuccess,
                           success_callback_.Get(), entries);
}

// FileSystemCallbacks --------------------------------------------------------

void FileSystemCallbacks::OnDidOpenFileSystemV8Impl::Trace(
    blink::Visitor* visitor) {
  visitor->Trace(callback_);
  OnDidOpenFileSystemCallback::Trace(visitor);
}

void FileSystemCallbacks::OnDidOpenFileSystemV8Impl::OnSuccess(
    DOMFileSystem* file_system) {
  callback_->InvokeAndReportException(nullptr, file_system);
}

FileSystemCallbacks::OnDidOpenFileSystemPromiseImpl::
    OnDidOpenFileSystemPromiseImpl(ScriptPromiseResolver* resolver)
    : resolver_(resolver) {}

void FileSystemCallbacks::OnDidOpenFileSystemPromiseImpl::Trace(
    Visitor* visitor) {
  OnDidOpenFileSystemCallback::Trace(visitor);
  visitor->Trace(resolver_);
}

void FileSystemCallbacks::OnDidOpenFileSystemPromiseImpl::OnSuccess(
    DOMFileSystem* file_system) {
  resolver_->Resolve(file_system->root()->asFileSystemHandle());
}

std::unique_ptr<AsyncFileSystemCallbacks> FileSystemCallbacks::Create(
    OnDidOpenFileSystemCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context,
    mojom::blink::FileSystemType type) {
  return base::WrapUnique(
      new FileSystemCallbacks(success_callback, error_callback, context, type));
}

FileSystemCallbacks::FileSystemCallbacks(
    OnDidOpenFileSystemCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context,
    mojom::blink::FileSystemType type)
    : FileSystemCallbacksBase(error_callback, nullptr, context),
      success_callback_(success_callback),
      type_(type) {}

void FileSystemCallbacks::DidOpenFileSystem(const String& name,
                                            const KURL& root_url) {
  if (!success_callback_)
    return;

  InvokeOrScheduleCallback(
      &OnDidOpenFileSystemCallback::OnSuccess, success_callback_.Release(),
      DOMFileSystem::Create(execution_context_.Get(), name, type_, root_url));
}

// ResolveURICallbacks --------------------------------------------------------

std::unique_ptr<AsyncFileSystemCallbacks> ResolveURICallbacks::Create(
    OnDidGetEntryCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context) {
  return base::WrapUnique(
      new ResolveURICallbacks(success_callback, error_callback, context));
}

ResolveURICallbacks::ResolveURICallbacks(
    OnDidGetEntryCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context)
    : FileSystemCallbacksBase(error_callback, nullptr, context),
      success_callback_(success_callback) {
  DCHECK(success_callback_);
}

void ResolveURICallbacks::DidResolveURL(const String& name,
                                        const KURL& root_url,
                                        mojom::blink::FileSystemType type,
                                        const String& file_path,
                                        bool is_directory) {
  DOMFileSystem* filesystem =
      DOMFileSystem::Create(execution_context_.Get(), name, type, root_url);
  DirectoryEntry* root = filesystem->root();

  String absolute_path;
  if (!DOMFileSystemBase::PathToAbsolutePath(type, root, file_path,
                                             absolute_path)) {
    DidFail(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  Entry* entry =
      is_directory
          ? static_cast<Entry*>(
                DirectoryEntry::Create(filesystem, absolute_path))
          : static_cast<Entry*>(FileEntry::Create(filesystem, absolute_path));
  InvokeOrScheduleCallback(&OnDidGetEntryCallback::OnSuccess,
                           success_callback_.Release(), entry);
}

// MetadataCallbacks ----------------------------------------------------------

void MetadataCallbacks::OnDidReadMetadataV8Impl::Trace(
    blink::Visitor* visitor) {
  visitor->Trace(callback_);
  OnDidReadMetadataCallback::Trace(visitor);
}

void MetadataCallbacks::OnDidReadMetadataV8Impl::OnSuccess(Metadata* metadata) {
  callback_->InvokeAndReportException(nullptr, metadata);
}

std::unique_ptr<AsyncFileSystemCallbacks> MetadataCallbacks::Create(
    OnDidReadMetadataCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context,
    DOMFileSystemBase* file_system) {
  return base::WrapUnique(new MetadataCallbacks(
      success_callback, error_callback, context, file_system));
}

MetadataCallbacks::MetadataCallbacks(
    OnDidReadMetadataCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context,
    DOMFileSystemBase* file_system)
    : FileSystemCallbacksBase(error_callback, file_system, context),
      success_callback_(success_callback) {}

void MetadataCallbacks::DidReadMetadata(const FileMetadata& metadata) {
  if (!success_callback_)
    return;

  InvokeOrScheduleCallback(&OnDidReadMetadataCallback::OnSuccess,
                           success_callback_.Release(),
                           Metadata::Create(metadata));
}

// FileWriterCallbacks ----------------------------------------------------

void FileWriterCallbacks::OnDidCreateFileWriterV8Impl::Trace(
    blink::Visitor* visitor) {
  visitor->Trace(callback_);
  OnDidCreateFileWriterCallback::Trace(visitor);
}

void FileWriterCallbacks::OnDidCreateFileWriterV8Impl::OnSuccess(
    FileWriterBase* file_writer) {
  // The call sites must pass a FileWriter in |file_writer|.
  callback_->InvokeAndReportException(nullptr,
                                      static_cast<FileWriter*>(file_writer));
}

std::unique_ptr<AsyncFileSystemCallbacks> FileWriterCallbacks::Create(
    FileWriterBase* file_writer,
    OnDidCreateFileWriterCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context) {
  return base::WrapUnique(new FileWriterCallbacks(file_writer, success_callback,
                                                  error_callback, context));
}

FileWriterCallbacks::FileWriterCallbacks(
    FileWriterBase* file_writer,
    OnDidCreateFileWriterCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context)
    : FileSystemCallbacksBase(error_callback, nullptr, context),
      file_writer_(file_writer),
      success_callback_(success_callback) {}

void FileWriterCallbacks::DidCreateFileWriter(const KURL& path,
                                              long long length) {
  if (!success_callback_)
    return;
  file_writer_->Initialize(path, length);
  InvokeOrScheduleCallback(&OnDidCreateFileWriterCallback::OnSuccess,
                           success_callback_.Release(), file_writer_);
}

// SnapshotFileCallback -------------------------------------------------------

void SnapshotFileCallback::OnDidCreateSnapshotFileV8Impl::Trace(
    blink::Visitor* visitor) {
  visitor->Trace(callback_);
  OnDidCreateSnapshotFileCallback::Trace(visitor);
}

void SnapshotFileCallback::OnDidCreateSnapshotFileV8Impl::OnSuccess(
    File* file) {
  callback_->InvokeAndReportException(nullptr, file);
}

std::unique_ptr<AsyncFileSystemCallbacks> SnapshotFileCallback::Create(
    DOMFileSystemBase* filesystem,
    const String& name,
    const KURL& url,
    OnDidCreateSnapshotFileCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context) {
  return base::WrapUnique(new SnapshotFileCallback(
      filesystem, name, url, success_callback, error_callback, context));
}

SnapshotFileCallback::SnapshotFileCallback(
    DOMFileSystemBase* filesystem,
    const String& name,
    const KURL& url,
    OnDidCreateSnapshotFileCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context)
    : FileSystemCallbacksBase(error_callback, filesystem, context),
      name_(name),
      url_(url),
      success_callback_(success_callback) {}

void SnapshotFileCallback::DidCreateSnapshotFile(
    const FileMetadata& metadata,
    scoped_refptr<BlobDataHandle> snapshot) {
  if (!success_callback_)
    return;

  // We can't directly use the snapshot blob data handle because the content
  // type on it hasn't been set.  The |snapshot| param is here to provide a
  // chain of custody thru thread bridging that is held onto until *after* we've
  // coined a File with a new handle that has the correct type set on it. This
  // allows the blob storage system to track when a temp file can and can't be
  // safely deleted.

  InvokeOrScheduleCallback(&OnDidCreateSnapshotFileCallback::OnSuccess,
                           success_callback_.Release(),
                           DOMFileSystemBase::CreateFile(
                               metadata, url_, file_system_->GetType(), name_));
}

// VoidCallbacks --------------------------------------------------------------

void VoidCallbacks::OnDidSucceedV8Impl::Trace(blink::Visitor* visitor) {
  visitor->Trace(callback_);
  OnDidSucceedCallback::Trace(visitor);
}

void VoidCallbacks::OnDidSucceedV8Impl::OnSuccess(
    ExecutionContext* dummy_arg_for_sync_helper) {
  callback_->InvokeAndReportException(nullptr);
}

VoidCallbacks::OnDidSucceedPromiseImpl::OnDidSucceedPromiseImpl(
    ScriptPromiseResolver* resolver)
    : resolver_(resolver) {}

void VoidCallbacks::OnDidSucceedPromiseImpl::Trace(Visitor* visitor) {
  OnDidSucceedCallback::Trace(visitor);
  visitor->Trace(resolver_);
}

void VoidCallbacks::OnDidSucceedPromiseImpl::OnSuccess(ExecutionContext*) {
  resolver_->Resolve();
}

std::unique_ptr<AsyncFileSystemCallbacks> VoidCallbacks::Create(
    OnDidSucceedCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context,
    DOMFileSystemBase* file_system) {
  return base::WrapUnique(new VoidCallbacks(success_callback, error_callback,
                                            context, file_system));
}

VoidCallbacks::VoidCallbacks(OnDidSucceedCallback* success_callback,
                             ErrorCallbackBase* error_callback,
                             ExecutionContext* context,
                             DOMFileSystemBase* file_system)
    : FileSystemCallbacksBase(error_callback, file_system, context),
      success_callback_(success_callback) {}

void VoidCallbacks::DidSucceed() {
  if (!success_callback_)
    return;

  InvokeOrScheduleCallback(&OnDidSucceedCallback::OnSuccess,
                           success_callback_.Release(),
                           execution_context_.Get());
}

}  // namespace blink
