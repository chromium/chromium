/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_LOCALIZED_STRING_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_LOCALIZED_STRING_H_

namespace blink {

struct WebLocalizedString {
  enum Name {
    kAXAMPMFieldText,
    kAXCalendarShowDatePicker,
    kAXCalendarShowMonthSelector,
    kAXCalendarShowNextMonth,
    kAXCalendarShowPreviousMonth,
    kAXCalendarWeekDescription,
    kAXDayOfMonthFieldText,
    kAXHourFieldText,
    kAXMediaAudioElement,
    kAXMediaAudioElementHelp,
    kAXMediaAudioSliderHelp,
    kAXMediaCastOffButton,
    kAXMediaCastOnButton,
    kAXMediaCurrentTimeDisplay,
    kAXMediaCurrentTimeDisplayHelp,
    kAXMediaDefault,
    kAXMediaDownloadButton,
    kAXMediaEnterFullscreenButton,
    kAXMediaExitFullscreenButton,
    kAXMediaHideClosedCaptionsButton,
    kAXMediaMuteButton,
    kAXMediaDisplayCutoutFullscreenButton,
    kAXMediaOverflowButton,
    kAXMediaOverflowButtonHelp,
    kAXMediaPauseButton,
    kAXMediaPlayButton,
    kAXMediaShowClosedCaptionsButton,
    kAXMediaTimeRemainingDisplay,
    kAXMediaTimeRemainingDisplayHelp,
    kAXMediaUnMuteButton,
    kAXMediaVideoElement,
    kAXMediaVideoElementHelp,
    kAXMediaVideoSliderHelp,
    kAXMediaVolumeSliderHelp,
    kAXMediaEnterPictureInPictureButton,
    kAXMediaExitPictureInPictureButton,
    kAXMillisecondFieldText,
    kAXMinuteFieldText,
    kAXMonthFieldText,
    kAXSecondFieldText,
    kAXWeekOfYearFieldText,
    kAXYearFieldText,
    kBlockedPluginText,
    kCalendarClear,
    kCalendarToday,
    kDetailsLabel,
    kFileButtonChooseFileLabel,
    kFileButtonChooseMultipleFilesLabel,
    kFileButtonNoFileSelectedLabel,
    kInputElementAltText,
    kMediaRemotingCastText,
    kMediaRemotingCastToUnknownDeviceText,
    kMediaRemotingStopByErrorText,
    kMediaRemotingStopByPlaybackQualityText,
    kMediaRemotingStopNoText,
    kMediaRemotingStopText,
    kMediaScrubbingMessageText,
    kMissingPluginText,
    kMultipleFileUploadText,
    kOtherColorLabel,
    kOtherDateLabel,
    kOtherMonthLabel,
    kOtherWeekLabel,
    kOverflowMenuCaptions,
    kOverflowMenuCaptionsSubmenuTitle,
    kOverflowMenuCast,
    kOverflowMenuEnterFullscreen,
    kOverflowMenuExitFullscreen,
    kOverflowMenuMute,
    kOverflowMenuUnmute,
    kOverflowMenuPlay,
    kOverflowMenuPause,
    kOverflowMenuDownload,
    kOverflowMenuEnterPictureInPicture,
    kOverflowMenuExitPictureInPicture,
    kPictureInPictureInterstitialText,
    // kPlaceholderForDayOfMonthField is for day placeholder text, e.g.
    // "dd", for date field used in multiple fields "date", "datetime", and
    // "datetime-local" input UI instead of "--".
    kPlaceholderForDayOfMonthField,
    // kPlaceholderForfMonthField is for month placeholder text, e.g.
    // "mm", for month field used in multiple fields "date", "datetime", and
    // "datetime-local" input UI instead of "--".
    kPlaceholderForMonthField,
    // kPlaceholderForYearField is for year placeholder text, e.g. "yyyy",
    // for year field used in multiple fields "date", "datetime", and
    // "datetime-local" input UI instead of "----".
    kPlaceholderForYearField,
    kResetButtonDefaultLabel,
    kSelectMenuListText,
    kSubmitButtonDefaultLabel,
    kTextTracksNoLabel,
    kTextTracksOff,
    kThisMonthButtonLabel,
    kThisWeekButtonLabel,
    kUnitsKibibytes,
    kUnitsMebibytes,
    kUnitsGibibytes,
    kUnitsTebibytes,
    kUnitsPebibytes,
    kValidationBadInputForNumber,
    kValidationBadInputForDateTime,
    kValidationPatternMismatch,
    kValidationRangeOverflow,
    kValidationRangeOverflowDateTime,
    kValidationRangeUnderflow,
    kValidationRangeUnderflowDateTime,
    kValidationStepMismatch,
    kValidationStepMismatchCloseToLimit,
    kValidationTooLong,
    kValidationTooShort,
    kValidationTooShortPlural,
    kValidationTypeMismatch,
    kValidationTypeMismatchForEmail,
    kValidationTypeMismatchForEmailEmpty,
    kValidationTypeMismatchForEmailEmptyDomain,
    kValidationTypeMismatchForEmailEmptyLocal,
    kValidationTypeMismatchForEmailInvalidDomain,
    kValidationTypeMismatchForEmailInvalidDots,
    kValidationTypeMismatchForEmailInvalidLocal,
    kValidationTypeMismatchForEmailNoAtSign,
    kValidationTypeMismatchForMultipleEmail,
    kValidationTypeMismatchForURL,
    kValidationValueMissing,
    kValidationValueMissingForCheckbox,
    kValidationValueMissingForFile,
    kValidationValueMissingForMultipleFile,
    kValidationValueMissingForRadio,
    kValidationValueMissingForSelect,
    kWeekFormatTemplate,
    kWeekNumberLabel,
  };
};

}  // namespace blink

#endif
