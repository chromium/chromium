// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VALUE_ID_MAPPINGS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VALUE_ID_MAPPINGS_H_

#include "base/notreached.h"
#include "third_party/blink/renderer/core/animation/effect_model.h"
#include "third_party/blink/renderer/core/css/css_value_id_mappings_generated.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

template <class T>
T CssValueIDToPlatformEnum(CSSValueID v) {
  // By default, we use the generated mappings. For special cases, we
  // specialize.
  return detail::cssValueIDToPlatformEnumGenerated<T>(v);
}

template <class T>
inline CSSValueID PlatformEnumToCSSValueID(T v) {
  // By default, we use the generated mappings. For special cases, we overload.
  return detail::platformEnumToCSSValueIDGenerated(v);
}

template <>
inline UnicodeBidi CssValueIDToPlatformEnum(CSSValueID v) {
  if (v == CSSValueID::kWebkitIsolate) {
    return UnicodeBidi::kIsolate;
  }
  if (v == CSSValueID::kWebkitIsolateOverride) {
    return UnicodeBidi::kIsolateOverride;
  }
  if (v == CSSValueID::kWebkitPlaintext) {
    return UnicodeBidi::kPlaintext;
  }
  return detail::cssValueIDToPlatformEnumGenerated<UnicodeBidi>(v);
}

template <>
inline EBoxOrient CssValueIDToPlatformEnum(CSSValueID v) {
  if (v == CSSValueID::kInlineAxis) {
    return EBoxOrient::kHorizontal;
  }
  if (v == CSSValueID::kBlockAxis) {
    return EBoxOrient::kVertical;
  }

  return detail::cssValueIDToPlatformEnumGenerated<EBoxOrient>(v);
}

template <>
inline ETextCombine CssValueIDToPlatformEnum(CSSValueID v) {
  if (v == CSSValueID::kHorizontal) {  // -webkit-text-combine
    return ETextCombine::kAll;
  }
  return detail::cssValueIDToPlatformEnumGenerated<ETextCombine>(v);
}

template <>
inline ETextAlign CssValueIDToPlatformEnum(CSSValueID v) {
  if (v ==
      CSSValueID::kWebkitAuto) {  // Legacy -webkit-auto. Eqiuvalent to start.
    return ETextAlign::kStart;
  }
  if (v == CSSValueID::kInternalCenter) {
    return ETextAlign::kCenter;
  }
  return detail::cssValueIDToPlatformEnumGenerated<ETextAlign>(v);
}

template <>
inline CSSValueID PlatformEnumToCSSValueID(EffectModel::CompositeOperation v) {
  switch (v) {
    case EffectModel::kCompositeReplace:
      return CSSValueID::kReplace;
    case EffectModel::kCompositeAdd:
      return CSSValueID::kAdd;
    case EffectModel::kCompositeAccumulate:
      return CSSValueID::kAccumulate;
  }
  NOTREACHED();
  return CSSValueID::kReplace;
}

template <>
inline EffectModel::CompositeOperation CssValueIDToPlatformEnum(CSSValueID v) {
  switch (v) {
    case CSSValueID::kReplace:
      return EffectModel::kCompositeReplace;
    case CSSValueID::kAdd:
      return EffectModel::kCompositeAdd;
    case CSSValueID::kAccumulate:
      return EffectModel::kCompositeAccumulate;
    default:
      NOTREACHED();
      return EffectModel::kCompositeReplace;
  }
}

template <>
inline ETextOrientation CssValueIDToPlatformEnum(CSSValueID v) {
  if (v == CSSValueID::kSidewaysRight) {  // Legacy -webkit-auto. Eqiuvalent to
                                          // start.
    return ETextOrientation::kSideways;
  }
  if (v == CSSValueID::kVerticalRight) {
    return ETextOrientation::kMixed;
  }
  return detail::cssValueIDToPlatformEnumGenerated<ETextOrientation>(v);
}

template <>
inline EResize CssValueIDToPlatformEnum(CSSValueID v) {
  if (v == CSSValueID::kAuto) {
    // Depends on settings, thus should be handled by the caller.
    NOTREACHED();
    return EResize::kNone;
  }
  return detail::cssValueIDToPlatformEnumGenerated<EResize>(v);
}

