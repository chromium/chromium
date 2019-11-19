// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_HANDLE_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_directory_handle.mojom-blink.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_handle.h"

namespace blink {
class FileSystemGetDirectoryOptions;
class FileSystemGetFileOptions;
class FileSystemRemoveOptions;
class GetSystemDirectoryOptions;

class NativeFileSystemDirectoryHandle final : public NativeFileSystemHandle {
  DEFINE_WRAPPERTYPEINFO();

 public:
  NativeFileSystemDirectoryHandle(
      ExecutionContext* context,
      const String& name,
      mojo::PendingRemote<mojom::blink::NativeFileSystemDirectoryHandle>);

  bool isDirectory() const override { return true; }

  ScriptPromise getFile(ScriptState*,
                        const String& name,
                        const FileSystemGetFileOptions*);
  ScriptPromise getDirectory(ScriptState*,
                             const String& name,
                             const FileSystemGetDirectoryOptions*);
  ScriptValue getEntries(ScriptState*);
  ScriptPromise removeEntry(ScriptState*,
                            const String& name,
                            const FileSystemRemoveOptions*);

  static ScriptPromise getSystemDirectory(ScriptState*,
                                          const GetSystemDirectoryOptions*);

  mojo::PendingRemote<mojom::blink::NativeFileSystemTransferToken> Transfer()
      override;

  mojom::blink::NativeFileSystemDirectoryHandle* MojoHandle() {
    return mojo_ptr_.get();
  }

  void ContextDestroyed(ExecutionContext*) override;

 private:
  void QueryPermissionImpl(
      bool writable,
      base::OnceCallback<void(mojom::blink::PermissionStatus)>) override;
  void RequestPermissionImpl(
      bool writable,
      base::OnceCallback<void(mojom::blink::NativeFileSystemErrorPtr,
                              mojom::blink::PermissionStatus)>) override;

  mojo::Remote<mojom::blink::NativeFileSystemDirectoryHandle> mojo_ptr_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_DIRECTORY_HANDLE_H_
