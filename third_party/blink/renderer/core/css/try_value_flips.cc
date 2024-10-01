// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/try_value_flips.h"

#include "third_party/blink/renderer/core/css/css_flip_revert_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/try_tactic_transform.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

const CSSPropertyValueSet* TryValueFlips::FlipSet(
    const TryTacticList& tactic_list) const {
  if (tactic_list == kNoTryTactics) {
    return nullptr;
  }

  TryTacticTransform transform(tactic_list);
  // We don't store the kNoTryTactics/nullptr case explicitly, i.e. the entry
  // at cached_flip_sets_[0] corresponds to CacheIndex()==1.
  unsigned index = transform.CacheIndex() - 1;
  cached_flip_sets_.resize(kCachedFlipSetsSize);
  CHECK_LT(index, cached_flip_sets_.size());
  if (!cached_flip_sets_[index]) {
    cached_flip_sets_[index] = CreateFlipSet(transform);
  }
  return cached_flip_sets_[index];
}

const CSSPropertyValueSet* TryValueFlips::CreateFlipSet(
    const TryTacticTransform& transform) const {
  // The maximum number of declarations that can be added to the flip set.
  constexpr wtf_size_t kMaxDeclarations = 17;
  HeapVector<CSSPropertyValue, kMaxDeclarations> declarations;

  auto add = [&declarations, transform](CSSPropertyID from, CSSPropertyID to) {
    declarations.push_back(CSSPropertyValue(
        CSSPropertyName(from),
        *MakeGarbageCollected<cssvalue::CSSFlipRevertValue>(to, transform)));
  };

  auto add_if_flipped = [&add](CSSPropertyID from, CSSPropertyID to) {
    if (from != to) {
      add(from, to);
    }
  };

  using Properties = TryTacticTransform::LogicalSides<CSSPropertyID>;

  // The value of insets.inline_start (etc) must contain the property
  // we should revert to using CSSFlipRevertValue. This means we need
  // the inverse transform.
  //
  // For example, consider this declaration:
  //
  //  right: anchor(left);
  //
  // If we flip this by "flip-inline flip-start", then we should ultimately
  // end up with:
  //
  //  top: anchor(bottom); /* via -internal-flip-revert(right) */
  //
  // The insets, as transformed by `transform` would look like this:
  //
  //  {
  //   .inline_start = CSSPropertyID::kInsetBlockEnd,   /* L -> B */
  //   .inline_end = CSSPropertyID::kInsetBlockStart,   /* R -> T */
  //   .block_start = CSSPropertyID::kInsetInlineStart, /* T -> L */
  //   .block_end = CSSPropertyID::kInsetInlineEnd,     /* B -> R */
  //  }
  //
  // That shows that a inline-end (right) constraint becomes a block-start
  // (top) constraint, which is correct, but if we generate a flip declaration
  // from that using add_if_flipped(kInsetBlockStart, insets.block_start),
  // we effectively get: top:-internal-flip-revert(left), which is not correct.
  // However, if you read above transformed properties the opposite way
  // (i.e. the inverse), you'll see that we indeed get
  // top:-internal-flip-revert(right).
  TryTacticTransform revert_transform = transform.Inverse();

  Properties insets = revert_transform.Transform(Properties{
      .inline_start = CSSPropertyID::kInsetInlineStart,
      .inline_end = CSSPropertyID::kInsetInlineEnd,
      .block_start = CSSPropertyID::kInsetBlockStart,
      .block_end = CSSPropertyID::kInsetBlockEnd,
  });

  add_if_flipped(CSSPropertyID::kInsetBlockStart, insets.block_start);
  add_if_flipped(CSSPropertyID::kInsetBlockEnd, insets.block_end);
  add_if_flipped(CSSPropertyID::kInsetInlineStart, insets.inline_start);
  add_if_flipped(CSSPropertyID::kInsetInlineEnd, insets.inline_end);

  Properties margin = revert_transform.Transform(Properties{
      .inline_start = CSSPropertyID::kMarginInlineStart,
      .inline_end = CSSPropertyID::kMarginInlineEnd,
      .block_start = CSSPropertyID::kMarginBlockStart,
      .block_end = CSSPropertyID::kMarginBlockEnd,
  });

  add_if_flipped(CSSPropertyID::kMarginBlockStart, margin.block_start);
  add_if_flipped(CSSPropertyID::kMarginBlockEnd, margin.block_end);
  add_if_flipped(CSSPropertyID::kMarginInlineStart, margin.inline_start);
  add_if_flipped(CSSPropertyID::kMarginInlineEnd, margin.inline_end);

  // Unlike the other properties, align-self, justify-self, position-area,
  // and inset-area are always added, because we might need to transform the
  // value without changing the property.
  // (E.g. justify-self:start + flip-inline => justify-self:end).
  add(CSSPropertyID::kAlignSelf, transform.FlippedStart()
                                     ? CSSPropertyID::kJustifySelf
                                     : CSSPropertyID::kAlignSelf);
  add(CSSPropertyID::kJustifySelf, transform.FlippedStart()
                                       ? CSSPropertyID::kAlignSelf
                                       : CSSPropertyID::kJustifySelf);
  add(CSSPropertyID::kPositionArea, CSSPropertyID::kPositionArea);
  if (RuntimeEnabledFeatures::CSSInsetAreaPropertyEnabled()) {
    add(CSSPropertyID::kInsetArea, CSSPropertyID::kInsetArea);
  }

  if (transform.FlippedStart()) {
    add(CSSPropertyID::kBlockSize, CSSPropertyID::kInlineSize);
    add(CSSPropertyID::kInlineSize, CSSPropertyID::kBlockSize);
    add(CSSPropertyID::kMinBlockSize, CSSPropertyID::kMinInlineSize);
    add(CSSPropertyID::kMinInlineSize, CSSPropertyID::kMinBlockSize);
    add(CSSPropertyID::kMaxBlockSize, CSSPropertyID::kMaxInlineSize);
    add(CSSPropertyID::kMaxInlineSize, CSSPropertyID::kMaxBlockSize);
  }

  // Consider updating `kMaxDeclarations` when new properties are added.

  return ImmutableCSSPropertyValueSet::Create(
      declarations.data(), declarations.size(), kHTMLStandardMode);
}

