/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MATH_EXPRESSION_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MATH_EXPRESSION_NODE_H_

#include <array>
#include <optional>
#include <unordered_map>

#include "base/check_op.h"
#include "base/containers/enum_set.h"
#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_anchor_query_enums.h"
#include "third_party/blink/renderer/core/css/css_color_channel_map.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_length_resolver.h"
#include "third_party/blink/renderer/core/css/css_math_operator.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_scoped_keyword_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/layout/geometry/axis.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

static const int kMaxExpressionDepth = 100;

class CalculationExpressionNode;
class CSSNumericLiteralValue;
class CSSParserContext;
class CSSParserTokenStream;
class TryTacticTransform;
class WritingDirectionMode;
class CSSMathExpressionNode;

// The order of this enum should not change since its elements are used as
// indices in the addSubtractResult matrix.
enum CalculationResultCategory {
  kCalcNumber,
  kCalcLength,
  kCalcPercent,
  // kCalcLengthFunction is used for expressions that can't be resolved
  // before layout time, as they depend on calculated lengths.
  // This includes mixes of length and percent (or other fractional units,
  // such as vw), and also anchor queries and intrinsic size keywords
  // in calc-size(). Note that even pure numerical, non-length values
  // can fall into this category, due to functions like sign()
  // (e.g. sign(1vw - 1px) returns a numerical value, but depends on
  // a length that cannot be resolved until layout).
  kCalcLengthFunction,
  // Represents intermediate result of typed arithmetic operation, e.g.
  // 10px * 10em. It's not kCalcOther, since, once it becomes 10px * 10em /
  // 10em, it's a valid kCalcLength.
  kCalcIntermediate,
  kCalcAngle,
  kCalcTime,
  kCalcFrequency,
  kCalcResolution,
  kCalcIdent,
  kCalcOther,
};
using CalculationResultCategorySet =
    base::EnumSet<CalculationResultCategory,
                  CalculationResultCategory::kCalcNumber,
                  CalculationResultCategory::kCalcOther>;

class CSSMathType final {
  DISALLOW_NEW();

  // https://drafts.css-houdini.org/css-typed-om-1/#cssnumericvalue-base-type
  // kPercent should always be the last one as it's used to do a percent hint
  // trick in Category().
  enum BaseType : uint8_t {
    kLength,
    kAngle,
    kTime,
    kFrequency,
    kResolution,
    kFlex,
    kPercent,
    kNumTypes
  };

 public:
  CSSMathType() = default;
  // https://drafts.css-houdini.org/css-typed-om-1/#cssnumericvalue-create-a-type
  CORE_EXPORT explicit CSSMathType(CalculationResultCategory category);

  CORE_EXPORT explicit CSSMathType(const CSSMathExpressionNode&);

  CSSMathType(const CSSMathType&) = default;
  CSSMathType(CSSMathType&&) = default;
  CSSMathType& operator=(const CSSMathType&) = default;
  CSSMathType& operator=(CSSMathType&&) = default;

  static CSSMathType InvalidType();

  CORE_EXPORT bool IsValid() const;
  CORE_EXPORT CalculationResultCategory Category() const;
  bool IsIntermediateResult() const;

  friend bool operator==(const CSSMathType& lhs,
                         const CSSMathType& rhs) = default;
  // https://drafts.css-houdini.org/css-typed-om-1/#cssnumericvalue-add-two-types
  CORE_EXPORT friend CSSMathType operator+(CSSMathType type1,
                                           CSSMathType type2);
  // https://drafts.css-houdini.org/css-typed-om-1/#cssnumericvalue-multiply-two-types
  CORE_EXPORT friend CSSMathType operator*(CSSMathType type1,
                                           CSSMathType type2);
  CORE_EXPORT friend CSSMathType operator/(CSSMathType type1,
                                           CSSMathType type2);
  CORE_EXPORT CSSMathType operator-() const;
#if DCHECK_IS_ON()
  friend std::ostream& operator<<(std::ostream& os, const CSSMathType& type);
#endif

 private:
  using BaseTypePowers =
      std::array<int8_t, static_cast<size_t>(BaseType::kNumTypes)>;
  using PercentageHint = std::optional<BaseType>;

  explicit CSSMathType(bool);
  CSSMathType(BaseTypePowers types_map, PercentageHint percentage_hint);

  static CalculationResultCategory BaseTypeToCalculationCategory(
      BaseType base_type);
  static BaseType CalculationCategoryToBaseType(
      CalculationResultCategory catergory);

  // https://drafts.css-houdini.org/css-typed-om-1/#apply-the-percent-hint
  void ApplyHint(BaseType hint);

  // To represent "failure" in terms of the spec.
  bool is_valid_ = true;
  // https://drafts.css-houdini.org/css-typed-om-1/#cssnumericvalue-create-a-type
  BaseTypePowers base_type_powers_{};
  // https://drafts.css-houdini.org/css-typed-om-1/#cssnumericvalue-percent-hint
  PercentageHint percentage_hint_;
};

