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

#include "third_party/blink/renderer/core/animation/document_animations.h"

#include <algorithm>

#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/animation_trigger.h"
#include "third_party/blink/renderer/core/animation/css/css_animation.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/pending_animations.h"
#include "third_party/blink/renderer/core/animation/worklet_animation_controller.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/named_animation_trigger_map.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"

namespace blink {

namespace {

void UpdateAnimationTiming(
    Document& document,
    HeapHashSet<WeakMember<AnimationTimeline>>& timelines,
    TimingUpdateReason reason) {
  for (auto& timeline : timelines)
    timeline->ServiceAnimations(reason);
  document.GetWorkletAnimationController().UpdateAnimationTimings(reason);
}
}  // namespace

// static
void DocumentAnimations::FindRelevantTriggerAttachments(
    CSSAnimation& animation,
    TriggerAttachmentMap& relevant_attachments_out) {
  const Member<const StyleTriggerAttachmentVector>&
      animation_trigger_attachments = animation.GetTriggerAttachments();
  if (!animation_trigger_attachments) {
    return;
  }

  const Element* owning_element = animation.OwningElement();
  if (!owning_element) {
    return;
  }

  // Map to accumulate all scoped triggers known to ancestors (who might be
  // aware of triggers that are outside the owning_element's ancestry but in the
  // same trigger-scope - such a trigger might come later in tree-order and
  // therefore be the correct candidate).
  TriggerScopedNameMap* global_trigger_map =
      MakeGarbageCollected<TriggerScopedNameMap>();
  // Map to accumulate triggers defined on elements in the ancestry of the
  // owning element. These are to be checked first before looking at the global
  // list.
  TriggerScopedNameMap* ancestor_trigger_map =
      MakeGarbageCollected<TriggerScopedNameMap>();

  const Element* element = owning_element;
  while (element) {
    const LayoutBox* element_box = element->GetLayoutBox();
    if (!element_box) {
      element = element->parentElement();
      continue;
    }

    if (NamedAnimationTriggerMap* element_named_triggers =
            element->NamedTriggers()) {
      for (const auto& named_trigger : element_named_triggers->Keys()) {
        TriggerScopedName* trigger_scoped_name =
            ToTriggerScopedName(*named_trigger, *element);
        auto it = ancestor_trigger_map->find(trigger_scoped_name);
        // Within the ancestry, the nearest ancestor with the name is selected.
        // So, if a name has already been found, skip the update.
        if (it == ancestor_trigger_map->end()) {
          ancestor_trigger_map->Set(trigger_scoped_name, element);
        }
      }
    }

    for (const auto& fragment : element_box->PhysicalFragments()) {
      const TriggerScopedNameMap* named_triggers = fragment.NamedTriggers();
      if (!named_triggers) {
        continue;
      }

      for (const auto& entry : *named_triggers) {
        global_trigger_map->Set(entry.key, entry.value);
      }
    }

    element = element->parentElement();
  }

  const auto add_relevant_trigger =
      [&](const Element* source, TriggerScopedName* trigger_scoped_name,
          const StyleTriggerAttachment* attachment) {
        AnimationTrigger* trigger =
            source->NamedTrigger(trigger_scoped_name->GetScopedName());
        DCHECK(trigger);
        relevant_attachments_out.Set(trigger_scoped_name,
                                     std::make_pair(trigger, attachment));
      };

  for (const auto& attachment : *animation_trigger_attachments) {
    TriggerScopedName* trigger_scoped_name =
        ToTriggerScopedName(*attachment->TriggerName(), *owning_element);

    auto it = ancestor_trigger_map->find(trigger_scoped_name);
    if (it != ancestor_trigger_map->end()) {
      add_relevant_trigger(it->value, trigger_scoped_name, attachment);
      continue;
    }

    // If we didn't find a name in the ancestry, search the global map.
    it = global_trigger_map->find(trigger_scoped_name);
    if (it != global_trigger_map->end()) {
      add_relevant_trigger(it->value, trigger_scoped_name, attachment);
    }
  }
}

// static
void DocumentAnimations::UpdateTriggerAttachments(
    CSSAnimation& animation,
    const TriggerAttachmentMap& relevant_attachments) {
  HeapHashMap<Member<const TriggerScopedName>, Member<AnimationTrigger>>
      named_trigger_attachments_copy;
  // Clear old trigger associations. Associations that are still relevant will
  // get added below.
  animation.NamedTriggerAttachments().swap(named_trigger_attachments_copy);

  const auto& relevant_attachment_values = relevant_attachments.Values();
  // Remove obsolete triggers.
  for (const auto& [scope, trigger] : named_trigger_attachments_copy) {
    // As only a single trigger is allowed per animation, we need to first
    // remove obsolete triggers before relevant triggers can be attached.
    if (std::any_of(
            relevant_attachment_values.begin(),
            relevant_attachment_values.end(),
            [&](const std::pair<Member<AnimationTrigger>,
                                Member<const StyleTriggerAttachment>>& pair) {
              return pair.first == trigger;
            })) {
      continue;
    }

    trigger->removeAnimation(&animation);
  }

  for (const auto& [scope, trigger_attachment] : relevant_attachments) {
    AnimationTrigger* trigger = trigger_attachment.first;
    const StyleTriggerAttachment* attachment = trigger_attachment.second;

    animation.SetNamedTriggerAttachment(scope, trigger);
    attachment->Attach(*trigger, *scope, animation);
  }
}

DocumentAnimations::DocumentAnimations(Document* document)
    : document_(document) {}

void DocumentAnimations::AddTimeline(AnimationTimeline& timeline) {
  timelines_.insert(&timeline);
}

void DocumentAnimations::UpdateAnimationTimingForAnimationFrame() {
  // https://w3.org/TR/web-animations-1/#timelines

  // 1. Update the current time of all timelines associated with doc passing now
  //    as the timestamp.
  UpdateAnimationTiming(*document_, timelines_, kTimingUpdateForAnimationFrame);

  // 2. Remove replaced animations for doc.
  ReplaceableAnimationsMap replaceable_animations_map;
  for (auto& timeline : timelines_)
    timeline->getReplaceableAnimations(&replaceable_animations_map);
  RemoveReplacedAnimations(&replaceable_animations_map);

  // 3. Perform a microtask checkpoint
  // This is to ensure that any microtasks queued up as a result of resolving or
  // rejecting Promise objects as part of updating timelines run their callbacks
  // prior to dispatching animation events and generating the next main frame.
  document_->GetAgent().event_loop()->PerformMicrotaskCheckpoint();
}

bool DocumentAnimations::NeedsAnimationTimingUpdate() {
  for (auto& timeline : timelines_) {
    if (timeline->HasOutdatedAnimation() ||
        timeline->NeedsAnimationTimingUpdate())
      return true;
  }
  return false;
}

void DocumentAnimations::UpdateAnimationTimingIfNeeded() {
  if (NeedsAnimationTimingUpdate())
    UpdateAnimationTiming(*document_, timelines_, kTimingUpdateOnDemand);
}

void DocumentAnimations::UpdateAnimations(
    DocumentLifecycle::LifecycleState required_lifecycle_state,
    const PaintArtifactCompositor* paint_artifact_compositor,
    bool compositor_properties_updated) {
  DCHECK(document_->Lifecycle().GetState() >= required_lifecycle_state);

  if (compositor_properties_updated)
    MarkPendingIfCompositorPropertyAnimationChanges(paint_artifact_compositor);

  if (document_->GetPendingAnimations().Update(paint_artifact_compositor)) {
    DCHECK(document_->View());
    document_->View()->ScheduleAnimation();
  }

  UpdateCompositorAnimationTriggers(paint_artifact_compositor);

  document_->GetWorkletAnimationController().UpdateAnimationStates();
  document_->GetFrame()->ScheduleNextServiceForPostLayoutSnapshotClients();
  for (auto& timeline : timelines_) {
    // ScrollSnapshotTimelines are already handled as PostLayoutSnapshotClients
    // above.
    if (!timeline->IsScrollSnapshotTimeline()) {
      timeline->ScheduleNextService();
    }
  }
}

void DocumentAnimations::MarkPendingIfCompositorPropertyAnimationChanges(
    const PaintArtifactCompositor* paint_artifact_compositor) {
  for (auto& timeline : timelines_) {
    timeline->MarkPendingIfCompositorPropertyAnimationChanges(
        paint_artifact_compositor);
  }
}

size_t DocumentAnimations::GetAnimationsCount() {
  wtf_size_t total_animations_count = 0;
  if (document_->View()) {
    if (document_->View()->GetCompositorAnimationHost()) {
      for (auto& timeline : timelines_) {
        if (timeline->HasAnimations())
          total_animations_count += timeline->AnimationsNeedingUpdateCount();
      }
    }
  }
  return total_animations_count;
}

void DocumentAnimations::MarkAnimationsCompositorPending() {
  for (auto& timeline : timelines_)
    timeline->MarkAnimationsCompositorPending();
}

HeapVector<Member<Animation>> DocumentAnimations::getAnimations(
    const TreeScope& tree_scope) {
  // This method implements the Document::getAnimations method defined in the
  // web-animations-1 spec.
  // https://w3.org/TR/web-animations-1/#extensions-to-the-documentorshadowroot-interface-mixin
  document_->UpdateStyleAndLayoutTree();
  HeapVector<Member<Animation>> animations;
  if (document_->GetPage())
    animations = document_->GetPage()->Animator().GetAnimations(tree_scope);
  else
    GetAnimationsTargetingTreeScope(animations, tree_scope);

  std::sort(animations.begin(), animations.end(), Animation::CompareAnimations);
  return animations;
}

void DocumentAnimations::DetachCompositorTimelines() {
  if (!Platform::Current()->IsThreadedAnimationEnabled() ||
      !document_->GetSettings()->GetAcceleratedCompositingEnabled() ||
      !document_->GetPage())
    return;

  for (auto& timeline : timelines_) {
    cc::AnimationTimeline* compositor_timeline = timeline->CompositorTimeline();
    if (!compositor_timeline)
      continue;

    if (cc::AnimationHost* host =
            document_->GetPage()->GetChromeClient().GetCompositorAnimationHost(
                *document_->GetFrame())) {
      host->DetachAnimationTimeline(compositor_timeline);
    }
  }
}

void DocumentAnimations::DetachCompositorTriggers() {
  if (!Platform::Current()->IsThreadedAnimationEnabled() ||
      !document_->GetSettings()->GetAcceleratedCompositingEnabled() ||
      !document_->GetPage()) {
    return;
  }

  for (auto& trigger : triggers_) {
    cc::AnimationTrigger* compositor_trigger = trigger->CompositorTrigger();
    if (!compositor_trigger) {
      continue;
    }

    if (cc::AnimationHost* host =
            document_->GetPage()->GetChromeClient().GetCompositorAnimationHost(
                *document_->GetFrame())) {
      host->DetachTrigger(compositor_trigger);
    }
  }
}

void DocumentAnimations::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(timelines_);
  visitor->Trace(triggers_);
  visitor->Trace(triggered_animations_);
  visitor->Trace(global_deferred_timelines_);
}

