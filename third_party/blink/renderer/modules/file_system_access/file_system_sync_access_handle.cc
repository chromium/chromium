// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_sync_access_handle.h"

namespace blink {

FileSystemSyncAccessHandle::FileSystemSyncAccessHandle(
    ExecutionContext* context,
    FileSystemAccessFileDelegate* file_delegate,
    mojo::PendingRemote<mojom::blink::FileSystemAccessAccessHandleHost>
        access_handle_remote)
    : file_delegate_(file_delegate), access_handle_remote_(context) {
  access_handle_remote_.Bind(
      std::move(access_handle_remote),
      context->GetTaskRunner(TaskType::kMiscPlatformAPI));
  DCHECK(access_handle_remote_.is_bound());
}

ScriptPromise FileSystemSyncAccessHandle::close(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();

  // TODO(fivedots): Add logic to close file delegate, and deal with
  // closures during IO operations, as done in Storage Foundation API.

  if (!access_handle_remote_.is_bound()) {
    // If the backend went away, no need to tell it that the handle was closed.
    resolver->Resolve();
    return promise;
  }

  access_handle_remote_->Close(
      WTF::Bind([](ScriptPromiseResolver* resolver) { resolver->Resolve(); },
                WrapPersistent(resolver)));
  return promise;
}

void FileSystemSyncAccessHandle::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(file_delegate_);
  visitor->Trace(access_handle_remote_);
}

}  // namespace blink