class CORE_EXPORT CSSMathExpressionNode
    : public GarbageCollected<CSSMathExpressionNode> {
 public:
  static CSSMathExpressionNode* Create(const CalculationValue& node);
  static CSSMathExpressionNode* Create(PixelsAndPercent pixels_and_percent);
  static CSSMathExpressionNode* Create(const CalculationExpressionNode& node);

  enum class Flag : uint8_t {
    AllowPercent,
    AllowCalcSize,
    AllowAutoInCalcSize,
    AllowContentInCalcSize,

    MinValue = AllowPercent,
    MaxValue = AllowContentInCalcSize,
  };

  using Flags = base::EnumSet<Flag, Flag::MinValue, Flag::MaxValue>;

  static CSSMathExpressionNode* ParseMathFunction(
      CSSValueID function_id,
      CSSParserTokenStream& stream,
      const CSSParserContext&,
      const Flags parsing_flags,
      CSSAnchorQueryTypes allowed_anchor_queries,
      // Variable substitutions for relative color syntax.
      // https://www.w3.org/TR/css-color-5/#relative-colors
      const CSSColorChannelMap& color_channel_map = {});

  virtual CSSMathExpressionNode* Copy() const = 0;

  // Checks if a CSS random() function is present in the value. If so, creates a
  // deep copy and binds the random value's identifier to the specified property
  // name and index. This ensures the random() function's internal identifier is
  // uniquely associated with the provided property name and value index for
  // caching purposes.
  virtual const CSSMathExpressionNode*
  CopyRandomWithPropertyNameAndValueIndexIfNeeded(
      const CSSPropertyName& property_name,
      wtf_size_t property_value_index) const {
    return this;
  }

  virtual bool IsNumericLiteral() const { return false; }
  virtual bool IsOperation() const { return false; }
  virtual bool IsAnchorQuery() const { return false; }
  virtual bool IsIdentifierLiteral() const { return false; }
  virtual bool IsKeywordLiteral() const { return false; }
  virtual bool IsContainerFeature() const { return false; }
  virtual bool IsSiblingFunction() const { return false; }
  virtual bool IsCalcSize() const { return false; }
  virtual bool IsRandomFunction() const { return false; }

  virtual bool IsMathFunction() const { return false; }

  virtual std::optional<double> GetValueIfKnown() const = 0;

  // Resolves the expression into one value *without doing any type conversion*.
  // Hits DCHECK if type conversion is required.
  virtual double DoubleValue() const = 0;

  virtual const CSSMathExpressionNode* ConvertLiteralsFromPercentageToNumber()
      const = 0;

  double ComputeNumber(const CSSLengthResolver& length_resolver) const {
    return ComputeDouble(length_resolver);
  }
  virtual double ComputeLengthPx(const CSSLengthResolver&) const = 0;
  virtual bool AccumulateLengthArray(CSSLengthArray&,
                                     double multiplier) const = 0;
  virtual void AccumulateLengthUnitTypes(
      CSSPrimitiveValue::LengthTypeFlags& types) const = 0;
  virtual const CalculationExpressionNode* ToCalculationExpression(
      const CSSLengthResolver&) const = 0;
  virtual std::optional<PixelsAndPercent> ToPixelsAndPercent(
      const CSSLengthResolver&) const = 0;

  const CalculationValue* ToCalcValue(
      const CSSLengthResolver& length_resolver,
      Length::ValueRange range,
      bool allows_negative_percentage_reference) const;

  // Evaluates the expression with type conversion (e.g., cm -> px) handled, and
  // returns the result value in the canonical unit of the corresponding
  // category (see https://www.w3.org/TR/css3-values/#canonical-unit).
  // TODO(crbug.com/984372): We currently use 'ms' as the canonical unit of
  // <time>. Switch to 's' to follow the spec.
  // Returns |nullopt| on evaluation failures due to the following reasons:
  // - The category doesn't have a canonical unit (e.g.,
  //   |kCalcLengthFunction|, |kCalcIntrinsicSize|).
  // - A type conversion that doesn't have a fixed conversion ratio is needed
  //   (e.g., between 'px' and 'em').
  // - There's an unsupported calculation, e.g., dividing two lengths.
  virtual std::optional<double> ComputeValueInCanonicalUnit() const = 0;
  // Same as ComputeValueInCanonicalUnit(), but resolves length as well.
  virtual std::optional<double> ComputeValueInCanonicalUnit(
      const CSSLengthResolver& length_resolver) const = 0;

  virtual String CustomCSSText() const = 0;
  virtual bool operator==(const CSSMathExpressionNode& other) const {
    return category_ == other.category_;
  }

  virtual bool IsComputationallyIndependent() const = 0;
  virtual bool IsElementDependent() const { return false; }
  virtual bool MayHaveRelativeUnit() const = 0;

  CalculationResultCategory Category() const { return category_; }

  // HasPercentage returns whether the toplevel result type involves a
  // percentage.  In some cases a result type having a percentage requires
  // different layout behavior (when there's nothing to resolve percentages
  // against), so this needs to be tracked accurately.  This examines the
  // cases of kCalcLengthFunction or kCalcIntrinsicSize to determine whether
  // it results from a percentage.
  virtual bool HasPercentage() const { return Category() == kCalcPercent; }

  // InvolvesLayout returns whether a percentage, an anchor query, or a
  // calc-size() keyword is used anywhere in the value, including in contexts
  // (such as the progress() function) that convert the result type of their
  // arguments into a number.
  virtual bool InvolvesLayout() const {
    return Category() == kCalcPercent || Category() == kCalcLengthFunction;
  }

  // Returns the unit type of the math expression *without doing any type
  // conversion* (e.g., 1px + 1em needs type conversion to resolve).
  // Returns |UnitType::kUnknown| if type conversion is required.
  virtual CSSPrimitiveValue::UnitType ResolvedUnitType() const = 0;

  CSSPrimitiveValue::UnitType ResolvedUnitTypeForSimplification() const {
    CSSPrimitiveValue::UnitType unit_type = ResolvedUnitType();
    if (unit_type == CSSPrimitiveValue::UnitType::kInteger) {
      return CSSPrimitiveValue::UnitType::kNumber;
    } else {
      return unit_type;
    }
  }

  bool IsNestedCalc() const { return is_nested_calc_; }
  void SetIsNestedCalc() { is_nested_calc_ = true; }

  bool HasComparisons() const { return has_comparisons_; }
  bool HasAnchorFunctions() const { return has_anchor_functions_; }
  bool IsScopedValue() const { return !needs_tree_scope_population_; }
  bool NeedsPropertyNameAndValueIndexForRandom() const {
    return needs_property_name_and_value_index_for_random_;
  }

  const CSSMathExpressionNode& EnsureScopedValue(
      const TreeScope* tree_scope) const {
    if (!needs_tree_scope_population_) {
      return *this;
    }
    return PopulateWithTreeScope(tree_scope);
  }
  virtual const CSSMathExpressionNode& PopulateWithTreeScope(
      const TreeScope*) const = 0;

#if DCHECK_IS_ON()
  // There's a subtle issue in comparing two percentages, e.g., min(10%, 20%).
  // It doesn't always resolve into 10%, because the reference value may be
  // negative. We use this to prevent comparing two percentages without knowing
  // the sign of the reference value.
  virtual bool InvolvesPercentageComparisons() const = 0;
#endif

  // Rewrite this function according to the specified TryTacticTransform,
  // e.g. anchor(left) -> anchor(right). If this function is not affected
  // by the transform, returns `this`.
  //
  // See also TryTacticTransform.
  virtual const CSSMathExpressionNode* TransformAnchors(
      LogicalAxis,
      const TryTacticTransform&,
      const WritingDirectionMode&) const = 0;

  virtual bool HasInvalidAnchorFunctions(const CSSLengthResolver&) const = 0;

  virtual void Trace(Visitor* visitor) const {}

 protected:
  CSSMathExpressionNode(CalculationResultCategory category,
                        bool has_comparisons,
                        bool has_anchor_functions,
                        bool needs_tree_scope_population)
      : category_(category),
        has_comparisons_(has_comparisons),
        has_anchor_functions_(has_anchor_functions),
        needs_tree_scope_population_(needs_tree_scope_population) {
    DCHECK_NE(category, kCalcOther);
  }

  virtual double ComputeDouble(
      const CSSLengthResolver& length_resolver) const = 0;
  static double ComputeDouble(const CSSMathExpressionNode* operand,
                              const CSSLengthResolver& length_resolver) {
    return operand->ComputeDouble(length_resolver);
  }

  CalculationResultCategory category_;
  bool is_nested_calc_ = false;
  bool has_comparisons_;
  bool has_anchor_functions_;
  bool needs_tree_scope_population_;
  bool needs_property_name_and_value_index_for_random_ = false;
};

