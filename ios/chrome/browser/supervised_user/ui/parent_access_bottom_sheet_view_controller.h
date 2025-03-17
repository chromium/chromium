// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_UI_PARENT_ACCESS_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_UI_PARENT_ACCESS_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/bottom_sheet/bottom_sheet_view_controller.h"
#import "ios/chrome/browser/supervised_user/ui/parent_access_consumer.h"

@protocol ParentAccessBottomSheetViewControllerPresentationDelegate;

// A view controller that displays the embedded PACP widget in a bottom sheet.
@interface ParentAccessBottomSheetViewController
    : BottomSheetViewController <ParentAccessConsumer>

@property(nonatomic, weak)
    id<ParentAccessBottomSheetViewControllerPresentationDelegate>
        presentationDelegate;

@end

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_UI_PARENT_ACCESS_BOTTOM_SHEET_VIEW_CONTROLLER_H_
