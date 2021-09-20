// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_promise.h"

#include <memory>
#include <utility>

#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_clipboard_item_options.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_reader.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_writer.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

// There are 2 clipboard permissions defined in the spec:
// * clipboard-read
// * clipboard-write
// See https://w3c.github.io/clipboard-apis/#clipboard-permissions
//
// These permissions map to these ContentSettings:
// * CLIPBOARD_READ_WRITE, for sanitized read, and unsanitized read/write.
// * CLIPBOARD_SANITIZED_WRITE, for sanitized write only.

namespace blink {

using mojom::blink::PermissionStatus;
using mojom::blink::PermissionService;

// static
ScriptPromise ClipboardPromise::CreateForRead(ExecutionContext* context,
                                              ScriptState* script_state,
                                              ClipboardItemOptions* options) {
  if (!script_state->ContextIsValid())
    return ScriptPromise();
  ClipboardPromise* clipboard_promise =
      MakeGarbageCollected<ClipboardPromise>(context, script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE,
      WTF::Bind(&ClipboardPromise::HandleRead,
                WrapPersistent(clipboard_promise), WrapPersistent(options)));
  return clipboard_promise->script_promise_resolver_->Promise();
}

// static
ScriptPromise ClipboardPromise::CreateForReadText(ExecutionContext* context,
                                                  ScriptState* script_state) {
  if (!script_state->ContextIsValid())
    return ScriptPromise();
  ClipboardPromise* clipboard_promise =
      MakeGarbageCollected<ClipboardPromise>(context, script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&ClipboardPromise::HandleReadText,
                           WrapPersistent(clipboard_promise)));
  return clipboard_promise->script_promise_resolver_->Promise();
}

// static
ScriptPromise ClipboardPromise::CreateForWrite(
    ExecutionContext* context,
    ScriptState* script_state,
    const HeapVector<Member<ClipboardItem>>& items) {
  if (!script_state->ContextIsValid())
    return ScriptPromise();
  ClipboardPromise* clipboard_promise =
      MakeGarbageCollected<ClipboardPromise>(context, script_state);
  HeapVector<Member<ClipboardItem>>* items_copy =
      MakeGarbageCollected<HeapVector<Member<ClipboardItem>>>(items);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE,
      WTF::Bind(&ClipboardPromise::HandleWrite,
                WrapPersistent(clipboard_promise), WrapPersistent(items_copy)));
  return clipboard_promise->script_promise_resolver_->Promise();
}

// static
ScriptPromise ClipboardPromise::CreateForWriteText(ExecutionContext* context,
                                                   ScriptState* script_state,
                                                   const String& data) {
  if (!script_state->ContextIsValid())
    return ScriptPromise();
  ClipboardPromise* clipboard_promise =
      MakeGarbageCollected<ClipboardPromise>(context, script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&ClipboardPromise::HandleWriteText,
                           WrapPersistent(clipboard_promise), data));
  return clipboard_promise->script_promise_resolver_->Promise();
}

ClipboardPromise::ClipboardPromise(ExecutionContext* context,
                                   ScriptState* script_state)
    : ExecutionContextLifecycleObserver(context),
      script_state_(script_state),
      script_promise_resolver_(
          MakeGarbageCollected<ScriptPromiseResolver>(script_state)),
      permission_service_(context),
      clipboard_representation_index_(0) {}

ClipboardPromise::~ClipboardPromise() = default;

void ClipboardPromise::CompleteWriteRepresentation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  clipboard_writer_.Clear();  // The previous write is done.
  ++clipboard_representation_index_;
  WriteNextRepresentation();
}

