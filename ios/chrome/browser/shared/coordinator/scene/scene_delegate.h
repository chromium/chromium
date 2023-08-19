// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_DELEGATE_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_DELEGATE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

// An object acting as a scene delegate for UIKit. Updates the scene state.
@interface SceneDelegate : NSObject <UIWindowSceneDelegate>

@property(nonatomic, strong) UIWindow* window;

// The object that holds the state of the scene associated with this delegate.
@property(nonatomic, readonly) SceneState* sceneState;

// The controller created and owned by this object.
@property(nonatomic, readonly) SceneController* sceneController;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_DELEGATE_H_
