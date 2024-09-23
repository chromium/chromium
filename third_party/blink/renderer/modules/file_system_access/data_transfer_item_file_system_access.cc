// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/data_transfer_item_file_system_access.h"

#include "third_party/blink/public/mojom/file_system_access/file_system_access_data_transfer_token.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/clipboard/data_object_item.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_item.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_error.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_manager.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// static
ScriptPromise<IDLNullable<FileSystemHandle>>
DataTransferItemFileSystemAccess::getAsFileSystemHandle(
    ScriptState* script_state,
    DataTransferItem& data_transfer_item,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLNullable<FileSystemHandle>>>(
      script_state, exception_state.GetContext());
  auto result = resolver->Promise();

  if (!data_transfer_item.GetDataTransfer()->CanReadData()) {
    resolver->Resolve();
    return result;
  }

  // If the DataObjectItem doesn't have an associated FileSystemAccessEntry,
  // return nullptr.
  if (!data_transfer_item.GetDataObjectItem()->HasFileSystemAccessEntry()) {
    resolver->Resolve();
    return result;
  }

  const DataObjectItem& data_object_item =
      *data_transfer_item.GetDataObjectItem();

  // Since tokens are move-only, we need to create a clone in order
  // to preserve the state of `data_object_item` for future calls.
  mojo::PendingRemote<mojom::blink::FileSystemAccessDataTransferToken>
      token_remote = data_object_item.CloneFileSystemAccessEntryToken();

  auto* execution_context = ExecutionContext::From(script_state);
  FileSystemAccessManager::From(execution_context)
      ->GetEntryFromDataTransferToken(
          std::move(token_remote),
          WTF::BindOnce(
              [](ScriptPromiseResolver<IDLNullable<FileSystemHandle>>* resolver,
                 mojom::blink::FileSystemAccessErrorPtr result,
                 mojom::blink::FileSystemAccessEntryPtr entry) {
                ScriptState* script_state = resolver->GetScriptState();
                if (!script_state) {
                  return;
                }

                if (result->status !=
                    mojom::blink::FileSystemAccessStatus::kOk) {
                  DCHECK(entry.is_null());
                  file_system_access_error::Reject(resolver, *result);
                  return;
                }

                resolver->Resolve(FileSystemHandle::CreateFromMojoEntry(
                    std::move(entry), ExecutionContext::From(script_state)));
              },
              WrapPersistent(resolver)));

  return result;
}

}  // namespace blink
