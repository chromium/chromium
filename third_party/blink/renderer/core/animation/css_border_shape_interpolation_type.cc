// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_border_shape_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/basic_shape_interpolation_functions.h"
#include "third_party/blink/renderer/core/animation/list_interpolation_functions.h"
#include "third_party/blink/renderer/core/animation/underlying_value_owner.h"
#include "third_party/blink/renderer/core/css/css_identifier_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/style_border_shape.h"

namespace blink {

namespace {

InterpolationValue ConvertBorderShape(const StyleBorderShape* border_shape,
                                      const CSSProperty& property,
                                      double zoom) {
  if (!border_shape) {
    return ListInterpolationFunctions::CreateEmptyList();
  }

  return ListInterpolationFunctions::CreateList(
      2, [border_shape, &property, zoom](wtf_size_t index) {
        const BasicShape& shape = index == 0 ? border_shape->OuterShape()
                                             : border_shape->InnerShape();
        GeometryBox box =
            index == 0 ? border_shape->OuterBox() : border_shape->InnerBox();
        return basic_shape_interpolation_functions::MaybeConvertBasicShape(
            &shape, property, zoom, box, CoordBox::kBorderBox);
      });
}

struct CSSBorderShapeEntry {
  STACK_ALLOCATED();

 public:
  CSSBorderShapeEntry() = default;
  CSSBorderShapeEntry(const CSSValue* shape_value, GeometryBox box)
      : shape_value(shape_value), box(box) {}

  const CSSValue* shape_value = nullptr;
  GeometryBox box = GeometryBox::kBorderBox;
};

template <GeometryBox default_box>
CSSBorderShapeEntry CreateEntryFromCSSValue(const CSSValue& value) {
  const CSSValue* shape_value = &value;
  GeometryBox box = default_box;
  if (const auto* pair = DynamicTo<CSSValuePair>(value)) {
    shape_value = &pair->First();
    const auto& ident = To<CSSIdentifierValue>(pair->Second());
    box = ident.ConvertTo<GeometryBox>();
  }
  return CSSBorderShapeEntry(shape_value, box);
}

class BorderShapeUnderlyingCompatibilityChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit BorderShapeUnderlyingCompatibilityChecker(
      const InterpolationValue& underlying)
      : underlying_(MakeGarbageCollected<InterpolationValueGCed>(underlying)) {}

  void Trace(Visitor* visitor) const override {
    CSSInterpolationType::CSSConversionChecker::Trace(visitor);
    visitor->Trace(underlying_);
  }

 private:
  bool IsValid(const StyleResolverState&,
               const InterpolationValue& underlying) const final {
    auto non_interpolable_values_are_compatible =
        [](const NonInterpolableValue* a, const NonInterpolableValue* b) {
          if (!a && !b) {
            return true;
          }
          if (!a || !b) {
            return false;
          }
          return basic_shape_interpolation_functions::ShapesAreCompatible(*a,
                                                                          *b);
        };

    return ListInterpolationFunctions::EqualValues(
        underlying_->underlying(), underlying,
        non_interpolable_values_are_compatible);
  }

  Member<const InterpolationValueGCed> underlying_;
};

class InheritedBorderShapeChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedBorderShapeChecker(const StyleBorderShape* inherited)
      : inherited_(inherited) {}

  void Trace(Visitor* visitor) const override {
    CSSInterpolationType::CSSConversionChecker::Trace(visitor);
    visitor->Trace(inherited_);
  }

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    const StyleBorderShape* current =
        state.ParentStyle() ? state.ParentStyle()->BorderShape() : nullptr;
    return base::ValuesEquivalent(inherited_.Get(), current);
  }

  Member<const StyleBorderShape> inherited_;
};

template <GeometryBox default_box>
GeometryBox GeometryBoxForNonInterpolableValue(
    const NonInterpolableValue* non_interpolable) {
  if (!non_interpolable) {
    return GeometryBox::kHalfBorderBox;
  }
  return basic_shape_interpolation_functions::GetGeometryBox(*non_interpolable,
                                                             default_box);
}

}  // namespace

InterpolationValue CSSBorderShapeInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  if (!underlying.interpolable_value) {
    return ListInterpolationFunctions::CreateEmptyList();
  }

  const auto* non_interpolable_list =
      DynamicTo<NonInterpolableList>(underlying.non_interpolable_value.Get());
  if (!non_interpolable_list) {
    return ListInterpolationFunctions::CreateEmptyList();
  }

  auto& interpolable_list =
      To<InterpolableList>(*underlying.interpolable_value);
  auto* neutral_list =
      MakeGarbageCollected<InterpolableList>(interpolable_list.length());
  for (wtf_size_t i = 0; i < interpolable_list.length(); ++i) {
    const NonInterpolableValue* non_interpolable =
        non_interpolable_list->Get(i);
    CHECK(non_interpolable);
    neutral_list->Set(i,
                      basic_shape_interpolation_functions::CreateNeutralValue(
                          *non_interpolable));
  }

  conversion_checkers.push_back(
      MakeGarbageCollected<BorderShapeUnderlyingCompatibilityChecker>(
          underlying));

  return InterpolationValue(neutral_list,
                            const_cast<NonInterpolableValue*>(
                                underlying.non_interpolable_value.Get()));
}

InterpolationValue CSSBorderShapeInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers&) const {
  const ComputedStyle& initial_style =
      state.GetDocument().GetStyleResolver().InitialStyle();
  return ConvertBorderShape(initial_style.BorderShape(), CssProperty(),
                            initial_style.EffectiveZoom());
}

