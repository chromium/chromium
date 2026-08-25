// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SCENE_IDENTIFIER_MAP_IMPL_H_
#define IOS_CHROME_APP_SCENE_IDENTIFIER_MAP_IMPL_H_

#import "base/containers/flat_map.h"
#import "base/memory/raw_ref.h"
#import "base/sequence_checker.h"
#import "base/types/strong_alias.h"
#import "ios/chrome/app/scene_identifier_map.h"

class PrefService;
class ProfileAttributesStorageIOS;

// Implementation of SceneIdentifierMap that maintains an indirect mapping
// between UISceneSession and SceneState identifiers in the local state.
class SceneIdentifierMapImpl : public SceneIdentifierMap {
 public:
  // Strong alias to allow the compiler to distinguish between the
  // UISceneSession and the SceneState identifiers. Public so that
  // they can be used by helper free functions.
  using ChromeIdentifier = base::StrongAlias<class ChromeTag, std::string>;
  using SystemIdentifier = base::StrongAlias<class SystemTag, std::string>;

  SceneIdentifierMapImpl(PrefService* local_state,
                         ProfileAttributesStorageIOS* storage,
                         bool device_supports_multiple_scenes);

  ~SceneIdentifierMapImpl() override;

  // SceneIdentifierMap implementation.
  void OnSessionsDiscarded(NSSet<UISceneSession*>* sessions) override;
  void OnLastSceneStateDisconnected(SceneState* scene_state) override;
  void AssignIdentifierToSceneState(SceneState* scene_state,
                                    UISceneSession* session,
                                    bool is_first_scene) override;

 private:
  // Loads the mapping between UISceneSession and SceneState identifiers
  // from the preferences.
  void LoadMappingFromPreferences();

  // Saves the mapping between UISceneSession and SceneState identifiers
  // to the preferences so that it is available on next execution.
  void SaveMappingToPreferences();

  // Generates a new unique identifier, using `hint` if valid and available.
  [[nodiscard]] ChromeIdentifier GenerateIdentifier(ChromeIdentifier hint);

  // The class is sequence-affine.
  SEQUENCE_CHECKER(sequence_checker_);

  // The PrefService and ProfileAttributesStorageIOS used by this instance.
  const raw_ref<PrefService> local_state_;
  const raw_ref<ProfileAttributesStorageIOS> profile_attributes_storage_;

  // Mapping between UISceneSession and SceneState identifiers.
  base::flat_map<SystemIdentifier, ChromeIdentifier> mapping_;

  // The identifier of the last connected scene.
  ChromeIdentifier last_connected_scene_;

  // Whether the device only supports multiple scenes.
  const bool device_supports_multiple_scenes_;
};

#endif  // IOS_CHROME_APP_SCENE_IDENTIFIER_MAP_IMPL_H_
