// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/coordinator/autofill_ai_private_inference_notice_mediator.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/metrics/autofill_metrics.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"

@implementation AutofillAIPrivateInferenceNoticeMediator {
  // Preference service to update timestamps.
  raw_ptr<PrefService> _prefService;
  // Handler for dispatching Autofill commands.
  __weak id<AutofillCommands> _autofillHandler;
  // Handler for dispatching Settings commands.
  __weak id<SettingsCommands> _settingsHandler;
  // Tracks whether an interaction was handled (Acknowledged, Settings click, or
  // Dismissed), preventing duplicate actions upon repeated calls.
  BOOL _interactionHandled;
}

- (instancetype)initWithPrefService:(PrefService*)prefService
                    autofillHandler:(id<AutofillCommands>)autofillHandler
                    settingsHandler:(id<SettingsCommands>)settingsHandler {
  self = [super init];
  if (self) {
    CHECK(prefService);
    _prefService = prefService;
    _autofillHandler = autofillHandler;
    _settingsHandler = settingsHandler;
    _interactionHandled = NO;
  }
  return self;
}

#pragma mark - AutofillAIPrivateInferenceNoticeMutator

- (void)markNoticeShown {
  autofill::AutofillMetrics::LogAutofillAiPrivateInferenceNoticeInteraction(
      autofill::AutofillMetrics::PopupNoticeInteractions::kShown);
  _prefService->SetTime(
      autofill::prefs::kAutofillAiPrivateInferenceNoticeShownTimestamp,
      base::Time::Now());
}

- (void)didAcknowledgeNotice {
  if (_interactionHandled) {
    return;
  }
  _interactionHandled = YES;
  autofill::AutofillMetrics::LogAutofillAiPrivateInferenceNoticeInteraction(
      autofill::AutofillMetrics::PopupNoticeInteractions::kAcknowledged);
  _prefService->SetTime(
      autofill::prefs::kAutofillAiPrivateInferenceNoticeAcknowledgedTimestamp,
      base::Time::Now());
  [_autofillHandler dismissAutofillAIPrivateInferenceNotice];
}

- (void)didTapSettings {
  if (_interactionHandled) {
    return;
  }
  _interactionHandled = YES;
  autofill::AutofillMetrics::LogAutofillAiPrivateInferenceNoticeInteraction(
      autofill::AutofillMetrics::PopupNoticeInteractions::kLinkButtonClicked);
  _prefService->SetTime(
      autofill::prefs::kAutofillAiPrivateInferenceNoticeAcknowledgedTimestamp,
      base::Time::Now());
  id<SettingsCommands> settingsHandler = _settingsHandler;
  [settingsHandler showAutofillSettingsFromNotice];
}

- (void)didDismissNotice {
  if (_interactionHandled) {
    return;
  }
  _interactionHandled = YES;
  autofill::AutofillMetrics::LogAutofillAiPrivateInferenceNoticeInteraction(
      autofill::AutofillMetrics::PopupNoticeInteractions::kDismissed);
  [_autofillHandler dismissAutofillAIPrivateInferenceNotice];
}

@end
