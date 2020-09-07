// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_OPERATOR_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_OPERATOR_ELEMENT_H_

#include "third_party/blink/renderer/core/mathml/mathml_element.h"

namespace blink {

class ComputedStyle;
class CSSToLengthConversionData;
class Document;

enum class MathMLOperatorDictionaryCategory : uint8_t;

// Math units are 1/18em.
constexpr double kMathUnitFraction = 1.0 / 18.0;

class CORE_EXPORT MathMLOperatorElement final : public MathMLElement {
 public:
  explicit MathMLOperatorElement(Document&);

  struct OperatorContent {
    String characters;
    bool is_vertical = true;
  };
  enum OperatorPropertyFlag {
    kStretchy = 0x1,
    kSymmetric = 0x2,
    kLargeOp = 0x4,
    kMovableLimits = 0x8,
  };
  // Query whether given flag is set in the operator dictionary.
  bool HasBooleanProperty(OperatorPropertyFlag);

  void AddMathLSpaceIfNeeded(ComputedStyle&, const CSSToLengthConversionData&);
  void AddMathRSpaceIfNeeded(ComputedStyle&, const CSSToLengthConversionData&);
  void AddMathMinSizeIfNeeded(ComputedStyle&, const CSSToLengthConversionData&);
  void AddMathMaxSizeIfNeeded(ComputedStyle&, const CSSToLengthConversionData&);
  const OperatorContent& GetOperatorContent();

  double DefaultLeadingSpace();
  double DefaultTrailingSpace();

 private:
  base::Optional<OperatorContent> operator_content_;
  // Operator properties calculated from dictionary and attributes.
  // It contains dirty flags to allow efficient dictionary updating.
  struct Properties {
    MathMLOperatorDictionaryCategory dictionary_category;
    unsigned flags : 4;
    unsigned dirty_flags : 4;
  };
  Properties properties_;
  void ComputeDictionaryCategory();
  void ComputeOperatorProperty(OperatorPropertyFlag);
  void SetOperatorFormDirty();
  void ParseAttribute(const AttributeModificationParams&) final;
  void SetOperatorPropertyDirtyFlagIfNeeded(const AttributeModificationParams&,
                                            const OperatorPropertyFlag&,
                                            bool& needs_layout);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_OPERATOR_ELEMENT_H_
