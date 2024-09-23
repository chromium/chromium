// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/global_fetch.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_deferred_request_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_request_init.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/core/fetch/fetch_later_result.h"
#include "third_party/blink/renderer/core/fetch/fetch_manager.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace {

void MeasureFetchProperties(ExecutionContext* execution_context,
                            FetchRequestData* data) {
  // 'redirect' measurement
  if (data->Redirect() == network::mojom::RedirectMode::kError)
    UseCounter::Count(execution_context, WebFeature::kFetchRedirectError);
  else if (data->Redirect() == network::mojom::RedirectMode::kManual)
    UseCounter::Count(execution_context, WebFeature::kFetchRedirectManual);

  // 'cache' measurement: https://crbug.com/959789
  if (data->CacheMode() == mojom::FetchCacheMode::kBypassCache)
    UseCounter::Count(execution_context, WebFeature::kFetchCacheReload);
}

template <typename T>
class GlobalFetchImpl final : public GarbageCollected<GlobalFetchImpl<T>>,
                              public GlobalFetch::ScopedFetcher,
                              public Supplement<T> {
 public:
  static const char kSupplementName[];

  static ScopedFetcher* From(T& supplementable,
                             ExecutionContext* execution_context) {
    GlobalFetchImpl* supplement =
        Supplement<T>::template From<GlobalFetchImpl>(supplementable);
    if (!supplement) {
      supplement = MakeGarbageCollected<GlobalFetchImpl>(supplementable,
                                                         execution_context);
      Supplement<T>::ProvideTo(supplementable, supplement);
    }
    return supplement;
  }

  explicit GlobalFetchImpl(T& supplementable,
                           ExecutionContext* execution_context)
      : Supplement<T>(supplementable),
        fetch_manager_(MakeGarbageCollected<FetchManager>(execution_context)),
        // TODO(crbug.com/1356128): FetchLater is only supported in Document.
        fetch_later_manager_(
            base::FeatureList::IsEnabled(blink::features::kFetchLaterAPI) &&
                    execution_context->IsWindow()
                ? MakeGarbageCollected<FetchLaterManager>(execution_context)
                : nullptr) {}

  ScriptPromise<Response> Fetch(ScriptState* script_state,
                                const V8RequestInfo* input,
                                const RequestInit* init,
                                ExceptionState& exception_state) override {
    fetch_count_ += 1;

    ExecutionContext* execution_context = fetch_manager_->GetExecutionContext();
    if (!script_state->ContextIsValid() || !execution_context) {
      // TODO(yhirano): Should this be moved to bindings?
      exception_state.ThrowTypeError("The global scope is shutting down.");
      return EmptyPromise();
    }

    // "Let |r| be the associated request of the result of invoking the
    // initial value of Request as constructor with |input| and |init| as
    // arguments. If this throws an exception, reject |p| with it."
    Request* r = Request::Create(script_state, input, init, exception_state);
    if (exception_state.HadException())
      return EmptyPromise();

    probe::WillSendXMLHttpOrFetchNetworkRequest(execution_context, r->url());
    FetchRequestData* request_data =
        r->PassRequestData(script_state, exception_state);
    MeasureFetchProperties(execution_context, request_data);

    // Even if this was checked at the beginning of the function, it might
    // have been set to nullptr during Request::Create.
    if (!fetch_manager_->GetExecutionContext()) {
      exception_state.ThrowTypeError("The global scope is shutting down.");
      return EmptyPromise();
    }

    auto promise = fetch_manager_->Fetch(script_state, request_data,
                                         r->signal(), exception_state);
    if (exception_state.HadException())
      return EmptyPromise();

    return promise;
  }

  FetchLaterResult* FetchLater(ScriptState* script_state,
                               const V8RequestInfo* input,
                               const DeferredRequestInit* init,
                               ExceptionState& exception_state) override {
    if (!base::FeatureList::IsEnabled(blink::features::kFetchLaterAPI) ||
        !fetch_later_manager_) {
      exception_state.ThrowTypeError(
          "FetchLater is not supported in this scope.");
      return nullptr;
    }
    ExecutionContext* ec = fetch_later_manager_->GetExecutionContext();
    if (!script_state->ContextIsValid() || !ec) {
      exception_state.ThrowTypeError("The global scope is shutting down.");
      return nullptr;
    }

    // https://whatpr.org/fetch/1647.html#dom-global-fetch-later
    // Run the fetchLater(input, init) method steps:

    // 1. Let `r` be the result of invoking the initial value of Request as
    // constructor with `input` and `init` as arguments. This may throw an
    // exception.
    Request* r =
        Request::Create(script_state, input,
                        static_cast<const RequestInit*>(init), exception_state);
    if (exception_state.HadException()) {
      return nullptr;
    }

    probe::WillSendXMLHttpOrFetchNetworkRequest(ec, r->url());
    FetchRequestData* request_data =
        r->PassRequestData(script_state, exception_state);
    MeasureFetchProperties(ec, request_data);
    // 5. If init is given and init ["activateAfter"] exists, then set
    // `activate_after` to init ["activateAfter"].
    std::optional<DOMHighResTimeStamp> activate_after =
        (init->hasActivateAfter() ? std::make_optional(init->activateAfter())
                                  : std::nullopt);
    auto* result = fetch_later_manager_->FetchLater(script_state, request_data,
                                                    r->signal(), activate_after,
                                                    exception_state);
    if (exception_state.HadException()) {
      return nullptr;
    }

    return result;
  }

  uint32_t FetchCount() const override { return fetch_count_; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(fetch_manager_);
    visitor->Trace(fetch_later_manager_);
    ScopedFetcher::Trace(visitor);
    Supplement<T>::Trace(visitor);
  }

 private:
  Member<FetchManager> fetch_manager_;
  Member<FetchLaterManager> fetch_later_manager_;
  uint32_t fetch_count_ = 0;
};

