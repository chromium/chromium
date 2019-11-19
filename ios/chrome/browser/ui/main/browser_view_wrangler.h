// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_BROWSER_VIEW_WRANGLER_H_
#define IOS_CHROME_BROWSER_UI_MAIN_BROWSER_VIEW_WRANGLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/main/browser_interface_provider.h"

@protocol ApplicationCommands;
@class BrowserCoordinator;
@class DeviceSharingManager;
@protocol WebStateListObserving;

class AppUrlLoadingService;

namespace ios {
class ChromeBrowserState;
}

// Protocol for objects that can handle switching browser state storage.
@protocol BrowserStateStorageSwitching
- (void)changeStorageFromBrowserState:(ios::ChromeBrowserState*)oldState
                       toBrowserState:(ios::ChromeBrowserState*)newState;
@end

// Wrangler (a class in need of further refactoring) for handling the creation
// and ownership of BrowserViewController instances and their associated
// TabModels, and a few related methods.
@interface BrowserViewWrangler : NSObject <BrowserInterfaceProvider>

// Initialize a new instance of this class using |browserState| as the primary
// browser state for the tab models and BVCs, and setting
// |WebStateListObserving|, if not nil, as the webStateListObsever for any
// WebStateLists that are created. |applicationCommandEndpoint| is the object
// that methods in the ApplicationCommands protocol should be dispatched to by
// any BVCs that are created. |storageSwitcher| is used to manage changing any
// storage associated with the interfaces when the current interface changes;
// this is handled in the implementation of -setCurrentInterface:.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                webStateListObserver:(id<WebStateListObserving>)observer
          applicationCommandEndpoint:
              (id<ApplicationCommands>)applicationCommandEndpoint
                appURLLoadingService:(AppUrlLoadingService*)appURLLoadingService
                     storageSwitcher:
                         (id<BrowserStateStorageSwitching>)storageSwitcher
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Creates the main Browser used by the receiver, using the browser state
// and tab model observer it was configured with. The main interface is then
// created; until this method is called, the main and incognito interfaces will
// be nil. This should be done before the main interface is accessed, usually
// immediately after initialization.
- (void)createMainBrowser;

// Update the device sharing manager. This should be done after updates to the
// tab model. This class creates and manages the state of the sharing manager.
- (void)updateDeviceSharingManager;

// Destroy and rebuild the incognito Browser.
- (void)destroyAndRebuildIncognitoBrowser;

// Called before the instance is deallocated.
- (void)shutdown;

@end

@interface BrowserViewWrangler (Testing)
@property(nonatomic, readonly) DeviceSharingManager* deviceSharingManager;
@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_BROWSER_VIEW_WRANGLER_H_
