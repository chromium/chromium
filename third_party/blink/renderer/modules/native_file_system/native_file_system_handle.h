// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_HANDLE_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_directory_handle.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_transfer_token.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
class ExecutionContext;
class FileSystemHandlePermissionDescriptor;

class NativeFileSystemHandle : public ScriptWrappable,
                               public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(NativeFileSystemHandle);

 public:
  NativeFileSystemHandle(ExecutionContext* execution_context,
                         const String& name);
  static NativeFileSystemHandle* CreateFromMojoEntry(
      mojom::blink::NativeFileSystemEntryPtr,
      ExecutionContext* execution_context);

  virtual bool isFile() const { return false; }
  virtual bool isDirectory() const { return false; }
  const String& name() const { return name_; }

  ScriptPromise queryPermission(ScriptState*,
                                const FileSystemHandlePermissionDescriptor*);
  ScriptPromise requestPermission(ScriptState*,
                                  const FileSystemHandlePermissionDescriptor*);

  // Grab a handle to a transfer token. This may return an invalid PendingRemote
  // if the context is already destroyed.
  virtual mojo::PendingRemote<mojom::blink::NativeFileSystemTransferToken>
  Transfer() = 0;

  void Trace(Visitor*) override;

 private:
  virtual void QueryPermissionImpl(
      bool writable,
      base::OnceCallback<void(mojom::blink::PermissionStatus)>) = 0;
  virtual void RequestPermissionImpl(
      bool writable,
      base::OnceCallback<void(mojom::blink::NativeFileSystemErrorPtr,
                              mojom::blink::PermissionStatus)>) = 0;

  String name_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_HANDLE_H_
