// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/cache_storage.h"

#include <utility>

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/cache_storage/cache_storage_utils.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_request_usvstring.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_multi_cache_query_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage_blob_client_list.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage_error.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage_trace_utils.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_utils.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/network/http_names.h"

namespace mojo {

using blink::mojom::blink::CacheQueryOptions;
using blink::mojom::blink::CacheQueryOptionsPtr;
using blink::mojom::blink::MultiCacheQueryOptions;
using blink::mojom::blink::MultiCacheQueryOptionsPtr;

template <>
struct TypeConverter<MultiCacheQueryOptionsPtr,
                     const blink::MultiCacheQueryOptions*> {
  static MultiCacheQueryOptionsPtr Convert(
      const blink::MultiCacheQueryOptions* input) {
    CacheQueryOptionsPtr query_options = CacheQueryOptions::New();
    query_options->ignore_search = input->ignoreSearch();
    query_options->ignore_method = input->ignoreMethod();
    query_options->ignore_vary = input->ignoreVary();

    MultiCacheQueryOptionsPtr output = MultiCacheQueryOptions::New();
    output->query_options = std::move(query_options);
    if (input->hasCacheName()) {
      output->cache_name = input->cacheName();
    }
    return output;
  }
};

}  // namespace mojo

namespace blink {

// Controls whether we apply an artificial delay to priming the CacheStorage
// data for all APIs. There are 2 parameters for each API that influence how
// long the delay is, `factor` and `offset`. If the actual time taken is
// `elapse` then the delay will be `elapse * factor + offset`.
BASE_FEATURE(kCacheStorageAblation, base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the ablation delay time per each API call.
BASE_FEATURE_PARAM(double,
                   kCacheStorageAblationOpenFactor,
                   &kCacheStorageAblation,
                   "open_factor",
                   0);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kCacheStorageAblationOpenOffset,
                   &kCacheStorageAblation,
                   "open_offset",
                   base::Milliseconds(0));

BASE_FEATURE_PARAM(double,
                   kCacheStorageAblationHasFactor,
                   &kCacheStorageAblation,
                   "has_factor",
                   0);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kCacheStorageAblationHasOffset,
                   &kCacheStorageAblation,
                   "has_offset",
                   base::Milliseconds(0));

BASE_FEATURE_PARAM(double,
                   kCacheStorageAblationDeleteFactor,
                   &kCacheStorageAblation,
                   "delete_factor",
                   0);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kCacheStorageAblationDeleteOffset,
                   &kCacheStorageAblation,
                   "delete_offset",
                   base::Milliseconds(0));

BASE_FEATURE_PARAM(double,
                   kCacheStorageAblationMatchFactor,
                   &kCacheStorageAblation,
                   "match_factor",
                   0);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kCacheStorageAblationMatchOffset,
                   &kCacheStorageAblation,
                   "match_offset",
                   base::Milliseconds(0));

BASE_FEATURE_PARAM(double,
                   kCacheStorageAblationKeysFactor,
                   &kCacheStorageAblation,
                   "keys_factor",
                   0);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kCacheStorageAblationKeysOffset,
                   &kCacheStorageAblation,
                   "keys_offset",
                   base::Milliseconds(0));

