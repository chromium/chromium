// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet.h"

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_worklet_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_run_operation_method_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_url_with_metadata.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_config.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage_window_supplement.h"
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

std::optional<BlinkCloneableMessage> Serialize(
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
    return std::nullopt;
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

// static
SharedStorageWorklet* SharedStorageWorklet::Create(
    ScriptState* script_state,
    bool cross_origin_script_allowed) {
  return MakeGarbageCollected<SharedStorageWorklet>(
      cross_origin_script_allowed);
}

void SharedStorageWorklet::Trace(Visitor* visitor) const {
  visitor->Trace(worklet_host_);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise<IDLUndefined> SharedStorageWorklet::addModule(
    ScriptState* script_state,
    const String& module_url,
    const WorkletOptions* options,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  AddModuleHelper(script_state, resolver, module_url, options, exception_state,
                  /*resolve_to_worklet=*/false,
                  SharedStorageDataOrigin::kContextOrigin);
  return promise;
}

void SharedStorageWorklet::AddModuleHelper(
    ScriptState* script_state,
    ScriptPromiseResolverBase* resolver,
    const String& module_url,
    const WorkletOptions* options,
    ExceptionState& exception_state,
    bool resolve_to_worklet,
    SharedStorageDataOrigin data_origin_type) {
  if (!CheckBrowsingContextIsValid(*script_state, exception_state)) {
    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kAddModuleWebVisible);
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  // An opaque data origin is not allowed. Here we reject the case where the
  // context origin is opaque and used as the data origin. Below we will address
  // the case where the script origin is opaque and used as the data origin.
  bool use_script_origin_as_data_origin =
      resolve_to_worklet &&
      (!base::FeatureList::IsEnabled(
           features::kSharedStorageCreateWorkletUseContextOriginByDefault) ||
       data_origin_type == SharedStorageDataOrigin::kScriptOrigin);

  if (!use_script_origin_as_data_origin &&
      execution_context->GetSecurityOrigin()->IsOpaque()) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidAccessError,
        kOpaqueContextOriginCheckErrorMessage));
    return;
  }

  KURL script_source_url = execution_context->CompleteURL(module_url);

  if (!script_source_url.IsValid()) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataError,
        "The module script url is invalid."));
    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kAddModuleWebVisible);
    return;
  }

  scoped_refptr<SecurityOrigin> script_security_origin =
      SecurityOrigin::Create(script_source_url);

  if (!resolve_to_worklet &&
      !execution_context->GetSecurityOrigin()->IsSameOriginWith(
          script_security_origin.get())) {
    // This `addModule()` call could be affected by the breaking change
    // proposed in https://github.com/WICG/shared-storage/pull/158 and now
    // implemented behind `blink::features::kSharedStorageCrossOriginScript`.
    // Measure its usage.
    execution_context->CountUse(
        WebFeature::kSharedStorageAPI_AddModule_CrossOriginScript);
  }

  if (!cross_origin_script_allowed_ &&
      !execution_context->GetSecurityOrigin()->IsSameOriginWith(
          script_security_origin.get())) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataError,
        "Only same origin module script is allowed."));
    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kAddModuleWebVisible);
    return;
  }

  if (worklet_host_) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kOperationError,
        "addModule() can only be invoked once per worklet."));
    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kAddModuleWebVisible);
    return;
  }

  if (resolve_to_worklet &&
      !execution_context->GetSecurityOrigin()->IsSameOriginWith(
          script_security_origin.get()) &&
      data_origin_type != SharedStorageDataOrigin::kScriptOrigin) {
    // This `createWorklet()` call could be affected by the breaking change
    // proposed in https://github.com/WICG/shared-storage/pull/158 and now
    // implemented behind
    // `blink::features::kSharedStorageCreateWorkletUseContextOriginByDefault`.
    // Increment the use counter.
    execution_context->CountUse(
        WebFeature::
            kSharedStorageAPI_CreateWorklet_CrossOriginScriptDefaultDataOrigin);
  }

  scoped_refptr<SecurityOrigin> shared_storage_security_origin =
      use_script_origin_as_data_origin
          ? script_security_origin->IsolatedCopy()
          : execution_context->GetSecurityOrigin()->IsolatedCopy();

  // Opaque data origins are not allowed. Earlier we rejected the case where the
  // context origin was both opaque and used as the data origin. Here we reject
  // the case where the script origin is opaque and used as the data origin.
  if (use_script_origin_as_data_origin &&
      shared_storage_security_origin->IsOpaque()) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidAccessError,
        kOpaqueDataOriginCheckErrorMessage));
    return;
  }

  url::Origin shared_storage_origin =
      shared_storage_security_origin->ToUrlOrigin();

  const PermissionsPolicy* policy =
      execution_context->GetSecurityContext().GetPermissionsPolicy();
  if (!policy || !policy->IsFeatureEnabledForOrigin(
                     mojom::blink::PermissionsPolicyFeature::kSharedStorage,
                     shared_storage_origin)) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidAccessError,
        "The \"shared-storage\" Permissions Policy denied the method for the "
        "worklet origin."));

    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kAddModuleWebVisible);
    return;
  }

  shared_storage_origin_ = std::move(shared_storage_origin);

  network::mojom::CredentialsMode credentials_mode =
      Request::V8RequestCredentialsToCredentialsMode(
          options->credentials().AsEnum());

  std::unique_ptr<Vector<mojom::blink::OriginTrialFeature>>
      origin_trial_features =
          OriginTrialContext::GetInheritedTrialFeatures(execution_context);

  SharedStorageWindowSupplement::From(To<LocalDOMWindow>(*execution_context))
      ->GetSharedStorageDocumentService()
      ->CreateWorklet(
          script_source_url, shared_storage_security_origin, credentials_mode,
          origin_trial_features ? *origin_trial_features
                                : Vector<mojom::blink::OriginTrialFeature>(),
          worklet_host_.BindNewEndpointAndPassReceiver(
              execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)),
          WTF::BindOnce(
              [](ScriptPromiseResolverBase* resolver,
                 SharedStorageWorklet* shared_storage_worklet,
                 base::TimeTicks start_time, bool resolve_to_worklet,
                 bool success, const String& error_message) {
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

                if (resolve_to_worklet) {
                  resolver->DowncastTo<SharedStorageWorklet>()->Resolve(
                      shared_storage_worklet);
                } else {
                  resolver->DowncastTo<IDLUndefined>()->Resolve();
                }

                // `SharedStorageWorkletErrorType::kSuccess` is logged in the
                // browser process for `addModule()` and `createWorklet()`.
              },
              WrapPersistent(resolver), WrapPersistent(this), start_time,
              resolve_to_worklet));
}

