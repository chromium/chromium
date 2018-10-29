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

#include "third_party/blink/renderer/core/css/css_calculation_value.h"
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
inline short CSSPrimitiveValue::ConvertTo() const {
  DCHECK(IsNumber());
  return clampTo<short>(GetDoubleValue());
}

template <>
inline unsigned short CSSPrimitiveValue::ConvertTo() const {
  DCHECK(IsNumber());
  return clampTo<unsigned short>(GetDoubleValue());
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
      value_id_ = CSSValueAbove;
      break;
    case kReflectionBelow:
      value_id_ = CSSValueBelow;
      break;
    case kReflectionLeft:
      value_id_ = CSSValueLeft;
      break;
    case kReflectionRight:
      value_id_ = CSSValueRight;
  }
}

template <>
inline CSSReflectionDirection CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueAbove:
      return kReflectionAbove;
    case CSSValueBelow:
      return kReflectionBelow;
    case CSSValueLeft:
      return kReflectionLeft;
    case CSSValueRight:
      return kReflectionRight;
    default:
      break;
  }

  NOTREACHED();
  return kReflectionBelow;
}

template <>
inline EBorderStyle CSSIdentifierValue::ConvertTo() const {
  if (value_id_ == CSSValueAuto)  // Valid for CSS outline-style
    return EBorderStyle::kDotted;
  return detail::cssValueIDToPlatformEnumGenerated<EBorderStyle>(value_id_);
}

template <>
inline OutlineIsAuto CSSIdentifierValue::ConvertTo() const {
  if (value_id_ == CSSValueAuto)
    return OutlineIsAuto::kOn;
  return OutlineIsAuto::kOff;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(CompositeOperator e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case kCompositeClear:
      value_id_ = CSSValueClear;
      break;
    case kCompositeCopy:
      value_id_ = CSSValueCopy;
      break;
    case kCompositeSourceOver:
      value_id_ = CSSValueSourceOver;
      break;
    case kCompositeSourceIn:
      value_id_ = CSSValueSourceIn;
      break;
    case kCompositeSourceOut:
      value_id_ = CSSValueSourceOut;
      break;
    case kCompositeSourceAtop:
      value_id_ = CSSValueSourceAtop;
      break;
    case kCompositeDestinationOver:
      value_id_ = CSSValueDestinationOver;
      break;
    case kCompositeDestinationIn:
      value_id_ = CSSValueDestinationIn;
      break;
    case kCompositeDestinationOut:
      value_id_ = CSSValueDestinationOut;
      break;
    case kCompositeDestinationAtop:
      value_id_ = CSSValueDestinationAtop;
      break;
    case kCompositeXOR:
      value_id_ = CSSValueXor;
      break;
    case kCompositePlusLighter:
      value_id_ = CSSValuePlusLighter;
      break;
    default:
      NOTREACHED();
      break;
  }
}

