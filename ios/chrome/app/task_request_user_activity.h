// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_TASK_REQUEST_USER_ACTIVITY_H_
#define IOS_CHROME_APP_TASK_REQUEST_USER_ACTIVITY_H_

#import "ios/chrome/app/task_request.h"

@class NSUserActivity;
@class SceneState;

// Task request for handling user activities.
@interface TaskRequestForUserActivity : TaskRequest

- (instancetype)initWithUserActivity:(NSUserActivity*)userActivity
                          sceneState:(SceneState*)sceneState
                         isColdStart:(BOOL)isColdStart;

@end

#endif  // IOS_CHROME_APP_TASK_REQUEST_USER_ACTIVITY_H_
