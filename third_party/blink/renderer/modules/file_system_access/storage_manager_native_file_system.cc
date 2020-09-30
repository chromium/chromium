// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/storage_manager_native_file_system.h"

#include <utility>

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/modules/file_system_access/native_file_system_directory_handle.h"
#include "third_party/blink/renderer/modules/file_system_access/native_file_system_error.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {
// The name to use for the root directory of a sandboxed file system.
constexpr const char kSandboxRootDirectoryName[] = "";

}  // namespace

// static
ScriptPromise StorageManagerNativeFileSystem::getDirectory(
    ScriptState* script_state,
    const StorageManager& storage,
    ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->GetSecurityOrigin()->CanAccessNativeFileSystem()) {
    if (context->IsSandboxed(network::mojom::blink::WebSandboxFlags::kOrigin)) {
      exception_state.ThrowSecurityError(
          "Storage directory access is denied because the context is "
          "sandboxed and lacks the 'allow-same-origin' flag.");
      return ScriptPromise();
    } else {
      exception_state.ThrowSecurityError("Storage directory access is denied.");
      return ScriptPromise();
    }
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  mojo::Remote<mojom::blink::NativeFileSystemManager> manager;
  context->GetBrowserInterfaceBroker().GetInterface(
      manager.BindNewPipeAndPassReceiver());

  auto* raw_manager = manager.get();
  raw_manager->GetSandboxedFileSystem(WTF::Bind(
      [](ScriptPromiseResolver* resolver,
         mojo::Remote<mojom::blink::NativeFileSystemManager>,
         mojom::blink::NativeFileSystemErrorPtr result,
         mojo::PendingRemote<mojom::blink::NativeFileSystemDirectoryHandle>
             handle) {
        ExecutionContext* context = resolver->GetExecutionContext();
        if (!context)
          return;
        if (result->status != mojom::blink::NativeFileSystemStatus::kOk) {
          native_file_system_error::Reject(resolver, *result);
          return;
        }
        resolver->Resolve(MakeGarbageCollected<NativeFileSystemDirectoryHandle>(
            context, kSandboxRootDirectoryName, std::move(handle)));
      },
      WrapPersistent(resolver), std::move(manager)));

  return result;
}

}  // namespace blink
