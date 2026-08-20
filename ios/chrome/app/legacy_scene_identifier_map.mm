// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/legacy_scene_identifier_map.h"

#import <UIKit/UIKit.h>

#import "base/check_deref.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"

namespace {

using StringSet = std::set<std::string, std::less<>>;

// Unique identifier used by device that do not support multiple scenes.
constexpr std::string_view kSyntheticSessionIdentifier =
    "{SyntheticIdentifier}";

// Inserts `session_ids` into the set of discarded sessions for `attrs`.
void InsertDiscardedSessions(const StringSet& session_ids,
                             ProfileAttributesIOS& attrs) {
  auto discarded_sessions = attrs.GetDiscardedSessions();
  discarded_sessions.insert(session_ids.begin(), session_ids.end());
  attrs.SetDiscardedSessions(discarded_sessions);
}

}  // namespace

LegacySceneIdentifierMap::LegacySceneIdentifierMap(
    AppState* app_state,
    ProfileAttributesStorageIOS* storage,
    bool device_supports_multiple_scenes)
    : app_state_(app_state),
      profile_attributes_storage_(CHECK_DEREF(storage)),
      device_supports_multiple_scenes_(device_supports_multiple_scenes) {}

LegacySceneIdentifierMap::~LegacySceneIdentifierMap() = default;

void LegacySceneIdentifierMap::OnSessionsDiscarded(
    NSSet<UISceneSession*>* sessions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the device does not support multiple scenes, then the data is saved
  // with a fixed identifier and we ignore the discard scenes so that users
  // do not lose their tabs when they swipe the app window (as the gesture
  // is overloaded and is sometimes interpreted as a request to discard the
  // window even on iPhone).
  if (!device_supports_multiple_scenes_) {
    return;
  }

  // As the SceneState identifier is initialized to -persistentIdentifier,
  // it is okay to use the value.
  for (UISceneSession* session in sessions) {
    discarded_session_identifiers_.insert(
        base::SysNSStringToUTF8(session.persistentIdentifier));
  }

  profile_attributes_storage_->IterateOverProfileAttributes(base::BindRepeating(
      &InsertDiscardedSessions, discarded_session_identifiers_));

  // It was found that -application:didDiscardSceneSessions: may be called with
  // UISceneSession* corresponding to SceneState* that are still connected. It
  // caused flakyness of EarlGrey tests (see https://crbug.com/390108895). The
  // behaviour has only been confirmed for EarlGrey tests. Record an histogram
  // counting how many Scenes are discarded while still connected to detect if
  // the issue also reproduce in production (if it were to reproduce, it would
  // cause unexplained tab losses).
  //
  // See https://crbug.com/392575873 for details.
  NSUInteger count_discarded_scene_still_connected = 0;
  for (SceneState* scene_state in app_state_.connectedScenes) {
    if (discarded_session_identifiers_.contains(scene_state.sceneSessionID)) {
      ++count_discarded_scene_still_connected;
    }
  }

  base::UmaHistogramExactLinear(
      "IOS.Sessions.DiscardedScenesStillConnectedCount",
      count_discarded_scene_still_connected, 100);
}

void LegacySceneIdentifierMap::OnLastSceneStateDisconnected(
    SceneState* scene_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Nothing to do.
}

void LegacySceneIdentifierMap::AssignIdentifierToSceneState(
    SceneState* scene_state,
    UISceneSession* session,
    bool is_first_scene) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scene_state.sceneSessionID = SessionIdentifierForScene(session);
  if (!device_supports_multiple_scenes_) {
    return;
  }

  // Record whether data has been purged for a scene with the same identifier.
  //
  // It is used to detect if data is lost due to the possible bug in UIKit
  // where the method -application:didDiscardSceneSessions: is called with
  // references to scenes that are still connected.
  //
  // See https://crbug.com/392575873 for more details.
  base::UmaHistogramBoolean(
      "IOS.Sessions.DiscardedSceneConnectedAfterBeingPurged",
      discarded_session_identifiers_.contains(scene_state.sceneSessionID));
}

std::string LegacySceneIdentifierMap::SessionIdentifierForScene(
    UISceneSession* session) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (device_supports_multiple_scenes_) {
    std::string identifier =
        base::SysNSStringToUTF8(session.persistentIdentifier);

    DCHECK_NE(identifier, "");
    DCHECK_NE(identifier, kSyntheticSessionIdentifier);
    return identifier;
  }

  return std::string(kSyntheticSessionIdentifier);
}
