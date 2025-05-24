// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/app/application_delegate/app_init_stage_test_utils.h"

#include "base/check_op.h"
#include "base/types/cxx23_to_underlying.h"

AppInitStage NextAppInitStage(AppInitStage app_init_stage) {
  DCHECK_LT(app_init_stage, AppInitStage::kFinal);
  return static_cast<AppInitStage>(base::to_underlying(app_init_stage) + 1);
}

AppInitStage PreviousAppInitStage(AppInitStage app_init_stage) {
  DCHECK_GT(app_init_stage, AppInitStage::kStart);
  return static_cast<AppInitStage>(base::to_underlying(app_init_stage) - 1);
}
