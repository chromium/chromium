// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TIMELINE_H_

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "cc/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_value.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"

namespace blink {

class Document;

enum class TimelinePhase { kInactive, kActive };

class CORE_EXPORT AnimationTimeline : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  struct PhaseAndTime {
    TimelinePhase phase;
    absl::optional<base::TimeDelta> time;
    bool operator==(const PhaseAndTime& other) const {
      return phase == other.phase && time == other.time;
    }
    bool operator!=(const PhaseAndTime& other) const {
      return !(*this == other);
    }
  };

  AnimationTimeline(Document*);
  ~AnimationTimeline() override = default;

  virtual V8CSSNumberish* currentTime();
  virtual CSSNumericValue* getCurrentTime(const String& rangeName) {
    return nullptr;
  }

  absl::optional<AnimationTimeDelta> CurrentTime();
  absl::optional<double> CurrentTimeMilliseconds();
  absl::optional<double> CurrentTimeSeconds();

  virtual V8CSSNumberish* duration();

  TimelinePhase Phase() { return CurrentPhaseAndTime().phase; }

  virtual bool IsDocumentTimeline() const { return false; }
  virtual bool IsScrollTimeline() const { return false; }
  virtual bool IsCSSScrollTimeline() const { return false; }
  virtual bool IsViewTimeline() const { return false; }

  virtual bool IsActive() const = 0;
  virtual AnimationTimeDelta ZeroTime() = 0;
  // https://w3.org/TR/web-animations-1/#monotonically-increasing-timeline
  // A timeline is monotonically increasing if its reported current time is
  // always greater than or equal than its previously reported current time.
  bool IsMonotonicallyIncreasing() const { return IsDocumentTimeline(); }
  // Returns the initial start time for animations that are linked to this
  // timeline. This method gets invoked when initializing the start time of an
  // animation on this timeline for the first time. It exists because the
  // initial start time for scroll-linked and time-linked animations are
  // different.
  //
  // Changing scroll-linked animation start_time initialization is under
  // consideration here: https://github.com/w3c/csswg-drafts/issues/2075.
  virtual absl::optional<base::TimeDelta> InitialStartTimeForAnimations() = 0;
  virtual AnimationTimeDelta CalculateIntrinsicIterationDuration(
      const Animation*,
      const Timing&) {
    return AnimationTimeDelta();
  }

  // Converts timeline offsets to start and end delays in time units based on
  // the timeline duration. In the event that the timeline is not an instance
  // of a view timeline, the delays are zero.
  using TimeDelayPair = std::pair<AnimationTimeDelta, AnimationTimeDelta>;
  virtual TimeDelayPair TimelineOffsetsToTimeDelays(
      const Animation* animation) const {
    return std::make_pair(AnimationTimeDelta(), AnimationTimeDelta());
  }

  Document* GetDocument() const { return document_; }
  virtual void AnimationAttached(Animation*);
  virtual void AnimationDetached(Animation*);

  // Updates animation timing.
  virtual void ServiceAnimations(TimingUpdateReason);
  // Schedules next animations timing update.
  virtual void ScheduleNextService() = 0;
  // Schedules animation timing update on next frame.
  virtual void ScheduleServiceOnNextFrame();

  Animation* Play(AnimationEffect*, ExceptionState& = ASSERT_NO_EXCEPTION);

  virtual bool NeedsAnimationTimingUpdate();
  virtual bool HasAnimations() const { return !animations_.empty(); }
  virtual bool HasOutdatedAnimation() const {
    return outdated_animation_count_ > 0;
  }
  void SetOutdatedAnimation(Animation*);
  void ClearOutdatedAnimation(Animation*);

  virtual wtf_size_t AnimationsNeedingUpdateCount() const;
  const HeapHashSet<WeakMember<Animation>>& GetAnimations() const {
    return animations_;
  }

  cc::AnimationTimeline* CompositorTimeline() const {
    return compositor_timeline_.get();
  }
  virtual cc::AnimationTimeline* EnsureCompositorTimeline() = 0;
  virtual void UpdateCompositorTimeline() {}

  void MarkAnimationsCompositorPending(bool source_changed = false);

  // Checks for animations of composited properties that would have no effect
  // and marks them as pending if this changes.
  void MarkPendingIfCompositorPropertyAnimationChanges(
      const PaintArtifactCompositor*);

  using ReplaceableAnimationsMap =
      HeapHashMap<Member<Element>, Member<HeapVector<Member<Animation>>>>;
  void getReplaceableAnimations(
      ReplaceableAnimationsMap* replaceable_animation_set);

  void Trace(Visitor*) const override;

  virtual absl::optional<AnimationTimeDelta> GetDuration() const {
    return absl::nullopt;
  }

 protected:
  virtual PhaseAndTime CurrentPhaseAndTime() = 0;

  Member<Document> document_;
  unsigned outdated_animation_count_;
  // Animations which will be updated on the next frame
  // i.e. current, in effect, or had timing changed
  HeapHashSet<Member<Animation>> animations_needing_update_;
  // All animations attached to this timeline.
  HeapHashSet<WeakMember<Animation>> animations_;

  scoped_refptr<cc::AnimationTimeline> compositor_timeline_;

  absl::optional<PhaseAndTime> last_current_phase_and_time_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_ANIMATION_TIMELINE_H_
