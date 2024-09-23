// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_CONFIRMATION_ALERT_CONFIRMATION_ALERT_VIEW_CONTROLLER_H_
#define IOS_CHROME_COMMON_UI_CONFIRMATION_ALERT_CONFIRMATION_ALERT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol ConfirmationAlertActionHandler;

// A view controller useful to show modal alerts and confirmations. The main
// content consists in a big image, a title, and a subtitle which are contained
// in a scroll view for cases when the content doesn't fit in the screen.
// The view controller can have up to three action buttons, which are positioned
// at the bottom. They are arranged, from top to bottom,
// `primaryActionString`, `secondaryActionString`, `tertiaryActionString`.
// Setting those properties will make those buttons be added to the view
// controller.
@interface ConfirmationAlertViewController : UIViewController

// The navigation bar title view. Nil if not needed. If needed, must be set
// before the view is loaded.
@property(nonatomic, strong) UIView* titleView;

// The view displayed above titles and subtitles, but under the navigation bar
// and the image view. Nil if not needed. If needed, must be set before the view
// is loaded.
@property(nonatomic, strong) UIView* aboveTitleView;

// The view displayed under titles and subtitles. Nil if not needed.
// If needed, must be set before the view is loaded.
@property(nonatomic, strong) UIView* underTitleView;

// The headline below the image. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* titleString;

// Text style for the title. If nil, will default to UIFontTextStyleTitle1.
@property(nonatomic, copy) UIFontTextStyle titleTextStyle;

// (Optional) The additional headline below the main title. Must be set before
// the view is loaded.
@property(nonatomic, copy) NSString* secondaryTitleString;

// The subtitle below the title. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* subtitleString;

// Text style for the subtitle. If nil, will default to UIFontTextStyleBody.
@property(nonatomic, copy) UIFontTextStyle subtitleTextStyle;

// The text for the primary action. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* primaryActionString;

// The text for the secondary action. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* secondaryActionString;

// The color of the text for the secondary action. Must be set before the view
// is loaded to be effective. Use the kBlueColor by default if the property is
// not set.
@property(nonatomic, copy) NSString* secondaryActionTextColor;

// The icon for the secondary action. Must be set before the view is loaded.
@property(nonatomic, strong) UIImage* secondaryActionImage;

// The text for the tertiary action. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* tertiaryActionString;

// The image. May be updated after the view is loaded.
@property(nonatomic, strong) UIImage* image;

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

// Sets the custom height for the gradient view above the action buttons.
@property(nonatomic, assign) CGFloat customGradientViewHeight;

// When YES, the content is attached to the top of the view instead of being
// centered.
@property(nonatomic) BOOL topAlignedLayout;

// Value to determine whether or not the image's size should be scaled.
@property(nonatomic) BOOL imageHasFixedSize;

// Always show the image view, regardless of size or orientation. Default is NO.
@property(nonatomic) BOOL alwaysShowImage;

// Controls if there is a help button in the view. Must be set before the
// view is loaded.
@property(nonatomic) BOOL helpButtonAvailable;

// Set to YES to enclose the image in a frame with a shadow and a corner badge
// with a green checkmark. Must be set before the view is loaded. Default is NO.
@property(nonatomic) BOOL imageEnclosedWithShadowAndBadge;

// Set to YES to enclose the image in a frame with a shadow without a corner
// green checkmark badge. Must be set before the view is loaded. Default is NO.
@property(nonatomic, assign) BOOL imageEnclosedWithShadowWithoutBadge;

// Set to NO to prevent the scroll view from showing a vertical scrollbar
// indicator. Must be set before the view is loaded. Default is YES.
@property(nonatomic) BOOL showsVerticalScrollIndicator;

// Set to NO to prevent the scroll view from scrolling. Default is YES.
@property(nonatomic) BOOL scrollEnabled;

// When set, this value will be set as the accessibility label for the help
// button.
@property(nonatomic, copy) NSString* helpButtonAccessibilityLabel;

// The help button item in the top left of the view. Nil if not available.
@property(nonatomic, readonly) UIBarButtonItem* helpButton;

// Controls if the navigation bar dismiss button is available in the view.
// Default is YES. Must be set before the view is loaded.
@property(nonatomic) BOOL showDismissBarButton;

// Allows to modify the system item for the dismiss bar button (defaults to
// UIBarButtonSystemItemDone). Must be set before the view is loaded.
@property(nonatomic, assign) UIBarButtonSystemItem dismissBarButtonSystemItem;

// Sets a custom UIBarButtonItem for the dismiss bar button.
@property(nonatomic, assign) UIImage* customDismissBarButtonImage;

// The action handler for interactions in this View Controller.
@property(nonatomic, weak) id<ConfirmationAlertActionHandler> actionHandler;

// Sets the custom scroll view bottom insets.
@property(nonatomic, assign) CGFloat customScrollViewBottomInsets;

// Indicates whether information stack view items should horizontally fill the
// space.
@property(nonatomic) BOOL shouldFillInformationStack;

// Bottom margin for the action stack view.
@property(nonatomic, assign) CGFloat actionStackBottomMargin;

// Button for the primary action string.
@property(nonatomic, readonly) UIButton* primaryActionButton;

// Color used for the activity indicator on the primary button when in the
// loading state. Defaults to kSolidWhiteColor.
@property(nonatomic, strong) UIColor* activityIndicatorColor;

// Color used for the confirmation checkmark on the primary button when in the
// confirmation state. Defaults to kBlue700Color.
@property(nonatomic, strong) UIColor* confirmationCheckmarkColor;

// Color used for the background on the primary button when in the confirmation
// state. Defaults to kBlue100Color.
@property(nonatomic, strong) UIColor* confirmationButtonColor;

// Indicates whether this view shows itself in a loading state: The primary
// button is disabled and shows an activity indicator instead of the primary
// action string; and other action buttons are disabled.
@property(nonatomic, assign) BOOL isLoading;

// Shows a checkmark on the primary action button instead of the primary action
// text, and shows the primary action button in a disabled state.
@property(nonatomic, assign) BOOL isConfirmed;

// Designated initializer.
- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;

// Can be overridden by subclasses to customize the secondary title, e.g. set a
// different style, or a UITextViewDelegate. The default implementation does
// nothing.
- (void)customizeSecondaryTitle:(UITextView*)secondaryTitle;

// Can be overridden by subclasses to customize the subtitle, e.g. set a
// different style, or a UITextViewDelegate. The default implementation does
// nothing.
- (void)customizeSubtitle:(UITextView*)subtitle;

// Show or hide the gradient view depending on the state of the bottom sheet.
- (void)displayGradientView:(BOOL)shouldShow;

// Returns YES if the scroll view is scrolled to the bottom.
- (BOOL)isScrolledToBottom;

// Detent that attempts to fit the preferred height of the content. Detent may
// be inactive in some size classes, so it should be used together with at
// least one other detent.
- (UISheetPresentationControllerDetent*)
    preferredHeightDetent API_AVAILABLE(ios(16));

// Calculates the preferred height of the content.
- (CGFloat)preferredHeightForContent;

@end

#endif  // IOS_CHROME_COMMON_UI_CONFIRMATION_ALERT_CONFIRMATION_ALERT_VIEW_CONTROLLER_H_