class CORE_EXPORT CSSMathExpressionNumericLiteral final
    : public CSSMathExpressionNode {
 public:
  static CSSMathExpressionNumericLiteral* Create(
      const CSSNumericLiteralValue* value);
  static CSSMathExpressionNumericLiteral* Create(
      double value,
      CSSPrimitiveValue::UnitType type);

  explicit CSSMathExpressionNumericLiteral(const CSSNumericLiteralValue* value);

  CSSMathExpressionNode* Copy() const final { return Create(value_.Get()); }

  const CSSNumericLiteralValue& GetValue() const { return *value_; }

  bool IsNumericLiteral() const final { return true; }

  const CSSMathExpressionNode& PopulateWithTreeScope(
      const TreeScope* tree_scope) const final {
    NOTREACHED();
  }
  const CSSMathExpressionNode* TransformAnchors(
      LogicalAxis,
      const TryTacticTransform&,
      const WritingDirectionMode&) const final {
    return this;
  }

  bool HasInvalidAnchorFunctions(const CSSLengthResolver&) const final {
    return false;
  }

  const CSSMathExpressionNode* ConvertLiteralsFromPercentageToNumber()
      const final;
  String CustomCSSText() const final;
  const CalculationExpressionNode* ToCalculationExpression(
      const CSSLengthResolver&) const final;
  std::optional<PixelsAndPercent> ToPixelsAndPercent(
      const CSSLengthResolver&) const final;
  double DoubleValue() const final;
  std::optional<double> ComputeValueInCanonicalUnit() const final;
  std::optional<double> ComputeValueInCanonicalUnit(
      const CSSLengthResolver& length_resolver) const final;
  double ComputeLengthPx(const CSSLengthResolver& length_resolver) const final;
  bool AccumulateLengthArray(CSSLengthArray& length_array,
                             double multiplier) const final;
  void AccumulateLengthUnitTypes(
      CSSPrimitiveValue::LengthTypeFlags& types) const final;
  bool IsComputationallyIndependent() const final;
  bool MayHaveRelativeUnit() const final;
  bool operator==(const CSSMathExpressionNode& other) const final;
  CSSPrimitiveValue::UnitType ResolvedUnitType() const final;
  void Trace(Visitor* visitor) const final;

#if DCHECK_IS_ON()
  bool InvolvesPercentageComparisons() const final;
#endif

 protected:
  double ComputeDouble(const CSSLengthResolver& length_resolver) const final;
  std::optional<double> GetValueIfKnown() const final {
    return ComputeValueInCanonicalUnit();
  }

 private:
  Member<const CSSNumericLiteralValue> value_;
};

template <>
struct DowncastTraits<CSSMathExpressionNumericLiteral> {
  static bool AllowFrom(const CSSMathExpressionNode& node) {
    return node.IsNumericLiteral();
  }
};

// Used for container name in container-progress().
// Will possibly be used in container name for container units function.
class CORE_EXPORT CSSMathExpressionIdentifierLiteral final
    : public CSSMathExpressionNode {
 public:
  static CSSMathExpressionIdentifierLiteral* Create(AtomicString identifier) {
    return MakeGarbageCollected<CSSMathExpressionIdentifierLiteral>(
        std::move(identifier));
  }

  explicit CSSMathExpressionIdentifierLiteral(AtomicString identifier);

  CSSMathExpressionNode* Copy() const final { return Create(identifier_); }

  const AtomicString& GetValue() const { return identifier_; }

  bool IsIdentifierLiteral() const final { return true; }

  const CSSMathExpressionNode& PopulateWithTreeScope(
      const TreeScope* tree_scope) const final {
    NOTREACHED();
  }
  const CSSMathExpressionNode* TransformAnchors(
      LogicalAxis,
      const TryTacticTransform&,
      const WritingDirectionMode&) const final {
    return this;
  }

  bool HasInvalidAnchorFunctions(const CSSLengthResolver&) const final {
    return false;
  }

  const CSSMathExpressionNode* ConvertLiteralsFromPercentageToNumber()
      const final {
    return this;
  }
  String CustomCSSText() const final { return identifier_; }
  const CalculationExpressionNode* ToCalculationExpression(
      const CSSLengthResolver&) const final;
  std::optional<PixelsAndPercent> ToPixelsAndPercent(
      const CSSLengthResolver&) const final {
    return std::nullopt;
  }
  double DoubleValue() const final { NOTREACHED(); }
  std::optional<double> ComputeValueInCanonicalUnit() const final {
    return std::nullopt;
  }
  std::optional<double> ComputeValueInCanonicalUnit(
      const CSSLengthResolver& length_resolver) const final {
    NOTREACHED();
  }
  double ComputeLengthPx(const CSSLengthResolver& length_resolver) const final {
    NOTREACHED();
  }
  bool AccumulateLengthArray(CSSLengthArray& length_array,
                             double multiplier) const final {
    return false;
  }
  void AccumulateLengthUnitTypes(
      CSSPrimitiveValue::LengthTypeFlags& types) const final {}
  bool IsComputationallyIndependent() const final { return true; }
  bool MayHaveRelativeUnit() const final { return false; }
  bool operator==(const CSSMathExpressionNode& other) const final {
    return other.IsIdentifierLiteral() &&
           DynamicTo<CSSMathExpressionIdentifierLiteral>(other)->GetValue() ==
               GetValue();
  }
  CSSPrimitiveValue::UnitType ResolvedUnitType() const final {
    return CSSPrimitiveValue::UnitType::kIdent;
  }
  void Trace(Visitor* visitor) const final {
    CSSMathExpressionNode::Trace(visitor);
  }

#if DCHECK_IS_ON()
  bool InvolvesPercentageComparisons() const final { return false; }
#endif

 protected:
  double ComputeDouble(const CSSLengthResolver& length_resolver) const final {
    NOTREACHED();
  }
  std::optional<double> GetValueIfKnown() const final { return std::nullopt; }

 private:
  AtomicString identifier_;
};

