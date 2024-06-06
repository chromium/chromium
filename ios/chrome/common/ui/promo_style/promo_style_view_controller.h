// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_PROMO_STYLE_PROMO_STYLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_COMMON_UI_PROMO_STYLE_PROMO_STYLE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

enum class PromoStyleImageType {
  kNone,
  kAvatar,
  kImage,
  kImageWithShadow,
};

enum class BannerImageSizeType {
  kShort,
  kStandard,
  kTall,
  kExtraTall,
};

// Indicates the visibility and style of action buttons.
enum class ActionButtonsVisibility {
  // No visibility changes have been made to the action buttons.
  kDefault,
  // Primary and secondary buttons are hidden.
  kHidden,
  // Primary and secondary buttons are shown in the default differing styles.
  kRegularButtonsShown,
  // Primary and secondary buttons are shown with the same style.
  kEquallyWeightedButtonShown,
};

// A base view controller for the common UI controls in the new Promo
// Style screens.
@interface PromoStyleViewController : UIViewController <UITextViewDelegate>

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// The name of the banner image. Must be set before the view is loaded.
@property(nonatomic, strong) NSString* bannerName;

// The ratio of the view covered by the banner.
// - kStandard: 25%,
// - kTall: 35%,
// - kExtraTall: 50%.
@property(nonatomic, assign) BannerImageSizeType bannerSize;

// When set to NO, the top of the banner will be constrained to the top of the
// view and its height will be 25% or 35% of the view height depending on the
// value of `isTallBanner`. When set to YES, the banner will be constrained so
// as to fill the top space (horizontally and vertically) while preserving
// aspect ratio and constraining the bottom of the image so as to ensure the
// correct ratio of the view is covered. Defaults to NO.
@property(nonatomic, assign) BOOL shouldBannerFillTopSpace;

// When set to YES, the banner is hidden. Defaults to NO.
@property(nonatomic, assign) BOOL shouldHideBanner;

// When set to YES, use `PromoStyleBackgroundView` as background. Only available
// with hidden banner. This value has to be set before the view is loaded.
// Defaults to NO.
@property(nonatomic, assign) BOOL usePromoStyleBackground;

// The type of image to display in the header. This value has to be set before
// the view is loaded. Defaults to kNone.
// See `headerImage` to set the actual image.
@property(nonatomic, assign) PromoStyleImageType headerImageType;

// Sets the header image. Needs to `headerImageType` to the correct type before.
@property(nonatomic, strong) UIImage* headerImage;

// Sets a background image for the header image. Must be set before the view is
// loaded.
@property(nonatomic, strong) UIImage* headerBackgroundImage;

// Sets the header image accessibility label. Needs to `headerImageType` to not
// kNone. before.
@property(nonatomic, copy) NSString* headerAccessibilityLabel;

// When set to YES, the header will be hidden when the content is taller than
// the scroll view. This can make the content fully visible or require less
// scrolling when using a smaller form factor device or a larger font size.
// Once hidden, the header will not reappear. Default to NO.
@property(nonatomic, assign) BOOL hideHeaderOnTallContent;

// When set to YES, forces UIUserInterfaceStyleLight for the header views. This
// value has to be set before the view is loaded. Default to NO.
@property(nonatomic, assign) BOOL headerViewForceStyleLight;

// The top margin percentage of the header view when there is no background.
// Must be set before the view is loaded. Defaults to
// `kNoBackgroundHeaderImageTopMarginPercentage`.
@property(nonatomic, assign) CGFloat noBackgroundHeaderImageTopMarginPercentage;

// The bottom margin of the header image, when the image is not of type kAvatar.
// Must be set before the view is loaded. Defaults to `kDefaultMargin`.
@property(nonatomic, assign) CGFloat headerImageBottomMargin;

// The inset of the header image shadow. Must be set before the view is loaded.
// Defaults to `kHeaderImageShadowShadowInset`.
@property(nonatomic, assign) CGFloat headerImageShadowInset;

// The label of the headline below the image. Must be set before the view is
// loaded. This is declared public so the accessibility can be enabled.
@property(nonatomic, strong) UILabel* titleLabel;

// The subtitle label below the title. Must be set before the view is loaded.
@property(nonatomic, strong) UILabel* subtitleLabel;

// The headline below the image. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* titleText;

// The margin on leading and trailing ends of the title label.
// Must be set before the view is loaded. Defaults to `kTitleHorizontalMargin`.
@property(nonatomic, assign) CGFloat titleHorizontalMargin;

// Top margin of the title label when there is no header image set. Must be set
// before the view is loaded. Defaults to zero.
@property(nonatomic, assign) CGFloat titleTopMarginWhenNoHeaderImage;

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

// Indicates that the specificContentView will not be used and must be hidden.
@property(nonatomic, assign) BOOL hideSpecificContentView;

// The text for the primary action. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* primaryActionString;

// The configuration update handler for the primaryActionButton. Must be set
// before the view is loaded.
@property(nonatomic, copy) UIButtonConfigurationUpdateHandler updateHandler;

// The primary action button is enabled when set to YES, disabled when NO. The
// button is enabled by default.
@property(nonatomic, assign) BOOL primaryButtonEnabled;

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

// Adds a rounded corner limit to the banner view to mimic a two card view.
@property(nonatomic, assign) BOOL bannerLimitWithRoundedCorner;

// If YES, constrains the scroll view to the top of the view (outside
// safeAreaLayoutGuide), putting it behind any navigation bars. By default,
// scroll view is constrained within the safeAreaLayoutGuide.
// Must be set before view is loaded.
@property(nonatomic, assign) BOOL layoutBehindNavigationBar;

// Aligns the elements to the top of the view.
@property(nonatomic, assign) BOOL topAlignedLayout;

// Visibility and style indicator of the primary and secondary action buttons.
@property(nonatomic, assign, readwrite)
    ActionButtonsVisibility actionButtonsVisibility;

// Whether the primary button should be disabled and have its button text
// replaced with a spinner. Should be set only after the view is loaded.
@property(nonatomic, assign) BOOL primaryButtonSpinnerEnabled;

@end

#endif  // IOS_CHROME_COMMON_UI_PROMO_STYLE_PROMO_STYLE_VIEW_CONTROLLER_H_
