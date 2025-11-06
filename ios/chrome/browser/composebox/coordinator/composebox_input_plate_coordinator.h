// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_INPUT_PLATE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_INPUT_PLATE_COORDINATOR_H_

#import <PhotosUI/PhotosUI.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "ios/chrome/browser/composebox/ui/composebox_input_plate_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol ComposeboxAnimationContextProvider;
@class ComposeboxInputPlateViewController;
enum class ComposeboxEntrypoint;
@protocol ComposeboxURLLoader;
class Browser;
@protocol OmniboxPopupPresenterDelegate;

/// ComposeboxInputPlateCoordinator presents AIM composebox.
@interface ComposeboxInputPlateCoordinator : ChromeCoordinator

// The context provider for the animations.
@property(nonatomic, readonly) id<ComposeboxAnimationContextProvider>
    contextProvider;

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
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_INPUT_PLATE_COORDINATOR_H_
