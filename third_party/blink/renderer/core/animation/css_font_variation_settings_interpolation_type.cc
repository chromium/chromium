// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_font_variation_settings_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/css/css_font_variation_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

class CSSFontVariationSettingsNonInterpolableValue final
    : public NonInterpolableValue {
 public:
  ~CSSFontVariationSettingsNonInterpolableValue() final = default;

  static scoped_refptr<CSSFontVariationSettingsNonInterpolableValue> Create(
      Vector<uint32_t> tags) {
    return base::AdoptRef(
        new CSSFontVariationSettingsNonInterpolableValue(std::move(tags)));
  }

  const Vector<uint32_t>& Tags() const { return tags_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  explicit CSSFontVariationSettingsNonInterpolableValue(Vector<uint32_t> tags)
      : tags_(std::move(tags)) {
    DCHECK_GT(tags_.size(), 0u);
  }

  const Vector<uint32_t> tags_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(
    CSSFontVariationSettingsNonInterpolableValue);
template <>
struct DowncastTraits<CSSFontVariationSettingsNonInterpolableValue> {
  static bool AllowFrom(const NonInterpolableValue* value) {
    return value && AllowFrom(*value);
  }
  static bool AllowFrom(const NonInterpolableValue& value) {
    return value.GetType() ==
           CSSFontVariationSettingsNonInterpolableValue::static_type_;
  }
};

static Vector<uint32_t> GetTags(
    const NonInterpolableValue& non_interpolable_value) {
  return To<CSSFontVariationSettingsNonInterpolableValue>(
             non_interpolable_value)
      .Tags();
}

static bool TagsMatch(const NonInterpolableValue& a,
                      const NonInterpolableValue& b) {
  return GetTags(a) == GetTags(b);
}

class UnderlyingTagsChecker final
    : public InterpolationType::ConversionChecker {
 public:
  explicit UnderlyingTagsChecker(const Vector<uint32_t>& tags) : tags_(tags) {}
  ~UnderlyingTagsChecker() final = default;

 private:
  bool IsValid(const InterpolationEnvironment&,
               const InterpolationValue& underlying) const final {
    return tags_ == GetTags(*underlying.non_interpolable_value);
  }

  const Vector<uint32_t> tags_;
};

class InheritedFontVariationSettingsChecker final
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedFontVariationSettingsChecker(
      const FontVariationSettings* settings)
      : settings_(settings) {}

  ~InheritedFontVariationSettingsChecker() final = default;

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    return ValuesEquivalent(
        settings_.get(),
        state.ParentStyle()->GetFontDescription().VariationSettings());
  }

  scoped_refptr<const FontVariationSettings> settings_;
};

static InterpolationValue ConvertFontVariationSettings(
    const FontVariationSettings* settings) {
  if (!settings || settings->size() == 0) {
    return nullptr;
  }
  wtf_size_t length = settings->size();
  auto* numbers = MakeGarbageCollected<InterpolableList>(length);
  Vector<uint32_t> tags;
  for (wtf_size_t i = 0; i < length; ++i) {
    numbers->Set(
        i, MakeGarbageCollected<InterpolableNumber>(settings->at(i).Value()));
    tags.push_back(settings->at(i).Tag());
  }
  return InterpolationValue(
      numbers,
      CSSFontVariationSettingsNonInterpolableValue::Create(std::move(tags)));
}

InterpolationValue
CSSFontVariationSettingsInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  conversion_checkers.push_back(MakeGarbageCollected<UnderlyingTagsChecker>(
      GetTags(*underlying.non_interpolable_value)));
  return InterpolationValue(underlying.interpolable_value->CloneAndZero(),
                            underlying.non_interpolable_value);
}

InterpolationValue
CSSFontVariationSettingsInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return ConvertFontVariationSettings(FontBuilder::InitialVariationSettings());
}

InterpolationValue
CSSFontVariationSettingsInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const FontVariationSettings* inherited =
      state.ParentStyle()->GetFontDescription().VariationSettings();
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedFontVariationSettingsChecker>(inherited));
  return ConvertFontVariationSettings(inherited);
}

InterpolationValue CSSFontVariationSettingsInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState* state,
    ConversionCheckers&) const {
  scoped_refptr<FontVariationSettings> settings =
      StyleBuilderConverter::ConvertFontVariationSettings(*state, value);
  return ConvertFontVariationSettings(settings.get());
}

InterpolationValue CSSFontVariationSettingsInterpolationType::
    MaybeConvertStandardPropertyUnderlyingValue(
        const ComputedStyle& style) const {
  return ConvertFontVariationSettings(
      style.GetFontDescription().VariationSettings());
}

PairwiseInterpolationValue
CSSFontVariationSettingsInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  if (TagsMatch(*start.non_interpolable_value, *end.non_interpolable_value)) {
    return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                      std::move(end.interpolable_value),
                                      std::move(start.non_interpolable_value));
  }
  return nullptr;
}

void CSSFontVariationSettingsInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  if (TagsMatch(*underlying_value_owner.Value().non_interpolable_value,
                *value.non_interpolable_value)) {
    underlying_value_owner.MutableValue().interpolable_value->ScaleAndAdd(
        underlying_fraction, *value.interpolable_value);
  } else {
    underlying_value_owner.Set(*this, value);
  }
}

void CSSFontVariationSettingsInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const auto& numbers = To<InterpolableList>(interpolable_value);
  const Vector<uint32_t>& tags = GetTags(*non_interpolable_value);
  DCHECK_EQ(numbers.length(), tags.size());

  scoped_refptr<FontVariationSettings> settings =
      FontVariationSettings::Create();
  wtf_size_t length = numbers.length();
  // Do ClampTo here, which follows the same logic as ConsumeFontVariationTag.
  for (wtf_size_t i = 0; i < length; ++i) {
    settings->Append(FontVariationAxis(
        tags[i],
        ClampTo<float>(To<InterpolableNumber>(numbers.Get(i))->Value())));
  }
  state.GetFontBuilder().SetVariationSettings(settings);
}

}  // namespace blink
