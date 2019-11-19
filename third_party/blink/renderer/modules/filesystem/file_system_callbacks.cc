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
#include <utility>

#include "base/memory/ptr_util.h"
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
#include "third_party/blink/renderer/modules/filesystem/file_writer.h"
#include "third_party/blink/renderer/modules/filesystem/metadata.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

FileSystemCallbacksBase::FileSystemCallbacksBase(DOMFileSystemBase* file_system,
                                                 ExecutionContext* context)
    : file_system_(file_system), execution_context_(context) {
  DCHECK(execution_context_);

  if (file_system_)
    file_system_->AddPendingCallbacks();
}

FileSystemCallbacksBase::~FileSystemCallbacksBase() {
  if (file_system_)
    file_system_->RemovePendingCallbacks();
}

// EntryCallbacks -------------------------------------------------------------

EntryCallbacks::EntryCallbacks(SuccessCallback success_callback,
                               ErrorCallback error_callback,
                               ExecutionContext* context,
                               DOMFileSystemBase* file_system,
                               const String& expected_path,
                               bool is_directory)
    : FileSystemCallbacksBase(file_system, context),
      success_callback_(std::move(success_callback)),
      error_callback_(std::move(error_callback)),
      expected_path_(expected_path),
      is_directory_(is_directory) {}

void EntryCallbacks::DidSucceed() {
  if (!success_callback_)
    return;

  Entry* entry = is_directory_
                     ? static_cast<Entry*>(MakeGarbageCollected<DirectoryEntry>(
                           file_system_, expected_path_))
                     : static_cast<Entry*>(MakeGarbageCollected<FileEntry>(
                           file_system_, expected_path_));

  std::move(success_callback_).Run(entry);
}

void EntryCallbacks::DidFail(base::File::Error error) {
  if (!error_callback_)
    return;

  std::move(error_callback_).Run(error);
}

// EntriesCallbacks -----------------------------------------------------------

EntriesCallbacks::EntriesCallbacks(const SuccessCallback& success_callback,
                                   ErrorCallback error_callback,
                                   ExecutionContext* context,
                                   DirectoryReaderBase* directory_reader,
                                   const String& base_path)
    : FileSystemCallbacksBase(directory_reader->Filesystem(), context),
      success_callback_(success_callback),
      error_callback_(std::move(error_callback)),
      directory_reader_(directory_reader),
      base_path_(base_path),
      entries_(MakeGarbageCollected<HeapVector<Member<Entry>>>()) {
  DCHECK(directory_reader_);
}

void EntriesCallbacks::DidReadDirectoryEntry(const String& name,
                                             bool is_directory) {
  DOMFileSystemBase* filesystem = directory_reader_->Filesystem();
  const String& path = DOMFilePath::Append(base_path_, name);
  Entry* entry = is_directory
                     ? static_cast<Entry*>(MakeGarbageCollected<DirectoryEntry>(
                           filesystem, path))
                     : static_cast<Entry*>(
                           MakeGarbageCollected<FileEntry>(filesystem, path));
  entries_->push_back(entry);
}

void EntriesCallbacks::DidReadDirectoryEntries(bool has_more) {
  directory_reader_->SetHasMoreEntries(has_more);
  EntryHeapVector* entries =
      MakeGarbageCollected<EntryHeapVector>(std::move(*entries_));

  if (!success_callback_) {
    return;
  }

  success_callback_.Run(entries);
}

void EntriesCallbacks::DidFail(base::File::Error error) {
  if (!error_callback_) {
    return;
  }

  std::move(error_callback_).Run(error);
}

// FileSystemCallbacks --------------------------------------------------------

FileSystemCallbacks::FileSystemCallbacks(SuccessCallback success_callback,
                                         ErrorCallback error_callback,
                                         ExecutionContext* context,
                                         mojom::blink::FileSystemType type)
    : FileSystemCallbacksBase(
          /*file_system=*/nullptr,
          context),
      success_callback_(std::move(success_callback)),
      error_callback_(std::move(error_callback)),
      type_(type) {}

void FileSystemCallbacks::DidOpenFileSystem(const String& name,
                                            const KURL& root_url) {
  if (!success_callback_)
    return;

  auto* filesystem = MakeGarbageCollected<DOMFileSystem>(
      execution_context_.Get(), name, type_, root_url);
  std::move(success_callback_).Run(filesystem);
}

void FileSystemCallbacks::DidFail(base::File::Error error) {
  if (!error_callback_)
    return;

  std::move(error_callback_).Run(error);
}

// ResolveURICallbacks --------------------------------------------------------

