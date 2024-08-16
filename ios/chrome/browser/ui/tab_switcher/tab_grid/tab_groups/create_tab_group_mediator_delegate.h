// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_CREATE_TAB_GROUP_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_CREATE_TAB_GROUP_MEDIATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

@class CreateTabGroupMediator;

// The delegate gets notified of CreateTabGroupMediator lifecycle events.
@protocol CreateTabGroupMediatorDelegate

// Notifies the delegate that the group that `mediator` is editing was
// externally deleted or its visual data changed (typically via Tab Group Sync
// on a different device).
- (void)createTabGroupMediatorEditedGroupWasExternallyMutated:
    (CreateTabGroupMediator*)mediator;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_CREATE_TAB_GROUP_MEDIATOR_DELEGATE_H_