void DocumentAnimations::GetAnimationsTargetingTreeScope(
    HeapVector<Member<Animation>>& animations,
    const TreeScope& tree_scope) {
  // This method follows the timelines in a given docmuent and append all the
  // animations to the reference animations.
  for (auto& timeline : timelines_) {
    for (const auto& animation : timeline->GetAnimations()) {
      if (animation->ReplaceStateRemoved())
        continue;
      if (!animation->effect() || (!animation->effect()->IsCurrent() &&
                                   !animation->effect()->IsInEffect())) {
        continue;
      }
      auto* effect = DynamicTo<KeyframeEffect>(animation->effect());
      Element* target = effect->target();
      if (!target || !target->isConnected())
        continue;
      if (&tree_scope != &target->GetTreeScope())
        continue;
      animations.push_back(animation);
    }
  }
}

void DocumentAnimations::RemoveReplacedAnimations(
    DocumentAnimations::ReplaceableAnimationsMap* replaceable_animations_map) {
  HeapVector<Member<Animation>> animations_to_remove;
  for (auto& elem_it : *replaceable_animations_map) {
    GCedHeapVector<Member<Animation>>* animations = elem_it.value;

    // Only elements with multiple animations in the replaceable state need to
    // be checked.
    if (animations->size() == 1)
      continue;

    // By processing in decreasing order by priority, we can perform a single
    // pass for discovery of replaced properties.
    std::sort(animations->begin(), animations->end(),
              Animation::CompareAnimations);
    PropertyHandleSet replaced_properties;
    for (auto anim_it = animations->rbegin(); anim_it != animations->rend();
         anim_it++) {
      // Remaining conditions for removal:
      // * has a replace state of active,  and
      // * for which there exists for each target property of every animation
      //   effect associated with animation, an animation effect associated with
      //   a replaceable animation with a higher composite order than animation
      //   that includes the same target property.

      // Only active animations can be removed. We still need to go through
      // the process of iterating over properties if not removable to update
      // the set of properties being replaced.
      bool replace = (*anim_it)->ReplaceStateActive();
      for (const auto& property :
           To<KeyframeEffect>((*anim_it)->effect())->Model()->Properties()) {
        auto inserted = replaced_properties.insert(property);
        if (inserted.is_new_entry) {
          // Top-most compositor order animation affecting this property.
          replace = false;
        }
      }
      if (replace)
        animations_to_remove.push_back(*anim_it);
    }
  }
  scoped_refptr<scheduler::EventLoop> event_loop =
      document_->GetAgent().event_loop();

  // The list of animations for removal is constructed in reverse composite
  // ordering for efficiency. Flip the ordering to ensure that events are
  // dispatched in composite order.  Queue as a microtask so that the finished
  // event is dispatched ahead of the remove event.
  for (auto it = animations_to_remove.rbegin();
       it != animations_to_remove.rend(); it++) {
    Animation* animation = *it;
    event_loop->EnqueueMicrotask(BindOnce(&Animation::RemoveReplacedAnimation,
                                          WrapWeakPersistent(animation)));
  }
}