void ClipboardPromise::WriteNextRepresentation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetExecutionContext())
    return;
  LocalFrame* local_frame = GetLocalFrame();
  // Commit to system clipboard when all representations are written.
  // This is in the start flow so that a |clipboard_item_data_| with 0 items
  // will still commit gracefully.
  if (clipboard_representation_index_ == clipboard_item_data_.size()) {
    local_frame->GetSystemClipboard()->CommitWrite();
    script_promise_resolver_->Resolve();
    return;
  }

  // We currently write the ClipboardItem type, but don't use the blob.type.
  const String& type =
      clipboard_item_data_[clipboard_representation_index_].first;
  const Member<Blob>& blob =
      clipboard_item_data_[clipboard_representation_index_].second;

  DCHECK(!clipboard_writer_);
  wtf_size_t item_index = custom_format_items_.Find(type);
  if (item_index != kNotFound) {
    clipboard_writer_ =
        ClipboardWriter::Create(local_frame->GetSystemClipboard(), type, this,
                                /*is_custom_format_type*/ true);
    if (ClipboardWriter::IsValidType(type, /*is_custom_format_type*/ false)) {
      // Decrement `clipboard_representation_index_` & remove the format from
      // the `custom_format_items_` so we can redo the write, but this time, it
      // will write a sanitized version of the format using the "standard"
      // format writer. Standard formats include text/html, text/plain,
      // text/rtf, image/png, text/uri-list & image/svg+xml.
      // https://github.com/w3c/editing/blob/gh-pages/docs/clipboard-pickling/explainer.md#pickled-version-for-sanitized-formats
      custom_format_items_.EraseAt(item_index);
      clipboard_representation_index_--;
    }
  } else {
    clipboard_writer_ =
        ClipboardWriter::Create(local_frame->GetSystemClipboard(), type, this,
                                /*is_custom_format_type*/ false);
  }
  clipboard_writer_->WriteToSystem(blob);
}

void ClipboardPromise::RejectFromReadOrDecodeFailure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetExecutionContext())
    return;
  script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kDataError,
      "Failed to read or decode Blob for clipboard item type " +
          clipboard_item_data_[clipboard_representation_index_].first + "."));
}

void ClipboardPromise::HandleRead(ClipboardItemOptions* options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (options->hasUnsanitized()) {
    for (const auto& unsanitized_item : options->unsanitized()) {
      custom_format_items_.push_back(unsanitized_item);
    }
  }
  RequestPermission(mojom::blink::PermissionName::CLIPBOARD_READ,
                    !custom_format_items_.IsEmpty(),
                    WTF::Bind(&ClipboardPromise::HandleReadWithPermission,
                              WrapPersistent(this)));
}

void ClipboardPromise::HandleReadText() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RequestPermission(mojom::blink::PermissionName::CLIPBOARD_READ, false,
                    WTF::Bind(&ClipboardPromise::HandleReadTextWithPermission,
                              WrapPersistent(this)));
}

void ClipboardPromise::HandleWrite(
    HeapVector<Member<ClipboardItem>>* clipboard_items) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(clipboard_items);
  if (!GetExecutionContext())
    return;

  if (clipboard_items->size() > 1) {
    script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "Support for multiple ClipboardItems is not implemented."));
    return;
  }
  if (!clipboard_items->size()) {
    // Do nothing if there are no ClipboardItems.
    script_promise_resolver_->Resolve();
    return;
  }

  // For now, we only process the first ClipboardItem.
  ClipboardItem* clipboard_item = (*clipboard_items)[0];
  clipboard_item_data_ = clipboard_item->GetItems();
  custom_format_items_ = clipboard_item->CustomFormats();
  DCHECK(RuntimeEnabledFeatures::ClipboardCustomFormatsEnabled() ||
         custom_format_items_.IsEmpty());

  RequestPermission(mojom::blink::PermissionName::CLIPBOARD_WRITE,
                    !custom_format_items_.IsEmpty(),
                    WTF::Bind(&ClipboardPromise::HandleWriteWithPermission,
                              WrapPersistent(this)));
}

void ClipboardPromise::HandleWriteText(const String& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  plain_text_ = data;
  RequestPermission(mojom::blink::PermissionName::CLIPBOARD_WRITE, false,
                    WTF::Bind(&ClipboardPromise::HandleWriteTextWithPermission,
                              WrapPersistent(this)));
}