template <>
struct DowncastTraits<CSSMathExpressionIdentifierLiteral> {
  static bool AllowFrom(const CSSMathExpressionNode& node) {
    return node.IsIdentifierLiteral();
  }
};

// Used for representation of the keywords, e.g. `size` keyword
// and intrinsic size keywords in calc-size(). Some of the keywords can
// be resolved to double with CSSLengthResolver.
class CORE_EXPORT CSSMathExpressionKeywordLiteral final
    : public CSSMathExpressionNode {
 public:
  enum class Context { kMediaProgress, kCalcSize, kColorChannel };

  static CSSMathExpressionKeywordLiteral* Create(CSSValueID keyword,
                                                 Context context) {
    return MakeGarbageCollected<CSSMathExpressionKeywordLiteral>(keyword,
                                                                 context);
  }

  CSSMathExpressionKeywordLiteral(CSSValueID keyword, Context context);

  CSSMathExpressionNode* Copy() const final {
    return Create(keyword_, context_);
  }

  CSSValueID GetValue() const { return keyword_; }
  Context GetContext() const { return context_; }

  bool IsKeywordLiteral() const final { return true; }

  const CSSMathExpressionNode& PopulateWithTreeScope(
      const TreeScope* tree_scope) const final {
    NOTREACHED();
  }
  const CSSMathExpressionNode* TransformAnchors(
      LogicalAxis,
      const TryTacticTransform&,
      const WritingDirectionMode&) const final {
    return this;
  }

  bool HasInvalidAnchorFunctions(const CSSLengthResolver&) const final {
    return false;
  }

  const CSSMathExpressionNode* ConvertLiteralsFromPercentageToNumber()
      const final {
    return this;
  }
  String CustomCSSText() const final {
    return GetCSSValueNameAs<AtomicString>(keyword_);
  }
  const CalculationExpressionNode* ToCalculationExpression(
      const CSSLengthResolver&) const final;
  std::optional<PixelsAndPercent> ToPixelsAndPercent(
      const CSSLengthResolver&) const final;
  double DoubleValue() const final { NOTREACHED(); }
  std::optional<double> ComputeValueInCanonicalUnit() const final {
    return std::nullopt;
  }
  std::optional<double> ComputeValueInCanonicalUnit(
      const CSSLengthResolver& length_resolver) const final {
    NOTREACHED();
  }
  double ComputeLengthPx(const CSSLengthResolver& length_resolver) const final {
    NOTREACHED();
  }
  bool AccumulateLengthArray(CSSLengthArray& length_array,
                             double multiplier) const final {
    return false;
  }
  void AccumulateLengthUnitTypes(
      CSSPrimitiveValue::LengthTypeFlags& types) const final {}
  bool IsComputationallyIndependent() const final { return true; }
  bool MayHaveRelativeUnit() const final { return false; }
  bool operator==(const CSSMathExpressionNode& other) const final {
    auto* other_keyword = DynamicTo<CSSMathExpressionKeywordLiteral>(other);
    return other_keyword && other_keyword->GetValue() == GetValue() &&
           other_keyword->GetContext() == GetContext();
  }
  CSSPrimitiveValue::UnitType ResolvedUnitType() const final {
    return CSSPrimitiveValue::UnitType::kIdent;
  }
  void Trace(Visitor* visitor) const final {
    CSSMathExpressionNode::Trace(visitor);
  }

#if DCHECK_IS_ON()
  bool InvolvesPercentageComparisons() const final { return false; }
#endif

 protected:
  double ComputeDouble(const CSSLengthResolver& length_resolver) const final;
  std::optional<double> GetValueIfKnown() const final { return std::nullopt; }

 private:
  CSSValueID keyword_;
  Context context_;
};

template <>
struct DowncastTraits<CSSMathExpressionKeywordLiteral> {
  static bool AllowFrom(const CSSMathExpressionNode& node) {
    return node.IsKeywordLiteral();
  }
};

