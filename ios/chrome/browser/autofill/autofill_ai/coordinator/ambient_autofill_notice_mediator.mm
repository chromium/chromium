// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/coordinator/ambient_autofill_notice_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/personal_context/core/personal_context_prefs.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/web/public/web_state.h"

@implementation AmbientAutofillNoticeMediator {
  // Pref service to read and write preferences.
  raw_ptr<PrefService> _prefService;
  // WebState associated with the active tab.
  base::WeakPtr<web::WebState> _webState;
  // Parameters of the form activity that triggered the notice.
  autofill::FormActivityParams _params;
  // Handler for dispatching Autofill commands.
  __weak id<AutofillCommands> _autofillHandler;
}

- (instancetype)initWithPrefService:(PrefService*)prefService
                           webState:(base::WeakPtr<web::WebState>)webState
                             params:(const autofill::FormActivityParams&)params
                    autofillHandler:(id<AutofillCommands>)autofillHandler {
  self = [super init];
  if (self) {
    _prefService = prefService;
    _webState = webState;
    _params = params;
    _autofillHandler = autofillHandler;
  }
  return self;
}

- (void)didAcknowledgeNotice {
  if (_webState) {
    AutofillBottomSheetTabHelper* tabHelper =
        AutofillBottomSheetTabHelper::FromWebState(_webState.get());
    if (tabHelper) {
      tabHelper->RefocusElementIfNeeded(_params.frame_id);
    }
  }
  [_autofillHandler dismissAmbientAutofillNotice];
}

- (void)didTapSettings {
  // TODO(crbug.com/533502803): Redirect to the appropriate settings page.
  [_autofillHandler dismissAmbientAutofillNotice];
}

- (void)didDismissNotice {
  [_autofillHandler dismissAmbientAutofillNotice];
}

#pragma mark - Public

- (void)markNoticeShown {
  if (_prefService) {
    _prefService->SetBoolean(
        personal_context::prefs::
            kPersonalContextAmbientAutofillNoticeShouldBeShown,
        false);
  }
}

@end
