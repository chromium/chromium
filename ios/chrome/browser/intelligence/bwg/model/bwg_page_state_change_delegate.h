// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_PAGE_STATE_CHANGE_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_PAGE_STATE_CHANGE_DELEGATE_H_

#import <Foundation/Foundation.h>

// Delegate for the BWG pageState changes.
@protocol BWGPageStateChangeDelegate

// Requests the sharing status of PageContext from prefs, and if false prompts
// the user to enable sharing. Executes the callback with the final status.
- (void)requestPageContextSharingStatusWithCompletion:
    (void (^)(BOOL sharingEnabled))completionCallBack;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_PAGE_STATE_CHANGE_DELEGATE_H_
