// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_YOUTUBE_INCOGNITO_COORDINATOR_YOUTUBE_INCOGNITO_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_YOUTUBE_INCOGNITO_COORDINATOR_YOUTUBE_INCOGNITO_COORDINATOR_DELEGATE_H_

@class YoutubeIncognitoCoordinator;

// Handles the stop of `YoutubeIncognitoCoordinator`.
@protocol YoutubeIncognitoCoordinatorDelegate <NSObject>

// Callback for the delegate to know it should call stop the youtube incognito
// coordinator.
- (void)shouldStopYoutubeIncognitoCoordinator:
    (YoutubeIncognitoCoordinator*)youtubeIncognitoCoordinator;

@end

#endif  // IOS_CHROME_BROWSER_YOUTUBE_INCOGNITO_COORDINATOR_YOUTUBE_INCOGNITO_COORDINATOR_DELEGATE_H_
