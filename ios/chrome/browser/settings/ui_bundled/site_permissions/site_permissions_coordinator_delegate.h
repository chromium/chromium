// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SITE_PERMISSIONS_SITE_PERMISSIONS_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SITE_PERMISSIONS_SITE_PERMISSIONS_COORDINATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

@class SitePermissionsCoordinator;

// Delegate protocol for SitePermissionsCoordinator events.
@protocol SitePermissionsCoordinatorDelegate <NSObject>

// Called when the SitePermissionsCoordinator was removed from the navigation
// stack.
- (void)sitePermissionsCoordinatorWasRemoved:
    (SitePermissionsCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SITE_PERMISSIONS_SITE_PERMISSIONS_COORDINATOR_DELEGATE_H_
