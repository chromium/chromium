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
NSString* const kStartSurfaceSceneEnterIntoBackgroundTime =
    @"StartSurfaceSceneEnterIntoBackgroundTime";

}  // namespace

namespace test {

void SetStartSurfaceSessionObjectForSceneStateForTesting(  // IN-TEST
    SceneState* scene_state,
    base::Time timestamp) {
  [scene_state setSessionObject:timestamp.ToNSDate()
                         forKey:kStartSurfaceSceneEnterIntoBackgroundTime];
}

}  // namespace test

std::optional<base::Time> GetTimeMostRecentTabWasOpenForSceneState(
    SceneState* scene_state) {
  if (NSDate* timestamp = base::apple::ObjCCast<NSDate>([scene_state
          sessionObjectForKey:kStartSurfaceSceneEnterIntoBackgroundTime])) {
    return base::Time::FromNSDate(timestamp);
  }
  return std::nullopt;
}

std::optional<base::TimeDelta> GetTimeSinceMostRecentTabWasOpenForSceneState(
    SceneState* scene_state) {
  if (const std::optional<base::Time> timestamp =
          GetTimeMostRecentTabWasOpenForSceneState(scene_state)) {
    return base::Time::Now() - *timestamp;
  }
  return std::nullopt;
}

bool ShouldShowTabGroupInGridForSceneState(SceneState* scene_state) {
  const std::optional<base::TimeDelta> elapsed =
      GetTimeSinceMostRecentTabWasOpenForSceneState(scene_state);
  if (!elapsed.has_value()) {
    return false;
  }

  const base::TimeDelta min_duration = GetReturnToTabGroupInGridDuration();
  const base::TimeDelta max_duration = GetReturnToStartSurfaceDuration();

  if (*elapsed <= min_duration || *elapsed >= max_duration) {
    return false;
  }
  if (scene_state.presentingModalOverlay ||
      scene_state.startupHadExternalIntent || scene_state.pendingUserActivity) {
    return false;
  }
  return true;
}

bool ShouldShowStartSurfaceForSceneState(SceneState* scene_state) {
  const std::optional<base::TimeDelta> elapsed =
      GetTimeSinceMostRecentTabWasOpenForSceneState(scene_state);
  if (!elapsed.has_value() || *elapsed < GetReturnToStartSurfaceDuration()) {
    return false;
  }
  if (scene_state.presentingModalOverlay ||
      scene_state.startupHadExternalIntent || scene_state.pendingUserActivity ||
      scene_state.incognitoContentVisible) {
    return false;
  }
  return true;
}

NSString* GetRecentTabTileTimeLabelForSceneState(SceneState* scene_state) {
  const std::optional<base::TimeDelta> time_since_open =
      GetTimeSinceMostRecentTabWasOpenForSceneState(scene_state);
  if (!time_since_open.has_value()) {
    return @"";
  }
  NSString* time_label = nil;
  if (*time_since_open > base::Days(1)) {
    // If it has been at least a day since the most recent tab was opened,
    // then show days since instead of hours.
    time_label =
        l10n_util::GetNSStringF(IDS_IOS_RETURN_TO_RECENT_TAB_TIME_DAYS,
                                base::FormatNumber(time_since_open->InDays()));
  } else {
    time_label =
        l10n_util::GetNSStringF(IDS_IOS_RETURN_TO_RECENT_TAB_TIME_HOURS,
                                base::FormatNumber(time_since_open->InHours()));
  }
  DCHECK(time_label);
  return [NSString stringWithFormat:@" · %@", time_label];
}

void SetStartSurfaceSessionObjectForSceneState(SceneState* scene_state) {
  [scene_state setSessionObject:base::Time::Now().ToNSDate()
                         forKey:kStartSurfaceSceneEnterIntoBackgroundTime];
}
