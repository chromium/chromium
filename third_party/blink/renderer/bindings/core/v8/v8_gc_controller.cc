/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"

#include <algorithm>

#include "third_party/blink/public/platform/blame_context.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/html/imports/html_imports_controller.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"
#include "third_party/blink/renderer/platform/heap/unified_heap_controller.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

Node* V8GCController::OpaqueRootForGC(v8::Isolate*, Node* node) {
  DCHECK(node);
  if (node->isConnected())
    return &node->GetDocument().MasterDocument();

  if (auto* attr = DynamicTo<Attr>(node)) {
    Node* owner_element = attr->ownerElement();
    if (!owner_element)
      return node;
    node = owner_element;
  }

  while (Node* parent = node->ParentOrShadowHostOrTemplateHostNode())
    node = parent;

  return node;
}

namespace {

bool IsDOMWrapperClassId(uint16_t class_id) {
  return class_id == WrapperTypeInfo::kNodeClassId ||
         class_id == WrapperTypeInfo::kObjectClassId ||
         class_id == WrapperTypeInfo::kCustomWrappableId;
}

size_t UsedHeapSize(v8::Isolate* isolate) {
  v8::HeapStatistics heap_statistics;
  isolate->GetHeapStatistics(&heap_statistics);
  return heap_statistics.used_heap_size();
}

bool IsNestedInV8GC(ThreadState* thread_state, v8::GCType type) {
  return thread_state && (type == v8::kGCTypeMarkSweepCompact ||
                          type == v8::kGCTypeIncrementalMarking);
}

}  // namespace

void V8GCController::GcPrologue(v8::Isolate* isolate,
                                v8::GCType type,
                                v8::GCCallbackFlags flags) {
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kGcPrologue);
  ThreadHeapStatsCollector::BlinkGCInV8Scope nested_scope(
      IsNestedInV8GC(ThreadState::Current(), type)
          ? ThreadState::Current()->Heap().stats_collector()
          : nullptr);
  ScriptForbiddenScope::Enter();

  // Attribute garbage collection to the all frames instead of a specific
  // frame.
  if (BlameContext* blame_context =
          Platform::Current()->GetTopLevelBlameContext())
    blame_context->Enter();

  // TODO(haraken): A GC callback is not allowed to re-enter V8. This means
  // that it's unsafe to run Oilpan's GC in the GC callback because it may
  // run finalizers that call into V8. To avoid the risk, we should post
  // a task to schedule the Oilpan's GC.
  // (In practice, there is no finalizer that calls into V8 and thus is safe.)

  v8::HandleScope scope(isolate);
  switch (type) {
    case v8::kGCTypeScavenge:
      TRACE_EVENT_BEGIN1("devtools.timeline,v8", "MinorGC",
                         "usedHeapSizeBefore", UsedHeapSize(isolate));
      if (ThreadState::Current())
        ThreadState::Current()->WillStartV8GC(BlinkGC::kV8MinorGC);
      break;
    case v8::kGCTypeMarkSweepCompact:
      if (ThreadState::Current())
        ThreadState::Current()->WillStartV8GC(BlinkGC::kV8MajorGC);

      TRACE_EVENT_BEGIN2("devtools.timeline,v8", "MajorGC",
                         "usedHeapSizeBefore", UsedHeapSize(isolate), "type",
                         "atomic pause");
      break;
    case v8::kGCTypeIncrementalMarking:
      if (ThreadState::Current())
        ThreadState::Current()->WillStartV8GC(BlinkGC::kV8MajorGC);

      TRACE_EVENT_BEGIN2("devtools.timeline,v8", "MajorGC",
                         "usedHeapSizeBefore", UsedHeapSize(isolate), "type",
                         "incremental marking");
      break;
    case v8::kGCTypeProcessWeakCallbacks:
      TRACE_EVENT_BEGIN2("devtools.timeline,v8", "MajorGC",
                         "usedHeapSizeBefore", UsedHeapSize(isolate), "type",
                         "weak processing");
      break;
    default:
      NOTREACHED();
  }
}

namespace {

void UpdateCollectedPhantomHandles(v8::Isolate* isolate) {
  ThreadHeapStatsCollector* stats_collector =
      ThreadState::Current()->Heap().stats_collector();
  const size_t count = isolate->NumberOfPhantomHandleResetsSinceLastCall();
  stats_collector->DecreaseWrapperCount(count);
  stats_collector->IncreaseCollectedWrapperCount(count);
}

}  // namespace