void ClipboardPromise::HandleReadWithPermission(PermissionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetExecutionContext())
    return;
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, "Read permission denied."));
    return;
  }

  SystemClipboard* system_clipboard = GetLocalFrame()->GetSystemClipboard();
  if (!custom_format_items_.IsEmpty()) {
    system_clipboard->ReadAvailableCustomAndStandardFormats(WTF::Bind(
        &ClipboardPromise::OnReadAvailableFormatNames, WrapPersistent(this)));
    return;
  }
  Vector<String> available_types = system_clipboard->ReadAvailableTypes();
  OnReadAvailableFormatNames(available_types);
}

void ClipboardPromise::ResolveRead() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetExecutionContext());

  if (!clipboard_item_data_.size()) {
    script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataError, "No valid data on clipboard."));
    return;
  }

  ClipboardItemOptions* options = ClipboardItemOptions::Create();
  options->setUnsanitized(custom_format_items_);

  HeapVector<Member<ClipboardItem>> clipboard_items = {
      MakeGarbageCollected<ClipboardItem>(clipboard_item_data_, options)};
  script_promise_resolver_->Resolve(clipboard_items);
}

void ClipboardPromise::OnReadAvailableFormatNames(
    const Vector<String>& format_names) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetExecutionContext())
    return;

  clipboard_item_data_.ReserveInitialCapacity(format_names.size());
  for (const String& format_name : format_names) {
    if (ClipboardWriter::IsValidType(
            format_name, base::Contains(custom_format_items_, format_name))) {
      clipboard_item_data_.emplace_back(format_name,
                                        /* Placeholder value. */ nullptr);
    }
  }
  ReadNextRepresentation();
}

void ClipboardPromise::ReadNextRepresentation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetExecutionContext())
    return;
  if (clipboard_representation_index_ == clipboard_item_data_.size()) {
    ResolveRead();
    return;
  }

  String format_name =
      clipboard_item_data_[clipboard_representation_index_].first;

  ClipboardReader* clipboard_reader = ClipboardReader::Create(
      GetLocalFrame()->GetSystemClipboard(), format_name, this,
      base::Contains(custom_format_items_, format_name));
  if (!clipboard_reader) {
    OnRead(nullptr);
    return;
  }
  clipboard_reader->Read();
}

void ClipboardPromise::OnRead(Blob* blob) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  clipboard_item_data_[clipboard_representation_index_].second = blob;
  ++clipboard_representation_index_;
  ReadNextRepresentation();
}

void ClipboardPromise::HandleReadTextWithPermission(PermissionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetExecutionContext())
    return;
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, "Read permission denied."));
    return;
  }

  String text = GetLocalFrame()->GetSystemClipboard()->ReadPlainText(
      mojom::blink::ClipboardBuffer::kStandard);
  script_promise_resolver_->Resolve(text);
}

void ClipboardPromise::HandleWriteWithPermission(PermissionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetExecutionContext())
    return;
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, "Write permission denied."));
    return;
  }

  // Check that all blobs have valid types.
  for (const auto& type_and_blob : clipboard_item_data_) {
    String type = type_and_blob.first;
    String type_with_args = type_and_blob.second->type();
    bool is_valid_custom_format = base::Contains(custom_format_items_, type);
    if (!ClipboardWriter::IsValidType(type, is_valid_custom_format)) {
      script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "Type " + type + " not supported on write."));
      return;
    }
    // For normal (not-custom) write, blobs may have a full MIME type with args
    // (ex. 'text/plain;charset=utf-8'), whereas the type must not have args
    // (ex. 'text/plain' only), so ensure that Blob->type is contained in type.
    if (!type_with_args.Contains(type.LowerASCII())) {
      script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "Type " + type + " does not match the blob's type " +
              type_with_args));
      return;
    }
  }

  DCHECK(!clipboard_representation_index_);
  WriteNextRepresentation();
}

void ClipboardPromise::HandleWriteTextWithPermission(PermissionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetExecutionContext())
    return;
  if (status != PermissionStatus::GRANTED) {
    script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, "Write permission denied."));
    return;
  }

  SystemClipboard* system_clipboard = GetLocalFrame()->GetSystemClipboard();
  system_clipboard->WritePlainText(plain_text_);
  system_clipboard->CommitWrite();
  script_promise_resolver_->Resolve();
}

