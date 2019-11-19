// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_SHARE_EXTENSION_SHARE_EXTENSION_VIEW_H_
#define IOS_CHROME_SHARE_EXTENSION_SHARE_EXTENSION_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/app_group/app_group_constants.h"

@class ShareExtensionView;

@protocol ShareExtensionViewActionTarget

// Called when the user presses the cancel button.
- (void)shareExtensionViewDidSelectCancel:(id)sender;

// Called when the user presses the "Add to bookmarks" button.
- (void)shareExtensionViewDidSelectAddToBookmarks:(id)sender;

// Called when the user presses the "Add to Reading List" button.
- (void)shareExtensionViewDidSelectAddToReadingList:(id)sender;

// Called when the user presses the "Open in Chrome" button.
- (void)shareExtensionViewDidSelectOpenInChrome:(id)sender;

@end

// This is the view for the ShareExtensionController. It shows the shared
// URL and title and let the user choose between adding a bookmark and
// adding item to reading list.
@interface ShareExtensionView : UIView

// Creates a ShareExtensionView with the |delegate|. Designated
// initializer.
- (instancetype)initWithActionTarget:(id<ShareExtensionViewActionTarget>)target
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Sets the |URL| displayed in the share view.
- (void)setURL:(NSURL*)URL;

// Sets the |title| displayed in the share view.
- (void)setTitle:(NSString*)title;

// Sets the |screenshot| displayed in the share view.
- (void)setScreenshot:(UIImage*)screenshot;

@end

#endif  // IOS_CHROME_SHARE_EXTENSION_SHARE_EXTENSION_VIEW_H_
