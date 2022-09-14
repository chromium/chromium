// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_CONTENT_MAIN_CONTENT_UI_BROADCASTING_UTIL_H_
#define IOS_CHROME_BROWSER_UI_MAIN_CONTENT_MAIN_CONTENT_UI_BROADCASTING_UTIL_H_

@class ChromeBroadcaster;
@protocol MainContentUI;

// Starts broadcasting `main_content`'s UI state using `broadcaster`.
void StartBroadcastingMainContentUI(id<MainContentUI> main_content,
                                    ChromeBroadcaster* broadcaster);

// Stops broadcasting MainContentUI properties using `broadcaster`.
void StopBroadcastingMainContentUI(ChromeBroadcaster* broadcaster);

#endif  // IOS_CHROME_BROWSER_UI_MAIN_CONTENT_MAIN_CONTENT_UI_BROADCASTING_UTIL_H_
