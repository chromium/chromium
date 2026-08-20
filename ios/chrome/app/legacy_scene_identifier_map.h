// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_LEGACY_SCENE_IDENTIFIER_MAP_H_
#define IOS_CHROME_APP_LEGACY_SCENE_IDENTIFIER_MAP_H_

#import <functional>
#import <set>
#import <string>

#import "base/memory/raw_ref.h"
#import "base/sequence_checker.h"
#import "ios/chrome/app/scene_identifier_map.h"

@class AppState;
class ProfileAttributesStorageIOS;

// Legacy implementation of SceneIdentifierMap that uses the UISceneSession
// as identifier on iPad and a constant on iPhone.
class LegacySceneIdentifierMap : public SceneIdentifierMap {
 public:
  LegacySceneIdentifierMap(AppState* app_state,
                           ProfileAttributesStorageIOS* storage,
                           bool device_supports_multiple_scenes);

  ~LegacySceneIdentifierMap() override;

  // SceneIdentifierMap implementation.
  void OnSessionsDiscarded(NSSet<UISceneSession*>* sessions) override;
  void OnLastSceneStateDisconnected(SceneState* scene_state) override;
  void AssignIdentifierToSceneState(SceneState* scene_state,
                                    UISceneSession* session,
                                    bool is_first_scene) override;

 private:
  // Returns the identifier to use for the session for `scene`.
  std::string SessionIdentifierForScene(UISceneSession* scene);

  // The class is sequence-affine.
  SEQUENCE_CHECKER(sequence_checker_);

  // The AppState used to retrieve the list of connected SceneState.
  __weak AppState* app_state_;

  // The ProfileAttributesStorageIOS used by this instance.
  const raw_ref<ProfileAttributesStorageIOS> profile_attributes_storage_;

  // The set of identifier for all discarded sessions.
  std::set<std::string, std::less<>> discarded_session_identifiers_;

  // Whether the device only supports multiple scenes.
  const bool device_supports_multiple_scenes_;
};

#endif  // IOS_CHROME_APP_LEGACY_SCENE_IDENTIFIER_MAP_H_
