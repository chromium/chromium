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

#include <optional>

#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/css/css_animation.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
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

  // Detach compositor timelines to prevent further ticking of any animations
  // associated with the timelines.  Detached timelines may be subsequently
  // reattached if needed.
  void DetachCompositorTimelines();

  // Detach animation triggers on the compositor.
  void DetachCompositorTriggers();

  const HeapHashSet<WeakMember<AnimationTimeline>>& GetTimelinesForTesting()
      const {
    return timelines_;
  }

  static void UpdateTriggerAttachment(
      CSSAnimation& animation,
      base::FunctionRef<void(AnimationTrigger& trigger,
                             const StyleTriggerAttachment& attachment)>
          function);

  void AddAnimationTrigger(AnimationTrigger& trigger);

  // This attaches CSS Animations to AnimationTriggers declared by
  // trigger-instantiating properties like timeline-trigger or event-trigger.
  // It matches the CSS Animations to the AnimationTriggers by matching the
  // names declared in the trigger-instantiating property with the names
  // declared in the animation-trigger property.
  void UpdateAnimationTriggerAttachments();
  // These two functions serve the same purpose as
  // UpdateAnimationTriggerAttachments above but restricts the updates to
  // animations with animation-trigger declarations, which is more efficient.
  // They are only used behind a flag while the renderer hang in
  // crbug.com/447174988 is investigated.
  // TODO(crbug.com/447174988): Remove UpdateAnimationTriggerAttachments when
  // the bug is resolved.
  void ExecutePendingTriggerAttachmentUpdates();
  void AddPendingTriggerAttachmentUpdate(CSSAnimation* animation);
  void RemovePendingTriggerAttachmentUpdate(CSSAnimation* animation);

  void UpdateCompositorAnimationTriggers();

  uint64_t current_transition_generation_;
  void Trace(Visitor*) const;

 protected:
  using ReplaceableAnimationsMap =
      HeapHashMap<Member<Element>, Member<GCedHeapVector<Member<Animation>>>>;
  void RemoveReplacedAnimations(ReplaceableAnimationsMap*);

 private:
  void MarkPendingIfCompositorPropertyAnimationChanges(
      const PaintArtifactCompositor*);

  Member<Document> document_;
  HeapHashSet<WeakMember<AnimationTimeline>> timelines_;
  HeapHashSet<WeakMember<AnimationTrigger>> triggers_;
  // Animations which should be attached to triggers after style and layout
  // updates.
  HeapHashSet<WeakMember<CSSAnimation>> pending_trigger_attachment_updates_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_DOCUMENT_ANIMATIONS_H_
