// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_util.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/i18n/number_formatting.h"
#import "base/time/time.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The key to store the timestamp when the scene enters into background.
NSString* kStartSurfaceSceneEnterIntoBackgroundTime =
    @"StartSurfaceSceneEnterIntoBackgroundTime";

}  // namespace

base::TimeDelta GetTimeSinceMostRecentTabWasOpenForSceneState(
    SceneState* scene_state) {
  NSDate* timestamp = base::apple::ObjCCast<NSDate>([scene_state
      sessionObjectForKey:kStartSurfaceSceneEnterIntoBackgroundTime]);

  if (timestamp == nil) {
    return base::TimeDelta();
  }

  return base::Time::Now() - base::Time::FromNSDate(timestamp);
}

bool ShouldShowStartSurfaceForSceneState(SceneState* scene_state) {
  NSDate* timestamp = base::apple::ObjCCast<NSDate>([scene_state
      sessionObjectForKey:kStartSurfaceSceneEnterIntoBackgroundTime]);
  if (timestamp == nil) {
    return NO;
  }

  const base::TimeDelta elapsed =
      base::Time::Now() - base::Time::FromNSDate(timestamp);
  if (elapsed < GetReturnToStartSurfaceDuration()) {
    return NO;
  }

  if (scene_state.presentingModalOverlay ||
      scene_state.startupHadExternalIntent || scene_state.pendingUserActivity ||
      scene_state.incognitoContentVisible) {
    return NO;
  }

  return YES;
}

NSString* GetRecentTabTileTimeLabelForSceneState(SceneState* scene_state) {
  const base::TimeDelta time_since_open =
      GetTimeSinceMostRecentTabWasOpenForSceneState(scene_state);
  if (time_since_open == base::TimeDelta()) {
    return @"";
  }
  NSString* time_label = nil;
  if (time_since_open > base::Days(1)) {
    // If it has been at least a day since the most recent tab was opened,
    // then show days since instead of hours.
    time_label =
        l10n_util::GetNSStringF(IDS_IOS_RETURN_TO_RECENT_TAB_TIME_DAYS,
                                base::FormatNumber(time_since_open.InDays()));
  } else {
    time_label =
        l10n_util::GetNSStringF(IDS_IOS_RETURN_TO_RECENT_TAB_TIME_HOURS,
                                base::FormatNumber(time_since_open.InHours()));
  }
  DCHECK(time_label);
  return [NSString stringWithFormat:@" · %@", time_label];
}

void SetStartSurfaceSessionObjectForSceneState(SceneState* scene_state) {
  [scene_state setSessionObject:base::Time::Now().ToNSDate()
                         forKey:kStartSurfaceSceneEnterIntoBackgroundTime];
}
