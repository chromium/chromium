// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/path_interpolation_functions.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/interpolated_svg_path_source.h"
#include "third_party/blink/renderer/core/animation/interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/svg_path_seg_interpolation_functions.h"
#include "third_party/blink/renderer/core/css/css_path_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/svg/svg_path.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream_builder.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream_source.h"
#include "third_party/blink/renderer/core/svg/svg_path_parser.h"

namespace blink {

class SVGPathNonInterpolableValue : public NonInterpolableValue {
 public:
  ~SVGPathNonInterpolableValue() override = default;

  static scoped_refptr<SVGPathNonInterpolableValue> Create(
      Vector<SVGPathSegType>& path_seg_types,
      WindRule wind_rule = RULE_NONZERO) {
    return base::AdoptRef(
        new SVGPathNonInterpolableValue(path_seg_types, wind_rule));
  }

  const Vector<SVGPathSegType>& PathSegTypes() const { return path_seg_types_; }
  WindRule GetWindRule() const { return wind_rule_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  SVGPathNonInterpolableValue(Vector<SVGPathSegType>& path_seg_types,
                              WindRule wind_rule)
      : wind_rule_(wind_rule) {
    path_seg_types_.swap(path_seg_types);
  }

  Vector<SVGPathSegType> path_seg_types_;
  WindRule wind_rule_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(SVGPathNonInterpolableValue);
template <>
struct DowncastTraits<SVGPathNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() == SVGPathNonInterpolableValue::static_type_;
  }
};

enum PathComponentIndex : unsigned {
  kPathArgsIndex,
  kPathNeutralIndex,
  kPathComponentIndexCount,
};

InterpolationValue PathInterpolationFunctions::ConvertValue(
    const StylePath* style_path,
    CoordinateConversion coordinate_conversion) {
  if (!style_path)
    return nullptr;

  SVGPathByteStreamSource path_source(style_path->ByteStream());
  wtf_size_t length = 0;
  PathCoordinates current_coordinates;
  HeapVector<Member<InterpolableValue>> interpolable_path_segs;
  Vector<SVGPathSegType> path_seg_types;

  while (path_source.HasMoreData()) {
    const PathSegmentData segment = path_source.ParseSegment();
    interpolable_path_segs.push_back(
        SVGPathSegInterpolationFunctions::ConsumePathSeg(segment,
                                                         current_coordinates));
    SVGPathSegType seg_type = segment.command;
    if (coordinate_conversion == kForceAbsolute)
      seg_type = ToAbsolutePathSegType(seg_type);
    path_seg_types.push_back(seg_type);
    length++;
  }

  auto* path_args = MakeGarbageCollected<InterpolableList>(length);
  for (wtf_size_t i = 0; i < interpolable_path_segs.size(); i++)
    path_args->Set(i, std::move(interpolable_path_segs[i]));

  auto* result =
      MakeGarbageCollected<InterpolableList>(kPathComponentIndexCount);
  result->Set(kPathArgsIndex, path_args);
  result->Set(kPathNeutralIndex, MakeGarbageCollected<InterpolableNumber>(0));

  return InterpolationValue(
      result, SVGPathNonInterpolableValue::Create(path_seg_types,
                                                  style_path->GetWindRule()));
}

class UnderlyingPathSegTypesChecker final
    : public InterpolationType::ConversionChecker {
 public:
  ~UnderlyingPathSegTypesChecker() final = default;

  static UnderlyingPathSegTypesChecker* Create(
      const InterpolationValue& underlying) {
    return MakeGarbageCollected<UnderlyingPathSegTypesChecker>(
        GetPathSegTypes(underlying), GetWindRule(underlying));
  }

  UnderlyingPathSegTypesChecker(const Vector<SVGPathSegType>& path_seg_types,
                                WindRule wind_rule)
      : path_seg_types_(path_seg_types), wind_rule_(wind_rule) {}

 private:
  static const Vector<SVGPathSegType>& GetPathSegTypes(
      const InterpolationValue& underlying) {
    return To<SVGPathNonInterpolableValue>(*underlying.non_interpolable_value)
        .PathSegTypes();
  }

  static WindRule GetWindRule(const InterpolationValue& underlying) {
    return To<SVGPathNonInterpolableValue>(*underlying.non_interpolable_value)
        .GetWindRule();
  }

  bool IsValid(const InterpolationEnvironment&,
               const InterpolationValue& underlying) const final {
    return path_seg_types_ == GetPathSegTypes(underlying) &&
           wind_rule_ == GetWindRule(underlying);
  }

  Vector<SVGPathSegType> path_seg_types_;
  WindRule wind_rule_;
};

InterpolationValue PathInterpolationFunctions::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    InterpolationType::ConversionCheckers& conversion_checkers) {
  conversion_checkers.push_back(
      UnderlyingPathSegTypesChecker::Create(underlying));
  auto* result =
      MakeGarbageCollected<InterpolableList>(kPathComponentIndexCount);
  result->Set(kPathArgsIndex,
              To<InterpolableList>(*underlying.interpolable_value)
                  .Get(kPathArgsIndex)
                  ->CloneAndZero());
  result->Set(kPathNeutralIndex, MakeGarbageCollected<InterpolableNumber>(1));
  return InterpolationValue(result, underlying.non_interpolable_value.get());
}