// static
template <typename T>
const char GlobalFetchImpl<T>::kSupplementName[] = "GlobalFetchImpl";

}  // namespace

GlobalFetch::ScopedFetcher::~ScopedFetcher() {}

FetchLaterResult* GlobalFetch::ScopedFetcher::FetchLater(
    ScriptState* script_state,
    const V8RequestInfo* input,
    const DeferredRequestInit* init,
    ExceptionState& exception_state) {
  NOTREACHED();
}

GlobalFetch::ScopedFetcher* GlobalFetch::ScopedFetcher::From(
    LocalDOMWindow& window) {
  return GlobalFetchImpl<LocalDOMWindow>::From(window,
                                               window.GetExecutionContext());
}

GlobalFetch::ScopedFetcher* GlobalFetch::ScopedFetcher::From(
    WorkerGlobalScope& worker) {
  return GlobalFetchImpl<WorkerGlobalScope>::From(worker,
                                                  worker.GetExecutionContext());
}

GlobalFetch::ScopedFetcher* GlobalFetch::ScopedFetcher::From(
    NavigatorBase& navigator) {
  return GlobalFetchImpl<NavigatorBase>::From(navigator,
                                              navigator.GetExecutionContext());
}

void GlobalFetch::ScopedFetcher::Trace(Visitor* visitor) const {}

ScriptPromise<Response> GlobalFetch::fetch(ScriptState* script_state,
                                           LocalDOMWindow& window,
                                           const V8RequestInfo* input,
                                           const RequestInit* init,
                                           ExceptionState& exception_state) {
  UseCounter::Count(window.GetExecutionContext(), WebFeature::kFetch);
  if (!window.GetFrame()) {
    exception_state.ThrowTypeError("The global scope is shutting down.");
    return EmptyPromise();
  }
  return ScopedFetcher::From(window)->Fetch(script_state, input, init,
                                            exception_state);
}

ScriptPromise<Response> GlobalFetch::fetch(ScriptState* script_state,
                                           WorkerGlobalScope& worker,
                                           const V8RequestInfo* input,
                                           const RequestInit* init,
                                           ExceptionState& exception_state) {
  UseCounter::Count(worker.GetExecutionContext(), WebFeature::kFetch);
  return ScopedFetcher::From(worker)->Fetch(script_state, input, init,
                                            exception_state);
}

FetchLaterResult* GlobalFetch::fetchLater(ScriptState* script_state,
                                          LocalDOMWindow& window,
                                          const V8RequestInfo* input,
                                          const DeferredRequestInit* init,
                                          ExceptionState& exception_state) {
  UseCounter::Count(window.GetExecutionContext(), WebFeature::kFetchLater);
  if (!window.GetFrame()) {
    exception_state.ThrowTypeError("The global scope is shutting down.");
    return nullptr;
  }
  return ScopedFetcher::From(window)->FetchLater(script_state, input, init,
                                                 exception_state);
}

}  // namespace blink
