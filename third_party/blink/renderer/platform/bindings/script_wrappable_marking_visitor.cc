// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/script_wrappable_marking_visitor.h"

#include "base/auto_reset.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/active_script_wrappable_base.h"
#include "third_party/blink/renderer/platform/bindings/custom_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_map.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/scoped_persistent.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable_visitor_verifier.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/heap/heap_compact.h"
#include "third_party/blink/renderer/platform/heap/heap_page.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

ScriptWrappableMarkingVisitor::~ScriptWrappableMarkingVisitor() = default;

void ScriptWrappableMarkingVisitor::TracePrologue() {
  // This CHECK ensures that wrapper tracing is not started from scopes
  // that forbid GC execution, e.g., constructors.
  CHECK(ThreadState::Current());
  CHECK(!ThreadState::Current()->IsWrapperTracingForbidden());
  PerformCleanup();

  CHECK(!tracing_in_progress_);
  CHECK(!should_cleanup_);
  CHECK(headers_to_unmark_.IsEmpty());
  CHECK(marking_deque_.IsEmpty());
  CHECK(verifier_deque_.IsEmpty());
  tracing_in_progress_ = true;
  ThreadState::Current()->EnableWrapperTracingBarrier();
}

void ScriptWrappableMarkingVisitor::EnterFinalPause(EmbedderStackState) {
  CHECK(ThreadState::Current());
  CHECK(!ThreadState::Current()->IsWrapperTracingForbidden());
  ActiveScriptWrappableBase::TraceActiveScriptWrappables(isolate(), this);
}

void ScriptWrappableMarkingVisitor::TraceEpilogue() {
  CHECK(ThreadState::Current());
  CHECK(!ThreadState::Current()->IsWrapperTracingForbidden());
  DCHECK(marking_deque_.IsEmpty());
#if DCHECK_IS_ON()
  ScriptWrappableVisitorVerifier verifier(ThreadState::Current());
  for (auto& marking_data : verifier_deque_) {
    // Check that all children of this object are marked.
    marking_data.Trace(&verifier);
  }
#endif

  should_cleanup_ = true;
  tracing_in_progress_ = false;
  ThreadState::Current()->DisableWrapperTracingBarrier();
  ScheduleIdleLazyCleanup();
}

void ScriptWrappableMarkingVisitor::AbortTracingForTermination() {
  CHECK(ThreadState::Current());
  should_cleanup_ = true;
  tracing_in_progress_ = false;
  ThreadState::Current()->DisableWrapperTracingBarrier();
  PerformCleanup();
}

bool ScriptWrappableMarkingVisitor::IsTracingDone() {
  return marking_deque_.empty();
}

void ScriptWrappableMarkingVisitor::PerformCleanup() {
  if (!should_cleanup_)
    return;

  CHECK(!tracing_in_progress_);
  for (auto* header : headers_to_unmark_) {
    // Dead objects residing in the marking deque may become invalid due to
    // minor garbage collections and are therefore set to nullptr. We have
    // to skip over such objects.
    if (header)
      header->UnmarkWrapperHeader();
  }

  headers_to_unmark_.clear();
  marking_deque_.clear();
  verifier_deque_.clear();
  should_cleanup_ = false;
}

void ScriptWrappableMarkingVisitor::ScheduleIdleLazyCleanup() {
  Thread* const thread = Platform::Current()->CurrentThread();
  // Thread might already be gone, or some threads (e.g. PPAPI) don't have a
  // scheduler.
  if (!thread || !thread->Scheduler())
    return;

  if (idle_cleanup_task_scheduled_)
    return;

  Platform::Current()->CurrentThread()->Scheduler()->PostIdleTask(
      FROM_HERE, WTF::Bind(&ScriptWrappableMarkingVisitor::PerformLazyCleanup,
                           WTF::Unretained(this)));
  idle_cleanup_task_scheduled_ = true;
}

