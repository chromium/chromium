// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_INSTRUCTIONS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_INSTRUCTIONS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/whats_new/data_source/whats_new_item.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

// View controller for the half screen instruction for What's New feature and
// chrome tip.
@interface WhatsNewInstructionsViewController : UIViewController

- (instancetype)initWithWhatsNewItem:(WhatsNewItem*)item
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// The action handler for interactions in this view controller.
@property(nonatomic, weak) id<ConfirmationAlertActionHandler> actionHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_INSTRUCTIONS_VIEW_CONTROLLER_H_
