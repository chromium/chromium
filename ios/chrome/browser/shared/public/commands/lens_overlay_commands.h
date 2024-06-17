// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_LENS_OVERLAY_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_LENS_OVERLAY_COMMANDS_H_

/// Commands related to Lens Overlay.
@protocol LensOverlayCommands

/// Creates a new Lens UI. Automatically destroys any existing Lens UI as only
/// one instance of it per BVC is supported.
- (void)createAndShowLensUI:(BOOL)animated;

/// Display the lens overlay, if it exists.
- (void)showLensUI:(BOOL)animated;

/// Hide lens overlay if it exists.
- (void)hideLensUI:(BOOL)animated;

/// Destroy lens overlay (called e.g. in response to memory pressure).
- (void)destroyLensUI:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_LENS_OVERLAY_COMMANDS_H_
