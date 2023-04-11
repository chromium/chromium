// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_UI_PROVIDER_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_UI_PROVIDER_H_

@class UIViewController;

// Protocol for a provider of objects and information tied to the scene's UI.
@protocol SceneUIProvider <NSObject>

// Returns the UIViewController of the view that is currently active on the
// scene.
- (UIViewController*)activeViewController;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_UI_PROVIDER_H_
