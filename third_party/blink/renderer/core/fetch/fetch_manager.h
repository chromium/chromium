// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_MANAGER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
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
  FetchLaterResult* FetchLater(ScriptState*,
                               FetchRequestData*,
                               AbortSignal*,
                               ExceptionState&);
  void ContextDestroyed() override;

  void Trace(Visitor*) const override;

 private:
  class Loader;
  class DeferredLoader;

  // Removes loader from `loaders_`.
  void OnLoaderFinished(Loader*);
  // Removes loader from `deferred_loaders_`.
  void OnDeferredLoaderFinished(DeferredLoader*);

  HeapHashSet<Member<Loader>> loaders_;
  HeapHashSet<Member<DeferredLoader>> deferred_loaders_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_MANAGER_H_
