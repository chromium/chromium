// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VALUE_ID_MAPPINGS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VALUE_ID_MAPPINGS_H_

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
  if (v == CSSValueID::kWebkitIsolate)
    return UnicodeBidi::kIsolate;
  if (v == CSSValueID::kWebkitIsolateOverride)
    return UnicodeBidi::kIsolateOverride;
  if (v == CSSValueID::kWebkitPlaintext)
    return UnicodeBidi::kPlaintext;
  return detail::cssValueIDToPlatformEnumGenerated<UnicodeBidi>(v);
}

template <>
inline EBoxOrient CssValueIDToPlatformEnum(CSSValueID v) {
  if (v == CSSValueID::kInlineAxis)
    return EBoxOrient::kHorizontal;
  if (v == CSSValueID::kBlockAxis)
    return EBoxOrient::kVertical;

  return detail::cssValueIDToPlatformEnumGenerated<EBoxOrient>(v);
}

template <>
inline ETextCombine CssValueIDToPlatformEnum(CSSValueID v) {
  if (v == CSSValueID::kHorizontal)  // -webkit-text-combine
    return ETextCombine::kAll;
  return detail::cssValueIDToPlatformEnumGenerated<ETextCombine>(v);
}

template <>
inline ETextAlign CssValueIDToPlatformEnum(CSSValueID v) {
  if (v ==
      CSSValueID::kWebkitAuto)  // Legacy -webkit-auto. Eqiuvalent to start.
    return ETextAlign::kStart;
  if (v == CSSValueID::kInternalCenter)
    return ETextAlign::kCenter;
  return detail::cssValueIDToPlatformEnumGenerated<ETextAlign>(v);
}

template <>
inline ETextOrientation CssValueIDToPlatformEnum(CSSValueID v) {
  if (v ==
      CSSValueID::kSidewaysRight)  // Legacy -webkit-auto. Eqiuvalent to start.
    return ETextOrientation::kSideways;
  if (v == CSSValueID::kVerticalRight)
    return ETextOrientation::kMixed;
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
  if (v == CSSValueID::kWebkitZoomIn)
    return ECursor::kZoomIn;
  if (v == CSSValueID::kWebkitZoomOut)
    return ECursor::kZoomOut;
  if (v == CSSValueID::kWebkitGrab)
    return ECursor::kGrab;
  if (v == CSSValueID::kWebkitGrabbing)
    return ECursor::kGrabbing;
  return detail::cssValueIDToPlatformEnumGenerated<ECursor>(v);
}

template <>
inline EDisplay CssValueIDToPlatformEnum(CSSValueID v) {
  if (v == CSSValueID::kNone)
    return EDisplay::kNone;
  if (v == CSSValueID::kInline)
    return EDisplay::kInline;
  if (v == CSSValueID::kBlock)
    return EDisplay::kBlock;
  if (v == CSSValueID::kFlowRoot)
    return EDisplay::kFlowRoot;
  if (v == CSSValueID::kListItem)
    return EDisplay::kListItem;
  if (v == CSSValueID::kInlineBlock)
    return EDisplay::kInlineBlock;
  if (v == CSSValueID::kTable)
    return EDisplay::kTable;
  if (v == CSSValueID::kInlineTable)
    return EDisplay::kInlineTable;
  if (v == CSSValueID::kTableRowGroup)
    return EDisplay::kTableRowGroup;
  if (v == CSSValueID::kTableHeaderGroup)
    return EDisplay::kTableHeaderGroup;
  if (v == CSSValueID::kTableFooterGroup)
    return EDisplay::kTableFooterGroup;
  if (v == CSSValueID::kTableRow)
    return EDisplay::kTableRow;
  if (v == CSSValueID::kTableColumnGroup)
    return EDisplay::kTableColumnGroup;
  if (v == CSSValueID::kTableColumn)
    return EDisplay::kTableColumn;
  if (v == CSSValueID::kTableCell)
    return EDisplay::kTableCell;
  if (v == CSSValueID::kTableCaption)
    return EDisplay::kTableCaption;
  if (v == CSSValueID::kWebkitBox)
    return EDisplay::kWebkitBox;
  if (v == CSSValueID::kWebkitInlineBox)
    return EDisplay::kWebkitInlineBox;
  if (v == CSSValueID::kFlex)
    return EDisplay::kFlex;
  if (v == CSSValueID::kInlineFlex)
    return EDisplay::kInlineFlex;
  if (v == CSSValueID::kGrid)
    return EDisplay::kGrid;
  if (v == CSSValueID::kInlineGrid)
    return EDisplay::kInlineGrid;
  if (v == CSSValueID::kContents)
    return EDisplay::kContents;
  if (v == CSSValueID::kWebkitFlex)
    return EDisplay::kFlex;
  if (v == CSSValueID::kWebkitInlineFlex)
    return EDisplay::kInlineFlex;
  if (v == CSSValueID::kMath)
    return EDisplay::kMath;

  NOTREACHED();
  return EDisplay::kInline;
}