template <>
inline WritingMode CssValueIDToPlatformEnum(CSSValueID v) {
  switch (v) {
    case CSSValueID::kHorizontalTb:
    case CSSValueID::kLr:
    case CSSValueID::kLrTb:
    case CSSValueID::kRl:
    case CSSValueID::kRlTb:
      return WritingMode::kHorizontalTb;
    case CSSValueID::kVerticalRl:
    case CSSValueID::kTb:
    case CSSValueID::kTbRl:
      return WritingMode::kVerticalRl;
    case CSSValueID::kVerticalLr:
      return WritingMode::kVerticalLr;
    default:
      break;
  }

  NOTREACHED();
  return WritingMode::kHorizontalTb;
}

template <>
inline ECursor CssValueIDToPlatformEnum(CSSValueID v) {
  if (v == CSSValueID::kWebkitZoomIn) {
    return ECursor::kZoomIn;
  }
  if (v == CSSValueID::kWebkitZoomOut) {
    return ECursor::kZoomOut;
  }
  if (v == CSSValueID::kWebkitGrab) {
    return ECursor::kGrab;
  }
  if (v == CSSValueID::kWebkitGrabbing) {
    return ECursor::kGrabbing;
  }
  return detail::cssValueIDToPlatformEnumGenerated<ECursor>(v);
}

template <>
inline EDisplay CssValueIDToPlatformEnum(CSSValueID v) {
  if (v == CSSValueID::kNone) {
    return EDisplay::kNone;
  }
  if (v == CSSValueID::kInline) {
    return EDisplay::kInline;
  }
  if (v == CSSValueID::kBlock || v == CSSValueID::kFlow) {
    return EDisplay::kBlock;
  }
  if (v == CSSValueID::kFlowRoot) {
    return EDisplay::kFlowRoot;
  }
  if (v == CSSValueID::kListItem) {
    return EDisplay::kListItem;
  }
  if (v == CSSValueID::kInlineBlock) {
    return EDisplay::kInlineBlock;
  }
  if (v == CSSValueID::kTable) {
    return EDisplay::kTable;
  }
  if (v == CSSValueID::kInlineTable) {
    return EDisplay::kInlineTable;
  }
  if (v == CSSValueID::kTableRowGroup) {
    return EDisplay::kTableRowGroup;
  }
  if (v == CSSValueID::kTableHeaderGroup) {
    return EDisplay::kTableHeaderGroup;
  }
  if (v == CSSValueID::kTableFooterGroup) {
    return EDisplay::kTableFooterGroup;
  }
  if (v == CSSValueID::kTableRow) {
    return EDisplay::kTableRow;
  }
  if (v == CSSValueID::kTableColumnGroup) {
    return EDisplay::kTableColumnGroup;
  }
  if (v == CSSValueID::kTableColumn) {
    return EDisplay::kTableColumn;
  }
  if (v == CSSValueID::kTableCell) {
    return EDisplay::kTableCell;
  }
  if (v == CSSValueID::kTableCaption) {
    return EDisplay::kTableCaption;
  }
  if (v == CSSValueID::kWebkitBox) {
    return EDisplay::kWebkitBox;
  }
  if (v == CSSValueID::kWebkitInlineBox) {
    return EDisplay::kWebkitInlineBox;
  }
  if (v == CSSValueID::kFlex) {
    return EDisplay::kFlex;
  }
  if (v == CSSValueID::kInlineFlex) {
    return EDisplay::kInlineFlex;
  }
  if (v == CSSValueID::kGrid) {
    return EDisplay::kGrid;
  }
  if (v == CSSValueID::kInlineGrid) {
    return EDisplay::kInlineGrid;
  }
  if (v == CSSValueID::kContents) {
    return EDisplay::kContents;
  }
  if (v == CSSValueID::kWebkitFlex) {
    return EDisplay::kFlex;
  }
  if (v == CSSValueID::kWebkitInlineFlex) {
    return EDisplay::kInlineFlex;
  }
  if (v == CSSValueID::kMath) {
    return EDisplay::kMath;
  }

  NOTREACHED();
  return EDisplay::kInline;
}

template <>
inline EUserSelect CssValueIDToPlatformEnum(CSSValueID v) {
  if (v == CSSValueID::kAuto) {
    return EUserSelect::kAuto;
  }
  return detail::cssValueIDToPlatformEnumGenerated<EUserSelect>(v);
}

