// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_TASK_REQUEST_URL_CONTEXT_PRIVATE_H_
#define IOS_CHROME_APP_TASK_REQUEST_URL_CONTEXT_PRIVATE_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/app/task_request_url_context.h"

class GURL;
@class SceneState;

// Class extension declaring private properties and methods of
// TaskRequestForURLContext for use by its subclasses.
@interface TaskRequestForURLContext ()

// The URL opening context associated with this task request.
@property(nonatomic, strong, readonly) UIOpenURLContext* URLContext;

// Initializes the task request with the given URL context, scene state, and
// cold start status.
- (instancetype)initWithURLContext:(UIOpenURLContext*)URLContext
                        sceneState:(SceneState*)sceneState
                       isColdStart:(BOOL)isColdStart;

// Records metrics for the URL context.
- (void)recordMetrics;

// Handles the command associated with the request using the given scene state.
- (void)handleCommandWithSceneState:(SceneState*)sceneState;

// Computes the adjusted application mode based on incognito enterprise policy
// and the initial mode.
- (ApplicationModeForTabOpening)
    targetModeForSceneState:(SceneState*)sceneState
                defaultMode:(ApplicationModeForTabOpening)defaultMode;

// Opens a tab with the specified parameters and dismisses modals. At most one
// of `postOpeningAction` (other than `NO_ACTION`) and `postOpeningCompletion`
// may be provided. If `postOpeningCompletion` is non-nil, it is executed after
// opening; otherwise `postOpeningAction` is resolved via `TabOpening`.
- (void)openTabWithSceneState:(SceneState*)sceneState
                  externalURL:(const GURL&)externalURL
                   virtualURL:(const GURL&)virtualURL
                   targetMode:(ApplicationModeForTabOpening)targetMode
            postOpeningAction:(TabOpeningPostOpeningAction)postOpeningAction
        postOpeningCompletion:(ProceduralBlock)postOpeningCompletion
             fromWidgetOrSiri:(BOOL)fromWidgetOrSiri;

@end

#endif  // IOS_CHROME_APP_TASK_REQUEST_URL_CONTEXT_PRIVATE_H_