template <>
inline CompositeOperator CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueClear:
      return kCompositeClear;
    case CSSValueCopy:
      return kCompositeCopy;
    case CSSValueSourceOver:
      return kCompositeSourceOver;
    case CSSValueSourceIn:
      return kCompositeSourceIn;
    case CSSValueSourceOut:
      return kCompositeSourceOut;
    case CSSValueSourceAtop:
      return kCompositeSourceAtop;
    case CSSValueDestinationOver:
      return kCompositeDestinationOver;
    case CSSValueDestinationIn:
      return kCompositeDestinationIn;
    case CSSValueDestinationOut:
      return kCompositeDestinationOut;
    case CSSValueDestinationAtop:
      return kCompositeDestinationAtop;
    case CSSValueXor:
      return kCompositeXOR;
    case CSSValuePlusLighter:
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
      value_id_ = CSSValueNone;
      break;
    case kCheckboxPart:
      value_id_ = CSSValueCheckbox;
      break;
    case kRadioPart:
      value_id_ = CSSValueRadio;
      break;
    case kPushButtonPart:
      value_id_ = CSSValuePushButton;
      break;
    case kSquareButtonPart:
      value_id_ = CSSValueSquareButton;
      break;
    case kButtonPart:
      value_id_ = CSSValueButton;
      break;
    case kButtonBevelPart:
      value_id_ = CSSValueButtonBevel;
      break;
    case kInnerSpinButtonPart:
      value_id_ = CSSValueInnerSpinButton;
      break;
    case kListboxPart:
      value_id_ = CSSValueListbox;
      break;
    case kListItemPart:
      value_id_ = CSSValueListitem;
      break;
    case kMediaEnterFullscreenButtonPart:
      value_id_ = CSSValueMediaEnterFullscreenButton;
      break;
    case kMediaExitFullscreenButtonPart:
      value_id_ = CSSValueMediaExitFullscreenButton;
      break;
    case kMediaPlayButtonPart:
      value_id_ = CSSValueMediaPlayButton;
      break;
    case kMediaOverlayPlayButtonPart:
      value_id_ = CSSValueMediaOverlayPlayButton;
      break;
    case kMediaMuteButtonPart:
      value_id_ = CSSValueMediaMuteButton;
      break;
    case kMediaToggleClosedCaptionsButtonPart:
      value_id_ = CSSValueMediaToggleClosedCaptionsButton;
      break;
    case kMediaCastOffButtonPart:
      value_id_ = CSSValueInternalMediaCastOffButton;
      break;
    case kMediaOverlayCastOffButtonPart:
      value_id_ = CSSValueInternalMediaOverlayCastOffButton;
      break;
    case kMediaSliderPart:
      value_id_ = CSSValueMediaSlider;
      break;
    case kMediaSliderThumbPart:
      value_id_ = CSSValueMediaSliderthumb;
      break;
    case kMediaVolumeSliderContainerPart:
      value_id_ = CSSValueMediaVolumeSliderContainer;
      break;
    case kMediaVolumeSliderPart:
      value_id_ = CSSValueMediaVolumeSlider;
      break;
    case kMediaVolumeSliderThumbPart:
      value_id_ = CSSValueMediaVolumeSliderthumb;
      break;
    case kMediaControlsBackgroundPart:
      value_id_ = CSSValueMediaControlsBackground;
      break;
    case kMediaControlsFullscreenBackgroundPart:
      value_id_ = CSSValueMediaControlsFullscreenBackground;
      break;
    case kMediaCurrentTimePart:
      value_id_ = CSSValueMediaCurrentTimeDisplay;
      break;
    case kMediaTimeRemainingPart:
      value_id_ = CSSValueMediaTimeRemainingDisplay;
      break;
    case kMediaTrackSelectionCheckmarkPart:
      value_id_ = CSSValueInternalMediaTrackSelectionCheckmark;
      break;
    case kMediaClosedCaptionsIconPart:
      value_id_ = CSSValueInternalMediaClosedCaptionsIcon;
      break;
    case kMediaSubtitlesIconPart:
      value_id_ = CSSValueInternalMediaSubtitlesIcon;
      break;
    case kMediaOverflowMenuButtonPart:
      value_id_ = CSSValueInternalMediaOverflowButton;
      break;
    case kMediaDownloadIconPart:
      value_id_ = CSSValueInternalMediaDownloadButton;
      break;
    case kMediaControlPart:
      value_id_ = CSSValueInternalMediaControl;
      break;
    case kMenulistPart:
      value_id_ = CSSValueMenulist;
      break;
    case kMenulistButtonPart:
      value_id_ = CSSValueMenulistButton;
      break;
    case kMenulistTextPart:
      value_id_ = CSSValueMenulistText;
      break;
    case kMenulistTextFieldPart:
      value_id_ = CSSValueMenulistTextfield;
      break;
    case kMeterPart:
      value_id_ = CSSValueMeter;
      break;
    case kProgressBarPart:
      value_id_ = CSSValueProgressBar;
      break;
    case kProgressBarValuePart:
      value_id_ = CSSValueProgressBarValue;
      break;
    case kSliderHorizontalPart:
      value_id_ = CSSValueSliderHorizontal;
      break;
    case kSliderVerticalPart:
      value_id_ = CSSValueSliderVertical;
      break;
    case kSliderThumbHorizontalPart:
      value_id_ = CSSValueSliderthumbHorizontal;
      break;
    case kSliderThumbVerticalPart:
      value_id_ = CSSValueSliderthumbVertical;
      break;
    case kCaretPart:
      value_id_ = CSSValueCaret;
      break;
    case kSearchFieldPart:
      value_id_ = CSSValueSearchfield;
      break;
    case kSearchFieldCancelButtonPart:
      value_id_ = CSSValueSearchfieldCancelButton;
      break;
    case kTextFieldPart:
      value_id_ = CSSValueTextfield;
      break;
    case kTextAreaPart:
      value_id_ = CSSValueTextarea;
      break;
    case kCapsLockIndicatorPart:
      value_id_ = CSSValueCapsLockIndicator;
      break;
    case kMediaRemotingCastIconPart:
      value_id_ = CSSValueInternalMediaRemotingCastIcon;
      break;
  }
}

template <>
inline ControlPart CSSIdentifierValue::ConvertTo() const {
  if (value_id_ == CSSValueNone)
    return kNoControlPart;
  return ControlPart(value_id_ - CSSValueCheckbox + 1);
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EFillAttachment e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case EFillAttachment::kScroll:
      value_id_ = CSSValueScroll;
      break;
    case EFillAttachment::kLocal:
      value_id_ = CSSValueLocal;
      break;
    case EFillAttachment::kFixed:
      value_id_ = CSSValueFixed;
      break;
  }
}

template <>
inline EFillAttachment CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueScroll:
      return EFillAttachment::kScroll;
    case CSSValueLocal:
      return EFillAttachment::kLocal;
    case CSSValueFixed:
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
      value_id_ = CSSValueBorderBox;
      break;
    case EFillBox::kPadding:
      value_id_ = CSSValuePaddingBox;
      break;
    case EFillBox::kContent:
      value_id_ = CSSValueContentBox;
      break;
    case EFillBox::kText:
      value_id_ = CSSValueText;
      break;
  }
}