namespace {

const char kSecurityErrorMessage[] =
    "An attempt was made to break through the security policy of the user "
    "agent.";

void OpenComplete(GlobalFetch::ScopedFetcher* fetcher,
                  CacheStorageBlobClientList* blob_client_list,
                  int64_t trace_id,
                  mojom::blink::CacheStorage::OpenResult result,
                  ScriptPromiseResolver<Cache>* resolver) {
  if (!result.has_value()) {
    TRACE_EVENT_WITH_FLOW1("CacheStorage", "CacheStorage::Open::Callback",
                           TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN,
                           "status", CacheStorageTracedValue(result.error()));
    RejectCacheStorageWithError(resolver, result.error());
  } else {
    TRACE_EVENT_WITH_FLOW1("CacheStorage", "CacheStorage::Open::Callback",
                           TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN,
                           "status", "success");
    // See https://bit.ly/2S0zRAS for task types.
    resolver->Resolve(MakeGarbageCollected<Cache>(
        fetcher, blob_client_list, std::move(result.value()),
        resolver->GetExecutionContext(), blink::TaskType::kMiscPlatformAPI));
  }
}

void HasComplete(mojom::blink::CacheStorageError result,
                 ScriptPromiseResolver<IDLBoolean>* resolver) {
  switch (result) {
    case mojom::blink::CacheStorageError::kSuccess:
      resolver->Resolve(true);
      break;
    case mojom::blink::CacheStorageError::kErrorNotFound:
      resolver->Resolve(false);
      break;
    default:
      RejectCacheStorageWithError(resolver, result);
      break;
  }
}

void DeleteComplete(mojom::blink::CacheStorageError result,
                    ScriptPromiseResolver<IDLBoolean>* resolver) {
  switch (result) {
    case mojom::blink::CacheStorageError::kSuccess:
      resolver->Resolve(true);
      break;
    case mojom::blink::CacheStorageError::kErrorStorage:
    case mojom::blink::CacheStorageError::kErrorNotFound:
      resolver->Resolve(false);
      break;
    default:
      RejectCacheStorageWithError(resolver, result);
      break;
  }
}

void MatchComplete(int64_t trace_id,
                   mojom::blink::CacheStorage::MatchResult result,
                   CacheStorageBlobClientList* blob_client_list,
                   ScriptPromiseResolver<Response>* resolver) {
  if (!result.has_value()) {
    TRACE_EVENT_WITH_FLOW1("CacheStorage", "CacheStorage::MatchImpl::Callback",
                           TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN,
                           "status", CacheStorageTracedValue(result.error()));
    switch (result.error()) {
      case mojom::CacheStorageError::kErrorNotFound:
      case mojom::CacheStorageError::kErrorStorage:
      case mojom::CacheStorageError::kErrorCacheNameNotFound:
        resolver->Resolve();
        break;
      default:
        RejectCacheStorageWithError(resolver, result.error());
        break;
    }
  } else {
    ScriptState::Scope scope(resolver->GetScriptState());
    auto& match_response = result.value();
    if (match_response->is_eager_response()) {
      TRACE_EVENT_WITH_FLOW1(
          "CacheStorage", "CacheStorage::MatchImpl::Callback",
          TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN, "eager_response",
          CacheStorageTracedValue(
              match_response->get_eager_response()->response));
      resolver->Resolve(CreateEagerResponse(
          resolver->GetScriptState(),
          std::move(match_response->get_eager_response()), blob_client_list));
    } else {
      TRACE_EVENT_WITH_FLOW1(
          "CacheStorage", "CacheStorage::MatchImpl::Callback",
          TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN, "response",
          CacheStorageTracedValue(match_response->get_response()));
      resolver->Resolve(Response::Create(resolver->GetScriptState(),
                                         *match_response->get_response()));
    }
  }
}

int GetAblationFactor(std::string_view operation_name) {
  if (operation_name == "Open") {
    return kCacheStorageAblationOpenFactor.Get();
  } else if (operation_name == "Has") {
    return kCacheStorageAblationHasFactor.Get();
  } else if (operation_name == "Delete") {
    return kCacheStorageAblationDeleteFactor.Get();
  } else if (operation_name == "Match") {
    return kCacheStorageAblationMatchFactor.Get();
  } else if (operation_name == "Keys") {
    return kCacheStorageAblationKeysFactor.Get();
  } else {
    NOTREACHED();
  }
}

base::TimeDelta GetAblationOffset(std::string_view operation_name) {
  if (operation_name == "Open") {
    return kCacheStorageAblationOpenOffset.Get();
  } else if (operation_name == "Has") {
    return kCacheStorageAblationHasOffset.Get();
  } else if (operation_name == "Delete") {
    return kCacheStorageAblationDeleteOffset.Get();
  } else if (operation_name == "Match") {
    return kCacheStorageAblationMatchOffset.Get();
  } else if (operation_name == "Keys") {
    return kCacheStorageAblationKeysOffset.Get();
  } else {
    NOTREACHED();
  }
}

void ProcessCompletion(base::OnceCallback<void()> complete,
                       base::TimeTicks start_time,
                       ExecutionContext* context,
                       const std::string& operation_name) {
  // Run `complete` immediately if kCacheStorageAblation feature is disabled.
  if (!base::FeatureList::IsEnabled(kCacheStorageAblation)) {
    std::move(complete).Run();
    return;
  }

  // Run `complete` with delay if kCacheStorageAblation feature is enabled to
  // run the ablation test.
  base::TimeDelta delay_to_schedule = (base::TimeTicks::Now() - start_time) *
                                          GetAblationFactor(operation_name) +
                                      GetAblationOffset(operation_name);

  if (delay_to_schedule.is_positive()) {
    context->GetTaskRunner(blink::TaskType::kMiscPlatformAPI)
        ->PostDelayedTask(FROM_HERE,
                          blink::BindOnce(
                              [](base::TimeTicks start_time,
                                 const std::string& operation_name) {
                                // Measure actual delay to record as metrics.
                                base::TimeDelta actual_delay =
                                    base::TimeTicks::Now() - start_time;
                                base::UmaHistogramTimes(
                                    "ServiceWorkerCache.CacheStorage."
                                    "Renderer." +
                                        operation_name + ".AblationDelay",
                                    actual_delay);
                              },
                              start_time, operation_name)
                              .Then(std::move(complete)),
                          delay_to_schedule);
  } else {
    std::move(complete).Run();
  }
}

}  // namespace

