// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/dialogs/ui_bundled/java_script_dialog_blocking_state.h"

#import "base/check_op.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"

JavaScriptDialogBlockingState::JavaScriptDialogBlockingState(
    web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
}

JavaScriptDialogBlockingState::~JavaScriptDialogBlockingState() {
  // It is expected that WebStateDestroyed() will be received before this state
  // is deallocated.
  DCHECK(!web_state_);
}

void JavaScriptDialogBlockingState::JavaScriptDialogBlockingOptionSelected() {
  blocked_item_ = web_state_->GetNavigationManager()->GetLastCommittedItem();
  DCHECK(blocked_item_);
}

void JavaScriptDialogBlockingState::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_EQ(web_state_, web_state);
  web::NavigationItem* item =
      web_state->GetNavigationManager()->GetLastCommittedItem();
  // The dialog blocking state should be reset for user-initiated loads or for
  // document-changing, non-reload navigations.
  bool navigation_is_reload = ui::PageTransitionCoreTypeIs(
      navigation_context->GetPageTransition(), ui::PAGE_TRANSITION_RELOAD);
  if (!navigation_context->IsRendererInitiated() ||
      (!navigation_context->IsSameDocument() && item != blocked_item_ &&
       !navigation_is_reload)) {
    dialog_count_ = 0;
    blocked_item_ = nullptr;
  }
}

void JavaScriptDialogBlockingState::WebStateDestroyed(
    web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

WEB_STATE_USER_DATA_KEY_IMPL(JavaScriptDialogBlockingState)
