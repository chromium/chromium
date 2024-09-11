// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/user_account_image_update_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_consumer.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view_controller_delegate.h"

@protocol ApplicationCommands;
@protocol BrowserCoordinatorCommands;
@protocol FakeboxFocuser;
@protocol HomeCustomizationDelegate;
@protocol NewTabPageControllerDelegate;
@protocol NewTabPageHeaderCommands;
@class NewTabPageMetricsRecorder;
@protocol OmniboxCommands;
@protocol LensCommands;
@class LayoutGuideCenter;
@class PrimaryToolbarViewController;
@class TabGroupIndicatorView;

// Controller for the header containing the logo and the fake omnibox, handling
// the interactions between the header and the collection, and the rest of the
// application.
@interface NewTabPageHeaderViewController
    : UIViewController <NewTabPageHeaderConsumer,
                        UserAccountImageUpdateDelegate>

- (instancetype)initWithUseNewBadgeForLensButton:(BOOL)useNewBadgeForLensButton
                 useNewBadgeForCustomizationMenu:
                     (BOOL)useNewBadgeForCustomizationMenu
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@property(nonatomic, weak) id<ApplicationCommands,
                              BrowserCoordinatorCommands,
                              OmniboxCommands,
                              FakeboxFocuser,
                              LensCommands>
    dispatcher;
@property(nonatomic, weak) id<NewTabPageHeaderViewControllerDelegate> delegate;
@property(nonatomic, weak) id<NewTabPageHeaderCommands> commandHandler;
@property(nonatomic, weak) id<NewTabPageControllerDelegate> toolbarDelegate;

// `YES` if Google is the default search engine.
@property(nonatomic, assign) BOOL isGoogleDefaultSearchEngine;

// `YES` if its view is visible.  When set to `NO` various UI updates are
// ignored.
@property(nonatomic, assign, getter=isShowing) BOOL showing;

// The base view controller from which to present UI.
@property(nonatomic, weak) UIViewController* baseViewController;

// The layout guide center for the current scene.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

// Metrics recorder for the new tab page.
@property(nonatomic, weak) NewTabPageMetricsRecorder* NTPMetricsRecorder;

// Identity disc shown in this ViewController.
// TODO(crbug.com/40165977): Remove once the Feed header properly supports
// ContentSuggestions.
@property(nonatomic, strong, readonly) UIButton* identityDiscButton;

// Customization menu button shown in this ViewController.
// TODO(crbug.com/40165977): Remove once the Feed header properly supports
// ContentSuggestions.
@property(nonatomic, readonly) UIButton* customizationMenuButton;

// Should be set to YES if an animation will run that requires animating the
// font scale, for example, during a fakebox defocus animation.
@property(nonatomic, assign) BOOL allowFontScaleAnimation;

// The delegate for the Home Customization menu.
@property(nonatomic, weak) id<HomeCustomizationDelegate> customizationDelegate;

// Animation to expand this header in response to focusing the omnibox to match
// the fake omnibox with the omnibox's.
- (void)expandHeaderForFocus;

// Configure the header after the focus omnibox animation has completed.
// `finalPosition` is important since the animation could be cancelled before
// completion.
- (void)completeHeaderFakeOmniboxFocusAnimationWithFinalPosition:
    (UIViewAnimatingPosition)finalPosition;

// Updates the iPhone fakebox's frame based on the current scroll view `offset`
// and `width`. `width` is the width of the screen, including the space outside
// the safe area. The `safeAreaInsets` is relative to the view used to calculate
// the `width`.
- (void)updateFakeOmniboxForOffset:(CGFloat)offset
                       screenWidth:(CGFloat)screenWidth
                    safeAreaInsets:(UIEdgeInsets)safeAreaInsets
            animateScrollAnimation:(BOOL)animateScrollAnimation;

// Updates the fakeomnibox's width in order to be adapted to the new `width`,
// without taking the y-position into account.
- (void)updateFakeOmniboxForWidth:(CGFloat)width;

// Returns the Y value to use for the scroll view's contentOffset when scrolling
// the omnibox to the top of the screen.
- (CGFloat)pinnedOffsetY;

// Return the toolbar view;
- (UIView*)toolBarView;

// Sends notification to focus the accessibility of the omnibox.
- (void)focusAccessibilityOnOmnibox;

// Calls layoutIfNeeded on the header.
- (void)layoutHeader;

// Returns the height of the entire header.
- (CGFloat)headerHeight;

// Shows the fakebox.
- (void)omniboxDidResignFirstResponder;

// Hides the new feature badge on the Home customization menu's entrypoint.
- (void)hideBadgeOnCustomizationMenu;

// Sets the tabgroupIndicatorView.
- (void)setTabGroupIndicatorView:(TabGroupIndicatorView*)view;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_VIEW_CONTROLLER_H_
