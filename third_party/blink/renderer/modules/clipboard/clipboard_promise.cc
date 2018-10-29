// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_promise.h"

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
#include "third_party/blink/public/platform/modules/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_access_policy.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_item.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_item_list.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"

// And now, a brief note about clipboard permissions.
//
// There are 2 clipboard permissions defined in the spec:
// * clipboard-read
// * clipboard-write
// See https://w3c.github.io/clipboard-apis/#clipboard-permissions
//
// In Chromium we automatically grant clipboard-write access and clipboard-read
// access is gated behind a permission prompt. Both clipboard read and write
// require the tab to be focused (and Chromium must be the foreground app) for
// the operation to be allowed.

namespace blink {

using mojom::blink::PermissionStatus;
using mojom::blink::PermissionService;
using mojom::PageVisibilityState;

ScriptPromise ClipboardPromise::CreateForRead(ScriptState* script_state) {
  ClipboardPromise* clipboard_promise = new ClipboardPromise(script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&ClipboardPromise::HandleRead,
                           WrapPersistent(clipboard_promise)));
  return clipboard_promise->script_promise_resolver_->Promise();
}

ScriptPromise ClipboardPromise::CreateForReadText(ScriptState* script_state) {
  ClipboardPromise* clipboard_promise = new ClipboardPromise(script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&ClipboardPromise::HandleReadText,
                           WrapPersistent(clipboard_promise)));
  return clipboard_promise->script_promise_resolver_->Promise();
}

ScriptPromise ClipboardPromise::CreateForWrite(ScriptState* script_state,
                                               DataTransfer* data) {
  ClipboardPromise* clipboard_promise = new ClipboardPromise(script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE,
      WTF::Bind(&ClipboardPromise::HandleWrite,
                WrapPersistent(clipboard_promise), WrapPersistent(data)));
  return clipboard_promise->script_promise_resolver_->Promise();
}

ScriptPromise ClipboardPromise::CreateForWriteText(ScriptState* script_state,
                                                   const String& data) {
  ClipboardPromise* clipboard_promise = new ClipboardPromise(script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&ClipboardPromise::HandleWriteText,
                           WrapPersistent(clipboard_promise), data));
  return clipboard_promise->script_promise_resolver_->Promise();
}

ClipboardPromise::ClipboardPromise(ScriptState* script_state)
    : ContextLifecycleObserver(blink::ExecutionContext::From(script_state)),
      script_state_(script_state),
      script_promise_resolver_(ScriptPromiseResolver::Create(script_state)),
      buffer_(mojom::ClipboardBuffer::kStandard) {}

scoped_refptr<base::SingleThreadTaskRunner> ClipboardPromise::GetTaskRunner() {
  // TODO(garykac): Replace MiscPlatformAPI with TaskType specific to clipboard.
  return GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
}

PermissionService* ClipboardPromise::GetPermissionService() {
  if (!permission_service_) {
    ConnectToPermissionService(ExecutionContext::From(script_state_),
                               mojo::MakeRequest(&permission_service_));
  }
  return permission_service_.get();
}

bool ClipboardPromise::IsFocusedDocument(ExecutionContext* context) {
  Document* doc = To<Document>(context);
  return doc && doc->hasFocus();
}

void ClipboardPromise::RequestReadPermission(
    PermissionService::RequestPermissionCallback callback) {
  DCHECK(script_promise_resolver_);

  ExecutionContext* context = ExecutionContext::From(script_state_);
  DCHECK(context->IsSecureContext());  // [SecureContext] in IDL

  // Document must be focused.
  if (!IsFocusedDocument(context) || !GetPermissionService()) {
    script_promise_resolver_->Reject();
    return;
  }

  // Query for permission if necessary.
  // See crbug.com/795929 for moving this check into the Browser process.
  permission_service_->RequestPermission(
      CreateClipboardPermissionDescriptor(
          mojom::blink::PermissionName::CLIPBOARD_READ, false),
      false, std::move(callback));
}

void ClipboardPromise::CheckWritePermission(
    PermissionService::HasPermissionCallback callback) {
  DCHECK(script_promise_resolver_);

  ExecutionContext* context = ExecutionContext::From(script_state_);
  DCHECK(context->IsSecureContext());  // [SecureContext] in IDL

  // Document must be focused.
  if (!IsFocusedDocument(context) || !GetPermissionService()) {
    script_promise_resolver_->Reject();
    return;
  }

  // Check current permission (but do not query the user).
  // See crbug.com/795929 for moving this check into the Browser process.
  permission_service_->HasPermission(
      CreateClipboardPermissionDescriptor(
          mojom::blink::PermissionName::CLIPBOARD_WRITE, false),
      std::move(callback));
}

void ClipboardPromise::HandleRead() {
  RequestReadPermission(WTF::Bind(&ClipboardPromise::HandleReadWithPermission,
                                  WrapPersistent(this)));
}

// TODO(garykac): This currently only handles plain text.
void ClipboardPromise::HandleReadWithPermission(PermissionStatus status) {
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject();
    return;
  }

  String plain_text = SystemClipboard::GetInstance().ReadPlainText(buffer_);

  const DataTransfer::DataTransferType type =
      DataTransfer::DataTransferType::kCopyAndPaste;
  const DataTransferAccessPolicy access = DataTransferAccessPolicy::kReadable;
  DataObject* data = DataObject::CreateFromString(plain_text);
  DataTransfer* dt = DataTransfer::Create(type, access, data);
  script_promise_resolver_->Resolve(dt);
}

void ClipboardPromise::HandleReadText() {
  RequestReadPermission(WTF::Bind(
      &ClipboardPromise::HandleReadTextWithPermission, WrapPersistent(this)));
}

void ClipboardPromise::HandleReadTextWithPermission(PermissionStatus status) {
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject();
    return;
  }

  String text = SystemClipboard::GetInstance().ReadPlainText(buffer_);
  script_promise_resolver_->Resolve(text);
}

// TODO(garykac): This currently only handles plain text.
void ClipboardPromise::HandleWrite(DataTransfer* data) {
  // Scan DataTransfer and extract data types that we support.
  uint32_t num_items = data->items()->length();
  for (uint32_t i = 0; i < num_items; i++) {
    DataTransferItem* item = data->items()->item(i);
    DataObjectItem* objectItem = item->GetDataObjectItem();
    if (objectItem->Kind() == DataObjectItem::kStringKind &&
        objectItem->GetType() == kMimeTypeTextPlain) {
      write_data_ = objectItem->GetAsString();
      break;
    }
  }
  CheckWritePermission(WTF::Bind(&ClipboardPromise::HandleWriteWithPermission,
                                 WrapPersistent(this)));
}

void ClipboardPromise::HandleWriteWithPermission(PermissionStatus status) {
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject();
    return;
  }

  SystemClipboard::GetInstance().WritePlainText(write_data_);
  script_promise_resolver_->Resolve();
}

void ClipboardPromise::HandleWriteText(const String& data) {
  write_data_ = data;
  CheckWritePermission(WTF::Bind(
      &ClipboardPromise::HandleWriteTextWithPermission, WrapPersistent(this)));
}

void ClipboardPromise::HandleWriteTextWithPermission(PermissionStatus status) {
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject();
    return;
  }

  DCHECK(script_promise_resolver_);
  SystemClipboard::GetInstance().WritePlainText(write_data_);
  script_promise_resolver_->Resolve();
}

void ClipboardPromise::Trace(blink::Visitor* visitor) {
  visitor->Trace(script_state_);
  visitor->Trace(script_promise_resolver_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