void ScriptWrappableMarkingVisitor::PerformLazyCleanup(TimeTicks deadline) {
  idle_cleanup_task_scheduled_ = false;

  if (!should_cleanup_)
    return;

  TRACE_EVENT1("blink_gc,devtools.timeline",
               "ScriptWrappableMarkingVisitor::performLazyCleanup",
               "idleDeltaInSeconds",
               (deadline - CurrentTimeTicks()).InSecondsF());

  const int kDeadlineCheckInterval = 2500;
  int processed_wrapper_count = 0;
  for (auto it = headers_to_unmark_.rbegin();
       it != headers_to_unmark_.rend();) {
    auto* header = *it;
    // Dead objects residing in the marking deque may become invalid due to
    // minor garbage collections and are therefore set to nullptr. We have
    // to skip over such objects.
    if (header)
      header->UnmarkWrapperHeader();

    ++it;
    headers_to_unmark_.pop_back();

    processed_wrapper_count++;
    if (processed_wrapper_count % kDeadlineCheckInterval == 0) {
      if (deadline <= CurrentTimeTicks()) {
        ScheduleIdleLazyCleanup();
        return;
      }
    }
  }

  // Unmarked all headers.
  CHECK(headers_to_unmark_.IsEmpty());
  marking_deque_.clear();
  verifier_deque_.clear();
  should_cleanup_ = false;
}

void ScriptWrappableMarkingVisitor::RegisterV8Reference(
    const std::pair<void*, void*>& internal_fields) {
  DCHECK(tracing_in_progress_);

  WrapperTypeInfo* wrapper_type_info =
      reinterpret_cast<WrapperTypeInfo*>(internal_fields.first);
  if (wrapper_type_info->gin_embedder != gin::GinEmbedder::kEmbedderBlink) {
    return;
  }

  wrapper_type_info->TraceWithWrappers(this, internal_fields.second);
}

void ScriptWrappableMarkingVisitor::RegisterV8References(
    const std::vector<std::pair<void*, void*>>&
        internal_fields_of_potential_wrappers) {
  CHECK(ThreadState::Current());
  for (auto& pair : internal_fields_of_potential_wrappers) {
    RegisterV8Reference(pair);
  }
}

bool ScriptWrappableMarkingVisitor::AdvanceTracing(double deadline_in_ms) {
  constexpr int kObjectsBeforeInterrupt = 100;
  // Do not drain the marking deque in a state where we can generally not
  // perform a GC. This makes sure that TraceTraits and friends find
  // themselves in a well-defined environment, e.g., properly set up vtables.
  CHECK(ThreadState::Current());
  CHECK(!ThreadState::Current()->IsWrapperTracingForbidden());
  CHECK(tracing_in_progress_);
  TimeTicks deadline =
      TimeTicks() + TimeDelta::FromMillisecondsD(deadline_in_ms);
  while (deadline.is_max() || WTF::CurrentTimeTicks() < deadline) {
    for (int objects = 0; objects++ < kObjectsBeforeInterrupt;) {
      if (marking_deque_.IsEmpty()) {
        return true;
      }
      marking_deque_.TakeFirst().Trace(this);
    }
  }
  return false;
}

void ScriptWrappableMarkingVisitor::MarkWrapperHeader(
    HeapObjectHeader* header) {
  DCHECK(!header->IsWrapperHeaderMarked());
  // Verify that no compactable & movable objects are slated for
  // lazy unmarking.
  DCHECK(!HeapCompact::IsCompactableArena(
      PageFromObject(header)->Arena()->ArenaIndex()));
  header->MarkWrapperHeader();
  headers_to_unmark_.push_back(header);
}

void ScriptWrappableMarkingVisitor::WriteBarrier(
    v8::Isolate* isolate,
    const TraceWrapperV8Reference<v8::Value>& dst_object) {
  if (dst_object.IsEmpty() || !ThreadState::IsAnyWrapperTracing())
    return;
  ScriptWrappableMarkingVisitor* visitor = CurrentVisitor(isolate);
  if (!visitor->WrapperTracingInProgress())
    return;

  // Conservatively assume that the source object containing |dst_object| is
  // marked.
  visitor->Trace(dst_object);
}

void ScriptWrappableMarkingVisitor::WriteBarrier(
    v8::Isolate* isolate,
    DOMWrapperMap<ScriptWrappable>* wrapper_map,
    ScriptWrappable* key) {
  if (!ThreadState::IsAnyWrapperTracing())
    return;
  ScriptWrappableMarkingVisitor* visitor = CurrentVisitor(isolate);
  if (!visitor->WrapperTracingInProgress())
    return;

  // Conservatively assume that the source object key is marked.
  visitor->Trace(wrapper_map, key);
}

