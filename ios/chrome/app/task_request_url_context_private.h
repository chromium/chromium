// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_TASK_REQUEST_URL_CONTEXT_PRIVATE_H_
#define IOS_CHROME_APP_TASK_REQUEST_URL_CONTEXT_PRIVATE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/app/task_request_url_context.h"

class GURL;
@class SceneState;

// Class extension declaring private properties and methods of
// TaskRequestForURLContext for use by its subclasses.
@interface TaskRequestForURLContext ()

@property(nonatomic, strong, readonly) UIOpenURLContext* URLContext;

- (void)recordStartupMetrics;

// Handles the command associated with the request using the given scene state.
- (void)handleCommandWithSceneState:(SceneState*)sceneState;

// Computes the adjusted application mode based on incognito enterprise policy
// and the initial mode.
- (ApplicationModeForTabOpening)
    targetModeForSceneState:(SceneState*)sceneState
                defaultMode:(ApplicationModeForTabOpening)defaultMode;

// Opens a tab with the specified parameters and dismisses modals.
- (void)openTabWithSceneState:(SceneState*)sceneState
                  externalURL:(const GURL&)externalURL
                   virtualURL:(const GURL&)virtualURL
                   targetMode:(ApplicationModeForTabOpening)targetMode
            postOpeningAction:(TabOpeningPostOpeningAction)postOpeningAction
             fromWidgetOrSiri:(BOOL)fromWidgetOrSiri;

@end

#endif  // IOS_CHROME_APP_TASK_REQUEST_URL_CONTEXT_PRIVATE_H_
