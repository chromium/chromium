// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_PROMO_STYLE_PROMO_STYLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_COMMON_UI_PROMO_STYLE_PROMO_STYLE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

// A base view controller for the common UI controls in the new Promo
// Style screens.
@interface PromoStyleViewController : UIViewController <UITextViewDelegate>

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// The name of the banner image. Must be set before the view is loaded.
@property(nonatomic, strong) NSString* bannerName;

// When set to YES, the banner will be tall (35% of view height). When set to
// NO, the banner will be of normal height (25% of view height). Defaults to NO.
@property(nonatomic, assign) BOOL isTallBanner;

// When set to NO, the top of the banner will be constrained to the top of the
// view and its height will be 25% or 35% of the view height depending on the
// value of `isTallBanner`. When set to YES, the banner will be constrained so
// as to fill the top space (horizontally and vertically) while preserving
// aspect ratio and constraining the bottom of the image so as to ensure 25% or
// 35% of the top space of the view is covered. Defaults to NO.
@property(nonatomic, assign) BOOL shouldBannerFillTopSpace;

// When set to YES, the banner is hidden. Defaults to NO.
@property(nonatomic, assign) BOOL shouldHideBanner;

// When set to `YES`, an avatar image view is shown. This value has to be set
// before the view is loaded. Defaults to NO.
// See `avatarImage` to set the avatar image.
@property(nonatomic, assign) BOOL hasAvatarImage;

// Sets the avatar image. Needs to `hasAvatarImage` to `YES` before.
@property(nonatomic, strong) UIImage* avatarImage;

// Sets the avatar accessibility label. Needs to `hasAvatarImage` to `YES`
// before.
@property(nonatomic, copy) NSString* avatarAccessibilityLabel;

// The label of the headline below the image. Must be set before the view is
// loaded. This is declared public so the accessibility can be enabled.
@property(nonatomic, strong) UILabel* titleLabel;

// The headline below the image. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* titleText;

// The margin on leading and trailing ends of the title label.
// Must be set before the view is loaded. Defaults to `kTitleHorizontalMargin`.
@property(nonatomic, assign) CGFloat titleHorizontalMargin;

// The subtitle below the title. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* subtitleText;

// The margin between the subtitle and the specificContentView. In cases where
// subtitle is blank, views may want to set this to zero to avoid adding extra
// spacing.
@property(nonatomic, assign) CGFloat subtitleBottomMargin;

// The disclaimer that shows at the bottom of the view, above the action items.
// The disclaimer does not move on scroll.
@property(nonatomic, copy) NSString* disclaimerText;

// URLs for links in disclaimer text, in order of appearance. If this property
// is set, the delegate method `didTapURLInDisclaimer:(NSURL*)` must be
// implemented.
@property(nonatomic, copy) NSArray<NSURL*>* disclaimerURLs;

// The container view for the screen-specific content. Derived view controllers
// should add their UI elements to it.
@property(nonatomic, strong) UIView* specificContentView;

// The text for the primary action. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* primaryActionString;

// The text for the secondary action. Must be set before the view is loaded. If
// not set, there won't be a secondary action button.
@property(nonatomic, copy) NSString* secondaryActionString;

// The text for the tertiary action. Must be set before the view is loaded. If
// not set, there won't be a tertiary action button.
@property(nonatomic, copy) NSString* tertiaryActionString;

// The delegate to invoke when buttons are tapped. Can be derived by screen-
// specific view controllers if additional buttons are used.
@property(nonatomic, weak) id<PromoStyleViewControllerDelegate> delegate;

// When set to YES, the primary button is temporarily replaced with a "More"
// button that scrolls the content, until the user scrolls to the very end of
// the content. If set to NO, the primary button behaves normally. Defaults to
// NO.
@property(nonatomic, assign) BOOL scrollToEndMandatory;

// The text for the "More" button. Must be set before the view is loaded
// for views with "scrollToMandatory = YES."
@property(nonatomic, copy) NSString* readMoreString;

// Controls if there is a help button in the view. Must be set before the
// view is loaded. Defaults to NO.
@property(nonatomic, assign) BOOL shouldShowLearnMoreButton;

// The help button item in the top left of the view. Nil if not available.
@property(nonatomic, readonly) UIButton* learnMoreButton;

// Whether the bottom of the view controller is reached. This value will always
// be YES when `self.scrollToEndMandatory` is NO.
@property(nonatomic, assign, readonly) BOOL didReachBottom;

@end

#endif  // IOS_CHROME_COMMON_UI_PROMO_STYLE_PROMO_STYLE_VIEW_CONTROLLER_H_
