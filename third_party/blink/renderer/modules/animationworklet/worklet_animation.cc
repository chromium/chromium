// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/worklet_animation.h"

#include "base/optional.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/animation_effect_or_animation_effect_sequence.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/animation/worklet_animation_controller.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/modules/animationworklet/css_animation_worklet.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

bool ConvertAnimationEffects(
    const AnimationEffectOrAnimationEffectSequence& effects,
    HeapVector<Member<KeyframeEffect>>& keyframe_effects,
    String& error_string) {
  DCHECK(keyframe_effects.IsEmpty());

  // Currently we only support KeyframeEffect.
  if (effects.IsAnimationEffect()) {
    auto* const effect = effects.GetAsAnimationEffect();
    if (!effect->IsKeyframeEffect()) {
      error_string = "Effect must be a KeyframeEffect object";
      return false;
    }
    keyframe_effects.push_back(ToKeyframeEffect(effect));
  } else {
    const HeapVector<Member<AnimationEffect>>& effect_sequence =
        effects.GetAsAnimationEffectSequence();
    keyframe_effects.ReserveInitialCapacity(effect_sequence.size());
    for (const auto& effect : effect_sequence) {
      if (!effect->IsKeyframeEffect()) {
        error_string = "Effects must all be KeyframeEffect objects";
        return false;
      }
      keyframe_effects.push_back(ToKeyframeEffect(effect));
    }
  }

  if (keyframe_effects.IsEmpty()) {
    error_string = "Effects array must be non-empty";
    return false;
  }

  // TODO(crbug.com/781816): Allow using effects with no target.
  for (const auto& effect : keyframe_effects) {
    if (!effect->target()) {
      error_string = "All effect targets must exist";
      return false;
    }
  }

  Document& target_document = keyframe_effects.at(0)->target()->GetDocument();
  for (const auto& effect : keyframe_effects) {
    if (effect->target()->GetDocument() != target_document) {
      error_string = "All effects must target elements in the same document";
      return false;
    }
  }
  return true;
}

bool IsActive(const Animation::AnimationPlayState& state) {
  switch (state) {
    case Animation::kIdle:
    case Animation::kPending:
      return false;
    case Animation::kRunning:
    case Animation::kPaused:
      return true;
    default:
      // kUnset and kFinished are not used in WorkletAnimation.
      NOTREACHED();
      return false;
  }
}

bool ValidateTimeline(const DocumentTimelineOrScrollTimeline& timeline,
                      String& error_string) {
  if (timeline.IsScrollTimeline()) {
    DoubleOrScrollTimelineAutoKeyword time_range;
    timeline.GetAsScrollTimeline()->timeRange(time_range);
    if (time_range.IsScrollTimelineAutoKeyword()) {
      error_string = "ScrollTimeline timeRange must have non-auto value";
      return false;
    }
  }
  return true;
}

AnimationTimeline* ConvertAnimationTimeline(
    const Document& document,
    const DocumentTimelineOrScrollTimeline& timeline) {
  if (timeline.IsScrollTimeline())
    return timeline.GetAsScrollTimeline();

  if (timeline.IsDocumentTimeline())
    return timeline.GetAsDocumentTimeline();

  return &document.Timeline();
}

bool CheckElementComposited(const Node& target) {
  return target.GetLayoutObject() &&
         target.GetLayoutObject()->GetCompositingState() ==
             kPaintsIntoOwnBacking;
}

base::Optional<CompositorElementId> GetCompositorScrollElementId(
    const Node& node) {
  if (!node.GetLayoutObject() || !node.GetLayoutObject()->UniqueId())
    return base::nullopt;
  return CompositorElementIdFromUniqueObjectId(
      node.GetLayoutObject()->UniqueId(),
      CompositorElementIdNamespace::kScroll);
}

// Convert the blink concept of a ScrollTimeline orientation into the cc one.
//
// The compositor does not know about writing modes, so we have to convert the
// web concepts of 'block' and 'inline' direction into absolute vertical or
// horizontal directions.
//
// TODO(smcgruer): If the writing mode of a scroller changes, we have to update
// any related cc::ScrollTimeline somehow.
CompositorScrollTimeline::ScrollDirection ConvertOrientation(
    ScrollTimeline::ScrollDirection orientation,
    bool is_horizontal_writing_mode) {
  switch (orientation) {
    case ScrollTimeline::Block:
      return is_horizontal_writing_mode ? CompositorScrollTimeline::Vertical
                                        : CompositorScrollTimeline::Horizontal;
    case ScrollTimeline::Inline:
      return is_horizontal_writing_mode ? CompositorScrollTimeline::Horizontal
                                        : CompositorScrollTimeline::Vertical;
    case ScrollTimeline::Horizontal:
      return CompositorScrollTimeline::Horizontal;
    case ScrollTimeline::Vertical:
      return CompositorScrollTimeline::Vertical;
    default:
      NOTREACHED();
      return CompositorScrollTimeline::Vertical;
  }
}

