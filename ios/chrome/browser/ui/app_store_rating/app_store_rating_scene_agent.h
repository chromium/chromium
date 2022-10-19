// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_APP_STORE_RATING_APP_STORE_RATING_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_UI_APP_STORE_RATING_APP_STORE_RATING_SCENE_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/main/observing_scene_state_agent.h"

class PromosManager;

// A scene agent that requests engaged users are presented the
// App Store Rating promo based on the SceneActivationLevel changes.
@interface AppStoreRatingSceneAgent : ObservingSceneAgent

// Initializes an AppStoreRatingSceneAgent instance with given PromosManager.
- (instancetype)initWithPromosManager:(PromosManager*)promosManager;

// Unavailable. Use initWithPromosManager:.
- (instancetype)init NS_UNAVAILABLE;

// Determines whether the user meets the criteria to be
// considered engaged.
//
// To be considered engaged, the user must have used Chrome on
// at least 15 days overall, and at least one of the following
// must be met: Chrome is set as the default browser, the Credentials
// Provider Extension is enabled, the user has used Chrome on 3
// different days in the past 7 days.
@property(nonatomic, assign, readonly, getter=isUserEngaged) BOOL userEngaged;

@end

#endif  // IOS_CHROME_BROWSER_UI_APP_STORE_RATING_APP_STORE_RATING_SCENE_AGENT_H_
