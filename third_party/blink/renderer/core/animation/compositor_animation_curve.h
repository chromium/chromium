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

  const CSSPropertyName& PropertyName() { return property_name_; }

 protected:
  explicit CompositorAnimationCurve(CSSPropertyName property_name)
      : property_name_(property_name) {}

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

  virtual const BaseKeyframe& GetBaseKeyframe(wtf_size_t index) const = 0;

  // Returns the number of keyframes.
  virtual wtf_size_t Size() const = 0;

  CSSPropertyName property_name_;
  wtf_size_t last_index_ = 0;
};

template <typename T>
class TypedCompositorAnimationCurve : public CompositorAnimationCurve {
 public:
  // Returns the interpolated value from the animation progress.
  T Interpolate(double progress) {
    unsigned result_index = ComputeKeyframeIndex(progress);
    double transformed_progress =
        ComputeKeyframeIntervalProgress(result_index, progress);
    return InterpolateKeyframes(result_index, transformed_progress);
  }

 protected:
  explicit TypedCompositorAnimationCurve(CSSPropertyName property_name)
      : CompositorAnimationCurve(property_name) {}

  struct TypedKeyframe : public BaseKeyframe {
    TypedKeyframe(double offset, std::unique_ptr<gfx::TimingFunction>& tf, T v)
        : BaseKeyframe(offset, tf), value(v) {}
    T value;
  };

  void AddKeyframe(double offset,
                   const TimingFunction& timing_function,
                   const CSSValue* value) override {
    std::unique_ptr<gfx::TimingFunction> timing_function_copy =
        timing_function.CloneToCC();
    keyframes_.push_back(
        TypedKeyframe(offset, timing_function_copy, ConvertCssValue(value)));
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

  wtf_size_t Size() const override { return keyframes_.size(); }

  const BaseKeyframe& GetBaseKeyframe(wtf_size_t index) const override {
    return keyframes_[index];
  }

  const TypedKeyframe& GetTypedKeyframe(wtf_size_t index) const {
    return keyframes_[index];
  }

  const Vector<TypedKeyframe>& GetKeyframes() const { return keyframes_; }

  virtual T ConvertCssValue(const CSSValue* value) = 0;
  virtual T ConvertTypedInterpolationValue(
      const TypedInterpolationValue* interpolation_value) = 0;
  virtual T InterpolateKeyframes(unsigned index, double proress) = 0;

 private:
  Vector<TypedKeyframe> keyframes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_COMPOSITOR_ANIMATION_CURVE_H_
