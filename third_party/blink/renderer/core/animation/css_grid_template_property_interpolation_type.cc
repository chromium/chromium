// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_grid_template_property_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/interpolable_grid_track_list.h"
#include "third_party/blink/renderer/core/animation/underlying_value_owner.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_grid_track_list.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

class CSSGridTrackListNonInterpolableValue final : public NonInterpolableValue {
 public:
  CSSGridTrackListNonInterpolableValue(
      NamedGridLinesMap named_grid_lines_from,
      OrderedNamedGridLines ordered_named_grid_lines_from,
      NamedGridLinesMap named_grid_lines_to = NamedGridLinesMap(),
      OrderedNamedGridLines ordered_named_grid_lines_to =
          OrderedNamedGridLines())
      : named_grid_lines_from_(std::move(named_grid_lines_from)),
        ordered_named_grid_lines_from_(
            std::move(ordered_named_grid_lines_from)),
        named_grid_lines_to_(std::move(named_grid_lines_to)),
        ordered_named_grid_lines_to_(std::move(ordered_named_grid_lines_to)) {}
  CSSGridTrackListNonInterpolableValue(
      const CSSGridTrackListNonInterpolableValue& start,
      const CSSGridTrackListNonInterpolableValue& end)
      : named_grid_lines_from_(start.GetNamedGridLines()),
        ordered_named_grid_lines_from_(start.GetOrderedNamedGridLines()),
        named_grid_lines_to_(end.GetNamedGridLines()),
        ordered_named_grid_lines_to_(end.GetOrderedNamedGridLines()) {}
  ~CSSGridTrackListNonInterpolableValue() final = default;

  bool Equals(const CSSGridTrackListNonInterpolableValue& other) const {
    return named_grid_lines_from_ == other.named_grid_lines_from_ &&
           ordered_named_grid_lines_from_ ==
               other.ordered_named_grid_lines_from_ &&
           named_grid_lines_to_ == other.named_grid_lines_to_ &&
           ordered_named_grid_lines_to_ == other.ordered_named_grid_lines_to_;
  }

  const NamedGridLinesMap& GetNamedGridLines() const {
    return named_grid_lines_from_;
  }
  const OrderedNamedGridLines& GetOrderedNamedGridLines() const {
    return ordered_named_grid_lines_from_;
  }

  const NamedGridLinesMap& GetCurrentNamedGridLines(double progress) const {
    return (progress < 0.5) ? named_grid_lines_from_ : named_grid_lines_to_;
  }
  const OrderedNamedGridLines& GetCurrentOrderedNamedGridLines(
      double progress) const {
    return (progress < 0.5) ? ordered_named_grid_lines_from_
                            : ordered_named_grid_lines_to_;
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:

  // For the first half of the interpolation, we return the 'from' values for
  // named grid lines. For the second half, we return the 'to' values. As the
  // named grid lines 'from' and 'to' values and its size may be different, we
  // have to cache both and return the appropriate value given the
  // interpolation's progress.
  NamedGridLinesMap named_grid_lines_from_;
  OrderedNamedGridLines ordered_named_grid_lines_from_;
  NamedGridLinesMap named_grid_lines_to_;
  OrderedNamedGridLines ordered_named_grid_lines_to_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSGridTrackListNonInterpolableValue);

template <>
struct DowncastTraits<CSSGridTrackListNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() ==
           CSSGridTrackListNonInterpolableValue::static_type_;
  }
};

