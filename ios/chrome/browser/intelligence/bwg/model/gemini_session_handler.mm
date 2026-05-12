// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_session_handler.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_session_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"

namespace {

// Returns an equivalent IOSGeminiFirstPromptSubmission enum value for a given
// gemini::InputType.
IOSGeminiFirstPromptSubmissionMethod ConvertInputTypeToHistogramEnum(
    gemini::InputType input_type) {
  switch (input_type) {
    case gemini::InputType::kUnknown:
      return IOSGeminiFirstPromptSubmissionMethod::kUnknown;
    case gemini::InputType::kText:
      return IOSGeminiFirstPromptSubmissionMethod::kText;
    case gemini::InputType::kSummarize:
      return IOSGeminiFirstPromptSubmissionMethod::kSummarize;
    case gemini::InputType::kCheckThisSite:
      return IOSGeminiFirstPromptSubmissionMethod::kCheckThisSite;
    case gemini::InputType::kFindRelatedSites:
      return IOSGeminiFirstPromptSubmissionMethod::kFindRelatedSites;
    case gemini::InputType::kAskAboutPage:
      return IOSGeminiFirstPromptSubmissionMethod::kAskAboutPage;
    case gemini::InputType::kCreateFaq:
      return IOSGeminiFirstPromptSubmissionMethod::kCreateFaq;
    case gemini::InputType::kZeroStateModelSuggestion:
      return IOSGeminiFirstPromptSubmissionMethod::kZeroStateSuggestions;
    case gemini::InputType::kWhatCanGeminiDo:
      return IOSGeminiFirstPromptSubmissionMethod::kWhatCanGeminiDo;
    case gemini::InputType::kDiscoveryCard:
      return IOSGeminiFirstPromptSubmissionMethod::kDiscoveryCard;
    case gemini::InputType::kOmniboxSummarize:
      return IOSGeminiFirstPromptSubmissionMethod::kOmniboxSummarize;
    case gemini::InputType::kOmniboxPrompt:
      return IOSGeminiFirstPromptSubmissionMethod::kOmniboxPrompt;
    case gemini::InputType::kTransitionToLive:
      return IOSGeminiFirstPromptSubmissionMethod::kTransitionToLive;
    case gemini::InputType::kOnboardingWhatCanGeminiDo:
      return IOSGeminiFirstPromptSubmissionMethod::kOnboardingWhatCanGeminiDo;
    case gemini::InputType::kOnboardingAskAboutPage:
      return IOSGeminiFirstPromptSubmissionMethod::kOnboardingAskAboutPage;
    case gemini::InputType::kOnboardingSummarize:
      return IOSGeminiFirstPromptSubmissionMethod::kOnboardingSummarize;
    case gemini::InputType::kOnboardingNoIAmDone:
      return IOSGeminiFirstPromptSubmissionMethod::kOnboardingNoIAmDone;
    case gemini::InputType::kOnboardingKeepLearning:
      return IOSGeminiFirstPromptSubmissionMethod::kOnboardingKeepLearning;
    case gemini::InputType::kSuggestedReply:
      return IOSGeminiFirstPromptSubmissionMethod::kSuggestedReply;
    case gemini::InputType::kNanoBananaTurnThisPageIntoAComicStrip:
      return IOSGeminiFirstPromptSubmissionMethod::
          kNanoBananaTurnThisPageIntoAComicStrip;
    case gemini::InputType::kNanoBananaMakeAFolkArtIllustration:
      return IOSGeminiFirstPromptSubmissionMethod::
          kNanoBananaMakeAFolkArtIllustration;
    case gemini::InputType::kNanoBananaMakeACustomMiniFigure:
      return IOSGeminiFirstPromptSubmissionMethod::
          kNanoBananaMakeACustomMiniFigure;
    case gemini::InputType::kNanoBananaGiveMeAGrungeMakeover:
      return IOSGeminiFirstPromptSubmissionMethod::
          kNanoBananaGiveMeAGrungeMakeover;
    case gemini::InputType::kNanoBananaTurnThisImageIntoAVintagePostcard:
      return IOSGeminiFirstPromptSubmissionMethod::
          kNanoBananaTurnThisImageIntoAVintagePostcard;
    case gemini::InputType::kNanoBananaTurnThisImageIntoAWatercolorPainting:
      return IOSGeminiFirstPromptSubmissionMethod::
          kNanoBananaTurnThisImageIntoAWatercolorPainting;
    case gemini::InputType::kNanoBananaMakeThisImageLookLikeInstantFilm:
      return IOSGeminiFirstPromptSubmissionMethod::
          kNanoBananaMakeThisImageLookLikeInstantFilm;
    case gemini::InputType::kEditMenuPrompt:
      return IOSGeminiFirstPromptSubmissionMethod::kEditMenuPrompt;
  }
}

IOSGeminiSessionCancellationReason HistogramEnumFromGeminiCancelType(
    GeminiCancelType cancel_type) {
  switch (cancel_type) {
    case GeminiCancelTypeUnknown:
      return IOSGeminiSessionCancellationReason::kUnknown;
    case GeminiCancelTypeStopButtonTapped:
      return IOSGeminiSessionCancellationReason::kStopButtonTapped;
    case GeminiCancelTypeOutsideTapped:
      return IOSGeminiSessionCancellationReason::kOutsideTapped;
    case GeminiCancelTypeExpandedStateCloseButtonTapped:
      return IOSGeminiSessionCancellationReason::
          kExpandedStateCloseButtonTapped;
    case GeminiCancelTypeCollapsedStateCloseButtonTapped:
      return IOSGeminiSessionCancellationReason::
          kCollapsedStateCloseButtonTapped;
    case GeminiCancelTypeLoadingStateCloseButtonTapped:
      return IOSGeminiSessionCancellationReason::kLoadingStateCloseButtonTapped;
  }
}

}  // namespace

