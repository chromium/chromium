// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_NATIVE_FILE_SYSTEM_FILE_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_NATIVE_FILE_SYSTEM_FILE_HANDLE_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_file_handle.mojom-blink.h"
#include "third_party/blink/renderer/modules/file_system_access/native_file_system_handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {
class FileSystemCreateWriterOptions;

class NativeFileSystemFileHandle final : public NativeFileSystemHandle {
  DEFINE_WRAPPERTYPEINFO();

 public:
  NativeFileSystemFileHandle(
      ExecutionContext* context,
      const String& name,
      mojo::PendingRemote<mojom::blink::NativeFileSystemFileHandle>);

  bool isFile() const override { return true; }

  ScriptPromise createWritable(ScriptState*,
                               const FileSystemCreateWriterOptions* options,
                               ExceptionState&);
  ScriptPromise getFile(ScriptState*, ExceptionState&);

  mojo::PendingRemote<mojom::blink::NativeFileSystemTransferToken> Transfer()
      override;

  mojom::blink::NativeFileSystemFileHandle* MojoHandle() {
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
  void IsSameEntryImpl(
      mojo::PendingRemote<mojom::blink::NativeFileSystemTransferToken> other,
      base::OnceCallback<void(mojom::blink::NativeFileSystemErrorPtr, bool)>)
      override;

  HeapMojoRemote<mojom::blink::NativeFileSystemFileHandle> mojo_ptr_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_NATIVE_FILE_SYSTEM_FILE_HANDLE_H_
