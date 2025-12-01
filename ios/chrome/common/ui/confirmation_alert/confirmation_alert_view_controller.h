// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_CONFIRMATION_ALERT_CONFIRMATION_ALERT_VIEW_CONTROLLER_H_
#define IOS_CHROME_COMMON_UI_CONFIRMATION_ALERT_CONFIRMATION_ALERT_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/button_stack/button_stack_view_controller.h"

@protocol ConfirmationAlertActionHandler;

// A view controller useful to show modal alerts and confirmations. The main
// content consists in a big image, a title, and a subtitle.
// The view controller can have up to three action buttons, which are positioned
// at the bottom. They are arranged, from top to bottom,
// `primaryActionString`, `secondaryActionString`, `tertiaryActionString`.
// Setting those properties will make those buttons be added to the view
// controller.
//
// The layout is structured as follows:
//
// +--------------------------------+
// |          navigationBar         |
// |  +--------------------------+  |
// |  |        titleView         |  |
// |  +--------------------------+  |
// +--------------------------------+
// |           scrollView           |
// |  +--------------------------+  |
// |  |      aboveTitleView      |  |
// |  +--------------------------+  |
// |  |          image           |  |
// |  +--------------------------+  |
// |  |         titleString      |  |
// |  +--------------------------+  |
// |  |        subtitleString    |  |
// |  +--------------------------+  |
// |  |      underTitleView      |  |
// |  +--------------------------+  |
// +--------------------------------+
// |      tertiaryActionButton      |
// +--------------------------------+
// |      primaryActionButton       |
// +--------------------------------+
// |     secondaryActionButton      |
// +--------------------------------+
@interface ConfirmationAlertViewController : ButtonStackViewController

// The background color to apply to the main view. If needed, must be set before
// the view is loaded.
@property(nonatomic, copy) UIColor* mainBackgroundColor;

// The view displayed above titles and subtitles, but under the navigation bar
// and the image view. Nil if not needed. If needed, must be set before the view
// is loaded.
@property(nonatomic, strong) UIView* aboveTitleView;

// The view displayed under titles and subtitles. Nil if not needed.
// If needed, must be set before the view is loaded.
@property(nonatomic, strong) UIView* underTitleView;

// The headline below the image. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* titleString;

// Label displaying the `titleString`. Nil if `titleString` is not set.
@property(nonatomic, strong) UILabel* titleLabel;

// Text style for the title. If nil, will default to UIFontTextStyleTitle1.
@property(nonatomic, copy) UIFontTextStyle titleTextStyle;

// (Optional) The additional headline below the main title. Must be set before
// the view is loaded.
@property(nonatomic, copy) NSString* secondaryTitleString;

// The subtitle below the title. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* subtitleString;

// Text style for the subtitle. If nil, will default to UIFontTextStyleBody.
@property(nonatomic, copy) UIFontTextStyle subtitleTextStyle;

// The color of the text for the subtitle. If nil, will default to
// kTextSecondaryColor.
@property(nonatomic, copy) UIColor* subtitleTextColor;

// The image. May be updated after the view is loaded.
@property(nonatomic, strong) UIImage* image;

// Color used for the image frame background when using
// `imageEnclosedWithShadowAndBadge` or `imageEnclosedWithShadowWithoutBadge`.
// Defaults to `kBackgroundColor`. Must be set before the view is loaded.
@property(nonatomic, strong) UIColor* imageBackgroundColor;

// When set, this value will be set as the accessibility label for the image
// view.
@property(nonatomic, copy) NSString* imageViewAccessibilityLabel;

// Sets the custom spacing at the top if there is no navigation bar. If image is
// set, the spacing is before the image. Otherwise, the spacing is before the
// title label. Must be set before the view is loaded.
@property(nonatomic, assign) CGFloat customSpacingBeforeImageIfNoNavigationBar;

// Sets the custom spacing between the image and the title / subtitle. Must be
// set before the view is loaded.
@property(nonatomic, assign) CGFloat customSpacingAfterImage;

// Sets the custom size for the favicon.
@property(nonatomic, assign) CGFloat customFaviconSideLength;

// Sets the custom spacing of the stackview. Values for
// `customSpacingBeforeImageIfNoNavigationBar` and `customSpacingAfterImage` are
// honored around the image, so this applies to all the other items of the
// stackview. Must be set before the view is loaded.
@property(nonatomic, assign) CGFloat customSpacing;

// When YES, the content is attached to the top of the view instead of being
// centered.
@property(nonatomic) BOOL topAlignedLayout;

// Value to determine whether or not the image's size should be scaled.
@property(nonatomic) BOOL imageHasFixedSize;

// Always show the image view, regardless of size or orientation. Default is NO.
@property(nonatomic) BOOL alwaysShowImage;

// Set to YES to enclose the image in a frame with a shadow and a corner badge
// with a green checkmark. Must be set before the view is loaded. Default is NO.
@property(nonatomic) BOOL imageEnclosedWithShadowAndBadge;

// Set to YES to enclose the image in a frame with a shadow without a corner
// green checkmark badge. Must be set before the view is loaded. Default is NO.
@property(nonatomic, assign) BOOL imageEnclosedWithShadowWithoutBadge;

// The action handler for interactions in this View Controller.
@property(nonatomic, weak) id<ConfirmationAlertActionHandler> actionHandler;

// Indicates whether information stack view items should horizontally fill the
// space.
@property(nonatomic) BOOL shouldFillInformationStack;

// Can be overridden by subclasses to customize the secondary title, e.g. set a
// different style, or a UITextViewDelegate. The default implementation does
// nothing.
- (void)customizeSecondaryTitle:(UITextView*)secondaryTitle;

// Can be overridden by subclasses to customize the subtitle, e.g. set a
// different style, or a UITextViewDelegate. The default implementation does
// nothing.
- (void)customizeSubtitle:(UITextView*)subtitle;

@end

#endif  // IOS_CHROME_COMMON_UI_CONFIRMATION_ALERT_CONFIRMATION_ALERT_VIEW_CONTROLLER_H_
