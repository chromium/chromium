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
#include "third_party/blink/renderer/core/css/try_tactic_transform.h"

namespace blink {

const CSSPropertyValueSet* TryValueFlips::FlipSet(
    const TryTacticList& tactic_list) {
  if (tactic_list == kNoTryTactics) {
    return nullptr;
  }

  TryTacticTransform transform(tactic_list);
  constexpr wtf_size_t kMaxDeclarations = 10;
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

  // Unlike the other properties, align/justify-self are always added,
  // because we might need to transform the value without changing
  // the property. (E.g. justify-self:start + flip-inline => justify-self:end).
  add(CSSPropertyID::kAlignSelf, transform.FlippedStart()
                                     ? CSSPropertyID::kJustifySelf
                                     : CSSPropertyID::kAlignSelf);
  add(CSSPropertyID::kJustifySelf, transform.FlippedStart()
                                       ? CSSPropertyID::kAlignSelf
                                       : CSSPropertyID::kJustifySelf);

  if (transform.FlippedStart()) {
    add(CSSPropertyID::kBlockSize, CSSPropertyID::kInlineSize);
    add(CSSPropertyID::kInlineSize, CSSPropertyID::kBlockSize);
    add(CSSPropertyID::kMinBlockSize, CSSPropertyID::kMinInlineSize);
    add(CSSPropertyID::kMinInlineSize, CSSPropertyID::kMinBlockSize);
    add(CSSPropertyID::kMaxBlockSize, CSSPropertyID::kMaxInlineSize);
    add(CSSPropertyID::kMaxInlineSize, CSSPropertyID::kMaxBlockSize);
  }

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

  NOTREACHED();
  return LogicalAxis::kInline;
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

}  // namespace

const CSSValue* TryValueFlips::FlipValue(
    CSSPropertyID from_property,
    const CSSValue* value,
    const TryTacticTransform& transform,
    const WritingDirectionMode& writing_direction) {
  LogicalAxis logical_axis =
      DeterminePropertyAxis(from_property, writing_direction);
  if (const auto* math_value = DynamicTo<CSSMathFunctionValue>(value)) {
    return math_value->TransformAnchors(logical_axis, transform,
                                        writing_direction);
  }
  if (from_property == CSSPropertyID::kAlignSelf ||
      from_property == CSSPropertyID::kJustifySelf) {
    return TransformSelfAlignment(value, logical_axis, transform,
                                  writing_direction);
  }
  return value;
}

}  // namespace blink