void CacheStorage::IsCacheStorageAllowed(ExecutionContext* context,
                                         ScriptPromiseResolverBase* resolver,
                                         base::OnceCallback<void()> callback) {
  DCHECK(context->IsWindow() || context->IsWorkerGlobalScope());

  auto wrapped_callback = blink::BindOnce(
      &CacheStorage::OnCacheStorageAllowed, WrapWeakPersistent(this),
      std::move(callback), WrapPersistent(resolver));

  if (allowed_.has_value()) {
    std::move(wrapped_callback).Run(allowed_.value());
    return;
  }

  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    LocalFrame* frame = window->GetFrame();
    if (!frame) {
      std::move(wrapped_callback).Run(false);
      return;
    }
    frame->AllowStorageAccessAndNotify(
        WebContentSettingsClient::StorageType::kCacheStorage,
        std::move(wrapped_callback));
  } else {
    WebContentSettingsClient* settings_client =
        To<WorkerGlobalScope>(context)->ContentSettingsClient();
    if (!settings_client) {
      std::move(wrapped_callback).Run(true);
      return;
    }
    settings_client->AllowStorageAccess(
        WebContentSettingsClient::StorageType::kCacheStorage,
        std::move(wrapped_callback));
  }
}

void CacheStorage::OnCacheStorageAllowed(base::OnceCallback<void()> callback,
                                         ScriptPromiseResolverBase* resolver,
                                         bool allow_access) {
  if (!resolver->GetScriptState()->ContextIsValid()) {
    return;
  }
  if (allowed_.has_value()) {
    DCHECK_EQ(allowed_.value(), allow_access);
  } else {
    allowed_ = allow_access;
  }

  if (allowed_.value()) {
    std::move(callback).Run();
    return;
  }

  resolver->RejectWithSecurityError(kSecurityErrorMessage,
                                    kSecurityErrorMessage);
}

