// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_UI_SEND_TAB_TO_SELF_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_UI_SEND_TAB_TO_SELF_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import <vector>

#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller.h"

namespace send_tab_to_self {
struct TargetDeviceInfo;
}

@protocol SendTabToSelfModalDelegate;

// SendTabToSelfBottomSheetViewController represents the bottom sheet to choose
// a target device to send the tab to.
@interface SendTabToSelfBottomSheetViewController
    : TableViewBottomSheetViewController

- (instancetype)initWithDeviceList:
                    (std::vector<send_tab_to_self::TargetDeviceInfo>)
                        targetDeviceList
                      accountEmail:(NSString*)accountEmail
                          delegate:(id<SendTabToSelfModalDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithConfiguration:(ButtonStackConfiguration*)configuration
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_UI_SEND_TAB_TO_SELF_BOTTOM_SHEET_VIEW_CONTROLLER_H_
