// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_operator_element.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/text/mathml_operator_dictionary.h"

namespace blink {

namespace {

static const uint32_t kOperatorPropertyFlagsAll =
    MathMLOperatorElement::kStretchy | MathMLOperatorElement::kSymmetric |
    MathMLOperatorElement::kLargeOp | MathMLOperatorElement::kMovableLimits;
static const uint32_t kOperatorPropertyFlagsNone = 0;

// https://w3c.github.io/mathml-core/#operator-dictionary-categories-values
// Leading and trailing spaces are respresented in math units, i.e. 1/18em.
struct MathMLOperatorDictionaryProperties {
  unsigned leading_space_in_math_unit : 3;
  unsigned trailing_space_in_math_unit : 3;
  unsigned flags : 4;
};
static const auto MathMLOperatorDictionaryCategories =
    std::to_array<MathMLOperatorDictionaryProperties>({
        {5, 5, kOperatorPropertyFlagsNone},        // None (default values)
        {5, 5, kOperatorPropertyFlagsNone},        // ForceDefault
        {5, 5, MathMLOperatorElement::kStretchy},  // Category A
        {4, 4, kOperatorPropertyFlagsNone},        // Category B
        {3, 3, kOperatorPropertyFlagsNone},        // Category C
        {0, 0, kOperatorPropertyFlagsNone},        // Categories D, E, K
        {0, 0,
         MathMLOperatorElement::kStretchy |
             MathMLOperatorElement::kSymmetric},  // Categories F, G
        {3, 3,
         MathMLOperatorElement::kSymmetric |
             MathMLOperatorElement::kLargeOp},     // Category H
        {0, 0, MathMLOperatorElement::kStretchy},  // Category I
        {3, 3,
         MathMLOperatorElement::kSymmetric | MathMLOperatorElement::kLargeOp |
             MathMLOperatorElement::kMovableLimits},  // Category J
        {3, 0, kOperatorPropertyFlagsNone},           // Category L
        {0, 3, kOperatorPropertyFlagsNone},           // Category M
    });

static const QualifiedName& OperatorPropertyFlagToAttributeName(
    MathMLOperatorElement::OperatorPropertyFlag flag) {
  switch (flag) {
    case MathMLOperatorElement::kLargeOp:
      return mathml_names::kLargeopAttr;
    case MathMLOperatorElement::kMovableLimits:
      return mathml_names::kMovablelimitsAttr;
    case MathMLOperatorElement::kStretchy:
      return mathml_names::kStretchyAttr;
    case MathMLOperatorElement::kSymmetric:
      return mathml_names::kSymmetricAttr;
  }
  NOTREACHED_IN_MIGRATION();
  return g_null_name;
}

}  // namespace

MathMLOperatorElement::MathMLOperatorElement(Document& doc)
    : MathMLTokenElement(mathml_names::kMoTag, doc) {
  properties_.dictionary_category =
      MathMLOperatorDictionaryCategory::kUndefined;
  properties_.dirty_flags = kOperatorPropertyFlagsAll;
}

void MathMLOperatorElement::ChildrenChanged(
    const ChildrenChange& children_change) {
  properties_.dictionary_category =
      MathMLOperatorDictionaryCategory::kUndefined;
  properties_.dirty_flags = kOperatorPropertyFlagsAll;
  MathMLTokenElement::ChildrenChanged(children_change);
}

void MathMLOperatorElement::SetOperatorPropertyDirtyFlagIfNeeded(
    const AttributeModificationParams& param,
    const OperatorPropertyFlag& flag,
    bool& needs_layout) {
  needs_layout = param.new_value != param.old_value;
  if (needs_layout)
    properties_.dirty_flags |= flag;
}

void MathMLOperatorElement::ParseAttribute(
    const AttributeModificationParams& param) {
  bool needs_layout = false;
  if (param.name == mathml_names::kFormAttr) {
    needs_layout = param.new_value != param.old_value;
    if (needs_layout) {
      SetOperatorFormDirty();
      properties_.dirty_flags |= kOperatorPropertyFlagsAll;
    }
  } else if (param.name == mathml_names::kStretchyAttr) {
    SetOperatorPropertyDirtyFlagIfNeeded(
        param, MathMLOperatorElement::kStretchy, needs_layout);
  } else if (param.name == mathml_names::kSymmetricAttr) {
    SetOperatorPropertyDirtyFlagIfNeeded(
        param, MathMLOperatorElement::kSymmetric, needs_layout);
  } else if (param.name == mathml_names::kLargeopAttr) {
    SetOperatorPropertyDirtyFlagIfNeeded(param, MathMLOperatorElement::kLargeOp,
                                         needs_layout);
  } else if (param.name == mathml_names::kMovablelimitsAttr) {
    SetOperatorPropertyDirtyFlagIfNeeded(
        param, MathMLOperatorElement::kMovableLimits, needs_layout);
  } else if (param.name == mathml_names::kLspaceAttr ||
             param.name == mathml_names::kRspaceAttr ||
             param.name == mathml_names::kMinsizeAttr ||
             param.name == mathml_names::kMaxsizeAttr) {
    if (param.new_value != param.old_value) {
      SetNeedsStyleRecalc(
          kLocalStyleChange,
          StyleChangeReasonForTracing::Create(style_change_reason::kAttribute));
    }
  }
  if (needs_layout && GetLayoutObject() && GetLayoutObject()->IsMathML()) {
    GetLayoutObject()
        ->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
            layout_invalidation_reason::kAttributeChanged);
  }
  MathMLTokenElement::ParseAttribute(param);
}