@implementation GeminiSessionHandler {
  // The associated WebStateList.
  raw_ptr<WebStateList> _webStateList;
  // Session start time for duration tracking.
  base::TimeTicks _sessionStartTime;
  // Tracks if user has received the first response in current session.
  BOOL _hasReceivedFirstResponse;
  // Tracks if user has sent their first prompt in current session.
  BOOL _hasSubmittedFirstPrompt;
  base::TimeTicks _lastPromptSentTime;
  BOOL _lastPromptHadPageContext;
  BOOL _waitingForResponse;
  // Track prompts per session.
  int _totalPromptsInSession;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _webStateList = webStateList;
  }
  return self;
}

#pragma mark - GeminiSessionDelegate

- (void)newSessionCreatedWithClientID:(NSString*)clientID
                             serverID:(NSString*)serverID {
  [self updateSessionWithClientID:clientID serverID:serverID];
}

- (void)didSwitchToViewState:(ios::provider::GeminiViewState)viewState
                   sessionID:(NSString*)sessionID
              conversationID:(NSString*)conversationID {
  [self.geminiViewStateDelegate didSwitchToViewState:viewState];
}

- (void)didUpdateProcessingStatus:(ios::provider::GeminiClientMode)processStatus
                        sessionID:(NSString*)sessionID
                   conversationID:(NSString*)conversationID {
  [self.geminiViewStateDelegate didUpdateProcessingStatus:processStatus
                                                sessionID:sessionID
                                           conversationID:conversationID];
}

// TODO(crbug.com/504596190): Remove this method when internal code doesn't use
// anymore.
- (void)didChangeLiveProcessStatus:
            (ios::provider::GeminiClientMode)processStatus
                         sessionID:(NSString*)sessionID
                    conversationID:(NSString*)conversationID {
  [self.geminiViewStateDelegate didUpdateProcessingStatus:processStatus
                                                sessionID:sessionID
                                           conversationID:conversationID];
}

- (void)UIDidAppearWithClientID:(NSString*)clientID
                       serverID:(NSString*)serverID {
  [self updateSessionWithClientID:clientID serverID:serverID];

  // Start session timer.
  _sessionStartTime = base::TimeTicks::Now();
  // Reset first response flag for new session.
  _hasReceivedFirstResponse = NO;
  // Reset first prompt flag for new session.
  _hasSubmittedFirstPrompt = NO;
  // Reset prompt counters for new session.
  _totalPromptsInSession = 0;

  [self dismissOtherActiveSessionsUsingClientID:clientID];
}