ScriptPromise<Cache> CacheStorage::open(ScriptState* script_state,
                                        const String& cache_name,
                                        ExceptionState& exception_state) {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW1("CacheStorage", "CacheStorage::Open",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT,
                         "name", CacheStorageTracedValue(cache_name));

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<Cache>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsContextThread());

  IsCacheStorageAllowed(context, resolver,
                        resolver->WrapCallbackInScriptScope(BindOnce(
                            &CacheStorage::OpenImpl, WrapWeakPersistent(this),
                            cache_name, trace_id)));

  return promise;
}

void CacheStorage::OpenImpl(const String& cache_name,
                            int64_t trace_id,
                            ScriptPromiseResolver<Cache>* resolver) {
  MaybeInit();

  // The context may be destroyed and the mojo connection unbound. However the
  // object may live on, reject any requests after the context is destroyed.
  if (!cache_storage_remote_.is_bound()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError, "");
    return;
  }
  ever_used_ = true;
  // Make sure to bind the CacheStorage object to keep the mojo interface
  // pointer alive during the operation.  Otherwise GC might prevent the
  // callback from ever being executed.
  cache_storage_remote_->Open(
      cache_name, trace_id,
      resolver->WrapCallbackInScriptScope(blink::BindOnce(
          [](GlobalFetch::ScopedFetcher* fetcher,
             CacheStorageBlobClientList* blob_client_list,
             base::TimeTicks start_time, int64_t trace_id,
             ScriptPromiseResolver<Cache>* resolver,
             mojom::blink::CacheStorage::OpenResult result) {
            base::UmaHistogramTimes(
                "ServiceWorkerCache.CacheStorage.Renderer.Open",
                base::TimeTicks::Now() - start_time);

            auto complete = resolver->WrapCallbackInScriptScope(blink::BindOnce(
                &OpenComplete, WrapPersistent(fetcher),
                WrapPersistent(blob_client_list), trace_id, std::move(result)));
            ProcessCompletion(std::move(complete), start_time,
                              resolver->GetExecutionContext(), "Open");
          },
          WrapPersistent(scoped_fetcher_.Get()),
          WrapPersistent(blob_client_list_.Get()), base::TimeTicks::Now(),
          trace_id)));
}

ScriptPromise<IDLBoolean> CacheStorage::has(ScriptState* script_state,
                                            const String& cache_name,
                                            ExceptionState& exception_state) {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW1("CacheStorage", "CacheStorage::Has",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT,
                         "name", CacheStorageTracedValue(cache_name));

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsContextThread());

  IsCacheStorageAllowed(context, resolver,
                        resolver->WrapCallbackInScriptScope(BindOnce(
                            &CacheStorage::HasImpl, WrapWeakPersistent(this),
                            cache_name, trace_id)));

  return promise;
}

void CacheStorage::HasImpl(const String& cache_name,
                           int64_t trace_id,
                           ScriptPromiseResolver<IDLBoolean>* resolver) {
  MaybeInit();

  // The context may be destroyed and the mojo connection unbound. However the
  // object may live on, reject any requests after the context is destroyed.
  if (!cache_storage_remote_.is_bound()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError, "");
    return;
  }
  ever_used_ = true;

  // Make sure to bind the CacheStorage object to keep the mojo interface
  // pointer alive during the operation.  Otherwise GC might prevent the
  // callback from ever being executed.
  cache_storage_remote_->Has(
      cache_name, trace_id,
      resolver->WrapCallbackInScriptScope(blink::BindOnce(
          [](base::TimeTicks start_time, int64_t trace_id,
             ScriptPromiseResolver<IDLBoolean>* resolver,
             mojom::blink::CacheStorageError result) {
            base::UmaHistogramTimes(
                "ServiceWorkerCache.CacheStorage.Renderer.Has",
                base::TimeTicks::Now() - start_time);
            TRACE_EVENT_WITH_FLOW1(
                "CacheStorage", "CacheStorage::Has::Callback",
                TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN, "status",
                CacheStorageTracedValue(result));

            auto complete = resolver->WrapCallbackInScriptScope(
                BindOnce(&HasComplete, result));
            ProcessCompletion(std::move(complete), start_time,
                              resolver->GetExecutionContext(), "Has");
          },
          base::TimeTicks::Now(), trace_id)));
}

