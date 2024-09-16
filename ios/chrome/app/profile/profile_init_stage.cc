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
      return ProfileInitStage::InitStageProfileLoaded;
    case InitStageEnterprise:
      return ProfileInitStage::InitStageEnterprise;
    case InitStageBrowserObjectsForUI:
      return ProfileInitStage::InitStagePrepareUI;
    case InitStageNormalUI:
      return ProfileInitStage::InitStageUIReady;
    case InitStageFirstRun:
      return ProfileInitStage::InitStageFirstRun;
    case InitStageChoiceScreen:
      return ProfileInitStage::InitStageChoiceScreen;
    case InitStageFinal:
      return ProfileInitStage::InitStageFinal;
  }
}