template <>
inline EFillBox CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueBorder:
    case CSSValueBorderBox:
      return EFillBox::kBorder;
    case CSSValuePadding:
    case CSSValuePaddingBox:
      return EFillBox::kPadding;
    case CSSValueContent:
    case CSSValueContentBox:
      return EFillBox::kContent;
    case CSSValueText:
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
      value_id_ = CSSValueRepeat;
      break;
    case EFillRepeat::kNoRepeatFill:
      value_id_ = CSSValueNoRepeat;
      break;
    case EFillRepeat::kRoundFill:
      value_id_ = CSSValueRound;
      break;
    case EFillRepeat::kSpaceFill:
      value_id_ = CSSValueSpace;
      break;
  }
}

template <>
inline EFillRepeat CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueRepeat:
      return EFillRepeat::kRepeatFill;
    case CSSValueNoRepeat:
      return EFillRepeat::kNoRepeatFill;
    case CSSValueRound:
      return EFillRepeat::kRoundFill;
    case CSSValueSpace:
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
      value_id_ = CSSValueTop;
      break;
    case BackgroundEdgeOrigin::kRight:
      value_id_ = CSSValueRight;
      break;
    case BackgroundEdgeOrigin::kBottom:
      value_id_ = CSSValueBottom;
      break;
    case BackgroundEdgeOrigin::kLeft:
      value_id_ = CSSValueLeft;
      break;
  }
}

template <>
inline BackgroundEdgeOrigin CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueTop:
      return BackgroundEdgeOrigin::kTop;
    case CSSValueRight:
      return BackgroundEdgeOrigin::kRight;
    case CSSValueBottom:
      return BackgroundEdgeOrigin::kBottom;
    case CSSValueLeft:
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
      value_id_ = CSSValueNone;
      break;
    case EFloat::kLeft:
      value_id_ = CSSValueLeft;
      break;
    case EFloat::kRight:
      value_id_ = CSSValueRight;
      break;
  }
}

template <>
inline EFloat CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueLeft:
      return EFloat::kLeft;
    case CSSValueRight:
      return EFloat::kRight;
    case CSSValueNone:
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
      value_id_ = CSSValueStatic;
      break;
    case EPosition::kRelative:
      value_id_ = CSSValueRelative;
      break;
    case EPosition::kAbsolute:
      value_id_ = CSSValueAbsolute;
      break;
    case EPosition::kFixed:
      value_id_ = CSSValueFixed;
      break;
    case EPosition::kSticky:
      value_id_ = CSSValueSticky;
      break;
  }
}

template <>
inline EPosition CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueStatic:
      return EPosition::kStatic;
    case CSSValueRelative:
      return EPosition::kRelative;
    case CSSValueAbsolute:
      return EPosition::kAbsolute;
    case CSSValueFixed:
      return EPosition::kFixed;
    case CSSValueSticky:
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
      value_id_ = CSSValueAuto;
      break;
    case ETableLayout::kFixed:
      value_id_ = CSSValueFixed;
      break;
  }
}

template <>
inline ETableLayout CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueFixed:
      return ETableLayout::kFixed;
    case CSSValueAuto:
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
      value_id_ = CSSValueTop;
      break;
    case EVerticalAlign::kBottom:
      value_id_ = CSSValueBottom;
      break;
    case EVerticalAlign::kMiddle:
      value_id_ = CSSValueMiddle;
      break;
    case EVerticalAlign::kBaseline:
      value_id_ = CSSValueBaseline;
      break;
    case EVerticalAlign::kTextBottom:
      value_id_ = CSSValueTextBottom;
      break;
    case EVerticalAlign::kTextTop:
      value_id_ = CSSValueTextTop;
      break;
    case EVerticalAlign::kSub:
      value_id_ = CSSValueSub;
      break;
    case EVerticalAlign::kSuper:
      value_id_ = CSSValueSuper;
      break;
    case EVerticalAlign::kBaselineMiddle:
      value_id_ = CSSValueWebkitBaselineMiddle;
      break;
    case EVerticalAlign::kLength:
      value_id_ = CSSValueInvalid;
  }
}

template <>
inline EVerticalAlign CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueTop:
      return EVerticalAlign::kTop;
    case CSSValueBottom:
      return EVerticalAlign::kBottom;
    case CSSValueMiddle:
      return EVerticalAlign::kMiddle;
    case CSSValueBaseline:
      return EVerticalAlign::kBaseline;
    case CSSValueTextBottom:
      return EVerticalAlign::kTextBottom;
    case CSSValueTextTop:
      return EVerticalAlign::kTextTop;
    case CSSValueSub:
      return EVerticalAlign::kSub;
    case CSSValueSuper:
      return EVerticalAlign::kSuper;
    case CSSValueWebkitBaselineMiddle:
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
      value_id_ = CSSValueFilled;
      break;
    case TextEmphasisFill::kOpen:
      value_id_ = CSSValueOpen;
      break;
  }
}

template <>
inline TextEmphasisFill CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueFilled:
      return TextEmphasisFill::kFilled;
    case CSSValueOpen:
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
      value_id_ = CSSValueDot;
      break;
    case TextEmphasisMark::kCircle:
      value_id_ = CSSValueCircle;
      break;
    case TextEmphasisMark::kDoubleCircle:
      value_id_ = CSSValueDoubleCircle;
      break;
    case TextEmphasisMark::kTriangle:
      value_id_ = CSSValueTriangle;
      break;
    case TextEmphasisMark::kSesame:
      value_id_ = CSSValueSesame;
      break;
    case TextEmphasisMark::kNone:
    case TextEmphasisMark::kAuto:
    case TextEmphasisMark::kCustom:
      NOTREACHED();
      value_id_ = CSSValueNone;
      break;
  }
}