static bool PathSegTypesMatch(const Vector<SVGPathSegType>& a,
                              const Vector<SVGPathSegType>& b) {
  if (a.size() != b.size())
    return false;

  for (wtf_size_t i = 0; i < a.size(); i++) {
    if (ToAbsolutePathSegType(a[i]) != ToAbsolutePathSegType(b[i]))
      return false;
  }

  return true;
}

bool PathInterpolationFunctions::IsPathNonInterpolableValue(
    const NonInterpolableValue& value) {
  return DynamicTo<SVGPathNonInterpolableValue>(value);
}

bool PathInterpolationFunctions::PathsAreCompatible(
    const NonInterpolableValue& start,
    const NonInterpolableValue& end) {
  auto& start_path = To<SVGPathNonInterpolableValue>(start);
  auto& end_path = To<SVGPathNonInterpolableValue>(end);

  if (start_path.GetWindRule() != end_path.GetWindRule())
    return false;

  const Vector<SVGPathSegType>& start_types = start_path.PathSegTypes();
  const Vector<SVGPathSegType>& end_types = end_path.PathSegTypes();
  if (start_types.size() == 0 || !PathSegTypesMatch(start_types, end_types))
    return false;

  return true;
}

PairwiseInterpolationValue PathInterpolationFunctions::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) {
  if (!PathsAreCompatible(*start.non_interpolable_value.get(),
                          *end.non_interpolable_value.get()))
    return nullptr;

  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(end.non_interpolable_value));
}

void PathInterpolationFunctions::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationType& type,
    const InterpolationValue& value) {
  const auto& list = To<InterpolableList>(*value.interpolable_value);
  // TODO(crbug.com/325821290): Avoid InterpolableNumber here.
  double neutral_component = To<InterpolableNumber>(list.Get(kPathNeutralIndex))
                                 ->Value(CSSToLengthConversionData());

  if (neutral_component == 0) {
    underlying_value_owner.Set(type, value);
    return;
  }

  DCHECK(PathSegTypesMatch(
      To<SVGPathNonInterpolableValue>(
          *underlying_value_owner.Value().non_interpolable_value)
          .PathSegTypes(),
      To<SVGPathNonInterpolableValue>(*value.non_interpolable_value)
          .PathSegTypes()));
  underlying_value_owner.MutableValue().interpolable_value->ScaleAndAdd(
      neutral_component, *value.interpolable_value);
  underlying_value_owner.MutableValue().non_interpolable_value =
      value.non_interpolable_value.get();
}

scoped_refptr<StylePath> PathInterpolationFunctions::AppliedValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value) {
  auto* non_interpolable_path_value =
      To<SVGPathNonInterpolableValue>(non_interpolable_value);
  InterpolatedSVGPathSource source(
      To<InterpolableList>(
          *To<InterpolableList>(interpolable_value).Get(kPathArgsIndex)),
      non_interpolable_path_value->PathSegTypes());
  SVGPathByteStreamBuilder builder;
  svg_path_parser::ParsePath(source, builder);

  return StylePath::Create(builder.CopyByteStream(),
                           non_interpolable_path_value->GetWindRule());
}

}  // namespace blink