// https://w3c.github.io/mathml-core/#dfn-algorithm-for-determining-the-properties-of-an-embellished-operator
void MathMLOperatorElement::ComputeDictionaryCategory() {
  if (properties_.dictionary_category !=
      MathMLOperatorDictionaryCategory::kUndefined)
    return;
  if (GetTokenContent().characters.empty()) {
    properties_.dictionary_category = MathMLOperatorDictionaryCategory::kNone;
    return;
  }

  // We first determine the form attribute and use the default spacing and
  // properties.
  // https://w3c.github.io/mathml-core/#dfn-form
  const auto& value = FastGetAttribute(mathml_names::kFormAttr);
  bool explicit_form = true;
  MathMLOperatorDictionaryForm form;
  if (EqualIgnoringASCIICase(value, "prefix")) {
    form = MathMLOperatorDictionaryForm::kPrefix;
  } else if (EqualIgnoringASCIICase(value, "infix")) {
    form = MathMLOperatorDictionaryForm::kInfix;
  } else if (EqualIgnoringASCIICase(value, "postfix")) {
    form = MathMLOperatorDictionaryForm::kPostfix;
  } else {
    // TODO(crbug.com/1121113): Implement the remaining rules for determining
    // form.
    // https://w3c.github.io/mathml-core/#dfn-algorithm-for-determining-the-form-of-an-embellished-operator
    explicit_form = false;
    bool nextSibling = ElementTraversal::NextSibling(*this);
    bool prevSibling = ElementTraversal::PreviousSibling(*this);
    if (!prevSibling && nextSibling)
      form = MathMLOperatorDictionaryForm::kPrefix;
    else if (prevSibling && !nextSibling)
      form = MathMLOperatorDictionaryForm::kPostfix;
    else
      form = MathMLOperatorDictionaryForm::kInfix;
  }

  // We then try and find an entry in the operator dictionary to override the
  // default values.
  // https://w3c.github.io/mathml-core/#dfn-algorithm-for-determining-the-properties-of-an-embellished-operator
  auto category = FindCategory(GetTokenContent().characters, form);
  if (category != MathMLOperatorDictionaryCategory::kNone) {
    // Step 2.
    properties_.dictionary_category = category;
  } else {
    if (!explicit_form) {
      // Step 3.
      for (const auto& fallback_form :
           {MathMLOperatorDictionaryForm::kInfix,
            MathMLOperatorDictionaryForm::kPostfix,
            MathMLOperatorDictionaryForm::kPrefix}) {
        if (fallback_form == form)
          continue;
        category = FindCategory(
            GetTokenContent().characters,
            static_cast<MathMLOperatorDictionaryForm>(fallback_form));
        if (category != MathMLOperatorDictionaryCategory::kNone) {
          properties_.dictionary_category = category;
          return;
        }
      }
    }
    // Step 4.
    properties_.dictionary_category = MathMLOperatorDictionaryCategory::kNone;
  }
}

