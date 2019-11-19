// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_OBSERVER_H_

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ExecutionContext;
class ExceptionState;
class Performance;
class PerformanceObserver;
class PerformanceObserverInit;
class V8PerformanceObserverCallback;

using PerformanceEntryVector = HeapVector<Member<PerformanceEntry>>;

class CORE_EXPORT PerformanceObserver final
    : public ScriptWrappable,
      public ActiveScriptWrappable<PerformanceObserver>,
      public ContextClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PerformanceObserver);
  friend class Performance;
  friend class PerformanceTest;
  friend class PerformanceObserverTest;

 public:
  static PerformanceObserver* Create(ScriptState*,
                                     V8PerformanceObserverCallback*);
  static Vector<AtomicString> supportedEntryTypes(ScriptState*);

  PerformanceObserver(ExecutionContext*,
                      Performance*,
                      V8PerformanceObserverCallback*);

  void observe(const PerformanceObserverInit*, ExceptionState&);
  void disconnect();
  PerformanceEntryVector takeRecords();
  void EnqueuePerformanceEntry(PerformanceEntry&);
  PerformanceEntryTypeMask FilterOptions() const { return filter_options_; }

  // ScriptWrappable
  bool HasPendingActivity() const final;

  void Trace(blink::Visitor*) override;

 private:
  // This describes the types of parameters that an observer can have in its
  // observe() function. An observer of type kEntryTypesObserver has already
  // made a call observe({entryTypes...}) so can only do subsequent observe()
  // calls with the 'entryTypes' parameter. An observer of type kTypeObserver
  // has already made a call observe({type...}) so it can only perform
  // subsequent observe() calls with the 'type' parameter. An observer of type
  // kUnknown has not called observe().
  enum class PerformanceObserverType {
    kEntryTypesObserver,
    kTypeObserver,
    kUnknown,
  };
  void Deliver();
  bool ShouldBeSuspended() const;

  Member<ExecutionContext> execution_context_;
  Member<V8PerformanceObserverCallback> callback_;
  WeakMember<Performance> performance_;
  PerformanceEntryVector performance_entries_;
  PerformanceEntryTypeMask filter_options_;
  PerformanceObserverType type_;
  bool is_registered_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_OBSERVER_H_