class UnderlyingGridTrackListChecker final
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit UnderlyingGridTrackListChecker(const InterpolationValue& underlying)
      : underlying_(MakeGarbageCollected<InterpolationValueGCed>(underlying)) {}
  ~UnderlyingGridTrackListChecker() final = default;

  void Trace(Visitor* visitor) const final {
    InterpolationType::ConversionChecker::Trace(visitor);
    visitor->Trace(underlying_);
  }

 private:
  bool IsValid(const StyleResolverState&,
               const InterpolationValue& underlying) const final {
    return To<InterpolableGridTrackList>(
               *underlying_->underlying().interpolable_value)
               .Equals(To<InterpolableGridTrackList>(
                   *underlying.interpolable_value)) &&
           To<CSSGridTrackListNonInterpolableValue>(
               *underlying_->underlying().non_interpolable_value)
               .Equals(To<CSSGridTrackListNonInterpolableValue>(
                   *underlying.non_interpolable_value));
  }

  const Member<const InterpolationValueGCed> underlying_;
};

class InheritedGridTrackListChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedGridTrackListChecker(const GridTrackList& track_list,
                                         const CSSPropertyID& property_id)
      : track_list_(track_list), property_id_(property_id) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    const ComputedStyle& style = *state.ParentStyle();
    const GridTrackList& state_track_list =
        (property_id_ == CSSPropertyID::kGridTemplateColumns)
            ? style.GridTemplateColumns().GetTrackList()
            : style.GridTemplateRows().GetTrackList();

    if (track_list_.HasAutoRepeater() || state_track_list.HasAutoRepeater() ||
        track_list_.RepeaterCount() != state_track_list.RepeaterCount() ||
        track_list_.TrackCountWithoutAutoRepeat() !=
            state_track_list.TrackCountWithoutAutoRepeat()) {
      return false;
    }

    for (wtf_size_t i = 0; i < track_list_.RepeaterCount(); ++i) {
      if (!(track_list_.RepeatType(i) == state_track_list.RepeatType(i) &&
            track_list_.RepeatCount(i, 0) ==
                state_track_list.RepeatCount(i, 0) &&
            track_list_.RepeatSize(i) == state_track_list.RepeatSize(i))) {
        return false;
      }
    }
    return true;
  }

  const GridTrackList track_list_;
  const CSSPropertyID property_id_;
};

// static
InterpolableValue*
CSSGridTemplatePropertyInterpolationType::CreateInterpolableGridTrackList(
    const GridTrackList& track_list,
    const CSSProperty& property,
    float zoom) {
  return InterpolableGridTrackList::MaybeCreate(track_list, property, zoom);
}

PairwiseInterpolationValue
CSSGridTemplatePropertyInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  if (!To<InterpolableGridTrackList>(*start.interpolable_value)
           .IsCompatibleWith(
               To<InterpolableGridTrackList>(*end.interpolable_value))) {
    return nullptr;
  }
  return PairwiseInterpolationValue(
      std::move(start.interpolable_value), std::move(end.interpolable_value),
      MakeGarbageCollected<CSSGridTrackListNonInterpolableValue>(
          To<CSSGridTrackListNonInterpolableValue>(
              *start.non_interpolable_value),
          To<CSSGridTrackListNonInterpolableValue>(
              *end.non_interpolable_value)));
}

InterpolationValue
CSSGridTemplatePropertyInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  conversion_checkers.push_back(
      MakeGarbageCollected<UnderlyingGridTrackListChecker>(underlying));
  return InterpolationValue(underlying.interpolable_value->CloneAndZero(),
                            underlying.non_interpolable_value);
}

InterpolationValue
CSSGridTemplatePropertyInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers&) const {
  // 'none' cannot be interpolated.
  return nullptr;
}

InterpolationValue
CSSGridTemplatePropertyInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const ComputedStyle* parent_style = state.ParentStyle();
  if (!parent_style)
    return nullptr;

  const ComputedGridTrackList& parent_computed_grid_track_list =
      (property_id_ == CSSPropertyID::kGridTemplateColumns)
          ? parent_style->GridTemplateColumns()
          : parent_style->GridTemplateRows();
  const GridTrackList& parent_track_list =
      parent_computed_grid_track_list.GetTrackList();

  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedGridTrackListChecker>(parent_track_list,
                                                          property_id_));
  return InterpolationValue(
      CreateInterpolableGridTrackList(parent_track_list, CssProperty(),
                                      parent_style->EffectiveZoom()),
      MakeGarbageCollected<CSSGridTrackListNonInterpolableValue>(
          parent_computed_grid_track_list.GetNamedGridLines(),
          parent_computed_grid_track_list.GetOrderedNamedGridLines()));
}