class CORE_EXPORT CSSMathExpressionOperation final
    : public CSSMathExpressionNode {
 public:
  using Operands = HeapVector<Member<const CSSMathExpressionNode>>;

  static CSSMathExpressionNode* CreateArithmeticOperation(
      const CSSMathExpressionNode* left_side,
      const CSSMathExpressionNode* right_side,
      CSSMathOperator op);

  static CSSMathExpressionNode* CreateComparisonFunction(Operands&& operands,
                                                         CSSMathOperator op);

  static CSSMathExpressionNode* CreateTrigonometricFunction(
      Operands&& operands,
      CSSValueID function_id);

  static CSSMathExpressionNode* CreateSteppedValueFunction(Operands&& operands,
                                                           CSSMathOperator op);

  static CSSMathExpressionNode* CreateExponentialFunction(
      Operands&& operands,
      CSSValueID function_id);

  static CSSMathExpressionNode* CreateArithmeticOperationSimplified(
      const CSSMathExpressionNode* left_side,
      const CSSMathExpressionNode* right_side,
      CSSMathOperator op);

  // In addition to the simplifications in
  // CreateArithmeticOperationSimplified, this does simplifications of
  // calc-size() that are invalid for parsing, but are useful for the
  // animation code to do math on things involving calc-size() expressions
  // while keeping the calc-size() expression at the top level.  For example,
  // calc(0.5 * calc-size(auto, size)) is not valid syntax, but this lets the
  // animation code pass that multiplication to this function and have it turn
  // into calc-size(auto, 0.5 * size).
  static const CSSMathExpressionNode*
  CreateArithmeticOperationAndSimplifyCalcSize(
      const CSSMathExpressionNode* left_side,
      const CSSMathExpressionNode* right_side,
      CSSMathOperator op);

  static CSSMathExpressionNode* CreateSignRelatedFunction(
      Operands&& operands,
      CSSValueID function_id);

  static CSSMathExpressionNode* CreateInvertFunction(
      const CSSMathExpressionNode* operand);

  static CSSMathExpressionNode* CreateCalcSizeOperation(
      const CSSMathExpressionNode* left_side,
      const CSSMathExpressionNode* right_side);

  // Note: `CSSMathType type` is default for all non-arithemtic operations.
  CSSMathExpressionOperation(const CSSMathExpressionNode* left_side,
                             const CSSMathExpressionNode* right_side,
                             CSSMathOperator op,
                             CalculationResultCategory category,
                             CSSMathType type);

  // Note: `CSSMathType type` is default for all non-arithemtic operations.
  CSSMathExpressionOperation(CalculationResultCategory category,
                             Operands&& operands,
                             CSSMathOperator op,
                             CSSMathType type);

  // Note: `CSSMathType type` is default for all non-arithemtic operations.
  CSSMathExpressionOperation(CalculationResultCategory category,
                             CSSMathOperator op,
                             CSSMathType type);

  CSSMathExpressionNode* Copy() const final {
    Operands operands(operands_);
    return MakeGarbageCollected<CSSMathExpressionOperation>(
        category_, std::move(operands), operator_, type_);
  }

  const CSSMathExpressionNode* CopyRandomWithPropertyNameAndValueIndexIfNeeded(
      const CSSPropertyName& property_name,
      wtf_size_t property_value_index) const final;

  const Operands& GetOperands() const { return operands_; }
  CSSMathOperator OperatorType() const { return operator_; }

  bool IsOperation() const final { return true; }
  bool IsAddOrSubtract() const {
    return operator_ == CSSMathOperator::kAdd ||
           operator_ == CSSMathOperator::kSubtract;
  }
  bool IsMultiplyOrDivide() const {
    return operator_ == CSSMathOperator::kMultiply ||
           operator_ == CSSMathOperator::kDivide;
  }
  bool IsArithmeticOperation() const {
    return IsAddOrSubtract() || IsMultiplyOrDivide() || IsInvert();
  }
  bool AllOperandsAreNumeric() const;
  bool IsMinOrMax() const {
    return operator_ == CSSMathOperator::kMin ||
           operator_ == CSSMathOperator::kMax;
  }
  bool IsClamp() const { return operator_ == CSSMathOperator::kClamp; }
  bool IsRoundingStrategyKeyword() const {
    return CSSMathOperator::kRoundNearest <= operator_ &&
           operator_ <= CSSMathOperator::kRoundToZero && !operands_.size();
  }
  bool IsSteppedValueFunction() const {
    return CSSMathOperator::kRoundNearest <= operator_ &&
           operator_ <= CSSMathOperator::kRem;
  }
  bool IsTrigonometricFunction() const {
    return operator_ == CSSMathOperator::kHypot;
  }
  bool IsSignRelatedFunction() const {
    return operator_ == CSSMathOperator::kAbs ||
           operator_ == CSSMathOperator::kSign;
  }
  bool IsCalcSize() const override {
    return operator_ == CSSMathOperator::kCalcSize;
  }
  bool IsProgressNotation() const {
    return operator_ == CSSMathOperator::kProgress ||
           operator_ == CSSMathOperator::kMediaProgress ||
           operator_ == CSSMathOperator::kContainerProgress;
  }
  bool IsInvert() const { return operator_ == CSSMathOperator::kInvert; }

  // TODO(crbug.com/1284199): Check other math functions too.
  bool IsMathFunction() const final {
    return IsMinOrMax() || IsClamp() || IsSteppedValueFunction() ||
           IsTrigonometricFunction() || IsSignRelatedFunction() ||
           IsCalcSize() || IsProgressNotation();
  }

  bool HasPercentage() const final;
  bool InvolvesLayout() const final;

  bool HasNestedIntermediateResult() const {
    return has_nested_intermediate_result_;
  }

  String CSSTextAsClamp() const;

  const CSSMathType& Type() const { return type_; }

  const CSSMathExpressionNode* ConvertLiteralsFromPercentageToNumber()
      const final;
  const CalculationExpressionNode* ToCalculationExpression(
      const CSSLengthResolver&) const final;
  std::optional<PixelsAndPercent> ToPixelsAndPercent(
      const CSSLengthResolver&) const final;
  double DoubleValue() const final;
  std::optional<double> ComputeValueInCanonicalUnit() const final;
  std::optional<double> ComputeValueInCanonicalUnit(
      const CSSLengthResolver& length_resolver) const final;
  double ComputeLengthPx(const CSSLengthResolver& length_resolver) const final;
  bool AccumulateLengthArray(CSSLengthArray& length_array,
                             double multiplier) const final;
  void AccumulateLengthUnitTypes(
      CSSPrimitiveValue::LengthTypeFlags& types) const final;
  bool IsComputationallyIndependent() const final;
  bool IsElementDependent() const final;
  bool MayHaveRelativeUnit() const final;
  String CustomCSSText() const final;
  bool operator==(const CSSMathExpressionNode& exp) const final;
  CSSPrimitiveValue::UnitType ResolvedUnitType() const final;
  const CSSMathExpressionNode& PopulateWithTreeScope(
      const TreeScope*) const final;
  const CSSMathExpressionNode* TransformAnchors(
      LogicalAxis,
      const TryTacticTransform&,
      const WritingDirectionMode&) const final;
  bool HasInvalidAnchorFunctions(const CSSLengthResolver&) const final;
  void Trace(Visitor* visitor) const final;

#if DCHECK_IS_ON()
  bool InvolvesPercentageComparisons() const final;
#endif

 protected:
  double ComputeDouble(const CSSLengthResolver& length_resolver) const final;
  std::optional<double> GetValueIfKnown() const final {
    return ComputeValueInCanonicalUnit();
  }

 private:
  static const CSSMathExpressionNode* GetNumericLiteralSide(
      const CSSMathExpressionNode* left_side,
      const CSSMathExpressionNode* right_side);

  double Evaluate(const Vector<double>& operands) const {
    return EvaluateOperator(operands, operator_);
  }

  static double EvaluateOperator(const Vector<double>& operands,
                                 CSSMathOperator op);

  // Helper for iterating from the 2nd to the last operands
  base::span<const Member<const CSSMathExpressionNode>> SecondToLastOperands()
      const {
    return base::span(operands_).subspan<1>();
  }

  // If operation has some nested operand which is of an intermediate type,
  // e.g. 10px * 20px or 1px / 1px. Used to ban simplifications on such
  // operations.
  bool has_nested_intermediate_result_ = false;
  Operands operands_;
  const CSSMathOperator operator_;
  const CSSMathType type_;
};

