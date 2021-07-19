// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_SYNC_ACCESS_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_SYNC_ACCESS_HANDLE_H_

#include "third_party/blink/public/mojom/file_system_access/file_system_access_access_handle_host.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_file_delegate.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class FileSystemSyncAccessHandle final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit FileSystemSyncAccessHandle(
      ExecutionContext* context,
      FileSystemAccessFileDelegate* file_delegate,
      mojo::PendingRemote<mojom::blink::FileSystemAccessAccessHandleHost>
          access_handle_host);

  FileSystemSyncAccessHandle(const FileSystemSyncAccessHandle&) = delete;
  FileSystemSyncAccessHandle& operator=(const FileSystemSyncAccessHandle&) =
      delete;

  ScriptPromise close(ScriptState*);

  // GarbageCollected
  void Trace(Visitor* visitor) const override;

 private:
  // Interface that provides file-like access to the backing storage.
  Member<FileSystemAccessFileDelegate> file_delegate_;

  // Mojo pipe that holds the renderer's write lock on the file.
  HeapMojoRemote<mojom::blink::FileSystemAccessAccessHandleHost>
      access_handle_remote_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_SYNC_ACCESS_HANDLE_H_
