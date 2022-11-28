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

#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"

#include <limits>
#include <utility>

#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/animation/compositor_animations.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_equality.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

PropertyHandleSet KeyframeEffectModelBase::Properties() const {
  PropertyHandleSet result;
  for (const auto& keyframe : keyframes_) {
    for (const auto& property : keyframe->Properties())
      result.insert(property);
  }
  return result;
}

template <class K>
void KeyframeEffectModelBase::SetFrames(HeapVector<K>& keyframes) {
  // TODO(samli): Should also notify/invalidate the animation
  keyframes_.clear();
  keyframes_.AppendVector(keyframes);
  ClearCachedData();
}

template CORE_EXPORT void KeyframeEffectModelBase::SetFrames(
    HeapVector<Member<Keyframe>>& keyframes);
template CORE_EXPORT void KeyframeEffectModelBase::SetFrames(
    HeapVector<Member<StringKeyframe>>& keyframes);

void KeyframeEffectModelBase::SetComposite(CompositeOperation composite) {
  composite_ = composite;
  ClearCachedData();
}

bool KeyframeEffectModelBase::Sample(
    int iteration,
    double fraction,
    AnimationTimeDelta iteration_duration,
    HeapVector<Member<Interpolation>>& result) const {
  DCHECK_GE(iteration, 0);
  EnsureKeyframeGroups();
  EnsureInterpolationEffectPopulated();

  bool changed = iteration != last_iteration_ || fraction != last_fraction_ ||
                 iteration_duration != last_iteration_duration_;
  last_iteration_ = iteration;
  last_fraction_ = fraction;
  last_iteration_duration_ = iteration_duration;
  interpolation_effect_->GetActiveInterpolations(fraction, result);
  return changed;
}

namespace {

static const size_t num_compositable_properties = 9;

const CSSProperty** CompositableProperties() {
  static const CSSProperty*
      kCompositableProperties[num_compositable_properties] = {
          &GetCSSPropertyOpacity(),        &GetCSSPropertyRotate(),
          &GetCSSPropertyScale(),          &GetCSSPropertyTransform(),
          &GetCSSPropertyTranslate(),      &GetCSSPropertyFilter(),
          &GetCSSPropertyBackdropFilter(), &GetCSSPropertyBackgroundColor(),
          &GetCSSPropertyClipPath()};
  return kCompositableProperties;
}

}  // namespace

bool KeyframeEffectModelBase::SnapshotNeutralCompositorKeyframes(
    Element& element,
    const ComputedStyle& old_style,
    const ComputedStyle& new_style,
    const ComputedStyle* parent_style) const {
  auto should_snapshot_property = [&old_style,
                                   &new_style](const PropertyHandle& property) {
    return !CSSPropertyEquality::PropertiesEqual(property, old_style,
                                                 new_style);
  };
  auto should_snapshot_keyframe = [](const PropertySpecificKeyframe& keyframe) {
    return keyframe.IsNeutral();
  };

  return SnapshotCompositableProperties(element, new_style, parent_style,
                                        should_snapshot_property,
                                        should_snapshot_keyframe);
}

bool KeyframeEffectModelBase::SnapshotAllCompositorKeyframesIfNecessary(
    Element& element,
    const ComputedStyle& base_style,
    const ComputedStyle* parent_style) const {
  if (!needs_compositor_keyframes_snapshot_)
    return false;
  needs_compositor_keyframes_snapshot_ = false;

  bool has_neutral_compositable_keyframe = false;
  auto should_snapshot_property = [](const PropertyHandle& property) {
    return true;
  };
  auto should_snapshot_keyframe =
      [&has_neutral_compositable_keyframe](
          const PropertySpecificKeyframe& keyframe) {
        has_neutral_compositable_keyframe |= keyframe.IsNeutral();
        return true;
      };

  bool updated = SnapshotCompositableProperties(
      element, base_style, parent_style, should_snapshot_property,
      should_snapshot_keyframe);

  if (updated && has_neutral_compositable_keyframe) {
    UseCounter::Count(element.GetDocument(),
                      WebFeature::kSyntheticKeyframesInCompositedCSSAnimation);
  }
  return updated;
}

