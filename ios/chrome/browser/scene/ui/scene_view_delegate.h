// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCENE_UI_SCENE_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_SCENE_UI_SCENE_VIEW_DELEGATE_H_

// The delegate for the scene view.
@protocol SceneViewDelegate <NSObject>

// Called when the scene view moves to a window.
- (void)sceneViewDidMoveToWindow:(SceneView*)sceneView;

@end

#endif  // IOS_CHROME_BROWSER_SCENE_UI_SCENE_VIEW_DELEGATE_H_
