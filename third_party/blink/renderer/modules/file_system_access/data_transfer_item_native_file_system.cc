// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/data_transfer_item_native_file_system.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_directory_handle.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_drag_drop_token.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_error.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_file_handle.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_transfer_token.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/clipboard/data_object_item.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_item.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/file_system_access/native_file_system_directory_handle.h"
#include "third_party/blink/renderer/modules/file_system_access/native_file_system_error.h"
#include "third_party/blink/renderer/modules/file_system_access/native_file_system_file_handle.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// static
ScriptPromise DataTransferItemNativeFileSystem::getAsFileSystemHandle(
    ScriptState* script_state,
    DataTransferItem& data_transfer_item) {
  if (!data_transfer_item.GetDataTransfer()->CanReadData()) {
    return ScriptPromise::CastUndefined(script_state);
  }

  // If the DataObjectItem doesn't have an associated NativeFileSystemEntry,
  // return nullptr.
  if (!data_transfer_item.GetDataObjectItem()->HasNativeFileSystemEntry()) {
    return ScriptPromise::CastUndefined(script_state);
  }

  mojo::Remote<mojom::blink::NativeFileSystemManager> nfs_manager;
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  execution_context->GetBrowserInterfaceBroker().GetInterface(
      nfs_manager.BindNewPipeAndPassReceiver());

  const DataObjectItem& data_object_item =
      *data_transfer_item.GetDataObjectItem();

  // Since tokens are move-only, we need to create a clone in order
  // to preserve the state of `data_object_item` for future calls.
  mojo::PendingRemote<mojom::blink::NativeFileSystemDragDropToken>
      token_remote = data_object_item.CloneNativeFileSystemEntryToken();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  // We need to move `nfs_manager` into GetEntryFromDragDropToken in order
  // to keep it in scope long enough for the callback to be executed. To do this
  // we extract `raw_nfs_manager` from `nfs_manager` and move `nfs_manager` into
  // the GetEntryFromDragDropToken callback.
  auto* raw_nfs_manager = nfs_manager.get();
  raw_nfs_manager->GetEntryFromDragDropToken(
      std::move(token_remote),
      WTF::Bind(
          [](mojo::Remote<mojom::blink::NativeFileSystemManager>,
             ScriptPromiseResolver* resolver,
             mojom::blink::NativeFileSystemEntryPtr entry) {
            ScriptState* script_state = resolver->GetScriptState();
            if (!script_state)
              return;
            resolver->Resolve(NativeFileSystemHandle::CreateFromMojoEntry(
                std::move(entry), ExecutionContext::From(script_state)));
          },
          std::move(nfs_manager), WrapPersistent(resolver)));

  return result;
}

}  // namespace blink
