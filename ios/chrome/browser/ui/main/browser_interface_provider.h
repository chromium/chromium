// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_BROWSER_INTERFACE_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_MAIN_BROWSER_INTERFACE_PROVIDER_H_

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"

class Browser;
@class BrowserCoordinator;
@class BrowserViewController;
class ChromeBrowserState;
@protocol SyncPresenter;

// A BrowserInterface is an abstraction that exposes an interface to the Chrome
// user interface (and related model objects) to the application layer. Each
// BrowserInterface is roughly equivalent to a window on a desktop browser --
// a collection of tabs (a tab model) associated with a user profile (the
// browser state) with the UI of the currently visible tab (the view controller)
// and some other attributes as well.
// TODO(crbug.com/914306): The long-term goal is to reduce the size of this
// interface; this protocol allows for easy encapsulation of that process.
// For legacy reasons, the primary UI entry point for an interface is a visible
// tab.
@protocol BrowserInterface

// The view controller showing the current tab for this interface. This property
// should be used wherever possible instead of the `bvc` property.
@property(nonatomic, readonly) UIViewController* viewController;
// The BrowserViewController showing the current tab. The API surface this
// property exposes will be refactored so that the BVC class isn't exposed.
@property(nonatomic, readonly) BrowserViewController* bvc;
@property(nonatomic, readonly) id<SyncPresenter> syncPresenter;
// The active browser. This can never be nullptr.
@property(nonatomic, readonly) Browser* browser;
// The browser state for this interface. This can never be nullptr.
@property(nonatomic, readonly) ChromeBrowserState* browserState;
// YES if the tab view is available for user interaction.
@property(nonatomic) BOOL userInteractionEnabled;
// YES if this interface is incognito.
@property(nonatomic, readonly) BOOL incognito;

// Sets the interface as "primary".
- (void)setPrimary:(BOOL)primary;

// Asks the implementor to clear any presented state, dismissing the omnibox if
// `dismissOmnibox` is YES, and calling `completion` once any animations are
// complete.
- (void)clearPresentedStateWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox;

@end

// A BrowserInterfaceProvider is an abstraction that exposes the available
// interfaces for the Chrome UI.
@protocol BrowserInterfaceProvider

// One interface must be designated as being the "current" interface.
// It's typically an error to assign this an interface which is neither of
// mainInterface` or `incognitoInterface`. The initial value of
// `currentInterface` is an implementation decision, but `mainInterface` is
// typical.
// Changing this value may or may not trigger actual UI changes, or may just be
// bookkeeping associated with UI changes handled elsewhere.
@property(nonatomic, weak) id<BrowserInterface> currentInterface;
// The "main" (meaning non-incognito -- the nomenclature is legacy) interface.
// This interface's `incognito` property is expected to be NO.
@property(nonatomic, readonly) id<BrowserInterface> mainInterface;
// The incognito interface. Its `incognito` property must be YES.
@property(nonatomic, readonly) id<BrowserInterface> incognitoInterface;
// YES iff `incognitoInterface` is already created.
@property(nonatomic, assign, readonly) BOOL hasIncognitoInterface;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_BROWSER_INTERFACE_PROVIDER_H_
