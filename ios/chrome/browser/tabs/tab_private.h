// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_PRIVATE_H_
#define IOS_CHROME_BROWSER_TABS_TAB_PRIVATE_H_

#include "ios/net/request_tracker.h"

@class CRWWebController;

// Exposed private methods for testing purpose.
@interface Tab ()

- (OpenInController*)openInController;

@end

@interface Tab (TestingSupport)

// Returns the Tab owning TabModel.
- (TabModel*)parentTabModel;

// The CRWWebController from the Tab's WebState. This should only be used
// by tests and will be removed when Tab can wrap TestWebState (see issue
// crbug.com/620465 for progress).
@property(nonatomic, readonly) CRWWebController* webController;

@end

#endif  // IOS_CHROME_BROWSER_TABS_TAB_PRIVATE_H_
