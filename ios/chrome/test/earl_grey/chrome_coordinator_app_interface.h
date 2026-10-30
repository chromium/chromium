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

#pragma mark - Dispatcher, State, and Lifecycle

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

// Returns YES if `selectorString` was previously dispatched and recorded.
+ (BOOL)selectorWasDispatched:(NSString*)selectorString;

// Dispatches `selectorString`. Must be a method that does not require params.
+ (void)dispatchSelector:(NSString*)selectorString;

// Calls `block` if the given `selectorString` is dispatched.
+ (void)setAction:(ProceduralBlock)block forSelector:(NSString*)selectorString;

// Ensures the browser is created. Used to initialize browser dependencies before
// starting the tested coordinator.
+ (void)startBrowser;

// Stops the currently started coordinator.
+ (void)stopCoordinator;

// Resets the isolated dispatcher and dismisses the blank rootViewController.
+ (void)reset;

#pragma mark - Feature Coordinators

// Starts the Bookmarks coordinator in isolation.
+ (void)startBookmarksCoordinator;

// Starts the Composebox coordinator in isolation.
+ (void)startComposeboxCoordinator;

// Starts the Enhanced Safe Browsing promo coordinator in isolation.
+ (void)startEnhancedSafeBrowsingPromoCoordinator;

// Starts the History coordinator in isolation.
+ (void)startHistoryCoordinator;

// Starts the Lens promo coordinator in isolation.
+ (void)startLensPromoCoordinator;

// Starts the New Tab Page coordinator in isolation.
+ (void)startNewTabPageCoordinator;

// Starts the Omnibox coordinator in isolation.
+ (void)startOmniboxCoordinator;

// Starts the Password Suggestion coordinator in isolation.
+ (void)startPasswordSuggestionCoordinator;

// Starts the Privacy Safe Browsing coordinator in isolation.
+ (void)startPrivacySafeBrowsingCoordinator;

// Starts the legacy QR Scanner coordinator in isolation.
+ (void)startQRScannerLegacyCoordinator;

// Starts the Reading List coordinator in isolation.
+ (void)startReadingListCoordinator;

// Starts the Search What You See promo coordinator in isolation.
+ (void)startSearchWhatYouSeePromoCoordinator;

// Starts the Snackbar coordinator in isolation.
+ (void)startSnackbarCoordinator;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_COORDINATOR_APP_INTERFACE_H_
