// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_TASK_REQUEST_URL_CONTEXT_H_
#define IOS_CHROME_APP_TASK_REQUEST_URL_CONTEXT_H_

#import "ios/chrome/app/task_request.h"

@class UIOpenURLContext;
@class SceneState;

// Task request for handling URL opening contexts.
@interface TaskRequestForURLContext : TaskRequest

- (instancetype)initWithURLContext:(UIOpenURLContext*)URLContext
                        sceneState:(SceneState*)sceneState
                       isColdStart:(BOOL)isColdStart;

@end

#endif  // IOS_CHROME_APP_TASK_REQUEST_URL_CONTEXT_H_
