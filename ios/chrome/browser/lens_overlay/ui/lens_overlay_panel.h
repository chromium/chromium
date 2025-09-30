// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_PANEL_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_PANEL_H_

#import <UIKit/UIKit.h>

// A general purpose presentation panel for Lens Overlay.
// Can be used either as a side panel or a bottom sheet.
@interface LensOverlayPanel : UIViewController

// Creates a new instance wrapping the given content view controller.
- (instancetype)initWithContent:(UIViewController*)contentViewController
                   insetContent:(BOOL)insetContent;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_PANEL_H_
