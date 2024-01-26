// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_WEB_STATE_SNAPSHOT_INFO_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_WEB_STATE_SNAPSHOT_INFO_H_

#ifdef __cplusplus
#import "ios/web/public/web_state.h"
#endif

// A Objective-C wrapper to expose the information of WebState to make it usable
// from Swift.
@interface WebStateSnapshotInfo : NSObject

#ifdef __cplusplus
- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;
#endif
- (instancetype)init NS_UNAVAILABLE;

#ifdef __cplusplus
@property(nonatomic, readonly) web::WebState* webState;
#endif

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_WEB_STATE_SNAPSHOT_INFO_H_