template <>
inline TextEmphasisMark CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueNone:
      return TextEmphasisMark::kNone;
    case CSSValueDot:
      return TextEmphasisMark::kDot;
    case CSSValueCircle:
      return TextEmphasisMark::kCircle;
    case CSSValueDoubleCircle:
      return TextEmphasisMark::kDoubleCircle;
    case CSSValueTriangle:
      return TextEmphasisMark::kTriangle;
    case CSSValueSesame:
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
      value_id_ = CSSValueAuto;
      return;
    case FontDescription::kNormalKerning:
      value_id_ = CSSValueNormal;
      return;
    case FontDescription::kNoneKerning:
      value_id_ = CSSValueNone;
      return;
  }

  NOTREACHED();
  value_id_ = CSSValueAuto;
}

template <>
inline FontDescription::Kerning CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueAuto:
      return FontDescription::kAutoKerning;
    case CSSValueNormal:
      return FontDescription::kNormalKerning;
    case CSSValueNone:
      return FontDescription::kNoneKerning;
    default:
      break;
  }

  NOTREACHED();
  return FontDescription::kAutoKerning;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(EFillSizeType fill_size)
    : CSSValue(kIdentifierClass) {
  switch (fill_size) {
    case EFillSizeType::kContain:
      value_id_ = CSSValueContain;
      break;
    case EFillSizeType::kCover:
      value_id_ = CSSValueCover;
      break;
    case EFillSizeType::kSizeNone:
      value_id_ = CSSValueNone;
      break;
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
      value_id_ = CSSValueAuto;
      return;
    case kNoSmoothing:
      value_id_ = CSSValueNone;
      return;
    case kAntialiased:
      value_id_ = CSSValueAntialiased;
      return;
    case kSubpixelAntialiased:
      value_id_ = CSSValueSubpixelAntialiased;
      return;
  }

  NOTREACHED();
  value_id_ = CSSValueAuto;
}

template <>
inline FontSmoothingMode CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueAuto:
      return kAutoSmoothing;
    case CSSValueNone:
      return kNoSmoothing;
    case CSSValueAntialiased:
      return kAntialiased;
    case CSSValueSubpixelAntialiased:
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
      value_id_ = CSSValueAuto;
      break;
    case kOptimizeSpeed:
      value_id_ = CSSValueOptimizespeed;
      break;
    case kOptimizeLegibility:
      value_id_ = CSSValueOptimizelegibility;
      break;
    case kGeometricPrecision:
      value_id_ = CSSValueGeometricprecision;
      break;
  }
}

template <>
inline TextRenderingMode CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueAuto:
      return kAutoTextRendering;
    case CSSValueOptimizespeed:
      return kOptimizeSpeed;
    case CSSValueOptimizelegibility:
      return kOptimizeLegibility;
    case CSSValueGeometricprecision:
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
    case CSSValueLogical:
      return EOrder::kLogical;
    case CSSValueVisual:
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
      value_id_ = CSSValueLogical;
      break;
    case EOrder::kVisual:
      value_id_ = CSSValueVisual;
      break;
  }
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(LineCap e)
    : CSSValue(kIdentifierClass) {
  switch (e) {
    case kButtCap:
      value_id_ = CSSValueButt;
      break;
    case kRoundCap:
      value_id_ = CSSValueRound;
      break;
    case kSquareCap:
      value_id_ = CSSValueSquare;
      break;
  }
}

template <>
inline LineCap CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueButt:
      return kButtCap;
    case CSSValueRound:
      return kRoundCap;
    case CSSValueSquare:
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
      value_id_ = CSSValueMiter;
      break;
    case kRoundJoin:
      value_id_ = CSSValueRound;
      break;
    case kBevelJoin:
      value_id_ = CSSValueBevel;
      break;
  }
}

template <>
inline LineJoin CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueMiter:
      return kMiterJoin;
    case CSSValueRound:
      return kRoundJoin;
    case CSSValueBevel:
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
      value_id_ = CSSValueNonzero;
      break;
    case RULE_EVENODD:
      value_id_ = CSSValueEvenodd;
      break;
  }
}

template <>
inline WindRule CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueNonzero:
      return RULE_NONZERO;
    case CSSValueEvenodd:
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
      value_id_ = CSSValueAuto;
      break;
    case AB_BASELINE:
      value_id_ = CSSValueBaseline;
      break;
    case AB_BEFORE_EDGE:
      value_id_ = CSSValueBeforeEdge;
      break;
    case AB_TEXT_BEFORE_EDGE:
      value_id_ = CSSValueTextBeforeEdge;
      break;
    case AB_MIDDLE:
      value_id_ = CSSValueMiddle;
      break;
    case AB_CENTRAL:
      value_id_ = CSSValueCentral;
      break;
    case AB_AFTER_EDGE:
      value_id_ = CSSValueAfterEdge;
      break;
    case AB_TEXT_AFTER_EDGE:
      value_id_ = CSSValueTextAfterEdge;
      break;
    case AB_IDEOGRAPHIC:
      value_id_ = CSSValueIdeographic;
      break;
    case AB_ALPHABETIC:
      value_id_ = CSSValueAlphabetic;
      break;
    case AB_HANGING:
      value_id_ = CSSValueHanging;
      break;
    case AB_MATHEMATICAL:
      value_id_ = CSSValueMathematical;
      break;
  }
}

