// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUP_MEDIATOR_H_

#import <Foundation/Foundation.h>

class WebStateList;

@interface TabGroupMediator : NSObject

// TODO(crbug.com/1501837): Add a tab group ID when the ID will be available.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUP_MEDIATOR_H_
