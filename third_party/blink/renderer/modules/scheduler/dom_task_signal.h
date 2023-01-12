// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_SIGNAL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_DOM_TASK_SIGNAL_H_

#include "base/functional/callback_forward.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
class ExceptionState;
class ExecutionContext;

class MODULES_EXPORT DOMTaskSignal final : public AbortSignal {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class PriorityChangeStatus {
    kNoPriorityChange = 0,
    kPriorityHasChanged = 1,

    kMaxValue = kPriorityHasChanged
  };

  DOMTaskSignal(ExecutionContext*, const AtomicString& priority, SignalType);
  ~DOMTaskSignal() override;

  // task_signal.idl
  AtomicString priority();
  DEFINE_ATTRIBUTE_EVENT_LISTENER(prioritychange, kPrioritychange)

  void AddPriorityChangeAlgorithm(base::RepeatingClosure algorithm);
  void SignalPriorityChange(const AtomicString& priority, ExceptionState&);

  bool IsTaskSignal() const override { return true; }

  void Trace(Visitor*) const override;

  PriorityChangeStatus GetPriorityChangeStatus() const {
    return priority_change_status_;
  }

 private:
  AtomicString priority_;

  PriorityChangeStatus priority_change_status_ =
      PriorityChangeStatus::kNoPriorityChange;

  Vector<base::RepeatingClosure> priority_change_algorithms_;

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
