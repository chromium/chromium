// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_DEFAULT_BROWSER_PROMO_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_DEFAULT_BROWSER_PROMO_COORDINATOR_DELEGATE_H_

// Protocol used to send events from a SetUpListDefaultBrowserPromoCoordinator.
@protocol SetUpListDefaultBrowserPromoCoordinatorDelegate

// Indicates that the promo finished displaying. If the user chose to go to
// settings, `success` will be `YES`.
- (void)setUpListDefaultBrowserPromoDidFinish:(BOOL)success;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_DEFAULT_BROWSER_PROMO_COORDINATOR_DELEGATE_H_
