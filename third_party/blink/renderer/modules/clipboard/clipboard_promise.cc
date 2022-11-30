// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_promise.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_reader.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_writer.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/base/clipboard/clipboard_constants.h"

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

// This class deals with all the Blob promises and executes the write
// operation after all the promises have been resolved.
class ClipboardPromise::BlobPromiseResolverFunction final
    : public ScriptFunction::Callable {
 public:
  enum class ResolveType { kFulfill, kReject };

  static void Create(ScriptState* script_state,
                     ScriptPromise promise,
                     ClipboardPromise* clipboard_promise) {
    promise.Then(
        MakeGarbageCollected<ScriptFunction>(
            script_state, MakeGarbageCollected<BlobPromiseResolverFunction>(
                              clipboard_promise, ResolveType::kFulfill)),
        MakeGarbageCollected<ScriptFunction>(
            script_state, MakeGarbageCollected<BlobPromiseResolverFunction>(
                              clipboard_promise, ResolveType::kReject)));
  }

  BlobPromiseResolverFunction(ClipboardPromise* clipboard_promise,
                              ResolveType type)
      : clipboard_promise_(clipboard_promise), type_(type) {}

  void Trace(Visitor* visitor) const final {
    ScriptFunction::Callable::Trace(visitor);
    visitor->Trace(clipboard_promise_);
  }

  ScriptValue Call(ScriptState* script_state, ScriptValue value) final {
    ExceptionState exception_state(script_state->GetIsolate(),
                                   ExceptionState::kExecutionContext,
                                   "Clipboard", "write");
    if (type_ == ResolveType::kFulfill) {
      HeapVector<Member<Blob>>* blob_list =
          MakeGarbageCollected<HeapVector<Member<Blob>>>(
              NativeValueTraits<IDLSequence<Blob>>::NativeValue(
                  script_state->GetIsolate(), value.V8Value(),
                  exception_state));
      if (exception_state.HadException()) {
        // Clear the exception here as it'll be fired in `RejectBlobPromise`.
        exception_state.ClearException();
        const String exception_text = "Invalid Blob types.";
        clipboard_promise_->GetTaskRunner()->PostTask(
            FROM_HERE, WTF::BindOnce(&ClipboardPromise::RejectBlobPromise,
                                     WrapPersistent(clipboard_promise_.Get()),
                                     std::move(exception_text)));
        return ScriptValue();
      }
      clipboard_promise_->GetTaskRunner()->PostTask(
          FROM_HERE, WTF::BindOnce(&ClipboardPromise::HandlePromiseBlobsWrite,
                                   WrapPersistent(clipboard_promise_.Get()),
                                   WrapPersistent(blob_list)));
      return ScriptValue();
    }
    const String exception_text = "Promises to Blobs were rejected.";
    clipboard_promise_->GetTaskRunner()->PostTask(
        FROM_HERE, WTF::BindOnce(&ClipboardPromise::RejectBlobPromise,
                                 WrapPersistent(clipboard_promise_.Get()),
                                 std::move(exception_text)));
    return ScriptValue();
  }

 private:
  Member<ClipboardPromise> clipboard_promise_;
  ResolveType type_;
};

// static
ScriptPromise ClipboardPromise::CreateForRead(ExecutionContext* context,
                                              ScriptState* script_state) {
  if (!script_state->ContextIsValid())
    return ScriptPromise();
  ClipboardPromise* clipboard_promise =
      MakeGarbageCollected<ClipboardPromise>(context, script_state);
  clipboard_promise->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::BindOnce(&ClipboardPromise::HandleRead,
                               WrapPersistent(clipboard_promise)));
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
      FROM_HERE, WTF::BindOnce(&ClipboardPromise::HandleReadText,
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
      FROM_HERE, WTF::BindOnce(&ClipboardPromise::HandleWrite,
                               WrapPersistent(clipboard_promise),
                               WrapPersistent(items_copy)));
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
      FROM_HERE, WTF::BindOnce(&ClipboardPromise::HandleWriteText,
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
  clipboard_writer_ =
      ClipboardWriter::Create(local_frame->GetSystemClipboard(), type, this);
  if (!clipboard_writer_) {
    script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "Type " + type + " is not supported"));
    return;
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

void ClipboardPromise::HandleRead() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RequestPermission(mojom::blink::PermissionName::CLIPBOARD_READ,
                    /*will_be_sanitized=*/
                    !RuntimeEnabledFeatures::ClipboardCustomFormatsEnabled(),
                    WTF::BindOnce(&ClipboardPromise::HandleReadWithPermission,
                                  WrapPersistent(this)));
}

void ClipboardPromise::HandleReadText() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RequestPermission(
      mojom::blink::PermissionName::CLIPBOARD_READ,
      /*will_be_sanitized=*/true,
      WTF::BindOnce(&ClipboardPromise::HandleReadTextWithPermission,
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
  clipboard_item_data_with_promises_ = clipboard_item->GetItems();
  custom_format_items_ = clipboard_item->CustomFormats();

  if (static_cast<int>(custom_format_items_.size()) >
      ui::kMaxRegisteredClipboardFormats) {
    script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "Number of custom formats exceeds the max limit which is set to 100."));
    return;
  }
  DCHECK(RuntimeEnabledFeatures::ClipboardCustomFormatsEnabled() ||
         custom_format_items_.empty());

  // Input in standard formats is sanitized, so the write will be sanitized
  // unless there are custom formats.
  RequestPermission(mojom::blink::PermissionName::CLIPBOARD_WRITE,
                    /*will_be_sanitized=*/custom_format_items_.empty(),
                    WTF::BindOnce(&ClipboardPromise::HandleWriteWithPermission,
                                  WrapPersistent(this)));
}