template <>
inline CSSValueID PlatformEnumToCSSValueID(EDisplay v) {
  if (v == EDisplay::kNone) {
    return CSSValueID::kNone;
  }
  if (v == EDisplay::kInline) {
    return CSSValueID::kInline;
  }
  if (v == EDisplay::kBlock) {
    return CSSValueID::kBlock;
  }
  if (v == EDisplay::kFlowRoot) {
    return CSSValueID::kFlowRoot;
  }
  if (v == EDisplay::kListItem) {
    return CSSValueID::kListItem;
  }
  if (v == EDisplay::kInlineBlock) {
    return CSSValueID::kInlineBlock;
  }
  if (v == EDisplay::kTable) {
    return CSSValueID::kTable;
  }
  if (v == EDisplay::kInlineTable) {
    return CSSValueID::kInlineTable;
  }
  if (v == EDisplay::kTableRowGroup) {
    return CSSValueID::kTableRowGroup;
  }
  if (v == EDisplay::kTableHeaderGroup) {
    return CSSValueID::kTableHeaderGroup;
  }
  if (v == EDisplay::kTableFooterGroup) {
    return CSSValueID::kTableFooterGroup;
  }
  if (v == EDisplay::kTableRow) {
    return CSSValueID::kTableRow;
  }
  if (v == EDisplay::kTableColumnGroup) {
    return CSSValueID::kTableColumnGroup;
  }
  if (v == EDisplay::kTableColumn) {
    return CSSValueID::kTableColumn;
  }
  if (v == EDisplay::kTableCell) {
    return CSSValueID::kTableCell;
  }
  if (v == EDisplay::kTableCaption) {
    return CSSValueID::kTableCaption;
  }
  if (v == EDisplay::kWebkitBox) {
    return CSSValueID::kWebkitBox;
  }
  if (v == EDisplay::kWebkitInlineBox) {
    return CSSValueID::kWebkitInlineBox;
  }
  if (v == EDisplay::kFlex) {
    return CSSValueID::kFlex;
  }
  if (v == EDisplay::kInlineFlex) {
    return CSSValueID::kInlineFlex;
  }
  if (v == EDisplay::kGrid) {
    return CSSValueID::kGrid;
  }
  if (v == EDisplay::kInlineGrid) {
    return CSSValueID::kInlineGrid;
  }
  if (v == EDisplay::kContents) {
    return CSSValueID::kContents;
  }
  if (v == EDisplay::kMath) {
    return CSSValueID::kMath;
  }

  NOTREACHED();
  return CSSValueID::kInline;
}

template <>
inline PageOrientation CssValueIDToPlatformEnum(CSSValueID v) {
  if (v == CSSValueID::kUpright) {
    return PageOrientation::kUpright;
  }
  if (v == CSSValueID::kRotateLeft) {
    return PageOrientation::kRotateLeft;
  }
  if (v == CSSValueID::kRotateRight) {
    return PageOrientation::kRotateRight;
  }

  NOTREACHED();
  return PageOrientation::kUpright;
}

template <>
inline ScrollbarGutter CssValueIDToPlatformEnum(CSSValueID v) {
  if (v == CSSValueID::kAuto) {
    return kScrollbarGutterAuto;
  }
  if (v == CSSValueID::kStable) {
    return kScrollbarGutterStable;
  }
  if (v == CSSValueID::kBothEdges) {
    return kScrollbarGutterBothEdges;
  }

  NOTREACHED();
  return kScrollbarGutterAuto;
}

template <>
inline EWhiteSpace CssValueIDToPlatformEnum(CSSValueID v) {
  switch (v) {
    case CSSValueID::kNormal:
      return EWhiteSpace::kNormal;
    case CSSValueID::kPre:
      return EWhiteSpace::kPre;
    case CSSValueID::kPreWrap:
      return EWhiteSpace::kPreWrap;
    case CSSValueID::kPreLine:
      return EWhiteSpace::kPreLine;
    case CSSValueID::kNowrap:
      return EWhiteSpace::kNowrap;
    case CSSValueID::kBreakSpaces:
      return EWhiteSpace::kBreakSpaces;
    default:
      NOTREACHED();
      return EWhiteSpace::kNormal;
  }
}