- (void)UIDidDisappearWithClientID:(NSString*)clientID
                          serverID:(NSString*)serverID {
  [_geminiHandler dismissGeminiFlowWithCompletion:nil];

  web::WebState* webState = [self webStateWithClientID:clientID];
  if (!webState) {
    return;
  }
  // Get the GeminiTabHelper from the WebState.
  GeminiTabHelper* geminiTabHelper = GeminiTabHelper::FromWebState(webState);
  // WebState should always be valid as long as the tab is open.
  if (!geminiTabHelper) {
    // Early exit if no valid tab helper is found.
    return;
  }
  bool isFirstSession = geminiTabHelper->GetIsFirstRun();
  geminiTabHelper->SetIsFirstRun(false);

  // Record session duration.
  if (!_sessionStartTime.is_null()) {
    base::TimeDelta session_duration =
        base::TimeTicks::Now() - _sessionStartTime;

    // Determine session type.
    IOSGeminiSessionType session_type;
    if (_hasSubmittedFirstPrompt) {
      session_type = IOSGeminiSessionType::kWithPrompt;
    } else {
      session_type = IOSGeminiSessionType::kAbandoned;
    }

    RecordGeminiSessionLengthByType(session_duration, isFirstSession,
                                    session_type);
    RecordGeminiSessionTime(session_duration);
    _sessionStartTime = base::TimeTicks();
  }
  // Reset latency tracking on session end.
  _waitingForResponse = NO;
  _lastPromptSentTime = base::TimeTicks();
  // Record prompt counts for the session.
  RecordSessionPromptCount(_totalPromptsInSession);
  RecordSessionFirstPrompt(_hasSubmittedFirstPrompt);
}

- (void)startReceivingResponseWithSessionID:(NSString*)sessionID
                             conversationID:(NSString*)conversationID {
  [self.geminiHandler
      updateFloatyVisibilityIfEligibleAnimated:NO
                                    fromSource:gemini::FloatyUpdateSource::
                                                   ForcedFromQueryResponse];
}

- (void)responseReceivedWithClientID:(NSString*)clientID
                            serverID:(NSString*)serverID
            isNanoBananaToolSelected:(BOOL)isNanoBananaToolSelected
                    isImageGenerated:(BOOL)isImageGenerated {
  [self updateSessionWithClientID:clientID serverID:serverID];

  // Calculate and record response latency.
  if (_waitingForResponse && !_lastPromptSentTime.is_null()) {
    base::TimeDelta latency = base::TimeTicks::Now() - _lastPromptSentTime;
    RecordResponseLatency(latency, _lastPromptHadPageContext, isImageGenerated);

    // Reset latency tracking.
    _waitingForResponse = NO;
    _lastPromptSentTime = base::TimeTicks();
  }

  if (!_hasReceivedFirstResponse) {
    _hasReceivedFirstResponse = YES;
    RecordFirstResponseReceived();
  }
  // Track all responses for conversation engagement.
  RecordGeminiResponseReceived(isImageGenerated);
}

- (void)didTapGeminiSettingsButton {
  [self.settingsHandler showGeminiSettings];
}

- (void)didSendQueryWithInputType:(gemini::InputType)inputType
         isNanoBananaToolSelected:(BOOL)isNanoBananaToolSelected
              imagesAttachedCount:(NSUInteger)imagesAttachedCount
                   longPressImage:(BOOL)longPressImage
              pageContextAttached:(BOOL)pageContextAttached {
  _totalPromptsInSession++;

  // Record that a prompt was sent with arguments.
  RecordGeminiPromptSent(isNanoBananaToolSelected,
                         static_cast<int>(imagesAttachedCount), longPressImage,
                         pageContextAttached);

  // Check if this is the user's first prompt.
  IOSGeminiFirstPromptSubmissionMethod method =
      ConvertInputTypeToHistogramEnum(inputType);

  if (!_hasSubmittedFirstPrompt) {
    _hasSubmittedFirstPrompt = YES;
    RecordFirstPromptSubmission(method);
  }

  // Log submission method for all prompts.
  RecordPromptSubmissionMethod(method);
  // Start latency tracking.
  _lastPromptSentTime = base::TimeTicks::Now();
  _lastPromptHadPageContext = pageContextAttached;
  _waitingForResponse = YES;
}

// Called when a new chat button is tapped.
- (void)didTapNewChatButtonWithSessionID:(NSString*)sessionID
                          conversationID:(NSString*)conversationID {
  web::WebState* webState = [self webStateWithClientID:sessionID];
  if (!webState) {
    return;
  }
  GeminiTabHelper* geminiTabHelper = GeminiTabHelper::FromWebState(webState);
  geminiTabHelper->DeleteGeminiSessionInStorage();
  // Ensure page context is attached for a new chat.
  ios::provider::UpdatePageAttachmentState(
      ios::provider::GeminiPageContextAttachmentState::kAttached);
  geminiTabHelper->NotifyPageContextUpdated(webState);
  // Record the new chat metric.
  RecordGeminiNewChatButtonTapped();

  // Reset flags for the new conversation session.
  _hasSubmittedFirstPrompt = NO;
  _hasReceivedFirstResponse = NO;
  _totalPromptsInSession = 0;
  _waitingForResponse = NO;
  _lastPromptSentTime = base::TimeTicks();
  _lastPromptHadPageContext = NO;
}

