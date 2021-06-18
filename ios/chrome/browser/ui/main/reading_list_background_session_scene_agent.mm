// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/reading_list_background_session_scene_agent.h"

#import "ios/chrome/browser/ui/reading_list/reading_list_constants.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// NSUserDefault key to save the last time the app backgrounded.
NSString* const kReadingListLastBackgroundTime =
    @"kReadingListLastBackgroundTime";
// Minimum time threshold app is not in the foreground before the next app open
// can be deemed a new "session". Currently set to half an hour.
const NSTimeInterval kReadingListBackgroundThreshold = 60 * 30;
}  // namespace

@interface ReadingListBackgroundSessionSceneAgent ()

@end

@implementation ReadingListBackgroundSessionSceneAgent

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (!IsReadingListMessagesEnabled()) {
    return;
  }
  if (level <= SceneActivationLevelBackground) {
    // If the ApplicationState is not active, then that means no other scenes
    // are in the foreground, meaning the app is completely backgrounded.
    if (UIApplication.sharedApplication.applicationState !=
        UIApplicationStateActive) {
      // Save the time when the app is backgrounded.
      [[NSUserDefaults standardUserDefaults]
          setObject:[NSDate date]
             forKey:kReadingListLastBackgroundTime];
    }
  } else if (level == SceneActivationLevelForegroundInactive) {
    NSDate* last_background_timestamp = [[NSUserDefaults standardUserDefaults]
        objectForKey:kReadingListLastBackgroundTime];
    if (!last_background_timestamp) {
      // There may not be a saved time if the app crashes. It's ok that
      // kLastTimeUserShownReadingListMessages is not reset in this situation.
      return;
    }
    NSDate* half_hour_ago_date =
        [NSDate dateWithTimeIntervalSinceNow:-kReadingListBackgroundThreshold];
    if ([last_background_timestamp compare:half_hour_ago_date] ==
        NSOrderedAscending) {
      // Reset the last Messages prompt timestamp when it is the start of a new
      // "session".
      [[NSUserDefaults standardUserDefaults]
          removeObjectForKey:kLastTimeUserShownReadingListMessages];
    }
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:kReadingListLastBackgroundTime];
  }
}

@end
