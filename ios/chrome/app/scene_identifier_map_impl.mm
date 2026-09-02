// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/scene_identifier_map_impl.h"

#import <UIKit/UIKit.h>

#import <functional>
#import <utility>

#import "base/check.h"
#import "base/check_deref.h"
#import "base/containers/flat_set.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"

namespace {

using ChromeIdentifier = SceneIdentifierMapImpl::ChromeIdentifier;
using SystemIdentifier = SceneIdentifierMapImpl::SystemIdentifier;
using SessionIds = ProfileAttributesIOS::SessionIds;

// Returns a SystemIdentifier for `session`.
SystemIdentifier SystemIdentifierForSession(UISceneSession* session) {
  return SystemIdentifier(
      base::SysNSStringToUTF8(session.persistentIdentifier));
}

// Removes `identifier` from the set of discarded sessions in `attrs`.
void RestoreDiscardedSession(const ChromeIdentifier& identifier,
                             ProfileAttributesIOS& attrs) {
  SessionIds discarded_sessions = attrs.GetDiscardedSessions();
  discarded_sessions.erase(*identifier);
  attrs.SetDiscardedSessions(discarded_sessions);
}

// Inserts all `identifiers` in the set of discarded sessions in `attrs`.
void InsertDiscardedSessions(const std::set<ChromeIdentifier>& identifiers,
                             ProfileAttributesIOS& attrs) {
  SessionIds discarded_sessions = attrs.GetDiscardedSessions();
  for (const auto& identifier : identifiers) {
    discarded_sessions.insert(*identifier);
  }
  attrs.SetDiscardedSessions(discarded_sessions);
}

// Returns whether `map` stores `value` for any key.
template <typename K, typename V>
bool HasValue(const base::flat_map<K, V>& map, const V& value) {
  for (const auto& [_, val] : map) {
    if (val == value) {
      return true;
    }
  }
  return false;
}

}  // namespace

SceneIdentifierMapImpl::SceneIdentifierMapImpl(
    PrefService* local_state,
    ProfileAttributesStorageIOS* storage,
    bool device_supports_multiple_scenes)
    : local_state_(CHECK_DEREF(local_state)),
      profile_attributes_storage_(CHECK_DEREF(storage)),
      device_supports_multiple_scenes_(device_supports_multiple_scenes) {
  LoadMappingFromPreferences();
  last_connected_scene_ = ChromeIdentifier(
      local_state_->GetString(prefs::kLastConnectedSceneIdentifier));

  // From the introduction of multi-window support in the application until
  // M-153, the UISceneSession -persistentIdentifier was used as identifier
  // for the SceneState on iPad, while iPhone used a constant. In that case
  // there won't be any value saved for the last connected SceneState, and
  // the data needs to be restored.
  //
  // The data can be recreated from the fact that the SceneState identifier
  // is used as a key for session scoped preferences and as another key in
  // the mapping of SceneState to ProfileIOS identifiers.
  //
  // This migration code can be deleted in a few releases (say from M-163)
  // when all users have had time to migrate.
  //
  // TODO(crbug.com/551914665): cleanup as part of removing old code once
  // the kRecoverTabsOfLastClosedWindow feature has launched.
  if (mapping_.empty() && last_connected_scene_->empty()) {
    base::flat_set<ChromeIdentifier> identifiers;
    profile_attributes_storage_->IterateOverProfileAttributes(
        base::BindRepeating(
            [](base::flat_set<ChromeIdentifier>& identifiers,
               const ProfileAttributesIOS& attrs) {
              for (const std::string& identifier : attrs.GetKnownSessions()) {
                identifiers.insert(ChromeIdentifier(identifier));
              }
            },
            std::ref(identifiers)));

    for (const auto [session, _] :
         local_state_->GetDict(prefs::kProfileForScene)) {
      identifiers.insert(ChromeIdentifier(session));
    }

    if (identifiers.size() == 1) {
      const ChromeIdentifier& chrome_id = *identifiers.begin();
      local_state_->SetString(prefs::kLastConnectedSceneIdentifier, *chrome_id);
      last_connected_scene_ = chrome_id;
    }

    if (device_supports_multiple_scenes_) {
      // On iPad, historically the SceneState identifier was initialized
      // as a copy of the UISceneSession identifier.
      for (const ChromeIdentifier& chrome_id : identifiers) {
        mapping_.emplace(SystemIdentifier(*chrome_id), chrome_id);
      }
    }

    SaveMappingToPreferences();
  }
}

SceneIdentifierMapImpl::~SceneIdentifierMapImpl() = default;

