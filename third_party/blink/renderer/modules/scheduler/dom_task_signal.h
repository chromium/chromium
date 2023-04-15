// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_SIGNAL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_SIGNAL_H_

#include "base/functional/callback_forward.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/abort_signal_composition_type.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {
class AbortSignalCompositionManager;
class ExceptionState;
class ExecutionContext;
class TaskSignalAnyInit;

class MODULES_EXPORT DOMTaskSignal final : public AbortSignal {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMTaskSignal* CreateFixedPriorityTaskSignal(
      ScriptState*,
      const AtomicString& priority);

  // Constructor for non-composite signals.
  DOMTaskSignal(ExecutionContext*, const AtomicString& priority, SignalType);

  // Constructor for composite signals.
  DOMTaskSignal(ScriptState*,
                const AtomicString& priority,
                DOMTaskSignal* source_task_signal,
                HeapVector<Member<AbortSignal>>& source_abort_signals);
  ~DOMTaskSignal() override;

  // task_signal.idl
  static DOMTaskSignal* any(ScriptState*,
                            HeapVector<Member<AbortSignal>> signals,
                            TaskSignalAnyInit*);
  AtomicString priority();
  DEFINE_ATTRIBUTE_EVENT_LISTENER(prioritychange, kPrioritychange)

  [[nodiscard]] DOMTaskSignal::AlgorithmHandle* AddPriorityChangeAlgorithm(
      base::RepeatingClosure algorithm);
  void SignalPriorityChange(const AtomicString& priority, ExceptionState&);

  bool IsTaskSignal() const override { return true; }

  void Trace(Visitor*) const override;
  bool HasPendingActivity() const override;

  bool HasFixedPriority() const;

 private:
  // AbortSignal overrides to support priority composition.
  void DetachFromController() override;
  AbortSignalCompositionManager* GetCompositionManager(
      AbortSignalCompositionType) override;
  void OnSignalSettled(AbortSignalCompositionType) override;

  AtomicString priority_;
  HeapLinkedHashSet<WeakMember<AlgorithmHandle>> priority_change_algorithms_;
  Member<AbortSignalCompositionManager> priority_composition_manager_;
  bool is_priority_changing_ = false;
};

template <>
struct DowncastTraits<DOMTaskSignal> {
  static bool AllowFrom(const AbortSignal& signal) {
    return signal.IsTaskSignal();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_SIGNAL_H_
