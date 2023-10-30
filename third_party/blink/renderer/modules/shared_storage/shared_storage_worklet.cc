// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet.h"

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_run_operation_method_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_url_with_metadata.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_config.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage.h"
#include "third_party/blink/renderer/modules/shared_storage/util.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "url/origin.h"

namespace blink {

namespace {

const char kSharedStorageWorkletExpiredMessage[] =
    "The sharedStorage worklet cannot execute further operations because the "
    "previous operation did not include the option \'keepAlive: true\'.";

absl::optional<BlinkCloneableMessage> Serialize(
    const SharedStorageRunOperationMethodOptions* options,
    const ExecutionContext& execution_context,
    ExceptionState& exception_state) {
  scoped_refptr<SerializedScriptValue> serialized_value =
      options->hasData()
          ? SerializedScriptValue::Serialize(
                options->data().GetIsolate(), options->data().V8Value(),
                SerializedScriptValue::SerializeOptions(), exception_state)
          : SerializedScriptValue::UndefinedValue();
  if (exception_state.HadException()) {
    return absl::nullopt;
  }

  BlinkCloneableMessage output;
  output.message = std::move(serialized_value);
  output.sender_agent_cluster_id = execution_context.GetAgentClusterID();
  output.sender_origin = execution_context.GetSecurityOrigin()->IsolatedCopy();
  // TODO(yaoxia): do we need to set `output.sender_stack_trace_id`?

  return output;
}

// TODO(crbug.com/1335504): Consider moving this function to
// third_party/blink/common/fenced_frame/fenced_frame_utils.cc.
bool IsValidFencedFrameReportingURL(const KURL& url) {
  if (!url.IsValid()) {
    return false;
  }
  return url.ProtocolIs("https");
}

}  // namespace

SharedStorageWorklet::SharedStorageWorklet(SharedStorage* shared_storage)
    : shared_storage_(shared_storage) {}

void SharedStorageWorklet::Trace(Visitor* visitor) const {
  visitor->Trace(worklet_host_);
  visitor->Trace(shared_storage_);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise SharedStorageWorklet::addModule(ScriptState* script_state,
                                              const String& module_url,
                                              ExceptionState& exception_state) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  if (!CheckBrowsingContextIsValid(*script_state, exception_state)) {
    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kAddModuleWebVisible);
    return ScriptPromise();
  }

  KURL script_source_url = execution_context->CompleteURL(module_url);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  if (!CheckSharedStoragePermissionsPolicy(*script_state, *execution_context,
                                           *resolver)) {
    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kAddModuleWebVisible);
    return promise;
  }

  if (!script_source_url.IsValid()) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataError,
        "The module script url is invalid."));
    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kAddModuleWebVisible);
    return promise;
  }

  scoped_refptr<SecurityOrigin> script_security_origin =
      SecurityOrigin::Create(script_source_url);

  if (!execution_context->GetSecurityOrigin()->IsSameOriginWith(
          script_security_origin.get())) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataError,
        "Only same origin module script is allowed."));
    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kAddModuleWebVisible);
    return promise;
  }

  if (worklet_host_) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kOperationError,
        "sharedStorage.worklet.addModule() can only be invoked once per "
        "browsing context."));
    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kAddModuleWebVisible);
    return promise;
  }

  std::unique_ptr<Vector<mojom::blink::OriginTrialFeature>>
      origin_trial_features =
          OriginTrialContext::GetInheritedTrialFeatures(execution_context);

  shared_storage_->GetSharedStorageDocumentService(execution_context)
      ->CreateWorklet(
          script_source_url,
          origin_trial_features ? *origin_trial_features
                                : Vector<mojom::blink::OriginTrialFeature>(),
          worklet_host_.BindNewEndpointAndPassReceiver(
              execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)),
          WTF::BindOnce(
              [](ScriptPromiseResolver* resolver,
                 SharedStorageWorklet* shared_storage_worklet,
                 base::TimeTicks start_time, bool success,
                 const String& error_message) {
                DCHECK(resolver);
                ScriptState* script_state = resolver->GetScriptState();

                if (!success) {
                  if (IsInParallelAlgorithmRunnable(
                          resolver->GetExecutionContext(), script_state)) {
                    ScriptState::Scope scope(script_state);
                    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
                        script_state->GetIsolate(),
                        DOMExceptionCode::kOperationError, error_message));
                  }

                  LogSharedStorageWorkletError(
                      SharedStorageWorkletErrorType::kAddModuleWebVisible);
                  return;
                }

                base::UmaHistogramMediumTimes(
                    "Storage.SharedStorage.Document.Timing.AddModule",
                    base::TimeTicks::Now() - start_time);
                resolver->Resolve();
              },
              WrapPersistent(resolver), WrapPersistent(this), start_time));

  return promise;
}

