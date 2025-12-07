// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_session_handler.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_session_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"

namespace {

IOSGeminiFirstPromptSubmissionMethod ConvertBWGInputTypeToHistogramEnum(
    BWGInputType input_type) {
  switch (input_type) {
    case BWGInputTypeText:
      return IOSGeminiFirstPromptSubmissionMethod::kText;
    case BWGInputTypeSummarize:
      return IOSGeminiFirstPromptSubmissionMethod::kSummarize;
    case BWGInputTypeCheckThisSite:
      return IOSGeminiFirstPromptSubmissionMethod::kCheckThisSite;
    case BWGInputTypeFindRelatedSites:
      return IOSGeminiFirstPromptSubmissionMethod::kFindRelatedSites;
    case BWGInputTypeAskAboutPage:
      return IOSGeminiFirstPromptSubmissionMethod::kAskAboutPage;
    case BWGInputTypeCreateFaq:
      return IOSGeminiFirstPromptSubmissionMethod::kCreateFaq;
    case BWGInputTypeZeroStateModelSuggestion:
      return IOSGeminiFirstPromptSubmissionMethod::kZeroStateSuggestions;
    case BWGInputTypeWhatCanGeminiDo:
      return IOSGeminiFirstPromptSubmissionMethod::kWhatCanGeminiDo;
    case BWGInputTypeDiscoveryCard:
      return IOSGeminiFirstPromptSubmissionMethod::kDiscoveryCard;
    case BWGInputTypeUnknown:
    default:
      return IOSGeminiFirstPromptSubmissionMethod::kUnknown;
  }
}

}  // namespace

@implementation BWGSessionHandler {
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

#pragma mark - BWGSessionDelegate

- (void)newSessionCreatedWithClientID:(NSString*)clientID
                             serverID:(NSString*)serverID {
  [self updateSessionWithClientID:clientID serverID:serverID];
}

- (void)UIDidAppearWithClientID:(NSString*)clientID
                       serverID:(NSString*)serverID {
  [self updateSessionWithClientID:clientID serverID:serverID];
  [self setSessionActive:YES clientID:clientID];

  // Start session timer.
  _sessionStartTime = base::TimeTicks::Now();
  // Reset first response flag for new session.
  _hasReceivedFirstResponse = NO;
  // Reset first prompt flag for new session.
  _hasSubmittedFirstPrompt = NO;

  if (IsGeminiCrossTabEnabled()) {
    [self dismissOtherActiveSessionsUsingClientID:clientID];
  }
  // Reset prompt counters for new session.
  _totalPromptsInSession = 0;
}

- (void)UIDidDisappearWithClientID:(NSString*)clientID
                          serverID:(NSString*)serverID {
  [_BWGHandler dismissBWGFlowWithCompletion:nil];
  [self setSessionActive:NO clientID:clientID];

  web::WebState* webState = [self webStateWithClientID:clientID];
  if (!webState) {
    return;
  }
  // Get the BWGTabHelper from the WebState.
  BwgTabHelper* BWGTabHelper = BwgTabHelper::FromWebState(webState);
  // WebState should always be valid as long as the tab is open.
  if (!BWGTabHelper) {
    // Early exit if no valid tab helper is found.
    return;
  }
  bool isFirstSession = BWGTabHelper->GetIsFirstRun();
  BWGTabHelper->SetIsFirstRun(false);

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

    RecordBWGSessionLengthByType(session_duration, isFirstSession,
                                 session_type);
    RecordBWGSessionTime(session_duration);
    _sessionStartTime = base::TimeTicks();
  }
  // Reset latency tracking on session end.
  _waitingForResponse = NO;
  _lastPromptSentTime = base::TimeTicks();
  // TODO(crbug.com/435649967): log # of times users dismissed the floaty before
  // receiving a response.
  // Record prompt counts for the session.
  RecordSessionPromptCount(_totalPromptsInSession);
  RecordSessionFirstPrompt(_hasSubmittedFirstPrompt);
}

- (void)responseReceivedWithClientID:(NSString*)clientID
                            serverID:(NSString*)serverID {
  [self updateSessionWithClientID:clientID serverID:serverID];

  // Calculate and record response latency.
  if (_waitingForResponse && !_lastPromptSentTime.is_null()) {
    base::TimeDelta latency = base::TimeTicks::Now() - _lastPromptSentTime;
    RecordResponseLatency(latency, _lastPromptHadPageContext);

    // Reset latency tracking.
    _waitingForResponse = NO;
    _lastPromptSentTime = base::TimeTicks();
  }

  if (!_hasReceivedFirstResponse) {
    _hasReceivedFirstResponse = YES;
    RecordFirstResponseReceived();
  }
  // Track all responses for conversation engagement.
  RecordBWGResponseReceived();
}

- (void)didTapBWGSettingsButton {
  [self.settingsHandler showBWGSettings];
}

- (void)didSendQueryWithInputType:(BWGInputType)inputType
              pageContextAttached:(BOOL)pageContextAttached {
  _totalPromptsInSession++;

  // Record user action for prompt sent.
  RecordBWGPromptSent();

  // Check if this is the user's first prompt.
  if (!_hasSubmittedFirstPrompt) {
    _hasSubmittedFirstPrompt = YES;
    IOSGeminiFirstPromptSubmissionMethod method =
        ConvertBWGInputTypeToHistogramEnum(inputType);
    RecordFirstPromptSubmission(method);
  }
  // Track context attachment for all prompts.
  RecordPromptContextAttachment(pageContextAttached);
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
  BwgTabHelper* BWGTabHelper = BwgTabHelper::FromWebState(webState);
  BWGTabHelper->DeleteBwgSessionInStorage();
  // Record the new chat metric.
  RecordBWGNewChatButtonTapped();
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

  BwgTabHelper* BWGTabHelper = BwgTabHelper::FromWebState(webState);
  BWGTabHelper->CreateOrUpdateBwgSessionInStorage(
      base::SysNSStringToUTF8(serverID));
}

// Sets the session's active state for the given client ID.
- (void)setSessionActive:(BOOL)active clientID:(NSString*)clientID {
  web::WebState* webState = [self webStateWithClientID:clientID];
  if (!webState) {
    return;
  }

  BwgTabHelper* BWGTabHelper = BwgTabHelper::FromWebState(webState);
  BWGTabHelper->SetBwgUiShowing(active);
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

    BwgTabHelper* BWGTabHelper = BwgTabHelper::FromWebState(webState);
    BWGTabHelper->DeactivateBWGSession();
  }
}

@end
