// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BLOCKING_OVERLAY_UI_BUNDLED_BLOCKING_OVERLAY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_BLOCKING_OVERLAY_UI_BUNDLED_BLOCKING_OVERLAY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol BlockingSceneCommands;

// This view controller presents an overlay UI that obscures all contents of the
// screen and instruct the user to finish a dialog in another window.
@interface BlockingOverlayViewController : UIViewController

// Handler for blocking scene commands.
@property(nonatomic, weak) id<BlockingSceneCommands>
    blockingSceneCommandHandler;

@end

#endif  // IOS_CHROME_BROWSER_BLOCKING_OVERLAY_UI_BUNDLED_BLOCKING_OVERLAY_VIEW_CONTROLLER_H_
