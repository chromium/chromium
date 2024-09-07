// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_ENTRYPOINT_VIEW_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_ENTRYPOINT_VIEW_H_

#import <UIKit/UIKit.h>

namespace LensOverlay {

// Returns the location bar lens overlay entrypoint UIButton.
UIButton* NewEntrypointButton();

}  // namespace LensOverlay

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_ENTRYPOINT_VIEW_H_
