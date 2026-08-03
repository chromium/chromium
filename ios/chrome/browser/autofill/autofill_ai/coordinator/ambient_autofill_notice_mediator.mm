// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/coordinator/ambient_autofill_notice_mediator.h"

#import "components/autofill/ios/browser/autofill_client_ios.h"
#import "components/personal_context/first_run/personal_context_first_run_service.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/web/public/web_state.h"

@implementation AmbientAutofillNoticeMediator {
  // WebState associated with the active tab.
  base::WeakPtr<web::WebState> _webState;
  // Parameters of the form activity that triggered the notice.
  autofill::FormActivityParams _params;
  // Handler for dispatching Autofill commands.
  __weak id<AutofillCommands> _autofillHandler;
}

- (instancetype)initWithWebState:(base::WeakPtr<web::WebState>)webState
                          params:(const autofill::FormActivityParams&)params
                 autofillHandler:(id<AutofillCommands>)autofillHandler {
  self = [super init];
  if (self) {
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
  if (!_webState) {
    return;
  }
  autofill::AutofillClientIOS* client =
      autofill::AutofillClientIOS::FromWebState(_webState.get());
  if (!client) {
    return;
  }
  if (personal_context::PersonalContextFirstRunService* service =
          client->GetPersonalContextFirstRunService()) {
    service->MarkPersonalContextAmbientAutofillNoticeAsAcknowledged();
  }
}

@end