// Called when a feedback button is tapped in the Gemini UI.
- (void)didTapFeedbackButton:(GeminiFeedbackType)feedbackType
                   sessionID:(NSString*)sessionID
              conversationID:(NSString*)conversationID {
  switch (feedbackType) {
    case GeminiFeedbackType::kThumbsUp:
      RecordGeminiFeedback(IOSGeminiFeedback::kThumbsUp);
      break;
    case GeminiFeedbackType::kThumbsDown:
      RecordGeminiFeedback(IOSGeminiFeedback::kThumbsDown);
      break;
  }
}

// Called when a gemini session is cancelled.
- (void)responseCancelledWithReason:(GeminiCancelType)reason
                          sessionID:(NSString*)sessionID
                     conversationID:(NSString*)conversationID {
  RecordGeminiSessionCancellation(HistogramEnumFromGeminiCancelType(reason));
}

// Called when the user taps on the photo, gallery, CreateImageSelected or
// CreateImageDeselected in Attachment sheet behind + button.
- (void)didTapInputPlateAttachmentOption:
            (gemini::InputPlateAttachmentOption)attachmentOption
                               sessionID:(NSString*)sessionID
                          conversationID:(NSString*)conversationID {
  RecordGeminiInputPlateAttachmentOptionTapped(attachmentOption);
}

// Called when the user taps on save / share / copy / download image action
// button.
- (void)imageActionButtonTapped:(gemini::ImageActionButtonType)actionButtonType
                      sessionID:(NSString*)sessionID
                 conversationID:(NSString*)conversationID {
  RecordGeminiImageActionButtonTapped(actionButtonType);
}

- (void)didRetryLastRequestWithRegenerateOptionType:
            (gemini::RegenerateOptionType)optionType
                                          sessionID:(NSString*)sessionID
                                     conversationID:(NSString*)conversationID {
  RecordGeminiRegenerateButtonTapped(optionType);
}

- (void)geminiLiveUserDidBargeIn {
  // TODO(crbug.com/512507489): Implement barge-in logic.
}

#pragma mark - Private

// Finds the web state with the given client ID as unique identifier.
- (web::WebState*)webStateWithClientID:(NSString*)clientID {
  for (int i = 0; i < _webStateList->count(); i++) {
    web::WebState* webState = _webStateList->GetWebStateAt(i);
    NSString* webStateUniqueID = base::SysUTF8ToNSString(
        base::NumberToString(webState->GetUniqueIdentifier().identifier()));
    if ([webStateUniqueID isEqualToString:clientID]) {
      return webState;
    }
  }

  return nil;
}

// Updates the session state in storage with the given client ID and server ID.
- (void)updateSessionWithClientID:(NSString*)clientID
                         serverID:(NSString*)serverID {
  web::WebState* webState = [self webStateWithClientID:clientID];
  if (!webState) {
    return;
  }

  GeminiTabHelper* geminiTabHelper = GeminiTabHelper::FromWebState(webState);
  geminiTabHelper->CreateOrUpdateGeminiSessionInStorage(
      base::SysNSStringToUTF8(serverID));
}

// Sets all BWG sessions inactive other than for the WebState matching
// `clientID`.
- (void)dismissOtherActiveSessionsUsingClientID:(NSString*)clientID {
  // TODO(crbug.com/437338434): Keep track of last known active instance to not
  // have to iterate over all WebStates.
  for (int i = 0; i < _webStateList->count(); i++) {
    web::WebState* webState = _webStateList->GetWebStateAt(i);
    NSString* webStateUniqueID = base::SysUTF8ToNSString(
        base::NumberToString(webState->GetUniqueIdentifier().identifier()));
    if (!webState->IsRealized() ||
        [webStateUniqueID isEqualToString:clientID]) {
      continue;
    }

    GeminiTabHelper* geminiTabHelper = GeminiTabHelper::FromWebState(webState);
    geminiTabHelper->DeactivateGeminiSession();
  }
}

@end
