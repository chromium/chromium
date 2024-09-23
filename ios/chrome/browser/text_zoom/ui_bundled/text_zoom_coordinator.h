// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TEXT_ZOOM_UI_BUNDLED_TEXT_ZOOM_COORDINATOR_H_
#define IOS_CHROME_BROWSER_TEXT_ZOOM_UI_BUNDLED_TEXT_ZOOM_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class TextZoomViewController;
@class ToolbarAccessoryPresenter;
@protocol ToolbarAccessoryCoordinatorDelegate;

// Coordinator for the UI of the text zoom feature, which allows adjusting the
// zoom level of the text of a webpage.
@interface TextZoomCoordinator : ChromeCoordinator

// Presenter used by this coordinator to present itself.
@property(nonatomic, strong) ToolbarAccessoryPresenter* presenter;

// Delegate to inform when this coordinator's UI is dismissed.
@property(nonatomic, weak) id<ToolbarAccessoryCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_TEXT_ZOOM_UI_BUNDLED_TEXT_ZOOM_COORDINATOR_H_