// Converts a blink::ScrollTimeline into a cc::ScrollTimeline.
//
// If the timeline cannot be converted, returns nullptr.
std::unique_ptr<CompositorScrollTimeline> ToCompositorScrollTimeline(
    AnimationTimeline* timeline) {
  if (!timeline || timeline->IsDocumentTimeline())
    return nullptr;

  ScrollTimeline* scroll_timeline = ToScrollTimeline(timeline);
  Node* scroll_source = scroll_timeline->ResolvedScrollSource();
  base::Optional<CompositorElementId> element_id =
      GetCompositorScrollElementId(*scroll_source);

  DoubleOrScrollTimelineAutoKeyword time_range;
  scroll_timeline->timeRange(time_range);
  // TODO(smcgruer): Handle 'auto' time range value.
  DCHECK(time_range.IsDouble());

  // TODO(smcgruer): If the scroll source later gets a LayoutBox (e.g. was
  // display:none and now isn't), we need to update the compositor to have the
  // correct orientation and start/end offset information.
  LayoutBox* box = scroll_source->GetLayoutBox();

  CompositorScrollTimeline::ScrollDirection orientation =
      ConvertOrientation(scroll_timeline->GetOrientation(),
                         box ? box->IsHorizontalWritingMode() : true);

  base::Optional<double> start_scroll_offset;
  base::Optional<double> end_scroll_offset;
  if (box) {
    double current_offset;
    double max_offset;
    scroll_timeline->GetCurrentAndMaxOffset(box, current_offset, max_offset);

    double resolved_start_scroll_offset = 0;
    double resolved_end_scroll_offset = max_offset;
    scroll_timeline->ResolveScrollStartAndEnd(box, max_offset,
                                              resolved_start_scroll_offset,
                                              resolved_end_scroll_offset);
    start_scroll_offset = resolved_start_scroll_offset;
    end_scroll_offset = resolved_end_scroll_offset;
  }

  return std::make_unique<CompositorScrollTimeline>(
      element_id, orientation, start_scroll_offset, end_scroll_offset,
      time_range.GetAsDouble());
}

void StartEffectOnCompositor(CompositorAnimation* animation,
                             KeyframeEffect* effect) {
  DCHECK(effect);
  Element& target = *effect->target();
  effect->Model()->SnapshotAllCompositorKeyframesIfNecessary(
      target, target.ComputedStyleRef(), target.ParentComputedStyle());

  int group = 0;
  base::Optional<double> start_time = base::nullopt;
  double time_offset = 0;
  double playback_rate = 1;

  effect->StartAnimationOnCompositor(group, start_time, time_offset,
                                     playback_rate, animation);
}

unsigned NextSequenceNumber() {
  // TODO(majidvp): This should actually come from the same source as other
  // animation so that they have the correct ordering.
  static unsigned next = 0;
  return ++next;
}
}  // namespace

WorkletAnimation* WorkletAnimation::Create(
    ScriptState* script_state,
    String animator_name,
    const AnimationEffectOrAnimationEffectSequence& effects,
    ExceptionState& exception_state) {
  return Create(script_state, animator_name, effects,
                DocumentTimelineOrScrollTimeline(), nullptr, exception_state);
}

