// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SEND_TAB_TO_SELF_SEND_TAB_PROMO_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SEND_TAB_TO_SELF_SEND_TAB_PROMO_DELEGATE_H_

// Protocol for events for the Send Tab Promo module.
@protocol SendTabPromoDelegate

// User taps 'Allow Send Tab Notifications' on the Send Tab Promo magic stack
// module.
- (void)allowSendTabNotifications;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SEND_TAB_TO_SELF_SEND_TAB_PROMO_DELEGATE_H_