void MathMLOperatorElement::ComputeOperatorProperty(OperatorPropertyFlag flag) {
  DCHECK(properties_.dirty_flags & flag);
  const auto& name = OperatorPropertyFlagToAttributeName(flag);
  if (std::optional<bool> value = BooleanAttribute(name)) {
    // https://w3c.github.io/mathml-core/#dfn-algorithm-for-determining-the-properties-of-an-embellished-operator
    // Step 1.
    if (*value) {
      properties_.flags |= flag;
    } else {
      properties_.flags &= ~flag;
    }
  } else {
    // By default, the value specified in the operator dictionary are used.
    ComputeDictionaryCategory();
    DCHECK(properties_.dictionary_category !=
           MathMLOperatorDictionaryCategory::kUndefined);
    if (MathMLOperatorDictionaryCategories
            [std::underlying_type_t<MathMLOperatorDictionaryCategory>(
                 properties_.dictionary_category)]
                .flags &
        flag) {
      properties_.flags |= flag;
    } else {
      properties_.flags &= ~flag;
    }
  }
}

bool MathMLOperatorElement::IsVertical() {
  if (!is_vertical_.has_value()) {
    is_vertical_ =
        Character::IsVerticalMathCharacter(GetTokenContent().code_point);
  }
  return is_vertical_.value();
}

bool MathMLOperatorElement::HasBooleanProperty(OperatorPropertyFlag flag) {
  if (properties_.dirty_flags & flag) {
    ComputeOperatorProperty(flag);
    properties_.dirty_flags &= ~flag;
  }
  return properties_.flags & flag;
}

void MathMLOperatorElement::CheckFormAfterSiblingChange() {
  if (properties_.dictionary_category !=
          MathMLOperatorDictionaryCategory::kUndefined &&
      !FastHasAttribute(mathml_names::kFormAttr))
    SetOperatorFormDirty();
}

void MathMLOperatorElement::SetOperatorFormDirty() {
  properties_.dictionary_category =
      MathMLOperatorDictionaryCategory::kUndefined;
}

void MathMLOperatorElement::AddMathLSpaceIfNeeded(
    ComputedStyleBuilder& builder,
    const CSSToLengthConversionData& conversion_data) {
  if (auto length_or_percentage_value = AddMathLengthToComputedStyle(
          conversion_data, mathml_names::kLspaceAttr)) {
    builder.SetMathLSpace(std::move(*length_or_percentage_value));
  }
}

void MathMLOperatorElement::AddMathRSpaceIfNeeded(
    ComputedStyleBuilder& builder,
    const CSSToLengthConversionData& conversion_data) {
  if (auto length_or_percentage_value = AddMathLengthToComputedStyle(
          conversion_data, mathml_names::kRspaceAttr)) {
    builder.SetMathRSpace(std::move(*length_or_percentage_value));
  }
}

void MathMLOperatorElement::AddMathMinSizeIfNeeded(
    ComputedStyleBuilder& builder,
    const CSSToLengthConversionData& conversion_data) {
  if (auto length_or_percentage_value = AddMathLengthToComputedStyle(
          conversion_data, mathml_names::kMinsizeAttr)) {
    builder.SetMathMinSize(std::move(*length_or_percentage_value));
  }
}

void MathMLOperatorElement::AddMathMaxSizeIfNeeded(
    ComputedStyleBuilder& builder,
    const CSSToLengthConversionData& conversion_data) {
  if (auto length_or_percentage_value = AddMathLengthToComputedStyle(
          conversion_data, mathml_names::kMaxsizeAttr)) {
    builder.SetMathMaxSize(std::move(*length_or_percentage_value));
  }
}

double MathMLOperatorElement::DefaultLeadingSpace() {
  ComputeDictionaryCategory();
  return static_cast<float>(
             MathMLOperatorDictionaryCategories
                 [std::underlying_type_t<MathMLOperatorDictionaryCategory>(
                      properties_.dictionary_category)]
                     .leading_space_in_math_unit) *
         kMathUnitFraction;
}

double MathMLOperatorElement::DefaultTrailingSpace() {
  ComputeDictionaryCategory();
  return static_cast<float>(
             MathMLOperatorDictionaryCategories
                 [std::underlying_type_t<MathMLOperatorDictionaryCategory>(
                      properties_.dictionary_category)]
                     .trailing_space_in_math_unit) *
         kMathUnitFraction;
}

}  // namespace blink
