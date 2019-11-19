// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/unified_heap_marking_visitor.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/blink_gc.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/heap/unified_heap_controller.h"

namespace blink {

UnifiedHeapMarkingVisitorBase::UnifiedHeapMarkingVisitorBase(
    ThreadState* thread_state,
    v8::Isolate* isolate,
    int task_id)
    : isolate_(isolate),
      controller_(thread_state->unified_heap_controller()),
      v8_references_worklist_(thread_state->Heap().GetV8ReferencesWorklist(),
                              task_id),
      task_id_(task_id) {
  DCHECK(controller_);
}

void UnifiedHeapMarkingVisitorBase::VisitImpl(
    const TraceWrapperV8Reference<v8::Value>& v8_reference) {
  if (v8_reference.Get().IsEmpty())
    return;
  DCHECK(isolate_);
  if (task_id_ != WorklistTaskId::MutatorThread) {
    // This is a temporary solution. Pushing directly from concurrent threads
    // to V8 marking worklist will currently result in data races. This
    // solution guarantees correctness until we implement a long-term solution
    // (i.e. allowing Oilpan concurrent threads concurrent-safe access to V8
    // marking worklist without data-races)
    v8_references_worklist_.Push(&v8_reference);
    return;
  }
  controller_->RegisterEmbedderReference(
      v8_reference.template Cast<v8::Data>().Get());
}

UnifiedHeapMarkingVisitor::UnifiedHeapMarkingVisitor(ThreadState* thread_state,
                                                     MarkingMode mode,
                                                     v8::Isolate* isolate)
    : MarkingVisitor(thread_state, mode),
      UnifiedHeapMarkingVisitorBase(thread_state,
                                    isolate,
                                    WorklistTaskId::MutatorThread) {}

// static
void UnifiedHeapMarkingVisitor::WriteBarrier(
    const TraceWrapperV8Reference<v8::Value>& object) {
  if (object.IsEmpty() || !ThreadState::IsAnyIncrementalMarking())
    return;

  ThreadState* thread_state = ThreadState::Current();
  if (!thread_state->IsIncrementalMarking())
    return;

  thread_state->CurrentVisitor()->Trace(object);
}

// static
void UnifiedHeapMarkingVisitor::WriteBarrier(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type_info,
    void* object) {
  // |object| here is either ScriptWrappable or CustomWrappable.

  if (!ThreadState::IsAnyIncrementalMarking())
    return;

  ThreadState* thread_state = ThreadState::Current();
  if (!thread_state->IsIncrementalMarking())
    return;

  wrapper_type_info->Trace(thread_state->CurrentVisitor(), object);
}

void UnifiedHeapMarkingVisitor::Visit(
    const TraceWrapperV8Reference<v8::Value>& v) {
  VisitImpl(v);
}

ConcurrentUnifiedHeapMarkingVisitor::ConcurrentUnifiedHeapMarkingVisitor(
    ThreadState* thread_state,
    MarkingMode mode,
    v8::Isolate* isolate,
    int task_id)
    : ConcurrentMarkingVisitor(thread_state, mode, task_id),
      UnifiedHeapMarkingVisitorBase(thread_state, isolate, task_id) {}

void ConcurrentUnifiedHeapMarkingVisitor::FlushWorklists() {
  ConcurrentMarkingVisitor::FlushWorklists();
  v8_references_worklist_.FlushToGlobal();
}

void ConcurrentUnifiedHeapMarkingVisitor::Visit(
    const TraceWrapperV8Reference<v8::Value>& v) {
  VisitImpl(v);
}

}  // namespace blink
