// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_TOOLBAR_CONSUMER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_TOOLBAR_CONSUMER_H_

/// Consumer of the lens toolbar in the bottom sheet.
@protocol LensToolbarConsumer

/// Update the UI for omnibox according to omnibox focus state.
- (void)setOmniboxFocused:(BOOL)isFocused;

/// Updates the back button availability.
- (void)setCanGoBack:(BOOL)canGoBack;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_TOOLBAR_CONSUMER_H_