template <>
inline EAlignmentBaseline CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueAuto:
      return AB_AUTO;
    case CSSValueBaseline:
      return AB_BASELINE;
    case CSSValueBeforeEdge:
      return AB_BEFORE_EDGE;
    case CSSValueTextBeforeEdge:
      return AB_TEXT_BEFORE_EDGE;
    case CSSValueMiddle:
      return AB_MIDDLE;
    case CSSValueCentral:
      return AB_CENTRAL;
    case CSSValueAfterEdge:
      return AB_AFTER_EDGE;
    case CSSValueTextAfterEdge:
      return AB_TEXT_AFTER_EDGE;
    case CSSValueIdeographic:
      return AB_IDEOGRAPHIC;
    case CSSValueAlphabetic:
      return AB_ALPHABETIC;
    case CSSValueHanging:
      return AB_HANGING;
    case CSSValueMathematical:
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
      value_id_ = CSSValueAuto;
      break;
    case BR_DYNAMIC:
      value_id_ = CSSValueDynamic;
      break;
    case BR_STATIC:
      value_id_ = CSSValueStatic;
      break;
  }
}

template <>
inline EBufferedRendering CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueAuto:
      return BR_AUTO;
    case CSSValueDynamic:
      return BR_DYNAMIC;
    case CSSValueStatic:
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
      value_id_ = CSSValueAuto;
      break;
    case CI_SRGB:
      value_id_ = CSSValueSRGB;
      break;
    case CI_LINEARRGB:
      value_id_ = CSSValueLinearrgb;
      break;
  }
}

template <>
inline EColorInterpolation CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueSRGB:
      return CI_SRGB;
    case CSSValueLinearrgb:
      return CI_LINEARRGB;
    case CSSValueAuto:
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
      value_id_ = CSSValueAuto;
      break;
    case CR_OPTIMIZESPEED:
      value_id_ = CSSValueOptimizespeed;
      break;
    case CR_OPTIMIZEQUALITY:
      value_id_ = CSSValueOptimizequality;
      break;
  }
}

template <>
inline EColorRendering CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueOptimizespeed:
      return CR_OPTIMIZESPEED;
    case CSSValueOptimizequality:
      return CR_OPTIMIZEQUALITY;
    case CSSValueAuto:
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
      value_id_ = CSSValueAuto;
      break;
    case DB_USE_SCRIPT:
      value_id_ = CSSValueUseScript;
      break;
    case DB_NO_CHANGE:
      value_id_ = CSSValueNoChange;
      break;
    case DB_RESET_SIZE:
      value_id_ = CSSValueResetSize;
      break;
    case DB_CENTRAL:
      value_id_ = CSSValueCentral;
      break;
    case DB_MIDDLE:
      value_id_ = CSSValueMiddle;
      break;
    case DB_TEXT_BEFORE_EDGE:
      value_id_ = CSSValueTextBeforeEdge;
      break;
    case DB_TEXT_AFTER_EDGE:
      value_id_ = CSSValueTextAfterEdge;
      break;
    case DB_IDEOGRAPHIC:
      value_id_ = CSSValueIdeographic;
      break;
    case DB_ALPHABETIC:
      value_id_ = CSSValueAlphabetic;
      break;
    case DB_HANGING:
      value_id_ = CSSValueHanging;
      break;
    case DB_MATHEMATICAL:
      value_id_ = CSSValueMathematical;
      break;
  }
}

template <>
inline EDominantBaseline CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueAuto:
      return DB_AUTO;
    case CSSValueUseScript:
      return DB_USE_SCRIPT;
    case CSSValueNoChange:
      return DB_NO_CHANGE;
    case CSSValueResetSize:
      return DB_RESET_SIZE;
    case CSSValueIdeographic:
      return DB_IDEOGRAPHIC;
    case CSSValueAlphabetic:
      return DB_ALPHABETIC;
    case CSSValueHanging:
      return DB_HANGING;
    case CSSValueMathematical:
      return DB_MATHEMATICAL;
    case CSSValueCentral:
      return DB_CENTRAL;
    case CSSValueMiddle:
      return DB_MIDDLE;
    case CSSValueTextAfterEdge:
      return DB_TEXT_AFTER_EDGE;
    case CSSValueTextBeforeEdge:
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
      value_id_ = CSSValueAuto;
      break;
    case SR_OPTIMIZESPEED:
      value_id_ = CSSValueOptimizespeed;
      break;
    case SR_CRISPEDGES:
      value_id_ = CSSValueCrispedges;
      break;
    case SR_GEOMETRICPRECISION:
      value_id_ = CSSValueGeometricprecision;
      break;
  }
}

