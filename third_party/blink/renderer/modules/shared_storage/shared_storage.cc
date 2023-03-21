// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_run_operation_method_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_set_method_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_url_with_metadata.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_config.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet.h"
#include "third_party/blink/renderer/modules/shared_storage/util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

namespace {

// Use the native v8::ValueSerializer here as opposed to using
// blink::V8ScriptValueSerializer. It's capable of serializing objects of
// primitive types. It's TBD whether we want to support any other non-primitive
// types supported by blink::V8ScriptValueSerializer.
bool Serialize(ScriptState* script_state,
               const SharedStorageRunOperationMethodOptions* options,
               ExceptionState& exception_state,
               Vector<uint8_t>& output) {
  DCHECK(output.empty());

  if (!options->hasData())
    return true;

  v8::Isolate* isolate = script_state->GetIsolate();
  v8::ValueSerializer serializer(isolate);

  v8::TryCatch try_catch(isolate);

  bool wrote_value;
  if (!serializer
           .WriteValue(script_state->GetContext(), options->data().V8Value())
           .To(&wrote_value)) {
    DCHECK(try_catch.HasCaught());
    exception_state.RethrowV8Exception(try_catch.Exception());
    return false;
  }

  DCHECK(wrote_value);

  std::pair<uint8_t*, size_t> buffer = serializer.Release();

  output.ReserveInitialCapacity(base::checked_cast<wtf_size_t>(buffer.second));
  output.Append(buffer.first, static_cast<wtf_size_t>(buffer.second));
  DCHECK_EQ(output.size(), buffer.second);

  free(buffer.first);

  return true;
}

void LogTimingHistogramForVoidOperation(
    blink::SharedStorageVoidOperation caller,
    base::TimeTicks start_time) {
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  switch (caller) {
    case blink::SharedStorageVoidOperation::kRun:
      base::UmaHistogramMediumTimes("Storage.SharedStorage.Document.Timing.Run",
                                    elapsed_time);
      break;
    case blink::SharedStorageVoidOperation::kSet:
      base::UmaHistogramMediumTimes("Storage.SharedStorage.Document.Timing.Set",
                                    elapsed_time);
      break;
    case blink::SharedStorageVoidOperation::kAppend:
      base::UmaHistogramMediumTimes(
          "Storage.SharedStorage.Document.Timing.Append", elapsed_time);
      break;
    case blink::SharedStorageVoidOperation::kDelete:
      base::UmaHistogramMediumTimes(
          "Storage.SharedStorage.Document.Timing.Delete", elapsed_time);
      break;
    case blink::SharedStorageVoidOperation::kClear:
      base::UmaHistogramMediumTimes(
          "Storage.SharedStorage.Document.Timing.Clear", elapsed_time);
      break;
    default:
      NOTREACHED();
  }
}

void OnVoidOperationFinished(ScriptPromiseResolver* resolver,
                             SharedStorage* shared_storage,
                             blink::SharedStorageVoidOperation caller,
                             base::TimeTicks start_time,
                             bool success,
                             const String& error_message) {
  DCHECK(resolver);
  ScriptState* script_state = resolver->GetScriptState();

  if (!success) {
    ScriptState::Scope scope(script_state);
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kOperationError,
        error_message));
    if (caller == blink::SharedStorageVoidOperation::kRun) {
      LogSharedStorageWorkletError(
          SharedStorageWorkletErrorType::kRunWebVisible);
    }
    return;
  }

  LogTimingHistogramForVoidOperation(caller, start_time);
  resolver->Resolve();
}

// TODO(crbug.com/1335504): Consider moving this function to
// third_party/blink/common/fenced_frame/fenced_frame_utils.cc.
bool IsValidFencedFrameReportingURL(const KURL& url) {
  if (!url.IsValid())
    return false;
  return url.ProtocolIs("https");
}

