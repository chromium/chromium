// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/unified_heap_controller.h"

#include "base/macros.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"
#include "third_party/blink/renderer/platform/heap/marking_visitor.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

constexpr BlinkGC::StackState ToBlinkGCStackState(
    v8::EmbedderHeapTracer::EmbedderStackState stack_state) {
  return stack_state == v8::EmbedderHeapTracer::EmbedderStackState::kEmpty
             ? BlinkGC::kNoHeapPointersOnStack
             : BlinkGC::kHeapPointersOnStack;
}

}  // namespace

UnifiedHeapController::UnifiedHeapController(ThreadState* thread_state)
    : thread_state_(thread_state) {
  thread_state->Heap().stats_collector()->RegisterObserver(this);
}

UnifiedHeapController::~UnifiedHeapController() {
  thread_state_->Heap().stats_collector()->UnregisterObserver(this);
}

void UnifiedHeapController::TracePrologue(
    v8::EmbedderHeapTracer::TraceFlags v8_flags) {
  VLOG(2) << "UnifiedHeapController::TracePrologue";
  ThreadHeapStatsCollector::BlinkGCInV8Scope nested_scope(
      thread_state_->Heap().stats_collector());

  // Be conservative here as a new garbage collection gets started right away.
  thread_state_->FinishIncrementalMarkingIfRunning(
      BlinkGC::kHeapPointersOnStack, BlinkGC::kIncrementalAndConcurrentMarking,
      BlinkGC::kConcurrentAndLazySweeping,
      thread_state_->current_gc_data_.reason);

  thread_state_->SetGCState(ThreadState::kNoGCScheduled);
  BlinkGC::GCReason gc_reason =
      (v8_flags & v8::EmbedderHeapTracer::TraceFlags::kReduceMemory)
          ? BlinkGC::GCReason::kUnifiedHeapForMemoryReductionGC
          : BlinkGC::GCReason::kUnifiedHeapGC;
  thread_state_->StartIncrementalMarking(gc_reason);

  is_tracing_done_ = false;
}

void UnifiedHeapController::EnterFinalPause(EmbedderStackState stack_state) {
  VLOG(2) << "UnifiedHeapController::EnterFinalPause";
  ThreadHeapStatsCollector::BlinkGCInV8Scope nested_scope(
      thread_state_->Heap().stats_collector());
  thread_state_->AtomicPauseMarkPrologue(
      ToBlinkGCStackState(stack_state),
      BlinkGC::kIncrementalAndConcurrentMarking,
      thread_state_->current_gc_data_.reason);
  thread_state_->AtomicPauseMarkRoots(ToBlinkGCStackState(stack_state),
                                      BlinkGC::kIncrementalAndConcurrentMarking,
                                      thread_state_->current_gc_data_.reason);
}

void UnifiedHeapController::TraceEpilogue(
    v8::EmbedderHeapTracer::TraceSummary* summary) {
  VLOG(2) << "UnifiedHeapController::TraceEpilogue";
  {
    ThreadHeapStatsCollector::BlinkGCInV8Scope nested_scope(
        thread_state_->Heap().stats_collector());
    thread_state_->AtomicPauseMarkEpilogue(
        BlinkGC::kIncrementalAndConcurrentMarking);
    thread_state_->AtomicPauseSweepAndCompact(
        BlinkGC::kIncrementalAndConcurrentMarking,
        BlinkGC::kConcurrentAndLazySweeping);

    ThreadHeapStatsCollector* const stats_collector =
        thread_state_->Heap().stats_collector();
    summary->allocated_size =
        static_cast<size_t>(stats_collector->marked_bytes());
    summary->time = stats_collector->marking_time_so_far().InMillisecondsF();
    buffered_allocated_size_ = 0;
  }
  thread_state_->AtomicPauseEpilogue();
}

void UnifiedHeapController::RegisterV8References(
    const std::vector<std::pair<void*, void*>>&
        internal_fields_of_potential_wrappers) {
  VLOG(2) << "UnifiedHeapController::RegisterV8References";
  DCHECK(thread_state()->IsMarkingInProgress());

  const bool was_in_atomic_pause = thread_state()->in_atomic_pause();
  if (!was_in_atomic_pause)
    ThreadState::Current()->EnterAtomicPause();
  for (auto& internal_fields : internal_fields_of_potential_wrappers) {
    WrapperTypeInfo* wrapper_type_info =
        reinterpret_cast<WrapperTypeInfo*>(internal_fields.first);
    if (wrapper_type_info->gin_embedder != gin::GinEmbedder::kEmbedderBlink) {
      continue;
    }
    is_tracing_done_ = false;
    wrapper_type_info->Trace(thread_state_->CurrentVisitor(),
                             internal_fields.second);
  }
  if (!was_in_atomic_pause)
    ThreadState::Current()->LeaveAtomicPause();
}

