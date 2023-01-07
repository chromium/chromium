// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_VIEWS_IDENTITY_VIEW_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_VIEWS_IDENTITY_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/views/views_constants.h"

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

// Style for the identity view (modify the avatar size, font sizes and some
// margins).
@property(nonatomic, assign) IdentityViewStyle style;

// Initialises IdentityView.
- (instancetype)initWithFrame:(CGRect)frame NS_DESIGNATED_INITIALIZER;

// See -[IdentityView initWithFrame:].
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Sets the avatar shown. The image is resized (40px) and shaped as a round
// image.
- (void)setAvatar:(UIImage*)avatar;

// Sets the title and subtitle. `subtitle` can be nil.
- (void)setTitle:(NSString*)title subtitle:(NSString*)subtitle;

// Sets the color of the title.
- (void)setTitleColor:(UIColor*)color;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_VIEWS_IDENTITY_VIEW_H_
