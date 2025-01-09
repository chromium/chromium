// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_YOUTUBE_INCOGNITO_COORDINATOR_YOUTUBE_INCOGNITO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_YOUTUBE_INCOGNITO_COORDINATOR_YOUTUBE_INCOGNITO_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"

@protocol YoutubeIncognitoCoordinatorDelegate;
@protocol TabOpening;

// This coordinator presents the youtube incognito interstitial sheet.
@interface YoutubeIncognitoCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<YoutubeIncognitoCoordinatorDelegate> delegate;

// Tab opener to be used to open a new tab.
@property(nonatomic, weak) id<TabOpening> tabOpener;

// URL load parameters associated with the external intent.
@property(nonatomic, assign) UrlLoadParams urlLoadParams;

@property(nonatomic, assign) BOOL incognitoDisabled;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_YOUTUBE_INCOGNITO_COORDINATOR_YOUTUBE_INCOGNITO_COORDINATOR_H_