ResolveURICallbacks::ResolveURICallbacks(SuccessCallback success_callback,
                                         ErrorCallback error_callback,
                                         ExecutionContext* context)
    : FileSystemCallbacksBase(
          /*file_system=*/nullptr,
          context),
      success_callback_(std::move(success_callback)),
      error_callback_(std::move(error_callback)) {
  DCHECK(success_callback_);
}

void ResolveURICallbacks::DidResolveURL(const String& name,
                                        const KURL& root_url,
                                        mojom::blink::FileSystemType type,
                                        const String& file_path,
                                        bool is_directory) {
  auto* filesystem = MakeGarbageCollected<DOMFileSystem>(
      execution_context_.Get(), name, type, root_url);
  DirectoryEntry* root = filesystem->root();

  String absolute_path;
  if (!DOMFileSystemBase::PathToAbsolutePath(type, root, file_path,
                                             absolute_path)) {
    DidFail(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  Entry* entry = is_directory
                     ? static_cast<Entry*>(MakeGarbageCollected<DirectoryEntry>(
                           filesystem, absolute_path))
                     : static_cast<Entry*>(MakeGarbageCollected<FileEntry>(
                           filesystem, absolute_path));

  std::move(success_callback_).Run(entry);
}

void ResolveURICallbacks::DidFail(base::File::Error error) {
  if (!error_callback_)
    return;

  std::move(error_callback_).Run(error);
}

// MetadataCallbacks ----------------------------------------------------------

MetadataCallbacks::MetadataCallbacks(SuccessCallback success_callback,
                                     ErrorCallback error_callback,
                                     ExecutionContext* context,
                                     DOMFileSystemBase* file_system)
    : FileSystemCallbacksBase(file_system, context),
      success_callback_(std::move(success_callback)),
      error_callback_(std::move(error_callback)) {}

void MetadataCallbacks::DidReadMetadata(const FileMetadata& metadata) {
  if (!success_callback_)
    return;

  std::move(success_callback_).Run(MakeGarbageCollected<Metadata>(metadata));
}

void MetadataCallbacks::DidFail(base::File::Error error) {
  if (!error_callback_)
    return;

  std::move(error_callback_).Run(error);
}

// FileWriterCallbacks ----------------------------------------------------

FileWriterCallbacks::FileWriterCallbacks(FileWriterBase* file_writer,
                                         SuccessCallback success_callback,
                                         ErrorCallback error_callback,
                                         ExecutionContext* context)
    : FileSystemCallbacksBase(
          /*file_system =*/nullptr,
          context),
      file_writer_(file_writer),
      success_callback_(std::move(success_callback)),
      error_callback_(std::move(error_callback)) {}

void FileWriterCallbacks::DidCreateFileWriter(const KURL& path,
                                              int64_t length) {
  if (!success_callback_)
    return;

  file_writer_->Initialize(path, length);
  std::move(success_callback_).Run(file_writer_);
}

void FileWriterCallbacks::DidFail(base::File::Error error) {
  if (!error_callback_)
    return;

  std::move(error_callback_).Run(error);
}

// SnapshotFileCallback -------------------------------------------------------

SnapshotFileCallback::SnapshotFileCallback(DOMFileSystemBase* filesystem,
                                           const String& name,
                                           const KURL& url,
                                           SuccessCallback success_callback,
                                           ErrorCallback error_callback,
                                           ExecutionContext* context)
    : FileSystemCallbacksBase(filesystem, context),
      name_(name),
      url_(url),
      success_callback_(std::move(success_callback)),
      error_callback_(std::move(error_callback)) {}

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
  std::move(success_callback_)
      .Run(DOMFileSystemBase::CreateFile(metadata, url_,
                                         file_system_->GetType(), name_));
}

void SnapshotFileCallback::DidFail(base::File::Error error) {
  if (!error_callback_)
    return;

  std::move(error_callback_).Run(error);
}

// VoidCallbacks --------------------------------------------------------------

VoidCallbacks::VoidCallbacks(SuccessCallback success_callback,
                             ErrorCallback error_callback,
                             ExecutionContext* context,
                             DOMFileSystemBase* file_system)
    : FileSystemCallbacksBase(file_system, context),
      success_callback_(std::move(success_callback)),
      error_callback_(std::move(error_callback)) {}

void VoidCallbacks::DidSucceed() {
  if (!success_callback_)
    return;

  std::move(success_callback_).Run();
}

void VoidCallbacks::DidFail(base::File::Error error) {
  if (!error_callback_)
    return;

  std::move(error_callback_).Run(error);
}

}  // namespace blink
