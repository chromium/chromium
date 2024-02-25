// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_STORAGE_MANAGER_FILE_SYSTEM_ACCESS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_STORAGE_MANAGER_FILE_SYSTEM_ACCESS_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-blink.h"

namespace blink {

class FileSystemDirectoryHandle;
class ExecutionContext;
class ExceptionState;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;
class StorageManager;

class StorageManagerFileSystemAccess {
  STATIC_ONLY(StorageManagerFileSystemAccess);

 public:
  static ScriptPromise getDirectory(ScriptState*,
                                    const StorageManager&,
                                    ExceptionState&);

  // Called to execute checks, both renderer side and browser side, that OPFS is
  // allowed. Will execute `on_allowed` with the result of browser side checks
  // if it gets that far.
  static ScriptPromise CheckGetDirectoryIsAllowed(
      ScriptState* script_state,
      ExceptionState& exception_state,
      base::OnceCallback<void(ScriptPromiseResolver*)> on_allowed);
  static void CheckGetDirectoryIsAllowed(
      ExecutionContext* context,
      base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr)>
          callback);

  // Handles resolving the `getDirectory` promise represented by `resolver`.
  static void DidGetSandboxedFileSystem(
      ScriptPromiseResolver* resolver,
      mojom::blink::FileSystemAccessErrorPtr result,
      mojo::PendingRemote<mojom::blink::FileSystemAccessDirectoryHandle>
          handle);

  static void DidGetSandboxedFileSystemForDevtools(
      ExecutionContext* context,
      base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr,
                              FileSystemDirectoryHandle*)> callback,
      mojom::blink::FileSystemAccessErrorPtr result,
      mojo::PendingRemote<mojom::blink::FileSystemAccessDirectoryHandle>
          handle);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_STORAGE_MANAGER_FILE_SYSTEM_ACCESS_H_