template <>
inline CSSValueID PlatformEnumToCSSValueID(EWhiteSpace v) {
  switch (v) {
    case EWhiteSpace::kNormal:
      return CSSValueID::kNormal;
    case EWhiteSpace::kNowrap:
      return CSSValueID::kNowrap;
    case EWhiteSpace::kPre:
      return CSSValueID::kPre;
    case EWhiteSpace::kPreLine:
      return CSSValueID::kPreLine;
    case EWhiteSpace::kPreWrap:
      return CSSValueID::kPreWrap;
    case EWhiteSpace::kBreakSpaces:
      return CSSValueID::kBreakSpaces;
  }
  if (ToTextWrap(v) == TextWrap::kBalance &&
      !RuntimeEnabledFeatures::CSSWhiteSpaceShorthandEnabled()) {
    // If `text-wrap: balance` but the shorthandifying `white-space` is off,
    // pretend as if `text-wrap: wrap`.
    return PlatformEnumToCSSValueID(
        ToWhiteSpace(ToWhiteSpaceCollapse(v), TextWrap::kWrap));
  }
  NOTREACHED();
  return CSSValueID::kNone;
}

template <>
inline WhiteSpaceCollapse CssValueIDToPlatformEnum(CSSValueID v) {
  switch (v) {
    case CSSValueID::kCollapse:
      return WhiteSpaceCollapse::kCollapse;
    case CSSValueID::kPreserve:
      return WhiteSpaceCollapse::kPreserve;
    case CSSValueID::kPreserveBreaks:
      return WhiteSpaceCollapse::kPreserveBreaks;
    case CSSValueID::kBreakSpaces:
      return WhiteSpaceCollapse::kBreakSpaces;
    default:
      NOTREACHED();
      return WhiteSpaceCollapse::kCollapse;
  }
}

template <>
inline CSSValueID PlatformEnumToCSSValueID(WhiteSpaceCollapse v) {
  switch (v) {
    case WhiteSpaceCollapse::kCollapse:
      return CSSValueID::kCollapse;
    case WhiteSpaceCollapse::kPreserveBreaks:
      return CSSValueID::kPreserveBreaks;
    case WhiteSpaceCollapse::kPreserve:
      return CSSValueID::kPreserve;
    case WhiteSpaceCollapse::kBreakSpaces:
      return CSSValueID::kBreakSpaces;
  }
  NOTREACHED();
  return CSSValueID::kNone;
}

template <>
inline TextWrap CssValueIDToPlatformEnum(CSSValueID v) {
  switch (v) {
    case CSSValueID::kWrap:
      return TextWrap::kWrap;
    case CSSValueID::kNowrap:
      DCHECK(RuntimeEnabledFeatures::CSSWhiteSpaceShorthandEnabled());
      return TextWrap::kNoWrap;
    case CSSValueID::kBalance:
      return TextWrap::kBalance;
    case CSSValueID::kPretty:
      DCHECK(RuntimeEnabledFeatures::CSSTextWrapPrettyEnabled());
      return TextWrap::kPretty;
    default:
      NOTREACHED();
      return TextWrap::kWrap;
  }
}

template <>
inline CSSValueID PlatformEnumToCSSValueID(TextWrap v) {
  switch (v) {
    case TextWrap::kWrap:
      return CSSValueID::kWrap;
    case TextWrap::kNoWrap:
      if (!RuntimeEnabledFeatures::CSSWhiteSpaceShorthandEnabled()) {
        // Note this is not right, but a compromise until `white-space` becomes
        // a shorthand. Simulate the behavior when it's off.
        return CSSValueID::kWrap;
      }
      return CSSValueID::kNowrap;
    case TextWrap::kBalance:
      return CSSValueID::kBalance;
    case TextWrap::kPretty:
      DCHECK(RuntimeEnabledFeatures::CSSTextWrapPrettyEnabled());
      return CSSValueID::kPretty;
  }
  NOTREACHED();
  return CSSValueID::kNone;
}

template <>
inline TimelineAttachment CssValueIDToPlatformEnum(CSSValueID v) {
  switch (v) {
    case CSSValueID::kLocal:
      return TimelineAttachment::kLocal;
    case CSSValueID::kDefer:
      return TimelineAttachment::kDefer;
    case CSSValueID::kAncestor:
      return TimelineAttachment::kAncestor;
    default:
      NOTREACHED();
      return TimelineAttachment::kAncestor;
  }
}

template <>
inline CSSValueID PlatformEnumToCSSValueID(TimelineAttachment v) {
  switch (v) {
    case TimelineAttachment::kLocal:
      return CSSValueID::kLocal;
    case TimelineAttachment::kDefer:
      return CSSValueID::kDefer;
    case TimelineAttachment::kAncestor:
      return CSSValueID::kAncestor;
  }
  NOTREACHED();
  return CSSValueID::kLocal;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VALUE_ID_MAPPINGS_H_
