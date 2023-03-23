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
  explicit SoftNavigationHeuristics(LocalDOMWindow& window)
      : Supplement<LocalDOMWindow>(window) {}
  virtual ~SoftNavigationHeuristics() = default;
  static SoftNavigationHeuristics* From(LocalDOMWindow&);

  // GarbageCollected boilerplate.
  void Trace(Visitor*) const override;

  // The class's API.
  void UserInitiatedClick(ScriptState*);
  void ClickEventEnded(ScriptState*);
  void SawURLChange(ScriptState*,
                    const String& url,
                    bool skip_descendant_check = false);
  void ModifiedDOM(ScriptState*);
  uint32_t SoftNavigationCount() { return soft_navigation_count_; }
  void SetAsyncSoftNavigationURL(ScriptState* script_state, const String& url);

  // TaskAttributionTracker::Observer's implementation.
  void OnCreateTaskScope(const scheduler::TaskAttributionId&) override;
  ExecutionContext* GetExecutionContext() override;

 private:
  void CheckAndReportSoftNavigation(ScriptState*);
  void SetIsTrackingSoftNavigationHeuristicsOnDocument(bool value) const;
  enum FlagType : uint8_t {
    kURLChange,
    kMainModification,
  };
  using FlagTypeSet = base::EnumSet<FlagType, kURLChange, kMainModification>;

  bool IsCurrentTaskDescendantOfClickEventHandler(ScriptState*);
  bool SetFlagIfDescendantAndCheck(ScriptState*,
                                   FlagType,
                                   absl::optional<String> url = absl::nullopt,
                                   bool skip_descendant_check = false);
  void ResetHeuristic();
  void ResetPaintsIfNeeded(LocalFrame*, LocalDOMWindow*);

  WTF::HashSet<scheduler::TaskAttributionIdType>
      potential_soft_navigation_task_ids_;
  FlagTypeSet flag_set_;
  bool did_reset_paints_ = false;
  String url_;
  // The timestamp just before the click event responding to the user's click
  // started processing.
  base::TimeTicks user_click_timestamp_;
  uint32_t soft_navigation_count_ = 0;
};

// This class defines a scope that would cover click or navigation related
// events, in order for the SoftNavigationHeuristics class to be able to keep
// track of them and their descendant tasks.
class SoftNavigationEventScope {
 public:
  SoftNavigationEventScope(SoftNavigationHeuristics* heuristics,
                           ScriptState* script_state)
      : heuristics_(heuristics), script_state_(script_state) {
    heuristics->UserInitiatedClick(script_state);
  }
  ~SoftNavigationEventScope() { heuristics_->ClickEventEnded(script_state_); }
  // TODO(yoav): Remove this method, as it's not doing anything useful.
  void SetResult(DispatchEventResult result) { result_ = result; }

 private:
  Persistent<SoftNavigationHeuristics> heuristics_;
  Persistent<ScriptState> script_state_;
  DispatchEventResult result_;
};

}  // namespace blink

#endif  //  THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_HEURISTICS_H_
