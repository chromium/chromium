// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_ERROR_HANDLER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_ERROR_HANDLER_H_

/// Commands for showing errors in lens overlay.
@protocol LensOverlayErrorHandler <NSObject>

/// Show an alert explaining to the user that there is a problem with the
/// internet connection at the moment.
- (void)showNoInternetAlert;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_ERROR_HANDLER_H_
