// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_HANDLE_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_directory_handle.mojom-blink.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {
class FileSystemGetDirectoryOptions;
class FileSystemGetFileOptions;
class FileSystemRemoveOptions;
class NativeFileSystemDirectoryIterator;

class NativeFileSystemDirectoryHandle final : public NativeFileSystemHandle {
  DEFINE_WRAPPERTYPEINFO();

 public:
  NativeFileSystemDirectoryHandle(
      ExecutionContext* context,
      const String& name,
      mojo::PendingRemote<mojom::blink::NativeFileSystemDirectoryHandle>);

  // FileSystemDirectoryHandle IDL interface:
  NativeFileSystemDirectoryIterator* entries();
  NativeFileSystemDirectoryIterator* keys();
  NativeFileSystemDirectoryIterator* values();

  bool isDirectory() const override { return true; }

  ScriptPromise getFileHandle(ScriptState*,
                              const String& name,
                              const FileSystemGetFileOptions*);
  ScriptPromise getDirectoryHandle(ScriptState*,
                                   const String& name,
                                   const FileSystemGetDirectoryOptions*);
  ScriptValue getEntries(ScriptState*);
  ScriptPromise removeEntry(ScriptState*,
                            const String& name,
                            const FileSystemRemoveOptions*);

  ScriptPromise resolve(ScriptState*, NativeFileSystemHandle* possible_child);

  mojo::PendingRemote<mojom::blink::NativeFileSystemTransferToken> Transfer()
      override;

  mojom::blink::NativeFileSystemDirectoryHandle* MojoHandle() {
    return mojo_ptr_.get();
  }

  void Trace(Visitor*) const override;

 private:
  void QueryPermissionImpl(
      bool writable,
      base::OnceCallback<void(mojom::blink::PermissionStatus)>) override;
  void RequestPermissionImpl(
      bool writable,
      base::OnceCallback<void(mojom::blink::NativeFileSystemErrorPtr,
                              mojom::blink::PermissionStatus)>) override;
  // IsSameEntry for directories is implemented in terms of resolve, as resolve
  // also can be used to figure out if two directories are the same entry.
  void IsSameEntryImpl(
      mojo::PendingRemote<mojom::blink::NativeFileSystemTransferToken> other,
      base::OnceCallback<void(mojom::blink::NativeFileSystemErrorPtr, bool)>)
      override;

  HeapMojoRemote<mojom::blink::NativeFileSystemDirectoryHandle> mojo_ptr_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_HANDLE_H_
