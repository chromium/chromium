// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/cache_storage.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage_error.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/network/http_names.h"

namespace blink {

CacheStorage* CacheStorage::Create(ExecutionContext* context,
                                   GlobalFetch::ScopedFetcher* fetcher) {
  return new CacheStorage(context, fetcher);
}

ScriptPromise CacheStorage::open(ScriptState* script_state,
                                 const String& cache_name) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);

  cache_storage_ptr_->Open(
      cache_name,
      WTF::Bind(
          [](ScriptPromiseResolver* resolver,
             GlobalFetch::ScopedFetcher* fetcher, TimeTicks start_time,
             mojom::blink::OpenResultPtr result) {
            if (!resolver->GetExecutionContext() ||
                resolver->GetExecutionContext()->IsContextDestroyed())
              return;
            if (result->is_status()) {
              switch (result->get_status()) {
                case mojom::blink::CacheStorageError::kErrorNotFound:
                case mojom::blink::CacheStorageError::kErrorStorage:
                  resolver->Resolve();
                  break;
                default:
                  resolver->Reject(
                      CacheStorageError::CreateException(result->get_status()));
                  break;
              }
            } else {
              UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Open",
                                  TimeTicks::Now() - start_time);
              resolver->Resolve(
                  Cache::Create(fetcher, std::move(result->get_cache())));
            }
          },
          WrapPersistent(resolver), WrapPersistent(scoped_fetcher_.Get()),
          TimeTicks::Now()));

  return resolver->Promise();
}

ScriptPromise CacheStorage::has(ScriptState* script_state,
                                const String& cache_name) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);

  cache_storage_ptr_->Has(
      cache_name,
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, TimeTicks start_time,
             mojom::blink::CacheStorageError result) {
            if (!resolver->GetExecutionContext() ||
                resolver->GetExecutionContext()->IsContextDestroyed())
              return;
            switch (result) {
              case mojom::blink::CacheStorageError::kSuccess:
                UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Has",
                                    TimeTicks::Now() - start_time);
                resolver->Resolve(true);
                break;
              case mojom::blink::CacheStorageError::kErrorNotFound:
                resolver->Resolve(false);
                break;
              default:
                resolver->Reject(CacheStorageError::CreateException(result));
                break;
            }
          },
          WrapPersistent(resolver), TimeTicks::Now()));

  return resolver->Promise();
}

ScriptPromise CacheStorage::Delete(ScriptState* script_state,
                                   const String& cache_name) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);

  cache_storage_ptr_->Delete(
      cache_name,
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, TimeTicks start_time,
             mojom::blink::CacheStorageError result) {
            if (!resolver->GetExecutionContext() ||
                resolver->GetExecutionContext()->IsContextDestroyed())
              return;
            switch (result) {
              case mojom::blink::CacheStorageError::kSuccess:
                UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Delete",
                                    TimeTicks::Now() - start_time);
                resolver->Resolve(true);
                break;
              case mojom::blink::CacheStorageError::kErrorStorage:
              case mojom::blink::CacheStorageError::kErrorNotFound:
                resolver->Resolve(false);
                break;
              default:
                resolver->Reject(CacheStorageError::CreateException(result));
                break;
            }
          },
          WrapPersistent(resolver), TimeTicks::Now()));

  return resolver->Promise();
}

ScriptPromise CacheStorage::keys(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);

  cache_storage_ptr_->Keys(WTF::Bind(
      [](ScriptPromiseResolver* resolver, TimeTicks start_time,
         const Vector<String>& keys) {
        if (!resolver->GetExecutionContext() ||
            resolver->GetExecutionContext()->IsContextDestroyed())
          return;
        UMA_HISTOGRAM_TIMES("ServiceWorkerCache.CacheStorage.Keys",
                            TimeTicks::Now() - start_time);
        resolver->Resolve(keys);
      },
      WrapPersistent(resolver), TimeTicks::Now()));

  return resolver->Promise();
}

ScriptPromise CacheStorage::match(ScriptState* script_state,
                                  const RequestInfo& request,
                                  const CacheQueryOptions& options,
                                  ExceptionState& exception_state) {
  DCHECK(!request.IsNull());

  if (request.IsRequest())
    return MatchImpl(script_state, request.GetAsRequest(), options);
  Request* new_request =
      Request::Create(script_state, request.GetAsUSVString(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise();
  return MatchImpl(script_state, new_request, options);
}

ScriptPromise CacheStorage::MatchImpl(ScriptState* script_state,
                                      const Request* request,
                                      const CacheQueryOptions& options) {
  WebServiceWorkerRequest web_request;
  request->PopulateWebServiceWorkerRequest(web_request);

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ScriptPromise promise = resolver->Promise();

  if (request->method() != HTTPNames::GET && !options.ignoreMethod()) {
    resolver->Resolve();
    return promise;
  }

  cache_storage_ptr_->Match(
      web_request, Cache::ToQueryParams(options),
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, TimeTicks start_time,
             const CacheQueryOptions& options,
             mojom::blink::MatchResultPtr result) {
            if (!resolver->GetExecutionContext() ||
                resolver->GetExecutionContext()->IsContextDestroyed())
              return;
            if (result->is_status()) {
              switch (result->get_status()) {
                case mojom::CacheStorageError::kErrorNotFound:
                case mojom::CacheStorageError::kErrorStorage:
                case mojom::CacheStorageError::kErrorCacheNameNotFound:
                  resolver->Resolve();
                  break;
                default:
                  resolver->Reject(
                      CacheStorageError::CreateException(result->get_status()));
                  break;
              }
            } else {
              TimeDelta elapsed = TimeTicks::Now() - start_time;
              UMA_HISTOGRAM_LONG_TIMES("ServiceWorkerCache.CacheStorage.Match2",
                                       elapsed);
              if (options.hasIgnoreSearch() && options.ignoreSearch()) {
                UMA_HISTOGRAM_LONG_TIMES(
                    "ServiceWorkerCache.CacheStorage.Match2."
                    "IgnoreSearchEnabled",
                    elapsed);
              } else {
                UMA_HISTOGRAM_LONG_TIMES(
                    "ServiceWorkerCache.CacheStorage.Match2."
                    "IgnoreSearchDisabled",
                    elapsed);
              }
              ScriptState::Scope scope(resolver->GetScriptState());
              resolver->Resolve(Response::Create(resolver->GetScriptState(),
                                                 *result->get_response()));
            }
          },
          WrapPersistent(resolver), TimeTicks::Now(), options));

  return promise;
}

CacheStorage::CacheStorage(ExecutionContext* context,
                           GlobalFetch::ScopedFetcher* fetcher)
    : scoped_fetcher_(fetcher) {
  // Service workers may already have a CacheStoragePtr provided as an
  // optimization.
  if (auto* service_worker = DynamicTo<ServiceWorkerGlobalScope>(context)) {
    mojom::blink::CacheStoragePtrInfo info = service_worker->TakeCacheStorage();
    if (info) {
      cache_storage_ptr_ = RevocableInterfacePtr<mojom::blink::CacheStorage>(
          std::move(info), context->GetInterfaceInvalidator());
      return;
    }
  }

  context->GetInterfaceProvider()->GetInterface(
      MakeRequest(&cache_storage_ptr_, context->GetInterfaceInvalidator()));
}

CacheStorage::~CacheStorage() = default;

void CacheStorage::Trace(blink::Visitor* visitor) {
  visitor->Trace(scoped_fetcher_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
