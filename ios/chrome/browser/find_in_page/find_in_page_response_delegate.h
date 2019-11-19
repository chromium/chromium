// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIND_IN_PAGE_FIND_IN_PAGE_RESPONSE_DELEGATE_H_
#define IOS_CHROME_BROWSER_FIND_IN_PAGE_FIND_IN_PAGE_RESPONSE_DELEGATE_H_

@class FindInPageModel;

// Delegate class to relay responses of FindInPageController calls to
// BrowserViewController.
@protocol FindInPageResponseDelegate
@optional
// Called once a Find action finishes.
- (void)findDidFinishWithUpdatedModel:(FindInPageModel*)model;
// Called once Find in Page is properly disabled.
- (void)findDidStop;
@end

#endif  // IOS_CHROME_BROWSER_FIND_IN_PAGE_FIND_IN_PAGE_RESPONSE_DELEGATE_H_
