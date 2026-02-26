// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_TASK_REQUEST_PRIVATE_H_
#define IOS_CHROME_APP_TASK_REQUEST_PRIVATE_H_

#import "ios/chrome/app/task_request.h"

@class SceneState;

// This header should only be used from TaskRequest sub-classes and from
// task_request.mm, do not use it from the rest of the codebase.
@interface TaskRequest ()

@property(nonatomic, strong, readwrite) NSString* gaiaID;

- (instancetype)initWithSceneState:(SceneState*)sceneState
                       isColdStart:(BOOL)isColdStart;

- (SceneState*)sceneStateFromSessionID;

@end

#endif  // IOS_CHROME_APP_TASK_REQUEST_PRIVATE_H_
