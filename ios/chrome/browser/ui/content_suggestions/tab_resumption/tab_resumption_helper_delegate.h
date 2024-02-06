// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TAB_RESUMPTION_TAB_RESUMPTION_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TAB_RESUMPTION_TAB_RESUMPTION_HELPER_DELEGATE_H_

#import <Foundation/Foundation.h>

// Delegate handling events from the TabResumptionHelper.
@protocol TabResumptionHelperDelegate

// Signals that the TabResumptionHelper received a new item configuration.
- (void)tabResumptionHelperDidReceiveItem;

// Signals that the Tab Resumption module should be removed.
- (void)removeTabResumptionModule;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TAB_RESUMPTION_TAB_RESUMPTION_HELPER_DELEGATE_H_
