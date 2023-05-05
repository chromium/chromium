// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_WRANGLED_BROWSER_H_
#define IOS_CHROME_BROWSER_UI_MAIN_WRANGLED_BROWSER_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"

@class BrowserCoordinator;
@class BrowserViewController;
class ChromeBrowserState;
@protocol SyncPresenter;

// Implementation of BrowserProvider -- for the most part a wrapper around
// BrowserCoordinator. This is a Wrangler (a class in need of further
// refactoring) and should *not* be used outside of SceneController.
@interface WrangledBrowser : NSObject <BrowserProvider>

- (instancetype)initWithCoordinator:(BrowserCoordinator*)coordinator;

// The view controller showing the current tab for this interface. This property
// should be used wherever possible instead of the `bvc` property.
@property(nonatomic, readonly) UIViewController* viewController;
// The BrowserViewController showing the current tab. The API surface this
// property exposes will be refactored so that the BVC class isn't exposed.
@property(nonatomic, readonly) BrowserViewController* bvc;
@property(nonatomic, readonly) id<SyncPresenter> syncPresenter;
// The active browser. This can never be nullptr.
@property(nonatomic, readonly) Browser* browser;
// The inactive browser. This can be nullptr if in an incognito interface or if
// Inactive Tabs is disabled.
@property(nonatomic) Browser* inactiveBrowser;
// The browser state for this interface. This can never be nullptr.
@property(nonatomic, readonly) ChromeBrowserState* browserState;
// YES if the tab view is available for user interaction.
@property(nonatomic) BOOL userInteractionEnabled;
// YES if this interface is incognito.
@property(nonatomic, readonly) BOOL incognito;
// YES if TTS audio is playing.
@property(nonatomic, readonly) BOOL playingTTS;

// Sets the interface as "primary".
- (void)setPrimary:(BOOL)primary;

// Asks the implementor to clear any presented state, dismissing the omnibox if
// `dismissOmnibox` is YES, and calling `completion` once any animations are
// complete.
- (void)clearPresentedStateWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_WRANGLED_BROWSER_H_
