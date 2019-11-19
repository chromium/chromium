// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTAINER_VIEW_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>

@class OmniboxTextFieldIOS;

// The omnibox container view is the view that is shown in the location bar's
// edit state. It contains the omnibox textfield and the buttons on the left and
// right of it.
@interface OmniboxContainerView : UIView

// Initialize the container view with the given frame, text color, and tint
// color for omnibox.
- (instancetype)initWithFrame:(CGRect)frame
                    textColor:(UIColor*)textColor
                textFieldTint:(UIColor*)textFieldTint
                     iconTint:(UIColor*)iconTint NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// The containted omnibox textfield.
@property(nonatomic, strong, readonly) OmniboxTextFieldIOS* textField;

// Incognito status of the location bar changes the appearance, such as text
// and icon colors.
@property(nonatomic, assign) BOOL incognito;

// Sets the leading button's image.
- (void)setLeadingImage:(UIImage*)image;

// Shows or hides the leading button.
- (void)setLeadingImageHidden:(BOOL)hidden;

// Sets the alpha level of the leading image view.
- (void)setLeadingImageAlpha:(CGFloat)alpha;

// Asks the container view to attch any layout guides to its views.
- (void)attachLayoutGuides;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTAINER_VIEW_H_