bool KeyframeEffectModelBase::SnapshotCompositableProperties(
    Element& element,
    const ComputedStyle& computed_style,
    const ComputedStyle* parent_style,
    ShouldSnapshotPropertyFunction should_snapshot_property,
    ShouldSnapshotKeyframeFunction should_snapshot_keyframe) const {
  EnsureKeyframeGroups();
  bool updated = false;
  static const CSSProperty** compositable_properties = CompositableProperties();
  for (size_t i = 0; i < num_compositable_properties; i++) {
    updated |= SnapshotCompositorKeyFrames(
        PropertyHandle(*compositable_properties[i]), element, computed_style,
        parent_style, should_snapshot_property, should_snapshot_keyframe);
  }

  // Custom properties need to be handled separately, since not all values
  // can be animated.  Need to resolve the value of each custom property to
  // ensure that it can be animated.
  const PropertyRegistry* property_registry =
      element.GetDocument().GetPropertyRegistry();
  if (!property_registry)
    return updated;

  for (const AtomicString& name : computed_style.GetVariableNames()) {
    if (property_registry->WasReferenced(name)) {
      // This variable has been referenced as a property value at least once
      // during style resolution in the document. Animating this property on
      // the compositor could introduce misalignment in frame synchronization.
      //
      // TODO(kevers): For non-inherited properites, check if referenced in
      // computed style. References elsewhere in the document should not prevent
      // compositing.
      continue;
    }
    updated |= SnapshotCompositorKeyFrames(
        PropertyHandle(name), element, computed_style, parent_style,
        should_snapshot_property, should_snapshot_keyframe);
  }
  return updated;
}

bool KeyframeEffectModelBase::SnapshotCompositorKeyFrames(
    const PropertyHandle& property,
    Element& element,
    const ComputedStyle& computed_style,
    const ComputedStyle* parent_style,
    ShouldSnapshotPropertyFunction should_snapshot_property,
    ShouldSnapshotKeyframeFunction should_snapshot_keyframe) const {
  if (!should_snapshot_property(property))
    return false;

  auto it = keyframe_groups_->find(property);
  if (it == keyframe_groups_->end())
    return false;

  PropertySpecificKeyframeGroup* keyframe_group = it->value;

  bool updated = false;
  for (auto& keyframe : keyframe_group->keyframes_) {
    if (!should_snapshot_keyframe(*keyframe))
      continue;

    updated |= keyframe->PopulateCompositorKeyframeValue(
        property, element, computed_style, parent_style);
  }
  return updated;
}

template <class K>
Vector<double> KeyframeEffectModelBase::GetComputedOffsets(
    const HeapVector<K>& keyframes) {
  // To avoid having to create two vectors when converting from the nullable
  // offsets to the non-nullable computed offsets, we keep the convention in
  // this function that std::numeric_limits::quiet_NaN() represents null.
  double last_offset = 0;
  Vector<double> result;
  result.reserve(keyframes.size());

  for (const auto& keyframe : keyframes) {
    absl::optional<double> offset = keyframe->Offset();
    if (offset) {
      DCHECK_GE(offset.value(), 0);
      DCHECK_LE(offset.value(), 1);
      DCHECK_GE(offset.value(), last_offset);
      last_offset = offset.value();
    }
    result.push_back(offset.value_or(std::numeric_limits<double>::quiet_NaN()));
  }

  if (result.empty())
    return result;

  if (std::isnan(result.back()))
    result.back() = 1;

  if (result.size() > 1 && std::isnan(result[0])) {
    result.front() = 0;
  }

  wtf_size_t last_index = 0;
  last_offset = result.front();
  for (wtf_size_t i = 1; i < result.size(); ++i) {
    double offset = result[i];
    if (!std::isnan(offset)) {
      for (wtf_size_t j = 1; j < i - last_index; ++j) {
        result[last_index + j] =
            last_offset + (offset - last_offset) * j / (i - last_index);
      }
      last_index = i;
      last_offset = offset;
    }
  }

  return result;
}

