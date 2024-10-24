// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_ENTRYPOINT_VIEW_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_ENTRYPOINT_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"

// The location bar lens overlay entrypoint UIButton.
@interface LensOverlayEntrypointButton : ExtendedTouchTargetButton

- (instancetype)init;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_ENTRYPOINT_VIEW_H_
