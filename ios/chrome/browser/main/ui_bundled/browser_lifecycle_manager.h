// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_UI_BUNDLED_BROWSER_LIFECYCLE_MANAGER_H_
#define IOS_CHROME_BROWSER_MAIN_UI_BUNDLED_BROWSER_LIFECYCLE_MANAGER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"

class ProfileIOS;
@class SceneState;
@class WrangledBrowser;

// Manager for handling the creation and ownership of Browser instances and
// their associated coordinators.
//
// The Browsers are exposed through `BrowserProviderInterface` as soon as they
// are created, before their coordinators are started. This lets scene-level
// objects register on the Browsers' `CommandDispatcher` before any
// `BrowserCoordinator` resolves its handlers.
@interface BrowserLifecycleManager : NSObject <BrowserProviderInterface>

// Initialize a new instance of this class using `profile` as the primary
// browser state for the tab models and BVCs. The Browser objects are created
// during the initialization (the OTR Browser may be destroyed and recreated
// during the application lifetime).
//
// `sceneState` is the scene state that will be associated with any Browsers
// created.
- (instancetype)initWithProfile:(ProfileIOS*)profile
                     sceneState:(SceneState*)sceneState
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

// Creates the incognito Browser for the recreated incognito profile, without
// starting its coordinator. This method should be called after the incognito
// profile has been created, and must be followed by a call to
// `-createIncognitoInterface`. Between both calls, the caller can register
// command handlers on the new `incognitoBrowser`.
- (void)createIncognitoBrowser;

// Starts the coordinator of the incognito Browser created by
// `-createIncognitoBrowser` and creates the incognito interface.
- (void)createIncognitoInterface;

// Prepares command dispatchers across all browsers for shutdown. Must be
// called before UI coordinators stop so unregistering targets fail silently.
- (void)prepareForShutdown;

// Tells the receiver to clean up prior to deallocation. It is an error for an
// instance of this class to deallocate without a call to this method first.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_MAIN_UI_BUNDLED_BROWSER_LIFECYCLE_MANAGER_H_
