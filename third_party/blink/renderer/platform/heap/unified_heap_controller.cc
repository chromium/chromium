// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/unified_heap_controller.h"

#include "third_party/blink/renderer/platform/bindings/active_script_wrappable_base.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"
#include "third_party/blink/renderer/platform/heap/marking_visitor.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

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
    : thread_state_(thread_state) {}

void UnifiedHeapController::TracePrologue() {
  VLOG(2) << "UnifiedHeapController::TracePrologue";
  ThreadHeapStatsCollector::BlinkGCInV8Scope nested_scope(
      thread_state_->Heap().stats_collector());

  // Be conservative here as a new garbage collection gets started right away.
  thread_state_->FinishIncrementalMarkingIfRunning(
      BlinkGC::kHeapPointersOnStack, BlinkGC::kIncrementalMarking,
      BlinkGC::kLazySweeping, thread_state_->current_gc_data_.reason);

  // Reset any previously scheduled garbage collections.
  thread_state_->SetGCState(ThreadState::kNoGCScheduled);
  // Start incremental marking with unified tracing.
  thread_state_->IncrementalMarkingStart(BlinkGC::GCReason::kUnifiedHeapGC);

  is_tracing_done_ = false;
}

void UnifiedHeapController::EnterFinalPause(EmbedderStackState stack_state) {
  VLOG(2) << "UnifiedHeapController::EnterFinalPause";
  ThreadHeapStatsCollector::BlinkGCInV8Scope nested_scope(
      thread_state_->Heap().stats_collector());
  ThreadHeapStatsCollector::Scope stats_scope(
      thread_state_->Heap().stats_collector(),
      ThreadHeapStatsCollector::kAtomicPhase);

  // ActiveScriptWrappable may not have persistents keeping them alive but rely
  // on explicit tracing to be kept alive.
  // TODO(mlippautz): Move to root scanning after stabilizing unified garbage
  // collection.
  ActiveScriptWrappableBase::TraceActiveScriptWrappables(
      isolate_, thread_state_->CurrentVisitor());

  thread_state_->EnterAtomicPause();
  thread_state_->EnterGCForbiddenScope();
  thread_state_->AtomicPauseMarkPrologue(ToBlinkGCStackState(stack_state),
                                         BlinkGC::kIncrementalMarking,
                                         BlinkGC::GCReason::kUnifiedHeapGC);
}

void UnifiedHeapController::TraceEpilogue() {
  VLOG(2) << "UnifiedHeapController::TraceEpilogue";

  {
    ThreadHeapStatsCollector::BlinkGCInV8Scope nested_scope(
        thread_state_->Heap().stats_collector());
    ThreadHeapStatsCollector::Scope stats_scope(
        thread_state_->Heap().stats_collector(),
        ThreadHeapStatsCollector::kAtomicPhase);
    thread_state_->AtomicPauseMarkEpilogue(BlinkGC::kIncrementalMarking);
    thread_state_->LeaveAtomicPause();
    thread_state_->LeaveGCForbiddenScope();
    thread_state_->AtomicPauseSweepAndCompact(BlinkGC::kIncrementalMarking,
                                              BlinkGC::kLazySweeping);
  }

  if (!thread_state_->IsSweepingInProgress()) {
    // Sweeping was finished during the atomic pause. Update statistics needs to
    // run outside of the top-most stats scope.
    thread_state_->UpdateStatisticsAfterSweeping();
  }
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

  if (!thread_state_->in_atomic_pause()) {
    // V8 calls into embedder tracing from its own marking to ensure
    // progress. Oilpan will additionally schedule marking steps.
    ThreadState::AtomicPauseScope atomic_pause_scope(thread_state_);
    TimeTicks deadline =
        TimeTicks() + TimeDelta::FromMillisecondsD(deadline_in_ms);
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

}  // namespace blink