void SceneIdentifierMapImpl::OnSessionsDiscarded(
    NSSet<UISceneSession*>* sessions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<ChromeIdentifier> identifiers;
  for (UISceneSession* session in sessions) {
    const SystemIdentifier system_id = SystemIdentifierForSession(session);
    if (auto iter = mapping_.find(system_id); iter != mapping_.end()) {
      identifiers.insert(iter->second);
      mapping_.erase(iter);
    }
  }

  if (!identifiers.empty()) {
    // If any mapping were found, mark them as discarded for all profiles,
    // and save the updated mapping to preferences.
    profile_attributes_storage_->IterateOverProfileAttributes(
        base::BindRepeating(&InsertDiscardedSessions, identifiers));

    SaveMappingToPreferences();
  }
}

void SceneIdentifierMapImpl::OnLastSceneStateDisconnected(
    SceneState* scene_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!scene_state.sceneSessionID.empty());
  ChromeIdentifier identifier(std::string(scene_state.sceneSessionID));
  local_state_->SetString(prefs::kLastConnectedSceneIdentifier, *identifier);
  last_connected_scene_ = std::move(identifier);
}

void SceneIdentifierMapImpl::AssignIdentifierToSceneState(
    SceneState* scene_state,
    UISceneSession* session,
    bool is_first_scene) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(scene_state.sceneSessionID.empty());

  // If the mapping exists, then it is a restored SceneState.
  const SystemIdentifier system_id = SystemIdentifierForSession(session);
  if (auto iter = mapping_.find(system_id); iter != mapping_.end()) {
    scene_state.sceneSessionID = *iter->second;
    scene_state.currentOrigin = WindowActivityRestoredOrigin;
    return;
  }

  ChromeIdentifier chrome_id;
  if (is_first_scene && !last_connected_scene_->empty()) {
    // This is the first SceneState connected, and there is no known mapping.
    // Use the identifier of the last closed SceneState, in order to restore
    // the tabs from that window (as this is what the user expects). See the
    // bug https://crbug.com/519105565 for more details.
    //
    // It is likely that the UISceneSession has been marked as discarded, so
    // remove it from the set of discarded sessions so that the tabs are not
    // deleted from disk.
    if (!HasValue(mapping_, last_connected_scene_)) {
      chrome_id = last_connected_scene_;
      scene_state.currentOrigin = WindowActivityRestoredOrigin;
      profile_attributes_storage_->IterateOverProfileAttributes(
          base::BindRepeating(&RestoreDiscardedSession, chrome_id));
    }
  }

  if (chrome_id->empty()) {
    // A new SceneState has been connected, assign a new identifier.
    //
    // To limit the risk of losing data if the feature is disabled, pass an
    // hint initialized with the value that would have been used by the old
    // code (i.e. UISceneSession identifier on iPad or a constant on iPhone).
    chrome_id =
        GenerateIdentifier(device_supports_multiple_scenes_
                               ? ChromeIdentifier(*system_id)
                               : ChromeIdentifier("{SyntheticIdentifier}"));
  }

  CHECK(!chrome_id->empty());
  mapping_.emplace(system_id, chrome_id);
  scene_state.sceneSessionID = *chrome_id;

  SaveMappingToPreferences();
}

void SceneIdentifierMapImpl::LoadMappingFromPreferences() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& dict = local_state_->GetDict(prefs::kSceneSessionIdentifierMap);
  for (const auto [key, value] : dict) {
    mapping_.emplace(SystemIdentifier(key),
                     ChromeIdentifier(value.GetString()));
  }
}

void SceneIdentifierMapImpl::SaveMappingToPreferences() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::DictValue dict;
  for (const auto& [system_id, chrome_id] : mapping_) {
    dict.Set(*system_id, base::Value(*chrome_id));
  }

  local_state_->SetDict(prefs::kSceneSessionIdentifierMap, std::move(dict));
}

ChromeIdentifier SceneIdentifierMapImpl::GenerateIdentifier(
    ChromeIdentifier hint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::flat_set<ChromeIdentifier> identifiers;
  for (const auto& [_, chrome_id] : mapping_) {
    identifiers.insert(chrome_id);
  }

  // If the hint is valid and unused, use it as identifier.
  if (!hint->empty() && !identifiers.contains(hint)) {
    return hint;
  }

  ChromeIdentifier identifier;
  for (size_t i = 0; i <= identifiers.size() + 1; ++i) {
    using base::NumberToString;
    ChromeIdentifier candidate(base::StrCat({"SceneState", NumberToString(i)}));
    if (!identifiers.contains(candidate)) {
      identifier = std::move(candidate);
      break;
    }
  }

  // Safety: as there are at most N = identifiers.size() already assigned
  // identifiers, if all "SceneState0", "SceneState1", ... are taken then
  // "SceneStateN" must be available. Otherwise one of the previous value
  // will have been used. Thus identifier can be used as valid identifier.
  CHECK(!identifiers.contains(identifier));
  CHECK(!identifier->empty());
  return identifier;
}
