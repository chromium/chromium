// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_PROFILE_INIT_STAGE_H_
#define IOS_CHROME_APP_PROFILE_PROFILE_INIT_STAGE_H_

// TODO(crbug.com/353683675): remove when profile init stage and app
// init stage are fully separate.
#include "ios/chrome/app/application_delegate/app_init_stage.h"

// Profile initialisation stages. The app will go sequentially in-order through
// each stage each time a new profile is added.
enum class ProfileInitStage {
  // Initial stage, nothing initialized yet.
  kStart,

  // Perform all asynch operation to load profile's preferences from disk.
  kLoadProfile,

  // Profile preferences have been loaded and the ProfileIOS object and all
  // KeyedServices can be used. The app will automatically transition to the
  // next stage.
  kProfileLoaded,

  // The application is fetching any enterprise policies for the profile. The
  // initialization is blocked on this because the policies might have an effect
  // on later init stages.
  kEnterprise,

  // The application is loading any elements needed for UI for this profile
  // (e.g. Session data, ...)
  kPrepareUI,

  // The application is ready to present UI for the profile, it will transition
  // to the next stage. This can be used to start background tasks to update UI.
  kUIReady,

  // All the stage between kUIReady and kNormalUI represent blocking screens
  // that the user must go through before proceeding to the next stage. If the
  // conditions are already handled, the transition will be instantanous.
  //
  // It is possible to add new stage between kNormalUI and kNormalUI to add new
  // blocking stage if a feature requires it.

  // This presents the first run experience. Only presented for new profile
  // (maybe first profile?)
  kFirstRun,

  // This presents the search engine selection screen. It is presented for each
  // profile and if the user did not select a default search engine yet.
  kChoiceScreen,

  // The application is presenting the regular chrome UI for this profile, it
  // will automatically transition to the next stage. This can be used to detect
  // that users can now start interacting with the UI.
  kNormalUI,

  // Final stage, no transition until the profile is shut down.
  kFinal,
};

// Returns the equivalent ProfileInitStage from AppInitStage.
// TODO(crbug.com/353683675): remove when profile init stage and app
// init stage are fully separate.
ProfileInitStage ProfileInitStageFromAppInitStage(AppInitStage app_init_stage);

#endif  // IOS_CHROME_APP_PROFILE_PROFILE_INIT_STAGE_H_
