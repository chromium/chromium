// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCENE_UI_SCENE_VIEW_H_
#define IOS_CHROME_BROWSER_SCENE_UI_SCENE_VIEW_H_

#import <UIKit/UIKit.h>

@protocol SceneViewDelegate;

// The view at the root of the app.
@interface SceneView : UIView

// The delegate for the scene view.
@property(nonatomic, weak) id<SceneViewDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_SCENE_UI_SCENE_VIEW_H_