// This C++ overload is called by JavaScript:
// sharedStorage.selectURL('foo', [{url: "bar.com"}]);
//
// It returns a JavaScript promise that resolves to an urn::uuid.
ScriptPromise<V8SharedStorageResponse> SharedStorageWorklet::selectURL(
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
ScriptPromise<V8SharedStorageResponse> SharedStorageWorklet::selectURL(
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
    return EmptyPromise();
  }

  LocalFrame* frame = To<LocalDOMWindow>(execution_context)->GetFrame();
  DCHECK(frame);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8SharedStorageResponse>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  if (!worklet_host_) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kOperationError,
        "sharedStorage.worklet.addModule() has to be called before "
        "selectURL()."));

    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kSelectURLWebVisible);

    return promise;
  }

  // The `kSharedStorage` permissions policy should have been checked in
  // addModule() already.
  const PermissionsPolicy* policy =
      execution_context->GetSecurityContext().GetPermissionsPolicy();
  CHECK(policy);
  CHECK(policy->IsFeatureEnabledForOrigin(
      mojom::blink::PermissionsPolicyFeature::kSharedStorage,
      shared_storage_origin_));

  if (!policy->IsFeatureEnabledForOrigin(
          mojom::blink::PermissionsPolicyFeature::kSharedStorageSelectUrl,
          shared_storage_origin_)) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidAccessError,
        "The \"shared-storage-select-url\" Permissions Policy denied the "
        "method for the worklet origin."));

    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kSelectURLWebVisible);

    return promise;
  }

  if (!cross_origin_script_allowed_) {
    // The opaque origin should have been checked in addModule() already.
    CHECK(!execution_context->GetSecurityOrigin()->IsOpaque());
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

  std::optional<BlinkCloneableMessage> serialized_data =
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

  mojom::blink::PrivateAggregationConfigPtr private_aggregation_config;
  if (!CheckPrivateAggregationConfig(*options, *script_state, *resolver,
                                     /*out_private_aggregation_config=*/
                                     private_aggregation_config)) {
    LogSharedStorageWorkletError(
        SharedStorageWorkletErrorType::kSelectURLWebVisible);
    return promise;
  }

  worklet_host_->SelectURL(
      name, std::move(converted_urls), std::move(*serialized_data), keep_alive,
      std::move(private_aggregation_config),
      WTF::BindOnce(
          [](ScriptPromiseResolver<V8SharedStorageResponse>* resolver,
             SharedStorageWorklet* shared_storage_worklet,
             base::TimeTicks start_time, bool resolve_to_config, bool success,
             const String& error_message,
             const std::optional<FencedFrame::RedactedFencedFrameConfig>&
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

            // `SharedStorageWorkletErrorType::kSuccess` is logged in the
            // browser process for `selectURL()`.
          },
          WrapPersistent(resolver), WrapPersistent(this), start_time,
          resolve_to_config));

  return promise;
}