InterpolationValue CSSBorderShapeInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle()) {
    return ListInterpolationFunctions::CreateEmptyList();
  }
  const StyleBorderShape* inherited = state.ParentStyle()->BorderShape();
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedBorderShapeChecker>(inherited));
  return ConvertBorderShape(inherited, CssProperty(),
                            state.ParentStyle()->EffectiveZoom());
}

InterpolationValue CSSBorderShapeInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState&,
    ConversionCheckers&) const {
  if (const auto* ident = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(ident->GetValueID(), CSSValueID::kNone);
    return ListInterpolationFunctions::CreateEmptyList();
  }

  std::array<CSSBorderShapeEntry, 2> entries;
  if (const auto* list = DynamicTo<CSSValueList>(value)) {
    DCHECK_EQ(list->length(), 2u);
    auto entry =
        CreateEntryFromCSSValue<GeometryBox::kBorderBox>(list->First());
    entries[0] = std::move(entry);
    entry = CreateEntryFromCSSValue<GeometryBox::kPaddingBox>(list->Last());
    entries[1] = std::move(entry);
  } else {
    auto entry = CreateEntryFromCSSValue<GeometryBox::kHalfBorderBox>(value);
    entries[0] = entry;
    entries[1] = entry;
  }

  return ListInterpolationFunctions::CreateList(
      entries.size(), [this, &entries](wtf_size_t index) {
        return basic_shape_interpolation_functions::MaybeConvertCSSValue(
            *entries[index].shape_value, CssProperty(), entries[index].box,
            CoordBox::kBorderBox);
      });
}

PairwiseInterpolationValue CSSBorderShapeInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  return ListInterpolationFunctions::MaybeMergeSingles(
      std::move(start), std::move(end),
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      [](InterpolationValue&& start_item, InterpolationValue&& end_item) {
        const NonInterpolableValue* start_non =
            start_item.non_interpolable_value.Get();
        const NonInterpolableValue* end_non =
            end_item.non_interpolable_value.Get();
        if (!start_non && !end_non) {
          return PairwiseInterpolationValue(
              std::move(start_item.interpolable_value),
              std::move(end_item.interpolable_value));
        }
        if (!start_non || !end_non) {
          return PairwiseInterpolationValue(nullptr);
        }
        if (!basic_shape_interpolation_functions::ShapesAreCompatible(
                *start_non, *end_non)) {
          return PairwiseInterpolationValue(nullptr);
        }
        return PairwiseInterpolationValue(
            std::move(start_item.interpolable_value),
            std::move(end_item.interpolable_value),
            std::move(start_item.non_interpolable_value));
      });
}

InterpolationValue
CSSBorderShapeInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return ConvertBorderShape(style.BorderShape(), CssProperty(),
                            style.EffectiveZoom());
}

void CSSBorderShapeInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  if (!value) {
    return;
  }

  if (!underlying_value_owner) {
    underlying_value_owner.Set(this, value);
    return;
  }

  auto non_interpolable_values_are_compatible =
      [](const NonInterpolableValue* a, const NonInterpolableValue* b) {
        if (!a && !b) {
          return true;
        }
        if (!a || !b) {
          return false;
        }
        return basic_shape_interpolation_functions::ShapesAreCompatible(*a, *b);
      };

  ListInterpolationFunctions::Composite(
      underlying_value_owner, underlying_fraction, this, value,
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      ListInterpolationFunctions::InterpolableValuesKnownCompatible,
      non_interpolable_values_are_compatible,
      [](UnderlyingValue& underlying_value, double fraction,
         const InterpolableValue& interpolable_value,
         const NonInterpolableValue*) {
        underlying_value.MutableInterpolableValue().ScaleAndAdd(
            fraction, interpolable_value);
      });
}

void CSSBorderShapeInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const auto& interpolable_list = To<InterpolableList>(interpolable_value);
  wtf_size_t length = interpolable_list.length();
  if (length == 0) {
    state.StyleBuilder().SetBorderShape(nullptr);
    return;
  }

  const auto* non_interpolable_list =
      DynamicTo<NonInterpolableList>(non_interpolable_value);
  CHECK(non_interpolable_list);
  CHECK_EQ(non_interpolable_list->length(), length);

  BasicShape* outer_shape =
      basic_shape_interpolation_functions::CreateBasicShape(
          *interpolable_list.Get(0), *non_interpolable_list->Get(0),
          state.CssToLengthConversionData());
  BasicShape* inner_shape =
      basic_shape_interpolation_functions::CreateBasicShape(
          *interpolable_list.Get(1), *non_interpolable_list->Get(1),
          state.CssToLengthConversionData());

  if (!outer_shape || !inner_shape) {
    state.StyleBuilder().SetBorderShape(nullptr);
    return;
  }

  GeometryBox outer_box =
      GeometryBoxForNonInterpolableValue<GeometryBox::kBorderBox>(
          non_interpolable_list->Get(0));
  GeometryBox inner_box =
      GeometryBoxForNonInterpolableValue<GeometryBox::kPaddingBox>(
          non_interpolable_list->Get(1));

  state.StyleBuilder().SetBorderShape(MakeGarbageCollected<StyleBorderShape>(
      *outer_shape, inner_shape, outer_box, inner_box));
}

}  // namespace blink
