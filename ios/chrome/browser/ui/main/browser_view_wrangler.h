// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_BROWSER_VIEW_WRANGLER_H_
#define IOS_CHROME_BROWSER_UI_MAIN_BROWSER_VIEW_WRANGLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/app/application_mode.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

@protocol ApplicationCommands;
@protocol SettingsCommands;
class Browser;
@class SceneState;
@class WrangledBrowser;

// Wrangler (a class in need of further refactoring) for handling the creation
// and ownership of Browser instances and their associated
// BrowserViewControllers.
@interface BrowserViewWrangler : NSObject <BrowserProviderInterface>

// Initialize a new instance of this class using `profile` as the primary
// browser state for the tab models and BVCs. The Browser objects are created
// during the initialization (the OTR Browser may be destroyed and recreated
// during the application lifetime).
//
// `sceneState` is the scene state that will be associated with any Browsers
// created.
// `applicationEndpoint`, `settingsEndpoint, and `browsingDataEndpoint` are the
// objects that methods in the respective command protocols should be
// dispatched to.
- (instancetype)initWithProfile:(ProfileIOS*)profile
                     sceneState:(SceneState*)sceneState
            applicationEndpoint:(id<ApplicationCommands>)applicationEndpoint
               settingsEndpoint:(id<SettingsCommands>)settingsEndpoint
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// One interface must be designated as being the "current" interface.
// It's typically an error to assign this an interface which is neither of
// mainInterface` or `incognitoInterface`. The initial value of
// `currentInterface` is an implementation decision, but `mainInterface` is
// typical.
// Changing this value may or may not trigger actual UI changes, or may just be
// bookkeeping associated with UI changes handled elsewhere.
@property(nonatomic, weak) WrangledBrowser* currentInterface;
// The "main" (meaning non-incognito -- the nomenclature is legacy) interface.
// This interface's `incognito` property is expected to be NO.
@property(nonatomic, readonly) WrangledBrowser* mainInterface;
// The incognito interface. Its `incognito` property must be YES.
@property(nonatomic, readonly) WrangledBrowser* incognitoInterface;

// Creates the main interface; until this
// method is called, the main and incognito interfaces will be nil. This should
// be done before the main interface is accessed, usually immediately after
// initialization.
- (void)createMainCoordinatorAndInterface;

// Requests the session to be loaded for all Browsers. Needs to be called
// after -createMainCoordinatorAndInterface.
- (void)loadSession;

// Tells the receiver to clean up all the state that is tied to the incognito
// profile. This method should be called before the incognito profile
// is destroyed.
- (void)willDestroyIncognitoProfile;

// Tells the receiver to create all state that is tied to the incognito
// profile. This method should be called after the incognito profile
// has been created.
- (void)incognitoProfileCreated;

// Tells the receiver to clean up prior to deallocation. It is an error for an
// instance of this class to deallocate without a call to this method first.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_BROWSER_VIEW_WRANGLER_H_
