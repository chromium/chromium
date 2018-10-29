/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_ANIMATION_SVG_SMIL_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_ANIMATION_SVG_SMIL_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/svg/animation/smil_time.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_tests.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class ConditionEventListener;
class SMILTimeContainer;
class IdTargetObserver;
class SVGSMILElement;

// This class implements SMIL interval timing model as needed for SVG animation.
class CORE_EXPORT SVGSMILElement : public SVGElement, public SVGTests {
  USING_GARBAGE_COLLECTED_MIXIN(SVGSMILElement);

 public:
  SVGSMILElement(const QualifiedName&, Document&);
  ~SVGSMILElement() override;

  void ParseAttribute(const AttributeModificationParams&) override;
  void SvgAttributeChanged(const QualifiedName&) override;
  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;

  virtual bool HasValidTarget();
  virtual void AnimationAttributeChanged() = 0;

  SMILTimeContainer* TimeContainer() const { return time_container_.Get(); }

  SVGElement* targetElement() const { return target_element_; }
  const QualifiedName& AttributeName() const { return attribute_name_; }

  void BeginByLinkActivation();

  enum Restart { kRestartAlways, kRestartWhenNotActive, kRestartNever };

  Restart GetRestart() const { return static_cast<Restart>(restart_); }

  enum FillMode { kFillRemove, kFillFreeze };

  FillMode Fill() const { return static_cast<FillMode>(fill_); }

  SMILTime Dur() const;
  SMILTime RepeatDur() const;
  SMILTime RepeatCount() const;
  SMILTime MaxValue() const;
  SMILTime MinValue() const;

  SMILTime Elapsed() const;

  SMILTime IntervalBegin() const { return interval_.begin; }
  SMILTime PreviousIntervalBegin() const { return previous_interval_begin_; }
  SMILTime SimpleDuration() const;

  void SeekToIntervalCorrespondingToTime(double elapsed);
  bool Progress(double elapsed, bool seek_to_time);
  SMILTime NextProgressTime() const;
  void UpdateAnimatedValue(SVGSMILElement* result_element) {
    UpdateAnimation(last_percent_, last_repeat_, result_element);
  }

  void Reset();

  static SMILTime ParseClockValue(const String&);
  static SMILTime ParseOffsetValue(const String&);

  bool IsContributing(double elapsed) const;
  bool IsFrozen() const;

  unsigned DocumentOrderIndex() const { return document_order_index_; }
  void SetDocumentOrderIndex(unsigned index) { document_order_index_ = index; }

  virtual void ResetAnimatedType() = 0;
  virtual void ClearAnimatedType() = 0;
  virtual void ApplyResultsToTarget() = 0;

  bool AnimatedTypeIsLocked() const { return animated_property_locked_; }
  void LockAnimatedType() {
    DCHECK(!animated_property_locked_);
    animated_property_locked_ = true;
  }
  void UnlockAnimatedType() {
    DCHECK(animated_property_locked_);
    animated_property_locked_ = false;
  }

  void ConnectSyncBaseConditions();
  void ConnectEventBaseConditions();

  void ScheduleEvent(const AtomicString& event_type);
  void ScheduleRepeatEvents(unsigned);
  void DispatchPendingEvent(const AtomicString& event_type);

  virtual bool IsSVGDiscardElement() const { return false; }

  void Trace(blink::Visitor*) override;

 protected:
  enum BeginOrEnd { kBegin, kEnd };

  void AddInstanceTime(
      BeginOrEnd,
      SMILTime,
      SMILTimeWithOrigin::Origin = SMILTimeWithOrigin::kParserOrigin);

  void SetInactive() { active_state_ = kInactive; }

  void SetTargetElement(SVGElement*);

  // Sub-classes may need to take action when the target is changed.
  virtual void WillChangeAnimationTarget();
  virtual void DidChangeAnimationTarget();

  QualifiedName attribute_name_;

 private:
  void BuildPendingResource() override;
  void ClearResourceAndEventBaseReferences();
  void ClearConditions();

  virtual void StartedActiveInterval() = 0;
  void EndedActiveInterval();
  virtual void UpdateAnimation(float percent,
                               unsigned repeat,
                               SVGSMILElement* result_element) = 0;

  bool LayoutObjectIsNeeded(const ComputedStyle&) const override {
    return false;
  }

  SMILTime FindInstanceTime(BeginOrEnd,
                            SMILTime minimum_time,
                            bool equals_minimum_ok) const;

  enum IntervalSelector { kFirstInterval, kNextInterval };

  SMILInterval ResolveInterval(IntervalSelector) const;
  void ResolveFirstInterval();
  bool ResolveNextInterval();
  SMILTime ResolveActiveEnd(SMILTime resolved_begin,
                            SMILTime resolved_end) const;
  SMILTime RepeatingDuration() const;

