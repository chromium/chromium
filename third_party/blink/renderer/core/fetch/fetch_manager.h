// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_MANAGER_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/child_url_loader_factory_bundle.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class AbortSignal;
class ExceptionState;
class ExecutionContext;
class FetchRequestData;
class ScriptState;
class FetchLaterResult;

class CORE_EXPORT FetchManager final
    : public GarbageCollected<FetchManager>,
      public ExecutionContextLifecycleObserver {
 public:
  explicit FetchManager(ExecutionContext*);

  ScriptPromise Fetch(ScriptState*,
                      FetchRequestData*,
                      AbortSignal*,
                      ExceptionState&);

  // ExecutionContextLifecycleObserver overrides:
  void ContextDestroyed() override;

  void Trace(Visitor*) const override;

 private:
  class Loader;

  // Removes loader from `loaders_`.
  void OnLoaderFinished(Loader*);

  HeapHashSet<Member<Loader>> loaders_;
};

class CORE_EXPORT FetchLaterManager final
    : public GarbageCollected<FetchLaterManager>,
      public ExecutionContextLifecycleObserver {
 public:
  explicit FetchLaterManager(ExecutionContext*);

  FetchLaterResult* FetchLater(ScriptState*,
                               FetchRequestData*,
                               AbortSignal*,
                               absl::optional<DOMHighResTimeStamp>,
                               ExceptionState&);

  // ExecutionContextLifecycleObserver overrides:
  void ContextDestroyed() override;

  void Trace(Visitor*) const override;

  // For testing only:
  size_t NumLoadersForTesting() const;
  void RecreateTimerForTesting(scoped_refptr<base::SingleThreadTaskRunner>,
                               const base::TickClock*);

 private:
  class DeferredLoader;
  // TODO(crbug.com/1293679): Update to proper TaskType once spec is finalized.
  // Using the `TaskType::kNetworkingUnfreezable` as deferred requests need to
  // work when ExecutionContext is in BackForwardCache/frozen.
  static constexpr TaskType kTaskType = TaskType::kNetworkingUnfreezable;

  // Returns a pointer to the wrapper that provides FetchLaterLoaderFactory.
  // Returns nullptr if the context is detached.
  blink::ChildURLLoaderFactoryBundle* GetFactory();

  // Removes a loader from `deferred_loaders_`.
  void OnDeferredLoaderFinished(DeferredLoader*);

  // Every deferred loader represents a FetchLater request.
  HeapHashSet<Member<DeferredLoader>> deferred_loaders_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_MANAGER_H_
