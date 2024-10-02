// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_APP_INIT_STAGE_TEST_UTILS_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_APP_INIT_STAGE_TEST_UTILS_H_

#include "ios/chrome/app/application_delegate/app_init_stage.h"

// Returns the AppInitStage after `app_init_stage`.
AppInitStage NextAppInitStage(AppInitStage app_init_stage);

// Returns the AppInitstage before `app_init_stage`.
AppInitStage PreviousAppInitStage(AppInitStage app_init_stage);

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_APP_INIT_STAGE_TEST_UTILS_H_
