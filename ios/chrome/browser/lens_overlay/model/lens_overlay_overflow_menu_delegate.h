// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_OVERFLOW_MENU_DELEGATE_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_OVERFLOW_MENU_DELEGATE_H_

class GURL;

// The Lens overlay overflow menu delegate.
@protocol LensOverlayOverflowMenuDelegate

// Navigates to a given URL in the same tab with an animation.
- (void)openActionURL:(GURL)URL;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_OVERFLOW_MENU_DELEGATE_H_