ScriptPromise<IDLAny> SharedStorageWorklet::run(
    ScriptState* script_state,
    const String& name,
    ExceptionState& exception_state) {
  return run(script_state, name,
             SharedStorageRunOperationMethodOptions::Create(), exception_state);
}

ScriptPromise<IDLAny> SharedStorageWorklet::run(
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
    return EmptyPromise();
  }

  std::optional<BlinkCloneableMessage> serialized_data =
      Serialize(options, *execution_context, exception_state);
  if (!serialized_data) {
    LogSharedStorageWorkletError(SharedStorageWorkletErrorType::kRunWebVisible);
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  if (!worklet_host_) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kOperationError,
        "sharedStorage.worklet.addModule() has to be called before run()."));

    LogSharedStorageWorkletError(SharedStorageWorkletErrorType::kRunWebVisible);

    return promise;
  }

  // The `kSharedStorage` permissions policy should have been checked in
  // addModule() already.
  const PermissionsPolicy* policy =
      execution_context->GetSecurityContext().GetPermissionsPolicy();
  CHECK(policy);
  CHECK(policy->IsFeatureEnabledForOrigin(
      mojom::blink::PermissionsPolicyFeature::kSharedStorage,
      shared_storage_origin_));

  if (!cross_origin_script_allowed_) {
    // The opaque origin should have been checked in addModule() already.
    CHECK(!execution_context->GetSecurityOrigin()->IsOpaque());
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

  mojom::blink::PrivateAggregationConfigPtr private_aggregation_config;
  if (!CheckPrivateAggregationConfig(
          *options, *script_state, *resolver,
          /*out_private_aggregation_config=*/private_aggregation_config)) {
    LogSharedStorageWorkletError(SharedStorageWorkletErrorType::kRunWebVisible);
    return promise;
  }

  worklet_host_->Run(
      name, std::move(*serialized_data), keep_alive,
      std::move(private_aggregation_config),
      WTF::BindOnce(
          [](ScriptPromiseResolver<IDLAny>* resolver,
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

            // `SharedStorageWorkletErrorType::kSuccess` is logged in the
            // browser process for `run()`.
          },
          WrapPersistent(resolver), WrapPersistent(this), start_time));

  return promise;
}

SharedStorageWorklet::SharedStorageWorklet(bool cross_origin_script_allowed)
    : cross_origin_script_allowed_(cross_origin_script_allowed) {}

}  // namespace blink