template <>
inline EListStyleType CssValueIDToPlatformEnum(CSSValueID v) {
  switch (v) {
    case CSSValueID::kDisc:
      return EListStyleType::kDisc;
    case CSSValueID::kCircle:
      return EListStyleType::kCircle;
    case CSSValueID::kSquare:
      return EListStyleType::kSquare;
    case CSSValueID::kDecimal:
      return EListStyleType::kDecimal;
    case CSSValueID::kDecimalLeadingZero:
      return EListStyleType::kDecimalLeadingZero;
    case CSSValueID::kArabicIndic:
      return EListStyleType::kArabicIndic;
    case CSSValueID::kBengali:
      return EListStyleType::kBengali;
    case CSSValueID::kCambodian:
      return EListStyleType::kCambodian;
    case CSSValueID::kKhmer:
      return EListStyleType::kKhmer;
    case CSSValueID::kDevanagari:
      return EListStyleType::kDevanagari;
    case CSSValueID::kGujarati:
      return EListStyleType::kGujarati;
    case CSSValueID::kGurmukhi:
      return EListStyleType::kGurmukhi;
    case CSSValueID::kKannada:
      return EListStyleType::kKannada;
    case CSSValueID::kLao:
      return EListStyleType::kLao;
    case CSSValueID::kMalayalam:
      return EListStyleType::kMalayalam;
    case CSSValueID::kMongolian:
      return EListStyleType::kMongolian;
    case CSSValueID::kMyanmar:
      return EListStyleType::kMyanmar;
    case CSSValueID::kOriya:
      return EListStyleType::kOriya;
    case CSSValueID::kPersian:
      return EListStyleType::kPersian;
    case CSSValueID::kUrdu:
      return EListStyleType::kUrdu;
    case CSSValueID::kTelugu:
      return EListStyleType::kTelugu;
    case CSSValueID::kTibetan:
      return EListStyleType::kTibetan;
    case CSSValueID::kThai:
      return EListStyleType::kThai;
    case CSSValueID::kLowerRoman:
      return EListStyleType::kLowerRoman;
    case CSSValueID::kUpperRoman:
      return EListStyleType::kUpperRoman;
    case CSSValueID::kLowerGreek:
      return EListStyleType::kLowerGreek;
    case CSSValueID::kLowerAlpha:
      return EListStyleType::kLowerAlpha;
    case CSSValueID::kLowerLatin:
      return EListStyleType::kLowerLatin;
    case CSSValueID::kUpperAlpha:
      return EListStyleType::kUpperAlpha;
    case CSSValueID::kUpperLatin:
      return EListStyleType::kUpperLatin;
    case CSSValueID::kCjkEarthlyBranch:
      return EListStyleType::kCjkEarthlyBranch;
    case CSSValueID::kCjkHeavenlyStem:
      return EListStyleType::kCjkHeavenlyStem;
    case CSSValueID::kEthiopicHalehame:
      return EListStyleType::kEthiopicHalehame;
    case CSSValueID::kEthiopicHalehameAm:
      return EListStyleType::kEthiopicHalehameAm;
    case CSSValueID::kEthiopicHalehameTiEr:
      return EListStyleType::kEthiopicHalehameTiEr;
    case CSSValueID::kEthiopicHalehameTiEt:
      return EListStyleType::kEthiopicHalehameTiEt;
    case CSSValueID::kHangul:
      return EListStyleType::kHangul;
    case CSSValueID::kHangulConsonant:
      return EListStyleType::kHangulConsonant;
    case CSSValueID::kKoreanHangulFormal:
      return EListStyleType::kKoreanHangulFormal;
    case CSSValueID::kKoreanHanjaFormal:
      return EListStyleType::kKoreanHanjaFormal;
    case CSSValueID::kKoreanHanjaInformal:
      return EListStyleType::kKoreanHanjaInformal;
    case CSSValueID::kHebrew:
      return EListStyleType::kHebrew;
    case CSSValueID::kArmenian:
      return EListStyleType::kArmenian;
    case CSSValueID::kLowerArmenian:
      return EListStyleType::kLowerArmenian;
    case CSSValueID::kUpperArmenian:
      return EListStyleType::kUpperArmenian;
    case CSSValueID::kGeorgian:
      return EListStyleType::kGeorgian;
    case CSSValueID::kCjkIdeographic:
      return EListStyleType::kCjkIdeographic;
    case CSSValueID::kSimpChineseFormal:
      return EListStyleType::kSimpChineseFormal;
    case CSSValueID::kSimpChineseInformal:
      return EListStyleType::kSimpChineseInformal;
    case CSSValueID::kTradChineseFormal:
      return EListStyleType::kTradChineseFormal;
    case CSSValueID::kTradChineseInformal:
      return EListStyleType::kTradChineseInformal;
    case CSSValueID::kHiragana:
      return EListStyleType::kHiragana;
    case CSSValueID::kKatakana:
      return EListStyleType::kKatakana;
    case CSSValueID::kHiraganaIroha:
      return EListStyleType::kHiraganaIroha;
    case CSSValueID::kKatakanaIroha:
      return EListStyleType::kKatakanaIroha;
    case CSSValueID::kNone:
      return EListStyleType::kNone;
    default:
      break;
  }

  return EListStyleType::kNone;
}

