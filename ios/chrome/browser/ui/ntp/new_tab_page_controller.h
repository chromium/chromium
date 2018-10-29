// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_CONTROLLER_H_

#import <UIKit/UIKit.h>
#include <string>

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/native_content_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_owning.h"
#import "ios/public/provider/chrome/browser/voice/logo_animation_controller.h"

namespace ios {
class ChromeBrowserState;
}

@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol CRWSwipeRecognizerProvider;
@class GoogleLandingViewController;
@protocol NewTabPageControllerDelegate;
@protocol OmniboxFocuser;
@protocol FakeboxFocuser;
@protocol SnackbarCommands;
@class TabModel;
@protocol UrlLoader;

// A controller for the New Tab Page user interface. Supports content
// suggestions and incognito, each with its own controller.
//
// The currently visible CRWNativeContent is accessible through
// |currentController_|.
//
@interface NewTabPageController
    : NativeContentController<NewTabPageOwning,
                              LogoAnimationControllerOwnerOwner,
                              UIGestureRecognizerDelegate,
                              UIScrollViewDelegate>

@property(nonatomic, weak) id<CRWSwipeRecognizerProvider>
    swipeRecognizerProvider;

// Exposes content inset of contentSuggestions collectionView to ensure all of
// content is visible under the bottom toolbar.
@property(nonatomic) UIEdgeInsets contentInset;

// Init with the given url (presumably "chrome://newtab") and loader object.
// |loader| may be nil, but isn't retained so it must outlive this controller.
- (id)initWithUrl:(const GURL&)url
                  loader:(id<UrlLoader>)loader
                 focuser:(id<OmniboxFocuser>)focuser
            browserState:(ios::ChromeBrowserState*)browserState
         toolbarDelegate:(id<NewTabPageControllerDelegate>)toolbarDelegate
                tabModel:(TabModel*)tabModel
    parentViewController:(UIViewController*)parentViewController
              dispatcher:(id<ApplicationCommands,
                             BrowserCommands,
                             OmniboxFocuser,
                             FakeboxFocuser,
                             SnackbarCommands,
                             UrlLoader>)dispatcher
           safeAreaInset:(UIEdgeInsets)safeAreaInset;

// Animates the NTP fakebox to the focused position and focuses the real
// omnibox.
- (void)focusFakebox;

@end

#pragma mark - Testing

@interface NewTabPageController (TestSupport)
- (id<CRWNativeContent>)currentController;
- (id<CRWNativeContent>)incognitoController;
@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_CONTROLLER_H_
