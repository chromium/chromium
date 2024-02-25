// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_CONTENT_NOTIFICATION_PROMO_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_CONTENT_NOTIFICATION_PROMO_COORDINATOR_DELEGATE_H_

// Protocol used to send events from a
// SetUpListContentNotificationPromoCoordinator.
@protocol SetUpListContentNotificationPromoCoordinatorDelegate

// Indicates that the promo finished displaying.
- (void)setUpListContentNotificationPromoDidFinish;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_CONTENT_NOTIFICATION_PROMO_COORDINATOR_DELEGATE_H_
