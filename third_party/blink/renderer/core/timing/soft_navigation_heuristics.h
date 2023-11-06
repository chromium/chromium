// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_HEURISTICS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_HEURISTICS_H_

#include "base/containers/enum_set.h"
#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

// This class contains the logic for calculating Single-Page-App soft navigation
// heuristics. See https://github.com/WICG/soft-navigations
class SoftNavigationHeuristics
    : public GarbageCollected<SoftNavigationHeuristics>,
      public Supplement<LocalDOMWindow>,
      public scheduler::TaskAttributionTracker::Observer {
 public:
  // Supplement boilerplate.
  static const char kSupplementName[];
  explicit SoftNavigationHeuristics(LocalDOMWindow& window);
  virtual ~SoftNavigationHeuristics() = default;
  static SoftNavigationHeuristics* From(LocalDOMWindow&);

  // GarbageCollected boilerplate.
  void Trace(Visitor*) const override;

  // The class's API.
  void UserInitiatedInteraction(ScriptState*,
                                bool is_unfocused_keyboard_event,
                                bool is_new_interaction);
  void ClickEventEnded(ScriptState*);
  void SameDocumentNavigationStarted(ScriptState*);
  void SameDocumentNavigationCommitted(ScriptState*, const String& url);
  bool ModifiedDOM(ScriptState*);
  uint32_t SoftNavigationCount() { return soft_navigation_count_; }

  // TaskAttributionTracker::Observer's implementation.
  void OnCreateTaskScope(scheduler::TaskAttributionInfo&) override;
  void OnTaskDisposal(const scheduler::TaskAttributionInfo&) override;
  ExecutionContext* GetExecutionContext() override;

  void RecordPaint(LocalFrame*,
                   uint64_t painted_area,
                   bool is_modified_by_soft_navigation);

 private:
  void ReportSoftNavigationToMetrics(LocalFrame* frame) const;
  void CheckSoftNavigationConditions();
  void SetIsTrackingSoftNavigationHeuristicsOnDocument(bool value) const;
  enum FlagType : uint8_t {
    kURLChange,
    kMainModification,
  };
  using FlagTypeSet = base::EnumSet<FlagType, kURLChange, kMainModification>;

  bool IsCurrentTaskDescendantOfClickEventHandler(ScriptState*);
  bool SetFlagIfDescendantAndCheck(ScriptState*,
                                   FlagType,
                                   bool run_descendent_check);
  void ResetHeuristic();
  void ResetPaintsIfNeeded(ScriptState*);
  void CommitPreviousPaints(LocalFrame*);
  void EmitSoftNavigationEntry(LocalFrame*);

  // Here we need a HashSet as we could have more than 1 task reacting to a user
  // interaction. E.g. a click event and a navigate event handler, or 3
  // different keyboard events handlers.
  WTF::HashSet<scheduler::TaskAttributionIdType>
      potential_soft_navigation_task_ids_;
  size_t disposed_soft_navigation_tasks_ = 0;
  WTF::HashMap<scheduler::TaskAttributionIdType, bool>
      soft_navigation_descendant_cache_;
  FlagTypeSet flag_set_;
  bool did_reset_paints_ = false;
  bool did_commit_previous_paints_ = false;
  String url_;
  // The timestamp just before the event responding to the user's interaction
  // started processing. In case of multiple events for a single interaction
  // (e.g. a keyboard key press resulting in keydown, keypress, and keyup), this
  // timestamp would be the time before processing started on the first event.
  base::TimeTicks user_interaction_timestamp_;
  uint32_t soft_navigation_count_ = 0;
  uint64_t softnav_painted_area_ = 0;
  uint64_t initial_painted_area_ = 0;
  uint64_t viewport_area_ = 0;
  bool soft_navigation_conditions_met_ = false;
  bool initial_interaction_encountered_ = false;
};

// This class defines a scope that would cover click or navigation related
// events, in order for the SoftNavigationHeuristics class to be able to keep
// track of them and their descendant tasks.
class SoftNavigationEventScope {
 public:
  SoftNavigationEventScope(SoftNavigationHeuristics* heuristics,
                           ScriptState* script_state,
                           bool is_unfocused_keyboard_event,
                           bool is_new_interaction)
      : heuristics_(heuristics), script_state_(script_state) {
    heuristics->UserInitiatedInteraction(
        script_state, is_unfocused_keyboard_event, is_new_interaction);
  }
  ~SoftNavigationEventScope() { heuristics_->ClickEventEnded(script_state_); }

 private:
  Persistent<SoftNavigationHeuristics> heuristics_;
  Persistent<ScriptState> script_state_;
};

}  // namespace blink

#endif  //  THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_HEURISTICS_H_
