// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_VIEWS_IDENTITY_VIEW_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_VIEWS_IDENTITY_VIEW_H_

#import <UIKit/UIKit.h>

// View with the avatar on the leading side, a title and a subtitle. Only the
// title is required. The title contains the user name if it exists, or the
// email address. The subtitle contains the email address if the name exists,
// otherwise it is hidden.
// The avatar is displayed as a round image.
// +--------------------------------+
// |  +------+                      |
// |  |      |  Title (name)        |
// |  |Avatar|  Subtitle (email)    |
// |  +------+                      |
// +--------------------------------+
@interface IdentityView : UIView

// Minimum vertical margin above the avatar image and title/subtitle, default
// value is 12.
@property(nonatomic, assign) CGFloat minimumTopMargin;
// Minimum vertical margin below the avatar image and title/subtitle, default
// value is 12.
@property(nonatomic, assign) CGFloat minimumBottomMargin;
// Avatar size, default value is 40.
@property(nonatomic, assign) CGFloat avatarSize;
// Horizontal distance between the avatar and the title, default
// value is 16.
@property(nonatomic, assign) CGFloat avatarTitleMargin;
// Vertical distance between the title and the subtitle., default
// value is 4.
@property(nonatomic, assign) CGFloat titleSubtitleMargin;
@property(nonatomic, strong) UIFont* titleFont;
@property(nonatomic, strong) UIFont* subtitleFont;

// Initialises IdentityView.
- (instancetype)initWithFrame:(CGRect)frame NS_DESIGNATED_INITIALIZER;

// See -[IdentityView initWithFrame:].
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Sets the avatar shown. The image is resized (40px) and shaped as a round
// image.
- (void)setAvatar:(UIImage*)avatar;

// Sets the title and subtitle. |subtitle| can be nil.
- (void)setTitle:(NSString*)title subtitle:(NSString*)subtitle;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_VIEWS_IDENTITY_VIEW_H_