void ClipboardPromise::HandleWriteText(const String& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  plain_text_ = data;
  RequestPermission(
      mojom::blink::PermissionName::CLIPBOARD_WRITE,
      /*will_be_sanitized=*/true,
      WTF::BindOnce(&ClipboardPromise::HandleWriteTextWithPermission,
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
  if (RuntimeEnabledFeatures::ClipboardCustomFormatsEnabled()) {
    system_clipboard->ReadAvailableCustomAndStandardFormats(WTF::BindOnce(
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

  ScriptState::Scope scope(script_state_);
  HeapVector<std::pair<String, ScriptPromise>> items;
  items.ReserveInitialCapacity(clipboard_item_data_.size());

  for (const auto& item : clipboard_item_data_) {
    ScriptPromise promise =
        ScriptPromise::Cast(script_state_, ToV8(item.second, script_state_));
    items.emplace_back(item.first, promise);
  }
  HeapVector<Member<ClipboardItem>> clipboard_items = {
      MakeGarbageCollected<ClipboardItem>(items)};
  script_promise_resolver_->Resolve(clipboard_items);
}

void ClipboardPromise::OnReadAvailableFormatNames(
    const Vector<String>& format_names) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetExecutionContext())
    return;

  clipboard_item_data_.ReserveInitialCapacity(format_names.size());
  for (const String& format_name : format_names) {
    if (ClipboardWriter::IsValidType(format_name)) {
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

  ClipboardReader* clipboard_reader = ClipboardReader::Create(
      GetLocalFrame()->GetSystemClipboard(),
      clipboard_item_data_[clipboard_representation_index_].first, this);
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

void ClipboardPromise::HandlePromiseBlobsWrite(
    HeapVector<Member<Blob>>* blob_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  wtf_size_t clipboard_item_index = 0;
  CHECK_EQ(clipboard_item_types_.size(), blob_list->size());
  for (const auto& blob_item : *blob_list) {
    const String& type = clipboard_item_types_[clipboard_item_index];
    const String& type_with_args = blob_item->type();
    // For web custom types, extract the MIME type after removing the "web "
    // prefix. For normal (not-custom) write, blobs may have a full MIME type
    // with args (ex. 'text/plain;charset=utf-8'), whereas the type must not
    // have args (ex. 'text/plain' only), so ensure that Blob->type is contained
    // in type.
    String web_custom_format = Clipboard::ParseWebCustomFormat(type);
    if ((!type_with_args.Contains(type.LowerASCII()) &&
         web_custom_format.empty()) ||
        (!web_custom_format.empty() &&
         !type_with_args.Contains(web_custom_format))) {
      script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "Type " + type + " does not match the blob's type " +
              type_with_args));
      return;
    }
    clipboard_item_data_.emplace_back(type, blob_item);
    clipboard_item_index++;
  }
  clipboard_item_types_.clear();

  DCHECK(!clipboard_representation_index_);
  WriteNextRepresentation();
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

  HeapVector<ScriptPromise> promise_list;
  promise_list.ReserveInitialCapacity(
      clipboard_item_data_with_promises_.size());
  clipboard_item_types_.ReserveInitialCapacity(
      clipboard_item_data_with_promises_.size());
  // Check that all types are valid.
  for (const auto& type_and_promise_to_blob :
       clipboard_item_data_with_promises_) {
    const String& type = type_and_promise_to_blob.first;
    clipboard_item_types_.emplace_back(type);
    promise_list.emplace_back(type_and_promise_to_blob.second);
    if (!ClipboardWriter::IsValidType(type)) {
      script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "Type " + type + " not supported on write."));
      return;
    }
  }
  ScriptState::Scope scope(script_state_);
  BlobPromiseResolverFunction::Create(
      script_state_, ScriptPromise::All(script_state_, promise_list), this);
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

void ClipboardPromise::RejectBlobPromise(const String& exception_text) {
  script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotAllowedError, exception_text));
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
    bool will_be_sanitized,
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

  bool has_transient_user_activation =
      LocalFrame::HasTransientUserActivation(GetLocalFrame());
  base::UmaHistogramBoolean("Blink.Clipboard.HasTransientUserActivation",
                            has_transient_user_activation);
  // `will_be_sanitized` is false only when we are trying to read/write
  // web custom formats.
  if (!will_be_sanitized &&
      RuntimeEnabledFeatures::ClipboardCustomFormatsEnabled() &&
      !has_transient_user_activation) {
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
      permission, /*has_user_gesture=*/has_transient_user_activation,
      /*will_be_sanitized=*/will_be_sanitized);

  // Note that extra checks are performed browser-side in
  // `ContentBrowserClient::IsClipboardPasteAllowed()`.
  permission_service_->RequestPermission(
      std::move(permission_descriptor),
      /*user_gesture=*/has_transient_user_activation, std::move(callback));
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
  visitor->Trace(clipboard_item_data_with_promises_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
