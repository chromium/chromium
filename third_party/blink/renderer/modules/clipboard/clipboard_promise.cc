// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_promise.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_clipboard_unsanitized_formats.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/commands/clipboard_commands.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_item.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_reader.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_writer.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
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

using mojom::blink::PermissionService;

// This class deals with all the Blob promises and executes the write
// operation after all the promises have been resolved.
class ClipboardPromise::BlobPromiseResolverFunction final
    : public ScriptFunction::Callable {
 public:
  enum class ResolveType { kFulfill, kReject };

  static void Create(ScriptState* script_state,
                     ScriptPromiseUntyped promise,
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
    if (type_ == ResolveType::kReject) {
      clipboard_promise_->RejectBlobPromise("Promises to Blobs were rejected.");
    } else {
      v8::TryCatch try_catch(script_state->GetIsolate());
      HeapVector<Member<Blob>>* blob_list =
          MakeGarbageCollected<HeapVector<Member<Blob>>>(
              NativeValueTraits<IDLSequence<Blob>>::NativeValue(
                  script_state->GetIsolate(), value.V8Value(),
                  PassThroughException(script_state->GetIsolate())));
      if (try_catch.HasCaught()) {
        // Swallow the exception here as it'll be fired in `RejectBlobPromise`.
        clipboard_promise_->RejectBlobPromise("Invalid Blob types.");
      } else {
        clipboard_promise_->HandlePromiseBlobsWrite(blob_list);
      }
    }
    return ScriptValue();
  }

 private:
  Member<ClipboardPromise> clipboard_promise_;
  ResolveType type_;
};

// static
ScriptPromise<IDLSequence<ClipboardItem>> ClipboardPromise::CreateForRead(
    ExecutionContext* context,
    ScriptState* script_state,
    ClipboardUnsanitizedFormats* formats,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    return ScriptPromise<IDLSequence<ClipboardItem>>();
  }
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<ClipboardItem>>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  ClipboardPromise* clipboard_promise = MakeGarbageCollected<ClipboardPromise>(
      context, resolver, exception_state);
  clipboard_promise->HandleRead(formats);
  return promise;
}

// static
ScriptPromise<IDLString> ClipboardPromise::CreateForReadText(
    ExecutionContext* context,
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    return EmptyPromise();
  }
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(
      script_state, exception_state.GetContext());
  ClipboardPromise* clipboard_promise = MakeGarbageCollected<ClipboardPromise>(
      context, resolver, exception_state);
  auto promise = resolver->Promise();
  clipboard_promise->HandleReadText();
  return promise;
}

// static
ScriptPromise<IDLUndefined> ClipboardPromise::CreateForWrite(
    ExecutionContext* context,
    ScriptState* script_state,
    const HeapVector<Member<ClipboardItem>>& items,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    return EmptyPromise();
  }
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  ClipboardPromise* clipboard_promise = MakeGarbageCollected<ClipboardPromise>(
      context, resolver, exception_state);
  auto promise = resolver->Promise();
  clipboard_promise->HandleWrite(items);
  return promise;
}

// static
ScriptPromise<IDLUndefined> ClipboardPromise::CreateForWriteText(
    ExecutionContext* context,
    ScriptState* script_state,
    const String& data,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    return EmptyPromise();
  }
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  ClipboardPromise* clipboard_promise = MakeGarbageCollected<ClipboardPromise>(
      context, resolver, exception_state);
  auto promise = resolver->Promise();
  clipboard_promise->HandleWriteText(data);
  return promise;
}

ClipboardPromise::ClipboardPromise(ExecutionContext* context,
                                   ScriptPromiseResolverBase* resolver,
                                   ExceptionState& exception_state)
    : ExecutionContextLifecycleObserver(context),
      script_promise_resolver_(resolver),
      permission_service_(context) {}

ClipboardPromise::~ClipboardPromise() = default;

void ClipboardPromise::CompleteWriteRepresentation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  clipboard_writer_.Clear();  // The previous write is done.
  ++clipboard_representation_index_;
  WriteNextRepresentation();
}

void ClipboardPromise::WriteNextRepresentation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetExecutionContext() || !GetScriptState()->ContextIsValid()) {
    return;
  }
  ScriptState::Scope scope(GetScriptState());
  LocalFrame* local_frame = GetLocalFrame();
  // Commit to system clipboard when all representations are written.
  // This is in the start flow so that a |clipboard_item_data_| with 0 items
  // will still commit gracefully.
  if (clipboard_representation_index_ == clipboard_item_data_.size()) {
    local_frame->GetSystemClipboard()->CommitWrite();
    script_promise_resolver_->DowncastTo<IDLUndefined>()->Resolve();
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
    script_promise_resolver_->RejectWithDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Type " + type + " is not supported");
    return;
  }
  clipboard_writer_->WriteToSystem(blob);
}

