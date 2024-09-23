// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"

@protocol SigninPromoViewDelegate;

// This class creates an image view, a label and 2 buttons. This view can be
// configured with 2 modes : "Cold State" and "Warm State".
// + "Cold State" mode displays the chomium icon in the image view, and only
//   displays the primary button.
// + "Warm State" mode displays the image view (big than the cold state mode),
//   displays both buttons.
//
//  For the warm state, the owner should set:
//   - the image for `imageView`, using -[SigninPromoView setProfileImage:]
//   - the label for `textLabel`
//   - the title for `primaryButton`
//   - the title for `secondaryButton`
@interface SigninPromoView : UIView

@property(nonatomic, weak) id<SigninPromoViewDelegate> delegate;
@property(nonatomic, assign) SigninPromoViewMode mode;
@property(nonatomic, strong, readonly) UIImageView* imageView;
@property(nonatomic, strong, readonly) UILabel* textLabel;
@property(nonatomic, strong, readonly) UIButton* primaryButton;
@property(nonatomic, strong, readonly) UIButton* secondaryButton;
// Hidden by default.
@property(nonatomic, strong, readonly) UIButton* closeButton;

// Horizontal padding used for `textLabel`, `primaryButton` and
// `secondaryButton`. Used to compute the preferred max layout width of
// `textLabel`.
@property(nonatomic, assign, readonly) CGFloat horizontalPadding;

// The current layout style. Defaults to `SigninPromoViewStyleStandard`.
@property(nonatomic, assign) SigninPromoViewStyle promoViewStyle;

// Designated initializer.
- (instancetype)initWithFrame:(CGRect)frame NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Sets the image in `imageView`. This method will add a circular background
// using CircularImageFromImage() (so if the image is not squared, it will be
// cropped first). Must only be called in the "Warm State" mode.
- (void)setProfileImage:(UIImage*)image;

// Sets the image in `imageView`. This image will be used as an
// alternative to the chromium icon in "Cold State" mode. This image
// will not use CircularImageFromImage(), instead it will be shown
// as is.
- (void)setNonProfileImage:(UIImage*)image;

// Resets the view to be reused.
- (void)prepareForReuse;

// Starts the spinner on top of the primary button, and disables all buttons.
- (void)startSignInSpinner;
// Stops the spinner on top of the primary button, and enables all buttons.
- (void)stopSignInSpinner;

// Configures primary button using UIButtonConfiguration. `title` should not be
// empty or nil.
- (void)configurePrimaryButtonWithTitle:(NSString*)title;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_SIGNIN_PROMO_VIEW_H_