  enum RestartedInterval { kDidNotRestartInterval, kDidRestartInterval };

  RestartedInterval MaybeRestartInterval(double elapsed);
  void BeginListChanged(SMILTime event_time);
  void EndListChanged(SMILTime event_time);

  // This represents conditions on elements begin or end list that need to be
  // resolved on runtime, for example
  // <animate begin="otherElement.begin + 8s; button.click" ... />
  class Condition : public GarbageCollectedFinalized<Condition> {
   public:
    enum Type { kEventBase, kSyncbase, kAccessKey };

    static Condition* Create(Type type,
                             BeginOrEnd begin_or_end,
                             const AtomicString& base_id,
                             const AtomicString& name,
                             SMILTime offset,
                             int repeat = -1) {
      return new Condition(type, begin_or_end, base_id, name, offset, repeat);
    }
    ~Condition();
    void Trace(blink::Visitor*);

    Type GetType() const { return type_; }
    BeginOrEnd GetBeginOrEnd() const { return begin_or_end_; }
    const AtomicString& GetName() const { return name_; }
    SMILTime Offset() const { return offset_; }
    int Repeat() const { return repeat_; }

    void ConnectSyncBase(SVGSMILElement&);
    void DisconnectSyncBase(SVGSMILElement&);
    bool SyncBaseEquals(SVGSMILElement& timed_element) const {
      return base_element_ == timed_element;
    }

    void ConnectEventBase(SVGSMILElement&);
    void DisconnectEventBase(SVGSMILElement&);

   private:
    Condition(Type,
              BeginOrEnd,
              const AtomicString& base_id,
              const AtomicString& name,
              SMILTime offset,
              int repeat);

    Type type_;
    BeginOrEnd begin_or_end_;
    AtomicString base_id_;
    AtomicString name_;
    SMILTime offset_;
    int repeat_;
    Member<SVGElement> base_element_;
    Member<IdTargetObserver> base_id_observer_;
    Member<ConditionEventListener> event_listener_;
  };
  bool ParseCondition(const String&, BeginOrEnd begin_or_end);
  void ParseBeginOrEnd(const String&, BeginOrEnd begin_or_end);

  void DisconnectSyncBaseConditions();
  void DisconnectEventBaseConditions();

  void NotifyDependentsIntervalChanged();
  void CreateInstanceTimesFromSyncbase(SVGSMILElement& syncbase);
  void AddSyncBaseDependent(SVGSMILElement&);
  void RemoveSyncBaseDependent(SVGSMILElement&);

  enum ActiveState { kInactive, kActive, kFrozen };

  ActiveState GetActiveState() const {
    return static_cast<ActiveState>(active_state_);
  }
  ActiveState DetermineActiveState(SMILTime elapsed) const;
  float CalculateAnimationPercentAndRepeat(double elapsed,
                                           unsigned& repeat) const;
  SMILTime CalculateNextProgressTime(double elapsed) const;

  Member<SVGElement> target_element_;
  Member<IdTargetObserver> target_id_observer_;

  HeapVector<Member<Condition>> conditions_;
  bool sync_base_conditions_connected_;
  bool has_end_event_conditions_;

  bool is_waiting_for_first_interval_;
  bool is_scheduled_;

  using TimeDependentSet = HeapHashSet<Member<SVGSMILElement>>;
  TimeDependentSet sync_base_dependents_;

  // Instance time lists
  Vector<SMILTimeWithOrigin> begin_times_;
  Vector<SMILTimeWithOrigin> end_times_;

  // This is the upcoming or current interval
  SMILInterval interval_;

  SMILTime previous_interval_begin_;

  unsigned active_state_ : 2;
  unsigned restart_ : 2;
  unsigned fill_ : 1;
  float last_percent_;
  unsigned last_repeat_;

  SMILTime next_progress_time_;

  Member<SMILTimeContainer> time_container_;
  unsigned document_order_index_;

  Vector<unsigned> repeat_event_count_list_;

  mutable SMILTime cached_dur_;
  mutable SMILTime cached_repeat_dur_;
  mutable SMILTime cached_repeat_count_;
  mutable SMILTime cached_min_;
  mutable SMILTime cached_max_;

  bool animated_property_locked_;

  friend class ConditionEventListener;
};

inline bool IsSVGSMILElement(const SVGElement& element) {
  return element.HasTagName(svg_names::kSetTag) ||
         element.HasTagName(svg_names::kAnimateTag) ||
         element.HasTagName(svg_names::kAnimateMotionTag) ||
         element.HasTagName(svg_names::kAnimateTransformTag) ||
         element.HasTagName((svg_names::kDiscardTag));
}

DEFINE_SVGELEMENT_TYPE_CASTS_WITH_FUNCTION(SVGSMILElement);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_ANIMATION_SVG_SMIL_ELEMENT_H_
