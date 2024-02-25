// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TABLE_VIEW_CONTROLLER_H_

#import <vector>

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

namespace send_tab_to_self {
struct TargetDeviceInfo;
}

@protocol SendTabToSelfModalDelegate;

// SendTabToSelfTableViewController represents the content for the
// Send Tab To Self Modal dialog.
@interface SendTabToSelfTableViewController : LegacyChromeTableViewController

- (instancetype)
    initWithDeviceList:
        (std::vector<send_tab_to_self::TargetDeviceInfo>)targetDeviceList
              delegate:(id<SendTabToSelfModalDelegate>)delegate
         accountAvatar:(UIImage*)accountAvatar
          accountEmail:(NSString*)accountEmail NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// The text used for the cancel button.
@property(nonatomic, copy) NSString* cancelButtonText;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TABLE_VIEW_CONTROLLER_H_