ScriptPromise<IDLBoolean> CacheStorage::Delete(
    ScriptState* script_state,
    const String& cache_name,
    ExceptionState& exception_state) {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW1("CacheStorage", "CacheStorage::Delete",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT,
                         "name", CacheStorageTracedValue(cache_name));

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsContextThread());

  IsCacheStorageAllowed(context, resolver,
                        resolver->WrapCallbackInScriptScope(BindOnce(
                            &CacheStorage::DeleteImpl, WrapWeakPersistent(this),
                            cache_name, trace_id)));

  return promise;
}

void CacheStorage::DeleteImpl(const String& cache_name,
                              int64_t trace_id,
                              ScriptPromiseResolver<IDLBoolean>* resolver) {
  MaybeInit();

  // The context may be destroyed and the mojo connection unbound. However the
  // object may live on, reject any requests after the context is destroyed.
  if (!cache_storage_remote_.is_bound()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError, "");
    return;
  }
  ever_used_ = true;

  // Make sure to bind the CacheStorage object to keep the mojo interface
  // pointer alive during the operation.  Otherwise GC might prevent the
  // callback from ever being executed.
  cache_storage_remote_->Delete(
      cache_name, trace_id,
      resolver->WrapCallbackInScriptScope(blink::BindOnce(
          [](base::TimeTicks start_time, int64_t trace_id,
             ScriptPromiseResolver<IDLBoolean>* resolver,
             mojom::blink::CacheStorageError result) {
            base::UmaHistogramTimes(
                "ServiceWorkerCache.CacheStorage.Renderer.Delete",
                base::TimeTicks::Now() - start_time);
            TRACE_EVENT_WITH_FLOW1(
                "CacheStorage", "CacheStorage::Delete::Callback",
                TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN, "status",
                CacheStorageTracedValue(result));

            auto complete = resolver->WrapCallbackInScriptScope(
                BindOnce(&DeleteComplete, result));
            ProcessCompletion(std::move(complete), start_time,
                              resolver->GetExecutionContext(), "Delete");
          },
          base::TimeTicks::Now(), trace_id)));
}

ScriptPromise<IDLSequence<IDLString>> CacheStorage::keys(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW0("CacheStorage", "CacheStorage::Keys",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<IDLString>>>(
          script_state, exception_state.GetContext());
  ScriptPromise<IDLSequence<IDLString>> promise = resolver->Promise();

  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsContextThread());

  IsCacheStorageAllowed(
      context, resolver,
      resolver->WrapCallbackInScriptScope(BindOnce(
          &CacheStorage::KeysImpl, WrapWeakPersistent(this), trace_id)));

  return promise;
}

void CacheStorage::KeysImpl(
    int64_t trace_id,
    ScriptPromiseResolver<IDLSequence<IDLString>>* resolver) {
  MaybeInit();

  // The context may be destroyed and the mojo connection unbound. However the
  // object may live on, reject any requests after the context is destroyed.
  if (!cache_storage_remote_.is_bound()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError, "");
    return;
  }
  ever_used_ = true;

  // Make sure to bind the CacheStorage object to keep the mojo interface
  // pointer alive during the operation.  Otherwise GC might prevent the
  // callback from ever being executed.
  cache_storage_remote_->Keys(
      trace_id,
      resolver->WrapCallbackInScriptScope(blink::BindOnce(
          [](base::TimeTicks start_time, int64_t trace_id,
             ScriptPromiseResolver<IDLSequence<IDLString>>* resolver,
             const Vector<String>& keys) {
            base::UmaHistogramTimes(
                "ServiceWorkerCache.CacheStorage.Renderer.Keys",
                base::TimeTicks::Now() - start_time);
            TRACE_EVENT_WITH_FLOW1(
                "CacheStorage", "CacheStorage::Keys::Callback",
                TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN, "key_list",
                CacheStorageTracedValue(keys));

            auto complete = resolver->WrapCallbackInScriptScope(BindOnce(
                [](const Vector<String>& keys,
                   ScriptPromiseResolver<IDLSequence<IDLString>>* resolver) {
                  resolver->Resolve(keys);
                },
                keys));
            ProcessCompletion(std::move(complete), start_time,
                              resolver->GetExecutionContext(), "Keys");
          },
          base::TimeTicks::Now(), trace_id)));
}