bool StringFromV8(v8::Isolate* isolate, v8::Local<v8::Value> val, String* out) {
  DCHECK(out);

  if (!val->IsString())
    return false;

  v8::Local<v8::String> str = v8::Local<v8::String>::Cast(val);
  wtf_size_t length = str->Utf8Length(isolate);
  LChar* buffer;
  *out = String::CreateUninitialized(length, buffer);

  str->WriteUtf8(isolate, reinterpret_cast<char*>(buffer), length, nullptr,
                 v8::String::NO_NULL_TERMINATION);

  return true;
}

}  // namespace

SharedStorage::SharedStorage() = default;
SharedStorage::~SharedStorage() = default;

void SharedStorage::Trace(Visitor* visitor) const {
  visitor->Trace(shared_storage_worklet_);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise SharedStorage::set(ScriptState* script_state,
                                 const String& key,
                                 const String& value,
                                 ExceptionState& exception_state) {
  return set(script_state, key, value, SharedStorageSetMethodOptions::Create(),
             exception_state);
}

ScriptPromise SharedStorage::set(ScriptState* script_state,
                                 const String& key,
                                 const String& value,
                                 const SharedStorageSetMethodOptions* options,
                                 ExceptionState& exception_state) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  if (!CheckBrowsingContextIsValid(*script_state, exception_state))
    return ScriptPromise();

  ScriptPromiseResolver* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  if (!CheckSharedStoragePermissionsPolicy(*script_state, *execution_context,
                                           *resolver)) {
    return promise;
  }

  if (!IsValidSharedStorageKeyStringLength(key.length())) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataError,
        "Length of the \"key\" parameter is not valid."));
    return promise;
  }

  if (!IsValidSharedStorageValueStringLength(value.length())) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataError,
        "Length of the \"value\" parameter is not valid."));
    return promise;
  }

  bool ignore_if_present =
      options->hasIgnoreIfPresent() && options->ignoreIfPresent();
  GetSharedStorageDocumentService(execution_context)
      ->SharedStorageSet(
          key, value, ignore_if_present,
          WTF::BindOnce(&OnVoidOperationFinished, WrapPersistent(resolver),
                        WrapPersistent(this),
                        blink::SharedStorageVoidOperation::kSet, start_time));

  return promise;
}

ScriptPromise SharedStorage::append(ScriptState* script_state,
                                    const String& key,
                                    const String& value,
                                    ExceptionState& exception_state) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  if (!CheckBrowsingContextIsValid(*script_state, exception_state))
    return ScriptPromise();

  ScriptPromiseResolver* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  if (!CheckSharedStoragePermissionsPolicy(*script_state, *execution_context,
                                           *resolver)) {
    return promise;
  }

  if (!IsValidSharedStorageKeyStringLength(key.length())) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataError,
        "Length of the \"key\" parameter is not valid."));
    return promise;
  }

  if (!IsValidSharedStorageValueStringLength(value.length())) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataError,
        "Length of the \"value\" parameter is not valid."));
    return promise;
  }

  GetSharedStorageDocumentService(execution_context)
      ->SharedStorageAppend(
          key, value,
          WTF::BindOnce(&OnVoidOperationFinished, WrapPersistent(resolver),
                        WrapPersistent(this),
                        blink::SharedStorageVoidOperation::kAppend,
                        start_time));

  return promise;
}

ScriptPromise SharedStorage::Delete(ScriptState* script_state,
                                    const String& key,
                                    ExceptionState& exception_state) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  if (!CheckBrowsingContextIsValid(*script_state, exception_state))
    return ScriptPromise();

  ScriptPromiseResolver* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  if (!CheckSharedStoragePermissionsPolicy(*script_state, *execution_context,
                                           *resolver)) {
    return promise;
  }

  if (!IsValidSharedStorageKeyStringLength(key.length())) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataError,
        "Length of the \"key\" parameter is not valid."));
    return promise;
  }

  GetSharedStorageDocumentService(execution_context)
      ->SharedStorageDelete(
          key, WTF::BindOnce(&OnVoidOperationFinished, WrapPersistent(resolver),
                             WrapPersistent(this),
                             blink::SharedStorageVoidOperation::kDelete,
                             start_time));

  return promise;
}

