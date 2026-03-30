// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCENE_UI_SCENE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_SCENE_UI_SCENE_VIEW_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

@class SceneViewController;

// Delegate for SceneViewController.
@protocol SceneViewControllerDelegate <NSObject>

// Notifies the delegate to show the Gemini floaty if invoked.
- (void)sceneViewControllerShowGeminiFloatyIfInvoked:
    (SceneViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_SCENE_UI_SCENE_VIEW_CONTROLLER_DELEGATE_H_
