// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SPELLCHECK_ON_DEMAND_SPELL_CHECK_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SPELLCHECK_ON_DEMAND_SPELL_CHECK_CONTROLLER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/weak_cell.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"

namespace blink {

class ContainerNode;
class Element;
class LocalDOMWindow;
class SpellCheckRequester;

class CORE_EXPORT OnDemandSpellCheckController final
    : public GarbageCollected<OnDemandSpellCheckController>,
      public ExecutionContextLifecycleObserver {
 public:
  enum class State { kInactive, kInProgress };

  explicit OnDemandSpellCheckController(LocalDOMWindow&, SpellCheckRequester&);

  OnDemandSpellCheckController(const OnDemandSpellCheckController&) = delete;
  OnDemandSpellCheckController& operator=(const OnDemandSpellCheckController&) =
      delete;

  ~OnDemandSpellCheckController() override;

  State GetState() const;

  void SetSpellCheckingDisabled(const Element& element);
  void ElementRemoved(const Element&);

  void RequestFullChecking(const ContainerNode* container_node);

  // GarbageCollected overrides:
  void Trace(Visitor*) const override;

  // ExecutionContextLifecycleObserver overrides:
  void ContextDestroyed() override;

 private:
  friend class OnDemandSpellCheckControllerTestPeer;

  void ClearProgress();

  void RequestCheckingForRemainingRange();
  bool FullyCheckedCurrentRootEditable() const;

  const Member<LocalDOMWindow> window_;
  const Member<SpellCheckRequester> spell_check_requester_;
  Member<const ContainerNode> root_editable_;
  Member<Range> remaining_check_range_;
  int last_chunk_index_ = -1;
  TaskHandle task_handle_;
  State state_ = State::kInactive;
  WeakCellFactory<OnDemandSpellCheckController> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SPELLCHECK_ON_DEMAND_SPELL_CHECK_CONTROLLER_H_