ScriptPromise SharedStorage::clear(ScriptState* script_state,
                                   ExceptionState& exception_state) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  if (!CheckBrowsingContextIsValid(*script_state, exception_state))
    return ScriptPromise();

  ScriptPromiseResolver* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  if (!CheckSharedStoragePermissionsPolicy(*script_state, *execution_context,
                                           *resolver)) {
    return promise;
  }

  GetSharedStorageDocumentService(execution_context)
      ->SharedStorageClear(
          WTF::BindOnce(&OnVoidOperationFinished, WrapPersistent(resolver),
                        WrapPersistent(this),
                        blink::SharedStorageVoidOperation::kClear, start_time));

  return promise;
}

// This C++ overload is called by JavaScript:
// sharedStorage.selectURL('foo', [{url: "bar.com"}]);
//
// It returns a JavaScript promise that resolves to an urn::uuid.
ScriptPromise SharedStorage::selectURL(
    ScriptState* script_state,
    const String& name,
    HeapVector<Member<SharedStorageUrlWithMetadata>> urls,
    ExceptionState& exception_state) {
  return selectURL(script_state, name, urls,
                   SharedStorageRunOperationMethodOptions::Create(),
                   exception_state);
}

// This C++ overload is called by JavaScript:
// 1. sharedStorage.selectURL('foo', [{url: "bar.com"}], {data: {'option': 0}});
// 2. sharedStorage.selectURL('foo', [{url: "bar.com"}], {data: {'option': 0},
// resolveToConfig: true});
//
// It returns a JavaScript promise:
// 1. that resolves to an urn::uuid, when `resolveToConfig` is false or
// unspecified.
// 2. that resolves to a fenced frame config, when `resolveToConfig` is true.
//
// This function implements the other overload, with `resolveToConfig`
// defaulting to false.
ScriptPromise SharedStorage::selectURL(
    ScriptState* script_state,
    const String& name,
    HeapVector<Member<SharedStorageUrlWithMetadata>> urls,
    const SharedStorageRunOperationMethodOptions* options,
    ExceptionState& exception_state) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  if (!CheckBrowsingContextIsValid(*script_state, exception_state)) {
    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kSelectURLWebVisible);
    return ScriptPromise();
  }

  LocalFrame* frame = To<LocalDOMWindow>(execution_context)->GetFrame();
  DCHECK(frame);

  ScriptPromiseResolver* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  // For `selectURL()` to succeed, it is currently enforced in the browser side
  // that `addModule()` must be called beforehand that passed the early
  // permission checks. Thus the permissions-policy check here isn't strictly
  // needed. But here we still check the permissions-policy for consistency and
  // consider this a higher priority error.
  if (!CheckSharedStoragePermissionsPolicy(*script_state, *execution_context,
                                           *resolver)) {
    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kSelectURLWebVisible);
    return promise;
  }

  if (!execution_context->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kSharedStorageSelectUrl)) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidAccessError,
        "The \"shared-storage-select-url\" Permissions Policy denied the usage "
        "of window.sharedStorage.selectURL()."));

    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kSelectURLWebVisible);

    return promise;
  }

  if (!IsValidSharedStorageURLsArrayLength(urls.size())) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataError,
        "Length of the \"urls\" parameter is not valid."));
    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kSelectURLWebVisible);
    return promise;
  }

  v8::Local<v8::Context> v8_context =
      script_state->GetIsolate()->GetCurrentContext();

  Vector<mojom::blink::SharedStorageUrlWithMetadataPtr> converted_urls;
  converted_urls.ReserveInitialCapacity(urls.size());

  wtf_size_t index = 0;
  for (const auto& url_with_metadata : urls) {
    DCHECK(url_with_metadata->hasUrl());

    KURL converted_url =
        execution_context->CompleteURL(url_with_metadata->url());

    // TODO(crbug.com/1318970): Use `IsValidFencedFrameURL()` or equivalent
    // logic here.
    if (!converted_url.IsValid()) {
      resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
          script_state->GetIsolate(), DOMExceptionCode::kDataError,
          "The url \"" + url_with_metadata->url() + "\" is invalid."));
      LogSharedStorageWorkletError(
          SharedStorageWorkletErrorType::kSelectURLWebVisible);
      return promise;
    }

    HashMap<String, KURL> converted_reporting_metadata;

    if (url_with_metadata->hasReportingMetadata()) {
      DCHECK(url_with_metadata->reportingMetadata().V8Value()->IsObject());

      v8::Local<v8::Object> obj =
          url_with_metadata->reportingMetadata().V8Value().As<v8::Object>();

      v8::MaybeLocal<v8::Array> maybe_fields =
          obj->GetOwnPropertyNames(v8_context);
      v8::Local<v8::Array> fields;
      if (!maybe_fields.ToLocal(&fields) || fields->Length() == 0) {
        resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
            script_state->GetIsolate(), DOMExceptionCode::kDataError,
            "selectURL could not get reportingMetadata object attributes"));
        LogSharedStorageWorkletError(
            SharedStorageWorkletErrorType::kSelectURLWebVisible);
        return promise;
      }

      converted_reporting_metadata.ReserveCapacityForSize(fields->Length());

      for (wtf_size_t idx = 0; idx < fields->Length(); idx++) {
        v8::Local<v8::Value> report_event =
            fields->Get(v8_context, idx).ToLocalChecked();
        String report_event_string;
        if (!StringFromV8(script_state->GetIsolate(), report_event,
                          &report_event_string)) {
          resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
              script_state->GetIsolate(), DOMExceptionCode::kDataError,
              "selectURL reportingMetadata object attributes must be "
              "strings"));
          LogSharedStorageWorkletError(
              SharedStorageWorkletErrorType::kSelectURLWebVisible);
          return promise;
        }

        v8::Local<v8::Value> report_url =
            obj->Get(v8_context, report_event).ToLocalChecked();
        String report_url_string;
        if (!StringFromV8(script_state->GetIsolate(), report_url,
                          &report_url_string)) {
          resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
              script_state->GetIsolate(), DOMExceptionCode::kDataError,
              "selectURL reportingMetadata object attributes must be "
              "strings"));
          LogSharedStorageWorkletError(
              SharedStorageWorkletErrorType::kSelectURLWebVisible);
          return promise;
        }

        KURL converted_report_url =
            execution_context->CompleteURL(report_url_string);

        if (!IsValidFencedFrameReportingURL(converted_report_url)) {
          resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
              script_state->GetIsolate(), DOMExceptionCode::kDataError,
              "The metadata for the url at index " +
                  String::NumberToStringECMAScript(index) +
                  " has an invalid or non-HTTPS report_url parameter \"" +
                  report_url_string + "\"."));
          LogSharedStorageWorkletError(
              SharedStorageWorkletErrorType::kSelectURLWebVisible);
          return promise;
        }

        converted_reporting_metadata.Set(report_event_string,
                                         converted_report_url);
      }
    }

    converted_urls.push_back(mojom::blink::SharedStorageUrlWithMetadata::New(
        converted_url, std::move(converted_reporting_metadata)));
    index++;
  }

  Vector<uint8_t> serialized_data;
  if (!Serialize(script_state, options, exception_state, serialized_data)) {
    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kSelectURLWebVisible);
    return promise;
  }

  bool resolve_to_config = options->resolveToConfig();
  if (!RuntimeEnabledFeatures::FencedFramesAPIChangesEnabled(
          execution_context)) {
    // If user specifies returning a `FencedFrameConfig` but the feature is not
    // enabled, fall back to return a urn::uuid.
    resolve_to_config = false;
  }

  bool keep_alive = options->keepAlive();

  GetSharedStorageDocumentService(execution_context)
      ->RunURLSelectionOperationOnWorklet(
          name, std::move(converted_urls), std::move(serialized_data),
          keep_alive,
          WTF::BindOnce(
              [](ScriptPromiseResolver* resolver, SharedStorage* shared_storage,
                 base::TimeTicks start_time, bool resolve_to_config,
                 bool success, const String& error_message,
                 const absl::optional<FencedFrame::RedactedFencedFrameConfig>&
                     result_config) {
                DCHECK(resolver);
                ScriptState* script_state = resolver->GetScriptState();

                if (!success) {
                  ScriptState::Scope scope(script_state);
                  resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
                      script_state->GetIsolate(),
                      DOMExceptionCode::kOperationError, error_message));
                  LogSharedStorageWorkletError(
                      SharedStorageWorkletErrorType::kSelectURLWebVisible);
                  return;
                }

                base::UmaHistogramMediumTimes(
                    "Storage.SharedStorage.Document.Timing.SelectURL",
                    base::TimeTicks::Now() - start_time);
                // `result_config` must have value. Otherwise `success` should
                // be false and program should not reach here.
                DCHECK(result_config.has_value());
                if (resolve_to_config) {
                  resolver->Resolve(
                      FencedFrameConfig::From(result_config.value()));
                } else {
                  resolver->Resolve(KURL(result_config->urn_uuid().value()));
                }
              },
              WrapPersistent(resolver), WrapPersistent(this), start_time,
              resolve_to_config));

  return promise;
}

