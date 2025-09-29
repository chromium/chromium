// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_ENTRYPOINT_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_ENTRYPOINT_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/public/commands/page_action_menu_entry_point_commands.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"

// View representing the entry point button of the page action menu.
@interface PageActionMenuEntrypointView
    : ExtendedTouchTargetButton <PageActionMenuEntryPointCommands>

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@property(nonatomic, assign) BOOL newBadgeVisible;

// If YES, highlights PageActionMenu entry point. Otherwise, unhighlights.
- (void)toggleEntryPointHighlight:(BOOL)highlight;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_ENTRYPOINT_VIEW_H_
