// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCENE_COORDINATOR_SCENE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SCENE_COORDINATOR_SCENE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/scene/ui/scene_mutator.h"

namespace feature_engagement {
class Tracker;
}

class GeminiService;

enum class AppBarPosition;
@protocol FullscreenUIElement;
class FullscreenController;
@protocol SceneConsumer;

// Mediator for the Scene coordinate.
@interface SceneMediator : NSObject <SceneMutator>

// The consumer of this mediator.
@property(nonatomic, weak) id<FullscreenUIElement, SceneConsumer> consumer;

// The feature engagement tracker.
@property(nonatomic, assign) feature_engagement::Tracker* tracker;

// The Gemini service.
@property(nonatomic, assign) GeminiService* geminiService;

// The position of the App Bar. Only set at startup, not updated after.
@property(nonatomic, assign) AppBarPosition appBarPositionAtLaunch;

// Initializes the mediator.
- (instancetype)initWithRegularFullscreenController:
                    (FullscreenController*)regularFullscreenController
                      incognitoFullscreenController:
                          (FullscreenController*)incognitoFullscreenController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

// Updates the incognito fullscreen controller.
- (void)setIncognitoFullscreenController:
    (FullscreenController*)incognitoFullscreenController;

@end

#endif  // IOS_CHROME_BROWSER_SCENE_COORDINATOR_SCENE_MEDIATOR_H_