void V8GCController::GcEpilogue(v8::Isolate* isolate,
                                v8::GCType type,
                                v8::GCCallbackFlags flags) {
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kGcEpilogue);
  ThreadHeapStatsCollector::BlinkGCInV8Scope nested_scope(
      IsNestedInV8GC(ThreadState::Current(), type)
          ? ThreadState::Current()->Heap().stats_collector()
          : nullptr);
  UpdateCollectedPhantomHandles(isolate);
  switch (type) {
    case v8::kGCTypeScavenge:
      TRACE_EVENT_END1("devtools.timeline,v8", "MinorGC", "usedHeapSizeAfter",
                       UsedHeapSize(isolate));
      break;
    case v8::kGCTypeMarkSweepCompact:
      TRACE_EVENT_END1("devtools.timeline,v8", "MajorGC", "usedHeapSizeAfter",
                       UsedHeapSize(isolate));
      break;
    case v8::kGCTypeIncrementalMarking:
      TRACE_EVENT_END1("devtools.timeline,v8", "MajorGC", "usedHeapSizeAfter",
                       UsedHeapSize(isolate));
      break;
    case v8::kGCTypeProcessWeakCallbacks:
      TRACE_EVENT_END1("devtools.timeline,v8", "MajorGC", "usedHeapSizeAfter",
                       UsedHeapSize(isolate));
      break;
    default:
      NOTREACHED();
  }

  ScriptForbiddenScope::Exit();

  if (BlameContext* blame_context =
          Platform::Current()->GetTopLevelBlameContext())
    blame_context->Leave();

  ThreadState* current_thread_state = ThreadState::Current();
  if (current_thread_state && !current_thread_state->IsGCForbidden()) {
    if (flags & v8::kGCCallbackFlagForced) {
      // Forces a precise GC at the end of the current event loop.
      // This is required for testing code that cannot use GC internals but
      // rather has to rely on window.gc().
      current_thread_state->ScheduleForcedGCForTesting();
    }
  }

  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data",
                       inspector_update_counters_event::Data());
}

void V8GCController::CollectAllGarbageForTesting(
    v8::Isolate* isolate,
    v8::EmbedderHeapTracer::EmbedderStackState stack_state) {
  constexpr unsigned kNumberOfGCs = 5;

  if (stack_state != v8::EmbedderHeapTracer::EmbedderStackState::kUnknown) {
    v8::EmbedderHeapTracer* const tracer = static_cast<v8::EmbedderHeapTracer*>(
        ThreadState::Current()->unified_heap_controller());
    // Passing a stack state is only supported when either wrapper tracing or
    // unified heap is enabled.
    for (unsigned i = 0; i < kNumberOfGCs; i++)
      tracer->GarbageCollectionForTesting(stack_state);
    return;
  }

  for (unsigned i = 0; i < kNumberOfGCs; i++)
    isolate->RequestGarbageCollectionForTesting(
        v8::Isolate::kFullGarbageCollection);
}

namespace {

// Visitor forwarding all DOM wrapper handles to the provided Blink visitor.
class DOMWrapperForwardingVisitor final
    : public v8::PersistentHandleVisitor,
      public v8::EmbedderHeapTracer::TracedGlobalHandleVisitor {
 public:
  explicit DOMWrapperForwardingVisitor(Visitor* visitor) : visitor_(visitor) {
    DCHECK(visitor_);
  }

  void VisitPersistentHandle(v8::Persistent<v8::Value>* value,
                             uint16_t class_id) final {
    // TODO(mlippautz): There should be no more v8::Persistent that have a class
    // id set.
    VisitHandle(value, class_id);
  }

  void VisitTracedGlobalHandle(const v8::TracedGlobal<v8::Value>&) final {
    CHECK(false) << "Blink does not use v8::TracedGlobal.";
  }

  void VisitTracedReference(const v8::TracedReference<v8::Value>& value) final {
    VisitHandle(&value, value.WrapperClassId());
  }

 private:
  template <typename T>
  void VisitHandle(T* value, uint16_t class_id) {
    if (!IsDOMWrapperClassId(class_id))
      return;

    WrapperTypeInfo* wrapper_type_info = const_cast<WrapperTypeInfo*>(
        ToWrapperTypeInfo(value->template As<v8::Object>()));

    // WrapperTypeInfo pointer may have been cleared before termination GCs on
    // worker threads.
    if (!wrapper_type_info)
      return;

    wrapper_type_info->Trace(
        visitor_, ToUntypedWrappable(value->template As<v8::Object>()));
  }

  Visitor* const visitor_;
};

}  // namespace

// static
void V8GCController::TraceDOMWrappers(v8::Isolate* isolate, Visitor* visitor) {
  DCHECK(isolate);
  DOMWrapperForwardingVisitor forwarding_visitor(visitor);
  isolate->VisitHandlesWithClassIds(&forwarding_visitor);
  v8::EmbedderHeapTracer* const tracer = static_cast<v8::EmbedderHeapTracer*>(
      ThreadState::Current()->unified_heap_controller());
  tracer->IterateTracedGlobalHandles(&forwarding_visitor);
}

}  // namespace blink