WorkletAnimation* WorkletAnimation::Create(
    ScriptState* script_state,
    String animator_name,
    const AnimationEffectOrAnimationEffectSequence& effects,
    DocumentTimelineOrScrollTimeline timeline,
    ExceptionState& exception_state) {
  return Create(script_state, animator_name, effects, timeline, nullptr,
                exception_state);
}
WorkletAnimation* WorkletAnimation::Create(
    ScriptState* script_state,
    String animator_name,
    const AnimationEffectOrAnimationEffectSequence& effects,
    DocumentTimelineOrScrollTimeline timeline,
    scoped_refptr<SerializedScriptValue> options,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  HeapVector<Member<KeyframeEffect>> keyframe_effects;
  String error_string;
  if (!ConvertAnimationEffects(effects, keyframe_effects, error_string)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      error_string);
    return nullptr;
  }

  if (!ValidateTimeline(timeline, error_string)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      error_string);
    return nullptr;
  }

  AnimationWorklet* worklet =
      CSSAnimationWorklet::animationWorklet(script_state);

  WorkletAnimationId id = worklet->NextWorkletAnimationId();

  Document& document = keyframe_effects.at(0)->target()->GetDocument();
  AnimationTimeline* animation_timeline =
      ConvertAnimationTimeline(document, timeline);

  WorkletAnimation* animation =
      new WorkletAnimation(id, animator_name, document, keyframe_effects,
                           animation_timeline, std::move(options));

  return animation;
}

WorkletAnimation::WorkletAnimation(
    WorkletAnimationId id,
    const String& animator_name,
    Document& document,
    const HeapVector<Member<KeyframeEffect>>& effects,
    AnimationTimeline* timeline,
    scoped_refptr<SerializedScriptValue> options)
    : sequence_number_(NextSequenceNumber()),
      id_(id),
      animator_name_(animator_name),
      play_state_(Animation::kIdle),
      last_play_state_(play_state_),
      document_(document),
      effects_(effects),
      timeline_(timeline),
      options_(std::make_unique<WorkletAnimationOptions>(options)),
      effect_needs_restart_(false) {
  DCHECK(IsMainThread());

  for (auto& effect : effects_) {
    AnimationEffect* target_effect = effect;
    target_effect->Attach(this);
    local_times_.push_back(base::nullopt);
  }

  if (timeline_->IsScrollTimeline())
    ToScrollTimeline(timeline_)->AttachAnimation();
}

String WorkletAnimation::playState() {
  DCHECK(IsMainThread());
  return Animation::PlayStateString(play_state_);
}

void WorkletAnimation::play(ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  if (play_state_ == Animation::kPending)
    return;

  String failure_message;
  if (!CheckCanStart(&failure_message)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      failure_message);
    return;
  }

  document_->GetWorkletAnimationController().AttachAnimation(*this);
  SetPlayState(Animation::kPending);

  for (auto& effect : effects_) {
    Element* target = effect->target();
    DCHECK(target);
    // TODO(yigu): Currently we have to keep a set of worklet animations in
    // ElementAnimations so that the compositor knows that there are active
    // worklet animations running. Ideally, this should be done via the regular
    // Animation path, i.e., unify the logic between the two Animations.
    // https://crbug.com/896249.
    target->EnsureElementAnimations().GetWorkletAnimations().insert(this);
    target->SetNeedsAnimationStyleRecalc();
  }
}

void WorkletAnimation::cancel() {
  DCHECK(IsMainThread());
  if (play_state_ == Animation::kIdle)
    return;
  document_->GetWorkletAnimationController().DetachAnimation(*this);

  if (compositor_animation_) {
    GetEffect()->CancelAnimationOnCompositor(compositor_animation_.get());
    DestroyCompositorAnimation();
  }

  local_times_.Fill(base::nullopt);
  start_time_ = base::nullopt;
  running_on_main_thread_ = false;
  // TODO(yigu): Because this animation has been detached and will not receive
  // updates anymore, we have to update its value upon cancel. Similar to
  // regular animations, we should not detach them immediately and update the
  // value in the next frame. See https://crbug.com/883312.
  if (IsActive(play_state_)) {
    for (auto& effect : effects_)
      effect->UpdateInheritedTime(NullValue(), kTimingUpdateOnDemand);
  }
  SetPlayState(Animation::kIdle);

  for (auto& effect : effects_) {
    Element* target = effect->target();
    DCHECK(target);
    // TODO(yigu): Currently we have to keep a set of worklet animations in
    // ElementAnimations so that the compositor knows that there are active
    // worklet animations running. Ideally, this should be done via the regular
    // Animation path, i.e., unify the logic between the two Animations.
    // https://crbug.com/896249.
    target->EnsureElementAnimations().GetWorkletAnimations().erase(this);
    target->SetNeedsAnimationStyleRecalc();
  }
}

bool WorkletAnimation::Playing() const {
  return play_state_ == Animation::kRunning;
}

void WorkletAnimation::UpdateIfNecessary() {
  // TODO(crbug.com/833846): This is updating more often than necessary. This
  // gets fixed once WorkletAnimation becomes a subclass of Animation.
  Update(kTimingUpdateOnDemand);
}

