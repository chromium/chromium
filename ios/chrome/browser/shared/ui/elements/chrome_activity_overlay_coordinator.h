// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_CHROME_ACTIVITY_OVERLAY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_CHROME_ACTIVITY_OVERLAY_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator for displaying a UIActivityIndicatorView overlay over the current
// context.
@interface ChromeActivityOverlayCoordinator : ChromeCoordinator

// Text that will be shown above the UIActivityIndicatorView.
@property(nonatomic, copy) NSString* messageText;

// YES if the Coordinator is started. Meaning that the UIActivityIndicatorView
// is currently being displayed.
@property(nonatomic, assign) BOOL started;

// YES if the coordinator should trgger blocking UI in all other windows as
// long as it is active.
@property(nonatomic, assign) BOOL blockAllWindows;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_CHROME_ACTIVITY_OVERLAY_COORDINATOR_H_
