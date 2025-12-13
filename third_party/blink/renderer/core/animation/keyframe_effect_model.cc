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
#include "third_party/blink/renderer/core/animation/keyframe.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_equality.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

namespace {

// Conditionally extracts a length valued property as an integer multiple
// of the quantization limit for layout-units.
std::optional<int> GetQuantizedLength(const CSSValue& value) {
  std::optional<int> result;
  const CSSPrimitiveValue* primitive = DynamicTo<CSSPrimitiveValue>(&value);
  if (!primitive) {
    return result;
  }
  if (!primitive->IsPx()) {
    return result;
  }
  std::optional<double> primitive_value = primitive->GetValueIfKnown();
  if (!primitive_value) {
    return result;
  }

  result = static_cast<int>(
      std::round(LayoutUnit::kFixedPointDenominator * primitive_value.value()));
  return result;
}

// Determine if a property value from the base computed style is equivalent to
// the target value. Special provision for matching lengths the handle
// quantization when converting to and from layout units.
bool AreEquivalent(const CSSValue& underlying_value,
                   const CSSValue& target_value) {
  if (underlying_value == target_value) {
    return true;
  }

  std::optional<int> target_length = GetQuantizedLength(target_value);
  if (!target_length) {
    return false;
  }

  std::optional<int> underlying_length = GetQuantizedLength(underlying_value);
  return target_length == underlying_length;
}

}  // end namespace

KeyframeEffectModelBase::KeyframeProperties::Iterator&
KeyframeEffectModelBase::KeyframeProperties::Iterator::operator++() {
  if (++(*current_property_) == keyframe_properties_->end()) {
    keyframes_.take_first_elem();
    AdvanceToNextKeyframeWithProperties();
  }
  return *this;
}

void KeyframeEffectModelBase::KeyframeProperties::Iterator::
    AdvanceToNextKeyframeWithProperties() {
  keyframe_properties_ = nullptr;
  current_property_.reset();
  while (!keyframes_.empty()) {
    const Keyframe* current_keyframe = keyframes_.front();
    if (!current_keyframe->HasComputedOffset()) {
      // Keyframe is not reachable. This case occurs when we have a timeline
      // offset in the keyframe but are not using a view timeline and thus the
      // offset cannot be resolved.
      keyframes_.take_first_elem();
      continue;
    }
    keyframe_properties_ = &current_keyframe->Properties();
    current_property_ = keyframe_properties_->begin();
    if (current_property_ != keyframe_properties_->end()) {
      return;
    }
    keyframes_.take_first_elem();
  }
}

PropertyHandleSet
KeyframeEffectModelBase::KeyframeProperties::UniqueProperties() const {
  PropertyHandleSet properties;
  for (const auto property : *this) {
    properties.insert(property);
  }
  return properties;
}

void KeyframeEffectModelBase::IterableDynamicProperties::Iterator::
    AdvanceToNextGroup() {
  while (current_keyframe_group_ != model_->keyframe_groups_->end() &&
         current_keyframe_group_->value->IsStaticMaybeDowngradeProvisional(
             current_keyframe_group_->key, element_)) {
    current_keyframe_group_++;
  }
}

bool KeyframeEffectModelBase::IterableDynamicProperties::Contains(
    const PropertyHandle& property) const {
  auto iter = model_->keyframe_groups_->find(property);
  if (iter == model_->keyframe_groups_->end()) {
    return false;
  }
  if (iter->value->IsStaticMaybeDowngradeProvisional(property, element_)) {
    return false;
  }
  return true;
}

KeyframeEffectModelBase::KeyframeProperties
KeyframeEffectModelBase::Properties() const {
  return KeyframeProperties(this);
}

KeyframeEffectModelBase::IterableDynamicProperties
KeyframeEffectModelBase::DynamicProperties(const Element* element) const {
  EnsureKeyframeGroups();
  return IterableDynamicProperties(this, element);
}

bool KeyframeEffectModelBase::HasStaticProperty() const {
  EnsureKeyframeGroups();
  for (const auto& entry : *keyframe_groups_) {
    if (entry.value->IsCurrentlyStatic()) {
      return true;
    }
  }
  return false;
}