template <>
inline EShapeRendering CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueAuto:
      return SR_AUTO;
    case CSSValueOptimizespeed:
      return SR_OPTIMIZESPEED;
    case CSSValueCrispedges:
      return SR_CRISPEDGES;
    case CSSValueGeometricprecision:
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
      value_id_ = CSSValueStart;
      break;
    case TA_MIDDLE:
      value_id_ = CSSValueMiddle;
      break;
    case TA_END:
      value_id_ = CSSValueEnd;
      break;
  }
}

template <>
inline ETextAnchor CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueStart:
      return TA_START;
    case CSSValueMiddle:
      return TA_MIDDLE;
    case CSSValueEnd:
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
      value_id_ = CSSValueNone;
      break;
    case VE_NON_SCALING_STROKE:
      value_id_ = CSSValueNonScalingStroke;
      break;
  }
}

template <>
inline EVectorEffect CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueNone:
      return VE_NONE;
    case CSSValueNonScalingStroke:
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
      value_id_ = CSSValueFill;
      break;
    case PT_STROKE:
      value_id_ = CSSValueStroke;
      break;
    case PT_MARKERS:
      value_id_ = CSSValueMarkers;
      break;
    default:
      NOTREACHED();
      value_id_ = CSSValueFill;
      break;
  }
}

template <>
inline EPaintOrderType CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueFill:
      return PT_FILL;
    case CSSValueStroke:
      return PT_STROKE;
    case CSSValueMarkers:
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
      value_id_ = CSSValueLuminance;
      break;
    case MT_ALPHA:
      value_id_ = CSSValueAlpha;
      break;
  }
}

template <>
inline EMaskType CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueLuminance:
      return MT_LUMINANCE;
    case CSSValueAlpha:
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
    case CSSValueNone:
      return TouchAction::kTouchActionNone;
    case CSSValueAuto:
      return TouchAction::kTouchActionAuto;
    case CSSValuePanLeft:
      return TouchAction::kTouchActionPanLeft;
    case CSSValuePanRight:
      return TouchAction::kTouchActionPanRight;
    case CSSValuePanX:
      return TouchAction::kTouchActionPanX;
    case CSSValuePanUp:
      return TouchAction::kTouchActionPanUp;
    case CSSValuePanDown:
      return TouchAction::kTouchActionPanDown;
    case CSSValuePanY:
      return TouchAction::kTouchActionPanY;
    case CSSValueManipulation:
      return TouchAction::kTouchActionManipulation;
    case CSSValuePinchZoom:
      return TouchAction::kTouchActionPinchZoom;
    default:
      break;
  }

  NOTREACHED();
  return TouchAction::kTouchActionNone;
}

