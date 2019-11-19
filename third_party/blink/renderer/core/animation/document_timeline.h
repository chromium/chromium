/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_DOCUMENT_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_DOCUMENT_TIMELINE_H_

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/effect_model.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_timeline.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class Animation;
class AnimationEffect;
class Document;
class DocumentTimelineOptions;

// DocumentTimeline is constructed and owned by Document, and tied to its
// lifecycle.
class CORE_EXPORT DocumentTimeline : public AnimationTimeline {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class PlatformTiming : public GarbageCollected<PlatformTiming> {
   public:
    // Calls DocumentTimeline's wake() method after duration seconds.
    virtual void WakeAfter(base::TimeDelta duration) = 0;
    virtual void ServiceOnNextFrame() = 0;
    virtual ~PlatformTiming() = default;
    virtual void Trace(blink::Visitor* visitor) {}
  };

  static DocumentTimeline* Create(
      Document*,
      base::TimeDelta origin_time = base::TimeDelta(),
      PlatformTiming* = nullptr);

  // Web Animations API IDL constructor
  static DocumentTimeline* Create(ExecutionContext*,
                                  const DocumentTimelineOptions*);

  DocumentTimeline(Document*, base::TimeDelta origin_time, PlatformTiming*);
  ~DocumentTimeline() override = default;

  bool IsDocumentTimeline() const final { return true; }

  void ServiceAnimations(TimingUpdateReason);
  void ScheduleNextService();

  Animation* Play(AnimationEffect*);
  HeapVector<Member<Animation>> getAnimations();

  void AnimationAttached(Animation*) override;
  // animations_ is a map of weak members so there is no need to explicitly
  // clean it up.
  void AnimationDetached(Animation*) override {}

  bool IsActive() const override;
  base::Optional<base::TimeDelta> InitialStartTimeForAnimations() override;
  bool HasPendingUpdates() const {
    return !animations_needing_update_.IsEmpty();
  }
  wtf_size_t PendingAnimationsCount() const {
    return animations_needing_update_.size();
  }
  base::TimeTicks ZeroTime();
  double currentTime(bool& is_null) override;
  double currentTime();
  base::Optional<base::TimeDelta> CurrentTimeInternal();
  double EffectiveTime();
  void PauseAnimationsForTesting(double);

  void SetAllCompositorPending(bool source_changed = false);
  void SetOutdatedAnimation(Animation*);
  void ClearOutdatedAnimation(Animation*);
  bool HasOutdatedAnimation() const { return outdated_animation_count_ > 0; }
  bool NeedsAnimationTimingUpdate();
  void InvalidateKeyframeEffects(const TreeScope&);

  void SetPlaybackRate(double);
  double PlaybackRate() const;

  CompositorAnimationTimeline* CompositorTimeline() const {
    return compositor_timeline_.get();
  }

  Document* GetDocument() override { return document_.Get(); }
  void Wake();
  void ResetForTesting();
  void SetTimingForTesting(PlatformTiming* timing);
  bool HasAnimations() { return !animations_.IsEmpty(); }

  void Trace(blink::Visitor*) override;

 private:
  Member<Document> document_;
  // Origin time for the timeline relative to the time origin of the document.
  // Provided when the timeline is constructed. See
  // https://drafts.csswg.org/web-animations/#dom-documenttimelineoptions-origintime.
  base::TimeDelta origin_time_;
  // The origin time. This is computed by adding |origin_time_| to the time
  // origin of the document.
  base::TimeTicks zero_time_;
  bool zero_time_initialized_;
  unsigned outdated_animation_count_;
  // Animations which will be updated on the next frame
  // i.e. current, in effect, or had timing changed
  HeapHashSet<Member<Animation>> animations_needing_update_;
  HeapHashSet<WeakMember<Animation>> animations_;

  double playback_rate_;

  friend class SMILTimeContainer;
  static const double kMinimumDelay;

  Member<PlatformTiming> timing_;
  base::Optional<base::TimeDelta> last_current_time_internal_;

  std::unique_ptr<CompositorAnimationTimeline> compositor_timeline_;

  class DocumentTimelineTiming final : public PlatformTiming {
   public:
    DocumentTimelineTiming(DocumentTimeline* timeline)
        : timeline_(timeline),
          timer_(timeline->GetDocument()->GetTaskRunner(
                     TaskType::kInternalDefault),
                 this,
                 &DocumentTimelineTiming::TimerFired) {
      DCHECK(timeline_);
    }

    void WakeAfter(base::TimeDelta duration) override;
    void ServiceOnNextFrame() override;

    void TimerFired(TimerBase*) { timeline_->Wake(); }

    void Trace(blink::Visitor*) override;

   private:
    Member<DocumentTimeline> timeline_;
    TaskRunnerTimer<DocumentTimelineTiming> timer_;
  };

  friend class AnimationDocumentTimelineTest;
};

DEFINE_TYPE_CASTS(DocumentTimeline,
                  AnimationTimeline,
                  timeline,
                  timeline->IsDocumentTimeline(),
                  timeline.IsDocumentTimeline());

}  // namespace blink

#endif
