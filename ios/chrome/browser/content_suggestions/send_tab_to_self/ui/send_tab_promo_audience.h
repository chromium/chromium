// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SEND_TAB_TO_SELF_UI_SEND_TAB_PROMO_AUDIENCE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SEND_TAB_TO_SELF_UI_SEND_TAB_PROMO_AUDIENCE_H_

// Interface to handle Send Tab promo card user events.
@protocol SendTabPromoAudience

// Called when the promo is selected by the user.
- (void)didSelectSendTabPromo;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SEND_TAB_TO_SELF_UI_SEND_TAB_PROMO_AUDIENCE_H_