void ClipboardPromise::RejectFromReadOrDecodeFailure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetExecutionContext() || !GetScriptState()->ContextIsValid()) {
    return;
  }
  ScriptState::Scope scope(GetScriptState());
  script_promise_resolver_->RejectWithDOMException(
      DOMExceptionCode::kDataError,
      "Failed to read or decode Blob for clipboard item type " +
          clipboard_item_data_[clipboard_representation_index_].first + ".");
}

void ClipboardPromise::HandleRead(ClipboardUnsanitizedFormats* formats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (formats && formats->hasUnsanitized() && !formats->unsanitized().empty()) {
    Vector<String> unsanitized_formats = formats->unsanitized();
    if (unsanitized_formats.size() > 1) {
      script_promise_resolver_->RejectWithDOMException(
          DOMExceptionCode::kNotAllowedError,
          "Reading multiple unsanitized formats is not supported.");
      return;
    }
    if (unsanitized_formats[0] != kMimeTypeTextHTML) {
      script_promise_resolver_->RejectWithDOMException(
          DOMExceptionCode::kNotAllowedError, "The unsanitized type " +
                                                  unsanitized_formats[0] +
                                                  " is not supported.");
      return;
    }
    // HTML is the only standard format that can be read without any processing
    // for now.
    will_read_unprocessed_html_ = true;
  }

  ValidatePreconditions(
      mojom::blink::PermissionName::CLIPBOARD_READ,
      /*will_be_sanitized=*/false,
      WTF::BindOnce(&ClipboardPromise::HandleReadWithPermission,
                    WrapPersistent(this)));
}

void ClipboardPromise::HandleReadText() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ValidatePreconditions(
      mojom::blink::PermissionName::CLIPBOARD_READ,
      /*will_be_sanitized=*/true,
      WTF::BindOnce(&ClipboardPromise::HandleReadTextWithPermission,
                    WrapPersistent(this)));
}

void ClipboardPromise::HandleWrite(
    const HeapVector<Member<ClipboardItem>>& clipboard_items) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetExecutionContext());

  if (clipboard_items.size() > 1) {
    script_promise_resolver_->RejectWithDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Support for multiple ClipboardItems is not implemented.");
    return;
  }
  if (!clipboard_items.size()) {
    // Do nothing if there are no ClipboardItems.
    script_promise_resolver_->DowncastTo<IDLUndefined>()->Resolve();
    return;
  }

  // For now, we only process the first ClipboardItem.
  ClipboardItem* clipboard_item = clipboard_items[0];
  clipboard_item_data_with_promises_ = clipboard_item->GetRepresentations();
  write_custom_format_types_ = clipboard_item->CustomFormats();

  if (static_cast<int>(write_custom_format_types_.size()) >
      ui::kMaxRegisteredClipboardFormats) {
    script_promise_resolver_->RejectWithDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Number of custom formats exceeds the max limit which is set to 100.");
    return;
  }

  // Input in standard formats is sanitized, so the write will be sanitized
  // unless there are custom formats.
  ValidatePreconditions(
      mojom::blink::PermissionName::CLIPBOARD_WRITE,
      /*will_be_sanitized=*/write_custom_format_types_.empty(),
      WTF::BindOnce(&ClipboardPromise::HandleWriteWithPermission,
                    WrapPersistent(this)));
}

void ClipboardPromise::HandleWriteText(const String& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  plain_text_ = data;
  ValidatePreconditions(
      mojom::blink::PermissionName::CLIPBOARD_WRITE,
      /*will_be_sanitized=*/true,
      WTF::BindOnce(&ClipboardPromise::HandleWriteTextWithPermission,
                    WrapPersistent(this)));
}

void ClipboardPromise::HandleReadWithPermission(
    mojom::blink::PermissionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetExecutionContext()) {
    return;
  }
  if (status != mojom::blink::PermissionStatus::GRANTED) {
    script_promise_resolver_->RejectWithDOMException(
        DOMExceptionCode::kNotAllowedError, "Read permission denied.");
    return;
  }

  SystemClipboard* system_clipboard = GetLocalFrame()->GetSystemClipboard();
  system_clipboard->ReadAvailableCustomAndStandardFormats(WTF::BindOnce(
      &ClipboardPromise::OnReadAvailableFormatNames, WrapPersistent(this)));
}

