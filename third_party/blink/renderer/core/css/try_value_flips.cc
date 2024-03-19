// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/try_value_flips.h"

#include "third_party/blink/renderer/core/css/css_flip_revert_value.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
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

  using Insets = TryTacticTransform::LogicalSides<CSSPropertyID>;

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

  Insets insets = revert_transform.Transform(Insets{
      .inline_start = CSSPropertyID::kInsetInlineStart,
      .inline_end = CSSPropertyID::kInsetInlineEnd,
      .block_start = CSSPropertyID::kInsetBlockStart,
      .block_end = CSSPropertyID::kInsetBlockEnd,
  });

  add_if_flipped(CSSPropertyID::kInsetBlockStart, insets.block_start);
  add_if_flipped(CSSPropertyID::kInsetBlockEnd, insets.block_end);
  add_if_flipped(CSSPropertyID::kInsetInlineStart, insets.inline_start);
  add_if_flipped(CSSPropertyID::kInsetInlineEnd, insets.inline_end);

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
    case CSSPropertyID::kWidth:
    case CSSPropertyID::kMaxWidth:
    case CSSPropertyID::kMinWidth:
      return writing_direction.IsHorizontal() ? LogicalAxis::kInline
                                              : LogicalAxis::kBlock;
    case CSSPropertyID::kTop:
    case CSSPropertyID::kBottom:
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
  return value;
}

}  // namespace blink
