// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_INPUT_PLATE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_INPUT_PLATE_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/public/commands/composebox_tab_picker_commands.h"

@class ComposeboxInputPlateViewController;
enum class ComposeboxEntrypoint;
@class ComposeboxModeHolder;
@class ComposeboxTheme;
@protocol ComposeboxURLLoader;
class Browser;
@protocol OmniboxPopupPresenterDelegate;

// The coordinator for the compose box input plate.
@interface ComposeboxInputPlateCoordinator
    : ChromeCoordinator <ComposeboxTabPickerCommands>

// The view controller managed by this coordinator.
@property(nonatomic, readonly)
    ComposeboxInputPlateViewController* inputViewController;

// Delegate for positioning the omnibox popup.
@property(nonatomic, weak) id<OmniboxPopupPresenterDelegate>
    omniboxPopupPresenterDelegate;

/// Init the composebox opened from `entrypoint` with an optional `query` in
/// the omnibox.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                entrypoint:(ComposeboxEntrypoint)entrypoint
                                     query:(NSString*)query
                                 URLLoader:(id<ComposeboxURLLoader>)URLLoader
                                     theme:(ComposeboxTheme*)theme
                                modeHolder:(ComposeboxModeHolder*)modeHolder
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_INPUT_PLATE_COORDINATOR_H_
