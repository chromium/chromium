// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/app/profile/profile_init_stage.h"

#include "base/notreached.h"

ProfileInitStage ProfileInitStageFromAppInitStage(InitStage app_init_stage) {
  switch (app_init_stage) {
    case InitStageStart:
    case InitStageBrowserBasic:
    case InitStageSafeMode:
    case InitStageVariationsSeed:
      NOTREACHED();

    case InitStageBrowserObjectsForBackgroundHandlers:
      return ProfileInitStage::kProfileLoaded;
    case InitStageEnterprise:
      return ProfileInitStage::kEnterprise;
    case InitStageBrowserObjectsForUI:
      return ProfileInitStage::kPrepareUI;
    case InitStageNormalUI:
      return ProfileInitStage::kUIReady;
    case InitStageFirstRun:
      return ProfileInitStage::kFirstRun;
    case InitStageChoiceScreen:
      return ProfileInitStage::kChoiceScreen;
    case InitStageFinal:
      return ProfileInitStage::kFinal;
  }
}