template <>
struct DowncastTraits<CSSMathExpressionOperation> {
  static bool AllowFrom(const CSSMathExpressionNode& node) {
    return node.IsOperation();
  }
};

class CORE_EXPORT CSSMathExpressionContainerFeature final
    : public CSSMathExpressionNode {
 public:
  CSSMathExpressionContainerFeature(const CSSIdentifierValue* size_feature,
                                    const CSSCustomIdentValue* container_name);

  CSSMathExpressionNode* Copy() const final {
    return MakeGarbageCollected<CSSMathExpressionContainerFeature>(
        size_feature_, container_name_);
  }

  bool IsContainerFeature() const final { return true; }

  const CSSMathExpressionNode& PopulateWithTreeScope(
      const TreeScope* tree_scope) const final {
    const auto* container_name =
        container_name_ ? To<CSSCustomIdentValue>(
                              &container_name_->EnsureScopedValue(tree_scope))
                        : nullptr;
    return *MakeGarbageCollected<CSSMathExpressionContainerFeature>(
        size_feature_, container_name);
  }
  const CSSMathExpressionNode* TransformAnchors(
      LogicalAxis axis,
      const TryTacticTransform& transform,
      const WritingDirectionMode& mode) const final {
    return this;
  }
  bool HasInvalidAnchorFunctions(const CSSLengthResolver&) const final {
    return false;
  }

  CSSValueID GetValue() const { return size_feature_->GetValueID(); }

  const CSSMathExpressionNode* ConvertLiteralsFromPercentageToNumber()
      const final {
    return this;
  }
  String CustomCSSText() const final;
  const CalculationExpressionNode* ToCalculationExpression(
      const CSSLengthResolver&) const final;
  std::optional<PixelsAndPercent> ToPixelsAndPercent(
      const CSSLengthResolver&) const final;
  double DoubleValue() const final { NOTREACHED(); }
  std::optional<double> ComputeValueInCanonicalUnit() const final {
    return std::nullopt;
  }
  std::optional<double> ComputeValueInCanonicalUnit(
      const CSSLengthResolver& length_resolver) const final {
    NOTREACHED();
  }
  double ComputeLengthPx(const CSSLengthResolver& length_resolver) const final {
    NOTREACHED();
  }
  bool AccumulateLengthArray(CSSLengthArray& length_array,
                             double multiplier) const final {
    return false;
  }
  void AccumulateLengthUnitTypes(
      CSSPrimitiveValue::LengthTypeFlags& types) const final {}
  bool IsComputationallyIndependent() const final { return true; }
  bool MayHaveRelativeUnit() const final { return false; }
  bool operator==(const CSSMathExpressionNode& other) const final {
    auto* other_progress = DynamicTo<CSSMathExpressionContainerFeature>(other);
    return other_progress &&
           base::ValuesEquivalent(other_progress->size_feature_,
                                  size_feature_) &&
           base::ValuesEquivalent(other_progress->container_name_,
                                  container_name_);
  }
  CSSPrimitiveValue::UnitType ResolvedUnitType() const final {
    return CSSPrimitiveValue::UnitType::kNumber;
  }
  void Trace(Visitor* visitor) const final {
    visitor->Trace(size_feature_);
    visitor->Trace(container_name_);
    CSSMathExpressionNode::Trace(visitor);
  }

#if DCHECK_IS_ON()
  bool InvolvesPercentageComparisons() const final { return false; }
#endif

 protected:
  double ComputeDouble(const CSSLengthResolver& length_resolver) const final;
  std::optional<double> GetValueIfKnown() const final { return std::nullopt; }

 private:
  Member<const CSSIdentifierValue> size_feature_;
  Member<const CSSCustomIdentValue> container_name_;
};

template <>
struct DowncastTraits<CSSMathExpressionContainerFeature> {
  static bool AllowFrom(const CSSMathExpressionNode& node) {
    return node.IsContainerFeature();
  }
};

// anchor() and anchor-size()
class CORE_EXPORT CSSMathExpressionAnchorQuery final
    : public CSSMathExpressionNode {
 public:
  CSSMathExpressionAnchorQuery(CSSAnchorQueryType type,
                               const CSSValue* anchor_specifier,
                               const CSSValue* value,
                               const CSSPrimitiveValue* fallback);

  CSSMathExpressionNode* Copy() const final {
    return MakeGarbageCollected<CSSMathExpressionAnchorQuery>(
        type_, anchor_specifier_, value_, fallback_);
  }

  bool IsAnchor() const { return type_ == CSSAnchorQueryType::kAnchor; }
  bool IsAnchorSize() const { return type_ == CSSAnchorQueryType::kAnchorSize; }

  // TODO(crbug.com/40059176): This is not entirely correct, since "math
  // function" should refer to functions defined in [1]. We may need to clean up
  // the terminology in the code.
  // [1] https://drafts.csswg.org/css-values-4/#math
  bool IsMathFunction() const final { return true; }

  bool IsAnchorQuery() const final { return true; }
  const CSSMathExpressionNode* ConvertLiteralsFromPercentageToNumber()
      const final {
    return this;
  }
  CSSPrimitiveValue::UnitType ResolvedUnitType() const final {
    return CSSPrimitiveValue::UnitType::kUnknown;
  }
  std::optional<double> ComputeValueInCanonicalUnit() const final {
    return std::nullopt;
  }
  std::optional<double> ComputeValueInCanonicalUnit(
      const CSSLengthResolver& length_resolver) const final {
    NOTREACHED();
  }
  std::optional<PixelsAndPercent> ToPixelsAndPercent(
      const CSSLengthResolver&) const final {
    return std::nullopt;
  }
  bool AccumulateLengthArray(CSSLengthArray& length_array,
                             double multiplier) const final {
    return false;
  }
  bool IsComputationallyIndependent() const final { return false; }
  bool MayHaveRelativeUnit() const final { return false; }
  double DoubleValue() const final;
  double ComputeLengthPx(const CSSLengthResolver& length_resolver) const final;
  void AccumulateLengthUnitTypes(
      CSSPrimitiveValue::LengthTypeFlags& types) const final {
    // AccumulateLengthUnitTypes() is only used when interpolating the
    // 'transform' property, where anchor queries are not allowed.
    NOTREACHED();
  }

  String CustomCSSText() const final;
  const CalculationExpressionNode* ToCalculationExpression(
      const CSSLengthResolver&) const final;
  bool operator==(const CSSMathExpressionNode& other) const final;
  const CSSMathExpressionNode& PopulateWithTreeScope(
      const TreeScope*) const final;
  void Trace(Visitor* visitor) const final;

#if DCHECK_IS_ON()
  bool InvolvesPercentageComparisons() const final { return false; }
#endif

  const CSSMathExpressionNode* TransformAnchors(
      LogicalAxis,
      const TryTacticTransform&,
      const WritingDirectionMode&) const final;
  bool HasInvalidAnchorFunctions(const CSSLengthResolver&) const final;

 protected:
  double ComputeDouble(const CSSLengthResolver&) const final;
  std::optional<double> GetValueIfKnown() const final { return std::nullopt; }

 private:
  std::optional<LayoutUnit> EvaluateQuery(const AnchorQuery& query,
                                          const CSSLengthResolver&) const;
  AnchorQuery ToQuery(const CSSLengthResolver& length_resolver) const;

  CSSAnchorQueryType type_;
  Member<const CSSValue> anchor_specifier_;
  Member<const CSSValue> value_;
  Member<const CSSPrimitiveValue> fallback_;
};

