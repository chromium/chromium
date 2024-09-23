// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol LensOverlayCommands;

/// The top level view controller for lens overlay.
/// Contains or presents the other view controllers.
/// Displays top-level chrome like close button.
@interface LensOverlayContainerViewController : UIViewController

- (instancetype)initWithLensOverlayCommandsHandler:
    (id<LensOverlayCommands>)handler NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// The selection view controller contained by this view controller.
/// Currently should be set by `viewDidLoad` and only set once.
@property(nonatomic, strong) UIViewController* selectionViewController;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_CONTAINER_VIEW_CONTROLLER_H_
