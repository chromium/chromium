// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_CONTROLLER_TESTING_H_

#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"

class Browser;
struct UrlLoadParams;
@class SceneCoordinator;
@class WrangledBrowser;

// If `params` is for a Dino game URL, update transition type to allow opening.
[[nodiscard]] UrlLoadParams UpdateParamsForDinoGame(UrlLoadParams params);

// Methods exposed for testing. This is terrible and should be rewritten.
@interface SceneController ()

- (void)addANewTabAndPresentBrowser:(Browser*)browser
                  withURLLoadParams:(const UrlLoadParams&)urlLoadParams;

- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox;

- (WrangledBrowser*)currentInterface;

- (SceneCoordinator*)mainCoordinator;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_CONTROLLER_TESTING_H_
