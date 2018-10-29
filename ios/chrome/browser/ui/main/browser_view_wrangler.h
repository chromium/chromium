// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_BROWSER_VIEW_WRANGLER_H_
#define IOS_CHROME_BROWSER_UI_MAIN_BROWSER_VIEW_WRANGLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/main/browser_view_information.h"

@protocol ApplicationCommands;
@class DeviceSharingManager;
@protocol TabModelObserver;

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
@interface BrowserViewWrangler : NSObject<BrowserViewInformation>

// Initialize a new instance of this class using |browserState| as the primary
// browser state for the tab models and BVCs, and setting |tabModelObserver|, if
// not nil, as the tab model delegate for any tab models that are created.
// |applicationCommandEndpoint| is the object that methods in the
// ApplicationCommands protocol should be dispatched to by any BVCs that are
// created.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                    tabModelObserver:(id<TabModelObserver>)tabModelObserver
          applicationCommandEndpoint:
              (id<ApplicationCommands>)applicationCommandEndpoint
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Set the current BVC to be |bvc|, and use |storageSwitcher| to handle the
// storage switch. |bvc| should be one of the view controller instances already
// owned by the receiver (either |mainBVC| or |otrBVBC|), and this method does
// not retain or take ownership of |bvc|.
// Note that the BrowserViewInformation protocol defines
// |currentBVC| as a readwrite property, so users of this class can directly
// call -setCurrentBVC: and bypass the logic in this method; that should only be
// done on BVC instances who do not yet have a browser state.
- (void)setCurrentBVC:(BrowserViewController*)bvc
      storageSwitcher:(id<BrowserStateStorageSwitching>)storageSwitcher;

// Update the device sharing manager. This should be done after updates to the
// tab model. This class creates and manages the state of the sharing manager.
- (void)updateDeviceSharingManager;

// Destroy and rebuild the incognito tab model.
- (void)destroyAndRebuildIncognitoTabModel;

// Called before the instance is deallocated.
- (void)shutdown;

@end

@interface BrowserViewWrangler (Testing)
@property(nonatomic, readonly) DeviceSharingManager* deviceSharingManager;
@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_BROWSER_VIEW_WRANGLER_H_