ScriptPromise<Response> CacheStorage::match(
    ScriptState* script_state,
    const V8RequestInfo* request,
    const MultiCacheQueryOptions* options,
    ExceptionState& exception_state) {
  DCHECK(request);
  Request* request_object = nullptr;
  switch (request->GetContentType()) {
    case V8RequestInfo::ContentType::kRequest:
      request_object = request->GetAsRequest();
      break;
    case V8RequestInfo::ContentType::kUSVString:
      request_object = Request::Create(script_state, request->GetAsUSVString(),
                                       exception_state);
      if (exception_state.HadException()) {
        return EmptyPromise();
      }
      break;
  }
  return MatchImpl(script_state, request_object, options, exception_state);
}

ScriptPromise<Response> CacheStorage::MatchImpl(
    ScriptState* script_state,
    const Request* request,
    const MultiCacheQueryOptions* options,
    ExceptionState& exception_state) {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  mojom::blink::FetchAPIRequestPtr mojo_request =
      request->CreateFetchAPIRequest();
  mojom::blink::MultiCacheQueryOptionsPtr mojo_options =
      mojom::blink::MultiCacheQueryOptions::From(options);

  ExecutionContext* context = ExecutionContext::From(script_state);
  bool in_related_fetch_event = false;
  bool in_range_fetch_event = false;
  if (auto* global_scope = DynamicTo<ServiceWorkerGlobalScope>(context)) {
    in_related_fetch_event = global_scope->HasRelatedFetchEvent(request->url());
    in_range_fetch_event = global_scope->HasRangeFetchEvent(request->url());
  }

  TRACE_EVENT_WITH_FLOW2("CacheStorage", "CacheStorage::MatchImpl",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT,
                         "request", CacheStorageTracedValue(mojo_request),
                         "options", CacheStorageTracedValue(mojo_options));

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<Response>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  if (request->method() != http_names::kGET && !options->ignoreMethod()) {
    resolver->Resolve();
    return promise;
  }

  IsCacheStorageAllowed(
      context, resolver,
      resolver->WrapCallbackInScriptScope(
          BindOnce(&CacheStorage::MatchImplHelper, WrapWeakPersistent(this),
                   WrapPersistent(options), std::move(mojo_request),
                   std::move(mojo_options), in_related_fetch_event,
                   in_range_fetch_event, trace_id)));

  return promise;
}

