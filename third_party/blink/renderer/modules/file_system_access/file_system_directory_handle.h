// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_DIRECTORY_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_DIRECTORY_HANDLE_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom-blink.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_handle.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {
class FileSystemGetDirectoryOptions;
class FileSystemGetFileOptions;
class FileSystemRemoveOptions;
class FileSystemDirectoryIterator;

class FileSystemDirectoryHandle final : public FileSystemHandle {
  DEFINE_WRAPPERTYPEINFO();

 public:
  FileSystemDirectoryHandle(
      ExecutionContext* context,
      const String& name,
      mojo::PendingRemote<mojom::blink::FileSystemAccessDirectoryHandle>);

  // FileSystemDirectoryHandle IDL interface:
  FileSystemDirectoryIterator* entries(ExceptionState&);
  FileSystemDirectoryIterator* keys(ExceptionState&);
  FileSystemDirectoryIterator* values(ExceptionState&);

  bool isDirectory() const override { return true; }

  ScriptPromise getFileHandle(ScriptState*,
                              const String& name,
                              const FileSystemGetFileOptions*,
                              ExceptionState&);
  ScriptPromise getDirectoryHandle(ScriptState*,
                                   const String& name,
                                   const FileSystemGetDirectoryOptions*,
                                   ExceptionState&);
  ScriptPromise removeEntry(ScriptState*,
                            const String& name,
                            const FileSystemRemoveOptions*,
                            ExceptionState&);

  ScriptPromise resolve(ScriptState*,
                        FileSystemHandle* possible_child,
                        ExceptionState&);

  mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken> Transfer()
      override;

  mojom::blink::FileSystemAccessDirectoryHandle* MojoHandle() {
    return mojo_ptr_.get();
  }

  void Trace(Visitor*) const override;

 private:
  void QueryPermissionImpl(
      bool writable,
      base::OnceCallback<void(mojom::blink::PermissionStatus)>) override;
  void RequestPermissionImpl(
      bool writable,
      base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr,
                              mojom::blink::PermissionStatus)>) override;
  void MoveImpl(
      mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken> dest,
      const String& new_entry_name,
      base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr)>)
      override;
  void RemoveImpl(
      const FileSystemRemoveOptions* options,
      base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr)>)
      override;
  // IsSameEntry for directories is implemented in terms of resolve, as resolve
  // also can be used to figure out if two directories are the same entry.
  void IsSameEntryImpl(
      mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken> other,
      base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr, bool)>)
      override;
  void GetUniqueIdImpl(
      base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr,
                              const WTF::String&)>) override;
  void GetCloudIdentifiersImpl(
      base::OnceCallback<void(
          mojom::blink::FileSystemAccessErrorPtr,
          Vector<mojom::blink::FileSystemAccessCloudIdentifierPtr>)>) override;

  HeapMojoRemote<mojom::blink::FileSystemAccessDirectoryHandle> mojo_ptr_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_DIRECTORY_HANDLE_H_
