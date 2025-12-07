// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_COORDINATOR_APP_INTERFACE_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_COORDINATOR_APP_INTERFACE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

@class ChromeCoordinator;
@class CommandDispatcher;

// An app interface to allow starting and stopping coordinators in isolation
// from the rest of the app UI.
@interface ChromeCoordinatorAppInterface : NSObject

// The isolated dispatcher used when starting coordinators from this app
// interface.
@property(class, readonly) CommandDispatcher* dispatcher;

// The coordinator that was started, if any.
@property(class, readonly) ChromeCoordinator* coordinator;

// The last URL loaded with the URLLoadingBrowserAgent. Blank if none have been
// loaded.
@property(class, readonly) NSURL* lastURLLoaded;

// YES if the last URL loaded was in incognito.
@property(class, readonly) BOOL lastURLLoadedInIncognito;

// Returns YES if the selector was previously dispatched and recorded.
+ (BOOL)selectorWasDispatched:(NSString*)selectorString;

// Dispatches a selector. Must be a method that does not require params.
+ (void)dispatchSelector:(NSString*)selectorString;

// Calls the block if the given selector is dispatched.
+ (void)setAction:(ProceduralBlock)block forSelector:(NSString*)selectorString;

// Methods to start coordinators.
+ (void)startEnhancedSafeBrowsingPromoCoordinator;
+ (void)startLensPromoCoordinator;
+ (void)startHistoryCoordinator;
+ (void)startPopupMenuCoordinator;
+ (void)startOmniboxCoordinator;
+ (void)startSearchWhatYouSeePromoCoordinator;
+ (void)startSnackbarCoordinator;

// Stops the currently started coordinator.
+ (void)stopCoordinator;

// Resets the isolated dispatcher and dismisses the blank rootViewController.
+ (void)reset;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_COORDINATOR_APP_INTERFACE_H_