template <>
inline ScrollCustomization::ScrollDirection CSSIdentifierValue::ConvertTo()
    const {
  switch (value_id_) {
    case CSSValueNone:
      return ScrollCustomization::kScrollDirectionNone;
    case CSSValueAuto:
      return ScrollCustomization::kScrollDirectionAuto;
    case CSSValuePanLeft:
      return ScrollCustomization::kScrollDirectionPanLeft;
    case CSSValuePanRight:
      return ScrollCustomization::kScrollDirectionPanRight;
    case CSSValuePanX:
      return ScrollCustomization::kScrollDirectionPanX;
    case CSSValuePanUp:
      return ScrollCustomization::kScrollDirectionPanUp;
    case CSSValuePanDown:
      return ScrollCustomization::kScrollDirectionPanDown;
    case CSSValuePanY:
      return ScrollCustomization::kScrollDirectionPanY;
    default:
      break;
  }

  NOTREACHED();
  return ScrollCustomization::kScrollDirectionNone;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(CSSBoxType css_box)
    : CSSValue(kIdentifierClass) {
  switch (css_box) {
    case CSSBoxType::kMargin:
      value_id_ = CSSValueMarginBox;
      break;
    case CSSBoxType::kBorder:
      value_id_ = CSSValueBorderBox;
      break;
    case CSSBoxType::kPadding:
      value_id_ = CSSValuePaddingBox;
      break;
    case CSSBoxType::kContent:
      value_id_ = CSSValueContentBox;
      break;
    case CSSBoxType::kMissing:
      // The missing box should convert to a null primitive value.
      NOTREACHED();
  }
}

template <>
inline CSSBoxType CSSIdentifierValue::ConvertTo() const {
  switch (GetValueID()) {
    case CSSValueMarginBox:
      return CSSBoxType::kMargin;
    case CSSValueBorderBox:
      return CSSBoxType::kBorder;
    case CSSValuePaddingBox:
      return CSSBoxType::kPadding;
    case CSSValueContentBox:
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
      value_id_ = CSSValueLegacy;
      break;
    case ItemPosition::kAuto:
      value_id_ = CSSValueAuto;
      break;
    case ItemPosition::kNormal:
      value_id_ = CSSValueNormal;
      break;
    case ItemPosition::kStretch:
      value_id_ = CSSValueStretch;
      break;
    case ItemPosition::kBaseline:
      value_id_ = CSSValueBaseline;
      break;
    case ItemPosition::kLastBaseline:
      value_id_ = CSSValueLastBaseline;
      break;
    case ItemPosition::kCenter:
      value_id_ = CSSValueCenter;
      break;
    case ItemPosition::kStart:
      value_id_ = CSSValueStart;
      break;
    case ItemPosition::kEnd:
      value_id_ = CSSValueEnd;
      break;
    case ItemPosition::kSelfStart:
      value_id_ = CSSValueSelfStart;
      break;
    case ItemPosition::kSelfEnd:
      value_id_ = CSSValueSelfEnd;
      break;
    case ItemPosition::kFlexStart:
      value_id_ = CSSValueFlexStart;
      break;
    case ItemPosition::kFlexEnd:
      value_id_ = CSSValueFlexEnd;
      break;
    case ItemPosition::kLeft:
      value_id_ = CSSValueLeft;
      break;
    case ItemPosition::kRight:
      value_id_ = CSSValueRight;
      break;
  }
}

template <>
inline ItemPosition CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueLegacy:
      return ItemPosition::kLegacy;
    case CSSValueAuto:
      return ItemPosition::kAuto;
    case CSSValueNormal:
      return ItemPosition::kNormal;
    case CSSValueStretch:
      return ItemPosition::kStretch;
    case CSSValueBaseline:
      return ItemPosition::kBaseline;
    case CSSValueFirstBaseline:
      return ItemPosition::kBaseline;
    case CSSValueLastBaseline:
      return ItemPosition::kLastBaseline;
    case CSSValueCenter:
      return ItemPosition::kCenter;
    case CSSValueStart:
      return ItemPosition::kStart;
    case CSSValueEnd:
      return ItemPosition::kEnd;
    case CSSValueSelfStart:
      return ItemPosition::kSelfStart;
    case CSSValueSelfEnd:
      return ItemPosition::kSelfEnd;
    case CSSValueFlexStart:
      return ItemPosition::kFlexStart;
    case CSSValueFlexEnd:
      return ItemPosition::kFlexEnd;
    case CSSValueLeft:
      return ItemPosition::kLeft;
    case CSSValueRight:
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
      value_id_ = CSSValueNormal;
      break;
    case ContentPosition::kBaseline:
      value_id_ = CSSValueBaseline;
      break;
    case ContentPosition::kLastBaseline:
      value_id_ = CSSValueLastBaseline;
      break;
    case ContentPosition::kCenter:
      value_id_ = CSSValueCenter;
      break;
    case ContentPosition::kStart:
      value_id_ = CSSValueStart;
      break;
    case ContentPosition::kEnd:
      value_id_ = CSSValueEnd;
      break;
    case ContentPosition::kFlexStart:
      value_id_ = CSSValueFlexStart;
      break;
    case ContentPosition::kFlexEnd:
      value_id_ = CSSValueFlexEnd;
      break;
    case ContentPosition::kLeft:
      value_id_ = CSSValueLeft;
      break;
    case ContentPosition::kRight:
      value_id_ = CSSValueRight;
      break;
  }
}

template <>
inline ContentPosition CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueNormal:
      return ContentPosition::kNormal;
    case CSSValueBaseline:
      return ContentPosition::kBaseline;
    case CSSValueFirstBaseline:
      return ContentPosition::kBaseline;
    case CSSValueLastBaseline:
      return ContentPosition::kLastBaseline;
    case CSSValueCenter:
      return ContentPosition::kCenter;
    case CSSValueStart:
      return ContentPosition::kStart;
    case CSSValueEnd:
      return ContentPosition::kEnd;
    case CSSValueFlexStart:
      return ContentPosition::kFlexStart;
    case CSSValueFlexEnd:
      return ContentPosition::kFlexEnd;
    case CSSValueLeft:
      return ContentPosition::kLeft;
    case CSSValueRight:
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
      value_id_ = CSSValueDefault;
      break;
    case ContentDistributionType::kSpaceBetween:
      value_id_ = CSSValueSpaceBetween;
      break;
    case ContentDistributionType::kSpaceAround:
      value_id_ = CSSValueSpaceAround;
      break;
    case ContentDistributionType::kSpaceEvenly:
      value_id_ = CSSValueSpaceEvenly;
      break;
    case ContentDistributionType::kStretch:
      value_id_ = CSSValueStretch;
      break;
  }
}

template <>
inline ContentDistributionType CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueSpaceBetween:
      return ContentDistributionType::kSpaceBetween;
    case CSSValueSpaceAround:
      return ContentDistributionType::kSpaceAround;
    case CSSValueSpaceEvenly:
      return ContentDistributionType::kSpaceEvenly;
    case CSSValueStretch:
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
      value_id_ = CSSValueDefault;
      break;
    case OverflowAlignment::kUnsafe:
      value_id_ = CSSValueUnsafe;
      break;
    case OverflowAlignment::kSafe:
      value_id_ = CSSValueSafe;
      break;
  }
}