template CORE_EXPORT Vector<double> KeyframeEffectModelBase::GetComputedOffsets(
    const HeapVector<Member<Keyframe>>& keyframes);
template CORE_EXPORT Vector<double> KeyframeEffectModelBase::GetComputedOffsets(
    const HeapVector<Member<StringKeyframe>>& keyframes);

bool KeyframeEffectModelBase::IsTransformRelatedEffect() const {
  return Affects(PropertyHandle(GetCSSPropertyTransform())) ||
         Affects(PropertyHandle(GetCSSPropertyRotate())) ||
         Affects(PropertyHandle(GetCSSPropertyScale())) ||
         Affects(PropertyHandle(GetCSSPropertyTranslate()));
}

bool KeyframeEffectModelBase::SetLogicalPropertyResolutionContext(
    TextDirection text_direction,
    WritingMode writing_mode) {
  bool changed = false;
  for (wtf_size_t i = 0; i < keyframes_.size(); i++) {
    if (auto* string_keyframe = DynamicTo<StringKeyframe>(*keyframes_[i])) {
      if (string_keyframe->HasLogicalProperty()) {
        string_keyframe->SetLogicalPropertyResolutionContext(text_direction,
                                                             writing_mode);
        changed = true;
      }
    }
  }
  if (changed)
    ClearCachedData();
  return changed;
}

void KeyframeEffectModelBase::Trace(Visitor* visitor) const {
  visitor->Trace(keyframes_);
  visitor->Trace(keyframe_groups_);
  visitor->Trace(interpolation_effect_);
  EffectModel::Trace(visitor);
}

void KeyframeEffectModelBase::EnsureKeyframeGroups() const {
  if (keyframe_groups_)
    return;

  keyframe_groups_ = MakeGarbageCollected<KeyframeGroupMap>();
  scoped_refptr<TimingFunction> zero_offset_easing = default_keyframe_easing_;
  Vector<double> computed_offsets = GetComputedOffsets(keyframes_);
  DCHECK_EQ(computed_offsets.size(), keyframes_.size());
  for (wtf_size_t i = 0; i < keyframes_.size(); i++) {
    double computed_offset = computed_offsets[i];
    const auto& keyframe = keyframes_[i];

    if (computed_offset == 0)
      zero_offset_easing = &keyframe->Easing();

    for (const PropertyHandle& property : keyframe->Properties()) {
      Member<PropertySpecificKeyframeGroup>& group =
          keyframe_groups_->insert(property, nullptr).stored_value->value;
      if (!group)
        group = MakeGarbageCollected<PropertySpecificKeyframeGroup>();

      Keyframe::PropertySpecificKeyframe* property_specific_keyframe =
          keyframe->CreatePropertySpecificKeyframe(property, composite_,
                                                   computed_offset);
      has_revert_ |= property_specific_keyframe->IsRevert();
      has_revert_ |= property_specific_keyframe->IsRevertLayer();
      group->AppendKeyframe(property_specific_keyframe);
    }
  }

  // Add synthetic keyframes.
  has_synthetic_keyframes_ = false;
  for (const auto& entry : *keyframe_groups_) {
    if (entry.value->AddSyntheticKeyframeIfRequired(zero_offset_easing))
      has_synthetic_keyframes_ = true;

    entry.value->RemoveRedundantKeyframes();
  }
}

bool KeyframeEffectModelBase::RequiresPropertyNode() const {
  for (const auto& keyframe : keyframes_) {
    for (const auto& property : keyframe->Properties()) {
      if (!property.IsCSSProperty() ||
          (property.GetCSSProperty().PropertyID() != CSSPropertyID::kVariable &&
           property.GetCSSProperty().PropertyID() !=
               CSSPropertyID::kBackgroundColor &&
           property.GetCSSProperty().PropertyID() != CSSPropertyID::kClipPath))
        return true;
    }
  }
  return false;
}

