// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SEND_TAB_TO_SELF_COORDINATOR_SEND_TAB_PROMO_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SEND_TAB_TO_SELF_COORDINATOR_SEND_TAB_PROMO_MEDIATOR_DELEGATE_H_

// Delegate handling events from the SendTabPromoMediator.
@protocol SendTabPromoMediatorDelegate

// Signals that the user has received a tab sent from one of their other
// devices.
- (void)sentTabReceived;

// Signals that the Send Tab Promo Module should be removed.
- (void)removeSendTabPromoModule;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SEND_TAB_TO_SELF_COORDINATOR_SEND_TAB_PROMO_MEDIATOR_DELEGATE_H_