template <>
inline EUserSelect CssValueIDToPlatformEnum(CSSValueID v) {
  if (v == CSSValueID::kAuto)
    return EUserSelect::kAuto;
  return detail::cssValueIDToPlatformEnumGenerated<EUserSelect>(v);
}

template <>
inline CSSValueID PlatformEnumToCSSValueID(EDisplay v) {
  if (v == EDisplay::kNone)
    return CSSValueID::kNone;
  if (v == EDisplay::kInline)
    return CSSValueID::kInline;
  if (v == EDisplay::kBlock)
    return CSSValueID::kBlock;
  if (v == EDisplay::kFlowRoot)
    return CSSValueID::kFlowRoot;
  if (v == EDisplay::kListItem)
    return CSSValueID::kListItem;
  if (v == EDisplay::kInlineBlock)
    return CSSValueID::kInlineBlock;
  if (v == EDisplay::kTable)
    return CSSValueID::kTable;
  if (v == EDisplay::kInlineTable)
    return CSSValueID::kInlineTable;
  if (v == EDisplay::kTableRowGroup)
    return CSSValueID::kTableRowGroup;
  if (v == EDisplay::kTableHeaderGroup)
    return CSSValueID::kTableHeaderGroup;
  if (v == EDisplay::kTableFooterGroup)
    return CSSValueID::kTableFooterGroup;
  if (v == EDisplay::kTableRow)
    return CSSValueID::kTableRow;
  if (v == EDisplay::kTableColumnGroup)
    return CSSValueID::kTableColumnGroup;
  if (v == EDisplay::kTableColumn)
    return CSSValueID::kTableColumn;
  if (v == EDisplay::kTableCell)
    return CSSValueID::kTableCell;
  if (v == EDisplay::kTableCaption)
    return CSSValueID::kTableCaption;
  if (v == EDisplay::kWebkitBox)
    return CSSValueID::kWebkitBox;
  if (v == EDisplay::kWebkitInlineBox)
    return CSSValueID::kWebkitInlineBox;
  if (v == EDisplay::kFlex)
    return CSSValueID::kFlex;
  if (v == EDisplay::kInlineFlex)
    return CSSValueID::kInlineFlex;
  if (v == EDisplay::kGrid)
    return CSSValueID::kGrid;
  if (v == EDisplay::kInlineGrid)
    return CSSValueID::kInlineGrid;
  if (v == EDisplay::kContents)
    return CSSValueID::kContents;
  if (v == EDisplay::kMath)
    return CSSValueID::kMath;

  NOTREACHED();
  return CSSValueID::kInline;
}

