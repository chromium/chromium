// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_COMPOSITOR_ANIMATION_CURVE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_COMPOSITOR_ANIMATION_CURVE_H_

#include "base/memory/ref_counted.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"

namespace blink {

class CSSPropertyName;
class CSSValue;
class Element;

class CORE_EXPORT CompositorAnimationCurve
    : public ThreadSafeRefCounted<CompositorAnimationCurve> {
 public:
  virtual ~CompositorAnimationCurve() = default;

  using ValueFilter =
      bool (*)(Element* element,
               const CSSValue* css_value,
               const TypedInterpolationValue* interpolation_value);

  struct BaseKeyframe {
    BaseKeyframe(double offset, std::unique_ptr<gfx::TimingFunction>& tf)
        : offset(offset), timing_function(tf.release()) {}
    double offset;
    std::unique_ptr<gfx::TimingFunction> timing_function;
  };

  wtf_size_t ComputeKeyframeIndex(double progress);
  double ComputeKeyframeIntervalProgress(wtf_size_t index, double progress);

  const CSSPropertyName& PropertyName() const { return property_name_; }

  bool HasStyleDependency() const {
    return !style_dependent_keyframe_indices_.empty();
  }

  // Updates style dependent keyframe values. A new curve is created if any
  // of the keyframe values were altered.
  scoped_refptr<CompositorAnimationCurve> UpdateKeyframeSnapshot(
      Animation* animation);

  virtual const BaseKeyframe& GetBaseKeyframe(wtf_size_t index) const = 0;

  // Returns the number of keyframes.
  virtual wtf_size_t Size() const = 0;

 protected:
  explicit CompositorAnimationCurve(CSSPropertyName property_name)
      : property_name_(property_name) {}
  CompositorAnimationCurve(const CompositorAnimationCurve& other)
      : property_name_(other.property_name_),
        style_dependent_keyframe_indices_(
            other.style_dependent_keyframe_indices_) {}

  // Extracts (offset, easing, value) triplets. A return value of true indicates
  // the extraction process was successful. Otherwise, an unsupported keyframe
  // value was encountered during the process,and curve cannot be interpolated.
  bool PopulateKeyframes(Animation* animation, ValueFilter value_filter);

  // Add a keyframe sourced from a CSSValue in a Stringkeyframe.
  virtual void AddKeyframe(double offset,
                           const TimingFunction& timing_function,
                           const CSSValue* css_value) = 0;

  // Add a keyframe sourced from an TypedInterpolationValue in a
  // TransitionKeyframe.
  virtual void AddKeyframe(
      double offset,
      const TimingFunction& timing_function,
      const TypedInterpolationValue* interpolation_value) = 0;

  virtual void UpdateKeyframe(scoped_refptr<CompositorAnimationCurve>& target,
                              wtf_size_t index,
                              const CSSValue* value) = 0;

  virtual scoped_refptr<CompositorAnimationCurve> Clone() = 0;

 private:
  CSSPropertyName property_name_;
  wtf_size_t last_index_ = 0;

  // Holds the indices of any keyframes that contain a style dependent values.
  // The corresponding values require updating when the keyframe snapshot is
  // marked as dirty. The update is lazy, and just needs to be performed ahead
  // of the next hand off to cc.
  Vector<wtf_size_t> style_dependent_keyframe_indices_;
};

template <typename T>
class TypedCompositorAnimationCurve : public CompositorAnimationCurve {
 public:
  // Returns the interpolated value from the animation progress.
  T Interpolate(double progress) {
    wtf_size_t result_index = ComputeKeyframeIndex(progress);
    double transformed_progress =
        ComputeKeyframeIntervalProgress(result_index, progress);
    return InterpolateKeyframes(result_index, transformed_progress);
  }

  struct TypedKeyframe : public BaseKeyframe {
    TypedKeyframe(double offset, std::unique_ptr<gfx::TimingFunction>& tf, T v)
        : BaseKeyframe(offset, tf), value(v) {}
    T value;
  };

  const BaseKeyframe& GetBaseKeyframe(wtf_size_t index) const override {
    return keyframes_[index];
  }

  const TypedKeyframe& GetTypedKeyframe(wtf_size_t index) const {
    return keyframes_[index];
  }

  const Vector<TypedKeyframe>& GetKeyframes() const { return keyframes_; }

  wtf_size_t Size() const override { return keyframes_.size(); }

 protected:
  explicit TypedCompositorAnimationCurve(CSSPropertyName property_name)
      : CompositorAnimationCurve(property_name) {}
  TypedCompositorAnimationCurve(const TypedCompositorAnimationCurve& other)
      : CompositorAnimationCurve(other) {
    for (const TypedKeyframe& keyframe : other.keyframes_) {
      std::unique_ptr<gfx::TimingFunction> timing_function_copy;
      if (keyframe.timing_function) {
        timing_function_copy = keyframe.timing_function->Clone();
      }
      keyframes_.push_back(
          TypedKeyframe(keyframe.offset, timing_function_copy, keyframe.value));
    }
  }

  void AddKeyframe(double offset,
                   const TimingFunction& timing_function,
                   const CSSValue* value) override {
    std::unique_ptr<gfx::TimingFunction> timing_function_copy =
        timing_function.CloneToCC();
    keyframes_.push_back(
        TypedKeyframe(offset, timing_function_copy, ConvertCssValue(value)));
  }

  void UpdateKeyframe(scoped_refptr<CompositorAnimationCurve>& clone,
                      wtf_size_t index,
                      const CSSValue* value) override {
    T updated_value = ConvertCssValue(value);
    if (keyframes_[index].value != updated_value) {
      if (!clone) {
        clone = Clone();
      }
      TypedCompositorAnimationCurve* target =
          static_cast<TypedCompositorAnimationCurve*>(clone.get());
      target->keyframes_[index].value = updated_value;
    }
  }

  void AddKeyframe(
      double offset,
      const TimingFunction& timing_function,
      const TypedInterpolationValue* interpolation_value) override {
    std::unique_ptr<gfx::TimingFunction> timing_function_copy =
        timing_function.CloneToCC();
    keyframes_.push_back(
        TypedKeyframe(offset, timing_function_copy,
                      ConvertTypedInterpolationValue(interpolation_value)));
  }

  void AddKeyframeForTesting(double offset, T value) {
    std::unique_ptr<gfx::TimingFunction> tf;
    keyframes_.push_back(TypedKeyframe(offset, tf, value));
  }

  virtual T ConvertCssValue(const CSSValue* value) = 0;
  virtual T ConvertTypedInterpolationValue(
      const TypedInterpolationValue* interpolation_value) = 0;
  virtual T InterpolateKeyframes(wtf_size_t index, double proress) = 0;

 private:
  Vector<TypedKeyframe> keyframes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_COMPOSITOR_ANIMATION_CURVE_H_