template <class K>
void KeyframeEffectModelBase::SetFrames(HeapVector<K>& keyframes) {
  // TODO(samli): Should also notify/invalidate the animation
  keyframes_.clear();
  keyframes_.AppendVector(keyframes);
  IndexKeyframesAndResolveComputedOffsets();
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

void KeyframeEffectModelBase::SetIterationComposite(
    IterationCompositeOperation iteration_composite) {
  iteration_composite_ = iteration_composite;
  ClearCachedData();
}

bool KeyframeEffectModelBase::Sample(
    int iteration,
    double fraction,
    TimingFunction::LimitDirection limit_direction,
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
  interpolation_effect_->GetActiveInterpolations(
      iteration, fraction, iteration_composite_, limit_direction, result);
  return changed;
}

namespace {

using CompositablePropertiesArray = std::array<const CSSProperty*, 9>;

const CompositablePropertiesArray& CompositableProperties() {
  static const CompositablePropertiesArray kCompositableProperties{
      &GetCSSPropertyOpacity(),        &GetCSSPropertyRotate(),
      &GetCSSPropertyScale(),          &GetCSSPropertyTransform(),
      &GetCSSPropertyTranslate(),      &GetCSSPropertyFilter(),
      &GetCSSPropertyBackdropFilter(), &GetCSSPropertyBackgroundColor(),
      &GetCSSPropertyClipPath()};
  return kCompositableProperties;
}

enum class OffsetType {
  // Specified percentage offsets, e.g.
  // elem.animate([{offset: 0, ...}, {offset: "50%", ...}], {});
  kSpecified,

  // Specified offset calculations, e.g.
  // elem.animate([{offset: "calc(10px)", ...}, {offset: "exit 50%", ...}], {});
  kTimeline,

  // Programmatic keyframes with missing offsets, e.g.
  // elem.animate([{... /* no offset */}, {... /* no offset */}], {});
  kComputed
};

}  // namespace

bool KeyframeEffectModelBase::SnapshotNeutralCompositorKeyframes(
    Element& element,
    const ComputedStyle& old_style,
    const ComputedStyle& new_style,
    const ComputedStyle* parent_style) const {
  auto should_snapshot_property = [&old_style,
                                   &new_style](const PropertyHandle& property) {
    return !CSSPropertyEquality::PropertiesEqual(property, old_style,
                                                 new_style) &&
           CompositorAnimations::CompositedPropertyRequiresSnapshot(property);
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
    return CompositorAnimations::CompositedPropertyRequiresSnapshot(property);
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
  for (const auto* compositable_property : CompositableProperties()) {
    updated |= SnapshotCompositorKeyFrames(
        PropertyHandle(*compositable_property), element, computed_style,
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
  double last_offset = -std::numeric_limits<double>::max();
  Vector<double> result;
  Vector<OffsetType> offset_types;
  result.reserve(keyframes.size());
  offset_types.reserve(keyframes.size());

  for (const auto& keyframe : keyframes) {
    std::optional<double> offset = keyframe->Offset();
    if (offset && !keyframe->GetTimelineOffset()) {
      DCHECK_GE(offset.value(), last_offset);
      last_offset = offset.value();
    }
    result.push_back(offset.value_or(Keyframe::kNullComputedOffset));

    // A timeline offset must always have a valid range within the context of
    // a keyframe model. Otherwise, it is converted to a specified offset during
    // construction of the model.
    DCHECK(!keyframe->GetTimelineOffset() ||
           keyframe->GetTimelineOffset()->name !=
               TimelineOffset::NamedRange::kNone);

    OffsetType type = keyframe->GetTimelineOffset()
                          ? OffsetType::kTimeline
                          : (offset.has_value() ? OffsetType::kSpecified
                                                : OffsetType::kComputed);
    offset_types.push_back(type);
  }

  if (result.empty()) {
    return result;
  }

  // Ensure we have an offset at the upper bound of the range.
  for (int i = result.size() - 1; i >= 0; --i) {
    if (offset_types[i] == OffsetType::kSpecified) {
      break;
    }
    if (offset_types[i] == OffsetType::kComputed) {
      result[i] = 1;
      break;
    }
  }

  // Ensure we have an offset at the lower bound of the range.
  wtf_size_t last_index = 0;
  for (wtf_size_t i = 0; i < result.size(); ++i) {
    if (offset_types[i] == OffsetType::kSpecified) {
      last_offset = result[i];
      last_index = i;
      break;
    }
    if (offset_types[i] == OffsetType::kComputed && result[i] != 1) {
      last_offset = 0;
      last_index = i;
      result[i] = 0;
      break;
    }
  }

  if (last_offset < 0) {
    // All offsets are timeline offsets.
    return result;
  }

  wtf_size_t skipped_since_last_index = 0;
  for (wtf_size_t i = last_index + 1; i < result.size(); ++i) {
    double offset = result[i];
    // Keyframes with timeline offsets do not participate in the evaluation of
    // computed offsets.
    bool skipKeyframe = keyframes[i]->GetTimelineOffset().has_value();
    if (skipKeyframe) {
      skipped_since_last_index++;
    } else if (!std::isnan(offset)) {
      wtf_size_t skipped_during_fill = 0;
      for (wtf_size_t j = 1; j < i - last_index; ++j) {
        if (keyframes[last_index + j]->GetTimelineOffset().has_value()) {
          skipped_during_fill++;
          continue;
        }
        result[last_index + j] =
            last_offset + (offset - last_offset) * (j - skipped_during_fill) /
                              (i - last_index - skipped_since_last_index);
      }
      last_index = i;
      last_offset = offset;
      skipped_since_last_index = 0;
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
    WritingDirectionMode writing_direction) {
  bool changed = false;
  for (wtf_size_t i = 0; i < keyframes_.size(); i++) {
    if (auto* string_keyframe = DynamicTo<StringKeyframe>(*keyframes_[i])) {
      if (string_keyframe->HasLogicalProperty()) {
        string_keyframe->SetLogicalPropertyResolutionContext(writing_direction);
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
  if (keyframe_groups_) {
    return;
  }

  keyframe_groups_ = MakeGarbageCollected<KeyframeGroupMap>();
  scoped_refptr<TimingFunction> zero_offset_easing = default_keyframe_easing_;
  for (wtf_size_t i = 0; i < keyframes_.size(); i++) {
    const auto& keyframe = keyframes_[i];
    double computed_offset = keyframe->ComputedOffset().value();

    if (computed_offset == 0) {
      zero_offset_easing = &keyframe->Easing();
    }

    if (!std::isfinite(computed_offset)) {
      continue;
    }

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

  // Add synthetic keyframes and determine if the keyframe values are static.
  has_synthetic_keyframes_ = false;
  for (const auto& entry : *keyframe_groups_) {
    if (entry.value->AddSyntheticKeyframeIfRequired(zero_offset_easing))
      has_synthetic_keyframes_ = true;

    entry.value->RemoveRedundantKeyframes();
    entry.value->CheckIfStatic(*this);
  }
}

bool KeyframeEffectModelBase::RequiresPropertyNode() const {
  for (const auto& property : DynamicProperties()) {
    if (!property.IsCSSProperty() ||
        (property.GetCSSProperty().PropertyID() != CSSPropertyID::kVariable &&
         property.GetCSSProperty().PropertyID() !=
             CSSPropertyID::kBackgroundColor &&
         property.GetCSSProperty().PropertyID() != CSSPropertyID::kClipPath))
      return true;
  }
  return false;
}

void KeyframeEffectModelBase::EnsureInterpolationEffectPopulated() const {
  if (interpolation_effect_->IsPopulated())
    return;

  for (const auto& entry : *keyframe_groups_) {
    const PropertySpecificKeyframeVector& keyframes = entry.value->Keyframes();
    // Skip cross-fade interpolations in the static property optimization to
    // avoid introducing a side-effect in serialization of the computed value.
    // cross-fade(A 50%, A 50%) is visually equivalent to rendering A, but at
    // present, we expect the computed style to reflect an explicit
    // cross-fade.
    PropertyHandle handle = entry.key;
    if (entry.value->IsStrictlyStatic() && handle.IsCSSProperty() &&
        handle.GetCSSProperty().PropertyID() !=
            CSSPropertyID::kListStyleImage) {
      // All keyframes have the same property value.
      // Create an interpolation from starting keyframe to starting keyframe.
      // The resulting interpolation record will be marked as static and can
      // short-circuit the local fraction calculation.
      CHECK(keyframes.size());
      interpolation_effect_->AddStaticValuedInterpolation(entry.key,
                                                          *keyframes[0]);
      continue;
    }

    const Keyframe::PropertySpecificKeyframe* final_keyframe = keyframes.back();

    for (wtf_size_t i = 0; i < keyframes.size() - 1; i++) {
      wtf_size_t start_index = i;
      wtf_size_t end_index = i + 1;
      double start_offset = keyframes[start_index]->Offset();
      double end_offset = keyframes[end_index]->Offset();
      double apply_from = start_offset;
      double apply_to = end_offset;

      if (i == 0) {
        apply_from = -std::numeric_limits<double>::infinity();
        if (end_offset == 0.0) {
          DCHECK_NE(keyframes[end_index + 1]->Offset(), 0.0);
          end_index = start_index;
        }
      }
      if (i == keyframes.size() - 2) {
        apply_to = std::numeric_limits<double>::infinity();
        if (start_offset == 1.0) {
          DCHECK_NE(keyframes[start_index - 1]->Offset(), 1.0);
          start_index = end_index;
        }
      }

      if (apply_from != apply_to) {
        interpolation_effect_->AddInterpolationsFromKeyframes(
            entry.key, *keyframes[start_index], *keyframes[end_index],
            final_keyframe, apply_from, apply_to);
      }
      // else the interpolation will never be used in sampling
    }
  }

  interpolation_effect_->SetPopulated();
}

void KeyframeEffectModelBase::IndexKeyframesAndResolveComputedOffsets() {
  Vector<double> computed_offsets = GetComputedOffsets(keyframes_);
  // Snapshot the indices so that we can recover the original ordering.
  for (wtf_size_t i = 0; i < keyframes_.size(); i++) {
    keyframes_[i]->SetIndex(i);
    keyframes_[i]->SetComputedOffset(computed_offsets[i]);
  }
}

bool KeyframeEffectModelBase::ResolveTimelineOffsets(
    const TimelineRange& timeline_range,
    double range_start,
    double range_end) {
  if (timeline_range == last_timeline_range_ &&
      last_range_start_ == range_start && last_range_end_ == range_end) {
    return false;
  }

  bool needs_update = false;
  for (const auto& keyframe : keyframes_) {
    needs_update |=
        keyframe->ResolveTimelineOffset(timeline_range, range_start, range_end);
  }
  if (needs_update) {
    std::stable_sort(keyframes_.begin(), keyframes_.end(), &Keyframe::LessThan);
    ClearCachedData();
  }

  last_timeline_range_ = timeline_range;
  last_range_start_ = range_start;
  last_range_end_ = range_end;

  return needs_update;
}

void KeyframeEffectModelBase::ClearCachedData() {
  keyframe_groups_ = nullptr;
  interpolation_effect_->Clear();
  last_fraction_ = std::numeric_limits<double>::quiet_NaN();
  needs_compositor_keyframes_snapshot_ = true;

  last_timeline_range_ = std::nullopt;
  last_range_start_ = std::nullopt;
  last_range_end_ = std::nullopt;
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
  // as both their neighbors, as they will never be used by sample().
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

const CSSPropertySpecificKeyframe* KeyframeEffectModelBase::
    PropertySpecificKeyframeGroup::FirstCssKeyframeWithSetValue() const {
  for (const auto& keyframe : keyframes_) {
    const CSSPropertySpecificKeyframe* property_keyframe =
        To<CSSPropertySpecificKeyframe>(keyframe.Get());
    if (property_keyframe->Value()) {
      return property_keyframe;
    }
  }
  return nullptr;
}

bool KeyframeEffectModelBase::PropertySpecificKeyframeGroup::
    IsStaticMaybeDowngradeProvisional(const PropertyHandle& property,
                                      const Element* element) const {
  switch (static_check_result_) {
    case StaticCheckResult::kDynamic:
      return false;

    case StaticCheckResult::kStatic:
      return true;

    case StaticCheckResult::kProvisionalChecked:
    case StaticCheckResult::kProvisionalUnchecked: {
      // When an element is not provided for validating provisional properties,
      // assume it is static of previously verified and dynamic otherwise.
      if (!element) {
        return static_check_result_ == StaticCheckResult::kProvisionalChecked;
      }

      // When we have an element, revalidate even though previously matching
      // in case of a change to the underlying property value.
      // TOOD(kevers): Can likely determine in RecalcOwnStyle if
      // BaseComputedStyle has changed and only recheck if the underlying style
      // has changed.

      // Limit support for provisionally static properties to CSS properties.
      if (!property.IsCSSProperty()) {
        return false;
      }

      // Update status check to be either kDynamic or kProvisionalChecked.
      const ComputedStyle* style = element->GetComputedStyle();
      if (!style) {
        static_check_result_ = StaticCheckResult::kProvisionalUnchecked;
        return false;
      }
      const ComputedStyle* base_style = style->GetBaseComputedStyle();
      if (!base_style) {
        static_check_result_ = StaticCheckResult::kProvisionalUnchecked;
        return false;
      }

      const CSSPropertySpecificKeyframe* keyframe =
          FirstCssKeyframeWithSetValue();
      CHECK(keyframe);
      const CSSValue* target_value = keyframe->Value();
      CHECK(target_value);

      const CSSValue* underlying_value =
          ComputedStyleUtils::ComputedPropertyValue(property.GetCSSProperty(),
                                                    *base_style);
      if (!underlying_value ||
          !AreEquivalent(*underlying_value, *target_value)) {
        static_check_result_ = StaticCheckResult::kDynamic;
        return false;
      }

      static_check_result_ = StaticCheckResult::kProvisionalChecked;
      return true;
    }

    case StaticCheckResult::kUnset:
      NOTREACHED();
  }
}

void KeyframeEffectModelBase::PropertySpecificKeyframeGroup::CheckIfStatic(
    const KeyframeEffectModelBase& model) {
  static_check_result_ = StaticCheckResult::kStatic;

  // Properties cannot be static when using iterationComposite: accumulate
  // because their values will change across iterations.
  if (model.IterationComposite() ==
      IterationCompositeOperation::kIterationCompositeAccumulate) {
    static_check_result_ = StaticCheckResult::kDynamic;
    return;
  }

  // Transitions are only started if the end-points mismatch with caveat for
  // visited/unvisited properties. For now, limit to detected static properties
  // in a CSS animations since a common source of static properties is expansion
  // of shorthand properties to their longhand counterparts.
  DCHECK_GE(keyframes_.size(), 2U);
  if (!DynamicTo<CSSPropertySpecificKeyframe>(keyframes_[0].Get())) {
    static_check_result_ = StaticCheckResult::kDynamic;
    return;
  }

  const CSSPropertySpecificKeyframe* reference_keyframe =
      FirstCssKeyframeWithSetValue();

  const CSSValue* target_value = reference_keyframe->Value();
  CompositeOperation target_composite_operation =
      reference_keyframe->Composite();
  for (const auto& keyframe : keyframes_) {
    const CSSPropertySpecificKeyframe* css_keyframe =
        To<CSSPropertySpecificKeyframe>(keyframe.Get());
    if (css_keyframe == reference_keyframe) {
      // Skip checks on the reference keyframe.
      continue;
    }

    if (!css_keyframe->Value()) {
      // Found a neutral keyframe. These keyframes are provisionally static if
      // the underlying property value matches the value for specified
      // keyframes, which must in turn use composite replace.
      if (target_composite_operation != CompositeOperation::kCompositeReplace) {
        static_check_result_ = StaticCheckResult::kDynamic;
        return;
      }
      static_check_result_ = StaticCheckResult::kProvisionalUnchecked;
      continue;
    }

    if (css_keyframe->Composite() != target_composite_operation ||
        *css_keyframe->Value() != *target_value) {
      static_check_result_ = StaticCheckResult::kDynamic;
      return;
    }
  }
}

bool KeyframeEffectModelBase::PropertySpecificKeyframeGroup::
    AddSyntheticKeyframeIfRequired(
        scoped_refptr<TimingFunction> zero_offset_easing) {
  DCHECK(!keyframes_.empty());

  bool added_synthetic_keyframe = false;

  if (keyframes_.front()->Offset() > 0.0) {
    keyframes_.insert(0, keyframes_.front()->NeutralKeyframe(
                             0, std::move(zero_offset_easing)));
    added_synthetic_keyframe = true;
  }
  if (keyframes_.back()->Offset() < 1.0) {
    AppendKeyframe(keyframes_.back()->NeutralKeyframe(1, nullptr));
    added_synthetic_keyframe = true;
  }

  return added_synthetic_keyframe;
}

}  // namespace blink
