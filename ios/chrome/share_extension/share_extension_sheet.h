// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_SHARE_EXTENSION_SHARE_EXTENSION_SHEET_H_
#define IOS_CHROME_SHARE_EXTENSION_SHARE_EXTENSION_SHEET_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"
#import "ios/chrome/common/ui/button_stack/button_stack_view_controller.h"
#import "ios/chrome/share_extension/account_info.h"

@protocol ShareExtensionDelegate;
@interface ShareExtensionSheet
    : ButtonStackViewController <ButtonStackActionDelegate>

// The image to share.
@property(nonatomic, strong) UIImage* sharedImage;

// The URL, its title and url preview to share.
@property(nonatomic, strong) NSURL* sharedURL;
@property(nonatomic, copy) NSString* sharedTitle;
@property(nonatomic, strong) UIImage* sharedURLPreview;
@property(nonatomic, strong) AccountInfo* selectedAccountInfo;

// The text to share.
@property(nonatomic, copy) NSString* sharedText;

// Whether to display the max limit at the end of the `sharedText`.
@property(nonatomic, assign) BOOL displayMaxLimit;

// Whether the sheet was dismissed for an action within itself (close button or
// primary/secondary buttons were tapped).
@property(nonatomic, assign) BOOL dismissedFromSheetAction;

// The delegate for interactions in `ShareExtensionSheet`.
@property(nonatomic, weak) id<ShareExtensionDelegate> delegate;

- (void)setAccounts:(NSArray<AccountInfo*>*)accounts;

@end

#endif  // IOS_CHROME_SHARE_EXTENSION_SHARE_EXTENSION_SHEET_H_