template <>
inline OverflowAlignment CSSIdentifierValue::ConvertTo() const {
  switch (value_id_) {
    case CSSValueUnsafe:
      return OverflowAlignment::kUnsafe;
    case CSSValueSafe:
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
      value_id_ = CSSValueAuto;
      break;
    case kScrollBehaviorSmooth:
      value_id_ = CSSValueSmooth;
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
    case CSSValueAuto:
      return kScrollBehaviorAuto;
    case CSSValueSmooth:
      return kScrollBehaviorSmooth;
    default:
      break;
  }
  NOTREACHED();
  return kScrollBehaviorAuto;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(SnapAxis axis)
    : CSSValue(kIdentifierClass) {
  switch (axis) {
    case SnapAxis::kX:
      value_id_ = CSSValueX;
      break;
    case SnapAxis::kY:
      value_id_ = CSSValueY;
      break;
    case SnapAxis::kBlock:
      value_id_ = CSSValueBlock;
      break;
    case SnapAxis::kInline:
      value_id_ = CSSValueInline;
      break;
    case SnapAxis::kBoth:
      value_id_ = CSSValueBoth;
      break;
  }
}

template <>
inline SnapAxis CSSIdentifierValue::ConvertTo() const {
  switch (GetValueID()) {
    case CSSValueX:
      return SnapAxis::kX;
    case CSSValueY:
      return SnapAxis::kY;
    case CSSValueBlock:
      return SnapAxis::kBlock;
    case CSSValueInline:
      return SnapAxis::kInline;
    case CSSValueBoth:
      return SnapAxis::kBoth;
    default:
      break;
  }
  NOTREACHED();
  return SnapAxis::kBoth;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(SnapStrictness strictness)
    : CSSValue(kIdentifierClass) {
  switch (strictness) {
    case SnapStrictness::kProximity:
      value_id_ = CSSValueProximity;
      break;
    case SnapStrictness::kMandatory:
      value_id_ = CSSValueMandatory;
      break;
  }
}

template <>
inline SnapStrictness CSSIdentifierValue::ConvertTo() const {
  switch (GetValueID()) {
    case CSSValueProximity:
      return SnapStrictness::kProximity;
    case CSSValueMandatory:
      return SnapStrictness::kMandatory;
    default:
      break;
  }
  NOTREACHED();
  return SnapStrictness::kProximity;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(SnapAlignment alignment)
    : CSSValue(kIdentifierClass) {
  switch (alignment) {
    case SnapAlignment::kNone:
      value_id_ = CSSValueNone;
      break;
    case SnapAlignment::kStart:
      value_id_ = CSSValueStart;
      break;
    case SnapAlignment::kEnd:
      value_id_ = CSSValueEnd;
      break;
    case SnapAlignment::kCenter:
      value_id_ = CSSValueCenter;
      break;
  }
}

template <>
inline SnapAlignment CSSIdentifierValue::ConvertTo() const {
  switch (GetValueID()) {
    case CSSValueNone:
      return SnapAlignment::kNone;
    case CSSValueStart:
      return SnapAlignment::kStart;
    case CSSValueEnd:
      return SnapAlignment::kEnd;
    case CSSValueCenter:
      return SnapAlignment::kCenter;
    default:
      break;
  }
  NOTREACHED();
  return SnapAlignment::kNone;
}

template <>
inline CSSIdentifierValue::CSSIdentifierValue(Containment snap_type)
    : CSSValue(kIdentifierClass) {
  switch (snap_type) {
    case kContainsNone:
      value_id_ = CSSValueNone;
      break;
    case kContainsStrict:
      value_id_ = CSSValueStrict;
      break;
    case kContainsContent:
      value_id_ = CSSValueContent;
      break;
    case kContainsPaint:
      value_id_ = CSSValuePaint;
      break;
    case kContainsStyle:
      value_id_ = CSSValueStyle;
      break;
    case kContainsLayout:
      value_id_ = CSSValueLayout;
      break;
    case kContainsSize:
      value_id_ = CSSValueSize;
      break;
  }
}

template <>
inline Containment CSSIdentifierValue::ConvertTo() const {
  switch (GetValueID()) {
    case CSSValueNone:
      return kContainsNone;
    case CSSValueStrict:
      return kContainsStrict;
    case CSSValueContent:
      return kContainsContent;
    case CSSValuePaint:
      return kContainsPaint;
    case CSSValueStyle:
      return kContainsStyle;
    case CSSValueLayout:
      return kContainsLayout;
    case CSSValueSize:
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
      value_id_ = CSSValueAuto;
      break;
    case kTextUnderlinePositionUnder:
      value_id_ = CSSValueUnder;
      break;
    case kTextUnderlinePositionLeft:
      value_id_ = CSSValueLeft;
      break;
    case kTextUnderlinePositionRight:
      value_id_ = CSSValueRight;
      break;
  }
}

template <>
inline TextUnderlinePosition CSSIdentifierValue::ConvertTo() const {
  switch (GetValueID()) {
    case CSSValueAuto:
      return kTextUnderlinePositionAuto;
    case CSSValueUnder:
      return kTextUnderlinePositionUnder;
    case CSSValueLeft:
      return kTextUnderlinePositionLeft;
    case CSSValueRight:
      return kTextUnderlinePositionRight;
    default:
      break;
  }
  NOTREACHED();
  return kTextUnderlinePositionAuto;
}

}  // namespace blink

#endif
