// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_image_list_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/css_image_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/image_list_property_functions.h"
#include "third_party/blink/renderer/core/animation/list_interpolation_functions.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class UnderlyingImageListChecker final
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit UnderlyingImageListChecker(const InterpolationValue& underlying)
      : underlying_(MakeGarbageCollected<InterpolationValueGCed>(underlying)) {}
  ~UnderlyingImageListChecker() final = default;

  void Trace(Visitor* visitor) const final {
    CSSConversionChecker::Trace(visitor);
    visitor->Trace(underlying_);
  }

 private:
  bool IsValid(const StyleResolverState&,
               const InterpolationValue& underlying) const final {
    return ListInterpolationFunctions::EqualValues(
        underlying_->underlying(), underlying,
        CSSImageInterpolationType::EqualNonInterpolableValues);
  }

  const Member<InterpolationValueGCed> underlying_;
};

InterpolationValue CSSImageListInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  conversion_checkers.push_back(
      MakeGarbageCollected<UnderlyingImageListChecker>(underlying));
  return underlying.Clone();
}

InterpolationValue CSSImageListInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversion_checkers) const {
  StyleImageList* initial_image_list = MakeGarbageCollected<StyleImageList>();
  ImageListPropertyFunctions::GetInitialImageList(CssProperty(),
                                                  initial_image_list);
  return MaybeConvertStyleImageList(initial_image_list);
}

InterpolationValue CSSImageListInterpolationType::MaybeConvertStyleImageList(
    const StyleImageList* image_list) const {
  if (image_list->size() == 0)
    return nullptr;

  return ListInterpolationFunctions::CreateList(
      image_list->size(), [&image_list](wtf_size_t index) {
        return CSSImageInterpolationType::MaybeConvertStyleImage(
            image_list->at(index).Get(), false);
      });
}

class InheritedImageListChecker final
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedImageListChecker(const CSSProperty& property,
                            const StyleImageList* inherited_image_list)
      : property_(property), inherited_image_list_(inherited_image_list) {}

  ~InheritedImageListChecker() final = default;

  void Trace(Visitor* visitor) const final {
    CSSConversionChecker::Trace(visitor);
    visitor->Trace(inherited_image_list_);
  }

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    StyleImageList* inherited_image_list =
        MakeGarbageCollected<StyleImageList>();
    ImageListPropertyFunctions::GetImageList(property_, *state.ParentStyle(),
                                             inherited_image_list);
    return inherited_image_list_ == inherited_image_list;
  }

  const CSSProperty& property_;
  Member<const StyleImageList> inherited_image_list_;
};

InterpolationValue CSSImageListInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle())
    return nullptr;

  StyleImageList* inherited_image_list = MakeGarbageCollected<StyleImageList>();
  ImageListPropertyFunctions::GetImageList(CssProperty(), *state.ParentStyle(),
                                           inherited_image_list);
  conversion_checkers.push_back(MakeGarbageCollected<InheritedImageListChecker>(
      CssProperty(), inherited_image_list));
  return MaybeConvertStyleImageList(inherited_image_list);
}

InterpolationValue CSSImageListInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value && identifier_value->GetValueID() == CSSValueID::kNone)
    return nullptr;

  CSSValueList* temp_list = nullptr;
  if (!value.IsBaseValueList()) {
    temp_list = CSSValueList::CreateCommaSeparated();
    temp_list->Append(value);
  }
  const auto& value_list = temp_list ? *temp_list : To<CSSValueList>(value);

  const wtf_size_t length = value_list.length();
  auto* interpolable_list = MakeGarbageCollected<InterpolableList>(length);
  Vector<scoped_refptr<const NonInterpolableValue>> non_interpolable_values(
      length);
  for (wtf_size_t i = 0; i < length; i++) {
    InterpolationValue component =
        CSSImageInterpolationType::MaybeConvertCSSValue(value_list.Item(i),
                                                        false);
    if (!component)
      return nullptr;
    interpolable_list->Set(i, std::move(component.interpolable_value));
    non_interpolable_values[i] = std::move(component.non_interpolable_value);
  }
  return InterpolationValue(
      std::move(interpolable_list),
      NonInterpolableList::Create(std::move(non_interpolable_values)));
}

PairwiseInterpolationValue CSSImageListInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  return ListInterpolationFunctions::MaybeMergeSingles(
      std::move(start), std::move(end),
      ListInterpolationFunctions::LengthMatchingStrategy::kLowestCommonMultiple,
      CSSImageInterpolationType::StaticMergeSingleConversions);
}

InterpolationValue
CSSImageListInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  StyleImageList* underlying_image_list =
      MakeGarbageCollected<StyleImageList>();
  ImageListPropertyFunctions::GetImageList(CssProperty(), style,
                                           underlying_image_list);
  return MaybeConvertStyleImageList(underlying_image_list);
}

void CSSImageListInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  underlying_value_owner.Set(*this, value);
}

void CSSImageListInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const auto& interpolable_list = To<InterpolableList>(interpolable_value);
  const wtf_size_t length = interpolable_list.length();
  DCHECK_GT(length, 0U);
  const auto& non_interpolable_list =
      To<NonInterpolableList>(*non_interpolable_value);
  DCHECK_EQ(non_interpolable_list.length(), length);
  StyleImageList* image_list = MakeGarbageCollected<StyleImageList>(length);
  for (wtf_size_t i = 0; i < length; i++) {
    image_list->at(i) = CSSImageInterpolationType::ResolveStyleImage(
        CssProperty(), *interpolable_list.Get(i), non_interpolable_list.Get(i),
        state);
  }
  ImageListPropertyFunctions::SetImageList(CssProperty(), state.StyleBuilder(),
                                           image_list);
}

}  // namespace blink
