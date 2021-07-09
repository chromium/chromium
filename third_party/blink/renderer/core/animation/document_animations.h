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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_DOCUMENT_ANIMATIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_DOCUMENT_ANIMATIONS_H_

#include "base/auto_reset.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class AnimationTimeline;
class Document;
class PaintArtifactCompositor;

class CORE_EXPORT DocumentAnimations final
    : public GarbageCollected<DocumentAnimations> {
 public:
  DocumentAnimations(Document*);
  ~DocumentAnimations() = default;

  uint64_t TransitionGeneration() const {
    return current_transition_generation_;
  }
  void IncrementTrasitionGeneration() { current_transition_generation_++; }
  void AddTimeline(AnimationTimeline&);
  void UpdateAnimationTimingForAnimationFrame();
  bool NeedsAnimationTimingUpdate();
  void UpdateAnimationTimingIfNeeded();
  void GetAnimationsTargetingTreeScope(HeapVector<Member<Animation>>&,
                                       const TreeScope&);

  // Updates existing animations as part of generating a new (document
  // lifecycle) frame. Note that this considers and updates state for
  // both composited and non-composited animations.
  void UpdateAnimations(
      DocumentLifecycle::LifecycleState required_lifecycle_state,
      const PaintArtifactCompositor*,
      bool compositor_properties_updated);

  size_t GetAnimationsCount();

  void MarkAnimationsCompositorPending();

  HeapVector<Member<Animation>> getAnimations(const TreeScope&);

  // All newly created AnimationTimelines are considered "unvalidated". This
  // means that the internal state of the timeline is considered tentative,
  // and that computing the actual state may require an additional style/layout
  // pass.
  //
  // The lifecycle update will call this function after style and layout has
  // completed. The function will then go though all unvalidated timelines,
  // and compare the current state snapshot to a fresh state snapshot. If they
  // are equal, then the tentative state turned out to be correct, and no
  // further action is needed. Otherwise, all effects targets associated with
  // the timeline are marked for recalc, which causes the style/layout phase
  // to run again.
  //
  // https://github.com/w3c/csswg-drafts/issues/5261
  void ValidateTimelines();

  // By default, animation updates are *implicitly* disallowed. This object
  // can be used to allow or disallow animation updates as follows:
  //
  // AllowAnimationUpdatesScope(..., true): Allow animation updates, unless
  // updates are currently *explicitly* disallowed.
  //
  // AllowAnimationUpdatesScope(..., false): Explicitly disallow animation
  // updates.
  class CORE_EXPORT AllowAnimationUpdatesScope {
    STACK_ALLOCATED();

   public:
    AllowAnimationUpdatesScope(DocumentAnimations&, bool);

   private:
    base::AutoReset<absl::optional<bool>> allow_;
  };

  // Add an element to the set of elements with a pending animation update.
  // The elements in the set can be applied later using,
  // ApplyPendingElementUpdates.
  //
  // It's invalid to call this function during if animation updates are not
  // allowed (see AnimationUpdatesAllowed).
  void AddElementWithPendingAnimationUpdate(Element&);

  // Apply pending updates for any elements previously added during AddElement-
  // WithPendingAnimationUpdate
  void ApplyPendingElementUpdates();

  bool AnimationUpdatesAllowed() const {
    return allow_animation_updates_.value_or(false);
  }

  const HeapHashSet<WeakMember<AnimationTimeline>>& GetTimelinesForTesting()
      const {
    return timelines_;
  }
  const HeapHashSet<WeakMember<AnimationTimeline>>&
  GetUnvalidatedTimelinesForTesting() const {
    return unvalidated_timelines_;
  }
  uint64_t current_transition_generation_;
  void Trace(Visitor*) const;

#if DCHECK_IS_ON()
  void AssertNoPendingUpdates() {
    DCHECK(elements_with_pending_updates_.IsEmpty());
  }
#endif

 protected:
  using ReplaceableAnimationsMap =
      HeapHashMap<Member<Element>, Member<HeapVector<Member<Animation>>>>;
  void RemoveReplacedAnimations(ReplaceableAnimationsMap*);

 private:
  friend class AllowAnimationUpdatesScope;
  friend class AnimationUpdateScope;

  void MarkPendingIfCompositorPropertyAnimationChanges(
      const PaintArtifactCompositor*);

  Member<Document> document_;
  HeapHashSet<WeakMember<AnimationTimeline>> timelines_;
  HeapHashSet<WeakMember<AnimationTimeline>> unvalidated_timelines_;
  HeapHashSet<WeakMember<Element>> elements_with_pending_updates_;
  absl::optional<bool> allow_animation_updates_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_DOCUMENT_ANIMATIONS_H_
