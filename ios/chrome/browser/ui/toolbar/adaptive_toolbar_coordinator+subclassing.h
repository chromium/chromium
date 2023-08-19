// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_COORDINATOR_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_COORDINATOR_SUBCLASSING_H_

#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator.h"

#import "ios/chrome/browser/ui/toolbar/public/toolbar_type.h"

@class ToolbarButtonFactory;
namespace web {
class WebState;
}

// Protected interface of the AdaptiveToolbarCoordinator.
@interface AdaptiveToolbarCoordinator (Subclassing)

// Returns a button factory
- (ToolbarButtonFactory*)buttonFactoryWithType:(ToolbarType)type;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_COORDINATOR_SUBCLASSING_H_