void ClipboardPromise::ResolveRead() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetExecutionContext());

  base::UmaHistogramCounts100("Blink.Clipboard.Read.NumberOfFormats",
                              clipboard_item_data_.size());
  ScriptState* script_state = GetScriptState();
  if (!script_state->ContextIsValid()) {
    return;
  }
  ScriptState::Scope scope(script_state);
  HeapVector<std::pair<String, ScriptPromise<Blob>>> items;
  items.ReserveInitialCapacity(clipboard_item_data_.size());

  for (const auto& item : clipboard_item_data_) {
    if (!item.second) {
      continue;
    }
    auto promise = ToResolvedPromise<Blob>(script_state, item.second);
    items.emplace_back(item.first, promise);
  }
  HeapVector<Member<ClipboardItem>> clipboard_items = {
      MakeGarbageCollected<ClipboardItem>(items)};
  script_promise_resolver_->DowncastTo<IDLSequence<ClipboardItem>>()->Resolve(
      clipboard_items);
}

void ClipboardPromise::OnReadAvailableFormatNames(
    const Vector<String>& format_names) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetExecutionContext()) {
    return;
  }

  clipboard_item_data_.ReserveInitialCapacity(format_names.size());
  for (const String& format_name : format_names) {
    if (ClipboardItem::supports(format_name)) {
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
      clipboard_item_data_[clipboard_representation_index_].first, this,
      /*sanitize_html=*/!will_read_unprocessed_html_);
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

void ClipboardPromise::HandleReadTextWithPermission(
    mojom::blink::PermissionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetExecutionContext()) {
    return;
  }
  if (status != mojom::blink::PermissionStatus::GRANTED) {
    script_promise_resolver_->RejectWithDOMException(
        DOMExceptionCode::kNotAllowedError, "Read permission denied.");
    return;
  }

  String text = GetLocalFrame()->GetSystemClipboard()->ReadPlainText(
      mojom::blink::ClipboardBuffer::kStandard);
  script_promise_resolver_->DowncastTo<IDLString>()->Resolve(text);
}

void ClipboardPromise::HandlePromiseBlobsWrite(
    HeapVector<Member<Blob>>* blob_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetClipboardTaskRunner()->PostTask(
      FROM_HERE,
      WTF::BindOnce(&ClipboardPromise::WriteBlobs, WrapPersistent(this),
                    WrapPersistent(blob_list)));
}

void ClipboardPromise::WriteBlobs(HeapVector<Member<Blob>>* blob_list) {
  wtf_size_t clipboard_item_index = 0;
  CHECK_EQ(write_clipboard_item_types_.size(), blob_list->size());
  for (const auto& blob_item : *blob_list) {
    const String& type = write_clipboard_item_types_[clipboard_item_index];
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
      script_promise_resolver_->RejectWithDOMException(
          DOMExceptionCode::kNotAllowedError,
          "Type " + type + " does not match the blob's type " + type_with_args);
      return;
    }
    clipboard_item_data_.emplace_back(type, blob_item);
    clipboard_item_index++;
  }
  write_clipboard_item_types_.clear();

  DCHECK(!clipboard_representation_index_);
  WriteNextRepresentation();
}

void ClipboardPromise::HandleWriteWithPermission(
    mojom::blink::PermissionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetExecutionContext()) {
    return;
  }
  if (status != mojom::blink::PermissionStatus::GRANTED) {
    script_promise_resolver_->RejectWithDOMException(
        DOMExceptionCode::kNotAllowedError, "Write permission denied.");
    return;
  }

  HeapVector<ScriptPromiseUntyped> promise_list;
  promise_list.ReserveInitialCapacity(
      clipboard_item_data_with_promises_.size());
  write_clipboard_item_types_.ReserveInitialCapacity(
      clipboard_item_data_with_promises_.size());
  // Check that all types are valid.
  for (const auto& type_and_promise_to_blob :
       clipboard_item_data_with_promises_) {
    const String& type = type_and_promise_to_blob.first;
    write_clipboard_item_types_.emplace_back(type);
    promise_list.emplace_back(type_and_promise_to_blob.second);
    if (!ClipboardItem::supports(type)) {
      script_promise_resolver_->RejectWithDOMException(
          DOMExceptionCode::kNotAllowedError,
          "Type " + type + " not supported on write.");
      return;
    }
  }
  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);
  BlobPromiseResolverFunction::Create(
      script_state, ScriptPromiseUntyped::All(script_state, promise_list),
      this);
}

