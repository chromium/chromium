// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_controlling.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_provider.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_consumer.h"
#import "ios/public/provider/chrome/browser/voice/logo_animation_controller.h"

@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol ContentSuggestionsCollectionSynchronizing;
@protocol ContentSuggestionsCommands;
@protocol ContentSuggestionsHeaderViewControllerDelegate;
@protocol FakeboxFocuser;
@protocol NewTabPageControllerDelegate;
@protocol OmniboxCommands;
@class PrimaryToolbarViewController;
class ReadingListModel;

// Controller for the header containing the logo and the fake omnibox, handling
// the interactions between the header and the collection, and the rest of the
// application.
@interface ContentSuggestionsHeaderViewController
    : UIViewController<ContentSuggestionsHeaderControlling,
                       ContentSuggestionsHeaderProvider,
                       NTPHomeConsumer,
                       LogoAnimationControllerOwnerOwner>

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@property(nonatomic, weak)
    id<ApplicationCommands, BrowserCommands, OmniboxCommands, FakeboxFocuser>
        dispatcher;
@property(nonatomic, weak) id<ContentSuggestionsHeaderViewControllerDelegate>
    delegate;
@property(nonatomic, weak) id<ContentSuggestionsCommands> commandHandler;
@property(nonatomic, assign) ReadingListModel* readingListModel;
@property(nonatomic, weak) id<NewTabPageControllerDelegate> toolbarDelegate;

// Whether the Google logo or doodle is being shown.
@property(nonatomic, assign) BOOL logoIsShowing;

// |YES| if a what's new promo can be displayed.
@property(nonatomic, assign) BOOL promoCanShow;

// Return the toolbar view;
- (UIView*)toolBarView;

// Animates the NTP fakebox to the focused position and focuses the real
// omnibox.
- (void)focusFakebox;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_H_
