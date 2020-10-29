// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_LOCATION_PERMISSIONS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_LOCATION_PERMISSIONS_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

#import "ios/chrome/browser/ui/first_run/location_permissions_commands.h"

// Coordinator that manages a LocationPermissionsViewController.
@interface LocationPermissionsCoordinator : ChromeCoordinator
// Handler for all actions of this coordinator.
@property(nonatomic, weak) id<LocationPermissionsCommands> handler;
@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_LOCATION_PERMISSIONS_COORDINATOR_H_