template <>
inline CSSValueID PlatformEnumToCSSValueID(EListStyleType v) {
  switch (v) {
    case EListStyleType::kDisc:
      return CSSValueID::kDisc;
    case EListStyleType::kCircle:
      return CSSValueID::kCircle;
    case EListStyleType::kSquare:
      return CSSValueID::kSquare;
    case EListStyleType::kDecimal:
      return CSSValueID::kDecimal;
    case EListStyleType::kDecimalLeadingZero:
      return CSSValueID::kDecimalLeadingZero;
    case EListStyleType::kArabicIndic:
      return CSSValueID::kArabicIndic;
    case EListStyleType::kBengali:
      return CSSValueID::kBengali;
    case EListStyleType::kCambodian:
      return CSSValueID::kCambodian;
    case EListStyleType::kKhmer:
      return CSSValueID::kKhmer;
    case EListStyleType::kDevanagari:
      return CSSValueID::kDevanagari;
    case EListStyleType::kGujarati:
      return CSSValueID::kGujarati;
    case EListStyleType::kGurmukhi:
      return CSSValueID::kGurmukhi;
    case EListStyleType::kKannada:
      return CSSValueID::kKannada;
    case EListStyleType::kLao:
      return CSSValueID::kLao;
    case EListStyleType::kMalayalam:
      return CSSValueID::kMalayalam;
    case EListStyleType::kMongolian:
      return CSSValueID::kMongolian;
    case EListStyleType::kMyanmar:
      return CSSValueID::kMyanmar;
    case EListStyleType::kOriya:
      return CSSValueID::kOriya;
    case EListStyleType::kPersian:
      return CSSValueID::kPersian;
    case EListStyleType::kUrdu:
      return CSSValueID::kUrdu;
    case EListStyleType::kTelugu:
      return CSSValueID::kTelugu;
    case EListStyleType::kTibetan:
      return CSSValueID::kTibetan;
    case EListStyleType::kThai:
      return CSSValueID::kThai;
    case EListStyleType::kLowerRoman:
      return CSSValueID::kLowerRoman;
    case EListStyleType::kUpperRoman:
      return CSSValueID::kUpperRoman;
    case EListStyleType::kLowerGreek:
      return CSSValueID::kLowerGreek;
    case EListStyleType::kLowerAlpha:
      return CSSValueID::kLowerAlpha;
    case EListStyleType::kLowerLatin:
      return CSSValueID::kLowerLatin;
    case EListStyleType::kUpperAlpha:
      return CSSValueID::kUpperAlpha;
    case EListStyleType::kUpperLatin:
      return CSSValueID::kUpperLatin;
    case EListStyleType::kCjkEarthlyBranch:
      return CSSValueID::kCjkEarthlyBranch;
    case EListStyleType::kCjkHeavenlyStem:
      return CSSValueID::kCjkHeavenlyStem;
    case EListStyleType::kEthiopicHalehame:
      return CSSValueID::kEthiopicHalehame;
    case EListStyleType::kEthiopicHalehameAm:
      return CSSValueID::kEthiopicHalehameAm;
    case EListStyleType::kEthiopicHalehameTiEr:
      return CSSValueID::kEthiopicHalehameTiEr;
    case EListStyleType::kEthiopicHalehameTiEt:
      return CSSValueID::kEthiopicHalehameTiEt;
    case EListStyleType::kHangul:
      return CSSValueID::kHangul;
    case EListStyleType::kHangulConsonant:
      return CSSValueID::kHangulConsonant;
    case EListStyleType::kKoreanHangulFormal:
      return CSSValueID::kKoreanHangulFormal;
    case EListStyleType::kKoreanHanjaFormal:
      return CSSValueID::kKoreanHanjaFormal;
    case EListStyleType::kKoreanHanjaInformal:
      return CSSValueID::kKoreanHanjaInformal;
    case EListStyleType::kHebrew:
      return CSSValueID::kHebrew;
    case EListStyleType::kArmenian:
      return CSSValueID::kArmenian;
    case EListStyleType::kLowerArmenian:
      return CSSValueID::kLowerArmenian;
    case EListStyleType::kUpperArmenian:
      return CSSValueID::kUpperArmenian;
    case EListStyleType::kGeorgian:
      return CSSValueID::kGeorgian;
    case EListStyleType::kCjkIdeographic:
      return CSSValueID::kCjkIdeographic;
    case EListStyleType::kSimpChineseFormal:
      return CSSValueID::kSimpChineseFormal;
    case EListStyleType::kSimpChineseInformal:
      return CSSValueID::kSimpChineseInformal;
    case EListStyleType::kTradChineseFormal:
      return CSSValueID::kTradChineseFormal;
    case EListStyleType::kTradChineseInformal:
      return CSSValueID::kTradChineseInformal;
    case EListStyleType::kHiragana:
      return CSSValueID::kHiragana;
    case EListStyleType::kKatakana:
      return CSSValueID::kKatakana;
    case EListStyleType::kHiraganaIroha:
      return CSSValueID::kHiraganaIroha;
    case EListStyleType::kKatakanaIroha:
      return CSSValueID::kKatakanaIroha;
    case EListStyleType::kNone:
      return CSSValueID::kNone;
    case EListStyleType::kString:
      NOTREACHED();
      break;
  }

  NOTREACHED();
  return CSSValueID::kDisc;
}

template <>
inline PageOrientation CssValueIDToPlatformEnum(CSSValueID v) {
  if (v == CSSValueID::kUpright)
    return PageOrientation::kUpright;
  if (v == CSSValueID::kRotateLeft)
    return PageOrientation::kRotateLeft;
  if (v == CSSValueID::kRotateRight)
    return PageOrientation::kRotateRight;

  NOTREACHED();
  return PageOrientation::kUpright;
}

template <>
inline ScrollbarGutter CssValueIDToPlatformEnum(CSSValueID v) {
  if (v == CSSValueID::kAuto)
    return kScrollbarGutterAuto;
  if (v == CSSValueID::kStable)
    return kScrollbarGutterStable;
  if (v == CSSValueID::kAlways)
    return kScrollbarGutterAlways;
  if (v == CSSValueID::kBoth)
    return kScrollbarGutterBoth;
  if (v == CSSValueID::kForce)
    return kScrollbarGutterForce;

  NOTREACHED();
  return kScrollbarGutterAuto;
}

}  // namespace blink

#endif
