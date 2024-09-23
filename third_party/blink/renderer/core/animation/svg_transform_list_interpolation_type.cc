// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_transform_list_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/animation/non_interpolable_value.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/animation/svg_interpolation_environment.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/svg/svg_transform.h"
#include "third_party/blink/renderer/core/svg/svg_transform_list.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

class SVGTransformNonInterpolableValue : public NonInterpolableValue {
 public:
  ~SVGTransformNonInterpolableValue() override = default;

  static scoped_refptr<SVGTransformNonInterpolableValue> Create(
      Vector<SVGTransformType>& transform_types) {
    return base::AdoptRef(
        new SVGTransformNonInterpolableValue(transform_types));
  }

  const Vector<SVGTransformType>& TransformTypes() const {
    return transform_types_;
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  SVGTransformNonInterpolableValue(Vector<SVGTransformType>& transform_types) {
    transform_types_.swap(transform_types);
  }

  Vector<SVGTransformType> transform_types_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(SVGTransformNonInterpolableValue);
template <>
struct DowncastTraits<SVGTransformNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() == SVGTransformNonInterpolableValue::static_type_;
  }
};

namespace {

InterpolableValue* TranslateToInterpolableValue(SVGTransform* transform) {
  gfx::Vector2dF translate = transform->Translate();
  auto* result = MakeGarbageCollected<InterpolableList>(2);
  result->Set(0, MakeGarbageCollected<InterpolableNumber>(translate.x()));
  result->Set(1, MakeGarbageCollected<InterpolableNumber>(translate.y()));
  return result;
}

SVGTransform* TranslateFromInterpolableValue(const InterpolableValue& value) {
  const auto& list = To<InterpolableList>(value);

  // Note: using default CSSToLengthConversionData here as it's
  // guaranteed to be a double.
  // TODO(crbug.com/325821290): Avoid InterpolableNumber in this file.
  CSSToLengthConversionData length_resolver;
  auto* transform =
      MakeGarbageCollected<SVGTransform>(SVGTransformType::kTranslate);
  transform->SetTranslate(
      To<InterpolableNumber>(list.Get(0))->Value(length_resolver),
      To<InterpolableNumber>(list.Get(1))->Value(length_resolver));
  return transform;
}

InterpolableValue* ScaleToInterpolableValue(SVGTransform* transform) {
  gfx::Vector2dF scale = transform->Scale();
  auto* result = MakeGarbageCollected<InterpolableList>(2);
  result->Set(0, MakeGarbageCollected<InterpolableNumber>(scale.x()));
  result->Set(1, MakeGarbageCollected<InterpolableNumber>(scale.y()));
  return result;
}

SVGTransform* ScaleFromInterpolableValue(const InterpolableValue& value) {
  const auto& list = To<InterpolableList>(value);

  CSSToLengthConversionData length_resolver;
  auto* transform =
      MakeGarbageCollected<SVGTransform>(SVGTransformType::kScale);
  transform->SetScale(
      To<InterpolableNumber>(list.Get(0))->Value(length_resolver),
      To<InterpolableNumber>(list.Get(1))->Value(length_resolver));
  return transform;
}

InterpolableValue* RotateToInterpolableValue(SVGTransform* transform) {
  gfx::PointF rotation_center = transform->RotationCenter();
  auto* result = MakeGarbageCollected<InterpolableList>(3);
  result->Set(0, MakeGarbageCollected<InterpolableNumber>(transform->Angle()));
  result->Set(1, MakeGarbageCollected<InterpolableNumber>(rotation_center.x()));
  result->Set(2, MakeGarbageCollected<InterpolableNumber>(rotation_center.y()));
  return result;
}

SVGTransform* RotateFromInterpolableValue(const InterpolableValue& value) {
  const auto& list = To<InterpolableList>(value);

  CSSToLengthConversionData length_resolver;
  auto* transform =
      MakeGarbageCollected<SVGTransform>(SVGTransformType::kRotate);
  transform->SetRotate(
      To<InterpolableNumber>(list.Get(0))->Value(length_resolver),
      To<InterpolableNumber>(list.Get(1))->Value(length_resolver),
      To<InterpolableNumber>(list.Get(2))->Value(length_resolver));
  return transform;
}

InterpolableValue* SkewXToInterpolableValue(SVGTransform* transform) {
  return MakeGarbageCollected<InterpolableNumber>(transform->Angle());
}

SVGTransform* SkewXFromInterpolableValue(const InterpolableValue& value) {
  auto* transform =
      MakeGarbageCollected<SVGTransform>(SVGTransformType::kSkewx);
  transform->SetSkewX(
      To<InterpolableNumber>(value).Value(CSSToLengthConversionData()));
  return transform;
}

InterpolableValue* SkewYToInterpolableValue(SVGTransform* transform) {
  return MakeGarbageCollected<InterpolableNumber>(transform->Angle());
}

SVGTransform* SkewYFromInterpolableValue(const InterpolableValue& value) {
  auto* transform =
      MakeGarbageCollected<SVGTransform>(SVGTransformType::kSkewy);
  transform->SetSkewY(
      To<InterpolableNumber>(value).Value(CSSToLengthConversionData()));
  return transform;
}

InterpolableValue* ToInterpolableValue(SVGTransform* transform,
                                       SVGTransformType transform_type) {
  switch (transform_type) {
    case SVGTransformType::kTranslate:
      return TranslateToInterpolableValue(transform);
    case SVGTransformType::kScale:
      return ScaleToInterpolableValue(transform);
    case SVGTransformType::kRotate:
      return RotateToInterpolableValue(transform);
    case SVGTransformType::kSkewx:
      return SkewXToInterpolableValue(transform);
    case SVGTransformType::kSkewy:
      return SkewYToInterpolableValue(transform);
    case SVGTransformType::kMatrix:
    case SVGTransformType::kUnknown:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

SVGTransform* FromInterpolableValue(const InterpolableValue& value,
                                    SVGTransformType transform_type) {
  switch (transform_type) {
    case SVGTransformType::kTranslate:
      return TranslateFromInterpolableValue(value);
    case SVGTransformType::kScale:
      return ScaleFromInterpolableValue(value);
    case SVGTransformType::kRotate:
      return RotateFromInterpolableValue(value);
    case SVGTransformType::kSkewx:
      return SkewXFromInterpolableValue(value);
    case SVGTransformType::kSkewy:
      return SkewYFromInterpolableValue(value);
    case SVGTransformType::kMatrix:
    case SVGTransformType::kUnknown:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

const Vector<SVGTransformType>& GetTransformTypes(
    const InterpolationValue& value) {
  return To<SVGTransformNonInterpolableValue>(*value.non_interpolable_value)
      .TransformTypes();
}

class SVGTransformListChecker : public InterpolationType::ConversionChecker {
 public:
  explicit SVGTransformListChecker(const InterpolationValue& underlying)
      : underlying_(MakeGarbageCollected<InterpolationValueGCed>(underlying)) {}

  void Trace(Visitor* visitor) const final {
    InterpolationType::ConversionChecker::Trace(visitor);
    visitor->Trace(underlying_);
  }

  bool IsValid(const InterpolationEnvironment&,
               const InterpolationValue& underlying) const final {
    // TODO(suzyh): change maybeConvertSingle so we don't have to recalculate
    // for changes to the interpolable values
    if (!underlying && !underlying_) {
      return true;
    }
    if (!underlying || !underlying_) {
      return false;
    }
    return underlying_->underlying().interpolable_value->Equals(
               *underlying.interpolable_value) &&
           GetTransformTypes(underlying_->underlying()) ==
               GetTransformTypes(underlying);
  }

 private:
  const Member<const InterpolationValueGCed> underlying_;
};

}  // namespace

InterpolationValue SVGTransformListInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  NOTREACHED_IN_MIGRATION();
  // This function is no longer called, because maybeConvertSingle has been
  // overridden.
  return nullptr;
}

InterpolationValue SVGTransformListInterpolationType::MaybeConvertSVGValue(
    const SVGPropertyBase& svg_value) const {
  const auto* svg_list = DynamicTo<SVGTransformList>(svg_value);
  if (!svg_list) {
    return nullptr;
  }

  auto* result = MakeGarbageCollected<InterpolableList>(svg_list->length());

  Vector<SVGTransformType> transform_types;
  for (wtf_size_t i = 0; i < svg_list->length(); i++) {
    const SVGTransform* transform = svg_list->at(i);
    SVGTransformType transform_type(transform->TransformType());
    if (transform_type == SVGTransformType::kMatrix) {
      // TODO(ericwilligers): Support matrix interpolation.
      return nullptr;
    }
    result->Set(i, ToInterpolableValue(transform->Clone(), transform_type));
    transform_types.push_back(transform_type);
  }
  return InterpolationValue(
      result, SVGTransformNonInterpolableValue::Create(transform_types));
}

InterpolationValue SVGTransformListInterpolationType::MaybeConvertSingle(
    const PropertySpecificKeyframe& keyframe,
    const InterpolationEnvironment& environment,
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  Vector<SVGTransformType> types;
  HeapVector<Member<InterpolableValue>> interpolable_parts;

  if (keyframe.Composite() == EffectModel::kCompositeAdd) {
    if (underlying) {
      types.AppendVector(GetTransformTypes(underlying));
      interpolable_parts.push_back(underlying.interpolable_value->Clone());
    }
    conversion_checkers.push_back(
        MakeGarbageCollected<SVGTransformListChecker>(underlying));
  } else {
    DCHECK(!keyframe.IsNeutral());
  }

  if (!keyframe.IsNeutral()) {
    auto* svg_value =
        To<SVGInterpolationEnvironment>(environment)
            .SvgBaseValue()
            .CloneForAnimation(
                To<SVGPropertySpecificKeyframe>(keyframe).Value());
    InterpolationValue value = MaybeConvertSVGValue(*svg_value);
    if (!value) {
      return nullptr;
    }
    types.AppendVector(GetTransformTypes(value));
    interpolable_parts.push_back(std::move(value.interpolable_value));
  }

  auto* interpolable_list =
      MakeGarbageCollected<InterpolableList>(types.size());
  wtf_size_t interpolable_list_index = 0;
  for (auto& part : interpolable_parts) {
    auto& list = To<InterpolableList>(*part);
    for (wtf_size_t i = 0; i < list.length(); ++i) {
      interpolable_list->Set(interpolable_list_index,
                             std::move(list.GetMutable(i)));
      ++interpolable_list_index;
    }
  }

  return InterpolationValue(interpolable_list,
                            SVGTransformNonInterpolableValue::Create(types));
}

SVGPropertyBase* SVGTransformListInterpolationType::AppliedSVGValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value) const {
  auto* result = MakeGarbageCollected<SVGTransformList>();
  const auto& list = To<InterpolableList>(interpolable_value);
  const Vector<SVGTransformType>& transform_types =
      To<SVGTransformNonInterpolableValue>(non_interpolable_value)
          ->TransformTypes();
  for (wtf_size_t i = 0; i < list.length(); ++i) {
    result->Append(FromInterpolableValue(*list.Get(i), transform_types.at(i)));
  }
  return result;
}

PairwiseInterpolationValue SVGTransformListInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  if (GetTransformTypes(start) != GetTransformTypes(end)) {
    return nullptr;
  }

  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(end.non_interpolable_value));
}

void SVGTransformListInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  underlying_value_owner.Set(*this, value);
}

}  // namespace blink
