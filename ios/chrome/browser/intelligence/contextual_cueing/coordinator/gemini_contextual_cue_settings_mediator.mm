// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/coordinator/gemini_contextual_cue_settings_mediator.h"

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_tab_helper.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/ui/gemini_contextual_cue_settings_consumer.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/web/public/web_state.h"

@implementation GeminiContextualCueSettingsMediator {
  // Active WebState from which the contextual cue prompt is read.
  base::WeakPtr<web::WebState> _webState;
  // Handler for Gemini commands.
  __weak id<GeminiCommands> _geminiHandler;
  // Handler for Settings commands.
  __weak id<SettingsCommands> _settingsHandler;
}

- (instancetype)initWithWebState:(web::WebState*)webState
                   geminiHandler:(id<GeminiCommands>)geminiHandler
                 settingsHandler:(id<SettingsCommands>)settingsHandler {
  self = [super init];
  if (self) {
    if (webState) {
      _webState = webState->GetWeakPtr();
    }
    _geminiHandler = geminiHandler;
    _settingsHandler = settingsHandler;
  }
  return self;
}

- (void)setConsumer:(id<GeminiContextualCueSettingsConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  [_consumer setPrompt:[self currentPrompt]];
}

- (void)disconnect {
  _webState = nullptr;
  _geminiHandler = nil;
  _settingsHandler = nil;
  _consumer = nil;
  _delegate = nil;
}

#pragma mark - GeminiContextualCueSettingsMutator

- (void)didTapManageSuggestions {
  id<SettingsCommands> settingsHandler = _settingsHandler;
  [self.delegate geminiContextualCueSettingsMediator:self
                       dismissSettingsWithCompletion:^{
                         [settingsHandler showGeminiContextualCueSettings];
                       }];
}

- (void)didTapEditPrompt {
  id<GeminiCommands> geminiHandler = _geminiHandler;
  GeminiStartupState* state = [[GeminiStartupState alloc]
      initWithEntryPoint:gemini::EntryPoint::ContextualCueInfobar];
  state.prepopulatedPrompt = [self currentPrompt];
  state.shouldAutoSubmit = NO;

  [self.delegate geminiContextualCueSettingsMediator:self
                       dismissSettingsWithCompletion:^{
                         [geminiHandler
                             startGeminiEntryFlowWithStartupState:state
                                               baseViewController:nil
                                         showSnackbarOnCompletion:YES
                                                       completion:nil];
                       }];
}

#pragma mark - Private

// Returns the contextual cue prompt for `_webState`, or nil if unavailable.
- (NSString*)currentPrompt {
  if (!_webState) {
    return nil;
  }
  contextual_cueing::ContextualCueingTabHelper* tabHelper =
      contextual_cueing::ContextualCueingTabHelper::FromWebState(
          _webState.get());
  if (!tabHelper) {
    return nil;
  }
  return [tabHelper->GetContextualCuePrompt() copy];
}

@end
