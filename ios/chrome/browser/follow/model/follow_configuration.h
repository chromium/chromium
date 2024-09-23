// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_CONFIGURATION_H_

#import <Foundation/Foundation.h>

class DiscoverFeedService;

// Configuration object used by the FollowService.
@interface FollowConfiguration : NSObject

// DiscoverFeedService used by FollowService.
@property(nonatomic, assign) DiscoverFeedService* feedService;

@end

#endif  // IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_CONFIGURATION_H_