void DocumentAnimations::UpdateAnimationTriggerAttachments() {
  ExecuteTriggerAttachmentUpdates();
}

void DocumentAnimations::AddAnimationTrigger(AnimationTrigger& trigger) {
  triggers_.insert(&trigger);
}

void DocumentAnimations::UpdateCompositorAnimationTriggers(
    const PaintArtifactCompositor* paint_artifact_compositor) {
  if (!RuntimeEnabledFeatures::AnimationTriggerEnabled() ||
      !Platform::Current()->IsThreadedAnimationEnabled()) {
    return;
  }

  for (AnimationTrigger* trigger : triggers_) {
    trigger->UpdateCompositorTrigger(paint_artifact_compositor);
  }
}

void DocumentAnimations::ExecuteTriggerAttachmentUpdates() {
  HeapHashSet<WeakMember<CSSAnimation>> triggered_animations;
  triggered_animations.swap(triggered_animations_);

  for (CSSAnimation* animation : triggered_animations) {
    const Member<const StyleTriggerAttachmentVector>&
        animation_trigger_attachments = animation->GetTriggerAttachments();
    TriggerAttachmentMap relevant_attachments;
    if (animation_trigger_attachments) {
      AddTriggeredAnimation(animation);
      FindRelevantTriggerAttachments(*animation, relevant_attachments);
    }
    // Add new triggers, remove obsolete ones.
    UpdateTriggerAttachments(*animation, relevant_attachments);
  }
}

void DocumentAnimations::AddTriggeredAnimation(CSSAnimation* animation) {
  triggered_animations_.insert(animation);
}

}  // namespace blink
