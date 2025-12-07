// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNCED_SET_UP_UTILS_UTILS_H_
#define IOS_CHROME_BROWSER_SYNCED_SET_UP_UTILS_UTILS_H_

class PrefService;
@class ProfileState;
@class SceneState;

// Returns the active, non-incognito `SceneState` if preconditions for
// triggering the Synced Set Up flow are met based on `profile_state`, and `nil`
// otherwise.
//
// Preconditions include:
// - Profile initialization is complete.
// - There is no current UI blocker.
// - There is a foreground active scene.
// - The active scene is not incognito and belongs to the main browser provider.
SceneState* GetEligibleSceneForSyncedSetUp(ProfileState* profile_state);

// Returns true if the Synced Set Up UI can be shown based on the impression
// limit. This should be passed a profile pref service.
bool CanShowSyncedSetUp(const PrefService* profile_pref_service);

#endif  // IOS_CHROME_BROWSER_SYNCED_SET_UP_UTILS_UTILS_H_
