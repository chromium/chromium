// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/cache_storage.h"

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
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

namespace {
const char kSecurityErrorMessage[] =
    "An attempt was made to break through the security policy of the user "
    "agent.";
}  // namespace

void CacheStorage::IsCacheStorageAllowed(ExecutionContext* context,
                                         ScriptPromiseResolverBase* resolver,
                                         base::OnceCallback<void()> callback) {
  DCHECK(context->IsWindow() || context->IsWorkerGlobalScope());

  auto wrapped_callback = WTF::BindOnce(
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
                        resolver->WrapCallbackInScriptScope(WTF::BindOnce(
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
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          [](GlobalFetch::ScopedFetcher* fetcher,
             CacheStorageBlobClientList* blob_client_list,
             base::TimeTicks start_time, int64_t trace_id,
             ScriptPromiseResolver<Cache>* resolver,
             mojom::blink::OpenResultPtr result) {
            base::UmaHistogramTimes(
                "ServiceWorkerCache.CacheStorage.Renderer.Open",
                base::TimeTicks::Now() - start_time);
            if (result->is_status()) {
              TRACE_EVENT_WITH_FLOW1(
                  "CacheStorage", "CacheStorage::Open::Callback",
                  TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN, "status",
                  CacheStorageTracedValue(result->get_status()));
              RejectCacheStorageWithError(resolver, result->get_status());
            } else {
              TRACE_EVENT_WITH_FLOW1(
                  "CacheStorage", "CacheStorage::Open::Callback",
                  TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN, "status",
                  "success");
              // See https://bit.ly/2S0zRAS for task types.
              resolver->Resolve(MakeGarbageCollected<Cache>(
                  fetcher, blob_client_list, std::move(result->get_cache()),
                  resolver->GetExecutionContext(),
                  blink::TaskType::kMiscPlatformAPI));
            }
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
                        resolver->WrapCallbackInScriptScope(WTF::BindOnce(
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
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
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
                        resolver->WrapCallbackInScriptScope(WTF::BindOnce(
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
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
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
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
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
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
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
            resolver->Resolve(keys);
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
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          &CacheStorage::MatchImplHelper, WrapWeakPersistent(this),
          WrapPersistent(options), std::move(mojo_request),
          std::move(mojo_options), in_related_fetch_event, in_range_fetch_event,
          trace_id)));

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
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          [](base::TimeTicks start_time, const MultiCacheQueryOptions* options,
             int64_t trace_id, CacheStorage* self,
             ScriptPromiseResolver<Response>* resolver,
             mojom::blink::MatchResultPtr result) {
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
            if (result->is_status()) {
              TRACE_EVENT_WITH_FLOW1(
                  "CacheStorage", "CacheStorage::MatchImpl::Callback",
                  TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN, "status",
                  CacheStorageTracedValue(result->get_status()));
              switch (result->get_status()) {
                case mojom::CacheStorageError::kErrorNotFound:
                case mojom::CacheStorageError::kErrorStorage:
                case mojom::CacheStorageError::kErrorCacheNameNotFound:
                  resolver->Resolve();
                  break;
                default:
                  RejectCacheStorageWithError(resolver, result->get_status());
                  break;
              }
            } else {
              ScriptState::Scope scope(resolver->GetScriptState());
              if (result->is_eager_response()) {
                TRACE_EVENT_WITH_FLOW1(
                    "CacheStorage", "CacheStorage::MatchImpl::Callback",
                    TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN,
                    "eager_response",
                    CacheStorageTracedValue(
                        result->get_eager_response()->response));
                resolver->Resolve(
                    CreateEagerResponse(resolver->GetScriptState(),
                                        std::move(result->get_eager_response()),
                                        self->blob_client_list_));
              } else {
                TRACE_EVENT_WITH_FLOW1(
                    "CacheStorage", "CacheStorage::MatchImpl::Callback",
                    TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN,
                    "response",
                    CacheStorageTracedValue(result->get_response()));
                resolver->Resolve(Response::Create(resolver->GetScriptState(),
                                                   *result->get_response()));
              }
            }
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
