// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_SHARE_EXTENSION_SHARE_EXTENSION_SHEET_H_
#define IOS_CHROME_SHARE_EXTENSION_SHARE_EXTENSION_SHEET_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

@protocol ShareExtensionDelegate;

@interface ShareExtensionSheet
    : ConfirmationAlertViewController <ConfirmationAlertActionHandler>

// The image to share.
@property(nonatomic, strong) UIImage* sharedImage;

// The URL, its title and url preview to share.
@property(nonatomic, strong) NSURL* sharedURL;
@property(nonatomic, copy) NSString* sharedTitle;
@property(nonatomic, strong) UIImage* sharedURLPreview;

// The text to share.
@property(nonatomic, copy) NSString* sharedText;

// Whether to display the max limit at the end of the `sharedText`.
@property(nonatomic, assign) BOOL displayMaxLimit;

// Whether the sheet was dismissed for an action within itself (close button or
// primary/secondary buttons were tapped).
@property(nonatomic, assign) BOOL dismissedFromSheetAction;

// The delegate for interactions in `ShareExtensionSheet`.
@property(nonatomic, weak) id<ShareExtensionDelegate> delegate;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_SHARE_EXTENSION_SHARE_EXTENSION_SHEET_H_