bool UnifiedHeapController::AdvanceTracing(double deadline_in_ms) {
  VLOG(2) << "UnifiedHeapController::AdvanceTracing";
  ThreadHeapStatsCollector::BlinkGCInV8Scope nested_scope(
      thread_state_->Heap().stats_collector());
  if (!thread_state_->in_atomic_pause()) {
    ThreadHeapStatsCollector::Scope advance_tracing_scope(
        thread_state_->Heap().stats_collector(),
        ThreadHeapStatsCollector::kUnifiedMarkingStep);
    // V8 calls into embedder tracing from its own marking to ensure
    // progress. Oilpan will additionally schedule marking steps.
    ThreadState::AtomicPauseScope atomic_pause_scope(thread_state_);
    ScriptForbiddenScope script_forbidden_scope;
    base::TimeTicks deadline =
        base::TimeTicks() + base::TimeDelta::FromMillisecondsD(deadline_in_ms);
    is_tracing_done_ = thread_state_->MarkPhaseAdvanceMarking(deadline);
    return is_tracing_done_;
  }
  thread_state_->AtomicPauseMarkTransitiveClosure();
  is_tracing_done_ = true;
  return true;
}

bool UnifiedHeapController::IsTracingDone() {
  return is_tracing_done_;
}

bool UnifiedHeapController::IsRootForNonTracingGC(
    const v8::TracedReference<v8::Value>& handle) {
  if (thread_state()->IsIncrementalMarking()) {
    // We have a non-tracing GC while unified GC is in progress. Treat all
    // objects as roots to avoid stale pointers in the marking worklists.
    return true;
  }
  const uint16_t class_id = handle.WrapperClassId();
  // Stand-alone reference or kCustomWrappableId. Keep as root as
  // we don't know better.
  if (class_id != WrapperTypeInfo::kNodeClassId &&
      class_id != WrapperTypeInfo::kObjectClassId)
    return true;

  const v8::TracedReference<v8::Object>& traced =
      handle.template As<v8::Object>();
  if (ToWrapperTypeInfo(traced)->IsActiveScriptWrappable() &&
      ToScriptWrappable(traced)->HasPendingActivity()) {
    return true;
  }

  if (ToScriptWrappable(traced)->HasEventListeners()) {
    return true;
  }

  return false;
}

void UnifiedHeapController::ResetHandleInNonTracingGC(
    const v8::TracedReference<v8::Value>& handle) {
  const uint16_t class_id = handle.WrapperClassId();
  // Only consider handles that have not been treated as roots, see
  // IsRootForNonTracingGCInternal.
  if (class_id != WrapperTypeInfo::kNodeClassId &&
      class_id != WrapperTypeInfo::kObjectClassId)
    return;

  // We should not reset any handles during an already running tracing
  // collection. Resetting a handle could re-allocate a backing or trigger
  // potential in place rehashing. Both operations may trigger write barriers by
  // moving references. Such references may already be dead but not yet cleared
  // which would result in reporting dead objects to V8.
  DCHECK(!thread_state()->IsIncrementalMarking());
  // Clearing the wrapper below adjusts the DOM wrapper store which may
  // re-allocate its backing. We have to avoid report memory to V8 as that may
  // trigger GC during GC.
  ThreadState::GCForbiddenScope gc_forbidden(thread_state());
  const v8::TracedReference<v8::Object>& traced = handle.As<v8::Object>();
  bool success = DOMWrapperWorld::UnsetSpecificWrapperIfSet(
      ToScriptWrappable(traced), traced);
  // Since V8 found a handle, Blink needs to find it as well when trying to
  // remove it.
  CHECK(success);
}

bool UnifiedHeapController::IsRootForNonTracingGC(
    const v8::TracedGlobal<v8::Value>& handle) {
  CHECK(false) << "Blink does not use v8::TracedGlobal.";
  return false;
}

void UnifiedHeapController::ReportBufferedAllocatedSizeIfPossible() {
  // Avoid reporting to V8 in the following conditions as that may trigger GC
  // finalizations where not allowed.
  // - Recursive sweeping.
  // - GC forbidden scope.
  if ((thread_state()->IsSweepingInProgress() &&
       thread_state()->SweepForbidden()) ||
      thread_state()->IsGCForbidden()) {
    return;
  }

  if (buffered_allocated_size_ < 0) {
    DecreaseAllocatedSize(static_cast<size_t>(-buffered_allocated_size_));
  } else {
    IncreaseAllocatedSize(static_cast<size_t>(buffered_allocated_size_));
  }
  buffered_allocated_size_ = 0;
}

void UnifiedHeapController::IncreaseAllocatedObjectSize(size_t delta_bytes) {
  buffered_allocated_size_ += static_cast<int64_t>(delta_bytes);
  ReportBufferedAllocatedSizeIfPossible();
}

void UnifiedHeapController::DecreaseAllocatedObjectSize(size_t delta_bytes) {
  buffered_allocated_size_ -= static_cast<int64_t>(delta_bytes);
  ReportBufferedAllocatedSizeIfPossible();
}

}  // namespace blink
