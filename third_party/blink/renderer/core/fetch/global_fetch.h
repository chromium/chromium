// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_GLOBAL_FETCH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_GLOBAL_FETCH_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class ExecutionContext;
class ExceptionState;
class LocalDOMWindow;
class RequestInit;
class DeferredRequestInit;
class Response;
class ScriptState;
class WorkerGlobalScope;
class FetchLaterResult;
class FetchManager;
class FetchLaterManager;

class CORE_EXPORT GlobalFetch {
  STATIC_ONLY(GlobalFetch);

 public:
  class CORE_EXPORT ScopedFetcher : public GarbageCollected<ScopedFetcher>,
                                    public Supplement<ExecutionContext> {
   public:
    static const char kSupplementName[];

    explicit ScopedFetcher(ExecutionContext&);
    virtual ~ScopedFetcher() = default;

    virtual ScriptPromise<Response> Fetch(ScriptState*,
                                          const V8RequestInfo*,
                                          const RequestInit*,
                                          ExceptionState&);

    virtual FetchLaterResult* FetchLater(ScriptState*,
                                         const V8RequestInfo*,
                                         const DeferredRequestInit*,
                                         ExceptionState&);

    // Returns the number of fetch() method calls in the associated execution
    // context.  This is used for metrics.
    virtual uint32_t FetchCount() const { return fetch_count_; }

    // A wrapper to expose `FetchLaterManager::UpdateDeferredBytesQuota()`.
    // This method should only be called when `FetchLater()` is available.
    void UpdateDeferredBytesQuota(const KURL& url,
                                  uint64_t& quota_for_url_origin,
                                  uint64_t& total_quota) const;

    static ScopedFetcher* From(ExecutionContext&);

    void Trace(Visitor*) const override;

   private:
    Member<FetchManager> fetch_manager_;
    Member<FetchLaterManager> fetch_later_manager_;
    uint32_t fetch_count_ = 0;
  };

  static ScriptPromise<Response> fetch(ScriptState* script_state,
                                       LocalDOMWindow& window,
                                       const V8RequestInfo* input,
                                       const RequestInit* init,
                                       ExceptionState& exception_state);
  static ScriptPromise<Response> fetch(ScriptState* script_state,
                                       WorkerGlobalScope& worker,
                                       const V8RequestInfo* input,
                                       const RequestInit* init,
                                       ExceptionState& exception_state);

  static FetchLaterResult* fetchLater(ScriptState* script_state,
                                      LocalDOMWindow& window,
                                      const V8RequestInfo* input,
                                      const DeferredRequestInit* init,
                                      ExceptionState& exception_state);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_GLOBAL_FETCH_H_
