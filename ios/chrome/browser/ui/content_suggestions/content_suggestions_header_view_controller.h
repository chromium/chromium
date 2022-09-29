// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_controlling.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_consumer.h"
#import "ios/chrome/browser/ui/ntp/logo_animation_controller.h"

@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol ContentSuggestionsCollectionSynchronizing;
@protocol ContentSuggestionsCommands;
@protocol ContentSuggestionsHeaderCommands;
@protocol ContentSuggestionsHeaderViewControllerDelegate;
@protocol FakeboxFocuser;
@protocol NewTabPageControllerDelegate;
@protocol OmniboxCommands;
@protocol LensCommands;
@class LayoutGuideCenter;
@class PrimaryToolbarViewController;
class ReadingListModel;

// Controller for the header containing the logo and the fake omnibox, handling
// the interactions between the header and the collection, and the rest of the
// application.
@interface ContentSuggestionsHeaderViewController
    : UIViewController <ContentSuggestionsHeaderControlling,
                        NTPHomeConsumer,
                        LogoAnimationControllerOwnerOwner>

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@property(nonatomic, weak) id<ApplicationCommands,
                              BrowserCommands,
                              OmniboxCommands,
                              FakeboxFocuser,
                              LensCommands>
    dispatcher;
@property(nonatomic, weak) id<ContentSuggestionsHeaderViewControllerDelegate>
    delegate;
@property(nonatomic, weak) id<ContentSuggestionsHeaderCommands> commandHandler;
@property(nonatomic, assign) ReadingListModel* readingListModel;
@property(nonatomic, weak) id<NewTabPageControllerDelegate> toolbarDelegate;

// Whether the Google logo or doodle is being shown.
@property(nonatomic, assign) BOOL logoIsShowing;

// `YES` if the omnibox should be focused on when the view appears for voice
// over.
@property(nonatomic, assign) BOOL focusOmniboxWhenViewAppears;

// `YES` if Google is the default search engine.
@property(nonatomic, assign) BOOL isGoogleDefaultSearchEngine;

// `YES` if the Start Surface is currently being shown.
@property(nonatomic, assign) BOOL isStartShowing;

// The base view controller from which to present UI.
@property(nonatomic, weak) UIViewController* baseViewController;

// The layout guide center for the current scene.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

// Return the toolbar view;
- (UIView*)toolBarView;

// Animates the NTP fakebox to the focused position and focuses the real
// omnibox.
- (void)focusFakebox;

// Sends notification to focus the accessibility of the omnibox.
- (void)focusAccessibilityOnOmnibox;

// Returns the height of the entire header.
- (CGFloat)headerHeight;

// Identity disc shown in this ViewController.
// TODO(crbug.com/1170995): Remove once the Feed header properly supports
// ContentSuggestions.
@property(nonatomic, strong, readonly) UIButton* identityDiscButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_H_
