// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_FULLSCREEN_TOOLBARS_SIZE_BROADCASTING_UTIL_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_FULLSCREEN_TOOLBARS_SIZE_BROADCASTING_UTIL_H_

@class ChromeBroadcaster;
@class ToolbarsSize;

// Starts broadcasting toolbars size state using `broadcaster`.
void StartBroadcastingToolbarsSize(ToolbarsSize* toolbar,
                                   ChromeBroadcaster* broadcaster);

// Stops broadcasting toolbars size state using `broadcaster`.
void StopBroadcastingToolbarsSize(ChromeBroadcaster* broadcaster);

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_FULLSCREEN_TOOLBARS_SIZE_BROADCASTING_UTIL_H_
