// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_PROFILE_INIT_STAGE_H_
#define IOS_CHROME_APP_PROFILE_PROFILE_INIT_STAGE_H_

// Profile initialisation stages. The app will go sequentially in-order through
// each stage each time a new profile is added.
enum class ProfileInitStage {
  // Perform all asynch operation to load profile's preferences from disk.
  InitStageLoadProfile,

  // Profile preferences have been loaded and the ChromeBrowserState object and
  // all KeyedServices can be used. The app will automatically transition to the
  // next stage.
  InitStageProfileLoaded,

  // The app is fetching any enterprise policies for the profile. The
  // initialization is blocked on this because the policies might have an effect
  // on later init stages.
  InitStageEnterprise,

  // The app is loading any elements needed for UI (e.g. Session data, ...)
  InitStagePrepareUI,

  // Application is ready to present UI for profile, it will automatically
  // transition to the next stage. This can be used to start background tasks to
  // update UI.
  InitStageUIReady,

  // All the stage between InitStageUIReady and InitStageNormalUI represent
  // blocking screens that the user must go through before proceeding to the
  // next
  // stage. If the conditions are already handled, the transition will be
  // instantanous.

  // It is possible to add new stage between InitStageUIReady and
  // InitStageNormalUI to add new blocking stage if a feature requires it.

  // This present the first run experience. Only presented for new profile
  // (maybe first profile?)
  InitStageFirstRun,

  // This present the search engine selection screen. It is presented for each
  // profile and if the user did not select a default search engine yet.
  InitStageChoiceScreen,

  // Application is presenting the regular chrome UI for this profile, it will
  // automatically transition to the next stage. This can be used to detect
  // users can now start interacting with the UI.
  InitStageNormalUI,

  // Final stage, no transition until the profile is shut down.
  InitStageFinal
};

#endif  // IOS_CHROME_APP_PROFILE_PROFILE_INIT_STAGE_H_
