// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_FILE_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_FILE_HANDLE_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom-blink.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {
class FileSystemCreateWritableOptions;

class FileSystemFileHandle final : public FileSystemHandle {
  DEFINE_WRAPPERTYPEINFO();

 public:
  FileSystemFileHandle(
      ExecutionContext* context,
      const String& name,
      mojo::PendingRemote<mojom::blink::FileSystemAccessFileHandle>);

  bool isFile() const override { return true; }

  ScriptPromise createWritable(ScriptState*,
                               const FileSystemCreateWritableOptions* options,
                               ExceptionState&);
  ScriptPromise getFile(ScriptState*, ExceptionState&);

  // TODO(fivedots): Define if this method should be generally exposed or only
  // on files backed by the Origin Private File System.
  ScriptPromise createSyncAccessHandle(ScriptState*, ExceptionState&);

  mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken> Transfer()
      override;

  mojom::blink::FileSystemAccessFileHandle* MojoHandle() {
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
  void IsSameEntryImpl(
      mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken> other,
      base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr, bool)>)
      override;
  void GetUniqueIdImpl(base::OnceCallback<void(const WTF::String&)>) override;

  HeapMojoRemote<mojom::blink::FileSystemAccessFileHandle> mojo_ptr_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_FILE_HANDLE_H_
