// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_FULLSCREEN_TOOLBAR_UI_BROADCASTING_UTIL_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_FULLSCREEN_TOOLBAR_UI_BROADCASTING_UTIL_H_

@class ChromeBroadcaster;
@protocol ToolbarUI;

// Starts broadcasting `toolbar`'s UI state using `broadcaster`.
void StartBroadcastingToolbarUI(id<ToolbarUI> toolbar,
                                ChromeBroadcaster* broadcaster);

// Stops broadcasting MainContentUI properties using `broadcaster`.
void StopBroadcastingToolbarUI(ChromeBroadcaster* broadcaster);

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_FULLSCREEN_TOOLBAR_UI_BROADCASTING_UTIL_H_
