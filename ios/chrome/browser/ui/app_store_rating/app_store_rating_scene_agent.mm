// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/app_store_rating/app_store_rating_scene_agent.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AppStoreRatingSceneAgent ()

// Determines whether the user has used Chrome for at least 3
// different days within the past 7 days.
@property(nonatomic, assign, readonly, getter=isChromeUsed3DaysInPastWeek)
    BOOL chromeUsed3DaysInPastWeek;

// Determines whether the user has used Chrome for at least 15
// different days overall.
@property(nonatomic, assign, readonly, getter=isChromeUsed15Days)
    BOOL chromeUsed15Days;

// Determines whether the user has enabled the Credentials
// Provider Extension.
@property(nonatomic, assign, readonly, getter=isCPEEnabled) BOOL CPEEnabled;

@end

@implementation AppStoreRatingSceneAgent

- (instancetype)init {
  self = [super init];

  return self;
}

- (BOOL)isUserEngaged {
  return NO;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
}

#pragma mark - Getters

- (BOOL)isChromeUsed3DaysInPastWeek {
  return NO;
}

- (BOOL)isChromeUsed15Days {
  return NO;
}

- (BOOL)isCPEEnabled {
  return NO;
}

#pragma mark - Private

// Calls the PromosManager to request iOS displays the
// App Store Rating prompt to the user.
- (void)requestPromoDisplay {
}

@end
