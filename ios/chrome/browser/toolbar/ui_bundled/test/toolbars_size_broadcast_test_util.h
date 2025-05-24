// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TEST_TOOLBARS_SIZE_BROADCAST_TEST_UTIL_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TEST_TOOLBARS_SIZE_BROADCAST_TEST_UTIL_H_

@class ChromeBroadcaster;
@class ToolbarsSize;

// Checks whether `toolbars_size`'s broadcast properties are being broadcast
// through `broadcaster`. Verifies broadcast setup according to
// `should_broadcast`.
void VerifyToolbarsSizeBroadcast(ToolbarsSize* toolbars_size,
                                 ChromeBroadcaster* broadcaster,
                                 bool should_broadcast);

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TEST_TOOLBARS_SIZE_BROADCAST_TEST_UTIL_H_
