// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CHANGE_PROFILE_CHANGE_PROFILE_CONTINUATION_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CHANGE_PROFILE_CHANGE_PROFILE_CONTINUATION_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

@class SceneState;

@protocol ChangeProfileContinuation <NSObject>

- (void)executeWithSceneState:(SceneState*)sceneState
                   completion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CHANGE_PROFILE_CHANGE_PROFILE_CONTINUATION_H_