void KeyframeEffectModelBase::EnsureInterpolationEffectPopulated() const {
  if (interpolation_effect_->IsPopulated())
    return;

  for (const auto& entry : *keyframe_groups_) {
    const PropertySpecificKeyframeVector& keyframes = entry.value->Keyframes();
    for (wtf_size_t i = 0; i < keyframes.size() - 1; i++) {
      wtf_size_t start_index = i;
      wtf_size_t end_index = i + 1;
      double start_offset = keyframes[start_index]->Offset();
      double end_offset = keyframes[end_index]->Offset();
      double apply_from = start_offset;
      double apply_to = end_offset;

      if (i == 0) {
        apply_from = -std::numeric_limits<double>::infinity();
        DCHECK_EQ(start_offset, 0.0);
        if (end_offset == 0.0) {
          DCHECK_NE(keyframes[end_index + 1]->Offset(), 0.0);
          end_index = start_index;
        }
      }
      if (i == keyframes.size() - 2) {
        apply_to = std::numeric_limits<double>::infinity();
        DCHECK_EQ(end_offset, 1.0);
        if (start_offset == 1.0) {
          DCHECK_NE(keyframes[start_index - 1]->Offset(), 1.0);
          start_index = end_index;
        }
      }

      if (apply_from != apply_to) {
        interpolation_effect_->AddInterpolationsFromKeyframes(
            entry.key, *keyframes[start_index], *keyframes[end_index],
            apply_from, apply_to);
      }
      // else the interpolation will never be used in sampling
    }
  }

  interpolation_effect_->SetPopulated();
}

void KeyframeEffectModelBase::ClearCachedData() {
  keyframe_groups_ = nullptr;
  interpolation_effect_->Clear();
  last_fraction_ = std::numeric_limits<double>::quiet_NaN();
  needs_compositor_keyframes_snapshot_ = true;
}

bool KeyframeEffectModelBase::IsReplaceOnly() const {
  EnsureKeyframeGroups();
  for (const auto& entry : *keyframe_groups_) {
    for (const auto& keyframe : entry.value->Keyframes()) {
      if (keyframe->Composite() != EffectModel::kCompositeReplace)
        return false;
    }
  }
  return true;
}

void KeyframeEffectModelBase::PropertySpecificKeyframeGroup::AppendKeyframe(
    Keyframe::PropertySpecificKeyframe* keyframe) {
  DCHECK(keyframes_.empty() ||
         keyframes_.back()->Offset() <= keyframe->Offset());
  keyframes_.push_back(std::move(keyframe));
}

void KeyframeEffectModelBase::PropertySpecificKeyframeGroup::
    RemoveRedundantKeyframes() {
  // As an optimization, removes interior keyframes that have the same offset
  // as both their neighbours, as they will never be used by sample().
  // Note that synthetic keyframes must be added before this method is
  // called.
  DCHECK_GE(keyframes_.size(), 2U);
  for (int i = keyframes_.size() - 2; i > 0; --i) {
    double offset = keyframes_[i]->Offset();
    bool has_same_offset_as_previous_neighbor =
        keyframes_[i - 1]->Offset() == offset;
    bool has_same_offset_as_next_neighbor =
        keyframes_[i + 1]->Offset() == offset;
    if (has_same_offset_as_previous_neighbor &&
        has_same_offset_as_next_neighbor)
      keyframes_.EraseAt(i);
  }
  DCHECK_GE(keyframes_.size(), 2U);
}

bool KeyframeEffectModelBase::PropertySpecificKeyframeGroup::
    AddSyntheticKeyframeIfRequired(
        scoped_refptr<TimingFunction> zero_offset_easing) {
  DCHECK(!keyframes_.empty());

  bool added_synthetic_keyframe = false;

  if (keyframes_.front()->Offset() != 0.0) {
    keyframes_.insert(0, keyframes_.front()->NeutralKeyframe(
                             0, std::move(zero_offset_easing)));
    added_synthetic_keyframe = true;
  }
  if (keyframes_.back()->Offset() != 1.0) {
    AppendKeyframe(keyframes_.back()->NeutralKeyframe(1, nullptr));
    added_synthetic_keyframe = true;
  }

  return added_synthetic_keyframe;
}

}  // namespace blink
