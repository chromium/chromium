// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/user_account_image_update_delegate.h"
#import "ios/chrome/browser/ui/ntp/logo_animation_controller.h"

@protocol ApplicationCommands;
@protocol BrowserCoordinatorCommands;
@protocol ContentSuggestionsCommands;
@protocol ContentSuggestionsHeaderCommands;
@protocol ContentSuggestionsHeaderViewControllerDelegate;
@protocol FakeboxFocuser;
@protocol NewTabPageControllerDelegate;
@protocol OmniboxCommands;
@protocol LensCommands;
@class LayoutGuideCenter;
@class PrimaryToolbarViewController;

// Controller for the header containing the logo and the fake omnibox, handling
// the interactions between the header and the collection, and the rest of the
// application.
@interface ContentSuggestionsHeaderViewController
    : UIViewController <ContentSuggestionsHeaderConsumer,
                        LogoAnimationControllerOwnerOwner,
                        UserAccountImageUpdateDelegate>

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@property(nonatomic, weak) id<ApplicationCommands,
                              BrowserCoordinatorCommands,
                              OmniboxCommands,
                              FakeboxFocuser,
                              LensCommands>
    dispatcher;
@property(nonatomic, weak) id<ContentSuggestionsHeaderViewControllerDelegate>
    delegate;
@property(nonatomic, weak) id<ContentSuggestionsHeaderCommands> commandHandler;
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

// Update any dynamic constraints.
- (void)updateConstraints;

// Identity disc shown in this ViewController.
// TODO(crbug.com/1170995): Remove once the Feed header properly supports
// ContentSuggestions.
@property(nonatomic, strong, readonly) UIButton* identityDiscButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_H_