void CacheStorage::MatchImplHelper(
    const MultiCacheQueryOptions* options,
    mojom::blink::FetchAPIRequestPtr mojo_request,
    mojom::blink::MultiCacheQueryOptionsPtr mojo_options,
    bool in_related_fetch_event,
    bool in_range_fetch_event,
    int64_t trace_id,
    ScriptPromiseResolver<Response>* resolver) {
  MaybeInit();

  // The context may be destroyed and the mojo connection unbound. However the
  // object may live on, reject any requests after the context is destroyed.
  if (!cache_storage_remote_.is_bound()) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError, "");
    return;
  }
  ever_used_ = true;

  // Make sure to bind the CacheStorage object to keep the mojo interface
  // pointer alive during the operation.  Otherwise GC might prevent the
  // callback from ever being executed.
  cache_storage_remote_->Match(
      std::move(mojo_request), std::move(mojo_options), in_related_fetch_event,
      in_range_fetch_event, trace_id,
      resolver->WrapCallbackInScriptScope(blink::BindOnce(
          [](base::TimeTicks start_time, const MultiCacheQueryOptions* options,
             int64_t trace_id, CacheStorage* self,
             ScriptPromiseResolver<Response>* resolver,
             mojom::blink::CacheStorage::MatchResult result) {
            base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
            if (!options->hasCacheName() || options->cacheName().empty()) {
              base::UmaHistogramLongTimes(
                  "ServiceWorkerCache.CacheStorage.Renderer.MatchAllCaches",
                  elapsed);
            } else {
              base::UmaHistogramLongTimes(
                  "ServiceWorkerCache.CacheStorage.Renderer.MatchOneCache",
                  elapsed);
            }

            auto complete = resolver->WrapCallbackInScriptScope(
                blink::BindOnce(&MatchComplete, trace_id, std::move(result),
                                WrapPersistent(self->blob_client_list_.Get())));
            ProcessCompletion(std::move(complete), start_time,
                              resolver->GetExecutionContext(), "Match");
          },
          base::TimeTicks::Now(), WrapPersistent(options), trace_id,
          WrapPersistent(this))));
}

CacheStorage::CacheStorage(ExecutionContext* context,
                           GlobalFetch::ScopedFetcher* fetcher)
    : CacheStorage(context, fetcher, {}) {}

CacheStorage::CacheStorage(
    ExecutionContext* context,
    GlobalFetch::ScopedFetcher* fetcher,
    mojo::PendingRemote<mojom::blink::CacheStorage> pending_remote)
    : ActiveScriptWrappable<CacheStorage>({}),
      ExecutionContextClient(context),
      scoped_fetcher_(fetcher),
      blob_client_list_(MakeGarbageCollected<CacheStorageBlobClientList>()),
      cache_storage_remote_(context) {
  // See https://bit.ly/2S0zRAS for task types.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      context->GetTaskRunner(blink::TaskType::kMiscPlatformAPI);

  if (pending_remote) {
    cache_storage_remote_.Bind(std::move(pending_remote), task_runner);
  } else if (auto* service_worker =
                 DynamicTo<ServiceWorkerGlobalScope>(context)) {
    // Service workers may already have a CacheStoragePtr provided as an
    // optimization.
    mojo::PendingRemote<mojom::blink::CacheStorage> info =
        service_worker->TakeCacheStorage();
    if (info) {
      cache_storage_remote_.Bind(std::move(info), task_runner);
    }
  }

  // Otherwise wait for MaybeInit() to bind a new mojo connection.
}

CacheStorage::~CacheStorage() = default;

bool CacheStorage::HasPendingActivity() const {
  // Once the CacheStorage has been used once we keep it alive until the
  // context goes away.  This allows us to use the existence of this
  // context as a hint to optimizations such as keeping backend disk_caches
  // open in the browser process.
  //
  // Note, this also keeps the CacheStorage alive during active Cache and
  // CacheStorage operations.
  return ever_used_;
}

void CacheStorage::Trace(Visitor* visitor) const {
  visitor->Trace(scoped_fetcher_);
  visitor->Trace(blob_client_list_);
  visitor->Trace(cache_storage_remote_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

void CacheStorage::MaybeInit() {
  if (cache_storage_remote_.is_bound()) {
    return;
  }

  auto* context = GetExecutionContext();
  if (!context || context->IsContextDestroyed()) {
    return;
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      context->GetTaskRunner(blink::TaskType::kMiscPlatformAPI);

  context->GetBrowserInterfaceBroker().GetInterface(
      cache_storage_remote_.BindNewPipeAndPassReceiver(task_runner));
}

mojom::blink::CacheStorage* CacheStorage::GetRemoteForDevtools(
    base::OnceClosure disconnect_handler) {
  cache_storage_remote_.set_disconnect_handler(std::move(disconnect_handler));
  return cache_storage_remote_.get();
}

}  // namespace blink