namespace {

LogicalAxis DeterminePropertyAxis(
    CSSPropertyID property_id,
    const WritingDirectionMode& writing_direction) {
  // We expect physical properties here.
  CHECK(!CSSProperty::Get(property_id).IsSurrogate());

  switch (property_id) {
    case CSSPropertyID::kLeft:
    case CSSPropertyID::kRight:
    case CSSPropertyID::kMarginLeft:
    case CSSPropertyID::kMarginRight:
    case CSSPropertyID::kJustifySelf:
    case CSSPropertyID::kWidth:
    case CSSPropertyID::kMaxWidth:
    case CSSPropertyID::kMinWidth:
      return writing_direction.IsHorizontal() ? LogicalAxis::kInline
                                              : LogicalAxis::kBlock;
    case CSSPropertyID::kTop:
    case CSSPropertyID::kBottom:
    case CSSPropertyID::kMarginTop:
    case CSSPropertyID::kMarginBottom:
    case CSSPropertyID::kAlignSelf:
    case CSSPropertyID::kHeight:
    case CSSPropertyID::kMaxHeight:
    case CSSPropertyID::kMinHeight:
      return writing_direction.IsHorizontal() ? LogicalAxis::kBlock
                                              : LogicalAxis::kInline;
    default:
      break;
  }

  NOTREACHED_IN_MIGRATION();
  return LogicalAxis::kInline;
}

std::optional<LogicalAxis> DetermineValueAxis(
    CSSValueID value_id,
    const WritingDirectionMode& writing_direction) {
  switch (value_id) {
    case CSSValueID::kLeft:
    case CSSValueID::kRight:
    case CSSValueID::kSpanLeft:
    case CSSValueID::kSpanRight:
    case CSSValueID::kXStart:
    case CSSValueID::kXEnd:
    case CSSValueID::kSpanXStart:
    case CSSValueID::kSpanXEnd:
    case CSSValueID::kXSelfStart:
    case CSSValueID::kXSelfEnd:
    case CSSValueID::kSpanXSelfStart:
    case CSSValueID::kSpanXSelfEnd:
      return writing_direction.IsHorizontal() ? LogicalAxis::kInline
                                              : LogicalAxis::kBlock;
    case CSSValueID::kTop:
    case CSSValueID::kBottom:
    case CSSValueID::kSpanTop:
    case CSSValueID::kSpanBottom:
    case CSSValueID::kYStart:
    case CSSValueID::kYEnd:
    case CSSValueID::kSpanYStart:
    case CSSValueID::kSpanYEnd:
    case CSSValueID::kYSelfStart:
    case CSSValueID::kYSelfEnd:
    case CSSValueID::kSpanYSelfStart:
    case CSSValueID::kSpanYSelfEnd:
      return writing_direction.IsHorizontal() ? LogicalAxis::kBlock
                                              : LogicalAxis::kInline;
    case CSSValueID::kBlockStart:
    case CSSValueID::kBlockEnd:
    case CSSValueID::kSpanBlockStart:
    case CSSValueID::kSpanBlockEnd:
    case CSSValueID::kSelfBlockStart:
    case CSSValueID::kSelfBlockEnd:
    case CSSValueID::kSpanSelfBlockStart:
    case CSSValueID::kSpanSelfBlockEnd:
      return LogicalAxis::kBlock;
    case CSSValueID::kInlineStart:
    case CSSValueID::kInlineEnd:
    case CSSValueID::kSpanInlineStart:
    case CSSValueID::kSpanInlineEnd:
    case CSSValueID::kSelfInlineStart:
    case CSSValueID::kSelfInlineEnd:
    case CSSValueID::kSpanSelfInlineStart:
    case CSSValueID::kSpanSelfInlineEnd:
      return LogicalAxis::kInline;
    case CSSValueID::kSpanAll:
    case CSSValueID::kCenter:
    case CSSValueID::kStart:
    case CSSValueID::kEnd:
    case CSSValueID::kSpanStart:
    case CSSValueID::kSpanEnd:
    case CSSValueID::kSelfStart:
    case CSSValueID::kSelfEnd:
    case CSSValueID::kSpanSelfStart:
    case CSSValueID::kSpanSelfEnd:
    default:
      return std::nullopt;
  }
}

CSSValueID ConvertLeftRightToLogical(
    CSSValueID value,
    const WritingDirectionMode& writing_direction) {
  if (value == CSSValueID::kLeft) {
    return writing_direction.IsLtr() ? CSSValueID::kSelfStart
                                     : CSSValueID::kSelfEnd;
  }
  if (value == CSSValueID::kRight) {
    return writing_direction.IsLtr() ? CSSValueID::kSelfEnd
                                     : CSSValueID::kSelfStart;
  }
  return value;
}

CSSValueID FlipSelfAlignmentKeyword(CSSValueID value) {
  switch (value) {
    case CSSValueID::kLeft:
      return CSSValueID::kRight;
    case CSSValueID::kRight:
      return CSSValueID::kLeft;
    case CSSValueID::kStart:
      return CSSValueID::kEnd;
    case CSSValueID::kEnd:
      return CSSValueID::kStart;
    case CSSValueID::kSelfStart:
      return CSSValueID::kSelfEnd;
    case CSSValueID::kSelfEnd:
      return CSSValueID::kSelfStart;
    case CSSValueID::kFlexStart:
      return CSSValueID::kFlexEnd;
    case CSSValueID::kFlexEnd:
      return CSSValueID::kFlexStart;
    default:
      return value;
  }
}

const CSSValue* TransformSelfAlignment(
    const CSSValue* value,
    LogicalAxis logical_axis,
    const TryTacticTransform& transform,
    const WritingDirectionMode& writing_direction) {
  auto* ident = DynamicTo<CSSIdentifierValue>(value);
  auto* pair = DynamicTo<CSSValuePair>(value);
  if (!ident && !pair) {
    return value;
  }
  // Flips start => end, end => start, etc.
  bool flip_side = (logical_axis == LogicalAxis::kInline)
                       ? transform.FlippedInline()
                       : transform.FlippedBlock();

  CSSValueID from = ident ? ident->GetValueID()
                          : To<CSSIdentifierValue>(pair->Second()).GetValueID();
  CSSValueID to = flip_side ? FlipSelfAlignmentKeyword(from) : from;
  // justify-self supports left and right, align-self does not. FlippedStart
  // means align-self may have acquired a left or right value, which needs to be
  // translated to a logical equivalent.
  to = transform.FlippedStart()
           ? ConvertLeftRightToLogical(to, writing_direction)
           : to;
  if (from == to) {
    return value;
  }
  // Return the same type of value that came in.
  if (ident) {
    return CSSIdentifierValue::Create(to);
  }
  return MakeGarbageCollected<CSSValuePair>(
      &pair->First(), CSSIdentifierValue::Create(to),
      pair->KeepIdenticalValues() ? CSSValuePair::kKeepIdenticalValues
                                  : CSSValuePair::kDropIdenticalValues);
}

LogicalToPhysical<CSSValueID> TransformPhysical(
    CSSValueID left,
    CSSValueID right,
    CSSValueID top,
    CSSValueID bottom,
    const TryTacticTransform& transform,
    const WritingDirectionMode& writing_direction) {
  // The transform is carried out on logical values, so we need to convert
  // to logical first.
  PhysicalToLogical logical(writing_direction, top, right, bottom, left);
  return transform.Transform(
      TryTacticTransform::LogicalSides<CSSValueID>{
          .inline_start = logical.InlineStart(),
          .inline_end = logical.InlineEnd(),
          .block_start = logical.BlockStart(),
          .block_end = logical.BlockEnd()},
      writing_direction);
}

LogicalToPhysical<CSSValueID> TransformXY(
    CSSValueID x_start,
    CSSValueID x_end,
    CSSValueID y_start,
    CSSValueID y_end,
    const TryTacticTransform& transform,
    const WritingDirectionMode& writing_direction) {
  // We can use TransformPhysical even though x-* and y-* are not fully
  // physical. We might get the start/end in the reverse order when we
  // convert from physical to logical, but it doesn't matter, because
  // we'll then un-reverse the start/end when we convert back to logical.
  return TransformPhysical(x_start, x_end, y_start, y_end, transform,
                           writing_direction);
}

TryTacticTransform::LogicalSides<CSSValueID> TransformLogical(
    CSSValueID inline_start,
    CSSValueID inline_end,
    CSSValueID block_start,
    CSSValueID block_end,
    const TryTacticTransform& transform) {
  return transform.Transform(
      TryTacticTransform::LogicalSides<CSSValueID>{.inline_start = inline_start,
                                                   .inline_end = inline_end,
                                                   .block_start = block_start,
                                                   .block_end = block_end});
}

// Transforms a CSSValueID, specified for the indicated logical axis,
// according to the transform.
CSSValueID TransformPositionAreaKeyword(
    CSSValueID from,
    LogicalAxis logical_axis,
    const TryTacticTransform& transform,
    const WritingDirectionMode& writing_direction) {
  bool flip_start_end = (logical_axis == LogicalAxis::kInline)
                            ? transform.FlippedInline()
                            : transform.FlippedBlock();

  auto transform_physical = [&transform, &writing_direction] {
    return TransformPhysical(CSSValueID::kLeft, CSSValueID::kRight,
                             CSSValueID::kTop, CSSValueID::kBottom, transform,
                             writing_direction);
  };

  auto transform_physical_span = [&transform, &writing_direction] {
    return TransformPhysical(CSSValueID::kSpanLeft, CSSValueID::kSpanRight,
                             CSSValueID::kSpanTop, CSSValueID::kSpanBottom,
                             transform, writing_direction);
  };

  auto transform_logical = [&transform] {
    return TransformLogical(CSSValueID::kInlineStart, CSSValueID::kInlineEnd,
                            CSSValueID::kBlockStart, CSSValueID::kBlockEnd,
                            transform);
  };

  auto transform_logical_span = [&transform] {
    return TransformLogical(
        CSSValueID::kSpanInlineStart, CSSValueID::kSpanInlineEnd,
        CSSValueID::kSpanBlockStart, CSSValueID::kSpanBlockEnd, transform);
  };

  auto transform_logical_self = [&transform] {
    return TransformLogical(
        CSSValueID::kSelfInlineStart, CSSValueID::kSelfInlineEnd,
        CSSValueID::kSelfBlockStart, CSSValueID::kSelfBlockEnd, transform);
  };

  auto transform_logical_span_self = [&transform] {
    return TransformLogical(CSSValueID::kSpanSelfInlineStart,
                            CSSValueID::kSpanSelfInlineEnd,
                            CSSValueID::kSpanSelfBlockStart,
                            CSSValueID::kSpanSelfBlockEnd, transform);
  };

  auto transform_xy = [&transform, &writing_direction] {
    return TransformXY(CSSValueID::kXStart, CSSValueID::kXEnd,
                       CSSValueID::kYStart, CSSValueID::kYEnd, transform,
                       writing_direction);
  };

  auto transform_xy_span = [&transform, &writing_direction] {
    return TransformXY(CSSValueID::kSpanXStart, CSSValueID::kSpanXEnd,
                       CSSValueID::kSpanYStart, CSSValueID::kSpanYEnd,
                       transform, writing_direction);
  };

  auto transform_xy_self = [&transform, &writing_direction] {
    return TransformXY(CSSValueID::kXSelfStart, CSSValueID::kXSelfEnd,
                       CSSValueID::kYSelfStart, CSSValueID::kYSelfEnd,
                       transform, writing_direction);
  };

  auto transform_xy_span_self = [&transform, &writing_direction] {
    return TransformXY(CSSValueID::kSpanXSelfStart, CSSValueID::kSpanXSelfEnd,
                       CSSValueID::kSpanYSelfStart, CSSValueID::kSpanYSelfEnd,
                       transform, writing_direction);
  };

  switch (from) {
      // Physical:

    case CSSValueID::kLeft:
      return transform_physical().Left();
    case CSSValueID::kRight:
      return transform_physical().Right();
    case CSSValueID::kTop:
      return transform_physical().Top();
    case CSSValueID::kBottom:
      return transform_physical().Bottom();

    case CSSValueID::kSpanLeft:
      return transform_physical_span().Left();
    case CSSValueID::kSpanRight:
      return transform_physical_span().Right();
    case CSSValueID::kSpanTop:
      return transform_physical_span().Top();
    case CSSValueID::kSpanBottom:
      return transform_physical_span().Bottom();

      // XY:

    case CSSValueID::kXStart:
      return transform_xy().Left();
    case CSSValueID::kXEnd:
      return transform_xy().Right();
    case CSSValueID::kYStart:
      return transform_xy().Top();
    case CSSValueID::kYEnd:
      return transform_xy().Bottom();

    case CSSValueID::kSpanXStart:
      return transform_xy_span().Left();
    case CSSValueID::kSpanXEnd:
      return transform_xy_span().Right();
    case CSSValueID::kSpanYStart:
      return transform_xy_span().Top();
    case CSSValueID::kSpanYEnd:
      return transform_xy_span().Bottom();

    case CSSValueID::kXSelfStart:
      return transform_xy_self().Left();
    case CSSValueID::kXSelfEnd:
      return transform_xy_self().Right();
    case CSSValueID::kYSelfStart:
      return transform_xy_self().Top();
    case CSSValueID::kYSelfEnd:
      return transform_xy_self().Bottom();

    case CSSValueID::kSpanXSelfStart:
      return transform_xy_span_self().Left();
    case CSSValueID::kSpanXSelfEnd:
      return transform_xy_span_self().Right();
    case CSSValueID::kSpanYSelfStart:
      return transform_xy_span_self().Top();
    case CSSValueID::kSpanYSelfEnd:
      return transform_xy_span_self().Bottom();

      // Logical:

    case CSSValueID::kInlineStart:
      return transform_logical().inline_start;
    case CSSValueID::kInlineEnd:
      return transform_logical().inline_end;
    case CSSValueID::kBlockStart:
      return transform_logical().block_start;
    case CSSValueID::kBlockEnd:
      return transform_logical().block_end;

    case CSSValueID::kSpanInlineStart:
      return transform_logical_span().inline_start;
    case CSSValueID::kSpanInlineEnd:
      return transform_logical_span().inline_end;
    case CSSValueID::kSpanBlockStart:
      return transform_logical_span().block_start;
    case CSSValueID::kSpanBlockEnd:
      return transform_logical_span().block_end;

    case CSSValueID::kSelfInlineStart:
      return transform_logical_self().inline_start;
    case CSSValueID::kSelfInlineEnd:
      return transform_logical_self().inline_end;
    case CSSValueID::kSelfBlockStart:
      return transform_logical_self().block_start;
    case CSSValueID::kSelfBlockEnd:
      return transform_logical_self().block_end;

    case CSSValueID::kSpanSelfInlineStart:
      return transform_logical_span_self().inline_start;
    case CSSValueID::kSpanSelfInlineEnd:
      return transform_logical_span_self().inline_end;
    case CSSValueID::kSpanSelfBlockStart:
      return transform_logical_span_self().block_start;
    case CSSValueID::kSpanSelfBlockEnd:
      return transform_logical_span_self().block_end;

      // Start/end

    case CSSValueID::kStart:
      return flip_start_end ? CSSValueID::kEnd : CSSValueID::kStart;
    case CSSValueID::kEnd:
      return flip_start_end ? CSSValueID::kStart : CSSValueID::kEnd;

    case CSSValueID::kSpanStart:
      return flip_start_end ? CSSValueID::kSpanEnd : CSSValueID::kSpanStart;
    case CSSValueID::kSpanEnd:
      return flip_start_end ? CSSValueID::kSpanStart : CSSValueID::kSpanEnd;

    case CSSValueID::kSelfStart:
      return flip_start_end ? CSSValueID::kSelfEnd : CSSValueID::kSelfStart;
    case CSSValueID::kSelfEnd:
      return flip_start_end ? CSSValueID::kSelfStart : CSSValueID::kSelfEnd;

    case CSSValueID::kSpanSelfStart:
      return flip_start_end ? CSSValueID::kSpanSelfEnd
                            : CSSValueID::kSpanSelfStart;
    case CSSValueID::kSpanSelfEnd:
      return flip_start_end ? CSSValueID::kSpanSelfStart
                            : CSSValueID::kSpanSelfEnd;

    default:
      return from;
  }
}

const CSSValue* TransformPositionArea(
    const CSSValue* value,
    const TryTacticTransform& transform,
    const WritingDirectionMode& writing_direction) {
  auto* ident = DynamicTo<CSSIdentifierValue>(value);
  auto* pair = DynamicTo<CSSValuePair>(value);
  if (!ident && !pair) {
    return value;
  }

  CSSValueID first_value = CSSValueID::kNone;
  CSSValueID second_value = CSSValueID::kNone;

  if (ident) {
    first_value = ident->GetValueID();
    second_value = css_parsing_utils::IsRepeatedPositionAreaValue(first_value)
                       ? first_value
                       : CSSValueID::kSpanAll;
  } else {
    first_value = To<CSSIdentifierValue>(pair->First()).GetValueID();
    second_value = To<CSSIdentifierValue>(pair->Second()).GetValueID();
  }

  std::optional<LogicalAxis> first_axis =
      DetermineValueAxis(first_value, writing_direction);
  std::optional<LogicalAxis> second_axis =
      DetermineValueAxis(second_value, writing_direction);

  // If one value is unambiguous about its axis, the other value must refer
  // to the other axis. If both are ambiguous, then the first value represents
  // the block axis.
  //
  // https://drafts.csswg.org/css-anchor-position-1/#resolving-spans
  if (first_axis.has_value()) {
    second_axis = (first_axis.value() == LogicalAxis::kInline)
                      ? LogicalAxis::kBlock
                      : LogicalAxis::kInline;
  } else if (second_axis.has_value()) {
    first_axis = (second_axis.value() == LogicalAxis::kInline)
                     ? LogicalAxis::kBlock
                     : LogicalAxis::kInline;
  } else {
    first_axis = LogicalAxis::kBlock;
    second_axis = LogicalAxis::kInline;
  }

  CSSValueID first_value_transformed = TransformPositionAreaKeyword(
      first_value, first_axis.value(), transform, writing_direction);
  CSSValueID second_value_transformed = TransformPositionAreaKeyword(
      second_value, second_axis.value(), transform, writing_direction);

  // Maintain grammar order after flip-start.
  if (transform.FlippedStart()) {
    std::swap(first_value_transformed, second_value_transformed);
  }

  if (first_value == first_value_transformed &&
      second_value == second_value_transformed) {
    // No transformation needed.
    return value;
  }

  if (first_value_transformed == second_value_transformed) {
    return CSSIdentifierValue::Create(first_value_transformed);
  }

  // Return a value on the canonical form, i.e. represent the value as a single
  // identifier when possible. See the end of the section 3.1.1 [1] for cases
  // where we should return a single identifier.
  // [1] https://drafts.csswg.org/css-anchor-position-1/#resolving-spans

  if (first_value_transformed == CSSValueID::kSpanAll &&
      !css_parsing_utils::IsRepeatedPositionAreaValue(
          second_value_transformed)) {
    return CSSIdentifierValue::Create(second_value_transformed);
  }
  if (second_value_transformed == CSSValueID::kSpanAll &&
      !css_parsing_utils::IsRepeatedPositionAreaValue(
          first_value_transformed)) {
    return CSSIdentifierValue::Create(first_value_transformed);
  }

  return MakeGarbageCollected<CSSValuePair>(
      CSSIdentifierValue::Create(first_value_transformed),
      CSSIdentifierValue::Create(second_value_transformed),
      pair->KeepIdenticalValues() ? CSSValuePair::kKeepIdenticalValues
                                  : CSSValuePair::kDropIdenticalValues);
}

}  // namespace

const CSSValue* TryValueFlips::FlipValue(
    CSSPropertyID from_property,
    const CSSValue* value,
    const TryTacticTransform& transform,
    const WritingDirectionMode& writing_direction) {
  if (const auto* math_value = DynamicTo<CSSMathFunctionValue>(value)) {
    LogicalAxis logical_axis =
        DeterminePropertyAxis(from_property, writing_direction);
    return math_value->TransformAnchors(logical_axis, transform,
                                        writing_direction);
  }
  if (from_property == CSSPropertyID::kAlignSelf ||
      from_property == CSSPropertyID::kJustifySelf) {
    LogicalAxis logical_axis =
        DeterminePropertyAxis(from_property, writing_direction);
    return TransformSelfAlignment(value, logical_axis, transform,
                                  writing_direction);
  }
  if (from_property == CSSPropertyID::kPositionArea ||
      from_property == CSSPropertyID::kInsetArea) {
    return TransformPositionArea(value, transform, writing_direction);
  }
  return value;
}

void TryValueFlips::Trace(Visitor* visitor) const {
  visitor->Trace(cached_flip_sets_);
}

}  // namespace blink