template <>
struct DowncastTraits<CSSMathExpressionAnchorQuery> {
  static bool AllowFrom(const CSSMathExpressionNode& node) {
    return node.IsAnchorQuery();
  }
};

// sibling-index() and sibling-count()
class CORE_EXPORT CSSMathExpressionSiblingFunction final
    : public CSSMathExpressionNode {
 public:
  explicit CSSMathExpressionSiblingFunction(
      const cssvalue::CSSScopedKeywordValue* function)
      : CSSMathExpressionNode(kCalcNumber,
                              /*has_comparisons=*/false,
                              /*has_anchor_functions=*/false,
                              /*needs_tree_scope_population=*/true),
        function_(function) {}

  // TODO(crbug.com/40059176): This is not entirely correct, since "math
  // function" should refer to functions defined in [1]. We may need to clean up
  // the terminology in the code.
  // [1] https://drafts.csswg.org/css-values-4/#math
  bool IsMathFunction() const final { return true; }

  CSSMathExpressionNode* Copy() const override {
    return MakeGarbageCollected<CSSMathExpressionSiblingFunction>(function_);
  }

  bool IsSiblingFunction() const override { return true; }

  const CSSMathExpressionNode* ConvertLiteralsFromPercentageToNumber()
      const final {
    return this;
  }
  CSSPrimitiveValue::UnitType ResolvedUnitType() const final {
    return CSSPrimitiveValue::UnitType::kNumber;
  }
  std::optional<double> ComputeValueInCanonicalUnit() const final {
    return std::nullopt;
  }
  std::optional<double> ComputeValueInCanonicalUnit(
      const CSSLengthResolver& length_resolver) const final;
  std::optional<PixelsAndPercent> ToPixelsAndPercent(
      const CSSLengthResolver&) const final {
    return std::nullopt;
  }
  bool AccumulateLengthArray(CSSLengthArray& length_array,
                             double multiplier) const final {
    return false;
  }
  bool IsComputationallyIndependent() const final { return false; }
  bool IsElementDependent() const final { return true; }
  bool MayHaveRelativeUnit() const final { return false; }
  double DoubleValue() const final { NOTREACHED(); }
  double ComputeLengthPx(const CSSLengthResolver& length_resolver) const final {
    NOTREACHED();
  }
  void AccumulateLengthUnitTypes(
      CSSPrimitiveValue::LengthTypeFlags& types) const final {}

  String CustomCSSText() const final;
  const CalculationExpressionNode* ToCalculationExpression(
      const CSSLengthResolver&) const final;
  bool operator==(const CSSMathExpressionNode& other) const final;
  const CSSMathExpressionNode& PopulateWithTreeScope(
      const TreeScope*) const final;

#if DCHECK_IS_ON()
  bool InvolvesPercentageComparisons() const final { return false; }
#endif

  const CSSMathExpressionNode* TransformAnchors(
      LogicalAxis,
      const TryTacticTransform&,
      const WritingDirectionMode&) const final {
    return this;
  }

  bool HasInvalidAnchorFunctions(const CSSLengthResolver&) const final {
    return false;
  }

  void Trace(Visitor* visitor) const final;

 protected:
  double ComputeDouble(const CSSLengthResolver&) const final;
  std::optional<double> GetValueIfKnown() const final { return std::nullopt; }

 private:
  std::optional<LayoutUnit> EvaluateQuery(const AnchorQuery& query,
                                          const CSSLengthResolver&) const;
  AnchorQuery ToQuery(const CSSLengthResolver& length_resolver) const;

  Member<const cssvalue::CSSScopedKeywordValue> function_;
};

template <>
struct DowncastTraits<CSSMathExpressionSiblingFunction> {
  static bool AllowFrom(const CSSMathExpressionNode& node) {
    return node.IsSiblingFunction();
  }
};

// <random-value-sharing> = [ [ auto | <dashed-ident> ] || element-shared ]
//                          | fixed <number [0,1]>
// https://drafts.csswg.org/css-values-5/#typedef-random-value-sharing
class RandomValueSharing : public GarbageCollected<RandomValueSharing> {
 public:
  static const RandomValueSharing* Parse(CSSParserTokenStream& stream,
                                         const CSSParserContext&);
  static const RandomValueSharing* Auto() {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        ThreadSpecific<Persistent<RandomValueSharing>>, thread_specific_random,
        ());