void ScriptWrappableMarkingVisitor::WriteBarrier(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type_info,
    void* object) {
  if (!ThreadState::IsAnyWrapperTracing())
    return;
  ScriptWrappableMarkingVisitor* visitor = CurrentVisitor(isolate);
  if (!visitor->WrapperTracingInProgress())
    return;

  wrapper_type_info->TraceWithWrappers(visitor, object);
}

void ScriptWrappableMarkingVisitor::Visit(
    const TraceWrapperV8Reference<v8::Value>& traced_wrapper) {
  // The write barrier may try to mark a wrapper because cleanup is still
  // delayed. Bail out in this case. We also allow unconditional marking which
  // requires us to bail out here when tracing is not in progress.
  if (!tracing_in_progress_ || traced_wrapper.Get().IsEmpty())
    return;
  traced_wrapper.Get().RegisterExternalReference(isolate());
}

void ScriptWrappableMarkingVisitor::VisitWithWrappers(
    void* object,
    TraceDescriptor descriptor) {
  HeapObjectHeader* header =
      HeapObjectHeader::FromPayload(descriptor.base_object_payload);
  if (header->IsWrapperHeaderMarked())
    return;
  MarkWrapperHeader(header);
  DCHECK(tracing_in_progress_);
  DCHECK(header->IsWrapperHeaderMarked());
  marking_deque_.push_back(MarkingDequeItem(descriptor));
#if DCHECK_IS_ON()
  verifier_deque_.push_back(MarkingDequeItem(descriptor));
#endif
}

void ScriptWrappableMarkingVisitor::VisitBackingStoreStrongly(
    void* object,
    void** object_slot,
    TraceDescriptor desc) {
  if (!object)
    return;
  desc.callback(this, desc.base_object_payload);
}

void ScriptWrappableMarkingVisitor::Visit(
    DOMWrapperMap<ScriptWrappable>* wrapper_map,
    const ScriptWrappable* key) {
  wrapper_map->MarkWrapper(const_cast<ScriptWrappable*>(key));
}

void ScriptWrappableMarkingVisitor::InvalidateDeadObjectsInMarkingDeque() {
  for (auto it = marking_deque_.begin(); it != marking_deque_.end(); ++it) {
    auto& marking_data = *it;
    if (marking_data.ShouldBeInvalidated()) {
      marking_data.Invalidate();
    }
  }
  for (auto it = verifier_deque_.begin(); it != verifier_deque_.end(); ++it) {
    auto& marking_data = *it;
    if (marking_data.ShouldBeInvalidated()) {
      marking_data.Invalidate();
    }
  }
  for (auto** it = headers_to_unmark_.begin(); it != headers_to_unmark_.end();
       ++it) {
    auto* header = *it;
    if (header && !header->IsMarked()) {
      *it = nullptr;
    }
  }
}

void ScriptWrappableMarkingVisitor::InvalidateDeadObjectsInMarkingDeque(
    v8::Isolate* isolate) {
  ScriptWrappableMarkingVisitor* script_wrappable_visitor =
      V8PerIsolateData::From(isolate)->GetScriptWrappableMarkingVisitor();
  if (script_wrappable_visitor)
    script_wrappable_visitor->InvalidateDeadObjectsInMarkingDeque();
}

void ScriptWrappableMarkingVisitor::PerformCleanup(v8::Isolate* isolate) {
  ScriptWrappableMarkingVisitor* script_wrappable_visitor =
      V8PerIsolateData::From(isolate)->GetScriptWrappableMarkingVisitor();
  if (script_wrappable_visitor)
    script_wrappable_visitor->PerformCleanup();
}

ScriptWrappableMarkingVisitor* ScriptWrappableMarkingVisitor::CurrentVisitor(
    v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->GetScriptWrappableMarkingVisitor();
}

bool ScriptWrappableMarkingVisitor::MarkingDequeContains(void* needle) {
  for (auto item : marking_deque_) {
    if (item.RawObjectPointer() == needle)
      return true;
  }
  return false;
}

}  // namespace blink
