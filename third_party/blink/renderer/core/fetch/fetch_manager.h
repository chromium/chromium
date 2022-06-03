// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_MANAGER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class AbortSignal;
class ExceptionState;
class ExecutionContext;
class FetchRequestData;
class ScriptState;

class CORE_EXPORT FetchManager final
    : public GarbageCollected<FetchManager>,
      public ExecutionContextLifecycleObserver {
 public:
  explicit FetchManager(ExecutionContext*);

  ScriptPromise Fetch(ScriptState*,
                      FetchRequestData*,
                      AbortSignal*,
                      ExceptionState&);
  void ContextDestroyed() override;

  void Trace(Visitor*) const override;

 private:
  class Loader;

  // Removes loader from |m_loaders|.
  void OnLoaderFinished(Loader*);

  HeapHashSet<Member<Loader>> loaders_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_MANAGER_H_
