/*
 * Copyright (C) 2007 Alexey Proskuryakov <ap@nypop.com>.
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2009 Jeff Schiller <codedread@gmail.com>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PRIMITIVE_VALUE_MAPPINGS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PRIMITIVE_VALUE_MAPPINGS_H_

#include "cc/input/scroll_snap_data.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_reflection_direction.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/scroll/scroll_customization.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/svg_computed_style_defs.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_smoothing_mode.h"
#include "third_party/blink/renderer/platform/fonts/text_rendering_mode.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/theme_types.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

// TODO(sashab): Move these to CSSPrimitiveValue.h.
template <>
inline int16_t CSSPrimitiveValue::ConvertTo() const {
  DCHECK(IsNumber());
  return clampTo<int16_t>(GetDoubleValue());
}

template <>
inline uint16_t CSSPrimitiveValue::ConvertTo() const {
  DCHECK(IsNumber());
  return clampTo<uint16_t>(GetDoubleValue());
}

template <>
inline int CSSPrimitiveValue::ConvertTo() const {
  DCHECK(IsNumber());
  return clampTo<int>(GetDoubleValue());
}

template <>
inline unsigned CSSPrimitiveValue::ConvertTo() const {
  DCHECK(IsNumber());
  return clampTo<unsigned>(GetDoubleValue());
}

template <>
inline float CSSPrimitiveValue::ConvertTo() const {
  DCHECK(IsNumber());
  return clampTo<float>(GetDoubleValue());
}

// TODO(sashab): Move these to CSSIdentifierValueMappings.h, and update to use
// the CSSValuePool.
template <>
inline CSSIdentifierValue::CSSIdentifierValue(CSSReflectionDirection e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case kReflectionAbove:
      value_id_ = CSSValueID::kAbove;
      break;
    case kReflectionBelow:
      value_id_ = CSSValueID::kBelow;
      break;
    case kReflectionLeft:
      value_id_ = CSSValueID::kLeft;
      break;
    case kReflectionRight:
      value_id_ = CSSValueID::kRight;
  }
}

template <>
inline CSSReflectionDirection CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kAbove:
      return kReflectionAbove;
    case CSSValueID::kBelow:
      return kReflectionBelow;
    case CSSValueID::kLeft:
      return kReflectionLeft;
    case CSSValueID::kRight:
      return kReflectionRight;
    default:
      break;
  }

  NOTREACHED();
  return kReflectionBelow;
}

template <>
inline EBorderStyle CSSIdentifierValue::ConvertTo() const {
  if (value_id_ == CSSValueID::kAuto)  // Valid for CSS outline-style
    return EBorderStyle::kDotted;
  return detail::cssValueIDToPlatformEnumGenerated<EBorderStyle>(value_id_);
}

template <>
inline OutlineIsAuto CSSIdentifierValue::ConvertTo() const {
  if (value_id_ == CSSValueID::kAuto)
    return OutlineIsAuto::kOn;
  return OutlineIsAuto::kOff;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(CompositeOperator e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case kCompositeClear:
      value_id_ = CSSValueID::kClear;
      break;
    case kCompositeCopy:
      value_id_ = CSSValueID::kCopy;
      break;
    case kCompositeSourceOver:
      value_id_ = CSSValueID::kSourceOver;
      break;
    case kCompositeSourceIn:
      value_id_ = CSSValueID::kSourceIn;
      break;
    case kCompositeSourceOut:
      value_id_ = CSSValueID::kSourceOut;
      break;
    case kCompositeSourceAtop:
      value_id_ = CSSValueID::kSourceAtop;
      break;
    case kCompositeDestinationOver:
      value_id_ = CSSValueID::kDestinationOver;
      break;
    case kCompositeDestinationIn:
      value_id_ = CSSValueID::kDestinationIn;
      break;
    case kCompositeDestinationOut:
      value_id_ = CSSValueID::kDestinationOut;
      break;
    case kCompositeDestinationAtop:
      value_id_ = CSSValueID::kDestinationAtop;
      break;
    case kCompositeXOR:
      value_id_ = CSSValueID::kXor;
      break;
    case kCompositePlusLighter:
      value_id_ = CSSValueID::kPlusLighter;
      break;
    default:
      NOTREACHED();
      break;
  }
}

template <>
inline CompositeOperator CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kClear:
      return kCompositeClear;
    case CSSValueID::kCopy:
      return kCompositeCopy;
    case CSSValueID::kSourceOver:
      return kCompositeSourceOver;
    case CSSValueID::kSourceIn:
      return kCompositeSourceIn;
    case CSSValueID::kSourceOut:
      return kCompositeSourceOut;
    case CSSValueID::kSourceAtop:
      return kCompositeSourceAtop;
    case CSSValueID::kDestinationOver:
      return kCompositeDestinationOver;
    case CSSValueID::kDestinationIn:
      return kCompositeDestinationIn;
    case CSSValueID::kDestinationOut:
      return kCompositeDestinationOut;
    case CSSValueID::kDestinationAtop:
      return kCompositeDestinationAtop;
    case CSSValueID::kXor:
      return kCompositeXOR;
    case CSSValueID::kPlusLighter:
      return kCompositePlusLighter;
    default:
      break;
  }

  NOTREACHED();
  return kCompositeClear;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ControlPart e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case kNoControlPart:
      value_id_ = CSSValueID::kNone;
      break;
    case kCheckboxPart:
      value_id_ = CSSValueID::kCheckbox;
      break;
    case kRadioPart:
      value_id_ = CSSValueID::kRadio;
      break;
    case kPushButtonPart:
      value_id_ = CSSValueID::kPushButton;
      break;
    case kSquareButtonPart:
      value_id_ = CSSValueID::kSquareButton;
      break;
    case kButtonPart:
      value_id_ = CSSValueID::kButton;
      break;
    case kInnerSpinButtonPart:
      value_id_ = CSSValueID::kInnerSpinButton;
      break;
    case kListboxPart:
      value_id_ = CSSValueID::kListbox;
      break;
    case kMediaSliderPart:
      value_id_ = CSSValueID::kMediaSlider;
      break;
    case kMediaSliderThumbPart:
      value_id_ = CSSValueID::kMediaSliderthumb;
      break;
    case kMediaVolumeSliderPart:
      value_id_ = CSSValueID::kMediaVolumeSlider;
      break;
    case kMediaVolumeSliderThumbPart:
      value_id_ = CSSValueID::kMediaVolumeSliderthumb;
      break;
    case kMediaControlPart:
      value_id_ = CSSValueID::kInternalMediaControl;
      break;
    case kMenulistPart:
      value_id_ = CSSValueID::kMenulist;
      break;
    case kMenulistButtonPart:
      value_id_ = CSSValueID::kMenulistButton;
      break;
    case kMeterPart:
      value_id_ = CSSValueID::kMeter;
      break;
    case kProgressBarPart:
      value_id_ = CSSValueID::kProgressBar;
      break;
    case kSliderHorizontalPart:
      value_id_ = CSSValueID::kSliderHorizontal;
      break;
    case kSliderVerticalPart:
      value_id_ = CSSValueID::kSliderVertical;
      break;
    case kSliderThumbHorizontalPart:
      value_id_ = CSSValueID::kSliderthumbHorizontal;
      break;
    case kSliderThumbVerticalPart:
      value_id_ = CSSValueID::kSliderthumbVertical;
      break;
    case kSearchFieldPart:
      value_id_ = CSSValueID::kSearchfield;
      break;
    case kSearchFieldCancelButtonPart:
      value_id_ = CSSValueID::kSearchfieldCancelButton;
      break;
    case kTextFieldPart:
      value_id_ = CSSValueID::kTextfield;
      break;
    case kTextAreaPart:
      value_id_ = CSSValueID::kTextarea;
      break;
  }
}

template <>
inline ControlPart CSSIdentifierValue::ConvertTo() const {
  if (value_id_ == CSSValueID::kNone)
    return kNoControlPart;
  return ControlPart(static_cast<int>(value_id_) -
                     static_cast<int>(CSSValueID::kCheckbox) + 1);
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EFillAttachment e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case EFillAttachment::kScroll:
      value_id_ = CSSValueID::kScroll;
      break;
    case EFillAttachment::kLocal:
      value_id_ = CSSValueID::kLocal;
      break;
    case EFillAttachment::kFixed:
      value_id_ = CSSValueID::kFixed;
      break;
  }
}

template <>
inline EFillAttachment CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kScroll:
      return EFillAttachment::kScroll;
    case CSSValueID::kLocal:
      return EFillAttachment::kLocal;
    case CSSValueID::kFixed:
      return EFillAttachment::kFixed;
    default:
      break;
  }

  NOTREACHED();
  return EFillAttachment::kScroll;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EFillBox e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case EFillBox::kBorder:
      value_id_ = CSSValueID::kBorderBox;
      break;
    case EFillBox::kPadding:
      value_id_ = CSSValueID::kPaddingBox;
      break;
    case EFillBox::kContent:
      value_id_ = CSSValueID::kContentBox;
      break;
    case EFillBox::kText:
      value_id_ = CSSValueID::kText;
      break;
  }
}

template <>
inline EFillBox CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kBorder:
    case CSSValueID::kBorderBox:
      return EFillBox::kBorder;
    case CSSValueID::kPadding:
    case CSSValueID::kPaddingBox:
      return EFillBox::kPadding;
    case CSSValueID::kContent:
    case CSSValueID::kContentBox:
      return EFillBox::kContent;
    case CSSValueID::kText:
      return EFillBox::kText;
    default:
      break;
  }

  NOTREACHED();
  return EFillBox::kBorder;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EFillRepeat e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case EFillRepeat::kRepeatFill:
      value_id_ = CSSValueID::kRepeat;
      break;
    case EFillRepeat::kNoRepeatFill:
      value_id_ = CSSValueID::kNoRepeat;
      break;
    case EFillRepeat::kRoundFill:
      value_id_ = CSSValueID::kRound;
      break;
    case EFillRepeat::kSpaceFill:
      value_id_ = CSSValueID::kSpace;
      break;
  }
}

template <>
inline EFillRepeat CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kRepeat:
      return EFillRepeat::kRepeatFill;
    case CSSValueID::kNoRepeat:
      return EFillRepeat::kNoRepeatFill;
    case CSSValueID::kRound:
      return EFillRepeat::kRoundFill;
    case CSSValueID::kSpace:
      return EFillRepeat::kSpaceFill;
    default:
      break;
  }

  NOTREACHED();
  return EFillRepeat::kRepeatFill;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(BackgroundEdgeOrigin e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case BackgroundEdgeOrigin::kTop:
      value_id_ = CSSValueID::kTop;
      break;
    case BackgroundEdgeOrigin::kRight:
      value_id_ = CSSValueID::kRight;
      break;
    case BackgroundEdgeOrigin::kBottom:
      value_id_ = CSSValueID::kBottom;
      break;
    case BackgroundEdgeOrigin::kLeft:
      value_id_ = CSSValueID::kLeft;
      break;
  }
}

template <>
inline BackgroundEdgeOrigin CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kTop:
      return BackgroundEdgeOrigin::kTop;
    case CSSValueID::kRight:
      return BackgroundEdgeOrigin::kRight;
    case CSSValueID::kBottom:
      return BackgroundEdgeOrigin::kBottom;
    case CSSValueID::kLeft:
      return BackgroundEdgeOrigin::kLeft;
    default:
      break;
  }

  NOTREACHED();
  return BackgroundEdgeOrigin::kTop;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EFloat e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case EFloat::kNone:
      value_id_ = CSSValueID::kNone;
      break;
    case EFloat::kLeft:
      value_id_ = CSSValueID::kLeft;
      break;
    case EFloat::kRight:
      value_id_ = CSSValueID::kRight;
      break;
    case EFloat::kInlineStart:
      value_id_ = CSSValueID::kInlineStart;
      break;
    case EFloat::kInlineEnd:
      value_id_ = CSSValueID::kInlineEnd;
      break;
  }
}

template <>
inline EFloat CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kLeft:
      return EFloat::kLeft;
    case CSSValueID::kRight:
      return EFloat::kRight;
    case CSSValueID::kInlineStart:
      return EFloat::kInlineStart;
    case CSSValueID::kInlineEnd:
      return EFloat::kInlineEnd;
    case CSSValueID::kNone:
      return EFloat::kNone;
    default:
      break;
  }

  NOTREACHED();
  return EFloat::kNone;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EPosition e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case EPosition::kStatic:
      value_id_ = CSSValueID::kStatic;
      break;
    case EPosition::kRelative:
      value_id_ = CSSValueID::kRelative;
      break;
    case EPosition::kAbsolute:
      value_id_ = CSSValueID::kAbsolute;
      break;
    case EPosition::kFixed:
      value_id_ = CSSValueID::kFixed;
      break;
    case EPosition::kSticky:
      value_id_ = CSSValueID::kSticky;
      break;
  }
}

template <>
inline EPosition CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kStatic:
      return EPosition::kStatic;
    case CSSValueID::kRelative:
      return EPosition::kRelative;
    case CSSValueID::kAbsolute:
      return EPosition::kAbsolute;
    case CSSValueID::kFixed:
      return EPosition::kFixed;
    case CSSValueID::kSticky:
      return EPosition::kSticky;
    default:
      break;
  }

  NOTREACHED();
  return EPosition::kStatic;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ETableLayout e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case ETableLayout::kAuto:
      value_id_ = CSSValueID::kAuto;
      break;
    case ETableLayout::kFixed:
      value_id_ = CSSValueID::kFixed;
      break;
  }
}

template <>
inline ETableLayout CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kFixed:
      return ETableLayout::kFixed;
    case CSSValueID::kAuto:
      return ETableLayout::kAuto;
    default:
      break;
  }

  NOTREACHED();
  return ETableLayout::kAuto;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EVerticalAlign a)
    : CSSValue(kIdentifierClass) {
  switch (a) {
    case EVerticalAlign::kTop:
      value_id_ = CSSValueID::kTop;
      break;
    case EVerticalAlign::kBottom:
      value_id_ = CSSValueID::kBottom;
      break;
    case EVerticalAlign::kMiddle:
      value_id_ = CSSValueID::kMiddle;
      break;
    case EVerticalAlign::kBaseline:
      value_id_ = CSSValueID::kBaseline;
      break;
    case EVerticalAlign::kTextBottom:
      value_id_ = CSSValueID::kTextBottom;
      break;
    case EVerticalAlign::kTextTop:
      value_id_ = CSSValueID::kTextTop;
      break;
    case EVerticalAlign::kSub:
      value_id_ = CSSValueID::kSub;
      break;
    case EVerticalAlign::kSuper:
      value_id_ = CSSValueID::kSuper;
      break;
    case EVerticalAlign::kBaselineMiddle:
      value_id_ = CSSValueID::kWebkitBaselineMiddle;
      break;
    case EVerticalAlign::kLength:
      value_id_ = CSSValueID::kInvalid;
  }
}

template <>
inline EVerticalAlign CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kTop:
      return EVerticalAlign::kTop;
    case CSSValueID::kBottom:
      return EVerticalAlign::kBottom;
    case CSSValueID::kMiddle:
      return EVerticalAlign::kMiddle;
    case CSSValueID::kBaseline:
      return EVerticalAlign::kBaseline;
    case CSSValueID::kTextBottom:
      return EVerticalAlign::kTextBottom;
    case CSSValueID::kTextTop:
      return EVerticalAlign::kTextTop;
    case CSSValueID::kSub:
      return EVerticalAlign::kSub;
    case CSSValueID::kSuper:
      return EVerticalAlign::kSuper;
    case CSSValueID::kWebkitBaselineMiddle:
      return EVerticalAlign::kBaselineMiddle;
    default:
      break;
  }

  NOTREACHED();
  return EVerticalAlign::kTop;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(TextEmphasisFill fill)
    : CSSValue(kIdentifierClass) {
  switch (fill) {
    case TextEmphasisFill::kFilled:
      value_id_ = CSSValueID::kFilled;
      break;
    case TextEmphasisFill::kOpen:
      value_id_ = CSSValueID::kOpen;
      break;
  }
}

template <>
inline TextEmphasisFill CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kFilled:
      return TextEmphasisFill::kFilled;
    case CSSValueID::kOpen:
      return TextEmphasisFill::kOpen;
    default:
      break;
  }

  NOTREACHED();
  return TextEmphasisFill::kFilled;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(TextEmphasisMark mark)
    : CSSValue(kIdentifierClass) {
  switch (mark) {
    case TextEmphasisMark::kDot:
      value_id_ = CSSValueID::kDot;
      break;
    case TextEmphasisMark::kCircle:
      value_id_ = CSSValueID::kCircle;
      break;
    case TextEmphasisMark::kDoubleCircle:
      value_id_ = CSSValueID::kDoubleCircle;
      break;
    case TextEmphasisMark::kTriangle:
      value_id_ = CSSValueID::kTriangle;
      break;
    case TextEmphasisMark::kSesame:
      value_id_ = CSSValueID::kSesame;
      break;
    case TextEmphasisMark::kNone:
    case TextEmphasisMark::kAuto:
    case TextEmphasisMark::kCustom:
      NOTREACHED();
      value_id_ = CSSValueID::kNone;
      break;
  }
}

template <>
inline TextEmphasisMark CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kNone:
      return TextEmphasisMark::kNone;
    case CSSValueID::kDot:
      return TextEmphasisMark::kDot;
    case CSSValueID::kCircle:
      return TextEmphasisMark::kCircle;
    case CSSValueID::kDoubleCircle:
      return TextEmphasisMark::kDoubleCircle;
    case CSSValueID::kTriangle:
      return TextEmphasisMark::kTriangle;
    case CSSValueID::kSesame:
      return TextEmphasisMark::kSesame;
    default:
      break;
  }

  NOTREACHED();
  return TextEmphasisMark::kNone;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(FontDescription::Kerning kerning)
    : CSSValue(kIdentifierClass) {
  switch (kerning) {
    case FontDescription::kAutoKerning:
      value_id_ = CSSValueID::kAuto;
      return;
    case FontDescription::kNormalKerning:
      value_id_ = CSSValueID::kNormal;
      return;
    case FontDescription::kNoneKerning:
      value_id_ = CSSValueID::kNone;
      return;
  }

  NOTREACHED();
  value_id_ = CSSValueID::kAuto;
}

template <>
inline FontDescription::Kerning CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kAuto:
      return FontDescription::kAutoKerning;
    case CSSValueID::kNormal:
      return FontDescription::kNormalKerning;
    case CSSValueID::kNone:
      return FontDescription::kNoneKerning;
    default:
      break;
  }

  NOTREACHED();
  return FontDescription::kAutoKerning;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(OpticalSizing optical_sizing)
    : CSSValue(kIdentifierClass) {
  switch (optical_sizing) {
    case kAutoOpticalSizing:
      value_id_ = CSSValueID::kAuto;
      return;
    case kNoneOpticalSizing:
      value_id_ = CSSValueID::kNone;
      return;
  }

  NOTREACHED();
  value_id_ = CSSValueID::kAuto;
}

template <>
inline OpticalSizing CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kAuto:
      return kAutoOpticalSizing;
    case CSSValueID::kNone:
      return kNoneOpticalSizing;
    default:
      break;
  }

  NOTREACHED();
  return kAutoOpticalSizing;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EFillSizeType fill_size)
    : CSSValue(kIdentifierClass) {
  switch (fill_size) {
    case EFillSizeType::kContain:
      value_id_ = CSSValueID::kContain;
      break;
    case EFillSizeType::kCover:
      value_id_ = CSSValueID::kCover;
      break;
    case EFillSizeType::kSizeNone:
    case EFillSizeType::kSizeLength:
    default:
      NOTREACHED();
  }
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(FontSmoothingMode smoothing)
    : CSSValue(kIdentifierClass) {
  switch (smoothing) {
    case kAutoSmoothing:
      value_id_ = CSSValueID::kAuto;
      return;
    case kNoSmoothing:
      value_id_ = CSSValueID::kNone;
      return;
    case kAntialiased:
      value_id_ = CSSValueID::kAntialiased;
      return;
    case kSubpixelAntialiased:
      value_id_ = CSSValueID::kSubpixelAntialiased;
      return;
  }

  NOTREACHED();
  value_id_ = CSSValueID::kAuto;
}

template <>
inline FontSmoothingMode CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kAuto:
      return kAutoSmoothing;
    case CSSValueID::kNone:
      return kNoSmoothing;
    case CSSValueID::kAntialiased:
      return kAntialiased;
    case CSSValueID::kSubpixelAntialiased:
      return kSubpixelAntialiased;
    default:
      break;
  }

  NOTREACHED();
  return kAutoSmoothing;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(TextRenderingMode e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case kAutoTextRendering:
      value_id_ = CSSValueID::kAuto;
      break;
    case kOptimizeSpeed:
      value_id_ = CSSValueID::kOptimizespeed;
      break;
    case kOptimizeLegibility:
      value_id_ = CSSValueID::kOptimizelegibility;
      break;
    case kGeometricPrecision:
      value_id_ = CSSValueID::kGeometricprecision;
      break;
  }
}

template <>
inline TextRenderingMode CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kAuto:
      return kAutoTextRendering;
    case CSSValueID::kOptimizespeed:
      return kOptimizeSpeed;
    case CSSValueID::kOptimizelegibility:
      return kOptimizeLegibility;
    case CSSValueID::kGeometricprecision:
      return kGeometricPrecision;
    default:
      break;
  }

  NOTREACHED();
  return kAutoTextRendering;
}

template <>
inline EOrder CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kLogical:
      return EOrder::kLogical;
    case CSSValueID::kVisual:
      return EOrder::kVisual;
    default:
      break;
  }

  NOTREACHED();
  return EOrder::kLogical;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EOrder e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case EOrder::kLogical:
      value_id_ = CSSValueID::kLogical;
      break;
    case EOrder::kVisual:
      value_id_ = CSSValueID::kVisual;
      break;
  }
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(LineCap e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case kButtCap:
      value_id_ = CSSValueID::kButt;
      break;
    case kRoundCap:
      value_id_ = CSSValueID::kRound;
      break;
    case kSquareCap:
      value_id_ = CSSValueID::kSquare;
      break;
  }
}

template <>
inline LineCap CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kButt:
      return kButtCap;
    case CSSValueID::kRound:
      return kRoundCap;
    case CSSValueID::kSquare:
      return kSquareCap;
    default:
      break;
  }

  NOTREACHED();
  return kButtCap;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(LineJoin e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case kMiterJoin:
      value_id_ = CSSValueID::kMiter;
      break;
    case kRoundJoin:
      value_id_ = CSSValueID::kRound;
      break;
    case kBevelJoin:
      value_id_ = CSSValueID::kBevel;
      break;
  }
}

template <>
inline LineJoin CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kMiter:
      return kMiterJoin;
    case CSSValueID::kRound:
      return kRoundJoin;
    case CSSValueID::kBevel:
      return kBevelJoin;
    default:
      break;
  }

  NOTREACHED();
  return kMiterJoin;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(WindRule e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case RULE_NONZERO:
      value_id_ = CSSValueID::kNonzero;
      break;
    case RULE_EVENODD:
      value_id_ = CSSValueID::kEvenodd;
      break;
  }
}

template <>
inline WindRule CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kNonzero:
      return RULE_NONZERO;
    case CSSValueID::kEvenodd:
      return RULE_EVENODD;
    default:
      break;
  }

  NOTREACHED();
  return RULE_NONZERO;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EAlignmentBaseline e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case AB_AUTO:
      value_id_ = CSSValueID::kAuto;
      break;
    case AB_BASELINE:
      value_id_ = CSSValueID::kBaseline;
      break;
    case AB_BEFORE_EDGE:
      value_id_ = CSSValueID::kBeforeEdge;
      break;
    case AB_TEXT_BEFORE_EDGE:
      value_id_ = CSSValueID::kTextBeforeEdge;
      break;
    case AB_MIDDLE:
      value_id_ = CSSValueID::kMiddle;
      break;
    case AB_CENTRAL:
      value_id_ = CSSValueID::kCentral;
      break;
    case AB_AFTER_EDGE:
      value_id_ = CSSValueID::kAfterEdge;
      break;
    case AB_TEXT_AFTER_EDGE:
      value_id_ = CSSValueID::kTextAfterEdge;
      break;
    case AB_IDEOGRAPHIC:
      value_id_ = CSSValueID::kIdeographic;
      break;
    case AB_ALPHABETIC:
      value_id_ = CSSValueID::kAlphabetic;
      break;
    case AB_HANGING:
      value_id_ = CSSValueID::kHanging;
      break;
    case AB_MATHEMATICAL:
      value_id_ = CSSValueID::kMathematical;
      break;
  }
}

template <>
inline EAlignmentBaseline CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kAuto:
      return AB_AUTO;
    case CSSValueID::kBaseline:
      return AB_BASELINE;
    case CSSValueID::kBeforeEdge:
      return AB_BEFORE_EDGE;
    case CSSValueID::kTextBeforeEdge:
      return AB_TEXT_BEFORE_EDGE;
    case CSSValueID::kMiddle:
      return AB_MIDDLE;
    case CSSValueID::kCentral:
      return AB_CENTRAL;
    case CSSValueID::kAfterEdge:
      return AB_AFTER_EDGE;
    case CSSValueID::kTextAfterEdge:
      return AB_TEXT_AFTER_EDGE;
    case CSSValueID::kIdeographic:
      return AB_IDEOGRAPHIC;
    case CSSValueID::kAlphabetic:
      return AB_ALPHABETIC;
    case CSSValueID::kHanging:
      return AB_HANGING;
    case CSSValueID::kMathematical:
      return AB_MATHEMATICAL;
    default:
      break;
  }

  NOTREACHED();
  return AB_AUTO;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EBufferedRendering e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case BR_AUTO:
      value_id_ = CSSValueID::kAuto;
      break;
    case BR_DYNAMIC:
      value_id_ = CSSValueID::kDynamic;
      break;
    case BR_STATIC:
      value_id_ = CSSValueID::kStatic;
      break;
  }
}

template <>
inline EBufferedRendering CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kAuto:
      return BR_AUTO;
    case CSSValueID::kDynamic:
      return BR_DYNAMIC;
    case CSSValueID::kStatic:
      return BR_STATIC;
    default:
      break;
  }

  NOTREACHED();
  return BR_AUTO;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EColorInterpolation e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case CI_AUTO:
      value_id_ = CSSValueID::kAuto;
      break;
    case CI_SRGB:
      value_id_ = CSSValueID::kSRGB;
      break;
    case CI_LINEARRGB:
      value_id_ = CSSValueID::kLinearrgb;
      break;
  }
}

template <>
inline EColorInterpolation CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kSRGB:
      return CI_SRGB;
    case CSSValueID::kLinearrgb:
      return CI_LINEARRGB;
    case CSSValueID::kAuto:
      return CI_AUTO;
    default:
      break;
  }

  NOTREACHED();
  return CI_AUTO;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EColorRendering e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case CR_AUTO:
      value_id_ = CSSValueID::kAuto;
      break;
    case CR_OPTIMIZESPEED:
      value_id_ = CSSValueID::kOptimizespeed;
      break;
    case CR_OPTIMIZEQUALITY:
      value_id_ = CSSValueID::kOptimizequality;
      break;
  }
}

template <>
inline EColorRendering CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kOptimizespeed:
      return CR_OPTIMIZESPEED;
    case CSSValueID::kOptimizequality:
      return CR_OPTIMIZEQUALITY;
    case CSSValueID::kAuto:
      return CR_AUTO;
    default:
      break;
  }

  NOTREACHED();
  return CR_AUTO;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EDominantBaseline e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case DB_AUTO:
      value_id_ = CSSValueID::kAuto;
      break;
    case DB_USE_SCRIPT:
      value_id_ = CSSValueID::kUseScript;
      break;
    case DB_NO_CHANGE:
      value_id_ = CSSValueID::kNoChange;
      break;
    case DB_RESET_SIZE:
      value_id_ = CSSValueID::kResetSize;
      break;
    case DB_CENTRAL:
      value_id_ = CSSValueID::kCentral;
      break;
    case DB_MIDDLE:
      value_id_ = CSSValueID::kMiddle;
      break;
    case DB_TEXT_BEFORE_EDGE:
      value_id_ = CSSValueID::kTextBeforeEdge;
      break;
    case DB_TEXT_AFTER_EDGE:
      value_id_ = CSSValueID::kTextAfterEdge;
      break;
    case DB_IDEOGRAPHIC:
      value_id_ = CSSValueID::kIdeographic;
      break;
    case DB_ALPHABETIC:
      value_id_ = CSSValueID::kAlphabetic;
      break;
    case DB_HANGING:
      value_id_ = CSSValueID::kHanging;
      break;
    case DB_MATHEMATICAL:
      value_id_ = CSSValueID::kMathematical;
      break;
  }
}

template <>
inline EDominantBaseline CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kAuto:
      return DB_AUTO;
    case CSSValueID::kUseScript:
      return DB_USE_SCRIPT;
    case CSSValueID::kNoChange:
      return DB_NO_CHANGE;
    case CSSValueID::kResetSize:
      return DB_RESET_SIZE;
    case CSSValueID::kIdeographic:
      return DB_IDEOGRAPHIC;
    case CSSValueID::kAlphabetic:
      return DB_ALPHABETIC;
    case CSSValueID::kHanging:
      return DB_HANGING;
    case CSSValueID::kMathematical:
      return DB_MATHEMATICAL;
    case CSSValueID::kCentral:
      return DB_CENTRAL;
    case CSSValueID::kMiddle:
      return DB_MIDDLE;
    case CSSValueID::kTextAfterEdge:
      return DB_TEXT_AFTER_EDGE;
    case CSSValueID::kTextBeforeEdge:
      return DB_TEXT_BEFORE_EDGE;
    default:
      break;
  }

  NOTREACHED();
  return DB_AUTO;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EShapeRendering e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case SR_AUTO:
      value_id_ = CSSValueID::kAuto;
      break;
    case SR_OPTIMIZESPEED:
      value_id_ = CSSValueID::kOptimizespeed;
      break;
    case SR_CRISPEDGES:
      value_id_ = CSSValueID::kCrispedges;
      break;
    case SR_GEOMETRICPRECISION:
      value_id_ = CSSValueID::kGeometricprecision;
      break;
  }
}

template <>
inline EShapeRendering CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kAuto:
      return SR_AUTO;
    case CSSValueID::kOptimizespeed:
      return SR_OPTIMIZESPEED;
    case CSSValueID::kCrispedges:
      return SR_CRISPEDGES;
    case CSSValueID::kGeometricprecision:
      return SR_GEOMETRICPRECISION;
    default:
      break;
  }

  NOTREACHED();
  return SR_AUTO;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ETextAnchor e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case TA_START:
      value_id_ = CSSValueID::kStart;
      break;
    case TA_MIDDLE:
      value_id_ = CSSValueID::kMiddle;
      break;
    case TA_END:
      value_id_ = CSSValueID::kEnd;
      break;
  }
}

template <>
inline ETextAnchor CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kStart:
      return TA_START;
    case CSSValueID::kMiddle:
      return TA_MIDDLE;
    case CSSValueID::kEnd:
      return TA_END;
    default:
      break;
  }

  NOTREACHED();
  return TA_START;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EVectorEffect e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case VE_NONE:
      value_id_ = CSSValueID::kNone;
      break;
    case VE_NON_SCALING_STROKE:
      value_id_ = CSSValueID::kNonScalingStroke;
      break;
  }
}

template <>
inline EVectorEffect CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kNone:
      return VE_NONE;
    case CSSValueID::kNonScalingStroke:
      return VE_NON_SCALING_STROKE;
    default:
      break;
  }

  NOTREACHED();
  return VE_NONE;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EPaintOrderType e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case PT_FILL:
      value_id_ = CSSValueID::kFill;
      break;
    case PT_STROKE:
      value_id_ = CSSValueID::kStroke;
      break;
    case PT_MARKERS:
      value_id_ = CSSValueID::kMarkers;
      break;
    default:
      NOTREACHED();
      value_id_ = CSSValueID::kFill;
      break;
  }
}

template <>
inline EPaintOrderType CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kFill:
      return PT_FILL;
    case CSSValueID::kStroke:
      return PT_STROKE;
    case CSSValueID::kMarkers:
      return PT_MARKERS;
    default:
      break;
  }

  NOTREACHED();
  return PT_NONE;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EMaskType e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case MT_LUMINANCE:
      value_id_ = CSSValueID::kLuminance;
      break;
    case MT_ALPHA:
      value_id_ = CSSValueID::kAlpha;
      break;
  }
}

template <>
inline EMaskType CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kLuminance:
      return MT_LUMINANCE;
    case CSSValueID::kAlpha:
      return MT_ALPHA;
    default:
      break;
  }

  NOTREACHED();
  return MT_LUMINANCE;
}

template <>
inline TouchAction CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kNone:
      return TouchAction::kTouchActionNone;
    case CSSValueID::kAuto:
      return TouchAction::kTouchActionAuto;
    case CSSValueID::kPanLeft:
      return TouchAction::kTouchActionPanLeft;
    case CSSValueID::kPanRight:
      return TouchAction::kTouchActionPanRight;
    case CSSValueID::kPanX:
      return TouchAction::kTouchActionPanX;
    case CSSValueID::kPanUp:
      return TouchAction::kTouchActionPanUp;
    case CSSValueID::kPanDown:
      return TouchAction::kTouchActionPanDown;
    case CSSValueID::kPanY:
      return TouchAction::kTouchActionPanY;
    case CSSValueID::kManipulation:
      return TouchAction::kTouchActionManipulation;
    case CSSValueID::kPinchZoom:
      return TouchAction::kTouchActionPinchZoom;
    default:
      break;
  }

  NOTREACHED();
  return TouchAction::kTouchActionNone;
}

template <>
inline scroll_customization::ScrollDirection CSSIdentifierValue::ConvertTo()
    const {
  switch (value_id_) {
    case CSSValueID::kNone:
      return scroll_customization::kScrollDirectionNone;
    case CSSValueID::kAuto:
      return scroll_customization::kScrollDirectionAuto;
    case CSSValueID::kPanLeft:
      return scroll_customization::kScrollDirectionPanLeft;
    case CSSValueID::kPanRight:
      return scroll_customization::kScrollDirectionPanRight;
    case CSSValueID::kPanX:
      return scroll_customization::kScrollDirectionPanX;
    case CSSValueID::kPanUp:
      return scroll_customization::kScrollDirectionPanUp;
    case CSSValueID::kPanDown:
      return scroll_customization::kScrollDirectionPanDown;
    case CSSValueID::kPanY:
      return scroll_customization::kScrollDirectionPanY;
    default:
      break;
  }

  NOTREACHED();
  return scroll_customization::kScrollDirectionNone;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(CSSBoxType css_box)
    : CSSValue(kIdentifierClass) {
  switch (css_box) {
    case CSSBoxType::kMargin:
      value_id_ = CSSValueID::kMarginBox;
      break;
    case CSSBoxType::kBorder:
      value_id_ = CSSValueID::kBorderBox;
      break;
    case CSSBoxType::kPadding:
      value_id_ = CSSValueID::kPaddingBox;
      break;
    case CSSBoxType::kContent:
      value_id_ = CSSValueID::kContentBox;
      break;
    case CSSBoxType::kMissing:
      // The missing box should convert to a null primitive value.
      NOTREACHED();
  }
}

template <>
inline CSSBoxType CSSIdentifierValue::ConvertTo() const {
  switch (GetValueID()) {
    case CSSValueID::kMarginBox:
      return CSSBoxType::kMargin;
    case CSSValueID::kBorderBox:
      return CSSBoxType::kBorder;
    case CSSValueID::kPaddingBox:
      return CSSBoxType::kPadding;
    case CSSValueID::kContentBox:
      return CSSBoxType::kContent;
    default:
      break;
  }
  NOTREACHED();
  return CSSBoxType::kContent;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ItemPosition item_position)
    : CSSValue(kIdentifierClass) {
  switch (item_position) {
    case ItemPosition::kLegacy:
      value_id_ = CSSValueID::kLegacy;
      break;
    case ItemPosition::kAuto:
      value_id_ = CSSValueID::kAuto;
      break;
    case ItemPosition::kNormal:
      value_id_ = CSSValueID::kNormal;
      break;
    case ItemPosition::kStretch:
      value_id_ = CSSValueID::kStretch;
      break;
    case ItemPosition::kBaseline:
      value_id_ = CSSValueID::kBaseline;
      break;
    case ItemPosition::kLastBaseline:
      value_id_ = CSSValueID::kLastBaseline;
      break;
    case ItemPosition::kCenter:
      value_id_ = CSSValueID::kCenter;
      break;
    case ItemPosition::kStart:
      value_id_ = CSSValueID::kStart;
      break;
    case ItemPosition::kEnd:
      value_id_ = CSSValueID::kEnd;
      break;
    case ItemPosition::kSelfStart:
      value_id_ = CSSValueID::kSelfStart;
      break;
    case ItemPosition::kSelfEnd:
      value_id_ = CSSValueID::kSelfEnd;
      break;
    case ItemPosition::kFlexStart:
      value_id_ = CSSValueID::kFlexStart;
      break;
    case ItemPosition::kFlexEnd:
      value_id_ = CSSValueID::kFlexEnd;
      break;
    case ItemPosition::kLeft:
      value_id_ = CSSValueID::kLeft;
      break;
    case ItemPosition::kRight:
      value_id_ = CSSValueID::kRight;
      break;
  }
}

template <>
inline ItemPosition CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kLegacy:
      return ItemPosition::kLegacy;
    case CSSValueID::kAuto:
      return ItemPosition::kAuto;
    case CSSValueID::kNormal:
      return ItemPosition::kNormal;
    case CSSValueID::kStretch:
      return ItemPosition::kStretch;
    case CSSValueID::kBaseline:
      return ItemPosition::kBaseline;
    case CSSValueID::kFirstBaseline:
      return ItemPosition::kBaseline;
    case CSSValueID::kLastBaseline:
      return ItemPosition::kLastBaseline;
    case CSSValueID::kCenter:
      return ItemPosition::kCenter;
    case CSSValueID::kStart:
      return ItemPosition::kStart;
    case CSSValueID::kEnd:
      return ItemPosition::kEnd;
    case CSSValueID::kSelfStart:
      return ItemPosition::kSelfStart;
    case CSSValueID::kSelfEnd:
      return ItemPosition::kSelfEnd;
    case CSSValueID::kFlexStart:
      return ItemPosition::kFlexStart;
    case CSSValueID::kFlexEnd:
      return ItemPosition::kFlexEnd;
    case CSSValueID::kLeft:
      return ItemPosition::kLeft;
    case CSSValueID::kRight:
      return ItemPosition::kRight;
    default:
      break;
  }
  NOTREACHED();
  return ItemPosition::kAuto;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ContentPosition content_position)
    : CSSValue(kIdentifierClass) {
  switch (content_position) {
    case ContentPosition::kNormal:
      value_id_ = CSSValueID::kNormal;
      break;
    case ContentPosition::kBaseline:
      value_id_ = CSSValueID::kBaseline;
      break;
    case ContentPosition::kLastBaseline:
      value_id_ = CSSValueID::kLastBaseline;
      break;
    case ContentPosition::kCenter:
      value_id_ = CSSValueID::kCenter;
      break;
    case ContentPosition::kStart:
      value_id_ = CSSValueID::kStart;
      break;
    case ContentPosition::kEnd:
      value_id_ = CSSValueID::kEnd;
      break;
    case ContentPosition::kFlexStart:
      value_id_ = CSSValueID::kFlexStart;
      break;
    case ContentPosition::kFlexEnd:
      value_id_ = CSSValueID::kFlexEnd;
      break;
    case ContentPosition::kLeft:
      value_id_ = CSSValueID::kLeft;
      break;
    case ContentPosition::kRight:
      value_id_ = CSSValueID::kRight;
      break;
  }
}

template <>
inline ContentPosition CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kNormal:
      return ContentPosition::kNormal;
    case CSSValueID::kBaseline:
      return ContentPosition::kBaseline;
    case CSSValueID::kFirstBaseline:
      return ContentPosition::kBaseline;
    case CSSValueID::kLastBaseline:
      return ContentPosition::kLastBaseline;
    case CSSValueID::kCenter:
      return ContentPosition::kCenter;
    case CSSValueID::kStart:
      return ContentPosition::kStart;
    case CSSValueID::kEnd:
      return ContentPosition::kEnd;
    case CSSValueID::kFlexStart:
      return ContentPosition::kFlexStart;
    case CSSValueID::kFlexEnd:
      return ContentPosition::kFlexEnd;
    case CSSValueID::kLeft:
      return ContentPosition::kLeft;
    case CSSValueID::kRight:
      return ContentPosition::kRight;
    default:
      break;
  }
  NOTREACHED();
  return ContentPosition::kNormal;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(
    ContentDistributionType content_distribution)
    : CSSValue(kIdentifierClass) {
  switch (content_distribution) {
    case ContentDistributionType::kDefault:
      value_id_ = CSSValueID::kDefault;
      break;
    case ContentDistributionType::kSpaceBetween:
      value_id_ = CSSValueID::kSpaceBetween;
      break;
    case ContentDistributionType::kSpaceAround:
      value_id_ = CSSValueID::kSpaceAround;
      break;
    case ContentDistributionType::kSpaceEvenly:
      value_id_ = CSSValueID::kSpaceEvenly;
      break;
    case ContentDistributionType::kStretch:
      value_id_ = CSSValueID::kStretch;
      break;
  }
}

template <>
inline ContentDistributionType CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kSpaceBetween:
      return ContentDistributionType::kSpaceBetween;
    case CSSValueID::kSpaceAround:
      return ContentDistributionType::kSpaceAround;
    case CSSValueID::kSpaceEvenly:
      return ContentDistributionType::kSpaceEvenly;
    case CSSValueID::kStretch:
      return ContentDistributionType::kStretch;
    default:
      break;
  }
  NOTREACHED();
  return ContentDistributionType::kStretch;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(
    OverflowAlignment overflow_alignment)
    : CSSValue(kIdentifierClass) {
  switch (overflow_alignment) {
    case OverflowAlignment::kDefault:
      value_id_ = CSSValueID::kDefault;
      break;
    case OverflowAlignment::kUnsafe:
      value_id_ = CSSValueID::kUnsafe;
      break;
    case OverflowAlignment::kSafe:
      value_id_ = CSSValueID::kSafe;
      break;
  }
}

template <>
inline OverflowAlignment CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueID::kUnsafe:
      return OverflowAlignment::kUnsafe;
    case CSSValueID::kSafe:
      return OverflowAlignment::kSafe;
    default:
      break;
  }
  NOTREACHED();
  return OverflowAlignment::kUnsafe;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(ScrollBehavior behavior)
    : CSSValue(kIdentifierClass) {
  switch (behavior) {
    case kScrollBehaviorAuto:
      value_id_ = CSSValueID::kAuto;
      break;
    case kScrollBehaviorSmooth:
      value_id_ = CSSValueID::kSmooth;
      break;
    case kScrollBehaviorInstant:
      // Behavior 'instant' is only allowed in ScrollOptions arguments passed to
      // CSSOM scroll APIs.
      NOTREACHED();
  }
}

template <>
inline ScrollBehavior CSSIdentifierValue::ConvertTo() const {
  switch (GetValueID()) {
    case CSSValueID::kAuto:
      return kScrollBehaviorAuto;
    case CSSValueID::kSmooth:
      return kScrollBehaviorSmooth;
    default:
      break;
  }
  NOTREACHED();
  return kScrollBehaviorAuto;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(cc::SnapAxis axis)
    : CSSValue(kIdentifierClass) {
  switch (axis) {
    case cc::SnapAxis::kX:
      value_id_ = CSSValueID::kX;
      break;
    case cc::SnapAxis::kY:
      value_id_ = CSSValueID::kY;
      break;
    case cc::SnapAxis::kBlock:
      value_id_ = CSSValueID::kBlock;
      break;
    case cc::SnapAxis::kInline:
      value_id_ = CSSValueID::kInline;
      break;
    case cc::SnapAxis::kBoth:
      value_id_ = CSSValueID::kBoth;
      break;
  }
}

template <>
inline cc::SnapAxis CSSIdentifierValue::ConvertTo() const {
  switch (GetValueID()) {
    case CSSValueID::kX:
      return cc::SnapAxis::kX;
    case CSSValueID::kY:
      return cc::SnapAxis::kY;
    case CSSValueID::kBlock:
      return cc::SnapAxis::kBlock;
    case CSSValueID::kInline:
      return cc::SnapAxis::kInline;
    case CSSValueID::kBoth:
      return cc::SnapAxis::kBoth;
    default:
      break;
  }
  NOTREACHED();
  return cc::SnapAxis::kBoth;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(cc::SnapStrictness strictness)
    : CSSValue(kIdentifierClass) {
  switch (strictness) {
    case cc::SnapStrictness::kProximity:
      value_id_ = CSSValueID::kProximity;
      break;
    case cc::SnapStrictness::kMandatory:
      value_id_ = CSSValueID::kMandatory;
      break;
  }
}

template <>
inline cc::SnapStrictness CSSIdentifierValue::ConvertTo() const {
  switch (GetValueID()) {
    case CSSValueID::kProximity:
      return cc::SnapStrictness::kProximity;
    case CSSValueID::kMandatory:
      return cc::SnapStrictness::kMandatory;
    default:
      break;
  }
  NOTREACHED();
  return cc::SnapStrictness::kProximity;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(cc::SnapAlignment alignment)
    : CSSValue(kIdentifierClass) {
  switch (alignment) {
    case cc::SnapAlignment::kNone:
      value_id_ = CSSValueID::kNone;
      break;
    case cc::SnapAlignment::kStart:
      value_id_ = CSSValueID::kStart;
      break;
    case cc::SnapAlignment::kEnd:
      value_id_ = CSSValueID::kEnd;
      break;
    case cc::SnapAlignment::kCenter:
      value_id_ = CSSValueID::kCenter;
      break;
  }
}

template <>
inline cc::SnapAlignment CSSIdentifierValue::ConvertTo() const {
  switch (GetValueID()) {
    case CSSValueID::kNone:
      return cc::SnapAlignment::kNone;
    case CSSValueID::kStart:
      return cc::SnapAlignment::kStart;
    case CSSValueID::kEnd:
      return cc::SnapAlignment::kEnd;
    case CSSValueID::kCenter:
      return cc::SnapAlignment::kCenter;
    default:
      break;
  }
  NOTREACHED();
  return cc::SnapAlignment::kNone;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(Containment snap_type)
    : CSSValue(kIdentifierClass) {
  switch (snap_type) {
    case kContainsNone:
      value_id_ = CSSValueID::kNone;
      break;
    case kContainsStrict:
      value_id_ = CSSValueID::kStrict;
      break;
    case kContainsContent:
      value_id_ = CSSValueID::kContent;
      break;
    case kContainsPaint:
      value_id_ = CSSValueID::kPaint;
      break;
    case kContainsStyle:
      value_id_ = CSSValueID::kStyle;
      break;
    case kContainsLayout:
      value_id_ = CSSValueID::kLayout;
      break;
    case kContainsSize:
      value_id_ = CSSValueID::kSize;
      break;
  }
}

template <>
inline Containment CSSIdentifierValue::ConvertTo() const {
  switch (GetValueID()) {
    case CSSValueID::kNone:
      return kContainsNone;
    case CSSValueID::kStrict:
      return kContainsStrict;
    case CSSValueID::kContent:
      return kContainsContent;
    case CSSValueID::kPaint:
      return kContainsPaint;
    case CSSValueID::kStyle:
      return kContainsStyle;
    case CSSValueID::kLayout:
      return kContainsLayout;
    case CSSValueID::kSize:
      return kContainsSize;
    default:
      break;
  }
  NOTREACHED();
  return kContainsNone;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(TextUnderlinePosition position)
    : CSSValue(kIdentifierClass) {
  switch (position) {
    case kTextUnderlinePositionAuto:
      value_id_ = CSSValueID::kAuto;
      break;
    case kTextUnderlinePositionUnder:
      value_id_ = CSSValueID::kUnder;
      break;
    case kTextUnderlinePositionLeft:
      value_id_ = CSSValueID::kLeft;
      break;
    case kTextUnderlinePositionRight:
      value_id_ = CSSValueID::kRight;
      break;
  }
}

template <>
inline TextUnderlinePosition CSSIdentifierValue::ConvertTo() const {
  switch (GetValueID()) {
    case CSSValueID::kAuto:
      return kTextUnderlinePositionAuto;
    case CSSValueID::kUnder:
      return kTextUnderlinePositionUnder;
    case CSSValueID::kLeft:
      return kTextUnderlinePositionLeft;
    case CSSValueID::kRight:
      return kTextUnderlinePositionRight;
    default:
      break;
  }
  NOTREACHED();
  return kTextUnderlinePositionAuto;
}

}  // namespace blink

#endif