ScriptPromise SharedStorage::run(ScriptState* script_state,
                                 const String& name,
                                 ExceptionState& exception_state) {
  return run(script_state, name,
             SharedStorageRunOperationMethodOptions::Create(), exception_state);
}

ScriptPromise SharedStorage::run(
    ScriptState* script_state,
    const String& name,
    const SharedStorageRunOperationMethodOptions* options,
    ExceptionState& exception_state) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  if (!CheckBrowsingContextIsValid(*script_state, exception_state)) {
    LogSharedStorageWorkletError(SharedStorageWorkletErrorType::kRunWebVisible);
    return ScriptPromise();
  }

  Vector<uint8_t> serialized_data;
  if (!Serialize(script_state, options, exception_state, serialized_data)) {
    LogSharedStorageWorkletError(SharedStorageWorkletErrorType::kRunWebVisible);
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  if (!CheckSharedStoragePermissionsPolicy(*script_state, *execution_context,
                                           *resolver)) {
    LogSharedStorageWorkletError(SharedStorageWorkletErrorType::kRunWebVisible);
    return promise;
  }

  bool keep_alive = options->keepAlive();

  GetSharedStorageDocumentService(execution_context)
      ->RunOperationOnWorklet(
          name, std::move(serialized_data), keep_alive,
          WTF::BindOnce(&OnVoidOperationFinished, WrapPersistent(resolver),
                        WrapPersistent(this),
                        blink::SharedStorageVoidOperation::kRun, start_time));

  return promise;
}

SharedStorageWorklet* SharedStorage::worklet(ScriptState* script_state,
                                             ExceptionState& exception_state) {
  if (shared_storage_worklet_)
    return shared_storage_worklet_.Get();

  shared_storage_worklet_ = MakeGarbageCollected<SharedStorageWorklet>(this);

  return shared_storage_worklet_.Get();
}

mojom::blink::SharedStorageDocumentService*
SharedStorage::GetSharedStorageDocumentService(
    ExecutionContext* execution_context) {
  CHECK(execution_context->IsWindow());
  if (!shared_storage_document_service_.is_bound()) {
    LocalFrame* frame = To<LocalDOMWindow>(execution_context)->GetFrame();
    DCHECK(frame);

    frame->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
        shared_storage_document_service_.BindNewEndpointAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return shared_storage_document_service_.get();
}

}  // namespace blink