void ClipboardPromise::HandleWriteTextWithPermission(
    mojom::blink::PermissionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetExecutionContext()) {
    return;
  }
  if (status != mojom::blink::PermissionStatus::GRANTED) {
    script_promise_resolver_->RejectWithDOMException(
        DOMExceptionCode::kNotAllowedError, "Write permission denied.");
    return;
  }

  SystemClipboard* system_clipboard = GetLocalFrame()->GetSystemClipboard();
  system_clipboard->WritePlainText(plain_text_);
  system_clipboard->CommitWrite();
  script_promise_resolver_->DowncastTo<IDLUndefined>()->Resolve();
}

void ClipboardPromise::RejectBlobPromise(const String& exception_text) {
  script_promise_resolver_->RejectWithDOMException(
      DOMExceptionCode::kNotAllowedError, exception_text);
}

PermissionService* ClipboardPromise::GetPermissionService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ExecutionContext* context = GetExecutionContext();
  DCHECK(context);
  if (!permission_service_.is_bound()) {
    ConnectToPermissionService(context,
                               permission_service_.BindNewPipeAndPassReceiver(
                                   GetClipboardTaskRunner()));
  }
  return permission_service_.get();
}

void ClipboardPromise::ValidatePreconditions(
    mojom::blink::PermissionName permission,
    bool will_be_sanitized,
    base::OnceCallback<void(mojom::blink::PermissionStatus)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(script_promise_resolver_);
  DCHECK(permission == mojom::blink::PermissionName::CLIPBOARD_READ ||
         permission == mojom::blink::PermissionName::CLIPBOARD_WRITE);

  ExecutionContext* context = GetExecutionContext();
  DCHECK(context);
  LocalDOMWindow& window = *To<LocalDOMWindow>(context);
  DCHECK(window.IsSecureContext());  // [SecureContext] in IDL

  if (!window.document()->hasFocus()) {
    script_promise_resolver_->RejectWithDOMException(
        DOMExceptionCode::kNotAllowedError, "Document is not focused.");
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
    script_promise_resolver_->RejectWithDOMException(
        DOMExceptionCode::kNotAllowedError, kFeaturePolicyMessage);
    return;
  }

  // Grant permission by-default if extension has read/write permissions.
  if (GetLocalFrame()->GetContentSettingsClient() &&
      ((permission == mojom::blink::PermissionName::CLIPBOARD_READ &&
        GetLocalFrame()
            ->GetContentSettingsClient()
            ->AllowReadFromClipboard()) ||
       (permission == mojom::blink::PermissionName::CLIPBOARD_WRITE &&
        GetLocalFrame()
            ->GetContentSettingsClient()
            ->AllowWriteToClipboard()))) {
    GetClipboardTaskRunner()->PostTask(
        FROM_HERE, WTF::BindOnce(std::move(callback),
                                 mojom::blink::PermissionStatus::GRANTED));
    return;
  }

  if ((permission == mojom::blink::PermissionName::CLIPBOARD_WRITE &&
       ClipboardCommands::IsExecutingCutOrCopy(*context)) ||
      (permission == mojom::blink::PermissionName::CLIPBOARD_READ &&
       ClipboardCommands::IsExecutingPaste(*context))) {
    GetClipboardTaskRunner()->PostTask(
        FROM_HERE, WTF::BindOnce(std::move(callback),
                                 mojom::blink::PermissionStatus::GRANTED));
    return;
  }

  if (!GetPermissionService()) {
    script_promise_resolver_->RejectWithDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Permission Service could not connect.");
    return;
  }

  bool has_transient_user_activation =
      LocalFrame::HasTransientUserActivation(GetLocalFrame());
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
  // In case the context was destroyed and the caller didn't check for it, we
  // just return nullptr.
  if (!context) {
    return nullptr;
  }
  LocalFrame* local_frame = To<LocalDOMWindow>(context)->GetFrame();
  return local_frame;
}

ScriptState* ClipboardPromise::GetScriptState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return script_promise_resolver_->GetScriptState();
}

scoped_refptr<base::SingleThreadTaskRunner>
ClipboardPromise::GetClipboardTaskRunner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetExecutionContext()->GetTaskRunner(TaskType::kClipboard);
}

// ExecutionContextLifecycleObserver implementation.
void ClipboardPromise::ContextDestroyed() {
  // This isn't the correct way to create a DOMException, but the correct way
  // probably wouldn't work at this point, and it probably doesn't matter.
  script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotAllowedError, "Document detached."));
  clipboard_writer_.Clear();
}

void ClipboardPromise::Trace(Visitor* visitor) const {
  visitor->Trace(script_promise_resolver_);
  visitor->Trace(clipboard_writer_);
  visitor->Trace(permission_service_);
  visitor->Trace(clipboard_item_data_);
  visitor->Trace(clipboard_item_data_with_promises_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