PermissionService* ClipboardPromise::GetPermissionService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ExecutionContext* context = GetExecutionContext();
  DCHECK(context);
  if (!permission_service_.is_bound()) {
    ConnectToPermissionService(
        context,
        permission_service_.BindNewPipeAndPassReceiver(GetTaskRunner()));
  }
  return permission_service_.get();
}

void ClipboardPromise::RequestPermission(
    mojom::blink::PermissionName permission,
    bool allow_without_sanitization,
    base::OnceCallback<void(::blink::mojom::PermissionStatus)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(script_promise_resolver_);
  DCHECK(permission == mojom::blink::PermissionName::CLIPBOARD_READ ||
         permission == mojom::blink::PermissionName::CLIPBOARD_WRITE);

  ExecutionContext* context = GetExecutionContext();
  if (!context)
    return;
  LocalDOMWindow& window = *To<LocalDOMWindow>(context);
  DCHECK(window.IsSecureContext());  // [SecureContext] in IDL

  if (!window.document()->hasFocus()) {
    script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, "Document is not focused."));
    return;
  }

  constexpr char kFeaturePolicyMessage[] =
      "The Clipboard API has been blocked because of a permissions policy "
      "applied to the current document. See https://goo.gl/EuHzyv for more "
      "details.";

  if ((permission == mojom::blink::PermissionName::CLIPBOARD_READ &&
       !window.IsFeatureEnabled(
           mojom::blink::PermissionsPolicyFeature::kClipboardRead,
           ReportOptions::kReportOnFailure, kFeaturePolicyMessage)) ||
      (permission == mojom::blink::PermissionName::CLIPBOARD_WRITE &&
       !window.IsFeatureEnabled(
           mojom::blink::PermissionsPolicyFeature::kClipboardWrite,
           ReportOptions::kReportOnFailure, kFeaturePolicyMessage))) {
    script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        kFeaturePolicyMessage));
    return;
  }

  if (!custom_format_items_.IsEmpty() &&
      !LocalFrame::HasTransientUserActivation(GetLocalFrame())) {
    script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError,
        "Must be handling a user gesture to use custom clipboard"));
    return;
  }

  if (!GetPermissionService()) {
    script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "Permission Service could not connect."));
    return;
  }

  auto permission_descriptor = CreateClipboardPermissionDescriptor(
      permission, false, allow_without_sanitization);
  if (permission == mojom::blink::PermissionName::CLIPBOARD_WRITE &&
      !allow_without_sanitization) {
    // Check permission (but do not query the user).
    // See crbug.com/795929 for moving this check into the Browser process.
    permission_service_->HasPermission(std::move(permission_descriptor),
                                       std::move(callback));
    return;
  }
  // Check permission, and query if necessary.
  // See crbug.com/795929 for moving this check into the Browser process.
  permission_service_->RequestPermission(std::move(permission_descriptor),
                                         false, std::move(callback));
}

LocalFrame* ClipboardPromise::GetLocalFrame() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ExecutionContext* context = GetExecutionContext();
  DCHECK(context);
  LocalFrame* local_frame = To<LocalDOMWindow>(context)->GetFrame();
  return local_frame;
}

scoped_refptr<base::SingleThreadTaskRunner> ClipboardPromise::GetTaskRunner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Get the User Interaction task runner, as Async Clipboard API calls require
  // user interaction, as specified in https://w3c.github.io/clipboard-apis/
  return GetExecutionContext()->GetTaskRunner(TaskType::kUserInteraction);
}

// ExecutionContextLifecycleObserver implementation.
void ClipboardPromise::ContextDestroyed() {
  script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotAllowedError, "Document detached."));
  clipboard_writer_.Clear();
}

void ClipboardPromise::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(script_promise_resolver_);
  visitor->Trace(clipboard_writer_);
  visitor->Trace(permission_service_);
  visitor->Trace(clipboard_item_data_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
