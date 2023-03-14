// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TEXT_ZOOM_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TEXT_ZOOM_COMMANDS_H_

@protocol TextZoomCommands <NSObject>

// Sets Text Zoom to active and shows the Text Zoom UI.
- (void)openTextZoom;

// Dismisses the Text Zoom UI and deactivates Text Zoom.
- (void)closeTextZoom;

// Shows the Text Zoom UI if Text Zoom is active.
- (void)showTextZoomUIIfActive;

// Hides the Text Zoom UI but does not deactivate Text Zoom.
- (void)hideTextZoomUI;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TEXT_ZOOM_COMMANDS_H_
