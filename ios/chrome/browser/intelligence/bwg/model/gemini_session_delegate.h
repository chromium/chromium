// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SESSION_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SESSION_DELEGATE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"

// TODO(crbug.com/481711842): Replace this enum and its gemini_metrics.h
// equivalent with an enum in gemini_constants.h
// The feedback type for Gemini queries.
// LINT.IfChange(GeminiFeedbackType)
enum class GeminiFeedbackType {
  // Thumbs up feedback type.
  kThumbsUp,
  // Thumbs down feedback type.
  kThumbsDown,
};
// LINT.ThenChange(
//   /ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h:IOSGeminiFeedback,
//   /tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiFeedback
// )

// TODO(crbug.com/481711842): Replace this enum and its gemini_metrics.h
// equivalent with an enum in gemini_constants.h
// Cancellation types for a Gemini session.
typedef NS_ENUM(NSInteger, GeminiCancelType) {
  // Unknown cancellation reason.
  GeminiCancelTypeUnknown = 0,
  // Stop button was tapped.
  GeminiCancelTypeStopButtonTapped = 1,
  // User tapped outside of floaty.
  GeminiCancelTypeOutsideTapped = 2,
  // Close button was tapped in the expanded state.
  GeminiCancelTypeExpandedStateCloseButtonTapped = 3,
  // Close button was tapped in the collapsed state.
  GeminiCancelTypeCollapsedStateCloseButtonTapped = 4,
  // Close button was tapped in the loading state.
  GeminiCancelTypeLoadingStateCloseButtonTapped = 5,
};

// TODO(crbug.com/481711842): Do not add any more enums here if they are only
// used for metrics.

// Delegate for Gemini session events. Keep up to date with GCR's
// SessionDelegate.
@protocol GeminiSessionDelegate

// Called when a new session is created.
- (void)newSessionCreatedWithClientID:(NSString*)clientID
                             serverID:(NSString*)serverID;

// Called when the UI is shown.
- (void)UIDidAppearWithClientID:(NSString*)clientID
                       serverID:(NSString*)serverID;

// Called when the UI is hidden.
- (void)UIDidDisappearWithClientID:(NSString*)clientID
                          serverID:(NSString*)serverID;

// Called when floaty starts receiving response for a query.
- (void)startReceivingResponseWithSessionID:(NSString*)sessionID
                             conversationID:(NSString*)conversationID;

// Called when a response is received from Gemini, including metadata about the
// response.
- (void)responseReceivedWithClientID:(NSString*)clientID
                            serverID:(NSString*)serverID
            isNanoBananaToolSelected:(BOOL)isNanoBananaToolSelected
                    isImageGenerated:(BOOL)isImageGenerated;

// Called when the user taps the Gemini settings button from within the Gemini
// UI.
- (void)didTapGeminiSettingsButton;

// Called when a query is sent to Gemini, including metadata about the query.
- (void)didSendQueryWithInputType:(gemini::InputType)inputType
         isNanoBananaToolSelected:(BOOL)isNanoBananaToolSelected
              imagesAttachedCount:(NSUInteger)imagesAttachedCount
                   longPressImage:(BOOL)longPressImage
              pageContextAttached:(BOOL)pageContextAttached;

// Called when a new chat button is tapped.
// TODO(crbug.com/436019705) Rename this to `clientID` and `serverID`.
- (void)didTapNewChatButtonWithSessionID:(NSString*)sessionID
                          conversationID:(NSString*)conversationID;

// Called when a feedback button is tapped in the Gemini UI.
- (void)didTapFeedbackButton:(GeminiFeedbackType)feedbackType
                   sessionID:(NSString*)sessionID
              conversationID:(NSString*)conversationID;

// Called when the Gemini view state changes.
- (void)didSwitchToViewState:(ios::provider::GeminiViewState)viewState
                   sessionID:(NSString*)sessionID
              conversationID:(NSString*)conversationID;
@optional
// Called when the processing status changes.
- (void)didUpdateProcessingStatus:(ios::provider::GeminiClientMode)processStatus
                        sessionID:(NSString*)sessionID
                   conversationID:(NSString*)conversationID;

// TODO(crbug.com/504596190): Remove this method when internal code doesn't use
// anymore.
// Called when the Live processing status changes.
- (void)didChangeLiveProcessStatus:
            (ios::provider::GeminiClientMode)processStatus
                         sessionID:(NSString*)sessionID
                    conversationID:(NSString*)conversationID;

// Called when gemini response is cancelled.
- (void)responseCancelledWithReason:(GeminiCancelType)reason
                          sessionID:(NSString*)sessionID
                     conversationID:(NSString*)conversationID;

// Called when the user taps on an attachment option e.g. camera, gallery,
// CreateImageSelected, CreateImageDeselected  in the input plate.
- (void)didTapInputPlateAttachmentOption:
            (gemini::InputPlateAttachmentOption)attachmentOption
                               sessionID:(NSString*)sessionID
                          conversationID:(NSString*)conversationID;

// Called when the user taps on an image action button e.g.  share, copy,
// download image.
- (void)imageActionButtonTapped:(gemini::ImageActionButtonType)actionButtonType
                      sessionID:(NSString*)sessionID
                 conversationID:(NSString*)conversationID;

// Called when the user retries the last request, optionally with a regenerate
// option.
- (void)didRetryLastRequestWithRegenerateOptionType:
            (gemini::RegenerateOptionType)optionType
                                          sessionID:(NSString*)sessionID
                                     conversationID:(NSString*)conversationID;

// Called when the user barges in during Gemini Live session.
- (void)geminiLiveUserDidBargeIn;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SESSION_DELEGATE_H_