ScriptPromise SharedStorageWorklet::SelectURL(
    ScriptState* script_state,
    const String& name,
    HeapVector<Member<SharedStorageUrlWithMetadata>> urls,
    const SharedStorageRunOperationMethodOptions* options,
    ExceptionState& exception_state) {
  CHECK(options);
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

  if (!worklet_host_) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kOperationError,
        "sharedStorage.worklet.addModule() has to be called before "
        "selectURL()."));

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

  absl::optional<BlinkCloneableMessage> serialized_data =
      Serialize(options, *execution_context, exception_state);
  if (!serialized_data) {
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

  if (!keep_alive_after_operation_) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kOperationError,
        kSharedStorageWorkletExpiredMessage));

    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kSelectURLWebVisible);

    return promise;
  }

  bool keep_alive = options->keepAlive();
  keep_alive_after_operation_ = keep_alive;

  WTF::String context_id;
  scoped_refptr<SecurityOrigin> aggregation_coordinator_origin;
  if (!CheckPrivateAggregationConfig(*options, *script_state, *resolver,
                                     /*out_context_id=*/context_id,
                                     /*out_aggregation_coordinator_origin=*/
                                     aggregation_coordinator_origin)) {
    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kSelectURLWebVisible);
    return promise;
  }

  worklet_host_->SelectURL(
      name, std::move(converted_urls), std::move(*serialized_data), keep_alive,
      std::move(context_id), aggregation_coordinator_origin,
      WTF::BindOnce(
          [](ScriptPromiseResolver* resolver,
             SharedStorageWorklet* shared_storage_worklet,
             base::TimeTicks start_time, bool resolve_to_config, bool success,
             const String& error_message,
             const absl::optional<FencedFrame::RedactedFencedFrameConfig>&
                 result_config) {
            DCHECK(resolver);
            ScriptState* script_state = resolver->GetScriptState();

            if (!success) {
              if (IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                                script_state)) {
                ScriptState::Scope scope(script_state);
                resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
                    script_state->GetIsolate(),
                    DOMExceptionCode::kOperationError, error_message));
              }
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
              resolver->Resolve(FencedFrameConfig::From(result_config.value()));
            } else {
              resolver->Resolve(KURL(result_config->urn_uuid().value()));
            }
          },
          WrapPersistent(resolver), WrapPersistent(this), start_time,
          resolve_to_config));

  return promise;
}

ScriptPromise SharedStorageWorklet::Run(
    ScriptState* script_state,
    const String& name,
    const SharedStorageRunOperationMethodOptions* options,
    ExceptionState& exception_state) {
  CHECK(options);
  base::TimeTicks start_time = base::TimeTicks::Now();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  if (!CheckBrowsingContextIsValid(*script_state, exception_state)) {
    LogSharedStorageWorkletError(SharedStorageWorkletErrorType::kRunWebVisible);
    return ScriptPromise();
  }

  absl::optional<BlinkCloneableMessage> serialized_data =
      Serialize(options, *execution_context, exception_state);
  if (!serialized_data) {
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

  if (!worklet_host_) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kOperationError,
        "sharedStorage.worklet.addModule() has to be called before run()."));

    LogSharedStorageWorkletError(SharedStorageWorkletErrorType::kRunWebVisible);

    return promise;
  }

  if (!keep_alive_after_operation_) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kOperationError,
        kSharedStorageWorkletExpiredMessage));

    LogSharedStorageWorkletError(SharedStorageWorkletErrorType::kRunWebVisible);

    return promise;
  }

  bool keep_alive = options->keepAlive();
  keep_alive_after_operation_ = keep_alive;

  WTF::String context_id;
  scoped_refptr<SecurityOrigin> aggregation_coordinator_origin;
  if (!CheckPrivateAggregationConfig(*options, *script_state, *resolver,
                                     /*out_context_id=*/context_id,
                                     /*out_aggregation_coordinator_origin=*/
                                     aggregation_coordinator_origin)) {
    LogSharedStorageWorkletError(SharedStorageWorkletErrorType::kRunWebVisible);
    return promise;
  }

  worklet_host_->Run(
      name, std::move(*serialized_data), keep_alive, std::move(context_id),
      std::move(aggregation_coordinator_origin),
      WTF::BindOnce(
          [](ScriptPromiseResolver* resolver,
             SharedStorageWorklet* shared_storage_worklet,
             base::TimeTicks start_time, bool success,
             const String& error_message) {
            DCHECK(resolver);
            ScriptState* script_state = resolver->GetScriptState();

            if (!success) {
              if (IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                                script_state)) {
                ScriptState::Scope scope(script_state);
                resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
                    script_state->GetIsolate(),
                    DOMExceptionCode::kOperationError, error_message));
              }

              LogSharedStorageWorkletError(
                  SharedStorageWorkletErrorType::kRunWebVisible);
              return;
            }

            base::UmaHistogramMediumTimes(
                "Storage.SharedStorage.Document.Timing.Run",
                base::TimeTicks::Now() - start_time);
            resolver->Resolve();
          },
          WrapPersistent(resolver), WrapPersistent(this), start_time));

  return promise;
}

}  // namespace blink
