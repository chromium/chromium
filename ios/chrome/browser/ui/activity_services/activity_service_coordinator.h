// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SERVICE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SERVICE_COORDINATOR_H_

#import "ios/chrome/browser/ui/activity_services/activity_scenario.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@class ActivityParams;
@protocol ActivityServicePositioner;
@protocol ActivityServicePresentation;
@protocol BookmarksCommands;
class Browser;
@protocol QRGenerationCommands;

// ActivityServiceCoordinator provides a public interface for the share
// menu feature.
@interface ActivityServiceCoordinator : ChromeCoordinator

// Initializes a coordinator instance configured to share the current tab's URL
// based on `baseViewController` and `browser`, and where `params` contains all
// necessary values to drive the scenario.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                    params:(ActivityParams*)params
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Provider of the share action location.
@property(nonatomic, readwrite, weak) id<ActivityServicePositioner>
    positionProvider;

// Provider of share action presentation.
@property(nonatomic, readwrite, weak) id<ActivityServicePresentation>
    presentationProvider;

// Handler for activities that need to be executed within a certain scope.
@property(nonatomic, readwrite, weak) id<QRGenerationCommands> scopedHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SERVICE_COORDINATOR_H_
