// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_ATTACHMENT_VIEW_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_ATTACHMENT_VIEW_H_

#import <UIKit/UIKit.h>

// Represents an individual attachment method in the attachment carrousel.
@interface ComposeboxMenuAttachmentView : UIButton

// The image that is displayed in the middle of this attachment view.
@property(nonatomic, strong) UIImage* image;

// The title of the attachment view.
@property(nonatomic, copy) NSString* title;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_ATTACHMENT_VIEW_H_
