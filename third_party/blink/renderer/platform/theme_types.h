/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_TYPES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_TYPES_H_

namespace blink {

enum ControlState {
  kHoverControlState = 1,
  kPressedControlState = 1 << 1,
  kFocusControlState = 1 << 2,
  kEnabledControlState = 1 << 3,
  kCheckedControlState = 1 << 4,
  kReadOnlyControlState = 1 << 5,
  kWindowInactiveControlState = 1 << 7,
  kIndeterminateControlState = 1 << 8,
  kSpinUpControlState =
      1 << 9,  // Sub-state for HoverControlState and PressedControlState.
  kAllControlStates = 0xffffffff
};

typedef unsigned ControlStates;

// Must follow css_value_keywords.json5 order
enum ControlPart {
  kNoControlPart,
  kCheckboxPart,
  kRadioPart,
  kPushButtonPart,
  kSquareButtonPart,
  kButtonPart,
  kInnerSpinButtonPart,
  kListboxPart,
  kMediaSliderPart,
  kMediaSliderThumbPart,
  kMediaVolumeSliderPart,
  kMediaVolumeSliderThumbPart,
  kMediaControlPart,
  kMenulistPart,
  kMenulistButtonPart,
  kMeterPart,
  kProgressBarPart,
  kSliderHorizontalPart,
  kSliderVerticalPart,
  kSliderThumbHorizontalPart,
  kSliderThumbVerticalPart,
  kSearchFieldPart,
  kSearchFieldCancelButtonPart,
  kTextFieldPart,
  kTextAreaPart,
};

enum SelectionPart { kSelectionBackground, kSelectionForeground };

enum ThemeFont {
  kCaptionFont,
  kIconFont,
  kMenuFont,
  kMessageBoxFont,
  kSmallCaptionFont,
  kStatusBarFont,
  kMiniControlFont,
  kSmallControlFont,
  kControlFont
};

enum ThemeColor {
  kActiveBorderColor,
  kActiveCaptionColor,
  kAppWorkspaceColor,
  kBackgroundColor,
  kButtonFaceColor,
  kButtonHighlightColor,
  kButtonShadowColor,
  kButtonTextColor,
  kCaptionTextColor,
  kGrayTextColor,
  kHighlightColor,
  kHighlightTextColor,
  kInactiveBorderColor,
  kInactiveCaptionColor,
  kInactiveCaptionTextColor,
  kInfoBackgroundColor,
  kInfoTextColor,
  kMatchColor,
  kMenuTextColor,
  kScrollbarColor,
  kThreeDDarkDhasowColor,
  kThreeDFaceColor,
  kThreeDHighlightColor,
  kThreeDLightShadowColor,
  kThreeDShadowCLor,
  kWindowColor,
  kWindowFrameColor,
  kWindowTextColor,
  kFocusRingColor,
  kActiveListBoxSelection,
  kActiveListBoxSelectionText,
  kInactiveListBoxSelection,
  kInactiveListBoxSelectionText
};

}  // namespace blink
#endif
