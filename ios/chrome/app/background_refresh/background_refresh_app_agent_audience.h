// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_BACKGROUND_REFRESH_BACKGROUND_REFRESH_APP_AGENT_AUDIENCE_H_
#define IOS_CHROME_APP_BACKGROUND_REFRESH_BACKGROUND_REFRESH_APP_AGENT_AUDIENCE_H_

// An object conforming to this protocol may be assigned to the
// BackgroundRefreshDelegate to be informed of background processing events.
@protocol BackgroundRefreshAudience <NSObject>

// Tells the audience that background processing has started and any due
// providers will be called.
- (void)backgroundRefreshDidStart;

// Tells that audience that background refresh processing has ended, successful
// or not.
- (void)backgroundRefreshDidEnd;

@end

#endif  // IOS_CHROME_APP_BACKGROUND_REFRESH_BACKGROUND_REFRESH_APP_AGENT_AUDIENCE_H_