void WorkletAnimation::EffectInvalidated() {
  InvalidateCompositingState();
}

void WorkletAnimation::Update(TimingUpdateReason reason) {
  if (play_state_ != Animation::kRunning)
    return;

  if (!start_time_)
    return;

  DCHECK_EQ(effects_.size(), local_times_.size());
  for (size_t i = 0; i < effects_.size(); ++i) {
    effects_[i]->UpdateInheritedTime(
        local_times_[i] ? local_times_[i]->InSecondsF() : NullValue(), reason);
  }
}

bool WorkletAnimation::CheckCanStart(String* failure_message) {
  DCHECK(IsMainThread());

  for (auto& effect : effects_) {
    if (effect->Model()->HasFrames())
      continue;
    *failure_message = "Animation effect has no keyframes";
    return false;
  }

  return true;
}

void WorkletAnimation::SetStartTimeToNow() {
  DCHECK(!start_time_);
  bool is_null;
  double time = timeline_->currentTime(is_null);
  if (!is_null)
    start_time_ = base::TimeDelta::FromSecondsD(time);
}

void WorkletAnimation::UpdateCompositingState() {
  DCHECK(play_state_ != Animation::kIdle && play_state_ != Animation::kUnset);

  if (play_state_ == Animation::kPending) {
#if DCHECK_IS_ON()
    String warning_message;
    DCHECK(CheckCanStart(&warning_message));
    DCHECK(warning_message.IsEmpty());
#endif  // DCHECK_IS_ON()
    if (StartOnCompositor())
      return;
    StartOnMain();
  } else if (play_state_ == Animation::kRunning) {
    // TODO(majidvp): If keyframes have changed then it may be possible to now
    // run the animation on compositor. The current logic does not allow this
    // switch from main to compositor to happen.
    if (!running_on_main_thread_)
      UpdateOnCompositor();
  }
  DCHECK(running_on_main_thread_ != !!compositor_animation_)
      << "Active worklet animation should either run on main or compositor";
}

void WorkletAnimation::InvalidateCompositingState() {
  effect_needs_restart_ = true;
  document_->GetWorkletAnimationController().InvalidateAnimation(*this);
}

void WorkletAnimation::StartOnMain() {
  running_on_main_thread_ = true;
  SetStartTimeToNow();
  SetPlayState(Animation::kRunning);
}

bool WorkletAnimation::StartOnCompositor() {
  DCHECK(IsMainThread());
  if (effects_.size() > 1) {
    // Compositor doesn't support multiple effects but they can be run via main.
    return false;
  }

  Element& target = *GetEffect()->target();

  // TODO(crbug.com/836393): This should not be possible but it is currently
  // happening and needs to be investigated/fixed.
  if (!target.GetComputedStyle())
    return false;
  // CheckCanStartAnimationOnCompositor requires that the property-specific
  // keyframe groups have been created. To ensure this we manually snapshot the
  // frames in the target effect.
  // TODO(smcgruer): This shouldn't be necessary - Animation doesn't do this.
  GetEffect()->Model()->SnapshotAllCompositorKeyframesIfNecessary(
      target, target.ComputedStyleRef(), target.ParentComputedStyle());

  double playback_rate = 1;
  CompositorAnimations::FailureCode failure_code =
      GetEffect()->CheckCanStartAnimationOnCompositor(
          base::Optional<CompositorElementIdSet>(), playback_rate);

  if (!failure_code.Ok()) {
    SetPlayState(Animation::kIdle);
    return false;
  }

  if (!CheckElementComposited(target))
    return false;

  if (!compositor_animation_) {
    compositor_animation_ = CompositorAnimation::CreateWorkletAnimation(
        id_, animator_name_, ToCompositorScrollTimeline(timeline_),
        std::move(options_));
    compositor_animation_->SetAnimationDelegate(this);
  }

  // Register ourselves on the compositor timeline. This will cause our cc-side
  // animation animation to be registered.
  if (CompositorAnimationTimeline* compositor_timeline =
          document_->Timeline().CompositorTimeline())
    compositor_timeline->AnimationAttached(*this);

  CompositorAnimations::AttachCompositedLayers(target,
                                               compositor_animation_.get());

  // TODO(smcgruer): We need to start all of the effects, not just the first.
  StartEffectOnCompositor(compositor_animation_.get(), GetEffect());
  SetPlayState(Animation::kRunning);

  bool is_null;
  double time = timeline_->currentTime(is_null);
  if (!is_null)
    start_time_ = base::TimeDelta::FromSecondsD(time);

  return true;
}