InterpolationValue CSSGridTemplatePropertyInterpolationType::
    MaybeConvertStandardPropertyUnderlyingValue(
        const ComputedStyle& style) const {
  const ComputedGridTrackList& computed_grid_track_list =
      (property_id_ == CSSPropertyID::kGridTemplateColumns)
          ? style.GridTemplateColumns()
          : style.GridTemplateRows();
  return InterpolationValue(
      CreateInterpolableGridTrackList(computed_grid_track_list.GetTrackList(),
                                      CssProperty(), style.EffectiveZoom()),
      MakeGarbageCollected<CSSGridTrackListNonInterpolableValue>(
          computed_grid_track_list.GetNamedGridLines(),
          computed_grid_track_list.GetOrderedNamedGridLines()));
}

InterpolationValue CSSGridTemplatePropertyInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState& state,
    ConversionCheckers&) const {
  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNone);
    return InterpolationValue(nullptr);
  }

  ComputedGridTrackList* computed_grid_track_list =
      StyleBuilderConverter::ConvertGridTrackList(
          const_cast<StyleResolverState&>(state), value);
  return InterpolationValue(
      CreateInterpolableGridTrackList(computed_grid_track_list->GetTrackList(),
                                      CssProperty(),
                                      state.StyleBuilder().EffectiveZoom()),
      MakeGarbageCollected<CSSGridTrackListNonInterpolableValue>(
          computed_grid_track_list->GetNamedGridLines(),
          computed_grid_track_list->GetOrderedNamedGridLines()));
}

void CSSGridTemplatePropertyInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const InterpolableGridTrackList& interpolable_grid_track_list =
      To<InterpolableGridTrackList>(interpolable_value);
  const CSSGridTrackListNonInterpolableValue* non_interoplable_grid_track_list =
      To<CSSGridTrackListNonInterpolableValue>(non_interpolable_value);

  double progress = interpolable_grid_track_list.GetProgress();
  bool is_for_columns = property_id_ == CSSPropertyID::kGridTemplateColumns;
  ComputedStyleBuilder& builder = state.StyleBuilder();
  CSSToLengthConversionData conversion_data = state.CssToLengthConversionData();
  ComputedGridTrackList* computed_grid_track_list(
      is_for_columns ? MakeGarbageCollected<ComputedGridTrackList>(
                           std::move(builder.GridTemplateColumns()))
                     : MakeGarbageCollected<ComputedGridTrackList>(
                           std::move(builder.GridTemplateRows())));

  computed_grid_track_list->SetTrackList(
      interpolable_grid_track_list.CreateGridTrackList(conversion_data));
  computed_grid_track_list->SetNamedGridLines(
      non_interoplable_grid_track_list->GetCurrentNamedGridLines(progress));
  computed_grid_track_list->SetOrderedNamedGridLines(
      non_interoplable_grid_track_list->GetCurrentOrderedNamedGridLines(
          progress));

  if (is_for_columns)
    builder.SetGridTemplateColumns(computed_grid_track_list);
  else
    builder.SetGridTemplateRows(computed_grid_track_list);
}

void CSSGridTemplatePropertyInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  if (!To<InterpolableGridTrackList>(
           *underlying_value_owner.Value().interpolable_value)
           .IsCompatibleWith(
               To<InterpolableGridTrackList>(*value.interpolable_value))) {
    underlying_value_owner.Set(this, value);
    return;
  }
  underlying_value_owner.SetNonInterpolableValue(value.non_interpolable_value);
  underlying_value_owner.MutableValue().interpolable_value->ScaleAndAdd(
      underlying_fraction, *value.interpolable_value);
}

}  // namespace blink
