// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_WEB_STATE_SNAPSHOT_INFO_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_WEB_STATE_SNAPSHOT_INFO_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/net/model/crurl.h"

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

// Calls `WebState::TakeSnapshot()`.
// Takes a snapshot of web view with `rect`. `rect` should be in the WebState
// view's coordinate system. `callback` is always called, but the image can be
// nil.
- (void)takeSnapshot:(CGRect)rect callback:(void (^)(UIImage*))callback;

// Calls `WebState::CanTakeSnapshot()`. Returns whether or not a snapshot can be
// taken.
- (BOOL)canTakeSnapshot;

// Calls `WebState::IsWebUsageEnabled()`. Returns whether or not a web view is
// allowed to exist in the associated WebState.
- (BOOL)isWebUsageEnabled;

// Returns CrURL initialized from the result of
// `WebState::GetLastCommittedURL()`.
- (CrURL*)lastCommittedURL;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_WEB_STATE_SNAPSHOT_INFO_H_
