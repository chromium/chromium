// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_dialog_blocking_action.h"

#include "base/logging.h"
#include "ios/chrome/browser/overlays/public/web_content_area/java_script_dialog_source.h"
#import "ios/chrome/browser/ui/dialogs/java_script_dialog_blocking_state.h"
#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_mediator.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns the blocking state for |source|'s WebState, or nullptr if the
// WebState was destroyed.
JavaScriptDialogBlockingState* GetBlockingState(
    const JavaScriptDialogSource& source) {
  web::WebState* web_state = source.web_state();
  if (!web_state)
    return nullptr;
  JavaScriptDialogBlockingState::CreateForWebState(web_state);
  return JavaScriptDialogBlockingState::FromWebState(web_state);
}
}  // namespace

AlertAction* GetBlockingAlertAction(AlertOverlayMediator* mediator,
                                    const JavaScriptDialogSource& source) {
  JavaScriptDialogBlockingState* blocking_state = GetBlockingState(source);
  if (!blocking_state || !blocking_state->show_blocking_option())
    return nil;

  __weak AlertOverlayMediator* weakMediator = mediator;
  NSString* action_title =
      l10n_util::GetNSString(IDS_IOS_JAVA_SCRIPT_DIALOG_BLOCKING_BUTTON_TEXT);
  return [AlertAction
      actionWithTitle:action_title
                style:UIAlertActionStyleDestructive
              handler:^(AlertAction* action) {
                // Re-fetch the blocking state before accessing it in case the
                // WebState was deallocated before block execution.
                JavaScriptDialogBlockingState* blocking_state =
                    GetBlockingState(source);
                if (blocking_state)
                  blocking_state->JavaScriptDialogBlockingOptionSelected();
                [weakMediator.delegate stopDialogForMediator:weakMediator];
              }];
}
