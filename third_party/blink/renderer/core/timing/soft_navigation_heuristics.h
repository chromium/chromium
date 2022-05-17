// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_HEURISTICS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_HEURISTICS_H_

#include "base/containers/enum_set.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_id.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

// This class contains the logic for calculating Single-Page-App soft navigation
// heuristics. See
// https://docs.google.com/document/d/1W5Yfcxq5zKgmW5ZCao9FDH85xw3B1K1OrRhSZu0U_IQ/edit#
class SoftNavigationHeuristics
    : public GarbageCollected<SoftNavigationHeuristics>,
      public Supplement<LocalDOMWindow>,
      public scheduler::TaskAttributionTracker::Observer {
 public:
  // Supplement boilerplate.
  static const char kSupplementName[];
  explicit SoftNavigationHeuristics(LocalDOMWindow& window)
      : Supplement<LocalDOMWindow>(window) {}
  virtual ~SoftNavigationHeuristics() = default;
  static SoftNavigationHeuristics* From(LocalDOMWindow&);

  // GarbageCollected boilerplate.
  void Trace(Visitor*) const override;

  // The class's API.
  void UserInitiatedClick(ScriptState*);
  void ClickEventEnded(ScriptState*, bool is_cancelled);
  void SawURLChange(ScriptState*);
  void ModifiedDOM(ScriptState*);
  unsigned SoftNavigationCount() { return soft_navigation_count_; }

  // TaskAttributionTracker::Observer's implementation.
  void OnCreateTaskScope(const scheduler::TaskId&) override;

 private:
  void CheckSoftNavigation(ScriptState*);
  enum FlagType : uint8_t {
    kURLChange,
    kDOMModification,
    kEventCancelled,
  };
  using FlagTypeSet = base::EnumSet<FlagType, kURLChange, kEventCancelled>;

  bool IsCurrentTaskDescendantOfClickEventHandler(ScriptState*);
  bool SetFlagIfDescendantAndCheck(ScriptState*, FlagType);
  void ResetHeuristic();

  WTF::HashSet<scheduler::TaskIdType> potential_soft_navigation_task_ids_;
  FlagTypeSet flag_set_;
  unsigned soft_navigation_count_ = 0;
};

class SoftNavigationEventScope {
 public:
  SoftNavigationEventScope(SoftNavigationHeuristics* heuristics,
                           ScriptState* script_state)
      : heuristics_(heuristics), script_state_(script_state) {
    heuristics->UserInitiatedClick(script_state);
  }
  ~SoftNavigationEventScope() {
    heuristics_->ClickEventEnded(
        script_state_, result_ == DispatchEventResult::kCanceledByEventHandler);
  }
  void SetResult(DispatchEventResult result) { result_ = result; }

 private:
  Persistent<SoftNavigationHeuristics> heuristics_;
  Persistent<ScriptState> script_state_;
  DispatchEventResult result_;
};

}  // namespace blink

#endif  //  THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_HEURISTICS_H_
