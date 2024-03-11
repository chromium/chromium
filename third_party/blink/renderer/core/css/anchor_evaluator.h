// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ANCHOR_EVALUATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ANCHOR_EVALUATOR_H_

#include <optional>
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_anchor_query_enums.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class AnchorQuery;
class AnchorScope;
class AnchorSpecifierValue;

class CORE_EXPORT AnchorEvaluator {
 public:
  // The evaluation of anchor() and anchor-size() functions is affected
  // by the context they are used in. For example, it is not allowed to
  // do anchor() queries "cross-axis" (e.g. left:anchor(--a top)),
  // and anchor-size() queries are only valid in sizing properties.
  // Queries that violate these rules instead resolve to their fallback
  // values (or 0px if no fallback value exists).
  //
  // The default mode of AnchorEvaluator (kNone) is to return nullopt (i.e.
  // fallback) for any query. This represents a context where no anchor query
  // is valid, e.g. a property unrelated to insets or sizing.
  //
  // The values kLeft, kRight, kTop and kBottom represent the corresponding
  // inset properties, and allow anchor() queries [1] (with restrictions),
  // but not anchor-size() queries.
  //
  // The value kSize represents supported sizing properties [2], and allows
  // anchor-size(), but not anchor().
  //
  // The current mode can be set by placing an AnchorScope object on the
  // stack.
  //
  // [1] https://drafts.csswg.org/css-anchor-position-1/#anchor-valid
  // [2] https://drafts.csswg.org/css-anchor-position-1/#anchor-size-valid
  enum class Mode {
    kNone,

    // anchor()
    kLeft,
    kRight,
    kTop,
    kBottom,

    // anchor-size()
    kSize
  };

  // Evaluates an anchor() or anchor-size() query.
  // Returns |nullopt| if the query is invalid (e.g., no targets or wrong
  // axis.), in which case the fallback should be used.
  virtual std::optional<LayoutUnit> Evaluate(const AnchorQuery&) = 0;

 protected:
  Mode GetMode() const { return mode_; }

 private:
  friend class AnchorScope;
  Mode mode_ = Mode::kNone;
};

// Temporarily sets the Mode of an AnchorEvaluator.
//
// This class behaves like base::AutoReset, except it allows `anchor_evalutor`
// to be nullptr (in which case the AnchorScope has no effect).
//
// See AnchorEvaluator::Mode for more information.
class CORE_EXPORT AnchorScope {
  STACK_ALLOCATED();

 public:
  using Mode = AnchorEvaluator::Mode;

  explicit AnchorScope(Mode mode, AnchorEvaluator* anchor_evaluator)
      : target_(anchor_evaluator ? &anchor_evaluator->mode_ : nullptr),
        original_(anchor_evaluator ? anchor_evaluator->mode_ : Mode::kNone) {
    if (target_) {
      *target_ = mode;
    }
  }
  ~AnchorScope() {
    if (target_) {
      *target_ = original_;
    }
  }

 private:
  Mode* target_;
  Mode original_;
};

// The input to AnchorEvaluator::Evaluate.
//
// It represents either an anchor() function, or an anchor-size() function.
//
// https://drafts.csswg.org/css-anchor-position-1/#anchor-pos
// https://drafts.csswg.org/css-anchor-position-1/#anchor-size-fn
class CORE_EXPORT AnchorQuery {
  DISALLOW_NEW();

 public:
  AnchorQuery(CSSAnchorQueryType query_type,
              const AnchorSpecifierValue* anchor_specifier,
              float percentage,
              absl::variant<CSSAnchorValue, CSSAnchorSizeValue> value)
      : query_type_(query_type),
        anchor_specifier_(anchor_specifier),
        percentage_(percentage),
        value_(value) {
    CHECK(anchor_specifier);
  }

  CSSAnchorQueryType Type() const { return query_type_; }
  const AnchorSpecifierValue& AnchorSpecifier() const {
    return *anchor_specifier_;
  }
  CSSAnchorValue AnchorSide() const {
    DCHECK_EQ(query_type_, CSSAnchorQueryType::kAnchor);
    return absl::get<CSSAnchorValue>(value_);
  }
  float AnchorSidePercentage() const {
    DCHECK_EQ(query_type_, CSSAnchorQueryType::kAnchor);
    DCHECK_EQ(AnchorSide(), CSSAnchorValue::kPercentage);
    return percentage_;
  }
  float AnchorSidePercentageOrZero() const {
    DCHECK_EQ(query_type_, CSSAnchorQueryType::kAnchor);
    return AnchorSide() == CSSAnchorValue::kPercentage ? percentage_ : 0;
  }
  CSSAnchorSizeValue AnchorSize() const {
    DCHECK_EQ(query_type_, CSSAnchorQueryType::kAnchorSize);
    return absl::get<CSSAnchorSizeValue>(value_);
  }

  bool operator==(const AnchorQuery& other) const;
  bool operator!=(const AnchorQuery& other) const { return !operator==(other); }
  unsigned GetHash() const;
  void Trace(Visitor*) const;

 private:
  CSSAnchorQueryType query_type_;
  Member<const AnchorSpecifierValue> anchor_specifier_;
  float percentage_;
  absl::variant<CSSAnchorValue, CSSAnchorSizeValue> value_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ANCHOR_EVALUATOR_H_
