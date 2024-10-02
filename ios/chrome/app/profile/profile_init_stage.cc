// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/app/profile/profile_init_stage.h"

#include "base/notreached.h"

ProfileInitStage ProfileInitStageFromAppInitStage(AppInitStage app_init_stage) {
  switch (app_init_stage) {
    case AppInitStage::kStart:
    case AppInitStage::kBrowserBasic:
    case AppInitStage::kSafeMode:
    case AppInitStage::kVariationsSeed:
      NOTREACHED();

    case AppInitStage::kBrowserObjectsForBackgroundHandlers:
      return ProfileInitStage::kProfileLoaded;
    case AppInitStage::kEnterprise:
      return ProfileInitStage::kEnterprise;
    case AppInitStage::kBrowserObjectsForUI:
      return ProfileInitStage::kPrepareUI;
    case AppInitStage::kNormalUI:
      return ProfileInitStage::kUIReady;
    case AppInitStage::kFirstRun:
      return ProfileInitStage::kFirstRun;
    case AppInitStage::kChoiceScreen:
      return ProfileInitStage::kChoiceScreen;
    case AppInitStage::kFinal:
      return ProfileInitStage::kFinal;
  }
}