    Persistent<RandomValueSharing>& random_value_sharing =
        *thread_specific_random;
    if (!random_value_sharing) {
      random_value_sharing = MakeGarbageCollected<RandomValueSharing>();
      LEAK_SANITIZER_IGNORE_OBJECT(&random_value_sharing);
    }
    return random_value_sharing;
  }
  static const RandomValueSharing* Fixed(double fixed_value);
  // Returns the current object if a name is already set or if it's fixed
  // value. Otherwise, returns a copy with the name bound to the specified
  // property name and index.
  const RandomValueSharing* CopyWithPropertyValueIndexNameIfNeeded(
      const CSSPropertyName& property_name,
      wtf_size_t property_value_index) const;

  RandomValueSharing() = default;

  using ElementShared = base::StrongAlias<class ElementSharedTag, bool>;
  explicit RandomValueSharing(ElementShared element_shared)
      : value_(NameAndElementShared(element_shared)) {}
  RandomValueSharing(AtomicString name, ElementShared element_shared)
      : value_(NameAndElementShared(name, element_shared)) {}
  explicit RandomValueSharing(const CSSPrimitiveValue* fixed_value)
      : value_(fixed_value) {}

  bool IsFixed() const;
  const CSSPrimitiveValue* GetFixed() const;
  bool IsAuto() const;
  AtomicString Name() const;
  bool IsElementShared() const;

  bool operator==(const RandomValueSharing& other) const;
  String CssText() const;
  void Trace(Visitor* visitor) const;

 private:
  // Used for non fixed <random-value-sharing> values, i.e.:
  // [ [ auto | <dashed-ident> ] || element-shared ]
  // "name" can refer to either the property name and property value index, or
  // the random identifier. NameAndElementShared are created without a "name"
  // when random identifier is not provided. But they will be replaced later
  // populated with the property name and property value index "name".
  struct NameAndElementShared {
    NameAndElementShared() = default;
    explicit NameAndElementShared(ElementShared element_shared)
        : element_shared(element_shared) {}
    NameAndElementShared(AtomicString random_name, ElementShared element_shared)
        : name(random_name), element_shared(element_shared) {}
    bool operator==(const NameAndElementShared& other) const {
      return name == other.name && element_shared == other.element_shared;
    }
    AtomicString name;
    ElementShared element_shared = ElementShared(false);
  };
  std::variant<NameAndElementShared, Member<const CSSPrimitiveValue>> value_ =
      NameAndElementShared();
};

// <random()> = random( <random-value-sharing>? , <calc-sum>, <calc-sum>,
// <calc-sum>? ) https://drafts.csswg.org/css-values-5/#random
class CORE_EXPORT CSSMathExpressionRandomFunction final
    : public CSSMathExpressionNode {
 public:
  explicit CSSMathExpressionRandomFunction(
      base::PassKey<CSSMathExpressionRandomFunction>,
      CalculationResultCategory category,
      const RandomValueSharing* random_value_sharing,
      const CSSMathExpressionNode* min,
      const CSSMathExpressionNode* max,
      const CSSMathExpressionNode* step);

  static CSSMathExpressionRandomFunction* Create(
      const RandomValueSharing* random_value_sharing,
      HeapVector<Member<const CSSMathExpressionNode>>&& nodes);

  CSSMathExpressionNode* Copy() const override;
  const CSSMathExpressionNode* CopyRandomWithPropertyNameAndValueIndexIfNeeded(
      const CSSPropertyName& property_name,
      wtf_size_t property_value_index) const final;
  bool IsRandomFunction() const final { return true; }
  double DoubleValue() const final { NOTREACHED(); }
  const CSSMathExpressionNode* ConvertLiteralsFromPercentageToNumber()
      const final {
    return this;
  }
  double ComputeLengthPx(const CSSLengthResolver&) const final;
  bool AccumulateLengthArray(CSSLengthArray&, double multiplier) const final {
    return false;
  }
  void AccumulateLengthUnitTypes(
      CSSPrimitiveValue::LengthTypeFlags& types) const final;
  const CalculationExpressionNode* ToCalculationExpression(
      const CSSLengthResolver&) const final;
  std::optional<PixelsAndPercent> ToPixelsAndPercent(
      const CSSLengthResolver&) const final {
    return std::nullopt;
  }
  std::optional<double> ComputeValueInCanonicalUnit() const final {
    NOTREACHED();
  }
  std::optional<double> ComputeValueInCanonicalUnit(
      const CSSLengthResolver& length_resolver) const final {
    NOTREACHED();
  }
  String CustomCSSText() const final;
  bool operator==(const CSSMathExpressionNode& other) const final;
  bool IsComputationallyIndependent() const final;
  bool IsElementDependent() const final;
  // TODO(crbug.com/40059176): This is not entirely correct, since "math
  // function" should refer to functions defined in [1]. We may need to clean up
  // the terminology in the code.
  // [1] https://drafts.csswg.org/css-values-4/#math
  bool IsMathFunction() const final { return true; }
  bool MayHaveRelativeUnit() const final;
  CSSPrimitiveValue::UnitType ResolvedUnitType() const final;
  const CSSMathExpressionNode& PopulateWithTreeScope(
      const TreeScope*) const final {
    NOTREACHED();
  }
#if DCHECK_IS_ON()
  bool InvolvesPercentageComparisons() const final;
#endif
  const CSSMathExpressionNode* TransformAnchors(
      LogicalAxis,
      const TryTacticTransform&,
      const WritingDirectionMode&) const final {
    NOTREACHED();
  }
  bool HasInvalidAnchorFunctions(const CSSLengthResolver&) const final;
  const RandomValueSharing* GetRandomValueSharing() const {
    return random_value_sharing_;
  }
  const CSSMathExpressionNode* Min() const { return min_; }
  const CSSMathExpressionNode* Max() const { return max_; }
  const CSSMathExpressionNode* Step() const { return step_; }
  void Trace(Visitor* visitor) const final;

 protected:
  double ComputeDouble(const CSSLengthResolver&) const final;
  std::optional<double> GetValueIfKnown() const final { return std::nullopt; }

 private:
  Member<const RandomValueSharing> random_value_sharing_;
  Member<const CSSMathExpressionNode> min_;
  Member<const CSSMathExpressionNode> max_;
  Member<const CSSMathExpressionNode> step_;
};

template <>
struct DowncastTraits<CSSMathExpressionRandomFunction> {
  static bool AllowFrom(const CSSMathExpressionNode& node) {
    return node.IsRandomFunction();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MATH_EXPRESSION_NODE_H_