void WorkletAnimation::UpdateOnCompositor() {
  if (effect_needs_restart_) {
    // We want to update the keyframe effect on compositor animation without
    // destroying the compositor animation instance. This is achieved by
    // canceling, and start the blink keyframe effect on compositor.
    effect_needs_restart_ = false;
    GetEffect()->CancelAnimationOnCompositor(compositor_animation_.get());
    StartEffectOnCompositor(compositor_animation_.get(), GetEffect());
  }

  if (timeline_->IsScrollTimeline()) {
    Node* scroll_source = ToScrollTimeline(timeline_)->ResolvedScrollSource();
    LayoutBox* box = scroll_source->GetLayoutBox();

    base::Optional<double> start_scroll_offset;
    base::Optional<double> end_scroll_offset;
    if (box) {
      double current_offset;
      double max_offset;
      ToScrollTimeline(timeline_)->GetCurrentAndMaxOffset(box, current_offset,
                                                          max_offset);

      double resolved_start_scroll_offset = 0;
      double resolved_end_scroll_offset = max_offset;
      ToScrollTimeline(timeline_)->ResolveScrollStartAndEnd(
          box, max_offset, resolved_start_scroll_offset,
          resolved_end_scroll_offset);
      start_scroll_offset = resolved_start_scroll_offset;
      end_scroll_offset = resolved_end_scroll_offset;
    }
    compositor_animation_->UpdateScrollTimeline(
        GetCompositorScrollElementId(*scroll_source), start_scroll_offset,
        end_scroll_offset);
  }
}

void WorkletAnimation::DestroyCompositorAnimation() {
  if (compositor_animation_ && compositor_animation_->IsElementAttached())
    compositor_animation_->DetachElement();

  if (CompositorAnimationTimeline* compositor_timeline =
          document_->Timeline().CompositorTimeline())
    compositor_timeline->AnimationDestroyed(*this);

  if (compositor_animation_) {
    compositor_animation_->SetAnimationDelegate(nullptr);
    compositor_animation_ = nullptr;
  }
}

KeyframeEffect* WorkletAnimation::GetEffect() const {
  DCHECK(effects_.at(0));
  return effects_.at(0);
}

bool WorkletAnimation::IsActiveAnimation() const {
  return IsActive(play_state_);
}

void WorkletAnimation::UpdateInputState(
    AnimationWorkletDispatcherInput* input_state) {
  if (!running_on_main_thread_) {
    input_state->Peek(id_);
    return;
  }

  bool was_active = IsActive(last_play_state_);
  bool is_active = IsActive(play_state_);

  DCHECK(start_time_);
  DCHECK(last_current_time_ || !was_active);
  bool is_null;
  double current_time = timeline_->currentTime(is_null);

  bool did_time_change =
      !last_current_time_ || current_time != last_current_time_->InSecondsF();
  last_current_time_ = base::TimeDelta::FromSecondsD(current_time);

  if (!was_active && is_active) {
    input_state->Add(
        {id_,
         std::string(animator_name_.Ascii().data(), animator_name_.length()),
         current_time, CloneOptions(), effects_.size()});
  } else if (was_active && is_active) {
    // Skip if the input time is not changed.
    if (did_time_change)
      input_state->Update({id_, current_time});
  } else if (was_active && !is_active) {
    input_state->Remove(id_);
  }
  last_play_state_ = play_state_;
}

void WorkletAnimation::SetOutputState(
    const AnimationWorkletOutput::AnimationState& state) {
  DCHECK(state.worklet_animation_id == id_);
  // The local times for composited effects, i.e. not running on main, are
  // peeked and set via the main thread. If an animator is not ready upon
  // peeking state.local_times will be empty.
  DCHECK(local_times_.size() == state.local_times.size() ||
         !running_on_main_thread_);
  for (size_t i = 0; i < state.local_times.size(); ++i)
    local_times_[i] = state.local_times[i];
}

void WorkletAnimation::Dispose() {
  DCHECK(IsMainThread());
  if (timeline_->IsScrollTimeline())
    ToScrollTimeline(timeline_)->DetachAnimation();
  DestroyCompositorAnimation();
}

void WorkletAnimation::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
  visitor->Trace(effects_);
  visitor->Trace(timeline_);
  WorkletAnimationBase::Trace(visitor);
}

}  // namespace blink
